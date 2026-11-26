#include "Pipeline.hpp"

/// @file
/// @brief 合并活动资源与采样对, 先确定逻辑布局再稳定分配, 最后独立核对各类真实限额.

#include <algorithm>
#include <limits>
#include <set>

namespace Physara::RHI::OpenGL
{
    /// @brief 为失败保留逻辑 set/binding, 不把它们误写成原生单元.
    static std::string KeyText(ShaderBindingKey key)
    {
        if (key == GraphicsPushConstantKey)
        {
            return " push constants";
        }
        return " set=" + std::to_string(key.set) + " binding=" + std::to_string(key.binding);
    }

    /// @brief 校验一种数量, 报告所需值/实际限额; 同一逻辑资源的 stage 贡献不能由去重数量替代.
    static void Limit(const char *name, std::uint64_t required, std::int64_t available)
    {
        RequirePreparation(available >= 0 && required <= std::uint64_t(available),
                           std::string("LimitExceeded: ") + name + " required=" + std::to_string(required) +
                               " available=" + std::to_string(available));
    }

    std::string BlockName(ShaderBindingKey key)
    {
        return "PhysaraBlock_" + std::to_string(key.set) + "_" + std::to_string(key.binding);
    }

    std::string PairName(std::uint32_t unit)
    {
        return "PhysaraPair_" + std::to_string(unit);
    }

    PipelineBindingPlan BuildBindingPlan(std::span<OpenGLShader *const> shaders, const BindingLayoutVector &layouts,
                                         const ShaderCaps &caps)
    {
        PipelineBindingPlan result;
        std::set<std::uint32_t> sets;
        bool first = true;
        bool explicitSets = false;
        std::optional<std::uint32_t> pushLayout;
        std::uint32_t pushSlot = 0;
        // 先确认布局的 set 解释唯一, 逻辑 slot 的稀疏程度不参与任何原生容量分配.
        for (std::size_t index = 0; index < layouts.size(); ++index)
        {
            const auto *layout = layouts[index]->GetDesc();
            RequirePreparation(layout != nullptr, "Unsupported: bindless pipeline layout");
            if (first)
            {
                explicitSets = layout->registerSpaceIsDescriptorSet;
                first = false;
            }
            RequirePreparation(layout->registerSpaceIsDescriptorSet == explicitSets,
                               "InvalidArgument: mixed descriptor-set strategies");
            const auto set = explicitSets ? layout->registerSpace : static_cast<std::uint32_t>(index);
            RequirePreparation(set < 4 && (explicitSets || layout->registerSpace == 0) && sets.insert(set).second,
                               "InvalidArgument: duplicate or invalid logical set");
            for (const auto &item : layout->bindings)
            {
                if (item.type == ResourceType::PushConstants)
                {
                    // push 在整条 Pipeline 上只有一个逻辑声明. 到活动反射匹配它时才分配物理 UBO 位置.
                    RequirePreparation(!pushLayout, "InvalidArgument: more than one pipeline push constant layout");
                    pushLayout = static_cast<std::uint32_t>(index);
                    pushSlot = item.slot;
                    result.pushBytes = result.pushBackingBytes = item.size;
                }
            }
        }
        for (const auto *shader : shaders)
        {
            // 资源 key 的相等与 stage 的贡献分别记录, 避免共享绑定点掩盖 combined 超限.
            const auto shaderType = shader->GetDesc().shaderType;
            const auto stage = shaderType == ShaderType::Vertex ? 0u : (shaderType == ShaderType::Pixel ? 1u : 2u);
            const auto bit = 1u << stage;
            for (const auto &resource : shader->Resources())
            {
                auto existing = std::ranges::find_if(
                    result.resources, [&](const auto &entry) { return entry.resource.key == resource.key; });
                if (existing != result.resources.end())
                {
                    // 同一 set/binding 跨阶段只分配一个原生位置. 缓冲布局或纹理维度与采样标量须一致.
                    RequirePreparation(existing->resource.type == resource.type &&
                                           existing->resource.dimension == resource.dimension &&
                                           existing->resource.sampledScalar == resource.sampledScalar &&
                                           existing->resource.extent == resource.extent &&
                                           existing->resource.layout == resource.layout,
                                       "InterfaceMismatch: cross-stage resource" + KeyText(resource.key));
                    const auto &layout = *layouts[existing->layoutIndex]->GetDesc();
                    RequirePreparation((layout.visibility & shader->GetDesc().shaderType) != ShaderType::None,
                                       "InterfaceMismatch: resource visibility" + KeyText(resource.key));
                    existing->stages |= bit;
                    continue;
                }
                if (resource.type == ResourceType::PushConstants)
                {
                    RequirePreparation(pushLayout && resource.extent <= result.pushBytes &&
                        (layouts[*pushLayout]->GetDesc()->visibility & shader->GetDesc().shaderType) != ShaderType::None,
                        "InterfaceMismatch: active push constant layout/size/visibility");
                    result.resources.push_back({resource, *pushLayout, pushSlot, 0, bit, ResourceType::PushConstants});
                    continue;
                }
                bool matched = false;
                for (std::size_t index = 0; index < layouts.size(); ++index)
                {
                    const auto &layout = *layouts[index]->GetDesc();
                    const auto set = explicitSets ? layout.registerSpace : static_cast<std::uint32_t>(index);
                    if (set != resource.key.set)
                    {
                        continue;
                    }
                    // 只读 SSBO 的 IR 也可能来自只读 u 槽. 以布局的 canonical binding 核对地址, 保留匹配到的实际类别.
                    const auto item = std::ranges::find_if(layout.bindings, [&](const auto &entry) {
                        const bool matchingType = entry.type == resource.type ||
                            (resource.type == ResourceType::ConstantBuffer &&
                             entry.type == ResourceType::VolatileConstantBuffer) ||
                            (shaderType == ShaderType::Compute && resource.type == ResourceType::StructuredBuffer_SRV &&
                             entry.type == ResourceType::StructuredBuffer_UAV);
                        return matchingType && CanonicalBinding(layout, entry) == resource.key.binding;
                    });
                    RequirePreparation(item != layout.bindings.end() &&
                                           (layout.visibility & shader->GetDesc().shaderType) != ShaderType::None,
                                       "InterfaceMismatch: resource layout/type/visibility" + KeyText(resource.key));
                    result.resources.push_back({resource, static_cast<std::uint32_t>(index), item->slot, 0, bit,
                                                item->type});
                    matched = true;
                    break;
                }
                RequirePreparation(matched, "InterfaceMismatch: missing layout" + KeyText(resource.key));
            }
            for (const auto &pair : shader->Pairs())
            {
                auto found = std::ranges::find(result.pairs, pair, &PlannedPair::pair);
                if (found == result.pairs.end())
                {
                    result.pairs.push_back({pair, 0, bit});
                }
                else
                {
                    found->stages |= bit;
                }
            }
        }
        // 合并完成后再排序/编号, 使 Shader 或布局的遍历顺序不成为原生 binding ABI.
        std::ranges::sort(result.resources, {}, [](const auto &entry) { return entry.resource.key; });
        std::ranges::sort(result.pairs, {}, &PlannedPair::pair);
        std::uint32_t ubo = 0;
        std::uint32_t ssbo = 0;
        std::uint32_t images = 0;
        for (auto &entry : result.resources)
        {
            const bool constant = entry.resource.type == ResourceType::ConstantBuffer ||
                                  entry.resource.type == ResourceType::PushConstants;
            if (entry.resource.type == ResourceType::Texture_UAV)
            {
                entry.unit = images++;
                continue;
            }
            if (!constant && entry.resource.type != ResourceType::StructuredBuffer_SRV &&
                entry.resource.type != ResourceType::StructuredBuffer_UAV)
            {
                continue;
            }
            entry.unit = constant ? ubo++ : ssbo++;
            entry.nativeExtent = entry.resource.extent;
            Limit((constant ? "UBO block bytes" : "SSBO block bytes"), entry.resource.extent,
                  constant ? caps.uniformBlockBytes : caps.storageBlockBytes);
            for (std::uint32_t stage = 0; stage < result.uniformCounts.size(); ++stage)
            {
                if ((entry.stages & (1u << stage)) != 0)
                {
                    ++(constant ? result.uniformCounts[stage] : result.storageCounts[stage]);
                }
            }
        }
        for (std::size_t index = 0; index < result.pairs.size(); ++index)
        {
            auto &entry = result.pairs[index];
            entry.unit = static_cast<std::uint32_t>(index);
            const auto has = [&](ShaderBindingKey key, ResourceType kind) {
                return std::ranges::any_of(result.resources, [&](const auto &resource) {
                    return resource.resource.key == key && resource.resource.type == kind;
                });
            };
            RequirePreparation(has(entry.pair.texture, ResourceType::Texture_SRV) &&
                                   (entry.pair.dummy || has(entry.pair.sampler, ResourceType::Sampler)),
                               "InterfaceMismatch: sampling pair resource" + KeyText(entry.pair.texture));
            for (std::uint32_t stage = 0; stage < result.uniformCounts.size(); ++stage)
            {
                if ((entry.stages & (1u << stage)) != 0)
                {
                    ++result.textureCounts[stage];
                }
            }
        }
        // binding-index 空间检查去重后的物理数量, per-stage/combined 检查各阶段实际贡献.
        Limit("UBO binding points", ubo, caps.uniformBindings);
        Limit("SSBO binding points", ssbo, caps.storageBindings);
        Limit("texture unit indices", result.pairs.size(), caps.combinedTextureUnits);
        Limit("image unit indices", images, caps.imageUnits);
        Limit("compute images", images, caps.computeImages);
        Limit("combined images", images, caps.combinedImages);
        Limit("compute shader output resources", std::uint64_t(images) + result.storageCounts[2],
              caps.combinedOutputs);
        for (std::uint32_t stage = 0; stage < result.uniformCounts.size(); ++stage)
        {
            Limit(stage == 0 ? "VS UBO" : (stage == 1 ? "FS UBO" : "CS UBO"), result.uniformCounts[stage], caps.uniformBlocks[stage]);
            Limit(stage == 0 ? "VS SSBO" : (stage == 1 ? "FS SSBO" : "CS SSBO"), result.storageCounts[stage], caps.storageBlocks[stage]);
            Limit(stage == 0 ? "VS textures" : (stage == 1 ? "FS textures" : "CS textures"), result.textureCounts[stage], caps.textureUnits[stage]);
        }
        Limit("combined UBO", std::uint64_t(result.uniformCounts[0]) + result.uniformCounts[1] + result.uniformCounts[2],
              caps.combinedUniformBlocks);
        Limit("combined SSBO", std::uint64_t(result.storageCounts[0]) + result.storageCounts[1] + result.storageCounts[2],
              caps.combinedStorageBlocks);
        Limit("combined textures", std::uint64_t(result.textureCounts[0]) + result.textureCounts[1] + result.textureCounts[2],
              caps.combinedTextureUnits);
        return result;
    }
} // namespace Physara::RHI::OpenGL
