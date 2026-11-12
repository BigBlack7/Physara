#pragma once

/// @file
/// @brief 管理主窗口固定面板及布局动作. 面板从会话借用只读的选择身份.
/// 工作区保存面板显隐, 停靠和组合状态由该会话的 ImGui context 管理.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include <Core/Identity.hpp>
#include <Input.hpp>

#include "Viewport/Viewport.hpp"

namespace Physara::Editor::Detail
{
    struct InputClaims;
    struct SelectionIdentityTag;
    /// @brief 按 Tag 隔离的选择身份, 外层 optional 表示当前是否有选择.
    using SelectionId = Core::Identity<SelectionIdentityTag>;

    /// @brief 已注册的布局动作. 编号对应一个显隐槽位, Count 表示槽位数.
    enum class ActionId : std::uint8_t
    {
        ToggleViewport,
        ToggleHierarchy,
        ToggleInspector,
        Count
    };

    /// @brief 会话内的工作区显隐状态, 菜单与快捷键共用布局修改入口.
    class Workspace final
    {
    public:
        /// @brief 提交布局菜单, 停靠区和可见面板. 选择身份仅在调用期间借用.
        /// @pre 当前会话的 ImGui 帧已开始.
        /// @return 当前视口的逻辑命中及可选的离屏像素请求. 未绘制视口时返回空布局.
        [[nodiscard]] ViewportLayout Draw(const std::optional<SelectionId> &selection, const Viewport &viewport,
                                           const Platform::WindowState &window);
        /// @brief 依到达顺序只从工作区拥有的 Press 产生一次性布局动作.
        /// @pre events 来自本次有效发布, claims 由该轮布局提交后取得.
        /// @note 本次 Draw 之后触发的显隐变更由下一次 Draw 消费.
        void ApplyShortcuts(std::span<const Platform::InputEvent> events, const InputClaims &claims) noexcept;

    private:
        /// @brief 切换一个已注册面板的显隐槽位, 供菜单和快捷键调用.
        void Toggle(ActionId action) noexcept;

        std::array<bool, static_cast<std::size_t>(ActionId::Count)> m_Visible{true, true, true};
    };
}
