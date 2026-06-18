#include "Gizmo.hpp"

#include <algorithm>

#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <imgui/ImGuizmo.h>

#include <Engine/Scene/Components/TransformComponent.hpp>
#include <Engine/Scene/Scene.hpp>

namespace Physara::Editor
{
    namespace GizmoDetail
    {
        ImGuizmo::OPERATION ToOperation(GizmoOperation operation)
        {
            switch (operation)
            {
            case GizmoOperation::Rotate:
                return ImGuizmo::ROTATE;
            case GizmoOperation::Scale:
                return ImGuizmo::SCALE;
            case GizmoOperation::Translate:
            default:
                return ImGuizmo::TRANSLATE;
            }
        }

        ImGuizmo::MODE ToMode(GizmoSpace space)
        {
            return space == GizmoSpace::Local ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
        }

    }

    void Gizmo::Draw(EditorContext &context, const ImVec2 &viewportOrigin, float width, float height)
    {
        m_UsingOrHovered = false;
        if (!CanDraw(context, width, height))
        {
            return;
        }

        Engine::Scene &scene = *context.activeScene;
        Engine::Entity entity = scene.GetEntity(context.selectedEntity);
        auto &transform = entity.GetComponent<Engine::TransformComponent>();

        glm::mat4 worldMatrix = transform.GetWorldMatrix();

        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
        ImGuizmo::SetRect(viewportOrigin.x, viewportOrigin.y, width, height);
        ImGuizmo::AllowAxisFlip(false);

        const ImGuizmo::OPERATION operation = GizmoDetail::ToOperation(context.settings.gizmoOperation);
        const ImGuizmo::MODE mode = GizmoDetail::ToMode(context.settings.gizmoSpace);
        const glm::mat4 &view = context.sceneView.lastRenderView.view;
        const glm::mat4 &projection = context.sceneView.lastRenderView.projection;

        const bool manipulated = ImGuizmo::Manipulate(
            glm::value_ptr(view),
            glm::value_ptr(projection),
            operation,
            mode,
            glm::value_ptr(worldMatrix));

        m_UsingOrHovered = ImGuizmo::IsUsing() || ImGuizmo::IsOver();
        if (manipulated)
        {
            scene.SetWorldMatrix(context.selectedEntity, worldMatrix);
        }
    }

    bool Gizmo::CanDraw(const EditorContext &context, float width, float height) const
    {
        if (context.activeScene == nullptr ||
            context.settings.gizmoOperation == GizmoOperation::None ||
            context.selectedEntity == Engine::NullEntity ||
            !context.activeScene->IsValid(context.selectedEntity) ||
            width <= 1.f || height <= 1.f)
        {
            return false;
        }

        return context.activeScene->GetEntity(context.selectedEntity).HasComponent<Engine::TransformComponent>();
    }
}