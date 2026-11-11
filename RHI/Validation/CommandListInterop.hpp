#pragma once

/// @file
/// @brief 宿主末端互操作的私有借用, 保持验证列表与底层闭合录制的对应关系.

#include "../RHI.hpp"

namespace Physara::RHI::Validation
{
    /// @brief 核对验证设备归属, Graphics 队列及包装列表的 Closed 阶段, 然后借出底层列表.
    /// @return 校验成功返回借用, 拒绝时返回空指针, 有设备时通过其回调报告原因.
    /// @note 由宿主 owner 线程使用. 借用止于包装列表的状态变化或释放, 提交仍使用原包装列表.
    [[nodiscard]] CommandList *BorrowClosedCommandList(Device *device, CommandList *commands);
}
