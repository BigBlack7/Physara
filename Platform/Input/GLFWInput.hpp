#pragma once

#include <array>
#include <cstddef>

#include <glm/vec2.hpp>

#include <Platform/Input/IInput.hpp>

struct GLFWwindow;

namespace Physara::Platform
{
    struct GLFWWindowContext;

    class GLFWInput final : public IInput
    {
    public:
        explicit GLFWInput(void *windowHandle);
        ~GLFWInput() override;

        bool IsKeyDown(Key key) const override;
        bool IsKeyPressed(Key key) const override;

        bool IsMouseButtonDown(Mouse button) const override;
        bool IsMouseButtonPressed(Mouse button) const override;

        glm::vec2 GetMousePos() const override;
        glm::vec2 GetMouseDelta() const override;
        float PeekScrollDelta() const override;
        float ConsumeScrollDelta() override;

        void SetCursorMode(CursorMode mode) override;
        void BeginFrame() override;

    private:
        static constexpr std::size_t kKeyCount = 349; // GLFW_KEY_LAST + 1
        static constexpr std::size_t kMouseCount = 3; // Left/Right/Middle
        using ScrollCallbackFn = void (*)(GLFWwindow *, double, double);

        static void ScrollCallback(GLFWwindow *window, double xOffset, double yOffset);

        bool IsValidKeyCode(int keyCode) const;
        bool IsValidMouseCode(int mouseCode) const;

    private:
        GLFWwindow *m_Window{nullptr};
        GLFWWindowContext *m_Context{nullptr};
        CursorMode m_CursorMode{CursorMode::Normal};

        std::array<bool, kKeyCount> m_CurrentKeys{};
        std::array<bool, kKeyCount> m_PreviousKeys{};

        std::array<bool, kMouseCount> m_CurrentMouseButtons{};
        std::array<bool, kMouseCount> m_PreviousMouseButtons{};

        glm::vec2 m_MousePos{0.f, 0.f};
        glm::vec2 m_PreviousMousePos{0.f, 0.f};
        glm::vec2 m_MouseDelta{0.f, 0.f};

        float m_ScrollDelta{0.f};

        bool m_FirstFrame{true};
        ScrollCallbackFn m_PreviousScrollCallback{nullptr};
    };
}