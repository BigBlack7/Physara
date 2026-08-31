#pragma once

#include <cstddef>
#include <cstdint>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

namespace Physara::Engine
{
    constexpr std::uint32_t MaxForwardLights = 128u;
    constexpr std::uint32_t MaxShadowCascades = 4u;

    enum class GPUBufferBinding : std::uint32_t
    {
        FrameUniforms = 0,
        Camera = 0,
        Objects = 1,
        Materials = 2,
        Lights = 3,
        InstanceIndices = 4,
        PostProcessSettings = 4,
        SkyboxSettings = 4,
        WorldGridSettings = 4,
        RenderSettings = 5,
        Shadow = 6,
        IBL = 7,
        MaterialTextureIndices = 8,
        BindlessTextureHandles = 9,
        ClusterEntries = 10,
        ClusterLightIndices = 11
    };

    enum class GPUTextureBinding : std::uint32_t
    {
        BaseColor = 0,
        MetallicRoughness = 1,
        Normal = 2,
        Occlusion = 3,
        Emissive = 4,
        Skybox = 5,
        SceneColor = 6,
        SceneDepth = 7,
        ShadowMap = 8,
        IBLPrefiltered = 9,
        IBLBRDFLut = 10,
        Bloom = 11,
        GBufferBaseColor = 12,
        GBufferNormal = 13,
        GBufferMaterial = 14,
        GBufferEmissive = 15
    };

    enum class GPUResourceSetIndex : std::uint32_t
    {
        PerView = 0,
        PerRenderable = 1,
        PerMaterial = 2
    };

    enum class LightTypeGPU : std::uint32_t
    {
        Directional = 0,
        Point = 1,
        Spot = 2,
        Area = 3
    };

    enum class ShadingModelGPU : std::uint32_t
    {
        Lit = 0,
        Unlit = 1
    };

    enum class AlphaModeGPU : std::uint32_t
    {
        Opaque = 0,
        Mask = 1,
        Blend = 2
    };

    enum class ShadowFilterGPU : std::uint32_t
    {
        Hard = 0,
        PCF3x3 = 1,
        PCF5x5 = 2,
        Poisson16 = 3,
        PCSS = 4
    };

    namespace ObjectFlags
    {
        constexpr std::uint32_t None = 0u;
        constexpr std::uint32_t CastShadow = 1u << 0;
        constexpr std::uint32_t ReceiveShadow = 1u << 1;
        constexpr std::uint32_t Transparent = 1u << 2;
        constexpr std::uint32_t Unlit = 1u << 3;
    }

    [[nodiscard]] constexpr std::uint32_t Binding(GPUBufferBinding binding)
    {
        return static_cast<std::uint32_t>(binding);
    }

    [[nodiscard]] constexpr std::uint32_t Binding(GPUTextureBinding binding)
    {
        return static_cast<std::uint32_t>(binding);
    }

    [[nodiscard]] constexpr std::uint32_t Binding(GPUResourceSetIndex setIndex)
    {
        return static_cast<std::uint32_t>(setIndex);
    }

    template <typename T>
    [[nodiscard]] constexpr std::uint32_t GPUValue(T value)
    {
        return static_cast<std::uint32_t>(value);
    }

    struct alignas(16) CameraData
    {
        glm::mat4 view{1.f};
        glm::mat4 projection{1.f};
        glm::mat4 viewProjection{1.f};
        glm::mat4 inverseView{1.f};
        glm::mat4 inverseProjection{1.f};
        glm::mat4 inverseViewProjection{1.f};
        glm::vec4 cameraPositionEV100{0.f, 0.f, 0.f, 0.f};
        glm::vec4 exposure{1.f, 1.f, 0.f, 0.f};
        glm::vec4 viewportRect{0.f, 0.f, 1.f, 1.f};
        glm::vec4 clipPlanes{0.1f, 1000.f, 0.f, 0.f};
    };

    struct alignas(16) ObjectData
    {
        glm::mat4 model{1.f};
        glm::mat4 inverseTransposeModel{1.f};
        glm::vec4 boundsCenterRadius{0.f, 0.f, 0.f, 0.f};
        std::uint32_t objectId{0};
        std::uint32_t meshIndex{0};
        std::uint32_t materialIndex{0};
        std::uint32_t flags{0};
    };

    struct alignas(16) MaterialGPUData
    {
        glm::vec4 baseColor{1.f};
        glm::vec4 emissiveColorLuminance{0.f, 0.f, 0.f, 0.f};
        glm::vec4 metallicRoughnessReflectanceAO{0.f, 0.5f, 0.5f, 1.f};
        glm::vec4 alphaNormalFlags{0.5f, 1.f, 0.f, 0.f};
        glm::vec4 textureFlags{0.f, 0.f, 0.f, 0.f};
        glm::vec4 textureCoordSets{0.f, 0.f, 0.f, 0.f};
        glm::vec4 materialFlags{0.f, 0.f, 0.f, 0.f};
        glm::vec4 textureInfluences{1.f, 1.f, 1.f, 0.f};
    };

    struct alignas(16) LightData
    {
        glm::vec4 positionRange{0.f, 0.f, 0.f, 0.f};
        glm::vec4 directionType{0.f, -1.f, 0.f, 0.f};
        glm::vec4 colorIntensity{1.f, 1.f, 1.f, 0.f};
        glm::vec4 spotAngles{0.f, 0.f, 0.f, 0.f};
        glm::vec4 shadowParams{0.f, 0.f, 0.f, 0.f};
    };

    struct alignas(16) ShadowData
    {
        glm::mat4 lightViewProjection[MaxShadowCascades]{
            glm::mat4(1.f),
            glm::mat4(1.f),
            glm::mat4(1.f),
            glm::mat4(1.f)};
        glm::vec4 cascadeSplits{0.f, 0.f, 0.f, 0.f};
        glm::vec4 cascadeTexelWorldSize{0.f, 0.f, 0.f, 0.f};
        glm::vec4 params{0.f, 0.f, 0.f, 0.f};
        glm::vec4 controls{0.f, 0.f, 0.f, 0.f};
        glm::vec4 samplingParams{0.f, 0.f, 0.f, 0.f};
    };

    struct alignas(16) IBLData
    {
        glm::vec4 irradianceSH[9]{};
        glm::vec4 params{0.f, 0.f, 0.f, 0.f};
    };

    struct alignas(16) ClusterGridData
    {
        glm::uvec4 dimensions{1u, 1u, 1u, 1u};
        glm::vec4 depthParams{0.1f, 1000.f, 1.f, 0.f};
        glm::uvec4 counts{1u, 0u, 0u, 0u};
    };

    struct ClusterEntryGPU
    {
        std::uint32_t offset{0};
        std::uint32_t count{0};
    };

    struct alignas(16) FrameUniforms
    {
        CameraData camera{};
        ShadowData shadow{};
        IBLData ibl{};
        ClusterGridData clusterGrid{};
        glm::vec4 debugParams{0.f, 0.f, 0.f, 0.f};
    };

    // CPU↔GPU 契约:布局编译期锁定。
    // 锁定每个 GPU 结构的 sizeof 与逐字段 offsetof:字段被重排/改型/增删即编译失败,
    // 阻止 CPU↔GLSL 布局静默漂移。有意重设计布局时须同步更新以下常量。
    static_assert(sizeof(CameraData) == 448);
    static_assert(offsetof(CameraData, view) == 0);
    static_assert(offsetof(CameraData, projection) == 64);
    static_assert(offsetof(CameraData, viewProjection) == 128);
    static_assert(offsetof(CameraData, inverseView) == 192);
    static_assert(offsetof(CameraData, inverseProjection) == 256);
    static_assert(offsetof(CameraData, inverseViewProjection) == 320);
    static_assert(offsetof(CameraData, cameraPositionEV100) == 384);
    static_assert(offsetof(CameraData, exposure) == 400);
    static_assert(offsetof(CameraData, viewportRect) == 416);
    static_assert(offsetof(CameraData, clipPlanes) == 432);

    static_assert(sizeof(ObjectData) == 160);
    static_assert(offsetof(ObjectData, model) == 0);
    static_assert(offsetof(ObjectData, inverseTransposeModel) == 64);
    static_assert(offsetof(ObjectData, boundsCenterRadius) == 128);
    static_assert(offsetof(ObjectData, objectId) == 144);
    static_assert(offsetof(ObjectData, meshIndex) == 148);
    static_assert(offsetof(ObjectData, materialIndex) == 152);
    static_assert(offsetof(ObjectData, flags) == 156);

    static_assert(sizeof(MaterialGPUData) == 128);
    static_assert(offsetof(MaterialGPUData, baseColor) == 0);
    static_assert(offsetof(MaterialGPUData, emissiveColorLuminance) == 16);
    static_assert(offsetof(MaterialGPUData, metallicRoughnessReflectanceAO) == 32);
    static_assert(offsetof(MaterialGPUData, alphaNormalFlags) == 48);
    static_assert(offsetof(MaterialGPUData, textureFlags) == 64);
    static_assert(offsetof(MaterialGPUData, textureCoordSets) == 80);
    static_assert(offsetof(MaterialGPUData, materialFlags) == 96);
    static_assert(offsetof(MaterialGPUData, textureInfluences) == 112);

    static_assert(sizeof(LightData) == 80);
    static_assert(offsetof(LightData, positionRange) == 0);
    static_assert(offsetof(LightData, directionType) == 16);
    static_assert(offsetof(LightData, colorIntensity) == 32);
    static_assert(offsetof(LightData, spotAngles) == 48);
    static_assert(offsetof(LightData, shadowParams) == 64);

    static_assert(sizeof(ShadowData) == 336);
    static_assert(offsetof(ShadowData, lightViewProjection) == 0);
    static_assert(offsetof(ShadowData, cascadeSplits) == 256);
    static_assert(offsetof(ShadowData, cascadeTexelWorldSize) == 272);
    static_assert(offsetof(ShadowData, params) == 288);
    static_assert(offsetof(ShadowData, controls) == 304);
    static_assert(offsetof(ShadowData, samplingParams) == 320);

    static_assert(sizeof(IBLData) == 160);
    static_assert(offsetof(IBLData, irradianceSH) == 0);
    static_assert(offsetof(IBLData, params) == 144);

    static_assert(sizeof(ClusterGridData) == 48);
    static_assert(offsetof(ClusterGridData, dimensions) == 0);
    static_assert(offsetof(ClusterGridData, depthParams) == 16);
    static_assert(offsetof(ClusterGridData, counts) == 32);

    static_assert(sizeof(ClusterEntryGPU) == 8);
    static_assert(offsetof(ClusterEntryGPU, offset) == 0);
    static_assert(offsetof(ClusterEntryGPU, count) == 4);

    static_assert(sizeof(FrameUniforms) == 1008);
    static_assert(offsetof(FrameUniforms, camera) == 0);
    static_assert(offsetof(FrameUniforms, shadow) == 448);
    static_assert(offsetof(FrameUniforms, ibl) == 784);
    static_assert(offsetof(FrameUniforms, clusterGrid) == 944);
    static_assert(offsetof(FrameUniforms, debugParams) == 992);
}