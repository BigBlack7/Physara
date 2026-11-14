#pragma once

/// @file
/// @brief 图编译内部阶段的共同入口. 用途阶段先为封存声明准备计划值, 编译器据依赖确定顺序,
/// 存储阶段再以该顺序填写寿命和槽映射. 各阶段的输出由编译调用方暂存.

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <vector>

#include "GraphCompiler.hpp"

namespace Physara::Renderer::Detail
{
    /// @brief 编译诊断的位置. 类型, 索引和版本定位封存图中的记录.
    /// @note name 借用原图. 生成的诊断独立持有文本, 可在原图销毁后报告.
    struct CompileResourceLocation
    {
        enum class Kind : std::uint8_t
        {
            Texture,
            Buffer
        };

        Kind kind;
        std::uint32_t index;
        std::uint32_t version;
        std::string_view name;
    };

    /// @brief 从已校验的类型化身份保留诊断必需的字段.
    template <typename Tag>
    [[nodiscard]] CompileResourceLocation LocateResource(GraphId<Tag> id, std::string_view name) noexcept
    {
        using Kind = CompileResourceLocation::Kind;
        return {std::is_same_v<Tag, GraphTextureTag> ? Kind::Texture : Kind::Buffer,
                id.identity.index, id.version, name};
    }

    /// @brief 将索引或状态值写入有界诊断, 无需分配临时字符串.
    /// @pre 本编译器使用十进制索引和十六进制状态, base 取 10 或 16.
    void AppendDiagnosticNumber(Core::BoundedText &text, std::size_t value, int base = 10) noexcept;

    [[nodiscard]] inline GraphError CompileError(GraphErrorCode code, std::string_view reason) noexcept
    {
        return MakeGraphError(code, "Compile", {}, {}, reason);
    }

    /// @brief 先保留原因及图内索引, 再加入可为空或过长的名称.
    [[nodiscard]] GraphError CompileError(GraphErrorCode code, std::string_view reason, CompileResourceLocation resource,
                                          const PassDeclaration *pass = nullptr,
                                          const PassDeclaration *predecessor = nullptr) noexcept;

    /// @brief 裁剪前核对全部声明, 按声明索引准备可转交给最终计划的 Pass 用途.
    /// @pre graph 已封存, passes 已按声明数量预建, options 的能力表在本次编译期间有效.
    /// @note 失败时调用方丢弃整份临时用途表.
    [[nodiscard]] GraphFailure BuildPassPlans(const RenderGraph &graph, const GraphCompileOptions &options,
                                               std::vector<PlannedGraphPass> &passes);

    /// @brief 从有序用途和图尾导出建立使用区间, 按兼容描述分配瞬态槽.
    /// @pre plan 已填写同图代的存活顺序和导出, 资源映射与槽表为空, 预算总量为零.
    /// @note 失败时调用方丢弃整份临时计划, 包括已登记的资源和槽.
    [[nodiscard]] GraphFailure BuildStoragePlan(const RenderGraph &graph, std::uint64_t budget,
                                                CompiledGraphPlan &plan);
}
