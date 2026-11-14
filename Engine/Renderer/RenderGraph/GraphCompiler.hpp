#pragma once

/// @file
/// @brief 定义封存图的 CPU 编译入口及执行计划. 调用方提供能力和预算, 编译器输出有序用途与存储映射.
/// 计划由执行器消费, 通过 PassId 和资源身份引用原图.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <variant>
#include <vector>

#include "GraphResources.hpp"

namespace Physara::Renderer
{
    class RenderGraph;

    /// @brief 以格式, 存储维度和样本数为键的能力记录. 缺少组合表示不支持.
    /// @pre 调用方为每个组合提供唯一记录. maxDepth 是体深, maxLayers 是数组层或面数.
    /// formatSupport 描述此组合的存储和同格式视图能力, 传输和只读深度联合采样有各自的标记.
    /// maxSampledLayers 限定一次采样视图的层数, 0 表示不支持采样. 纹理 ShaderRead 导出也遵守此限制.
    /// readOnlyDepthSampling 用于同一 mip/layer 上的采样和只读深度/模板附件.
    /// presentation 标记可交给窗口 owner 的组合, 编译另行核对形状和宿主结束状态.
    struct GraphTextureSupport
    {
        RHI::Format format = RHI::Format::UNKNOWN;
        RHI::TextureDimension dimension = RHI::TextureDimension::Unknown;
        std::uint32_t sampleCount = 0;
        std::uint32_t maxWidth = 0;
        std::uint32_t maxHeight = 0;
        std::uint32_t maxDepth = 0;
        std::uint32_t maxLayers = 0;
        RHI::FormatSupport formatSupport = RHI::FormatSupport::None;
        bool copySource = false;
        bool copyDestination = false;
        bool clear = false;
        bool readOnlyDepthSampling = false;
        std::uint32_t maxSampledLayers = 0;
        bool presentation = false;
    };

    /// @brief 普通图 Buffer 的存储容量, 常量及结构化读视图的最大范围与偏移对齐, 单位为字节.
    /// @note 零视图容量或零偏移对齐表示相应用途不可用. 公共 RHI 的常量子区间对齐另行检查.
    struct GraphBufferSupport
    {
        std::uint64_t maxBytes = 0;
        std::uint64_t maxConstantBytes = 0;
        std::uint64_t constantOffsetAlignment = 0;
        std::uint64_t maxStructuredBytes = 0;
        std::uint64_t structuredOffsetAlignment = 0;
    };

    /// @brief 调用方提供的 CPU 能力表和瞬态逻辑字节预算. 边界状态表按完整状态组合匹配.
    /// @note 能力表涵盖实际存储及用途, 借用至编译返回. 预算按不同瞬态槽累计, 导入由外部 owner 持有.
    /// presentationFinalState 为窗口 owner 要求的完整出口状态, 必须与呈现导入的 final 相等.
    /// 首发 PresentColor 使用 ShaderResource. 导入的进出状态仍须各自匹配纹理边界状态表.
    struct GraphCompileOptions
    {
        std::span<const GraphTextureSupport> textures;
        GraphBufferSupport buffers;
        std::span<const RHI::ResourceStates> textureBoundaryStates;
        std::span<const RHI::ResourceStates> bufferBoundaryStates;
        std::uint32_t maxColorAttachments = 0;
        std::uint64_t transientByteBudget = 0;
        RHI::ResourceStates presentationFinalState = RHI::ResourceStates::ShaderResource;
    };

    /// @brief Pass 声明的精确版本和范围, 以及清除之后的公共 RHI 状态请求.
    /// @note 写记录保留 Load/Store 与上一版本. 纯传输 Clear 的普通状态为空, 由清除步骤请求状态.
    /// 普通用途按纹理 mip/layer 和整 Buffer 校验相容性. 执行方按该粒度合并请求再提交.
    /// 原始 aspect 与字节区间仍用于内容回溯和 Pass 资源权限.
    struct PlannedGraphUse
    {
        PassUse declaration;
        std::optional<RHI::ResourceStates> state;
    };

    /// @brief 普通 Pass 用途之前的清除步骤.
    /// @note useIndex 指向同一 Pass 的 uses, 其中的写声明提供精确范围和清除值. state 用于清除前转换.
    struct PlannedGraphClear
    {
        std::size_t useIndex;
        RHI::ResourceStates state = RHI::ResourceStates::CopyDest;
    };

    /// @brief 供执行器物化的单 mip, 单层附件视图描述, 使用原资源格式.
    /// @note resource 用版本 0 表示存储身份, 实际访问版本从 Pass 用途取得.
    /// readOnly 对应深度/模板附件的整体访问模式.
    struct PlannedGraphAttachment
    {
        TextureId resource;
        TextureRange range;
        RHI::Format format;
        bool readOnly;
    };

    /// @brief 执行序列中的一个 Pass. 先处理 clears, 再准备普通用途与视图, 最后按 id 调用原图回调.
    /// @note uses 保留声明索引, readbackUses 引用其中的 CopySource. 颜色附件按声明顺序编号.
    struct PlannedGraphPass
    {
        PassId id;
        std::vector<PlannedGraphUse> uses = std::vector<PlannedGraphUse>(0);
        std::vector<PlannedGraphClear> clears = std::vector<PlannedGraphClear>(0);
        std::vector<PlannedGraphAttachment> colorAttachments = std::vector<PlannedGraphAttachment>(0);
        std::optional<PlannedGraphAttachment> depthStencilAttachment;
        std::vector<std::size_t> readbackUses = std::vector<std::size_t>(0);
    };

    /// @brief 存活资源的物理映射. resource 为版本 0 的存储身份, 供该资源的所有版本共用.
    /// @note 首末使用为执行位置的闭区间, passes.size() 表示图尾交接.
    /// range 是图内允许范围. 导入有进出状态, 原图按身份持有强引用. 瞬态资源有物理槽号.
    template <typename Id, typename Range>
    struct PlannedGraphResource
    {
        Id resource;
        Range range;
        std::size_t firstUse;
        std::size_t lastUse;
        std::optional<std::size_t> transientSlot;
        std::optional<ImportState> importState;
    };

    using PlannedGraphTexture = PlannedGraphResource<TextureId, TextureRange>;
    using PlannedGraphBuffer = PlannedGraphResource<BufferId, RHI::BufferRange>;

    /// @brief 供顺序复用的物理存储描述. 资源映射的 transientSlot 为此项在槽表中的索引.
    /// @note bytes 是完整描述的逻辑字节数, 原生分配开销由实际分配方处理.
    struct PlannedGraphSlot
    {
        std::variant<RHI::TextureDesc, RHI::BufferDesc> desc;
        std::uint64_t bytes;
    };

    /// @brief 已验证的纯 CPU 执行计划. passes 按执行顺序保存, 资源映射按各类型的声明索引排列.
    /// @note exports 在全部 Pass 后消费, transientBytes 汇总不同瞬态槽.
    /// 计划持有自身数据, 导入强引用和回调由同图代原图保活.
    struct CompiledGraphPlan
    {
        std::uint64_t graphGeneration = 0;
        std::vector<PlannedGraphPass> passes = std::vector<PlannedGraphPass>(0);
        std::vector<PlannedGraphTexture> textures = std::vector<PlannedGraphTexture>(0);
        std::vector<PlannedGraphBuffer> buffers = std::vector<PlannedGraphBuffer>(0);
        std::vector<PlannedGraphSlot> transientSlots = std::vector<PlannedGraphSlot>(0);
        std::vector<GraphExport> exports = std::vector<GraphExport>(0);
        std::uint64_t transientBytes = 0;
    };

    /// @brief 从封存图生成完整计划, 全部校验通过后才发布.
    /// @pre 与其它图操作串行调用, graph 已封存且空闲, options 的能力表保持有效至返回.
    /// @return 成功时交付非空的只读计划所有权, 其中可包含零个 Pass. 失败仅交付自持诊断.
    /// @note 编译只读原图, 回调与原生操作留给执行方. 执行方保留同图代原图至 CPU 消费结束.
    [[nodiscard]] GraphResult<std::unique_ptr<const CompiledGraphPlan>>
    CompileRenderGraph(const RenderGraph &graph, const GraphCompileOptions &options) noexcept;
}
