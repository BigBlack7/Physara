#include "RenderProxy.hpp"

#include <algorithm>
#include <array>
#include <functional>
#include <limits>

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

    void RenderCullCommandBuckets::Clear()
    {
        singleSided.clear();
        doubleSided.clear();
    }

    bool RenderCullCommandBuckets::Empty() const
    {
        return singleSided.empty() && doubleSided.empty();
    }

    std::size_t RenderCullCommandBuckets::Size() const
    {
        return singleSided.size() + doubleSided.size();
    }

    void RenderCommandBuckets::Clear()
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
        RenderSystem::Collect(scene, m_SubmissionScratch, assetManager, &m_CollectScratch);
        CullAndBucket(m_SubmissionScratch, view, frameData);
        SortBuckets();
        RepackObjectsForSortedBuckets(frameData);
        BuildCommands(frameData);
        frameData.stats.visibleSubmissions = m_VisibleSubmissionCount;
        frameData.stats.opaqueItems = static_cast<std::uint32_t>(m_Buckets.opaque.Size());
        frameData.stats.unlitItems = static_cast<std::uint32_t>(m_Buckets.unlit.Size());
        frameData.stats.transparentItems = static_cast<std::uint32_t>(m_Buckets.transparent.Size());
        frameData.stats.materialInstances = static_cast<std::uint32_t>(frameData.materialInstanceIds.size());
    }

    void RenderProxy::Reset()
    {
        m_Buckets.Clear();
        m_Commands.Clear();
        m_SubmissionScratch.clear();
        m_VisibleSubmissionCount = 0;
    }

    void RenderProxy::CullAndBucket(const std::vector<RenderMeshSubmission> &submissions, const RenderView &view, FrameData &frameData)
    {
        const auto frustumPlanes = RenderProxyDetail::BuildFrustumPlanes(view.viewProjection);

        for (std::uint32_t submissionIndex = 0; submissionIndex < static_cast<std::uint32_t>(submissions.size()); ++submissionIndex)
        {
            const RenderMeshSubmission &submission = submissions[submissionIndex];
            MaterialInstanceId materialInstanceId = InvalidMaterialInstanceId;
            if (submission.material.castShadow &&
                (submission.material.alphaMode == AlphaMode::Opaque || submission.material.alphaMode == AlphaMode::Mask))
            {
                materialInstanceId = m_MaterialRegistry.Resolve(submission.materialSignature, submission.material);
                RenderDrawItem item{};
                item.submission = &submission;
                item.sourceSubmissionIndex = submissionIndex;
                item.sortKey = BuildSortKey(submission, materialInstanceId);
                item.meshKey = submission.meshKey;
                item.primitiveKey = submission.primitiveKey;
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
                materialInstanceId = m_MaterialRegistry.Resolve(visibleSubmission.materialSignature, visibleSubmission.material);
            }

            RenderDrawItem item{};
            item.submission = &visibleSubmission;
            item.objectIndex = m_VisibleSubmissionCount++;
            item.sourceSubmissionIndex = submissionIndex;
            item.sortKey = BuildSortKey(visibleSubmission, materialInstanceId);
            const glm::vec3 cameraToObject = visibleSubmission.boundsCenter - view.position;
            item.cameraDistanceSq = glm::dot(cameraToObject, cameraToObject);
            item.meshKey = visibleSubmission.meshKey;
            item.primitiveKey = visibleSubmission.primitiveKey;
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
        const auto byMaterialMeshPrimitive = [](const RenderDrawItem &lhs, const RenderDrawItem &rhs)
        {
            if (lhs.materialInstanceId != rhs.materialInstanceId)
            {
                return lhs.materialInstanceId < rhs.materialInstanceId;
            }
            if (lhs.meshKey != rhs.meshKey)
            {
                return lhs.meshKey < rhs.meshKey;
            }
            if (lhs.primitiveKey != rhs.primitiveKey)
            {
                return lhs.primitiveKey < rhs.primitiveKey;
            }
            return lhs.objectIndex < rhs.objectIndex;
        };

        const auto byShadowMeshPrimitive = [](const RenderDrawItem &lhs, const RenderDrawItem &rhs)
        {
            if (lhs.meshKey != rhs.meshKey)
            {
                return lhs.meshKey < rhs.meshKey;
            }
            if (lhs.primitiveKey != rhs.primitiveKey)
            {
                return lhs.primitiveKey < rhs.primitiveKey;
            }
            if (lhs.materialInstanceId != rhs.materialInstanceId)
            {
                return lhs.materialInstanceId < rhs.materialInstanceId;
            }
            return std::less<const RenderMeshSubmission *>{}(lhs.submission, rhs.submission);
        };

        RenderProxyDetail::SortCullBuckets(m_Buckets.opaque, byMaterialMeshPrimitive);
        RenderProxyDetail::SortCullBuckets(m_Buckets.unlit, byMaterialMeshPrimitive);
        std::sort(m_Buckets.shadowCasters.begin(), m_Buckets.shadowCasters.end(), byShadowMeshPrimitive);
        RenderProxyDetail::SortCullBuckets(
            m_Buckets.transparent,
            [](const RenderDrawItem &lhs, const RenderDrawItem &rhs)
            {
                if (lhs.cameraDistanceSq != rhs.cameraDistanceSq)
                {
                    return lhs.cameraDistanceSq > rhs.cameraDistanceSq;
                }
                if (lhs.materialInstanceId != rhs.materialInstanceId)
                {
                    return lhs.materialInstanceId < rhs.materialInstanceId;
                }
                if (lhs.meshKey != rhs.meshKey)
                {
                    return lhs.meshKey < rhs.meshKey;
                }
                return lhs.primitiveKey < rhs.primitiveKey;
            });
    }

    void RenderProxy::RepackObjectsForSortedBuckets(FrameData &frameData)
    {
        frameData.objects.clear();
        frameData.materials.clear();
        frameData.materialInstanceIds.clear();
        frameData.materialSignatures.clear();
        frameData.objects.reserve(
            m_Buckets.opaque.Size() + m_Buckets.unlit.Size() + m_Buckets.transparent.Size() + m_Buckets.shadowCasters.size());
        frameData.materials.reserve(m_SubmissionScratch.size());
        frameData.materialInstanceIds.reserve(m_SubmissionScratch.size());
        frameData.materialSignatures.reserve(m_SubmissionScratch.size());

        constexpr std::uint32_t InvalidIndex = std::numeric_limits<std::uint32_t>::max();
        m_ObjectIndexBySubmissionScratch.assign(m_SubmissionScratch.size(), InvalidIndex);
        m_MaterialIndexByInstanceScratch.assign(m_MaterialRegistry.GetCount(), InvalidIndex);

        const auto resolveMaterialIndex = [this, &frameData](MaterialInstanceId materialInstanceId)
        {
            if (materialInstanceId == InvalidMaterialInstanceId)
            {
                return std::numeric_limits<std::uint32_t>::max();
            }

            if (materialInstanceId >= m_MaterialIndexByInstanceScratch.size())
            {
                m_MaterialIndexByInstanceScratch.resize(materialInstanceId + 1u, std::numeric_limits<std::uint32_t>::max());
            }

            std::uint32_t &cachedMaterialIndex = m_MaterialIndexByInstanceScratch[materialInstanceId];
            if (cachedMaterialIndex != std::numeric_limits<std::uint32_t>::max())
            {
                return cachedMaterialIndex;
            }

            const MaterialComponent *material = m_MaterialRegistry.Get(materialInstanceId);
            if (material == nullptr)
            {
                return std::numeric_limits<std::uint32_t>::max();
            }

            const std::uint32_t materialIndex = static_cast<std::uint32_t>(frameData.materials.size());
            cachedMaterialIndex = materialIndex;
            frameData.materialInstanceIds.push_back(materialInstanceId);
            frameData.materialSignatures.push_back(m_MaterialRegistry.GetSignature(materialInstanceId));
            frameData.materials.push_back(*material);
            return materialIndex;
        };

        const auto appendObject = [this, &frameData, &resolveMaterialIndex](RenderDrawItem &item)
        {
            if (item.submission == nullptr)
            {
                return;
            }

            if (item.sourceSubmissionIndex < m_ObjectIndexBySubmissionScratch.size())
            {
                const std::uint32_t cachedObjectIndex = m_ObjectIndexBySubmissionScratch[item.sourceSubmissionIndex];
                if (cachedObjectIndex != std::numeric_limits<std::uint32_t>::max())
                {
                    item.objectIndex = cachedObjectIndex;
                    return;
                }
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
            if (item.sourceSubmissionIndex < m_ObjectIndexBySubmissionScratch.size())
            {
                m_ObjectIndexBySubmissionScratch[item.sourceSubmissionIndex] = objectIndex;
            }
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

    void RenderProxy::BuildCommands(FrameData &frameData)
    {
        m_Commands.Clear();

        const auto canAppend = [](const RenderCommand &command, const RenderDrawItem &item)
        {
            return item.submission != nullptr &&
                   item.sortKey == command.sortKey &&
                   item.primitiveKey == command.primitiveKey &&
                   item.materialInstanceId == command.materialInstanceId &&
                   item.doubleSided == command.doubleSided;
        };

        m_Commands.instanceObjectIndices.reserve(
            m_Buckets.opaque.Size() + m_Buckets.unlit.Size() + m_Buckets.transparent.Size());

        const auto buildBucketCommands = [this, &canAppend](const std::vector<RenderDrawItem> &items, std::vector<RenderCommand> &commands, RenderBucket bucket)
        {
            commands.clear();
            commands.reserve(items.size());
            for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(items.size()); ++i)
            {
                const RenderDrawItem &item = items[i];
                if (item.submission == nullptr)
                {
                    continue;
                }

                if (!commands.empty() && canAppend(commands.back(), item))
                {
                    ++commands.back().instanceCount;
                    m_Commands.instanceObjectIndices.push_back(item.objectIndex);
                    continue;
                }

                RenderCommand command{};
                command.submission = item.submission;
                command.sourceItemIndex = i;
                command.instanceCount = 1u;
                command.firstObjectIndex = item.objectIndex;
                command.firstInstanceIndex = static_cast<std::uint32_t>(m_Commands.instanceObjectIndices.size());
                command.sortKey = item.sortKey;
                command.meshKey = item.meshKey;
                command.primitiveKey = item.primitiveKey;
                command.materialInstanceId = item.materialInstanceId;
                command.bucket = bucket;
                command.doubleSided = item.doubleSided;
                commands.push_back(command);
                m_Commands.instanceObjectIndices.push_back(item.objectIndex);
            }
        };

        buildBucketCommands(m_Buckets.opaque.singleSided, m_Commands.opaque.singleSided, RenderBucket::Opaque);
        buildBucketCommands(m_Buckets.opaque.doubleSided, m_Commands.opaque.doubleSided, RenderBucket::Opaque);
        buildBucketCommands(m_Buckets.unlit.singleSided, m_Commands.unlit.singleSided, RenderBucket::Unlit);
        buildBucketCommands(m_Buckets.unlit.doubleSided, m_Commands.unlit.doubleSided, RenderBucket::Unlit);
        buildBucketCommands(m_Buckets.transparent.singleSided, m_Commands.transparent.singleSided, RenderBucket::Transparent);
        buildBucketCommands(m_Buckets.transparent.doubleSided, m_Commands.transparent.doubleSided, RenderBucket::Transparent);

        frameData.stats.forwardOpaqueBatches =
            static_cast<std::uint32_t>(m_Commands.opaque.Size() + m_Commands.unlit.Size());
        frameData.stats.forwardTransparentBatches = static_cast<std::uint32_t>(m_Commands.transparent.Size());
        frameData.stats.drawBatches = frameData.stats.forwardOpaqueBatches + frameData.stats.forwardTransparentBatches;
    }

    std::uint64_t RenderProxy::BuildSortKey(const RenderMeshSubmission &submission, MaterialInstanceId materialInstanceId)
    {
        const std::uint64_t materialKey = static_cast<std::uint64_t>(materialInstanceId) & 0xffffffffull;
        const std::uint64_t meshHash = submission.meshKey & 0xffffull;
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
