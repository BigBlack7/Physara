#include "ViewportNavigation.hpp"

/// @file
/// @brief 用真实捕获区间的有序鼠标运动和有界键盘步长构造导航相机位姿.
/// 点和方向使用右手系, 相机局部 -Z 为前方, 局部 +Y 为上方.

#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>

#include <glm/gtc/quaternion.hpp>

namespace Physara::Editor::Detail
{
    static constexpr glm::dvec3 Up{0, 1, 0};
    static constexpr glm::dvec3 Forward{0, 0, -1};
    static constexpr glm::dvec3 Right{1, 0, 0};
    static constexpr double DefaultOrbitDistance = 4.0; // 未选择可用几何时的导航距离, 单位为米.

    // 使用当前领域保存的 xyzw 约定, GLM 构造参数为 wxyz.
    static glm::dquat Rotation(const Scene::Pose &pose) noexcept
    {
        const auto &q = pose.rotation;
        return {q[3], q[0], q[1], q[2]};
    }

    // 仅使用在眼前的非退化锚点. 选择不带几何的实体时使用相机光轴上的确定距离.
    static std::array<double, 3> Pivot(const Scene::Scene &scene,
        const std::optional<Scene::EntityHandle> &selection, const Scene::Pose &pose) noexcept
    {
        const glm::dvec3 eye(pose.translation[0], pose.translation[1], pose.translation[2]);
        const auto forward = Rotation(pose) * Forward;
        if (const auto entity = selection ? scene.Get(*selection) : std::nullopt)
        {
            if (const auto &bounds = entity->world.bounds)
            {
                const glm::dvec3 center(std::midpoint(bounds->minimum[0], bounds->maximum[0]),
                    std::midpoint(bounds->minimum[1], bounds->maximum[1]),
                    std::midpoint(bounds->minimum[2], bounds->maximum[2]));
                const auto distance = glm::dot(center - eye, forward);
                if (std::isfinite(distance) && distance > Renderer::NavigationProjection.znear)
                {
                    return {center.x, center.y, center.z};
                }
            }
        }
        const auto center = eye + forward * DefaultOrbitDistance;
        return {center.x, center.y, center.z};
    }

    // 这里只选择手势并固定目标. 原始按下位置的正常图命中和顶层输入归属已由 Session 核对.
    FieldGesture ViewportNavigation::Start(const Scene::Scene &scene, const Renderer::View &view,
        const std::optional<Scene::EntityHandle> &selection, const ViewportRegion &region,
        const Platform::WindowState &window, const Platform::MouseButtonEvent &press) noexcept
    {
        using enum Platform::MouseButton;
        Mode mode;
        if (press.button == Secondary)
        {
            mode = Mode::Fly;
        }
        else if (press.button == Middle)
        {
            mode = Mode::Pan;
        }
        else if (press.button == Primary && (static_cast<unsigned>(press.modifiers) &
                 static_cast<unsigned>(Platform::Modifier::Alt)) != 0)
        {
            mode = Mode::Orbit;
        }
        else
        {
            return {};
        }
        const NavigationTarget target{scene.Id(), view.Id()};
        m_Capture = Capture{target, press.button, mode, region, window.metricsRevision, view.Settings().requestRevision,
                            Pivot(scene, selection, view.DocumentRequests().cameraPose)};
        return {target, {}, true, false};
    }

    // 运动只累加到首次释放. Host 已按焦点和相对模式建立逻辑位移基线.
    FieldGesture ViewportNavigation::Step(const Scene::Scene *scene, const Renderer::View *view,
        const std::optional<ViewportRegion> &region, const Platform::HostFrameInput &input,
        std::span<const Platform::InputEvent> events, bool eligible, double seconds) noexcept
    {
        const auto &capture = *m_Capture;
        if (!eligible || !scene || !view || scene->Id() != capture.target.scene || view->Id() != capture.target.view ||
            view->DocumentRequests().cameraEntityId || region != capture.region ||
            input.window.metricsRevision != capture.metricsRevision ||
            view->Settings().requestRevision != capture.extentRevision ||
            input.state.cursorMode != Platform::CursorMode::Relative)
        {
            Reset();
            return {};
        }
        Platform::LogicalPoint motion;
        bool released = false;
        for (const auto &event : events)
        {
            if (const auto *key = std::get_if<Platform::KeyEvent>(&event);
                key && key->key == Platform::Key::Escape && key->action == Platform::KeyAction::Press)
            {
                Reset();
                return {};
            }
            if (const auto *button = std::get_if<Platform::MouseButtonEvent>(&event);
                button && button->button == capture.button && button->action == Platform::KeyAction::Release)
            {
                released = true;
                break;
            }
            if (const auto *cursor = std::get_if<Platform::CursorPositionEvent>(&event);
                cursor && cursor->mode == Platform::CursorMode::Relative)
            {
                motion.x += cursor->deltaLogical.x;
                motion.y += cursor->deltaLogical.y;
            }
        }
        if (!released && !input.state.MouseState(capture.button).down)
        {
            Reset();
            return {};
        }
        const auto pose = Move(capture, view->DocumentRequests().cameraPose, motion, input.state, seconds, released);
        FieldGesture result{capture.target, pose ? std::optional<FieldValue>(*pose) : std::nullopt, false, released};
        if (released)
        {
            Reset();
        }
        return result;
    }

    // 围绕世界 Y 轴转向, 再绕视线的水平横轴俯仰. 同一增量也用于 Orbit 的眼点偏移.
    static glm::dquat LookTurn(const glm::dquat &rotation, Platform::LogicalPoint motion) noexcept
    {
        constexpr double RadiansPerLogicalPixel = 0.004;
        constexpr double PitchLimit = std::numbers::pi / 2 - 0.01;
        const auto yaw = glm::angleAxis(-motion.x * RadiansPerLogicalPixel, Up);
        const auto turned = yaw * rotation;
        const auto forward = turned * Forward;
        const auto elevation = std::asin(std::clamp(forward.y, -1.0, 1.0));
        const double pitch = std::clamp(-motion.y * RadiansPerLogicalPixel,
            std::min(0.0, -PitchLimit - elevation), std::max(0.0, PitchLimit - elevation));
        auto axis = glm::cross(forward, Up);
        // 接近竖直的保存位姿仍以现有相机右轴作为有效的起始基准.
        axis = glm::dot(axis, axis) > 1.0e-12 ? glm::normalize(axis) : turned * Right;
        return glm::angleAxis(pitch, axis) * yaw;
    }

    // 键盘读取本轮 held, 鼠标只用捕获期间的逻辑位移. 拖动速度与 framebuffer 比例无关.
    std::optional<Scene::Pose> ViewportNavigation::Move(const Capture &capture, const Scene::Pose &current,
        Platform::LogicalPoint motion, const Platform::InputSnapshot &input, double seconds, bool released) noexcept
    {
        if (!std::isfinite(motion.x) || !std::isfinite(motion.y))
        {
            return std::nullopt;
        }
        const bool moved = motion.x != 0 || motion.y != 0;
        const glm::dvec3 original(current.translation[0], current.translation[1], current.translation[2]);
        const glm::dvec3 pivot(capture.pivot[0], capture.pivot[1], capture.pivot[2]);
        auto eye = original;
        auto pose = current;
        auto rotation = Rotation(current);
        if (moved && capture.mode != Mode::Pan)
        {
            const auto turn = LookTurn(rotation, motion);
            rotation = glm::normalize(turn * rotation);
            pose.rotation = {rotation.x, rotation.y, rotation.z, rotation.w};
            if (capture.mode == Mode::Orbit)
            {
                eye = pivot + turn * (eye - pivot);
            }
        }
        if (moved && capture.mode == Mode::Pan)
        {
            // 导航投影填满画布. 用逻辑高度和观察深度求每逻辑单位的世界位移, 不再乘 DPI 或比例.
            const auto &canvas = capture.region.canvas;
            const double depth = glm::dot(pivot - eye, rotation * Forward);
            const double distance = depth > Renderer::NavigationProjection.znear ? depth : DefaultOrbitDistance;
            const auto scale = 2.0 * distance * std::tan(Renderer::NavigationProjection.yfov / 2) /
                               (canvas[3] - canvas[1]);
            eye += scale * ((rotation * Right) * -motion.x + (rotation * Up) * motion.y);
        }
        if (capture.mode == Mode::Fly && !released)
        {
            // Release 批次不再使用批末 held 推进键盘位移, 避免消费启动按钮释放之后才到达的按键.
            const auto down = [&](Platform::Key key) { return static_cast<int>(input.KeyState(key).down); };
            using Key = Platform::Key;
            const auto step = (rotation * Forward) * static_cast<double>(down(Key::W) - down(Key::S)) +
                              (rotation * Right) * static_cast<double>(down(Key::D) - down(Key::A)) +
                              Up * static_cast<double>(down(Key::E) - down(Key::Q));
            const auto length = glm::length(step);
            if (length > 0)
            {
                constexpr double MoveSpeedMetersPerSecond = 4.0;
                eye += step / length * (std::clamp(seconds, 0.0, 0.1) * MoveSpeedMetersPerSecond);
            }
        }
        pose.translation = {eye.x, eye.y, eye.z};
        return pose != current ? std::optional(pose) : std::nullopt;
    }
}
