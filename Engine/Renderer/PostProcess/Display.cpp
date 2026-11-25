#include "Display.hpp"

/// @file
/// @brief 编排颜色分级、抗锯齿及缩放, 只由本图最终消费者决定其活性.

#include <algorithm>
#include <cassert>
#include <new>
#include <utility>

#include <DisplayPost/Bindings.hpp>
#include <ForwardDisplay/Bindings.hpp>

namespace Physara::Renderer::PostProcess
{
    static constexpr auto LutBoundary = RHI::ResourceStates::Common;

    DisplayDevice::DisplayDevice(RHI::Device &device, std::array<std::filesystem::path, 4> paths)
        : m_Device(device), m_Paths(std::move(paths))
    {
    }

    using Shader::ProductShader;
    static constexpr std::array DisplayRoles{ProductShader::ForwardDisplay, ProductShader::DisplayColorGrading,
        ProductShader::DisplayFxaa, ProductShader::DisplayScale};
    static constexpr std::array<std::string_view, 4> DisplayNames{
        "Forward display clamp", "Display color grading", "Display FXAA", "Display scale"};

    bool DisplayDevice::IsRole(ProductShader role) noexcept
    {
        return std::ranges::find(DisplayRoles, role) != DisplayRoles.end();
    }

    std::size_t DisplayDevice::ProgramIndex(ProductShader role) noexcept
    {
        const auto index = std::ranges::find(DisplayRoles, role);
        assert(index != DisplayRoles.end());
        return static_cast<std::size_t>(index - DisplayRoles.begin());
    }

    DisplayDevice::Requests DisplayDevice::SelectStages(const ViewLayouts &layouts, DisplaySettings settings) noexcept
    {
        const bool physical = settings.profile == DisplayProfile::AcesSdr;
        const bool fxaa = settings.antiAliasing == AntiAliasing::Fxaa;
        const bool scale = layouts.render != layouts.display;
        Requests result{};
        result.keys[result.count++] = {physical ? ProductShader::DisplayColorGrading : ProductShader::ForwardDisplay,
                                      std::size_t(!fxaa && !scale)};
        if (fxaa)
        {
            result.keys[result.count++] = {ProductShader::DisplayFxaa, std::size_t(!scale)};
        }
        if (scale)
        {
            result.keys[result.count++] = {ProductShader::DisplayScale, 1};
        }
        return result;
    }

    // 阶段由包内逻辑入口辨别. FXAA 和缩放的活动槽相同, 仍各自匹配源身份.
    void DisplayDevice::PrepareStage(ProgramTable &programs, Detail::ShaderPipelineRequest key,
                                     const Shader::ProductPackages &packages)
    {
        static constexpr std::array<std::string_view, 4> Grade{
            "displayRects", "sourceColor", "colorLut", "linearSampler"};
        static constexpr std::array<std::string_view, 3> Filter{"displayRects", "sourceColor", "linearSampler"};
        namespace ClampAbi = Shader::Generated::ForwardDisplay;
        const auto index = ProgramIndex(key.role);
        const auto stage = static_cast<Stage>(index);
        auto &program = programs[index];
        auto &binding = m_Layouts[index];
        if (!program.shaders.vertex)
        {
            const Shader::ExpectedActiveResources resources{Shader::Stage::Fragment, 0,
                stage == Stage::Grade ? std::span<const std::string_view>(Grade) :
                                        std::span<const std::string_view>(Filter)};
            const auto expected = stage == Stage::Clamp ? std::span<const Shader::ExpectedActiveResources>{} :
                std::span(&resources, 1);
            auto shaders = Detail::LoadProductProgram(m_Device, key.role, m_Paths[index], packages,
                stage == Stage::Clamp ? ClampAbi::AbiHash : Abi::AbiHash, expected);
            if (const auto *error = std::get_if<Detail::ProductProgramFailure>(&shaders))
            {
                // 线性与 sRGB 目标使用同一阶段包. 将加载首因连同阶段名称交给帧前准备器.
                Detail::RejectProductProgram(DisplayNames[index], *error);
            }
            if (!binding)
            {
                // 已验证的角色配方固定, 所有代共用同一个布局对象供实际 BindSet 使用.
                auto layout = stage == Stage::Clamp ? ClampAbi::MakeBindingLayouts().front() :
                                                      Abi::MakeBindingLayouts().front();
                if (stage == Stage::Fxaa || stage == Stage::Scale)
                {
                    std::erase_if(layout.bindings, [](const auto &item)
                    {
                        return item.type == RHI::ResourceType::Texture_SRV && item.slot == 1;
                    });
                }
                binding = m_Device.CreateBindingLayout(layout);
                if (!binding)
                {
                    throw Core::BoundedText("Cannot create display binding layout");
                }
            }
            program.shaders = std::get<Shader::RhiProgram>(std::move(shaders));
        }
        // 同一角色的两种目标共用 Shader 和绑定布局, 只有附件格式不同的 Pipeline 要分别链接.
        auto &pipeline = program.variants[key.variant];
        if (!pipeline)
        {
            const auto format = key.variant ? OutputFormat : LinearFormat;
            RHI::GraphicsPipelineDesc desc;
            desc.SetVertexShader(program.shaders.vertex.Get()).SetFragmentShader(program.shaders.fragment.Get())
                .AddBindingLayout(binding.Get());
            desc.renderState.depthStencilState.DisableDepthTest().DisableDepthWrite().DisableStencil();
            desc.renderState.rasterState.SetCullMode(RHI::RasterCullMode::None);
            pipeline = m_Device.CreateGraphicsPipeline(desc, RHI::FramebufferInfo().AddColorFormat(format));
            if (!pipeline)
            {
                Core::BoundedText issue("Cannot link ");
                issue.Append(DisplayNames[index]);
                throw issue;
            }
        }
    }

    // 单一固定表的候选可在原字节上重试. 共享纹理以同一 Graphics 序列进出 Common.
    void DisplayDevice::PrepareLut()
    {
        if (m_Lut)
        {
            return;
        }
        auto entry = std::make_shared<ResidentLut>();
        entry->texture = m_Device.CreateTexture({.width = LutAtlasEdge, .height = LutAtlasEdge,
            .format = LinearFormat, .debugName = "Fixed ACES SDR LUT",
            .initialState = LutBoundary, .keepInitialState = true});
        if (!entry->texture)
        {
            throw Core::BoundedText("Device rejected the fixed ACES LUT texture");
        }
        auto pixels = BakeColorLut();
        if (pixels.empty())
        {
            throw Core::BoundedText("Fixed ACES LUT produced non-finite color");
        }
        entry->pendingPixels = std::make_shared<const std::vector<LutTexel>>(std::move(pixels));
        m_Lut = std::move(entry);
    }

    std::optional<Core::BoundedText> DisplayDevice::PrepareInputs(DisplayFrame &frame, const ViewLayouts &layouts,
                                                                 DisplaySettings settings)
    try
    {
        const bool physical = settings.profile == DisplayProfile::AcesSdr;
        const bool fxaa = settings.antiAliasing == AntiAliasing::Fxaa;
        const bool scale = layouts.render != layouts.display;
        DisplayFrame candidate;
        candidate.m_Layouts = layouts;
        if (physical || scale)
        {
            if (!m_Sampler)
            {
                m_Sampler = m_Device.CreateSampler(
                    RHI::SamplerDesc().SetAllFilters(true).SetAllAddressModes(RHI::SamplerAddressMode::Clamp));
                if (!m_Sampler)
                {
                    return Core::BoundedText("Device rejected the display linear sampler");
                }
            }
            const auto &render = layouts.render.rect;
            const auto &output = layouts.display.rect;
            candidate.m_Sampler = m_Sampler;
            candidate.m_Rects.renderRect = {render.x, render.y, render.extent.width, render.extent.height};
            candidate.m_Rects.outputRect = {output.x, output.y, output.extent.width, output.extent.height};
            candidate.m_Rects.renderExtent = {layouts.render.outputExtent.width, layouts.render.outputExtent.height};
            candidate.m_Rects.encodeLuma = fxaa ? 1u : 0u;
        }
        if (physical)
        {
            PrepareLut();
            candidate.m_Lut = m_Lut;
            candidate.m_LutPixels = m_Lut->pendingPixels;
        }
        frame = std::move(candidate);
        return std::nullopt;
    }
    catch (const Core::BoundedText &error)
    {
        return error;
    }
    catch (const std::bad_alloc &)
    {
        return Core::BoundedText("Display preparation allocation failed");
    }

    void DisplayDevice::BindStages(DisplayFrame &frame, const ProgramTable &programs,
                                   DisplaySettings settings) const noexcept
    {
        assert(frame.m_StageCount == 0);
        const auto requests = SelectStages(frame.m_Layouts, settings);
        for (const auto key : requests.Stages())
        {
            const auto index = ProgramIndex(key.role);
            const auto &pipeline = programs[index].variants[key.variant];
            assert(pipeline && m_Layouts[index]);
            frame.m_Stages[frame.m_StageCount++] = {DisplayNames[index], m_Layouts[index], pipeline};
        }
    }

    std::size_t DisplayFrame::UploadBytes() const noexcept
    {
        return LutUploadBytes() + (m_Sampler ? sizeof(m_Rects) : 0);
    }

    std::size_t DisplayFrame::UploadCount() const noexcept
    {
        return std::size_t(bool(m_LutPixels)) + std::size_t(bool(m_Sampler));
    }

    std::size_t DisplayFrame::LutUploadBytes() const noexcept
    {
        return m_LutPixels ? m_LutPixels->size() * sizeof(LutTexel) : 0;
    }

    /// @brief 在候选声明内传递首个图错误, 调用者放弃部分声明的图.
    template <typename T>
    static T TakeGraph(GraphResult<T> result)
    {
        if (const auto *error = std::get_if<GraphError>(&result))
        {
            throw *error;
        }
        return std::get<T>(std::move(result));
    }

    GraphResult<TextureId> DisplayFrame::DeclareLut(RenderGraph &graph, std::vector<GraphUpload> &uploads) const
    try
    {
        if (!m_Lut)
        {
            return TextureId{};
        }
        // 本候选持有待写字节时, 两路都读取下面这一笔写出的图版本. 新帧在确认后可直接导入.
        const auto version = TakeGraph(graph.ImportTexture(m_Lut->texture, {LutBoundary, LutBoundary, !m_LutPixels}));
        if (!m_LutPixels)
        {
            return version;
        }
        return TakeGraph(AddTextureUpload(graph, version, std::as_bytes(std::span(*m_LutPixels)),
            LutAtlasEdge * sizeof(LutTexel), uploads)).texture;
    }
    catch (const GraphError &failure)
    {
        return failure;
    }

    /// @brief 本图授权的源与常量, 设备句柄由 DisplayFrame 持有至提交后安全复用.
    struct PassData
    {
        TextureId input;
        TextureId lut;
        BufferId rects;
        RHI::Sampler *sampler;
        RHI::BindingLayout *layout;
        RHI::GraphicsPipeline *pipeline;
    };

    /// @brief 在同一图内为实际阶段绑定已定义的输入范围, 并完整写入目标画布.
    static TextureId AddDisplayPass(RenderGraph &graph, const DisplayStage &stage, PassData data, TextureId target)
    {
        TextureId written;
        static_cast<void>(TakeGraph(graph.AddPass<PassData>(stage.name,
            [&](PassBuilder &builder, PassData &pass)
            {
                pass = data;
                written = builder.Write(target, GraphUsage::ColorAttachment,
                    {.load = LoadOp::Discard, .coverage = WriteCoverage::Full});
                return written.IsValid() && builder.Read(data.input, GraphUsage::Sampled) &&
                    (!data.rects.IsValid() || builder.Read(data.rects, GraphUsage::ConstantBuffer)) &&
                    (!data.lut.IsValid() || builder.Read(data.lut, GraphUsage::Sampled));
            }, +[](PassResources &resources, const PassData &pass, RHI::CommandList &commands) noexcept -> GraphFailure
            {
                auto *framebuffer = resources.Framebuffer();
                RHI::GraphicsState state;
                state.SetFramebuffer(framebuffer).SetPipeline(pass.pipeline).AddBindingSet(resources.BindingSet(0));
                state.viewport.AddViewport(framebuffer->GetFramebufferInfo().GetViewport());
                commands.SetGraphicsState(state);
                commands.Draw(RHI::DrawArguments().SetVertexCount(3).SetInstanceCount(1));
                return std::nullopt;
            }, +[](PassPreparation &preparation, const PassData &pass) -> GraphFailure
            {
                const auto source = preparation.SampledTexture(0, pass.input);
                if (!source)
                {
                    return std::nullopt;
                }
                if (!pass.rects.IsValid())
                {
                    static_cast<void>(preparation.CreateBindingSet(*pass.layout, std::span(&*source, 1)));
                    return std::nullopt;
                }
                const auto rects = preparation.ReadBuffer(0, pass.rects, GraphUsage::ConstantBuffer);
                if (!rects)
                {
                    return std::nullopt;
                }
                std::array<RHI::BindingSetItem, 4> items{*source, *rects,
                    RHI::BindingSetItem::Sampler(0, pass.sampler)};
                std::size_t count = 3;
                if (pass.lut.IsValid())
                {
                    const auto lut = preparation.SampledTexture(1, pass.lut);
                    if (!lut)
                    {
                        return std::nullopt;
                    }
                    items[count++] = *lut;
                }
                static_cast<void>(preparation.CreateBindingSet(*pass.layout, std::span(items.data(), count)));
                return std::nullopt;
            })));
        return written;
    }

    GraphResult<TextureId> DisplayFrame::Declare(RenderGraph &graph, std::vector<GraphUpload> &uploads,
                                                 TextureId working, TextureId lut) const
    try
    {
        assert(m_StageCount != 0);
        BufferId rects;
        if (m_Sampler)
        {
            const auto target = TakeGraph(graph.CreateBuffer({.byteSize = sizeof(m_Rects),
                .debugName = "Display rectangles", .isConstantBuffer = true}));
            rects = TakeGraph(AddBufferUpload(graph, target, std::as_bytes(std::span(&m_Rects, 1)), uploads));
        }
        // 前一阶段的写版本是后一阶段唯一的颜色输入. 只有末级改用显示画布和 sRGB 附件.
        TextureId current = working;
        for (std::size_t index = 0; index < m_StageCount; ++index)
        {
            const auto &stage = m_Stages[index];
            const bool final = index + 1 == m_StageCount;
            const auto extent = final ? m_Layouts.display.outputExtent : m_Layouts.render.outputExtent;
            const auto target = TakeGraph(graph.CreateTexture({.width = extent.width, .height = extent.height,
                .format = final ? OutputFormat : LinearFormat,
                .debugName = final ? "Forward viewport color" : "Display linear intermediate", .isRenderTarget = true,
                .initialState = final ? RHI::ResourceStates::ShaderResource : RHI::ResourceStates::Unknown,
                .keepInitialState = final}));
            const bool clamp = index == 0 && !m_Lut;
            current = AddDisplayPass(graph, stage, {current, index == 0 ? lut : TextureId{},
                clamp ? BufferId{} : rects, m_Sampler.Get(), stage.layout.Get(), stage.pipeline.Get()}, target);
        }
        return current;
    }
    catch (const GraphError &failure)
    {
        return failure;
    }

    void DisplayFrame::Confirm() noexcept
    {
        if (m_LutPixels)
        {
            // 只释放设备的待写引用. 已准备槽仍各自拥有其图所借用的字节.
            m_Lut->pendingPixels.reset();
        }
    }

    void DisplayFrame::ReleasePayload() noexcept
    {
        m_LutPixels.reset();
    }
}
