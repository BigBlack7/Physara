#pragma once

#include <cstdint>
#include <vector>

#include <Engine/Renderer/FrameData.hpp>
#include <Engine/Scene/Systems/RenderSystem.hpp>

namespace Physara::Engine
{
    class AssetManager;
    class Scene;

    enum class RenderBucket : std::uint32_t
    {
        Opaque = 0,
        Unlit = 1,
        Transparent = 2
    };

    namespace ObjectFlags
    {
        constexpr std::uint32_t None = 0u;
        constexpr std::uint32_t CastShadow = 1u << 0;
        constexpr std::uint32_t ReceiveShadow = 1u << 1;
        constexpr std::uint32_t Transparent = 1u << 2;
        constexpr std::uint32_t Unlit = 1u << 3;
    }

    struct RenderDrawItem
    {
        const RenderMeshSubmission *submission{nullptr};
        std::uint32_t objectIndex{0};
        std::uint64_t sortKey{0};
        float cameraDistanceSq{0.f};
        std::uint64_t meshKey{0};
        std::uint64_t primitiveKey{0};
        bool doubleSided{false};
    };

    struct RenderDrawBuckets
    {
        std::vector<RenderDrawItem> opaque{};
        std::vector<RenderDrawItem> unlit{};
        std::vector<RenderDrawItem> transparent{};
        std::vector<RenderDrawItem> shadowCasters{};

        void Clear();
    };

    struct RenderDrawBatch
    {
        const RenderMeshSubmission *submission{nullptr};
        std::uint32_t firstItem{0};
        std::uint32_t itemCount{0};
        std::uint32_t firstObjectIndex{0};
        std::uint32_t firstInstanceIndex{0};
        std::uint64_t sortKey{0};
        std::uint64_t meshKey{0};
        std::uint64_t primitiveKey{0};
        bool doubleSided{false};
    };

    struct RenderDrawBatchBuckets
    {
        std::vector<RenderDrawBatch> opaque{};
        std::vector<RenderDrawBatch> unlit{};
        std::vector<RenderDrawBatch> transparent{};
        std::vector<std::uint32_t> instanceObjectIndices{};

        void Clear();
    };

    class RenderProxy final
    {
    public:
        void Build(Scene &scene, const RenderView &view, FrameData &frameData, AssetManager *assetManager = nullptr);
        void Reset();

        [[nodiscard]] const RenderDrawBuckets &GetBuckets() const { return m_Buckets; }
        [[nodiscard]] const RenderDrawBatchBuckets &GetBatches() const { return m_Batches; }

    private:
        void CullAndBucket(const std::vector<RenderMeshSubmission> &submissions, const RenderView &view, FrameData &frameData);
        void SortBuckets();
        void RepackObjectsForSortedBuckets(FrameData &frameData);
        void BuildBatches(FrameData &frameData);
        [[nodiscard]] static std::uint64_t BuildSortKey(const RenderMeshSubmission &submission);
        [[nodiscard]] static ObjectData BuildObjectData(const RenderMeshSubmission &submission, RenderBucket bucket);
        [[nodiscard]] static RenderBucket GetBucket(const RenderMeshSubmission &submission);

    private:
        RenderDrawBuckets m_Buckets{};
        RenderDrawBatchBuckets m_Batches{};
        std::vector<RenderMeshSubmission> m_SubmissionScratch{};
        std::uint32_t m_VisibleSubmissionCount{0};
    };
}