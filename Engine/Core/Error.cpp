#include "Error.hpp"

#include <algorithm>
#include <cstring>

namespace
{
    constexpr std::string_view TruncationMarker = "...[truncated]";

    std::size_t Utf8PrefixLength(std::string_view text, std::size_t limit) noexcept
    {
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
