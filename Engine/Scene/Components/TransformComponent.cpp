#include "TransformComponent.hpp"

#include <cmath>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/matrix_decompose.hpp>

namespace Physara::Engine
{
    namespace TransformComponentDetail
    {
        bool IsFinite(const glm::vec3 &value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
        }

        bool DecomposeTRS(
            const glm::mat4 &matrix,
            glm::vec3 &position,
            glm::quat &rotation,
            glm::vec3 &scale)
        {
            glm::vec3 skew{};
            glm::vec4 perspective{};
            if (!glm::decompose(matrix, scale, rotation, position, skew, perspective) ||
                !IsFinite(position) || !IsFinite(scale) ||
                !std::isfinite(rotation.x) || !std::isfinite(rotation.y) ||
                !std::isfinite(rotation.z) || !std::isfinite(rotation.w))
            {
                return false;
            }

            rotation = glm::normalize(rotation);
            return true;
        }
    }

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

    bool TransformComponent::SetLocalMatrix(const glm::mat4 &matrix)
    {
        glm::vec3 position{};
        glm::quat rotation{};
        glm::vec3 scale{};
        if (!TransformComponentDetail::DecomposeTRS(matrix, position, rotation, scale))
        {
            return false;
        }

        SetLocalTRS(position, rotation, scale);
        return true;
    }

    glm::vec3 TransformComponent::GetLocalEulerRotation() const
    {
        return glm::eulerAngles(localRotationQuat);
    }

    bool TransformComponent::GetWorldTRS(glm::vec3 &position, glm::quat &rotation, glm::vec3 &scale) const
    {
        return TransformComponentDetail::DecomposeTRS(worldMatrix, position, rotation, scale);
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
        inverseTransposeWorldMatrix = glm::inverseTranspose(worldMatrix);
        worldDirty = false;
    }
}