#pragma once

/// @file
/// @brief 提供帧内图的声明入口, 持有导入和延迟 Pass 数据, 并限制执行期的资源查询权限.
/// Seal 固定编译所需的记录. 回调由执行方按编译顺序调用, 物理资源由执行方解析.

#include "GraphResources.hpp"

#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <span>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

#include "Core/FrameArena.hpp"

namespace Physara::Renderer
{
    class RenderGraph;
    namespace Detail
    {
        class GraphCompiler;
    }

    /// @brief 执行方提供的物理资源查询, 输入为当前 Pass 已授权的版本, 用途和规范范围.
    class PassResourceResolver
    {
    public:
        virtual ~PassResourceResolver() = default;
        /// @return 返回与声明匹配的物理对象, 尚不可用时返回空指针.
        /// @note access 只在本次调用期间有效, 解析方保活对象至 execute 返回.
        [[nodiscard]] virtual RHI::Texture *Resolve(const TextureAccess &access) noexcept = 0;
        [[nodiscard]] virtual RHI::Buffer *Resolve(const BufferAccess &access) noexcept = 0;
    };

    /// @brief 当前 execute 的查询入口. 一条同版本且同用途的声明必须覆盖所请求的完整范围.
    /// @note 查询只在回调内有效. 首次失败后本回调的查询都返回空, InvokePass 返回首次查询错误.
    class PassResources final
    {
    public:
        PassResources(const PassResources &) = delete;
        PassResources &operator=(const PassResources &) = delete;

        /// @brief 查询已授权的纹理或缓冲. 省略 range 表示资源的整个图内允许范围.
        /// @return 借用的 RHI 对象, 身份, 范围, 权限或解析失败时返回空指针.
        /// @note 回调对返回对象的使用须遵守本次查询的用途和范围.
        [[nodiscard]] RHI::Texture *Resolve(TextureId id, GraphUsage usage,
                                            std::optional<TextureRange> range = std::nullopt) noexcept;
        [[nodiscard]] RHI::Buffer *Resolve(BufferId id, GraphUsage usage,
                                           std::optional<RHI::BufferRange> range = std::nullopt) noexcept;

    private:
        friend class RenderGraph;
        PassResources(const RenderGraph &graph, const PassDeclaration &pass, PassResourceResolver &resolver) noexcept
            : m_Graph(graph), m_Pass(pass), m_Resolver(resolver)
        {
        }

        template <typename Id, typename Range, typename Declaration, typename Use, typename ResolveFn>
        auto ResolveDeclared(Id id, GraphUsage usage, const std::optional<Range> &range,
                             const std::deque<Declaration> &resources, ResolveFn resolve) noexcept;

        const RenderGraph &m_Graph;
        const PassDeclaration &m_Pass;
        PassResourceResolver &m_Resolver;
        GraphFailure m_Failure;
    };

    /// @brief 同步 setup 借用的声明入口, 暂存读写和导出直至 Pass 整体成功.
    /// @note 合同拒绝时返回 false 或空身份并保留首个诊断. Builder 只在该次 setup 内有效.
    /// 容器存储异常由 AddPass 处理. 省略范围时使用整个允许范围, 显式范围须非空且完整位于其中.
    class PassBuilder final
    {
    public:
        PassBuilder(const PassBuilder &) = delete;
        PassBuilder &operator=(const PassBuilder &) = delete;

        /// @brief 核对用途与内容后登记指定版本的读取.
        /// @note 纹理复制及普通阶段的联合用途由编译器核对, 包括 packed depth/stencil 的传输范围.
        [[nodiscard]] bool Read(TextureId id, GraphUsage usage, std::optional<TextureRange> range = std::nullopt);
        [[nodiscard]] bool Read(BufferId id, GraphUsage usage, std::optional<RHI::BufferRange> range = std::nullopt);

        /// @brief 将本 Pass 已声明的同版本, 同范围 CopySource 读取标记为图内读回.
        /// @note 省略范围时选择整个允许范围. 重复标记共用索引, 无匹配读取时拒绝候选 Pass.
        /// 标记随 Pass 发布, 由编译器作为活性根消费. 请求与结果的寿命由读回消费方管理.
        [[nodiscard]] bool MarkReadback(TextureId id, std::optional<TextureRange> range = std::nullopt);
        [[nodiscard]] bool MarkReadback(BufferId id, std::optional<RHI::BufferRange> range = std::nullopt);

        /// @brief 从当前版本写入, 返回本次 setup 内可引用的新版本. 纹理写的内容由 ops 说明.
        /// @note 候选版本随 Pass 一起发布, AddPass 失败后不得使用候选身份.
        /// @note Buffer 的 CopyDestination 声明定义完整字节区间. 区间外仍来自前一版本.
        [[nodiscard]] TextureId Write(TextureId id, GraphUsage usage, const TextureWriteOps &ops,
                                      std::optional<TextureRange> range = std::nullopt);
        [[nodiscard]] BufferId Write(BufferId id, GraphUsage usage,
                                     std::optional<RHI::BufferRange> range = std::nullopt);

        /// @brief 核对外部用途与内容后, 将指定版本和范围登记为外部消费根.
        /// @note 导出随本 Pass 一起发布, 回调查询权限来自 Read 和 Write.
        [[nodiscard]] bool Export(TextureId id, ExportUsage usage, std::optional<TextureRange> range = std::nullopt);
        [[nodiscard]] bool Export(BufferId id, ExportUsage usage, std::optional<RHI::BufferRange> range = std::nullopt);

    private:
        friend class RenderGraph;
        PassBuilder(const RenderGraph &graph, PassId id, std::string_view name);

        template <typename Id, typename Range, typename Declaration, typename Use>
        [[nodiscard]] std::optional<Range> Validate(Id id, const std::optional<Range> &requested,
                                                     const std::deque<Declaration> &resources);
        template <typename Id, typename Declaration, typename Use>
        [[nodiscard]] std::size_t LatestVersion(Id id, const Declaration &resource) const noexcept;
        template <typename Use, typename Id, typename Range, typename Declaration>
        [[nodiscard]] bool RequireContent(Id id, const Range &range, const Declaration &resource);
        template <typename Use, typename Id, typename Range, typename Declaration>
        [[nodiscard]] bool MarkReadbackUse(Id id, const std::optional<Range> &requested,
                                           const std::deque<Declaration> &resources);
        bool Fail(GraphErrorCode code, std::string_view name, std::uint32_t version, std::string_view reason) noexcept;

        const RenderGraph &m_Graph;
        PassId m_Id;
        std::string_view m_Name;
        std::vector<PassUse> m_Uses = std::vector<PassUse>(0);
        std::vector<GraphExport> m_Exports = std::vector<GraphExport>(0);
        std::vector<std::size_t> m_ReadbackUses = std::vector<std::size_t>(0);
        GraphFailure m_Failure;
    };

    /// @brief 帧内声明图. 图持有资源描述, 导入引用和延迟 Pass 对象, 只读记录供后续编译使用.
    /// @pre 调用方在创建前预留专用 arena, 并在图销毁后再复用. 图独占从构造游标起的尾部.
    /// @note 图操作由同一调用方串行调度, 包括 const 的 InvokePass. setup 和 execute 均同步完成.
    class RenderGraph final
    {
    public:
        /// @brief 取得一张独立的图代并记住专用 arena 的初始游标.
        /// @throws std::bad_alloc 无法建立声明表时.
        explicit RenderGraph(Core::FrameArena &arena);
        /// @brief 逆序销毁拥有型 Pass 对象, 再将 arena 游标退回图构造时的位置.
        ~RenderGraph();
        RenderGraph(const RenderGraph &) = delete;
        RenderGraph &operator=(const RenderGraph &) = delete;

        /// @brief 创建无内容的逻辑资源. 成功返回版本 0, 失败时图不新增身份.
        [[nodiscard]] GraphResult<TextureId> CreateTexture(const RHI::TextureDesc &desc);
        [[nodiscard]] GraphResult<BufferId> CreateBuffer(const RHI::BufferDesc &desc);

        /// @brief 持有已存在的 RHI 对象. 同一对象只接受等价的规范范围, 状态和内容声明.
        /// @pre 调用方传入外部 owner 使用的规范 RHI 对象.
        /// @note 省略 range 导入整个资源. 等价导入始终返回版本 0, 冲突时保留原记录.
        [[nodiscard]] GraphResult<TextureId> ImportTexture(const RHI::TextureHandle &texture, ImportState states,
                                                            std::optional<TextureRange> range = std::nullopt);
        [[nodiscard]] GraphResult<BufferId> ImportBuffer(const RHI::BufferHandle &buffer, ImportState states,
                                                          std::optional<RHI::BufferRange> range = std::nullopt);

        /// @brief 标记外部读用途. 省略 range 表示资源的整个图内允许范围.
        /// @return 成功时为空. 拒绝时保留此前的导出记录.
        [[nodiscard]] GraphFailure Export(TextureId id, ExportUsage usage,
                                          std::optional<TextureRange> range = std::nullopt);
        [[nodiscard]] GraphFailure Export(BufferId id, ExportUsage usage,
                                          std::optional<RHI::BufferRange> range = std::nullopt);

        /// @brief 在专用 arena 构造 Data 后同步运行 setup, 成功才发布声明与延迟回调.
        /// @tparam Data 可默认初始化且析构不抛异常, 对齐不超过 FrameArena 的能力.
        /// @param name 诊断名称, 字符在本次调用期间有效, 成功后由图保存副本.
        /// @param setup 返回 true 表示接受候选 Pass. setup 取得的 Builder 和 Data 引用限于本次调用.
        /// @param execute 为无捕获的函数入口. Data 必须拥有延迟使用的值或引用寿命足够的帧存储.
        /// @note 构图期候选失败后放弃该图, 已发布记录可用于诊断. 回调不得重入声明或消费操作.
        template <typename Data, typename Setup>
        [[nodiscard]] GraphResult<PassId> AddPass(std::string_view name, Setup &&setup,
                                                 GraphFailure (*execute)(PassResources &, const Data &) noexcept)
        {
            static_assert(std::is_nothrow_destructible_v<Data>);
            static_assert(alignof(StoredPass<Data>) <= Core::FrameArena::MaxAlignment);

            const auto stage = std::string_view("AddPass");
            if (auto failure = CheckBuilding(stage, name))
            {
                return *failure;
            }
            if (m_Passes.size() == MaxIndex || !execute)
            {
                m_Poisoned = true;
                const auto code = execute ? GraphErrorCode::OutOfStorage : GraphErrorCode::InvalidUsage;
                const auto reason = execute ? "Pass index space exhausted" : "Missing execute callback";
                return Detail::MakeGraphError(code, stage, name, {}, reason);
            }

            // 失败时先析构候选, 再撤回它占用的 arena 尾部. 已发布 Pass 的地址保持稳定.
            const std::size_t start = m_Arena.Used();
            const auto memory = m_Arena.Allocate(1, sizeof(StoredPass<Data>), alignof(StoredPass<Data>));
            if (memory.empty())
            {
                m_Poisoned = true;
                return Detail::MakeGraphError(GraphErrorCode::OutOfStorage, stage, name, {}, "Pass arena exhausted");
            }

            Activity activity(m_Active);
            struct PendingStorage
            {
                Core::FrameArena &arena;
                std::size_t start;
                bool published = false;

                ~PendingStorage()
                {
                    if (!published)
                    {
                        arena.Rewind(start);
                    }
                }
            } storage{m_Arena, start};

            try
            {
                // 临时 owner 管理已构造的对象, 先于存储回退析构. setup 只积累候选声明.
                std::unique_ptr<StoredPass<Data>, decltype(&DestroyPass<Data>)> value(
                    ::new (static_cast<void *>(memory.data())) StoredPass<Data>{.execute = execute},
                    &DestroyPass<Data>);
                const PassId id{static_cast<std::uint32_t>(m_Passes.size()), m_Generation};
                PassBuilder builder(*this, id, name);
                const bool accepted = std::invoke(std::forward<Setup>(setup), builder, value->data);
                if (builder.m_Failure)
                {
                    m_Poisoned = true;
                    return *builder.m_Failure;
                }
                if (!accepted)
                {
                    m_Poisoned = true;
                    return Detail::MakeGraphError(GraphErrorCode::SetupFailed, stage, name, {},
                                                  "Setup rejected this Pass");
                }

                // 发布声明成功后, 对象的析构责任随回调表转给图.
                PublishPass(builder, {value.get(), &DestroyPass<Data>, &InvokeStoredPass<Data>});
                storage.published = true;
                static_cast<void>(value.release());
                return id;
            }
            catch (const std::bad_alloc &)
            {
                m_Poisoned = true;
                return Detail::MakeGraphError(GraphErrorCode::OutOfStorage, stage, name, {},
                                              "Could not store Pass declarations");
            }
            catch (const std::length_error &)
            {
                m_Poisoned = true;
                return Detail::MakeGraphError(GraphErrorCode::OutOfStorage, stage, name, {},
                                              "Pass declaration table is full");
            }
            catch (...)
            {
                m_Poisoned = true;
                return Detail::MakeGraphError(GraphErrorCode::SetupFailed, stage, name, {},
                                              "Setup or Pass data construction threw");
            }
        }

        /// @brief 结束声明, 固定供编译和回调消费的记录.
        /// @return 封存成功时为空. 已拒绝候选 Pass 的图无法封存, 原记录仍可读取.
        /// @note 封存只切换阶段. 用途组合, 依赖和存储预算由 CompileRenderGraph 检查.
        [[nodiscard]] GraphFailure Seal() noexcept;

        /// @brief 在已封存且空闲的图上同步调用指定 Pass.
        /// @note 执行方负责按计划选择 Pass 并准备物理资源. 此入口检查图代与阶段, 并限制本次查询权限.
        /// @return 返回首个查询错误或 execute 自身的结果. 失败后调用方负责停止后续消费.
        [[nodiscard]] GraphFailure InvokePass(PassId id, PassResourceResolver &resolver) const noexcept;

        /// @brief 借用图内记录. 用途, 版本索引和导出在构建期间可能失效, Seal 后稳定至图销毁.
        [[nodiscard]] const std::deque<TextureDeclaration> &Textures() const noexcept { return m_Textures; }
        [[nodiscard]] const std::deque<BufferDeclaration> &Buffers() const noexcept { return m_Buffers; }
        [[nodiscard]] const std::deque<PassDeclaration> &Passes() const noexcept { return m_Passes; }
        [[nodiscard]] std::span<const GraphExport> Exports() const noexcept { return m_Exports; }

    private:
        friend class PassBuilder;
        friend class PassResources;
        friend class Detail::GraphCompiler;

        /// @brief 在 setup 和 execute 期间阻止重入, 返回时恢复调用边界.
        struct Activity
        {
            explicit Activity(bool &active) noexcept : flag(active) { flag = true; }
            ~Activity() { flag = false; }
            bool &flag;
        };

        /// @brief 将 arena 中的具体 Pass 类型与图的统一回调表连接. 每项随同索引的 Pass 发布.
        struct PassCallback
        {
            void *data;
            void (*destroy)(void *) noexcept;
            GraphFailure (*invoke)(void *, PassResources &) noexcept;
        };

        /// @brief 以稳定地址存放拥有型数据和类型化执行入口.
        template <typename Data>
        struct StoredPass
        {
            Data data{};
            GraphFailure (*execute)(PassResources &, const Data &) noexcept;
        };

        template <typename Data>
        static void DestroyPass(void *value) noexcept
        {
            std::destroy_at(static_cast<StoredPass<Data> *>(value));
        }

        template <typename Data>
        [[nodiscard]] static GraphFailure InvokeStoredPass(void *value, PassResources &resources) noexcept
        {
            const auto &stored = *static_cast<const StoredPass<Data> *>(value);
            return stored.execute(resources, stored.data);
        }

        [[nodiscard]] GraphFailure CheckBuilding(std::string_view stage, std::string_view name) const noexcept;
        void PublishPass(PassBuilder &builder, PassCallback callback);

        template <typename Id, typename Range>
        [[nodiscard]] GraphFailure ExportResource(Id id, ExportUsage usage, std::optional<Range> range);
        template <typename Id, typename Declaration>
        [[nodiscard]] GraphResult<Id> DeclareResource(std::string_view stage, std::deque<Declaration> &resources,
                                                      const typename Declaration::Description &desc,
                                                      std::optional<typename Declaration::AccessRange> requested,
                                                      const typename Declaration::ResourceHandle &imported,
                                                      std::optional<ImportState> state);

        static constexpr auto MaxIndex = (std::numeric_limits<std::uint32_t>::max)();

        Core::FrameArena &m_Arena;
        std::size_t m_ArenaStart;
        std::uint64_t m_Generation;
        std::deque<TextureDeclaration> m_Textures;
        std::deque<BufferDeclaration> m_Buffers;
        std::deque<PassDeclaration> m_Passes;
        std::vector<PassCallback> m_Callbacks = std::vector<PassCallback>(0);
        std::vector<GraphExport> m_Exports = std::vector<GraphExport>(0);
        mutable bool m_Active = false;
        bool m_Sealed = false;
        bool m_Poisoned = false;
    };
}
