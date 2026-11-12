#include "UiTextures.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

#include <Core/FrameArena.hpp>
#include <DeviceHost.hpp>

/// @file
/// @brief 将固定 ImGui 的纹理请求转换为有界 RHI 驻留与本槽持有的像素数据.
/// 创建和更新由录制回执确认. 逻辑注销立即生效, 已发资源由命令列表和提交包保活.

namespace Physara::Editor::Detail
{
    static constexpr std::size_t UploadBudget = 16 * 1024 * 1024;
    static constexpr std::size_t ResidentBudget = 32 * 1024 * 1024;
    static constexpr std::size_t PixelBytes = 4;
    static constexpr std::array<std::uint8_t, PixelBytes> PlaceholderPixel{255, 0, 255, 255};
    static_assert(std::numeric_limits<ImTextureID>::digits == 64 && ImTextureID_Invalid == 0);

    // 宿主只允许一个活动会话, 在同一 owner 线程为当前及后续接入分配不同的 token 代.
    static std::uint32_t LastTextureGeneration = 0;

    ImTextureID EncodeUiTexture(UiTextureId id) noexcept
    {
        assert(id.generation <= std::numeric_limits<std::uint32_t>::max());
        return (static_cast<ImTextureID>(id.generation) << 32) | id.index;
    }

    // 从 ImGui 纹理值恢复逻辑槽位和代, 由 Find 核对是否仍有效.
    static UiTextureId DecodeUiTexture(ImTextureID token) noexcept
    {
        return {.index = static_cast<std::uint32_t>(token), .generation = token >> 32};
    }

    // 管理纹理和区域暂存保持同格式, 复制保留 sRGB 编码后的 texel.
    static RHI::TextureHandle CreateUploadTexture(RHI::Device &device, std::uint32_t width, std::uint32_t height,
                                                  bool sampled)
    {
        // 直接构造名称, 让 MSVC Debug 的 string 代理分配走可抛构造.
        const RHI::TextureDesc desc{
            .width = width,
            .height = height,
            .format = RHI::Format::SRGBA8_UNORM,
            .debugName = sampled ? "Editor UI texture" : "Editor UI update region",
            .isShaderResource = sampled,
            .initialState = RHI::ResourceStates::Common,
            .keepInitialState = true};
        return device.CreateTexture(desc);
    }

    std::optional<Core::BoundedText> UiTextures::Initialize(const RHI::BindingLayoutDesc &layout)
    {
        // 占位也用生成布局和共享 sampler. 像素等到首次需要该绑定的批次才写入.
        m_Layout = m_Device.CreateBindingLayout(layout);
        m_Projection = m_Device.CreateBuffer(
            RHI::BufferDesc().SetByteSize(RHI::ConstantBufferOffsetSizeAlignment)
                .SetIsConstantBuffer(true).SetKeepInitialState(true).SetDebugName("Editor UI projection"));
        m_Sampler = m_Device.CreateSampler(RHI::SamplerDesc().SetMipFilter(false));
        m_Placeholder = CreateUploadTexture(m_Device, 1, 1, true);
        if (!m_Layout || !m_Projection || !m_Sampler || !m_Placeholder)
        {
            return Core::BoundedText("Editor UI initialization failed to create binding resources");
        }
        m_PlaceholderBinding = Bind(m_Placeholder.Get());
        if (!m_PlaceholderBinding)
        {
            return Core::BoundedText("Editor UI initialization failed to bind the placeholder texture");
        }
        return std::nullopt;
    }

    // 按生成布局组装每代的采样集合, 由 Device 建立资源保活关系.
    RHI::BindingSetHandle UiTextures::Bind(RHI::Texture *texture) const
    {
        // 用可抛的 count 构造. 帧准备中的 Debug 代理分配失败展开到 Prepare, 由批次析构回滚.
        RHI::BindingSetDesc set{.bindings = std::vector<RHI::BindingSetItem>(0)};
        for (const auto &item : m_Layout->GetDesc()->bindings)
        {
            switch (item.type)
            {
            case RHI::ResourceType::ConstantBuffer:
                set.AddItem(RHI::BindingSetItem::ConstantBuffer(item.slot, m_Projection.Get()));
                break;
            case RHI::ResourceType::Texture_SRV:
                set.AddItem(RHI::BindingSetItem::Texture_SRV(item.slot, texture));
                break;
            case RHI::ResourceType::Sampler:
                set.AddItem(RHI::BindingSetItem::Sampler(item.slot, m_Sampler.Get()));
                break;
            default:
                assert(false && "Unexpected generated EditorUi layout");
                return {};
            }
        }
        return m_Device.CreateBindingSet(set, m_Layout.Get());
    }

    UiTextures::Entry *UiTextures::Find(UiTextureId id) noexcept
    {
        if (!id.IsValid() || id.index >= m_Entries.size() || m_Entries[id.index].id != id)
        {
            return nullptr;
        }
        return &m_Entries[id.index];
    }

    // 槽位和代有余量且绑定完整后才发布条目. 失败不消耗代或驻留字节.
    UiTextureResult UiTextures::Register(Entry entry)
    {
        const auto slot = std::ranges::find_if(m_Entries, [](const Entry &entry) { return !entry.id.IsValid(); });
        if (slot == m_Entries.end() || LastTextureGeneration == std::numeric_limits<std::uint32_t>::max())
        {
            return Core::BoundedText("Editor UI texture identity capacity exhausted");
        }
        if (entry.texture)
        {
            entry.binding = Bind(entry.texture.Get());
            if (!entry.binding)
            {
                return Core::BoundedText("Editor UI failed to bind the texture");
            }
        }
        entry.id = {static_cast<std::uint32_t>(slot - m_Entries.begin()), ++LastTextureGeneration};
        m_ManagedBytes += entry.managedBytes;
        *slot = std::move(entry);
        return slot->id;
    }

    UiTextureResult UiTextures::RegisterResident(RHI::Texture &texture)
    {
        const auto &desc = texture.GetDesc();
        if (desc.dimension != RHI::TextureDimension::Texture2D || desc.sampleCount != 1 || !desc.isShaderResource ||
            (desc.format != RHI::Format::RGBA8_UNORM && desc.format != RHI::Format::SRGBA8_UNORM))
        {
            return Core::BoundedText("Editor UI resident texture requires a single-sample RGBA8 or SRGBA8 2D SRV");
        }
        return Register(Entry{.texture = RHI::TextureHandle(&texture)});
    }

    // 登记逻辑身份. 具体 View 颜色的句柄和采样绑定留在解析它的批次中.
    UiTextureResult UiTextures::RegisterView(Renderer::ViewId view)
    {
        assert(view.IsValid());
        return Register(Entry{.view = view, .role = Renderer::ViewOutputRole::DiagnosticColor});
    }

    // 解除注册表持有并归还逻辑驻留预算, 已准备的绑定及 GPU 在途引用由各自 owner 释放.
    void UiTextures::Erase(Entry &entry) noexcept
    {
        m_ManagedBytes -= entry.managedBytes;
        entry = {};
    }

    bool UiTextures::UnregisterResident(UiTextureId id) noexcept
    {
        auto *entry = Find(id);
        if (entry == nullptr || entry->managed != nullptr || entry->view.IsValid())
        {
            return false;
        }
        Erase(*entry);
        return true;
    }

    // 只有身份与源元数据都匹配时才回收槽位. 向 ImGui 回报时先清 token 再改状态.
    void UiTextures::Destroy(ImTextureData &source) noexcept
    {
        if (auto *entry = Find(DecodeUiTexture(source.TexID)); entry && entry->managed == &source)
        {
            Erase(*entry);
        }
        source.SetTexID(ImTextureID_Invalid);
        // 固定 ImGui 的 SetStatus 会按当前像素寿命决定是否重新请求创建.
        source.SetStatus(ImTextureStatus_Destroyed);
    }

    std::optional<Core::BoundedText> UiTextures::PlanUploads(const ImDrawData &drawData, UiTextureUploads &uploads)
    {
        if (drawData.Textures == nullptr)
        {
            return std::nullopt;
        }
        // 先释放逻辑槽位, 让同帧增长的 atlas 能占用新代. GPU 引用仍由提交包持有.
        for (auto *source : *drawData.Textures)
        {
            if (source->Status == ImTextureStatus_WantDestroy)
            {
                Destroy(*source);
            }
        }
        auto remainingResident = ResidentBudget - m_ManagedBytes;
        for (auto *source : *drawData.Textures)
        {
            if (source->Status == ImTextureStatus_WantCreate || source->Status == ImTextureStatus_WantUpdates)
            {
                if (auto error = PlanUpload(*source, uploads, remainingResident))
                {
                    return error;
                }
            }
        }
        return std::nullopt;
    }

    // 逐行填满紧密的输出区域. RGBA32 直接复制, Alpha8 扩展为白色及原覆盖率.
    static void CopyTexturePixels(const ImTextureData &source, const RHI::TextureSlice &region,
                                  std::uint8_t *pixels)
    {
        const auto pitch = static_cast<std::size_t>(source.GetPitch());
        const auto rowBytes = region.width * PixelBytes;
        const auto pixelStride = static_cast<std::size_t>(source.BytesPerPixel);
        for (std::size_t y = 0; y < region.height; ++y)
        {
            const auto *row = source.Pixels + (region.y + y) * pitch + region.x * pixelStride;
            auto *target = pixels + y * rowBytes;
            if (source.Format == ImTextureFormat_RGBA32)
            {
                std::memcpy(target, row, rowBytes);
                continue;
            }
            for (std::size_t x = 0; x < region.width; ++x)
            {
                target[0] = target[1] = target[2] = 255;
                target[3] = row[x];
                target += PixelBytes;
            }
        }
    }

    std::optional<Core::BoundedText> UiTextures::PlanUpload(ImTextureData &source, UiTextureUploads &uploads,
                                                           std::size_t &remainingResident)
    {
        // 核对格式, 行距的来源和纹理尺寸. 边长上限也约束下面的行距和字节乘法.
        const bool create = source.Status == ImTextureStatus_WantCreate;
        const int pixelStride = source.Format == ImTextureFormat_RGBA32 ? 4 : 1;
        if ((source.Format != ImTextureFormat_RGBA32 && source.Format != ImTextureFormat_Alpha8) ||
            source.BytesPerPixel != pixelStride || source.Pixels == nullptr ||
            source.Width <= 0 || source.Width > MaxTextureSize || source.Height <= 0 || source.Height > MaxTextureSize)
        {
            return Core::BoundedText("Editor UI prepare rejected texture pixels, format or dimensions");
        }
        const auto width = static_cast<std::uint32_t>(source.Width);
        const auto height = static_cast<std::uint32_t>(source.Height);
        RHI::TextureSlice region;
        region.SetSize(width, height, 1);
        Entry *entry = nullptr;
        if (create)
        {
            if (source.TexID != ImTextureID_Invalid || source.BackendUserData != nullptr)
            {
                return Core::BoundedText("Editor UI texture create already has a backend identity");
            }
        }
        else
        {
            // 更新保持当前代. 在原像素空间校验包围矩形, 准备结果只保存该区域的紧密行.
            entry = Find(DecodeUiTexture(source.TexID));
            if (entry == nullptr || entry->managed != &source ||
                entry->texture->GetDesc().width != width || entry->texture->GetDesc().height != height)
            {
                return Core::BoundedText("Editor UI update has no matching managed texture");
            }
            const auto rect = source.UpdateRect;
            if (rect.w == 0 || rect.h == 0 || rect.x + rect.w > source.Width || rect.y + rect.h > source.Height)
            {
                return Core::BoundedText("Editor UI texture update rectangle is out of bounds");
            }
            region.SetOrigin(rect.x, rect.y).SetSize(rect.w, rect.h, 1);
        }
        const auto bytes = region.width * region.height * PixelBytes;
        if (bytes > UploadBudget - uploads.bytes || (create && bytes > remainingResident))
        {
            return Core::BoundedText("Editor UI texture upload or residency budget exceeded");
        }

        // 此阶段只封存范围. 所有像素在完成统一预留后才写入帧槽.
        auto &upload = uploads.items.emplace_back();
        upload.request = &source;
        upload.region = region;
        uploads.bytes += bytes;
        if (create)
        {
            remainingResident -= bytes;
        }
        else
        {
            upload.target = entry->texture;
        }
        return std::nullopt;
    }

    // 从已预留的 arena 填充像素. 更新复用原身份, 创建只在绑定完整后安装临时 token.
    std::optional<Core::BoundedText> UiTextures::PrepareUploads(UiTextureUploads &uploads, Core::FrameArena &arena)
    {
        for (auto &upload : uploads.items)
        {
            const auto &region = upload.region;
            // 同一请求只能安装一次临时身份. 前面的上传项可能已经为它建立 token.
            if (!upload.target && upload.request->TexID != ImTextureID_Invalid)
            {
                return Core::BoundedText("Editor UI texture create repeats a prepared identity");
            }
            auto pixels = arena.Allocate(upload.ByteCount(), 1);
            if (pixels.empty())
            {
                return Core::BoundedText("Editor UI texture upload exceeds the reserved frame storage");
            }
            CopyTexturePixels(*upload.request, region, reinterpret_cast<std::uint8_t *>(pixels.data()));
            upload.pixels = pixels;

            const bool create = !upload.target;
            auto texture = CreateUploadTexture(m_Device, region.width, region.height, create);
            if (!texture)
            {
                return Core::BoundedText("Editor UI failed to create an upload texture");
            }
            if (!create)
            {
                upload.staging = std::move(texture);
                continue;
            }
            auto registered = Register(Entry{.texture = texture, .managed = upload.request,
                                              .managedBytes = pixels.size()});
            if (const auto *error = std::get_if<Core::BoundedText>(&registered))
            {
                return *error;
            }
            upload.target = std::move(texture);
            upload.created = std::get<UiTextureId>(registered);
            // 临时 token 只供当前准备解析. 上传获有效提交前维持 WantCreate.
            upload.request->SetTexID(EncodeUiTexture(upload.created));
        }
        return std::nullopt;
    }

    bool UiTextures::HasManagedTexture(const ImTextureData &source) noexcept
    {
        const auto *entry = Find(DecodeUiTexture(source.TexID));
        return entry && entry->managed == &source;
    }

    std::size_t UiTextureUpload::ByteCount() const noexcept
    {
        return static_cast<std::size_t>(region.width) * region.height * PixelBytes;
    }

    // 两个阶段共用身份与占位判定. 预检可看到计划创建的托管纹理, 绑定阶段使用已安装的关联.
    UiTextures::Sample UiTextures::FindSample(const ImDrawCmd &draw, const UiFrameBindings &bindings,
                                                std::span<const UiTextureUpload> planned) noexcept
    {
        // 视口图像已为同一个原因排入可见状态. 按命令身份选用对应的占位绑定.
        if (&draw == bindings.viewportDraw && bindings.viewportFallback != UiFallback::None)
        {
            return {nullptr, bindings.viewportFallback};
        }
        const auto &ref = draw.TexRef;
        if (const auto *entry = Find(DecodeUiTexture(ref.GetTexID()));
            entry && (ref._TexData != nullptr ? entry->managed == ref._TexData :
                      !entry->managed || entry->managed->Status != ImTextureStatus_WantCreate))
        {
            // 临时创建只由其 ImTextureData 引用采样. 直接 token 必须在本批准备前已经发布.
            if (!entry->view.IsValid() ||
                (bindings.output && bindings.output->view == entry->view && bindings.output->role == entry->role))
            {
                return {entry};
            }
            return {nullptr, bindings.uploadPending ? UiFallback::UploadPending : UiFallback::MissingOutput};
        }
        if (ref._TexData != nullptr)
        {
            const auto &fontTextures = ImGui::GetIO().Fonts->TexList;
            // 本帧计划创建的身份会在解析前安装. 必需字体缺失则在解析时拒绝整批.
            if (std::ranges::find(fontTextures, ref._TexData) != fontTextures.end() ||
                std::ranges::any_of(planned, [&](const auto &upload)
                {
                    return upload.request == ref._TexData && !upload.target;
                }))
            {
                return {};
            }
        }
        return {nullptr, UiFallback::InvalidIdentity};
    }

    UiTextures::SampleUse UiTextures::MeasureSample(const ImDrawCmd &draw, const UiTextureUploads &uploads,
                                                      const UiFrameBindings &bindings) noexcept
    {
        const auto sample = FindSample(draw, bindings, uploads.items);
        if (sample.fallback != UiFallback::None)
        {
            return SampleUse::Placeholder;
        }
        return sample.entry && sample.entry->view.IsValid() ? SampleUse::Diagnostic : SampleUse::Other;
    }

    RHI::BindingSetHandle UiTextures::Resolve(const ImDrawCmd &draw, UiTextureUploads &uploads,
                                              UiFrameBindings &bindings)
    {
        const auto sample = FindSample(draw, bindings);
        if (!sample.entry)
        {
            return sample.fallback == UiFallback::None ? RHI::BindingSetHandle{} :
                                                         Placeholder(uploads, sample.fallback);
        }
        if (!sample.entry->view.IsValid())
        {
            return sample.entry->binding;
        }
        // 本帧只有一个候选 View 输出. 重复采样可共享这次准备创建的绑定.
        if (!bindings.viewBinding)
        {
            bindings.viewBinding = Bind(bindings.output->texture.Get());
        }
        return bindings.viewBinding;
    }

    // 所有占位共用一个已绑定的像素. 首个占位请求登记上传需求并记录本批诊断.
    RHI::BindingSetHandle UiTextures::Placeholder(UiTextureUploads &uploads, UiFallback reason) const
    {
        if (!uploads.usesPlaceholder)
        {
            uploads.usesPlaceholder = true;
            if (auto *callback = m_Device.GetMessageCallback())
            {
                const char *message = "Editor UI texture identity is invalid or stale";
                if (reason == UiFallback::UploadPending)
                {
                    message = "Editor UI viewport is waiting for diagnostic upload capacity";
                }
                else if (reason == UiFallback::MissingOutput)
                {
                    message = "Editor UI viewport output is unavailable for this frame";
                }
                callback->Message(RHI::MessageSeverity::Warning, message);
            }
        }
        return m_PlaceholderBinding;
    }

    std::size_t UiTextures::PlaceholderUploadBytes(bool used) const noexcept
    {
        return used && !m_PlaceholderReady ? PlaceholderPixel.size() : 0;
    }

    bool UiTextures::HasUploads(const UiTextureUploads &uploads) const noexcept
    {
        return !uploads.items.empty() || PlaceholderUploadBytes(uploads.usesPlaceholder) != 0;
    }

    bool UiTextures::Record(RHI::CommandList &commands, const UiTextureUploads &uploads,
                            const Platform::RhiErrorScope &errors) const
    {
        if (const auto placeholderBytes = PlaceholderUploadBytes(uploads.usesPlaceholder))
        {
            commands.WriteTexture(m_Placeholder.Get(), 0, 0, PlaceholderPixel.data(), placeholderBytes);
            if (!errors.IsClear())
            {
                return false;
            }
        }
        for (const auto &upload : uploads.items)
        {
            auto *writeTarget = upload.staging ? upload.staging.Get() : upload.target.Get();
            commands.WriteTexture(writeTarget, 0, 0, upload.pixels.data(), upload.region.width * PixelBytes);
            if (!errors.IsClear())
            {
                return false;
            }
            if (upload.staging)
            {
                // 公共 WriteTexture 接收整 mip. 先写紧密的区域纹理, 再按原位置 1:1 复制.
                const auto source = RHI::TextureSlice().SetSize(upload.region.width, upload.region.height, 1);
                commands.CopyTexture(upload.target.Get(), upload.region, upload.staging.Get(), source);
                if (!errors.IsClear())
                {
                    return false;
                }
            }
        }
        return true;
    }

    void UiTextures::Finish(const UiTextureUploads &uploads, bool accepted) noexcept
    {
        // OK 会使 ImGui 在下一次 NewFrame 清除更新记录. 失败只撤回本批临时创建的身份.
        for (const auto &upload : uploads.items)
        {
            if (accepted)
            {
                upload.request->SetStatus(ImTextureStatus_OK);
            }
            else if (upload.created.IsValid())
            {
                upload.request->SetTexID(ImTextureID_Invalid);
                Erase(*Find(upload.created));
            }
        }
        m_PlaceholderReady |= accepted && uploads.usesPlaceholder;
    }

    // 会话销毁前结束纹理协议关联. 已发工作需要的绑定和纹理由原提交包保活.
    void UiTextures::Detach() noexcept
    {
        for (auto &entry : m_Entries)
        {
            if (entry.managed != nullptr)
            {
                Destroy(*entry.managed);
            }
        }
    }
}
