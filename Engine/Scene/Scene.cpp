#include "Scene.hpp"

#include <vector>

#include <Engine/Scene/Components/CameraComponent.hpp>
#include <Engine/Scene/Components/RelationshipComponent.hpp>
#include <Engine/Scene/Components/TagComponent.hpp>
#include <Engine/Scene/Components/TransformComponent.hpp>
#include <Engine/Scene/Systems/TransformSystem.hpp>

namespace Physara::Engine
{
    Entity Scene::CreateEntity(std::string_view name)
    {
        const EntityId handle = m_Registry.create();
        m_Registry.emplace<TagComponent>(handle, name);
        m_Registry.emplace<TransformComponent>(handle);
        m_Registry.emplace<RelationshipComponent>(handle);
        return Entity(handle, &m_Registry);
    }

    Entity Scene::EnsureSceneCamera()
    {
        EntityId sceneCamera = NullEntity;
        std::vector<EntityId> extraCameras;

        auto cameraView = m_Registry.view<CameraComponent>();
        for (EntityId entity : cameraView)
        {
            const auto &camera = cameraView.get<CameraComponent>(entity);
            if (camera.primary)
            {
                sceneCamera = entity;
                break;
            }
            if (sceneCamera == NullEntity)
            {
                sceneCamera = entity;
            }
        }

        for (EntityId entity : cameraView)
        {
            if (entity != sceneCamera)
            {
                extraCameras.push_back(entity);
            }
        }

        for (EntityId entity : extraCameras)
        {
            m_Registry.remove<CameraComponent>(entity);
        }

        if (sceneCamera == NullEntity || !IsValid(sceneCamera))
        {
            Entity cameraEntity = CreateEntity("Scene Camera");
            sceneCamera = cameraEntity.GetHandle();
            cameraEntity.AddComponent<CameraComponent>(true);
        }

        auto &camera = m_Registry.get<CameraComponent>(sceneCamera);
        camera.primary = true;
        camera.Sanitize();

        if (auto *tag = m_Registry.try_get<TagComponent>(sceneCamera); tag != nullptr && tag->name.empty())
        {
            tag->name = "Scene Camera";
        }

        DetachFromParent(sceneCamera);

        std::vector<EntityId> cameraChildren;
        if (const auto *relationship = m_Registry.try_get<RelationshipComponent>(sceneCamera))
        {
            for (EntityId child = relationship->firstChild; child != NullEntity;)
            {
                const auto *childRelationship = m_Registry.try_get<RelationshipComponent>(child);
                const EntityId next = childRelationship != nullptr ? childRelationship->nextSibling : NullEntity;
                cameraChildren.push_back(child);
                child = next;
            }
        }

        for (EntityId child : cameraChildren)
        {
            ClearParent(child);
        }

        return Entity(sceneCamera, &m_Registry);
    }

    Entity Scene::GetSceneCameraEntity()
    {
        return EnsureSceneCamera();
    }

    EntityId Scene::GetSceneCameraEntityId() const
    {
        auto cameraView = m_Registry.view<const CameraComponent>();
        for (EntityId entity : cameraView)
        {
            return entity;
        }

        return NullEntity;
    }

    bool Scene::IsSceneCamera(EntityId entity) const
    {
        return entity != NullEntity && entity == GetSceneCameraEntityId();
    }

    void Scene::DestroyEntity(Entity entity)
    {
        DestroyEntity(entity.GetHandle());
    }

    void Scene::DestroyEntity(EntityId entity)
    {
        if (!IsValid(entity))
        {
            return;
        }

        if (IsSceneCamera(entity))
        {
            DetachFromParent(entity);
            MarkWorldTransformDirty(entity);
            return;
        }

        std::vector<EntityId> children;
        if (auto *relationship = m_Registry.try_get<RelationshipComponent>(entity))
        {
            for (EntityId child = relationship->firstChild; child != NullEntity;)
            {
                const auto *childRelationship = m_Registry.try_get<RelationshipComponent>(child);
                const EntityId next = childRelationship != nullptr ? childRelationship->nextSibling : NullEntity;
                children.push_back(child);
                child = next;
            }
        }

        for (EntityId child : children)
        {
            DestroyEntity(child);
        }

        DetachFromParent(entity);
        m_Registry.destroy(entity);
    }

    void Scene::Clear()
    {
        m_Registry.clear();
    }

    bool Scene::SetParent(Entity child, Entity parent)
    {
        return SetParent(child.GetHandle(), parent.GetHandle());
    }

    bool Scene::SetParent(EntityId child, EntityId parent)
    {
        if (!IsValid(child))
        {
            return false;
        }

        if (parent == NullEntity)
        {
            ClearParent(child);
            return true;
        }

        if (!IsValid(parent) || child == parent || IsSceneCamera(parent) || WouldCreateCycle(child, parent))
        {
            return false;
        }

        DetachFromParent(child);
        AttachToParent(child, parent);
        MarkWorldTransformDirty(child);
        return true;
    }

    void Scene::ClearParent(Entity child)
    {
        ClearParent(child.GetHandle());
    }

    void Scene::ClearParent(EntityId child)
    {
        if (!IsValid(child))
        {
            return;
        }

        DetachFromParent(child);
        MarkWorldTransformDirty(child);
    }

    bool Scene::IsValid(EntityId entity) const
    {
        return entity != NullEntity && m_Registry.valid(entity);
    }

    Entity Scene::GetEntity(EntityId entity)
    {
        return IsValid(entity) ? Entity(entity, &m_Registry) : Entity{};
    }

    std::vector<Entity> Scene::GetRootEntities()
    {
        std::vector<Entity> roots;
        auto view = m_Registry.view<RelationshipComponent>();
        for (EntityId entity : view)
        {
            const auto &relationship = view.get<RelationshipComponent>(entity);
            if (relationship.IsRoot())
            {
                roots.emplace_back(entity, &m_Registry);
            }
        }

        return roots;
    }

    void Scene::UpdateTransforms()
    {
        TransformSystem::Update(*this);
    }

    void Scene::DetachFromParent(EntityId child)
    {
        auto *relationship = m_Registry.try_get<RelationshipComponent>(child);
        if (relationship == nullptr || relationship->parent == NullEntity)
        {
            return;
        }

        if (auto *parentRelationship = m_Registry.try_get<RelationshipComponent>(relationship->parent))
        {
            if (parentRelationship->firstChild == child)
            {
                parentRelationship->firstChild = relationship->nextSibling;
            }
            if (parentRelationship->childCount > 0)
            {
                --parentRelationship->childCount;
            }
        }

        if (auto *previous = m_Registry.try_get<RelationshipComponent>(relationship->previousSibling))
        {
            previous->nextSibling = relationship->nextSibling;
        }
        if (auto *next = m_Registry.try_get<RelationshipComponent>(relationship->nextSibling))
        {
            next->previousSibling = relationship->previousSibling;
        }

        relationship->parent = NullEntity;
        relationship->previousSibling = NullEntity;
        relationship->nextSibling = NullEntity;
    }

    void Scene::AttachToParent(EntityId child, EntityId parent)
    {
        auto &childRelationship = m_Registry.get_or_emplace<RelationshipComponent>(child);
        auto &parentRelationship = m_Registry.get_or_emplace<RelationshipComponent>(parent);

        childRelationship.parent = parent;
        childRelationship.previousSibling = NullEntity;
        childRelationship.nextSibling = NullEntity;

        if (parentRelationship.firstChild == NullEntity)
        {
            parentRelationship.firstChild = child;
        }
        else
        {
            EntityId tail = parentRelationship.firstChild;
            while (auto *tailRelationship = m_Registry.try_get<RelationshipComponent>(tail))
            {
                if (tailRelationship->nextSibling == NullEntity)
                {
                    tailRelationship->nextSibling = child;
                    childRelationship.previousSibling = tail;
                    break;
                }
                tail = tailRelationship->nextSibling;
            }
        }

        ++parentRelationship.childCount;
    }

    bool Scene::WouldCreateCycle(EntityId child, EntityId parent) const
    {
        EntityId cursor = parent;
        while (cursor != NullEntity)
        {
            if (cursor == child)
            {
                return true;
            }

            const auto *relationship = m_Registry.try_get<RelationshipComponent>(cursor);
            cursor = relationship != nullptr ? relationship->parent : NullEntity;
        }

        return false;
    }

    void Scene::MarkWorldTransformDirty(EntityId entity)
    {
        if (auto *transform = m_Registry.try_get<TransformComponent>(entity))
        {
            transform->MarkWorldDirty();
        }
    }
}