#include "ContextState.hpp"

namespace Physara::RHI::OpenGL
{
    void ContextState::Invalidate() noexcept
    {
        m_Known = false;
    }

    void ContextState::ApplyUnpack(GLuint buffer) noexcept
    {
        if (!m_Known || m_UnpackBuffer != buffer)
        {
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer);
        }
        if (!m_Known)
        {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
            glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
            glPixelStorei(GL_UNPACK_SKIP_IMAGES, 0);
            glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
            glPixelStorei(GL_UNPACK_LSB_FIRST, GL_FALSE);
        }
        m_Known = true;
        m_UnpackBuffer = buffer;
    }
}
