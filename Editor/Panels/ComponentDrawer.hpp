#pragma once

#include <array>
#include <cstdio>

#include <glm/trigonometric.hpp>
#include <imgui/imgui.h>

#include <Engine/Scene/Components/TagComponent.hpp>
#include <Engine/Scene/Components/CameraComponent.hpp>
#include <Engine/Scene/Components/MaterialComponent.hpp>
#include <Engine/Scene/Components/MeshComponent.hpp>
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

    template <>
    inline bool DrawComponent<Engine::MeshComponent>(Engine::Entity entity)
    {
        auto &mesh = entity.GetComponent<Engine::MeshComponent>();
        bool changed = false;

        ImGui::TextWrapped("Asset: %s", mesh.primitive.assetPath.c_str());
        ImGui::Text("Mesh: %u", mesh.primitive.meshIndex);
        ImGui::Text("Primitive: %u", mesh.primitive.primitiveIndex);
        changed |= ImGui::Checkbox("Visible", &mesh.visible);
        changed |= ImGui::Checkbox("Receive Shadows", &mesh.receiveShadows);

        if (mesh.localBounds.valid)
        {
            ImGui::Text("Bounds Min: %.3f, %.3f, %.3f", mesh.localBounds.min.x, mesh.localBounds.min.y, mesh.localBounds.min.z);
            ImGui::Text("Bounds Max: %.3f, %.3f, %.3f", mesh.localBounds.max.x, mesh.localBounds.max.y, mesh.localBounds.max.z);
            ImGui::Text("Radius: %.3f", mesh.localBounds.radius);
        }

        if (!mesh.materialSlots.empty())
        {
            ImGui::SeparatorText("Material Slots");
            for (const Engine::MaterialSlotRef &slot : mesh.materialSlots)
            {
                ImGui::Text("Slot %u", slot.slotIndex);
                ImGui::SameLine();
                ImGui::TextWrapped("%s", slot.materialPath.empty() ? "<none>" : slot.materialPath.c_str());
            }
        }

        return changed;
    }

    template <>
    inline bool DrawComponent<Engine::MaterialComponent>(Engine::Entity entity)
    {
        auto &material = entity.GetComponent<Engine::MaterialComponent>();
        bool changed = false;

        ImGui::TextWrapped("Material: %s", material.materialPath.empty() ? "<embedded>" : material.materialPath.c_str());
        changed |= ImGui::ColorEdit4("Base Color", &material.baseColor.x);
        changed |= ImGui::SliderFloat("Metallic", &material.metallic, 0.f, 1.f);
        changed |= ImGui::SliderFloat("Roughness", &material.roughness, 0.045f, 1.f);
        changed |= ImGui::SliderFloat("AO", &material.ambientOcclusion, 0.f, 1.f);
        changed |= ImGui::ColorEdit3("Emissive", &material.emissiveColor.x);
        changed |= ImGui::DragFloat("Emissive Luminance", &material.emissiveLuminance, 1.f, 0.f, 100000.f, "%.1f cd/m2");
        changed |= ImGui::Checkbox("Double Sided", &material.doubleSided);
        changed |= ImGui::Checkbox("Cast Shadow", &material.castShadow);

        if (changed)
        {
            material.Sanitize();
        }

        if (material.baseColorTexture.IsBound() || material.normalTexture.IsBound() ||
            material.metallicRoughnessTexture.IsBound() || material.occlusionTexture.IsBound() ||
            material.emissiveTexture.IsBound())
        {
            ImGui::SeparatorText("Textures");
            if (material.baseColorTexture.IsBound())
            {
                ImGui::TextWrapped("Base Color: %s", material.baseColorTexture.path.c_str());
            }
            if (material.metallicRoughnessTexture.IsBound())
            {
                ImGui::TextWrapped("Metallic Roughness: %s", material.metallicRoughnessTexture.path.c_str());
            }
            if (material.normalTexture.IsBound())
            {
                ImGui::TextWrapped("Normal: %s", material.normalTexture.path.c_str());
            }
            if (material.occlusionTexture.IsBound())
            {
                ImGui::TextWrapped("Occlusion: %s", material.occlusionTexture.path.c_str());
            }
            if (material.emissiveTexture.IsBound())
            {
                ImGui::TextWrapped("Emissive: %s", material.emissiveTexture.path.c_str());
            }
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