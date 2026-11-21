#pragma once

/// @file
/// @brief 在调用方已打开的即时 Graphics 录制中消费封存图和只读计划.
/// Pass 先借用准备器建立绑定, 再借用查询入口和命令列表录制. 图尾返回外部消费所需的资源句柄.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <variant>
#include <vector>

#include "GraphCompiler.hpp"
#include "RenderGraph.hpp"

namespace Physara::Renderer
{
    /// @brief 用当前 Pass 声明生成绑定条目, 并持有本次 prepare 创建的集合.
    /// @note 准备器和中间条目只在同步 prepare 中使用. 成功的集合按从 0 开始的创建索引借给 execute,
    /// 用到的集合随后由 RHI 录制持有. 准备失败会阻止该 Pass 的清除和执行回调.
    class PassPreparation final
    {
    public:
        PassPreparation(const PassPreparation &) = delete;
        PassPreparation &operator=(const PassPreparation &) = delete;

        /// @brief 按单条采样声明解析版本和范围, 建立供本次创建集合使用的同格式条目.
        /// @note Packed depth/stencil 的同格式采样条目使用明确的 Depth 范围.
        /// cube 消费者显式指定 TextureCube, 其范围须包含实际采样的完整六面.
        /// @return 身份, 范围, 权限或视图用途被拒绝时返回空并记录准备失败.
        [[nodiscard]] std::optional<RHI::BindingSetItem> SampledTexture(
            std::uint32_t slot, TextureId id, std::optional<TextureRange> range = std::nullopt,
            RHI::TextureDimension dimension = RHI::TextureDimension::Unknown);
        /// @brief 按 ConstantBuffer 或 ShaderRead 声明生成常量或结构化读条目, 保留确切字节范围.
        /// @return 请求失败时返回空并记录准备失败.
        [[nodiscard]] std::optional<RHI::BindingSetItem> ReadBuffer(
            std::uint32_t slot, BufferId id, GraphUsage usage, std::optional<RHI::BufferRange> range = std::nullopt);

        /// @brief 按当前 Pass 的版本, 存储用途及范围生成一个 mip 的 image 条目.
        /// @param dimension 视图目标须匹配活动 Shader. 省略时沿用存储目标, 数组的单层可显式选择 Texture2D.
        [[nodiscard]] std::optional<RHI::BindingSetItem> StorageImage(
            std::uint32_t slot, TextureId id, GraphUsage usage, std::optional<TextureRange> range = std::nullopt,
            RHI::TextureDimension dimension = RHI::TextureDimension::Unknown);
        /// @brief 为声明版本内的字节范围生成结构化 UAV 条目. 读改写使用 Write 返回的版本.
        /// @note 两种 storage 准备入口在权限拒绝时返回空并留下首错, 成功的条目可交给当前 Pass 的 CreateBindingSet.
        [[nodiscard]] std::optional<RHI::BindingSetItem> StorageBuffer(
            std::uint32_t slot, BufferId id, GraphUsage usage, std::optional<RHI::BufferRange> range = std::nullopt);

        /// @brief 用本次准备生成的资源条目及调用方的 Sampler 建立普通绑定集.
        /// @note 布局和条目在调用期间借用, RHI 校验同设备归属和条目匹配后建立对象引用.
        /// @return 供 PassResources::BindingSet 使用的索引. 条目或设备创建被拒绝时记录准备失败并返回空.
        /// @throws std::bad_alloc 本地存储不足时可由本次图执行的准备阶段接收.
        [[nodiscard]] std::optional<std::size_t> CreateBindingSet(
            RHI::BindingLayout &layout, std::span<const RHI::BindingSetItem> items);

    private:
        friend class Detail::GraphExecutor;
        PassPreparation(RHI::Device &device, PassResources &resources);

        RHI::Device &m_Device;
        PassResources &m_Resources;
        std::vector<RHI::BindingSetItem> m_Authorized;
        std::vector<RHI::BindingSetHandle> m_BindingSets;
    };

    /// @brief 拥有实际资源及原图的版本, 范围与外部用途. 交付顺序沿计划的 exports.
    /// @note 句柄可跨图销毁保留. 声明中的资源身份仍属原图代, 跨录制消费使用 Close 后的状态与真实提交顺序.
    template <typename Export, typename Handle>
    struct GraphOutput
    {
        Export declaration;
        Handle object;
    };

    using GraphTextureOutput = GraphOutput<TextureExport, RHI::TextureHandle>;
    using GraphBufferOutput = GraphOutput<BufferExport, RHI::BufferHandle>;
    using GraphExecutionOutputList = std::vector<std::variant<GraphTextureOutput, GraphBufferOutput>>;

    /// @brief 成功结果独占只读图尾输出表. 零导出时也持有空表, 元素借用止于持有者释放.
    using GraphExecutionOutputs = std::unique_ptr<const GraphExecutionOutputList>;

    /// @brief 按计划顺序物化资源, 消费存活 Pass, 并在同一次录制中完成图尾状态交接.
    /// @pre graph 已封存且空闲, plan 来自同一图代. 在设备 owner 线程上提供打开且有效的 Automatic Graphics 录制,
    /// 并先用 CommitBarriers 提交已有的状态请求. diagnostics 从该设备当次调用前同步取样, scope 借用至返回.
    /// @note 回调借用现有录制和本 Pass 授权范围. 调用方负责 Close, 提交和完成观察.
    /// @return 图尾和录制恢复完成后交付导出. 失败返回首错, 已发前缀继续随原录制或退出协议保活.
    [[nodiscard]] GraphResult<GraphExecutionOutputs> ExecuteRenderGraph(
        const RenderGraph &graph, const CompiledGraphPlan &plan, RHI::Device &device, RHI::CommandList &commands,
        GraphExecutionDiagnostics diagnostics);
}
