#include "UiRenderer.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <new>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include <Core/FrameArena.hpp>
#include <DeviceHost.hpp>
#include <EditorUi/Bindings.hpp>
#include <Shader/RhiProgram.hpp>

#include "Viewport/Viewport.hpp"

/// @file
/// @brief 准备 UI 几何与绑定, 向开放列表发出上传和绘制, 消费同一次提交的回执.
/// 在列表开放前解析当轮的诊断颜色或占位, 并按实际录制顺序预检上传容量.
/// 批次持有 RHI 句柄, 借用帧槽几何与像素, 纹理请求借用到确认或取消.

namespace Physara::Editor::Detail
{
    namespace UiShader = Shader::Generated::EditorUi;

    static constexpr std::size_t GeometryBudget = 8 * 1024 * 1024;
    static constexpr auto TargetFormat = RHI::Format::SRGBA8_UNORM;
    // 索引按固定 ImGui 编译配置取数. 顶点色的四个字节与 RGBA8 输入顺序一致.
    static constexpr auto IndexFormat = sizeof(ImDrawIdx) == 2 ? RHI::Format::R16_UINT : RHI::Format::R32_UINT;
    static_assert(std::is_unsigned_v<ImDrawIdx> && (sizeof(ImDrawIdx) == 2 || sizeof(ImDrawIdx) == 4));
    static_assert(std::endian::native == std::endian::little);
    static_assert(IM_COL32_R_SHIFT == 0 && IM_COL32_G_SHIFT == 8 && IM_COL32_B_SHIFT == 16 && IM_COL32_A_SHIFT == 24);
    static_assert(sizeof(ImVec2) == 2 * sizeof(float) && sizeof(ImDrawVert::col) == 4);
    static_assert(sizeof(UiShader::UiProjection) <= RHI::ConstantBufferOffsetSizeAlignment);
    using ProjectionBytes = std::array<std::byte, sizeof(UiShader::UiProjection)>;

    /// @brief 空绑定表示 reset-state, 普通 draw 持有本帧已解析的纹理依赖.
    struct UiDraw
    {
        RHI::Rect clip;
        RHI::DrawArguments arguments;
        RHI::BindingSetHandle binding;
    };

    /// @brief 从封存帧提取的一次录制输入. 按 draw 顺序保存 scissor 和绑定.
    /// 顶点和索引借用本次帧槽的已复制区段. 纯纹理上传或 reset 批次可省去几何缓冲.
    struct UiBatch
    {
        // MSVC Debug 会为空容器分配代理. 显式 count 构造允许准备入口捕获分配失败.
        UiBatch() : draws(0)
        {
        }

        RHI::FramebufferHandle target;
        RHI::CommandListHandle recorded; ///< 完整录制使用的列表, 供 Finish 核对回执对象.
        Platform::WindowExtent extent;
        ProjectionBytes projection;
        std::span<std::byte> vertices;
        std::span<std::byte> indices;
        std::vector<UiDraw> draws;
        UiTextureUploads uploads;
        RHI::BufferHandle vertexBuffer;
        RHI::BufferHandle indexBuffer;
        std::size_t uploadChunkSize = 0;
        bool hasDraws = false;
        bool usesDiagnosticOutput = false;
        bool diagnosticUploadPending = false;
    };

    /// @brief 准备值的控制对象. owner 与 batch 一起解除后, 外部句柄可以安全地过期.
    struct UiFrameData
    {
        ~UiFrameData()
        {
            if (owner != nullptr)
            {
                owner->Complete(*this, false);
            }
        }

        UiRenderer *owner = nullptr;
        std::optional<UiBatch> batch{std::in_place};
    };

    /// @brief 先由批次析构结束尾部借用, 再归还本次失败准备所占的空间.
    struct ArenaTail
    {
        Core::FrameArena &arena;
        const std::size_t used = arena.Used();
        bool keep = false;

        ~ArenaTail()
        {
            if (!keep)
            {
                arena.Rewind(used);
            }
        }
    };

    // 固定 ImGui 用函数地址标记 reset. 录制器识别该地址后经公共 RHI 执行重建.
    static void ResetRenderState(const ImDrawList *, const ImDrawCmd *)
    {
    }

    // Pipeline 与目标共享格式和样本合同, 尺寸在每轮准备时单独校验.
    static RHI::FramebufferInfo UiFramebufferInfo()
    {
        return RHI::FramebufferInfo().AddColorFormat(TargetFormat);
    }

    /// @brief 只接受相同发布的真实像素范围和基线单色二维目标.
    static bool IsCompatibleTarget(const RHI::Framebuffer *target, Platform::WindowExtent extent)
    {
        constexpr auto maxCoordinate = static_cast<std::uint32_t>(std::numeric_limits<int>::max());
        if (target == nullptr || extent.width > maxCoordinate || extent.height > maxCoordinate)
        {
            return false;
        }
        const auto &info = target->GetFramebufferInfo();
        const auto &desc = target->GetDesc();
        return info.width == extent.width && info.height == extent.height && info.arraySize == 1 &&
               info == UiFramebufferInfo() && !desc.shadingRateAttachment.Valid() &&
               desc.colorAttachments.size() == 1 &&
               desc.colorAttachments[0].texture->GetDesc().dimension == RHI::TextureDimension::Texture2D;
    }

    /// @brief 从逻辑左上坐标得到 NDC, 像素比例另用于 scissor.
    static bool PrepareProjection(const ImDrawData &data, UiShader::UiProjection &projection)
    {
        if (!std::isfinite(data.DisplaySize.x) || !std::isfinite(data.DisplaySize.y) ||
            data.DisplaySize.x <= 0.0f || data.DisplaySize.y <= 0.0f ||
            !std::isfinite(data.FramebufferScale.x) || !std::isfinite(data.FramebufferScale.y) ||
            data.FramebufferScale.x <= 0.0f || data.FramebufferScale.y <= 0.0f)
        {
            return false;
        }
        projection.scale = {2.0f / data.DisplaySize.x, -2.0f / data.DisplaySize.y};
        projection.bias = {-1.0f - data.DisplayPos.x * projection.scale[0],
                           1.0f - data.DisplayPos.y * projection.scale[1]};
        return std::ranges::all_of(projection.scale, [](float value) { return std::isfinite(value); }) &&
               std::ranges::all_of(projection.bias, [](float value) { return std::isfinite(value); });
    }

    // 半开裁剪区间按像素中心转换. 在整数转换之前夹取, 保持 RHI 左上行序.
    static int ClipEdge(float coordinate, float origin, float scale, std::uint32_t extent) noexcept
    {
        const double pixel = (static_cast<double>(coordinate) - origin) * scale;
        return static_cast<int>(std::ceil(std::clamp(pixel, 0.0, static_cast<double>(extent)) - 0.5));
    }

    // 将四条逻辑边转换为同一像素范围内的 scissor. 非有限输入由准备阶段拒绝.
    static std::optional<RHI::Rect> ClipRect(const ImVec4 &clip, const ImDrawData &data,
                                            Platform::WindowExtent extent) noexcept
    {
        if (!std::isfinite(clip.x) || !std::isfinite(clip.y) || !std::isfinite(clip.z) || !std::isfinite(clip.w))
        {
            return std::nullopt;
        }
        return RHI::Rect(ClipEdge(clip.x, data.DisplayPos.x, data.FramebufferScale.x, extent.width),
                         ClipEdge(clip.z, data.DisplayPos.x, data.FramebufferScale.x, extent.width),
                         ClipEdge(clip.y, data.DisplayPos.y, data.FramebufferScale.y, extent.height),
                         ClipEdge(clip.w, data.DisplayPos.y, data.FramebufferScale.y, extent.height));
    }

    // 写入计划和几何准备共用可见性条件, 以同一个像素裁剪判断普通命令是否消费资源.
    static bool HasVisibleIndices(const ImDrawCmd &draw, const RHI::Rect &clip) noexcept
    {
        return draw.ElemCount != 0 && clip.Width() > 0 && clip.Height() > 0;
    }

    /// @brief 可见命令实际触发的写入. 托管纹理写入已由纹理请求单独确定.
    struct DrawUploads
    {
        bool geometry = false;
        bool diagnostic = false;
        bool placeholder = false;
    };

    // 准入前只检查采样及裁剪需求, 此时尚未复制像素或建立临时纹理身份.
    static DrawUploads MeasureDrawUploads(const ImDrawData &data, std::span<ImDrawList *const> lists,
                                          UiTextures &textures, const UiBatch &batch, const UiFrameBindings &bindings)
    {
        DrawUploads writes;
        for (const auto *list : lists)
        {
            for (const auto &draw : list->CmdBuffer)
            {
                if (draw.UserCallback != nullptr)
                {
                    continue;
                }
                const auto clip = ClipRect(draw.ClipRect, data, batch.extent);
                if (!clip || !HasVisibleIndices(draw, *clip))
                {
                    continue;
                }
                writes.geometry = true;
                const auto use = textures.MeasureSample(draw, batch.uploads, bindings);
                writes.diagnostic |= use == UiTextures::SampleUse::Diagnostic;
                writes.placeholder |= use == UiTextures::SampleUse::Placeholder;
            }
        }
        return writes;
    }

    /// @brief 封存几何的元素计数, 供拥有型缓冲预留和字节预算检查.
    struct GeometrySize
    {
        std::size_t vertices = 0;
        std::size_t indices = 0;
        std::size_t commands = 0;

        /// @brief 当前元素计数对应的字节数, 供几何预算检查. arena 预留另计对齐填充.
        [[nodiscard]] std::size_t Bytes() const noexcept
        {
            return vertices * sizeof(ImDrawVert) + indices * sizeof(ImDrawIdx);
        }
    };

    // 合成器生成的原因列表和 ImGui 封存列表使用同一总几何预算.
    static bool AddGeometry(const ImDrawList &list, GeometrySize &size) noexcept
    {
        if (list.VtxBuffer.Size < 0 || list.IdxBuffer.Size < 0 || list.CmdBuffer.Size < 0)
        {
            return false;
        }
        size.vertices += static_cast<std::size_t>(list.VtxBuffer.Size);
        size.indices += static_cast<std::size_t>(list.IdxBuffer.Size);
        size.commands += static_cast<std::size_t>(list.CmdBuffer.Size);
        return size.Bytes() <= GeometryBudget && size.commands <= GeometryBudget / sizeof(UiDraw);
    }

    // 在分配和访问列表数据前核对封存的计数. 准备的命令副本也有固定字节预算.
    static std::optional<GeometrySize> MeasureGeometry(const ImDrawData &data)
    {
        if (!data.Valid || data.CmdListsCount < 0 || data.CmdListsCount != data.CmdLists.Size)
        {
            return std::nullopt;
        }
        GeometrySize size;
        for (const auto *list : data.CmdLists)
        {
            if (list == nullptr || !AddGeometry(*list, size))
            {
                return std::nullopt;
            }
        }
        if (size.vertices != static_cast<std::size_t>(data.TotalVtxCount) ||
            size.indices != static_cast<std::size_t>(data.TotalIdxCount))
        {
            return std::nullopt;
        }
        return size;
    }

    // 先核对索引子区间, 再验证它相对于 VtxOffset 的顶点访问. RHI 只看到拼接后的缓冲.
    static bool HasValidDrawRange(const ImDrawList &list, const ImDrawCmd &draw) noexcept
    {
        const auto indexCount = static_cast<std::uint32_t>(list.IdxBuffer.Size);
        const auto vertexCount = static_cast<std::uint32_t>(list.VtxBuffer.Size);
        if (draw.IdxOffset > indexCount || draw.ElemCount > indexCount - draw.IdxOffset ||
            draw.VtxOffset >= vertexCount)
        {
            return false;
        }
        const std::span indices(list.IdxBuffer.Data + draw.IdxOffset, draw.ElemCount);
        return std::ranges::all_of(indices, [&](ImDrawIdx index) { return index < vertexCount - draw.VtxOffset; });
    }

    // 分流回调后才处理普通命令. 先裁剪和校验局部索引, 再解析绑定并换算到连续缓冲的元素基址.
    static std::optional<Core::BoundedText> PrepareGeometry(const ImDrawData &data,
                                                           std::span<ImDrawList *const> lists,
                                                           const GeometrySize &size,
                                                           UiTextures &textures, UiBatch &batch,
                                                           Core::FrameArena &arena, UiFrameBindings &bindings)
    {
        batch.vertices = arena.Allocate(size.vertices, sizeof(ImDrawVert), alignof(ImDrawVert));
        batch.indices = arena.Allocate(size.indices, sizeof(ImDrawIdx), alignof(ImDrawIdx));
        if ((size.vertices != 0 && batch.vertices.empty()) || (size.indices != 0 && batch.indices.empty()))
        {
            return Core::BoundedText("Editor UI geometry exceeds the reserved frame storage");
        }
        batch.draws.reserve(size.commands);
        std::size_t vertexBase = 0;
        std::size_t indexBase = 0;
        for (const auto *list : lists)
        {
            for (const auto &draw : list->CmdBuffer)
            {
                if (draw.UserCallback != nullptr)
                {
                    if (draw.UserCallback != ImGui::GetPlatformIO().DrawCallback_ResetRenderState)
                    {
                        return Core::BoundedText("Editor UI prepare rejected an undeclared draw callback");
                    }
                    batch.draws.emplace_back();
                    continue;
                }
                const auto clip = ClipRect(draw.ClipRect, data, batch.extent);
                if (!clip)
                {
                    return Core::BoundedText("Editor UI prepare rejected a non-finite clip rectangle");
                }
                if (!HasVisibleIndices(draw, *clip))
                {
                    continue;
                }
                if (!HasValidDrawRange(*list, draw))
                {
                    return Core::BoundedText("Editor UI prepare rejected an out-of-range indexed draw");
                }
                auto binding = textures.Resolve(draw, batch.uploads, bindings);
                if (!binding)
                {
                    return Core::BoundedText("Editor UI prepare could not resolve a font or viewport binding");
                }
                // 总几何字节预算保证两个元素基址均能表示为 RHI 的 32 位参数.
                RHI::DrawArguments arguments;
                arguments.SetVertexCount(draw.ElemCount)
                    .SetStartIndexLocation(static_cast<std::uint32_t>(indexBase + draw.IdxOffset))
                    .SetStartVertexLocation(static_cast<std::uint32_t>(vertexBase + draw.VtxOffset));
                batch.draws.push_back({*clip, arguments, std::move(binding)});
                batch.hasDraws = true;
            }
            if (!list->VtxBuffer.empty())
            {
                const auto count = static_cast<std::size_t>(list->VtxBuffer.Size);
                std::memcpy(batch.vertices.data() + vertexBase * sizeof(ImDrawVert), list->VtxBuffer.Data,
                            count * sizeof(ImDrawVert));
                vertexBase += count;
            }
            if (!list->IdxBuffer.empty())
            {
                const auto count = static_cast<std::size_t>(list->IdxBuffer.Size);
                std::memcpy(batch.indices.data() + indexBase * sizeof(ImDrawIdx), list->IdxBuffer.Data,
                            count * sizeof(ImDrawIdx));
                indexBase += count;
            }
        }
        return std::nullopt;
    }

    UiRenderer::UiRenderer(Platform::DeviceHost &host, RHI::Device &device) noexcept
        : m_Host(host), m_Device(device), m_Textures(device)
    {
    }

    UiRenderer::~UiRenderer()
    {
        CancelPending();
        if (m_Attached)
        {
            // 先解除 ImGui 纹理关联, 再清除对当前 context 公布的能力与回调.
            m_Textures.Detach();
            ImGui::GetIO().BackendFlags &=
                ~(ImGuiBackendFlags_RendererHasTextures | ImGuiBackendFlags_RendererHasVtxOffset);
            auto &platform = ImGui::GetPlatformIO();
            platform.DrawCallback_ResetRenderState = nullptr;
            platform.Renderer_TextureMaxWidth = platform.Renderer_TextureMaxHeight = 0;
        }
    }

    UiRenderer::CreateResult UiRenderer::Create(Platform::DeviceHost &host, const UiRendererDesc &desc)
    {
        auto renderer = std::unique_ptr<UiRenderer>(new UiRenderer(host, desc.device));
        if (auto error = renderer->Initialize(desc.packagePath))
        {
            return *error;
        }
        // 资源完整后才启用动态 atlas. 在 NewFrame 使用字体前确认固定默认字体已装入.
        auto &io = ImGui::GetIO();
        io.Fonts->TexDesiredFormat = ImTextureFormat_Alpha8;
        // 固定版本从 atlas 自身的字段限制增长, 与纹理请求的大小上限一致.
        io.Fonts->TexMaxWidth = io.Fonts->TexMaxHeight = UiTextures::MaxTextureSize;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures | ImGuiBackendFlags_RendererHasVtxOffset;
        auto &platform = ImGui::GetPlatformIO();
        platform.DrawCallback_ResetRenderState = ResetRenderState;
        platform.Renderer_TextureMaxWidth = platform.Renderer_TextureMaxHeight = UiTextures::MaxTextureSize;
        renderer->m_Attached = true;
        // 当前字体来自 ImGui 内置默认字体, 动态 atlas 只装入该字体可提供的字形.
        if (io.Fonts->AddFontDefault() == nullptr || io.Fonts->TexData == nullptr || io.Fonts->TexData->Pixels == nullptr)
        {
            return Core::BoundedText("Editor base font initialization failed to create the default atlas");
        }
        return renderer;
    }

    // 由生成 ABI 校验 Shader, 再建立采样绑定和顶点布局, 最后创建与目标格式兼容的 Pipeline.
    std::optional<Core::BoundedText> UiRenderer::Initialize(const std::filesystem::path &packagePath)
    {
        const auto program = Shader::LoadRhiProgram(m_Device, packagePath, UiShader::AbiHash);
        if (const auto *failure = std::get_if<Shader::Failure>(&program))
        {
            Core::BoundedText message("Editor UI Shader package: ");
            message.Append(failure->context.description.View());
            return message;
        }
        const auto layouts = UiShader::MakeBindingLayouts();
        static_assert(std::tuple_size_v<decltype(layouts)> == 1);
        if (auto error = m_Textures.Initialize(layouts.front()))
        {
            return error;
        }
        const auto &shaders = std::get<Shader::RhiProgram>(program);
        const std::array attributes{
            RHI::VertexAttributeDesc().SetName("Position").SetFormat(RHI::Format::RG32_FLOAT)
                .SetOffset(offsetof(ImDrawVert, pos)).SetElementStride(sizeof(ImDrawVert)),
            RHI::VertexAttributeDesc().SetName("Uv").SetFormat(RHI::Format::RG32_FLOAT)
                .SetOffset(offsetof(ImDrawVert, uv)).SetElementStride(sizeof(ImDrawVert)),
            RHI::VertexAttributeDesc().SetName("Color").SetFormat(RHI::Format::RGBA8_UNORM)
                .SetOffset(offsetof(ImDrawVert, col)).SetElementStride(sizeof(ImDrawVert))};
        auto input = m_Device.CreateInputLayout(attributes.data(), static_cast<std::uint32_t>(attributes.size()),
                                                shaders.vertex.Get());
        if (!input)
        {
            return Core::BoundedText("Editor UI initialization failed to create the vertex layout");
        }
        RHI::GraphicsPipelineDesc pipeline;
        pipeline.SetVertexShader(shaders.vertex.Get()).SetFragmentShader(shaders.fragment.Get())
            .SetInputLayout(input.Get()).AddBindingLayout(m_Textures.GetLayout());
        pipeline.renderState.depthStencilState.DisableDepthTest().DisableDepthWrite().DisableStencil();
        pipeline.renderState.rasterState.SetCullMode(RHI::RasterCullMode::None).EnableScissor();
        // 源颜色是 straight alpha. RGB 乘一次源 alpha, 输出 alpha 单独做覆盖累积.
        pipeline.renderState.blendState.targets[0].EnableBlend()
            .SetSrcBlend(RHI::BlendFactor::SrcAlpha).SetDestBlend(RHI::BlendFactor::OneMinusSrcAlpha)
            .SetSrcBlendAlpha(RHI::BlendFactor::One).SetDestBlendAlpha(RHI::BlendFactor::OneMinusSrcAlpha);
        m_Pipeline = m_Device.CreateGraphicsPipeline(pipeline, UiFramebufferInfo());
        if (!m_Pipeline)
        {
            return Core::BoundedText("Editor UI initialization failed to create the graphics pipeline");
        }
        return std::nullopt;
    }

    // 同队列有序写入复用普通缓冲. 替换后的旧缓冲仍由已发的 RHI 包持有.
    static RHI::BufferHandle ReserveGeometry(RHI::Device &device, const RHI::BufferHandle &current,
                                             std::size_t capacity, bool vertex)
    {
        if (current && current->GetDesc().byteSize == capacity)
        {
            return current;
        }
        const RHI::BufferDesc desc{
            .byteSize = capacity,
            .debugName = vertex ? "Editor UI vertices" : "Editor UI indices",
            .isVertexBuffer = vertex,
            .isIndexBuffer = !vertex,
            .keepInitialState = true};
        return device.CreateBuffer(desc);
    }

    // 容量按需增长. 两种缓冲都准备成功后才替换 renderer 持有的缓存.
    bool UiRenderer::PrepareBuffers(UiBatch &batch)
    {
        const auto vertexBytes = batch.vertices.size();
        const auto indexBytes = batch.indices.size();
        auto vertexCapacity = std::max(m_VertexBuffer ? m_VertexBuffer->GetDesc().byteSize : 0,
                                       static_cast<std::uint64_t>(std::bit_ceil(vertexBytes)));
        auto indexCapacity = std::max(m_IndexBuffer ? m_IndexBuffer->GetDesc().byteSize : 0,
                                      static_cast<std::uint64_t>(std::bit_ceil(indexBytes)));
        if (vertexCapacity + indexCapacity > GeometryBudget)
        {
            // 两类缓冲的峰值可出现在不同帧. 接受实际预算内的组合, 必要时收缩旧峰值.
            vertexCapacity = vertexBytes;
            indexCapacity = indexBytes;
        }
        batch.vertexBuffer = ReserveGeometry(m_Device, m_VertexBuffer, vertexCapacity, true);
        batch.indexBuffer = ReserveGeometry(m_Device, m_IndexBuffer, indexCapacity, false);
        if (!batch.vertexBuffer || !batch.indexBuffer)
        {
            return false;
        }
        m_VertexBuffer = batch.vertexBuffer;
        m_IndexBuffer = batch.indexBuffer;
        return true;
    }

    // 基础 atlas 尚未获得有效回执时, 用启动阶段保留纹理准备的原始原因.
    static Core::BoundedText TexturePreparationError(const Core::BoundedText &error, bool baseFontReady) noexcept
    {
        if (baseFontReady)
        {
            return error;
        }
        Core::BoundedText detail("Editor base font initialization: ");
        detail.Append(error.View());
        return detail;
    }

    // 将 View 的无颜色结果转换为本轮占位及状态标签的原因.
    static UiFallback MissingViewportOutput(const Renderer::NoSceneOutputs &outputs) noexcept
    {
        if (outputs.UploadPending())
        {
            return UiFallback::UploadPending;
        }
        switch (outputs.Rejection())
        {
        case Renderer::ViewSettingReason::InvalidExtent: return UiFallback::InvalidExtent;
        case Renderer::ViewSettingReason::TextureSizeLimit: return UiFallback::TextureSizeLimit;
        case Renderer::ViewSettingReason::DiagnosticBudget: return UiFallback::DiagnosticBudget;
        default: return UiFallback::MissingOutput;
        }
    }

    // 先解析视口与容量需求, 再统一预留 arena 并复制数据, 最后封存供 Record 使用的 RHI 依赖.
    PrepareUiResult UiRenderer::Prepare(const ImDrawData &drawData, RHI::Framebuffer *target,
                                        Platform::WindowExtent extent, Core::FrameArena &arena,
                                        const Renderer::NoSceneOutputs &outputs, const ViewportImage &viewport,
                                        const RHI::CommandListParameters &commands)
    {
        if (m_Pending != nullptr)
        {
            return Core::BoundedText("Editor UI prepare requires the previous batch to be finished or cancelled");
        }
        if (!IsCompatibleTarget(target, extent))
        {
            return Core::BoundedText("Editor UI prepare requires a matching single-sample SRGBA8 2D target");
        }
        auto size = MeasureGeometry(drawData);
        UiShader::UiProjection projection;
        if (!size || !PrepareProjection(drawData, projection))
        {
            return Core::BoundedText("Editor UI prepare rejected frame metadata or geometry budget");
        }

        // 守卫先于批次建立. 失败展开时先结束临时身份及借用, 再退回本次分配的尾部.
        ArenaTail tail{arena};
        try
        {
            // 先绑定取消责任. 提前返回会释放批次引用并撤回已安装的临时 token.
            auto frame = std::make_unique<UiFrameData>();
            frame->owner = this;
            m_Pending = frame.get();
            auto &batch = *frame->batch;
            batch.target = target;
            batch.extent = extent;
            batch.projection = std::bit_cast<ProjectionBytes>(projection);
            batch.uploadChunkSize = commands.uploadChunkSize;
            if (auto error = m_Textures.PlanUploads(drawData, batch.uploads))
            {
                return TexturePreparationError(*error, m_BaseFontReady);
            }

            UiFrameBindings bindings{.output = outputs.Diagnostic(), .uploadPending = outputs.UploadPending(),
                                     .viewportDraw = viewport.FindDraw(drawData)};
            if (bindings.viewportDraw)
            {
                // 从封存图像命令读取实际身份. 有效但不属于该视口的 token 也不能绑定.
                const auto &ref = bindings.viewportDraw->TexRef;
                if (ref._TexData != nullptr || ref._TexID != viewport.token)
                {
                    bindings.viewportFallback = UiFallback::InvalidIdentity;
                    bindings.output = nullptr;
                }
                else if (!bindings.output)
                {
                    bindings.viewportFallback = MissingViewportOutput(outputs);
                }
            }
            else
            {
                bindings.output = nullptr;
            }

            std::span<ImDrawList *const> lists(drawData.CmdLists.Data, drawData.CmdLists.Size);
            // 字形已在布局时请求. 准备期把状态文字放入独立列表, 保持原布局命令的顺序.
            std::optional<ImDrawList> status;
            std::optional<std::vector<ImDrawList *>> composed;
            const auto addStatus = [&]
            {
                if (bindings.viewportFallback == UiFallback::None)
                {
                    return true;
                }
                status.emplace(ImGui::GetDrawListSharedData());
                viewport.DrawStatus(*status, *bindings.viewportDraw, bindings.viewportFallback);
                if (status->VtxBuffer.empty())
                {
                    return true;
                }
                if (!AddGeometry(*status, *size))
                {
                    return false;
                }
                // 跟在视口自己的列表之后, 保留其它面板与弹出层的原有覆盖顺序.
                composed.emplace(lists.begin(), lists.end());
                const auto where = std::ranges::find(*composed, viewport.list);
                composed->insert(where + 1, &*status);
                lists = *composed;
                return true;
            };
            if (!addStatus())
            {
                return Core::BoundedText("Editor UI geometry budget exceeded with the viewport status");
            }

            // Runtime 先写诊断色, UI 再写首次占位和托管纹理, 最后写缓冲. 预检须沿用该顺序.
            std::vector<std::size_t> textureWrites(batch.uploads.items.size() + 2);
            std::ranges::transform(batch.uploads.items, textureWrites.begin() + 2, &UiTextureUpload::ByteCount);
            DrawUploads drawUploads;
            const auto checkCapacity = [&]
            {
                // 视口切换到占位时重新测量同一帧的可见命令, 包括新增的原因标签.
                drawUploads = MeasureDrawUploads(drawData, lists, m_Textures, batch, bindings);
                textureWrites[0] = drawUploads.diagnostic ? outputs.UploadBytes() : 0;
                textureWrites[1] = m_Textures.PlaceholderUploadBytes(drawUploads.placeholder);
                const auto bufferWrites = drawUploads.geometry ?
                    std::array{batch.projection.size(), size->vertices * sizeof(ImDrawVert),
                               size->indices * sizeof(ImDrawIdx)} : std::array<std::size_t, 3>{};
                return m_Host.CheckUploadCapacity(batch.uploadChunkSize, textureWrites, bufferWrites);
            };
            auto admission = checkCapacity();
            if ((admission == Platform::UploadAdmission::Pending || admission == Platform::UploadAdmission::ExceedsLimit) &&
                textureWrites[0] != 0)
            {
                // 基础 UI 优先. 改用本帧可见占位后, 按实际合成几何重新核算其上传.
                bindings.output = nullptr;
                bindings.uploadPending = true;
                bindings.viewportFallback = UiFallback::UploadPending;
                if (!addStatus())
                {
                    return Core::BoundedText("Editor UI geometry budget exceeded with the viewport status");
                }
                admission = checkCapacity();
            }
            if (admission == Platform::UploadAdmission::Pending)
            {
                return UiPrepareStatus::UploadPending;
            }
            if (admission != Platform::UploadAdmission::Ready)
            {
                return Core::BoundedText("Editor UI could not admit the planned Graphics uploads");
            }

            // 所有加数来自有界几何/纹理预算. 发布第一段借用前一次预留, 包含准入的诊断像素.
            constexpr auto Padding = alignof(ImDrawVert) + alignof(ImDrawIdx) - 2;
            if (!arena.Reserve(arena.Used() + size->Bytes() + batch.uploads.bytes + textureWrites[0] + Padding))
            {
                return Core::BoundedText("Editor UI prepare could not reserve frame storage");
            }
            if (auto error = m_Textures.PrepareUploads(batch.uploads, arena))
            {
                return TexturePreparationError(*error, m_BaseFontReady);
            }
            // 首次准备须能解析当前 atlas. 此处只检查已建立的关联, 就绪状态留待有效回执置位.
            const auto *baseAtlas = ImGui::GetIO().Fonts->TexData;
            if (!m_BaseFontReady && (!baseAtlas || !m_Textures.HasManagedTexture(*baseAtlas)))
            {
                return Core::BoundedText("Editor base font initialization did not prepare a usable atlas");
            }
            if (auto error = PrepareGeometry(drawData, lists, *size, m_Textures, batch, arena, bindings))
            {
                return *error;
            }
            // 实际为普通 draw 建立诊断绑定时, Runtime 才为本轮生成并录制诊断像素.
            // 此时使用的写入集合须与容量预检一致.
            batch.usesDiagnosticOutput = bindings.viewBinding.Get() != nullptr;
            batch.diagnosticUploadPending = bindings.viewportFallback == UiFallback::UploadPending;
            assert(batch.hasDraws == drawUploads.geometry && batch.uploads.usesPlaceholder == drawUploads.placeholder &&
                   batch.usesDiagnosticOutput == drawUploads.diagnostic);
            if (batch.draws.empty() && !m_Textures.HasUploads(batch.uploads))
            {
                return std::monostate{};
            }
            if (batch.hasDraws && !PrepareBuffers(batch))
            {
                return Core::BoundedText("Editor UI prepare failed to allocate geometry buffers");
            }
            tail.keep = true;
            return PreparedUiFrame(std::move(frame));
        }
        catch (const std::bad_alloc &)
        {
            return Core::BoundedText("Editor UI prepare ran out of CPU storage");
        }
    }

    // 以同一同步区间检查依赖命令. 失败即停止, 列表仍归调用者提交或收拢已发前缀.
    std::optional<Core::BoundedText> UiRenderer::Record(UiBatch &batch, RHI::CommandList &commands)
    {
        if (batch.recorded || commands.GetDevice() != &m_Device ||
            commands.GetDesc().queueType != RHI::CommandQueue::Graphics ||
            commands.GetDesc().uploadChunkSize != batch.uploadChunkSize)
        {
            return Core::BoundedText("Editor UI record requires an unused batch and the prepared Graphics upload chunk size");
        }
        // Validation 和原生后端共用宿主回调, 在首条 UI 命令前取样.
        const auto errors = m_Host.CaptureRhiErrors();
        if (!errors.IsClear())
        {
            return Core::BoundedText("Editor UI record requires a ready RHI error scope");
        }
        // 上传先于本批采样. 几何和像素由 RHI 即时复制, 已发访问随后沿物理包退休.
        if (!m_Textures.Record(commands, batch.uploads, errors))
        {
            return TexturePreparationError(Core::BoundedText("Editor UI record stopped during texture upload"),
                                           m_BaseFontReady);
        }
        RHI::GraphicsState state;
        if (batch.hasDraws)
        {
            commands.WriteBuffer(m_Textures.GetProjectionBuffer(), batch.projection.data(), batch.projection.size());
            if (!errors.IsClear())
            {
                return Core::BoundedText("Editor UI record stopped during projection upload");
            }
            commands.WriteBuffer(batch.vertexBuffer.Get(), batch.vertices.data(), batch.vertices.size());
            if (!errors.IsClear())
            {
                return Core::BoundedText("Editor UI record stopped during vertex upload");
            }
            commands.WriteBuffer(batch.indexBuffer.Get(), batch.indices.data(), batch.indices.size());
            if (!errors.IsClear())
            {
                return Core::BoundedText("Editor UI record stopped during index upload");
            }
            state.SetPipeline(m_Pipeline.Get()).SetFramebuffer(batch.target.Get())
                .AddVertexBuffer(RHI::VertexBufferBinding().SetBuffer(batch.vertexBuffer.Get()))
                .SetIndexBuffer(RHI::IndexBufferBinding().SetBuffer(batch.indexBuffer.Get()).SetFormat(IndexFormat));
            state.viewport.AddViewport(batch.target->GetFramebufferInfo().GetViewport());
            state.viewport.scissorRects.resize(1);
            state.bindings.resize(1);
        }
        for (const auto &draw : batch.draws)
        {
            // reset 只清除图形状态. 下一条普通命令重新提供完整状态及自己的绑定和 scissor.
            if (!draw.binding)
            {
                commands.ClearState();
                if (!errors.IsClear())
                {
                    return Core::BoundedText("Editor UI record stopped during render-state reset");
                }
                continue;
            }
            state.bindings[0] = draw.binding.Get();
            state.viewport.scissorRects[0] = draw.clip;
            commands.SetGraphicsState(state);
            // 候选被拒绝时 RHI 可能保留旧状态. 通过检查后才消费本条索引.
            if (!errors.IsClear())
            {
                return Core::BoundedText("Editor UI record stopped during graphics-state setup");
            }
            commands.DrawIndexed(draw.arguments);
            if (!errors.IsClear())
            {
                return Core::BoundedText("Editor UI record stopped during indexed draw");
            }
        }
        batch.recorded = &commands;
        return std::nullopt;
    }

    // 先处理借用的纹理请求, 再解除批次关系并释放准备数据与 CPU 持有的 RHI 引用.
    void UiRenderer::Complete(UiFrameData &frame, bool accepted) noexcept
    {
        assert(m_Pending == &frame && frame.batch);
        m_Textures.Finish(frame.batch->uploads, accepted);
        m_BaseFontReady |= accepted;
        m_Pending = nullptr;
        frame.owner = nullptr;
        frame.batch.reset();
    }

    void UiRenderer::CancelPending() noexcept
    {
        if (m_Pending != nullptr)
        {
            Complete(*m_Pending, false);
        }
    }

    UiTextureResult UiRenderer::RegisterResident(RHI::Texture &texture)
    {
        return m_Textures.RegisterResident(texture);
    }

    bool UiRenderer::UnregisterResident(UiTextureId id) noexcept
    {
        return m_Textures.UnregisterResident(id);
    }

    UiTextureResult UiRenderer::RegisterView(Renderer::ViewId view)
    {
        return m_Textures.RegisterView(view);
    }
}

namespace Physara::Editor
{
    PreparedUiFrame::PreparedUiFrame(std::unique_ptr<Detail::UiFrameData> data) noexcept : m_Data(std::move(data))
    {
    }

    PreparedUiFrame::~PreparedUiFrame() = default;
    PreparedUiFrame::PreparedUiFrame(PreparedUiFrame &&) noexcept = default;
    PreparedUiFrame &PreparedUiFrame::operator=(PreparedUiFrame &&) noexcept = default;

    bool PreparedUiFrame::UsesDiagnosticOutput() const noexcept
    {
        return m_Data && m_Data->batch && m_Data->batch->usesDiagnosticOutput;
    }

    bool PreparedUiFrame::DiagnosticUploadPending() const noexcept
    {
        return m_Data && m_Data->batch && m_Data->batch->diagnosticUploadPending;
    }

    // 外部准备值可比会话存活更久. 先核对控制对象的 owner 再访问帧槽借用.
    std::optional<Core::BoundedText> PreparedUiFrame::Record(RHI::CommandList &commands)
    {
        if (!m_Data || m_Data->owner == nullptr)
        {
            return Core::BoundedText("Editor UI record received an expired batch");
        }
        auto error = m_Data->owner->Record(*m_Data->batch, commands);
        if (error)
        {
            Cancel();
        }
        return error;
    }

    bool PreparedUiFrame::Finish(const UiSubmission &submission) noexcept
    {
        if (!m_Data || m_Data->owner == nullptr)
        {
            return false;
        }
        // 这里核对列表身份, 同一录制的诊断和接受票据由调用者提供.
        const bool accepted = m_Data->batch->recorded.Get() == &submission.commands &&
                              submission.executedWithoutErrors && submission.ticket != 0;
        m_Data->owner->Complete(*m_Data, accepted);
        m_Data.reset();
        return accepted;
    }

    void PreparedUiFrame::Cancel() noexcept
    {
        m_Data.reset();
    }
}
