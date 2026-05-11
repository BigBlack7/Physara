#pragma once

#include <array>
#include <cstdio>

#include <glm/trigonometric.hpp>
#include <imgui/imgui.h>

#include <Engine/Scene/Components/TagComponent.hpp>
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