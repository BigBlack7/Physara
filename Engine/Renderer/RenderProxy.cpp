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

        void HashCombine(std::uint64_t &seed, std::string_view value)
        {
            const std::uint64_t hash = HashString(value);
            seed ^= hash + 0x9e3779b97f4a7c15ull + (seed << 6u) + (seed >> 2u);
        }

        void HashCombine(std::uint64_t &seed, std::uint64_t value)
        {
            seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6u) + (seed >> 2u);
        }

        std::uint64_t HashMaterialSignature(const MaterialComponent &material)
        {
            std::uint64_t seed = HashString(material.materialPath);
            HashCombine(seed, material.baseColorTexture.path);
            HashCombine(seed, material.metallicRoughnessTexture.path);
            HashCombine(seed, material.normalTexture.path);
            HashCombine(seed, material.occlusionTexture.path);
            HashCombine(seed, material.emissiveTexture.path);
            seed ^= static_cast<std::uint64_t>(material.alphaMode) << 8u;
            seed ^= material.doubleSided ? 0x8000000000000000ull : 0ull;
            return seed;
        }

        std::uint64_t BuildMeshKey(const RenderMeshSubmission &submission)
        {
            std::uint64_t seed = HashString(submission.meshPath);
            HashCombine(seed, submission.meshIndex);
            return seed;
        }

        std::uint64_t BuildPrimitiveKey(const RenderMeshSubmission &submission)
        {
            std::uint64_t seed = BuildMeshKey(submission);
            HashCombine(seed, submission.primitiveIndex);
            return seed;
        }
    }

    void RenderDrawBuckets::Clear()
    {
        opaque.clear();
        unlit.clear();
        transparent.clear();
        shadowCasters.clear();
    }

    void RenderProxy::Build(Scene &scene, const RenderView &view, FrameData &frameData, AssetManager *assetManager)
    {
        Reset();
        frameData.objects.clear();
        frameData.lights = LightSystem::Collect(scene);
        CullAndBucket(RenderSystem::Collect(scene, assetManager), view, frameData);
        SortBuckets();
        RepackObjectsForSortedBuckets(frameData);
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
            ObjectData object = BuildObjectData(submission, bucket);
            object.materialIndex = objectIndex;
            frameData.objects.push_back(object);
            m_VisibleSubmissions.push_back(submission);

            RenderDrawItem item{};
            item.submission = submission;
            item.objectIndex = objectIndex;
            item.sortKey = BuildSortKey(submission);
            const glm::vec3 cameraToObject = submission.boundsCenter - view.position;
            item.cameraDistanceSq = glm::dot(cameraToObject, cameraToObject);
            item.meshKey = RenderProxyDetail::BuildMeshKey(submission);
            item.primitiveKey = RenderProxyDetail::BuildPrimitiveKey(submission);
            item.doubleSided = submission.material.doubleSided;

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

    void RenderProxy::RepackObjectsForSortedBuckets(FrameData &frameData)
    {
        std::vector<ObjectData> objects;
        objects.reserve(frameData.objects.size());

        auto repackBucket = [&objects](std::vector<RenderDrawItem> &bucket)
        {
            for (RenderDrawItem &item : bucket)
            {
                ObjectData object = RenderProxy::BuildObjectData(
                    item.submission,
                    item.submission.material.IsTransparent()
                        ? RenderBucket::Transparent
                        : (item.submission.material.shadingModel == ShadingModel::Unlit ? RenderBucket::Unlit : RenderBucket::Opaque));
                item.objectIndex = static_cast<std::uint32_t>(objects.size());
                object.materialIndex = item.objectIndex;
                objects.push_back(object);
            }
        };

        repackBucket(m_Buckets.opaque);
        repackBucket(m_Buckets.unlit);
        repackBucket(m_Buckets.transparent);
        frameData.objects = std::move(objects);
    }

    std::uint64_t RenderProxy::BuildSortKey(const RenderMeshSubmission &submission)
    {
        const std::uint64_t materialHash = RenderProxyDetail::HashMaterialSignature(submission.material) & 0xffffffffull;
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