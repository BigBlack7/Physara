#include "HierarchyPanel.hpp"

#include <cstdint>
#include <cstdio>
#include <string>

#include <glm/trigonometric.hpp>
#include <imgui/imgui.h>

#include <Engine/Scene/Components/LightComponent.hpp>
#include <Engine/Scene/Components/MaterialComponent.hpp>
#include <Engine/Scene/Components/MeshComponent.hpp>
#include <Engine/Scene/Components/RelationshipComponent.hpp>
#include <Engine/Scene/Components/TagComponent.hpp>
#include <Engine/Scene/Components/TransformComponent.hpp>
#include <Engine/Scene/Scene.hpp>

namespace Physara::Editor
{
    namespace HierarchyPanelDetail
    {
        constexpr const char *PanelName = "Hierarchy";
    }

    HierarchyPanel::HierarchyPanel(EditorContext &context) : m_Context(context) {}

    void HierarchyPanel::Draw()
    {
        ImGui::Begin(HierarchyPanelDetail::PanelName);

        if (m_Context.activeScene == nullptr)
        {
            ImGui::TextUnformatted("No active scene.");
            ImGui::End();
            return;
        }

        for (Engine::Entity root : m_Context.activeScene->GetRootEntities())
        {
            DrawEntityNode(root);
        }

        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered())
        {
            m_Context.selectedEntity = Engine::NullEntity;
        }

        DrawEmptySceneContextMenu();
        DrawRenamePopup();

        ImGui::End();
    }

    void HierarchyPanel::DrawEntityNode(Engine::Entity entity)
    {
        if (!entity)
        {
            return;
        }

        auto &tag = entity.GetComponent<Engine::TagComponent>();
        const auto &relationship = entity.GetComponent<Engine::RelationshipComponent>();
        const bool hasChildren = relationship.HasChildren();
        const bool isSceneCamera = m_Context.activeScene->IsSceneCamera(entity.GetHandle());

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                   ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                   ImGuiTreeNodeFlags_SpanAvailWidth;

        if (m_Context.selectedEntity == entity.GetHandle())
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        if (!hasChildren)
        {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }

        const bool open = ImGui::TreeNodeEx(reinterpret_cast<void *>(static_cast<std::uintptr_t>(entity.GetHandle())),
                                            flags, "%s", tag.name.c_str());

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        {
            m_Context.selectedEntity = entity.GetHandle();
        }

        if (ImGui::BeginPopupContextItem())
        {
            m_Context.selectedEntity = entity.GetHandle();

            if (ImGui::BeginMenu("Create Child", !isSceneCamera))
            {
                DrawCreateMenu(entity);
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Rename"))
            {
                OpenRenamePopup(entity);
            }
            if (ImGui::MenuItem("Delete", "Backspace", false, !isSceneCamera))
            {
                const bool shouldPopTree = open && hasChildren;
                m_Context.activeScene->DestroyEntity(entity);
                m_Context.selectedEntity = Engine::NullEntity;
                ImGui::EndPopup();
                if (shouldPopTree)
                {
                    ImGui::TreePop();
                }
                return;
            }

            ImGui::EndPopup();
        }

        if (open && hasChildren)
        {
            auto &registry = m_Context.activeScene->GetRegistry();
            for (Engine::EntityId child = relationship.firstChild; child != Engine::NullEntity;)
            {
                const auto *childRelationship = registry.try_get<Engine::RelationshipComponent>(child);
                const Engine::EntityId next =
                    childRelationship != nullptr ? childRelationship->nextSibling : Engine::NullEntity;
                DrawEntityNode(m_Context.activeScene->GetEntity(child));
                child = next;
            }

            ImGui::TreePop();
        }
    }

    void HierarchyPanel::DrawEmptySceneContextMenu()
    {
        if (ImGui::BeginPopupContextWindow("HierarchyContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            if (ImGui::BeginMenu("Create"))
            {
                DrawCreateMenu({});
                ImGui::EndMenu();
            }

            ImGui::EndPopup();
        }
    }

    void HierarchyPanel::DrawCreateMenu(Engine::Entity parent)
    {
        if (ImGui::MenuItem("Empty Entity"))
        {
            CreateEntity(CreateEntityKind::Empty, parent);
        }

        if (ImGui::BeginMenu("Light"))
        {
            if (ImGui::MenuItem("Directional Light"))
            {
                CreateEntity(CreateEntityKind::DirectionalLight, parent);
            }
            if (ImGui::MenuItem("Point Light"))
            {
                CreateEntity(CreateEntityKind::PointLight, parent);
            }
            if (ImGui::MenuItem("Spot Light"))
            {
                CreateEntity(CreateEntityKind::SpotLight, parent);
            }
            if (ImGui::MenuItem("Area Light"))
            {
                CreateEntity(CreateEntityKind::AreaLight, parent);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Primitive"))
        {
            if (ImGui::MenuItem("Cube"))
            {
                CreateEntity(CreateEntityKind::Cube, parent);
            }
            if (ImGui::MenuItem("Sphere"))
            {
                CreateEntity(CreateEntityKind::Sphere, parent);
            }
            if (ImGui::MenuItem("Plane"))
            {
                CreateEntity(CreateEntityKind::Plane, parent);
            }
            ImGui::EndMenu();
        }
    }

    Engine::Entity HierarchyPanel::CreateEntity(CreateEntityKind kind, Engine::Entity parent)
    {
        if (m_Context.activeScene == nullptr)
        {
            return {};
        }

        const char *name = "Entity";
        switch (kind)
        {
        case CreateEntityKind::Empty:
            name = "Empty Entity";
            break;
        case CreateEntityKind::DirectionalLight:
            name = "Directional Light";
            break;
        case CreateEntityKind::PointLight:
            name = "Point Light";
            break;
        case CreateEntityKind::SpotLight:
            name = "Spot Light";
            break;
        case CreateEntityKind::AreaLight:
            name = "Area Light";
            break;
        case CreateEntityKind::Cube:
            name = "Cube";
            break;
        case CreateEntityKind::Sphere:
            name = "Sphere";
            break;
        case CreateEntityKind::Plane:
            name = "Plane";
            break;
        }

        Engine::Entity entity = m_Context.activeScene->CreateEntity(name);
        if (parent)
        {
            m_Context.activeScene->SetParent(entity, parent);
        }

        auto addLight = [&entity](Engine::LightType type)
        {
            entity.AddComponent<Engine::LightComponent>(type);
            auto &transform = entity.GetComponent<Engine::TransformComponent>();
            if (type == Engine::LightType::Directional)
            {
                transform.SetLocalEulerRotation({glm::radians(-45.f), glm::radians(35.f), 0.f});
            }
            else if (type == Engine::LightType::Point)
            {
                auto &light = entity.GetComponent<Engine::LightComponent>();
                light.pointLuminousPowerLumens = 200000.f;
                light.rangeMeters = 20.f;
                transform.SetLocalPosition({0.f, 2.f, 3.f});
            }
            else if (type == Engine::LightType::Spot)
            {
                auto &light = entity.GetComponent<Engine::LightComponent>();
                light.spotLuminousIntensityCandela = 50000.f;
                light.rangeMeters = 25.f;
                light.innerConeAngleRadians = glm::radians(18.f);
                light.outerConeAngleRadians = glm::radians(32.f);
                transform.SetLocalPosition({0.f, 3.f, 4.f});
                transform.SetLocalEulerRotation({glm::radians(-37.f), 0.f, 0.f});
            }
        };

        auto addPrimitive = [&entity](const char *path, const Engine::MeshBounds &bounds)
        {
            auto &mesh = entity.AddComponent<Engine::MeshComponent>(path);
            mesh.localBounds = bounds;
            mesh.materialSlots.emplace_back(0u);
            entity.AddComponent<Engine::MaterialComponent>();
        };

        switch (kind)
        {
        case CreateEntityKind::DirectionalLight:
            addLight(Engine::LightType::Directional);
            break;
        case CreateEntityKind::PointLight:
            addLight(Engine::LightType::Point);
            break;
        case CreateEntityKind::SpotLight:
            addLight(Engine::LightType::Spot);
            break;
        case CreateEntityKind::AreaLight:
            addLight(Engine::LightType::Area);
            break;
        case CreateEntityKind::Cube:
            addPrimitive("Builtin/Primitives/Cube", {{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}, {0.f, 0.f, 0.f}, 0.866f, true});
            break;
        case CreateEntityKind::Sphere:
            addPrimitive("Builtin/Primitives/Sphere", {{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}, {0.f, 0.f, 0.f}, 0.5f, true});
            break;
        case CreateEntityKind::Plane:
            addPrimitive("Builtin/Primitives/Plane", {{-0.5f, 0.f, -0.5f}, {0.5f, 0.f, 0.5f}, {0.f, 0.f, 0.f}, 0.707f, true});
            break;
        case CreateEntityKind::Empty:
            break;
        }

        m_Context.selectedEntity = entity.GetHandle();
        return entity;
    }

    void HierarchyPanel::OpenRenamePopup(Engine::Entity entity)
    {
        if (!entity.HasComponent<Engine::TagComponent>())
        {
            return;
        }

        m_RenameTarget = entity.GetHandle();
        m_RenameBuffer.fill('\0');
        const std::string &name = entity.GetComponent<Engine::TagComponent>().name;
        std::snprintf(m_RenameBuffer.data(), m_RenameBuffer.size(), "%s", name.c_str());
        ImGui::OpenPopup("Rename Entity");
    }

    void HierarchyPanel::DrawRenamePopup()
    {
        if (ImGui::BeginPopupModal("Rename Entity", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::InputText("Name", m_RenameBuffer.data(), m_RenameBuffer.size());

            const bool confirm = ImGui::Button("Apply") || ImGui::IsKeyPressed(ImGuiKey_Enter);
            ImGui::SameLine();
            const bool cancel = ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape);

            if (confirm)
            {
                if (m_Context.activeScene != nullptr && m_Context.activeScene->IsValid(m_RenameTarget))
                {
                    Engine::Entity entity = m_Context.activeScene->GetEntity(m_RenameTarget);
                    entity.GetComponent<Engine::TagComponent>().name = m_RenameBuffer.data();
                }
                m_RenameTarget = Engine::NullEntity;
                ImGui::CloseCurrentPopup();
            }
            else if (cancel)
            {
                m_RenameTarget = Engine::NullEntity;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }
}