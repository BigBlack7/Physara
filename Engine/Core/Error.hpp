#pragma once

// 错误边界使用自持的有界文本和原生错误上下文, 不依赖日志或原生系统已经初始化.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <source_location>
#include <string_view>

namespace Physara::Core
{
    inline constexpr std::size_t MaxTextBytes = 2048;

    // 拷贝后独立拥有文本; 截断标记计入字节预算, 有效 UTF-8 不截断半个码点.
    class BoundedText final
    {
    public:
        BoundedText() = default;
        explicit BoundedText(std::string_view text) noexcept;

        // Assign 开始一份新正文, Append 延续现有正文; 一旦截断, 后续 Append 忽略输入直到下次 Assign.
        // View 借用本对象的存储, 修改或销毁对象会结束该借用; 需要长期保留时复制 BoundedText.
        void Assign(std::string_view text) noexcept;
        void Append(std::string_view text) noexcept;
        [[nodiscard]] std::string_view View() const noexcept;
        [[nodiscard]] bool WasTruncated() const noexcept;

    private:
        std::array<char, MaxTextBytes + 1> m_Buffer{};
        std::size_t m_Size = 0;
        bool m_Truncated = false;
    };

    enum class NativeErrorDomain : std::uint8_t
    {
        Glfw,
        OpenGL
    };

    struct NativeErrorCode
    {
        NativeErrorDomain domain;
        std::int64_t value;
    };

    // source 只能来自编译期 source_location, 不保存调用方临时字符串指针.
    struct ErrorContext
    {
        BoundedText description;
        std::optional<NativeErrorCode> nativeCode;
        std::source_location source = std::source_location::current();
    };
}
