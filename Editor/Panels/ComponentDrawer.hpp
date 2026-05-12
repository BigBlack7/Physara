#pragma once

#include <array>
#include <cstdio>

#include <glm/trigonometric.hpp>
#include <imgui/imgui.h>

#include <Engine/Scene/Components/TagComponent.hpp>
#include <Engine/Scene/Components/CameraComponent.hpp>
#include <Engine/Scene/Components/TransformComponent.hpp>
#include <Engine/Scene/Entity.hpp>

namespace Physara::Editor
{
    template <typename T>
    bool DrawComponent(Engine::Entity entity)
    {
        (void)entity;
        return false;
    }

    template <>
    inline bool DrawComponent<Engine::TagComponent>(Engine::Entity entity)
    {
        auto &tag = entity.GetComponent<Engine::TagComponent>();

        std::array<char, 128> buffer{};
        std::snprintf(buffer.data(), buffer.size(), "%s", tag.name.c_str());

        if (ImGui::InputText("Name", buffer.data(), buffer.size()))
        {
            tag.name = buffer.data();
            return true;
        }

        return false;
    }

    template <>
    inline bool DrawComponent<Engine::TransformComponent>(Engine::Entity entity)
    {
        auto &transform = entity.GetComponent<Engine::TransformComponent>();
        bool changed = false;

        glm::vec3 position = transform.localPosition;
        if (ImGui::DragFloat3("Position", &position.x, 0.05f))
        {
            transform.SetLocalPosition(position);
            changed = true;
        }

        glm::vec3 rotationDegrees = glm::degrees(transform.GetLocalEulerRotation());
        if (ImGui::DragFloat3("Rotation", &rotationDegrees.x, 0.25f))
        {
            transform.SetLocalEulerRotation(glm::radians(rotationDegrees));
            changed = true;
        }

        glm::vec3 scale = transform.localScale;
        if (ImGui::DragFloat3("Scale", &scale.x, 0.02f, 0.001f, 0.f))
        {
            transform.SetLocalScale(scale);
            changed = true;
        }

        return changed;
    }

    template <>
    inline bool DrawComponent<Engine::CameraComponent>(Engine::Entity entity)
    {
        auto &camera = entity.GetComponent<Engine::CameraComponent>();
        bool changed = false;

        int projectionIndex = static_cast<int>(camera.projectionType);
        const char *projectionLabels[] = {"Perspective", "Orthographic"};
        if (ImGui::Combo("Projection", &projectionIndex, projectionLabels, IM_ARRAYSIZE(projectionLabels)))
        {
            camera.projectionType = static_cast<Engine::CameraProjectionType>(projectionIndex);
            changed = true;
        }

        changed |= ImGui::Checkbox("Primary", &camera.primary);
        changed |= ImGui::DragFloat("Sensor Width", &camera.sensorWidthMillimeters, 0.1f, 1.f, 100.f, "%.1f mm");
        changed |= ImGui::DragFloat("Sensor Height", &camera.sensorHeightMillimeters, 0.1f, 1.f, 100.f, "%.1f mm");
        changed |= ImGui::DragFloat("Focal Length", &camera.focalLengthMillimeters, 0.25f, 1.f, 300.f, "%.1f mm");
        changed |= ImGui::DragFloat("Aperture", &camera.apertureFStop, 0.05f, 0.1f, 64.f, "f/%.1f");
        changed |= ImGui::DragFloat("Shutter", &camera.shutterTimeSeconds, 0.0005f, 1.f / 32000.f, 30.f, "%.4f s");
        changed |= ImGui::DragFloat("ISO", &camera.iso, 1.f, 1.f, 25600.f, "%.0f");
        changed |= ImGui::DragFloat("Near Clip", &camera.nearClipMeters, 0.01f, 0.001f, 100.f, "%.3f m");
        changed |= ImGui::DragFloat("Far Clip", &camera.farClipMeters, 1.f, 0.01f, 100000.f, "%.1f m");

        if (camera.projectionType == Engine::CameraProjectionType::Orthographic)
        {
            changed |= ImGui::DragFloat("Ortho Height", &camera.orthographicHeightMeters, 0.1f, 0.001f, 10000.f, "%.2f m");
        }

        if (changed)
        {
            camera.Sanitize();
        }

        ImGui::Text("EV100: %.2f", camera.GetEV100());
        return changed;
    }

    template <typename T>
    bool TryDrawComponent(Engine::Entity entity, const char *label)
    {
        if (!entity.HasComponent<T>())
        {
            return false;
        }

        bool changed = false;
        if (ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::PushID(label);
            changed = DrawComponent<T>(entity);
            ImGui::PopID();
        }

        return changed;
    }
}