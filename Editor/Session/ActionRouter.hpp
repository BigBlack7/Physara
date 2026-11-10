#pragma once

/// @file
/// @brief 使用当帧 UI 占用和宿主观察解释业务输入归属.
/// 原生回调链负责向 ImGui 和宿主投递, 本层只为 Editor 动作选择消费者.

#include <cstdint>

#include <Input.hpp>

namespace Physara::Editor::Detail
{
    /// @brief 一条业务输入的唯一归属. None 表示本层没有消费者, Ui 表示交还给已接收回调的 UI.
    enum class InputOwner : std::uint8_t
    {
        None,
        Ui,
        Gizmo,
        Camera,
        Viewport,
        Workspace
    };

    /// @brief Session 的整批占用声明, 加上宿主观察到的光标模式.
    /// 布局释放的键盘占用保留到本批结束. 当前弹出层占用两类输入, Gizmo 只有活动时才参与.
    struct InputClaims
    {
        bool blockingPopup = false;
        bool uiKeyboard = false;
        bool uiPointer = false;
        bool gizmoActive = false;
        /// @brief 宿主发布的生效模式. Relative 表示可以交给相机通道的捕获状态.
        Platform::CursorMode cursorMode = Platform::CursorMode::Normal;
        /// @brief 当前逻辑鼠标位置命中未被遮挡的视口内容区.
        bool viewportHit = false;
    };

    /// @brief 为单个键鼠事件选择唯一归属, 调用方按返回值派发动作.
    /// @pre 当前发布已通过焦点和暂停检查.
    /// @return 窗口事实和文本返回 None, 继续由各自的发布或 UI 消费路径处理.
    [[nodiscard]] InputOwner ResolveInputOwner(const InputClaims &claims, const Platform::InputEvent &event) noexcept;

    /// @brief 识别批次内进入失焦, 最小化或零像素的边沿, 与发布末值分开判断.
    [[nodiscard]] bool InterruptsInput(const Platform::InputEvent &event) noexcept;
}
