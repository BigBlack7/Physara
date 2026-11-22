#pragma once

/// @file
/// @brief Device 独占的物理包与有界 fence FIFO, recording 身份和公开提交票据分开.

#include "Upload.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "../ImmediateGraphics.hpp"

namespace Physara::RHI::OpenGL
{
    /// @brief 物理包的录制/待提交/已接受阶段, Closed 不等于 GPU 完成, orphaned 是另一条交接事实.
    enum class PacketPhase : std::uint8_t
    {
        Recording,
        Closed,
        Submitted
    };

    /// @brief 区分自动推导, 调用方显式请求和图执行接管. GraphManaged 按物理包身份配对进出.
    enum class UseMode : std::uint8_t
    {
        Automatic,
        Explicit,
        GraphManaged
    };

    /// @brief 当前 GraphManaged 消费阶段的精确物理访问权限, 与包内 tracker 的状态请求独立.
    /// @note 每次提交复制声明, 回调或 clear 结束后清空. 权限表只借用资源, 包内 lease 负责延长对象寿命.
    struct GraphUseBatch
    {
        std::vector<GraphTextureUse> textures = std::vector<GraphTextureUse>(0);
        std::vector<GraphBufferUse> buffers = std::vector<GraphBufferUse>(0);

        /// @brief 丢弃阶段登记, tracker 状态和包内对象引用继续保留.
        void Reset() noexcept
        {
            textures.clear();
            buffers.clear();
        }
    };

    class OpenGLTexture;

    /// @brief 一个确定的 native 消费及其底层范围, 访问域独立于公开状态.
    struct NativeAccess
    {
        OpenGLBuffer *buffer = nullptr;
        OpenGLTexture *texture = nullptr;
        BufferRange bytes;
        TextureSubresourceSet subresources;
        GLbitfield domain = 0;
        bool shaderWrite = false;
        bool replaces = false;
    };

    /// @brief 持有 tracker, 资源 lease 和映射 chunk 的稳定物理包, 寿命跨逻辑列表与 GPU 完成.
    /// @note 无逻辑列表强引用; orphaned 交接后只由 owner 处理, 避免内部寿命环.
    /// Timer 结果与包完成分别推进, 销毁 tracker/lease 前先按 recording 身份收拢关联查询.
    struct UploadWorkPacket
    {
        /// @brief 准备可失败的 tracker/容器; Device 取得唯一 claim 后才发布本包.
        explicit UploadWorkPacket(MessageCallback *callback, std::uint64_t identity, std::size_t minimumChunk);
        CommandListResourceStateTracker tracker;
        const std::uint64_t recordingId;
        const std::size_t minimumChunk;
        std::uint64_t submissionId = 0; ///< 仅 fence 和 flush 成功接受本包时写入, orphan 收拢不发布身份.
        PacketPhase phase = PacketPhase::Recording;
        // worker 最后释放列表时以 release 交接, owner 以 acquire 接收后才可读取/退休包; 交接后 worker 不再访问.
        std::atomic<bool> orphaned{false};
        bool issuedWork = false;
        bool invalid = false;
        UseMode useMode = UseMode::Automatic;
        GraphUseBatch graphUses;
        // 暂存当前候选命令的访问, 下个命令开始时清空. 跨命令的已发历史属于 Buffer/Texture.
        std::vector<NativeAccess> accesses = std::vector<NativeAccess>(0);
        // 两类状态快照均借用对象. 包保活 Pipeline 和启用 trackLiveness 的集合, 其余集合由调用方保活到完成.
        // 区域标记只记录本 recording 的显式输入, 不从跨 recording 的 native 缓存推定已设置.
        GraphicsState graphics;
        bool graphicsSet = false;
        ComputeState compute;
        bool computeSet = false;
        bool viewportSet = false;
        bool scissorSet = false;
        // push 借用本包 chunk 的最近一次设置区段. 切换或重设可执行状态后须重新提供数据.
        UploadSlice push;
        /// @brief 本物理包占用的一槽 volatile backing. 资源 lease 保证退休时仍可归还该槽.
        struct VolatileLease
        {
            OpenGLBuffer *buffer; ///< 借用包已取得的资源 lease.
            std::uint32_t version;
        };
        std::vector<VolatileLease> volatileLeases = std::vector<VolatileLease>(0);
        /// @brief resources 保活 backing. 已发 Read 在包退休时向所选子资源交付结果, Write 只归还 GPU 占用.
        /// @note Prepare 可提前取得表项. 只有 issued 的表项才有资格在退休时修改目标状态.
        struct StagingLease
        {
            StagingAccess *access;
            bool *readReady; ///< Read 目标子资源的可映射性; Write 只需保留存储.
            bool issued = false;
            bool publish = false;
            bool graphUse = false; ///< 最新写入属于当前接管的图, 失败时取消结果发布.
        };
        std::vector<StagingLease> staging = std::vector<StagingLease>(0);
        std::size_t writes = 0;
        std::size_t logicalBytes = 0;
        std::vector<UploadResourceLease> resources;
        /// @brief 观察表借包内资源 lease, 只把 Close 状态和提交事实写给外部 owner.
        struct ResourceObservation
        {
            OpenGLTexture *texture;
            OpenGLBuffer *buffer;
            std::shared_ptr<GraphicsResourceUse> use;
        };
        std::vector<ResourceObservation> observations = std::vector<ResourceObservation>(0);
        std::vector<OpenGLTimerQuery *> timers; ///< 借用 resources 保活的 query, 配对与退休时核对 recording 身份.
        std::vector<std::unique_ptr<UploadChunk>> chunks;
    };

    /// @brief 一个真实 fence 覆盖的物理包组, submissionId=0 用于 orphan 或失败前缀, 不作为公开成功票据.
    struct SubmissionRecord
    {
        GLsync sync = nullptr;
        std::uint64_t submissionId = 0;
        std::uint64_t accessEpoch = 0;
        std::array<UploadWorkPacket *, UploadPacketLimit> packets{};
        std::size_t count = 0;
    };

    /// @brief Device 发布前取得存储的有界 FIFO, 发出 fence 后不再为记录申请 CPU 内存.
    struct SubmissionState
    {
        std::array<SubmissionRecord, UploadPacketLimit> records{};
        std::size_t head = 0;
        std::size_t count = 0;
    };
}
