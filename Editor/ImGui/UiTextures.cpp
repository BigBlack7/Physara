#include "UiTextures.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

#include <Core/FrameArena.hpp>
#include <EditorUi/Data.hpp>

/// @file
/// @brief 将固定 ImGui 的纹理请求转换为有界 RHI 驻留与本槽持有的像素数据.
/// 准备期核对逻辑身份并复制像素, 构图期关联当前写版本, 成功提交后才发布物理句柄和请求状态.

namespace Physara::Editor::Detail
{
    static constexpr std::size_t UploadBudget = 16 * 1024 * 1024;
    static constexpr std::size_t ResidentBudget = 32 * 1024 * 1024;
    static constexpr std::size_t PixelBytes = 4;
    static constexpr std::array<std::uint8_t, PixelBytes> PlaceholderPixel{255, 0, 255, 255};
    static_assert(std::numeric_limits<ImTextureID>::digits == 64 && ImTextureID_Invalid == 0);

    // 宿主只允许一个活动会话, 在同一 owner 线程为当前及后续接入分配不同的 token 代.
    static std::uint32_t LastTextureGeneration = 0;

    // 高 32 位为进程内不复用的代, 低 32 位为本会话的槽位, 与固定 ImGui 整数协议一致.
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
    static RHI::TextureDesc UploadDescription(std::uint32_t width, std::uint32_t height, bool sampled)
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
        return desc;
    }

    // 共享采样接口先就绪, 各纹理的 BindingSet 留给所属图 Pass 的 prepare.
    std::optional<Core::BoundedText> UiTextures::Initialize(const RHI::BindingLayoutDesc &layout)
    {
        // 投影采用 ABI 的完整范围, 避免把 16 字节数据绑定成未定义的 256 字节子区间.
        // 占位纹理直到图中首次被实际采样才物化.
        m_Layout = m_Device.CreateBindingLayout(layout);
        m_Projection = m_Device.CreateBuffer(
            RHI::BufferDesc().SetByteSize(sizeof(Shader::Generated::EditorUi::UiProjection))
                .SetIsConstantBuffer(true).SetKeepInitialState(true).SetDebugName("Editor UI projection"));
        m_Sampler = m_Device.CreateSampler(RHI::SamplerDesc().SetMipFilter(false));
        if (!m_Layout || !m_Projection || !m_Sampler)
        {
            return Core::BoundedText("Editor UI initialization failed to create binding resources");
        }
        return std::nullopt;
    }

    // 先核对代再返回槽内借用, 防止旧 token 命中新近复用的条目.
    UiTextures::Entry *UiTextures::Find(UiTextureId id) noexcept
    {
        if (!id.IsValid() || id.index >= m_Entries.size() || m_Entries[id.index].id != id)
        {
            return nullptr;
        }
        return &m_Entries[id.index];
    }

    // 槽位和代有余量后发布逻辑条目, 创建请求的物理纹理留到接受回执.
    UiTextureResult UiTextures::Register(Entry entry)
    {
        const auto slot = std::ranges::find_if(m_Entries, [](const Entry &entry) { return !entry.id.IsValid(); });
        if (slot == m_Entries.end() || LastTextureGeneration == std::numeric_limits<std::uint32_t>::max())
        {
            return Core::BoundedText("Editor UI texture identity capacity exhausted");
        }
        entry.id = {static_cast<std::uint32_t>(slot - m_Entries.begin()), ++LastTextureGeneration};
        m_ManagedBytes += entry.managedBytes;
        *slot = std::move(entry);
        return slot->id;
    }

    // 外部 owner 提供内容和状态事实. 此处校验采样形状, 物理绑定的设备归属由执行准备核对.
    UiTextureResult UiTextures::RegisterResident(RHI::Texture &texture, UiTextureImport imported)
    {
        const auto &desc = texture.GetDesc();
        if (desc.dimension != RHI::TextureDimension::Texture2D || desc.sampleCount != 1 || !desc.isShaderResource ||
            (desc.format != RHI::Format::RGBA8_UNORM && desc.format != RHI::Format::SRGBA8_UNORM))
        {
            return Core::BoundedText("Editor UI resident texture requires a single-sample RGBA8 or SRGBA8 2D SRV");
        }
        if (!imported.states.contentDefined || imported.states.entry == RHI::ResourceStates::Unknown ||
            imported.states.final == RHI::ResourceStates::Unknown ||
            !Renderer::Detail::HasKnownResourceStateBits(imported.states.entry) ||
            !Renderer::Detail::HasKnownResourceStateBits(imported.states.final) ||
            imported.range.firstMip >= desc.mipLevels || imported.range.mipCount != 1 ||
            imported.range.firstLayer != 0 || imported.range.layerCount != 1 ||
            imported.range.aspect != Renderer::TextureAspect::Color ||
            (desc.keepInitialState && imported.states.final != desc.initialState))
        {
            return Core::BoundedText(
                "Editor UI resident texture requires defined content and explicit valid import facts");
        }
        return Register(Entry{.texture = RHI::TextureHandle(&texture), .imported = imported});
    }

    // 登记逻辑身份. 具体 View 颜色的句柄和采样绑定留在解析它的批次中.
    UiTextureResult UiTextures::RegisterView(Renderer::ViewId view)
    {
        assert(view.IsValid());
        return Register(Entry{.view = view, .role = Renderer::ViewOutputRole::DiagnosticColor});
    }

    // 解除注册表持有并归还逻辑驻留预算, 已准备的驻留快照及 GPU 在途引用由各自 owner 释放.
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

    // 先消费逻辑删除, 再按驻留和上传两种预算规划 create/update; 尚不分配原生纹理.
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

    // 创建计完整图像, 更新计包围矩形. 规划只借用源元数据, 后续统一预留后才复制像素.
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

    // 从已预留的 arena 填充像素. 更新复用原身份, 创建只安装当轮协议关联的临时 token.
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

            if (upload.target)
            {
                continue;
            }
            auto registered = Register(Entry{
                .imported = {{RHI::ResourceStates::Common, RHI::ResourceStates::Common, true}, {}},
                .managed = upload.request, .managedBytes = pixels.size()});
            if (const auto *error = std::get_if<Core::BoundedText>(&registered))
            {
                return *error;
            }
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

    // 解析前已安装全部临时身份. 本轮图声明沿同一逻辑 token 查找已复制载荷的生产版本.
    UiTextures::Sample UiTextures::FindSample(const ImDrawCmd &draw, const UiFrameBindings &bindings) noexcept
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
            if (std::ranges::find(fontTextures, ref._TexData) != fontTextures.end())
            {
                return {};
            }
        }
        return {nullptr, UiFallback::InvalidIdentity};
    }

    // 普通可见 draw 才进入这里. 无效非字体引用登记共享占位, View 引用记录本轮实际消费.
    std::optional<UiTextureId> UiTextures::Resolve(const ImDrawCmd &draw, UiTextureUploads &uploads,
                                                  UiFrameBindings &bindings)
    {
        const auto sample = FindSample(draw, bindings);
        if (!sample.entry)
        {
            if (sample.fallback == UiFallback::None)
            {
                return std::nullopt;
            }
            Placeholder(uploads, sample.fallback);
            return UiTextureId{};
        }
        bindings.usesDiagnostic |= sample.entry->view.IsValid();
        // 托管和 View 条目不能由注销入口移除, 下一次 Advance 又会先取消准备值, 只需快照外部驻留项.
        if (!sample.entry->view.IsValid() && !sample.entry->managed &&
            std::ranges::find(bindings.residents, sample.entry->id, &UiResidentSnapshot::token) ==
                bindings.residents.end())
        {
            // 准备后 registry 可被注销或复用. 原对象和导入事实属于已封存批次, 不再借用可变条目.
            bindings.residents.push_back({sample.entry->id, sample.entry->texture, sample.entry->imported});
        }
        return sample.entry->id;
    }

    // 所有占位共用同一像素. 首个占位请求登记图上传需求并记录本批诊断.
    void UiTextures::Placeholder(UiTextureUploads &uploads, UiFallback reason) const
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
                else if (reason != UiFallback::InvalidIdentity)
                {
                    message = "Editor UI viewport output is unavailable for this frame";
                }
                callback->Message(RHI::MessageSeverity::Warning, message);
            }
        }
    }

    // 每个区域更新分成暂存写和目标复制, 避免同 Pass 的重叠传输读写.
    struct UiRegionCopy
    {
        Renderer::TextureId source;
        Renderer::TextureId target;
        RHI::TextureSlice region;
    };

    // 声明全部合法候选但不设导出根. 之后的普通 draw 决定哪些上传真正存活.
    Renderer::GraphFailure UiTextures::DeclareUploads(Renderer::RenderGraph &graph, UiTextureUploads &uploads,
        FrameSlotUiFrameBindings &bindings, std::vector<Renderer::GraphUpload> &writes)
    {
        using namespace Renderer;
        if (uploads.usesPlaceholder)
        {
            auto target = m_Placeholder ? graph.ImportTexture(m_Placeholder,
                {RHI::ResourceStates::Common, RHI::ResourceStates::Common, true}) :
                graph.CreateTexture(UploadDescription(1, 1, true));
            if (const auto *error = std::get_if<GraphError>(&target))
            {
                return *error;
            }
            uploads.placeholder.texture = std::get<TextureId>(target);
            if (!m_Placeholder)
            {
                auto written = AddTextureUpload(graph, uploads.placeholder.texture,
                    std::as_bytes(std::span(PlaceholderPixel)), PixelBytes, writes);
                if (const auto *error = std::get_if<GraphError>(&written))
                {
                    return *error;
                }
                uploads.placeholder = std::get<GraphTextureWrite>(written);
            }
            bindings.tokens.push_back({{}, {uploads.placeholder.texture, {}}});
        }
        for (auto &upload : uploads.items)
        {
            const auto &region = upload.region;
            auto source = graph.CreateTexture(UploadDescription(region.width, region.height, !upload.target));
            if (const auto *error = std::get_if<GraphError>(&source))
            {
                return *error;
            }
            auto written = AddTextureUpload(graph, std::get<TextureId>(source), upload.pixels,
                                             region.width * PixelBytes, writes);
            if (const auto *error = std::get_if<GraphError>(&written))
            {
                return *error;
            }
            upload.written = std::get<GraphTextureWrite>(written);
            if (upload.target)
            {
                // 托管纹理在 Close 后保持 Common. Load 保留区域外 texel, 终结 Pass 改为目标复制.
                auto imported = graph.ImportTexture(upload.target,
                    {RHI::ResourceStates::Common, RHI::ResourceStates::Common, true});
                if (const auto *error = std::get_if<GraphError>(&imported))
                {
                    return *error;
                }
                TextureId target;
                auto copied = graph.AddPass<UiRegionCopy>("UI region copy", [&](PassBuilder &builder, auto &data)
                {
                    target = builder.Write(std::get<TextureId>(imported), GraphUsage::CopyDestination,
                                           {.load = LoadOp::Load});
                    data = {upload.written.texture, target, region};
                    return builder.Read(data.source, GraphUsage::CopySource) && target.IsValid();
                }, +[](PassResources &resources, const UiRegionCopy &data, RHI::CommandList &commands) noexcept
                    -> GraphFailure
                {
                    auto *sourceTexture = resources.Resolve(data.source, GraphUsage::CopySource);
                    auto *targetTexture = resources.Resolve(data.target, GraphUsage::CopyDestination);
                    if (sourceTexture && targetTexture)
                    {
                        commands.CopyTexture(targetTexture, data.region, sourceTexture,
                            RHI::TextureSlice().SetSize(data.region.width, data.region.height, 1));
                    }
                    return std::nullopt;
                });
                if (const auto *error = std::get_if<GraphError>(&copied))
                {
                    return *error;
                }
                upload.written = {target, std::get<PassId>(copied)};
            }
            bindings.tokens.push_back({DecodeUiTexture(upload.request->TexID), {upload.written.texture, {}}});
        }
        return std::nullopt;
    }

    // 先取本轮生产者版本, 再使用驻留快照. 其余托管和 View 条目的寿命由活动准备值的会话合同保证.
    Renderer::GraphResult<UiFrameTexture> UiTextures::ResolveGraph(Renderer::RenderGraph &graph,
        UiTextureId token, Renderer::TextureId diagnostic, FrameSlotUiFrameBindings &bindings,
        std::span<const UiResidentSnapshot> residents)
    {
        using namespace Renderer;
        const auto known = std::ranges::find(bindings.tokens, token, &FrameSlotUiFrameBindings::Token::token);
        if (known != bindings.tokens.end())
        {
            return known->value;
        }
        // 快照按原 token 的完整代匹配. registry 即使复用同一槽位, 本批仍导入准备时的原对象.
        const auto resident = std::ranges::find(residents, token, &UiResidentSnapshot::token);
        const auto *entry = resident == residents.end() ? Find(token) : nullptr;
        if (resident == residents.end() && !entry)
        {
            return GraphError{GraphErrorCode::InvalidResource, Core::BoundedText("Expired UI graph token")};
        }
        UiFrameTexture value{diagnostic, {}};
        if (resident != residents.end() || !entry->view.IsValid())
        {
            const auto &texture = resident != residents.end() ? resident->texture : entry->texture;
            const auto &facts = resident != residents.end() ? resident->imported : entry->imported;
            auto imported = graph.ImportTexture(texture, facts.states, facts.range);
            if (const auto *error = std::get_if<GraphError>(&imported))
            {
                return *error;
            }
            value = {std::get<TextureId>(imported), facts.range};
            // 等价导入返回版本零, alias 必须沿当前帧的规范身份取最新写版本.
            for (const auto &item : bindings.tokens)
            {
                if (item.value.texture.identity == value.texture.identity &&
                    item.value.texture.version > value.texture.version)
                {
                    value.texture = item.value.texture;
                }
            }
        }
        bindings.tokens.push_back({token, value});
        return value;
    }

    // 非空回执已证明整图成功, 因而存活终结 Pass 等价于本请求实际执行成功.
    // 创建还需导出句柄才能持久发布; 更新继续使用原对象. 未确认的创建撤销身份并归还预算.
    void UiTextures::Finish(const UiTextureUploads &uploads, const UiSubmission *submission) noexcept
    {
        for (const auto &upload : uploads.items)
        {
            const bool live = submission && Renderer::IsLivePass(submission->plan, upload.written.pass);
            auto *created = upload.created.IsValid() ? Find(upload.created) : nullptr;
            auto *texture = live && created ?
                Renderer::FindTextureOutput(*submission->outputs, upload.written.texture) : nullptr;
            if (live && (!created || texture))
            {
                if (created)
                {
                    created->texture = texture;
                }
                upload.request->SetStatus(ImTextureStatus_OK);
            }
            else if (created)
            {
                upload.request->SetTexID(ImTextureID_Invalid);
                Erase(*created);
            }
        }
        if (submission && Renderer::IsLivePass(submission->plan, uploads.placeholder.pass))
        {
            m_Placeholder = Renderer::FindTextureOutput(*submission->outputs, uploads.placeholder.texture);
        }
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
