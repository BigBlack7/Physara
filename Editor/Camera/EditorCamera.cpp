#include "EditorCamera.hpp"

#include <algorithm>
#include <cmath>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>

#include <Engine/Scene/Scene.hpp>
#include <Engine/Scene/Components/TransformComponent.hpp>

namespace Physara::Editor
{
    namespace Internal
    {
        constexpr glm::vec3 WorldUp{0.f, 1.f, 0.f};

        float SafePositive(float value, float fallback)
        {
            return value > 0.f ? value : fallback;
        }
    }

    EditorCamera::EditorCamera() = default;

    void EditorCamera::Update(const EditorCameraInputFrame &input, float deltaTimeSeconds)
    {
        UpdateMode(input);

        if (!input.hovered && !input.focused && m_Mode != EditorCameraMode::Fly)
        {
            return;
        }

        if (input.rightMouseDown)
        {
            Rotate(input.mouseDeltaX, input.mouseDeltaY);
        }

        if (m_Mode == EditorCameraMode::Fly)
        {
            Fly(input, deltaTimeSeconds);
        }
        else if (input.hovered && input.mouseWheel != 0.f)
        {
            Dolly(input.mouseWheel);
        }
    }

    void EditorCamera::SetViewportSize(std::uint32_t width, std::uint32_t height)
    {
        m_ViewportWidth = std::max(width, 1u);
        m_ViewportHeight = std::max(height, 1u);
    }

    glm::mat4 EditorCamera::GetViewMatrix() const
    {
        return glm::lookAt(m_Position, m_Position + GetForward(), GetUp());
    }

    glm::mat4 EditorCamera::GetProjectionMatrix() const
    {
        Engine::CameraComponent camera = ToCameraComponent();
        return camera.GetProjectionMatrix(GetAspectRatio());
    }

    Engine::RenderView EditorCamera::BuildRenderView() const
    {
        Engine::CameraComponent camera = ToCameraComponent();
        return Engine::RenderView::FromMatrices(
            GetViewMatrix(),
            camera.GetProjectionMatrix(GetAspectRatio()),
            m_Position,
            Engine::ViewportRect{0, 0, m_ViewportWidth, m_ViewportHeight},
            camera.GetEV100(),
            camera.nearClipMeters,
            camera.farClipMeters);
    }

    Engine::RenderView EditorCamera::BuildCaptureView(Engine::Scene *scene,
                                                      Engine::EntityId selectedEntity,
                                                      CaptureViewSource source) const
    {
        (void)selectedEntity;

        if (source == CaptureViewSource::SceneCamera && scene != nullptr)
        {
            Engine::Entity entity = scene->GetSceneCameraEntity();
            if (entity.HasComponent<Engine::CameraComponent>() && entity.HasComponent<Engine::TransformComponent>())
            {
                Engine::CameraComponent camera = entity.GetComponent<Engine::CameraComponent>();
                camera.Sanitize();

                const Engine::TransformComponent &transform = entity.GetComponent<Engine::TransformComponent>();
                const glm::mat4 &world = transform.GetWorldMatrix();
                const glm::vec3 position = glm::vec3(world[3]);
                const glm::vec3 forward = glm::normalize(glm::vec3(world * glm::vec4(0.f, 0.f, -1.f, 0.f)));
                const glm::vec3 up = glm::normalize(glm::vec3(world * glm::vec4(0.f, 1.f, 0.f, 0.f)));
                const glm::mat4 view = glm::lookAt(position, position + forward, up);

                return Engine::RenderView::FromMatrices(
                    view,
                    camera.GetProjectionMatrix(GetAspectRatio()),
                    position,
                    Engine::ViewportRect{0, 0, m_ViewportWidth, m_ViewportHeight},
                    camera.GetEV100(),
                    camera.nearClipMeters,
                    camera.farClipMeters);
            }
        }

        return BuildRenderView();
    }

    void EditorCamera::SetYawPitchDegrees(float yawDegrees, float pitchDegrees)
    {
        m_YawDegrees = yawDegrees;
        m_PitchDegrees = std::clamp(pitchDegrees, -89.f, 89.f);
    }

    glm::vec3 EditorCamera::GetForward() const
    {
        const float yaw = glm::radians(m_YawDegrees);
        const float pitch = glm::radians(m_PitchDegrees);
        glm::vec3 forward{};
        forward.x = std::cos(pitch) * std::cos(yaw);
        forward.y = std::sin(pitch);
        forward.z = std::cos(pitch) * std::sin(yaw);
        return glm::normalize(forward);
    }

    glm::vec3 EditorCamera::GetRight() const
    {
        return glm::normalize(glm::cross(GetForward(), Internal::WorldUp));
    }

    glm::vec3 EditorCamera::GetUp() const
    {
        return glm::normalize(glm::cross(GetRight(), GetForward()));
    }

    float EditorCamera::GetAspectRatio() const
    {
        return static_cast<float>(std::max(m_ViewportWidth, 1u)) /
               static_cast<float>(std::max(m_ViewportHeight, 1u));
    }

    float EditorCamera::GetEV100() const
    {
        return ToCameraComponent().GetEV100();
    }

    Engine::CameraComponent EditorCamera::ToCameraComponent() const
    {
        Engine::CameraComponent camera{};
        camera.sensorWidthMillimeters = Internal::SafePositive(m_Settings.sensorWidthMillimeters, 36.f);
        camera.sensorHeightMillimeters = Internal::SafePositive(m_Settings.sensorHeightMillimeters, 24.f);
        camera.focalLengthMillimeters = Internal::SafePositive(m_Settings.focalLengthMillimeters, 35.f);
        camera.apertureFStop = Internal::SafePositive(m_Settings.apertureFStop, 2.8f);
        camera.shutterTimeSeconds = Internal::SafePositive(m_Settings.shutterTimeSeconds, 1.f / 60.f);
        camera.iso = Internal::SafePositive(m_Settings.iso, 100.f);
        camera.nearClipMeters = Internal::SafePositive(m_Settings.nearClipMeters, 0.1f);
        camera.farClipMeters = std::max(m_Settings.farClipMeters, camera.nearClipMeters + 0.001f);
        camera.Sanitize();
        return camera;
    }

    void EditorCamera::Rotate(float mouseDeltaX, float mouseDeltaY)
    {
        m_YawDegrees += mouseDeltaX * m_Settings.rotationSensitivity;
        m_PitchDegrees = std::clamp(m_PitchDegrees - mouseDeltaY * m_Settings.rotationSensitivity, -89.f, 89.f);
    }

    void EditorCamera::Dolly(float wheelDelta)
    {
        m_Position += GetForward() * (wheelDelta * std::max(m_Settings.wheelDollyMeters, 0.01f));
    }

    void EditorCamera::Fly(const EditorCameraInputFrame &input, float deltaTimeSeconds)
    {
        glm::vec3 direction{0.f};
        if (input.moveForward)
        {
            direction += GetForward();
        }
        if (input.moveBackward)
        {
            direction -= GetForward();
        }
        if (input.moveRight)
        {
            direction += GetRight();
        }
        if (input.moveLeft)
        {
            direction -= GetRight();
        }
        if (input.moveUp)
        {
            direction += Internal::WorldUp;
        }
        if (input.moveDown)
        {
            direction -= Internal::WorldUp;
        }

        if (glm::dot(direction, direction) <= 0.f)
        {
            return;
        }

        const float boost = input.speedBoost ? std::max(m_Settings.boostMultiplier, 1.f) : 1.f;
        const float speed = std::max(m_Settings.flySpeedMetersPerSecond, 0.f) * boost;
        m_Position += glm::normalize(direction) * speed * std::max(deltaTimeSeconds, 0.f);
    }

    void EditorCamera::UpdateMode(const EditorCameraInputFrame &input)
    {
        if (input.escapePressed)
        {
            m_ToggleFlyMode = false;
        }
        else if (input.gravePressed)
        {
            m_ToggleFlyMode = !m_ToggleFlyMode;
        }

        m_Mode = (m_ToggleFlyMode || input.rightMouseDown) ? EditorCameraMode::Fly : EditorCameraMode::Orbit;
    }
}