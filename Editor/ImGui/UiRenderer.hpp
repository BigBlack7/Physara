#pragma once

/// @file
/// @brief 会话内的公共 RHI/Graph UI renderer. 从封存 DrawData 复制载荷并声明上传和 Raster.
/// renderer 管理准备值和纹理协议的有效期, 物理采样绑定由图执行器按 Pass 权限建立.

#include <filesystem>
#include <memory>
#include <optional>
#include <variant>

#include <Input.hpp>

#include "Session/UiFrame.hpp"
#include "UiTextures.hpp"

namespace Physara::Platform
{
    class DeviceHost;
}

namespace Physara::Editor::Detail
{
    struct UiBatch;
    struct ViewportImage;

    /// @brief 持有固定 UI Pipeline, 可复用几何缓冲和本会话的纹理资源.
    /// @note 调用限宿主 owner 线程, 资源释放和回执处理早于 ImGui context 销毁.
    class UiRenderer final
    {
    public:
        using CreateResult = std::variant<std::unique_ptr<UiRenderer>, Core::BoundedText>;

        /// @brief 加载 ABI 匹配的 Shader 包并建立绘制资源, 成功后启用 ImGui renderer 能力.
        /// @pre 当前 context 为本会话的 context, 且尚未开始首个 NewFrame.
        /// desc.device 为 host 借出的 Device, 录制使用同一宿主的错误区间.
        [[nodiscard]] static CreateResult Create(Platform::DeviceHost &host, const UiRendererDesc &desc);
        ~UiRenderer();

        /// @brief 检查封存列表中是否有指定 token 的普通绘制命令, 且命令的像素裁剪框非空.
        /// @pre drawData 来自本次 ImGui::Render, token 为当前会话注册的颜色角色.
        /// @note 普通显示色已在布局时生成 Image 命令. 仅预留画布的表面预览使用下面的拟合检查.
        [[nodiscard]] static bool HasViewSample(const ImDrawData &drawData,
                                                Platform::WindowExtent extent, ImTextureID token) noexcept;
        /// @brief 与实际绘制共用栅格拟合及像素裁剪, 供原生 Shader 需求登记前检查.
        [[nodiscard]] static bool HasSurfacePreviewSample(const ImDrawData &drawData, Platform::WindowExtent extent,
                                                          const ViewportImage &preview, Renderer::ViewExtent raster) noexcept;

        /// @brief 测量本轮字节后统一预留帧槽. 复制像素和几何, 并封存本轮 draw 的采样依赖.
        /// @note 构图后才按存活上传核算 GPU 容量, 本入口只校验 CPU/驻留预算及固定几何资源.
        /// @pre drawData 及纹理请求来自当前封存帧. arena 及其区段存活到确认或取消.
        /// @note 上一准备值尚有效时拒绝新准备. 构建失败时撤回临时纹理身份和本次分配的尾部范围.
        /// SafeUi 为旧图像追加恢复标签, 不生成新的 View 像素或改变封存布局.
        [[nodiscard]] PrepareUiResult Prepare(const ImDrawData &drawData,
                                              Platform::WindowExtent extent, Core::FrameArena &arena,
                                              const Renderer::PreparedViewOutput &outputs, const ViewportImage &viewport,
                                              UiFrameMode mode, const ViewportImage &surfaceImage,
                                              SurfacePreviewInput surface);

        /// @brief 基础 atlas 曾由有效 UI 回执确认. 后续取消不撤销这一会话启动事实.
        [[nodiscard]] bool BaseFontReady() const noexcept { return m_BaseFontReady; }

        /// @brief 在推进或销毁 context 前结束纹理请求的借用, 并使外部准备值失效.
        void CancelPending() noexcept;

        /// @brief 驻留注册与注销沿用纹理表的会话身份, 图拥有当前导入, 已发绑定由 RHI 保活.
        [[nodiscard]] UiTextureResult RegisterResident(RHI::Texture &texture, UiTextureImport imported);
        [[nodiscard]] bool UnregisterResident(UiTextureId id) noexcept;
        /// @brief 为主 View 的一个颜色角色注册独立 token, 当前图构建时再解析对应的纹理版本.
        [[nodiscard]] UiTextureResult RegisterView(Renderer::ViewId view,
            Renderer::ViewOutputRole role = Renderer::ViewOutputRole::DisplayColor);

    private:
        friend class Physara::Editor::PreparedUiFrame;
        friend struct UiFrameData;

        UiRenderer(Platform::DeviceHost &host, RHI::Device &device) noexcept;
        /// @brief 建立与生成 ABI 匹配的固定 UI 资源, 失败时由尚未发布的 renderer 收拢部分对象.
        [[nodiscard]] std::optional<Core::BoundedText> Initialize(const std::filesystem::path &packagePath);
        /// @brief 为实际可见绘制准备有界几何缓冲, 两项都成功且无 Fatal 后才替换缓存.
        [[nodiscard]] bool PrepareBuffers(UiBatch &batch);
        /// @brief 将批次的上传和采样转换为同图版本, 成功回调写入 batch 的录制回执.
        /// @pre batch 仍属于活动准备值, 图执行结束前不能 Complete.
        [[nodiscard]] Renderer::GraphResult<Renderer::TextureId> BuildGraph(UiBatch &batch,
            Renderer::RenderGraph &graph, Renderer::TextureId color, Renderer::TextureId diagnostic,
            Renderer::TextureId surfacePreview, SurfacePreviewIssue previewIssue,
            FrameSlotUiFrameBindings &bindings, std::vector<Renderer::GraphUpload> &uploads,
            Renderer::GraphExecutionDiagnostics diagnostics);
        /// @brief 结束唯一活动批次的元数据借用. 空回执取消请求, 非空回执已核对整图成功.
        /// @note 同时使外部 PreparedUiFrame 失效, 已发 GPU 引用仍由原 RHI 包退休.
        void Complete(UiFrameData &frame, const UiSubmission *submission) noexcept;

        RHI::Device &m_Device; ///< 借用宿主的就绪 Device, 销毁 renderer 后才关闭宿主.
        Platform::DeviceHost &m_Host; ///< 只用于准备期的 Fatal 门控, 与 Device 共享同一 owner 寿命.
        UiTextures m_Textures;
        RHI::GraphicsPipelineHandle m_Pipeline;
        RHI::BufferHandle m_VertexBuffer;
        RHI::BufferHandle m_IndexBuffer;
        UiFrameData *m_Pending = nullptr; ///< 借用当前准备值, 完成或取消会同时解除双方引用.
        bool m_Attached = false;
        bool m_BaseFontReady = false; ///< 首个基础 atlas 上传获得有效回执后置位.
    };
}
