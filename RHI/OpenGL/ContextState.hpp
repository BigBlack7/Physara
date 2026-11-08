#pragma once

/// @file
/// @brief context 的唯一原生绑定/固定状态缓存, 不代替逻辑 Graphics 状态, 用途 tracker 或 GPU 完成.

#include <array>
#include <vector>

#include <glad/gl.h>

#include "../RHI.hpp"

namespace Physara::RHI::OpenGL
{
    class OpenGLDevice;
    class OpenGLInputLayout;

    /// @brief 实际颜色通路是否消费 blend 因子, 准备拒绝与原生映射共用相同的 inactive 语义.
    struct BlendFactorUse
    {
        bool color = false; ///< 至少一个实际可写 RGB 分量的方程需要因子.
        bool alpha = false; ///< 实际可写 alpha 分量的方程需要因子, 与 RGB 通路独立.
    };

    /// @brief 排除关闭混合, 整数附件, Min/Max 与未写分量, 不查询或改变 GL 状态.
    /// @pre format 与固定状态枚举已在准备边界确认有效; 结果同时供 Pipeline 拒绝和 native 因子选择使用.
    [[nodiscard]] BlendFactorUse GetBlendFactorUse(const BlendState::RenderTarget &target, Format format) noexcept;

    /// @brief owner context 的有界 native 状态与辅助 VAO/FBO, 外部互操作后失效并按需重建.
    /// @note 所有 Apply/Release 操作由调用方保证 current context; 不查询并恢复外部状态.
    class ContextState
    {
    public:
        /// @brief count 构造让 MSVC Debug proxy 的可传播分配失败进入 Device 工厂边界.
        ContextState() : m_UniformRanges(0), m_StorageRanges(0), m_TextureUnits(0)
        {
        }
        /// @brief 预留实际 limits 对应缓存并创建唯一 VAO/清除 FBO, 失败由 Device 工厂回收.
        [[nodiscard]] bool Initialize(OpenGLDevice &device);
        /// @brief 在已收拢工作后解除绑定并删除辅助对象, context 失效时只清 CPU 身份.
        void Shutdown(OpenGLDevice &device, bool nativeAvailable) noexcept;
        /// @brief 仅标记缓存未知, 不发出 GL 调用或修改资源状态.
        void Invalidate() noexcept;
        /// @brief 原生删除前解除可能扣留旧对象的绑定, 保留逻辑状态供下一次 Draw 重建.
        void ReleaseBindings() noexcept;
        /// @brief 建立紧凑 texel 的标准 unpack 状态并按需绑定 PBO, 调用者保证 current context.
        void ApplyUnpack(GLuint buffer) noexcept;
        /// @brief 冷路径客户端查询结果必须使用指针语义, 外部互操作失效后重新解绑 query buffer.
        /// @pre 调用方已通过 BeginNative, 随后仍须 CheckNative 确认本批状态/结果读取.
        void ApplyQueryRead() noexcept;
        /// @brief 绑定 Program, 按活动 PointSize 输出选择 Shader 点大小, 否则恢复单位点大小.
        void ApplyProgram(GLuint program, bool pointSize) noexcept;
        /// @brief 同时建立读/写 FBO 绑定; 本 profile 的 DSA clear/copy 不另建一份绑定缓存.
        void ApplyFramebuffer(GLuint framebuffer) noexcept;
        /// @brief 绑定已验证的 UBO/SSBO 字节范围, unit 来自本 Pipeline 计划, 不持有对应 wrapper.
        void ApplyBufferRange(bool storage, GLuint unit, GLuint name, GLintptr offset, GLsizeiptr size) noexcept;
        /// @brief 独立比较同一物理单元的 texture 与 sampler, 任一端改变都更新实际组合.
        void ApplyTexture(GLuint unit, GLuint texture, GLuint sampler) noexcept;
        /// @brief 绑定唯一 core VAO 并设置完整属性启用/格式计划, 空布局也取得合法 VAO.
        void ApplyInput(const OpenGLInputLayout *layout) noexcept;
        /// @brief 更新已验证 slot 的 Buffer/字节 offset/stride/divisor; 须先 ApplyInput 取得 VAO.
        void ApplyVertexBuffer(GLuint slot, GLuint name, GLintptr offset, GLsizei stride, GLuint divisor) noexcept;
        /// @brief 更新唯一 VAO 的 IB, 零解绑使用已绑定 VAO 的 core 入口, 不更改格式或 Draw offset.
        void ApplyIndexBuffer(GLuint name) noexcept;
        /// @brief 应用已经由 Pipeline/候选状态验证的普通固定状态及实际目标编码.
        void ApplyRenderState(const RenderState &state, const FramebufferInfo &info, const Color &blendColor,
                              std::uint8_t stencilRef) noexcept;
        /// @brief 应用已验证的 viewport/depth range, 坐标与逻辑纹理 row 0 对应, 不按窗口高度翻转.
        void ApplyViewport(const Viewport &viewport) noexcept;
        /// @brief 应用已验证的单 scissor 区域, 启用状态由固定 raster 状态决定.
        void ApplyScissor(const Rect &rect) noexcept;
        /// @brief 为完整子资源 typed clear 建立无 scissor/完整 mask 状态, 下次 Draw 重新应用固定状态.
        void ApplyClearState(bool srgb) noexcept;
        [[nodiscard]] GLuint ClearFramebuffer() const noexcept
        {
            return m_ClearFramebuffer;
        }

    private:
        // known=false 表示必须重新发出 native 设置; 保存的名称只是缓存, 不承担资源保活.
        struct BufferRangeState
        {
            GLuint name = 0;
            GLintptr offset = 0;
            GLsizeiptr size = 0;
            bool known = false;
        };
        struct TextureUnitState
        {
            GLuint texture = 0;
            GLuint sampler = 0;
            bool known = false;
        };
        // 属性格式与动态 Buffer 绑定分别比较, 避免按每次 VB/IB 组合创建新的 VAO.
        struct AttributeState
        {
            GLuint binding = 0;
            GLuint offset = 0;
            GLenum scalar = 0;
            GLenum storage = 0;
            GLint count = 0;
            bool normalized = false;
            bool enabled = false;
            bool operator==(const AttributeState &) const = default;
        };
        struct VertexBindingState
        {
            GLuint name = 0;
            GLintptr offset = 0;
            GLsizei stride = 0;
            GLuint divisor = 0;
            bool known = false;
        };
        std::vector<BufferRangeState> m_UniformRanges;
        std::vector<BufferRangeState> m_StorageRanges;
        std::vector<TextureUnitState> m_TextureUnits;
        std::array<AttributeState, MaxVertexAttributes> m_Attributes{};
        std::array<VertexBindingState, MaxVertexAttributes> m_VertexBindings{};
        GLuint m_VertexArray = 0;
        GLuint m_ClearFramebuffer = 0;
        GLuint m_Program = 0;
        GLuint m_Framebuffer = 0;
        GLuint m_IndexBuffer = 0;
        bool m_ProgramKnown = false;
        bool m_PointSize = false;
        bool m_FramebufferKnown = false;
        bool m_InputKnown = false;
        bool m_IndexKnown = false;
        bool m_VertexArrayKnown = false;
        bool m_RenderKnown = false;
        bool m_ViewportKnown = false;
        bool m_ScissorKnown = false;
        bool m_SrgbKnown = false;
        bool m_Srgb = false;
        RenderState m_RenderState;
        Color m_BlendColor;
        std::uint8_t m_StencilRef = 0;
        StaticVector<Format, MaxRenderTargets> m_ColorFormats;
        Viewport m_Viewport;
        Rect m_Scissor;
        bool m_Known = false;
        GLuint m_UnpackBuffer = 0;
        bool m_QueryReadKnown = false; ///< true 只表示已建立 GL_QUERY_BUFFER=0, 不表示任何查询已 ready.
    };
}
