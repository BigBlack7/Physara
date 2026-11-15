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

        /// @brief 测量本轮字节后统一预留帧槽. 复制像素和几何, 并封存本轮 draw 的采样依赖.
        /// @note 构图后才按存活上传核算 GPU 容量, 本入口只校验 CPU/驻留预算及固定几何资源.
        /// @pre drawData 及纹理请求来自当前封存帧. arena 及其区段存活到确认或取消.
        /// @note 上一准备值尚有效时拒绝新准备. 构建失败时撤回临时纹理身份和本次分配的尾部范围.
        [[nodiscard]] PrepareUiResult Prepare(const ImDrawData &drawData,
                                              Platform::WindowExtent extent, Core::FrameArena &arena,
                                              const Renderer::NoSceneOutputs &outputs, const ViewportImage &viewport,
                                              bool deferDiagnostic);

        /// @brief 在推进或销毁 context 前结束纹理请求的借用, 并使外部准备值失效.
        void CancelPending() noexcept;

        /// @brief 驻留注册与注销沿用纹理表的会话身份, 图拥有当前导入, 已发绑定由 RHI 保活.
        [[nodiscard]] UiTextureResult RegisterResident(RHI::Texture &texture, UiTextureImport imported);
        [[nodiscard]] bool UnregisterResident(UiTextureId id) noexcept;
        /// @brief 为主视口注册稳定的逻辑身份, 具体颜色只在每次准备时解析.
        [[nodiscard]] UiTextureResult RegisterView(Renderer::ViewId view);

    private:
        friend class Physara::Editor::PreparedUiFrame;
        friend struct UiFrameData;

        explicit UiRenderer(RHI::Device &device) noexcept;
        [[nodiscard]] std::optional<Core::BoundedText> Initialize(const std::filesystem::path &packagePath);
        [[nodiscard]] bool PrepareBuffers(UiBatch &batch);
        [[nodiscard]] Renderer::GraphResult<Renderer::TextureId> BuildGraph(UiBatch &batch,
            Renderer::RenderGraph &graph, Renderer::TextureId color, Renderer::TextureId diagnostic,
            FrameSlotUiFrameBindings &bindings, std::vector<Renderer::GraphUpload> &uploads,
            Renderer::GraphExecutionDiagnostics diagnostics);
        void Complete(UiFrameData &frame, const UiSubmission *submission) noexcept;

        RHI::Device &m_Device; ///< 借用宿主的就绪 Device, 销毁 renderer 后才关闭宿主.
        UiTextures m_Textures;
        RHI::GraphicsPipelineHandle m_Pipeline;
        RHI::BufferHandle m_VertexBuffer;
        RHI::BufferHandle m_IndexBuffer;
        UiFrameData *m_Pending = nullptr; ///< 借用当前准备值, 完成或取消会同时解除双方引用.
        bool m_Attached = false;
        bool m_BaseFontReady = false; ///< 首个基础 atlas 上传获得有效回执后置位.
    };
}
