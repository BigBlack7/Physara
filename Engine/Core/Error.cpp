#include "Error.hpp"

/// @file
/// @brief 实现固定字节预算的文本复制/追加, 截断时为有效 UTF-8 保留完整码点及标记.

#include <algorithm>
#include <cstring>

namespace Physara::Core
{
    static constexpr std::string_view TruncationMarker = "...[truncated]";

    // 将字节上限退到 UTF-8 码点边界, 不扫描或验证整个输入.
    static std::size_t Utf8PrefixLength(std::string_view text, std::size_t limit) noexcept
    {
        // count 指向保留前缀后的首字节; 若落在续字节上, 回退到该码点的起始位置.
        std::size_t count = std::min(text.size(), limit);
        if (count < text.size())
        {
            while (count > 0 && (static_cast<unsigned char>(text[count]) & 0xC0u) == 0x80u)
            {
                --count;
            }
        }
        return count;
    }
}

namespace Physara::Core
{
    BoundedText::BoundedText(std::string_view text) noexcept
    {
        Assign(text);
    }

    void BoundedText::Assign(std::string_view text) noexcept
    {
        m_Size = 0;
        m_Truncated = false;
        Append(text);
        m_Buffer[m_Size] = '\0';
    }

    void BoundedText::Append(std::string_view text) noexcept
    {
        if (m_Truncated)
        {
            return;
        }
        if (text.size() <= MaxTextBytes - m_Size)
        {
            if (!text.empty())
            {
                std::memmove(m_Buffer.data() + m_Size, text.data(), text.size());
            }
            m_Size += text.size();
        }
        else
        {
            // 先从已有正文腾出标记空间, 再追加新正文, 两段合计仍受同一字节预算约束.
            constexpr auto BodyLimit = MaxTextBytes - TruncationMarker.size();
            m_Size = Utf8PrefixLength(View(), BodyLimit);
            const auto copied = Utf8PrefixLength(text, BodyLimit - m_Size);
            if (copied > 0)
            {
                std::memmove(m_Buffer.data() + m_Size, text.data(), copied);
            }
            m_Size += copied;
            std::memcpy(m_Buffer.data() + m_Size, TruncationMarker.data(), TruncationMarker.size());
            m_Size += TruncationMarker.size();
            m_Truncated = true;
        }
        m_Buffer[m_Size] = '\0';
    }

    std::string_view BoundedText::View() const noexcept
    {
        return {m_Buffer.data(), m_Size};
    }

    bool BoundedText::WasTruncated() const noexcept
    {
        return m_Truncated;
    }
}
