#pragma once

/// @file
/// @brief Editor 在宿主线程上的会话入口. 从窗口输入发布生成 CPU UI 帧.
/// Runtime 取得帧状态和宿主请求. 接入 renderer 的会话还可准备离屏 RHI 绘制.

#include <memory>
#include <optional>
#include <variant>

#include <Core/Error.hpp>
#include <Input.hpp>

#include "UiFrame.hpp"

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

    /// @brief 拥有单窗口 UI context, 工作区, 输入适配及可选的 RHI renderer, 借用已就绪的宿主.
    /// @note 在宿主 owner 线程创建, 推进和销毁. 会话必须在宿主关闭前析构以恢复原回调.
    class EditorSession final
    {
    public:
        using CreateResult = std::variant<std::unique_ptr<EditorSession>, Core::BoundedText>;

        /// @brief 在就绪宿主上安装唯一活动的 CPU UI 会话并预建字体 atlas.
        /// @return 可推进的独占会话或有界的初始化错误.
        /// @throws std::bad_alloc 会话或内部状态分配失败, 由组合入口统一报告.
        [[nodiscard]] static CreateResult Create(Platform::DeviceHost &host);

        /// @brief 在首个 UI 帧前装配公共 RHI renderer 和固定 ImGui 纹理协议.
        /// @pre renderer.device 为就绪宿主借出的 Device, Shader 包路径在创建期间有效.
        /// @return 成功会话或有界初始化错误. 失败时逆序释放已取得的对象.
        /// @throws std::bad_alloc 会话或 renderer 装配期间的普通 CPU 分配失败.
        [[nodiscard]] static CreateResult Create(Platform::DeviceHost &host, const UiRendererDesc &renderer);
        ~EditorSession();
        EditorSession(const EditorSession &) = delete;
        EditorSession &operator=(const EditorSession &) = delete;

        /// @brief 消费一次继续运行的输入发布, 先处理失效边界再按窗口状态生成 UI.
        /// @pre 调用方已经处理终止条件, 每批按发布顺序最多调用一次, events 在调用期间有效.
        /// @return 当前 CPU 帧状态及可选的宿主请求. 暂停时 hasDrawData 为 false.
        /// @note 进入下一次 Advance 即结束前一轮封存数据的借用, 并取消未确认的离屏准备值.
        [[nodiscard]] EditorFrame Advance(const Platform::HostFrameInput &input);

        /// @brief 为本轮封存帧准备离屏绘制, 持有目标及本轮几何和上传数据.
        /// @param extentPixels 与本次输入发布一致的实际像素尺寸. 无帧或零尺寸时不使用 target.
        /// @pre 非零目标为会话 Device 的单色单样本 SRGBA8 二维 framebuffer.
        /// @see PreparedUiFrame::Record
        /// @return 无工作, 可录制的准备值或有界原因. 同时只允许一份活动准备值.
        [[nodiscard]] PrepareUiResult PrepareUiFrame(RHI::Framebuffer *target, Platform::WindowExtent extentPixels);

        /// @brief 注册已初始化的单样本 RGBA8/SRGBA8 二维 SRV, 以实际格式解释采样域.
        /// @pre 资源来自会话的 Device, 采样内容和可跟踪前态由调用者准备.
        /// @return 本会话的逻辑身份或有界错误. 注册成功后会话持有纹理和采样绑定.
        [[nodiscard]] UiTextureResult RegisterUiTexture(RHI::Texture &texture);

        /// @brief 结束驻留纹理的新解析, 已准备或已发的引用沿各自的 owner 退休.
        /// @return 本次注销有效的驻留条目时为 true.
        [[nodiscard]] bool UnregisterUiTexture(UiTextureId id) noexcept;

        /// @brief 根据本轮 Advance 的窗口末值完成光标归还后的输入恢复.
        /// @pre 本轮归还请求已由宿主确认成功, 调用方尚未等待或轮询下一批事件.
        /// @note 已恢复焦点且可渲染时同步指针并开放输入. 仍失焦或暂停时等待有效发布.
        void CompleteCursorRelease();

    private:
        EditorSession();
        [[nodiscard]] static CreateResult CreateImpl(Platform::DeviceHost &host, const UiRendererDesc *renderer);

        struct State;
        std::unique_ptr<State> m_State;
    };
}
