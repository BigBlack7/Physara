#pragma once

/// @file
/// @brief Editor 在宿主线程上的会话入口. 从窗口输入发布生成 CPU UI 帧.
/// 会话应用领域意图后向 Runtime 交付视口像素请求及独立的宿主请求.
/// 接入 renderer 时, 同一份封存 UI 还用于准备图内上传与合成, 会话不保存本帧 GraphId.
/// 装配 Scene 后还拥有单文档操作与选择, 文件结果在 CPU 输入边界整体发布.

#include <array>
#include <memory>
#include <optional>
#include <variant>

#include <Core/Error.hpp>
#include <DeviceHost.hpp>

#include "UiFrame.hpp"
#include "SceneDocumentTypes.hpp"

namespace Physara::Editor
{
    struct FrameStatistics;

    /// @brief 一次 Advance 的值结果. 暂停轮次也可以请求归还光标.
    struct EditorFrame
    {
        bool hasDrawData = false; ///< 本轮是否封存了新帧, 暂停期间为 false.
        /// @brief 请求相对捕获或归还. 只有归还成功后在等待或轮询前通知会话.
        std::optional<Platform::HostRequest> hostRequest;
        std::optional<Platform::SetWindowTitle> titleRequest; ///< 本轮确认的标题, 消费前保有正文.
        std::optional<Platform::SetWindowVSync> vsyncRequest;
        /// @brief 无可测量布局时为空. 有布局但像素换算失败时, 零范围供 View 报告拒绝.
        std::optional<Renderer::ViewportRequest> viewport; ///< 本轮主 View 与可见布局匹配时发布, 单位为离屏像素.
        std::optional<Renderer::SurfacePreviewMode> surfacePreview; ///< 只用于当前封存 UI 的会话选择.
    };

    /// @brief 拥有单窗口 UI context, 工作区, 文档与编辑历史, 输入适配和可选的 RHI renderer.
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

        /// @brief 在首帧前接入实际项目资源根和活动 Scene. 会话拥有 Scene 并负责其整体替换.
        /// @note 只能接入一次, View 须与会话主视口一致. root 在实际文件操作时核对物理位置.
        [[nodiscard]] std::optional<Core::BoundedText>
            AttachSceneDocument(std::filesystem::path root, std::unique_ptr<Scene::Scene> scene,
                                const Renderer::View &view);
        /// @brief 返回活动 Scene 的只读借用, 成功 New/Open 或会话销毁后失效; 未接入时为空.
        [[nodiscard]] const Scene::Scene *ActiveScene() const noexcept;
        /// @brief 结束本次活动文档的变更分发, 后续事务从当前内容代开始累积.
        /// @pre Runtime 已将同一活动 Scene 的变更转给全部槽, 包括尚未取得的槽.
        void AcknowledgeSceneRenderChanges() noexcept;
        /// @brief 复制当前文档状态, 主 View 失配或未接入文档时为空.
        [[nodiscard]] std::optional<SceneDocumentStatus> SceneStatus(const Renderer::View &view) const;
        [[nodiscard]] std::optional<Scene::EntityHandle> Selection() const noexcept;
        /// @brief 只接受活动 Scene 中仍有效的 handle, 空值清除选择; 选择不改变文档保存版本.
        [[nodiscard]] bool Select(std::optional<Scene::EntityHandle> entity) noexcept;

        /// @brief CPU 编辑边界的类型化入口. 当前文档的有效修改和历史共享同一提交路径.
        /// @note Open 冻结期间拒绝文档编辑. 删除或撤销后退役无效选择, 相机回退由同一历史事务处理.
        [[nodiscard]] std::optional<Core::BoundedText>
            EditScene(Scene::SceneId scene, Renderer::View &view, const Scene::Command &command);
        [[nodiscard]] std::optional<Core::BoundedText>
            EditMainView(Scene::SceneId scene, Renderer::View &view, Renderer::ViewDocumentRequests requests);
        /// @brief 为活动文档和主 View 撤销或重做会话历史.
        /// @note Open 期间可重放会话显隐. 冻结的文档条目保留原状态和历史游标.
        /// @return 成功, 无可重放记录或文档条目被 Open 门控时返回空, 后两者不移动游标.
        /// 身份失配, 会话停机或领域重放失败时返回原因.
        [[nodiscard]] std::optional<Core::BoundedText>
            ReplayEdit(Scene::SceneId scene, Renderer::View &view, bool undo);

        /// @brief 接收本轮文件意图. 打开和保存的异步结果由 CPU 输入边界推进.
        /// @return 空值表示意图已接受, 不代表异步 I/O 已成功; 后续进度和错误由 SceneStatus 返回.
        [[nodiscard]] std::optional<Core::BoundedText>
            RequestSceneFile(const SceneFileRequest &request, Renderer::View &view);
        /// @brief 保存当前快照后尝试指定的 New/Open. 仅实际成功且版本匹配才继续.
        /// @note 已有路径时使用当前路径, 否则使用 saveAs. 接受返回不表示原替换已经完成.
        /// 保存期间的新编辑使继续意图失效, 后续原因由 SceneStatus 返回.
        [[nodiscard]] std::optional<Core::BoundedText> SaveBeforeReplacing(SceneFileRequest replacement,
            const std::filesystem::path &saveAs, Renderer::View &view);
        /// @brief 按操作编号请求取消, 进入替换提交后返回不可取消原因.
        [[nodiscard]] std::optional<Core::BoundedText> CancelSceneFile(std::uint64_t operation) noexcept;
        /// @brief 为当前可编辑文档请求静态模型. 成功准入后由 CPU 边界提交实例.
        [[nodiscard]] std::optional<Core::BoundedText> RequestModelImport(Asset::AssetManager &assets,
            Scene::SceneId scene, const std::filesystem::path &path, std::optional<std::uint32_t> sourceScene = {});
        [[nodiscard]] const SceneImportStatus &ImportStatus() const noexcept;
        /// @brief 取消匹配的实例意图, 其它消费者可继续使用共享解码结果.
        [[nodiscard]] bool CancelModelImport(std::uint64_t operation) noexcept;
        /// @brief 在资产发布后推进文档, 模型实例和待应用的纹理或环境来源, 窗口暂停时也可调用.
        /// @note 先处理文档替换与冻结, 再将仍属于活动 Scene 的候选交给共享编辑历史. 活动手势期间等待.
        void AdvanceSceneDocument(Asset::AssetManager &assets, Renderer::View &view);
        /// @brief 所有退出路径在销毁发布目标前收拢后台文件任务.
        void ShutdownSceneDocument() noexcept;

        /// @brief 消费继续运行的输入, 封存 UI 后应用本会话的字段编辑.
        /// @pre 调用方已处理终止条件. input 由本会话借用的宿主发布, 每批按发布顺序最多调用一次.
        /// events 在调用期间有效. 会话只将接入后到达的事件用于交互路由.
        /// @param view 可借用本会话登记的主 View. 面板取快照, 会话应用比例后生成本轮像素请求.
        /// 缺省或身份失配时仍可封存 UI, 可见视口由合成器显示占位.
        /// @param assets 供本轮模型和物理资源路径请求使用. 已准入的结果由 AdvanceSceneDocument 推进.
        /// @param window 可借用活动宿主的窗口设置, 供本轮菜单生成请求和显示回执.
        /// @param statistics 本次布局借用的最近正常场景统计和最近就绪的 GPU 区间. 两者各自带提交身份.
        /// @return 当前 CPU 帧状态, 本轮离屏及独立宿主请求. 暂停时 hasDrawData 为 false.
        /// @note 进入下一次 Advance 即结束前一轮封存数据的借用, 并取消未确认的离屏准备值.
        [[nodiscard]] EditorFrame Advance(const Platform::HostFrameInput &input, Renderer::View *view = nullptr,
            const Platform::WindowSettings *window = nullptr, Asset::AssetManager *assets = nullptr,
            const FrameStatistics *statistics = nullptr);

        /// @brief 冻结两套布局、场景身份及内容/相机版本、View 文档代和预览模式, 供正常合成匹配候选.
        /// @pre 非空输入来自本次 Advance 后的 View 准备.
        void SetPreparedViewport(const Renderer::PreparedView *prepared) noexcept;
        /// @brief 记录本轮由正常图实际采样并发布的图像依据. 空值使旧输入映射失效.
        /// @pre 非空输入来自本轮实际接受的正常输出, View 已发布该颜色.
        /// @note 只保存关联布局和身份值, 无 PreparedView 或图的借用.
        /// 候选准备与实际接受分别由 SetPreparedViewport 和本入口管理, 安全 UI 不授予新命中资格.
        void SetAcceptedViewport(const Renderer::PreparedView *accepted, const Renderer::View &view) noexcept;
        /// @brief 将本轮逻辑窗口位置映射为左上原点的有效图像 UV. 裁掉的区域和留边返回空.
        /// @note 仅在本轮布局有匹配的正常已接受图像时可用; 输入归属由调用者另行判断.
        [[nodiscard]] std::optional<std::array<double, 2>> ViewportImageUv(double x, double y) const noexcept;

        /// @brief 为本轮封存帧准备图内合成, 将几何与上传字节复制到调用方的帧槽存储.
        /// @param extentPixels 与本次输入发布一致的实际像素尺寸. 无帧或零尺寸返回空值.
        /// @pre 在构图和打开列表前调用. 目标格式及像素范围由 BuildGraph 核对.
        /// arena 由调用方持有. 批次借用的字节区段须保持有效至 Finish 或 Cancel.
        /// @param outputs 仅在准备期间借用. 正常候选须匹配 View 身份与比例请求代.
        /// 测试图核对渲染布局, 场景色核对两套布局和本轮准备的生产来源. 失配时改用占位并撤销命中.
        /// @param mode 上传压力模式重建占位; SafeUi 只接受同一 View 的只读旧输出快照或无输出.
        /// 安全图按当前矩形显示旧尺寸并标注恢复状态, 不能把它发布为本轮请求结果.
        /// 重建候选前须先销毁旧图并取消旧准备值, 然后再清理帧槽载荷和绑定.
        /// @see PreparedUiFrame::BuildGraph
        /// @return 无工作, 可构图的准备值或有界原因. 同时只允许一份活动准备值.
        [[nodiscard]] PrepareUiResult PrepareUiFrame(Platform::WindowExtent extentPixels,
                                                     Core::FrameArena &arena,
                                                     const Renderer::PreparedViewOutput &outputs = {},
                                                     UiFrameMode mode = UiFrameMode::Normal,
                                                     SurfacePreviewInput surface = {});

        /// @brief 只读检查本轮封存的 Viewport token 是否仍有实际像素消费者.
        [[nodiscard]] bool HasViewportSample(Platform::WindowExtent extentPixels) const noexcept;
        /// @brief 检查按本轮栅格拟合后的预览图像是否仍覆盖实际像素.
        [[nodiscard]] bool HasSurfacePreviewSample(Platform::WindowExtent extentPixels,
                                                   Renderer::ViewExtent raster) const noexcept;
        /// @brief 仅记录已接受的预览历史. 本轮占位原因随对应图内采样选择显示.
        void ReportSurfacePreview(const SurfacePreviewStatus &status) noexcept;
        /// @brief 基础字体曾由完整有效 UI 提交接受, 用于区分启动必需项失败与普通帧取消.
        [[nodiscard]] bool BaseFontReady() const noexcept;
        /// @brief 保存本轮失败的拥有型快照. 无新失败时只更新已有记录的恢复状态.
        /// @note 最迟在下一次可绘制 Advance 的工作区显示, 不重新推进当前 ImGui 帧.
        void ReportFrameOutcome(FrameRecovery recovery, const std::optional<Core::BoundedText> &primary = {},
                                const std::optional<Core::BoundedText> &additional = {}) noexcept;

        /// @brief 注册已初始化的单样本 RGBA8/SRGBA8 二维 SRV, 以实际格式解释采样域.
        /// @pre 资源来自会话的 Device, owner 提供真实状态和定义过的允许采样范围.
        /// @return 本会话的逻辑身份或有界错误. 会话持有纹理, 绑定留到图执行期创建.
        [[nodiscard]] UiTextureResult RegisterUiTexture(RHI::Texture &texture, UiTextureImport imported);

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
