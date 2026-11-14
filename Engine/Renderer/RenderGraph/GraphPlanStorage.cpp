#include "GraphCompilerInternal.hpp"

#include <algorithm>
#include <limits>
#include <tuple>
#include <type_traits>
#include <utility>

#include "RenderGraph.hpp"

/// @file
/// @brief 从存活顺序和图尾交接计算整项资源的首末使用, 为所有版本建立同一存储映射.
/// 瞬态槽以确定顺序分配并按完整存储核算逻辑字节, 导入沿用图持有的外部对象.

namespace Physara::Renderer::Detail
{
    using ByteCount = std::uint64_t;
    static constexpr auto MaxBytes = (std::numeric_limits<ByteCount>::max)();

    // 比较影响原生对象和默认视图行为的字段. 调试名不参与, 未启用的清除值不影响兼容性.
    static bool Compatible(const RHI::TextureDesc &a, const RHI::TextureDesc &b) noexcept
    {
        const auto fields = [](const RHI::TextureDesc &d)
        {
            return std::tie(d.width, d.height, d.depth, d.arraySize, d.mipLevels, d.sampleCount, d.sampleQuality,
                            d.format, d.dimension, d.defaultComponentMapping, d.isShaderResource, d.isRenderTarget,
                            d.isUAV, d.isTypeless, d.isShadingRateSurface, d.sharedResourceFlags, d.isVirtual, d.isTiled,
                            d.useClearValue, d.initialState, d.keepInitialState);
        };
        return fields(a) == fields(b) && (!a.useClearValue || a.clearValue == b.clearValue);
    }

    // 同时比较容量和视图/状态相关描述, 因此同容量的不同用法仍可能需要独立槽.
    static bool Compatible(const RHI::BufferDesc &a, const RHI::BufferDesc &b) noexcept
    {
        const auto fields = [](const RHI::BufferDesc &d)
        {
            return std::tie(d.byteSize, d.structStride, d.maxVersions, d.format, d.canHaveUAVs, d.canHaveTypedViews,
                            d.canHaveRawViews, d.isVertexBuffer, d.isIndexBuffer, d.isConstantBuffer,
                            d.isDrawIndirectArgs, d.isAccelStructBuildInput, d.isAccelStructStorage,
                            d.isShaderBindingTable, d.isVolatile, d.isVirtual, d.initialState, d.keepInitialState,
                            d.cpuAccess, d.sharedResourceFlags);
        };
        return fields(a) == fields(b);
    }

    // 预算覆盖所有 mip, 即使某些 mip 没有用途. 宽高按格式块向上取整, 体深随 mip 缩小.
    // 数组层数和样本数保持不变. 乘积和总和在运算前检查可表示范围.
    static std::optional<ByteCount> StorageBytes(const RHI::TextureDesc &desc) noexcept
    {
        const auto &format = RHI::GetFormatInfo(desc.format);
        const auto blocks = [size = ByteCount(format.blockSize)](std::uint32_t extent, std::uint32_t mip)
        {
            return (ByteCount((std::max)(1u, extent >> mip)) + size - 1) / size;
        };
        ByteCount total = 0;
        for (std::uint32_t mip = 0; mip < desc.mipLevels; ++mip)
        {
            ByteCount bytes = format.bytesPerBlock;
            const ByteCount factors[] = {blocks(desc.width, mip), blocks(desc.height, mip),
                                         (std::max)(1u, desc.depth >> mip), desc.arraySize, desc.sampleCount};
            for (const auto factor : factors)
            {
                if (bytes > MaxBytes / factor)
                {
                    return std::nullopt;
                }
                bytes *= factor;
            }
            if (bytes > MaxBytes - total)
            {
                return std::nullopt;
            }
            total += bytes;
        }
        return total;
    }

    // 缓冲按完整字节容量计量, 图内访问范围只影响寿命.
    static std::optional<ByteCount> StorageBytes(const RHI::BufferDesc &desc) noexcept
    {
        return desc.byteSize;
    }

    /// @brief 仅在编译期间记录寿命. 没有 firstUse 的声明不进入物理计划.
    struct UseInterval
    {
        std::optional<std::size_t> firstUse;
        std::size_t lastUse = 0;

        // 调用方按最终执行位置递增登记用途.
        void Include(std::size_t position) noexcept
        {
            if (!firstUse)
            {
                firstUse = position;
            }
            lastUse = position;
        }
    };

    /// @brief 待分配瞬态条目. index 指向同类型计划资源表, 该表按声明索引收集.
    /// @note 首用相同则纹理在缓冲之前, 同类型沿资源表顺序.
    struct TransientResource
    {
        std::size_t firstUse;
        bool buffer;
        std::size_t index;
    };

    // 按声明索引收集有实际用途的资源, 保留身份和允许范围供执行方查询.
    // 导入携带原进出状态, 只有图内创建的资源进入瞬态分配队列.
    template <typename Declarations, typename PlannedResource>
    static void CollectResources(const Declarations &declared, std::span<const UseInterval> lifetimes,
                                 std::uint64_t generation, bool buffer, std::vector<PlannedResource> &used,
                                 std::vector<TransientResource> &transients)
    {
        for (std::size_t i = 0; i < lifetimes.size(); ++i)
        {
            const auto &life = lifetimes[i];
            if (!life.firstUse)
            {
                continue;
            }
            const auto &resource = declared[i];
            if (!resource.importState)
            {
                transients.push_back({*life.firstUse, buffer, used.size()});
            }
            used.push_back({{{static_cast<std::uint32_t>(i), generation}, 0}, resource.range, *life.firstUse,
                            life.lastUse, std::nullopt, resource.importState});
        }
    }

    // 复用首个描述兼容且末用严格早于本次首用的槽. 同一 Pass 首末相接仍是重叠.
    // 预算只在新开槽时增长, 超限或估算不可表示时由调用方放弃局部计划.
    template <typename Desc, typename Resource>
    static GraphFailure AssignSlot(const Desc &desc, Resource &resource, ByteCount budget,
                                   std::vector<std::size_t> &lastUses, CompiledGraphPlan &plan)
    {
        for (std::size_t i = 0; i < lastUses.size(); ++i)
        {
            const auto *existing = std::get_if<Desc>(&plan.transientSlots[i].desc);
            if (existing && lastUses[i] < resource.firstUse && Compatible(*existing, desc))
            {
                resource.transientSlot = i;
                lastUses[i] = resource.lastUse;
                return std::nullopt;
            }
        }
        const auto bytes = StorageBytes(desc);
        if (!bytes || *bytes > budget - plan.transientBytes)
        {
            const auto reason = bytes ? "Transient plan exceeds byte budget" :
                                        "Transient storage size is not representable";
            return CompileError(GraphErrorCode::TransientBudgetExceeded, reason,
                                LocateResource(resource.resource, desc.debugName));
        }
        const auto slot = plan.transientSlots.size();
        // 在目标存储内复制描述, 避免 Debug string 的 noexcept move 分配迭代器代理.
        plan.transientSlots.emplace_back(desc, *bytes);
        lastUses.push_back(resource.lastUse);
        resource.transientSlot = slot;
        plan.transientBytes += *bytes;
        return std::nullopt;
    }

    // 用实际执行位置覆盖每项资源的寿命, 再按首用分配瞬态槽.
    GraphFailure BuildStoragePlan(const RenderGraph &graph, ByteCount budget, CompiledGraphPlan &plan)
    {
        std::vector<UseInterval> textures(graph.Textures().size());
        std::vector<UseInterval> buffers(graph.Buffers().size());
        for (std::size_t i = 0; i < plan.passes.size(); ++i)
        {
            for (const auto &use : plan.passes[i].uses)
            {
                if (const auto *texture = std::get_if<TextureUse>(&use.declaration))
                {
                    textures[texture->access.resource.identity.index].Include(i);
                }
                else
                {
                    buffers[std::get<BufferUse>(use.declaration).access.resource.identity.index].Include(i);
                }
            }
        }
        // 图尾消费可以是直接导入的首次使用, 也可以延长已有存储的寿命.
        for (const auto &output : plan.exports)
        {
            if (const auto *texture = std::get_if<TextureExport>(&output))
            {
                textures[texture->resource.identity.index].Include(plan.passes.size());
            }
            else
            {
                buffers[std::get<BufferExport>(output).resource.identity.index].Include(plan.passes.size());
            }
        }

        std::vector<TransientResource> transients(0);
        CollectResources(graph.Textures(), textures, plan.graphGeneration, false, plan.textures, transients);
        CollectResources(graph.Buffers(), buffers, plan.graphGeneration, true, plan.buffers, transients);
        std::ranges::sort(transients, {}, [](const TransientResource &resource)
        {
            return std::tie(resource.firstUse, resource.buffer, resource.index);
        });
        plan.transientSlots.reserve(transients.size());
        std::vector<std::size_t> lastUses(0);
        for (const auto &entry : transients)
        {
            if (entry.buffer)
            {
                auto &resource = plan.buffers[entry.index];
                if (auto error = AssignSlot(graph.Buffers()[resource.resource.identity.index].desc, resource,
                                            budget, lastUses, plan))
                {
                    return error;
                }
            }
            else
            {
                auto &resource = plan.textures[entry.index];
                if (auto error = AssignSlot(graph.Textures()[resource.resource.identity.index].desc, resource,
                                            budget, lastUses, plan))
                {
                    return error;
                }
            }
        }
        return std::nullopt;
    }
}
