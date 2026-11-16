#include "StaticGpu.hpp"

#include <cassert>
#include <utility>

#include <ForwardDisplay/Bindings.hpp>
#include <StaticForward/Bindings.hpp>

/// @file
/// @brief 设备级 Shader、Pipeline、绑定布局和静态驻留预算. 这里创建对象, 写入留给图.

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
                          std::span<const std::byte> source)
    {
        ReserveWrites(source.size(), 1);
        buffers.push_back({revision, buffer, {source.begin(), source.end()}});
    }

    void UploadBatch::Clear() noexcept
    {
        buffers.clear();
        textures.clear();
        bytes = writes = 0;
    }

    /// @brief 包读取失败保留包角色及首个编译/反射诊断, 不交付半套 Shader.
    static Shader::RhiProgram LoadProgram(RHI::Device &device, const std::filesystem::path &file,
                                          std::string_view abi, std::string_view role)
    {
        auto program = Shader::LoadRhiProgram(device, file, abi);
        if (const auto *error = std::get_if<Shader::Failure>(&program))
        {
            Core::BoundedText text(role);
            text.Append(": ");
            text.Append(error->context.description.View());
            throw ForwardFailure{ViewSettingReason::ScenePipelineUnavailable, text};
        }
        return std::get<Shader::RhiProgram>(std::move(program));
    }

    ForwardPrograms::ForwardPrograms(RHI::Device &device, const ForwardPackagePaths &paths)
        : color(LoadProgram(device, paths.color, StaticAbi::AbiHash, "StaticForward package")),
          depth(LoadProgram(device, paths.depth, StaticAbi::AbiHash, "StaticDepth package"))
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
        namespace DisplayAbi = Shader::Generated::ForwardDisplay;
        const auto program = LoadProgram(device, paths.display, DisplayAbi::AbiHash, "ForwardDisplay package");
        displayLayout = device.CreateBindingLayout(DisplayAbi::MakeBindingLayouts().front());
        if (!displayLayout)
        {
            Reject(ViewSettingReason::ScenePipelineUnavailable, "Cannot create Forward display binding layout");
        }
        RHI::GraphicsPipelineDesc desc;
        desc.SetVertexShader(program.vertex.Get()).SetFragmentShader(program.fragment.Get())
            .AddBindingLayout(displayLayout.Get());
        desc.renderState.depthStencilState.DisableDepthTest().DisableDepthWrite().DisableStencil();
        desc.renderState.rasterState.SetCullMode(RHI::RasterCullMode::None);
        displayPipeline = device.CreateGraphicsPipeline(desc, RHI::FramebufferInfo().AddColorFormat(DisplayFormat));
        if (!displayPipeline)
        {
            Reject(ViewSettingReason::ScenePipelineUnavailable, "Cannot link Forward display pipeline");
        }
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
        bool mirrored, bool doubleSided, SurfacePhase phase)
    {
        auto &pipeline = pipelines[PipelineKey(layout, mirrored, doubleSided, phase)];
        if (pipeline)
        {
            return pipeline;
        }
        const auto &shaders = phase == SurfacePhase::Depth ? depth : color;
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
        : device(source), maxTextureSize(textureLimit), programs(source, paths)
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
        preparedBytes = preparedEntries = 0;
    }

    bool ForwardDevice::Current(const Allocation &entry) const noexcept
    {
        return entry.accepted || entry.lastUse == stamp;
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
            return entry.use_count() == 1 && entry->lastUse != stamp;
        };
        // 材质拥有纹理依赖, 先释放它才能识别真正空闲的图像. 留在槽中的代始终占用额度.
        std::erase_if(materials, [&](const auto &pair) { return idle(pair.second); });
        std::erase_if(objects, [&](const auto &pair) { return idle(pair.second.resident); });
        std::erase_if(geometry, [&](const auto &pair) { return idle(pair.second); });
        std::erase_if(textures, [&](const auto &pair) { return idle(pair.second); });
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
