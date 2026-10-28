#pragma once

#include <glad/gl.h>

namespace Physara::RHI::OpenGL
{
    // 唯一的 unpack 状态权威. 原生互操作后调用列表 ClearState, 不快照/恢复外部状态.
    class ContextState
    {
    public:
        void Invalidate() noexcept;
        void ApplyUnpack(GLuint buffer) noexcept;

    private:
        bool m_Known = false;
        GLuint m_UnpackBuffer = 0;
    };
}
