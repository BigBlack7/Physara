#pragma once

/// @file
/// @brief Editor 在宿主线程上的会话入口. 从一次窗口输入发布生成 CPU UI 帧.
/// Runtime 取得帧状态和可延迟应用的宿主请求. UI 绘制数据由会话内部持有.

#include <memory>
#include <optional>
#include <variant>

#include <Core/Error.hpp>
#include <Input.hpp>

namespace Physara::Platform
{
    class DeviceHost;
}

namespace Physara::Editor
{
    /// @brief 一次 Advance 的值结果. 暂停轮次也可以返回用于清理交互的宿主请求.
    struct EditorFrame
    {
        bool hasDrawData = false; ///< 本轮是否封存了新帧, 暂停期间为 false.
        /// @brief 失效轮次仍观测到相对模式时请求归还. 成功后在等待或轮询前通知会话.
        std::optional<Platform::HostRequest> hostRequest;
    };

    /// @brief 拥有单窗口 UI context, 工作区, 选择状态和输入适配, 借用已就绪的宿主.
    /// @note 在宿主 owner 线程创建, 推进和销毁. 会话必须在宿主关闭前析构以恢复原回调.
    class EditorSession final
    {
    public:
        using CreateResult = std::variant<std::unique_ptr<EditorSession>, Core::BoundedText>;

        /// @brief 在就绪宿主上安装唯一活动会话, 失败时释放本次已取得的 UI 状态.
        /// @return 可推进的独占会话或有界的初始化错误.
        /// @throws std::bad_alloc 会话或内部状态分配失败, 由组合入口统一报告.
        [[nodiscard]] static CreateResult Create(Platform::DeviceHost &host);
        ~EditorSession();
        EditorSession(const EditorSession &) = delete;
        EditorSession &operator=(const EditorSession &) = delete;

        /// @brief 消费一次继续运行的输入发布, 先处理失效边界再按窗口状态生成 UI.
        /// @pre 调用方已经处理终止条件, 每批按发布顺序最多调用一次, events 在调用期间有效.
        /// @return 当前 CPU 帧状态及可选的宿主请求. 暂停时 hasDrawData 为 false.
        /// @note Editor 内的 DrawData 借用止于下一次 UI 帧开始或会话销毁. 跨边界需取得数据副本.
        [[nodiscard]] EditorFrame Advance(const Platform::HostFrameInput &input);

        /// @brief 根据本轮 Advance 的窗口末值完成光标归还后的输入恢复.
        /// @pre 本轮归还请求已由宿主确认成功, 调用方尚未等待或轮询下一批事件.
        /// @note 已恢复焦点且可渲染时同步指针并开放输入. 仍失焦或暂停时等待有效发布.
        void CompleteCursorRelease();

    private:
        EditorSession();

        struct State;
        std::unique_ptr<State> m_State;
    };
}
