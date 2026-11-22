#pragma once

/// @file
/// @brief 即时 Graphics 列表提供的录制互操作. 图借用现有物理包并接管显式用途, 调用方持有录制和提交职责.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

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

    /// @brief 同一资源在一个 Graphics 包内的 Close 状态及接受/退休事实, 不持有资源或 Device.
    /// @note 只在录制 owner 线程读取. states 按 layer 后 mip 排列, Buffer 只有一个单元.
    /// ticket 只由真实接受写入. complete 由已接受包的 fence 完成置位, unused 由未发工作的弃包退休置位.
    /// states 描述物理访问状态, 对应逻辑版本的内容覆盖由图所有者维护.
    struct GraphicsResourceUse
    {
        /// @brief 按物理单元构造可抛出的状态数组, 使 Debug 迭代器代理分配也走录制错误出口.
        explicit GraphicsResourceUse(std::size_t cells) : states(cells) {}

        /// @brief 查询 Graphics 包全部退休后, 池自身引用之外是否仍有强引用.
        using ExternalOwnerCheck = bool (*)(Resource &resource, unsigned long poolHolds) noexcept;

        std::vector<ResourceStates> states;
        ExternalOwnerCheck hasExternalOwners{}; ///< 只以观察的同一活对象在其 owner 线程查询.
        std::uint64_t recording{};
        std::uint64_t ticket{};
        bool complete{};
        bool unused{};
    };

    /// @brief 底层接管协议. 状态修改和提交只经一次成功接管的 GraphRecording 到达.
    class ImmediateGraphicsRecording
    {
    public:
        /// @brief 在当前物理包登记可跨 Close/提交观察的资源. 同包同对象共享观察结果.
        /// @param previous 上次已接受的同资源状态, 只用于保留新包未追踪的单元.
        /// @return 只读观察随包推进. 设备或录制不匹配, 无法登记或后端未提供观察时返回空.
        [[nodiscard]] virtual std::shared_ptr<const GraphicsResourceUse> ObserveResource(
            Device &, Resource &, const GraphicsResourceUse *previous)
        {
            static_cast<void>(previous);
            return {};
        }

        /// @brief 从 range 的基础 mip 逐级过滤选中的层或 cube 面. 单级范围成功时没有写入.
        /// @note 使用 Automatic 或已提交用途的 Explicit 录制. 基础级需 CopySource, 后续级需 CopyDest.
        [[nodiscard]] virtual bool GenerateMipmaps(Texture *texture, TextureSubresourceSet range) = 0;

    private:
        friend class GraphRecording;
        friend class Validation::CommandListWrapper;

        /// @brief 在同设备的有效 Graphics 录制上进入 GraphManaged, 撤销原有 Draw 和 Dispatch 候选.
        /// @pre 列表处于 Automatic, 调用方已提交待决状态请求.
        /// @note 拒绝时不修改列表或原生状态.
        /// @return 非零的物理录制身份, 用于结束同一录制.
        [[nodiscard]] virtual std::uint64_t BeginGraphManaged(Device &device) = 0;

        /// @brief 恢复 Automatic, 清除图授权和两类可执行状态. 资源跟踪与已发命令的引用继续有效.
        /// @param executionSucceeded 图的 Pass 和图尾用途均已成功录制时为 true.
        /// @return 原录制已归还且仍可用时为 true. 身份匹配的失效录制也会清除图授权并归还模式.
        /// @note 图执行和归还均成功后, 读回仍须等所属包正式提交并退休才可映射.
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

    /// @brief 在已打开的即时 Graphics 录制中登记本资源的结束事实. 观察本身不发出 GPU 命令.
    [[nodiscard]] inline std::shared_ptr<const GraphicsResourceUse> ObserveGraphicsResource(
        CommandList &commands, Device &device, Resource &resource, const GraphicsResourceUse *previous = nullptr)
    {
        auto *recording = dynamic_cast<ImmediateGraphicsRecording *>(&commands);
        return recording ? recording->ObserveResource(device, resource, previous) : nullptr;
    }

    /// @brief 在原列表上调用即时颜色传输扩展. 缺少此扩展或本次请求被拒绝时返回 false.
    [[nodiscard]] inline bool GenerateMipmaps(CommandList &commands, Texture *texture, TextureSubresourceSet range)
    {
        auto *recording = dynamic_cast<ImmediateGraphicsRecording *>(&commands);
        return recording && recording->GenerateMipmaps(texture, range);
    }

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
