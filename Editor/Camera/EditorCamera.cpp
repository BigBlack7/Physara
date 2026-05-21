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
    namespace EditorCameraDetail
    {
        constexpr glm::vec3 WorldUp{0.f, 1.f, 0.f};
    }

    EditorCamera::EditorCamera() = default;

    void EditorCamera::Update(const EditorCameraInputFrame &input, float deltaTimeSeconds)
    {
        UpdateMode(input);

        if (!input.hovered && !input.focused && m_Mode != EditorCameraMode::PlayFly)
        {
            return;
        }

        if (m_Mode == EditorCameraMode::ViewportNavigate || m_Mode == EditorCameraMode::PlayFly)
        {
            Rotate(input.mouseDeltaX, input.mouseDeltaY);
        }

        if (m_Mode == EditorCameraMode::ViewportNavigate || m_Mode == EditorCameraMode::PlayFly)
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

    void EditorCamera::SyncFromSceneCamera(Engine::Scene *scene)
    {
        if (scene == nullptr)
        {
            return;
        }

        Engine::Entity entity = scene->GetSceneCameraEntity();
        if (!entity.HasComponent<Engine::TransformComponent>())
        {
            return;
        }

        const Engine::TransformComponent &transform = entity.GetComponent<Engine::TransformComponent>();
        m_Position = transform.localPosition;

        glm::vec3 forward = transform.localRotationQuat * glm::vec3(0.f, 0.f, -1.f);
        if (glm::dot(forward, forward) <= 0.f)
        {
            return;
        }

        forward = glm::normalize(forward);
        m_YawDegrees = glm::degrees(std::atan2(forward.z, forward.x));
        m_PitchDegrees = glm::degrees(std::asin(std::clamp(forward.y, -1.f, 1.f)));
    }

    void EditorCamera::SyncToSceneCamera(Engine::Scene *scene) const
    {
        if (scene == nullptr)
        {
            return;
        }

        Engine::Entity entity = scene->GetSceneCameraEntity();
        if (!entity.HasComponent<Engine::TransformComponent>())
        {
            return;
        }

        auto &transform = entity.GetComponent<Engine::TransformComponent>();
        const glm::quat rotation = glm::quatLookAtRH(GetForward(), EditorCameraDetail::WorldUp);
        transform.SetLocalTRS(m_Position, rotation, transform.localScale);

        if (entity.HasComponent<Engine::CameraComponent>())
        {
            auto &camera = entity.GetComponent<Engine::CameraComponent>();
            camera.primary = true;
            camera.Sanitize();
        }
    }

    glm::mat4 EditorCamera::GetViewMatrix() const
    {
        return glm::lookAt(m_Position, m_Position + GetForward(), GetUp());
    }

    glm::mat4 EditorCamera::GetProjectionMatrix(const Engine::CameraComponent &camera) const
    {
        Engine::CameraComponent sanitized = camera;
        sanitized.Sanitize();
        return sanitized.GetProjectionMatrix(GetAspectRatio());
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

    Engine::RenderView EditorCamera::BuildRenderView(Engine::Scene *scene) const
    {
        if (scene != nullptr)
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
        return glm::normalize(glm::cross(GetForward(), EditorCameraDetail::WorldUp));
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
            direction += EditorCameraDetail::WorldUp;
        }
        if (input.moveDown)
        {
            direction -= EditorCameraDetail::WorldUp;
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
            m_PlayFlyMode = false;
        }
        else if (input.gravePressed && (input.hovered || input.focused || m_PlayFlyMode))
        {
            m_PlayFlyMode = !m_PlayFlyMode;
        }

        if (m_PlayFlyMode)
        {
            m_Mode = EditorCameraMode::PlayFly;
        }
        else if (input.rightMouseDown && (input.hovered || input.focused))
        {
            m_Mode = EditorCameraMode::ViewportNavigate;
        }
        else
        {
            m_Mode = EditorCameraMode::Orbit;
        }
    }
}