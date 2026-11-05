#pragma once

// 一次请求拥有全部源内容. 静态声明图负责无环/依赖, glslang 回调只展开已核对的边.

#include <list>
#include <map>

#include <glslang/Public/ShaderLang.h>

#include "BuildSupport.hpp"

namespace Physara::Shader
{
    struct SourceFile
    {
        std::string logicalPath;
        std::filesystem::path path;
        std::string text;
        std::string normalizedText;
        [[nodiscard]] std::string_view GetCompilerText() const noexcept
        {
            return normalizedText.empty() ? std::string_view(text) : std::string_view(normalizedText);
        }
        struct Include
        {
            std::string operand;
            std::string target;
            std::filesystem::path path;
            bool system = false;
            std::size_t edge = 0;
        };
        std::vector<Include> includes;
        bool complete = false;
    };

    class SourceGraph final : public glslang::TShader::Includer
    {
      public:
        SourceGraph(const std::filesystem::path &sourceRoot, const std::filesystem::path &interfaceRoot);
        SourceGraph(const SourceGraph &) = delete;
        SourceGraph &operator=(const SourceGraph &) = delete;
        SourceGraph(SourceGraph &&) = delete;
        SourceGraph &operator=(SourceGraph &&) = delete;
        const SourceFile &AddSource(std::string_view relative);
        [[nodiscard]] const std::map<std::string, SourceFile> &GetFiles() const noexcept
        {
            return m_Files;
        }
        [[nodiscard]] const std::vector<IncludeEdge> &GetEdges() const noexcept
        {
            return m_Edges;
        }
        // Physara 的 glslang 本地适配提供原引用行, 用于区分相同目标的多次声明.
        void RecordInclude(const char *target, const char *includer, std::size_t line) override;
        // glslang::TShader::Includer 的固定虚函数名, 必须保留上游拼写才能 override.
        // 这三个回调属于外部接口命名例外, 模块自己的函数仍使用 PascalCase.
        IncludeResult *includeSystem(const char *name, const char *includer, std::size_t depth) override;
        IncludeResult *includeLocal(const char *name, const char *includer, std::size_t depth) override;
        void releaseInclude(IncludeResult *result) override;

      private:
        std::filesystem::path m_SourceRoot;
        std::filesystem::path m_InterfaceRoot;
        std::map<std::string, SourceFile> m_Files;
        std::vector<IncludeEdge> m_Edges;
        std::list<IncludeResult> m_Results;
        std::size_t m_TotalBytes = 0;

        std::pair<std::string, std::filesystem::path> Resolve(std::string_view operand, bool system,
                                                              std::string_view includer) const;
        SourceFile &ReadFile(const std::string &logical, const std::filesystem::path &path);
        const SourceFile &Load(const std::string &logical, const std::filesystem::path &path);
        IncludeResult *Expand(const char *name, const char *includer, std::size_t depth, bool system);
    };
}
