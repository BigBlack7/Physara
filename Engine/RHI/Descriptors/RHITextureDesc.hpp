#pragma once

#include <Engine/RHI/RHIDefinitions.hpp>

namespace Physara::RHI
{
    struct RHITextureDesc
    {
        std::uint32_t width = 1;
        std::uint32_t height = 1;
        std::uint32_t depth = 1; // 3D纹理使用, 2D/Cube填1
        std::uint32_t mipLevels = 1;
        std::uint32_t arrayLayers = 1; // Cubemap填6
        std::uint32_t samples = 1;     // MSAA样本数
        TextureFormat format = TextureFormat::RGBA8;
        TextureDimension dimension = TextureDimension::Tex2D;
        TextureUsageFlags usage = TextureUsage::Sampled; // flags, 可按位组合
        const void *initialData = nullptr;               // 可选初始像素数据
    };
}