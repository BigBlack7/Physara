#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace Physara::Engine
{
    struct TransformComponent
    {
        glm::vec3 localPosition{0.f, 0.f, 0.f};
        glm::vec3 localRotation{0.f, 0.f, 0.f};
        glm::vec3 localScale{1.f, 1.f, 1.f};

        glm::mat4 localMatrix{1.f};
        glm::mat4 worldMatrix{1.f};
        bool localDirty{true};
        bool worldDirty{true};

        TransformComponent() = default;
        explicit TransformComponent(const glm::vec3 &position);
        TransformComponent(const glm::vec3 &position, const glm::vec3 &rotation, const glm::vec3 &scale);

        void SetLocalPosition(const glm::vec3 &position);
        void SetLocalRotation(const glm::vec3 &rotation);
        void SetLocalScale(const glm::vec3 &scale);
        void SetLocalTRS(const glm::vec3 &position, const glm::vec3 &rotation, const glm::vec3 &scale);

        [[nodiscard]] const glm::mat4 &GetLocalMatrix();
        [[nodiscard]] const glm::mat4 &GetWorldMatrix() const { return worldMatrix; }

        void MarkLocalDirty();
        void MarkWorldDirty();
        void RecalculateLocalMatrix();
        void RecalculateWorldMatrix(const glm::mat4 &parentWorldMatrix);
    };
}