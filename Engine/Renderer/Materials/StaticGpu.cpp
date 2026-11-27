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

    bool Allocation::UsedSince(std::uint64_t preparation) const noexcept
    {
        return std::ranges::any_of(lastUse, [preparation](auto use) { return use >= preparation; });
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

    void UploadBatch::ReleaseWrites(std::size_t byteCount, std::size_t count) noexcept
    {
        assert(!sealed && byteCount <= bytes && count <= writes);
        bytes -= byteCount;
        writes -= count;
    }

    void UploadBatch::DiscardView(std::size_t index, std::uint64_t stamp,
                                  const std::array<std::uint64_t, ForwardViewCount> &retained) noexcept
    {
        assert(!sealed && stamp != 0 && retained[index] == 0);
        const auto used = [&](Allocation &revision)
        {
            if (revision.lastUse[index] == stamp)
            {
                // 撤回未封存的使用证明. 后续 View 可以重新准备失去生产者的未接受存储.
                revision.lastUse[index] = 0;
            }
            for (std::size_t view = 0; view < retained.size(); ++view)
            {
                if (retained[view] != 0 && revision.lastUse[view] == retained[view])
                {
                    return true;
                }
            }
            return false;
        };

        // 以已登记的载荷重算额度, 同时归还中途失败时尚未附着到任何写入的预留.
        bytes = writes = 0;
        std::erase_if(buffers, [&](const auto &upload)
        {
            const bool sharedConsumer = used(*upload.revision);
            if (upload.view ? retained[*upload.view] == 0 : !sharedConsumer)
            {
                return true;
            }
            bytes += upload.payload.size();
            ++writes;
            return false;
        });
        std::erase_if(textures, [&](const auto &upload)
        {
            if (!used(*upload.revision))
            {
                return true;
            }
            bytes += upload.revision->bytes;
            writes += upload.levels.size();
            return false;
        });
        std::erase_if(environmentTextures, [&](const auto &upload)
        {
            if (!used(*upload.revision))
            {
                return true;
            }
            const auto &desc = upload.revision->texture->GetDesc();
            bytes += upload.revision->bytes;
            writes += std::size_t(desc.mipLevels) * desc.arraySize;
            return false;
        });
        std::erase_if(coldTables, [&](const auto &page)
        {
            if (std::ranges::find(buffers, page.get(), &BufferUpload::page) == buffers.end())
            {
                return true;
            }
            bytes += page->buffer->GetDesc().byteSize;
            ++writes;
            return false;
        });
    }

    void UploadBatch::Add(const std::shared_ptr<Allocation> &revision, const RHI::BufferHandle &buffer,
                          std::span<const std::byte> payload, GraphUploadSource source,
                          std::optional<std::size_t> view)
    {
        ReserveWrites(payload.size(), 1);
        try
        {
            buffers.push_back({revision, buffer, {payload.begin(), payload.end()}, source, 0, nullptr, view});
        }
        catch (...)
        {
            ReleaseWrites(payload.size(), 1);
            throw;
        }
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
    static Shader::RhiProgram LoadProgram(RHI::Device &device, Shader::ProductShader role,
        const std::filesystem::path &file, const Shader::ProductPackages &packages, std::string_view abi)
    {
        auto program = LoadProductProgram(device, role, file, packages, abi);
        if (const auto *error = std::get_if<ProductProgramFailure>(&program))
        {
            RejectProductProgram(Shader::FindProductProgram(role)->name, *error);
        }
        return std::get<Shader::RhiProgram>(std::move(program));
    }

    static constexpr std::array<std::string_view, 3> CoverageNames{
        "materialRows", "baseColorImage", "baseColorSampler"};
    static constexpr Shader::ExpectedActiveResources CoverageResources{Shader::Stage::Fragment, 2, CoverageNames};

    using Shader::ProductShader;

    static constexpr std::array<std::string_view, 3> ShadowQueryNames{"shadowView", "shadowImage", "shadowLayers"};

    // 三个诊断角色共用按组类型分派的查询. 级联分支在三个片元模块中均活动, 从 set 0 取主相机眼点.
    // 诊断片元从 set 2 读覆盖材质, 从 set 3 读阴影查询数据.
    // 带阴影的普通着色还需完整灯表和按灯行寻址的引用.
    static std::span<const Shader::ExpectedActiveResources> ShadowProgramResources(ProductShader role) noexcept
    {
        static constexpr std::array<std::string_view, 1> PreviewView{"perView"};
        static constexpr std::array preview{
            Shader::ExpectedActiveResources{Shader::Stage::Fragment, 0, PreviewView},
            CoverageResources,
            Shader::ExpectedActiveResources{Shader::Stage::Fragment, 3, ShadowQueryNames}};
        static constexpr std::array<std::string_view, 5> Direct{
            "lightRows", "shadowView", "shadowImage", "shadowLayers", "shadowReferences"};
        static constexpr std::array<std::string_view, 7> Physical{
            "lightRows", "dfgImage", "environmentSampler", "shadowView", "shadowImage", "shadowLayers", "shadowReferences"};
        static constexpr std::array<std::string_view, 9> Ibl{
            "lightRows", "dfgImage", "environmentSampler", "environment", "reflectionImage",
            "shadowView", "shadowImage", "shadowLayers", "shadowReferences"};
        static constexpr std::array lighting{
            Shader::ExpectedActiveResources{Shader::Stage::Fragment, 3, Direct},
            Shader::ExpectedActiveResources{Shader::Stage::Fragment, 3, Physical},
            Shader::ExpectedActiveResources{Shader::Stage::Fragment, 3, Ibl}};
        switch (role)
        {
        case ProductShader::StaticCascadePreview:
        case ProductShader::StaticSpotShadowPreview:
        case ProductShader::StaticPointShadowPreview: return preview;
        case ProductShader::StaticForwardShadows: return std::span(&lighting[0], 1);
        case ProductShader::StaticForwardPhysicalShadows: return std::span(&lighting[1], 1);
        case ProductShader::StaticForwardIblShadows: return std::span(&lighting[2], 1);
        default: return {};
        }
    }

    static constexpr std::array<ProductShader, ForwardColorPrograms> LightingRoles{
        ProductShader::StaticForward, ProductShader::StaticForwardPhysical, ProductShader::StaticForwardIbl,
        ProductShader::StaticCascadePreview, ProductShader::StaticSpotShadowPreview, ProductShader::StaticPointShadowPreview,
        ProductShader::StaticForwardShadows, ProductShader::StaticForwardPhysicalShadows, ProductShader::StaticForwardIblShadows,
        ProductShader::StaticForwardPlus, ProductShader::StaticForwardPlusPhysical, ProductShader::StaticForwardPlusIbl,
        ProductShader::StaticForwardPlusShadows, ProductShader::StaticForwardPlusPhysicalShadows,
        ProductShader::StaticForwardPlusIblShadows};
    static_assert(std::size_t(LightingProgram::Froxel) == ForwardPerPassLayouts);
    static_assert(std::size_t(LightingProgram::FroxelIblShadows) + 1 == LightingRoles.size());

    /// @brief 共用 StaticForward ABI 的颜色角色. 每个活动资源名指向 schema 中的唯一绑定.
    static std::span<const Shader::ExpectedActiveResources> LightingResources(LightingProgram kind) noexcept
    {
        static constexpr std::array<std::string_view, 1> Legacy{"lightRows"};
        static constexpr std::array<std::string_view, 3> Physical{"lightRows", "dfgImage", "environmentSampler"};
        static constexpr std::array<std::string_view, 5> Ibl{
            "lightRows", "dfgImage", "environmentSampler", "environment", "reflectionImage"};
        static constexpr std::array groups{
            Shader::ExpectedActiveResources{Shader::Stage::Fragment, 3, Legacy},
            Shader::ExpectedActiveResources{Shader::Stage::Fragment, 3, Physical},
            Shader::ExpectedActiveResources{Shader::Stage::Fragment, 3, Ibl}};
        const auto index = std::size_t(kind);
        const auto shadows = ShadowProgramResources(LightingRoles[index]);
        return shadows.empty() ? std::span(&groups[index], 1) : shadows;
    }

    /// @brief 从 PerPass 生成布局筛选此次角色实际使用的资源. 诊断只消费查询表与深度.
    static bool UsesLightingBinding(LightingProgram kind, const RHI::BindingLayoutItem &item) noexcept
    {
        using Type = RHI::ResourceType;
        const auto slot = item.slot;
        const bool shadow = (item.type == Type::ConstantBuffer && slot == ShadowConstantsSlot) ||
            (item.type == Type::Texture_SRV && slot == ShadowImageSlot) ||
            (item.type == Type::StructuredBuffer_SRV && slot == ShadowLayersSlot);
        if (IsShadowPreview(kind))
        {
            return shadow;
        }
        if (kind >= LightingProgram::LegacyShadows)
        {
            if (shadow || (item.type == Type::StructuredBuffer_SRV && slot == ShadowReferencesSlot))
            {
                return true;
            }
            kind = static_cast<LightingProgram>(std::size_t(kind) - std::size_t(LightingProgram::LegacyShadows));
        }
        const bool lights = item.type == Type::StructuredBuffer_SRV && slot == 0;
        const bool dfg = item.type == Type::Texture_SRV && slot == DfgSlot;
        switch (kind)
        {
        case LightingProgram::Legacy: return lights;
        case LightingProgram::Physical: return lights || item.type == Type::Sampler || dfg;
        case LightingProgram::Ibl:
            return lights || item.type == Type::Sampler || dfg || (item.type == Type::ConstantBuffer && slot == 0) ||
                (item.type == Type::Texture_SRV && slot == ReflectionSlot);
        default: return false;
        }
    }

    bool IsDepthRole(ProductShader role) noexcept
    {
        return role == ProductShader::StaticDepth || role == ProductShader::StaticShadowDepth;
    }

    // 颜色程序, 片元活动集和 set 3 布局共用同一个光照索引.
    static std::size_t LightingIndex(ProductShader role) noexcept
    {
        const auto found = std::ranges::find(LightingRoles, role);
        assert(found != LightingRoles.end());
        return static_cast<std::size_t>(found - LightingRoles.begin());
    }

    bool IsFroxelProgram(ProductShader role) noexcept
    {
        const auto froxelRoles = std::span(LightingRoles).subspan(ForwardPerPassLayouts);
        return std::ranges::find(froxelRoles, role) != froxelRoles.end();
    }

    std::span<const Shader::ExpectedActiveResources> OpaqueProgramResources(ProductShader role) noexcept
    {
        if (IsDeferredLighting(role))
        {
            return LightingResources(FullListProgram(DeferredLightingKind(role)));
        }
        if (role == ProductShader::StaticSurfaceAux)
        {
            static constexpr std::array<std::string_view, 7> names{
                "materialRows", "baseColorImage", "baseColorSampler", "metallicRoughnessImage",
                "metallicRoughnessSampler", "normalImage", "normalSampler"};
            static constexpr Shader::ExpectedActiveResources resources{Shader::Stage::Fragment, 2, names};
            return std::span(&resources, 1);
        }
        if (role == ProductShader::StaticCoreGBuffer)
        {
            return {}; // 与基本颜色共用完整 PerMaterial, 核心写入没有 PerPass 灯表.
        }
        if (IsDepthRole(role))
        {
            return std::span(&CoverageResources, 1);
        }
        return LightingResources(FullListProgram(static_cast<LightingProgram>(LightingIndex(role))));
    }

    ForwardPrograms::ForwardPrograms(RHI::Device &device)
    {
        auto sceneLayout = StaticAbi::MakeBindingLayouts();
        // 全灯颜色及所有深度角色共用只含 perView (b0) 的布局. Froxel 颜色布局按需求另行准备.
        std::erase_if(sceneLayout.front().bindings, [](const auto &item)
        {
            return item.type != RHI::ResourceType::ConstantBuffer || item.slot != 0;
        });
        // 几何的 set 1 仍只有对象和实例行, 屏幕重建所用的片元资源另取布局.
        std::erase_if(sceneLayout[1].bindings, [](const auto &item)
        {
            return item.type != RHI::ResourceType::StructuredBuffer_SRV;
        });
        sceneLayout[1].visibility = RHI::ShaderType::Vertex;
        for (std::size_t i = 0; i < layouts.size(); ++i)
        {
            layouts[i] = device.CreateBindingLayout(sceneLayout[i]);
            if (!layouts[i])
            {
                Reject(ViewSettingReason::ScenePipelineUnavailable, "Cannot create Forward scene binding layout");
            }
        }
    }

    /// @brief 按实际 PerPass 灯、环境与阴影组合取得同设备可共享的布局.
    static RHI::BindingLayout *PrepareLightLayout(ForwardDevice &owner, LightingProgram kind)
    {
        const auto fullList = FullListProgram(kind);
        auto &layout = owner.lightLayouts[std::size_t(fullList)];
        if (!layout)
        {
            // 同一 schema 给出 set 3 的所有槽. 每个角色保留自己预期的活动绑定和布局.
            auto layoutsForProgram = StaticAbi::MakeBindingLayouts();
            auto &desc = layoutsForProgram.back();
            std::erase_if(desc.bindings, [fullList](const auto &item) { return !UsesLightingBinding(fullList, item); });
            layout = owner.device.CreateBindingLayout(desc);
            if (!layout)
            {
                Reject(ViewSettingReason::ScenePipelineUnavailable, "Cannot create Forward lighting layout");
            }
        }
        return layout.Get();
    }

    /// @brief 几何和屏幕光照使用相同的 PerView 灯数、曝光和四块网格数据.
    static RHI::BindingLayout *PrepareFroxelViewLayout(ForwardDevice &owner)
    {
        if (!owner.froxelViewLayout)
        {
            owner.froxelViewLayout = owner.device.CreateBindingLayout(StaticAbi::MakeBindingLayouts().front());
            if (!owner.froxelViewLayout)
            {
                Reject(ViewSettingReason::ScenePipelineUnavailable, "Cannot prepare Froxel PerView layout");
            }
        }
        return owner.froxelViewLayout.Get();
    }

    void ForwardPrograms::PrepareLighting(ForwardDevice &owner, LightingProgram kind,
                                          const Shader::ProductPackages &packages)
    {
        const auto index = std::size_t(kind);
        if (colors[index].vertex)
        {
            return;
        }
        const auto &paths = owner.packagePaths;
        const std::array<const std::filesystem::path *, ForwardPerPassLayouts> inputs{
            &paths.color, &paths.physical, &paths.ibl, &paths.cascadePreview, &paths.spotShadowPreview,
            &paths.pointShadowPreview, &paths.colorShadows, &paths.physicalShadows, &paths.iblShadows};
        const auto &path = UsesFroxels(kind) ? paths.froxelColors[index - ForwardPerPassLayouts] : *inputs[index];
        auto program = LoadProgram(owner.device, LightingRoles[index], path, packages, StaticAbi::AbiHash);
        PrepareLightLayout(owner, kind);
        colors[index] = std::move(program);
    }

    void ForwardPrograms::PrepareSky(RHI::Device &device, const ForwardPackagePaths &paths,
                                     const Shader::ProductPackages &packages)
    {
        if (skyPipeline)
        {
            return;
        }
        namespace SkyAbi = Shader::Generated::ForwardSky;
        auto program = LoadProgram(device, ProductShader::ForwardSky, paths.sky, packages, SkyAbi::AbiHash);
        auto layout = skyLayout ? skyLayout : device.CreateBindingLayout(SkyAbi::MakeBindingLayouts().front());
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
        skyContent = std::move(program.contentHash);
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

    ShaderPipelineRequest GeometryRequest(std::uint8_t layout, bool mirrored, bool doubleSided, SurfacePhase phase,
                                          LightingProgram kind) noexcept
    {
        assert(layout < 8);
        auto role = LightingRoles[std::size_t(kind)];
        if (phase == SurfacePhase::Depth)
        {
            role = ProductShader::StaticDepth;
        }
        else if (phase == SurfacePhase::ShadowDepth)
        {
            role = ProductShader::StaticShadowDepth;
        }
        const auto raster = (doubleSided ? 16u : 0u) + (mirrored ? 8u : 0u);
        return {role, std::size_t(phase) * GeometryPhaseSlots + raster + layout};
    }

    // 深度共用首张表, 颜色按角色和阶段寻址. 屏幕光照每角色一个固定状态.
    static std::size_t PipelineSlot(ShaderPipelineRequest key) noexcept
    {
        if (IsDeferredLighting(key.role))
        {
            assert(key.variant == 0);
            return DeferredPipelineBase + DeferredLightingIndex(key.role);
        }
        if (IsSurfacePreview(key.role))
        {
            assert(key.variant == 0);
            return SurfacePreviewPipelineBase + SurfacePreviewIndex(key.role);
        }
        if (key.role == ProductShader::StaticCoreGBuffer)
        {
            return CorePipelineBase + key.variant;
        }
        if (key.role == ProductShader::StaticSurfaceAux)
        {
            return SurfaceAuxPipelineBase + key.variant;
        }
        return (IsDepthRole(key.role) ? 0 : LightingIndex(key.role)) * GeometryPipelineSlots + key.variant;
    }

    void ForwardPrograms::PrepareDeferredLighting(ForwardDevice &owner, ShaderPipelineRequest key,
                                                   const Shader::ProductPackages &packages)
    {
        auto &pipeline = pipelines[PipelineSlot(key)];
        if (pipeline)
        {
            return;
        }
        const auto index = DeferredLightingIndex(key.role);
        auto shaders = LoadProgram(owner.device, key.role, owner.packagePaths.deferredColors[index], packages,
                                   StaticAbi::AbiHash);
        if (!owner.deferredSurfaceLayout)
        {
            auto layoutsForProgram = StaticAbi::MakeBindingLayouts();
            auto &layout = layoutsForProgram[1];
            std::erase_if(layout.bindings, [](const auto &item)
            {
                return item.type == RHI::ResourceType::StructuredBuffer_SRV;
            });
            layout.visibility = RHI::ShaderType::Pixel;
            owner.deferredSurfaceLayout = owner.device.CreateBindingLayout(layout);
            if (!owner.deferredSurfaceLayout)
            {
                Reject(ViewSettingReason::ScenePipelineUnavailable, "Cannot prepare DeferredLighting surface layout");
            }
        }
        RHI::GraphicsPipelineDesc desc;
        desc.SetVertexShader(shaders.vertex.Get()).SetFragmentShader(shaders.fragment.Get())
            .AddBindingLayout(PrepareFroxelViewLayout(owner))
            .AddBindingLayout(owner.deferredSurfaceLayout.Get())
            .AddBindingLayout(PrepareLightLayout(owner, DeferredLightingKind(key.role)));
        desc.renderState.depthStencilState.DisableDepthTest().DisableDepthWrite().DisableStencil();
        desc.renderState.rasterState.SetCullMode(RHI::RasterCullMode::None).EnableScissor();
        auto ready = owner.device.CreateGraphicsPipeline(desc, RHI::FramebufferInfo().AddColorFormat(WorkingFormat));
        if (!ready)
        {
            Reject(ViewSettingReason::ScenePipelineUnavailable, "Cannot link DeferredLighting pipeline");
        }
        deferredColors[index] = std::move(shaders);
        pipeline = std::move(ready);
    }

    RHI::GraphicsPipelineHandle ForwardPrograms::FindPipeline(ShaderPipelineRequest key) const noexcept
    {
        return key.role == ProductShader::ForwardSky ? skyPipeline : pipelines[PipelineSlot(key)];
    }

    std::string_view ForwardPrograms::ProgramContent(ProductShader role) const noexcept
    {
        if (IsDeferredLighting(role))
        {
            return deferredColors[DeferredLightingIndex(role)].contentHash;
        }
        if (IsSurfacePreview(role))
        {
            return surfacePreviews[SurfacePreviewIndex(role)].contentHash;
        }
        switch (role)
        {
        case ProductShader::StaticCoreGBuffer: return coreGBuffer.contentHash;
        case ProductShader::StaticSurfaceAux: return surfaceAux.contentHash;
        case ProductShader::ForwardSky: return skyContent;
        case ProductShader::StaticDepth: return depth.contentHash;
        case ProductShader::StaticShadowDepth: return shadowDepth.contentHash;
        default: return colors[LightingIndex(role)].contentHash;
        }
    }

    bool ForwardPrograms::ReusePipeline(const ForwardPrograms &source, ShaderPipelineRequest key,
                                        std::string_view content)
    {
        auto existing = source.FindPipeline(key);
        if (!existing || source.ProgramContent(key.role) != content)
        {
            return false;
        }
        if (key.role == ProductShader::ForwardSky)
        {
            skyContent = source.skyContent;
            skyLayout = source.skyLayout;
            skyPipeline = std::move(existing);
            return true;
        }
        if (IsDeferredLighting(key.role))
        {
            const auto mode = DeferredLightingIndex(key.role);
            auto shaders = source.deferredColors[mode];
            deferredColors[mode] = std::move(shaders);
        }
        else if (IsSurfacePreview(key.role))
        {
            const auto index = SurfacePreviewIndex(key.role);
            surfacePreviews[index] = source.surfacePreviews[index];
        }
        else if (!SurfaceAttachments(key.role).empty())
        {
            const bool core = key.role == ProductShader::StaticCoreGBuffer;
            auto &program = core ? coreGBuffer : surfaceAux;
            if (!program.vertex)
            {
                auto shaders = core ? source.coreGBuffer : source.surfaceAux;
                program = std::move(shaders);
            }
        }
        else if (IsDepthRole(key.role))
        {
            const bool shadow = key.role == ProductShader::StaticShadowDepth;
            auto &program = shadow ? shadowDepth : depth;
            if (!program.vertex)
            {
                // 包摘要的复制可能失败. 整个程序集构造成功后才提交目标槽.
                auto shaders = shadow ? source.shadowDepth : source.depth;
                program = std::move(shaders);
            }
        }
        else
        {
            const auto mode = LightingIndex(key.role);
            if (!colors[mode].vertex)
            {
                auto shaders = source.colors[mode];
                colors[mode] = std::move(shaders);
            }
        }
        pipelines[PipelineSlot(key)] = std::move(existing);
        return true;
    }

    void ForwardPrograms::ReplaceProgram(ProductShader role) noexcept
    {
        if (role == ProductShader::ForwardSky)
        {
            skyPipeline = nullptr;
            skyContent.clear();
        }
        else if (IsDeferredLighting(role))
        {
            deferredColors[DeferredLightingIndex(role)] = {};
            pipelines[PipelineSlot({role, 0})] = nullptr;
        }
        else if (IsSurfacePreview(role))
        {
            surfacePreviews[SurfacePreviewIndex(role)] = {};
            pipelines[PipelineSlot({role, 0})] = nullptr;
        }
        else if (!SurfaceAttachments(role).empty())
        {
            auto &program = role == ProductShader::StaticCoreGBuffer ? coreGBuffer : surfaceAux;
            program = {};
            std::fill_n(pipelines.begin() + PipelineSlot({role, 0}), GeometryPhaseSlots, RHI::GraphicsPipelineHandle{});
        }
        else if (IsDepthRole(role))
        {
            const bool shadow = role == ProductShader::StaticShadowDepth;
            auto &program = shadow ? shadowDepth : depth;
            program = {};
            const auto phase = shadow ? SurfacePhase::ShadowDepth : SurfacePhase::Depth;
            std::fill_n(pipelines.begin() + std::size_t(phase) * GeometryPhaseSlots, GeometryPhaseSlots,
                        RHI::GraphicsPipelineHandle{});
        }
        else
        {
            const auto mode = LightingIndex(role);
            colors[mode] = {};
            const auto begin = pipelines.begin() + mode * GeometryPipelineSlots;
            std::fill_n(begin + std::size_t(SurfacePhase::Combined) * GeometryPhaseSlots, GeometryPhaseSlots,
                        RHI::GraphicsPipelineHandle{});
            std::fill_n(begin + std::size_t(SurfacePhase::ReadOnlyDepth) * GeometryPhaseSlots, GeometryPhaseSlots,
                        RHI::GraphicsPipelineHandle{});
        }
    }

    void ForwardPrograms::PreparePipeline(ForwardDevice &owner, ShaderPipelineRequest key,
                                          const Shader::ProductPackages &packages)
    {
        auto &device = owner.device;
        const auto &paths = owner.packagePaths;
        if (key.role == ProductShader::ForwardSky)
        {
            PrepareSky(device, paths, packages);
            return;
        }
        if (IsDeferredLighting(key.role))
        {
            PrepareDeferredLighting(owner, key, packages);
            return;
        }
        if (IsSurfacePreview(key.role))
        {
            PrepareSurfacePreview(owner, key, packages);
            return;
        }
        auto &pipeline = pipelines[PipelineSlot(key)];
        if (pipeline)
        {
            return;
        }
        const auto phase = static_cast<SurfacePhase>(key.variant / GeometryPhaseSlots);
        const auto layout = static_cast<std::uint8_t>(key.variant % 8);
        const bool mirrored = (key.variant & 8) != 0;
        const bool doubleSided = (key.variant & 16) != 0;
        const bool depthOnly = IsDepthRole(key.role);
        const bool shadow = phase == SurfacePhase::ShadowDepth;
        const auto attachments = SurfaceAttachments(key.role);
        const bool surface = !attachments.empty();
        const bool core = key.role == ProductShader::StaticCoreGBuffer;
        const auto mode = depthOnly || surface ? 0 : LightingIndex(key.role);
        auto &shaders = surface ? (core ? coreGBuffer : surfaceAux) : (depthOnly ? (shadow ? shadowDepth : depth)
                                                                                : colors[mode]);
        if (surface)
        {
            if (!shaders.vertex)
            {
                shaders = LoadProgram(device, key.role, core ? paths.coreGBuffer : paths.surfaceAux,
                                      packages, StaticAbi::AbiHash);
            }
        }
        else if (depthOnly)
        {
            if (!shaders.vertex)
            {
                // 深度图只为覆盖配方授权基色槽. 同 ABI 的包也须匹配片元实际使用的材质资源.
                shaders = LoadProgram(device, key.role, shadow ? paths.shadowDepth : paths.depth, packages,
                    StaticAbi::AbiHash);
            }
        }
        else
        {
            PrepareLighting(owner, static_cast<LightingProgram>(mode), packages);
        }
        // 顶点存储仅依属性位决定, 同一格式可跨深度和颜色角色复用. 实际 Shader 输入由 Pipeline 核对.
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
        const auto lighting = static_cast<LightingProgram>(mode);
        if (UsesFroxels(lighting))
        {
            desc.AddBindingLayout(PrepareFroxelViewLayout(owner));
        }
        else
        {
            desc.AddBindingLayout(layouts.front().Get());
        }
        for (const auto &binding : std::span(layouts).subspan(1))
        {
            desc.AddBindingLayout(binding.Get());
        }
        if (!depthOnly && !surface)
        {
            desc.AddBindingLayout(owner.lightLayouts[std::size_t(FullListProgram(lighting))].Get());
        }
        // glTF 的正面是 NDC 中的 CCW. 映射到公共左上目标坐标后为顺时针, 负 determinant 再翻转一次.
        desc.renderState.rasterState.SetFrontCounterClockwise(mirrored).EnableScissor()
            .SetCullMode(doubleSided ? RHI::RasterCullMode::None : RHI::RasterCullMode::Back);
        // 核心和辅助表面写入均为 GreaterOrEqual. 重画辅助颜色时用独立的 ReadOnlyDepth 槽读取最终 D32.
        const bool equal = phase == SurfacePhase::ReadOnlyDepth;
        const auto compare = shadow ? ShadowDepthConvention::Compare :
            (equal ? RHI::ComparisonFunc::Equal : RHI::ComparisonFunc::GreaterOrEqual);
        desc.renderState.depthStencilState.SetDepthTestEnable(true).SetDepthWriteEnable(!equal)
            .SetDepthFunc(compare).DisableStencil();
        if (shadow)
        {
            // 常量项已写入该层的顶点投影. 这里的斜率因子作用于光栅深度, 两项都沿常规深度正向偏移.
            desc.renderState.rasterState.SetSlopeScaleDepthBias(1.0f);
        }
        RHI::FramebufferInfo target;
        target.SetDepthFormat(shadow ? ShadowDepthFormat : DepthFormat);
        if (surface)
        {
            for (auto index : attachments)
            {
                target.AddColorFormat(CoreGBufferOutput::Formats[index]);
            }
        }
        else if (!depthOnly)
        {
            target.AddColorFormat(WorkingFormat);
        }
        pipeline = device.CreateGraphicsPipeline(desc, target);
        if (!pipeline)
        {
            Reject(ViewSettingReason::ScenePipelineUnavailable, "Cannot link Forward geometry pipeline");
        }
    }

    ForwardDevice::ForwardDevice(RHI::Device &source, const ForwardPackagePaths &paths, std::uint32_t textureLimit,
                                 std::uint32_t alignment, Core::BoundedText nativeIdentity,
                                 ShaderPreparationLimits limits)
        : device(source), maxTextureSize(textureLimit), uniformAlignment(alignment), packagePaths(paths),
          shaderDevice(nativeIdentity), shaderLimits(limits),
          activeShaders(std::make_shared<ForwardShaderGeneration>(source, nativeIdentity)),
          display(source, {paths.display, paths.grading, paths.fxaa, paths.scale})
    {
        shaderStatistics.budget = limits.perStep;
        const auto tableSlots = ForwardPipelineLimit * (limits.frames + 2);
        shaderStatistics.cacheLimit = limits.entryLimit ? limits.entryLimit : tableSlots;
        // Active, 候选和在途旧表最多同时占 frames + 2 张. 提交阶段按此空间记账, 以及时归还已释放的槽.
        retiredShaders.reserve(limits.frames + 1);
        countedPipelines.reserve(tableSlots);
        // 每个有效键最多一条原生失败. 捕获创建异常后可直接保留原因.
        activeFailures.reserve(ForwardPipelineLimit);
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

    void ForwardDevice::BeginView(std::size_t index)
    {
        // frameBegin 对两路相同. 每路另取一个 stamp, 让共享资源在各自的工作集预算中各计一次.
        if (!Core::AdvanceGeneration(stamp))
        {
            Reject(ViewSettingReason::SceneResidencyLimit, "Forward view preparation identity exhausted");
        }
        viewBudgets[index] = {stamp};
        ResumeView(index);
    }

    void ForwardDevice::ResumeView(std::size_t index) noexcept
    {
        assert(viewBudgets[index].stamp >= frameBegin);
        preparingView = index;
    }

    bool ForwardDevice::Current(const Allocation &entry) const noexcept
    {
        return entry.accepted || entry.UsedSince(frameBegin);
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
        auto &budget = viewBudgets[preparingView];
        auto &lastUse = entry.lastUse[preparingView];
        if (lastUse != budget.stamp)
        {
            CheckViewCapacity(entry.bytes, budget.bytes, budget.entries);
            lastUse = budget.stamp;
            budget.bytes += entry.bytes;
            ++budget.entries;
        }
    }

    void ForwardDevice::EvictUnused() noexcept
    {
        const auto idle = [this](const auto &entry)
        {
            return entry.use_count() == 1 && !entry->UsedSince(frameBegin);
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
        const auto &budget = viewBudgets[preparingView];
        CheckViewCapacity(bytes, budget.bytes, budget.entries);
        CheckDeviceResidency({paged ? 0 : bytes, 1});
    }

    void ForwardDevice::CheckDeviceResidency(ResidencyDemand demand)
    {
        const auto full = [&]
        {
            return demand.bytes > ResidentBytes - capacity->bytes ||
                demand.entries > ResidentEntries - capacity->entries;
        };
        if (full())
        {
            EvictUnused();
            if (full())
            {
                if (demand.entries > ResidentEntries - capacity->entries)
                {
                    RejectCapacity(ViewSettingReason::SceneResidencyPending, "Forward residency entries pending",
                                   demand.entries, capacity->entries, ResidentEntries);
                }
                RejectCapacity(ViewSettingReason::SceneResidencyPending, "Forward residency bytes pending",
                               demand.bytes, capacity->bytes, ResidentBytes);
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

    RHI::BufferHandle AllocateBuffer(RHI::Device &device, GraphUsage usage, std::size_t size, std::uint32_t stride,
                                     const char *name)
    {
        const RHI::BufferDesc desc{.byteSize = size, .structStride = stride, .debugName = name,
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

    std::shared_ptr<ConstantRow> CreateConstantRow(ForwardDevice &device, std::size_t bytes,
                                                   std::optional<std::size_t> set)
    {
        // 先取得逻辑容量再创建物理对象, 构造中途失败由局部 shared_ptr 归还已记账容量.
        device.Reserve(bytes);
        auto row = std::make_shared<ConstantRow>(device.capacity, bytes);
        row->buffer = AllocateBuffer(device.device, GraphUsage::ConstantBuffer, bytes);
        if (set)
        {
            const auto desc = RHI::BindingSetDesc().AddItem(RHI::BindingSetItem::ConstantBuffer(0, row->buffer.Get()));
            row->binding = device.device.CreateBindingSet(desc, device.activeShaders->programs.layouts[*set].Get());
            if (!row->binding)
            {
                Reject(ViewSettingReason::ScenePreparationFailed, "Cannot prepare Forward constant binding");
            }
        }
        device.Use(*row);
        return row;
    }
}
