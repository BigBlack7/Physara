#pragma once

/// @file
/// @brief 会话内的公共 RHI UI renderer. 从封存 DrawData 生成持有依赖并借用帧槽的批次.
/// renderer 管理批次的有效期, 纹理请求借用在本轮内结束.

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
        /// @pre drawData 及纹理请求来自当前封存帧. arena 及其区段存活到确认或取消.
        /// @note 上一准备值尚有效时拒绝新准备. 构建失败时撤回临时纹理身份和本次分配的尾部范围.
        [[nodiscard]] PrepareUiResult Prepare(const ImDrawData &drawData, RHI::Framebuffer *target,
                                              Platform::WindowExtent extent, Core::FrameArena &arena,
                                              const Renderer::NoSceneOutputs &outputs, const ViewportImage &viewport,
                                              const RHI::CommandListParameters &commands);

        /// @brief 在推进或销毁 context 前结束纹理请求的借用, 并使外部准备值失效.
        void CancelPending() noexcept;

        /// @brief 驻留注册与注销沿用纹理表的会话身份, 已准备的绑定继续由批次持有.
        [[nodiscard]] UiTextureResult RegisterResident(RHI::Texture &texture);
        [[nodiscard]] bool UnregisterResident(UiTextureId id) noexcept;
        /// @brief 为主视口注册稳定的逻辑身份, 具体颜色只在每次准备时解析.
        [[nodiscard]] UiTextureResult RegisterView(Renderer::ViewId view);

    private:
        friend class Physara::Editor::PreparedUiFrame;
        friend struct UiFrameData;

        UiRenderer(Platform::DeviceHost &host, RHI::Device &device) noexcept;
        [[nodiscard]] std::optional<Core::BoundedText> Initialize(const std::filesystem::path &packagePath);
        [[nodiscard]] bool PrepareBuffers(UiBatch &batch);
        [[nodiscard]] std::optional<Core::BoundedText> Record(UiBatch &batch, RHI::CommandList &commands);
        void Complete(UiFrameData &frame, bool accepted) noexcept;

        Platform::DeviceHost &m_Host; ///< 录制期间观察同一 Device 的同步失败回调.
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
