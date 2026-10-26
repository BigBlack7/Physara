/*
* Copyright (c) 2014-2021, NVIDIA CORPORATION. All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*/

#pragma once

// 来源与受控差异见 RHI/README.md; 此头不代表后端能力已实现.

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "Containers.hpp"
#include "Resource.hpp"
#include "ShaderTypes.hpp"

#define PHYSARA_RHI_ENUM_CLASS_FLAG_OPERATORS(T)                                                                       \
    inline T operator|(T a, T b)                                                                                       \
    {                                                                                                                  \
        return T(uint32_t(a) | uint32_t(b));                                                                           \
    }                                                                                                                  \
    inline T operator&(T a, T b)                                                                                       \
    {                                                                                                                  \
        return T(uint32_t(a) & uint32_t(b));                                                                           \
    } /* NOLINT(bugprone-macro-parentheses) */                                                                         \
    inline T operator~(T a)                                                                                            \
    {                                                                                                                  \
        return T(~uint32_t(a));                                                                                        \
    } /* NOLINT(bugprone-macro-parentheses) */                                                                         \
    inline bool operator!(T a)                                                                                         \
    {                                                                                                                  \
        return uint32_t(a) == 0;                                                                                       \
    }                                                                                                                  \
    inline bool operator==(T a, uint32_t b)                                                                            \
    {                                                                                                                  \
        return uint32_t(a) == b;                                                                                       \
    }                                                                                                                  \
    inline bool operator!=(T a, uint32_t b)                                                                            \
    {                                                                                                                  \
        return uint32_t(a) != b;                                                                                       \
    }

#if defined(PHYSARA_RHI_SHARED_LIBRARY_BUILD)
#if defined(_MSC_VER)
#define PHYSARA_RHI_API __declspec(dllexport)
#elif defined(__GNUC__)
#define PHYSARA_RHI_API __attribute__((visibility("default")))
#else
#define PHYSARA_RHI_API
#pragma warning "Unknown dynamic link import/export semantics."
#endif
#elif defined(PHYSARA_RHI_SHARED_LIBRARY_INCLUDE)
#if defined(_MSC_VER)
#define PHYSARA_RHI_API __declspec(dllimport)
#else
#define PHYSARA_RHI_API
#endif
#else
#define PHYSARA_RHI_API
#endif

namespace Physara::RHI
{
    // Version of the public API provided by NVRHI.
    // Increment this when any changes to the API are made.
    static constexpr uint32_t HeaderVersion = 27;
    inline constexpr uint32_t OriginalHeaderVersion = 27;

    // Verifies that the version of the implementation matches the version of the header.
    // Returns true if they match. Use this when initializing apps using NVRHI as a shared library.
    PHYSARA_RHI_API bool VerifyHeaderVersion(uint32_t version = HeaderVersion);

    static constexpr uint32_t MaxRenderTargets = 8;
    static constexpr uint32_t MaxViewports = 16;
    static constexpr uint32_t MaxVertexAttributes = 16;
    static constexpr uint32_t MaxBindingLayouts = 8;
    static constexpr uint32_t MaxBindlessRegisterSpaces = 16;
    static constexpr uint32_t MaxVolatileConstantBuffersPerLayout = 6;
    static constexpr uint32_t MaxVolatileConstantBuffers = 32;
    static constexpr uint32_t MaxPushConstantSize =
        128; // D3D12: root signature is 256 bytes max., Vulkan: 128 bytes of push constants guaranteed
    static constexpr uint32_t ConstantBufferOffsetSizeAlignment =
        256; // Partially bound constant buffers must have offsets aligned to this and sizes multiple of this

    //////////////////////////////////////////////////////////////////////////
    // Basic Types
    //////////////////////////////////////////////////////////////////////////

    struct Color
    {
        float r, g, b, a;

        Color() : r(0.f), g(0.f), b(0.f), a(0.f)
        {
        }
        Color(float c) : r(c), g(c), b(c), a(c)
        {
        }
        Color(float _r, float _g, float _b, float _a) : r(_r), g(_g), b(_b), a(_a)
        {
        }

        bool operator==(const Color &_b) const
        {
            return r == _b.r && g == _b.g && b == _b.b && a == _b.a;
        }
        bool operator!=(const Color &_b) const
        {
            return !(*this == _b);
        }
    };

    struct Viewport
    {
        float minX, maxX;
        float minY, maxY;
        float minZ, maxZ;

        Viewport() : minX(0.f), maxX(0.f), minY(0.f), maxY(0.f), minZ(0.f), maxZ(1.f)
        {
        }

        Viewport(float width, float height) : minX(0.f), maxX(width), minY(0.f), maxY(height), minZ(0.f), maxZ(1.f)
        {
        }

        Viewport(float _minX, float _maxX, float _minY, float _maxY, float _minZ, float _maxZ)
            : minX(_minX), maxX(_maxX), minY(_minY), maxY(_maxY), minZ(_minZ), maxZ(_maxZ)
        {
        }

        bool operator==(const Viewport &b) const
        {
            return minX == b.minX && minY == b.minY && minZ == b.minZ && maxX == b.maxX && maxY == b.maxY &&
                   maxZ == b.maxZ;
        }
        bool operator!=(const Viewport &b) const
        {
            return !(*this == b);
        }

        [[nodiscard]] float Width() const
        {
            return maxX - minX;
        }
        [[nodiscard]] float Height() const
        {
            return maxY - minY;
        }
    };

    struct Rect
    {
        int minX, maxX;
        int minY, maxY;

        Rect() : minX(0), maxX(0), minY(0), maxY(0)
        {
        }
        Rect(int width, int height) : minX(0), maxX(width), minY(0), maxY(height)
        {
        }
        Rect(int _minX, int _maxX, int _minY, int _maxY) : minX(_minX), maxX(_maxX), minY(_minY), maxY(_maxY)
        {
        }
        explicit Rect(const Viewport &viewport)
            : minX(int(floorf(viewport.minX))), maxX(int(ceilf(viewport.maxX))), minY(int(floorf(viewport.minY))),
              maxY(int(ceilf(viewport.maxY)))
        {
        }

        bool operator==(const Rect &b) const
        {
            return minX == b.minX && minY == b.minY && maxX == b.maxX && maxY == b.maxY;
        }
        bool operator!=(const Rect &b) const
        {
            return !(*this == b);
        }

        [[nodiscard]] int Width() const
        {
            return maxX - minX;
        }
        [[nodiscard]] int Height() const
        {
            return maxY - minY;
        }
    };

    enum class GraphicsAPI : uint8_t
    {
        D3D11,
        D3D12,
        VULKAN,
        OpenGL = 3
    };

    enum class Format : uint8_t
    {
        UNKNOWN,

        R8_UINT,
        R8_SINT,
        R8_UNORM,
        R8_SNORM,
        RG8_UINT,
        RG8_SINT,
        RG8_UNORM,
        RG8_SNORM,
        R16_UINT,
        R16_SINT,
        R16_UNORM,
        R16_SNORM,
        R16_FLOAT,
        BGRA4_UNORM,
        B5G6R5_UNORM,
        B5G5R5A1_UNORM,
        RGBA8_UINT,
        RGBA8_SINT,
        RGBA8_UNORM,
        RGBA8_SNORM,
        BGRA8_UNORM,
        BGRX8_UNORM,
        SRGBA8_UNORM,
        SBGRA8_UNORM,
        SBGRX8_UNORM,
        R10G10B10A2_UNORM,
        R11G11B10_FLOAT,
        RG16_UINT,
        RG16_SINT,
        RG16_UNORM,
        RG16_SNORM,
        RG16_FLOAT,
        R32_UINT,
        R32_SINT,
        R32_FLOAT,
        RGBA16_UINT,
        RGBA16_SINT,
        RGBA16_FLOAT,
        RGBA16_UNORM,
        RGBA16_SNORM,
        RG32_UINT,
        RG32_SINT,
        RG32_FLOAT,
        RGB32_UINT,
        RGB32_SINT,
        RGB32_FLOAT,
        RGBA32_UINT,
        RGBA32_SINT,
        RGBA32_FLOAT,

        D16,
        D24S8,
        X24G8_UINT,
        D32,
        D32S8,
        X32G8_UINT,

        BC1_UNORM,
        BC1_UNORM_SRGB,
        BC2_UNORM,
        BC2_UNORM_SRGB,
        BC3_UNORM,
        BC3_UNORM_SRGB,
        BC4_UNORM,
        BC4_SNORM,
        BC5_UNORM,
        BC5_SNORM,
        BC6H_UFLOAT,
        BC6H_SFLOAT,
        BC7_UNORM,
        BC7_UNORM_SRGB,

        COUNT,
    };

    enum class FormatKind : uint8_t
    {
        Integer,
        Normalized,
        Float,
        DepthStencil
    };

    struct FormatInfo
    {
        Format format;
        const char *name;
        uint8_t bytesPerBlock;
        uint8_t blockSize;
        FormatKind kind;
        bool hasRed : 1;
        bool hasGreen : 1;
        bool hasBlue : 1;
        bool hasAlpha : 1;
        bool hasDepth : 1;
        bool hasStencil : 1;
        bool isSigned : 1;
        bool isSRGB : 1;
    };

    PHYSARA_RHI_API const FormatInfo &GetFormatInfo(Format format);

    enum class FormatSupport : uint32_t
    {
        None = 0,

        Buffer = 0x00000001,
        IndexBuffer = 0x00000002,
        VertexBuffer = 0x00000004,

        Texture = 0x00000008,
        DepthStencil = 0x00000010,
        RenderTarget = 0x00000020,
        Blendable = 0x00000040,

        ShaderLoad = 0x00000080,
        ShaderSample = 0x00000100,
        ShaderUavLoad = 0x00000200,
        ShaderUavStore = 0x00000400,
        ShaderAtomic = 0x00000800,
    };

    PHYSARA_RHI_ENUM_CLASS_FLAG_OPERATORS(FormatSupport)

    //////////////////////////////////////////////////////////////////////////
    // Heap
    //////////////////////////////////////////////////////////////////////////

    enum class HeapType : uint8_t
    {
        DeviceLocal,
        Upload,
        Readback
    };

    struct HeapDesc
    {
        uint64_t capacity = 0;
        HeapType type;
        std::string debugName;

        constexpr HeapDesc &SetCapacity(uint64_t value)
        {
            capacity = value;
            return *this;
        }
        constexpr HeapDesc &SetType(HeapType value)
        {
            type = value;
            return *this;
        }
        HeapDesc &SetDebugName(const std::string &value)
        {
            debugName = value;
            return *this;
        }
    };

    class Heap : public Resource
    {
      public:
        virtual const HeapDesc &GetDesc() = 0;
    };

    typedef RefCountPtr<Heap> HeapHandle;

    struct MemoryRequirements
    {
        uint64_t size = 0;
        uint64_t alignment = 0;
    };

    //////////////////////////////////////////////////////////////////////////
    // Texture
    //////////////////////////////////////////////////////////////////////////

    enum class TextureDimension : uint8_t
    {
        Unknown,
        Texture1D,
        Texture1DArray,
        Texture2D,
        Texture2DArray,
        TextureCube,
        TextureCubeArray,
        Texture2DMS,
        Texture2DMSArray,
        Texture3D
    };

    enum class CpuAccessMode : uint8_t
    {
        None,
        Read,
        Write
    };

    enum class ResourceStates : uint32_t
    {
        Unknown = 0,
        Common = 0x00000001,
        ConstantBuffer = 0x00000002,
        VertexBuffer = 0x00000004,
        IndexBuffer = 0x00000008,
        IndirectArgument = 0x00000010,
        PixelShaderResource = 0x00000020,
        NonPixelShaderResource = 0x00000040,
        ShaderResource = PixelShaderResource | NonPixelShaderResource,
        UnorderedAccess = 0x00000080,
        RenderTarget = 0x00000100,
        DepthWrite = 0x00000200,
        DepthRead = 0x00000400,
        StreamOut = 0x00000800,
        CopyDest = 0x00001000,
        CopySource = 0x00002000,
        ResolveDest = 0x00004000,
        ResolveSource = 0x00008000,
        Present = 0x00010000,
        AccelStructRead = 0x00020000,
        AccelStructWrite = 0x00040000,
        AccelStructBuildInput = 0x00080000,
        AccelStructBuildBlas = 0x00100000,
        ShadingRateSurface = 0x00200000,
        OpacityMicromapWrite = 0x00400000,
        OpacityMicromapBuildInput = 0x00800000,
        ConvertCoopVecMatrixInput = 0x01000000,
        ConvertCoopVecMatrixOutput = 0x02000000,
    };

    PHYSARA_RHI_ENUM_CLASS_FLAG_OPERATORS(ResourceStates)

    typedef uint32_t MipLevel;
    typedef uint32_t ArraySlice;

    // Flags for resources that need to be shared with other graphics APIs or other GPU devices.
    enum class SharedResourceFlags : uint32_t
    {
        None = 0,

        // D3D11: adds D3D11_RESOURCE_MISC_SHARED
        // D3D12: adds D3D12_HEAP_FLAG_SHARED
        // Vulkan: adds vk::ExternalMemoryImageCreateInfo and vk::ExportMemoryAllocateInfo/vk::ExternalMemoryBufferCreateInfo
        Shared = 0x01,

        // D3D11: adds (D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX | D3D11_RESOURCE_MISC_SHARED_NTHANDLE)
        // D3D12, Vulkan: ignored
        Shared_NTHandle = 0x02,

        // D3D12: adds D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER and D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER
        // D3D11, Vulkan: ignored
        Shared_CrossAdapter = 0x04,
    };

    PHYSARA_RHI_ENUM_CLASS_FLAG_OPERATORS(SharedResourceFlags)

    // Per-output-component source selector for an SRV, mirroring
    // D3D12_SHADER_COMPONENT_MAPPING / VkComponentSwizzle. For each shader-visible
    // component (r,g,b,a) it names which source channel it reads (or a constant).
    enum class ComponentSwizzle : uint8_t
    {
        R = 0,
        G = 1,
        B = 2,
        A = 3,
        Zero = 4,
        One = 5,
    };

    // A 4-channel SRV component mapping. The default is identity (r<-R, g<-G, ...),
    // which every backend encodes to its native "default mapping" so behavior is
    // bit-identical to no swizzle. Used to reinterpret a texture's channels at the
    // view level, e.g. to sample a BC5 (2-channel) map as if it were a packed
    // RGBA layout. NOTE: D3D11 SRVs have no component-mapping field, so a
    // non-identity mapping is unsupported there (the backend warns and ignores it).
    struct ComponentMapping
    {
        ComponentSwizzle r = ComponentSwizzle::R;
        ComponentSwizzle g = ComponentSwizzle::G;
        ComponentSwizzle b = ComponentSwizzle::B;
        ComponentSwizzle a = ComponentSwizzle::A;

        constexpr bool IsIdentity() const
        {
            return r == ComponentSwizzle::R && g == ComponentSwizzle::G && b == ComponentSwizzle::B &&
                   a == ComponentSwizzle::A;
        }
        constexpr bool operator==(const ComponentMapping &o) const
        {
            return r == o.r && g == o.g && b == o.b && a == o.a;
        }
        constexpr bool operator!=(const ComponentMapping &o) const
        {
            return !(*this == o);
        }

        // Pack into 16 bits (3 bits per channel) for compact storage in BindingSetItem.
        constexpr uint16_t Pack() const
        {
            return uint16_t(uint32_t(r) | (uint32_t(g) << 3) | (uint32_t(b) << 6) | (uint32_t(a) << 9));
        }
        static constexpr ComponentMapping Unpack(uint16_t v)
        {
            return ComponentMapping{ComponentSwizzle(v & 0x7), ComponentSwizzle((v >> 3) & 0x7),
                                    ComponentSwizzle((v >> 6) & 0x7), ComponentSwizzle((v >> 9) & 0x7)};
        }

        constexpr ComponentMapping &SetR(ComponentSwizzle v)
        {
            r = v;
            return *this;
        }
        constexpr ComponentMapping &SetG(ComponentSwizzle v)
        {
            g = v;
            return *this;
        }
        constexpr ComponentMapping &SetB(ComponentSwizzle v)
        {
            b = v;
            return *this;
        }
        constexpr ComponentMapping &SetA(ComponentSwizzle v)
        {
            a = v;
            return *this;
        }
    };

    // pack() only uses bits 0-11, so bit 15 marks a BindingSetItem's mapping as
    // explicitly set rather than inherited from the texture.
    constexpr uint16_t ComponentMappingExplicit = 0x8000;

    // Encodes a BindingSetItem override; std::nullopt means "inherit the texture's default".
    constexpr uint16_t PackComponentMapping(std::optional<ComponentMapping> mapping)
    {
        return mapping ? uint16_t(mapping->Pack() | ComponentMappingExplicit) : uint16_t(0);
    }

    // An explicit binding-level mapping replaces the texture's default, which lets a
    // binding request identity even when the texture carries a non-identity default.
    constexpr ComponentMapping ResolveComponentMapping(uint16_t overrideMapping, const ComponentMapping &textureDefault)
    {
        return (overrideMapping & ComponentMappingExplicit) ? ComponentMapping::Unpack(overrideMapping)
                                                            : textureDefault;
    }

    constexpr ComponentMapping ResolveComponentMapping(std::optional<ComponentMapping> overrideMapping,
                                                       const ComponentMapping &textureDefault)
    {
        return overrideMapping ? *overrideMapping : textureDefault;
    }

    struct TextureDesc
    {
        uint32_t width = 1;
        uint32_t height = 1;
        uint32_t depth = 1;
        uint32_t arraySize = 1;
        uint32_t mipLevels = 1;
        uint32_t sampleCount = 1;
        uint32_t sampleQuality = 0;
        Format format = Format::UNKNOWN;
        TextureDimension dimension = TextureDimension::Texture2D;
        std::string debugName;

        // Default SRV component mapping for views of this texture. Identity unless
        // an SRV BindingSetItem overrides it. Lets a texture carry a channel
        // reinterpretation (e.g. BC5 packed as RGBA) without every binding site
        // having to specify it.
        ComponentMapping defaultComponentMapping;

        bool isShaderResource = true; // Note: isShaderResource is initialized to 'true' for backward compatibility
        bool isRenderTarget = false;
        bool isUAV = false;
        bool isTypeless = false;
        bool isShadingRateSurface = false;

        SharedResourceFlags sharedResourceFlags = SharedResourceFlags::None;

        // Indicates that the texture is created with no backing memory,
        // and memory is bound to the texture later using bindTextureMemory.
        // On DX12, the texture resource is created at the time of memory binding.
        bool isVirtual = false;
        bool isTiled = false;

        Color clearValue;
        bool useClearValue = false;

        ResourceStates initialState = ResourceStates::Unknown;

        // If keepInitialState is true, command lists that use the texture will automatically
        // begin tracking the texture from the initial state and transition it to the initial state
        // on command list close.
        bool keepInitialState = false;

        constexpr TextureDesc &SetWidth(uint32_t value)
        {
            width = value;
            return *this;
        }
        constexpr TextureDesc &SetHeight(uint32_t value)
        {
            height = value;
            return *this;
        }
        constexpr TextureDesc &SetDepth(uint32_t value)
        {
            depth = value;
            return *this;
        }
        constexpr TextureDesc &SetArraySize(uint32_t value)
        {
            arraySize = value;
            return *this;
        }
        constexpr TextureDesc &SetMipLevels(uint32_t value)
        {
            mipLevels = value;
            return *this;
        }
        constexpr TextureDesc &SetSampleCount(uint32_t value)
        {
            sampleCount = value;
            return *this;
        }
        constexpr TextureDesc &SetSampleQuality(uint32_t value)
        {
            sampleQuality = value;
            return *this;
        }
        constexpr TextureDesc &SetFormat(Format value)
        {
            format = value;
            return *this;
        }
        constexpr TextureDesc &SetDefaultComponentMapping(ComponentMapping value)
        {
            defaultComponentMapping = value;
            return *this;
        }
        constexpr TextureDesc &SetDimension(TextureDimension value)
        {
            dimension = value;
            return *this;
        }
        TextureDesc &SetDebugName(const std::string &value)
        {
            debugName = value;
            return *this;
        }
        constexpr TextureDesc &SetIsRenderTarget(bool value)
        {
            isRenderTarget = value;
            return *this;
        }
        constexpr TextureDesc &SetIsUAV(bool value)
        {
            isUAV = value;
            return *this;
        }
        constexpr TextureDesc &SetIsTypeless(bool value)
        {
            isTypeless = value;
            return *this;
        }
        constexpr TextureDesc &SetIsVirtual(bool value)
        {
            isVirtual = value;
            return *this;
        }
        constexpr TextureDesc &SetClearValue(const Color &value)
        {
            clearValue = value;
            useClearValue = true;
            return *this;
        }
        constexpr TextureDesc &SetUseClearValue(bool value)
        {
            useClearValue = value;
            return *this;
        }
        constexpr TextureDesc &SetInitialState(ResourceStates value)
        {
            initialState = value;
            return *this;
        }
        constexpr TextureDesc &SetKeepInitialState(bool value)
        {
            keepInitialState = value;
            return *this;
        }
        constexpr TextureDesc &SetSharedResourceFlags(SharedResourceFlags value)
        {
            sharedResourceFlags = value;
            return *this;
        }

        // Equivalent to .setInitialState(_initialState).setKeepInitialState(true)
        constexpr TextureDesc &EnableAutomaticStateTracking(ResourceStates _initialState)
        {
            initialState = _initialState;
            keepInitialState = true;
            return *this;
        }
    };

    // Describes a 2D or 3D section of a single mip level, single array slice of a texture.
    struct TextureSlice
    {
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t z = 0;
        // -1 means the entire dimension is part of the region
        // resolve() below will translate these values into actual dimensions
        uint32_t width = uint32_t(-1);
        uint32_t height = uint32_t(-1);
        uint32_t depth = uint32_t(-1);

        MipLevel mipLevel = 0;
        ArraySlice arraySlice = 0;

        // 前提: desc 有效, mipLevel<desc.mipLevels 且<32; 外部输入由 Validation/业务边界先校验.
        [[nodiscard]] PHYSARA_RHI_API TextureSlice Resolve(const TextureDesc &desc) const;

        constexpr TextureSlice &SetOrigin(uint32_t vx = 0, uint32_t vy = 0, uint32_t vz = 0)
        {
            x = vx;
            y = vy;
            z = vz;
            return *this;
        }
        constexpr TextureSlice &SetWidth(uint32_t value)
        {
            width = value;
            return *this;
        }
        constexpr TextureSlice &SetHeight(uint32_t value)
        {
            height = value;
            return *this;
        }
        constexpr TextureSlice &SetDepth(uint32_t value)
        {
            depth = value;
            return *this;
        }
        constexpr TextureSlice &SetSize(uint32_t vx = uint32_t(-1), uint32_t vy = uint32_t(-1),
                                        uint32_t vz = uint32_t(-1))
        {
            width = vx;
            height = vy;
            depth = vz;
            return *this;
        }
        constexpr TextureSlice &SetMipLevel(MipLevel level)
        {
            mipLevel = level;
            return *this;
        }
        constexpr TextureSlice &SetArraySlice(ArraySlice slice)
        {
            arraySlice = slice;
            return *this;
        }
    };

    struct TextureSubresourceSet
    {
        static constexpr MipLevel AllMipLevels = MipLevel(-1);
        static constexpr ArraySlice AllArraySlices = ArraySlice(-1);

        MipLevel baseMipLevel = 0;
        MipLevel numMipLevels = 1;
        ArraySlice baseArraySlice = 0;
        ArraySlice numArraySlices = 1;

        TextureSubresourceSet() = default;

        TextureSubresourceSet(MipLevel _baseMipLevel, MipLevel _numMipLevels, ArraySlice _baseArraySlice,
                              ArraySlice _numArraySlices)
            : baseMipLevel(_baseMipLevel), numMipLevels(_numMipLevels), baseArraySlice(_baseArraySlice),
              numArraySlices(_numArraySlices)
        {
        }

        [[nodiscard]] PHYSARA_RHI_API TextureSubresourceSet Resolve(const TextureDesc &desc, bool singleMipLevel) const;
        [[nodiscard]] PHYSARA_RHI_API bool IsEntireTexture(const TextureDesc &desc) const;

        bool operator==(const TextureSubresourceSet &other) const
        {
            return baseMipLevel == other.baseMipLevel && numMipLevels == other.numMipLevels &&
                   baseArraySlice == other.baseArraySlice && numArraySlices == other.numArraySlices;
        }
        bool operator!=(const TextureSubresourceSet &other) const
        {
            return !(*this == other);
        }

        constexpr TextureSubresourceSet &SetBaseMipLevel(MipLevel value)
        {
            baseMipLevel = value;
            return *this;
        }
        constexpr TextureSubresourceSet &SetNumMipLevels(MipLevel value)
        {
            numMipLevels = value;
            return *this;
        }
        constexpr TextureSubresourceSet &SetMipLevels(MipLevel base, MipLevel num)
        {
            baseMipLevel = base;
            numMipLevels = num;
            return *this;
        }
        constexpr TextureSubresourceSet &SetBaseArraySlice(ArraySlice value)
        {
            baseArraySlice = value;
            return *this;
        }
        constexpr TextureSubresourceSet &SetNumArraySlices(ArraySlice value)
        {
            numArraySlices = value;
            return *this;
        }
        constexpr TextureSubresourceSet &SetArraySlices(ArraySlice base, ArraySlice num)
        {
            baseArraySlice = base;
            numArraySlices = num;
            return *this;
        }

        // see the bottom of this file for a specialization of std::hash<TextureSubresourceSet>
    };

    static const TextureSubresourceSet AllSubresources =
        TextureSubresourceSet(0, TextureSubresourceSet::AllMipLevels, 0, TextureSubresourceSet::AllArraySlices);

    class Texture : public Resource
    {
      public:
        [[nodiscard]] virtual const TextureDesc &GetDesc() const = 0;

        // Similar to getNativeObject, returns a native view for a specified set of subresources. Returns nullptr if unavailable.
        // TODO: on D3D12, the views might become invalid later if the view heap is grown/reallocated, we should do something about that.
        // 'overrideComponentMapping' applies to SRV object types only; std::nullopt uses the texture's defaultComponentMapping.
        virtual Object GetNativeView(ObjectType objectType, Format format = Format::UNKNOWN,
                                     TextureSubresourceSet subresources = AllSubresources,
                                     TextureDimension dimension = TextureDimension::Unknown, bool isReadOnlyDSV = false,
                                     std::optional<ComponentMapping> overrideComponentMapping = std::nullopt) = 0;
    };
    typedef RefCountPtr<Texture> TextureHandle;

    class StagingTexture : public Resource
    {
      public:
        [[nodiscard]] virtual const TextureDesc &GetDesc() const = 0;
    };
    typedef RefCountPtr<StagingTexture> StagingTextureHandle;

    struct TiledTextureCoordinate
    {
        uint16_t mipLevel = 0;
        uint16_t arrayLevel = 0;
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t z = 0;
    };

    struct TiledTextureRegion
    {
        uint32_t tilesNum = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t depth = 0;
    };

    struct TextureTilesMapping
    {
        TiledTextureCoordinate *tiledTextureCoordinates = nullptr;
        TiledTextureRegion *tiledTextureRegions = nullptr;
        uint64_t *byteOffsets = nullptr;
        uint32_t numTextureRegions = 0;
        Heap *heap = nullptr;
    };

    struct PackedMipDesc
    {
        uint32_t numStandardMips = 0;
        uint32_t numPackedMips = 0;
        uint32_t numTilesForPackedMips = 0;
        uint32_t startTileIndexInOverallResource = 0;
    };

    struct TileShape
    {
        uint32_t widthInTexels = 0;
        uint32_t heightInTexels = 0;
        uint32_t depthInTexels = 0;
    };

    struct SubresourceTiling
    {
        uint32_t widthInTiles = 0;
        uint32_t heightInTiles = 0;
        uint32_t depthInTiles = 0;
        uint32_t startTileIndexInOverallResource = 0;
    };

    enum SamplerFeedbackFormat : uint8_t
    {
        MinMipOpaque = 0x0,
        MipRegionUsedOpaque = 0x1,
    };

    struct SamplerFeedbackTextureDesc
    {
        SamplerFeedbackFormat samplerFeedbackFormat = SamplerFeedbackFormat::MinMipOpaque;
        uint32_t samplerFeedbackMipRegionX = 0;
        uint32_t samplerFeedbackMipRegionY = 0;
        uint32_t samplerFeedbackMipRegionZ = 0;
        ResourceStates initialState = ResourceStates::Unknown;
        bool keepInitialState = false;
    };

    class SamplerFeedbackTexture : public Resource
    {
      public:
        [[nodiscard]] virtual const SamplerFeedbackTextureDesc &GetDesc() const = 0;
        virtual TextureHandle GetPairedTexture() = 0;
    };
    typedef RefCountPtr<SamplerFeedbackTexture> SamplerFeedbackTextureHandle;

    //////////////////////////////////////////////////////////////////////////
    // Input Layout
    //////////////////////////////////////////////////////////////////////////

    struct VertexAttributeDesc
    {
        std::string name;
        Format format = Format::UNKNOWN;
        uint32_t arraySize = 1;
        uint32_t bufferIndex = 0;
        uint32_t offset = 0;
        // note: for most APIs, all strides for a given bufferIndex must be identical
        uint32_t elementStride = 0;
        bool isInstanced = false;

        VertexAttributeDesc &SetName(const std::string &value)
        {
            name = value;
            return *this;
        }
        constexpr VertexAttributeDesc &SetFormat(Format value)
        {
            format = value;
            return *this;
        }
        constexpr VertexAttributeDesc &SetArraySize(uint32_t value)
        {
            arraySize = value;
            return *this;
        }
        constexpr VertexAttributeDesc &SetBufferIndex(uint32_t value)
        {
            bufferIndex = value;
            return *this;
        }
        constexpr VertexAttributeDesc &SetOffset(uint32_t value)
        {
            offset = value;
            return *this;
        }
        constexpr VertexAttributeDesc &SetElementStride(uint32_t value)
        {
            elementStride = value;
            return *this;
        }
        constexpr VertexAttributeDesc &SetIsInstanced(bool value)
        {
            isInstanced = value;
            return *this;
        }
    };

    class InputLayout : public Resource
    {
      public:
        [[nodiscard]] virtual uint32_t GetNumAttributes() const = 0;
        [[nodiscard]] virtual const VertexAttributeDesc *GetAttributeDesc(uint32_t index) const = 0;
    };

    typedef RefCountPtr<InputLayout> InputLayoutHandle;

    //////////////////////////////////////////////////////////////////////////
    // Buffer
    //////////////////////////////////////////////////////////////////////////

    struct BufferDesc
    {
        uint64_t byteSize = 0;
        uint32_t structStride = 0; // if non-zero it's structured
        uint32_t maxVersions = 0;  // only valid and required to be nonzero for volatile buffers on Vulkan
        std::string debugName;
        Format format = Format::UNKNOWN; // for typed buffer views
        bool canHaveUAVs = false;
        bool canHaveTypedViews = false;
        bool canHaveRawViews = false;
        bool isVertexBuffer = false;
        bool isIndexBuffer = false;
        bool isConstantBuffer = false;
        bool isDrawIndirectArgs = false;
        bool isAccelStructBuildInput = false;
        bool isAccelStructStorage = false;
        bool isShaderBindingTable = false;

        // A dynamic/upload buffer whose contents only live in the current command list
        bool isVolatile = false;

        // Indicates that the buffer is created with no backing memory,
        // and memory is bound to the buffer later using bindBufferMemory.
        // On DX12, the buffer resource is created at the time of memory binding.
        bool isVirtual = false;

        ResourceStates initialState = ResourceStates::Common;

        // see TextureDesc::keepInitialState
        bool keepInitialState = false;

        CpuAccessMode cpuAccess = CpuAccessMode::None;

        SharedResourceFlags sharedResourceFlags = SharedResourceFlags::None;

        constexpr BufferDesc &SetByteSize(uint64_t value)
        {
            byteSize = value;
            return *this;
        }
        constexpr BufferDesc &SetStructStride(uint32_t value)
        {
            structStride = value;
            return *this;
        }
        constexpr BufferDesc &SetMaxVersions(uint32_t value)
        {
            maxVersions = value;
            return *this;
        }
        BufferDesc &SetDebugName(const std::string &value)
        {
            debugName = value;
            return *this;
        }
        constexpr BufferDesc &SetFormat(Format value)
        {
            format = value;
            return *this;
        }
        constexpr BufferDesc &SetCanHaveUAVs(bool value)
        {
            canHaveUAVs = value;
            return *this;
        }
        constexpr BufferDesc &SetCanHaveTypedViews(bool value)
        {
            canHaveTypedViews = value;
            return *this;
        }
        constexpr BufferDesc &SetCanHaveRawViews(bool value)
        {
            canHaveRawViews = value;
            return *this;
        }
        constexpr BufferDesc &SetIsVertexBuffer(bool value)
        {
            isVertexBuffer = value;
            return *this;
        }
        constexpr BufferDesc &SetIsIndexBuffer(bool value)
        {
            isIndexBuffer = value;
            return *this;
        }
        constexpr BufferDesc &SetIsConstantBuffer(bool value)
        {
            isConstantBuffer = value;
            return *this;
        }
        constexpr BufferDesc &SetIsDrawIndirectArgs(bool value)
        {
            isDrawIndirectArgs = value;
            return *this;
        }
        constexpr BufferDesc &SetIsAccelStructBuildInput(bool value)
        {
            isAccelStructBuildInput = value;
            return *this;
        }
        constexpr BufferDesc &SetIsAccelStructStorage(bool value)
        {
            isAccelStructStorage = value;
            return *this;
        }
        constexpr BufferDesc &SetIsShaderBindingTable(bool value)
        {
            isShaderBindingTable = value;
            return *this;
        }
        constexpr BufferDesc &SetIsVolatile(bool value)
        {
            isVolatile = value;
            return *this;
        }
        constexpr BufferDesc &SetIsVirtual(bool value)
        {
            isVirtual = value;
            return *this;
        }
        constexpr BufferDesc &SetInitialState(ResourceStates value)
        {
            initialState = value;
            return *this;
        }
        constexpr BufferDesc &SetKeepInitialState(bool value)
        {
            keepInitialState = value;
            return *this;
        }
        constexpr BufferDesc &SetCpuAccess(CpuAccessMode value)
        {
            cpuAccess = value;
            return *this;
        }

        // Equivalent to .setInitialState(_initialState).setKeepInitialState(true)
        constexpr BufferDesc &EnableAutomaticStateTracking(ResourceStates _initialState)
        {
            initialState = _initialState;
            keepInitialState = true;
            return *this;
        }
    };

    struct BufferRange
    {
        uint64_t byteOffset = 0;
        uint64_t byteSize = 0;

        BufferRange() = default;

        BufferRange(uint64_t _byteOffset, uint64_t _byteSize) : byteOffset(_byteOffset), byteSize(_byteSize)
        {
        }

        [[nodiscard]] PHYSARA_RHI_API BufferRange Resolve(const BufferDesc &desc) const;
        [[nodiscard]] constexpr bool IsEntireBuffer(const BufferDesc &desc) const
        {
            return (byteOffset == 0) && (byteSize == ~0ull || byteSize == desc.byteSize);
        }
        constexpr bool operator==(const BufferRange &other) const
        {
            return byteOffset == other.byteOffset && byteSize == other.byteSize;
        }

        constexpr BufferRange &SetByteOffset(uint64_t value)
        {
            byteOffset = value;
            return *this;
        }
        constexpr BufferRange &SetByteSize(uint64_t value)
        {
            byteSize = value;
            return *this;
        }
    };

    static const BufferRange EntireBuffer = BufferRange(0, ~0ull);

    class Buffer : public Resource
    {
      public:
        [[nodiscard]] virtual const BufferDesc &GetDesc() const = 0;
        [[nodiscard]] virtual GpuVirtualAddress GetGpuVirtualAddress() const = 0;
    };

    typedef RefCountPtr<Buffer> BufferHandle;

    //////////////////////////////////////////////////////////////////////////
    // Shader
    //////////////////////////////////////////////////////////////////////////

    // Shader type mask. The values match ones used in Vulkan.
    enum class ShaderType : uint16_t
    {
        None = 0x0000,

        Compute = 0x0020,

        Vertex = 0x0001,
        Hull = 0x0002,
        Domain = 0x0004,
        Geometry = 0x0008,
        Pixel = 0x0010,
        Amplification = 0x0040,
        Mesh = 0x0080,
        AllGraphics = 0x00DF,

        RayGeneration = 0x0100,
        AnyHit = 0x0200,
        ClosestHit = 0x0400,
        Miss = 0x0800,
        Intersection = 0x1000,
        Callable = 0x2000,
        AllRayTracing = 0x3F00,

        All = 0x3FFF,
    };

    PHYSARA_RHI_ENUM_CLASS_FLAG_OPERATORS(ShaderType)

    enum class FastGeometryShaderFlags : uint8_t
    {
        ForceFastGS = 0x01,
        UseViewportMask = 0x02,
        OffsetTargetIndexByViewportIndex = 0x04,
        StrictApiOrder = 0x08
    };

    PHYSARA_RHI_ENUM_CLASS_FLAG_OPERATORS(FastGeometryShaderFlags)

    struct CustomSemantic
    {
        enum Type
        {
            Undefined = 0,
            XRight = 1,
            ViewportMask = 2
        };

        Type type;
        std::string name;

        constexpr CustomSemantic &SetType(Type value)
        {
            type = value;
            return *this;
        }
        CustomSemantic &SetName(const std::string &value)
        {
            name = value;
            return *this;
        }
    };

    struct ShaderDesc
    {
        ShaderType shaderType = ShaderType::None;
        std::string debugName;
        std::string entryName = "main";

        int hlslExtensionsUAV = -1;

        bool useSpecificShaderExt = false;
        uint32_t numCustomSemantics = 0;
        CustomSemantic *pCustomSemantics = nullptr;

        FastGeometryShaderFlags fastGSFlags = FastGeometryShaderFlags(0);
        uint32_t *pCoordinateSwizzling = nullptr;

        constexpr ShaderDesc &SetShaderType(ShaderType value)
        {
            shaderType = value;
            return *this;
        }
        ShaderDesc &SetDebugName(const std::string &value)
        {
            debugName = value;
            return *this;
        }
        ShaderDesc &SetEntryName(const std::string &value)
        {
            entryName = value;
            return *this;
        }
        constexpr ShaderDesc &SetHlslExtensionsUAV(int value)
        {
            hlslExtensionsUAV = value;
            return *this;
        }
        constexpr ShaderDesc &SetUseSpecificShaderExt(bool value)
        {
            useSpecificShaderExt = value;
            return *this;
        }
        constexpr ShaderDesc &SetCustomSemantics(uint32_t count, CustomSemantic *data)
        {
            numCustomSemantics = count;
            pCustomSemantics = data;
            return *this;
        }
        constexpr ShaderDesc &SetFastGSFlags(FastGeometryShaderFlags value)
        {
            fastGSFlags = value;
            return *this;
        }
        constexpr ShaderDesc &SetCoordinateSwizzling(uint32_t *value)
        {
            pCoordinateSwizzling = value;
            return *this;
        }
    };

    struct ShaderSpecialization
    {
        uint32_t constantID = 0;
        union
        {
            uint32_t u = 0;
            int32_t i;
            float f;
        } value;

        static ShaderSpecialization UInt32(uint32_t constantID, uint32_t u)
        {
            ShaderSpecialization s;
            s.constantID = constantID;
            s.value.u = u;
            return s;
        }

        static ShaderSpecialization Int32(uint32_t constantID, int32_t i)
        {
            ShaderSpecialization s;
            s.constantID = constantID;
            s.value.i = i;
            return s;
        }

        static ShaderSpecialization Float(uint32_t constantID, float f)
        {
            ShaderSpecialization s;
            s.constantID = constantID;
            s.value.f = f;
            return s;
        }
    };

    class Shader : public Resource
    {
      public:
        [[nodiscard]] virtual const ShaderDesc &GetDesc() const = 0;
        virtual void GetBytecode(const void **ppBytecode, size_t *pSize) const = 0;
    };

    typedef RefCountPtr<Shader> ShaderHandle;

    //////////////////////////////////////////////////////////////////////////
    // Shader Library
    //////////////////////////////////////////////////////////////////////////

    class ShaderLibrary : public Resource
    {
      public:
        virtual void GetBytecode(const void **ppBytecode, size_t *pSize) const = 0;
        virtual ShaderHandle GetShader(const char *entryName, ShaderType shaderType) = 0;
    };

    typedef RefCountPtr<ShaderLibrary> ShaderLibraryHandle;

    //////////////////////////////////////////////////////////////////////////
    // Blend State
    //////////////////////////////////////////////////////////////////////////

    enum class BlendFactor : uint8_t
    {
        Zero = 1,
        One = 2,
        SrcColor = 3,
        InvSrcColor = 4,
        SrcAlpha = 5,
        InvSrcAlpha = 6,
        DstAlpha = 7,
        InvDstAlpha = 8,
        DstColor = 9,
        InvDstColor = 10,
        SrcAlphaSaturate = 11,
        ConstantColor = 14,
        InvConstantColor = 15,
        Src1Color = 16,
        InvSrc1Color = 17,
        Src1Alpha = 18,
        InvSrc1Alpha = 19,

        // Vulkan names
        OneMinusSrcColor = InvSrcColor,
        OneMinusSrcAlpha = InvSrcAlpha,
        OneMinusDstAlpha = InvDstAlpha,
        OneMinusDstColor = InvDstColor,
        OneMinusConstantColor = InvConstantColor,
        OneMinusSrc1Color = InvSrc1Color,
        OneMinusSrc1Alpha = InvSrc1Alpha,
    };

    enum class BlendOp : uint8_t
    {
        Add = 1,
        Subtract = 2,
        ReverseSubtract = 3,
        Min = 4,
        Max = 5
    };

    enum class ColorMask : uint8_t
    {
        // These values are equal to their counterparts in DX11, DX12, and Vulkan.
        Red = 1,
        Green = 2,
        Blue = 4,
        Alpha = 8,
        All = 0xF
    };

    PHYSARA_RHI_ENUM_CLASS_FLAG_OPERATORS(ColorMask)

    struct BlendState
    {
        struct RenderTarget
        {
            bool blendEnable = false;
            BlendFactor srcBlend = BlendFactor::One;
            BlendFactor destBlend = BlendFactor::Zero;
            BlendOp blendOp = BlendOp::Add;
            BlendFactor srcBlendAlpha = BlendFactor::One;
            BlendFactor destBlendAlpha = BlendFactor::Zero;
            BlendOp blendOpAlpha = BlendOp::Add;
            ColorMask colorWriteMask = ColorMask::All;

            constexpr RenderTarget &SetBlendEnable(bool enable)
            {
                blendEnable = enable;
                return *this;
            }
            constexpr RenderTarget &EnableBlend()
            {
                blendEnable = true;
                return *this;
            }
            constexpr RenderTarget &DisableBlend()
            {
                blendEnable = false;
                return *this;
            }
            constexpr RenderTarget &SetSrcBlend(BlendFactor value)
            {
                srcBlend = value;
                return *this;
            }
            constexpr RenderTarget &SetDestBlend(BlendFactor value)
            {
                destBlend = value;
                return *this;
            }
            constexpr RenderTarget &SetBlendOp(BlendOp value)
            {
                blendOp = value;
                return *this;
            }
            constexpr RenderTarget &SetSrcBlendAlpha(BlendFactor value)
            {
                srcBlendAlpha = value;
                return *this;
            }
            constexpr RenderTarget &SetDestBlendAlpha(BlendFactor value)
            {
                destBlendAlpha = value;
                return *this;
            }
            constexpr RenderTarget &SetBlendOpAlpha(BlendOp value)
            {
                blendOpAlpha = value;
                return *this;
            }
            constexpr RenderTarget &SetColorWriteMask(ColorMask value)
            {
                colorWriteMask = value;
                return *this;
            }

            [[nodiscard]] PHYSARA_RHI_API bool UsesConstantColor() const;

            constexpr bool operator==(const RenderTarget &other) const
            {
                return blendEnable == other.blendEnable && srcBlend == other.srcBlend && destBlend == other.destBlend &&
                       blendOp == other.blendOp && srcBlendAlpha == other.srcBlendAlpha &&
                       destBlendAlpha == other.destBlendAlpha && blendOpAlpha == other.blendOpAlpha &&
                       colorWriteMask == other.colorWriteMask;
            }

            constexpr bool operator!=(const RenderTarget &other) const
            {
                return !(*this == other);
            }
        };

        RenderTarget targets[MaxRenderTargets];
        bool alphaToCoverageEnable = false;

        constexpr BlendState &SetRenderTarget(uint32_t index, const RenderTarget &target)
        {
            targets[index] = target;
            return *this;
        }
        constexpr BlendState &SetAlphaToCoverageEnable(bool enable)
        {
            alphaToCoverageEnable = enable;
            return *this;
        }
        constexpr BlendState &EnableAlphaToCoverage()
        {
            alphaToCoverageEnable = true;
            return *this;
        }
        constexpr BlendState &DisableAlphaToCoverage()
        {
            alphaToCoverageEnable = false;
            return *this;
        }

        [[nodiscard]] bool UsesConstantColor(uint32_t numTargets) const;

        constexpr bool operator==(const BlendState &other) const
        {
            if (alphaToCoverageEnable != other.alphaToCoverageEnable)
            {
                return false;
            }

            for (uint32_t i = 0; i < MaxRenderTargets; ++i)
            {
                if (targets[i] != other.targets[i])
                {
                    return false;
                }
            }

            return true;
        }

        constexpr bool operator!=(const BlendState &other) const
        {
            return !(*this == other);
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // Raster State
    //////////////////////////////////////////////////////////////////////////

    enum class RasterFillMode : uint8_t
    {
        Solid,
        Wireframe,

        // Vulkan names
        Fill = Solid,
        Line = Wireframe
    };

    enum class RasterCullMode : uint8_t
    {
        Back,
        Front,
        None
    };

    struct RasterState
    {
        RasterFillMode fillMode = RasterFillMode::Solid;
        RasterCullMode cullMode = RasterCullMode::Back;
        bool frontCounterClockwise = false;
        bool depthClipEnable = false;
        bool scissorEnable = false;
        bool multisampleEnable = false;
        bool antialiasedLineEnable = false;
        int depthBias = 0;
        float depthBiasClamp = 0.f;
        float slopeScaledDepthBias = 0.f;

        // Extended rasterizer state supported by Maxwell
        // In D3D11, use NvAPI_D3D11_CreateRasterizerState to create such rasterizer state.
        uint8_t forcedSampleCount = 0;
        bool programmableSamplePositionsEnable = false;
        bool conservativeRasterEnable = false;
        bool quadFillEnable = false;
        char samplePositionsX[16]{};
        char samplePositionsY[16]{};

        constexpr RasterState &SetFillMode(RasterFillMode value)
        {
            fillMode = value;
            return *this;
        }
        constexpr RasterState &SetFillSolid()
        {
            fillMode = RasterFillMode::Solid;
            return *this;
        }
        constexpr RasterState &SetFillWireframe()
        {
            fillMode = RasterFillMode::Wireframe;
            return *this;
        }
        constexpr RasterState &SetCullMode(RasterCullMode value)
        {
            cullMode = value;
            return *this;
        }
        constexpr RasterState &SetCullBack()
        {
            cullMode = RasterCullMode::Back;
            return *this;
        }
        constexpr RasterState &SetCullFront()
        {
            cullMode = RasterCullMode::Front;
            return *this;
        }
        constexpr RasterState &SetCullNone()
        {
            cullMode = RasterCullMode::None;
            return *this;
        }
        constexpr RasterState &SetFrontCounterClockwise(bool value)
        {
            frontCounterClockwise = value;
            return *this;
        }
        constexpr RasterState &SetDepthClipEnable(bool value)
        {
            depthClipEnable = value;
            return *this;
        }
        constexpr RasterState &EnableDepthClip()
        {
            depthClipEnable = true;
            return *this;
        }
        constexpr RasterState &DisableDepthClip()
        {
            depthClipEnable = false;
            return *this;
        }
        constexpr RasterState &SetScissorEnable(bool value)
        {
            scissorEnable = value;
            return *this;
        }
        constexpr RasterState &EnableScissor()
        {
            scissorEnable = true;
            return *this;
        }
        constexpr RasterState &DisableScissor()
        {
            scissorEnable = false;
            return *this;
        }
        constexpr RasterState &SetMultisampleEnable(bool value)
        {
            multisampleEnable = value;
            return *this;
        }
        constexpr RasterState &EnableMultisample()
        {
            multisampleEnable = true;
            return *this;
        }
        constexpr RasterState &DisableMultisample()
        {
            multisampleEnable = false;
            return *this;
        }
        constexpr RasterState &SetAntialiasedLineEnable(bool value)
        {
            antialiasedLineEnable = value;
            return *this;
        }
        constexpr RasterState &EnableAntialiasedLine()
        {
            antialiasedLineEnable = true;
            return *this;
        }
        constexpr RasterState &DisableAntialiasedLine()
        {
            antialiasedLineEnable = false;
            return *this;
        }
        constexpr RasterState &SetDepthBias(int value)
        {
            depthBias = value;
            return *this;
        }
        constexpr RasterState &SetDepthBiasClamp(float value)
        {
            depthBiasClamp = value;
            return *this;
        }
        constexpr RasterState &SetSlopeScaleDepthBias(float value)
        {
            slopeScaledDepthBias = value;
            return *this;
        }
        constexpr RasterState &SetForcedSampleCount(uint8_t value)
        {
            forcedSampleCount = value;
            return *this;
        }
        constexpr RasterState &SetProgrammableSamplePositionsEnable(bool value)
        {
            programmableSamplePositionsEnable = value;
            return *this;
        }
        constexpr RasterState &EnableProgrammableSamplePositions()
        {
            programmableSamplePositionsEnable = true;
            return *this;
        }
        constexpr RasterState &DisableProgrammableSamplePositions()
        {
            programmableSamplePositionsEnable = false;
            return *this;
        }
        constexpr RasterState &SetConservativeRasterEnable(bool value)
        {
            conservativeRasterEnable = value;
            return *this;
        }
        constexpr RasterState &EnableConservativeRaster()
        {
            conservativeRasterEnable = true;
            return *this;
        }
        constexpr RasterState &DisableConservativeRaster()
        {
            conservativeRasterEnable = false;
            return *this;
        }
        constexpr RasterState &SetQuadFillEnable(bool value)
        {
            quadFillEnable = value;
            return *this;
        }
        constexpr RasterState &EnableQuadFill()
        {
            quadFillEnable = true;
            return *this;
        }
        constexpr RasterState &DisableQuadFill()
        {
            quadFillEnable = false;
            return *this;
        }
        constexpr RasterState &SetSamplePositions(const char *x, const char *y, int count)
        {
            for (int i = 0; i < count; i++)
            {
                samplePositionsX[i] = x[i];
                samplePositionsY[i] = y[i];
            }
            return *this;
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // Depth Stencil State
    //////////////////////////////////////////////////////////////////////////

    enum class StencilOp : uint8_t
    {
        Keep = 1,
        Zero = 2,
        Replace = 3,
        IncrementAndClamp = 4,
        DecrementAndClamp = 5,
        Invert = 6,
        IncrementAndWrap = 7,
        DecrementAndWrap = 8
    };

    enum class ComparisonFunc : uint8_t
    {
        Never = 1,
        Less = 2,
        Equal = 3,
        LessOrEqual = 4,
        Greater = 5,
        NotEqual = 6,
        GreaterOrEqual = 7,
        Always = 8
    };

    struct DepthStencilState
    {
        struct StencilOpDesc
        {
            StencilOp failOp = StencilOp::Keep;
            StencilOp depthFailOp = StencilOp::Keep;
            StencilOp passOp = StencilOp::Keep;
            ComparisonFunc stencilFunc = ComparisonFunc::Always;

            constexpr StencilOpDesc &SetFailOp(StencilOp value)
            {
                failOp = value;
                return *this;
            }
            constexpr StencilOpDesc &SetDepthFailOp(StencilOp value)
            {
                depthFailOp = value;
                return *this;
            }
            constexpr StencilOpDesc &SetPassOp(StencilOp value)
            {
                passOp = value;
                return *this;
            }
            constexpr StencilOpDesc &SetStencilFunc(ComparisonFunc value)
            {
                stencilFunc = value;
                return *this;
            }
        };

        bool depthTestEnable = true;
        bool depthWriteEnable = true;
        ComparisonFunc depthFunc = ComparisonFunc::Less;
        bool stencilEnable = false;
        uint8_t stencilReadMask = 0xff;
        uint8_t stencilWriteMask = 0xff;
        uint8_t stencilRefValue = 0;
        bool dynamicStencilRef = false;
        StencilOpDesc frontFaceStencil;
        StencilOpDesc backFaceStencil;

        constexpr DepthStencilState &SetDepthTestEnable(bool value)
        {
            depthTestEnable = value;
            return *this;
        }
        constexpr DepthStencilState &EnableDepthTest()
        {
            depthTestEnable = true;
            return *this;
        }
        constexpr DepthStencilState &DisableDepthTest()
        {
            depthTestEnable = false;
            return *this;
        }
        constexpr DepthStencilState &SetDepthWriteEnable(bool value)
        {
            depthWriteEnable = value;
            return *this;
        }
        constexpr DepthStencilState &EnableDepthWrite()
        {
            depthWriteEnable = true;
            return *this;
        }
        constexpr DepthStencilState &DisableDepthWrite()
        {
            depthWriteEnable = false;
            return *this;
        }
        constexpr DepthStencilState &SetDepthFunc(ComparisonFunc value)
        {
            depthFunc = value;
            return *this;
        }
        constexpr DepthStencilState &SetStencilEnable(bool value)
        {
            stencilEnable = value;
            return *this;
        }
        constexpr DepthStencilState &EnableStencil()
        {
            stencilEnable = true;
            return *this;
        }
        constexpr DepthStencilState &DisableStencil()
        {
            stencilEnable = false;
            return *this;
        }
        constexpr DepthStencilState &SetStencilReadMask(uint8_t value)
        {
            stencilReadMask = value;
            return *this;
        }
        constexpr DepthStencilState &SetStencilWriteMask(uint8_t value)
        {
            stencilWriteMask = value;
            return *this;
        }
        constexpr DepthStencilState &SetStencilRefValue(uint8_t value)
        {
            stencilRefValue = value;
            return *this;
        }
        constexpr DepthStencilState &SetFrontFaceStencil(const StencilOpDesc &value)
        {
            frontFaceStencil = value;
            return *this;
        }
        constexpr DepthStencilState &SetBackFaceStencil(const StencilOpDesc &value)
        {
            backFaceStencil = value;
            return *this;
        }
        constexpr DepthStencilState &SetDynamicStencilRef(bool value)
        {
            dynamicStencilRef = value;
            return *this;
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // Viewport State
    //////////////////////////////////////////////////////////////////////////

    struct ViewportState
    {
        //These are in pixels
        // note: you can only set each of these either in the PSO or per draw call in DrawArguments
        // it is not legal to have the same state set in both the PSO and DrawArguments
        // leaving these vectors empty means no state is set
        StaticVector<Viewport, MaxViewports> viewports;
        StaticVector<Rect, MaxViewports> scissorRects;

        ViewportState &AddViewport(const Viewport &v)
        {
            viewports.push_back(v);
            return *this;
        }
        ViewportState &AddScissorRect(const Rect &r)
        {
            scissorRects.push_back(r);
            return *this;
        }
        ViewportState &AddViewportAndScissorRect(const Viewport &v)
        {
            return AddViewport(v).AddScissorRect(Rect(v));
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // Sampler
    //////////////////////////////////////////////////////////////////////////

    enum class SamplerAddressMode : uint8_t
    {
        // D3D names
        Clamp,
        Wrap,
        Border,
        Mirror,
        MirrorOnce,

        // Vulkan names
        ClampToEdge = Clamp,
        Repeat = Wrap,
        ClampToBorder = Border,
        MirroredRepeat = Mirror,
        MirrorClampToEdge = MirrorOnce
    };

    enum class SamplerReductionType : uint8_t
    {
        Standard,
        Comparison,
        Minimum,
        Maximum
    };

    struct SamplerDesc
    {
        Color borderColor = 1.f;
        float maxAnisotropy = 1.f;
        float mipBias = 0.f;

        bool minFilter = true;
        bool magFilter = true;
        bool mipFilter = true;
        SamplerAddressMode addressU = SamplerAddressMode::Clamp;
        SamplerAddressMode addressV = SamplerAddressMode::Clamp;
        SamplerAddressMode addressW = SamplerAddressMode::Clamp;
        SamplerReductionType reductionType = SamplerReductionType::Standard;

        SamplerDesc &SetBorderColor(const Color &color)
        {
            borderColor = color;
            return *this;
        }
        SamplerDesc &SetMaxAnisotropy(float value)
        {
            maxAnisotropy = value;
            return *this;
        }
        SamplerDesc &SetMipBias(float value)
        {
            mipBias = value;
            return *this;
        }
        SamplerDesc &SetMinFilter(bool enable)
        {
            minFilter = enable;
            return *this;
        }
        SamplerDesc &SetMagFilter(bool enable)
        {
            magFilter = enable;
            return *this;
        }
        SamplerDesc &SetMipFilter(bool enable)
        {
            mipFilter = enable;
            return *this;
        }
        SamplerDesc &SetAllFilters(bool enable)
        {
            minFilter = magFilter = mipFilter = enable;
            return *this;
        }
        SamplerDesc &SetAddressU(SamplerAddressMode mode)
        {
            addressU = mode;
            return *this;
        }
        SamplerDesc &SetAddressV(SamplerAddressMode mode)
        {
            addressV = mode;
            return *this;
        }
        SamplerDesc &SetAddressW(SamplerAddressMode mode)
        {
            addressW = mode;
            return *this;
        }
        SamplerDesc &SetAllAddressModes(SamplerAddressMode mode)
        {
            addressU = addressV = addressW = mode;
            return *this;
        }
        SamplerDesc &SetReductionType(SamplerReductionType type)
        {
            reductionType = type;
            return *this;
        }
    };

    class Sampler : public Resource
    {
      public:
        [[nodiscard]] virtual const SamplerDesc &GetDesc() const = 0;
    };

    typedef RefCountPtr<Sampler> SamplerHandle;

    //////////////////////////////////////////////////////////////////////////
    // Framebuffer
    //////////////////////////////////////////////////////////////////////////

    struct FramebufferAttachment
    {
        Texture *texture = nullptr;
        TextureSubresourceSet subresources = TextureSubresourceSet(0, 1, 0, 1);
        Format format = Format::UNKNOWN;
        bool isReadOnly = false;

        constexpr FramebufferAttachment &SetTexture(Texture *t)
        {
            texture = t;
            return *this;
        }
        constexpr FramebufferAttachment &SetSubresources(TextureSubresourceSet value)
        {
            subresources = value;
            return *this;
        }
        constexpr FramebufferAttachment &SetArraySlice(ArraySlice index)
        {
            subresources.baseArraySlice = index;
            subresources.numArraySlices = 1;
            return *this;
        }
        constexpr FramebufferAttachment &SetArraySliceRange(ArraySlice index, ArraySlice count)
        {
            subresources.baseArraySlice = index;
            subresources.numArraySlices = count;
            return *this;
        }
        constexpr FramebufferAttachment &SetMipLevel(MipLevel level)
        {
            subresources.baseMipLevel = level;
            subresources.numMipLevels = 1;
            return *this;
        }
        constexpr FramebufferAttachment &SetFormat(Format f)
        {
            format = f;
            return *this;
        }
        constexpr FramebufferAttachment &SetReadOnly(bool ro)
        {
            isReadOnly = ro;
            return *this;
        }

        [[nodiscard]] bool Valid() const
        {
            return texture != nullptr;
        }
    };

    struct FramebufferDesc
    {
        StaticVector<FramebufferAttachment, MaxRenderTargets> colorAttachments;
        FramebufferAttachment depthAttachment;
        FramebufferAttachment shadingRateAttachment;

        FramebufferDesc &AddColorAttachment(const FramebufferAttachment &a)
        {
            colorAttachments.push_back(a);
            return *this;
        }
        FramebufferDesc &AddColorAttachment(Texture *texture)
        {
            colorAttachments.push_back(FramebufferAttachment().SetTexture(texture));
            return *this;
        }
        FramebufferDesc &AddColorAttachment(Texture *texture, TextureSubresourceSet subresources)
        {
            colorAttachments.push_back(FramebufferAttachment().SetTexture(texture).SetSubresources(subresources));
            return *this;
        }
        FramebufferDesc &SetDepthAttachment(const FramebufferAttachment &d)
        {
            depthAttachment = d;
            return *this;
        }
        FramebufferDesc &SetDepthAttachment(Texture *texture)
        {
            depthAttachment = FramebufferAttachment().SetTexture(texture);
            return *this;
        }
        FramebufferDesc &SetDepthAttachment(Texture *texture, TextureSubresourceSet subresources)
        {
            depthAttachment = FramebufferAttachment().SetTexture(texture).SetSubresources(subresources);
            return *this;
        }
        FramebufferDesc &SetShadingRateAttachment(const FramebufferAttachment &d)
        {
            shadingRateAttachment = d;
            return *this;
        }
        FramebufferDesc &SetShadingRateAttachment(Texture *texture)
        {
            shadingRateAttachment = FramebufferAttachment().SetTexture(texture);
            return *this;
        }
        FramebufferDesc &SetShadingRateAttachment(Texture *texture, TextureSubresourceSet subresources)
        {
            shadingRateAttachment = FramebufferAttachment().SetTexture(texture).SetSubresources(subresources);
            return *this;
        }
    };

    // Describes the parameters of a framebuffer that can be used to determine if a given framebuffer
    // is compatible with a certain graphics or meshlet pipeline object. All fields of FramebufferInfo
    // must match between the framebuffer and the pipeline for them to be compatible.
    struct FramebufferInfo
    {
        StaticVector<Format, MaxRenderTargets> colorFormats;
        Format depthFormat = Format::UNKNOWN;
        uint32_t sampleCount = 1;
        uint32_t sampleQuality = 0;

        FramebufferInfo() = default;
        PHYSARA_RHI_API FramebufferInfo(const FramebufferDesc &desc);

        bool operator==(const FramebufferInfo &other) const
        {
            return FormatsEqual(colorFormats, other.colorFormats) && depthFormat == other.depthFormat &&
                   sampleCount == other.sampleCount && sampleQuality == other.sampleQuality;
        }
        bool operator!=(const FramebufferInfo &other) const
        {
            return !(*this == other);
        }

        FramebufferInfo &AddColorFormat(Format format)
        {
            colorFormats.push_back(format);
            return *this;
        }
        FramebufferInfo &SetDepthFormat(Format format)
        {
            depthFormat = format;
            return *this;
        }
        FramebufferInfo &SetSampleCount(uint32_t count)
        {
            sampleCount = count;
            return *this;
        }
        FramebufferInfo &SetSampleQuality(uint32_t quality)
        {
            sampleQuality = quality;
            return *this;
        }

      private:
        static bool FormatsEqual(const StaticVector<Format, MaxRenderTargets> &a,
                                 const StaticVector<Format, MaxRenderTargets> &b)
        {
            if (a.size() != b.size())
            {
                return false;
            }
            for (size_t i = 0; i < a.size(); i++)
            {
                if (a[i] != b[i])
                {
                    return false;
                }
            }
            return true;
        }
    };

    // An extended version of FramebufferInfo that also contains the framebuffer dimensions.
    struct FramebufferInfoEx : FramebufferInfo
    {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t arraySize = 1;

        FramebufferInfoEx() = default;
        // 前提: 附件描述及 mip/layer 已验证, mip<32; 此纯 metadata 构造不执行设备校验.
        PHYSARA_RHI_API FramebufferInfoEx(const FramebufferDesc &desc);

        FramebufferInfoEx &SetWidth(uint32_t value)
        {
            width = value;
            return *this;
        }
        FramebufferInfoEx &SetHeight(uint32_t value)
        {
            height = value;
            return *this;
        }
        FramebufferInfoEx &SetArraySize(uint32_t value)
        {
            arraySize = value;
            return *this;
        }

        [[nodiscard]] Viewport GetViewport(float minZ = 0.f, float maxZ = 1.f) const
        {
            return Viewport(0.f, float(width), 0.f, float(height), minZ, maxZ);
        }
    };

    class Framebuffer : public Resource
    {
      public:
        [[nodiscard]] virtual const FramebufferDesc &GetDesc() const = 0;
        [[nodiscard]] virtual const FramebufferInfoEx &GetFramebufferInfo() const = 0;
    };

    typedef RefCountPtr<Framebuffer> FramebufferHandle;

    namespace RT
    {
        //////////////////////////////////////////////////////////////////////////
        // rt::OpacityMicromap
        //////////////////////////////////////////////////////////////////////////

        enum class OpacityMicromapFormat
        {
            OC1_2_State = 1,
            OC1_4_State = 2,
        };

        enum class OpacityMicromapBuildFlags : uint8_t
        {
            None = 0,
            FastTrace = 1,
            FastBuild = 2,
            AllowCompaction = 4
        };

        PHYSARA_RHI_ENUM_CLASS_FLAG_OPERATORS(OpacityMicromapBuildFlags)

        struct OpacityMicromapUsageCount
        {
            // Number of OMMs with the specified subdivision level and format.
            uint32_t count;
            // Micro triangle count is 4^N, where N is the subdivision level.
            uint32_t subdivisionLevel;
            // OMM input sub format.
            OpacityMicromapFormat format;
        };

        struct OpacityMicromapDesc
        {
            std::string debugName;
            bool trackLiveness = true;

            // OMM flags. Applies to all OMMs in array.
            OpacityMicromapBuildFlags flags;
            // OMM counts for each subdivision level and format combination in the inputs.
            std::vector<OpacityMicromapUsageCount> counts;

            // Base pointer for raw OMM input data.
            // Individual OMMs must be 1B aligned, though natural alignment is recommended.
            // It's also recommended to try to organize OMMs together that are expected to be used spatially close together.
            Buffer *inputBuffer = nullptr;
            uint64_t inputBufferOffset = 0;

            // One NVAPI_D3D12_RAYTRACING_OPACITY_MICROMAP_DESC entry per OMM.
            Buffer *perOmmDescs = nullptr;
            uint64_t perOmmDescsOffset = 0;

            OpacityMicromapDesc &SetDebugName(const std::string &value)
            {
                debugName = value;
                return *this;
            }
            OpacityMicromapDesc &SetTrackLiveness(bool value)
            {
                trackLiveness = value;
                return *this;
            }
            OpacityMicromapDesc &SetFlags(OpacityMicromapBuildFlags value)
            {
                flags = value;
                return *this;
            }
            OpacityMicromapDesc &SetCounts(const std::vector<OpacityMicromapUsageCount> &value)
            {
                counts = value;
                return *this;
            }
            OpacityMicromapDesc &SetInputBuffer(Buffer *value)
            {
                inputBuffer = value;
                return *this;
            }
            OpacityMicromapDesc &SetInputBufferOffset(uint64_t value)
            {
                inputBufferOffset = value;
                return *this;
            }
            OpacityMicromapDesc &SetPerOmmDescs(Buffer *value)
            {
                perOmmDescs = value;
                return *this;
            }
            OpacityMicromapDesc &SetPerOmmDescsOffset(uint64_t value)
            {
                perOmmDescsOffset = value;
                return *this;
            }
        };

        class OpacityMicromap : public Resource
        {
          public:
            [[nodiscard]] virtual const OpacityMicromapDesc &GetDesc() const = 0;
            [[nodiscard]] virtual bool IsCompacted() const = 0;
            [[nodiscard]] virtual uint64_t GetDeviceAddress() const = 0;
        };

        typedef RefCountPtr<OpacityMicromap> OpacityMicromapHandle;

        //////////////////////////////////////////////////////////////////////////
        // rt::AccelStruct
        //////////////////////////////////////////////////////////////////////////

        class AccelStruct;

        typedef float AffineTransform[12];

        constexpr AffineTransform IdentityTransform = {
            //  +----+----+---------  rotation and scaling
            //  v    v    v
            1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f,
            0.f, 0.f, 0.f, 1.f, 0.f
            //                 ^
            //                 +----  translation
        };

        enum class GeometryFlags : uint8_t
        {
            None = 0,
            Opaque = 1,
            NoDuplicateAnyHitInvocation = 2
        };

        PHYSARA_RHI_ENUM_CLASS_FLAG_OPERATORS(GeometryFlags)

        enum class GeometryType : uint8_t
        {
            Triangles = 0,
            AABBs = 1,
            Spheres = 2,
            Lss = 3
        };

        struct GeometryAABB
        {
            float minX;
            float minY;
            float minZ;
            float maxX;
            float maxY;
            float maxZ;
        };

        struct GeometryTriangles
        {
            Buffer *indexBuffer = nullptr;  // make sure the first 2 fields in all Geometry
            Buffer *vertexBuffer = nullptr; // structs are IBuffer* for easier debugging
            Format indexFormat = Format::UNKNOWN;
            Format vertexFormat = Format::
                UNKNOWN; // See D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC for accepted formats and how they are interpreted
            uint64_t indexOffset = 0;
            uint64_t vertexOffset = 0;
            uint32_t indexCount = 0;
            uint32_t vertexCount = 0;
            uint32_t vertexStride = 0;

            OpacityMicromap *opacityMicromap = nullptr;
            Buffer *ommIndexBuffer = nullptr;
            uint64_t ommIndexBufferOffset = 0;
            Format ommIndexFormat = Format::UNKNOWN;
            const OpacityMicromapUsageCount *pOmmUsageCounts = nullptr;
            uint32_t numOmmUsageCounts = 0;

            GeometryTriangles &SetIndexBuffer(Buffer *value)
            {
                indexBuffer = value;
                return *this;
            }
            GeometryTriangles &SetVertexBuffer(Buffer *value)
            {
                vertexBuffer = value;
                return *this;
            }
            GeometryTriangles &SetIndexFormat(Format value)
            {
                indexFormat = value;
                return *this;
            }
            GeometryTriangles &SetVertexFormat(Format value)
            {
                vertexFormat = value;
                return *this;
            }
            GeometryTriangles &SetIndexOffset(uint64_t value)
            {
                indexOffset = value;
                return *this;
            }
            GeometryTriangles &SetVertexOffset(uint64_t value)
            {
                vertexOffset = value;
                return *this;
            }
            GeometryTriangles &SetIndexCount(uint32_t value)
            {
                indexCount = value;
                return *this;
            }
            GeometryTriangles &SetVertexCount(uint32_t value)
            {
                vertexCount = value;
                return *this;
            }
            GeometryTriangles &SetVertexStride(uint32_t value)
            {
                vertexStride = value;
                return *this;
            }
            GeometryTriangles &SetOpacityMicromap(OpacityMicromap *value)
            {
                opacityMicromap = value;
                return *this;
            }
            GeometryTriangles &SetOmmIndexBuffer(Buffer *value)
            {
                ommIndexBuffer = value;
                return *this;
            }
            GeometryTriangles &SetOmmIndexBufferOffset(uint64_t value)
            {
                ommIndexBufferOffset = value;
                return *this;
            }
            GeometryTriangles &SetOmmIndexFormat(Format value)
            {
                ommIndexFormat = value;
                return *this;
            }
            GeometryTriangles &SetPOmmUsageCounts(const OpacityMicromapUsageCount *value)
            {
                pOmmUsageCounts = value;
                return *this;
            }
            GeometryTriangles &SetNumOmmUsageCounts(uint32_t value)
            {
                numOmmUsageCounts = value;
                return *this;
            }
        };

        struct GeometryAABBs
        {
            Buffer *buffer = nullptr;
            Buffer *unused = nullptr;
            uint64_t offset = 0;
            uint32_t count = 0;
            uint32_t stride = 0;

            GeometryAABBs &SetBuffer(Buffer *value)
            {
                buffer = value;
                return *this;
            }
            GeometryAABBs &SetOffset(uint64_t value)
            {
                offset = value;
                return *this;
            }
            GeometryAABBs &SetCount(uint32_t value)
            {
                count = value;
                return *this;
            }
            GeometryAABBs &SetStride(uint32_t value)
            {
                stride = value;
                return *this;
            }
        };

        struct GeometrySpheres
        {
            Buffer *indexBuffer = nullptr;
            Buffer *vertexBuffer = nullptr;
            Format indexFormat = Format::UNKNOWN;
            Format vertexPositionFormat = Format::UNKNOWN;
            Format vertexRadiusFormat = Format::UNKNOWN;
            uint64_t indexOffset = 0;
            uint64_t vertexPositionOffset = 0;
            uint64_t vertexRadiusOffset = 0;
            uint32_t indexCount = 0;
            uint32_t vertexCount = 0;
            uint32_t indexStride = 0;
            uint32_t vertexPositionStride = 0;
            uint32_t vertexRadiusStride = 0;

            GeometrySpheres &SetIndexBuffer(Buffer *value)
            {
                indexBuffer = value;
                return *this;
            }
            GeometrySpheres &SetVertexBuffer(Buffer *value)
            {
                vertexBuffer = value;
                return *this;
            }
            GeometrySpheres &SetIndexFormat(Format value)
            {
                indexFormat = value;
                return *this;
            }
            GeometrySpheres &SetVertexPositionFormat(Format value)
            {
                vertexPositionFormat = value;
                return *this;
            }
            GeometrySpheres &SetVertexRadiusFormat(Format value)
            {
                vertexRadiusFormat = value;
                return *this;
            }
            GeometrySpheres &SetIndexOffset(uint64_t value)
            {
                indexOffset = value;
                return *this;
            }
            GeometrySpheres &SetVertexPositionOffset(uint64_t value)
            {
                vertexPositionOffset = value;
                return *this;
            }
            GeometrySpheres &SetVertexRadiusOffset(uint64_t value)
            {
                vertexRadiusOffset = value;
                return *this;
            }
            GeometrySpheres &SetIndexCount(uint32_t value)
            {
                indexCount = value;
                return *this;
            }
            GeometrySpheres &SetVertexCount(uint32_t value)
            {
                vertexCount = value;
                return *this;
            }
            GeometrySpheres &SetIndexStride(uint32_t value)
            {
                indexStride = value;
                return *this;
            }
            GeometrySpheres &SetVertexPositionStride(uint32_t value)
            {
                vertexPositionStride = value;
                return *this;
            }
            GeometrySpheres &SetVertexRadiusStride(uint32_t value)
            {
                vertexRadiusStride = value;
                return *this;
            }
        };

        enum class GeometryLssPrimitiveFormat : uint8_t
        {
            List = 0,
            SuccessiveImplicit = 1
        };

        enum class GeometryLssEndcapMode : uint8_t
        {
            None = 0,
            Chained = 1
        };

        struct GeometryLss
        {
            Buffer *indexBuffer = nullptr;
            Buffer *vertexBuffer = nullptr;
            Format indexFormat = Format::UNKNOWN;
            Format vertexPositionFormat = Format::UNKNOWN;
            Format vertexRadiusFormat = Format::UNKNOWN;
            uint64_t indexOffset = 0;
            uint64_t vertexPositionOffset = 0;
            uint64_t vertexRadiusOffset = 0;
            uint32_t indexCount = 0;
            uint32_t primitiveCount = 0;
            uint32_t vertexCount = 0;
            uint32_t indexStride = 0;
            uint32_t vertexPositionStride = 0;
            uint32_t vertexRadiusStride = 0;
            GeometryLssPrimitiveFormat primitiveFormat = GeometryLssPrimitiveFormat::List;
            GeometryLssEndcapMode endcapMode = GeometryLssEndcapMode::None;

            GeometryLss &SetIndexBuffer(Buffer *value)
            {
                indexBuffer = value;
                return *this;
            }
            GeometryLss &SetVertexBuffer(Buffer *value)
            {
                vertexBuffer = value;
                return *this;
            }
            GeometryLss &SetIndexFormat(Format value)
            {
                indexFormat = value;
                return *this;
            }
            GeometryLss &SetVertexPositionFormat(Format value)
            {
                vertexPositionFormat = value;
                return *this;
            }
            GeometryLss &SetVertexRadiusFormat(Format value)
            {
                vertexRadiusFormat = value;
                return *this;
            }
            GeometryLss &SetIndexOffset(uint64_t value)
            {
                indexOffset = value;
                return *this;
            }
            GeometryLss &SetVertexPositionOffset(uint64_t value)
            {
                vertexPositionOffset = value;
                return *this;
            }
            GeometryLss &SetVertexRadiusOffset(uint64_t value)
            {
                vertexRadiusOffset = value;
                return *this;
            }
            GeometryLss &SetIndexCount(uint32_t value)
            {
                indexCount = value;
                return *this;
            }
            GeometryLss &SetPrimitiveCount(uint32_t value)
            {
                primitiveCount = value;
                return *this;
            }
            GeometryLss &SetVertexCount(uint32_t value)
            {
                vertexCount = value;
                return *this;
            }
            GeometryLss &SetIndexStride(uint32_t value)
            {
                indexStride = value;
                return *this;
            }
            GeometryLss &SetVertexPositionStride(uint32_t value)
            {
                vertexPositionStride = value;
                return *this;
            }
            GeometryLss &SetVertexRadiusStride(uint32_t value)
            {
                vertexRadiusStride = value;
                return *this;
            }
            GeometryLss &SetPrimitiveFormat(GeometryLssPrimitiveFormat value)
            {
                primitiveFormat = value;
                return *this;
            }
            GeometryLss &SetEndcapMode(GeometryLssEndcapMode value)
            {
                endcapMode = value;
                return *this;
            }
        };

        struct GeometryDesc
        {
            union GeomTypeUnion
            {
                GeometryTriangles triangles;
                GeometryAABBs aabbs;
                GeometrySpheres spheres;
                GeometryLss lss;
            } geometryData;

            bool useTransform = false;
            AffineTransform transform{};
            GeometryFlags flags = GeometryFlags::None;
            GeometryType geometryType = GeometryType::Triangles;

            GeometryDesc() : geometryData{}
            {
            }

            GeometryDesc &SetTransform(const AffineTransform &value)
            {
                memcpy(&transform, &value, sizeof(AffineTransform));
                useTransform = true;
                return *this;
            }
            GeometryDesc &SetFlags(GeometryFlags value)
            {
                flags = value;
                return *this;
            }
            GeometryDesc &SetTriangles(const GeometryTriangles &value)
            {
                geometryData.triangles = value;
                geometryType = GeometryType::Triangles;
                return *this;
            }
            GeometryDesc &SetAABBs(const GeometryAABBs &value)
            {
                geometryData.aabbs = value;
                geometryType = GeometryType::AABBs;
                return *this;
            }
            GeometryDesc &SetSpheres(const GeometrySpheres &value)
            {
                geometryData.spheres = value;
                geometryType = GeometryType::Spheres;
                return *this;
            }
            GeometryDesc &SetLss(const GeometryLss &value)
            {
                geometryData.lss = value;
                geometryType = GeometryType::Lss;
                return *this;
            }
        };

        enum class InstanceFlags : unsigned
        {
            None = 0,
            TriangleCullDisable = 1,
            TriangleFrontCounterclockwise = 2,
            ForceOpaque = 4,
            ForceNonOpaque = 8,
            ForceOMM2State = 16,
            DisableOMMs = 32,
        };

        PHYSARA_RHI_ENUM_CLASS_FLAG_OPERATORS(InstanceFlags)

        struct InstanceDesc
        {
            AffineTransform transform;
            unsigned instanceID : 24;
            unsigned instanceMask : 8;
            unsigned instanceContributionToHitGroupIndex : 24;
            InstanceFlags flags : 8;
            union
            {
                AccelStruct *bottomLevelAS; // for buildTopLevelAccelStruct
                uint64_t
                    blasDeviceAddress; // for buildTopLevelAccelStructFromBuffer - use IAccelStruct::getDeviceAddress()
            };

            InstanceDesc() // NOLINT(cppcoreguidelines-pro-type-member-init)
                : instanceID(0), instanceMask(0), instanceContributionToHitGroupIndex(0), flags(InstanceFlags::None),
                  bottomLevelAS(nullptr)
            {
                SetTransform(IdentityTransform);
            }

            InstanceDesc &SetInstanceID(uint32_t value)
            {
                instanceID = value;
                return *this;
            }
            InstanceDesc &SetInstanceContributionToHitGroupIndex(uint32_t value)
            {
                instanceContributionToHitGroupIndex = value;
                return *this;
            }
            InstanceDesc &SetInstanceMask(uint32_t value)
            {
                instanceMask = value;
                return *this;
            }
            InstanceDesc &SetTransform(const AffineTransform &value)
            {
                memcpy(&transform, &value, sizeof(AffineTransform));
                return *this;
            }
            InstanceDesc &SetFlags(InstanceFlags value)
            {
                flags = value;
                return *this;
            }
            InstanceDesc &SetBLAS(AccelStruct *value)
            {
                bottomLevelAS = value;
                return *this;
            }
        };

        static_assert(sizeof(InstanceDesc) == 64, "sizeof(InstanceDesc) is supposed to be 64 bytes");
        static_assert(sizeof(IndirectInstanceDesc) == sizeof(InstanceDesc));

        enum class AccelStructBuildFlags : uint8_t
        {
            None = 0,
            AllowUpdate = 1,
            AllowCompaction = 2,
            PreferFastTrace = 4,
            PreferFastBuild = 8,
            MinimizeMemory = 0x10,
            PerformUpdate = 0x20,

            // Removes the errors or warnings that NVRHI validation layer issues when a TLAS
            // includes an instance that points at a NULL BLAS or has a zero instance mask.
            // Only affects the validation layer, doesn't translate to Vk/DX12 AS build flags.
            AllowEmptyInstances = 0x80
        };

        PHYSARA_RHI_ENUM_CLASS_FLAG_OPERATORS(AccelStructBuildFlags)

        struct AccelStructPrebuildInfo
        {
            uint64_t resultBytes = 0;
            uint64_t scratchBytes = 0;
            uint64_t updateScratchBytes = 0;
        };

        struct AccelStructDesc
        {
            size_t topLevelMaxInstances = 0;                 // only applies when isTopLevel = true
            std::vector<GeometryDesc> bottomLevelGeometries; // only applies when isTopLevel = false
            AccelStructBuildFlags buildFlags = AccelStructBuildFlags::None;
            std::string debugName;
            bool trackLiveness = true;
            bool isTopLevel = false;
            bool isVirtual = false;

            AccelStructDesc &SetTopLevelMaxInstances(size_t value)
            {
                topLevelMaxInstances = value;
                isTopLevel = true;
                return *this;
            }
            AccelStructDesc &AddBottomLevelGeometry(const GeometryDesc &value)
            {
                bottomLevelGeometries.push_back(value);
                isTopLevel = false;
                return *this;
            }
            AccelStructDesc &SetBuildFlags(AccelStructBuildFlags value)
            {
                buildFlags = value;
                return *this;
            }
            AccelStructDesc &SetDebugName(const std::string &value)
            {
                debugName = value;
                return *this;
            }
            AccelStructDesc &SetTrackLiveness(bool value)
            {
                trackLiveness = value;
                return *this;
            }
            AccelStructDesc &SetIsTopLevel(bool value)
            {
                isTopLevel = value;
                return *this;
            }
            AccelStructDesc &SetIsVirtual(bool value)
            {
                isVirtual = value;
                return *this;
            }
        };

        //////////////////////////////////////////////////////////////////////////
        // rt::AccelStruct
        //////////////////////////////////////////////////////////////////////////

        class AccelStruct : public Resource
        {
          public:
            [[nodiscard]] virtual const AccelStructDesc &GetDesc() const = 0;
            [[nodiscard]] virtual bool IsCompacted() const = 0;
            [[nodiscard]] virtual uint64_t GetDeviceAddress() const = 0;
        };

        typedef RefCountPtr<AccelStruct> AccelStructHandle;

        //////////////////////////////////////////////////////////////////////////
        // Clusters
        //////////////////////////////////////////////////////////////////////////
        namespace Cluster
        {
            enum class OperationType : uint8_t
            {
                Move,                     // Moves CLAS, CLAS Templates, or Cluster BLAS
                ClasBuild,                // Builds CLAS from clusters of triangles
                ClasBuildTemplates,       // Builds CLAS templates from triangles
                ClasInstantiateTemplates, // Instantiates CLAS templates
                BlasBuild                 // Builds Cluster BLAS from CLAS
            };

            enum class OperationMoveType : uint8_t
            {
                BottomLevel,  // Moved objects are Clustered BLAS
                ClusterLevel, // Moved objects are CLAS
                Template      // Moved objects are Cluster Templates
            };

            enum class OperationMode : uint8_t
            {
                ImplicitDestinations, // Provide total buffer space, driver places results within, returns VAs and actual sizes
                ExplicitDestinations, // Provide individual target VAs, driver places them there, returns actual sizes
                GetSizes              // Get minimum size per element
            };

            enum class OperationFlags : uint8_t
            {
                None = 0x0,
                FastTrace = 0x1,
                FastBuild = 0x2,
                NoOverlap = 0x4,
                AllowOMM = 0x8
            };
            PHYSARA_RHI_ENUM_CLASS_FLAG_OPERATORS(OperationFlags);

            enum class OperationIndexFormat : uint8_t
            {
                IndexFormat8bit = 1,
                IndexFormat16bit = 2,
                IndexFormat32bit = 4
            };

            struct OperationSizeInfo
            {
                uint64_t resultMaxSizeInBytes = 0;
                uint64_t scratchSizeInBytes = 0;
            };

            struct OperationMoveParams
            {
                OperationMoveType type;
                uint32_t maxBytes = 0;
            };

            struct OperationClasBuildParams
            {
                // See D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC for accepted formats and how they are interpreted
                Format vertexFormat = Format::RGB32_FLOAT;

                // Index of the last geometry in a single CLAS
                uint32_t maxGeometryIndex = 0;

                // Maximum number of unique geometries in a single CLAS
                uint32_t maxUniqueGeometryCount = 1;

                // Maximum number of triangles in a single CLAS
                uint32_t maxTriangleCount = 0;

                // Maximum number of vertices in a single CLAS
                uint32_t maxVertexCount = 0;

                // Maximum number of triangles summed over all CLAS (in the current cluster operation)
                uint32_t maxTotalTriangleCount = 0;

                // Maximum number of vertices summed over all CLAS (in the current cluster operation)
                uint32_t maxTotalVertexCount = 0;

                // Minimum number of bits to be truncated in vertex positions across all CLAS (in the current cluster operation)
                uint32_t minPositionTruncateBitCount = 0;
            };

            struct OperationBlasBuildParams
            {
                // Maximum number of CLAS references in a single BLAS
                uint32_t maxClasPerBlasCount = 0;

                // Maximum number of CLAS references summed over all BLAS (in the current cluster operation)
                uint32_t maxTotalClasCount = 0;
            };

            // Params that can be used to getClusterOperationSizeInfo on this shared struct before passing to executeMultiIndirectClusterOperation
            struct OperationParams
            {
                // Maximum number of acceleration structures (or templates) to build/instantiate/move
                uint32_t maxArgCount = 0;

                OperationType type;
                OperationMode mode;
                OperationFlags flags;

                OperationMoveParams move;
                OperationClasBuildParams clas;
                OperationBlasBuildParams blas;
            };

            struct OperationDesc
            {
                OperationParams params;

                uint64_t scratchSizeInBytes =
                    0; // Size of scratch resource returned by getClusterOperationSizeInfo() scratchSizeInBytes

                // Input Resources
                Buffer *inIndirectArgCountBuffer =
                    nullptr; // Buffer containing the number of AS to build, instantiate, or move
                uint64_t inIndirectArgCountOffsetInBytes =
                    0; // Offset (in bytes) to where the count is in the inIndirectArgCountBuffer
                Buffer *inIndirectArgsBuffer =
                    nullptr; // Buffer of descriptor array of format IndirectTriangleClasArgs, IndirectTriangleTemplateArgs, IndirectInstantiateTemplateArgs
                uint64_t inIndirectArgsOffsetInBytes =
                    0; // Offset (in bytes) to where the descriptor array starts inIndirectArgsBuffer

                // In/Out Resources
                Buffer *inOutAddressesBuffer = nullptr; // Array of addresses of CLAS, CLAS Templates, or BLAS
                uint64_t inOutAddressesOffsetInBytes =
                    0; // Offset (in bytes) to where the addresses array starts in inOutAddressesBuffer

                // Output Resources
                Buffer *outSizesBuffer = nullptr; // Sizes (in bytes) of CLAS, CLAS Templates, or BLAS
                uint64_t outSizesOffsetInBytes =
                    0; // Offset (in bytes) to where the output sizes array starts in outSizesBuffer
                Buffer *outAccelerationStructuresBuffer =
                    nullptr; // Destination buffer for CLAS, CLAS Template, or BLAS data. Size must be calculated with getOperationSizeInfo or with the outSizesBuffer result of OperationMode::GetSizes
                uint64_t outAccelerationStructuresOffsetInBytes =
                    0; // Offset (in bytes) to where the output acceleration structures starts in outAccelerationStructuresBuffer
            };
        } // namespace Cluster
    }

    //////////////////////////////////////////////////////////////////////////
    // Binding Layouts
    //////////////////////////////////////////////////////////////////////////

    // identifies the underlying resource type in a binding
    enum class ResourceType : uint8_t
    {
        None,
        Texture_SRV,
        Texture_UAV,
        TypedBuffer_SRV,
        TypedBuffer_UAV,
        StructuredBuffer_SRV,
        StructuredBuffer_UAV,
        RawBuffer_SRV,
        RawBuffer_UAV,
        ConstantBuffer,
        VolatileConstantBuffer,
        Sampler,
        RayTracingAccelStruct,
        PushConstants,
        SamplerFeedbackTexture_UAV,

        Count
    };

    struct BindingLayoutItem
    {
        uint32_t slot;

        ResourceType type : 8;
        uint8_t unused : 8;
        // Push constant byte size when (type == PushConstants)
        // Descriptor array size (1 or more) for all other resource types
        // Must be 1 for VolatileConstantBuffer
        uint16_t size : 16;

        bool operator==(const BindingLayoutItem &b) const
        {
            return slot == b.slot && type == b.type && size == b.size;
        }
        bool operator!=(const BindingLayoutItem &b) const
        {
            return !(*this == b);
        }

        constexpr BindingLayoutItem &SetSlot(uint32_t value)
        {
            slot = value;
            return *this;
        }
        constexpr BindingLayoutItem &SetType(ResourceType value)
        {
            type = value;
            return *this;
        }
        constexpr BindingLayoutItem &SetSize(uint32_t value)
        {
            size = uint16_t(value);
            return *this;
        }

        uint32_t GetArraySize() const
        {
            return (type == ResourceType::PushConstants) ? 1 : size;
        }

        // Helper functions for strongly typed initialization
#define PHYSARA_RHI_BINDING_LAYOUT_ITEM_INITIALIZER(TYPE) /* NOLINT(cppcoreguidelines-macro-usage) */                  \
    static BindingLayoutItem TYPE(const uint32_t slot)                                                                 \
    {                                                                                                                  \
        BindingLayoutItem result{};                                                                                    \
        result.slot = slot;                                                                                            \
        result.type = ResourceType::TYPE;                                                                              \
        result.size = 1;                                                                                               \
        return result;                                                                                                 \
    }

        PHYSARA_RHI_BINDING_LAYOUT_ITEM_INITIALIZER(Texture_SRV)
        PHYSARA_RHI_BINDING_LAYOUT_ITEM_INITIALIZER(Texture_UAV)
        PHYSARA_RHI_BINDING_LAYOUT_ITEM_INITIALIZER(TypedBuffer_SRV)
        PHYSARA_RHI_BINDING_LAYOUT_ITEM_INITIALIZER(TypedBuffer_UAV)
        PHYSARA_RHI_BINDING_LAYOUT_ITEM_INITIALIZER(StructuredBuffer_SRV)
        PHYSARA_RHI_BINDING_LAYOUT_ITEM_INITIALIZER(StructuredBuffer_UAV)
        PHYSARA_RHI_BINDING_LAYOUT_ITEM_INITIALIZER(RawBuffer_SRV)
        PHYSARA_RHI_BINDING_LAYOUT_ITEM_INITIALIZER(RawBuffer_UAV)
        PHYSARA_RHI_BINDING_LAYOUT_ITEM_INITIALIZER(ConstantBuffer)
        PHYSARA_RHI_BINDING_LAYOUT_ITEM_INITIALIZER(VolatileConstantBuffer)
        PHYSARA_RHI_BINDING_LAYOUT_ITEM_INITIALIZER(Sampler)
        PHYSARA_RHI_BINDING_LAYOUT_ITEM_INITIALIZER(RayTracingAccelStruct)
        PHYSARA_RHI_BINDING_LAYOUT_ITEM_INITIALIZER(SamplerFeedbackTexture_UAV)

        static BindingLayoutItem PushConstants(const uint32_t slot, const size_t size)
        {
            BindingLayoutItem result{};
            result.slot = slot;
            result.type = ResourceType::PushConstants;
            result.size = uint16_t(size);
            return result;
        }
#undef PHYSARA_RHI_BINDING_LAYOUT_ITEM_INITIALIZER
    };

    // verify the packing of BindingLayoutItem for good alignment
    static_assert(sizeof(BindingLayoutItem) == 8, "sizeof(BindingLayoutItem) is supposed to be 8 bytes");

    // Describes compile-time settings for HLSL -> SPIR-V register allocation.
    struct VulkanBindingOffsets
    {
        uint32_t shaderResource = 0;
        uint32_t sampler = 128;
        uint32_t constantBuffer = 256;
        uint32_t unorderedAccess = 384;

        constexpr VulkanBindingOffsets &SetShaderResourceOffset(uint32_t value)
        {
            shaderResource = value;
            return *this;
        }
        constexpr VulkanBindingOffsets &SetSamplerOffset(uint32_t value)
        {
            sampler = value;
            return *this;
        }
        constexpr VulkanBindingOffsets &SetConstantBufferOffset(uint32_t value)
        {
            constantBuffer = value;
            return *this;
        }
        constexpr VulkanBindingOffsets &SetUnorderedAccessViewOffset(uint32_t value)
        {
            unorderedAccess = value;
            return *this;
        }
    };

    struct BindingLayoutDesc
    {
        ShaderType visibility = ShaderType::None;

        // On DX11, the registerSpace is ignored, and all bindings are placed in the same space.
        // On DX12, it controls the register space of the bindings.
        // On Vulkan, DXC maps register spaces to descriptor sets by default, so this can be used to
        // determine the descriptor set index for the binding layout.
        // In order to use this behavior, you must set `registerSpaceIsDescriptorSet` to true. See below.
        uint32_t registerSpace = 0;

        // This flag controls the behavior for pipelines that use multiple binding layouts.
        // It must be set to the same value for _all_ of the binding layouts in a pipeline.
        // - When it's set to `false`, the `registerSpace` parameter only affects the DX12 implementation,
        //   and the validation layer will report an error when non-zero `registerSpace` is used with other APIs.
        // - When it's set to `true` the parameter also affects the Vulkan implementation, allowing any
        //   layout to occupy any register space or descriptor set, regardless of their order in the pipeline.
        //   However, a consequence of DXC mapping the descriptor set index to register space is that you may
        //   not have more than one `BindingLayout` using the same `registerSpace` value in the same pipeline.
        // - When it's set to different values for the layouts in a pipeline, the validation layer will report
        //   an error.
        bool registerSpaceIsDescriptorSet = false;

        std::vector<BindingLayoutItem> bindings;
        VulkanBindingOffsets bindingOffsets;

        BindingLayoutDesc &SetVisibility(ShaderType value)
        {
            visibility = value;
            return *this;
        }
        BindingLayoutDesc &SetRegisterSpace(uint32_t value)
        {
            registerSpace = value;
            return *this;
        }
        BindingLayoutDesc &SetRegisterSpaceIsDescriptorSet(bool value)
        {
            registerSpaceIsDescriptorSet = value;
            return *this;
        }
        // Shortcut for .setRegisterSpace(value).setRegisterSpaceIsDescriptorSet(true)
        BindingLayoutDesc &SetRegisterSpaceAndDescriptorSet(uint32_t value)
        {
            registerSpace = value;
            registerSpaceIsDescriptorSet = true;
            return *this;
        }
        BindingLayoutDesc &AddItem(const BindingLayoutItem &value)
        {
            bindings.push_back(value);
            return *this;
        }
        BindingLayoutDesc &SetBindingOffsets(const VulkanBindingOffsets &value)
        {
            bindingOffsets = value;
            return *this;
        }
    };

    // Bindless layouts allow applications to attach a descriptor table to an unbounded
    // resource array in the shader. The size of the array is not known ahead of time.
    // The same table can be bound to multiple register spaces on DX12, in order to
    // access different types of resources stored in the table through different arrays.
    // The `registerSpaces` vector specifies which spaces will the table be bound to,
    // with the table type (SRV or UAV) derived from the resource type assigned to each space.
    struct BindlessLayoutDesc
    {

        // BindlessDescriptorType bridges the DX12 and Vulkan in supporting HLSL ResourceDescriptorHeap and SamplerDescriptorHeap
        // For DX12:
        // - MutableSrvUavCbv, MutableCounters will enable D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED for the Root Signature
        // - MutableSampler will enable D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED for the Root Signature
        // - The BindingLayout will be ignored in terms of setting a descriptor set. DescriptorIndexing should use GetDescriptorIndexInHeap()
        // For Vulkan:
        // - The type corresponds to the SPIR-V bindings which map to ResourceDescriptorHeap and SamplerDescriptorHeap
        // - The shader needs to be compiled with the same descriptor set index as is passed into setState
        // https://github.com/microsoft/DirectXShaderCompiler/blob/main/docs/SPIR-V.rst#resourcedescriptorheaps-samplerdescriptorheaps
        enum class LayoutType
        {
            Immutable = 0, // Must use registerSpaces to define a fixed descriptor type

            MutableSrvUavCbv, // Corresponds to SPIR-V binding -fvk-bind-resource-heap (Counter resources ResourceDescriptorHeap)
                              // Valid descriptor types: Texture_SRV, Texture_UAV, TypedBuffer_SRV, TypedBuffer_UAV,
            // StructuredBuffer_SRV, StructuredBuffer_UAV, RawBuffer_SRV, RawBuffer_UAV, ConstantBuffer

            MutableCounters, // Corresponds to SPIR-V binding -fvk-bind-counter-heap (Counter resources accessed via ResourceDescriptorHeap)
                // Valid descriptor types: StructuredBuffer_UAV

            MutableSampler, // Corresponds to SPIR-V binding -fvk-bind-sampler-heap (SamplerDescriptorHeap)
                            // Valid descriptor types: Sampler
        };

        ShaderType visibility = ShaderType::None;
        uint32_t firstSlot = 0;
        uint32_t maxCapacity = 0;
        StaticVector<BindingLayoutItem, MaxBindlessRegisterSpaces> registerSpaces;

        LayoutType layoutType = LayoutType::Immutable;

        BindlessLayoutDesc &SetVisibility(ShaderType value)
        {
            visibility = value;
            return *this;
        }
        BindlessLayoutDesc &SetFirstSlot(uint32_t value)
        {
            firstSlot = value;
            return *this;
        }
        BindlessLayoutDesc &SetMaxCapacity(uint32_t value)
        {
            maxCapacity = value;
            return *this;
        }
        BindlessLayoutDesc &AddRegisterSpace(const BindingLayoutItem &value)
        {
            registerSpaces.push_back(value);
            return *this;
        }
        BindlessLayoutDesc &SetLayoutType(LayoutType value)
        {
            layoutType = value;
            return *this;
        }
    };

    class BindingLayout : public Resource
    {
      public:
        [[nodiscard]] virtual const BindingLayoutDesc *GetDesc() const = 0; // returns nullptr for bindless layouts
        [[nodiscard]] virtual const BindlessLayoutDesc *GetBindlessDesc()
            const = 0; // returns nullptr for regular layouts
    };

    typedef RefCountPtr<BindingLayout> BindingLayoutHandle;

    //////////////////////////////////////////////////////////////////////////
    // Binding Sets
    //////////////////////////////////////////////////////////////////////////

    // 资源指针为借用. key 须已初始化且 type 与活动载荷匹配; reserved 不参与 key.
    struct BindingSetItem
    {
        Resource *resourceHandle;

        uint32_t slot;

        // Specifies the index in a binding array.
        // Must be less than the 'size' property of the matching BindingLayoutItem.
        // - DX11/12: Effective binding slot index is calculated as (slot + arrayElement), i.e. arrays are flattened
        // - Vulkan: Descriptor arrays are used.
        // This behavior matches the behavior of HLSL resource array declarations when compiled with DXC.
        uint32_t arrayElement;

        ResourceType type : 8;
        TextureDimension dimension : 8; // valid for Texture_SRV, Texture_UAV
        Format format : 8;              // valid for Texture_SRV, Texture_UAV, Buffer_SRV, Buffer_UAV
        uint8_t unused : 8;

        // Packed ComponentMapping for Texture_SRV; 0 inherits the texture's
        // defaultComponentMapping. See resolveComponentMapping().
        uint16_t overrideComponentMapping;
        uint16_t unused2; // padding

        union
        {
            TextureSubresourceSet subresources; // valid for Texture_SRV, Texture_UAV
            BufferRange range;                  // valid for Buffer_SRV, Buffer_UAV, ConstantBuffer
            uint64_t rawData[2];
        };

        // verify that the `subresources` and `range` have the same size and are covered by `rawData`
        static_assert(sizeof(TextureSubresourceSet) == 16, "sizeof(TextureSubresourceSet) is supposed to be 16 bytes");
        static_assert(sizeof(BufferRange) == 16, "sizeof(BufferRange) is supposed to be 16 bytes");

        bool operator==(const BindingSetItem &b) const
        {
            if (resourceHandle != b.resourceHandle || slot != b.slot || arrayElement != b.arrayElement ||
                type != b.type || dimension != b.dimension || format != b.format ||
                overrideComponentMapping != b.overrideComponentMapping)
            {
                return false;
            }
            switch (type)
            {
            case ResourceType::Texture_SRV:
            case ResourceType::Texture_UAV:
            case ResourceType::SamplerFeedbackTexture_UAV:
                return subresources.baseMipLevel == b.subresources.baseMipLevel &&
                       subresources.numMipLevels == b.subresources.numMipLevels &&
                       subresources.baseArraySlice == b.subresources.baseArraySlice &&
                       subresources.numArraySlices == b.subresources.numArraySlices;
            case ResourceType::TypedBuffer_SRV:
            case ResourceType::TypedBuffer_UAV:
            case ResourceType::StructuredBuffer_SRV:
            case ResourceType::StructuredBuffer_UAV:
            case ResourceType::RawBuffer_SRV:
            case ResourceType::RawBuffer_UAV:
            case ResourceType::ConstantBuffer:
            case ResourceType::VolatileConstantBuffer:
            case ResourceType::PushConstants:
                return range.byteOffset == b.range.byteOffset && range.byteSize == b.range.byteSize;
            case ResourceType::None:
            case ResourceType::Sampler:
            case ResourceType::RayTracingAccelStruct:
                return true;
            default:
                assert(false);
                return false;
            }
        }

        bool operator!=(const BindingSetItem &b) const
        {
            return !(*this == b);
        }

        // Default constructor that doesn't initialize anything for performance:
        // BindingSetItem's are stored in large statically sized arrays.
        // 暂存记录未初始化; 必须先由工厂/匹配 setter 填充, 再比较、哈希或提交.
        BindingSetItem()
        {
        } // NOLINT(cppcoreguidelines-pro-type-member-init, modernize-use-equals-default)

        // Helper functions for strongly typed initialization

        static BindingSetItem None(uint32_t slot = 0)
        {
            BindingSetItem result;
            result.slot = slot;
            result.arrayElement = 0;
            result.type = ResourceType::None;
            result.resourceHandle = nullptr;
            result.format = Format::UNKNOWN;
            result.dimension = TextureDimension::Unknown;
            result.rawData[0] = 0;
            result.rawData[1] = 0;
            result.unused = 0;
            result.overrideComponentMapping = 0;
            result.unused2 = 0;
            return result;
        }

        static BindingSetItem Texture_SRV(uint32_t slot, Texture *texture, Format format = Format::UNKNOWN,
                                          TextureSubresourceSet subresources = AllSubresources,
                                          TextureDimension dimension = TextureDimension::Unknown,
                                          std::optional<ComponentMapping> overrideComponentMapping = std::nullopt)
        {
            BindingSetItem result;
            result.slot = slot;
            result.arrayElement = 0;
            result.type = ResourceType::Texture_SRV;
            result.resourceHandle = texture;
            result.format = format;
            result.dimension = dimension;
            result.subresources = subresources;
            result.unused = 0;
            result.overrideComponentMapping = PackComponentMapping(overrideComponentMapping);
            result.unused2 = 0;
            return result;
        }

        static BindingSetItem Texture_UAV(
            uint32_t slot, Texture *texture, Format format = Format::UNKNOWN,
            TextureSubresourceSet subresources = TextureSubresourceSet(0, 1, 0, TextureSubresourceSet::AllArraySlices),
            TextureDimension dimension = TextureDimension::Unknown)
        {
            BindingSetItem result;
            result.slot = slot;
            result.arrayElement = 0;
            result.type = ResourceType::Texture_UAV;
            result.resourceHandle = texture;
            result.format = format;
            result.dimension = dimension;
            result.subresources = subresources;
            result.unused = 0;
            result.overrideComponentMapping = 0;
            result.unused2 = 0;
            return result;
        }

        static BindingSetItem TypedBuffer_SRV(uint32_t slot, Buffer *buffer, Format format = Format::UNKNOWN,
                                              BufferRange range = EntireBuffer)
        {
            BindingSetItem result;
            result.slot = slot;
            result.arrayElement = 0;
            result.type = ResourceType::TypedBuffer_SRV;
            result.resourceHandle = buffer;
            result.format = format;
            result.dimension = TextureDimension::Unknown;
            result.range = range;
            result.unused = 0;
            result.overrideComponentMapping = 0;
            result.unused2 = 0;
            return result;
        }

        static BindingSetItem TypedBuffer_UAV(uint32_t slot, Buffer *buffer, Format format = Format::UNKNOWN,
                                              BufferRange range = EntireBuffer)
        {
            BindingSetItem result;
            result.slot = slot;
            result.arrayElement = 0;
            result.type = ResourceType::TypedBuffer_UAV;
            result.resourceHandle = buffer;
            result.format = format;
            result.dimension = TextureDimension::Unknown;
            result.range = range;
            result.unused = 0;
            result.overrideComponentMapping = 0;
            result.unused2 = 0;
            return result;
        }

        static BindingSetItem ConstantBuffer(uint32_t slot, Buffer *buffer, BufferRange range = EntireBuffer)
        {
            bool isVolatile = buffer && buffer->GetDesc().isVolatile;

            BindingSetItem result;
            result.slot = slot;
            result.arrayElement = 0;
            result.type = isVolatile ? ResourceType::VolatileConstantBuffer : ResourceType::ConstantBuffer;
            result.resourceHandle = buffer;
            result.format = Format::UNKNOWN;
            result.dimension = TextureDimension::Unknown;
            result.range = range;
            result.unused = 0;
            result.overrideComponentMapping = 0;
            result.unused2 = 0;
            return result;
        }

        static BindingSetItem Sampler(uint32_t slot, Sampler *sampler)
        {
            BindingSetItem result;
            result.slot = slot;
            result.arrayElement = 0;
            result.type = ResourceType::Sampler;
            result.resourceHandle = sampler;
            result.format = Format::UNKNOWN;
            result.dimension = TextureDimension::Unknown;
            result.rawData[0] = 0;
            result.rawData[1] = 0;
            result.unused = 0;
            result.overrideComponentMapping = 0;
            result.unused2 = 0;
            return result;
        }

        static BindingSetItem RayTracingAccelStruct(uint32_t slot, RT::AccelStruct *as)
        {
            BindingSetItem result;
            result.slot = slot;
            result.arrayElement = 0;
            result.type = ResourceType::RayTracingAccelStruct;
            result.resourceHandle = as;
            result.format = Format::UNKNOWN;
            result.dimension = TextureDimension::Unknown;
            result.rawData[0] = 0;
            result.rawData[1] = 0;
            result.unused = 0;
            result.overrideComponentMapping = 0;
            result.unused2 = 0;
            return result;
        }

        static BindingSetItem StructuredBuffer_SRV(uint32_t slot, Buffer *buffer, Format format = Format::UNKNOWN,
                                                   BufferRange range = EntireBuffer)
        {
            BindingSetItem result;
            result.slot = slot;
            result.arrayElement = 0;
            result.type = ResourceType::StructuredBuffer_SRV;
            result.resourceHandle = buffer;
            result.format = format;
            result.dimension = TextureDimension::Unknown;
            result.range = range;
            result.unused = 0;
            result.overrideComponentMapping = 0;
            result.unused2 = 0;
            return result;
        }

        static BindingSetItem StructuredBuffer_UAV(uint32_t slot, Buffer *buffer, Format format = Format::UNKNOWN,
                                                   BufferRange range = EntireBuffer)
        {
            BindingSetItem result;
            result.slot = slot;
            result.arrayElement = 0;
            result.type = ResourceType::StructuredBuffer_UAV;
            result.resourceHandle = buffer;
            result.format = format;
            result.dimension = TextureDimension::Unknown;
            result.range = range;
            result.unused = 0;
            result.overrideComponentMapping = 0;
            result.unused2 = 0;
            return result;
        }

        static BindingSetItem RawBuffer_SRV(uint32_t slot, Buffer *buffer, BufferRange range = EntireBuffer)
        {
            BindingSetItem result;
            result.slot = slot;
            result.arrayElement = 0;
            result.type = ResourceType::RawBuffer_SRV;
            result.resourceHandle = buffer;
            result.format = Format::UNKNOWN;
            result.dimension = TextureDimension::Unknown;
            result.range = range;
            result.unused = 0;
            result.overrideComponentMapping = 0;
            result.unused2 = 0;
            return result;
        }

        static BindingSetItem RawBuffer_UAV(uint32_t slot, Buffer *buffer, BufferRange range = EntireBuffer)
        {
            BindingSetItem result;
            result.slot = slot;
            result.arrayElement = 0;
            result.type = ResourceType::RawBuffer_UAV;
            result.resourceHandle = buffer;
            result.format = Format::UNKNOWN;
            result.dimension = TextureDimension::Unknown;
            result.range = range;
            result.unused = 0;
            result.overrideComponentMapping = 0;
            result.unused2 = 0;
            return result;
        }

        static BindingSetItem PushConstants(uint32_t slot, uint32_t byteSize)
        {
            BindingSetItem result;
            result.slot = slot;
            result.arrayElement = 0;
            result.type = ResourceType::PushConstants;
            result.resourceHandle = nullptr;
            result.format = Format::UNKNOWN;
            result.dimension = TextureDimension::Unknown;
            result.range.byteOffset = 0;
            result.range.byteSize = byteSize;
            result.unused = 0;
            result.overrideComponentMapping = 0;
            result.unused2 = 0;
            return result;
        }

        static BindingSetItem SamplerFeedbackTexture_UAV(uint32_t slot, SamplerFeedbackTexture *texture)
        {
            BindingSetItem result;
            result.slot = slot;
            result.arrayElement = 0;
            result.type = ResourceType::SamplerFeedbackTexture_UAV;
            result.resourceHandle = texture;
            result.format = Format::UNKNOWN;
            result.dimension = TextureDimension::Unknown;
            result.subresources = AllSubresources;
            result.unused = 0;
            result.overrideComponentMapping = 0;
            result.unused2 = 0;
            return result;
        }

        BindingSetItem &SetArrayElement(uint32_t value)
        {
            arrayElement = value;
            return *this;
        }
        BindingSetItem &SetFormat(Format value)
        {
            format = value;
            return *this;
        }
        BindingSetItem &SetDimension(TextureDimension value)
        {
            dimension = value;
            return *this;
        }
        BindingSetItem &SetSubresources(TextureSubresourceSet value)
        {
            subresources = value;
            return *this;
        }
        BindingSetItem &SetRange(BufferRange value)
        {
            range = value;
            return *this;
        }

        // Overrides the texture's defaultComponentMapping for this binding, including
        // when 'value' is identity -- pass ComponentMapping() to force identity and
        // std::nullopt to inherit. Only valid on Texture_SRV.
        BindingSetItem &SetOverrideComponentMapping(std::optional<ComponentMapping> value)
        {
            overrideComponentMapping = PackComponentMapping(value);
            return *this;
        }
    };

    // verify the packing of BindingSetItem for good alignment
    static_assert(sizeof(BindingSetItem) == 40, "sizeof(BindingSetItem) is supposed to be 40 bytes");

    // Describes a set of bindings corresponding to one binding layout
    struct BindingSetDesc
    {
        std::vector<BindingSetItem> bindings;

        // Enables automatic liveness tracking of this binding set by nvrhi command lists.
        // By setting trackLiveness to false, you take the responsibility of not releasing it
        // until all rendering commands using the binding set are finished.
        bool trackLiveness = true;

        bool operator==(const BindingSetDesc &b) const
        {
            if (trackLiveness != b.trackLiveness || bindings.size() != b.bindings.size())
            {
                return false;
            }
            for (size_t i = 0; i < bindings.size(); ++i)
            {
                if (bindings[i] != b.bindings[i])
                {
                    return false;
                }
            }
            return true;
        }

        bool operator!=(const BindingSetDesc &b) const
        {
            return !(*this == b);
        }

        BindingSetDesc &AddItem(const BindingSetItem &value)
        {
            bindings.push_back(value);
            return *this;
        }
        BindingSetDesc &SetTrackLiveness(bool value)
        {
            trackLiveness = value;
            return *this;
        }
    };

    class BindingSet : public Resource
    {
      public:
        [[nodiscard]] virtual const BindingSetDesc *GetDesc() const = 0; // returns nullptr for descriptor tables
        [[nodiscard]] virtual BindingLayout *GetLayout() const = 0;
    };

    typedef RefCountPtr<BindingSet> BindingSetHandle;

    // Descriptor tables are bare, without extra mappings, state, or liveness tracking.
    // Unlike binding sets, descriptor tables are mutable - moreover, modification is the only way to populate them.
    // They can be grown or shrunk, and they are not tied to any binding layout.
    // All tracking is off, so applications should use descriptor tables with great care.
    // IDescriptorTable is derived from IBindingSet to allow mixing them in the binding arrays.
    class DescriptorTable : public BindingSet
    {
      public:
        [[nodiscard]] virtual uint32_t GetCapacity() const = 0;
        [[nodiscard]] virtual uint32_t GetFirstDescriptorIndexInHeap() const = 0;
    };

    typedef RefCountPtr<DescriptorTable> DescriptorTableHandle;

    //////////////////////////////////////////////////////////////////////////
    // Draw State
    //////////////////////////////////////////////////////////////////////////

    enum class PrimitiveType : uint8_t
    {
        PointList,
        LineList,
        LineStrip,
        TriangleList,
        TriangleStrip,
        TriangleFan,
        TriangleListWithAdjacency,
        TriangleStripWithAdjacency,
        PatchList
    };

    struct SinglePassStereoState
    {
        bool enabled = false;
        bool independentViewportMask = false;
        uint16_t renderTargetIndexOffset = 0;

        bool operator==(const SinglePassStereoState &b) const
        {
            return enabled == b.enabled && independentViewportMask == b.independentViewportMask &&
                   renderTargetIndexOffset == b.renderTargetIndexOffset;
        }

        bool operator!=(const SinglePassStereoState &b) const
        {
            return !(*this == b);
        }

        constexpr SinglePassStereoState &SetEnabled(bool value)
        {
            enabled = value;
            return *this;
        }
        constexpr SinglePassStereoState &SetIndependentViewportMask(bool value)
        {
            independentViewportMask = value;
            return *this;
        }
        constexpr SinglePassStereoState &SetRenderTargetIndexOffset(uint16_t value)
        {
            renderTargetIndexOffset = value;
            return *this;
        }
    };

    struct RenderState
    {
        BlendState blendState;
        DepthStencilState depthStencilState;
        RasterState rasterState;
        SinglePassStereoState singlePassStereo;

        constexpr RenderState &SetBlendState(const BlendState &value)
        {
            blendState = value;
            return *this;
        }
        constexpr RenderState &SetDepthStencilState(const DepthStencilState &value)
        {
            depthStencilState = value;
            return *this;
        }
        constexpr RenderState &SetRasterState(const RasterState &value)
        {
            rasterState = value;
            return *this;
        }
        constexpr RenderState &SetSinglePassStereoState(const SinglePassStereoState &value)
        {
            singlePassStereo = value;
            return *this;
        }
    };

    enum class VariableShadingRate : uint8_t
    {
        e1x1,
        e1x2,
        e2x1,
        e2x2,
        e2x4,
        e4x2,
        e4x4
    };

    enum class ShadingRateCombiner : uint8_t
    {
        Passthrough,
        Override,
        Min,
        Max,
        ApplyRelative
    };

    struct VariableRateShadingState
    {
        bool enabled = false;
        VariableShadingRate shadingRate = VariableShadingRate::e1x1;
        ShadingRateCombiner pipelinePrimitiveCombiner = ShadingRateCombiner::Passthrough;
        ShadingRateCombiner imageCombiner = ShadingRateCombiner::Passthrough;

        bool operator==(const VariableRateShadingState &b) const
        {
            return enabled == b.enabled && shadingRate == b.shadingRate &&
                   pipelinePrimitiveCombiner == b.pipelinePrimitiveCombiner && imageCombiner == b.imageCombiner;
        }

        bool operator!=(const VariableRateShadingState &b) const
        {
            return !(*this == b);
        }

        constexpr VariableRateShadingState &SetEnabled(bool value)
        {
            enabled = value;
            return *this;
        }
        constexpr VariableRateShadingState &SetShadingRate(VariableShadingRate value)
        {
            shadingRate = value;
            return *this;
        }
        constexpr VariableRateShadingState &SetPipelinePrimitiveCombiner(ShadingRateCombiner value)
        {
            pipelinePrimitiveCombiner = value;
            return *this;
        }
        constexpr VariableRateShadingState &SetImageCombiner(ShadingRateCombiner value)
        {
            imageCombiner = value;
            return *this;
        }
    };

    typedef StaticVector<BindingLayoutHandle, MaxBindingLayouts> BindingLayoutVector;

    struct GraphicsPipelineDesc
    {
        PrimitiveType primType = PrimitiveType::TriangleList;
        uint32_t patchControlPoints = 0;
        InputLayoutHandle inputLayout;

        ShaderHandle VS;
        ShaderHandle HS;
        ShaderHandle DS;
        ShaderHandle GS;
        ShaderHandle PS;

        RenderState renderState;
        VariableRateShadingState shadingRateState;

        BindingLayoutVector bindingLayouts;

        GraphicsPipelineDesc &SetPrimType(PrimitiveType value)
        {
            primType = value;
            return *this;
        }
        GraphicsPipelineDesc &SetPatchControlPoints(uint32_t value)
        {
            patchControlPoints = value;
            return *this;
        }
        GraphicsPipelineDesc &SetInputLayout(InputLayout *value)
        {
            inputLayout = value;
            return *this;
        }
        GraphicsPipelineDesc &SetVertexShader(Shader *value)
        {
            VS = value;
            return *this;
        }
        GraphicsPipelineDesc &SetHullShader(Shader *value)
        {
            HS = value;
            return *this;
        }
        GraphicsPipelineDesc &SetTessellationControlShader(Shader *value)
        {
            HS = value;
            return *this;
        }
        GraphicsPipelineDesc &SetDomainShader(Shader *value)
        {
            DS = value;
            return *this;
        }
        GraphicsPipelineDesc &SetTessellationEvaluationShader(Shader *value)
        {
            DS = value;
            return *this;
        }
        GraphicsPipelineDesc &SetGeometryShader(Shader *value)
        {
            GS = value;
            return *this;
        }
        GraphicsPipelineDesc &SetPixelShader(Shader *value)
        {
            PS = value;
            return *this;
        }
        GraphicsPipelineDesc &SetFragmentShader(Shader *value)
        {
            PS = value;
            return *this;
        }
        GraphicsPipelineDesc &SetRenderState(const RenderState &value)
        {
            renderState = value;
            return *this;
        }
        GraphicsPipelineDesc &SetVariableRateShadingState(const VariableRateShadingState &value)
        {
            shadingRateState = value;
            return *this;
        }
        GraphicsPipelineDesc &AddBindingLayout(BindingLayout *layout)
        {
            bindingLayouts.push_back(layout);
            return *this;
        }
    };

    class GraphicsPipeline : public Resource
    {
      public:
        [[nodiscard]] virtual const GraphicsPipelineDesc &GetDesc() const = 0;
        [[nodiscard]] virtual const FramebufferInfo &GetFramebufferInfo() const = 0;
    };

    typedef RefCountPtr<GraphicsPipeline> GraphicsPipelineHandle;

    struct ComputePipelineDesc
    {
        ShaderHandle CS;

        BindingLayoutVector bindingLayouts;

        ComputePipelineDesc &SetComputeShader(Shader *value)
        {
            CS = value;
            return *this;
        }
        ComputePipelineDesc &AddBindingLayout(BindingLayout *layout)
        {
            bindingLayouts.push_back(layout);
            return *this;
        }
    };

    class ComputePipeline : public Resource
    {
      public:
        [[nodiscard]] virtual const ComputePipelineDesc &GetDesc() const = 0;
    };

    typedef RefCountPtr<ComputePipeline> ComputePipelineHandle;

    struct MeshletPipelineDesc
    {
        PrimitiveType primType = PrimitiveType::TriangleList;

        ShaderHandle AS;
        ShaderHandle MS;
        ShaderHandle PS;

        RenderState renderState;

        BindingLayoutVector bindingLayouts;

        MeshletPipelineDesc &SetPrimType(PrimitiveType value)
        {
            primType = value;
            return *this;
        }
        MeshletPipelineDesc &SetTaskShader(Shader *value)
        {
            AS = value;
            return *this;
        }
        MeshletPipelineDesc &SetAmplificationShader(Shader *value)
        {
            AS = value;
            return *this;
        }
        MeshletPipelineDesc &SetMeshShader(Shader *value)
        {
            MS = value;
            return *this;
        }
        MeshletPipelineDesc &SetPixelShader(Shader *value)
        {
            PS = value;
            return *this;
        }
        MeshletPipelineDesc &SetFragmentShader(Shader *value)
        {
            PS = value;
            return *this;
        }
        MeshletPipelineDesc &SetRenderState(const RenderState &value)
        {
            renderState = value;
            return *this;
        }
        MeshletPipelineDesc &AddBindingLayout(BindingLayout *layout)
        {
            bindingLayouts.push_back(layout);
            return *this;
        }
    };

    class MeshletPipeline : public Resource
    {
      public:
        [[nodiscard]] virtual const MeshletPipelineDesc &GetDesc() const = 0;
        [[nodiscard]] virtual const FramebufferInfo &GetFramebufferInfo() const = 0;
    };

    typedef RefCountPtr<MeshletPipeline> MeshletPipelineHandle;

    //////////////////////////////////////////////////////////////////////////
    // Draw and Dispatch
    //////////////////////////////////////////////////////////////////////////

    class EventQuery : public Resource
    {
    };
    typedef RefCountPtr<EventQuery> EventQueryHandle;

    class TimerQuery : public Resource
    {
    };
    typedef RefCountPtr<TimerQuery> TimerQueryHandle;

    struct VertexBufferBinding
    {
        Buffer *buffer = nullptr;
        uint32_t slot;
        uint64_t offset;

        bool operator==(const VertexBufferBinding &b) const
        {
            return buffer == b.buffer && slot == b.slot && offset == b.offset;
        }
        bool operator!=(const VertexBufferBinding &b) const
        {
            return !(*this == b);
        }

        VertexBufferBinding &SetBuffer(Buffer *value)
        {
            buffer = value;
            return *this;
        }
        VertexBufferBinding &SetSlot(uint32_t value)
        {
            slot = value;
            return *this;
        }
        VertexBufferBinding &SetOffset(uint64_t value)
        {
            offset = value;
            return *this;
        }
    };

    struct IndexBufferBinding
    {
        Buffer *buffer = nullptr;
        Format format;
        uint32_t offset;

        bool operator==(const IndexBufferBinding &b) const
        {
            return buffer == b.buffer && format == b.format && offset == b.offset;
        }
        bool operator!=(const IndexBufferBinding &b) const
        {
            return !(*this == b);
        }

        IndexBufferBinding &SetBuffer(Buffer *value)
        {
            buffer = value;
            return *this;
        }
        IndexBufferBinding &SetFormat(Format value)
        {
            format = value;
            return *this;
        }
        IndexBufferBinding &SetOffset(uint32_t value)
        {
            offset = value;
            return *this;
        }
    };

    typedef StaticVector<BindingSet *, MaxBindingLayouts> BindingSetVector;

    struct GraphicsState
    {
        GraphicsPipeline *pipeline = nullptr;
        Framebuffer *framebuffer = nullptr;
        ViewportState viewport;
        VariableRateShadingState shadingRateState;
        Color blendConstantColor{};
        uint8_t dynamicStencilRefValue = 0;

        BindingSetVector bindings;

        StaticVector<VertexBufferBinding, MaxVertexAttributes> vertexBuffers;
        IndexBufferBinding indexBuffer;

        Buffer *indirectParams = nullptr;
        Buffer *indirectCountBuffer = nullptr;

        GraphicsState &SetPipeline(GraphicsPipeline *value)
        {
            pipeline = value;
            return *this;
        }
        GraphicsState &SetFramebuffer(Framebuffer *value)
        {
            framebuffer = value;
            return *this;
        }
        GraphicsState &SetViewport(const ViewportState &value)
        {
            viewport = value;
            return *this;
        }
        GraphicsState &SetShadingRateState(const VariableRateShadingState &value)
        {
            shadingRateState = value;
            return *this;
        }
        GraphicsState &SetBlendColor(const Color &value)
        {
            blendConstantColor = value;
            return *this;
        }
        GraphicsState &SetDynamicStencilRefValue(uint8_t value)
        {
            dynamicStencilRefValue = value;
            return *this;
        }
        GraphicsState &AddBindingSet(BindingSet *value)
        {
            bindings.push_back(value);
            return *this;
        }
        GraphicsState &AddVertexBuffer(const VertexBufferBinding &value)
        {
            vertexBuffers.push_back(value);
            return *this;
        }
        GraphicsState &SetIndexBuffer(const IndexBufferBinding &value)
        {
            indexBuffer = value;
            return *this;
        }
        GraphicsState &SetIndirectParams(Buffer *value)
        {
            indirectParams = value;
            return *this;
        }
        GraphicsState &SetIndirectCountBuffer(Buffer *value)
        {
            indirectCountBuffer = value;
            return *this;
        }
    };

    struct DrawArguments
    {
        uint32_t vertexCount = 0;
        uint32_t instanceCount = 1;
        uint32_t startIndexLocation = 0;
        uint32_t startVertexLocation = 0;
        uint32_t startInstanceLocation = 0;

        constexpr DrawArguments &SetVertexCount(uint32_t value)
        {
            vertexCount = value;
            return *this;
        }
        constexpr DrawArguments &SetInstanceCount(uint32_t value)
        {
            instanceCount = value;
            return *this;
        }
        constexpr DrawArguments &SetStartIndexLocation(uint32_t value)
        {
            startIndexLocation = value;
            return *this;
        }
        constexpr DrawArguments &SetStartVertexLocation(uint32_t value)
        {
            startVertexLocation = value;
            return *this;
        }
        constexpr DrawArguments &SetStartInstanceLocation(uint32_t value)
        {
            startInstanceLocation = value;
            return *this;
        }
    };

    struct DrawIndirectArguments
    {
        uint32_t vertexCount = 0;
        uint32_t instanceCount = 1;
        uint32_t startVertexLocation = 0;
        uint32_t startInstanceLocation = 0;

        constexpr DrawIndirectArguments &SetVertexCount(uint32_t value)
        {
            vertexCount = value;
            return *this;
        }
        constexpr DrawIndirectArguments &SetInstanceCount(uint32_t value)
        {
            instanceCount = value;
            return *this;
        }
        constexpr DrawIndirectArguments &SetStartVertexLocation(uint32_t value)
        {
            startVertexLocation = value;
            return *this;
        }
        constexpr DrawIndirectArguments &SetStartInstanceLocation(uint32_t value)
        {
            startInstanceLocation = value;
            return *this;
        }
    };

    struct DrawIndexedIndirectArguments
    {
        uint32_t indexCount = 0;
        uint32_t instanceCount = 1;
        uint32_t startIndexLocation = 0;
        int32_t baseVertexLocation = 0;
        uint32_t startInstanceLocation = 0;

        constexpr DrawIndexedIndirectArguments &SetIndexCount(uint32_t value)
        {
            indexCount = value;
            return *this;
        }
        constexpr DrawIndexedIndirectArguments &SetInstanceCount(uint32_t value)
        {
            instanceCount = value;
            return *this;
        }
        constexpr DrawIndexedIndirectArguments &SetStartIndexLocation(uint32_t value)
        {
            startIndexLocation = value;
            return *this;
        }
        constexpr DrawIndexedIndirectArguments &SetBaseVertexLocation(int32_t value)
        {
            baseVertexLocation = value;
            return *this;
        }
        constexpr DrawIndexedIndirectArguments &SetStartInstanceLocation(uint32_t value)
        {
            startInstanceLocation = value;
            return *this;
        }
    };

    struct ComputeState
    {
        ComputePipeline *pipeline = nullptr;

        BindingSetVector bindings;

        Buffer *indirectParams = nullptr;

        ComputeState &SetPipeline(ComputePipeline *value)
        {
            pipeline = value;
            return *this;
        }
        ComputeState &AddBindingSet(BindingSet *value)
        {
            bindings.push_back(value);
            return *this;
        }
        ComputeState &SetIndirectParams(Buffer *value)
        {
            indirectParams = value;
            return *this;
        }
    };

    struct DispatchIndirectArguments
    {
        uint32_t groupsX = 1;
        uint32_t groupsY = 1;
        uint32_t groupsZ = 1;

        constexpr DispatchIndirectArguments &SetGroupsX(uint32_t value)
        {
            groupsX = value;
            return *this;
        }
        constexpr DispatchIndirectArguments &SetGroupsY(uint32_t value)
        {
            groupsY = value;
            return *this;
        }
        constexpr DispatchIndirectArguments &SetGroupsZ(uint32_t value)
        {
            groupsZ = value;
            return *this;
        }
        constexpr DispatchIndirectArguments &SetGroups2D(uint32_t x, uint32_t y)
        {
            groupsX = x;
            groupsY = y;
            return *this;
        }
        constexpr DispatchIndirectArguments &SetGroups3D(uint32_t x, uint32_t y, uint32_t z)
        {
            groupsX = x;
            groupsY = y;
            groupsZ = z;
            return *this;
        }
    };

    struct MeshletState
    {
        MeshletPipeline *pipeline = nullptr;
        Framebuffer *framebuffer = nullptr;
        ViewportState viewport;
        Color blendConstantColor{};
        uint8_t dynamicStencilRefValue = 0;

        BindingSetVector bindings;

        Buffer *indirectParams = nullptr;
        Buffer *indirectCountBuffer = nullptr;

        MeshletState &SetPipeline(MeshletPipeline *value)
        {
            pipeline = value;
            return *this;
        }
        MeshletState &SetFramebuffer(Framebuffer *value)
        {
            framebuffer = value;
            return *this;
        }
        MeshletState &SetViewport(const ViewportState &value)
        {
            viewport = value;
            return *this;
        }
        MeshletState &SetBlendColor(const Color &value)
        {
            blendConstantColor = value;
            return *this;
        }
        MeshletState &AddBindingSet(BindingSet *value)
        {
            bindings.push_back(value);
            return *this;
        }
        MeshletState &SetIndirectParams(Buffer *value)
        {
            indirectParams = value;
            return *this;
        }
        MeshletState &SetIndirectCountBuffer(Buffer *value)
        {
            indirectCountBuffer = value;
            return *this;
        }
        MeshletState &SetDynamicStencilRefValue(uint8_t value)
        {
            dynamicStencilRefValue = value;
            return *this;
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // Ray Tracing
    //////////////////////////////////////////////////////////////////////////

    namespace RT
    {
        struct PipelineShaderDesc
        {
            std::string exportName;
            ShaderHandle shader;
            BindingLayoutHandle bindingLayout;

            PipelineShaderDesc &SetExportName(const std::string &value)
            {
                exportName = value;
                return *this;
            }
            PipelineShaderDesc &SetShader(Shader *value)
            {
                shader = value;
                return *this;
            }
            PipelineShaderDesc &SetBindingLayout(BindingLayout *value)
            {
                bindingLayout = value;
                return *this;
            }
        };

        struct PipelineHitGroupDesc
        {
            std::string exportName;
            ShaderHandle closestHitShader;
            ShaderHandle anyHitShader;
            ShaderHandle intersectionShader;
            BindingLayoutHandle bindingLayout;
            bool isProceduralPrimitive = false;

            PipelineHitGroupDesc &SetExportName(const std::string &value)
            {
                exportName = value;
                return *this;
            }
            PipelineHitGroupDesc &SetClosestHitShader(Shader *value)
            {
                closestHitShader = value;
                return *this;
            }
            PipelineHitGroupDesc &SetAnyHitShader(Shader *value)
            {
                anyHitShader = value;
                return *this;
            }
            PipelineHitGroupDesc &SetIntersectionShader(Shader *value)
            {
                intersectionShader = value;
                return *this;
            }
            PipelineHitGroupDesc &SetBindingLayout(BindingLayout *value)
            {
                bindingLayout = value;
                return *this;
            }
            PipelineHitGroupDesc &SetIsProceduralPrimitive(bool value)
            {
                isProceduralPrimitive = value;
                return *this;
            }
        };

        struct PipelineDesc
        {
            std::vector<PipelineShaderDesc> shaders;
            std::vector<PipelineHitGroupDesc> hitGroups;
            BindingLayoutVector globalBindingLayouts;
            uint32_t maxPayloadSize = 0;
            uint32_t maxAttributeSize = sizeof(float) * 2; // typical case: float2 uv;
            uint32_t maxRecursionDepth = 1;
            int32_t hlslExtensionsUAV = -1;
            bool allowOpacityMicromaps = false;

            PipelineDesc &AddShader(const PipelineShaderDesc &value)
            {
                shaders.push_back(value);
                return *this;
            }
            PipelineDesc &AddHitGroup(const PipelineHitGroupDesc &value)
            {
                hitGroups.push_back(value);
                return *this;
            }
            PipelineDesc &AddBindingLayout(BindingLayout *value)
            {
                globalBindingLayouts.push_back(value);
                return *this;
            }
            PipelineDesc &SetMaxPayloadSize(uint32_t value)
            {
                maxPayloadSize = value;
                return *this;
            }
            PipelineDesc &SetMaxAttributeSize(uint32_t value)
            {
                maxAttributeSize = value;
                return *this;
            }
            PipelineDesc &SetMaxRecursionDepth(uint32_t value)
            {
                maxRecursionDepth = value;
                return *this;
            }
            PipelineDesc &SetHlslExtensionsUAV(int32_t value)
            {
                hlslExtensionsUAV = value;
                return *this;
            }
            PipelineDesc &SetAllowOpacityMicromaps(bool value)
            {
                allowOpacityMicromaps = value;
                return *this;
            }
        };

        class Pipeline;

        struct ShaderTableDesc
        {
            // Controls the memory usage and building behavior of the shader table.
            //
            // - When a shader table is cached, it creates an additional buffer that holds the built shader table.
            //   This buffer is updated in CommandList::setRayTracingState after the shader table is modified.
            // - When a shader table is uncached, this buffer is suballocated from the upload manager when the shader
            //   table is first used in CommandList::setRayTracingState after opening a command list, and reallocated
            //   and rebuilt on subsequent calls to setRayTracingState if the shader table is modified.
            //
            // The legacy and default behavior is uncached.
            // It is recommended to enable caching for large and infrequently updated shader tables.
            bool isCached = false;

            // Maximum number of entries in a cached shader table.
            // Must be nonzero when isCached == true.
            // Ignored when isCached == false.
            uint32_t maxEntries = 0;

            std::string debugName;

            ShaderTableDesc &SetIsCached(bool value)
            {
                isCached = value;
                return *this;
            }
            ShaderTableDesc &SetMaxEntries(uint32_t value)
            {
                maxEntries = value;
                return *this;
            }
            ShaderTableDesc &SetDebugName(const std::string &value)
            {
                debugName = value;
                return *this;
            }
            ShaderTableDesc &EnableCaching(uint32_t _maxEntries)
            {
                isCached = true;
                maxEntries = _maxEntries;
                return *this;
            }
        };

        class ShaderTable : public Resource
        {
          public:
            virtual ShaderTableDesc const &GetDesc() const = 0;
            virtual uint32_t GetNumEntries() const = 0;
            virtual Pipeline *GetPipeline() const = 0;
            virtual void SetRayGenerationShader(const char *exportName, BindingSet *bindings = nullptr) = 0;
            virtual int AddMissShader(const char *exportName, BindingSet *bindings = nullptr) = 0;
            virtual int AddHitGroup(const char *exportName, BindingSet *bindings = nullptr) = 0;
            virtual int AddCallableShader(const char *exportName, BindingSet *bindings = nullptr) = 0;
            virtual void ClearMissShaders() = 0;
            virtual void ClearHitShaders() = 0;
            virtual void ClearCallableShaders() = 0;
        };

        typedef RefCountPtr<ShaderTable> ShaderTableHandle;

        class Pipeline : public Resource
        {
          public:
            [[nodiscard]] virtual const RT::PipelineDesc &GetDesc() const = 0;
            virtual ShaderTableHandle CreateShaderTable(ShaderTableDesc const &desc = ShaderTableDesc()) = 0;
        };

        typedef RefCountPtr<Pipeline> PipelineHandle;

        struct State
        {
            ShaderTable *shaderTable = nullptr;

            BindingSetVector bindings;

            State &SetShaderTable(ShaderTable *value)
            {
                shaderTable = value;
                return *this;
            }
            State &AddBindingSet(BindingSet *value)
            {
                bindings.push_back(value);
                return *this;
            }
        };

        struct DispatchRaysArguments
        {
            uint32_t width = 1;
            uint32_t height = 1;
            uint32_t depth = 1;

            constexpr DispatchRaysArguments &SetWidth(uint32_t value)
            {
                width = value;
                return *this;
            }
            constexpr DispatchRaysArguments &SetHeight(uint32_t value)
            {
                height = value;
                return *this;
            }
            constexpr DispatchRaysArguments &SetDepth(uint32_t value)
            {
                depth = value;
                return *this;
            }
            constexpr DispatchRaysArguments &SetDimensions(uint32_t w, uint32_t h = 1, uint32_t d = 1)
            {
                width = w;
                height = h;
                depth = d;
                return *this;
            }
        };
    }

    //////////////////////////////////////////////////////////////////////////
    // Linear Algebra / Cooperative Vectors
    //////////////////////////////////////////////////////////////////////////

    namespace CoopVec
    {
        enum class DataType
        {
            UInt8,
            SInt8,
            UInt8Packed,
            SInt8Packed,
            UInt16,
            SInt16,
            UInt32,
            SInt32,
            UInt64,
            SInt64,
            FloatE4M3,
            FloatE5M2,
            Float16,
            BFloat16,
            Float32,
            Float64
        };

        enum class MatrixLayout
        {
            RowMajor,
            ColumnMajor,
            InferencingOptimal,
            TrainingOptimal
        };

        // Describes a combination of input and output data types for CoopVec matrix multiplication.
        struct MatMulFormatCombo
        {
            // Raw storage element type. May differ from inputInterpretation for packed or interpreted formats.
            DataType inputType;
            DataType inputInterpretation;
            DataType matrixInterpretation;
            DataType biasInterpretation;
            DataType outputType;
            bool transposeSupported;
        };

        // Result of queryCoopVecMatMulFormatSupport.
        struct MatMulFormatSupport
        {
            bool supported = false;
            bool transposeSupported = false;
            bool emulatedInputs = false;
        };

        // Result of queryCoopVecTrainingFormatSupport.
        struct TrainingFormatSupport
        {
            // True if the backend supports a complete buffer training path for the queried accumulation type.
            bool bufferTrainingSupported = false;

            bool threadOuterProductSupported = false;
            bool bufferAccumulateStoreSupported = false;
            bool groupSharedAccumulateStoreSupported = false;
        };

        // Deprecated aggregate CoopVec support information.
        // New code should use queryCoopVecMatMulFormatSupport(...) and
        // queryCoopVecTrainingFormatSupport(...) instead.
        struct DeviceFeatures
        {
            // Format combinations supported by the device for CoopVec matrix multiplication.
            std::vector<MatMulFormatCombo> matMulFormats;

            // True if the backend supports a complete buffer training path for Float16 accumulation.
            bool trainingFloat16 = false;

            // True if the backend supports a complete buffer training path for Float32 accumulation.
            bool trainingFloat32 = false;
        };

        struct MatrixLayoutDesc
        {
            // Buffer where the matrix is stored.
            Physara::RHI::Buffer *buffer = nullptr;

            // Offset in bytes from the start of the buffer where the matrix starts.
            uint64_t offset = 0;

            // Data type of the matrix elements.
            DataType type = DataType::UInt8;

            // Layout of the matrix in memory.
            MatrixLayout layout = MatrixLayout::RowMajor;

            // Size in bytes of the matrix.
            size_t size = 0;

            // Stride in bytes between rows or columns, depending on the layout.
            // For RowMajor and ColumnMajor layouts, stride may be zero, in which case it is computed automatically.
            // For InferencingOptimal and TrainingOptimal layouts, stride does not matter and should be zero.
            size_t stride = 0;
        };

        // Describes a single matrix layout conversion operation.
        // Used by ICommandList::convertCoopVecMatrices(...)
        struct ConvertMatrixLayoutDesc
        {
            MatrixLayoutDesc src;
            MatrixLayoutDesc dst;

            uint32_t numRows = 0;
            uint32_t numColumns = 0;
        };

        // Returns the size in bytes of a given data type.
        PHYSARA_RHI_API size_t GetDataTypeSize(DataType type);

        // Returns the stride for a given matrix if it's stored in a RowMajor or ColumnMajor layout.
        // For other layouts, returns 0.
        PHYSARA_RHI_API size_t GetOptimalMatrixStride(DataType type, MatrixLayout layout, uint32_t rows,
                                                      uint32_t columns);
    }

    //////////////////////////////////////////////////////////////////////////
    // Misc
    //////////////////////////////////////////////////////////////////////////

    enum class Feature : uint8_t
    {
        ComputeQueue,
        ConservativeRasterization,
        ConstantBufferRanges,
        CopyQueue,
        DeferredCommandLists,
        FastGeometryShader,
        HeapDirectlyIndexed,
        HlslExtensionUAV,
        LinearSweptSpheres,
        Meshlets,
        RayQuery,
        RayTracingAccelStruct,
        RayTracingClusters,
        RayTracingOpacityMicromap,
        RayTracingPipeline,
        RayTracingPositionFetch,
        SamplerFeedback,
        ShaderExecutionReordering,
        ShaderSpecializations,
        SinglePassStereo,
        Spheres,
        VariableRateShading,
        VirtualResources,
        WaveLaneCountMinMax,
        CooperativeVectorInferencing,
        CooperativeVectorTraining,
        EnhancedBarriers, // DX12 only feature
    };

    enum class MessageSeverity : uint8_t
    {
        Info,
        Warning,
        Error,
        Fatal
    };

    enum class CommandQueue : uint8_t
    {
        Graphics = 0,
        Compute,
        Copy,

        Count
    };

    struct VariableRateShadingFeatureInfo
    {
        uint32_t shadingRateImageTileSize;
    };

    struct WaveLaneCountMinMaxFeatureInfo
    {
        uint32_t minWaveLaneCount;
        uint32_t maxWaveLaneCount;
    };

    // IMessageCallback should be implemented by the application.
    class MessageCallback
    {
      protected:
        MessageCallback() = default;
        virtual ~MessageCallback() = default;

      public:
        // NVRHI will call message(...) whenever it needs to signal something.
        // The application is free to ignore the messages, show message boxes, or terminate.
        virtual void Message(MessageSeverity severity, const char *messageText) = 0;

        MessageCallback(const MessageCallback &) = delete;
        MessageCallback(const MessageCallback &&) = delete;
        MessageCallback &operator=(const MessageCallback &) = delete;
        MessageCallback &operator=(const MessageCallback &&) = delete;
    };

    class Device;
    class CommandListLifetimeTracker;

    struct CommandListParameters
    {
        // A command list with enableImmediateExecution = true maps to the immediate context on DX11.
        // Two immediate command lists cannot be open at the same time, which is checked by the validation layer.
        bool enableImmediateExecution = true;

        // Minimum size of memory chunks created to upload data to the device on DX12.
        size_t uploadChunkSize = 64 * 1024;

        // Minimum size of memory chunks created for AS build scratch buffers.
        size_t scratchChunkSize = 64 * 1024;

        // Maximum total memory size used for all AS build scratch buffers owned by this command list.
        size_t scratchMaxMemory = 1024 * 1024 * 1024;

        // Type of the queue that this command list is to be executed on.
        // COPY and COMPUTE queues have limited subsets of methods available.
        CommandQueue queueType = CommandQueue::Graphics;

        // Optional external lifetime tracker to use for this command list.
        // If left nullptr, lifetime will be tracked by the device.
        CommandListLifetimeTracker *lifetimeTracker = nullptr;

        CommandListParameters &SetEnableImmediateExecution(bool value)
        {
            enableImmediateExecution = value;
            return *this;
        }
        CommandListParameters &SetUploadChunkSize(size_t value)
        {
            uploadChunkSize = value;
            return *this;
        }
        CommandListParameters &SetScratchChunkSize(size_t value)
        {
            scratchChunkSize = value;
            return *this;
        }
        CommandListParameters &SetScratchMaxMemory(size_t value)
        {
            scratchMaxMemory = value;
            return *this;
        }
        CommandListParameters &SetQueueType(CommandQueue value)
        {
            queueType = value;
            return *this;
        }
        CommandListParameters &SetLifetimeTracker(CommandListLifetimeTracker *value)
        {
            lifetimeTracker = value;
            return *this;
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // ICommandListLifetimeTracker
    //////////////////////////////////////////////////////////////////////////

    // Manages the lifetime of command list objects in a way that is scalable for multithreading.
    // Each thread that submits work to the GPU (calls IDevice::executeCommandList[s]) should own its own command list tracker
    // for each queue it submits work to.
    // - DX11: Multithreaded work submission is not supported.
    // - DX12, Vulkan: A lifetime tracker can be optionally specified when creating a command list. After submitting a command list,
    //   internal command lists and their referenced resources will be held by the lifetime tracker until work has finished
    //   execution on the GPU. Execute runGarbageCollection frequently to poll the GPU and release resources when possible.
    //   If no lifetime tracker is specified, the Device will add the command list to its own internal lifetime trackers.
    class CommandListLifetimeTracker : public Resource
    {
      public:
        // Releases any command lists that have finished executing on the GPU.
        // This should be called frequently, e.g. once per frame, once per simulation step, etc.
        virtual void RunGarbageCollection() = 0;
    };

    typedef RefCountPtr<CommandListLifetimeTracker> CommandListLifetimeTrackerHandle;

    //////////////////////////////////////////////////////////////////////////
    // ICommandList
    //////////////////////////////////////////////////////////////////////////

    // Represents a sequence of GPU operations.
    // - DX11: All command list objects map to the single immediate context. Only one command list may be in the open
    //   state at any given time, and all command lists must have CommandListParameters::enableImmediateExecution = true.
    // - DX12: One command list object may contain multiple instances of ID3D12GraphicsCommandList* and
    //   ID3D12CommandAllocator objects, reusing older ones as they finish executing on the GPU. A command list object
    //   also contains the upload manager (for suballocating memory from the upload heap on operations such as
    //   writeBuffer) and the DXR scratch manager (for suballocating memory for acceleration structure builds).
    //   The upload and scratch managers' memory is reused when possible, but it is only freed when the command list
    //   object is destroyed. Thus, it might be a good idea to use a dedicated NVRHI command list for uploading large
    //   amounts of data and to destroy it when uploading is finished.
    // - Vulkan: The command list objects don't own the VkCommandBuffer-s but request available ones from the queue
    //   instead. The upload and scratch buffers behave the same way they do on DX12.
    class CommandList : public Resource
    {
      public:
        // Prepares the command list for recording a new sequence of commands.
        // All other methods of ICommandList must only be used when the command list is open.
        // - DX11: The immediate command list may always stay in the open state, although that prohibits other
        //   command lists from opening.
        // - DX12, Vulkan: Creates or reuses the command list or buffer object and the command allocator (DX12),
        //   starts tracking the resources being referenced in the command list.
        virtual void Open() = 0;

        // Finalizes the command list and prepares it for execution.
        // Use IDevice::executeCommandLists(...) to execute it.
        // Re-opening the command list without execution is allowed but not well-tested.
        virtual void Close() = 0;

        // Resets the NVRHI state cache associated with the command list, clears some of the underlying API state.
        // This method is mostly useful when switching from recording commands to the open command list using
        // non-NVRHI code - see getNativeObject(...) - to recording further commands using NVRHI.
        virtual void ClearState() = 0;

        // Clears some or all subresources of the given color texture using the provided color.
        // - DX11/12: The clear operation uses either an RTV or a UAV, depending on the texture usage flags
        //   (isRenderTarget and isUAV).
        // - Vulkan: vkCmdClearColorImage is always used with the Float32 color fields set.
        // At least one of the 'isRenderTarget' and 'isUAV' flags must be set, and the format of the texture
        // must be of a color type.
        virtual void ClearTextureFloat(Texture *t, TextureSubresourceSet subresources, const Color &clearColor) = 0;

        // Clears some or all subresources of the given depth-stencil texture using the provided depth and/or stencil
        // values. The texture must have the isRenderTarget flag set, and its format must be of a depth-stencil type.
        virtual void ClearDepthStencilTexture(Texture *t, TextureSubresourceSet subresources, bool clearDepth,
                                              float depth, bool clearStencil, uint8_t stencil) = 0;

        // Clears some or all subresources of the given color texture using the provided integer value.
        // - DX11/12: If the texture has the isUAV flag set, the clear is performed using ClearUnorderedAccessViewUint.
        //   Otherwise, the clear value is converted to a float, and the texture is cleared as an RTV with all 4
        //   color components using the same value.
        // - Vulkan: vkCmdClearColorImage is always used with the UInt32 and Int32 color fields set.
        virtual void ClearTextureUInt(Texture *t, TextureSubresourceSet subresources, uint32_t clearColor) = 0;

        // Copies a single 2D or 3D region of texture data from texture 'src' into texture 'dst'.
        // The region's dimensions must be compatible between the two textures, meaning that for simple color textures
        // they must be equal, and for reinterpret copies between compressed and uncompressed textures, they must differ
        // by a factor equal to the block size. The function does not resize textures, only 1:1 pixel copies are
        // supported.
        virtual void CopyTexture(Texture *dest, const TextureSlice &destSlice, Texture *src,
                                 const TextureSlice &srcSlice) = 0;

        // Copies a single 2D or 3D region of texture data from regular texture 'src' into staging texture 'dst'.
        virtual void CopyTexture(StagingTexture *dest, const TextureSlice &destSlice, Texture *src,
                                 const TextureSlice &srcSlice) = 0;

        // Copies a single 2D or 3D region of texture data from staging texture 'src' into regular texture 'dst'.
        virtual void CopyTexture(Texture *dest, const TextureSlice &destSlice, StagingTexture *src,
                                 const TextureSlice &srcSlice) = 0;

        // Uploads the contents of an entire 2D or 3D mip level of a single array slice of the texture from CPU memory.
        // The data in CPU memory must be in the same pixel format as the texture. Pixels in every row must be tightly
        // packed, rows are packed with a stride of 'rowPitch' which must not be 0 unless the texture has a height of 1,
        // and depth slices are packed with a stride of 'depthPitch' which also must not be 0 if the texture is 3D.
        // - DX11: Maps directly to UpdateSubresource.
        // - DX12, Vulkan: A region of the automatic upload buffer is suballocated, data is copied there, and then
        //   copied on the GPU into the destination texture using CopyTextureRegion (DX12) or vkCmdCopyBufferToImage (VK).
        //   The upload buffer region can only be reused when this command list instance finishes executing on the GPU.
        // For more advanced uploading operations, such as updating only a region in the texture, use staging texture
        // objects and copyTexture(...).
        virtual void WriteTexture(Texture *dest, uint32_t arraySlice, uint32_t mipLevel, const void *data,
                                  size_t rowPitch, size_t depthPitch = 0) = 0;

        // Performs a resolve operation to combine samples from some or all subresources of a multisample texture 'src'
        // into matching subresources of a non-multisample texture 'dest'. Both textures' formats must be of color type.
        // - DX11/12: Maps to a sequence of ResolveSubresource calls, one per subresource.
        // - Vulkan: Maps to a single vkCmdResolveImage call.
        virtual void ResolveTexture(Texture *dest, const TextureSubresourceSet &dstSubresources, Texture *src,
                                    const TextureSubresourceSet &srcSubresources) = 0;

        // Uploads 'dataSize' bytes of data from CPU memory into the GPU buffer 'b' at offset 'destOffsetBytes'.
        // - DX11: If the buffer's 'cpuAccess' mode is set to Write, maps the buffer and uploads the data that way.
        //   Otherwise, uses UpdateSubresource.
        // - DX12: If the buffer's 'isVolatile' flag is set, a region of the automatic upload buffer is suballocated,
        //   and the data is copied there. Subsequent uses of the buffer will directly refer to that location in the
        //   upload buffer, until the next call to writeBuffer(...) or until the command list is closed. A volatile
        //   buffer can not be used until writeBuffer(...) is called on it every time after the command list is opened.
        //   If the 'isVolatile' flag is not set, a region of the automatic upload buffer is suballocated, the data
        //   is copied there, and then copied into the real GPU buffer object using CopyBufferRegion.
        // - Vulkan: Similar behavior to DX12, except that each volatile buffer actually has its own Vulkan resource.
        //   The size of such resource is determined by the 'maxVersions' field of the BufferDesc. When writeBuffer(...)
        //   is called on a volatile buffer, a region of that buffer object (a single version) is suballocated, data
        //   is copied there, and subsequent uses of the buffer in the same command list will refer to that version.
        //   For non-volatile buffers, writes of 64 kB or smaller use vkCmdUpdateBuffer. Larger writes suballocate
        //   a portion of the automatic upload buffer and copy the data to the real GPU buffer through that and
        //   vkCmdCopyBuffer.
        virtual void WriteBuffer(Buffer *b, const void *data, size_t dataSize, uint64_t destOffsetBytes = 0) = 0;

        // Fills the entire buffer using the provided uint32 value.
        // - DX11/12: Maps to ClearUnorderedAccessViewUint.
        // - Vulkan: Maps to vkCmdFillBuffer.
        virtual void ClearBufferUInt(Buffer *b, uint32_t clearValue) = 0;

        // Copies 'dataSizeBytes' of data from buffer 'src' at offset 'srcOffsetBytes' into buffer 'dest' at offset
        // 'destOffsetBytes'. The source and destination regions must be within the sizes of the respective buffers.
        // - DX11: Maps to CopySubresourceRegion.
        // - DX12: Maps to CopyBufferRegion.
        // - Vulkan: Maps to vkCmdCopyBuffer.
        virtual void CopyBuffer(Buffer *dest, uint64_t destOffsetBytes, Buffer *src, uint64_t srcOffsetBytes,
                                uint64_t dataSizeBytes) = 0;

        // Clears the entire sampler feedback texture.
        // - DX12: Maps to ClearUnorderedAccessViewUint.
        // - DX11, Vulkan: Unsupported.
        virtual void ClearSamplerFeedbackTexture(SamplerFeedbackTexture *texture) = 0;

        // Decodes the sampler feedback texture into an application-usable format, storing data into the provided buffer.
        // The 'format' parameter should be Format::R8_UINT.
        // - DX12: Maps to ResolveSubresourceRegion.
        //   See https://microsoft.github.io/DirectX-Specs/d3d/SamplerFeedback.html
        // - DX11, Vulkan: Unsupported.
        virtual void DecodeSamplerFeedbackTexture(Buffer *buffer, SamplerFeedbackTexture *texture,
                                                  Physara::RHI::Format format) = 0;

        // Transitions the sampler feedback texture into the requested state, placing a barrier if necessary.
        // The barrier is appended into the pending barrier list and not issued immediately,
        // instead waiting for any rendering, compute or transfer operation.
        // Use commitBarriers() to issue the barriers explicitly.
        // Like the other sampler feedback functions, only supported on DX12.
        virtual void SetSamplerFeedbackTextureState(SamplerFeedbackTexture *texture, ResourceStates stateBits) = 0;

        // Writes the provided data into the push constants block for the currently set pipeline.
        // A graphics, compute, ray tracing or meshlet state must be set using the corresponding call
        // (setGraphicsState etc.) before using setPushConstants. Changing the state invalidates push constants.
        // - DX11: Push constants for all pipelines and command lists use a single buffer associated with the
        //   NVRHI context. This function maps to UpdateSubresource on that buffer.
        // - DX12: Push constants map to root constants in the PSO/root signature. This function maps to
        //   SetGraphicsRoot32BitConstants for graphics or meshlet pipelines, and SetComputeRoot32BitConstants for
        //   compute or ray tracing pipelines.
        // - Vulkan: Push constants are just Vulkan push constants. This function maps to vkCmdPushConstants.
        // Note that NVRHI only supports one push constants binding in all layouts used in a pipeline.
        virtual void SetPushConstants(const void *data, size_t byteSize) = 0;

        // Sets the specified graphics state on the command list.
        // The state includes the pipeline (or individual shaders on DX11) and all resources bound to it,
        // from input buffers to render targets. See the members of GraphicsState for more information.
        // State is cached by NVRHI, so if some parts of it are not modified by the setGraphicsState(...) call,
        // the corresponding changes won't be made on the underlying graphics API. When combining command list
        // operations made through NVRHI and through direct access to the command list, state caching may lead to
        // incomplete or incorrect state being set on the underlying API because of cache mismatch with the actual
        // state. To avoid these issues, call clearState() when switching from direct command list access to NVRHI.
        virtual void SetGraphicsState(const GraphicsState &state) = 0;

        // Draws non-indexed primitives using the current graphics state.
        // setGraphicsState(...) must be called between opening the command list or using other types of pipelines
        // and calling draw(...) or any of its siblings. If the pipeline uses push constants, those must be set
        // using setPushConstants(...) between setGraphicsState(...) and draw(...). If the pipeline uses volatile
        // constant buffers, their contents must be written using writeBuffer(...) between open(...) and draw(...),
        // which may be before or after setGraphicsState(...).
        // - DX11/12: Maps to DrawInstanced.
        // - Vulkan: Maps to vkCmdDraw.
        virtual void Draw(const DrawArguments &args) = 0;

        // Draws indexed primitives using the current graphics state.
        // See the comment to draw(...) for state information.
        // - DX11/12: Maps to DrawIndexedInstanced.
        // - Vulkan: Maps to vkCmdDrawIndexed.
        virtual void DrawIndexed(const DrawArguments &args) = 0;

        // Draws one or multiple sets of non-indexed primitives using the parameters provided in the indirect buffer
        // specified in the prior call to setGraphicsState(...). The memory layout in the buffer is the same for all
        // graphics APIs and is described by the DrawIndirectArguments structure. If drawCount is more than 1,
        // multiple sets of primitives are drawn, and the parameter structures for them are tightly packed in the
        // indirect parameter buffer one after another.
        // See the comment to draw(...) for state information.
        // - DX11: Maps to multiple calls to DrawInstancedIndirect.
        // - DX12: Maps to ExecuteIndirect with a predefined signature.
        // - Vulkan: Maps to vkCmdDrawIndirect.
        virtual void DrawIndirect(uint32_t offsetBytes, uint32_t drawCount = 1) = 0;

        // Draws one or multiple sets of indexed primitives using the parameters provided in the indirect buffer
        // specified in the prior call to setGraphicsState(...). The memory layout in the buffer is the same for all
        // graphics APIs and is described by the DrawIndexedIndirectArguments structure. If drawCount is more than 1,
        // multiple sets of primitives are drawn, and the parameter structures for them are tightly packed in the
        // indirect parameter buffer one after another.
        // See the comment to draw(...) for state information.
        // - DX11: Maps to multiple calls to DrawIndexedInstancedIndirect.
        // - DX12: Maps to ExecuteIndirect with a predefined signature.
        // - Vulkan: Maps to vkCmdDrawIndexedIndirect.
        virtual void DrawIndexedIndirect(uint32_t offsetBytes, uint32_t drawCount = 1) = 0;

        // Draws primitives with indexed vertices using the parameters provided in the indirect arguments buffer
        //   at offset 'paramOffsetBytes'.
        // The draw count is read from the indirectCountBuffer specified in setGraphicsState(...)
        //   at offset 'countOffsetBytes'.
        // - DX11: Falls back to drawIndexedIndirect(paramOffsetBytes, maxDrawCount)
        // - DX12: Maps to ExecuteIndirect with pCountBuffer parameter.
        // - Vulkan: Maps to vkCmdDrawIndexedIndirectCount.
        virtual void DrawIndexedIndirectCount(uint32_t paramOffsetBytes, uint32_t countOffsetBytes,
                                              uint32_t maxDrawCount) = 0;

        // Sets the specified compute state on the command list.
        // The state includes the pipeline (or individual shaders on DX11) and all resources bound to it.
        // See the members of ComputeState for more information.
        // See the comment to setGraphicsState(...) for information on state caching.
        virtual void SetComputeState(const ComputeState &state) = 0;

        // Launches a compute kernel using the current compute state.
        // See the comment to draw(...) for information on state setting, push constants, and volatile constant buffers,
        // replacing graphics with compute.
        // - DX11/12: Maps to Dispatch.
        // - Vulkan: Maps to vkCmdDispatch.
        virtual void Dispatch(uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1) = 0;

        // Launches a compute kernel using the parameters provided in the indirect buffer specified in the prior
        // call to setComputeState(...). The memory layout in the buffer is the same for all graphics APIs and is
        // described by the DispatchIndirectArguments structure.
        // See the comment to dispatch(...) for state information.
        // - DX11: Maps to DispatchIndirect.
        // - DX12: Maps to ExecuteIndirect with a predefined signature.
        // - Vulkan: Maps to vkCmdDispatchIndirect.
        virtual void DispatchIndirect(uint32_t offsetBytes) = 0;

        // Sets the specified meshlet rendering state on the command list.
        // The state includes the pipeline and all resources bound to it.
        // Not supported on DX11.
        // Meshlet support on DX12 and Vulkan can be queried using IDevice::queryFeatureSupport(Feature::Meshlets).
        // See the members of MeshletState for more information.
        // See the comment to setGraphicsState(...) for information on state caching.
        virtual void SetMeshletState(const MeshletState &state) = 0;

        // Draws meshlet primitives using the current meshlet state.
        // See the comment to draw(...) for information on state setting, push constants, and volatile constant buffers,
        // replacing graphics with meshlets.
        // - DX11: Not supported.
        // - DX12: Maps to DispatchMesh.
        // - Vulkan: Maps to vkCmdDispatchMesh.
        virtual void DispatchMesh(uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1) = 0;

        // Draws meshlet primitives using the parameters provided in the indirect buffer specified in the prior
        // call to setMeshletState(...). The memory layout in the buffer is the same for all graphics APIs and is
        // described by the DispatchIndirectArguments structure.
        // See the comment to dispatchMesh(...) for state information.
        // - DX11: Not supported.
        // - DX12: Maps to ExecuteIndirect with a predefined signature.
        // - Vulkan: Maps to vkCmdDrawMeshTasksIndirectEXT.
        virtual void DispatchMeshIndirect(uint32_t offsetBytes, uint32_t maxDrawCount) = 0;

        // Draws meshlet primitives using the parameters provided in the indirect buffer specified in the prior
        // call to setMeshletState(...).
        // The draw count is read from the indirectCountBuffer specified in setMeshletState(...)
        //   at offset 'countOffsetBytes'.
        // - DX11: Not supported.
        // - DX12: Not supported.
        // - Vulkan: Maps to vkCmdDrawMeshTasksIndirectCountEXT.
        virtual void DispatchMeshIndirectCount(uint32_t paramOffsetBytes, uint32_t countOffsetBytes,
                                               uint32_t maxDrawCount) = 0;

        // Sets the specified ray tracing state on the command list.
        // The state includes the shader table, which references the pipeline, and all bound resources.
        // Not supported on DX11.
        // See the members of rt::State for more information.
        // See the comment to setGraphicsState(...) for information on state caching.
        virtual void SetRayTracingState(const RT::State &state) = 0;

        // Launches a grid of ray generation shader threads using the current ray tracing state.
        // The ray generation shader to use is specified by the shader table, which currently supports only one
        // ray generation shader. There may be multiple shaders of all other ray tracing types in the shader table.
        // See the comment to draw(...) for information on state setting, push constants, and volatile constant buffers,
        // replacing graphics with ray tracing.
        // - DX11: Not supported.
        // - DX12: Maps to DispatchRays.
        // - Vulkan: Maps to vkCmdTraceRaysKHR.
        virtual void DispatchRays(const RT::DispatchRaysArguments &args) = 0;

        // Launches an opacity micromap (OMM) build kernel.
        // A temporary memory region for the build is suballocated using the scratch buffer manager attached to the
        // command list. The size of this memory region is determined automatically inside this function.
        // - DX11: Not supported.
        // - DX12: Maps to NvAPI_D3D12_BuildRaytracingOpacityMicromapArray and requires NVAPI.
        // - Vulkan: Maps to vkCmdBuildMicromapsEXT.
        virtual void BuildOpacityMicromap(RT::OpacityMicromap *omm, const RT::OpacityMicromapDesc &desc) = 0;

        // Builds or updates a bottom-level ray tracing acceleration structure (BLAS).
        // A temporary memory region for the build is suballocated using the scratch buffer manager attached to the
        // command list. The size of this memory region is determined automatically inside this function.
        // The type of operation to perform is specified by the buildFlags parameter.
        // When building a new BLAS, the amount of memory allocated for it must be sufficient to build the BLAS
        // for the provided geometry. Usually this is achieved by passing the same geometry descriptors to this function
        // and to IDevice::createAccelStruct(...).
        // When updating a BLAS, the geometries and primitive counts must match the BLAS that was previously built,
        // and the BLAS must have been built with the AllowUpdate flag.
        // If compaction is enabled when building the BLAS, the BLAS cannot be rebuilt or updated later, it can only
        // be compacted.
        // - DX11: Not supported.
        // - DX12: Maps to BuildRaytracingAccelerationStructure, or NvAPI_D3D12_BuildRaytracingAccelerationStructureEx
        //   if Opacity Micromaps or Line-Swept Sphere geometries are supported by the device.
        // - Vulkan: Maps to vkCmdBuildAccelerationStructuresKHR.
        // If NVRHI is built with RTXMU enabled, all BLAS builds, updates and compactions are handled by RTXMU.
        // Note that RTXMU currently doesn't support OMM or LSS.
        virtual void BuildBottomLevelAccelStruct(
            RT::AccelStruct *as, const RT::GeometryDesc *pGeometries, size_t numGeometries,
            RT::AccelStructBuildFlags buildFlags = RT::AccelStructBuildFlags::None) = 0;

        // Compacts all bottom-level ray tracing acceleration structures (BLASes) that are currently available
        // for compaction. This process is handled by the RTXMU library. If NVRHI is built without RTXMU,
        // this function has no effect.
        virtual void CompactBottomLevelAccelStructs() = 0;

        // Copies a ray tracing acceleration structure to another memory location.
        // This function clones the acceleration structure, creating a copy that can be used independently.
        // - DX11: Not supported.
        // - DX12: Maps to ID3D12GraphicsCommandList4::CopyRaytracingAccelerationStructure with
        //   D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE_CLONE.
        // - Vulkan: Maps to vkCmdCopyAccelerationStructureKHR or vkCmdCopyAccelerationStructureToMemoryKHR.
        virtual void CopyRaytracingAccelerationStructure(RT::AccelStruct *destination, RT::AccelStruct *source) = 0;

        // Builds or updates a top-level ray tracing acceleration structure (TLAS).
        // A temporary memory region for the build is suballocated using the scratch buffer manager attached to the
        // command list. The size of this memory region is determined automatically inside this function.
        // The type of operation to perform is specified by the buildFlags parameter.
        // When building a new TLAS, the amount of memory allocated for it must be sufficient to build the TLAS
        // for the provided geometry. Usually this is achieved by making sure that the instance count does not exceed
        // that provided to IDevice::createAccelStruct(...).
        // When updating a TLAS, the instance counts and types must match the TLAS that was previously built,
        // and the TLAS must have been built with the AllowUpdate flag.
        // - DX11: Not supported.
        // - DX12: Maps to BuildRaytracingAccelerationStructure.
        // - Vulkan: Maps to vkCmdBuildAccelerationStructuresKHR.
        virtual void BuildTopLevelAccelStruct(
            RT::AccelStruct *as, const RT::InstanceDesc *pInstances, size_t numInstances,
            RT::AccelStructBuildFlags buildFlags = RT::AccelStructBuildFlags::None) = 0;

        // Performs one of the supported operations on clustered ray tracing acceleration structures (CLAS).
        // See the comments to rt::cluster::OperationDesc for more information.
        // - DX11: Not supported.
        // - DX12: Maps to NvAPI_D3D12_RaytracingExecuteMultiIndirectClusterOperation and requires NVAPI.
        // - Vulkan: Not supported.
        virtual void ExecuteMultiIndirectClusterOperation(const RT::Cluster::OperationDesc &desc) = 0;

        // Builds or updates a top-level ray tracing acceleration structure (TLAS) using instance data provided
        // through a buffer on the GPU. The buffer must be pre-filled with rt::InstanceDesc structures using a
        // copy operation or a shader. No validation on the buffer contents is performed by NVRHI, and no state
        // or liveness tracking is done for the referenced BLAS'es.
        // See the comment to buildTopLevelAccelStruct(...) for more information.
        // - DX11: Not supported.
        // - DX12: Maps to BuildRaytracingAccelerationStructure.
        // - Vulkan: Maps to vkCmdBuildAccelerationStructuresKHR.
        virtual void BuildTopLevelAccelStructFromBuffer(
            RT::AccelStruct *as, Physara::RHI::Buffer *instanceBuffer, uint64_t instanceBufferOffset,
            size_t numInstances, RT::AccelStructBuildFlags buildFlags = RT::AccelStructBuildFlags::None) = 0;

        // Converts one or several CoopVec compatible matrices between layouts in GPU memory.
        // Source and destination buffers must be different.
        // - DX11: Not supported.
        // - DX12: Maps to ConvertLinearAlgebraMatrix.
        // - Vulkan: Maps to vkCmdConvertCooperativeVectorMatrixNV.
        virtual void ConvertCoopVecMatrices(CoopVec::ConvertMatrixLayoutDesc const *convertDescs, size_t numDescs) = 0;

        // Starts measuring GPU execution time using the provided timer query at this point in the command list.
        // Use endTimerQuery(...) to stop measuring time, and IDevice::getTimerQueryTime(...) to get the results later.
        // The same timer query cannot be used multiple times within the same command list, or in different
        // command lists until it is resolved.
        // - DX11: Maps to Begin and End calls on two ID3D11Query objects.
        // - DX12: Maps to EndQuery.
        // - Vulkan: Maps to vkCmdResetQueryPool and vkCmdWriteTimestamp.
        virtual void BeginTimerQuery(TimerQuery *query) = 0;

        // Stops measuring GPU execution time using the provided timer query at this point in the command list.
        // beginTimerQuery(...) must have been used on the same timer query in this command list previously.
        // - DX11: Maps to End calls on two ID3D11Query objects.
        // - DX12: Maps to EndQuery and ResolveQueryData.
        // - Vulkan: Maps to vkCmdWriteTimestamp.
        virtual void EndTimerQuery(TimerQuery *query) = 0;

        // Places a debug marker denoting the beginning of a range of commands in the command list.
        // Use endMarker() to denote the end of the range. Ranges may be nested, i.e. calling beginMarker(...)
        // multiple times, followed by multiple endMarker(), is allowed.
        // - DX11: Maps to ID3DUserDefinedAnnotation::BeginEvent.
        // - DX12: Maps to PIXBeginEvent.
        // - Vulkan: Maps to cmdBeginDebugUtilsLabelEXT or cmdDebugMarkerBeginEXT.
        // If NSight Aftermath integration is enabled, also calls GFSDK_Aftermath_SetEventMarker on DX11 and DX12.
        virtual void BeginMarker(const char *name) = 0;

        // Places a debug marker denoting the end of a range of commands in the command list.
        // - DX11: Maps to ID3DUserDefinedAnnotation::EndEvent.
        // - DX12: Maps to PIXEndEvent.
        // - Vulkan: Maps to cmdEndDebugUtilsLabelEXT or cmdDebugMarkerEndEXT.
        virtual void EndMarker() = 0;

        // Enables or disables the automatic barrier placement on set[...]State, copy, write, and clear operations.
        // By default, automatic barriers are enabled, but can be optionally disabled to improve CPU performance
        // and/or specific barrier placement. When automatic barriers are disabled, it is application's responsibility
        // to set correct states for all used resources.
        virtual void SetEnableAutomaticBarriers(bool enable) = 0;

        // Sets the necessary resource states for all non-permanent resources used in the binding set.
        virtual void SetResourceStatesForBindingSet(BindingSet *bindingSet) = 0;

        // Sets the necessary resource states for all targets of the framebuffer.
        PHYSARA_RHI_API void SetResourceStatesForFramebuffer(Framebuffer *framebuffer);

        // Enables or disables the placement of UAV barriers for the given texture (DX12/VK) or all resources (DX11)
        // between draw or dispatch calls. Disabling UAV barriers may improve performance in cases when the same
        // resource is used by multiple draws or dispatches, but they don't depend on each other's results.
        // Note that this only affects barriers between multiple uses of the same texture as a UAV, and the
        // transition barrier when the texture is first used as a UAV will still be placed.
        // - DX11: Maps to NvAPI_D3D11_BeginUAVOverlap (once - see source code) and requires NVAPI.
        // - DX12, Vulkan: Does not map to any specific API calls, affects NVRHI automatic barriers.
        virtual void SetEnableUavBarriersForTexture(Texture *texture, bool enableBarriers) = 0;

        // Enables or disables the placement of UAV barriers for the given buffer (DX12/VK) or all resources (DX11)
        // between draw or dispatch calls.
        // See the comment to setEnableUavBarriersForTexture(...) for more information.
        virtual void SetEnableUavBarriersForBuffer(Buffer *buffer, bool enableBarriers) = 0;

        // Informs the command list state tracker of the current state of a texture or some of its subresources.
        // This function must be called after opening the command list and before the first use of any textures
        // that do not have the keepInitialState flag set, and that were not transitioned to a permanent state
        // previously using setPermanentTextureState(...).
        virtual void BeginTrackingTextureState(Texture *texture, TextureSubresourceSet subresources,
                                               ResourceStates stateBits) = 0;

        // Informs the command list state tracker of the current state of a buffer.
        // See the comment to beginTrackingTextureState(...) for more information.
        virtual void BeginTrackingBufferState(Buffer *buffer, ResourceStates stateBits) = 0;

        // Places the necessary barriers to make sure that the texture or some of its subresources are in the given
        // state. If the texture or subresources are already in that state, no action is performed.
        // If the texture was previously transitioned to a permanent state, the new state must be compatible
        // with that permanent state, and no action is performed.
        // The barriers are not immediately submitted to the underlying graphics API, but are placed to the pending
        // list instead. Call commitBarriers() to submit them to the graphics API explicitly or set graphics
        // or other type of state.
        // Has no effect on DX11.
        virtual void SetTextureState(Texture *texture, TextureSubresourceSet subresources,
                                     ResourceStates stateBits) = 0;

        // Places the necessary barriers to make sure that the buffer is in the given state.
        // See the comment to setTextureState(...) for more information.
        // Has no effect on DX11.
        virtual void SetBufferState(Buffer *buffer, ResourceStates stateBits) = 0;

        // Places the necessary barriers to make sure that the underlying buffer for the acceleration structure is
        // in the given state. See the comment to setTextureState(...) for more information.
        // Has no effect on DX11.
        virtual void SetAccelStructState(RT::AccelStruct *as, ResourceStates stateBits) = 0;

        // Places the necessary barriers to make sure that the entire texture is in the given state, and marks that
        // state as the texture's permanent state. Once a texture is transitioned into a permanent state, its state
        // can not be modified. This can improve performance by excluding the texture from automatic state tracking
        // in the future.
        // The barriers are not immediately submitted to the underlying graphics API, but are placed to the pending
        // list instead. Call commitBarriers() to submit them to the graphics API explicitly or set graphics
        // or other type of state.
        // Note that the permanent state transitions affect all command lists, and are only applied when the command
        // list that sets them is executed. If the command list is closed but not executed, the permanent states
        // will be abandoned.
        // Has no effect on DX11.
        virtual void SetPermanentTextureState(Texture *texture, ResourceStates stateBits) = 0;

        // Places the necessary barriers to make sure that the buffer is in the given state, and marks that state
        // as the buffer's permanent state. See the comment to setPermanentTextureState(...) for more information.
        // Has no effect on DX11.
        virtual void SetPermanentBufferState(Buffer *buffer, ResourceStates stateBits) = 0;

        // Flushes the barriers from the pending list into the graphics API command list.
        // Has no effect on DX11.
        virtual void CommitBarriers() = 0;

        // Returns the current tracked state of a texture subresource.
        // If the state is not known to the command list, returns ResourceStates::Unknown. Using the texture in this
        // state is not allowed.
        // On DX11, always returns ResourceStates::Common.
        virtual ResourceStates GetTextureSubresourceState(Texture *texture, ArraySlice arraySlice,
                                                          MipLevel mipLevel) = 0;

        // Returns the current tracked state of a buffer.
        // See the comment to getTextureSubresourceState(...) for more information.
        virtual ResourceStates GetBufferState(Buffer *buffer) = 0;

        // Returns the owning device, does NOT call AddRef on it.
        virtual Device *GetDevice() = 0;

        // Returns the CommandListParameters structure that was used to create the command list.
        virtual const CommandListParameters &GetDesc() = 0;
    };

    typedef RefCountPtr<CommandList> CommandListHandle;

    //////////////////////////////////////////////////////////////////////////
    // IDevice
    //////////////////////////////////////////////////////////////////////////

    class AftermathCrashDumpHelper;

    class Device : public Resource
    {
      public:
        virtual HeapHandle CreateHeap(const HeapDesc &d) = 0;

        // Optional TLAS prebuild query. See doc/memory-queries.md for the contract
        // and backend limitations.
        virtual bool QueryTopLevelAccelStructPrebuildInfo(const RT::AccelStructDesc &desc, uint32_t instanceCount,
                                                          RT::AccelStructPrebuildInfo &outInfo) = 0;

        virtual TextureHandle CreateTexture(const TextureDesc &d) = 0;
        virtual MemoryRequirements GetTextureMemoryRequirements(Texture *texture) = 0;
        virtual bool BindTextureMemory(Texture *texture, Heap *heap, uint64_t offset) = 0;

        virtual TextureHandle CreateHandleForNativeTexture(ObjectType objectType, Object texture,
                                                           const TextureDesc &desc) = 0;

        virtual StagingTextureHandle CreateStagingTexture(const TextureDesc &d, CpuAccessMode cpuAccess) = 0;
        virtual void *MapStagingTexture(StagingTexture *tex, const TextureSlice &slice, CpuAccessMode cpuAccess,
                                        size_t *outRowPitch) = 0;
        virtual void UnmapStagingTexture(StagingTexture *tex) = 0;

        virtual void GetTextureTiling(Texture *texture, uint32_t *numTiles, PackedMipDesc *desc, TileShape *tileShape,
                                      uint32_t *subresourceTilingsNum, SubresourceTiling *subresourceTilings) = 0;
        virtual void UpdateTextureTileMappings(Texture *texture, const TextureTilesMapping *tileMappings,
                                               uint32_t numTileMappings,
                                               CommandQueue executionQueue = CommandQueue::Graphics) = 0;

        virtual SamplerFeedbackTextureHandle CreateSamplerFeedbackTexture(Texture *pairedTexture,
                                                                          const SamplerFeedbackTextureDesc &desc) = 0;
        virtual SamplerFeedbackTextureHandle CreateSamplerFeedbackForNativeTexture(ObjectType objectType,
                                                                                   Object texture,
                                                                                   Texture *pairedTexture) = 0;

        virtual BufferHandle CreateBuffer(const BufferDesc &d) = 0;
        virtual void *MapBuffer(Buffer *buffer, CpuAccessMode cpuAccess) = 0;
        virtual void UnmapBuffer(Buffer *buffer) = 0;
        virtual MemoryRequirements GetBufferMemoryRequirements(Buffer *buffer) = 0;
        virtual bool BindBufferMemory(Buffer *buffer, Heap *heap, uint64_t offset) = 0;

        virtual BufferHandle CreateHandleForNativeBuffer(ObjectType objectType, Object buffer,
                                                         const BufferDesc &desc) = 0;

        virtual ShaderHandle CreateShader(const ShaderDesc &d, const void *binary, size_t binarySize) = 0;
        virtual ShaderHandle CreateShaderSpecialization(Shader *baseShader, const ShaderSpecialization *constants,
                                                        uint32_t numConstants) = 0;
        virtual ShaderLibraryHandle CreateShaderLibrary(const void *binary, size_t binarySize) = 0;

        virtual SamplerHandle CreateSampler(const SamplerDesc &d) = 0;

        // Note: vertexShader is only necessary on D3D11, otherwise it may be null
        virtual InputLayoutHandle CreateInputLayout(const VertexAttributeDesc *d, uint32_t attributeCount,
                                                    Shader *vertexShader) = 0;

        // Event queries
        virtual EventQueryHandle CreateEventQuery() = 0;
        virtual void SetEventQuery(EventQuery *query, CommandQueue queue) = 0;
        virtual bool PollEventQuery(EventQuery *query) = 0;
        virtual void WaitEventQuery(EventQuery *query) = 0;
        virtual void ResetEventQuery(EventQuery *query) = 0;

        // Timer queries - see also begin/endTimerQuery in ICommandList
        virtual TimerQueryHandle CreateTimerQuery() = 0;
        virtual bool PollTimerQuery(TimerQuery *query) = 0;
        // returns time in seconds
        virtual float GetTimerQueryTime(TimerQuery *query) = 0;
        virtual void ResetTimerQuery(TimerQuery *query) = 0;

        // Returns the API kind that the RHI backend is running on top of.
        virtual GraphicsAPI GetGraphicsAPI() = 0;

        virtual FramebufferHandle CreateFramebuffer(const FramebufferDesc &desc) = 0;

        virtual GraphicsPipelineHandle CreateGraphicsPipeline(const GraphicsPipelineDesc &desc,
                                                              FramebufferInfo const &fbinfo) = 0;

        [[deprecated("Use CreateGraphicsPipeline with FramebufferInfo instead")]]
        virtual GraphicsPipelineHandle CreateGraphicsPipeline(const GraphicsPipelineDesc &desc, Framebuffer *fb) = 0;

        virtual ComputePipelineHandle CreateComputePipeline(const ComputePipelineDesc &desc) = 0;

        virtual MeshletPipelineHandle CreateMeshletPipeline(const MeshletPipelineDesc &desc,
                                                            FramebufferInfo const &fbinfo) = 0;

        [[deprecated("Use CreateMeshletPipeline with FramebufferInfo instead")]]
        virtual MeshletPipelineHandle CreateMeshletPipeline(const MeshletPipelineDesc &desc, Framebuffer *fb) = 0;

        virtual RT::PipelineHandle CreateRayTracingPipeline(const RT::PipelineDesc &desc) = 0;

        virtual BindingLayoutHandle CreateBindingLayout(const BindingLayoutDesc &desc) = 0;
        virtual BindingLayoutHandle CreateBindlessLayout(const BindlessLayoutDesc &desc) = 0;

        virtual BindingSetHandle CreateBindingSet(const BindingSetDesc &desc, BindingLayout *layout) = 0;
        virtual DescriptorTableHandle CreateDescriptorTable(BindingLayout *layout) = 0;

        virtual void ResizeDescriptorTable(DescriptorTable *descriptorTable, uint32_t newSize,
                                           bool keepContents = true) = 0;
        virtual bool WriteDescriptorTable(DescriptorTable *descriptorTable, const BindingSetItem &item) = 0;

        virtual RT::OpacityMicromapHandle CreateOpacityMicromap(const RT::OpacityMicromapDesc &desc) = 0;
        virtual RT::AccelStructHandle CreateAccelStruct(const RT::AccelStructDesc &desc) = 0;
        virtual MemoryRequirements GetAccelStructMemoryRequirements(RT::AccelStruct *as) = 0;
        virtual RT::Cluster::OperationSizeInfo GetClusterOperationSizeInfo(
            const RT::Cluster::OperationParams &params) = 0;
        virtual bool BindAccelStructMemory(RT::AccelStruct *as, Heap *heap, uint64_t offset) = 0;

        virtual CommandListHandle CreateCommandList(const CommandListParameters &params = CommandListParameters()) = 0;
        virtual uint64_t ExecuteCommandLists(CommandList *const *pCommandLists, size_t numCommandLists,
                                             CommandQueue executionQueue = CommandQueue::Graphics) = 0;
        virtual void QueueWaitForCommandList(CommandQueue waitQueue, CommandQueue executionQueue,
                                             uint64_t instance) = 0;
        // returns true if the wait completes successfully, false if detecting a problem (e.g. device removal)
        virtual bool WaitForIdle() = 0;

        virtual CommandListLifetimeTrackerHandle CreateCommandListLifetimeTracker(CommandQueue executionQueue) = 0;

        // Releases the resources that were referenced in the command lists that have finished executing.
        // IMPORTANT: Call this method at least once per frame.
        virtual void RunGarbageCollection() = 0;

        virtual bool QueryFeatureSupport(Feature feature, void *pInfo = nullptr, size_t infoSize = 0) = 0;

        virtual FormatSupport QueryFormatSupport(Format format) = 0;

        // Returns aggregate CoopVec support information.
        // Deprecated for new code: use queryCoopVecMatMulFormatSupport(...) and
        // queryCoopVecTrainingFormatSupport(...) instead.
        // Some backends may not populate matMulFormats; use queryCoopVecMatMulFormatSupport(...)
        // to query specific matrix multiplication combinations.
        virtual CoopVec::DeviceFeatures QueryCoopVecFeatures() = 0;

        // Queries support for CoopVec matrix multiplication with the given type combination.
        // combination.transposeSupported is ignored; transpose capability is reported in MatMulFormatSupport.
        virtual CoopVec::MatMulFormatSupport QueryCoopVecMatMulFormatSupport(
            const CoopVec::MatMulFormatCombo &combination) = 0;

        // Queries training support for the given accumulation component type (typically Float16 or Float32).
        // See TrainingFormatSupport for details on what each flag means.
        virtual CoopVec::TrainingFormatSupport QueryCoopVecTrainingFormatSupport(CoopVec::DataType componentType) = 0;

        // Calculates and returns the on-device size for a CoopVec matrix of the given dimensions, type and layout.
        virtual size_t GetCoopVecMatrixSize(CoopVec::DataType type, CoopVec::MatrixLayout layout, int rows,
                                            int columns) = 0;

        virtual Object GetNativeQueue(ObjectType objectType, CommandQueue queue) = 0;

        virtual MessageCallback *GetMessageCallback() = 0;

        virtual bool IsAftermathEnabled() = 0;
        // 仅在 crash helper 实际可用时获取引用; 初始 OpenGL Validation 报告 Unsupported 后抛 std::logic_error.
        virtual AftermathCrashDumpHelper &GetAftermathCrashDumpHelper() = 0;

        // Front-end for executeCommandLists(..., 1) for compatibility and convenience
        uint64_t ExecuteCommandList(CommandList *commandList, CommandQueue executionQueue = CommandQueue::Graphics)
        {
            return ExecuteCommandLists(&commandList, 1, executionQueue);
        }
    };

    typedef RefCountPtr<Device> DeviceHandle;

    template <class T> void HashCombine(size_t &seed, const T &v)
    {
        std::hash<T> hasher;
        seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
}

#undef PHYSARA_RHI_ENUM_CLASS_FLAG_OPERATORS

namespace std
{
    template <typename T> struct hash<Physara::RHI::RefCountPtr<T>>
    {
        std::size_t operator()(Physara::RHI::RefCountPtr<T> const &s) const noexcept
        {
            std::hash<T *> hash;
            return hash(s.Get());
        }
    };

    template <> struct hash<Physara::RHI::TextureSubresourceSet>
    {
        std::size_t operator()(Physara::RHI::TextureSubresourceSet const &s) const noexcept
        {
            size_t hash = 0;
            Physara::RHI::HashCombine(hash, s.baseMipLevel);
            Physara::RHI::HashCombine(hash, s.numMipLevels);
            Physara::RHI::HashCombine(hash, s.baseArraySlice);
            Physara::RHI::HashCombine(hash, s.numArraySlices);
            return hash;
        }
    };

    template <> struct hash<Physara::RHI::BufferRange>
    {
        std::size_t operator()(Physara::RHI::BufferRange const &s) const noexcept
        {
            size_t hash = 0;
            Physara::RHI::HashCombine(hash, s.byteOffset);
            Physara::RHI::HashCombine(hash, s.byteSize);
            return hash;
        }
    };

    template <> struct hash<Physara::RHI::BindingSetItem>
    {
        std::size_t operator()(Physara::RHI::BindingSetItem const &s) const noexcept
        {
            size_t value = 0;
            using Physara::RHI::ResourceType;
            Physara::RHI::HashCombine(value, s.resourceHandle);
            Physara::RHI::HashCombine(value, s.slot);
            Physara::RHI::HashCombine(value, s.arrayElement);
            Physara::RHI::HashCombine(value, s.type);
            Physara::RHI::HashCombine(value, s.dimension);
            Physara::RHI::HashCombine(value, s.format);
            Physara::RHI::HashCombine(value, s.overrideComponentMapping);
            switch (s.type)
            {
            case ResourceType::Texture_SRV:
            case ResourceType::Texture_UAV:
            case ResourceType::SamplerFeedbackTexture_UAV:
                Physara::RHI::HashCombine(value, s.subresources.baseMipLevel);
                Physara::RHI::HashCombine(value, s.subresources.numMipLevels);
                Physara::RHI::HashCombine(value, s.subresources.baseArraySlice);
                Physara::RHI::HashCombine(value, s.subresources.numArraySlices);
                break;
            case ResourceType::TypedBuffer_SRV:
            case ResourceType::TypedBuffer_UAV:
            case ResourceType::StructuredBuffer_SRV:
            case ResourceType::StructuredBuffer_UAV:
            case ResourceType::RawBuffer_SRV:
            case ResourceType::RawBuffer_UAV:
            case ResourceType::ConstantBuffer:
            case ResourceType::VolatileConstantBuffer:
            case ResourceType::PushConstants:
                Physara::RHI::HashCombine(value, s.range.byteOffset);
                Physara::RHI::HashCombine(value, s.range.byteSize);
                break;
            case ResourceType::None:
            case ResourceType::Sampler:
            case ResourceType::RayTracingAccelStruct:

                break;
            default:
                assert(false);
                break;
            }
            return value;
        }
    };

    template <> struct hash<Physara::RHI::BindingSetDesc>
    {
        std::size_t operator()(Physara::RHI::BindingSetDesc const &s) const noexcept
        {
            size_t value = 0;
            Physara::RHI::HashCombine(value, s.trackLiveness);
            for (const auto &item : s.bindings)
            {
                Physara::RHI::HashCombine(value, item);
            }
            return value;
        }
    };

    template <> struct hash<Physara::RHI::FramebufferInfo>
    {
        std::size_t operator()(Physara::RHI::FramebufferInfo const &s) const noexcept
        {
            size_t hash = 0;
            for (auto format : s.colorFormats)
            {
                Physara::RHI::HashCombine(hash, format);
            }
            Physara::RHI::HashCombine(hash, s.depthFormat);
            Physara::RHI::HashCombine(hash, s.sampleCount);
            Physara::RHI::HashCombine(hash, s.sampleQuality);
            return hash;
        }
    };

    template <> struct hash<Physara::RHI::BlendState::RenderTarget>
    {
        std::size_t operator()(Physara::RHI::BlendState::RenderTarget const &s) const noexcept
        {
            size_t hash = 0;
            Physara::RHI::HashCombine(hash, s.blendEnable);
            Physara::RHI::HashCombine(hash, s.srcBlend);
            Physara::RHI::HashCombine(hash, s.destBlend);
            Physara::RHI::HashCombine(hash, s.blendOp);
            Physara::RHI::HashCombine(hash, s.srcBlendAlpha);
            Physara::RHI::HashCombine(hash, s.destBlendAlpha);
            Physara::RHI::HashCombine(hash, s.blendOpAlpha);
            Physara::RHI::HashCombine(hash, s.colorWriteMask);
            return hash;
        }
    };

    template <> struct hash<Physara::RHI::BlendState>
    {
        std::size_t operator()(Physara::RHI::BlendState const &s) const noexcept
        {
            size_t hash = 0;
            Physara::RHI::HashCombine(hash, s.alphaToCoverageEnable);
            for (const auto &target : s.targets)
            {
                Physara::RHI::HashCombine(hash, target);
            }
            return hash;
        }
    };

    template <> struct hash<Physara::RHI::VariableRateShadingState>
    {
        std::size_t operator()(Physara::RHI::VariableRateShadingState const &s) const noexcept
        {
            size_t hash = 0;
            Physara::RHI::HashCombine(hash, s.enabled);
            Physara::RHI::HashCombine(hash, s.shadingRate);
            Physara::RHI::HashCombine(hash, s.pipelinePrimitiveCombiner);
            Physara::RHI::HashCombine(hash, s.imageCombiner);
            return hash;
        }
    };
}
