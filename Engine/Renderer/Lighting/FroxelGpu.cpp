#include "FroxelGpu.hpp"

/// @file
/// @brief 按生成的固定容量交付 Froxel 常量和记录, 沿用已有场景上传和读权限.

#include <algorithm>
#include <cassert>
#include <span>
#include <type_traits>
#include <utility>

#include "../Materials/StaticGpu.hpp"
#include "../Prepare/ViewPreparation.hpp"
#include "Froxel.hpp"

namespace Physara::Renderer::Detail
{
    /// @brief 生成 ABI 的 PerView 块范围和图用途. slot 分别位于 CBV 和 SRV 命名空间.
    struct FroxelBinding
    {
        std::size_t bytes;
        std::uint32_t stride;
        GraphUsage usage;
        std::uint32_t slot;
    };

    // Prepare 的载荷顺序, Resolve 取得的写版本以及 Bind 交给 PerView 的条目顺序共用本表.
    static constexpr std::array Bindings{
        FroxelBinding{sizeof(StaticAbi::FroxelView), 0, GraphUsage::ConstantBuffer, 1},
        FroxelBinding{sizeof(StaticAbi::FroxelHeadersData), sizeof(StaticAbi::FroxelCellHeader),
                      GraphUsage::ShaderRead, 0},
        FroxelBinding{sizeof(StaticAbi::FroxelIndicesData), sizeof(StaticAbi::FroxelLightIndex),
                      GraphUsage::ShaderRead, 1},
        FroxelBinding{sizeof(StaticAbi::FroxelGlobalLightsData), sizeof(StaticAbi::FroxelLightIndex),
                      GraphUsage::ShaderRead, 2},
    };

    FroxelUpload::FroxelUpload(std::shared_ptr<Allocation> allocation) noexcept : m_Allocation(std::move(allocation))
    {
    }

    std::optional<FroxelUpload> FroxelUpload::Prepare(ForwardDevice &device, UploadBatch &batch,
                                                      const PreparedView &view, const GraphBufferSupport &limits)
    {
        if (!view.froxels)
        {
            return std::nullopt;
        }

        constexpr auto constantBytes = Bindings[0].bytes;
        constexpr auto storageBytes = std::max({Bindings[1].bytes, Bindings[2].bytes, Bindings[3].bytes});
        if (limits.constantOffsetAlignment == 0 || constantBytes > std::min(limits.maxBytes, limits.maxConstantBytes))
        {
            Reject(ViewSettingReason::ScenePipelineUnavailable, "Froxel view constant block exceeds binding limits");
        }
        if (limits.structuredOffsetAlignment == 0 || storageBytes > std::min(limits.maxBytes, limits.maxStructuredBytes))
        {
            Reject(ViewSettingReason::ScenePipelineUnavailable, "Froxel structured block exceeds binding limits");
        }

        // 固定块容量在分配前已知. 先登记整组上传额度, 之后才取得驻留并生成拥有型载荷.
        constexpr auto bytes = Bindings[0].bytes + Bindings[1].bytes + Bindings[2].bytes + Bindings[3].bytes;
        batch.ReserveWrites(bytes, Bindings.size());
        try
        {
            // 源 span 只借用本轮 ViewWork. 复制结束后, 同一工作区可以为下一路或下一帧重新分箱.
            const auto &lists = *view.froxels;
            const std::array sources{std::as_bytes(std::span(&lists.grid, 1)), std::as_bytes(std::span(lists.headers)),
                                     std::as_bytes(std::span(lists.indices)),
                                     std::as_bytes(std::span(lists.globalLights))};
            device.Reserve(bytes);
            auto allocation = std::make_shared<Allocation>(device.capacity, bytes);
            std::array<BufferUpload, Bindings.size()> pending;
            for (std::size_t index = 0; index < Bindings.size(); ++index)
            {
                const auto &binding = Bindings[index];
                auto &write = pending[index];
                write.revision = allocation;
                write.buffer = AllocateBuffer(device.device, binding.usage, binding.bytes, binding.stride);
                write.source = GraphUploadSource::ForwardView;
                // 反射范围包括整个固定数组. 零有效行也需要定义绑定块的全部字节.
                assert(sources[index].size() <= binding.bytes);
                write.payload.resize(binding.bytes);
                std::ranges::copy(sources[index], write.payload.begin());
            }

            // 先完成容器分配和驻留记账, 再连续移入四项. 发布阶段只移动已就绪的载荷和引用.
            static_assert(std::is_nothrow_move_constructible_v<BufferUpload>);
            batch.buffers.reserve(batch.buffers.size() + pending.size());
            device.Use(*allocation);
            for (auto &write : pending)
            {
                batch.buffers.push_back(std::move(write));
            }
            return FroxelUpload(std::move(allocation));
        }
        catch (...)
        {
            // 末尾四次移入已有容量且不会抛异常. 进入此处时临时资源已析构, 只需撤回本组预留的额度.
            batch.bytes -= bytes;
            batch.writes -= Bindings.size();
            throw;
        }
    }

    GraphResult<FroxelGraphInputs> FroxelUpload::Resolve(const UploadBatch &batch,
        const std::map<RHI::Buffer *, BufferId> &versions) const noexcept
    {
        assert(batch.sealed);
        auto write = std::ranges::find(batch.buffers, m_Allocation, &BufferUpload::revision);
        FroxelGraphInputs inputs;
        for (auto &version : inputs.m_Versions)
        {
            // 场景上传入口拥有全批次的写入声明. 这里只从同组载荷定位它产生的读版本.
            if (write == batch.buffers.end() || write->revision != m_Allocation)
            {
                return GraphError{GraphErrorCode::InvalidResource, Core::BoundedText("Missing Froxel upload group")};
            }
            const auto written = versions.find(write->buffer.Get());
            if (written == versions.end() || written->second.version == 0)
            {
                return GraphError{GraphErrorCode::UndefinedContent, Core::BoundedText("Missing Froxel write version")};
            }
            version = written->second.AsReadOnly();
            ++write;
        }
        return inputs;
    }

    bool FroxelGraphInputs::DeclareReads(PassBuilder &builder) const
    {
        for (std::size_t index = 0; index < Bindings.size(); ++index)
        {
            const auto &binding = Bindings[index];
            if (!builder.Read(m_Versions[index], binding.usage, RHI::BufferRange{0, binding.bytes}))
            {
                return false;
            }
        }
        return true;
    }

    std::optional<std::array<RHI::BindingSetItem, 4>> FroxelGraphInputs::Bind(PassPreparation &preparation) const
    {
        std::array<RHI::BindingSetItem, Bindings.size()> result;
        for (std::size_t index = 0; index < Bindings.size(); ++index)
        {
            const auto &binding = Bindings[index];
            const auto item = preparation.ReadBuffer(binding.slot, m_Versions[index], binding.usage,
                                                     RHI::BufferRange{0, binding.bytes});
            if (!item)
            {
                return std::nullopt;
            }
            result[index] = *item;
        }
        return result;
    }
}
