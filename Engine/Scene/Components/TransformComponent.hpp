#pragma once

#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

namespace Physara::Engine
{
    struct TransformComponent
    {
        glm::vec3 localPosition{0.f, 0.f, 0.f};
        glm::quat localRotationQuat{1.f, 0.f, 0.f, 0.f};
        glm::vec3 localScale{1.f, 1.f, 1.f};

        glm::mat4 localMatrix{1.f};
        glm::mat4 worldMatrix{1.f};
        glm::mat4 inverseTransposeWorldMatrix{1.f};
        bool localDirty{true};
        bool worldDirty{true};

        TransformComponent() = default;
        explicit TransformComponent(const glm::vec3 &position);
        TransformComponent(const glm::vec3 &position, const glm::vec3 &eulerRotation, const glm::vec3 &scale);

        void SetLocalPosition(const glm::vec3 &position);
        void SetLocalRotation(const glm::quat &rotation);
        void SetLocalRotation(const glm::vec3 &eulerRotation);
        void SetLocalEulerRotation(const glm::vec3 &eulerRotation);
        void SetLocalScale(const glm::vec3 &scale);
        void SetLocalTRS(const glm::vec3 &position, const glm::quat &rotation, const glm::vec3 &scale);
        void SetLocalTRS(const glm::vec3 &position, const glm::vec3 &eulerRotation, const glm::vec3 &scale);
        bool SetLocalMatrix(const glm::mat4 &matrix);

        [[nodiscard]] glm::vec3 GetLocalEulerRotation() const;
        [[nodiscard]] bool GetWorldTRS(glm::vec3 &position, glm::quat &rotation, glm::vec3 &scale) const;
        [[nodiscard]] const glm::mat4 &GetLocalMatrix();
        [[nodiscard]] const glm::mat4 &GetWorldMatrix() const { return worldMatrix; }
        [[nodiscard]] const glm::mat4 &GetInverseTransposeWorldMatrix() const { return inverseTransposeWorldMatrix; }

        void MarkLocalDirty();
        void MarkWorldDirty();
        void RecalculateLocalMatrix();
        void RecalculateWorldMatrix(const glm::mat4 &parentWorldMatrix);
    };
}