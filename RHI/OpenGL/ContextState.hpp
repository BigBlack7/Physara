#pragma once

/// @file
/// @brief 上传路径拥有的 unpack 原生状态缓存, 与资源用途 tracker 和 GPU 完成分开.

#include <glad/gl.h>

namespace Physara::RHI::OpenGL
{
    /// @brief unpack PBO/pixel-store 的唯一缓存, 外部互操作后失效, 不快照/恢复外部状态.
    class ContextState
    {
    public:
        /// @brief 仅标记缓存未知, 不发出 GL 调用或修改资源状态.
        void Invalidate() noexcept;
        /// @brief 建立紧凑 texel 的标准 unpack 状态并按需绑定 PBO, 调用者保证 current context.
        void ApplyUnpack(GLuint buffer) noexcept;

    private:
        bool m_Known = false;
        GLuint m_UnpackBuffer = 0;
    };
}
