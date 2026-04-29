#pragma once

namespace Physara::Platform
{
    class GLFWInput;
    class GLFWWindowOpenGL;

    struct GLFWWindowContext
    {
        GLFWWindowOpenGL *window{nullptr};
        GLFWInput *input{nullptr};
    };
}