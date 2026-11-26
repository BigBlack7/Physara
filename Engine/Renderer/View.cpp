#include "View.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <string_view>
#include <utility>

#include <Core/FrameArena.hpp>

#include "Prepare/ViewPreparation.hpp"

/// @file
/// @brief 准备视口候选和测试图. 主路经 UI 采样, 离屏路经显式导出, 均以匹配的正常提交发布显示色.
/// 持久请求先校验并求值镜头/曝光, 再一起发布; 准备和确认输出是之后的独立边界.
/// 文档, 几何相机和比例各有请求代, 不能用布局或矩阵未变来认定曝光候选仍有效.

namespace Physara::Renderer
{
    static constexpr std::size_t PixelBytes = 4;
    // 进程只有一个活动宿主. View 在该宿主 owner 线程分配身份, 后续创建不会复用旧代.
    static std::uint64_t LastViewIdentity = 0;

    [[nodiscard]] static constexpr bool ValidOpaquePath(OpaquePath path) noexcept
    {
        return path == OpaquePath::Forward || path == OpaquePath::ForwardPlus;
    }

    /// @brief 校验文档的路径, AA, 比例, 位姿和引用形状. 组合方核对 Scene 的具体相机引用.
    static std::optional<Core::BoundedText> ValidateDocumentFields(const ViewDocumentRequests &requests) noexcept
    {
        if (!(requests.renderScale >= MinRenderScale && requests.renderScale <= MaxRenderScale))
        {
            return Core::BoundedText("Invalid mainView.requested.renderScale");
        }
        if (requests.antiAliasing > AntiAliasing::Fxaa)
        {
            return Core::BoundedText("Invalid mainView.requested.antiAliasing");
        }
        if (!ValidOpaquePath(requests.opaquePath))
        {
            return Core::BoundedText("Invalid mainView.requested.opaquePath");
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

    /// @brief 校验物理字段, 派生垂直视场和可用于 GPU 的预曝光.
    static std::variant<PhysicalCameraEvaluation, Core::BoundedText>
    EvaluatePhysicalCamera(const PhysicalCameraRequest &camera) noexcept
    {
        using Field = std::pair<std::string_view, double>;
        const std::array positive{
            Field{"fNumber", camera.fNumber},
            Field{"shutterSeconds", camera.shutterSeconds},
            Field{"iso", camera.iso},
            Field{"sensorHeightMeters", camera.sensorHeightMeters},
            Field{"navigationFocalLengthMeters", camera.navigationFocalLengthMeters},
            Field{"focusDistanceMeters", camera.focusDistanceMeters}};
        for (const auto &[field, value] : positive)
        {
            if (!(std::isfinite(value) && value > 0.0))
            {
                Core::BoundedText error("mainView.requested.physicalCamera.");
                error.Append(field);
                error.Append(" requires a finite positive value");
                return error;
            }
        }
        if (!std::isfinite(camera.exposureCompensationEv))
        {
            return Core::BoundedText("mainView.requested.physicalCamera.exposureCompensationEv requires a finite value");
        }

        // 先缩放两项长度, 保留 atan2 的比值, 避免中间的两倍焦距溢出.
        const double lensScale = std::max(camera.sensorHeightMeters, camera.navigationFocalLengthMeters);
        const double yfov = 2.0 * std::atan2(camera.sensorHeightMeters / lensScale,
                                            2.0 * (camera.navigationFocalLengthMeters / lensScale));
        if (!(yfov > 0.0 && yfov < std::numbers::pi))
        {
            return Core::BoundedText("mainView.requested.physicalCamera navigation lens yields an invalid vertical FOV");
        }

        // EV100 保持未补偿的名义值. 倍率在对数域应用补偿及 1.2 标定, 避免先计算 N^2/t 溢出.
        // 转成 GPU float 前拒绝非正规正值及溢出, 下游直接复制这一份 FP32 倍率.
        const double ev100 = 2.0 * std::log2(camera.fNumber) - std::log2(camera.shutterSeconds) +
                             std::log2(100.0) - std::log2(camera.iso);
        const double preExposure = std::exp2(camera.exposureCompensationEv - ev100 - std::log2(1.2));
        if (!(preExposure >= std::numeric_limits<float>::min() && preExposure <= std::numeric_limits<float>::max()))
        {
            return Core::BoundedText("mainView.requested.physicalCamera preExposure exceeds the normal FP32 range");
        }
        return PhysicalCameraEvaluation{yfov, {ev100, static_cast<float>(preExposure)}};
    }

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

    // 固定缺省满足求值前提, 新建 View 与读取旧 v1 文档使用同一组参数.
    View::View(ViewId id, std::uint32_t maxTextureSize) noexcept
        : m_Id(id), m_MaxTextureSize(maxTextureSize),
          m_Physical(std::get<PhysicalCameraEvaluation>(EvaluatePhysicalCamera(m_Document.physicalCamera)))
    {
    }

    Asset::ModelPerspective View::NavigationCameraProjection() const noexcept
    {
        auto projection = NavigationProjection;
        projection.yfov = m_Physical.navigationYfov;
        return projection;
    }

    CameraLens View::ResolveLens(const Scene::Camera *sceneProjection) const noexcept
    {
        const auto &request = m_Document.physicalCamera;
        CameraLens lens{request.fNumber, request.sensorHeightMeters, request.focusDistanceMeters, {}, {}};
        if (!sceneProjection)
        {
            lens.yfov = m_Physical.navigationYfov;
            lens.focalLengthMeters = request.navigationFocalLengthMeters;
        }
        else if (const auto *perspective = std::get_if<Asset::ModelPerspective>(sceneProjection))
        {
            lens.yfov = perspective->yfov;
            // 源投影继续作为权威. 等效焦距只在可表示为正有限长度时交付.
            const double focalLength = request.sensorHeightMeters * (0.5 / std::tan(perspective->yfov * 0.5));
            if (std::isfinite(focalLength) && focalLength > 0.0)
            {
                lens.focalLengthMeters = focalLength;
            }
        }
        // 正交没有透视视场或等效焦距, 光圈, 传感器和对焦请求仍属于这个 View.
        return lens;
    }

    ViewSettings View::Settings() const noexcept
    {
        ViewSettings snapshot{.view = m_Id, .requestedScale = m_Document.renderScale, .reason = m_Reason,
                              .requestRevision = m_RequestRevision, .preview = m_Preview,
                              .requestedAa = m_Document.antiAliasing};
        if (m_Accepted)
        {
            snapshot.effective = EffectiveViewport{m_EffectiveScale, m_Accepted->layout};
        }
        return snapshot;
    }

    // 模式是会话请求. 保留旧颜色和场景记录, 直到 Publish 收到与新模式匹配的候选.
    void View::RequestPreview(PreviewMode mode) noexcept
    {
        assert(mode <= PreviewMode::PhysicalLighting);
        if (m_Preview != mode)
        {
            m_Preview = mode;
            m_Reason = ViewSettingReason::SubmissionPending;
        }
    }

    bool View::RequestOpaquePath(OpaquePath path) noexcept
    {
        if (!ValidOpaquePath(path))
        {
            return false;
        }
        if (m_Document.opaquePath != path)
        {
            if (!Core::AdvanceGeneration(m_DocumentRevision))
            {
                return false;
            }
            m_Document.opaquePath = path;
            OpaquePathChanged();
        }
        return true;
    }

    // 新意图留待正常渲染确认. 原颜色与相机继续对应其实际提交.
    void View::OpaquePathChanged() noexcept
    {
        m_Reason = ViewSettingReason::SubmissionPending;
        m_OpaqueReason = m_AcceptedOpaquePlan && m_AcceptedOpaquePlan->key.path == RequestedOpaquePath()
            ? ViewSettingReason::None : ViewSettingReason::SubmissionPending;
        m_OpaqueDetail.reset();
        m_OpaqueRejection.reset();
    }

    void View::ObserveScene(Scene::SceneId scene) noexcept
    {
        if (m_PlanScene != scene)
        {
            m_PlanScene = scene;
            m_AcceptedOpaquePlan.reset();
            m_OpaqueReason = ViewSettingReason::LayoutPending;
            m_OpaqueDetail.reset();
            m_OpaqueRejection.reset();
        }
    }

    OpaquePathSettings View::OpaqueSettings(Scene::SceneId scene) const noexcept
    {
        if (scene != m_PlanScene)
        {
            return {RequestedOpaquePath(), {}, ViewSettingReason::LayoutPending, nullptr};
        }
        return {RequestedOpaquePath(), m_AcceptedOpaquePlan ? std::optional{m_AcceptedOpaquePlan->key.path}
                                                          : std::nullopt,
                m_OpaqueReason, m_OpaqueDetail ? &*m_OpaqueDetail : nullptr};
    }

    void View::ReportOpaquePreparation(Scene::SceneId scene, std::uint64_t revision,
        ViewSettingReason reason, const Core::BoundedText *detail) noexcept
    {
        if (scene == m_PlanScene && revision == m_DocumentRevision)
        {
            m_OpaqueReason = reason;
            m_OpaqueDetail = detail ? std::optional{*detail} : std::nullopt;
        }
    }

    void View::RejectOpaqueSubmission(const PreparedViewOutput &candidate, ViewSettingReason reason,
        const Core::BoundedText *detail) noexcept
    {
        if (!candidate.m_OpaquePlan || candidate.IsRetained())
        {
            return;
        }
        const auto &scene = *candidate.m_Output->scene;
        if (scene.scene != m_PlanScene || scene.documentRevision != m_DocumentRevision ||
            candidate.m_CameraRevision != m_CameraRevision || scene.opaquePath != RequestedOpaquePath())
        {
            return;
        }
        ReportOpaquePreparation(scene.scene, scene.documentRevision, reason, detail);
        if (!m_AcceptedOpaquePlan || m_AcceptedOpaquePlan->key.path != scene.opaquePath)
        {
            m_OpaqueRejection = OpaqueRejection{
                {scene.scene, scene.contentRevision, scene.documentRevision, candidate.m_CameraRevision,
                 scene.camera.sceneCameraRevision, {scene.renderLayout, candidate.m_Output->layout}, scene.mode,
                 scene.shadowRequestRevision, scene.shaderGeneration}, reason, m_OpaqueDetail};
        }
    }

    bool View::CanSwitchOpaquePath(const PreparedView &prepared, std::uint64_t shaderGeneration) noexcept
    {
        if (!m_OpaqueRejection)
        {
            return true;
        }
        const OpaqueAttempt current{prepared.scene.scene, prepared.scene.contentRevision, prepared.documentRevision,
            prepared.cameraRevision, prepared.camera->sceneCameraRevision, prepared.layouts, prepared.preview,
            prepared.shadowStatistics.requestRevision, shaderGeneration};
        // 相机或计划输入更新后允许重试. 等待期间旧策略取得的新票据仍属于旧策略, 不会解除拒绝.
        if (current != m_OpaqueRejection->input)
        {
            m_OpaqueRejection.reset();
            return true;
        }
        const auto &rejection = *m_OpaqueRejection;
        ReportOpaquePreparation(current.scene, current.documentRevision, rejection.reason,
                                rejection.detail ? &*rejection.detail : nullptr);
        return false;
    }

    bool View::RequestCameraCut(ViewId view) noexcept
    {
        if (view != m_Id || !Core::AdvanceGeneration(m_CameraRevision))
        {
            return false;
        }
        // cut 是相机代的一种更新, 因而在相机代能前进时没有独立的回绕风险.
        ++m_CameraCutRevision;
        m_Reason = ViewSettingReason::SubmissionPending;
        return true;
    }

    std::optional<Core::BoundedText> View::RequestShadows(ViewId view, ShadowSettings settings) noexcept
    {
        if (view != m_Id)
        {
            return Core::BoundedText("Shadow planning requires the active View identity");
        }
        if (!std::isfinite(settings.shadowDistanceMeters) || settings.shadowDistanceMeters <= 0 ||
            (settings.edge != MaxShadowEdge && settings.edge != 512 && settings.edge != 256))
        {
            return Core::BoundedText("Shadow planning requires a finite positive distance and a supported edge");
        }
        if (settings.directionalCascades < 1 || settings.directionalCascades > MaxDirectionalCascades)
        {
            return Core::BoundedText("Directional shadow cascade count must be in [1,4]");
        }
        if (!(settings.splitLambda >= 0 && settings.splitLambda <= 1) ||
            !(settings.transitionFraction >= 0 && settings.transitionFraction <= 0.25))
        {
            return Core::BoundedText("Cascade split weight and transition must be finite and in [0,1] and [0,0.25]");
        }
        constexpr std::uint64_t MiB = 1024 * 1024;
        if ((settings.filter != ShadowFilter::Hard && settings.filter != ShadowFilter::Pcf) ||
            (settings.byteBudget != 4 * MiB && settings.byteBudget != 8 * MiB &&
             settings.byteBudget != 16 * MiB && settings.byteBudget != MaxShadowBytes))
        {
            return Core::BoundedText("Shadow filtering or logical budget is outside the supported choices");
        }
        if (settings != m_ShadowSettings)
        {
            // 先为新值取得会话请求代. 阴影设置只由下一次 View 准备消费, 保存和撤销使用独立的文档代.
            if (!Core::AdvanceGeneration(m_ShadowRequestRevision))
            {
                return Core::BoundedText("Shadow request revision exhausted");
            }
            m_ShadowSettings = settings;
            if (!settings.enabled)
            {
                m_ShadowDepth.reset();
            }
        }
        return std::nullopt;
    }

    std::optional<Core::BoundedText> View::RequestCascadePreview(ViewId view, bool enabled) noexcept
    {
        return SetShadowDiagnostic(view, enabled ? ShadowDiagnostic{CascadeDiagnostic{}} : ShadowDiagnostic{});
    }

    std::optional<Core::BoundedText> View::RequestShadowDiagnostic(ViewId view, const Scene::Scene &scene,
                                                                  ShadowDiagnostic diagnostic) noexcept
    {
        if (const auto *local = std::get_if<LocalShadowSelection>(&diagnostic))
        {
            const auto entity = scene.Get(local->light);
            if (!entity || !entity->record.properties.light || entity->record.properties.light->type != local->kind ||
                (local->kind != Asset::ModelLightKind::Spot && local->kind != Asset::ModelLightKind::Point))
            {
                return Core::BoundedText("Shadow diagnostic requires a current spot or point light");
            }
        }
        return SetShadowDiagnostic(view, diagnostic);
    }

    std::optional<Core::BoundedText> View::SetShadowDiagnostic(ViewId view, ShadowDiagnostic diagnostic) noexcept
    {
        if (view != m_Id)
        {
            return Core::BoundedText("Shadow diagnostic requires the active View identity");
        }
        if (diagnostic != m_ShadowDiagnostic)
        {
            if (!Core::AdvanceGeneration(m_ShadowRequestRevision))
            {
                return Core::BoundedText("Shadow request revision exhausted");
            }
            m_ShadowDiagnostic = diagnostic;
        }
        return std::nullopt;
    }

    bool ShadowDepthSet::Matches(ViewId owner, Scene::SceneId current, const ShadowPlan &plan,
                                 std::span<const ShadowLightPlan> required) const noexcept
    {
        if (view != owner || scene != current || edge != plan.edge || groups != required.size())
        {
            return false;
        }
        // index 是本数组的物理层. 每层保存的逻辑地址和内容代仍可因预算及场景编辑而改变.
        std::size_t index = 0;
        for (const auto &group : required)
        {
            for (const auto &region : group.Regions())
            {
                if (index == count || layers[index].light != group.light || layers[index].region != region)
                {
                    return false;
                }
                ++index;
            }
        }
        return index == count;
    }

    void View::AcceptShadowDepth(ShadowDepthSet depth, std::uint64_t ticket) noexcept
    {
        // 调用方已发布本 View 的受光或诊断颜色. 数组出口与实际消费对应同一次正常接受.
        const auto *scene = AcceptedScenePreview();
        if (!m_ShadowSettings.enabled || !scene || depth.view != m_Id || scene->scene != depth.scene ||
            scene->submission != ticket || scene->shadowRequestRevision != m_ShadowRequestRevision ||
            !scene->shadows || scene->shadows->layers != depth.count || scene->shadows->edge != depth.edge ||
            depth.retained.Usage() != ExportUsage::ShaderRead || !depth.retained.Accepted(ticket))
        {
            return;
        }
        m_ShadowDepth = std::move(depth);
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
                                           ViewDocumentRequests value, PhysicalCameraEvaluation physical,
                                           bool cameraChanged) noexcept
        : m_View(view), m_Revision(revision), m_Value(std::move(value)), m_Physical(physical),
          m_CameraChanged(cameraChanged)
    {
    }

    std::optional<Core::BoundedText> View::ValidateDocumentRequests(const ViewDocumentRequests &requests) noexcept
    {
        if (auto failure = ValidateDocumentFields(requests))
        {
            return failure;
        }
        const auto result = EvaluatePhysicalCamera(requests.physicalCamera);
        if (const auto *failure = std::get_if<Core::BoundedText>(&result))
        {
            return *failure;
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
        if (auto failure = ValidateDocumentFields(requests))
        {
            return *failure;
        }
        auto physical = m_Physical;
        if (requests.physicalCamera != m_Document.physicalCamera)
        {
            const auto result = EvaluatePhysicalCamera(requests.physicalCamera);
            if (const auto *failure = std::get_if<Core::BoundedText>(&result))
            {
                return *failure;
            }
            physical = std::get<PhysicalCameraEvaluation>(result);
        }
        constexpr auto Max = std::numeric_limits<std::uint64_t>::max();
        // 文档代区分全部物理请求. 只有导航的有效 FOV 变化才额外改变几何相机代.
        const bool cameraChanged = requests.cameraPose != m_Document.cameraPose ||
            requests.cameraEntityId != m_Document.cameraEntityId ||
            (!requests.cameraEntityId && physical.navigationYfov != m_Physical.navigationYfov);
        if ((requests != m_Document && m_DocumentRevision == Max) ||
            (requests.renderScale != m_Document.renderScale && m_RequestRevision == Max) ||
            (cameraChanged && m_CameraRevision == Max))
        {
            return Core::BoundedText("View document revision exhausted");
        }
        return ViewDocumentUpdate(m_Id, m_DocumentRevision, std::move(requests), physical, cameraChanged);
    }

    void View::PublishDocumentUpdate(ViewDocumentUpdate &&update) noexcept
    {
        // 同一 owner 线程在预检后发布已拥有的值, 这里不再做可失败的物理求值或分配.
        assert(update.m_View == m_Id && update.m_Revision == m_DocumentRevision);
        if (m_Document != update.m_Value)
        {
            ++m_DocumentRevision;
            if (update.m_CameraChanged)
            {
                ++m_CameraRevision;
            }
            // AA 改动不推进像素比例代或几何相机代. 文档代使同尺寸的旧场景候选不能通过 Publish.
            if (m_Document.physicalCamera != update.m_Value.physicalCamera ||
                m_Document.antiAliasing != update.m_Value.antiAliasing)
            {
                m_Reason = ViewSettingReason::SubmissionPending;
            }
            if (m_Document.opaquePath != update.m_Value.opaquePath)
            {
                m_Document.opaquePath = update.m_Value.opaquePath;
                OpaquePathChanged();
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
            m_Physical = update.m_Physical;
        }
    }

    /// @brief 版号已经预留. 只更新比例使用的布局代, 保留最后被接受的输出.
    void View::ApplyRenderScale(float scale) noexcept
    {
        ++m_RequestRevision;
        m_Document.renderScale = scale;
        m_Reason = ViewSettingReason::LayoutPending;
    }

    // 旧图只在最近的物理状态仍可接入时交给安全 UI. 快照独立持有该颜色代, 即使布局已经改变.
    PreparedViewOutput View::LastAcceptedOutput(ViewSettingReason reason) const noexcept
    {
        PreparedViewOutput outputs;
        if (m_Accepted && m_Accepted->HasReadableColor())
        {
            outputs.m_Output = m_Accepted;
        }
        outputs.m_Retained = true;
        outputs.m_Scale = m_EffectiveScale;
        outputs.m_UnavailableReason = reason;
        return outputs;
    }

    const RenderedScene *View::AcceptedScenePreview() const noexcept
    {
        return m_Accepted && m_Accepted->scene ? &*m_Accepted->scene : nullptr;
    }

    // 只更新待确认状态的原因, 已接受的请求继续报告有效.
    void View::Defer(ViewSettingReason reason) noexcept
    {
        if (m_Reason != ViewSettingReason::None)
        {
            m_Reason = reason;
        }
    }

    ViewSettingReason View::CheckViewport(const std::optional<ViewportRequest> &request, ViewportUse use) const noexcept
    {
        if (!request || request->view != m_Id || request->requestRevision != m_RequestRevision)
        {
            return ViewSettingReason::LayoutPending;
        }
        const auto extent = request->renderExtent;
        if (extent.width == 0 || extent.height == 0 ||
            request->outputExtent.width == 0 || request->outputExtent.height == 0)
        {
            return ViewSettingReason::InvalidExtent;
        }
        const auto output = request->outputExtent;
        if (extent.width > m_MaxTextureSize || extent.height > m_MaxTextureSize ||
            (use == ViewportUse::DisplayOutput && (output.width > m_MaxTextureSize || output.height > m_MaxTextureSize)))
        {
            return ViewSettingReason::TextureSizeLimit;
        }
        return ViewSettingReason::None;
    }

    // 准备只推进待确认状态. 已接受的画布、有效图像和比例由正常提交单独确认.
    std::variant<PreparedViewOutput, Core::BoundedText> View::PrepareDiagnostic(const PreparedView &prepared,
                                                                          std::size_t remainingUploadBytes)
    {
        PreparedViewOutput outputs;
        if (prepared.reason == ViewSettingReason::LayoutPending || prepared.view != m_Id ||
            prepared.requestRevision != m_RequestRevision || prepared.cameraRevision != m_CameraRevision)
        {
            Defer(ViewSettingReason::LayoutPending);
            return outputs;
        }
        if (prepared.reason != ViewSettingReason::None)
        {
            return Unavailable(prepared.reason);
        }
        outputs.m_RequestRevision = prepared.requestRevision;
        outputs.m_CameraRevision = prepared.cameraRevision;
        outputs.m_Scale = m_Document.renderScale;
        const auto &layout = prepared.layouts.render;
        // 画布尺寸相同且有效矩形吻合时, 可以直接读取已接受的颜色代.
        // 当前图案只依赖布局. 纯相机位姿变化不改写像素, 也不发布场景渲染历史.
        if (m_Accepted && !m_Accepted->scene && m_Accepted->layout == layout)
        {
            outputs.m_Output = m_Accepted;
            if (m_Reason != ViewSettingReason::None || m_Document.renderScale != m_EffectiveScale)
            {
                m_Reason = ViewSettingReason::SubmissionPending;
            }
            return outputs;
        }
        const auto bytes = DiagnosticPixelBytes(layout.outputExtent);
        if (bytes == 0)
        {
            return Unavailable(ViewSettingReason::DiagnosticBudget);
        }
        // 保留无颜色结果供 Editor 解析占位. 未创建新纹理, 下次按届时布局重新判断额度.
        if (bytes > remainingUploadBytes)
        {
            return Unavailable(ViewSettingReason::UploadPending);
        }

        if (!Core::AdvanceGeneration(m_LastGeneration))
        {
            return Core::BoundedText("Viewport diagnostic generation exhausted");
        }
        outputs.m_Output = ViewOutput{m_Id, ViewOutputRole::DisplayColor, m_LastGeneration, layout, {}, {}};
        outputs.m_UploadBytes = bytes;
        m_Reason = ViewSettingReason::SubmissionPending;
        return outputs;
    }

    std::variant<PreparedViewOutput, Core::BoundedText> View::PreparePreview(
        const PreparedView &prepared, std::optional<Core::BoundedText> degradation,
        std::optional<RenderedEnvironment> environment, std::uint64_t shaderGeneration,
        const ShadowDepthSet *shadow, OpaqueProduction production)
    {
        // 即使布局相同也分配独立场景代, 失败回退仍可只读上一代颜色及其相机记录.
        assert(prepared.camera && prepared.view == m_Id && prepared.reason == ViewSettingReason::None);
        if (!Core::AdvanceGeneration(m_LastGeneration))
        {
            return Core::BoundedText("Viewport scene output generation exhausted");
        }
        PreparedViewOutput outputs;
        outputs.m_RequestRevision = prepared.requestRevision;
        outputs.m_CameraRevision = prepared.cameraRevision;
        outputs.m_Scale = m_Document.renderScale;
        if (!shadow || !shadow->retained)
        {
            // 即使只有深度程序换代也应让旧组退出 View 记账. 旧图和槽各自保有原访问.
            m_ShadowDepth.reset();
        }
        const bool diagnostic = shadow && prepared.previewShadow;
        const auto displayMode = diagnostic ? PreviewMode::BaseColor : prepared.preview;
        const auto *local = diagnostic ? std::get_if<LocalShadowSelection>(&prepared.shadowDiagnostic) : nullptr;
        const auto cascades = diagnostic && !local ? shadow->count : 0;
        std::optional<RenderedShadows> shadowWork;
        if (shadow)
        {
            shadowWork = RenderedShadows{shadow->filter, shadow->edge, shadow->groups, shadow->count,
                shadow->retained ? 0u : shadow->count, shadow->retained ? shadow->count : 0u,
                shadow->drawBatches, shadow->LogicalBytes(), diagnostic};
        }
        std::uint64_t compatiblePredecessor = 0;
        if (!diagnostic && IsLitPreview(prepared.preview))
        {
            const auto &camera = *prepared.camera;
            auto shadows = shadow ? std::optional{m_ShadowSettings} : std::nullopt;
            if (shadows)
            {
                shadows->edge = shadow->edge;
                shadows->filter = shadow->filter;
            }
            // 投影对角尺度独立于本帧抖动. 采用渲染远面的语义, 不把随裁剪变化的有限工作范围当作相机 cut.
            outputs.m_OpaquePlan.emplace(PreparedViewOutput::OpaquePlan{
                prepared.scene.scene, prepared.scene.contentRevision, prepared.opaquePath, prepared.preview,
                prepared.layouts, camera.entity, camera.cutRevision, camera.renderClipFromView[0],
                camera.renderClipFromView[5],
                camera.nearPlane, camera.renderFar, camera.orthographic, shadows, environment, production});
            // 只给这份候选标记可接续的提交. 候选被裁剪或执行失败时, 上次接受的生产记录仍由 View 持有.
            if (m_AcceptedOpaquePlan && m_AcceptedOpaquePlan->key == *outputs.m_OpaquePlan)
            {
                compatiblePredecessor = m_AcceptedOpaquePlan->submission;
            }
        }
        // 将本轮策略, 曝光, 显示设置和程序代一起留在颜色候选. Publish 核对当前请求与提交回执后才更新历史.
        outputs.m_Output = ViewOutput{m_Id, ViewOutputRole::DisplayColor, m_LastGeneration, prepared.layouts.display, {},
            RenderedScene{prepared.scene.scene, prepared.scene.contentRevision, *prepared.camera, prepared.preview,
                          std::move(degradation), prepared.documentRevision, std::move(environment),
                          ResolveDisplaySettings(displayMode, prepared.requestedAa), prepared.layouts.render,
                          prepared.layouts.display.outputExtent, shaderGeneration, cascades,
                          prepared.shadowStatistics.requestRevision, local ? std::optional{*local} : std::nullopt,
                          0, shadowWork, prepared.opaquePath, compatiblePredecessor}};
        m_Reason = ViewSettingReason::SubmissionPending;
        return outputs;
    }

    PreparedViewOutput View::Unavailable(ViewSettingReason reason) noexcept
    {
        PreparedViewOutput outputs;
        outputs.m_UnavailableReason = m_Reason = reason;
        return outputs;
    }

    // 一次填充当前新代的紧密像素. 声明图只借用该区段, 不转移 arena 所有权.
    bool PreparedViewOutput::PrepareUpload(Core::FrameArena &arena) noexcept
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
    GraphResult<TextureId> PreparedViewOutput::Declare(RenderGraph &graph, std::vector<GraphUpload> &uploads) const
    {
        assert(m_Output);
        if (m_Output->retained)
        {
            return graph.ImportTexture(m_Output->retained, RHI::ResourceStates::ShaderResource);
        }
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
    bool View::Publish(PreparedViewOutput &&outputs, const RHI::TextureHandle &acceptedTexture,
                       const GraphRetainedResource &retained, std::uint64_t ticket) noexcept
    {
        // 拒绝安全图旧快照和已被后继编辑取代的候选, 即使尺寸或比例碰巧相同也不重新确认.
        const auto *color = retained ? retained.Texture() : acceptedTexture.Get();
        const auto *scene = outputs.m_Output && outputs.m_Output->scene ? &*outputs.m_Output->scene : nullptr;
        if (outputs.m_Retained || !color || !outputs.m_Output || outputs.m_Output->view != m_Id ||
            outputs.m_RequestRevision != m_RequestRevision || outputs.m_CameraRevision != m_CameraRevision ||
            (scene && (scene->mode != m_Preview || scene->shadowRequestRevision != m_ShadowRequestRevision ||
                scene->documentRevision != m_DocumentRevision || outputs.m_Output->generation != m_LastGeneration ||
                (m_PlanScene.IsValid() && scene->scene != m_PlanScene))))
        {
            return false;
        }
        // 等候另一条路径时仍可接受本轮重建的有效计划. 其资格来自同一场景的已接受路径.
        if (outputs.m_OpaquePlan && (!ticket || (scene->opaquePath != RequestedOpaquePath() &&
            OpaqueSettings(scene->scene).effective != scene->opaquePath)))
        {
            return false;
        }
        if (retained && (!retained.Accepted(ticket) || retained.Usage() != ExportUsage::ShaderRead ||
            color->GetDesc().width != outputs.m_Output->layout.outputExtent.width ||
            color->GetDesc().height != outputs.m_Output->layout.outputExtent.height))
        {
            return false;
        }
        if (outputs.m_Output->scene)
        {
            if (outputs.m_OpaquePlan)
            {
                ObserveScene(scene->scene);
                m_AcceptedOpaquePlan = AcceptedOpaquePlan{std::move(*outputs.m_OpaquePlan), ticket};
                if (scene->opaquePath == RequestedOpaquePath())
                {
                    m_OpaqueReason = ViewSettingReason::None;
                    m_OpaqueDetail.reset();
                    m_OpaqueRejection.reset();
                }
            }
            // 测试图恢复为空文档时只替换显示色, 不伪造一次新的场景渲染记录.
            outputs.m_Output->scene->submission = ticket;
            m_RenderedScene = outputs.m_Output->scene;
            if (m_RenderedScene->directionalCascades != 0 || m_RenderedScene->localShadow)
            {
                m_LastShadowDiagnostic = RenderedShadowDiagnostic{m_RenderedScene->scene, m_ShadowDiagnostic, ticket};
            }
            else if (m_LastShadowDiagnostic && m_LastShadowDiagnostic->scene != m_RenderedScene->scene)
            {
                m_LastShadowDiagnostic.reset();
            }
        }
        outputs.m_Output->texture = retained ? RHI::TextureHandle{} : acceptedTexture;
        outputs.m_Output->retained = retained;
        m_Accepted = std::move(outputs.m_Output);
        m_EffectiveScale = outputs.m_Scale;
        m_Reason = ViewSettingReason::None;
        return true;
    }
}
