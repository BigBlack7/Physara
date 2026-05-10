#include "TransformComponent.hpp"

#include <glm/ext/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

namespace Physara::Engine
{
    TransformComponent::TransformComponent(const glm::vec3 &position)
        : localPosition(position)
    {
    }

    TransformComponent::TransformComponent(const glm::vec3 &position, const glm::vec3 &rotation, const glm::vec3 &scale)
        : localPosition(position), localRotation(rotation), localScale(scale)
    {
    }

    void TransformComponent::SetLocalPosition(const glm::vec3 &position)
    {
        localPosition = position;
        MarkLocalDirty();
    }

    void TransformComponent::SetLocalRotation(const glm::vec3 &rotation)
    {
        localRotation = rotation;
        MarkLocalDirty();
    }

    void TransformComponent::SetLocalScale(const glm::vec3 &scale)
    {
        localScale = scale;
        MarkLocalDirty();
    }

    void TransformComponent::SetLocalTRS(const glm::vec3 &position, const glm::vec3 &rotation, const glm::vec3 &scale)
    {
        localPosition = position;
        localRotation = rotation;
        localScale = scale;
        MarkLocalDirty();
    }

    const glm::mat4 &TransformComponent::GetLocalMatrix()
    {
        if (localDirty)
        {
            RecalculateLocalMatrix();
        }

        return localMatrix;
    }

    void TransformComponent::MarkLocalDirty()
    {
        localDirty = true;
        MarkWorldDirty();
    }

    void TransformComponent::MarkWorldDirty()
    {
        worldDirty = true;
    }

    void TransformComponent::RecalculateLocalMatrix()
    {
        const glm::mat4 translation = glm::translate(glm::mat4(1.f), localPosition);
        const glm::mat4 rotation = glm::eulerAngleXYZ(localRotation.x, localRotation.y, localRotation.z);
        const glm::mat4 scale = glm::scale(glm::mat4(1.f), localScale);

        localMatrix = translation * rotation * scale;
        localDirty = false;
        worldDirty = true;
    }

    void TransformComponent::RecalculateWorldMatrix(const glm::mat4 &parentWorldMatrix)
    {
        worldMatrix = parentWorldMatrix * GetLocalMatrix();
        worldDirty = false;
    }
}