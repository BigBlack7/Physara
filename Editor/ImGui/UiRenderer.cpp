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
/// @brief 准备封存的 UI 值, 声明上传及 Raster, 消费同一次图执行的提交回执.
/// 普通 draw 解析成帧内版本, 执行时按精确权限建立绑定. 纹理请求借用到确认或取消.

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

    /// @brief reset 独立表示状态重建, 普通 draw 从 token 解析到帧内绑定索引.
    struct UiDraw
    {
        RHI::Rect clip;
        RHI::DrawArguments arguments;
        UiTextureId token;
        bool reset = true;
        std::size_t binding = 0;
    };

    /// @brief 从封存帧提取的一次录制输入. 按 draw 顺序保存 scissor 和绑定.
    /// 顶点和索引借用帧槽区段, 投影字节由批次拥有. 三者在图执行结束前均须保持稳定.
    /// 驻留快照保留准备时的原纹理和导入事实, 注销 registry 不影响本批构图.
    /// 无普通可见 draw 时不声明几何上传和 Raster, 纹理候选由图活性裁剪.
    struct UiBatch
    {
        // MSVC Debug 会为空容器分配代理. 显式 count 构造允许准备入口捕获分配失败.
        UiBatch() : draws(0), residents(0)
        {
        }

        RHI::CommandListHandle recorded; ///< 完整录制使用的列表, 供 Finish 核对回执对象.
        Platform::WindowExtent extent;
        ProjectionBytes projection;
        std::span<std::byte> vertices;
        std::span<std::byte> indices;
        std::vector<UiDraw> draws;
        std::vector<UiResidentSnapshot> residents; ///< 本批普通可见 draw 的外部驻留快照, 按 token 去重.
        UiTextureUploads uploads;
        RHI::BufferHandle vertexBuffer;
        RHI::BufferHandle indexBuffer;
        std::uint64_t uploadsGeneration = 0; ///< 可早于 Raster 声明, 但必须消费同一图和帧槽绑定表.
        std::uint64_t graphGeneration = 0;
        bool hasDraws = false;
        bool usesDiagnosticOutput = false;
    };

    /// @brief 准备值的控制对象. owner 与 batch 一起解除后, 外部句柄可以安全地过期.
    struct UiFrameData
    {
        ~UiFrameData()
        {
            if (owner != nullptr)
            {
                owner->Complete(*this, nullptr);
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
                batch.draws.push_back({*clip, arguments, *binding, false, 0});
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
        : m_Device(device), m_Host(host), m_Textures(device)
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

    // 先验证宿主设备和固定 Shader 接口, 再向当前 ImGui context 发布能力及基础 atlas.
    UiRenderer::CreateResult UiRenderer::Create(Platform::DeviceHost &host, const UiRendererDesc &desc)
    {
        if (host.GetDevice() != &desc.device)
        {
            return Core::BoundedText("UI renderer requires the ready host device");
        }
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
        if (!batch.vertexBuffer || m_Host.HasFatalGraphicsError())
        {
            return false;
        }
        batch.indexBuffer = ReserveGeometry(m_Device, m_IndexBuffer, indexCapacity, false);
        if (!batch.indexBuffer || m_Host.HasFatalGraphicsError())
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

    // 从 View 的唯一原因转换占位标签, 不要求失败 View 提供纹理.
    // UI 只解释等待或硬拒绝, 不再把合成模式推导成等待状态回写 View.
    static UiFallback MissingViewportOutput(const Renderer::PreparedViewOutput &outputs) noexcept
    {
        switch (outputs.UnavailableReason())
        {
        case Renderer::ViewSettingReason::UploadPending: return UiFallback::UploadPending;
        case Renderer::ViewSettingReason::InvalidExtent: return UiFallback::InvalidExtent;
        case Renderer::ViewSettingReason::TextureSizeLimit: return UiFallback::TextureSizeLimit;
        case Renderer::ViewSettingReason::DiagnosticBudget: return UiFallback::DiagnosticBudget;
        case Renderer::ViewSettingReason::CameraUnavailable: return UiFallback::CameraUnavailable;
        case Renderer::ViewSettingReason::InvalidProjection: return UiFallback::InvalidProjection;
        case Renderer::ViewSettingReason::InvalidImageRect: return UiFallback::InvalidImageRect;
        case Renderer::ViewSettingReason::InvalidSampling: return UiFallback::InvalidSampling;
        case Renderer::ViewSettingReason::ScenePreparationFailed: return UiFallback::ScenePreparationFailed;
        case Renderer::ViewSettingReason::ScenePipelineUnavailable: return UiFallback::ScenePipelineUnavailable;
        case Renderer::ViewSettingReason::SceneResidencyPending: return UiFallback::SceneResidencyPending;
        case Renderer::ViewSettingReason::SceneResidencyLimit: return UiFallback::SceneResidencyLimit;
        case Renderer::ViewSettingReason::SceneAttachmentLimit: return UiFallback::SceneAttachmentLimit;
        case Renderer::ViewSettingReason::SceneUnsupported: return UiFallback::SceneUnsupported;
        case Renderer::ViewSettingReason::SceneUploadLimit: return UiFallback::SceneUploadLimit;
        default: return UiFallback::MissingOutput;
        }
    }

    // 先解析视口及状态标签, 再统一预留 arena 并复制数据. GPU 上传准入在编译后进行.
    PrepareUiResult UiRenderer::Prepare(const ImDrawData &drawData,
                                        Platform::WindowExtent extent, Core::FrameArena &arena,
                                        const Renderer::PreparedViewOutput &outputs, const ViewportImage &viewport,
                                        UiFrameMode mode)
    {
        if (m_Host.HasFatalGraphicsError())
        {
            return Core::BoundedText("Fatal graphics diagnostic prevents UI preparation");
        }
        if (m_Pending != nullptr)
        {
            return Core::BoundedText("Editor UI prepare requires the previous batch to be finished or cancelled");
        }
        if (extent.width == 0 || extent.height == 0 ||
            extent.width > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
            extent.height > static_cast<std::uint32_t>(std::numeric_limits<int>::max()))
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
            batch.extent = extent;
            batch.projection = std::bit_cast<ProjectionBytes>(projection);
            if (auto error = m_Textures.PlanUploads(drawData, batch.uploads))
            {
                return TexturePreparationError(*error, m_BaseFontReady);
            }

            UiFrameBindings bindings{.output = outputs.Color(),
                                     .missingView = mode == UiFrameMode::SafeUi ? UiFallback::FrameFailed :
                                                                                 MissingViewportOutput(outputs),
                                     .viewportDraw = viewport.FindDraw(drawData)};
            if (bindings.viewportDraw)
            {
                // 从封存图像命令读取实际身份. 有效但不属于该视口的 token 也不能绑定.
                const auto &ref = bindings.viewportDraw->TexRef;
                if (ref._TexData != nullptr || ref._TexID != viewport.token)
                {
                    bindings.viewportFallback = UiFallback::InvalidIdentity;
                }
                else if (!bindings.output)
                {
                    bindings.viewportFallback = bindings.missingView;
                }
                else if (mode == UiFrameMode::SafeUi)
                {
                    // 恢复标签与采样资格分开. 旧输出保持原对象及尺寸, 当前面板矩形只决定显示区域.
                    bindings.viewportFallback = UiFallback::RetainedOutput;
                }
            }

            std::span<ImDrawList *const> lists(drawData.CmdLists.Data, drawData.CmdLists.Size);
            // 字形已在布局时请求. 准备期把状态文字放入独立列表, 保持原布局命令的顺序.
            std::optional<ImDrawList> status;
            std::optional<std::vector<ImDrawList *>> composed;
            const auto addStatus = [&]
            {
                if (!bindings.viewportDraw)
                {
                    return true;
                }
                status.emplace(ImGui::GetDrawListSharedData());
                viewport.DrawStatus(*status, *bindings.viewportDraw, bindings.viewportFallback, bindings.output);
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

            // 所有加数来自有界几何/纹理预算. 发布第一段借用前一次预留, 包含准入的诊断像素.
            constexpr auto Padding = alignof(ImDrawVert) + alignof(ImDrawIdx) - 2;
            if (!arena.Reserve(arena.Used() + size->Bytes() + batch.uploads.bytes + outputs.UploadBytes() + Padding))
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
            // 所有 draw 校验后移交强引用. 后续缓冲准备失败仍由 frame 析构一并结束快照和 pending 关系.
            batch.residents = std::move(bindings.residents);
            batch.usesDiagnosticOutput = bindings.usesDiagnostic;
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

    /// @brief Raster 拥有绘制值和固定 RHI 对象, 回执地址借用活动批次, 诊断回调借用调用方作用域.
    struct UiRasterData
    {
        std::vector<UiDraw> draws = std::vector<UiDraw>(0);
        std::vector<UiFrameTexture> sampled = std::vector<UiFrameTexture>(0);
        Renderer::BufferId projection;
        Renderer::BufferId vertices;
        Renderer::BufferId indices;
        std::size_t vertexBytes = 0;
        std::size_t indexBytes = 0;
        RHI::GraphicsPipelineHandle pipeline;
        RHI::BindingLayoutHandle layout;
        RHI::SamplerHandle sampler;
        RHI::CommandListHandle *recorded = nullptr;
        Renderer::GraphExecutionDiagnostics diagnostics;
    };
    static_assert(sizeof(UiRasterData) + sizeof(void *) * 2 + Core::FrameArena::MaxAlignment <= 512);

    // 绑定条目来自当前 Pass 的精确读取权限, 采样去重顺序就是 execute 使用的集合索引.
    static Renderer::GraphFailure PrepareRaster(Renderer::PassPreparation &preparation, const UiRasterData &data)
    {
        for (const auto &sample : data.sampled)
        {
            std::vector<RHI::BindingSetItem> items(0);
            for (const auto &item : data.layout->GetDesc()->bindings)
            {
                std::optional<RHI::BindingSetItem> resource;
                switch (item.type)
                {
                case RHI::ResourceType::ConstantBuffer:
                    resource = preparation.ReadBuffer(item.slot, data.projection, Renderer::GraphUsage::ConstantBuffer,
                                                       RHI::BufferRange(0, sizeof(UiShader::UiProjection)));
                    break;
                case RHI::ResourceType::Texture_SRV:
                    resource = preparation.SampledTexture(item.slot, sample.texture, sample.range);
                    break;
                case RHI::ResourceType::Sampler:
                    resource = RHI::BindingSetItem::Sampler(item.slot, data.sampler.Get());
                    break;
                default:
                    assert(false && "Unexpected generated UI layout");
                    break;
                }
                if (!resource)
                {
                    return Renderer::GraphError{Renderer::GraphErrorCode::AccessDenied,
                        Core::BoundedText("UI graph binding preparation failed")};
                }
                items.push_back(*resource);
            }
            if (!preparation.CreateBindingSet(*data.layout, items))
            {
                return Renderer::GraphError{Renderer::GraphErrorCode::ExecutionFailed,
                    Core::BoundedText("UI graph binding creation failed")};
            }
        }
        return std::nullopt;
    }

    // 每条状态/绘制后观察同一错误区间. 状态拒绝不能继续沿用前一 draw 的候选.
    static Renderer::GraphFailure ExecuteRaster(Renderer::PassResources &resources, const UiRasterData &data,
                                                 RHI::CommandList &commands) noexcept
    {
        using namespace Renderer;
        auto *vertices = resources.Resolve(data.vertices, GraphUsage::VertexBuffer,
                                           RHI::BufferRange(0, data.vertexBytes));
        auto *indices = resources.Resolve(data.indices, GraphUsage::IndexBuffer, RHI::BufferRange(0, data.indexBytes));
        if (!vertices || !indices)
        {
            return std::nullopt;
        }
        RHI::GraphicsState state;
        state.SetPipeline(data.pipeline.Get()).SetFramebuffer(resources.Framebuffer())
            .AddVertexBuffer(RHI::VertexBufferBinding().SetBuffer(vertices))
            .SetIndexBuffer(RHI::IndexBufferBinding().SetBuffer(indices).SetFormat(IndexFormat));
        state.viewport.AddViewport(resources.Framebuffer()->GetFramebufferInfo().GetViewport());
        state.viewport.scissorRects.resize(1);
        state.bindings.resize(1);
        for (const auto &draw : data.draws)
        {
            if (draw.reset)
            {
                commands.ClearState();
            }
            else
            {
                auto *binding = resources.BindingSet(draw.binding);
                if (!binding)
                {
                    return std::nullopt;
                }
                state.bindings[0] = binding;
                state.viewport.scissorRects[0] = draw.clip;
                commands.SetGraphicsState(state);
                if (data.diagnostics.IsClear())
                {
                    commands.DrawIndexed(draw.arguments);
                }
            }
            if (!data.diagnostics.IsClear())
            {
                return GraphError{GraphErrorCode::ExecutionFailed,
                    Core::BoundedText("UI graph Raster stopped on RHI error")};
            }
        }
        *data.recorded = &commands;
        return std::nullopt;
    }

    // 纹理生产者已由公共准备值声明, 此处核对目标, 去重采样, 导出并追加几何上传和 Raster.
    // 场景路径可提前声明 UI 纹理, 保证所有纹理写先于场景及 UI 缓冲写; 不取得瞬态物理纹理.
    Renderer::GraphResult<Renderer::TextureId> UiRenderer::BuildGraph(UiBatch &batch,
        Renderer::RenderGraph &graph, Renderer::TextureId color, Renderer::TextureId diagnostic,
        FrameSlotUiFrameBindings &bindings, std::vector<Renderer::GraphUpload> &uploads,
        Renderer::GraphExecutionDiagnostics diagnostics)
    {
        using namespace Renderer;
        if (!color.IsValid() || color.identity.index >= graph.Textures().size())
        {
            return GraphError{GraphErrorCode::InvalidResource, Core::BoundedText("UI graph target is invalid")};
        }
        const auto &target = graph.Textures()[color.identity.index];
        if (target.desc.width != batch.extent.width || target.desc.height != batch.extent.height ||
            target.desc.dimension != RHI::TextureDimension::Texture2D || target.desc.sampleCount != 1 ||
            target.desc.format != TargetFormat || !target.desc.isRenderTarget || target.range != TextureRange{})
        {
            return GraphError{GraphErrorCode::IncompatibleAttachments,
                Core::BoundedText("UI graph requires the sealed frame's single-sample SRGBA8 2D target")};
        }
        // 一旦开始声明便消费该准备值的构图机会. 后续失败由调用方丢弃整张候选图并取消准备值.
        batch.graphGeneration = color.identity.generation;
        for (auto &draw : batch.draws)
        {
            if (draw.reset)
            {
                continue;
            }
            auto resolved = m_Textures.ResolveGraph(graph, draw.token, diagnostic, bindings, batch.residents);
            if (const auto *error = std::get_if<GraphError>(&resolved))
            {
                return *error;
            }
            const auto sample = std::get<UiFrameTexture>(resolved);
            auto found = std::ranges::find_if(bindings.sampled, [&](const auto &item)
            {
                return item.texture == sample.texture && item.range == sample.range;
            });
            draw.binding = static_cast<std::size_t>(found - bindings.sampled.begin());
            if (found == bindings.sampled.end())
            {
                bindings.sampled.push_back(sample);
            }
        }
        // 持久结果只为实际采样者导出. 未使用创建不会成为活性根.
        for (const auto &sample : bindings.sampled)
        {
            const bool created = std::ranges::any_of(batch.uploads.items, [&](const auto &upload)
            {
                return upload.created.IsValid() && upload.written.texture == sample.texture;
            });
            if (created || sample.texture == diagnostic || sample.texture == batch.uploads.placeholder.texture)
            {
                if (auto error = graph.Export(sample.texture, ExportUsage::ShaderRead, sample.range))
                {
                    return *error;
                }
            }
        }
        if (!batch.hasDraws)
        {
            // 空 UI 沿用已初始化颜色, 不制造几何依赖或通过副作用保留未使用上传.
            return color;
        }
        UiRasterData raster;
        raster.draws = batch.draws;
        raster.sampled = bindings.sampled;
        raster.vertexBytes = batch.vertices.size();
        raster.indexBytes = batch.indices.size();
        raster.pipeline = m_Pipeline;
        raster.layout = m_Textures.GetLayout();
        raster.sampler = m_Textures.GetSampler();
        raster.recorded = std::addressof(batch.recorded);
        raster.diagnostics = diagnostics;
        const std::array<RHI::BufferHandle, 3> buffers{RHI::BufferHandle(m_Textures.GetProjectionBuffer()),
                                                       batch.vertexBuffer, batch.indexBuffer};
        const std::array<std::span<const std::byte>, 3> bytes{batch.projection, batch.vertices, batch.indices};
        const std::array ids{&raster.projection, &raster.vertices, &raster.indices};
        // 纹理上传都已声明. 产品拓扑中这些 Buffer 写入位于其后, 与设备容量预检的分组次序一致.
        for (std::size_t index = 0; index < buffers.size(); ++index)
        {
            auto imported = graph.ImportBuffer(buffers[index],
                {RHI::ResourceStates::Common, RHI::ResourceStates::Common, false});
            if (const auto *error = std::get_if<GraphError>(&imported))
            {
                return *error;
            }
            auto written = AddBufferUpload(graph, std::get<BufferId>(imported), bytes[index], uploads);
            if (const auto *error = std::get_if<GraphError>(&written))
            {
                return *error;
            }
            *ids[index] = std::get<BufferId>(written);
        }
        TextureId composed;
        auto pass = graph.AddPass<UiRasterData>("Editor UI Raster", [&](PassBuilder &builder, auto &data)
        {
            data = std::move(raster);
            for (const auto &sample : data.sampled)
            {
                if (!builder.Read(sample.texture, GraphUsage::Sampled, sample.range))
                {
                    return false;
                }
            }
            composed = builder.Write(color, GraphUsage::ColorAttachment, {.load = LoadOp::Load});
            return composed.IsValid() &&
                builder.Read(data.projection, GraphUsage::ConstantBuffer,
                             RHI::BufferRange(0, sizeof(UiShader::UiProjection))) &&
                builder.Read(data.vertices, GraphUsage::VertexBuffer, RHI::BufferRange(0, data.vertexBytes)) &&
                builder.Read(data.indices, GraphUsage::IndexBuffer, RHI::BufferRange(0, data.indexBytes));
        }, ExecuteRaster, PrepareRaster);
        if (const auto *error = std::get_if<GraphError>(&pass))
        {
            return *error;
        }
        return composed;
    }

    // 先处理借用的纹理请求, 再解除批次关系并释放准备数据与 CPU 持有的 RHI 引用.
    // 取消不会回滚已发像素写入, 也不会确认请求; 下一次 Prepare 从仍 pending 的元数据重新复制载荷.
    void UiRenderer::Complete(UiFrameData &frame, const UiSubmission *submission) noexcept
    {
        assert(m_Pending == &frame && frame.batch);
        m_Textures.Finish(frame.batch->uploads, submission);
        const auto *atlas = ImGui::GetIO().Fonts->TexData;
        m_BaseFontReady |= atlas && atlas->Status == ImTextureStatus_OK && m_Textures.HasManagedTexture(*atlas);
        m_Pending = nullptr;
        frame.owner = nullptr;
        frame.batch.reset();
    }

    void UiRenderer::CancelPending() noexcept
    {
        if (m_Pending != nullptr)
        {
            Complete(*m_Pending, nullptr);
        }
    }

    UiTextureResult UiRenderer::RegisterResident(RHI::Texture &texture, UiTextureImport imported)
    {
        return m_Textures.RegisterResident(texture, imported);
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

    bool PreparedUiFrame::UsesViewOutput() const noexcept
    {
        return m_Data && m_Data->batch && m_Data->batch->usesDiagnosticOutput;
    }

    std::size_t PreparedUiFrame::GraphStorageBytes() const noexcept
    {
        // 每个请求最多两条上传/复制, 其余为占位, 三个缓冲及 Raster. 每条 recipe 上界 512 字节.
        return m_Data && m_Data->batch ? (m_Data->batch->uploads.items.size() * 2 + 6) * 512 : 0;
    }

    Renderer::GraphFailure PreparedUiFrame::DeclareTextureUploads(Renderer::RenderGraph &graph,
        Renderer::TextureId target, FrameSlotUiFrameBindings &bindings, std::vector<Renderer::GraphUpload> &uploads)
    try
    {
        if (!target.IsValid() || !m_Data || !m_Data->owner || m_Data->batch->uploadsGeneration ||
            bindings.graphGeneration != 0)
        {
            return Renderer::GraphError{Renderer::GraphErrorCode::InvalidPhase,
                Core::BoundedText("UI texture declaration requires a live preparation and empty frame bindings")};
        }
        auto &batch = *m_Data->batch;
        batch.uploadsGeneration = bindings.graphGeneration = target.identity.generation;
        return m_Data->owner->m_Textures.DeclareUploads(graph, batch.uploads, bindings, uploads);
    }
    catch (const std::bad_alloc &)
    {
        return Renderer::GraphError{Renderer::GraphErrorCode::OutOfStorage,
            Core::BoundedText("UI texture declaration ran out of CPU storage")};
    }

    // 活动准备值只构图一次. 错误转换为有界诊断, 但已建立的图声明仍由候选图析构收拢.
    Renderer::GraphResult<Renderer::TextureId> PreparedUiFrame::BuildGraph(
        Renderer::RenderGraph &graph, Renderer::TextureId color, Renderer::TextureId diagnostic,
        FrameSlotUiFrameBindings &bindings, std::vector<Renderer::GraphUpload> &uploads,
        Renderer::GraphExecutionDiagnostics diagnostics)
    {
        if (!m_Data || !m_Data->owner || m_Data->batch->graphGeneration != 0)
        {
            return Renderer::GraphError{Renderer::GraphErrorCode::InvalidPhase,
                Core::BoundedText("UI graph build requires a live unused preparation")};
        }
        if (m_Data->batch->uploadsGeneration == 0)
        {
            if (auto failure = DeclareTextureUploads(graph, color, bindings, uploads))
            {
                return *failure;
            }
        }
        if (m_Data->batch->uploadsGeneration != color.identity.generation ||
            bindings.graphGeneration != color.identity.generation)
        {
            return Renderer::GraphError{Renderer::GraphErrorCode::InvalidPhase,
                Core::BoundedText("UI raster and texture uploads must belong to the same graph")};
        }
        try
        {
            return m_Data->owner->BuildGraph(*m_Data->batch, graph, color, diagnostic, bindings, uploads, diagnostics);
        }
        catch (const std::bad_alloc &)
        {
            return Renderer::GraphError{Renderer::GraphErrorCode::OutOfStorage,
                Core::BoundedText("UI graph build ran out of CPU storage")};
        }
    }

    // 图代阻止跨图确认, Raster 写回的列表阻止错配录制. 票据与同次提交的关联由调用者保证.
    // 无论回执是否接受都结束 CPU 协议借用, GPU 引用仍由已发包持有.
    bool PreparedUiFrame::Finish(const UiSubmission &submission) noexcept
    {
        if (!m_Data || m_Data->owner == nullptr)
        {
            return false;
        }
        const auto &batch = *m_Data->batch;
        const bool accepted = batch.graphGeneration != 0 && batch.graphGeneration == submission.plan.graphGeneration &&
            (!batch.hasDraws || batch.recorded.Get() == &submission.commands) && submission.outputs &&
            submission.executedWithoutErrors && submission.ticket != 0;
        m_Data->owner->Complete(*m_Data, accepted ? &submission : nullptr);
        m_Data.reset();
        return accepted;
    }

    void FrameSlotUiFrameBindings::Reset() noexcept
    {
        graphGeneration = 0;
        tokens.clear();
        sampled.clear();
    }

    const UiFrameTexture *FrameSlotUiFrameBindings::Find(std::uint64_t generation, std::size_t index) const noexcept
    {
        return generation != 0 && generation == graphGeneration && index < sampled.size() ? &sampled[index] : nullptr;
    }

    void PreparedUiFrame::Cancel() noexcept
    {
        m_Data.reset();
    }
}
