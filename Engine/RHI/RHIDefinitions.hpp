#pragma once

#include <cstdint>

namespace Physara::RHI
{
    using TextureUsageFlags = std::uint32_t;
    using BufferUsageFlags = std::uint32_t;
    using ShaderStageFlags = std::uint32_t;
    using ResourceAccessFlags = std::uint32_t;

    namespace TextureUsage
    {
        constexpr TextureUsageFlags None = 0u;
        constexpr TextureUsageFlags Sampled = 1u << 0;      // 采样纹理
        constexpr TextureUsageFlags RenderTarget = 1u << 1; // 渲染目标
        constexpr TextureUsageFlags DepthStencil = 1u << 2; // 深度/模板缓冲
        constexpr TextureUsageFlags Storage = 1u << 3;      // 存储纹理
    }

    namespace BufferUsage
    {
        constexpr BufferUsageFlags None = 0u;
        constexpr BufferUsageFlags Vertex = 1u << 0;  // 顶点缓冲
        constexpr BufferUsageFlags Index = 1u << 1;   // 索引缓冲
        constexpr BufferUsageFlags Uniform = 1u << 2; // 常量/Uniform缓冲
        constexpr BufferUsageFlags Storage = 1u << 3; // 存储缓冲
    }

    enum class VertexFormat : std::uint8_t
    {
        RGBA32F = 0,
        RGBA16F,
        RG16F,
        RGBA8,
        R8
    };

    enum class TextureFormat : std::uint8_t
    {
        RGBA8 = 0,
        RGBA16F,
        RGBA32F,
        RG16F,
        R8,
        BC6H_UF16,
        BC7_UNORM,
        Depth24Stencil8,
        Depth32F
    };

    enum class TextureDimension : std::uint8_t
    {
        Tex2D = 0,
        TexCube,
        Tex2DArray
    };

    enum class FilterMode : std::uint8_t
    {
        Nearest = 0,
        Linear,
        LinearMipmapLinear
    };

    enum class WrapMode : std::uint8_t
    {
        Repeat = 0,
        ClampToEdge,
        ClampToBorder,
        MirroredRepeat
    };

    enum class CompareOp : std::uint8_t
    {
        None = 0,
        Less,
        LessEqual,
        Greater
    };

    enum class ShaderStage : std::uint8_t
    {
        Vertex = 0,
        Fragment,
        Compute
    };

    namespace ShaderStageBit
    {
        constexpr ShaderStageFlags None = 0u;
        constexpr ShaderStageFlags Vertex = 1u << 0;
        constexpr ShaderStageFlags Fragment = 1u << 1;
        constexpr ShaderStageFlags Compute = 1u << 2;
        constexpr ShaderStageFlags AllGraphics = Vertex | Fragment;
        constexpr ShaderStageFlags All = Vertex | Fragment | Compute;
    }

    enum class ResourceState : std::uint8_t
    {
        Undefined = 0,
        Common,
        VertexBuffer,
        IndexBuffer,
        ConstantBuffer,
        ShaderResource,
        UnorderedAccess,
        RenderTarget,
        DepthWrite,
        DepthRead,
        CopySource,
        CopyDest,
        Present
    };

    namespace ResourceAccess
    {
        constexpr ResourceAccessFlags None = 0u;
        constexpr ResourceAccessFlags IndirectCommandRead = 1u << 0;
        constexpr ResourceAccessFlags IndexRead = 1u << 1;
        constexpr ResourceAccessFlags VertexAttributeRead = 1u << 2;
        constexpr ResourceAccessFlags UniformRead = 1u << 3;
        constexpr ResourceAccessFlags ShaderRead = 1u << 4;
        constexpr ResourceAccessFlags ShaderWrite = 1u << 5;
        constexpr ResourceAccessFlags ColorAttachmentRead = 1u << 6;
        constexpr ResourceAccessFlags ColorAttachmentWrite = 1u << 7;
        constexpr ResourceAccessFlags DepthStencilRead = 1u << 8;
        constexpr ResourceAccessFlags DepthStencilWrite = 1u << 9;
        constexpr ResourceAccessFlags TransferRead = 1u << 10;
        constexpr ResourceAccessFlags TransferWrite = 1u << 11;
        constexpr ResourceAccessFlags HostRead = 1u << 12;
        constexpr ResourceAccessFlags HostWrite = 1u << 13;
        constexpr ResourceAccessFlags MemoryRead = 1u << 14;
        constexpr ResourceAccessFlags MemoryWrite = 1u << 15;
    }

    enum class CullMode : std::uint8_t
    {
        None = 0,
        Front,
        Back
    };

    enum class DepthCompareOp : std::uint8_t
    {
        Less = 0,
        LessEqual,
        Equal,
        Always
    };

    enum class BlendFactor : std::uint8_t
    {
        Zero = 0,
        One,
        SrcColor,
        OneMinusSrcColor,
        DstColor,
        OneMinusDstColor,
        SrcAlpha,
        OneMinusSrcAlpha,
        DstAlpha,
        OneMinusDstAlpha
    };

    enum class BlendOp : std::uint8_t
    {
        Add = 0,
        Subtract,
        ReverseSubtract
    };

    enum class PrimitiveTopology : std::uint8_t
    {
        Triangles = 0,
        Lines,
        Points
    };

    enum class PolygonMode : std::uint8_t
    {
        Fill = 0,
        Line
    };

    enum class LoadOp : std::uint8_t
    {
        Load = 0,
        Clear,
        DontCare
    };

    enum class StoreOp : std::uint8_t
    {
        Store = 0,
        DontCare
    };

    struct RHIViewport
    {
        float x = 0.f;
        float y = 0.f;
        float width = 0.f;
        float height = 0.f;
        float minDepth = 0.f;
        float maxDepth = 1.f;
    };

    struct RHIRect2D
    {
        std::int32_t x = 0;
        std::int32_t y = 0;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
    };

    struct RHIClearValue
    {
        float color[4]{0.f, 0.f, 0.f, 1.f};
        float depth = 1.f;
        std::uint32_t stencil = 0;
    };

    struct RHIResourceBarrier
    {
        ResourceState before = ResourceState::Undefined;
        ResourceState after = ResourceState::Common;
        ShaderStageFlags srcStages = ShaderStageBit::None;
        ShaderStageFlags dstStages = ShaderStageBit::None;
        ResourceAccessFlags srcAccess = ResourceAccess::None;
        ResourceAccessFlags dstAccess = ResourceAccess::None;
    };
}