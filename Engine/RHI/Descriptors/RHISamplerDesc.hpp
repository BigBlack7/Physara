#pragma once

#include <Engine/RHI/RHIDefinitions.hpp>

namespace Physara::RHI
{
    struct RHISamplerDesc
    {
        FilterMode minFilter = FilterMode::Linear;
        FilterMode magFilter = FilterMode::Linear;
        FilterMode mipFilter = FilterMode::Linear;

        WrapMode wrapU = WrapMode::Repeat;
        WrapMode wrapV = WrapMode::Repeat;
        WrapMode wrapW = WrapMode::Repeat;

        float anisotropy = 1.f;                // 1.0为关闭, 常见上限16.0
        CompareOp compareOp = CompareOp::None; // 阴影比较采样时可设为LessEqual
        float lodBias = 0.f;
        float minLod = 0.f;
        float maxLod = 32.f;
    };
}