#include "ViewPreparation.hpp"

/// @file
/// @brief 以相机刚体轴和稳定有限投影准备主可见集, 采样偏移只改变渲染矩阵.
/// 布局和相机先确定稳定视锥, 再从同一 SceneFrame 选出主可见集并交付每路的阴影计划.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <new>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "ViewFrustum.hpp"

namespace Physara::Renderer
{
    using Detail::CullingPlane;
    using Detail::Outside;
    using Detail::PlaneTerms;
    using Detail::SupportPoint;

    /// @brief 成对保留正反投影, 解析逆式避免用通用 4x4 行列式放大近远范围的数值风险.
    struct ProjectionMatrices
    {
        glm::dmat4 clipFromView{0.0};
        glm::dmat4 viewFromClip{0.0};
    };

    /// @brief 已接受投影的派生尺度. aspect 用于留边, scaleX/scaleY 用于矩阵与裁剪面.
    struct ProjectionParameters
    {
        double aspect;
        double scaleX;
        double scaleY;
        double nearPlane;
        std::optional<double> farPlane;
        bool orthographic;
        bool fixedAspect; ///< 显式透视比例或正交画幅须分别在两张栅格留边; 自适应透视则共享显示投影.
    };

    /// @brief 从有效渲染像素换算的 NDC 参数, 将实际偏移与稳定裁剪上界分开保存.
    struct SamplingOffsets
    {
        glm::dvec2 jitter;
        glm::dvec2 sideLimit; ///< 展宽后的 NDC 侧面极值, 正负两侧共用.
    };

    /// @brief 相机数组为列主序的 CPU 值, 无第三方对象寿命依赖.
    static std::array<double, 16> MatrixValue(const glm::dmat4 &matrix) noexcept
    {
        std::array<double, 16> value;
        std::copy_n(glm::value_ptr(matrix), value.size(), value.begin());
        return value;
    }

    /// @brief 拒绝派生矩阵中的非有限分量; 投影参数的合法区间已由 Scene/View 入口检查.
    static bool FiniteMatrix(const glm::dmat4 &matrix) noexcept
    {
        return std::ranges::all_of(std::span(glm::value_ptr(matrix), 16),
                                   [](double value) { return std::isfinite(value); });
    }

    /// @brief 从已校验相机取得矩阵尺度. 画布只为未指定比例的透视提供宽高比.
    static ProjectionParameters Parameters(const Scene::Camera &projection, ViewExtent output) noexcept
    {
        if (const auto *camera = std::get_if<Asset::ModelPerspective>(&projection))
        {
            const auto aspect = camera->aspectRatio.value_or(static_cast<double>(output.width) / output.height);
            const auto scale = 1.0 / std::tan(camera->yfov * 0.5);
            return {aspect, scale / aspect, scale, camera->znear, camera->zfar, false, camera->aspectRatio.has_value()};
        }
        const auto &camera = std::get<Asset::ModelOrthographic>(projection);
        return {camera.xmag / camera.ymag, 1.0 / camera.xmag, 1.0 / camera.ymag, camera.znear, camera.zfar, true, true};
    }

    /// @brief 以限制轴居中保留比例. 非限制轴向下取整, 余下的奇数像素位于右或下.
    static std::optional<ViewImageLayout> FitImage(ViewExtent canvas, double aspect) noexcept
    {
        auto extent = canvas;
        const double canvasAspect = static_cast<double>(canvas.width) / canvas.height;
        if (aspect > canvasAspect)
        {
            extent.height = static_cast<std::uint32_t>(std::floor(canvas.width / aspect));
        }
        else if (aspect < canvasAspect)
        {
            extent.width = static_cast<std::uint32_t>(std::floor(canvas.height * aspect));
        }
        if (extent.width == 0 || extent.height == 0)
        {
            return std::nullopt;
        }
        return ViewImageLayout{canvas, {(canvas.width - extent.width) / 2, (canvas.height - extent.height) / 2,
                                        extent}};
    }

    /// @brief 对矩形有效采样尺寸求 NDC 偏移和保守侧面范围, 下方像素对应 NDC 负 Y.
    static std::optional<SamplingOffsets> SampleOffsets(const CameraSampling &sampling, ViewExtent render) noexcept
    {
        SamplingOffsets offsets{};
        const std::array<double, 2> extent{static_cast<double>(render.width), static_cast<double>(render.height)};
        for (glm::length_t axis = 0; axis < 2; ++axis)
        {
            const auto i = static_cast<std::size_t>(axis);
            const auto displacement = sampling.jitterPixels[i];
            const auto limit = sampling.maxAbsJitterPixels[i];
            const auto filter = sampling.filterRadiusPixels[i];
            if (!std::isfinite(displacement) || !std::isfinite(limit) || !std::isfinite(filter) ||
                !(std::abs(displacement) <= limit) || filter < 0)
            {
                return std::nullopt;
            }
            offsets.jitter[axis] = (axis == 0 ? 2.0 : -2.0) * (displacement / extent[i]);
            offsets.sideLimit[axis] = 1.0 + 2.0 * (limit / extent[i] + filter / extent[i]);
            if (!std::isfinite(offsets.sideLimit[axis]))
            {
                return std::nullopt;
            }
        }
        return offsets;
    }

    /// @brief 构造右手系 Reverse-Z 投影. 透视 clip.w 为正向视深, 正交 clip.w 固定为 1.
    static ProjectionMatrices Project(const ProjectionParameters &parameters, std::optional<double> farPlane,
                                       const glm::dvec2 &jitter) noexcept
    {
        const double nearPlane = parameters.nearPlane;
        const double scaleX = parameters.scaleX;
        const double scaleY = parameters.scaleY;
        ProjectionMatrices matrices;
        auto &clip = matrices.clipFromView;
        auto &view = matrices.viewFromClip;
        clip[0][0] = scaleX;
        clip[1][1] = scaleY;
        view[0][0] = 1.0 / scaleX;
        view[1][1] = 1.0 / scaleY;
        view[3][0] = -jitter.x / scaleX;
        view[3][1] = -jitter.y / scaleY;
        if (parameters.orthographic)
        {
            const double span = *farPlane - nearPlane;
            clip[2][2] = 1.0 / span;
            clip[3] = {jitter.x, jitter.y, *farPlane / span, 1.0};
            view[2][2] = span;
            view[3][2] = -*farPlane;
            view[3][3] = 1.0;
        }
        else
        {
            // 令 d=-viewZ, 深度为 -depthScale + depthOffset/d; 无远面时退化为 near/d.
            const double depthScale = farPlane ? nearPlane / (*farPlane - nearPlane) : 0;
            const double depthOffset = nearPlane * (1.0 + depthScale);
            clip[2] = {-jitter.x, -jitter.y, depthScale, -1.0};
            clip[3][2] = depthOffset;
            view[2][3] = 1.0 / depthOffset;
            view[3][2] = -1.0;
            view[3][3] = depthScale / depthOffset;
        }
        return matrices;
    }

    /// @brief 有限相机沿用远面. 无限透视的稳定远面覆盖场景可绘范围的最远正向深度.
    static double CullingFar(const ProjectionParameters &parameters, const std::optional<Scene::Bounds> &bounds,
                             const glm::dvec3 &position, const glm::dvec3 &forward) noexcept
    {
        if (parameters.farPlane)
        {
            return *parameters.farPlane;
        }
        // 空场景仍须有严格大于 near 的有限裁剪区间, 渲染投影的无限远语义不受影响.
        double farPlane = 2 * parameters.nearPlane;
        if (bounds)
        {
            const auto terms = PlaneTerms(SupportPoint(*bounds, forward), position, forward);
            const double depth = terms.x + terms.y + terms.z;
            if (!std::isfinite(depth))
            {
                return depth;
            }
            farPlane = std::max(farPlane, depth);
        }
        return farPlane;
    }

    /// @brief 从投影尺度构造未抖动的视锥半空间. 侧面使用 profile 的最大采样范围.
    static std::array<CullingPlane, 6> Frustum(const CameraSnapshot &camera,
                                              const SamplingOffsets &sampling) noexcept
    {
        const auto world = glm::make_mat4(camera.worldFromView.data());
        const glm::dvec3 right(world[0]);
        const glm::dvec3 up(world[1]);
        const glm::dvec3 forward(-world[2]);
        const double sx = camera.stableClipFromView[0];
        const double sy = camera.stableClipFromView[5];
        auto side = [&](const glm::dvec3 &axis, double scale, double limit) -> CullingPlane
        {
            if (camera.orthographic)
            {
                return {axis, limit / scale};
            }
            // 先归一系数, 避免投影尺度较大时方向长度溢出.
            const double largest = std::max(scale, limit);
            return {glm::normalize(axis * (scale / largest) + forward * (limit / largest)), 0};
        };
        return {side(right, sx, sampling.sideLimit.x), side(-right, sx, sampling.sideLimit.x),
                side(up, sy, sampling.sideLimit.y), side(-up, sy, sampling.sideLimit.y),
                CullingPlane{forward, -camera.nearPlane}, CullingPlane{-forward, camera.cullingFar}};
    }

    std::optional<CameraRay> CameraSnapshot::RayAt(double u, double v) const noexcept
    {
        if (!(u >= 0 && u < 1 && v >= 0 && v < 1))
        {
            return std::nullopt;
        }
        const double x = (2 * u - 1) * viewFromStableClip[0];
        const double y = (1 - 2 * v) * viewFromStableClip[5];
        const auto basis = glm::make_mat4(worldFromView.data());
        glm::dvec3 origin(basis[3]);
        glm::dvec3 direction(-basis[2]);
        if (orthographic)
        {
            // 直接使用声明的近平面, 避免近远跨度大时相减丢失近面距离.
            origin = glm::dvec3(basis * glm::dvec4(x, y, -nearPlane, 1));
        }
        else
        {
            // 在视深 1 处取方向, 省去透视除法及两个世界位置相减.
            direction = glm::dmat3(basis) * glm::dvec3(x, y, -1);
        }
        const auto length = std::hypot(direction.x, direction.y, direction.z);
        if (!std::isfinite(length) || length <= 0 ||
            !std::isfinite(origin.x) || !std::isfinite(origin.y) || !std::isfinite(origin.z))
        {
            return std::nullopt;
        }
        return CameraRay{{origin.x, origin.y, origin.z}, {direction.x / length, direction.y / length,
                                                         direction.z / length}};
    }

    /// @brief 捕获刚体相机, 两份投影及 View 已求值的镜头/曝光. 仅接受正反矩阵都可表示的结果.
    static std::optional<CameraSnapshot> BuildCamera(const Scene::CameraState &input,
        const ProjectionParameters &parameters, const SceneFrame &frame, std::uint64_t cameraRevision,
        const CameraSampling &profile, const SamplingOffsets &sampling, const View &owner) noexcept
    {
        const auto &q = input.pose.rotation;
        auto world = glm::mat4_cast(glm::normalize(glm::dquat(q[3], q[0], q[1], q[2])));
        const auto position = glm::make_vec3(input.pose.translation.data());
        const double farPlane = CullingFar(parameters, frame.drawableBounds, position, glm::dvec3(-world[2]));
        if (!std::isfinite(farPlane) || farPlane <= parameters.nearPlane)
        {
            return std::nullopt;
        }
        auto view = glm::transpose(world);
        view[3] = glm::dvec4(-(glm::dmat3(view) * position), 1);
        world[3] = glm::dvec4(position, 1);
        const auto stable = Project(parameters, farPlane, glm::dvec2(0));
        const auto render = Project(parameters, parameters.farPlane, sampling.jitter);
        if (!FiniteMatrix(view) || !FiniteMatrix(stable.clipFromView) || !FiniteMatrix(stable.viewFromClip) ||
            !FiniteMatrix(render.clipFromView) || !FiniteMatrix(render.viewFromClip) ||
            stable.clipFromView[2][2] <= 0)
        {
            return std::nullopt;
        }
        // 曝光不参与几何矩阵或裁剪. 场景投影保留源 yfov, 有效显示焦距另由 View 反推.
        return CameraSnapshot{input.entity.scene.IsValid() ? std::optional(input.entity) : std::nullopt, cameraRevision,
            MatrixValue(world), MatrixValue(view), MatrixValue(stable.clipFromView), MatrixValue(stable.viewFromClip),
            MatrixValue(render.clipFromView), MatrixValue(render.viewFromClip), profile.jitterPixels,
            parameters.aspect, parameters.nearPlane, farPlane, parameters.farPlane, parameters.orthographic,
            owner.ResolveLens(input.entity.scene.IsValid() ? &input.projection : nullptr), owner.Exposure()};
    }

    const std::variant<SceneFrame, Core::BoundedText> &SceneFramePreparation::Extract()
    {
        if (!m_Extracted)
        {
            m_Extracted.emplace(m_Storage.Prepare(m_Scene));
        }
        return *m_Extracted;
    }

    const SceneFrame *SceneFramePreparation::Result() const noexcept
    {
        return m_Extracted ? std::get_if<SceneFrame>(&*m_Extracted) : nullptr;
    }

    PreparedView ViewWork::Prepare(SceneFrameStorage &sceneStorage, const Scene::Scene &scene, const View &view,
                                   const std::optional<ViewportRequest> &request, const CameraSampling &sampling,
                                   ViewportUse use, const ShadowPlanningLimits &shadowLimits)
    {
        SceneFramePreparation frame(sceneStorage, scene);
        return Prepare(frame, view, request, sampling, use, shadowLimits);
    }

    ShadowPlanStatistics ViewWork::UnavailableShadows(const Scene::Scene &scene, const View &view,
                                                      const ShadowPlanningLimits &limits) noexcept
    {
        m_Shadows.reset();
        view.m_ShadowDepth.reset();
        return view.m_ShadowPlan.Unavailable(scene.Id(), scene.Versions(), view.m_ShadowSettings,
                                             view.m_ShadowRequestRevision, limits).statistics;
    }

    PreparedView ViewWork::Prepare(SceneFramePreparation &shared, const View &view,
                                   const std::optional<ViewportRequest> &request, const CameraSampling &sampling,
                                   ViewportUse use, const ShadowPlanningLimits &shadowLimits,
                                   bool requireShadowPreview)
    {
        m_Primitives.clear();
        m_Lights.clear();
        PreparedView result{.view = view.Id(), .requestRevision = request ? request->requestRevision : 0,
                            .cameraRevision = view.CameraRevision(), .documentRevision = view.DocumentRevision(),
                            .preview = view.Preview(), .reason = view.CheckViewport(request, use),
                            .shadowDiagnostic = view.RequestedShadowDiagnostic()};
        const auto &scene = shared.Source();
        const auto unavailable = [&]
        {
            // 即使本轮在提取前退出, 也要结束上次阴影映射, 并将原因与这次 View 准备一起交付.
            result.shadowStatistics = UnavailableShadows(scene, view, shadowLimits);
            return result;
        };
        if (result.reason != ViewSettingReason::None)
        {
            return unavailable();
        }
        const auto &document = view.DocumentRequests();
        // 实体相机读取 Scene 已传播的世界刚体和资产投影, 导航相机读取 View 的位姿和镜头.
        const auto camera = document.cameraEntityId ? scene.FindCamera(*document.cameraEntityId) :
            std::optional(Scene::CameraState{{}, document.cameraPose, view.NavigationCameraProjection()});
        if (!camera)
        {
            result.reason = ViewSettingReason::CameraUnavailable;
            return unavailable();
        }
        const auto parameters = Parameters(camera->projection, request->outputExtent);
        if (!std::isfinite(parameters.aspect) || parameters.aspect <= 0 ||
            !std::isfinite(parameters.scaleX) || parameters.scaleX <= 0 ||
            !std::isfinite(parameters.scaleY) || parameters.scaleY <= 0)
        {
            result.reason = ViewSettingReason::InvalidProjection;
            return unavailable();
        }
        // 固定投影在各栅格独立留边; 自适应透视取显示宽高比, 使用各栅格的全部像素.
        const auto display = FitImage(request->outputExtent, parameters.aspect);
        const auto render = parameters.fixedAspect ? FitImage(request->renderExtent, parameters.aspect) :
            std::optional(ViewImageLayout{request->renderExtent, {0, 0, request->renderExtent}});
        if (!display || !render)
        {
            result.reason = ViewSettingReason::InvalidImageRect;
            return unavailable();
        }
        const auto offsets = SampleOffsets(sampling, render->rect.extent);
        if (!offsets)
        {
            result.reason = ViewSettingReason::InvalidSampling;
            return unavailable();
        }
        // 后续的无限远裁剪需要场景范围. 到达这里的各路共用提取结果, 裁剪索引分别写到各自的 ViewWork.
        const auto &extracted = shared.Extract();
        if (const auto *error = std::get_if<Core::BoundedText>(&extracted))
        {
            result.reason = ViewSettingReason::ScenePreparationFailed;
            result.failure = *error;
            return unavailable();
        }
        const auto &frame = std::get<SceneFrame>(extracted);
        const auto snapshot = BuildCamera(*camera, parameters, frame, scene.Versions().camera, sampling, *offsets, view);
        if (!snapshot)
        {
            result.reason = ViewSettingReason::InvalidProjection;
            return unavailable();
        }

        // 仅工作集随相机/采样 profile 重建, 静态记录在同一安全槽内复用.
        // 相交或数值上无法证明分离时保留候选, 这些索引不承担屏幕外阴影 caster 的筛选.
        const auto cullingStart = std::chrono::steady_clock::now();
        const auto planes = Frustum(*snapshot, *offsets);
        const auto origin = glm::make_vec3(camera->pose.translation.data());
        try
        {
            for (std::uint32_t index = 0; index < frame.primitives.size(); ++index)
            {
                const auto &primitive = frame.primitives[index];
                if (primitive.visible && primitive.bounds && std::ranges::none_of(planes, [&](const auto &plane)
                {
                    return Outside(plane, SupportPoint(*primitive.bounds, plane.normal), origin);
                }))
                {
                    m_Primitives.push_back(index);
                }
            }
            for (std::uint32_t index = 0; index < frame.lights.size(); ++index)
            {
                const auto &light = frame.lights[index];
                // Scene 保证方向灯没有 range. 有限点光和聚光均用完整影响球, 不以灯心代替范围.
                if (light.visible && (!light.light.range || std::ranges::none_of(planes, [&](const auto &plane)
                {
                    return Outside(plane, glm::make_vec3(light.position.data()), origin, *light.light.range);
                })))
                {
                    m_Lights.push_back(index);
                }
            }
        }
        catch (const std::bad_alloc &)
        {
            result.reason = ViewSettingReason::ScenePreparationFailed;
            result.failure.emplace("View visibility storage allocation failed");
            return unavailable();
        }
        result.camera = snapshot;
        result.requestedAa = document.antiAliasing;
        result.layouts = {*render, *display};
        result.scene = frame;
        result.primitives = m_Primitives;
        result.lights = m_Lights;
        result.cullingSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - cullingStart).count();
        const bool hasVisibleReceivers = !m_Primitives.empty();
        // Lighting 读取冻结场景全表. 保持主可见集及其计时口径, 即使阴影预算拒绝也交付完整灯集.
        auto shadows = view.m_ShadowPlan.Prepare(frame, *snapshot, planes, {offsets->sideLimit.x, offsets->sideLimit.y},
            m_Lights, view.m_ShadowSettings, view.m_ShadowRequestRevision, shadowLimits,
            result.shadowDiagnostic, requireShadowPreview, hasVisibleReceivers);
        // PreparedView 只借裸指针. 本槽保有这份计划, 让后续槽可以独立替换同一 View 的缓存.
        m_Shadows = std::move(shadows.plan);
        result.shadows = m_Shadows.get();
        result.previewShadow = hasVisibleReceivers ? shadows.diagnostic : nullptr;
        result.shadowStatistics = shadows.statistics;
        if (shadows.diagnostic && !hasVisibleReceivers)
        {
            result.shadowStatistics.diagnosticReason = ShadowPlanReason::NoReceiverCoverage;
        }
        if (requireShadowPreview && !result.previewShadow)
        {
            // 规划器已在旧计划上完成释放. 保留该次摘要和实际执行的裁剪记录.
            result.reason = ViewSettingReason::SceneAttachmentLimit;
        }
        // 候选仍有消费时借用本 View 的旧数组. Forward 按实际绘制组、内容和程序匹配它.
        if (view.m_ShadowDepth)
        {
            const bool lighting = hasVisibleReceivers &&
                (result.preview == PreviewMode::DirectLighting || result.preview == PreviewMode::PhysicalLighting);
            if (!result.ShadowGroups(lighting).empty())
            {
                result.acceptedShadowDepth = view.m_ShadowDepth;
            }
            else
            {
                view.m_ShadowDepth.reset();
            }
        }
        return result;
    }
}
