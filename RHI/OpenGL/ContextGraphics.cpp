#include "ContextState.hpp"

/// @file
/// @brief 将已验证的普通固定/动态状态映射为 GL, 保持 clip, 线性颜色和目标空间绕序合同.

#include <algorithm>
#include <tuple>

namespace Physara::RHI::OpenGL
{
    BlendFactorUse GetBlendFactorUse(const BlendState::RenderTarget &target, Format format) noexcept
    {
        // 先确定目标存在且允许写入的分量, 再分别判断 RGB/alpha 方程, Min/Max 不消费任何权重.
        const auto &info = GetFormatInfo(format);
        const auto mask = unsigned(target.colorWriteMask);
        const bool blend = target.blendEnable && info.kind != FormatKind::Integer;
        const bool color =
            (info.hasRed && (mask & 1) != 0) || (info.hasGreen && (mask & 2) != 0) || (info.hasBlue && (mask & 4) != 0);
        return {blend && color && target.blendOp != BlendOp::Min && target.blendOp != BlendOp::Max,
                blend && info.hasAlpha && (mask & 8) != 0 && target.blendOpAlpha != BlendOp::Min &&
                    target.blendOpAlpha != BlendOp::Max};
    }

    /// @brief 公开比较/模板操作的有效连续值已由 Pipeline 工厂核对.
    static GLenum Compare(ComparisonFunc value) noexcept
    {
        constexpr GLenum Values[] = {GL_NEVER,   GL_LESS,     GL_EQUAL,  GL_LEQUAL,
                                     GL_GREATER, GL_NOTEQUAL, GL_GEQUAL, GL_ALWAYS};
        return Values[unsigned(value) - 1];
    }

    // 与公开模板操作的连续有效值逐项对应.
    static GLenum Stencil(StencilOp value) noexcept
    {
        constexpr GLenum Values[] = {GL_KEEP, GL_ZERO,   GL_REPLACE,   GL_INCR,
                                     GL_DECR, GL_INVERT, GL_INCR_WRAP, GL_DECR_WRAP};
        return Values[unsigned(value) - 1];
    }

    /// @brief 映射本次实际消费的普通因子; 准备边界已排除活动双源因子及非法值.
    static GLenum Factor(BlendFactor value) noexcept
    {
        constexpr GLenum Values[] = {0,
                                     GL_ZERO,
                                     GL_ONE,
                                     GL_SRC_COLOR,
                                     GL_ONE_MINUS_SRC_COLOR,
                                     GL_SRC_ALPHA,
                                     GL_ONE_MINUS_SRC_ALPHA,
                                     GL_DST_ALPHA,
                                     GL_ONE_MINUS_DST_ALPHA,
                                     GL_DST_COLOR,
                                     GL_ONE_MINUS_DST_COLOR,
                                     GL_SRC_ALPHA_SATURATE,
                                     0,
                                     0,
                                     GL_CONSTANT_COLOR,
                                     GL_ONE_MINUS_CONSTANT_COLOR};
        return Values[unsigned(value)];
    }

    // blend 方程与源值一一对应, 不重解释 min/max 的因子语义.
    static GLenum Equation(BlendOp value) noexcept
    {
        constexpr GLenum Values[] = {GL_FUNC_ADD, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT, GL_MIN, GL_MAX};
        return Values[unsigned(value) - 1];
    }

    // 普通全局开关共用此入口, 按附件/viewport 的 indexed 开关在调用处单独设置.
    static void Enable(GLenum cap, bool enabled) noexcept
    {
        if (enabled)
        {
            glEnable(cap);
        }
        else
        {
            glDisable(cap);
        }
    }

    /// @brief 仅比较已实现普通 raster 字段, 扩展值在准备边界拒绝.
    static auto RasterValues(const RasterState &r) noexcept
    {
        return std::tie(r.fillMode, r.cullMode, r.frontCounterClockwise, r.depthClipEnable, r.scissorEnable,
                        r.depthBias, r.depthBiasClamp, r.slopeScaledDepthBias);
    }

    // 模板两面的静态操作独立参与比较, reference 的动态/静态选择在 ApplyRenderState 中完成.
    static auto StencilValues(const DepthStencilState::StencilOpDesc &s) noexcept
    {
        return std::tie(s.failOp, s.depthFailOp, s.passOp, s.stencilFunc);
    }

    // 只比较影响 native 测试/写入的字段; reference 按最终数值另比, 避免把动态开关当作引用值.
    static bool SameDepth(const DepthStencilState &a, const DepthStencilState &b) noexcept
    {
        return std::tie(a.depthTestEnable, a.depthWriteEnable, a.depthFunc, a.stencilEnable, a.stencilReadMask,
                        a.stencilWriteMask) == std::tie(b.depthTestEnable, b.depthWriteEnable, b.depthFunc,
                                                        b.stencilEnable, b.stencilReadMask, b.stencilWriteMask) &&
               StencilValues(a.frontFaceStencil) == StencilValues(b.frontFaceStencil) &&
               StencilValues(a.backFaceStencil) == StencilValues(b.backFaceStencil);
    }

    void ContextState::ApplyRenderState(const RenderState &state, const FramebufferInfo &info, const Color &blendColor,
                                        std::uint8_t stencilRef) noexcept
    {
        const auto &raster = state.rasterState;
        const auto &depth = state.depthStencilState;
        // 固定状态分为 raster, depth/stencil 与逐颜色目标三组, 各自只在其事实变化时重设.
        if (!m_RenderKnown || RasterValues(raster) != RasterValues(m_RenderState.rasterState))
        {
            glPolygonMode(GL_FRONT_AND_BACK, raster.fillMode == RasterFillMode::Solid ? GL_FILL : GL_LINE);
            Enable(GL_CULL_FACE, raster.cullMode != RasterCullMode::None);
            glCullFace(raster.cullMode == RasterCullMode::Front ? GL_FRONT : GL_BACK);
            // 公共绕序在目标的左上坐标中解释; clip origin 已补偿 GL 面积符号, 故这里转换一次.
            glFrontFace(raster.frontCounterClockwise ? GL_CW : GL_CCW);
            Enable(GL_DEPTH_CLAMP, !raster.depthClipEnable);
            if (raster.scissorEnable)
            {
                glEnablei(GL_SCISSOR_TEST, 0);
            }
            else
            {
                glDisablei(GL_SCISSOR_TEST, 0);
            }
            const bool bias = raster.depthBias != 0 || raster.slopeScaledDepthBias != 0.f;
            Enable(GL_POLYGON_OFFSET_FILL, bias);
            Enable(GL_POLYGON_OFFSET_LINE, bias);
            Enable(GL_POLYGON_OFFSET_POINT, bias);
            glPolygonOffsetClamp(raster.slopeScaledDepthBias, GLfloat(raster.depthBias), raster.depthBiasClamp);
        }
        const auto reference = depth.dynamicStencilRef ? stencilRef : depth.stencilRefValue;
        if (!m_RenderKnown || !SameDepth(depth, m_RenderState.depthStencilState) || m_StencilRef != reference)
        {
            Enable(GL_DEPTH_TEST, depth.depthTestEnable);
            glDepthMask(GLboolean(depth.depthWriteEnable));
            glDepthFunc(Compare(depth.depthFunc));
            Enable(GL_STENCIL_TEST, depth.stencilEnable);
            for (const auto face : {GL_FRONT, GL_BACK})
            {
                const auto &ops = face == GL_FRONT ? depth.frontFaceStencil : depth.backFaceStencil;
                glStencilMaskSeparate(face, depth.stencilWriteMask);
                glStencilFuncSeparate(face, Compare(ops.stencilFunc), reference, depth.stencilReadMask);
                glStencilOpSeparate(face, Stencil(ops.failOp), Stencil(ops.depthFailOp), Stencil(ops.passOp));
            }
        }
        // 同一 blend 描述在 R32 与 RGBA 上的 alpha 活性不同, 格式序列也必须参与 native 差分.
        if (!m_RenderKnown || state.blendState != m_RenderState.blendState ||
            !std::ranges::equal(m_ColorFormats, info.colorFormats))
        {
            for (GLuint index = 0; index < info.colorFormats.size(); ++index)
            {
                const auto &target = state.blendState.targets[index];
                if (target.blendEnable)
                {
                    glEnablei(GL_BLEND, index);
                }
                else
                {
                    glDisablei(GL_BLEND, index);
                }
                // 未消费的因子使用无害原生值, 不把 inactive Src1 送入 GL 的双源限制检查.
                const auto use = GetBlendFactorUse(target, info.colorFormats[index]);
                glBlendFuncSeparatei(index, use.color ? Factor(target.srcBlend) : GL_ONE,
                                     use.color ? Factor(target.destBlend) : GL_ZERO,
                                     use.alpha ? Factor(target.srcBlendAlpha) : GL_ONE,
                                     use.alpha ? Factor(target.destBlendAlpha) : GL_ZERO);
                glBlendEquationSeparatei(index, Equation(target.blendOp), Equation(target.blendOpAlpha));
                const auto mask = unsigned(target.colorWriteMask);
                glColorMaski(index, GLboolean((mask & 1) != 0), GLboolean((mask & 2) != 0), GLboolean((mask & 4) != 0),
                             GLboolean((mask & 8) != 0));
            }
            Enable(GL_SAMPLE_ALPHA_TO_COVERAGE, state.blendState.alphaToCoverageEnable);
        }
        if (!m_RenderKnown || std::tie(m_BlendColor.r, m_BlendColor.g, m_BlendColor.b, m_BlendColor.a) !=
                                  std::tie(blendColor.r, blendColor.g, blendColor.b, blendColor.a))
        {
            glBlendColor(blendColor.r, blendColor.g, blendColor.b, blendColor.a);
        }
        bool srgb = false;
        // GL 的开关为全局, 实际编码仍由每个附件的格式决定; 线性/整数附件不因此重复编码.
        for (const auto format : info.colorFormats)
        {
            srgb = srgb || GetFormatInfo(format).isSRGB;
        }
        if (!m_SrgbKnown || m_Srgb != srgb)
        {
            Enable(GL_FRAMEBUFFER_SRGB, srgb);
        }
        if (!m_RenderKnown)
        {
            // 失效后重建本 profile 的固定前提, 不能继承外部互操作遗留的 restart/logic-op 等开关.
            glDisable(GL_RASTERIZER_DISCARD);
            glDisable(GL_COLOR_LOGIC_OP);
            glDisable(GL_SAMPLE_COVERAGE);
            glDisable(GL_SAMPLE_MASK);
            glDisable(GL_SAMPLE_ALPHA_TO_ONE);
            glDisable(GL_LINE_SMOOTH);
            glLineWidth(1.f);
            glDisable(GL_DITHER);
            glDisable(GL_PRIMITIVE_RESTART);
            glDisable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
            // canonical Vulkan 的 flat 值来自首顶点, 不沿用 GL 默认的末顶点约定.
            glProvokingVertex(GL_FIRST_VERTEX_CONVENTION);
        }
        m_RenderState = state;
        m_BlendColor = blendColor;
        m_ColorFormats = info.colorFormats;
        m_StencilRef = reference;
        m_RenderKnown = m_SrgbKnown = true;
        m_Srgb = srgb;
    }

    void ContextState::ApplyViewport(const Viewport &viewport) noexcept
    {
        if (!m_ViewportKnown || m_Viewport != viewport)
        {
            glViewportIndexedf(0, viewport.minX, viewport.minY, viewport.Width(), viewport.Height());
            glDepthRangeIndexed(0, viewport.minZ, viewport.maxZ);
        }
        m_Viewport = viewport;
        m_ViewportKnown = true;
    }

    void ContextState::ApplyScissor(const Rect &rect) noexcept
    {
        if (!m_ScissorKnown || m_Scissor != rect)
        {
            glScissorIndexed(0, rect.minX, rect.minY, rect.Width(), rect.Height());
        }
        m_Scissor = rect;
        m_ScissorKnown = true;
    }

    void ContextState::ApplyClearState(bool srgb) noexcept
    {
        // 公共 clear 是完整子资源操作, 不能继承 Draw 的区域或写掩码.
        glDisablei(GL_SCISSOR_TEST, 0);
        glDepthMask(GL_TRUE);
        glStencilMaskSeparate(GL_FRONT_AND_BACK, 0xff);
        for (GLuint index = 0; index < MaxRenderTargets; ++index)
        {
            glColorMaski(index, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        }
        Enable(GL_FRAMEBUFFER_SRGB, srgb);
        glDisable(GL_DITHER);
        m_RenderKnown = false;
        m_SrgbKnown = true;
        m_Srgb = srgb;
    }
} // namespace Physara::RHI::OpenGL
