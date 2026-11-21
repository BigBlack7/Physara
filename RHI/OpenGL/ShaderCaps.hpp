#pragma once

/// @file
/// @brief 记录图形及计算的原生限额与计算入口可用性, 供 Pipeline 准备及直接 Dispatch 核对.

#include <array>
#include <cstdint>

#include <glad/gl.h>

namespace Physara::RHI::OpenGL
{
    /// @brief stage 数组依次为 Vertex/Fragment/Compute, combined 计数包含各活动 stage 的贡献.
    /// @note 数量与 binding-index 上限是独立限制. 某 stage 的零限额只拒绝实际使用该资源的计划.
    struct ShaderCaps
    {
        std::array<GLint, 3> uniformBlocks{};
        std::array<GLint, 3> storageBlocks{};
        std::array<GLint, 3> textureUnits{};
        GLint uniformBindings = 0;
        GLint storageBindings = 0;
        GLint combinedUniformBlocks = 0;
        GLint combinedStorageBlocks = 0;
        GLint combinedTextureUnits = 0;
        // 对齐与 blockBytes 均以字节计; 对齐作用于绑定 range 的起点, 不代替公共 CBV 范围合同.
        GLint uniformAlignment = 0;
        GLint storageAlignment = 0;
        GLint64 uniformBlockBytes = 0;
        GLint64 storageBlockBytes = 0;
        GLint imageUnits = 0;
        GLint computeImages = 0;
        GLint combinedImages = 0;
        GLint combinedOutputs = 0;
        // groupSize 限制一个组内各轴的 invocation 数, groupCount 限制一次 Dispatch 的各轴组数.
        std::array<std::uint32_t, 3> groupSize{};
        std::array<std::uint32_t, 3> groupCount{};
        GLint workGroupInvocations = 0;
        bool compute = false;
        GLint vertexAttributes = 0;
        GLint vertexBindings = 0;
        GLint vertexOffset = 0;
        GLint vertexStride = 0;
    };
} // namespace Physara::RHI::OpenGL
