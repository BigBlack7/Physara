#include "RenderProxy.hpp"

#include <algorithm>
#include <array>
#include <functional>

#include <glm/geometric.hpp>
#include <glm/vec4.hpp>

#include <Engine/Scene/Scene.hpp>
#include <Engine/Scene/Systems/LightSystem.hpp>

namespace Physara::Engine
{
    namespace RenderProxyDetail
    {
        struct Plane
        {
            glm::vec3 normal{0.f};
            float distance{0.f};
        };

        glm::vec4 GetRow(const glm::mat4 &matrix, std::uint32_t row)
        {
            return {matrix[0][row], matrix[1][row], matrix[2][row], matrix[3][row]};
        }

        Plane NormalizePlane(const glm::vec4 &plane)
        {
            const glm::vec3 normal(plane);
            const float length = glm::length(normal);
            if (length <= 0.f)
            {
                return {};
            }
            return Plane{normal / length, plane.w / length};
        }

        std::array<Plane, 6> BuildFrustumPlanes(const glm::mat4 &viewProjection)
        {
            const glm::vec4 row0 = GetRow(viewProjection, 0);
            const glm::vec4 row1 = GetRow(viewProjection, 1);
            const glm::vec4 row2 = GetRow(viewProjection, 2);
            const glm::vec4 row3 = GetRow(viewProjection, 3);

            return {
                NormalizePlane(row3 + row0),
                NormalizePlane(row3 - row0),
                NormalizePlane(row3 + row1),
                NormalizePlane(row3 - row1),
                NormalizePlane(row3 + row2),
                NormalizePlane(row3 - row2)};
        }

        bool SphereIntersectsFrustum(const glm::vec3 &center, float radius, const std::array<Plane, 6> &planes)
        {
            for (const Plane &plane : planes)
            {
                if (glm::dot(plane.normal, center) + plane.distance < -radius)
                {
                    return false;
                }
            }
            return true;
        }

        std::uint32_t EntityToObjectId(EntityId entity)
        {
            return static_cast<std::uint32_t>(entity);
        }

        std::uint64_t HashString(std::string_view value)
        {
            return static_cast<std::uint64_t>(std::hash<std::string_view>{}(value));
        }
    }

    void RenderDrawBuckets::Clear()
    {
        opaque.clear();
        unlit.clear();
        transparent.clear();
        shadowCasters.clear();
    }

    void RenderProxy::Build(Scene &scene, const RenderView &view, FrameData &frameData)
    {
        Reset();
        frameData.objects.clear();
        frameData.lights = LightSystem::Collect(scene);
        CullAndBucket(RenderSystem::Collect(scene), view, frameData);
        SortBuckets();
    }

    void RenderProxy::Reset()
    {
        m_Buckets.Clear();
        m_VisibleSubmissions.clear();
    }

    void RenderProxy::CullAndBucket(const std::vector<RenderMeshSubmission> &submissions, const RenderView &view, FrameData &frameData)
    {
        const auto frustumPlanes = RenderProxyDetail::BuildFrustumPlanes(view.viewProjection);

        frameData.objects.reserve(submissions.size());
        m_VisibleSubmissions.reserve(submissions.size());

        for (const RenderMeshSubmission &submission : submissions)
        {
            if (submission.hasBounds &&
                !RenderProxyDetail::SphereIntersectsFrustum(submission.boundsCenter, submission.boundsRadius, frustumPlanes))
            {
                continue;
            }

            const bool transparent = submission.material.IsTransparent();
            const bool unlit = submission.material.shadingModel == ShadingModel::Unlit;
            const RenderBucket bucket = transparent ? RenderBucket::Transparent : (unlit ? RenderBucket::Unlit : RenderBucket::Opaque);

            const std::uint32_t objectIndex = static_cast<std::uint32_t>(frameData.objects.size());
            frameData.objects.push_back(BuildObjectData(submission, bucket));
            m_VisibleSubmissions.push_back(submission);

            RenderDrawItem item{};
            item.submission = submission;
            item.objectIndex = objectIndex;
            item.sortKey = BuildSortKey(submission);
            const glm::vec3 cameraToObject = submission.boundsCenter - view.position;
            item.cameraDistanceSq = glm::dot(cameraToObject, cameraToObject);

            if (transparent)
            {
                m_Buckets.transparent.push_back(item);
            }
            else if (unlit)
            {
                m_Buckets.unlit.push_back(item);
            }
            else
            {
                m_Buckets.opaque.push_back(item);
            }

            if (submission.material.castShadow)
            {
                m_Buckets.shadowCasters.push_back(item);
            }
        }
    }

    void RenderProxy::SortBuckets()
    {
        const auto bySortKey = [](const RenderDrawItem &lhs, const RenderDrawItem &rhs)
        {
            if (lhs.sortKey == rhs.sortKey)
            {
                return lhs.objectIndex < rhs.objectIndex;
            }
            return lhs.sortKey < rhs.sortKey;
        };

        std::sort(m_Buckets.opaque.begin(), m_Buckets.opaque.end(), bySortKey);
        std::sort(m_Buckets.unlit.begin(), m_Buckets.unlit.end(), bySortKey);
        std::sort(m_Buckets.shadowCasters.begin(), m_Buckets.shadowCasters.end(), bySortKey);
        std::sort(m_Buckets.transparent.begin(), m_Buckets.transparent.end(), [](const RenderDrawItem &lhs, const RenderDrawItem &rhs)
                  {
                      return lhs.cameraDistanceSq > rhs.cameraDistanceSq;
                  });
    }

    std::uint64_t RenderProxy::BuildSortKey(const RenderMeshSubmission &submission)
    {
        const std::uint64_t materialHash = RenderProxyDetail::HashString(submission.materialPath) & 0xffffffffull;
        const std::uint64_t meshHash = RenderProxyDetail::HashString(submission.meshPath) & 0xffffull;
        const std::uint64_t primitive = static_cast<std::uint64_t>(submission.primitiveIndex & 0xffffu);
        return (materialHash << 32u) | (meshHash << 16u) | primitive;
    }

    ObjectData RenderProxy::BuildObjectData(const RenderMeshSubmission &submission, RenderBucket bucket)
    {
        ObjectData object{};
        object.model = submission.model;
        object.inverseTransposeModel = submission.inverseTransposeModel;
        object.boundsCenterRadius = glm::vec4(submission.boundsCenter, submission.boundsRadius);
        object.objectId = RenderProxyDetail::EntityToObjectId(submission.entity);
        object.meshIndex = submission.meshIndex;
        object.materialIndex = 0;

        std::uint32_t flags = ObjectFlags::None;
        if (submission.material.castShadow)
        {
            flags |= ObjectFlags::CastShadow;
        }
        if (submission.receiveShadows)
        {
            flags |= ObjectFlags::ReceiveShadow;
        }
        if (bucket == RenderBucket::Transparent)
        {
            flags |= ObjectFlags::Transparent;
        }
        if (bucket == RenderBucket::Unlit)
        {
            flags |= ObjectFlags::Unlit;
        }
        object.flags = flags;
        return object;
    }
}