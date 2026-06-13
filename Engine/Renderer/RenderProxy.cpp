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

        template <typename Compare>
        void SortCullBuckets(RenderCullDrawBuckets &buckets, Compare compare)
        {
            std::sort(buckets.singleSided.begin(), buckets.singleSided.end(), compare);
            std::sort(buckets.doubleSided.begin(), buckets.doubleSided.end(), compare);
        }
    }

    void RenderCullDrawBuckets::Clear()
    {
        singleSided.clear();
        doubleSided.clear();
    }

    void RenderCullDrawBuckets::Push(RenderDrawItem item)
    {
        if (item.doubleSided)
        {
            doubleSided.push_back(item);
        }
        else
        {
            singleSided.push_back(item);
        }
    }

    bool RenderCullDrawBuckets::Empty() const
    {
        return singleSided.empty() && doubleSided.empty();
    }

    std::size_t RenderCullDrawBuckets::Size() const
    {
        return singleSided.size() + doubleSided.size();
    }

    void RenderDrawBuckets::Clear()
    {
        opaque.Clear();
        unlit.Clear();
        transparent.Clear();
        shadowCasters.clear();
    }

    void RenderCullBatchBuckets::Clear()
    {
        singleSided.clear();
        doubleSided.clear();
    }

    bool RenderCullBatchBuckets::Empty() const
    {
        return singleSided.empty() && doubleSided.empty();
    }

    std::size_t RenderCullBatchBuckets::Size() const
    {
        return singleSided.size() + doubleSided.size();
    }

    void RenderDrawBatchBuckets::Clear()
    {
        opaque.Clear();
        unlit.Clear();
        transparent.Clear();
        instanceObjectIndices.clear();
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
        BuildBatches(frameData);
        frameData.stats.visibleSubmissions = m_VisibleSubmissionCount;
        frameData.stats.opaqueItems = static_cast<std::uint32_t>(m_Buckets.opaque.Size());
        frameData.stats.unlitItems = static_cast<std::uint32_t>(m_Buckets.unlit.Size());
        frameData.stats.transparentItems = static_cast<std::uint32_t>(m_Buckets.transparent.Size());
        frameData.stats.materialInstances = static_cast<std::uint32_t>(frameData.materialInstanceIds.size());
    }

    void RenderProxy::Reset()
    {
        m_Buckets.Clear();
        m_Batches.Clear();
        m_SubmissionScratch.clear();
        m_VisibleSubmissionCount = 0;
    }

    void RenderProxy::CullAndBucket(const std::vector<RenderMeshSubmission> &submissions, const RenderView &view, FrameData &frameData)
    {
        const auto frustumPlanes = RenderProxyDetail::BuildFrustumPlanes(view.viewProjection);

        for (const RenderMeshSubmission &submission : submissions)
        {
            MaterialInstanceId materialInstanceId = InvalidMaterialInstanceId;
            if (submission.material.castShadow &&
                (submission.material.alphaMode == AlphaMode::Opaque || submission.material.alphaMode == AlphaMode::Mask))
            {
                materialInstanceId = m_MaterialRegistry.Resolve(submission.material);
                RenderDrawItem item{};
                item.submission = &submission;
                item.sortKey = BuildSortKey(submission, materialInstanceId);
                item.meshKey = RenderProxyDetail::BuildMeshKey(submission);
                item.primitiveKey = RenderProxyDetail::BuildPrimitiveKey(submission);
                item.materialInstanceId = materialInstanceId;
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
            if (materialInstanceId == InvalidMaterialInstanceId)
            {
                materialInstanceId = m_MaterialRegistry.Resolve(visibleSubmission.material);
            }

            RenderDrawItem item{};
            item.submission = &visibleSubmission;
            item.objectIndex = m_VisibleSubmissionCount++;
            item.sortKey = BuildSortKey(visibleSubmission, materialInstanceId);
            const glm::vec3 cameraToObject = visibleSubmission.boundsCenter - view.position;
            item.cameraDistanceSq = glm::dot(cameraToObject, cameraToObject);
            item.meshKey = RenderProxyDetail::BuildMeshKey(visibleSubmission);
            item.primitiveKey = RenderProxyDetail::BuildPrimitiveKey(visibleSubmission);
            item.materialInstanceId = materialInstanceId;
            item.doubleSided = visibleSubmission.material.doubleSided;

            if (bucket == RenderBucket::Transparent)
            {
                m_Buckets.transparent.Push(item);
            }
            else if (bucket == RenderBucket::Unlit)
            {
                m_Buckets.unlit.Push(item);
            }
            else
            {
                m_Buckets.opaque.Push(item);
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

        RenderProxyDetail::SortCullBuckets(m_Buckets.opaque, bySortKey);
        RenderProxyDetail::SortCullBuckets(m_Buckets.unlit, bySortKey);
        std::sort(m_Buckets.shadowCasters.begin(), m_Buckets.shadowCasters.end(), bySortKey);
        RenderProxyDetail::SortCullBuckets(
            m_Buckets.transparent,
            [](const RenderDrawItem &lhs, const RenderDrawItem &rhs)
            {
                return lhs.cameraDistanceSq > rhs.cameraDistanceSq;
            });
    }

    void RenderProxy::RepackObjectsForSortedBuckets(FrameData &frameData)
    {
        frameData.objects.clear();
        frameData.materials.clear();
        frameData.materialInstanceIds.clear();
        frameData.objects.reserve(
            m_Buckets.opaque.Size() + m_Buckets.unlit.Size() + m_Buckets.transparent.Size() + m_Buckets.shadowCasters.size());
        frameData.materials.reserve(m_SubmissionScratch.size());
        frameData.materialInstanceIds.reserve(m_SubmissionScratch.size());

        std::unordered_map<const RenderMeshSubmission *, std::uint32_t> objectIndexBySubmission{};
        objectIndexBySubmission.reserve(frameData.objects.capacity());
        std::unordered_map<MaterialInstanceId, std::uint32_t> materialIndexByInstance{};
        materialIndexByInstance.reserve(m_SubmissionScratch.size());

        const auto resolveMaterialIndex = [this, &frameData, &materialIndexByInstance](MaterialInstanceId materialInstanceId)
        {
            const auto found = materialIndexByInstance.find(materialInstanceId);
            if (found != materialIndexByInstance.end())
            {
                return found->second;
            }

            const MaterialComponent *material = m_MaterialRegistry.Get(materialInstanceId);
            if (material == nullptr)
            {
                return std::numeric_limits<std::uint32_t>::max();
            }

            const std::uint32_t materialIndex = static_cast<std::uint32_t>(frameData.materials.size());
            materialIndexByInstance.emplace(materialInstanceId, materialIndex);
            frameData.materialInstanceIds.push_back(materialInstanceId);
            frameData.materials.push_back(*material);
            return materialIndex;
        };

        const auto appendObject = [&frameData, &objectIndexBySubmission, &resolveMaterialIndex](RenderDrawItem &item)
        {
            if (item.submission == nullptr)
            {
                return;
            }

            const auto foundObjectIndex = objectIndexBySubmission.find(item.submission);
            if (foundObjectIndex != objectIndexBySubmission.end())
            {
                item.objectIndex = foundObjectIndex->second;
                return;
            }

            const RenderMeshSubmission &submission = *item.submission;
            const std::uint32_t materialIndex = resolveMaterialIndex(item.materialInstanceId);
            if (materialIndex == std::numeric_limits<std::uint32_t>::max())
            {
                return;
            }
            ObjectData object = BuildObjectData(submission, GetBucket(submission));
            object.materialIndex = materialIndex;
            const std::uint32_t objectIndex = static_cast<std::uint32_t>(frameData.objects.size());
            objectIndexBySubmission.emplace(&submission, objectIndex);
            item.objectIndex = objectIndex;
            frameData.objects.push_back(object);
        };

        const auto appendBucketObjects = [&](std::vector<RenderDrawItem> &items)
        {
            for (RenderDrawItem &item : items)
            {
                appendObject(item);
            }
        };

        appendBucketObjects(m_Buckets.opaque.singleSided);
        appendBucketObjects(m_Buckets.opaque.doubleSided);
        appendBucketObjects(m_Buckets.unlit.singleSided);
        appendBucketObjects(m_Buckets.unlit.doubleSided);
        appendBucketObjects(m_Buckets.transparent.singleSided);
        appendBucketObjects(m_Buckets.transparent.doubleSided);
        appendBucketObjects(m_Buckets.shadowCasters);
    }

    void RenderProxy::BuildBatches(FrameData &frameData)
    {
        m_Batches.Clear();

        const auto canAppend = [](const RenderDrawBatch &batch, const RenderDrawItem &item)
        {
            return item.submission != nullptr &&
                   item.sortKey == batch.sortKey &&
                   item.primitiveKey == batch.primitiveKey &&
                   item.materialInstanceId == batch.materialInstanceId &&
                   item.doubleSided == batch.doubleSided;
        };

        m_Batches.instanceObjectIndices.reserve(
            m_Buckets.opaque.Size() + m_Buckets.unlit.Size() + m_Buckets.transparent.Size());

        const auto buildBucketBatches = [this, &canAppend](const std::vector<RenderDrawItem> &items, std::vector<RenderDrawBatch> &batches)
        {
            batches.clear();
            batches.reserve(items.size());
            for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(items.size()); ++i)
            {
                const RenderDrawItem &item = items[i];
                if (item.submission == nullptr)
                {
                    continue;
                }

                if (!batches.empty() && canAppend(batches.back(), item))
                {
                    ++batches.back().itemCount;
                    m_Batches.instanceObjectIndices.push_back(item.objectIndex);
                    continue;
                }

                RenderDrawBatch batch{};
                batch.submission = item.submission;
                batch.firstItem = i;
                batch.itemCount = 1u;
                batch.firstObjectIndex = item.objectIndex;
                batch.firstInstanceIndex = static_cast<std::uint32_t>(m_Batches.instanceObjectIndices.size());
                batch.sortKey = item.sortKey;
                batch.meshKey = item.meshKey;
                batch.primitiveKey = item.primitiveKey;
                batch.materialInstanceId = item.materialInstanceId;
                batch.doubleSided = item.doubleSided;
                batches.push_back(batch);
                m_Batches.instanceObjectIndices.push_back(item.objectIndex);
            }
        };

        buildBucketBatches(m_Buckets.opaque.singleSided, m_Batches.opaque.singleSided);
        buildBucketBatches(m_Buckets.opaque.doubleSided, m_Batches.opaque.doubleSided);
        buildBucketBatches(m_Buckets.unlit.singleSided, m_Batches.unlit.singleSided);
        buildBucketBatches(m_Buckets.unlit.doubleSided, m_Batches.unlit.doubleSided);
        buildBucketBatches(m_Buckets.transparent.singleSided, m_Batches.transparent.singleSided);
        buildBucketBatches(m_Buckets.transparent.doubleSided, m_Batches.transparent.doubleSided);

        frameData.stats.forwardOpaqueBatches =
            static_cast<std::uint32_t>(m_Batches.opaque.Size() + m_Batches.unlit.Size());
        frameData.stats.forwardTransparentBatches = static_cast<std::uint32_t>(m_Batches.transparent.Size());
        frameData.stats.drawBatches = frameData.stats.forwardOpaqueBatches + frameData.stats.forwardTransparentBatches;
    }

    std::uint64_t RenderProxy::BuildSortKey(const RenderMeshSubmission &submission, MaterialInstanceId materialInstanceId)
    {
        const std::uint64_t materialKey = static_cast<std::uint64_t>(materialInstanceId) & 0xffffffffull;
        const std::uint64_t meshHash = RenderProxyDetail::BuildMeshKey(submission) & 0xffffull;
        const std::uint64_t primitive = static_cast<std::uint64_t>(submission.primitiveIndex & 0xffffu);
        return (materialKey << 32u) | (meshHash << 16u) | primitive;
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