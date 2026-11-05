#pragma once

/// @file
/// @brief 错误边界使用自持的有界文本和原生错误上下文, 不依赖日志或原生系统已经初始化.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <source_location>
#include <string_view>

namespace Physara::Core
{
    inline constexpr std::size_t MaxTextBytes = 2048;

    /// @brief 无需动态分配的错误正文, 拷贝后独立拥有文本.
    /// @invariant 正文最多 MaxTextBytes 字节且始终以零结尾, 截断标记计入预算.
    /// 对有效 UTF-8 输入保留完整码点; 不承担输入编码验证.
    class BoundedText final
    {
    public:
        BoundedText() = default;
        explicit BoundedText(std::string_view text) noexcept;

        /// @brief 替换正文并重置截断状态, 超出字节预算时加入截断标记.
        void Assign(std::string_view text) noexcept;
        /// @brief 延续正文; 一旦截断, 后续追加忽略输入直到下次 Assign.
        void Append(std::string_view text) noexcept;
        /// @brief 借用当前正文; 修改或销毁对象后应重新取得视图, 长期保留应复制 BoundedText.
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

    /// @brief 由来源域解释的原生数值, 不同域的相同数值不表示同一错误.
    struct NativeErrorCode
    {
        NativeErrorDomain domain;
        std::int64_t value;
    };

    /// @brief 携带独立正文, 可选原生错误及报告位置的冷路径上下文.
    /// @note source 只能来自编译期 source_location, 不保存调用方临时字符串指针.
    struct ErrorContext
    {
        BoundedText description;
        std::optional<NativeErrorCode> nativeCode;
        std::source_location source = std::source_location::current();
    };
}
