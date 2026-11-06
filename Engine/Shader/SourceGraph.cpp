#include "SourceGraph.hpp"

/// @file
/// @brief 扫描受支持的源指令并迭代建立依赖图, 将静态声明边与编译器实际展开的边区分记录.

#include <algorithm>
#include <cctype>
#include <sstream>

namespace Physara::Shader
{
    // 按 UTF-8 原字节构造 Windows 路径, 不经当前系统代码页解释逻辑源名.
    static std::filesystem::path Utf8Path(std::string_view text)
    {
        return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t *>(text.data()), text.size()));
    }

    // 仅裁掉指令周边空白, 返回仍借用原输入的视图.
    static std::string_view TrimSource(std::string_view text)
    {
        const auto begin = text.find_first_not_of(" \t\r");
        if (begin == std::string_view::npos)
        {
            return {};
        }
        return text.substr(begin, text.find_last_not_of(" \t\r") - begin + 1);
    }

    // 保留原文件坐标的源指令, 用于 profile 预检和声明图, 不替代预处理器求值.
    struct Directive
    {
        std::string word;
        std::string operand;
        std::uint32_t line = 0;
    };

    // 先拼接续行并维护原行号, 再屏蔽字符串外注释, 最后提取 include/version/extension/line.
    // 不判断条件分支或展开宏, 因而未活动 include 也参加静态依赖与环检查.
    static std::vector<Directive> ScanDirectives(std::string_view source)
    {
        // 这里只识别声明边和 profile 指令, 不展开宏或判断条件分支. 先处理续行, 再屏蔽注释.
        std::string text;
        std::vector<std::uint32_t> lines;
        std::uint32_t line = 1;
        for (std::size_t index = 0; index < source.size(); ++index)
        {
            if (source[index] == '\\' && index + 1 < source.size())
            {
                if (source[index + 1] == '\n')
                {
                    ++line;
                    ++index;
                    continue;
                }
            }
            text.push_back(source[index]);
            lines.push_back(line);
            if (source[index] == '\n')
            {
                ++line;
            }
        }
        bool block = false;
        bool comment = false;
        bool quoted = false;
        bool escaped = false;
        for (std::size_t index = 0; index < text.size(); ++index)
        {
            auto &character = text[index];
            if (character == '\n')
            {
                comment = false;
                continue;
            }
            if (block)
            {
                if (character == '*' && index + 1 < text.size() && text[index + 1] == '/')
                {
                    text[index + 1] = ' ';
                    block = false;
                }
                character = ' ';
                continue;
            }
            if (comment)
            {
                character = ' ';
                continue;
            }
            if (!quoted && character == '/' && index + 1 < text.size())
            {
                if (text[index + 1] == '*' || text[index + 1] == '/')
                {
                    block = text[index + 1] == '*';
                    comment = !block;
                    character = text[++index] = ' ';
                    continue;
                }
            }
            if (character == '"' && !escaped)
            {
                quoted = !quoted;
            }
            escaped = quoted && character == '\\' && !escaped;
        }
        std::vector<Directive> result;
        for (std::size_t begin = 0; begin < text.size();)
        {
            const auto newline = text.find('\n', begin);
            const auto end = newline == std::string::npos ? text.size() : newline;
            auto value = TrimSource(std::string_view(text).substr(begin, end - begin));
            if (value.starts_with('#'))
            {
                const auto directiveLine = lines[static_cast<std::size_t>(value.data() - text.data())];
                value = TrimSource(value.substr(1));
                const auto separator = value.find_first_of(" \t\r<\"");
                const auto word = value.substr(0, separator);
                if (word == "include" || word == "extension" || word == "version" || word == "line")
                {
                    const auto operand =
                        separator == std::string_view::npos ? std::string_view{} : TrimSource(value.substr(separator));
                    result.push_back({std::string(word), std::string(operand), directiveLine});
                }
            }
            begin = end + 1;
        }
        return result;
    }

    SourceGraph::SourceGraph(const std::filesystem::path &sourceRoot, const std::filesystem::path &interfaceRoot)
        : m_SourceRoot(CanonicalInputPath(sourceRoot)), m_InterfaceRoot(CanonicalInputPath(interfaceRoot))
    {
        if (!std::filesystem::is_directory(m_SourceRoot) || !std::filesystem::is_directory(m_InterfaceRoot) ||
            IsWithin(m_SourceRoot, m_InterfaceRoot) || IsWithin(m_InterfaceRoot, m_SourceRoot))
        {
            throw BuildError(ErrorCode::InvalidInput,
                             "Source and Interface roots must be distinct, non-overlapping directories.");
        }
    }

    std::pair<std::string, std::filesystem::path> SourceGraph::Resolve(std::string_view operand, bool system,
                                                                       std::string_view includer) const
    {
        if (operand.empty() || operand.find_first_of("\\:\r\n") != std::string_view::npos ||
            operand.find('\0') != std::string_view::npos)
        {
            throw BuildError(ErrorCode::InvalidInput, "Invalid include path: " + std::string(operand));
        }
        auto logical = system ? Utf8Path(operand) : Utf8Path(includer).parent_path() / Utf8Path(operand);
        if (logical.is_absolute())
        {
            throw BuildError(ErrorCode::InvalidInput, "Absolute source paths are not permitted.");
        }
        const auto originalLabel = logical.empty() ? std::filesystem::path{} : *logical.begin();
        logical = logical.lexically_normal();
        const auto label = logical.begin();
        if (label == logical.end() || (*label != "Source" && *label != "Interface") || *label != originalLabel)
        {
            throw BuildError(ErrorCode::InvalidInput,
                             "Include requires the unique Source/ or Interface/ virtual root.");
        }
        std::filesystem::path relative;
        for (auto component = std::next(label); component != logical.end(); ++component)
        {
            relative /= *component;
        }
        const auto &root = *label == "Source" ? m_SourceRoot : m_InterfaceRoot;
        const auto physical = CanonicalInputPath(root / relative);
        if (!IsWithin(root, physical) || !std::filesystem::is_regular_file(physical))
        {
            throw BuildError(ErrorCode::InvalidInput, "Source escapes its virtual root: " + PathText(logical));
        }
        // 同一实际文件不允许由不同逻辑拼写重复命名, 包括大小写别名和跨根链接.
        const auto name = PathText(logical);
        for (const auto &[existingName, file] : m_Files)
        {
            if (name != existingName && std::filesystem::equivalent(file.path, physical))
            {
                throw BuildError(ErrorCode::InvalidInput, "Ambiguous source identity: " + name + " / " + existingName);
            }
        }
        return {name, physical};
    }

    SourceFile &SourceGraph::ReadFile(const std::string &logical, const std::filesystem::path &path)
    {
        if (const auto previous = m_Files.find(logical); previous != m_Files.end())
        {
            return previous->second;
        }
        for (const auto &[name, file] : m_Files)
        {
            if (name != logical && std::filesystem::equivalent(file.path, path))
            {
                throw BuildError(ErrorCode::InvalidInput, "Ambiguous source identity: " + logical + " / " + name);
            }
        }
        if (m_Files.size() >= 256)
        {
            throw BuildError(ErrorCode::InvalidInput, "Source dependency count exceeds 256.");
        }
        auto text = ReadText(path, MaxSourceBytes);
        if (text.find('\0') != std::string::npos || text.size() > MaxPackageBytes - m_TotalBytes)
        {
            throw BuildError(ErrorCode::InvalidInput, "Invalid source bytes or total input exceeds 64 MiB: " + logical);
        }
        m_TotalBytes += text.size();
        std::string normalized;
        if (text.find('\r') != std::string::npos)
        {
            // 固定 glslang 对裸 CR 不推进行号. 扫描和编译共用 LF 视图, 原字节仍是身份输入.
            normalized.reserve(text.size());
            for (std::size_t index = 0; index < text.size(); ++index)
            {
                if (text[index] == '\r')
                {
                    normalized.push_back('\n');
                    if (index + 1 < text.size() && text[index + 1] == '\n')
                    {
                        ++index;
                    }
                }
                else
                {
                    normalized.push_back(text[index]);
                }
            }
        }
        auto &file =
            m_Files.emplace(logical, SourceFile{logical, path, std::move(text), std::move(normalized), {}, false})
                .first->second;
        for (const auto &directive : ScanDirectives(file.GetCompilerText()))
        {
            const auto context = logical + ":" + std::to_string(directive.line) + ": ";
            if (directive.word == "line")
            {
                // 用户重映射可能伪装成另一条声明边的行号. 原文件坐标只由编译器 include 流程管理.
                throw BuildError(ErrorCode::UnsupportedProfile, context + "#line is reserved for compiler source mapping");
            }
            else if (directive.word == "extension")
            {
                const auto colon = directive.operand.find(':');
                const auto extension = TrimSource(std::string_view(directive.operand).substr(0, colon));
                // samplerless 仅开放已规划 fetch 的源写法, 最终 IR 仍由原有 profile/ABI 边界核对.
                if (colon == std::string::npos ||
                    (extension != "GL_GOOGLE_include_directive" && extension != "GL_EXT_samplerless_texture_functions") ||
                    (TrimSource(std::string_view(directive.operand).substr(colon + 1)) != "require" &&
                     TrimSource(std::string_view(directive.operand).substr(colon + 1)) != "enable"))
                {
                    throw BuildError(ErrorCode::UnsupportedProfile,
                                     context + "extension is outside GraphicsBaselineV1");
                }
            }
            else if (directive.word == "version")
            {
                const auto value = std::string_view(directive.operand);
                const bool version =
                    value == "450" || (value.starts_with("450") && value.size() > 3 &&
                                       (value[3] == ' ' || value[3] == '\t') && TrimSource(value.substr(3)) == "core");
                if (!version)
                {
                    throw BuildError(ErrorCode::UnsupportedProfile, context + "GraphicsBaselineV1 requires GLSL 450");
                }
            }
            else
            {
                const auto &value = directive.operand;
                const bool system = value.starts_with('<');
                const char close = system ? '>' : '"';
                if (value.size() < 3 || (!system && !value.starts_with('"')) || value.back() != close ||
                    value.find(close, 1) != value.size() - 1)
                {
                    throw BuildError(ErrorCode::InvalidInput,
                                     context + "include requires one literal path; macros are unsupported");
                }
                const auto operand = value.substr(1, value.size() - 2);
                try
                {
                    const auto [target, physical] = Resolve(operand, system, logical);
                    const auto edge = m_Edges.size();
                    m_Edges.push_back({logical, target, directive.line, false});
                    file.includes.push_back({operand, target, physical, system, edge});
                }
                catch (const BuildError &error)
                {
                    // 原始失败原因保留在有界诊断头部, 深层引用链不能把 depth/cycle 等原因挤掉.
                    throw BuildError(error.GetFailure().code, std::string(error.what()) + "\nIncluded from " + logical +
                                                                  ":" + std::to_string(directive.line));
                }
                catch (const std::filesystem::filesystem_error &error)
                {
                    throw BuildError(ErrorCode::IoFailure, context + error.what());
                }
            }
        }
        return file;
    }

    const SourceFile &SourceGraph::Load(const std::string &logical, const std::filesystem::path &path)
    {
        auto &root = ReadFile(logical, path);
        if (root.complete)
        {
            return root;
        }
        struct Visit
        {
            SourceFile *file;
            std::size_t nextInclude = 0;
        };
        // 显式 DFS 栈将图深度预算与线程栈解耦. complete 仅在所有声明子边完成后发布.
        std::vector<Visit> visits(0);
        visits.push_back({&root, 0});
        while (!visits.empty())
        {
            auto &visit = visits.back();
            if (visit.nextInclude == visit.file->includes.size())
            {
                visit.file->complete = true;
                visits.pop_back();
                continue;
            }
            const auto &include = visit.file->includes[visit.nextInclude++];
            const auto &edge = m_Edges[include.edge];
            if (visits.size() > 64)
            {
                throw BuildError(ErrorCode::InvalidInput, "Include depth exceeds 64 at " + include.target +
                                                              "\nIncluded from " + edge.includingPath + ":" +
                                                              std::to_string(edge.line));
            }
            const auto previous = m_Files.find(include.target);
            if (previous != m_Files.end())
            {
                if (!previous->second.complete)
                {
                    Core::BoundedText diagnostic("Declared include cycle reaches ");
                    diagnostic.Append(include.target);
                    for (const auto &parent : visits)
                    {
                        diagnostic.Append("\nIncluded from ");
                        diagnostic.Append(parent.file->logicalPath);
                    }
                    throw BuildError(ErrorCode::InvalidInput, diagnostic.View());
                }
                continue;
            }
            auto &child = ReadFile(include.target, include.path);
            visits.push_back({&child, 0});
        }
        return root;
    }

    const SourceFile &SourceGraph::AddSource(std::string_view relative)
    {
        const auto [logical, physical] = Resolve("Source/" + std::string(relative), true, {});
        return Load(logical, physical);
    }

    SourceGraph::IncludeResult *SourceGraph::Expand(const char *name, const char *includer, std::size_t depth,
                                                    bool system)
    {
        if (depth > 64)
        {
            throw BuildError(ErrorCode::InvalidInput, "Expanded include depth exceeds 64.");
        }
        const auto &file = m_Files.at(includer);
        const auto found = std::ranges::find_if(file.includes,
                                                [&](const auto &include)
                                                {
                                                    return include.operand == name && include.system == system;
                                                });
        if (found == file.includes.end())
        {
            throw BuildError(ErrorCode::InvalidInput,
                             "Compiler requested an undeclared include from " + std::string(includer));
        }
        const auto &included = m_Files.at(found->target);
        // IncludeResult 借用源图节点中的字节. releaseInclude 只结束这次展开, 同一节点还可供后续 include 使用.
        const auto text = included.GetCompilerText();
        m_Results.emplace_back(found->target, text.data(), text.size(), nullptr);
        return &m_Results.back();
    }

    SourceGraph::IncludeResult *SourceGraph::includeSystem(const char *name, const char *includer, std::size_t depth)
    {
        return Expand(name, includer, depth, true);
    }

    void SourceGraph::RecordInclude(const char *target, const char *includer, std::size_t line)
    {
        const auto &file = m_Files.at(includer);
        const auto found =
            std::ranges::find_if(file.includes,
                                 [&](const auto &include)
                                 {
                                     return include.target == target && m_Edges[include.edge].line == line;
                                 });
        if (found == file.includes.end())
        {
            throw BuildError(ErrorCode::InvalidInput, "Expanded include does not match its original source location: " +
                                                          std::string(includer) + ":" + std::to_string(line));
        }
        // glslang 在实际处理成功的 include 处通知原引用行, 不把同目标的其它声明边标成已展开.
        m_Edges[found->edge].expanded = true;
    }

    SourceGraph::IncludeResult *SourceGraph::includeLocal(const char *name, const char *includer, std::size_t depth)
    {
        return Expand(name, includer, depth, false);
    }

    void SourceGraph::releaseInclude(IncludeResult *result)
    {
        const auto found = std::ranges::find_if(m_Results,
                                                [result](const auto &value)
                                                {
                                                    return &value == result;
                                                });
        if (found != m_Results.end())
        {
            m_Results.erase(found);
        }
    }
}
