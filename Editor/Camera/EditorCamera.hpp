#pragma once

#include <cstdint>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <Engine/Renderer/RenderView.hpp>
#include <Engine/Scene/EntityId.hpp>
#include <Engine/Scene/Components/CameraComponent.hpp>

namespace Physara::Engine
{
    class Scene;
}

namespace Physara::Editor
{
    enum class EditorCameraMode
    {
        Orbit,
        ViewportNavigate,
        PlayFly
    };

    struct EditorCameraInputFrame
    {
        float mouseDeltaX{0.f};
        float mouseDeltaY{0.f};
        bool hovered{false};
        bool focused{false};
        bool rightMouseDown{false};
        bool gravePressed{false};
        bool escapePressed{false};
        bool moveForward{false};
        bool moveBackward{false};
        bool moveLeft{false};
        bool moveRight{false};
        bool moveUp{false};
        bool moveDown{false};
        bool speedBoost{false};
        float mouseWheel{0.f};
    };

    struct EditorCameraSettings
    {
        float rotationSensitivity{0.12f};
        float flySpeedMetersPerSecond{15.f};
        float boostMultiplier{4.f};
        float wheelDollyMeters{1.f};
    };

    class EditorCamera final
    {
    public:
        EditorCamera();

        void Update(const EditorCameraInputFrame &input, float deltaTimeSeconds);
        void SetViewportSize(std::uint32_t width, std::uint32_t height);
        void SyncFromSceneCamera(Engine::Scene *scene);
        void SyncToSceneCamera(Engine::Scene *scene) const;

        [[nodiscard]] glm::mat4 GetViewMatrix() const;
        [[nodiscard]] glm::mat4 GetProjectionMatrix(const Engine::CameraComponent &camera) const;
        [[nodiscard]] Engine::RenderView BuildRenderView() const;
        [[nodiscard]] Engine::RenderView BuildRenderView(Engine::Scene *scene) const;

        [[nodiscard]] const glm::vec3 &GetPosition() const { return m_Position; }
        void SetPosition(const glm::vec3 &position) { m_Position = position; }

        [[nodiscard]] float GetYawDegrees() const { return m_YawDegrees; }
        [[nodiscard]] float GetPitchDegrees() const { return m_PitchDegrees; }
        void SetYawPitchDegrees(float yawDegrees, float pitchDegrees);

        [[nodiscard]] glm::vec3 GetForward() const;
        [[nodiscard]] glm::vec3 GetRight() const;
        [[nodiscard]] glm::vec3 GetUp() const;

        [[nodiscard]] float GetAspectRatio() const;
        [[nodiscard]] float GetEV100() const;
        [[nodiscard]] Engine::CameraComponent ToCameraComponent() const;

        [[nodiscard]] EditorCameraMode GetMode() const { return m_Mode; }
        [[nodiscard]] bool IsPlayFlyModeActive() const { return m_PlayFlyMode; }
        [[nodiscard]] bool WantsLockedCursor() const { return m_Mode == EditorCameraMode::ViewportNavigate || m_Mode == EditorCameraMode::PlayFly; }

        EditorCameraSettings &GetSettings() { return m_Settings; }
        [[nodiscard]] const EditorCameraSettings &GetSettings() const { return m_Settings; }

    private:
        void Rotate(float mouseDeltaX, float mouseDeltaY);
        void Dolly(float wheelDelta);
        void Fly(const EditorCameraInputFrame &input, float deltaTimeSeconds);
        void UpdateMode(const EditorCameraInputFrame &input);

    private:
        glm::vec3 m_Position{0.f, 0.f, 7.f};
        float m_YawDegrees{-90.f};
        float m_PitchDegrees{-12.f};
        std::uint32_t m_ViewportWidth{1};
        std::uint32_t m_ViewportHeight{1};
        bool m_PlayFlyMode{false};
        EditorCameraMode m_Mode{EditorCameraMode::Orbit};
        EditorCameraSettings m_Settings{};
    };
}