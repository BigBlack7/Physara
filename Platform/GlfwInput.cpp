#include "DeviceHostInternal.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <optional>
#include <span>
#include <variant>

namespace Physara::Platform::Detail
{
    static constexpr auto NativeKeys = []
    {
        std::array<Key, GLFW_KEY_LAST + 1> result{};
        result[GLFW_KEY_SPACE] = Key::Space;
        result[GLFW_KEY_APOSTROPHE] = Key::Apostrophe;
        result[GLFW_KEY_COMMA] = Key::Comma;
        result[GLFW_KEY_MINUS] = Key::Minus;
        result[GLFW_KEY_PERIOD] = Key::Period;
        result[GLFW_KEY_SLASH] = Key::Slash;
        result[GLFW_KEY_0] = Key::D0;
        result[GLFW_KEY_1] = Key::D1;
        result[GLFW_KEY_2] = Key::D2;
        result[GLFW_KEY_3] = Key::D3;
        result[GLFW_KEY_4] = Key::D4;
        result[GLFW_KEY_5] = Key::D5;
        result[GLFW_KEY_6] = Key::D6;
        result[GLFW_KEY_7] = Key::D7;
        result[GLFW_KEY_8] = Key::D8;
        result[GLFW_KEY_9] = Key::D9;
        result[GLFW_KEY_SEMICOLON] = Key::Semicolon;
        result[GLFW_KEY_EQUAL] = Key::Equal;
        result[GLFW_KEY_A] = Key::A;
        result[GLFW_KEY_B] = Key::B;
        result[GLFW_KEY_C] = Key::C;
        result[GLFW_KEY_D] = Key::D;
        result[GLFW_KEY_E] = Key::E;
        result[GLFW_KEY_F] = Key::F;
        result[GLFW_KEY_G] = Key::G;
        result[GLFW_KEY_H] = Key::H;
        result[GLFW_KEY_I] = Key::I;
        result[GLFW_KEY_J] = Key::J;
        result[GLFW_KEY_K] = Key::K;
        result[GLFW_KEY_L] = Key::L;
        result[GLFW_KEY_M] = Key::M;
        result[GLFW_KEY_N] = Key::N;
        result[GLFW_KEY_O] = Key::O;
        result[GLFW_KEY_P] = Key::P;
        result[GLFW_KEY_Q] = Key::Q;
        result[GLFW_KEY_R] = Key::R;
        result[GLFW_KEY_S] = Key::S;
        result[GLFW_KEY_T] = Key::T;
        result[GLFW_KEY_U] = Key::U;
        result[GLFW_KEY_V] = Key::V;
        result[GLFW_KEY_W] = Key::W;
        result[GLFW_KEY_X] = Key::X;
        result[GLFW_KEY_Y] = Key::Y;
        result[GLFW_KEY_Z] = Key::Z;
        result[GLFW_KEY_LEFT_BRACKET] = Key::LeftBracket;
        result[GLFW_KEY_BACKSLASH] = Key::Backslash;
        result[GLFW_KEY_RIGHT_BRACKET] = Key::RightBracket;
        result[GLFW_KEY_GRAVE_ACCENT] = Key::GraveAccent;
        result[GLFW_KEY_WORLD_1] = Key::World1;
        result[GLFW_KEY_WORLD_2] = Key::World2;
        result[GLFW_KEY_ESCAPE] = Key::Escape;
        result[GLFW_KEY_ENTER] = Key::Enter;
        result[GLFW_KEY_TAB] = Key::Tab;
        result[GLFW_KEY_BACKSPACE] = Key::Backspace;
        result[GLFW_KEY_INSERT] = Key::Insert;
        result[GLFW_KEY_DELETE] = Key::Delete;
        result[GLFW_KEY_RIGHT] = Key::Right;
        result[GLFW_KEY_LEFT] = Key::Left;
        result[GLFW_KEY_DOWN] = Key::Down;
        result[GLFW_KEY_UP] = Key::Up;
        result[GLFW_KEY_PAGE_UP] = Key::PageUp;
        result[GLFW_KEY_PAGE_DOWN] = Key::PageDown;
        result[GLFW_KEY_HOME] = Key::Home;
        result[GLFW_KEY_END] = Key::End;
        result[GLFW_KEY_CAPS_LOCK] = Key::CapsLock;
        result[GLFW_KEY_SCROLL_LOCK] = Key::ScrollLock;
        result[GLFW_KEY_NUM_LOCK] = Key::NumLock;
        result[GLFW_KEY_PRINT_SCREEN] = Key::PrintScreen;
        result[GLFW_KEY_PAUSE] = Key::Pause;
        result[GLFW_KEY_F1] = Key::F1;
        result[GLFW_KEY_F2] = Key::F2;
        result[GLFW_KEY_F3] = Key::F3;
        result[GLFW_KEY_F4] = Key::F4;
        result[GLFW_KEY_F5] = Key::F5;
        result[GLFW_KEY_F6] = Key::F6;
        result[GLFW_KEY_F7] = Key::F7;
        result[GLFW_KEY_F8] = Key::F8;
        result[GLFW_KEY_F9] = Key::F9;
        result[GLFW_KEY_F10] = Key::F10;
        result[GLFW_KEY_F11] = Key::F11;
        result[GLFW_KEY_F12] = Key::F12;
        result[GLFW_KEY_F13] = Key::F13;
        result[GLFW_KEY_F14] = Key::F14;
        result[GLFW_KEY_F15] = Key::F15;
        result[GLFW_KEY_F16] = Key::F16;
        result[GLFW_KEY_F17] = Key::F17;
        result[GLFW_KEY_F18] = Key::F18;
        result[GLFW_KEY_F19] = Key::F19;
        result[GLFW_KEY_F20] = Key::F20;
        result[GLFW_KEY_F21] = Key::F21;
        result[GLFW_KEY_F22] = Key::F22;
        result[GLFW_KEY_F23] = Key::F23;
        result[GLFW_KEY_F24] = Key::F24;
        result[GLFW_KEY_F25] = Key::F25;
        result[GLFW_KEY_KP_0] = Key::Keypad0;
        result[GLFW_KEY_KP_1] = Key::Keypad1;
        result[GLFW_KEY_KP_2] = Key::Keypad2;
        result[GLFW_KEY_KP_3] = Key::Keypad3;
        result[GLFW_KEY_KP_4] = Key::Keypad4;
        result[GLFW_KEY_KP_5] = Key::Keypad5;
        result[GLFW_KEY_KP_6] = Key::Keypad6;
        result[GLFW_KEY_KP_7] = Key::Keypad7;
        result[GLFW_KEY_KP_8] = Key::Keypad8;
        result[GLFW_KEY_KP_9] = Key::Keypad9;
        result[GLFW_KEY_KP_DECIMAL] = Key::KeypadDecimal;
        result[GLFW_KEY_KP_DIVIDE] = Key::KeypadDivide;
        result[GLFW_KEY_KP_MULTIPLY] = Key::KeypadMultiply;
        result[GLFW_KEY_KP_SUBTRACT] = Key::KeypadSubtract;
        result[GLFW_KEY_KP_ADD] = Key::KeypadAdd;
        result[GLFW_KEY_KP_ENTER] = Key::KeypadEnter;
        result[GLFW_KEY_KP_EQUAL] = Key::KeypadEqual;
        result[GLFW_KEY_LEFT_SHIFT] = Key::LeftShift;
        result[GLFW_KEY_LEFT_CONTROL] = Key::LeftControl;
        result[GLFW_KEY_LEFT_ALT] = Key::LeftAlt;
        result[GLFW_KEY_LEFT_SUPER] = Key::LeftSuper;
        result[GLFW_KEY_RIGHT_SHIFT] = Key::RightShift;
        result[GLFW_KEY_RIGHT_CONTROL] = Key::RightControl;
        result[GLFW_KEY_RIGHT_ALT] = Key::RightAlt;
        result[GLFW_KEY_RIGHT_SUPER] = Key::RightSuper;
        result[GLFW_KEY_MENU] = Key::Menu;
        return result;
    }();

    static DeviceHostState *Target(GLFWwindow *window) noexcept
    {
        auto *state = static_cast<DeviceHostState *>(glfwGetWindowUserPointer(window));
        if (state != nullptr && state->ownerThread != std::this_thread::get_id())
        {
            TerminateForContract("Input callback requires the owner thread");
        }
        if (state != nullptr && (!state->inputAccepting || state->stopped))
        {
            return nullptr;
        }
        return state;
    }
    static std::optional<KeyAction> Action(int action) noexcept
    {
        if (action == GLFW_PRESS)
        {
            return KeyAction::Press;
        }
        if (action == GLFW_RELEASE)
        {
            return KeyAction::Release;
        }
        if (action == GLFW_REPEAT)
        {
            return KeyAction::Repeat;
        }
        return std::nullopt;
    }
    static std::optional<Modifier> TranslateModifiers(int mods) noexcept
    {
        constexpr int KnownMods =
            GLFW_MOD_SHIFT | GLFW_MOD_CONTROL | GLFW_MOD_ALT | GLFW_MOD_SUPER | GLFW_MOD_CAPS_LOCK | GLFW_MOD_NUM_LOCK;
        if ((mods & ~KnownMods) != 0)
        {
            return std::nullopt;
        }
        const auto flags =
            static_cast<std::uint8_t>(((mods & GLFW_MOD_SHIFT) ? 1 : 0) | ((mods & GLFW_MOD_CONTROL) ? 2 : 0) |
                                      ((mods & GLFW_MOD_ALT) ? 4 : 0) | ((mods & GLFW_MOD_SUPER) ? 8 : 0) |
                                      ((mods & GLFW_MOD_CAPS_LOCK) ? 16 : 0) | ((mods & GLFW_MOD_NUM_LOCK) ? 32 : 0));
        return static_cast<Modifier>(flags);
    }
    static void OnKey(GLFWwindow *window, int key, int scanCode, int action, int mods) noexcept
    {
        if (auto *state = Target(window))
        {
            const auto translated = Action(action);
            const auto modifiers = TranslateModifiers(mods);
            if (!translated || !modifiers)
            {
                state->input.Fail(InputFaultReason::InvalidData);
                return;
            }
            const auto logical =
                key >= 0 && key <= GLFW_KEY_LAST ? NativeKeys[static_cast<std::size_t>(key)] : Key::Unknown;
            state->input.Record(KeyEvent{logical, scanCode, *translated, *modifiers});
        }
    }
    static void OnChar(GLFWwindow *window, unsigned int codepoint) noexcept
    {
        if (auto *state = Target(window))
        {
            state->input.Record(CharEvent{static_cast<char32_t>(codepoint)});
        }
    }
    static void OnMouseButton(GLFWwindow *window, int button, int action, int mods) noexcept
    {
        if (auto *state = Target(window))
        {
            const auto translated = Action(action);
            const auto modifiers = TranslateModifiers(mods);
            if (button < 0 || button >= static_cast<int>(MouseButtonCount) || !translated || !modifiers)
            {
                state->input.Fail(InputFaultReason::InvalidData);
                return;
            }
            state->input.Record(MouseButtonEvent{static_cast<MouseButton>(button), *translated, *modifiers});
        }
    }
    static void OnCursorPosition(GLFWwindow *window, double x, double y) noexcept
    {
        if (auto *state = Target(window))
        {
            state->input.Record(CursorPositionEvent{{x, y}});
        }
    }
    static void OnCursorEnter(GLFWwindow *window, int entered) noexcept
    {
        if (auto *state = Target(window))
        {
            state->input.Record(CursorEnterEvent{entered == GLFW_TRUE});
        }
    }
    static void OnScroll(GLFWwindow *window, double x, double y) noexcept
    {
        if (auto *state = Target(window))
        {
            state->input.Record(ScrollEvent{{x, y}});
        }
    }
    static void OnFocus(GLFWwindow *window, int focused) noexcept
    {
        if (auto *state = Target(window))
        {
            state->input.Record(FocusEvent{focused == GLFW_TRUE});
        }
    }
    static void OnIconify(GLFWwindow *window, int iconified) noexcept
    {
        if (auto *state = Target(window))
        {
            state->input.Record(IconifyEvent{iconified == GLFW_TRUE});
        }
    }
    static void OnClose(GLFWwindow *window) noexcept
    {
        if (auto *state = Target(window))
        {
            state->input.Record(CloseRequestedEvent{true});
        }
    }
    static void OnClientSize(GLFWwindow *window, int width, int height) noexcept
    {
        if (auto *state = Target(window))
        {
            if (width < 0 || height < 0)
            {
                state->input.Fail(InputFaultReason::InvalidData);
                return;
            }
            state->input.Record(
                ClientSizeEvent{{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)}});
        }
    }
    static void OnFramebufferSize(GLFWwindow *window, int width, int height) noexcept
    {
        if (auto *state = Target(window))
        {
            if (width < 0 || height < 0)
            {
                state->input.Fail(InputFaultReason::InvalidData);
                return;
            }
            state->input.Record(
                FramebufferSizeEvent{{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)}});
        }
    }
    static void OnContentScale(GLFWwindow *window, float x, float y) noexcept
    {
        if (auto *state = Target(window))
        {
            state->input.Record(ContentScaleEvent{{x, y}});
        }
    }

    struct WindowFacts
    {
        WindowState window;
        LogicalPoint cursor;
        CursorMode mode = CursorMode::Normal;
    };
    static std::variant<WindowFacts, HostError> ReadWindowFacts(DeviceHostState &state)
    {
        BeginNativeOperation(state);
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
        if (state.checks.nativeObservationError == state.stage)
        {
            state.checks.nativeObservationError.reset();
            static_cast<void>(glfwGetWindowAttrib(state.window, -1));
        }
#endif
        int width = 0, height = 0, pixelWidth = 0, pixelHeight = 0;
        WindowFacts facts;
        glfwGetWindowSize(state.window, &width, &height);
        glfwGetFramebufferSize(state.window, &pixelWidth, &pixelHeight);
        glfwGetWindowContentScale(state.window, &facts.window.contentScale.x, &facts.window.contentScale.y);
        facts.window.focused = glfwGetWindowAttrib(state.window, GLFW_FOCUSED) == GLFW_TRUE;
        facts.window.iconified = glfwGetWindowAttrib(state.window, GLFW_ICONIFIED) == GLFW_TRUE;
        facts.window.closeRequested = glfwWindowShouldClose(state.window) == GLFW_TRUE;
        glfwGetCursorPos(state.window, &facts.cursor.x, &facts.cursor.y);
        const int mode = glfwGetInputMode(state.window, GLFW_CURSOR);
        if (auto error = TakeNativeError(state, HostErrorReason::EventProcessing, state.stage))
        {
            return *error;
        }
        if (width < 0 || height < 0 || pixelWidth < 0 || pixelHeight < 0 ||
            (mode != GLFW_CURSOR_NORMAL && mode != GLFW_CURSOR_DISABLED))
        {
            return MakeError(&state, HostErrorReason::InputInvalidData, state.stage,
                             "Invalid native window dimensions or cursor mode");
        }
        facts.window.clientSizeLogical = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};
        facts.window.framebufferSizePixels = {static_cast<std::uint32_t>(pixelWidth),
                                              static_cast<std::uint32_t>(pixelHeight)};
        facts.mode = mode == GLFW_CURSOR_DISABLED ? CursorMode::Relative : CursorMode::Normal;
        return facts;
    }
    static std::optional<HostError> RefreshWindowInput(DeviceHostState &state)
    {
        auto result = ReadWindowFacts(state);
        if (auto *error = std::get_if<HostError>(&result))
        {
            return *error;
        }
        const auto &facts = std::get<WindowFacts>(result);
        auto &input = state.input;
        if (facts.window.clientSizeLogical != input.Window().clientSizeLogical)
        {
            input.Record(ClientSizeEvent{facts.window.clientSizeLogical});
        }
        if (facts.window.framebufferSizePixels != input.Window().framebufferSizePixels)
        {
            input.Record(FramebufferSizeEvent{facts.window.framebufferSizePixels});
        }
        if (facts.window.contentScale != input.Window().contentScale)
        {
            input.Record(ContentScaleEvent{facts.window.contentScale});
        }
        if (facts.window.focused != input.Window().focused)
        {
            input.Record(FocusEvent{facts.window.focused});
        }
        if (facts.window.iconified != input.Window().iconified)
        {
            input.Record(IconifyEvent{facts.window.iconified});
        }
        if (facts.window.closeRequested != input.Window().closeRequested)
        {
            input.Record(CloseRequestedEvent{facts.window.closeRequested});
        }
        input.ObserveCursorMode(facts.mode);
        if (facts.cursor != input.Snapshot().cursorPositionLogical)
        {
            input.Record(CursorPositionEvent{facts.cursor});
        }
        return TakeInputFault(state);
    }
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
    static bool StopSetup(DeviceHostState &state, int step)
    {
        return state.checks.failInputSetupStep == step;
    }
    static void InjectNativeError(DeviceHostState &state, HostStage stage)
    {
        if (state.checks.nativeInputError == stage)
        {
            state.checks.nativeInputError.reset();
            static_cast<void>(glfwGetInputMode(state.window, -1));
        }
    }
#endif
    static std::optional<HostError> TakeRequestFailure(DeviceHostState &state, std::size_t index)
    {
        // 原生首错优先; 回调终止故障必须先于可恢复的捕获拒绝报告.
        auto error = TakeNativeError(state, HostErrorReason::RequestApplication, state.stage);
        if (!error)
        {
            error = TakeInputFault(state);
        }
        if (error)
        {
            error->requestIndex = index;
        }
        return error;
    }
    static std::optional<HostError> FinishEvents(DeviceHostState &state)
    {
        auto error = TakeNativeError(state, HostErrorReason::EventProcessing, state.stage);
        if (!error)
        {
            error = TakeInputFault(state);
        }
        if (!error)
        {
            error = RefreshWindowInput(state);
        }
        if (!error && state.input.NeedsCursorRelease())
        {
            error = ReleaseRelativeCursor(state);
        }
        if (!error)
        {
            error = TakeInputFault(state);
        }
        return error;
    }
}

namespace Physara::Platform::Detail
{
    std::optional<HostError> TakeInputFault(DeviceHostState &state)
    {
        if (!state.input.Fault())
        {
            return std::nullopt;
        }
        const auto &fault = *state.input.Fault();
        const auto reason = fault.reason == InputFaultReason::Overflow           ? HostErrorReason::InputOverflow
                            : fault.reason == InputFaultReason::CounterExhausted ? HostErrorReason::CounterExhausted
                                                                                 : HostErrorReason::InputInvalidData;
        const auto message = fault.reason == InputFaultReason::Overflow ? "Input event capacity exceeded"
                             : fault.reason == InputFaultReason::CounterExhausted
                                 ? "Input sequence or metrics revision exhausted"
                                 : "Invalid input value";
        auto error = MakeError(&state, reason, state.stage, message, fault.source);
        std::array<char, 32> number{};
        const auto result = std::to_chars(number.data(), number.data() + number.size(), fault.rejected);
        error.context.description.Append("; rejected=");
        error.context.description.Append({number.data(), static_cast<std::size_t>(result.ptr - number.data())});
        return error;
    }

    std::optional<HostError> InitializeWindowInput(DeviceHostState &state)
    {
        state.stage = HostStage::InputSetup;
        auto observed = ReadWindowFacts(state);
        if (auto *error = std::get_if<HostError>(&observed))
        {
            return *error;
        }
        const auto &facts = std::get<WindowFacts>(observed);
        state.input.Initialize(facts.window, facts.cursor, facts.mode);
        if (auto error = TakeInputFault(state))
        {
            return error;
        }
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
        if (StopSetup(state, 0))
        {
            return MakeError(&state, HostErrorReason::InternalFailure, state.stage, "Injected input seed failure");
        }
#endif
        BeginNativeOperation(state);
        state.inputCallbacksInstalled = true;
        state.inputAccepting = true;
        glfwSetKeyCallback(state.window, OnKey);
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
        if (StopSetup(state, 1))
        {
            return MakeError(&state, HostErrorReason::InternalFailure, state.stage,
                             "Injected partial callback installation failure");
        }
#endif
        glfwSetCharCallback(state.window, OnChar);
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
        if (StopSetup(state, 2))
        {
            return MakeError(&state, HostErrorReason::InternalFailure, state.stage,
                             "Injected partial callback installation failure");
        }
#endif
        glfwSetMouseButtonCallback(state.window, OnMouseButton);
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
        if (StopSetup(state, 3))
        {
            return MakeError(&state, HostErrorReason::InternalFailure, state.stage,
                             "Injected partial callback installation failure");
        }
#endif
        glfwSetCursorPosCallback(state.window, OnCursorPosition);
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
        if (StopSetup(state, 4))
        {
            return MakeError(&state, HostErrorReason::InternalFailure, state.stage,
                             "Injected partial callback installation failure");
        }
#endif
        glfwSetCursorEnterCallback(state.window, OnCursorEnter);
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
        if (StopSetup(state, 5))
        {
            return MakeError(&state, HostErrorReason::InternalFailure, state.stage,
                             "Injected partial callback installation failure");
        }
#endif
        glfwSetScrollCallback(state.window, OnScroll);
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
        if (StopSetup(state, 6))
        {
            return MakeError(&state, HostErrorReason::InternalFailure, state.stage,
                             "Injected partial callback installation failure");
        }
#endif
        glfwSetWindowFocusCallback(state.window, OnFocus);
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
        if (StopSetup(state, 7))
        {
            return MakeError(&state, HostErrorReason::InternalFailure, state.stage,
                             "Injected partial callback installation failure");
        }
#endif
        glfwSetWindowIconifyCallback(state.window, OnIconify);
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
        if (StopSetup(state, 8))
        {
            return MakeError(&state, HostErrorReason::InternalFailure, state.stage,
                             "Injected partial callback installation failure");
        }
#endif
        glfwSetWindowCloseCallback(state.window, OnClose);
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
        if (StopSetup(state, 9))
        {
            return MakeError(&state, HostErrorReason::InternalFailure, state.stage,
                             "Injected partial callback installation failure");
        }
#endif
        glfwSetWindowSizeCallback(state.window, OnClientSize);
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
        if (StopSetup(state, 10))
        {
            return MakeError(&state, HostErrorReason::InternalFailure, state.stage,
                             "Injected partial callback installation failure");
        }
#endif
        glfwSetFramebufferSizeCallback(state.window, OnFramebufferSize);
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
        if (StopSetup(state, 11))
        {
            return MakeError(&state, HostErrorReason::InternalFailure, state.stage,
                             "Injected partial callback installation failure");
        }
#endif
        glfwSetWindowContentScaleCallback(state.window, OnContentScale);
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
        if (StopSetup(state, 12))
        {
            return MakeError(&state, HostErrorReason::InternalFailure, state.stage,
                             "Injected partial callback installation failure");
        }
#endif

        glfwSetInputMode(state.window, GLFW_LOCK_KEY_MODS, GLFW_TRUE);
        if (auto error = TakeNativeError(state, HostErrorReason::EventProcessing, state.stage))
        {
            return error;
        }
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
        if (StopSetup(state, 13) || state.checks.stopAfter == HostStage::InputSetup)
        {
            return MakeError(&state, HostErrorReason::InternalFailure, state.stage, "Injected input setup failure");
        }
#endif
        return RefreshWindowInput(state);
    }

    std::optional<HostError> ReleaseRelativeCursor(DeviceHostState &state)
    {
        BeginNativeOperation(state);
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
        if (state.checks.failCursorRelease)
        {
            state.checks.failCursorRelease = false;
            static_cast<void>(glfwGetInputMode(state.window, -1));
        }
#endif
        glfwSetInputMode(state.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        const int observed = glfwGetInputMode(state.window, GLFW_CURSOR);
        auto error = TakeNativeError(state, HostErrorReason::Cleanup, state.stage);
        if (observed == GLFW_CURSOR_NORMAL)
        {
            state.input.ObserveCursorMode(CursorMode::Normal);
        }
        else if (!error)
        {
            error = MakeError(&state, HostErrorReason::Cleanup, state.stage, "Cursor mode did not return to normal");
        }
        return error;
    }

    std::optional<HostError> ReleaseWindowInput(DeviceHostState &state)
    {
        // 先停止写入, 再解除可能同步产生回调的光标模式, 最后解绑原生回调.
        state.inputAccepting = false;
        if (!state.inputCallbacksInstalled)
        {
            return std::nullopt;
        }
        std::optional<HostError> error;
        if (state.input.Snapshot().cursorMode == CursorMode::Relative || state.input.NeedsCursorRelease())
        {
            error = ReleaseRelativeCursor(state);
        }
        BeginNativeOperation(state);
        glfwSetKeyCallback(state.window, nullptr);
        glfwSetCharCallback(state.window, nullptr);
        glfwSetMouseButtonCallback(state.window, nullptr);
        glfwSetCursorPosCallback(state.window, nullptr);
        glfwSetCursorEnterCallback(state.window, nullptr);
        glfwSetScrollCallback(state.window, nullptr);
        glfwSetWindowFocusCallback(state.window, nullptr);
        glfwSetWindowIconifyCallback(state.window, nullptr);
        glfwSetWindowCloseCallback(state.window, nullptr);
        glfwSetWindowSizeCallback(state.window, nullptr);
        glfwSetFramebufferSizeCallback(state.window, nullptr);
        glfwSetWindowContentScaleCallback(state.window, nullptr);
        state.inputCallbacksInstalled = false;
        MergeCleanupError(error, TakeNativeError(state, HostErrorReason::Cleanup, state.stage));
        return error;
    }

    std::optional<HostError> PollNativeInput(DeviceHostState &state)
    {
        if (auto error = TakeInputFault(state))
        {
            return error;
        }
        BeginNativeOperation(state);
        glfwPollEvents();
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
        InjectNativeError(state, HostStage::EventPolling);
#endif
        return FinishEvents(state);
    }
    std::optional<HostError> WaitNativeInput(DeviceHostState &state, double timeoutSeconds)
    {
        if (!std::isfinite(timeoutSeconds) || timeoutSeconds <= 0.0)
        {
            return MakeError(&state, HostErrorReason::InvalidConfiguration, state.stage,
                             "Wait timeout must be finite positive seconds");
        }
        if (auto error = TakeInputFault(state))
        {
            return error;
        }
        BeginNativeOperation(state);
        glfwWaitEventsTimeout(timeoutSeconds);
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
        InjectNativeError(state, HostStage::EventWait);
#endif
        return FinishEvents(state);
    }

    std::optional<HostError> ApplyNativeRequests(DeviceHostState &state, std::span<const HostRequest> requests)
    {
        for (std::size_t index = 0; index < requests.size(); ++index)
        {
            if (const auto *cursor = std::get_if<SetCursorMode>(&requests[index]))
            {
                if (cursor->mode > CursorMode::Relative ||
                    (cursor->mode == CursorMode::Relative && !state.input.Window().focused))
                {
                    auto error = MakeError(&state, HostErrorReason::RequestRejected, state.stage,
                                           "Invalid cursor mode or unfocused relative capture");
                    error.requestIndex = index;
                    return error;
                }
            }
        }
        if (auto error = TakeInputFault(state))
        {
            return error;
        }
        if (state.input.NeedsCursorRelease())
        {
            if (auto error = ReleaseRelativeCursor(state))
            {
                return error;
            }
        }
        if (auto error = TakeInputFault(state))
        {
            return error;
        }
        for (std::size_t index = 0; index < requests.size(); ++index)
        {
            BeginNativeOperation(state);
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
            InjectNativeError(state, HostStage::RequestApplication);
            if (state.checks.failRequestIndex == index)
            {
                static_cast<void>(glfwGetInputMode(state.window, -1));
            }
#endif
            if (auto error = TakeRequestFailure(state, index))
            {
                return error;
            }
            BeginNativeOperation(state);
            if (const auto *cursor = std::get_if<SetCursorMode>(&requests[index]))
            {
                const bool changed = cursor->mode != state.input.Snapshot().cursorMode;
                if (changed)
                {
                    state.input.ResetMotion();
                }
                glfwSetInputMode(state.window, GLFW_CURSOR,
                                 cursor->mode == CursorMode::Relative ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
                if (cursor->mode == CursorMode::Relative && state.checks.loseFocusOnCapture)
                {
                    glfwIconifyWindow(state.window);
                }
#endif
                const int observed = glfwGetInputMode(state.window, GLFW_CURSOR);
                if (auto error = TakeRequestFailure(state, index))
                {
                    return error;
                }
                if (observed != (cursor->mode == CursorMode::Relative ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL))
                {
                    auto error = MakeError(&state, HostErrorReason::RequestApplication, state.stage,
                                           "Native cursor mode request was not applied");
                    error.requestIndex = index;
                    return error;
                }
                state.input.ObserveCursorMode(cursor->mode);
                if (changed)
                {
                    state.input.ResetMotion();
                }
                if (cursor->mode == CursorMode::Relative)
                {
                    BeginNativeOperation(state);
                    const bool focused = glfwGetWindowAttrib(state.window, GLFW_FOCUSED) == GLFW_TRUE;
                    if (auto error = TakeRequestFailure(state, index))
                    {
                        return error;
                    }
                    if (!focused || state.input.NeedsCursorRelease())
                    {
                        if (!focused && state.input.Window().focused)
                        {
                            state.input.Record(FocusEvent{false});
                        }
                        if (auto error = TakeRequestFailure(state, index))
                        {
                            return error;
                        }
                        auto error = MakeError(&state, HostErrorReason::RequestRejected, state.stage,
                                               "Focus lost while applying relative capture");
                        error.requestIndex = index;
                        if (auto cleanup = ReleaseRelativeCursor(state))
                        {
                            AttachCleanupFailure(error, *cleanup);
                        }
                        if (auto inputError = TakeInputFault(state))
                        {
                            inputError->requestIndex = index;
                            inputError->cleanupError = error.cleanupError;
                            inputError->additionalCleanupErrors = error.additionalCleanupErrors;
                            return inputError;
                        }
                        return error;
                    }
                }
            }
            else
            {
                const auto &close = std::get<SetCloseRequested>(requests[index]);
                glfwSetWindowShouldClose(state.window, close.requested ? GLFW_TRUE : GLFW_FALSE);
                const bool observed = glfwWindowShouldClose(state.window) == GLFW_TRUE;
                if (auto error = TakeRequestFailure(state, index))
                {
                    return error;
                }
                if (observed != state.input.Window().closeRequested)
                {
                    state.input.Record(CloseRequestedEvent{observed});
                }
            }
            if (auto error = TakeRequestFailure(state, index))
            {
                return error;
            }
            if (state.input.NeedsCursorRelease())
            {
                if (auto error = ReleaseRelativeCursor(state))
                {
                    error->requestIndex = index;
                    return error;
                }
            }
            if (auto error = TakeRequestFailure(state, index))
            {
                return error;
            }
        }
        return std::nullopt;
    }
}
