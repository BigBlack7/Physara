#include "Forward.hpp"

/// @file
/// @brief 从当前主 View 已选的表面版本生成普通 UI 颜色. 原生角色随帧前 Shader 代准备.

#include <algorithm>
#include <bit>
#include <cassert>
#include <utility>

#include <SurfacePreview/Bindings.hpp>

#include "../Materials/StaticGpu.hpp"
#include "../RenderGraph/GraphResourcePool.hpp"

namespace Physara::Renderer
{
    namespace PreviewAbi = Shader::Generated::SurfacePreview;
    using Shader::ProductShader;

    static_assert(std::uint32_t(SurfacePreviewMode::BaseColor) == PreviewAbi::PreviewBaseColor &&
                  std::uint32_t(SurfacePreviewMode::Occlusion) == PreviewAbi::PreviewOcclusion &&
                  std::uint32_t(SurfacePreviewMode::Normal) == PreviewAbi::PreviewNormal &&
                  std::uint32_t(SurfacePreviewMode::Roughness) == PreviewAbi::PreviewRoughness &&
                  std::uint32_t(SurfacePreviewMode::Metallic) == PreviewAbi::PreviewMetallic &&
                  std::uint32_t(SurfacePreviewMode::Emission) == PreviewAbi::PreviewEmission &&
                  std::uint32_t(SurfacePreviewMode::Route) == PreviewAbi::PreviewRoute);

    // 深度直接读取 D32. 其余七个通道共用混合类型的片元程序, 通道号通过常量传入.
    static Detail::ShaderPipelineRequest PreviewKey(SurfacePreviewMode mode) noexcept
    {
        return {mode == SurfacePreviewMode::Depth ? ProductShader::SurfaceDepthPreview :
                                                    ProductShader::SurfaceGBufferPreview, 0};
    }

    void ForwardRenderer::RequestSurfacePreview(SurfacePreviewMode mode) noexcept
    {
        if (m_Resources && mode <= SurfacePreviewMode::Route)
        {
            m_Resources->shaderDemand.Add(PreviewKey(mode), false);
        }
    }

    std::variant<PreparedSurfacePreview, ForwardFailure> ForwardRenderer::PrepareSurfacePreview(
        const ForwardFrame &frame, const PreparedView &view, SurfacePreviewMode mode) const noexcept
    {
        assert(frame.m_Data && frame.m_Data->shaders && m_Resources);
        // 选路已结束. 请求尚未切换时, 只有本轮实际准备的主 View 能决定可读的表面.
        const auto &prepared = frame.m_Data->views.front();
        if (mode > SurfacePreviewMode::Route || !IsLitPreview(prepared.source.mode) ||
            prepared.source.directionalCascades != 0 || prepared.source.localShadow)
        {
            return ForwardFailure{ViewSettingReason::SceneOutputUnavailable,
                                  Core::BoundedText("Surface preview requires the current lit View")};
        }
        if (mode != SurfacePreviewMode::Depth && !prepared.needs.deferredLighting)
        {
            return ForwardFailure{ViewSettingReason::SceneOutputUnavailable,
                                  Core::BoundedText("Current path has no Deferred GBuffer")};
        }
        const auto display = ResolveDisplaySettings(prepared.source.mode, view.requestedAa);
        if (const auto failure = CheckAttachments(view.layouts.render.outputExtent, view.layouts.display.outputExtent,
            display, view.layouts.render != view.layouts.display, prepared.needs, 1, true))
        {
            return ForwardFailure{failure->reason, Core::BoundedText("Surface preview exceeds the View attachment budget")};
        }
        if (mode == SurfacePreviewMode::Depth &&
            (m_Device.QueryFormatSupport(Detail::DepthFormat) & RHI::FormatSupport::ShaderSample) == 0)
        {
            return ForwardFailure{ViewSettingReason::SceneAttachmentLimit,
                                  Core::BoundedText("Device cannot sample the main D32 surface")};
        }
        const auto key = PreviewKey(mode);
        auto pipeline = frame.m_Data->shaders->programs.FindPipeline(key);
        if (!pipeline)
        {
            return m_Resources->CheckSwitchShader(key).value_or(ForwardFailure{
                ViewSettingReason::ShaderPending, Core::BoundedText("Waiting for the frame's surface preview program")});
        }
        return PreparedSurfacePreview(mode, std::move(pipeline));
    }

    PreparedSurfacePreview::PreparedSurfacePreview(SurfacePreviewMode mode, RHI::GraphicsPipelineHandle pipeline) noexcept
        : m_Mode(mode), m_Pipeline(std::move(pipeline))
    {
        // 路由值来自生产者的 ABI, 避免两个生成接口各自声明相同的位型.
        const PreviewAbi::PreviewControls controls{{std::uint32_t(mode), Detail::StaticAbi::CoreDeferredEligible,
            Detail::StaticAbi::SurfaceUnlit, Detail::StaticAbi::SurfaceForwardExtension}};
        m_Controls = std::bit_cast<decltype(m_Controls)>(controls);
    }

    GraphPoolCapacity PreparedSurfacePreview::StorageDemand(ViewExtent extent) const noexcept
    {
        const auto bytes = UploadBytes();
        return {std::uint64_t(extent.width) * extent.height * RHI::GetFormatInfo(Detail::DisplayFormat).bytesPerBlock +
                bytes, bytes ? 2u : 1u};
    }

    std::size_t PreparedSurfacePreview::UploadBytes() const noexcept
    {
        return m_Mode == SurfacePreviewMode::Depth ? 0 : m_Controls.size();
    }

    /// @brief Pass 只借用同一提交的已准备 Pipeline, 读取的每个图版本由声明授权.
    struct PreviewRaster
    {
        TextureId depth;
        std::array<TextureId, CoreGBufferOutput::Formats.size()> colors{};
        BufferId controls;
        ViewImageRect rect;
        RHI::GraphicsPipeline *pipeline{};
    };

    static constexpr TextureRange PreviewDepthRange{.aspect = TextureAspect::Depth};

    // 绑定与片元配方一致的采样集合. 解析或建组失败由 PassPreparation 锁存, 执行器在绘制前检查.
    static GraphFailure BindPreview(PassPreparation &preparation, const PreviewRaster &data)
    {
        auto &layout = *data.pipeline->GetDesc().bindingLayouts.front();
        if (data.depth.IsValid())
        {
            if (const auto depth = preparation.SampledTexture(Detail::PreviewDepthSlot, data.depth, PreviewDepthRange))
            {
                static_cast<void>(preparation.CreateBindingSet(layout, std::span(&*depth, 1)));
            }
            return {};
        }
        const auto params = preparation.ReadBuffer(Detail::PreviewControlsSlot, data.controls, GraphUsage::ConstantBuffer);
        if (!params)
        {
            return {};
        }
        std::array<RHI::BindingSetItem, CoreGBufferOutput::Formats.size() + 1> items;
        items.back() = *params;
        for (std::uint32_t i = 0; i < data.colors.size(); ++i)
        {
            const auto color = preparation.SampledTexture(Detail::PreviewGBufferSlot + i, data.colors[i]);
            if (!color)
            {
                return {};
            }
            items[i] = *color;
        }
        static_cast<void>(preparation.CreateBindingSet(layout, items));
        return {};
    }

    GraphResult<TextureId> PreparedSurfacePreview::Declare(RenderGraph &graph, const ForwardOutput &source,
        std::vector<GraphUpload> &uploads) const
    {
        PreviewRaster inputs{.rect = source.source.layout.rect, .pipeline = m_Pipeline.Get()};
        if (m_Mode == SurfacePreviewMode::Depth)
        {
            inputs.depth = source.depth;
        }
        else
        {
            assert(source.coreGBuffer && source.coreGBuffer->depth == source.depth);
            inputs.colors = source.coreGBuffer->colors;
            const auto buffer = graph.CreateBuffer({.byteSize = m_Controls.size(), .debugName = "Surface preview controls",
                .isConstantBuffer = true});
            if (const auto *error = std::get_if<GraphError>(&buffer))
            {
                return *error;
            }
            const auto written = AddBufferUpload(graph, std::get<BufferId>(buffer), m_Controls, uploads);
            if (const auto *error = std::get_if<GraphError>(&written))
            {
                return *error;
            }
            inputs.controls = std::get<BufferId>(written);
        }

        // 目标和源共用渲染画布. 整张清背景后只绘制有效矩形, 片元按同一像素坐标取源 texel.
        const auto extent = source.source.layout.outputExtent;
        const auto target = graph.CreateTexture({.width = extent.width, .height = extent.height,
            .format = Detail::DisplayFormat, .debugName = "Surface preview color", .isRenderTarget = true});
        if (const auto *error = std::get_if<GraphError>(&target))
        {
            return *error;
        }
        TextureId result;
        const auto pass = graph.AddPass<PreviewRaster>("Surface preview", [&](PassBuilder &builder, auto &data)
        {
            data = inputs;
            if (data.depth.IsValid())
            {
                if (!builder.Read(data.depth, GraphUsage::Sampled, PreviewDepthRange))
                {
                    return false;
                }
            }
            else if (!builder.Read(data.controls, GraphUsage::ConstantBuffer) ||
                !std::ranges::all_of(data.colors, [&](auto color) { return builder.Read(color, GraphUsage::Sampled); }))
            {
                return false;
            }
            result = builder.Write(std::get<TextureId>(target), GraphUsage::ColorAttachment,
                {.load = LoadOp::Clear, .clearValue = RHI::Color(0.018f, 0.018f, 0.018f, 1.f)});
            return result.IsValid();
        }, +[](PassResources &resources, const PreviewRaster &data, RHI::CommandList &commands) noexcept -> GraphFailure
        {
            const auto &rect = data.rect;
            RHI::GraphicsState state;
            state.SetPipeline(data.pipeline).SetFramebuffer(resources.Framebuffer()).AddBindingSet(resources.BindingSet(0));
            state.viewport.AddViewportAndScissorRect(RHI::Viewport(float(rect.x), float(rect.x + rect.extent.width),
                float(rect.y), float(rect.y + rect.extent.height), 0.f, 1.f));
            commands.SetGraphicsState(state);
            commands.Draw(RHI::DrawArguments().SetVertexCount(3).SetInstanceCount(1));
            return {};
        }, BindPreview);
        if (const auto *error = std::get_if<GraphError>(&pass))
        {
            return *error;
        }
        return result;
    }
}

namespace Physara::Renderer::Detail
{
    void ForwardPrograms::PrepareSurfacePreview(ForwardDevice &owner, ShaderPipelineRequest key,
                                                 const Shader::ProductPackages &packages)
    {
        const auto index = SurfacePreviewIndex(key.role);
        auto &pipeline = pipelines[SurfacePreviewPipelineBase + index];
        assert(!pipeline && key.variant == 0);
        auto loaded = LoadProductProgram(owner.device, key.role, owner.packagePaths.surfacePreviews[index],
            packages, PreviewAbi::AbiHash);
        if (const auto *failure = std::get_if<ProductProgramFailure>(&loaded))
        {
            RejectProductProgram(Shader::FindProductProgram(key.role)->name, *failure);
        }
        auto shaders = std::get<Shader::RhiProgram>(std::move(loaded));
        // 生成接口同时描述两种角色. 只保留当前程序使用的绑定, 常量 b0 和深度 t0 属于不同资源类型.
        auto previewLayouts = PreviewAbi::MakeBindingLayouts();
        auto &layout = previewLayouts.front();
        const bool depthOnly = key.role == ProductShader::SurfaceDepthPreview;
        std::erase_if(layout.bindings, [depthOnly](const auto &binding)
        {
            return (binding.type == RHI::ResourceType::Texture_SRV && binding.slot == PreviewDepthSlot) != depthOnly;
        });
        auto binding = owner.device.CreateBindingLayout(layout);
        if (!binding)
        {
            Reject(ViewSettingReason::ScenePipelineUnavailable, "Cannot create the surface preview layout");
        }
        RHI::GraphicsPipelineDesc desc;
        desc.SetVertexShader(shaders.vertex.Get()).SetFragmentShader(shaders.fragment.Get()).AddBindingLayout(binding.Get());
        desc.renderState.depthStencilState.DisableDepthTest().DisableDepthWrite().DisableStencil();
        desc.renderState.rasterState.SetCullMode(RHI::RasterCullMode::None).EnableScissor();
        auto ready = owner.device.CreateGraphicsPipeline(desc, RHI::FramebufferInfo().AddColorFormat(DisplayFormat));
        if (!ready)
        {
            Reject(ViewSettingReason::ScenePipelineUnavailable, "Cannot link the surface preview pipeline");
        }
        surfacePreviews[index] = std::move(shaders);
        pipeline = std::move(ready);
    }
}
