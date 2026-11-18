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
/// @brief 准备视口候选和测试图, 仅用被实际采样的接受回执发布显示色与场景记录.
/// 持久请求先校验并求值镜头/曝光, 再一起发布; 准备和确认输出是之后的独立边界.
/// 文档, 几何相机和比例各有请求代, 不能用布局或矩阵未变来认定曝光候选仍有效.

namespace Physara::Renderer
{
    static constexpr std::size_t PixelBytes = 4;
    // 进程只有一个活动宿主. View 在该宿主 owner 线程分配身份, 后续创建不会复用旧代.
    static std::uint64_t LastViewIdentity = 0;

    /// @brief 校验文档中的比例、位姿和引用形状. 具体相机引用仍由组合方对 Scene 检查.
    static std::optional<Core::BoundedText> ValidateDocumentFields(const ViewDocumentRequests &requests) noexcept
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
                              .requestRevision = m_RequestRevision, .preview = m_Preview};
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
            if (m_Document.physicalCamera != update.m_Value.physicalCamera)
            {
                m_Reason = ViewSettingReason::SubmissionPending;
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

    // 只复制已接受资源的强引用, 不调用 Prepare, 因而不借用当前布局或生成新候选.
    PreparedViewOutput View::LastAcceptedOutput(ViewSettingReason reason) const noexcept
    {
        PreparedViewOutput outputs;
        outputs.m_Output = m_Accepted;
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
        // 画布尺寸相同且有效矩形吻合时, 可以直接读取已接受的颜色代.
        // 当前图案只依赖布局. 纯相机位姿变化不改写像素, 也不发布场景渲染历史.
        if (m_Accepted && !m_Accepted->scene && m_Accepted->layout == prepared.layout)
        {
            outputs.m_Output = m_Accepted;
            if (m_Reason != ViewSettingReason::None || m_Document.renderScale != m_EffectiveScale)
            {
                m_Reason = ViewSettingReason::SubmissionPending;
            }
            return outputs;
        }
        const auto bytes = DiagnosticPixelBytes(prepared.layout.outputExtent);
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
        outputs.m_Output = ViewOutput{m_Id, ViewOutputRole::DisplayColor, m_LastGeneration, prepared.layout, {}, {}};
        outputs.m_UploadBytes = bytes;
        m_Reason = ViewSettingReason::SubmissionPending;
        return outputs;
    }

    std::variant<PreparedViewOutput, Core::BoundedText> View::PreparePreview(
        const PreparedView &prepared, std::optional<Core::BoundedText> degradation,
        std::optional<RenderedEnvironment> environment)
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
        // 记录来自准备快照的曝光与请求代, 不能在确认阶段改取届时的当前请求.
        outputs.m_Output = ViewOutput{m_Id, ViewOutputRole::DisplayColor, m_LastGeneration, prepared.layout, {},
            RenderedScene{prepared.scene.scene, prepared.scene.contentRevision, *prepared.camera, m_Preview,
                          std::move(degradation), prepared.documentRevision, std::move(environment)}};
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
    bool View::Publish(PreparedViewOutput &&outputs, const RHI::TextureHandle &acceptedTexture) noexcept
    {
        // 拒绝安全图旧快照和已被后继编辑取代的候选, 即使尺寸或比例碰巧相同也不重新确认.
        if (outputs.m_Retained || !acceptedTexture || !outputs.m_Output || outputs.m_Output->view != m_Id ||
            outputs.m_RequestRevision != m_RequestRevision || outputs.m_CameraRevision != m_CameraRevision ||
            (outputs.m_Output->scene && (outputs.m_Output->scene->mode != m_Preview ||
                                         outputs.m_Output->scene->documentRevision != m_DocumentRevision)))
        {
            return false;
        }
        if (outputs.m_Output->scene)
        {
            // 测试图恢复为空文档时只替换显示色, 不伪造一次新的场景渲染记录.
            m_RenderedScene = outputs.m_Output->scene;
        }
        outputs.m_Output->texture = acceptedTexture;
        m_Accepted = std::move(outputs.m_Output);
        m_EffectiveScale = outputs.m_Scale;
        m_Reason = ViewSettingReason::None;
        return true;
    }
}
