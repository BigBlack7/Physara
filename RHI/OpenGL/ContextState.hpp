#pragma once

// 上传路径当前拥有的原生 unpack 状态缓存; 资源用途跟踪与 GPU 完成不属于本缓存.

#include <glad/gl.h>

namespace Physara::RHI::OpenGL
{
    // 唯一的 unpack 状态权威. 原生互操作后调用列表 ClearState, 不快照/恢复外部状态.
    class ContextState
    {
    public:
        void Invalidate() noexcept;
        // 只维护本后端拥有的 unpack PBO/pixel-store. 互操作后须先 Invalidate, 不查询/恢复外部 GL 状态.
        void ApplyUnpack(GLuint buffer) noexcept;

    private:
        bool m_Known = false;
        GLuint m_UnpackBuffer = 0;
    };
}
