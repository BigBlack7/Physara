#include "View.hpp"

#include <array>
#include <cassert>
#include <cstring>
#include <utility>

#include <Core/FrameArena.hpp>

/// @file
/// @brief 生成用于验证视口尺寸, 颜色和方向的静态诊断纹理.
/// 将范围校验, 候选准备和提交确认分开, 便于等待期间保留最后接受的颜色与比例.

namespace Physara::Renderer
{
    static constexpr std::size_t PixelBytes = 4;
    // 进程只有一个活动宿主. View 在该宿主 owner 线程分配身份, 后续创建不会复用旧代.
    static std::uint64_t LastViewIdentity = 0;

    // 用预算的除法上界校验尺寸, 再计算紧密 RGBA8 字节数.
    std::size_t DiagnosticPixelBytes(ViewExtent extent) noexcept
    {
        if (extent.width == 0 || extent.height == 0 ||
            extent.height > DiagnosticUploadBudget / PixelBytes / extent.width)
        {
            return 0;
        }
        return static_cast<std::size_t>(extent.width) * extent.height * PixelBytes;
    }

    // 只分配逻辑身份与能力上限, View 不持有 Device 或原生分配权.
    std::optional<View> View::Create(std::uint32_t maxTextureSize) noexcept
    {
        if (!Core::AdvanceGeneration(LastViewIdentity))
        {
            return std::nullopt;
        }
        return View({0, LastViewIdentity}, maxTextureSize);
    }

    ViewSettings View::Settings() const noexcept
    {
        ViewSettings snapshot{.view = m_Id, .requestedScale = m_RequestedScale, .reason = m_Reason,
                              .requestRevision = m_RequestRevision};
        if (m_Diagnostic)
        {
            snapshot.effective = EffectiveViewport{m_EffectiveScale, m_Diagnostic->extent};
        }
        return snapshot;
    }

    std::optional<Core::BoundedText> View::RequestRenderScale(ViewId view, float scale) noexcept
    {
        if (view != m_Id || !(scale >= MinRenderScale && scale <= MaxRenderScale))
        {
            return Core::BoundedText("Render scale requires the active View and a finite value in [0.5, 1.0]");
        }
        if (scale != m_RequestedScale)
        {
            // 每次实际变化分配请求代, 使回到同一比例也无法接受早先准备的候选.
            if (!Core::AdvanceGeneration(m_RequestRevision))
            {
                return Core::BoundedText("View request revision exhausted");
            }
            m_RequestedScale = scale;
            m_Reason = ViewSettingReason::LayoutPending;
        }
        return std::nullopt;
    }

    // 只更新待确认状态的原因, 已接受的请求继续报告有效.
    void View::Defer(ViewSettingReason reason) noexcept
    {
        if (m_Reason != ViewSettingReason::None)
        {
            m_Reason = reason;
        }
    }

    // 根据当前请求形成候选, 只更新等待原因; 最后接受的颜色和比例由 Publish 单独推进.
    std::variant<NoSceneOutputs, Core::BoundedText> View::Prepare(const std::optional<ViewportRequest> &request,
                                                                std::size_t remainingUploadBytes)
    {
        NoSceneOutputs outputs;
        // 先确认尺寸请求对应当前比例. 过期请求保持最后有效输出, 后续布局可重新准备.
        if (!request || request->view != m_Id || request->requestRevision != m_RequestRevision)
        {
            Defer(ViewSettingReason::LayoutPending);
            return outputs;
        }
        // 区分换算失败, 设备边长上限及单图像预算. 这些拒绝都可在后续布局中恢复.
        const auto extent = request->extent;
        if (extent.width == 0 || extent.height == 0)
        {
            outputs.m_Rejection = m_Reason = ViewSettingReason::InvalidExtent;
            return outputs;
        }
        if (extent.width > m_MaxTextureSize || extent.height > m_MaxTextureSize)
        {
            outputs.m_Rejection = m_Reason = ViewSettingReason::TextureSizeLimit;
            return outputs;
        }
        const auto bytes = DiagnosticPixelBytes(extent);
        if (bytes == 0)
        {
            outputs.m_Rejection = m_Reason = ViewSettingReason::DiagnosticBudget;
            return outputs;
        }
        outputs.m_RequestRevision = request->requestRevision;
        outputs.m_Scale = m_RequestedScale;
        // 静态诊断图由实际采样尺寸确定. 相同尺寸可沿 Graphics 队列顺序直接读取已接受代.
        if (m_Diagnostic && m_Diagnostic->extent == extent)
        {
            outputs.m_Output = m_Diagnostic;
            if (m_Reason != ViewSettingReason::None || m_RequestedScale != m_EffectiveScale)
            {
                m_Reason = ViewSettingReason::SubmissionPending;
            }
            return outputs;
        }
        // 保留无颜色结果供 Editor 解析占位. 未创建新纹理, 下次按届时布局重新判断额度.
        if (bytes > remainingUploadBytes)
        {
            outputs.m_UploadPending = true;
            m_Reason = ViewSettingReason::UploadPending;
            return outputs;
        }

        if (!Core::AdvanceGeneration(m_LastGeneration))
        {
            return Core::BoundedText("Viewport diagnostic generation exhausted");
        }
        outputs.m_Output = ViewOutput{m_Id, ViewOutputRole::DiagnosticColor, m_LastGeneration, extent, {}};
        outputs.m_UploadBytes = bytes;
        m_Reason = ViewSettingReason::SubmissionPending;
        return outputs;
    }

    // 一次填充当前新代的紧密像素. 声明图只借用该区段, 不转移 arena 所有权.
    bool NoSceneOutputs::PrepareUpload(Core::FrameArena &arena) noexcept
    {
        if (m_UploadBytes == 0)
        {
            return true;
        }
        auto pixels = arena.Allocate(m_UploadBytes, 1);
        if (pixels.empty())
        {
            return false;
        }
        // 四个象限用固定的 sRGB 编码字节. 单独的颜色可观测上下行序, 不依赖窗口底色.
        constexpr std::array<std::array<std::uint8_t, PixelBytes>, 4> Colors{{
            {180, 71, 53, 255}, {41, 109, 149, 255}, {55, 127, 82, 255}, {118, 75, 161, 255}}};
        auto *pixel = pixels.data();
        const auto extent = m_Output->extent;
        for (std::uint32_t y = 0; y < extent.height; ++y)
        {
            const auto row = y < extent.height / 2 ? 0u : 2u;
            for (std::uint32_t x = 0; x < extent.width; ++x)
            {
                const auto &color = Colors[row + (x >= extent.width / 2)];
                std::memcpy(pixel, color.data(), PixelBytes);
                pixel += PixelBytes;
            }
        }
        m_Pixels = pixels;
        return true;
    }

    // 新代只有描述和上传声明. 实际 UI 读取决定其活性, 执行时才物化.
    GraphResult<TextureId> NoSceneOutputs::Declare(RenderGraph &graph, std::vector<GraphUpload> &uploads) const
    {
        assert(m_Output);
        if (m_Output->texture)
        {
            return graph.ImportTexture(m_Output->texture,
                {RHI::ResourceStates::ShaderResource, RHI::ResourceStates::ShaderResource, true});
        }
        assert(!m_Pixels.empty());
        const RHI::TextureDesc desc{.width = m_Output->extent.width, .height = m_Output->extent.height,
            .format = RHI::Format::SRGBA8_UNORM, .debugName = "Viewport diagnostic color",
            .initialState = RHI::ResourceStates::ShaderResource, .keepInitialState = true};
        auto texture = graph.CreateTexture(desc);
        if (const auto *error = std::get_if<GraphError>(&texture))
        {
            return *error;
        }
        auto written = AddTextureUpload(graph, std::get<TextureId>(texture), m_Pixels,
                                        m_Output->extent.width * PixelBytes, uploads);
        if (const auto *error = std::get_if<GraphError>(&written))
        {
            return *error;
        }
        return std::get<GraphTextureWrite>(written).texture;
    }

    // 换入已接受的新代并释放旧代的 View 引用. 原有 UI 绑定由已提交的 RHI 包继续持有.
    void View::Publish(NoSceneOutputs &&outputs, const RHI::TextureHandle &acceptedTexture) noexcept
    {
        // 拒绝已被后继编辑取代的候选, 即使比例随后回到了相同值.
        if (!acceptedTexture || !outputs.m_Output || outputs.m_Output->view != m_Id ||
            outputs.m_RequestRevision != m_RequestRevision)
        {
            return;
        }
        if (outputs.m_UploadBytes != 0)
        {
            assert(outputs.m_Output->view == m_Id && !outputs.m_Pixels.empty());
            outputs.m_Output->texture = acceptedTexture;
            m_Diagnostic = std::move(outputs.m_Output);
        }
        // 复用颜色时也使用本轮候选所记录的比例, 尺寸仍来自已接受的颜色代.
        m_EffectiveScale = outputs.m_Scale;
        m_Reason = ViewSettingReason::None;
    }
}
