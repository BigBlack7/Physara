#include "TransformComponent.hpp"

#include <glm/ext/matrix_transform.hpp>

namespace Physara::Engine
{
    TransformComponent::TransformComponent(const glm::vec3 &position)
        : localPosition(position)
    {
    }

    TransformComponent::TransformComponent(const glm::vec3 &position, const glm::vec3 &eulerRotation, const glm::vec3 &scale)
        : localPosition(position), localRotationQuat(glm::quat(eulerRotation)), localScale(scale)
    {
    }

    void TransformComponent::SetLocalPosition(const glm::vec3 &position)
    {
        localPosition = position;
        MarkLocalDirty();
    }

    void TransformComponent::SetLocalRotation(const glm::quat &rotation)
    {
        localRotationQuat = glm::normalize(rotation);
        MarkLocalDirty();
    }

    void TransformComponent::SetLocalRotation(const glm::vec3 &eulerRotation)
    {
        SetLocalEulerRotation(eulerRotation);
    }

    void TransformComponent::SetLocalEulerRotation(const glm::vec3 &eulerRotation)
    {
        localRotationQuat = glm::quat(eulerRotation);
        MarkLocalDirty();
    }

    void TransformComponent::SetLocalScale(const glm::vec3 &scale)
    {
        localScale = scale;
        MarkLocalDirty();
    }

    void TransformComponent::SetLocalTRS(const glm::vec3 &position, const glm::quat &rotation, const glm::vec3 &scale)
    {
        localPosition = position;
        localRotationQuat = glm::normalize(rotation);
        localScale = scale;
        MarkLocalDirty();
    }

    void TransformComponent::SetLocalTRS(const glm::vec3 &position, const glm::vec3 &eulerRotation, const glm::vec3 &scale)
    {
        localPosition = position;
        localRotationQuat = glm::quat(eulerRotation);
        localScale = scale;
        MarkLocalDirty();
    }

    glm::vec3 TransformComponent::GetLocalEulerRotation() const
    {
        return glm::eulerAngles(localRotationQuat);
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
        const glm::mat4 rotation = glm::mat4_cast(localRotationQuat);
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