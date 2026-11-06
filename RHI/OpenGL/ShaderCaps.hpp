#pragma once

/// @file
/// @brief 图形准备使用的真实 GL 限额值, 与格式能力分开, 不代表命令执行能力.

#include <array>

#include <glad/gl.h>

namespace Physara::RHI::OpenGL
{
    /// @brief stage 数组依次为 Vertex/Fragment, combined 计数包含各活动 stage 的贡献.
    /// @note 数量与 binding-index 上限是独立限制. 某 stage 的零限额只拒绝实际使用该资源的计划.
    struct ShaderCaps
    {
        std::array<GLint, 2> uniformBlocks{};
        std::array<GLint, 2> storageBlocks{};
        std::array<GLint, 2> textureUnits{};
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
        GLint vertexAttributes = 0;
        GLint vertexBindings = 0;
        GLint vertexOffset = 0;
        GLint vertexStride = 0;
    };
} // namespace Physara::RHI::OpenGL
