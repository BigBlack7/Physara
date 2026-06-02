#include "RenderProxy.hpp"

#include <algorithm>
#include <array>
#include <functional>
#include <limits>
#include <unordered_map>

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

        void HashCombine(std::uint64_t &seed, float value)
        {
            HashCombine(seed, static_cast<std::uint64_t>(std::hash<float>{}(value)));
        }

        void HashCombine(std::uint64_t &seed, const glm::vec3 &value)
        {
            HashCombine(seed, value.x);
            HashCombine(seed, value.y);
            HashCombine(seed, value.z);
        }

        void HashCombine(std::uint64_t &seed, const glm::vec4 &value)
        {
            HashCombine(seed, value.x);
            HashCombine(seed, value.y);
            HashCombine(seed, value.z);
            HashCombine(seed, value.w);
        }

        void HashCombine(std::uint64_t &seed, const TextureSlot &slot)
        {
            HashCombine(seed, slot.path);
            HashCombine(seed, static_cast<std::uint64_t>(slot.texCoord));
        }

        std::uint64_t HashMaterialSignature(const MaterialComponent &material)
        {
            std::uint64_t seed = HashString(material.materialPath);
            HashCombine(seed, static_cast<std::uint64_t>(material.shadingModel));
            HashCombine(seed, static_cast<std::uint64_t>(material.alphaMode));
            HashCombine(seed, material.doubleSided ? 1ull : 0ull);
            HashCombine(seed, material.castShadow ? 1ull : 0ull);
            HashCombine(seed, material.baseColor);
            HashCombine(seed, material.metallic);
            HashCombine(seed, material.roughness);
            HashCombine(seed, material.reflectance);
            HashCombine(seed, material.ambientOcclusion);
            HashCombine(seed, material.alphaCutoff);
            HashCombine(seed, material.metallicTextureInfluence);
            HashCombine(seed, material.roughnessTextureInfluence);
            HashCombine(seed, material.ambientOcclusionTextureInfluence);
            HashCombine(seed, material.emissiveColor);
            HashCombine(seed, material.emissiveLuminance);
            HashCombine(seed, material.normalScale);
            HashCombine(seed, material.flipNormalY ? 1ull : 0ull);
            HashCombine(seed, material.baseColorTexture);
            HashCombine(seed, material.metallicRoughnessTexture);
            HashCombine(seed, material.normalTexture);
            HashCombine(seed, material.occlusionTexture);
            HashCombine(seed, material.emissiveTexture);
            return seed;
        }

        bool TextureSlotEquals(const TextureSlot &lhs, const TextureSlot &rhs)
        {
            return lhs.path == rhs.path && lhs.texCoord == rhs.texCoord;
        }

        bool MaterialEquals(const MaterialComponent &lhs, const MaterialComponent &rhs)
        {
            return lhs.materialPath == rhs.materialPath &&
                   lhs.shadingModel == rhs.shadingModel &&
                   lhs.alphaMode == rhs.alphaMode &&
                   lhs.doubleSided == rhs.doubleSided &&
                   lhs.castShadow == rhs.castShadow &&
                   lhs.baseColor == rhs.baseColor &&
                   lhs.metallic == rhs.metallic &&
                   lhs.roughness == rhs.roughness &&
                   lhs.reflectance == rhs.reflectance &&
                   lhs.ambientOcclusion == rhs.ambientOcclusion &&
                   lhs.alphaCutoff == rhs.alphaCutoff &&
                   lhs.metallicTextureInfluence == rhs.metallicTextureInfluence &&
                   lhs.roughnessTextureInfluence == rhs.roughnessTextureInfluence &&
                   lhs.ambientOcclusionTextureInfluence == rhs.ambientOcclusionTextureInfluence &&
                   lhs.emissiveColor == rhs.emissiveColor &&
                   lhs.emissiveLuminance == rhs.emissiveLuminance &&
                   lhs.normalScale == rhs.normalScale &&
                   lhs.flipNormalY == rhs.flipNormalY &&
                   TextureSlotEquals(lhs.baseColorTexture, rhs.baseColorTexture) &&
                   TextureSlotEquals(lhs.metallicRoughnessTexture, rhs.metallicRoughnessTexture) &&
                   TextureSlotEquals(lhs.normalTexture, rhs.normalTexture) &&
                   TextureSlotEquals(lhs.occlusionTexture, rhs.occlusionTexture) &&
                   TextureSlotEquals(lhs.emissiveTexture, rhs.emissiveTexture);
        }

        std::uint64_t BuildMeshKey(const RenderMeshSubmission &submission)
        {
            std::uint64_t seed = HashString(submission.meshPath);
            HashCombine(seed, static_cast<std::uint64_t>(submission.meshIndex));
            return seed;
        }

        std::uint64_t BuildPrimitiveKey(const RenderMeshSubmission &submission)
        {
            std::uint64_t seed = BuildMeshKey(submission);
            HashCombine(seed, static_cast<std::uint64_t>(submission.primitiveIndex));
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
        LightSystem::Collect(scene, frameData.lights, &view);
        frameData.stats.lightCount = static_cast<std::uint32_t>(frameData.lights.size());
        RenderSystem::Collect(scene, m_SubmissionScratch, assetManager);
        CullAndBucket(m_SubmissionScratch, view, frameData);
        SortBuckets();
        RepackObjectsForSortedBuckets(frameData);
        frameData.stats.visibleSubmissions = m_VisibleSubmissionCount;
        frameData.stats.opaqueItems = static_cast<std::uint32_t>(m_Buckets.opaque.size());
        frameData.stats.unlitItems = static_cast<std::uint32_t>(m_Buckets.unlit.size());
        frameData.stats.transparentItems = static_cast<std::uint32_t>(m_Buckets.transparent.size());
    }

    void RenderProxy::Reset()
    {
        m_Buckets.Clear();
        m_SubmissionScratch.clear();
        m_VisibleSubmissionCount = 0;
    }

    void RenderProxy::CullAndBucket(const std::vector<RenderMeshSubmission> &submissions, const RenderView &view, FrameData &frameData)
    {
        const auto frustumPlanes = RenderProxyDetail::BuildFrustumPlanes(view.viewProjection);

        for (const RenderMeshSubmission &submission : submissions)
        {
            if (submission.material.castShadow &&
                (submission.material.alphaMode == AlphaMode::Opaque || submission.material.alphaMode == AlphaMode::Mask))
            {
                RenderDrawItem item{};
                item.submission = &submission;
                item.sortKey = BuildSortKey(submission);
                item.meshKey = RenderProxyDetail::BuildMeshKey(submission);
                item.primitiveKey = RenderProxyDetail::BuildPrimitiveKey(submission);
                item.doubleSided = submission.material.doubleSided;
                m_Buckets.shadowCasters.push_back(item);
            }

            const bool visible = !submission.hasBounds ||
                                 RenderProxyDetail::SphereIntersectsFrustum(submission.boundsCenter, submission.boundsRadius, frustumPlanes);
            if (!visible)
            {
                continue;
            }

            const RenderBucket bucket = GetBucket(submission);

            const RenderMeshSubmission &visibleSubmission = submission;

            RenderDrawItem item{};
            item.submission = &visibleSubmission;
            item.objectIndex = m_VisibleSubmissionCount++;
            item.sortKey = BuildSortKey(visibleSubmission);
            const glm::vec3 cameraToObject = visibleSubmission.boundsCenter - view.position;
            item.cameraDistanceSq = glm::dot(cameraToObject, cameraToObject);
            item.meshKey = RenderProxyDetail::BuildMeshKey(visibleSubmission);
            item.primitiveKey = RenderProxyDetail::BuildPrimitiveKey(visibleSubmission);
            item.doubleSided = visibleSubmission.material.doubleSided;

            if (bucket == RenderBucket::Transparent)
            {
                m_Buckets.transparent.push_back(item);
            }
            else if (bucket == RenderBucket::Unlit)
            {
                m_Buckets.unlit.push_back(item);
            }
            else
            {
                m_Buckets.opaque.push_back(item);
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
        frameData.objects.clear();
        frameData.materials.clear();
        frameData.objects.reserve(m_Buckets.opaque.size() + m_Buckets.unlit.size() + m_Buckets.transparent.size());
        frameData.materials.reserve(m_SubmissionScratch.size());

        std::unordered_map<const RenderMeshSubmission *, std::uint32_t> objectIndexBySubmission{};
        objectIndexBySubmission.reserve(frameData.objects.capacity());
        std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> materialIndicesBySignature{};
        materialIndicesBySignature.reserve(m_SubmissionScratch.size());

        const auto appendBucketObjects = [&](std::vector<RenderDrawItem> &bucket)
        {
            for (RenderDrawItem &item : bucket)
            {
                if (item.submission == nullptr)
                {
                    continue;
                }

                const RenderMeshSubmission &submission = *item.submission;
                const std::uint64_t materialSignature = RenderProxyDetail::HashMaterialSignature(submission.material);
                std::uint32_t materialIndex = std::numeric_limits<std::uint32_t>::max();
                auto &candidateIndices = materialIndicesBySignature[materialSignature];
                for (std::uint32_t candidateIndex : candidateIndices)
                {
                    if (RenderProxyDetail::MaterialEquals(frameData.materials[candidateIndex], submission.material))
                    {
                        materialIndex = candidateIndex;
                        break;
                    }
                }
                if (materialIndex == std::numeric_limits<std::uint32_t>::max())
                {
                    materialIndex = static_cast<std::uint32_t>(frameData.materials.size());
                    candidateIndices.push_back(materialIndex);
                    frameData.materials.push_back(submission.material);
                }

                ObjectData object = BuildObjectData(submission, GetBucket(submission));
                object.materialIndex = materialIndex;
                const std::uint32_t objectIndex = static_cast<std::uint32_t>(frameData.objects.size());
                objectIndexBySubmission.emplace(&submission, objectIndex);
                item.objectIndex = objectIndex;
                frameData.objects.push_back(object);
            }
        };

        appendBucketObjects(m_Buckets.opaque);
        appendBucketObjects(m_Buckets.unlit);
        appendBucketObjects(m_Buckets.transparent);

        const auto bindObjectIndices = [&objectIndexBySubmission](std::vector<RenderDrawItem> &bucket)
        {
            for (RenderDrawItem &item : bucket)
            {
                const auto found = objectIndexBySubmission.find(item.submission);
                if (found != objectIndexBySubmission.end())
                {
                    item.objectIndex = found->second;
                }
            }
        };

        bindObjectIndices(m_Buckets.opaque);
        bindObjectIndices(m_Buckets.unlit);
        bindObjectIndices(m_Buckets.transparent);
        bindObjectIndices(m_Buckets.shadowCasters);
    }

    std::uint64_t RenderProxy::BuildSortKey(const RenderMeshSubmission &submission)
    {
        const std::uint64_t materialHash = RenderProxyDetail::HashMaterialSignature(submission.material) & 0xffffffffull;
        const std::uint64_t meshHash = RenderProxyDetail::BuildMeshKey(submission) & 0xffffull;
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

    RenderBucket RenderProxy::GetBucket(const RenderMeshSubmission &submission)
    {
        if (submission.material.IsTransparent())
        {
            return RenderBucket::Transparent;
        }
        if (submission.material.shadingModel == ShadingModel::Unlit)
        {
            return RenderBucket::Unlit;
        }
        return RenderBucket::Opaque;
    }
}