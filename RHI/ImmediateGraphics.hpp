#pragma once

/// @file
/// @brief 即时 Graphics 列表提供的录制互操作. 图借用现有物理包并接管显式用途, 调用方持有录制和提交职责.

#include <cstdint>
#include <optional>
#include <span>
#include <utility>

#include "RHI.hpp"

namespace Physara::RHI
{
    namespace Validation
    {
        class CommandListWrapper;
    }

    /// @brief 存储访问在图授权与活动 Shader 反射之间传递, 与共享的 RHI 状态分开.
    enum class StorageAccess : std::uint8_t
    {
        None = 0,
        Read = 1,
        Write = 2,
        ReadWrite = 3
    };

    /// @brief 声明方向须包含实际访问的所有位. 普通用途传 None, 其角色仍由资源状态单独核对.
    [[nodiscard]] constexpr bool CoversStorageAccess(StorageAccess available, StorageAccess requested) noexcept
    {
        return (static_cast<unsigned>(available) & static_cast<unsigned>(requested)) == static_cast<unsigned>(requested);
    }

    /// @brief 当前阶段可消费的物理纹理用途, 保留原 mip/layer 和 aspect.
    /// @note range 与 aspect 为已解析的明确范围. state 表示实际消费域. 采样附带的跟踪状态由图单独合并.
    struct GraphTextureUse
    {
        Texture *texture;
        TextureSubresourceSet range;
        TextureAspect aspect;
        ResourceStates state;
        StorageAccess storage = StorageAccess::None;
    };

    /// @brief 当前阶段可消费的物理 Buffer 用途和确切字节区间. 跟踪状态仍按整对象合并.
    /// @note range 是由声明解析出的非空半开字节区间, 按本条用途核对.
    struct GraphBufferUse
    {
        Buffer *buffer;
        BufferRange range;
        ResourceStates state;
        std::optional<BufferRange> indexedBinding; ///< 仅限匹配的结构化 SRV 绑定, 不授权传输或其它额外访问.
        StorageAccess storage = StorageAccess::None;
    };

    /// @brief 底层接管协议. 状态修改和提交只经一次成功接管的 GraphRecording 到达.
    class ImmediateGraphicsRecording
    {
    private:
        friend class GraphRecording;
        friend class Validation::CommandListWrapper;

        /// @brief 在同设备的有效 Graphics 录制上进入 GraphManaged, 撤销原有 Draw 和 Dispatch 候选.
        /// @pre 列表处于 Automatic, 调用方已提交待决状态请求.
        /// @note 拒绝时不修改列表或原生状态.
        /// @return 非零的物理录制身份, 用于结束同一录制.
        [[nodiscard]] virtual std::uint64_t BeginGraphManaged(Device &device) = 0;

        /// @brief 恢复 Automatic, 清除图授权和两类可执行状态. 资源跟踪与已发命令的引用继续有效.
        /// @param executionSucceeded 全部显式用途和边界请求均已成功提交时为 true.
        /// @return 身份匹配并且该录制仍可用时为 true. 对匹配的失效录制仍归还模式并收拢合法 CPU 状态.
        [[nodiscard]] virtual bool EndGraphManaged(std::uint64_t recordingId, bool executionSucceeded) noexcept = 0;

        /// @brief 将已知前态登记到当前 tracker, 供后续状态请求比较.
        virtual void TrackGraphTexture(Texture &texture, TextureSubresourceSet range, ResourceStates state) = 0;
        virtual void TrackGraphBuffer(Buffer &buffer, ResourceStates state) = 0;
        /// @brief 请求当前阶段合并后的物理状态. 消费范围随 CommitGraphUses 单独提交.
        virtual void RequestGraphTexture(Texture &texture, TextureSubresourceSet range, ResourceStates state) = 0;
        virtual void RequestGraphBuffer(Buffer &buffer, ResourceStates state) = 0;
        /// @brief 消费 tracker 依赖并发布本阶段精确用途. 列表保存副本至撤销或替换.
        virtual void CommitGraphUses(std::span<const GraphTextureUse> textures, std::span<const GraphBufferUse> buffers) = 0;
        /// @brief 包装层在图内拒绝候选时撤销 Draw, Dispatch 和 push 资格. 当前录制的动态区域, 授权和已发前缀继续有效.
        virtual void RevokeGraphPipelineState() noexcept = 0;
        /// @brief 同步撤销当前阶段授权, 保留实际跟踪状态和录制包内引用.
        virtual void RevokeGraphUses() noexcept = 0;

    protected:
        ImmediateGraphicsRecording() = default;
        ~ImmediateGraphicsRecording() = default;
        ImmediateGraphicsRecording(const ImmediateGraphicsRecording &) = delete;
        ImmediateGraphicsRecording &operator=(const ImmediateGraphicsRecording &) = delete;
    };

    /// @brief 在同设备 owner 线程同步接管当前录制的状态准备. 执行器持有接管, 回调沿原列表消费.
    /// @pre 列表处于 Automatic, 原状态请求已提交, 列表及设备借用至 Finish 或析构结束.
    /// @note 接管结果由 bool 判断. 同一录制接管期间再次构造只得到无效对象.
    /// 状态操作在接管成功后使用. 已接管的操作失败通过设备诊断返回, 调用方须在进入下一阶段前检查.
    class GraphRecording final
    {
    public:
        GraphRecording(CommandList &commands, Device &device)
            : m_Recording(dynamic_cast<ImmediateGraphicsRecording *>(&commands)),
              m_Id(m_Recording ? m_Recording->BeginGraphManaged(device) : 0)
        {
        }
        GraphRecording(const GraphRecording &) = delete;
        GraphRecording &operator=(const GraphRecording &) = delete;
        ~GraphRecording()
        {
            if (m_Id != 0)
            {
                static_cast<void>(Finish(false));
            }
        }

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return m_Id != 0;
        }

        /// @brief 按物理身份归还录制. 仍有效时返回 true, 已发工作保留在原包中.
        [[nodiscard]] bool Finish(bool executionSucceeded) noexcept
        {
            return m_Id != 0 && m_Recording->EndGraphManaged(std::exchange(m_Id, 0), executionSucceeded);
        }

        /// @brief 为选定纹理范围或整个 Buffer 登记已知前态, 不产生状态转换.
        void BeginTracking(Texture &texture, TextureSubresourceSet range, ResourceStates state)
        {
            if (m_Id != 0)
            {
                m_Recording->TrackGraphTexture(texture, range, state);
            }
        }
        void BeginTracking(Buffer &buffer, ResourceStates state)
        {
            if (m_Id != 0)
            {
                m_Recording->TrackGraphBuffer(buffer, state);
            }
        }
        /// @brief 为同一阶段请求已合并的状态. 准备完成后调用 CommitUses.
        void RequestState(Texture &texture, TextureSubresourceSet range, ResourceStates state)
        {
            if (m_Id != 0)
            {
                m_Recording->RequestGraphTexture(texture, range, state);
            }
        }
        void RequestState(Buffer &buffer, ResourceStates state)
        {
            if (m_Id != 0)
            {
                m_Recording->RequestGraphBuffer(buffer, state);
            }
        }
        /// @brief 提交待决状态并发布精确消费权限. 输入短借至返回, 空范围表用于无回调消费的图尾阶段.
        void CommitUses(std::span<const GraphTextureUse> textures, std::span<const GraphBufferUse> buffers)
        {
            if (m_Id != 0)
            {
                m_Recording->CommitGraphUses(textures, buffers);
            }
        }
        /// @brief 结束本次消费授权, 保留物理状态与已发工作的引用.
        void RevokeUses() noexcept
        {
            if (m_Id != 0)
            {
                m_Recording->RevokeGraphUses();
            }
        }

    private:
        ImmediateGraphicsRecording *m_Recording;
        std::uint64_t m_Id;
    };
}
