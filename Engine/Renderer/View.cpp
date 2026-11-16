#include "View.hpp"

#include <array>
#include <cassert>
#include <cstring>
#include <limits>
#include <utility>

#include <Core/FrameArena.hpp>

#include "Prepare/ViewPreparation.hpp"

/// @file
/// @brief 生成用于验证视口尺寸, 颜色和方向的静态诊断纹理.
/// 将范围校验, 候选准备和提交确认分开, 便于等待期间保留最后接受的颜色与比例.
/// 持久请求单独预检并发布, 纯相机变化不推进用于诊断尺寸的比例请求代.

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

    std::optional<std::array<double, 2>> ViewImageLayout::ImageUv(double x, double y) const noexcept
    {
        // 使用连续像素坐标和半开边界, 不在此叠加 DPI、渲染比例或第二次 Y 翻转.
        x -= rect.x;
        y -= rect.y;
        if (!(x >= 0 && x < rect.extent.width && y >= 0 && y < rect.extent.height))
        {
            return std::nullopt;
        }
        return std::array{x / rect.extent.width, y / rect.extent.height};
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
        ViewSettings snapshot{.view = m_Id, .requestedScale = m_Document.renderScale, .reason = m_Reason,
                              .requestRevision = m_RequestRevision};
        if (m_Diagnostic)
        {
            snapshot.effective = EffectiveViewport{m_EffectiveScale, m_Diagnostic->layout};
        }
        return snapshot;
    }

    std::optional<Core::BoundedText> View::RequestRenderScale(ViewId view, float scale) noexcept
    {
        if (view != m_Id || !(scale >= MinRenderScale && scale <= MaxRenderScale))
        {
            return Core::BoundedText("Render scale requires the active View and a finite value in [0.5, 1.0]");
        }
        if (scale != m_Document.renderScale)
        {
            // 每次实际变化分配请求代, 使回到同一比例也无法接受早先准备的候选.
            if (m_RequestRevision == std::numeric_limits<std::uint64_t>::max() ||
                !Core::AdvanceGeneration(m_DocumentRevision))
            {
                return Core::BoundedText("View request revision exhausted");
            }
            ApplyRenderScale(scale);
        }
        return std::nullopt;
    }

    ViewDocumentUpdate::ViewDocumentUpdate(ViewId view, std::uint64_t revision,
                                           ViewDocumentRequests value) noexcept
        : m_View(view), m_Revision(revision), m_Value(std::move(value))
    {
    }

    std::optional<Core::BoundedText> View::ValidateDocumentRequests(const ViewDocumentRequests &requests) noexcept
    {
        if (!(requests.renderScale >= MinRenderScale && requests.renderScale <= MaxRenderScale))
        {
            return Core::BoundedText("Invalid mainView.requested.renderScale");
        }
        if (!Scene::IsValidPose(requests.cameraPose))
        {
            return Core::BoundedText("Invalid mainView.cameraPose");
        }
        if (requests.cameraEntityId && requests.cameraEntityId->empty())
        {
            return Core::BoundedText("Empty mainView.cameraEntityId");
        }
        return std::nullopt;
    }

    std::variant<ViewDocumentUpdate, Core::BoundedText>
    View::PrepareDocumentUpdate(ViewId view, ViewDocumentRequests requests) const noexcept
    {
        if (view != m_Id)
        {
            return Core::BoundedText("Document update targets an expired View");
        }
        if (auto failure = ValidateDocumentRequests(requests))
        {
            return *failure;
        }
        constexpr auto Max = std::numeric_limits<std::uint64_t>::max();
        const bool cameraChanged = requests.cameraPose != m_Document.cameraPose ||
            requests.cameraEntityId != m_Document.cameraEntityId;
        if ((requests != m_Document && m_DocumentRevision == Max) ||
            (requests.renderScale != m_Document.renderScale && m_RequestRevision == Max) ||
            (cameraChanged && m_CameraRevision == Max))
        {
            return Core::BoundedText("View document revision exhausted");
        }
        return ViewDocumentUpdate(m_Id, m_DocumentRevision, std::move(requests));
    }

    void View::PublishDocumentUpdate(ViewDocumentUpdate &&update) noexcept
    {
        assert(update.m_View == m_Id && update.m_Revision == m_DocumentRevision);
        if (m_Document != update.m_Value)
        {
            ++m_DocumentRevision;
            if (m_Document.cameraPose != update.m_Value.cameraPose ||
                m_Document.cameraEntityId != update.m_Value.cameraEntityId)
            {
                ++m_CameraRevision;
            }
            if (m_Document.cameraEntityId != update.m_Value.cameraEntityId)
            {
                m_Reason = ViewSettingReason::LayoutPending;
            }
            if (m_Document.renderScale != update.m_Value.renderScale)
            {
                ApplyRenderScale(update.m_Value.renderScale);
            }
            m_Document = std::move(update.m_Value);
        }
    }

    /// @brief 版号已经预留. 只更新比例使用的布局代, 保留最后被接受的输出.
    void View::ApplyRenderScale(float scale) noexcept
    {
        ++m_RequestRevision;
        m_Document.renderScale = scale;
        m_Reason = ViewSettingReason::LayoutPending;
    }

    // 只复制已接受资源的强引用, 不调用 Prepare, 因而不借用当前布局或生成新候选.
    NoSceneOutputs View::LastAcceptedOutput() const noexcept
    {
        NoSceneOutputs outputs;
        outputs.m_Output = m_Diagnostic;
        outputs.m_Retained = true;
        outputs.m_Scale = m_EffectiveScale;
        return outputs;
    }

    // 只更新待确认状态的原因, 已接受的请求继续报告有效.
    void View::Defer(ViewSettingReason reason) noexcept
    {
        if (m_Reason != ViewSettingReason::None)
        {
            m_Reason = reason;
        }
    }

    ViewSettingReason View::CheckViewport(const std::optional<ViewportRequest> &request) const noexcept
    {
        if (!request || request->view != m_Id || request->requestRevision != m_RequestRevision)
        {
            return ViewSettingReason::LayoutPending;
        }
        const auto extent = request->extent;
        if (extent.width == 0 || extent.height == 0)
        {
            return ViewSettingReason::InvalidExtent;
        }
        if (extent.width > m_MaxTextureSize || extent.height > m_MaxTextureSize)
        {
            return ViewSettingReason::TextureSizeLimit;
        }
        return DiagnosticPixelBytes(extent) == 0 ? ViewSettingReason::DiagnosticBudget : ViewSettingReason::None;
    }

    // 准备只推进待确认状态. 已接受的画布、有效图像和比例由正常提交单独确认.
    std::variant<NoSceneOutputs, Core::BoundedText> View::PrepareDiagnostic(const PreparedView &prepared,
                                                                          std::size_t remainingUploadBytes)
    {
        NoSceneOutputs outputs;
        if (prepared.reason == ViewSettingReason::LayoutPending || prepared.view != m_Id ||
            prepared.requestRevision != m_RequestRevision || prepared.cameraRevision != m_CameraRevision)
        {
            Defer(ViewSettingReason::LayoutPending);
            return outputs;
        }
        if (prepared.reason != ViewSettingReason::None)
        {
            outputs.m_Rejection = m_Reason = prepared.reason;
            return outputs;
        }
        outputs.m_RequestRevision = prepared.requestRevision;
        outputs.m_CameraRevision = prepared.cameraRevision;
        outputs.m_Scale = m_Document.renderScale;
        // 画布尺寸相同且有效矩形吻合时, 可以直接读取已接受的颜色代.
        // 当前图案只依赖布局. 纯相机位姿变化不改写像素, 也不发布场景渲染历史.
        if (m_Diagnostic && m_Diagnostic->layout == prepared.layout)
        {
            outputs.m_Output = m_Diagnostic;
            if (m_Reason != ViewSettingReason::None || m_Document.renderScale != m_EffectiveScale)
            {
                m_Reason = ViewSettingReason::SubmissionPending;
            }
            return outputs;
        }
        const auto bytes = DiagnosticPixelBytes(prepared.layout.outputExtent);
        assert(bytes != 0);
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
        outputs.m_Output = ViewOutput{m_Id, ViewOutputRole::DiagnosticColor, m_LastGeneration, prepared.layout, {}};
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
        constexpr std::array<std::uint8_t, PixelBytes> Background{28, 28, 28, 255};
        auto *pixel = pixels.data();
        const auto &[extent, rect] = m_Output->layout;
        for (std::uint32_t y = 0; y < extent.height; ++y)
        {
            const auto row = y < rect.y + rect.extent.height / 2 ? 0u : 2u;
            const bool insideY = y >= rect.y && y < rect.y + rect.extent.height;
            for (std::uint32_t x = 0; x < extent.width; ++x)
            {
                const bool inside = insideY && x >= rect.x && x < rect.x + rect.extent.width;
                const auto &color = inside ? Colors[row + (x >= rect.x + rect.extent.width / 2)] : Background;
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
        const auto extent = m_Output->layout.outputExtent;
        const RHI::TextureDesc desc{.width = extent.width, .height = extent.height,
            .format = RHI::Format::SRGBA8_UNORM, .debugName = "Viewport diagnostic color",
            .initialState = RHI::ResourceStates::ShaderResource, .keepInitialState = true};
        auto texture = graph.CreateTexture(desc);
        if (const auto *error = std::get_if<GraphError>(&texture))
        {
            return *error;
        }
        auto written = AddTextureUpload(graph, std::get<TextureId>(texture), m_Pixels,
                                        extent.width * PixelBytes, uploads);
        if (const auto *error = std::get_if<GraphError>(&written))
        {
            return *error;
        }
        return std::get<GraphTextureWrite>(written).texture;
    }

    // 换入已接受的新代并释放旧代的 View 引用. 原有 UI 绑定由已提交的 RHI 包继续持有.
    void View::Publish(NoSceneOutputs &&outputs, const RHI::TextureHandle &acceptedTexture) noexcept
    {
        // 拒绝安全图旧快照和已被后继编辑取代的候选, 即使尺寸或比例碰巧相同也不重新确认.
        if (outputs.m_Retained || !acceptedTexture || !outputs.m_Output || outputs.m_Output->view != m_Id ||
            outputs.m_RequestRevision != m_RequestRevision || outputs.m_CameraRevision != m_CameraRevision)
        {
            return;
        }
        if (outputs.m_UploadBytes != 0)
        {
            assert(outputs.m_Output->view == m_Id && !outputs.m_Pixels.empty());
            outputs.m_Output->texture = acceptedTexture;
            m_Diagnostic = std::move(outputs.m_Output);
        }
        // 复用颜色时也使用本轮候选所记录的比例, 完整布局仍来自已接受的颜色代.
        m_EffectiveScale = outputs.m_Scale;
        m_Reason = ViewSettingReason::None;
    }
}
