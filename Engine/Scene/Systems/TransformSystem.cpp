#include "TransformSystem.hpp"

#include <Engine/Scene/Components/RelationshipComponent.hpp>
#include <Engine/Scene/Components/TransformComponent.hpp>
#include <Engine/Scene/Scene.hpp>

namespace Physara::Engine
{
    void TransformSystem::Update(Scene &scene)
    {
        auto &registry = scene.GetRegistry();
        // 取出所有拥有RelationshipComponent组件的entity实体集合视图
        auto view = registry.view<RelationshipComponent>();

        for (EntityId entity : view)
        {
            const auto &relationship = view.get<RelationshipComponent>(entity);
            if (relationship.IsRoot())
            {
                UpdateRecursive(scene, entity, glm::mat4(1.f), false);
            }
        }
    }

    void TransformSystem::UpdateRecursive(Scene &scene, EntityId entity, const glm::mat4 &parentWorldMatrix, bool parentDirty)
    {
        auto &registry = scene.GetRegistry();
        glm::mat4 currentWorldMatrix = parentWorldMatrix;
        bool currentDirty = parentDirty;

        // 更新当前实体自己的Transform
        if (auto *transform = registry.try_get<TransformComponent>(entity))
        {
            currentDirty = parentDirty || transform->localDirty || transform->worldDirty;
            if (currentDirty)
            {
                transform->RecalculateWorldMatrix(parentWorldMatrix);
            }
            currentWorldMatrix = transform->worldMatrix;
        }

        const auto *relationship = registry.try_get<RelationshipComponent>(entity);
        if (relationship == nullptr)
        {
            return;
        }

        for (EntityId child = relationship->firstChild; child != NullEntity;)
        {
            const auto *childRelationship = registry.try_get<RelationshipComponent>(child);
            const EntityId next = childRelationship != nullptr ? childRelationship->nextSibling : NullEntity;
            UpdateRecursive(scene, child, currentWorldMatrix, currentDirty);
            child = next;
        }
    }
}