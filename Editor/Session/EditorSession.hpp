#pragma once

/// @file
/// @brief Editor 在宿主线程上的会话入口. 从窗口输入发布生成 CPU UI 帧.
/// 会话应用领域意图后向 Runtime 交付视口像素请求及独立的宿主请求.
/// 接入 renderer 时, 同一份封存 UI 还用于准备离屏 RHI 绘制.

#include <memory>
#include <optional>
#include <variant>

#include <Core/Error.hpp>
#include <DeviceHost.hpp>

#include "UiFrame.hpp"

namespace Physara::Editor
{
    /// @brief 一次 Advance 的值结果. 暂停轮次也可以返回用于清理交互的宿主请求.
    struct EditorFrame
    {
        bool hasDrawData = false; ///< 本轮是否封存了新帧, 暂停期间为 false.
        /// @brief 失效轮次仍观测到相对模式时请求归还. 成功后在等待或轮询前通知会话.
        std::optional<Platform::HostRequest> hostRequest;
        std::optional<Platform::SetWindowTitle> titleRequest; ///< 本轮确认的标题, 消费前保有正文.
        std::optional<Platform::SetWindowVSync> vsyncRequest;
        /// @brief 无可测量布局时为空. 有布局但像素换算失败时, 零范围供 View 报告拒绝.
        std::optional<Renderer::ViewportRequest> viewport; ///< 本轮主 View 与可见布局匹配时发布, 单位为离屏像素.
    };

    /// @brief 拥有单窗口 UI context, 工作区, 编辑历史, 输入适配和可选的 RHI renderer.
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

        /// @brief 消费继续运行的输入, 封存 UI 后应用本会话的字段编辑.
        /// @pre 调用方已处理终止条件. input 由本会话借用的宿主发布, 每批按发布顺序最多调用一次.
        /// events 在调用期间有效. 会话只将接入后到达的事件用于交互路由.
        /// @param view 可借用本会话登记的主 View. 面板取快照, 会话应用比例后生成本轮像素请求.
        /// 缺省或身份失配时仍可封存 UI, 可见视口由合成器显示占位.
        /// @param window 可借用活动宿主的窗口设置, 供本轮菜单生成请求和显示回执.
        /// @return 当前 CPU 帧状态, 本轮离屏及独立宿主请求. 暂停时 hasDrawData 为 false.
        /// @note 进入下一次 Advance 即结束前一轮封存数据的借用, 并取消未确认的离屏准备值.
        [[nodiscard]] EditorFrame Advance(const Platform::HostFrameInput &input, Renderer::View *view = nullptr,
                                           const Platform::WindowSettings *window = nullptr);

        /// @brief 为本轮封存帧准备离屏绘制, 将几何与上传字节复制到调用方的帧槽存储.
        /// @param extentPixels 与本次输入发布一致的实际像素尺寸. 无帧或零尺寸时不使用 target.
        /// @pre 在打开列表前调用. 非零目标为会话 Device 的单色单样本 SRGBA8 二维 framebuffer.
        /// arena 由调用方持有. 批次借用的字节区段须保持有效至 Finish 或 Cancel.
        /// outputs 来自本轮视口请求, 仅在准备调用期间借用.
        /// 颜色的 View 身份, 像素范围或比例请求代与封存请求失配时使用占位.
        /// @param commands 拟录制列表的参数. 上传预检采用它的最小 chunk 大小.
        /// @see PreparedUiFrame::Record
        /// @return 无工作, 可录制的准备值, 等待上传容量或有界原因. 同时只允许一份活动准备值.
        /// 等待时本次准备已结束, 之后可按届时的封存帧和可用容量重试.
        [[nodiscard]] PrepareUiResult PrepareUiFrame(RHI::Framebuffer *target, Platform::WindowExtent extentPixels,
                                                     Core::FrameArena &arena,
                                                     const Renderer::NoSceneOutputs &outputs = {},
                                                     const RHI::CommandListParameters &commands = {});

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
