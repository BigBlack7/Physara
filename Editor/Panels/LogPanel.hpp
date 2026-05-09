#pragma once

#include <string>
#include <vector>

namespace Physara::Editor
{
    class LogPanel final
    {
    public:
        LogPanel() = default;

        void Draw();

    private:
        void Refresh();

        std::vector<std::string> m_CachedLogs{};
        bool m_AutoScroll{true};
    };
}