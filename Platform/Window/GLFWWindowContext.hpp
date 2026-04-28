#pragma once

namespace Physara::Platform
{
    class GLFWInput;
    class GLFWWindowOpenGL;

    struct GLFWWindowContext
    {
        GLFWWindowOpenGL *Window{nullptr};
        GLFWInput *Input{nullptr};
    };
}