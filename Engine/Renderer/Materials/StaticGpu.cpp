#include "StaticGpu.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <utility>

#include <ForwardSky/Bindings.hpp>
#include <StaticForward/Bindings.hpp>

/// @file
/// @brief 为 Forward 创建程序与 Pipeline, 按逻辑版本分配驻留资源. 页拥有物理容量, 区间跟随使用它的版本释放.

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

    /// @brief 保留超限发生位置和预算口径, 供待处理状态及诊断使用.
    [[noreturn]] static void RejectCapacity(ViewSettingReason reason, const char *resource,
                                           std::size_t requested, std::size_t retained, std::size_t limit)
    {
        char description[144];
        std::snprintf(description, sizeof(description), "%s: requested=%zu retained=%zu limit=%zu",
                      resource, requested, retained, limit);
        Reject(reason, description);
    }

    Allocation::Allocation(std::shared_ptr<ResidencyCapacity> quota, std::size_t size, bool paged) noexcept
        : capacity(std::move(quota)), bytes(size), chargedBytes(paged ? 0 : size)
    {
        capacity->bytes += chargedBytes;
        ++capacity->entries;
    }

    Allocation::~Allocation()
    {
        capacity->bytes -= chargedBytes;
        --capacity->entries;
    }

    BufferPage::BufferPage(std::shared_ptr<ResidencyCapacity> quota, RHI::BufferHandle storage, GraphUsage use,
                           std::uint8_t layoutKey, std::uint64_t sequence)
        : capacity(std::move(quota)), buffer(std::move(storage)), usage(use), layout(layoutKey), ordinal(sequence),
          largestGap(static_cast<std::size_t>(buffer->GetDesc().byteSize))
    {
        capacity->bytes += buffer->GetDesc().byteSize;
    }

    BufferPage::~BufferPage()
    {
        assert(occupied.empty());
        capacity->bytes -= buffer->GetDesc().byteSize;
    }

    std::optional<RHI::BufferRange> BufferPage::Claim(const Allocation &owner, std::size_t size,
                                                      std::size_t alignment)
    {
        assert(size != 0 && alignment != 0);
        // 每个已占区间之间只检查一次. 已占和可用区间均受页的实际容量约束.
        if (size > largestGap)
        {
            return std::nullopt;
        }
        std::uint64_t start = 0;
        for (auto it = occupied.begin();;)
        {
            const auto end = it == occupied.end() ? buffer->GetDesc().byteSize : it->range.byteOffset;
            const auto padding = (alignment - start % alignment) % alignment;
            if (padding <= end - start && size <= end - start - padding)
            {
                const RHI::BufferRange range{start + padding, size};
                occupied.insert(it, Occupied{range, &owner});
                usedBytes += size;
                if (end - start == largestGap)
                {
                    // 原最大空洞切开后, 从剩余空洞重新确定上界. 稳态统计可直接读取该事实.
                    largestGap = 0;
                    std::uint64_t previousEnd = 0;
                    for (const auto &entry : occupied)
                    {
                        largestGap = std::max(largestGap, std::size_t(entry.range.byteOffset - previousEnd));
                        previousEnd = entry.range.byteOffset + entry.range.byteSize;
                    }
                    largestGap = std::max(largestGap, std::size_t(buffer->GetDesc().byteSize - previousEnd));
                }
                return range;
            }
            if (it == occupied.end())
            {
                return std::nullopt;
            }
            start = it->range.byteOffset + it->range.byteSize;
            ++it;
        }
    }

    void BufferPage::Release(RHI::BufferRange range) noexcept
    {
        const auto existing = std::ranges::lower_bound(occupied, range.byteOffset, {},
            [](const Occupied &entry) { return entry.range.byteOffset; });
        assert(existing != occupied.end() && existing->range == range);
        // 移除后只有前后两个邻居之间的空洞可能变大, 其余空洞沿用已有上界.
        const auto start = existing == occupied.begin() ? 0 : (existing - 1)->range.byteOffset +
                                                               (existing - 1)->range.byteSize;
        if (existing->retiring)
        {
            retiringBytes -= static_cast<std::size_t>(range.byteSize);
        }
        const auto next = occupied.erase(existing);
        const auto end = next == occupied.end() ? buffer->GetDesc().byteSize : next->range.byteOffset;
        largestGap = std::max(largestGap, std::size_t(end - start));
        usedBytes -= static_cast<std::size_t>(range.byteSize);
    }

    void BufferPage::Retire(RHI::BufferRange range) noexcept
    {
        const auto existing = std::ranges::lower_bound(occupied, range.byteOffset, {},
            [](const Occupied &entry) { return entry.range.byteOffset; });
        assert(existing != occupied.end() && existing->range == range && !existing->retiring);
        existing->retiring = true;
        retiringBytes += static_cast<std::size_t>(range.byteSize);
    }

    PageSlice::~PageSlice()
    {
        if (page)
        {
            page->Release(range);
        }
    }

    void PageSlice::Retire() noexcept
    {
        assert(page);
        page->Retire(range);
    }

    void GeometryRef::Retire() noexcept
    {
        vertices.Retire();
        indices.Retire();
    }

    ResidentTexture::ResidentTexture(std::shared_ptr<ResidencyCapacity> quota, std::size_t bytes, bool hdr)
        : Allocation(std::move(quota), bytes), environment(hdr)
    {
        (environment ? capacity->environmentBytes : capacity->textureBytes) += bytes;
    }

    ResidentTexture::~ResidentTexture()
    {
        (environment ? capacity->environmentBytes : capacity->textureBytes) -= bytes;
    }

    void UploadBatch::ReserveWrites(std::size_t byteCount, std::size_t count)
    {
        assert(!sealed);
        if (byteCount > ForwardUploadBudget - bytes)
        {
            RejectCapacity(ViewSettingReason::SceneUploadLimit, "Forward upload batch bytes",
                           byteCount, bytes, ForwardUploadBudget);
        }
        if (count > ForwardUploadItems - writes)
        {
            RejectCapacity(ViewSettingReason::SceneUploadLimit, "Forward upload batch writes",
                           count, writes, ForwardUploadItems);
        }
        bytes += byteCount;
        writes += count;
    }

    void UploadBatch::Add(const std::shared_ptr<Allocation> &revision, const RHI::BufferHandle &buffer,
                          std::span<const std::byte> payload, GraphUploadSource source)
    {
        ReserveWrites(payload.size(), 1);
        buffers.push_back({revision, buffer, {payload.begin(), payload.end()}, source});
    }

    void UploadBatch::Add(const std::shared_ptr<Allocation> &revision, const PageSlice &slice,
                          std::span<const std::byte> source)
    {
        assert(slice.range.byteSize == source.size());
        if (slice.page->usage == GraphUsage::ShaderRead && !slice.page->initialized &&
            std::ranges::find(coldTables, slice.page) == coldTables.end())
        {
            // 构图将所有使用的冷行合入一次完整页写入. 即使零填充也计入真实字节需求.
            ReserveWrites(slice.page->buffer->GetDesc().byteSize, 1);
            coldTables.push_back(slice.page);
        }
        ReserveWrites(source.size(), 1);
        buffers.push_back({revision, slice.page->buffer, {source.begin(), source.end()}, GraphUploadSource::ForwardStatic,
                           slice.range.byteOffset, slice.page.get()});
    }

    void UploadBatch::Clear() noexcept
    {
        buffers.clear();
        textures.clear();
        environmentTextures.clear();
        merged.clear();
        coldTables.clear();
        bytes = writes = 0;
        sealed = false;
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

    ForwardDevice::ForwardDevice(RHI::Device &source, const ForwardPackagePaths &paths, std::uint32_t textureLimit,
                                 std::uint32_t alignment)
        : device(source), maxTextureSize(textureLimit), uniformAlignment(alignment), programs(source, paths),
          display(source, {paths.display, paths.grading, paths.fxaa, paths.scale})
    {
        if (uniformAlignment == 0)
        {
            Reject(ViewSettingReason::ScenePipelineUnavailable, "Forward requires known constant-buffer alignment");
        }
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
        std::erase_if(materialBindings, [](const auto &entry) { return entry.second.expired(); });
    }

    void ForwardDevice::ObserveGeometry(const SceneFrame &scene)
    {
        if (geometryScene == scene.scene && geometryStructure == scene.structureRevision &&
            geometryContent == scene.contentRevision)
        {
            return;
        }
        if (scene.models.empty())
        {
            // 先登记每代的待退休字节, 再清除目录. 此时仍有 Draw 持有的区间在相应槽安全释放后归还.
            for (const auto &entry : geometry)
            {
                entry.second->Retire();
            }
            geometry.clear();
            geometryVersions.clear();
        }
        else
        {
            // 使用整个 SceneFrame 的固定资产, 与当前相机剔除无关. 同一源的多个固定代可以同时有效.
            std::vector<AssetVersionKey> active(0);
            active.reserve(scene.models.size());
            for (const auto &model : scene.models)
            {
                active.push_back(Version(model->asset, model->revision));
            }
            std::ranges::sort(active);
            // 变换或材质编辑可以推进内容代, 固定版本集合不变时沿用原几何目录.
            if (active != geometryVersions)
            {
                std::erase_if(geometry, [&](const auto &entry)
                {
                    if (std::ranges::binary_search(active, std::get<0>(entry.first)))
                    {
                        return false;
                    }
                    entry.second->Retire();
                    return true;
                });
                geometryVersions = std::move(active);
            }
        }
        geometryScene = scene.scene;
        geometryStructure = scene.structureRevision;
        geometryContent = scene.contentRevision;
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

    /// @brief 新分配和缓存命中共用同一 View 工作集预算.
    static void CheckViewCapacity(std::size_t bytes, std::size_t usedBytes, std::size_t usedEntries)
    {
        if (usedEntries == FrameResidentEntries)
        {
            RejectCapacity(ViewSettingReason::SceneResidencyLimit, "Forward view residency entries",
                           1, usedEntries, FrameResidentEntries);
        }
        if (bytes > FrameResidentBytes - usedBytes)
        {
            RejectCapacity(ViewSettingReason::SceneResidencyLimit, "Forward view residency bytes",
                           bytes, usedBytes, FrameResidentBytes);
        }
    }

    void ForwardDevice::Use(Allocation &entry)
    {
        if (entry.lastUse != stamp)
        {
            CheckViewCapacity(entry.bytes, preparedBytes, preparedEntries);
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
        std::erase_if(materialBindings, [](const auto &entry) { return entry.second.expired(); });
        std::erase_if(objects, [&](const auto &pair) { return idle(pair.second.resident); });
        std::erase_if(geometry, [&](const auto &pair) { return idle(pair.second); });
        std::erase_if(textures, [&](const auto &pair) { return idle(pair.second); });
        std::erase_if(environmentTextures, [&](const auto &pair) { return idle(pair.second.texture); });
        std::erase_if(samplers, [](const auto &pair) { return pair.second->GetRefCount() == 1; });
        std::erase_if(pages, [](const auto &page) { return page->occupied.empty(); });
    }

    void ForwardDevice::Reserve(std::size_t bytes, bool paged)
    {
        // 单 View 超限无法靠等待消除. 仅全缓存被其它引用占用时报告 Pending.
        CheckViewCapacity(bytes, preparedBytes, preparedEntries);
        const auto storageBytes = paged ? 0 : bytes;
        const auto full = [&]
        {
            return storageBytes > ResidentBytes - capacity->bytes || capacity->entries == ResidentEntries;
        };
        if (full())
        {
            EvictUnused();
            if (full())
            {
                if (capacity->entries == ResidentEntries)
                {
                    RejectCapacity(ViewSettingReason::SceneResidencyPending, "Forward residency entries pending",
                                   1, capacity->entries, ResidentEntries);
                }
                RejectCapacity(ViewSettingReason::SceneResidencyPending, "Forward residency bytes pending",
                               storageBytes, capacity->bytes, ResidentBytes);
            }
        }
    }

    void ForwardDevice::AllocateSlice(PageSlice &slice, const Allocation &owner, GraphUsage usage,
                                      std::uint8_t layout, std::size_t bytes, std::size_t alignment,
                                      std::size_t tableRows)
    {
        assert(!slice.page && bytes != 0 && alignment != 0);
        const auto tryPages = [&]
        {
            for (const auto &page : pages)
            {
                // 已尝试整页写入却未获确认时, 旧前缀可能仍在使用这张表. 新代从其它页申请.
                if (page->usage != usage || page->layout != layout ||
                    (page->initializationRecorded && !page->initialized))
                {
                    continue;
                }
                if (const auto available = page->Claim(owner, bytes, alignment))
                {
                    slice.range = *available;
                    slice.page = page;
                    return true;
                }
            }
            return false;
        };
        if (tryPages())
        {
            return;
        }
        // 新页通常按用途的基准粒度增长. 额度紧张时允许缩到实际余量, 仍须能容纳完整申请.
        const std::size_t minimum = usage == GraphUsage::VertexBuffer ? 4 * 1024 * 1024 :
                                    usage == GraphUsage::IndexBuffer ? 1024 * 1024 : 64 * 1024;
        assert(tableRows == 0 || (usage == GraphUsage::ShaderRead && tableRows <= ForwardBufferLimit / bytes));
        const auto required = tableRows ? bytes * tableRows : bytes;
        if (required > ResidentBytes - capacity->bytes)
        {
            EvictUnused();
            if (tryPages())
            {
                return;
            }
            if (required > ResidentBytes - capacity->bytes)
            {
                RejectCapacity(ViewSettingReason::SceneResidencyPending, "Forward page bytes pending",
                               required, capacity->bytes, ResidentBytes);
            }
        }
        // 表绑定从字节 0 开始, 页容量取 ABI 行数乘步长. 几何页的容量按用途与设备余量确定.
        const auto storage = tableRows ? required :
            std::min(std::max(bytes, minimum), ResidentBytes - capacity->bytes);
        if (!Core::AdvanceGeneration(pageOrdinal))
        {
            Reject(ViewSettingReason::SceneResidencyLimit, "Forward page identity exhausted");
        }
        const auto stride = tableRows ? static_cast<std::uint32_t>(bytes) : 0u;
        auto page = std::make_shared<BufferPage>(capacity, AllocateBuffer(device, usage, storage, stride),
                                                 usage, layout, pageOrdinal);
        pages.push_back(page);
        slice.range = page->Claim(owner, bytes, alignment).value();
        slice.page = std::move(page);
    }

    RHI::BufferHandle AllocateBuffer(RHI::Device &device, GraphUsage usage, std::size_t size, std::uint32_t stride)
    {
        const RHI::BufferDesc desc{.byteSize = size, .structStride = stride, .debugName = "Forward buffer",
            .isVertexBuffer = usage == GraphUsage::VertexBuffer, .isIndexBuffer = usage == GraphUsage::IndexBuffer,
            .isConstantBuffer = usage == GraphUsage::ConstantBuffer,
            .isDrawIndirectArgs = usage == GraphUsage::IndirectArgument,
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
