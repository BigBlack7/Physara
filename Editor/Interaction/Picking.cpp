#include "Picking.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <limits>
#include <memory>
#include <string>

#include <glm/common.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <Engine/Scene/Components/LightComponent.hpp>
#include <Engine/Scene/Components/MeshComponent.hpp>
#include <Engine/Scene/Components/TransformComponent.hpp>
#include <Engine/Resource/AssetManager.hpp>
#include <Engine/Resource/Types/Mesh.hpp>
#include <Engine/Scene/Scene.hpp>

namespace Physara::Editor
{
    namespace PickingDetail
    {
        constexpr float RayEpsilon = 0.000001f;
        constexpr float LightProxyRadius = 0.28f;

        bool RayIntersectsAABB(const glm::vec3 &origin, const glm::vec3 &direction, const glm::vec3 &minBounds, const glm::vec3 &maxBounds, float &outT)
        {
            float tMin = 0.f;
            float tMax = std::numeric_limits<float>::max();

            for (int axis = 0; axis < 3; ++axis)
            {
                if (std::abs(direction[axis]) < RayEpsilon)
                {
                    if (origin[axis] < minBounds[axis] || origin[axis] > maxBounds[axis])
                    {
                        return false;
                    }
                    continue;
                }

                const float invDirection = 1.f / direction[axis];
                float t0 = (minBounds[axis] - origin[axis]) * invDirection;
                float t1 = (maxBounds[axis] - origin[axis]) * invDirection;
                if (t0 > t1)
                {
                    std::swap(t0, t1);
                }

                tMin = std::max(tMin, t0);
                tMax = std::min(tMax, t1);
                if (tMin > tMax)
                {
                    return false;
                }
            }

            if (tMax <= RayEpsilon)
            {
                return false;
            }

            outT = tMin > RayEpsilon ? tMin : tMax;
            return true;
        }

        bool RayIntersectsSphere(const glm::vec3 &origin, const glm::vec3 &direction, const glm::vec3 &center, float radius, float &outT)
        {
            const glm::vec3 oc = origin - center;
            const float b = glm::dot(oc, direction);
            const float c = glm::dot(oc, oc) - radius * radius;
            const float h = b * b - c;
            if (h < 0.f)
            {
                return false;
            }

            const float root = std::sqrt(h);
            const float t = -b - root;
            outT = t > 0.f ? t : -b + root;
            return outT > 0.f;
        }

        bool RayIntersectsTriangle(const glm::vec3 &origin, const glm::vec3 &direction, const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c, float &outT)
        {
            const glm::vec3 edgeAB = b - a;
            const glm::vec3 edgeAC = c - a;
            const glm::vec3 p = glm::cross(direction, edgeAC);
            const float determinant = glm::dot(edgeAB, p);
            if (std::abs(determinant) < RayEpsilon)
            {
                return false;
            }

            const float invDeterminant = 1.f / determinant;
            const glm::vec3 t = origin - a;
            const float u = glm::dot(t, p) * invDeterminant;
            if (u < 0.f || u > 1.f)
            {
                return false;
            }

            const glm::vec3 q = glm::cross(t, edgeAB);
            const float v = glm::dot(direction, q) * invDeterminant;
            if (v < 0.f || u + v > 1.f)
            {
                return false;
            }

            outT = glm::dot(edgeAC, q) * invDeterminant;
            return outT > RayEpsilon;
        }

        void Expand(glm::vec3 &minBounds, glm::vec3 &maxBounds, const glm::vec3 &point)
        {
            minBounds = glm::min(minBounds, point);
            maxBounds = glm::max(maxBounds, point);
        }

        void BuildWorldAABB(const Engine::MeshBounds &bounds, const glm::mat4 &world, glm::vec3 &minBounds, glm::vec3 &maxBounds)
        {
            const glm::vec3 localMin = bounds.min;
            const glm::vec3 localMax = bounds.max;
            const glm::vec3 corners[8]{
                {localMin.x, localMin.y, localMin.z},
                {localMax.x, localMin.y, localMin.z},
                {localMin.x, localMax.y, localMin.z},
                {localMax.x, localMax.y, localMin.z},
                {localMin.x, localMin.y, localMax.z},
                {localMax.x, localMin.y, localMax.z},
                {localMin.x, localMax.y, localMax.z},
                {localMax.x, localMax.y, localMax.z}};

            minBounds = glm::vec3(world * glm::vec4(corners[0], 1.f));
            maxBounds = minBounds;
            for (const glm::vec3 &corner : corners)
            {
                Expand(minBounds, maxBounds, glm::vec3(world * glm::vec4(corner, 1.f)));
            }
        }

        void RemoveSelection(std::vector<Engine::EntityId> &selection, Engine::EntityId entity)
        {
            selection.erase(std::remove(selection.begin(), selection.end(), entity), selection.end());
        }

        std::string MeshResourcePath(const Engine::MeshComponent &mesh)
        {
            if (mesh.primitive.assetPath.find("#mesh/") != std::string::npos)
            {
                return mesh.primitive.assetPath;
            }

            return mesh.primitive.assetPath + "#mesh/" + std::to_string(mesh.primitive.meshIndex);
        }

        bool RayIntersectsMeshPrimitive(const Engine::MeshComponent &mesh, const glm::mat4 &world, const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection, Engine::AssetManager *assetManager, bool &testedGeometry, float &outT)
        {
            testedGeometry = false;
            if (assetManager == nullptr || !mesh.HasMesh())
            {
                return false;
            }

            const std::shared_ptr<Engine::Mesh> meshResource = assetManager->GetByPath<Engine::Mesh>(MeshResourcePath(mesh));
            if (meshResource == nullptr || mesh.primitive.primitiveIndex >= meshResource->primitives.size())
            {
                return false;
            }

            const Engine::MeshPrimitive &primitive = meshResource->primitives[mesh.primitive.primitiveIndex];
            if (!primitive.HasGeometry())
            {
                return false;
            }

            testedGeometry = true;
            bool hit = false;
            float closestT = std::numeric_limits<float>::max();
            for (std::size_t i = 0; i + 2u < primitive.indices.size(); i += 3u)
            {
                const std::uint32_t ia = primitive.indices[i];
                const std::uint32_t ib = primitive.indices[i + 1u];
                const std::uint32_t ic = primitive.indices[i + 2u];
                if (ia >= primitive.vertices.size() || ib >= primitive.vertices.size() || ic >= primitive.vertices.size())
                {
                    continue;
                }

                const glm::vec3 a = glm::vec3(world * glm::vec4(primitive.vertices[ia].position, 1.f));
                const glm::vec3 b = glm::vec3(world * glm::vec4(primitive.vertices[ib].position, 1.f));
                const glm::vec3 c = glm::vec3(world * glm::vec4(primitive.vertices[ic].position, 1.f));
                float t = 0.f;
                if (RayIntersectsTriangle(rayOrigin, rayDirection, a, b, c, t) && t < closestT)
                {
                    closestT = t;
                    hit = true;
                }
            }

            outT = closestT;
            return hit;
        }
    }

    Engine::EntityId Picking::Pick(const EditorContext &context, const PickingRequest &request, Engine::AssetManager *assetManager) const
    {
        if (context.activeScene == nullptr || context.sceneView.width <= 0.f || context.sceneView.height <= 0.f)
        {
            return Engine::NullEntity;
        }

        const float ndcX = request.viewportPosition.x / context.sceneView.width * 2.f - 1.f;
        const float ndcY = 1.f - request.viewportPosition.y / context.sceneView.height * 2.f;
        const glm::mat4 inverseViewProjection = glm::inverse(context.sceneView.lastRenderView.viewProjection);
        const glm::vec4 nearH = inverseViewProjection * glm::vec4(ndcX, ndcY, -1.f, 1.f);
        const glm::vec4 farH = inverseViewProjection * glm::vec4(ndcX, ndcY, 1.f, 1.f);
        const glm::vec3 nearPoint = glm::vec3(nearH) / nearH.w;
        const glm::vec3 farPoint = glm::vec3(farH) / farH.w;
        const glm::vec3 rayDirection = glm::normalize(farPoint - nearPoint);

        float lightClosestT = std::numeric_limits<float>::max();
        const Engine::EntityId light = PickLightProxy(context, nearPoint, rayDirection, lightClosestT);
        if (light != Engine::NullEntity)
        {
            return light;
        }

        float closestT = std::numeric_limits<float>::max();
        Engine::EntityId selected = PickMesh(context, nearPoint, rayDirection, closestT, assetManager);
        return selected;
    }

    void Picking::Select(EditorContext &context, Engine::EntityId entity, const PickingRequest &request) const
    {
        if (context.activeScene == nullptr)
        {
            context.selectedEntity = Engine::NullEntity;
            context.selectedEntities.clear();
            return;
        }

        if (entity == Engine::NullEntity || !context.activeScene->IsValid(entity))
        {
            if (!request.appendSelection && !request.toggleSelection)
            {
                context.selectedEntity = Engine::NullEntity;
                context.selectedEntities.clear();
            }
            return;
        }

        if (request.toggleSelection)
        {
            const bool alreadySelected = std::find(context.selectedEntities.begin(), context.selectedEntities.end(), entity) != context.selectedEntities.end();
            if (alreadySelected)
            {
                PickingDetail::RemoveSelection(context.selectedEntities, entity);
                context.selectedEntity = context.selectedEntities.empty() ? Engine::NullEntity : context.selectedEntities.back();
            }
            else
            {
                context.selectedEntities.push_back(entity);
                context.selectedEntity = entity;
            }
            return;
        }

        if (request.appendSelection)
        {
            if (std::find(context.selectedEntities.begin(), context.selectedEntities.end(), entity) == context.selectedEntities.end())
            {
                context.selectedEntities.push_back(entity);
            }
            context.selectedEntity = entity;
            return;
        }

        context.selectedEntity = entity;
        context.selectedEntities.clear();
        context.selectedEntities.push_back(entity);
    }

    Engine::EntityId Picking::PickMesh(const EditorContext &context, const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection, float &closestT, Engine::AssetManager *assetManager) const
    {
        Engine::EntityId result = Engine::NullEntity;
        auto &registry = context.activeScene->GetRegistry();
        auto view = registry.view<Engine::MeshComponent, Engine::TransformComponent>();
        view.each([&](Engine::EntityId entity, const Engine::MeshComponent &mesh, Engine::TransformComponent &transform)
        {
            if (!mesh.visible || !mesh.localBounds.valid)
            {
                return;
            }

            glm::vec3 minBounds{};
            glm::vec3 maxBounds{};
            const glm::mat4 &world = transform.GetWorldMatrix();
            PickingDetail::BuildWorldAABB(mesh.localBounds, world, minBounds, maxBounds);

            float aabbT = 0.f;
            if (!PickingDetail::RayIntersectsAABB(rayOrigin, rayDirection, minBounds, maxBounds, aabbT) || aabbT >= closestT)
            {
                return;
            }

            float t = 0.f;
            bool testedGeometry = false;
            const bool preciseHit = PickingDetail::RayIntersectsMeshPrimitive(mesh, world, rayOrigin, rayDirection, assetManager, testedGeometry, t);
            if (testedGeometry && !preciseHit)
            {
                return;
            }
            if (!testedGeometry)
            {
                t = aabbT;
            }

            if (t < closestT)
            {
                closestT = t;
                result = entity;
            }
        });
        return result;
    }

    Engine::EntityId Picking::PickLightProxy(const EditorContext &context, const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection, float &closestT) const
    {
        Engine::EntityId result = Engine::NullEntity;
        auto &registry = context.activeScene->GetRegistry();
        auto view = registry.view<Engine::LightComponent, Engine::TransformComponent>();
        view.each([&](Engine::EntityId entity, const Engine::LightComponent &light, Engine::TransformComponent &transform)
        {
            const glm::vec3 position = glm::vec3(transform.GetWorldMatrix()[3]);
            const float radius = light.type == Engine::LightType::Point || light.type == Engine::LightType::Spot
                                     ? std::max(PickingDetail::LightProxyRadius, std::min(light.rangeMeters * 0.08f, 1.25f))
                                     : PickingDetail::LightProxyRadius;
            float t = 0.f;
            if (PickingDetail::RayIntersectsSphere(rayOrigin, rayDirection, position, radius, t) && t < closestT)
            {
                closestT = t;
                result = entity;
            }
        });
        return result;
    }
}