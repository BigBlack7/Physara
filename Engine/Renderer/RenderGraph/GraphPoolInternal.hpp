#pragma once

/// @file
/// @brief 图执行和跨帧导入共用的物理池项. 持有仅说明 CPU 寿命, GPU 事实来自录制包.

#include <ImmediateGraphics.hpp>

#include "GraphResourcePool.hpp"

namespace Physara::Renderer::Detail
{
    /// @brief 仅可原地更新的 Texture 需要跨图内容账目. 物理状态和完成仍由录制观察提供.
    /// 每个 mip/layer 分别保存实际定义的 aspect. 弃包无实发工作时可回到前次已接受的覆盖.
    class GraphMutableTextureContent final
    {
    public:
        explicit GraphMutableTextureContent(const RHI::TextureDesc &desc);
        /// @brief 开始新的物理录制时保存入口覆盖. 若上个包无工作取消, 从它的入口恢复.
        void BeginRecording(bool restorePrior) noexcept;
        /// @brief 仅设置或清除所选 mip/layer 的 aspect 位, 其余单元沿用原覆盖.
        void SetDefined(const TextureRange &range, bool defined) noexcept;
        /// @brief 查询整段是否全部定义. usePrior 选择无工作弃包前的覆盖.
        [[nodiscard]] bool Covers(const TextureRange &range, bool usePrior) const noexcept;
        /// @brief 提取请求与有效覆盖的交集, 供新图登记版本 0. 返回值可含多个不连续范围.
        [[nodiscard]] std::vector<TextureRange> DefinedRanges(const TextureRange &range, bool usePrior) const;
        /// @brief 读者强持有精确范围, 直到相应只读出口全部释放. 池只弱引用这些登记.
        [[nodiscard]] std::shared_ptr<const TextureRange> Pin(const TextureRange &range);
        /// @brief 在接管前查询仍存活的只读出口, 只要分量和子资源都相交就须保护它.
        [[nodiscard]] bool HasReader(const TextureRange &range) const noexcept;

    private:
        [[nodiscard]] std::size_t Cell(std::uint32_t layer, std::uint32_t mip) const noexcept
        {
            return std::size_t(layer) * m_Mips + mip;
        }

        std::uint32_t m_Mips;
        std::vector<TextureAspect> m_Current, m_Prior;
        std::vector<std::weak_ptr<const TextureRange>> m_Readers;
    };

    /// @brief 池账本中的一个完整物理槽. 准入时确定描述与额度, 首次执行到该槽才填写 object.
    /// 同一表项还可由预留和跨帧出口共同持有. 它们解除持有后仍须检查录制事实和 RHI 依赖.
    struct GraphPoolEntry
    {
        GraphPoolEntry(GraphResourcePool &pool, RHI::Device &device, const PlannedGraphSlot &slot)
            : owner(pool), device(&device), desc(slot.desc), bytes(slot.bytes)
        {
        }

        GraphResourcePool &owner;
        RHI::DeviceHandle device;
        decltype(PlannedGraphSlot::desc) desc;
        std::uint64_t bytes;
        RHI::ResourceHandle object;
        std::shared_ptr<const RHI::GraphicsResourceUse> use; ///< 最近登记的包, 可能还没有接受票据.
        std::shared_ptr<const RHI::GraphicsResourceUse> prior; ///< 登记后继包前保留的已接受状态来源.
        std::optional<GraphMutableTextureContent> mutableContent;
        bool reserved{}; ///< 此槽已分给一份仍存活的准入预留.

        /// @brief 返回可作为跨录制入口的观察. 最新包已发工作却没有票据时返回空.
        /// 无工作取消才允许沿用 prior. Share 供跨调用持有, AcceptedUse 只借到下次登记.
        [[nodiscard]] std::shared_ptr<const RHI::GraphicsResourceUse> ShareAcceptedUse() const noexcept;
        [[nodiscard]] const RHI::GraphicsResourceUse *AcceptedUse() const noexcept;
        /// @brief 最新有效观察尚无完成证明时为 true, 包括尚未取得接受票据的录制.
        [[nodiscard]] bool Pending() const noexcept;
        /// @brief 只报告仍需交接的录制, 区别于已接受包尚未完成.
        [[nodiscard]] bool AwaitingSubmission() const noexcept;
        /// @brief 登记同一对象的下一次实际录制, 先前已经接受的同队列工作无需等待.
        [[nodiscard]] bool Observe(RHI::CommandList &commands);
    };

    /// @brief 返回快照中对应物理 mip/layer 的状态.
    inline RHI::ResourceStates TextureStateAt(const RHI::GraphicsResourceUse &use, const RHI::TextureDesc &desc,
                                              std::uint32_t layer, std::uint32_t mip) noexcept
    {
        return use.states[std::size_t(layer) * desc.mipLevels + mip];
    }
}
