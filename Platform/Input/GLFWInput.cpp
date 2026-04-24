#include "GLFWInput.hpp"

#include <stdexcept>

namespace Physara::Platform
{
    GLFWInput::GLFWInput(GLFWwindow *window) : m_Window(window)
    {
        if (m_Window == nullptr)
        {
            throw std::invalid_argument("GLFWInput requires a valid GLFWwindow*.");
        }

        glfwSetWindowUserPointer(m_Window, this);
        m_PreviousScrollCallback = glfwSetScrollCallback(m_Window, &GLFWInput::ScrollCallback); // 鼠标滚轮回调

        // 获取初始鼠标位置
        double x = 0., y = 0.;
        glfwGetCursorPos(m_Window, &x, &y);
        m_MousePos = glm::vec2(static_cast<float>(x), static_cast<float>(y));
        m_PreviousMousePos = m_MousePos;
    }

    GLFWInput::~GLFWInput()
    {
        if (m_Window != nullptr)
        {
            glfwSetScrollCallback(m_Window, m_PreviousScrollCallback);
            glfwSetWindowUserPointer(m_Window, nullptr);
        }
    }

    bool GLFWInput::IsKeyDown(Key key) const
    {
        const int keyCode = static_cast<int>(key);
        if (!IsValidKeyCode(keyCode))
        {
            return false;
        }
        return m_CurrentKeys[static_cast<std::size_t>(keyCode)];
    }

    bool GLFWInput::IsKeyPressed(Key key) const
    {
        const int keyCode = static_cast<int>(key);
        if (!IsValidKeyCode(keyCode))
        {
            return false;
        }
        const std::size_t idx = static_cast<std::size_t>(keyCode);
        return m_CurrentKeys[idx] && !m_PreviousKeys[idx];
    }

    bool GLFWInput::IsMouseButtonDown(Mouse button) const
    {
        const int mouseCode = static_cast<int>(button);
        if (!IsValidMouseCode(mouseCode))
        {
            return false;
        }
        return m_CurrentMouseButtons[static_cast<std::size_t>(mouseCode)];
    }

    bool GLFWInput::IsMouseButtonPressed(Mouse button) const
    {
        const int mouseCode = static_cast<int>(button);
        if (!IsValidMouseCode(mouseCode))
        {
            return false;
        }
        const std::size_t idx = static_cast<std::size_t>(mouseCode);
        return m_CurrentMouseButtons[idx] && !m_PreviousMouseButtons[idx];
    }

    glm::vec2 GLFWInput::GetMousePos() const
    {
        return m_MousePos;
    }

    glm::vec2 GLFWInput::GetMouseDelta() const
    {
        return m_MouseDelta;
    }

    float GLFWInput::GetScrollDelta() const
    {
        const float delta = m_ScrollDelta;
        m_ScrollDelta = 0.f;
        return delta;
    }

    void GLFWInput::SetCursorMode(CursorMode mode)
    {
        if (m_Window == nullptr)
        {
            return;
        }

        m_CursorMode = mode;

        int glfwCursorMode = GLFW_CURSOR_NORMAL;
        switch (mode)
        {
        case CursorMode::Normal:
            glfwCursorMode = GLFW_CURSOR_NORMAL;
            break;
        case CursorMode::Hidden:
            glfwCursorMode = GLFW_CURSOR_HIDDEN;
            break;
        case CursorMode::Locked:
            glfwCursorMode = GLFW_CURSOR_DISABLED;
            break;
        default:
            glfwCursorMode = GLFW_CURSOR_NORMAL;
            break;
        }

        glfwSetInputMode(m_Window, GLFW_CURSOR, glfwCursorMode);

        // 切换模式时重置delta防止相机跳变
        double x = 0., y = 0.;
        glfwGetCursorPos(m_Window, &x, &y);
        m_MousePos = glm::vec2(static_cast<float>(x), static_cast<float>(y));
        m_PreviousMousePos = m_MousePos;
        m_MouseDelta = glm::vec2(0.f, 0.f);
    }

    void GLFWInput::BeginFrame()
    {
        if (m_Window == nullptr)
        {
            return;
        }

        m_PreviousKeys = m_CurrentKeys;
        m_PreviousMouseButtons = m_CurrentMouseButtons;

        for (int key = 0; key <= GLFW_KEY_LAST; ++key)
        {
            const int state = glfwGetKey(m_Window, key);
            m_CurrentKeys[static_cast<std::size_t>(key)] = (state == GLFW_PRESS || state == GLFW_REPEAT);
        }

        for (int button = 0; button < static_cast<int>(kMouseCount); ++button)
        {
            const int state = glfwGetMouseButton(m_Window, button);
            m_CurrentMouseButtons[static_cast<std::size_t>(button)] = (state == GLFW_PRESS);
        }

        double x = 0., y = 0.;
        glfwGetCursorPos(m_Window, &x, &y);
        m_MousePos = glm::vec2(static_cast<float>(x), static_cast<float>(y));

        if (m_FirstFrame)
        {
            m_MouseDelta = glm::vec2(0.f, 0.f);
            m_PreviousMousePos = m_MousePos;
            m_FirstFrame = false;
            return;
        }

        if (m_CursorMode == CursorMode::Locked)
        {
            m_MouseDelta = m_MousePos - m_PreviousMousePos;
        }
        else
        {
            // 普通模式下清零, 避免切回锁定时跳变
            m_MouseDelta = glm::vec2(0.0f, 0.0f);
        }

        m_PreviousMousePos = m_MousePos;
    }

    void GLFWInput::ScrollCallback(GLFWwindow *window, double xOffset, double yOffset)
    {
        if (window == nullptr)
        {
            return;
        }

        void *ptr = glfwGetWindowUserPointer(window);
        if (ptr == nullptr)
        {
            return;
        }

        auto *self = static_cast<GLFWInput *>(ptr);
        self->m_ScrollDelta += static_cast<float>(yOffset);

        // 如果外部之前设置过callback, 这里继续链式调用, 避免吞掉别人的逻辑
        if (self->m_PreviousScrollCallback != nullptr)
        {
            self->m_PreviousScrollCallback(window, 0., yOffset);
        }
    }

    bool GLFWInput::IsValidKeyCode(int keyCode) const
    {
        return keyCode >= 0 && keyCode <= GLFW_KEY_LAST;
    }

    bool GLFWInput::IsValidMouseCode(int mouseCode) const
    {
        return mouseCode >= 0 && mouseCode < static_cast<int>(kMouseCount);
    }
}