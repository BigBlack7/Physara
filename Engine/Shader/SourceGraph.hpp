#pragma once

/// @file
/// @brief 一次编译请求拥有的源图, 静态声明图管理依赖, glslang 回调仅展开已核对的边.

#include <list>
#include <map>

#include <glslang/Public/ShaderLang.h>

#include "BuildSupport.hpp"

namespace Physara::Shader
{
    /// @brief 一个稳定源节点, 原始 text 用于身份, normalizedText 仅在需要时提供统一 LF 编译视图.
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

    /// @brief 同时保存静态 include 图与实际展开记录, 持有编译器借用的全部源字节.
        /// @note 单请求串行使用, 不可移动; glslang 的 include 结果必须在图销毁前释放.
        /// 图构建失败会结束本次编译请求, 不将未完成的节点作为另一请求的缓存复用.
    class SourceGraph final : public glslang::TShader::Includer
    {
      public:
        /// @brief 取得两个无重定向的规范根, 不读取入口或展开宏.
        SourceGraph(const std::filesystem::path &sourceRoot, const std::filesystem::path &interfaceRoot);
        SourceGraph(const SourceGraph &) = delete;
        SourceGraph &operator=(const SourceGraph &) = delete;
        SourceGraph(SourceGraph &&) = delete;
        SourceGraph &operator=(SourceGraph &&) = delete;
        /// @brief 读取相对入口并迭代建立完整声明图, 拒绝越根, 别名, 环与预算溢出.
        /// @return 由本图拥有的稳定节点引用, 不跨图销毁保留.
        const SourceFile &AddSource(std::string_view relative);
        [[nodiscard]] const std::map<std::string, SourceFile> &GetFiles() const noexcept
        {
            return m_Files;
        }
        [[nodiscard]] const std::vector<IncludeEdge> &GetEdges() const noexcept
        {
            return m_Edges;
        }
        /// @brief 本地 glslang 适配提供原引用行, 只把这一次实际处理的声明边标为展开.
        void RecordInclude(const char *target, const char *includer, std::size_t line) override;
        /// @brief glslang::TShader::Includer 的固定回调, 保留上游拼写以满足 override 协议.
        /// @note includeSystem/includeLocal 共用受控源图; releaseInclude 只结束结果借用, 不删除源节点.
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

        /// @brief 解析受控逻辑根与物理路径, 同一实际文件不允许多种逻辑身份.
        std::pair<std::string, std::filesystem::path> Resolve(std::string_view operand, bool system,
                                                              std::string_view includer) const;
        /// @brief 首次读取时保存字节, 扫描指令并记录声明边; 已有节点直接复用.
        SourceFile &ReadFile(const std::string &logical, const std::filesystem::path &path);
        /// @brief 用显式 DFS 完成静态声明图, complete 只在全部子边完成后发布.
        const SourceFile &Load(const std::string &logical, const std::filesystem::path &path);
        /// @brief 为编译器分配借用已加载节点的 include 结果, 不动态发现声明图以外的文件.
        IncludeResult *Expand(const char *name, const char *includer, std::size_t depth, bool system);
    };
}
