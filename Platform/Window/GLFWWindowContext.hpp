#pragma once

namespace Physara::Platform
{
    class GLFWInput;
    class GLFWWindowOpenGL;

    // 解决C风格回调无法直接访问Cpp类成员的问题
    struct GLFWWindowContext
    {
        GLFWWindowOpenGL *window{nullptr};
        GLFWInput *input{nullptr};
    };
}