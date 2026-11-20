#pragma once

/// @file
/// @brief 产品上传的图声明与存活账单. 本模块不打开列表, 提交或确认纹理请求.
/// CPU 载荷由调用方的本轮稳定存储保活, 编译后的存活 Pass 决定实际上传额度.

#include "RenderGraph/GraphExecutor.hpp"

namespace Physara::Renderer
{
    /// @brief 上传账单按数据来源分类. ForwardDraw 合计各路实例块和间接参数, ForwardView 包含观察及显示常量.
    /// 静态行和灯表记入 ForwardStatic. UI 与诊断使用 Other. 所有存活写入共用一次设备准入.
    enum class GraphUploadSource : std::uint8_t { Other, ForwardStatic, ForwardView, ForwardDraw };

    /// @brief 一次实际 Write 的大小和所属 Pass, 用于按编译顺序核算共享上传池.
    struct GraphUpload
    {
        PassId pass;
        std::size_t bytes = 0;
        bool texture = false;
        GraphUploadSource source{};
    };

    /// @brief 一次纹理写的版本和终结 Pass. 局部更新的终结 Pass 可以是后续复制.
    struct GraphTextureWrite
    {
        TextureId texture;
        PassId pass;
    };

    /// @brief 完整定义颜色纹理的一层 mip 和 array slice, 载荷在执行结束前由帧槽保活.
    /// @pre mip 和 layer 在 target 的范围内, pixels 覆盖完整紧密像素, rowPitch 匹配行宽.
    /// @param recorded 可选的存储代标记, 由调用方保活至回调结束. 回调在调用 WriteTexture 前置位,
    /// 即使随后录制失败也保留该值. 上传版本的接受由调用方依据完整图回执确认.
    /// @note 声明成功才追加账单. 返回错误或存储分配抛出后放弃候选图, 不在部分图上重试.
    [[nodiscard]] GraphResult<GraphTextureWrite> AddTextureUpload(
        RenderGraph &graph, TextureId target, std::span<const std::byte> pixels, std::size_t rowPitch,
        std::vector<GraphUpload> &uploads, std::uint32_t mip = 0, std::uint32_t layer = 0, bool *recorded = nullptr);
    /// @brief 写入指定偏移处的实际字节区间, 后续消费者须声明所读的有效范围.
    /// @param recorded 可选的拥有者标记, 执行尝试写入时锁存, 与完整图的接受状态独立.
    /// @param offset 从物理 Buffer 起点计的字节偏移, 导入页子区间时也使用同一坐标.
    /// @pre bytes 非空, 位于 target 的允许范围内, 底层字节在图执行结束前保持地址及内容稳定.
    /// @note 同一 Pass 对应一次 WriteBuffer. 几何缓冲的剩余容量不属于本次已初始化内容.
    [[nodiscard]] GraphResult<BufferId> AddBufferUpload(
        RenderGraph &graph, BufferId target, std::span<const std::byte> bytes, std::vector<GraphUpload> &uploads,
        std::uint64_t offset = 0, bool *recorded = nullptr);
    /// @brief 只从同图代的存活计划判断生产者, 不将声明成功当成执行成功.
    [[nodiscard]] bool IsLivePass(const CompiledGraphPlan &plan, PassId pass) noexcept;
    /// @brief 在成功执行的导出中查找精确版本. 返回借用句柄, 调用者按接受回执决定是否持久保存.
    [[nodiscard]] RHI::Texture *FindTextureOutput(const GraphExecutionOutputList &outputs, TextureId id) noexcept;

    /// @brief 与设备准入顺序一致的写入清单, 纹理写在所有 Buffer 写之前.
    struct GraphUploadBudget
    {
        std::vector<std::size_t> textures = std::vector<std::size_t>(0);
        std::vector<std::size_t> buffers = std::vector<std::size_t>(0);
    };
    /// @brief 从实际计划筛选写入. 拒绝 Buffer 之后的纹理写, 避免准入次序与执行次序不同.
    /// @pre uploads 来自本图的上传声明, 每个写 Pass 恰有一个账单项.
    /// @note 结果只供 Open 前的容量预检, 不是 GPU 额度预留或请求完成证明.
    [[nodiscard]] GraphResult<GraphUploadBudget> CollectGraphUploads(
        const CompiledGraphPlan &plan, std::span<const GraphUpload> uploads);
}
