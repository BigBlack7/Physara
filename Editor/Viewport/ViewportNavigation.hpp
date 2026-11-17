#pragma once

/// @file
/// @brief 主视口的单次 Fly/Orbit/Pan 相对捕获. 输出拥有型位姿意图, 由历史管理实际写入.
/// Pan 使用开始捕获时的有效导航镜头, 对曝光和物理参数本身不产生编辑.

#include <span>

#include "Commands/EditHistory.hpp"
#include "Viewport.hpp"

namespace Physara::Editor::Detail
{
    /// @brief 单活动导航捕获的状态机. 只生成位姿手势, 不直接改变 View 或 GLFW 模式.
    /// @note Session 负责输入归属与历史, Runtime 负责宿主请求; 下一轮观察实际相对模式后才移动.
    class ViewportNavigation final
    {
    public:
        [[nodiscard]] bool Active() const noexcept { return m_Capture.has_value(); }
        /// @brief 用已通过图像命中和输入归属的 Press 启动手势, 首帧只申请宿主捕获.
        /// @pre 当前没有捕获, view 使用导航相机, region 是有效逻辑图像范围且按下尚未释放.
        /// @return 非导航按钮返回空手势, 有效按钮固定文档/View/布局, 导航视场和本次世界锚点.
        [[nodiscard]] FieldGesture Start(const Scene::Scene &scene, const Renderer::View &view,
            const std::optional<Scene::EntityHandle> &selection, const ViewportRegion &region,
            const Platform::WindowState &window, const Platform::MouseButtonEvent &press) noexcept;
        /// @brief 消费本轮有资格的相对位移, 遇到 Release 截断运动并完成手势.
        /// @param eligible 当轮 UI、焦点及布局归属仍允许相机捕获.
        /// @pre 已有活动捕获; events 是本轮尚未消费的有序宿主事件, seconds 使用秒.
        /// @note 位姿候选由 View 再校验. 捕获不因其自身位姿更新的图像尚未接受而重新开始.
        /// @return 空目标表示取消. 结束后调用者按宿主已观察模式归还光标.
        [[nodiscard]] FieldGesture Step(const Scene::Scene *scene, const Renderer::View *view,
            const std::optional<ViewportRegion> &region, const Platform::HostFrameInput &input,
            std::span<const Platform::InputEvent> events, bool eligible, double seconds) noexcept;
        void Reset() noexcept { m_Capture.reset(); }

    private:
        enum class Mode : std::uint8_t { Fly, Orbit, Pan };

        /// @brief 按下时冻结的输入目标和布局. 不保存 Scene/View/ImGui 的借用或可写副本.
        struct Capture
        {
            NavigationTarget target;
            Platform::MouseButton button;
            Mode mode;
            ViewportRegion region;
            std::uint64_t metricsRevision;
            std::uint64_t extentRevision;
            std::array<double, 3> pivot; ///< 按下时的固定世界锚点, 单位为米.
            double tanHalfYfov; ///< 捕获时导航相机半垂直视场的正切, 用于把 Pan 像素位移换成米.
        };

        /// @brief 从最近接受的导航位姿计算本批候选, 鼠标用逻辑单位, Fly 键步长最多取 0.1 秒.
        [[nodiscard]] static std::optional<Scene::Pose> Move(const Capture &capture, const Scene::Pose &current,
            Platform::LogicalPoint motion, const Platform::InputSnapshot &input, double seconds, bool released)
            noexcept;

        std::optional<Capture> m_Capture;
    };
}
