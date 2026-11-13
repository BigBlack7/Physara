#pragma once

/// @file
/// @brief 定义构图阶段与后续编译共用的值类型.
/// 资源记录保存描述和导入事实, Pass 记录保存逐版本的用途, 写入效果及前一版本.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <RHI.hpp>

#include "Core/Error.hpp"
#include "Core/Identity.hpp"

namespace Physara::Renderer
{
    struct GraphTextureTag;
    struct GraphBufferTag;
    struct GraphPassTag;

    /// @brief 某一图代中指定类型资源的逻辑版本. 同一 identity 的版本共享底层资源身份.
    /// @note 版本 0 表示创建或导入时的内容. 该值只在所属图内使用.
    template <typename Tag>
    struct GraphId
    {
        Core::Identity<Tag> identity;
        std::uint32_t version = 0;

        /// @brief 仅区分空值. 所属图, 索引和已发布版本由图验证.
        [[nodiscard]] constexpr bool IsValid() const noexcept { return identity.IsValid(); }
        bool operator==(const GraphId &) const = default;
    };

    using TextureId = GraphId<GraphTextureTag>;
    using BufferId = GraphId<GraphBufferTag>;
    /// @brief 由图代和 Pass 声明索引组成, 供生产者记录和回调入口使用.
    using PassId = Core::Identity<GraphPassTag>;

    /// @brief 纹理范围的 aspect 位集. 深度和模板可单独选取, 颜色只用于颜色格式.
    enum class TextureAspect : std::uint8_t
    {
        Color = 1,
        Depth = 2,
        Stencil = 4,
        DepthStencil = 6
    };

    /// @brief mip 和 layer 使用起点与正计数, aspect 必须来自实际格式.
    /// @note 整个允许范围以省略参数表达. 显式范围不裁剪, 3D 纹理的深度不作为数组层.
    struct TextureRange
    {
        std::uint32_t firstMip = 0;
        std::uint32_t mipCount = 1;
        std::uint32_t firstLayer = 0;
        std::uint32_t layerCount = 1;
        TextureAspect aspect = TextureAspect::Color;

        bool operator==(const TextureRange &) const = default;
    };

    /// @brief Graph 的访问用途, 同时作为 Pass 查询的匹配键.
    /// 写版本和内容读写由对应的 Use 记录表达.
    enum class GraphUsage : std::uint8_t
    {
        Sampled,
        ShaderRead,
        VertexBuffer,
        IndexBuffer,
        ConstantBuffer,
        CopySource,
        CopyDestination,
        ColorAttachment,
        DepthStencilAttachment,
        DepthStencilRead
    };

    /// @brief 写入前对范围内内容的处理. Load 保留并依赖旧内容, Clear 初始化, Discard 放弃旧值.
    enum class LoadOp : std::uint8_t
    {
        Load,
        Clear,
        Discard
    };

    /// @brief 写入后的保留策略. Store 保留产生的结果, DontCare 使所选范围的内容无效.
    enum class StoreOp : std::uint8_t
    {
        Store,
        DontCare
    };

    /// @brief Pass 对声明范围的覆盖承诺. Full 覆盖该范围的全部内容, Partial 可留下未写区域.
    enum class WriteCoverage : std::uint8_t
    {
        Partial,
        Full
    };

    /// @brief 深度与模板清除值. 只有声明选中的字段会被消费, 深度为 [0, 1] 的无量纲值.
    struct DepthStencilClear
    {
        float depth = 0.f;
        std::uint8_t stencil = 0;
    };

    /// @brief 声明所选纹理范围的写入. 对于局部复制, Load 表示保留范围内未改写的内容.
    /// @note 附件和传输 Clear 都要求 isRenderTarget, 清除值须匹配格式与所选 aspect.
    /// Load 和 Discard 使用 monostate.
    /// Full 由 Pass 保证. Store 保留已有写入效果, 本身不初始化内容.
    struct TextureWriteOps
    {
        LoadOp load = LoadOp::Discard;
        StoreOp store = StoreOp::Store;
        WriteCoverage coverage = WriteCoverage::Partial;
        std::variant<std::monostate, RHI::Color, std::uint32_t, DepthStencilClear> clearValue;
    };

    /// @brief 外部 owner 提供允许范围的进出用途状态与初始内容事实.
    /// @note 导入方须填写 entry 和 final. 两者都须为公共 RHI 定义的非零状态位组合.
    /// contentDefined 单独声明整个导入范围的初始内容有效性.
    struct ImportState
    {
        RHI::ResourceStates entry = RHI::ResourceStates::Unknown;
        RHI::ResourceStates final = RHI::ResourceStates::Unknown;
        bool contentDefined = false;

        bool operator==(const ImportState &) const = default;
    };

    /// @brief 图外消费者的用途. Presentation 用于导入的颜色目标.
    enum class ExportUsage : std::uint8_t
    {
        ShaderRead,
        Readback,
        Presentation
    };

    /// @brief 一次访问的精确版本, 已校验范围与用途. 查询解析器也接收同一结构.
    template <typename Id, typename Range>
    struct ResourceAccess
    {
        Id resource;
        Range range;
        GraphUsage usage;
    };

    using TextureAccess = ResourceAccess<TextureId, TextureRange>;
    using BufferAccess = ResourceAccess<BufferId, RHI::BufferRange>;

    /// @brief 纹理写入的前一版本和内容效果. 所选范围由关联的 access 提供.
    struct TextureWrite
    {
        TextureId previous;
        TextureWriteOps ops;
    };

    /// @brief 缓冲写入的前一版本. 声明字节区间以外的内容沿此版本继承.
    struct BufferWrite
    {
        BufferId previous;
    };

    /// @brief 无 write 表示读. 写的 access 指向新版本, Load 通过 previous 引用旧内容.
    /// 纹理的写范围以外也继承前一版本. 旧内容是否被此写读取由 ops 决定.
    /// Buffer 写入完整定义它声明的字节区间, 区间外继承旧内容.
    template <typename Access, typename Write>
    struct ResourceUse
    {
        using AccessType = Access;
        Access access;
        std::optional<Write> write;
    };

    using TextureUse = ResourceUse<TextureAccess, TextureWrite>;
    using BufferUse = ResourceUse<BufferAccess, BufferWrite>;
    using PassUse = std::variant<TextureUse, BufferUse>;

    /// @brief 明确外部消费的版本和范围, 供编译器确定输出根.
    template <typename Id, typename Range>
    struct ResourceExport
    {
        Id resource;
        Range range;
        ExportUsage usage;
    };

    using TextureExport = ResourceExport<TextureId, TextureRange>;
    using BufferExport = ResourceExport<BufferId, RHI::BufferRange>;
    using GraphExport = std::variant<TextureExport, BufferExport>;

    /// @brief 由 PassId 和该 Pass 的用途索引定位写入. 版本 0 没有图内生产者.
    struct VersionProducer
    {
        PassId pass;
        std::size_t useIndex = 0;
    };

    /// @brief 图持有描述, 允许范围和外部强引用. 创建的逻辑资源不带导入引用或状态.
    /// @invariant writes[v - 1] 指向已发布版本 v 的生产者, writes.size() 为当前写版本.
    template <typename Desc, typename Range, typename Handle>
    struct ResourceDeclaration
    {
        using Description = Desc;
        using AccessRange = Range;
        using ResourceHandle = Handle;

        ResourceDeclaration(const Desc &description, Range allowed, const Handle &handle,
                            std::optional<ImportState> state)
            : desc(description), range(allowed), imported(handle), importState(state)
        {
        }

        Desc desc;
        Range range;
        Handle imported;
        std::optional<ImportState> importState;
        std::vector<VersionProducer> writes = std::vector<VersionProducer>(0);
    };

    using TextureDeclaration = ResourceDeclaration<RHI::TextureDesc, TextureRange, RHI::TextureHandle>;
    using BufferDeclaration = ResourceDeclaration<RHI::BufferDesc, RHI::BufferRange, RHI::BufferHandle>;

    /// @brief 供后续编译读取的已发布 Pass. 名称仅用于诊断, uses 按 setup 的声明顺序保存.
    /// 图另行保存延迟回调与拥有型数据, 使用者通过 PassId 进入回调.
    struct PassDeclaration
    {
        PassDeclaration(PassId pass, std::string_view label) : id(pass), name(label)
        {
        }

        PassId id;
        std::string name;
        std::vector<PassUse> uses = std::vector<PassUse>(0);
    };

    /// @brief 区分声明阶段的合同拒绝, 存储失败和回调中的权限或解析失败.
    enum class GraphErrorCode : std::uint8_t
    {
        InvalidPhase,
        InvalidResource,
        InvalidDescription,
        InvalidRange,
        ConflictingImport,
        InvalidUsage,
        StaleWrite,
        UndefinedContent,
        OutOfStorage,
        SetupFailed,
        InvalidPass,
        AccessDenied,
        UnresolvedResource
    };

    /// @brief 错误正文独立持有阶段及已知对象信息, 可在图销毁后报告.
    struct GraphError
    {
        GraphErrorCode code;
        Core::BoundedText description;
    };

    /// @brief 成功时返回声明身份等值, 失败时返回诊断.
    template <typename T>
    using GraphResult = std::variant<T, GraphError>;

    /// @brief 无业务值的操作结果. 空值表示成功.
    using GraphFailure = std::optional<GraphError>;
}
