#pragma once

#include <cstdint>

namespace Physara::RHI
{
    using TextureUsageFlags = std::uint32_t;
    using BufferUsageFlags = std::uint32_t;

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
}