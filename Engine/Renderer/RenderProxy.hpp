#pragma once

#include <cstdint>
#include <vector>

#include <Engine/Renderer/FrameData.hpp>
#include <Engine/Scene/Systems/RenderSystem.hpp>

namespace Physara::Engine
{
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
        RenderMeshSubmission submission{};
        std::uint32_t objectIndex{0};
        std::uint64_t sortKey{0};
        float cameraDistanceSq{0.f};
    };

    struct RenderDrawBuckets
    {
        std::vector<RenderDrawItem> opaque{};
        std::vector<RenderDrawItem> unlit{};
        std::vector<RenderDrawItem> transparent{};
        std::vector<RenderDrawItem> shadowCasters{};

        void Clear();
    };

    class RenderProxy final
    {
    public:
        void Build(Scene &scene, const RenderView &view, FrameData &frameData);
        void Reset();

        [[nodiscard]] const RenderDrawBuckets &GetBuckets() const { return m_Buckets; }
        [[nodiscard]] const std::vector<RenderMeshSubmission> &GetVisibleSubmissions() const { return m_VisibleSubmissions; }

    private:
        void CullAndBucket(const std::vector<RenderMeshSubmission> &submissions, const RenderView &view, FrameData &frameData);
        void SortBuckets();
        [[nodiscard]] static std::uint64_t BuildSortKey(const RenderMeshSubmission &submission);
        [[nodiscard]] static ObjectData BuildObjectData(const RenderMeshSubmission &submission, RenderBucket bucket);

    private:
        RenderDrawBuckets m_Buckets{};
        std::vector<RenderMeshSubmission> m_VisibleSubmissions{};
    };
}