#pragma once

/// @file
/// @brief 把普通用途或图尾请求合并为公共 RHI 状态批次. 访问授权和传输使用原声明范围.

#include <cstddef>
#include <map>
#include <span>
#include <string_view>
#include <utility>

#include <ImmediateGraphics.hpp>

#include "GraphCompiler.hpp"
#include "RenderGraph.hpp"

namespace Physara::Renderer::Detail
{
    /// @brief 物化前检查同一瞬态追踪单元的出口是否可合并. 冲突返回资源和相交范围.
    [[nodiscard]] GraphFailure CheckTransientExports(const RenderGraph &graph, const CompiledGraphPlan &plan) noexcept;
    /// @brief 将瞬态图尾的着色读或读回用途转换为相应 RHI 状态. 导入采用显式 final.
    [[nodiscard]] RHI::ResourceStates OutputState(const TextureExport &output) noexcept;
    [[nodiscard]] RHI::ResourceStates OutputState(const BufferExport &output) noexcept;

    /// @brief 保存一个阶段内兼容的状态集合. Texture 按物理 mip/layer, Buffer 按整个对象合并.
    /// @note 组合兼容性由编译及图尾预检保证. 对象借用至 Commit 返回, 记录随后由命令列表所有.
    class GraphExecutionStates final
    {
    public:
        /// @brief 收集已验证范围的状态位, 供本阶段一次提交.
        void Add(RHI::Texture &texture, const TextureRange &range, RHI::ResourceStates state);
        void Add(RHI::Buffer &buffer, RHI::ResourceStates state);
        /// @brief 逐项向列表登记状态并提交批次. 首次诊断处停止, 已登记状态由执行退出路径收拢.
        /// @note textures 和 buffers 保留独立授权范围. 图尾交接只请求状态并提交空的授权表.
        [[nodiscard]] GraphFailure Commit(RHI::GraphRecording &recording, GraphExecutionDiagnostics diagnostics,
                                          std::string_view stage, std::string_view pass,
                                          std::span<const RHI::GraphTextureUse> textures = {},
                                          std::span<const RHI::GraphBufferUse> buffers = {}) const;

    private:
        using Subresource = std::pair<RHI::ArraySlice, RHI::MipLevel>;
        std::map<RHI::Texture *, std::map<Subresource, RHI::ResourceStates>> m_Textures;
        std::map<RHI::Buffer *, RHI::ResourceStates> m_Buffers;
    };
}
