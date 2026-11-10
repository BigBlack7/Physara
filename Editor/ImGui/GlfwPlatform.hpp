#pragma once

/// @file
/// @brief Session 私有的 ImGui context 和 GLFW 输入适配.
/// 适配借用宿主窗口, 管理回调接合, 输入队列清理和 CPU UI 帧前置步骤.

#include <optional>

#include <Core/Error.hpp>

struct GLFWwindow;
struct ImGuiContext;

namespace Physara::Platform
{
    class DeviceHost;
    struct WindowState;
}

namespace Physara::Editor::Detail
{
    /// @brief 拥有会话的 ImGui context, 并在析构时先恢复宿主回调.
    /// @note 在宿主 owner 线程使用. 接入期间保持本 context 为当前 context, 宿主须活到适配析构.
    class GlfwPlatform final
    {
    public:
        GlfwPlatform() = default;
        ~GlfwPlatform();
        GlfwPlatform(const GlfwPlatform &) = delete;
        GlfwPlatform &operator=(const GlfwPlatform &) = delete;

        /// @brief 借用已就绪的宿主窗口, 创建 context 并接入官方 GLFW 回调链.
        /// @return 成功返回空值. 失败返回有界正文, 已取得的 UI 资源由适配析构释放.
        [[nodiscard]] std::optional<Core::BoundedText> Attach(Platform::DeviceHost &host);
        /// @brief 暂停接收键鼠和文本, 清理队列, 当前键鼠状态, 点击历史, 活动项及拖放载荷.
        /// @param focused 宿主当前发布的焦点末值, 用于同步 ImGui 焦点状态.
        void DiscardInput(bool focused);
        /// @brief 取当前窗口逻辑位置, 开放回调并把基线排在下一批键鼠事件前.
        /// @pre 适配已接入, 宿主已观察到普通模式, 下一批原生事件尚未处理.
        void ResumeInput();
        /// @brief 在已接入的适配上按当前发布开始一帧, 由 Session 构建布局并封存绘制数据.
        /// @param resumed 上一轮因窗口暂停跳过 UI 帧, 此时使用启动步长重建时间基线.
        void BeginFrame(const Platform::WindowState &window, bool resumed);

    private:
        GLFWwindow *m_Window = nullptr; ///< 借用自宿主, 仅在输入适配寿命内使用.
        ImGuiContext *m_Context = nullptr;
    };
}
