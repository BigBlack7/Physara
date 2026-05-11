#include "HierarchyPanel.hpp"

#include <cstdint>
#include <cstdio>
#include <string>

#include <imgui/imgui.h>

#include <Engine/Scene/Components/RelationshipComponent.hpp>
#include <Engine/Scene/Components/TagComponent.hpp>
#include <Engine/Scene/Scene.hpp>

namespace Physara::Editor
{
    namespace Internal
    {
        constexpr const char *PanelName = "Hierarchy";
    }

    HierarchyPanel::HierarchyPanel(EditorContext &context) : m_Context(context) {}

    void HierarchyPanel::Draw()
    {
        ImGui::Begin(Internal::PanelName);

        if (m_Context.activeScene == nullptr)
        {
            ImGui::TextUnformatted("No active scene.");
            ImGui::End();
            return;
        }

        if (ImGui::Button("Create Entity"))
        {
            Engine::Entity entity = m_Context.activeScene->CreateEntity("Entity");
            m_Context.selectedEntity = entity.GetHandle();
        }

        ImGui::Separator();

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

            if (ImGui::MenuItem("Create Child"))
            {
                Engine::Entity child = m_Context.activeScene->CreateEntity("Entity");
                m_Context.activeScene->SetParent(child, entity);
                m_Context.selectedEntity = child.GetHandle();
            }
            if (ImGui::MenuItem("Rename"))
            {
                OpenRenamePopup(entity);
            }
            if (ImGui::MenuItem("Delete", "Delete"))
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
            if (ImGui::MenuItem("Create Entity"))
            {
                Engine::Entity entity = m_Context.activeScene->CreateEntity("Entity");
                m_Context.selectedEntity = entity.GetHandle();
            }

            ImGui::EndPopup();
        }
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