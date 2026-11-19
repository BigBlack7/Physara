#include "StaticGpu.hpp"

#include <cassert>
#include <utility>

#include <ForwardSky/Bindings.hpp>
#include <StaticForward/Bindings.hpp>

/// @file
/// @brief 管理设备级程序, 绑定布局, Pipeline 及驻留额度. 资源创建与图内上传分开进行.

namespace Physara::Renderer::Detail
{
    static constexpr std::size_t FrameResidentBytes = 128 * 1024 * 1024;
    static constexpr std::size_t ResidentBytes = 256 * 1024 * 1024;
    static constexpr std::size_t FrameResidentEntries = 2048;
    static constexpr std::size_t ResidentEntries = 4096;

    void Reject(ViewSettingReason reason, std::string_view description)
    {
        throw ForwardFailure{reason, Core::BoundedText(description)};
    }

    Allocation::Allocation(std::shared_ptr<ResidencyCapacity> quota, std::size_t size) noexcept
        : capacity(std::move(quota)), bytes(size)
    {
        capacity->bytes += bytes;
        ++capacity->entries;
    }

    Allocation::~Allocation()
    {
        capacity->bytes -= bytes;
        --capacity->entries;
    }

    void UploadBatch::ReserveWrites(std::size_t byteCount, std::size_t count)
    {
        if (byteCount > ForwardUploadBudget - bytes || count > ForwardUploadItems - writes)
        {
            Reject(ViewSettingReason::SceneUploadLimit, "Forward upload batch exceeds its byte or write limit");
        }
        bytes += byteCount;
        writes += count;
    }

    void UploadBatch::Add(const std::shared_ptr<Allocation> &revision, const RHI::BufferHandle &buffer,
                          std::span<const std::byte> source, bool viewConstant)
    {
        ReserveWrites(source.size(), 1);
        buffers.push_back({revision, buffer, {source.begin(), source.end()}, viewConstant});
    }

    void UploadBatch::Clear() noexcept
    {
        buffers.clear();
        textures.clear();
        environmentTextures.clear();
        bytes = writes = 0;
    }

    /// @brief 包读取失败保留包角色及首个编译/反射诊断, 不交付半套 Shader.
    static Shader::RhiProgram LoadProgram(RHI::Device &device, const std::filesystem::path &file,
                                          std::string_view abi, std::string_view role,
                                          std::optional<Shader::ExpectedActiveResources> expectedActive = {})
    {
        auto program = Shader::LoadRhiProgram(device, file, abi, expectedActive);
        if (const auto *error = std::get_if<Shader::Failure>(&program))
        {
            Core::BoundedText text(role);
            text.Append(": ");
            text.Append(error->context.description.View());
            throw ForwardFailure{ViewSettingReason::ScenePipelineUnavailable, text};
        }
        return std::get<Shader::RhiProgram>(std::move(program));
    }

    /// @brief 共用 StaticForward ABI 的三种颜色角色. 每个活动资源名指向 schema 中的唯一绑定.
    static Shader::ExpectedActiveResources LightingResources(LightingProgram kind) noexcept
    {
        static constexpr std::array<std::string_view, 1> Legacy{"lightRows"};
        static constexpr std::array<std::string_view, 3> Physical{"lightRows", "dfgImage", "environmentSampler"};
        static constexpr std::array<std::string_view, 5> Ibl{
            "lightRows", "dfgImage", "environmentSampler", "environment", "reflectionImage"};
        const std::array<std::span<const std::string_view>, 3> names{Legacy, Physical, Ibl};
        return {Shader::Stage::Fragment, 3, names[std::size_t(kind)]};
    }

    ForwardPrograms::ForwardPrograms(RHI::Device &device, const ForwardPackagePaths &paths)
        : colors{LoadProgram(device, paths.color, StaticAbi::AbiHash, "StaticForward package",
                             LightingResources(LightingProgram::Legacy))},
          depth(LoadProgram(device, paths.depth, StaticAbi::AbiHash, "StaticDepth package")),
          physicalPaths{paths.physical, paths.ibl}, skyPath(paths.sky)
    {
        const auto sceneLayout = StaticAbi::MakeBindingLayouts();
        for (std::size_t i = 0; i < layouts.size(); ++i)
        {
            layouts[i] = device.CreateBindingLayout(sceneLayout[i]);
            if (!layouts[i])
            {
                Reject(ViewSettingReason::ScenePipelineUnavailable, "Cannot create Forward scene binding layout");
            }
        }
        PrepareLighting(device, LightingProgram::Legacy);
    }

    void ForwardPrograms::PrepareLighting(RHI::Device &device, LightingProgram kind)
    {
        const auto index = std::size_t(kind);
        if (lightLayouts[index])
        {
            return;
        }
        // 同一 schema 给出 set 3 的所有槽. 按角色裁出预期槽, 加载时核对 ABI 和实际活动资源.
        auto layoutsForProgram = StaticAbi::MakeBindingLayouts();
        auto &desc = layoutsForProgram.back();
        std::erase_if(desc.bindings, [kind](const auto &item) {
            if (kind == LightingProgram::Legacy)
            {
                return item.type != RHI::ResourceType::StructuredBuffer_SRV;
            }
            return kind == LightingProgram::Physical &&
                (item.type == RHI::ResourceType::ConstantBuffer ||
                 (item.type == RHI::ResourceType::Texture_SRV && item.slot == ReflectionSlot));
        });
        auto layout = device.CreateBindingLayout(desc);
        if (!layout)
        {
            Reject(ViewSettingReason::ScenePipelineUnavailable, "Cannot create Forward lighting layout");
        }
        if (kind != LightingProgram::Legacy)
        {
            colors[index] = LoadProgram(device, physicalPaths[index - 1], StaticAbi::AbiHash,
                kind == LightingProgram::Ibl ? "StaticForwardIbl package" : "StaticForwardPhysical package",
                LightingResources(kind));
        }
        lightLayouts[index] = std::move(layout);
    }

    void ForwardPrograms::PrepareSky(RHI::Device &device)
    {
        if (skyPipeline)
        {
            return;
        }
        namespace SkyAbi = Shader::Generated::ForwardSky;
        const auto program = LoadProgram(device, skyPath, SkyAbi::AbiHash, "ForwardSky package");
        auto layout = device.CreateBindingLayout(SkyAbi::MakeBindingLayouts().front());
        if (!layout)
        {
            Reject(ViewSettingReason::ScenePipelineUnavailable, "Cannot create Forward sky layout");
        }
        RHI::GraphicsPipelineDesc desc;
        desc.SetVertexShader(program.vertex.Get()).SetFragmentShader(program.fragment.Get())
            .AddBindingLayout(layout.Get());
        desc.renderState.depthStencilState.DisableDepthTest().DisableDepthWrite().DisableStencil();
        desc.renderState.rasterState.SetCullMode(RHI::RasterCullMode::None).EnableScissor();
        // 工作色的目的 alpha 保存几何覆盖. 背景取天空, 已覆盖处取原工作色, RGB 写掩码保留覆盖位.
        desc.renderState.blendState.targets[0].SetBlendEnable(true)
            .SetSrcBlend(RHI::BlendFactor::InvDstAlpha).SetDestBlend(RHI::BlendFactor::DstAlpha)
            .SetColorWriteMask(RHI::ColorMask::Red | RHI::ColorMask::Green | RHI::ColorMask::Blue);
        auto pipeline = device.CreateGraphicsPipeline(desc, RHI::FramebufferInfo().AddColorFormat(WorkingFormat));
        if (!pipeline)
        {
            Reject(ViewSettingReason::ScenePipelineUnavailable, "Cannot link Forward sky pipeline");
        }
        skyLayout = std::move(layout);
        skyPipeline = std::move(pipeline);
    }

    VertexLayout StaticVertexLayout(std::uint8_t attributes) noexcept
    {
        // position/normal/color 固定, 未使用 UV 和 tangent 共享已有字段的输入地址, 不增加存储.
        VertexLayout layout{{0, 12, 24, 0, 0, 24}, 40};
        for (std::uint32_t i = 0; i < 3; ++i)
        {
            if ((attributes & (1u << i)) != 0)
            {
                layout.offsets[3 + i] = layout.stride;
                layout.stride += i == 2 ? 16 : 8;
            }
        }
        return layout;
    }

    std::uint32_t PipelineKey(std::uint8_t layout, bool mirrored, bool doubleSided, SurfacePhase phase) noexcept
    {
        return static_cast<std::uint32_t>(phase) * 32 + (doubleSided ? 16u : 0u) + (mirrored ? 8u : 0u) + layout;
    }

    RHI::GraphicsPipelineHandle ForwardPrograms::GeometryPipeline(RHI::Device &device, std::uint8_t layout,
        bool mirrored, bool doubleSided, SurfacePhase phase, LightingProgram kind)
    {
        const auto mode = phase == SurfacePhase::Depth ? std::size_t(LightingProgram::Legacy) : std::size_t(kind);
        constexpr std::size_t pipelinesPerMode = 8 * 4 * 3;
        auto &pipeline = pipelines[mode * pipelinesPerMode + PipelineKey(layout, mirrored, doubleSided, phase)];
        if (pipeline)
        {
            return pipeline;
        }
        const auto &shaders = phase == SurfacePhase::Depth ? depth : colors[mode];
        if (!inputLayouts[layout])
        {
            const auto storage = StaticVertexLayout(layout);
            constexpr std::array names{"Position", "Normal", "Color", "UV0", "UV1", "Tangent"};
            constexpr std::array formats{RHI::Format::RGB32_FLOAT, RHI::Format::RGB32_FLOAT,
                RHI::Format::RGBA32_FLOAT, RHI::Format::RG32_FLOAT, RHI::Format::RG32_FLOAT, RHI::Format::RGBA32_FLOAT};
            std::array<RHI::VertexAttributeDesc, names.size()> attributes;
            for (std::size_t i = 0; i < attributes.size(); ++i)
            {
                attributes[i].SetName(names[i]).SetFormat(formats[i]).SetOffset(storage.offsets[i])
                    .SetElementStride(storage.stride);
            }
            inputLayouts[layout] = device.CreateInputLayout(attributes.data(),
                static_cast<std::uint32_t>(attributes.size()), shaders.vertex.Get());
            if (!inputLayouts[layout])
            {
                Reject(ViewSettingReason::ScenePipelineUnavailable, "Cannot prepare Forward vertex input layout");
            }
        }
        RHI::GraphicsPipelineDesc desc;
        desc.SetVertexShader(shaders.vertex.Get()).SetFragmentShader(shaders.fragment.Get())
            .SetInputLayout(inputLayouts[layout].Get());
        for (const auto &binding : layouts)
        {
            desc.AddBindingLayout(binding.Get());
        }
        if (phase != SurfacePhase::Depth)
        {
            desc.AddBindingLayout(lightLayouts[mode].Get());
        }
        // glTF 的正面是 NDC 中的 CCW. 映射到公共左上目标坐标后为顺时针, 负 determinant 再翻转一次.
        desc.renderState.rasterState.SetFrontCounterClockwise(mirrored).EnableScissor()
            .SetCullMode(doubleSided ? RHI::RasterCullMode::None : RHI::RasterCullMode::Back);
        const bool equal = phase == SurfacePhase::ReadOnlyDepth;
        desc.renderState.depthStencilState.SetDepthTestEnable(true).SetDepthWriteEnable(!equal)
            .SetDepthFunc(equal ? RHI::ComparisonFunc::Equal : RHI::ComparisonFunc::GreaterOrEqual).DisableStencil();
        RHI::FramebufferInfo target;
        target.SetDepthFormat(DepthFormat);
        if (phase != SurfacePhase::Depth)
        {
            target.AddColorFormat(WorkingFormat);
        }
        pipeline = device.CreateGraphicsPipeline(desc, target);
        if (!pipeline)
        {
            Reject(ViewSettingReason::ScenePipelineUnavailable, "Cannot link Forward geometry pipeline");
        }
        return pipeline;
    }

    ForwardDevice::ForwardDevice(RHI::Device &source, const ForwardPackagePaths &paths, std::uint32_t textureLimit)
        : device(source), maxTextureSize(textureLimit), programs(source, paths),
          display(source, {paths.display, paths.grading, paths.fxaa, paths.scale})
    {
        const auto requiredColor = RHI::FormatSupport::RenderTarget | RHI::FormatSupport::ShaderSample;
        if ((device.QueryFormatSupport(WorkingFormat) & requiredColor) != requiredColor ||
            (device.QueryFormatSupport(DisplayFormat) & requiredColor) != requiredColor ||
            (device.QueryFormatSupport(DepthFormat) & RHI::FormatSupport::DepthStencil) == 0)
        {
            Reject(ViewSettingReason::ScenePipelineUnavailable, "Forward needs RGBA16F, SRGBA8 and D32 attachments");
        }
    }

    void ForwardDevice::Begin()
    {
        if (!Core::AdvanceGeneration(stamp))
        {
            Reject(ViewSettingReason::SceneResidencyLimit, "Forward preparation identity exhausted");
        }
        frameBegin = stamp;
    }

    void ForwardDevice::BeginView()
    {
        // frameBegin 对两路相同. 每路另取一个 stamp, 让共享资源在各自的工作集预算中各计一次.
        if (!Core::AdvanceGeneration(stamp))
        {
            Reject(ViewSettingReason::SceneResidencyLimit, "Forward view preparation identity exhausted");
        }
        preparedBytes = preparedEntries = 0;
    }

    bool ForwardDevice::Current(const Allocation &entry) const noexcept
    {
        return entry.accepted || entry.lastUse >= frameBegin;
    }

    void ForwardDevice::Use(Allocation &entry)
    {
        if (entry.lastUse != stamp)
        {
            if (entry.bytes > FrameResidentBytes - preparedBytes || preparedEntries == FrameResidentEntries)
            {
                Reject(ViewSettingReason::SceneResidencyLimit, "Forward view exceeds its residency budget");
            }
            entry.lastUse = stamp;
            preparedBytes += entry.bytes;
            ++preparedEntries;
        }
    }

    void ForwardDevice::EvictUnused() noexcept
    {
        const auto idle = [this](const auto &entry)
        {
            return entry.use_count() == 1 && entry->lastUse < frameBegin;
        };
        // 材质拥有纹理依赖, 先释放它才能识别真正空闲的图像. 留在槽中的代始终占用额度.
        std::erase_if(materials, [&](const auto &pair) { return idle(pair.second.resident); });
        std::erase_if(objects, [&](const auto &pair) { return idle(pair.second.resident); });
        std::erase_if(geometry, [&](const auto &pair) { return idle(pair.second); });
        std::erase_if(textures, [&](const auto &pair) { return idle(pair.second); });
        std::erase_if(environmentTextures, [&](const auto &pair) { return idle(pair.second); });
    }

    void ForwardDevice::Reserve(std::size_t bytes)
    {
        // 单 View 超限无法靠等待消除. 仅全缓存被其它引用占用时报告 Pending.
        if (bytes > FrameResidentBytes - preparedBytes || preparedEntries == FrameResidentEntries)
        {
            Reject(ViewSettingReason::SceneResidencyLimit, "Forward resource exceeds its residency budget");
        }
        const auto full = [&]
        {
            return bytes > ResidentBytes - capacity->bytes || capacity->entries == ResidentEntries;
        };
        if (full())
        {
            EvictUnused();
            if (full())
            {
                Reject(ViewSettingReason::SceneResidencyPending, "Forward residency waiting for frame-slot completion");
            }
        }
    }

    RHI::BufferHandle AllocateBuffer(RHI::Device &device, GraphUsage usage, std::size_t size)
    {
        const RHI::BufferDesc desc{.byteSize = size, .debugName = "Forward static buffer",
            .isVertexBuffer = usage == GraphUsage::VertexBuffer, .isIndexBuffer = usage == GraphUsage::IndexBuffer,
            .isConstantBuffer = usage == GraphUsage::ConstantBuffer,
            .initialState = BufferBoundary, .keepInitialState = true};
        auto buffer = device.CreateBuffer(desc);
        if (!buffer)
        {
            Reject(ViewSettingReason::ScenePreparationFailed, "Cannot allocate Forward buffer");
        }
        return buffer;
    }

    std::shared_ptr<ConstantRow> CreateConstantRow(ForwardDevice &device, std::size_t bytes, std::size_t set)
    {
        // 先取得逻辑容量再创建物理对象, 构造中途失败由局部 shared_ptr 归还已记账容量.
        device.Reserve(bytes);
        auto row = std::make_shared<ConstantRow>(device.capacity, bytes);
        row->buffer = AllocateBuffer(device.device, GraphUsage::ConstantBuffer, bytes);
        const auto desc = RHI::BindingSetDesc().AddItem(RHI::BindingSetItem::ConstantBuffer(0, row->buffer.Get()));
        row->binding = device.device.CreateBindingSet(desc, device.programs.layouts[set].Get());
        if (!row->binding)
        {
            Reject(ViewSettingReason::ScenePreparationFailed, "Cannot prepare Forward constant binding");
        }
        device.Use(*row);
        return row;
    }
}
