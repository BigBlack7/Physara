#include "RenderSystem.hpp"

#include <algorithm>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/vec4.hpp>

#include <Engine/Scene/Components/MeshComponent.hpp>
#include <Engine/Scene/Components/TransformComponent.hpp>
#include <Engine/Scene/Scene.hpp>

namespace Physara::Engine
{
    namespace RenderSystemDetail
    {
        float GetMaxScaleAxis(const glm::mat4 &matrix)
        {
            const float x = glm::length(glm::vec3(matrix[0]));
            const float y = glm::length(glm::vec3(matrix[1]));
            const float z = glm::length(glm::vec3(matrix[2]));
            return std::max(x, std::max(y, z));
        }

        MaterialComponent GetMaterial(const entt::registry &registry, EntityId entity, const MeshComponent &mesh)
        {
            MaterialComponent material{};
            if (const auto *component = registry.try_get<MaterialComponent>(entity))
            {
                material = *component;
            }

            if (!mesh.materialSlots.empty() && mesh.materialSlots.front().HasOverride())
            {
                material.materialPath = mesh.materialSlots.front().materialPath;
            }

            material.Sanitize();
            return material;
        }
    }

    std::vector<RenderMeshSubmission> RenderSystem::Collect(Scene &scene)
    {
        scene.UpdateTransforms();

        auto &registry = scene.GetRegistry();
        auto view = registry.view<MeshComponent, TransformComponent>();

        std::vector<RenderMeshSubmission> submissions;
        submissions.reserve(view.size_hint());

        view.each([&submissions, &registry](EntityId entity, const MeshComponent &mesh, const TransformComponent &transform)
        {
            if (!mesh.visible || !mesh.HasMesh())
            {
                return;
            }

            RenderMeshSubmission submission{};
            submission.entity = entity;
            submission.meshPath = mesh.primitive.assetPath;
            submission.meshIndex = mesh.primitive.meshIndex;
            submission.primitiveIndex = mesh.primitive.primitiveIndex;
            submission.material = RenderSystemDetail::GetMaterial(registry, entity, mesh);
            submission.materialPath = submission.material.materialPath;
            submission.model = transform.GetWorldMatrix();
            submission.inverseTransposeModel = glm::inverseTranspose(submission.model);
            submission.receiveShadows = mesh.receiveShadows;

            if (mesh.localBounds.valid)
            {
                submission.boundsCenter = glm::vec3(submission.model * glm::vec4(mesh.localBounds.center, 1.f));
                submission.boundsRadius = mesh.localBounds.radius * RenderSystemDetail::GetMaxScaleAxis(submission.model);
                submission.hasBounds = true;
            }

            submissions.push_back(std::move(submission));
        });

        return submissions;
    }
}