#pragma once

/// @file
/// @brief Forward 的设备缓存、拥有型写区段与可绘记录. 类型仅在 Renderer 实现内共享.

#include <algorithm>
#include <array>
#include <bitset>
#include <map>
#include <tuple>
#include <utility>

#include <Shader/RhiProgram.hpp>
#include <StaticForward/Data.hpp>

#include "../Paths/Forward.hpp"
#include "../Lighting/EnvironmentCache.hpp"
#include "../Lighting/FroxelGpu.hpp"
#include "../PostProcess/Display.hpp"

namespace Physara::Renderer::Detail
{
    namespace StaticAbi = Shader::Generated::StaticForward;

    inline constexpr std::size_t ForwardUploadBudget = 64 * 1024 * 1024;
    inline constexpr std::size_t ForwardUploadItems = 4096;
    inline constexpr std::uint8_t VertexUv0 = 1, VertexUv1 = 2, VertexTangent = 4;
    inline constexpr std::size_t NormalSlot = static_cast<std::size_t>(Asset::MaterialSlot::Normal);
    inline constexpr auto WorkingFormat = RHI::Format::RGBA16_FLOAT;
    inline constexpr auto DepthFormat = RHI::Format::D32;
    inline constexpr auto DisplayFormat = RHI::Format::SRGBA8_UNORM;
    inline constexpr auto BufferBoundary = RHI::ResourceStates::Common;
    // 新建纹理首次录制的真实初态为 Common. 之后也交还 Common, 使失败前缀与正常重用入口一致.
    inline constexpr auto TextureBoundary = RHI::ResourceStates::Common;

    using AssetVersionKey = std::array<std::uint64_t, 3>; ///< 领域槽、分配代、该资产内容版本.
    /// @brief 资源键只由发布身份组成, 不使用路径、进程内地址或绘制顺序.
    [[nodiscard]] inline AssetVersionKey Version(Asset::AssetId id, Asset::ContentRevision revision) noexcept
    {
        return {id.index, id.generation, revision.value};
    }
    /// @brief 模型版本, 紧凑 mesh, 源 Primitive 槽, 属性位和重建切线的 UV. UV=-1 表示沿用源几何.
    using GeometryKey = std::tuple<AssetVersionKey, std::uint32_t, std::uint32_t, std::uint8_t, std::int8_t>;
    /// @brief 外置标志, 固定资产版本, 源 image 下标, 颜色域及完整 mip 策略, sampler 另行共享.
    using TextureKey = std::tuple<bool, AssetVersionKey, std::uint32_t, Asset::ImageDomain, bool>;
    using ObjectKey = std::array<std::uint64_t, 4>; ///< Scene 与对象各自的槽和分配代.
    /// @brief 以对象身份和固定图元的局部基点定位对象行. 命中后核对实际变换代.
    using ObjectRowKey = std::pair<ObjectKey, std::array<float, 3>>;
    /// @brief 按消费者区分固定布局内绑定真实图像的位置. 主材质行可逐步补齐所需的连续槽.
    enum class MaterialUse : std::uint8_t { Main, Coverage, OpaqueAuxiliary };
    /// @brief 模型版本, 源材质, 覆盖对象, 源 Primitive 槽及消费用途. 无实例覆盖时对象与槽取零.
    /// 独立 OPAQUE 辅助表面的键保留基色占位, 供颜色与表面请求各自复用正确的绑定.
    using MaterialKey = std::tuple<AssetVersionKey, std::uint32_t, ObjectKey, std::uint32_t, MaterialUse>;
    using SamplerKey = std::array<std::uint32_t, 4>; ///< min、mag、wrapS、wrapT, 已填充缺省值.

    /// @brief 记账附着于资源实际寿命, 被替换而仍由槽保活的代继续占用额度.
    struct ResidencyCapacity
    {
        std::size_t bytes = 0;
        std::size_t entries = 0;
        std::size_t textureBytes = 0;
        std::size_t environmentBytes = 0;
    };

    /// @brief 尚未分配的设备驻留需求. 字节数是物理容量, 条目数按 Allocation 对象计.
    struct ResidencyDemand
    {
        std::size_t bytes = 0;
        std::size_t entries = 0;
    };

    /// @brief 一代存储的逻辑预算与上传事实. accepted 允许后继有序读取, 不表示 GPU 已完成.
    /// @note 由派生类型的 shared_ptr 销毁, 最后一个缓存或帧槽引用释放时归还预算.
    struct Allocation
    {
        Allocation(std::shared_ptr<ResidencyCapacity> quota, std::size_t size, bool paged = false) noexcept;
        ~Allocation();
        Allocation(const Allocation &) = delete;
        Allocation &operator=(const Allocation &) = delete;
        /// @brief 判断任一路是否自本次共享批开始使用了该代.
        [[nodiscard]] bool UsedSince(std::uint64_t preparation) const noexcept;

        std::shared_ptr<ResidencyCapacity> capacity;
        std::size_t bytes;
        std::size_t chargedBytes; ///< 分页资源的物理容量只由页计入设备账目.
        std::array<std::uint64_t, ForwardViewCount> lastUse{}; ///< 各路最近一次准备代, 不是提交票据.
        bool accepted = false;
    };

    /// @brief 兼容用途的物理页及其已出租区间. 空洞可用于新代, 活区间不迁移.
    struct BufferPage
    {
        /// @brief 按起点排序的一笔租用. owner 的上传确认决定本区间能否作为图的初始内容.
        struct Occupied
        {
            RHI::BufferRange range;
            const Allocation *owner; ///< 区间随该代的最后一个强引用退回, 页只借用该地址.
            bool retiring = false;
        };

        BufferPage(std::shared_ptr<ResidencyCapacity> quota, RHI::BufferHandle storage, GraphUsage use,
                   std::uint8_t layout, std::uint64_t ordinal);
        ~BufferPage();
        /// @brief 从首个满足对齐的空洞分配连续字节. 容量不足时返回空值, 原有租用保持有效.
        [[nodiscard]] std::optional<RHI::BufferRange> Claim(const Allocation &owner, std::size_t size,
                                                            std::size_t alignment);
        /// @brief 所属版本的 PageSlice 析构时归还这个区间, 相邻空洞随即可以一起出租.
        void Release(RHI::BufferRange range) noexcept;
        /// @brief 目录替换该代时标为待退休. 原租用一直保留到 Release.
        void Retire(RHI::BufferRange range) noexcept;

        std::shared_ptr<ResidencyCapacity> capacity;
        RHI::BufferHandle buffer;
        GraphUsage usage;
        std::uint8_t layout;
        std::uint64_t ordinal; ///< 页分配顺序, 作为本设备稳定的排序身份.
        bool initialized = false; ///< 正常图已定义整张表页的字节. 各行逻辑内容另由其 Allocation 确认.
        bool initializationRecorded = false; ///< 曾录制冷初始化且无接受回执时, 新代使用独立页.
        std::vector<Occupied> occupied = std::vector<Occupied>(0);
        std::size_t usedBytes = 0;
        std::size_t retiringBytes = 0;
        std::size_t largestGap;
    };

    /// @brief 一个固定版本的页内区间. 由拥有它的几何代或材质行携带到槽安全复用.
    struct PageSlice
    {
        PageSlice() = default;
        ~PageSlice();
        PageSlice(const PageSlice &) = delete;
        PageSlice &operator=(const PageSlice &) = delete;
        void Retire() noexcept;

        std::shared_ptr<BufferPage> page;
        RHI::BufferRange range;
    };

    /// @brief 仅存此布局实际消费的 UV 与切线. 缺席的输入位置绑定到不会被材质读取的字段.
    struct VertexLayout
    {
        std::array<std::uint32_t, 6> offsets{};
        std::uint32_t stride = 0;
    };

    /// @brief 一项固定布局的顶点和索引区间. 索引值保留原图元的局部顶点地址.
    struct GeometryRef final : Allocation
    {
        using Allocation::Allocation;
        /// @brief 目录撤销命中资格时登记两个区间待退休. Draw 强引用仍保活原区间.
        void Retire() noexcept;
        GeometryKey key;
        PageSlice vertices;
        PageSlice indices;
        std::uint32_t indexCount = 0;
    };

    /// @brief 指定颜色解释及 mip 策略的图像代, 层级内容由 UploadBatch 完整生产.
    struct ResidentTexture final : Allocation
    {
        ResidentTexture(std::shared_ptr<ResidencyCapacity> quota, std::size_t bytes, bool environment = false);
        ~ResidentTexture();
        RHI::TextureHandle texture;
        RHI::TextureSubresourceSet sampling; ///< 与初始化上传范围匹配的视图.
        bool environment;
        bool uploadRecorded = false; ///< 材质上传回调触及目标后锁存, 即使图未获接受.
    };

    /// @brief GPU 环境代及其查询所需的小量 CPU 元数据. 像素仍由冷烘焙缓存或本轮上传持有.
    struct ResidentEnvironment
    {
        std::shared_ptr<ResidentTexture> texture;
        std::array<EnvironmentTexel, 9> sh{};
    };

    /// @brief 一个普通 UBO 及其布局匹配的集合. per-view 行只在安全槽内重写.
    struct ConstantRow final : Allocation
    {
        using Allocation::Allocation;
        RHI::BufferHandle buffer;
        RHI::BindingSetHandle binding;
    };

    /// @brief 固定对象版本租用只读对象表中的一行.
    struct ObjectStorage final : Allocation
    {
        using Allocation::Allocation;
        PageSlice storage;
    };

    /// @brief 一行灯字节的源身份和数据代, 独立于当前提取数组中的索引.
    struct LightSource
    {
        Scene::EntityHandle entity;
        std::uint64_t revision;
        bool operator==(const LightSource &) const = default;
    };

    /// @brief 一块定长灯缓冲及其 CPU 复用依据. sources 保存可见灯原序, 字节按灯类型分段.
    /// 两个计数随同槽 View 常量交付. 接受前只可依赖本图的上传版本, 同槽 GPU 完成后才可重写.
    struct LightTable final : Allocation
    {
        using Allocation::Allocation;
        RHI::BufferHandle buffer;
        std::vector<LightSource> sources;
        std::vector<StaticAbi::ForwardLight> packed;
        std::uint32_t directionalCount = 0;
        std::uint32_t localCount = 0;
    };

    /// @brief 只用此对象的变换代核对行, 兼容检查在矩阵编码前完成.
    struct ObjectRow
    {
        std::uint64_t transformRevision;
        std::shared_ptr<ObjectStorage> resident;
    };

    /// @brief 有效材质对几何和光栅状态的要求, 命中时无需重新解析全部纹理槽.
    struct MaterialGeometry
    {
        std::uint8_t attributes{};
        std::int8_t normalUv = -1;
        bool doubleSided{};
        std::uint8_t surfaceAttributes{}; ///< SurfaceAux 使用的 MASK, 粗糙度及法线相关属性.
        std::uint8_t shadowAttributes{}; ///< 写阴影时仅依赖 MASK 实际使用的 UV.
        std::uint8_t specularAttributes{}; ///< 只有扩展受光角色需要的附加 UV.
    };

    /// @brief 保活期间唯一的视图及 sampler 组合. 仅用于查找相等组合, 不是绘制排序值.
    using SamplingKey = std::array<std::uintptr_t, 2 * Scene::MaterialSlotCount>;
    using MaterialBinding = std::shared_ptr<const RHI::BindingSetHandle>;

    /// @brief 可进入不透明绘制的材质 profile. MASK 的覆盖规则独立于路由.
    enum class OpaqueSurfaceRoute : std::uint8_t { CoreDeferred, Unlit, OpaqueForwardExtension };

    /// @brief GPU 材质行与预解析集合. UV 行描述有效材质, sampledSlots 记录本次实际准备到哪个槽.
    /// 未采样的槽复用已准备的句柄. 独立 OPAQUE 辅助行的基色槽也使用占位绑定.
    /// 已冻结的 Draw 持有原行直至其帧槽释放.
    struct MaterialRow final : Allocation
    {
        using Allocation::Allocation;
        std::array<std::shared_ptr<ResidentTexture>, Scene::MaterialSlotCount> textures;
        std::array<RHI::SamplerHandle, Scene::MaterialSlotCount> samplers;
        PageSlice storage;
        SamplingKey sampling;
        MaterialBinding binding;
        StaticAbi::ForwardMaterial values{};
        OpaqueSurfaceRoute route = OpaqueSurfaceRoute::CoreDeferred;
        std::size_t sampledSlots{}; ///< 原生角色所需的有效绑定前缀长度, 含该角色的占位. 命中时核对内容就绪.
    };

    /// @brief 路由和版本只属于当前缓存, 已准备的 Draw 独立保有 GPU 行.
    struct MaterialEntry
    {
        std::uint64_t revision;
        MaterialGeometry geometry;
        std::shared_ptr<MaterialRow> resident;
    };

    /// @brief 当前场景内一行已编码的世界灯, 供选择它的 View 复用.
    struct EncodedLight
    {
        StaticAbi::ForwardLight row;
        std::uint64_t revision;
        const char *error; ///< 编码失败时为静态字段名, 只影响实际选择此灯的 View.
    };

    /// @brief 拥有缓冲和稳定字节, revision 用于接受后确认所属存储代, 不以声明成功确认.
    struct BufferUpload
    {
        std::shared_ptr<Allocation> revision;
        RHI::BufferHandle buffer;
        std::vector<std::byte> payload;
        GraphUploadSource source = GraphUploadSource::ForwardStatic;
        std::uint64_t offset = 0;
        BufferPage *page = nullptr; ///< 非空时由 revision 的区间强引用保活.
        std::optional<std::size_t> view; ///< 仅 View 独占的目标登记所属工作集位置. 共享存储留空.
    };

    /// @brief levels 按 mip 0 起的完整紧密 RGBA8 层级排列, 同一次接受确认整条链.
    struct TextureUpload
    {
        std::shared_ptr<ResidentTexture> revision;
        std::vector<std::vector<std::byte>> levels;
    };

    /// @brief 每项持有一种紧密 CPU 载荷. dfg 按单层上传, environment 由目标 mip 数决定所需的六面.
    /// revision 标记同一目标的全部写入, 完整图接受后方可在后继帧直接读取.
    struct EnvironmentUpload
    {
        std::shared_ptr<ResidentTexture> revision;
        std::shared_ptr<const BakedEnvironment> environment;
        std::shared_ptr<const DfgTable> dfg;
    };

    /// @brief 帧拥有的写入内容. 缓冲按精确字节区间, 纹理按完整 mip 层写入.
    /// @note 各路在准备期间共用 bytes/writes 额度. 显示阶段的矩形常量和 LUT 也计入额度, 字节由 DisplayFrame 保有.
    /// 超限使正在准备的 View 返回失败. 构图后, 本批字节保持稳定直到录制或放弃结束.
    struct UploadBatch
    {
        std::vector<BufferUpload> buffers;
        std::vector<TextureUpload> textures;
        std::vector<EnvironmentUpload> environmentTextures;
        std::vector<std::vector<std::byte>> merged;
        std::vector<std::shared_ptr<const BufferPage>> coldTables; ///< 同一冷页的真实上传只预留一次.
        std::size_t bytes = 0;
        std::size_t writes = 0;
        bool sealed = false;

        /// @brief 在复制载荷前累计逻辑字节和命令数, 超限抛 ForwardFailure; 不预留设备上传池.
        void ReserveWrites(std::size_t byteCount, std::size_t count);
        /// @brief 归还尚未封存的预留, 从未录制过这些写入.
        void ReleaseWrites(std::size_t byteCount, std::size_t count) noexcept;
        /// @brief 撤回本次 View 使用, 只保留还有准备代引用的写入并据此重算批次额度.
        /// @pre 批次未封存. retained 的每项是该路仍有效的准备代, 零表示无候选.
        /// 返回时 bytes/writes 只含保留下来的载荷与冷表页预留. 调用方再补计各路尚未物化的不透明写入预留,
        /// 存活的显示常量及共用的一份 LUT.
        void DiscardView(std::size_t index, std::uint64_t stamp,
                         const std::array<std::uint64_t, ForwardViewCount> &retained) noexcept;
        /// @brief 材质纹理已接受, 或本批持有定义全部可采样层的载荷.
        [[nodiscard]] bool HasMaterialTexture(const ResidentTexture &texture) const noexcept;
        /// @brief 保有目标及所属代, 复制 source 全部字节. 调用返回后不再借用源区段.
        void Add(const std::shared_ptr<Allocation> &revision, const RHI::BufferHandle &buffer,
                 std::span<const std::byte> payload, GraphUploadSource source = GraphUploadSource::ForwardStatic,
                 std::optional<std::size_t> view = std::nullopt);
        /// @brief 复制恰好覆盖 slice 的字节, 保留页内偏移和所属代供构图合并及逐代确认.
        void Add(const std::shared_ptr<Allocation> &revision, const PageSlice &slice,
                 std::span<const std::byte> source);
        /// @brief 结束 CPU 载荷与上传记录的所有权, 不修改资源代的 accepted 状态.
        void Clear() noexcept;
    };

    /// @brief 主表面及常规阴影光栅角色. 每个角色使用自己对应的深度约定.
    enum class SurfacePhase : std::uint8_t { Combined, Depth, ReadOnlyDepth, ShadowDepth };
    /// @brief 按实际资源形状选择颜色变体, 与每轮连续创作参数独立.
    enum class LightingProgram : std::uint8_t
    {
        Legacy, Physical, Ibl, CascadePreview, SpotPreview, PointPreview, LegacyShadows, PhysicalShadows, IblShadows,
        Froxel, FroxelPhysical, FroxelIbl, FroxelShadows, FroxelPhysicalShadows, FroxelIblShadows
    };

    [[nodiscard]] constexpr bool IsShadowPreview(LightingProgram program) noexcept
    {
        return program >= LightingProgram::CascadePreview && program <= LightingProgram::PointPreview;
    }

    [[nodiscard]] constexpr bool UsesFroxels(LightingProgram program) noexcept
    {
        return program >= LightingProgram::Froxel;
    }

    /// @brief 两个候选策略共用 PerPass 的灯、环境与阴影配方, 仅 PerView 布局不同.
    [[nodiscard]] constexpr LightingProgram FullListProgram(LightingProgram program) noexcept
    {
        if (program >= LightingProgram::FroxelShadows)
        {
            return static_cast<LightingProgram>(std::size_t(LightingProgram::LegacyShadows) +
                std::size_t(program) - std::size_t(LightingProgram::FroxelShadows));
        }
        return UsesFroxels(program)
            ? static_cast<LightingProgram>(std::size_t(program) - std::size_t(LightingProgram::Froxel)) : program;
    }

    /// @brief 根据已解析的普通受光角色取得同资源组合的 Froxel 颜色角色.
    /// @pre program 属于全灯 Direct, Physical 或 IBL 及其带阴影版本. 诊断角色单独规划.
    [[nodiscard]] constexpr LightingProgram WithFroxels(LightingProgram program) noexcept
    {
        return program >= LightingProgram::LegacyShadows
            ? static_cast<LightingProgram>(std::size_t(LightingProgram::FroxelShadows) + std::size_t(program) -
                                           std::size_t(LightingProgram::LegacyShadows))
            : static_cast<LightingProgram>(std::size_t(LightingProgram::Froxel) + std::size_t(program));
    }

    /// @brief 屏幕角色与前向 Froxel 角色共享 Direct, DFG, IBL 和对应的阴影配方.
    [[nodiscard]] constexpr LightingProgram DeferredLightingKind(Shader::ProductShader role) noexcept
    {
        return static_cast<LightingProgram>(ForwardPerPassLayouts + DeferredLightingIndex(role));
    }

    /// @brief 将已选好的六种 Froxel 受光组合之一映射到屏幕程序, 每种组合只使用固定状态槽零.
    [[nodiscard]] constexpr ShaderPipelineRequest DeferredLightingRequest(LightingProgram program) noexcept
    {
        const auto role = std::size_t(Shader::ProductShader::DeferredLighting) +
                          std::size_t(program) - ForwardPerPassLayouts;
        return {static_cast<Shader::ProductShader>(role), 0};
    }

    inline constexpr std::uint32_t DeferredViewSlot = 0; ///< 屏幕专用 set 1 b0.
    inline constexpr std::uint32_t GBufferFirstSlot = 2; ///< 逻辑 G0 至 G3 连续映射到 set 1 t2 至 t5.
    inline constexpr std::uint32_t GBufferDepthSlot = 6;

    inline constexpr std::uint32_t DfgSlot = 1; ///< StaticForward 的 PerPass t1.
    inline constexpr std::uint32_t ReflectionSlot = 2; ///< StaticForward 的 PerPass t2.
    inline constexpr std::uint32_t ShadowConstantsSlot = 1; ///< StaticForward 的 PerPass b1.
    inline constexpr std::uint32_t ShadowImageSlot = 3; ///< 本 View 的连续深度层, PerPass t3.
    inline constexpr std::uint32_t ShadowLayersSlot = 4; ///< 组首行和层投影表, PerPass t4.
    inline constexpr std::uint32_t ShadowReferencesSlot = 5; ///< 按灯行索引的组引用表, PerPass t5.
    inline constexpr std::size_t CoreMaterialTextureCount = StaticAbi::SpecularTextureIndex;
    static_assert(CoreMaterialTextureCount == std::size_t(Asset::MaterialSlot::Specular));
    static_assert(StaticAbi::SpecularColorTextureIndex == std::size_t(Asset::MaterialSlot::SpecularColor));
    inline constexpr std::size_t CoverageTextureCount = 1; ///< 覆盖配方只读取材质首槽的基色.
    inline constexpr std::size_t AuxSurfaceTextureCount = std::size_t(Asset::MaterialSlot::Normal) + 1;

    /// @brief 返回该角色额外核对的活动资源组, 供冷包和热候选共用.
    /// Aux 明列片元的三个纹理槽及材质行. Core 的空组由调用方补上完整的静态材质合同.
    [[nodiscard]] std::span<const Shader::ExpectedActiveResources> OpaqueProgramResources(
        Shader::ProductShader role) noexcept;

    /// @brief 两种只写深度的几何角色使用独立的阶段槽, 片元没有颜色输出.
    [[nodiscard]] bool IsDepthRole(Shader::ProductShader role) noexcept;

    /// @brief 只有六个 Froxel 颜色角色允许访问额外的 PerView 绑定.
    [[nodiscard]] bool IsFroxelProgram(Shader::ProductShader role) noexcept;

    /// @brief 明确区分同为 ShaderRead 的对象和材质页, 两者记录步长由生成接口决定.
    enum class TableLayout : std::uint8_t { Objects, Materials };

    /// @brief 安全 View 槽可复用的绘制缓冲. accepted 表示缓存字节具有正常读取资格.
    /// 屏幕几何常量在图确认后更新缓存字节.
    struct DrawBuffer final : Allocation
    {
        using Allocation::Allocation;
        RHI::BufferHandle buffer;
        std::vector<std::byte> contents;
    };

    /// @brief 一个可见表面的有序绘制输入. 几何和对象保有其固定代, material 索引本 View 的 materialRows.
    /// PackBatches 从对象和材质各自的驻留区间取得 Shader 所需的页内行号.
    /// 每批首项在本帧代确定后持有 Pipeline, 其余实例共享首项的状态.
    struct DrawRecord
    {
        std::shared_ptr<GeometryRef> geometry;
        std::shared_ptr<ObjectStorage> object;
        std::size_t material;
        RHI::GraphicsPipelineHandle pipeline;
        RHI::GraphicsPipelineHandle depthPipeline;
        RHI::GraphicsPipelineHandle surfacePipeline; ///< 请求表面时, 批首按同一材质路由持有对应 MRT.
        std::uint32_t pipelineKey;
        double viewDepth;
        std::uint32_t source; ///< 本 SceneFrame 的 Primitive 下标, 用作最终稳定排序键.
        std::uint8_t attributes;
        bool mirrored;
        bool doubleSided;
        bool iorSpecularColor{}; ///< 当前 Draw 的受光颜色使用扩展产品角色.
    };

    /// @brief 相邻兼容状态的一次 indexed indirect 调用, 使用本 View 的实例块及参数区段.
    struct DrawBatch
    {
        std::size_t firstDraw; ///< 代表完整批次状态的有序表面下标.
        std::size_t instanceBlock;
        std::uint32_t firstCommand; ///< 参数缓冲中的记录下标.
        std::uint32_t commandCount;
        RHI::BindingSetHandle renderables;
    };

    /// @brief 本轮着色与天空所需的实际驻留产品, 保有期覆盖帧槽在途工作.
    struct EnvironmentFrame
    {
        LightingProgram program = LightingProgram::Legacy;
        std::shared_ptr<ResidentTexture> dfg;
        std::shared_ptr<ResidentTexture> reflection;
        std::shared_ptr<ResidentTexture> sky;
        std::shared_ptr<ConstantRow> lightingConstants;
        std::shared_ptr<ConstantRow> skyConstants;
        RHI::SamplerHandle sampler;
        RHI::BindingLayoutHandle skyLayout;
        RHI::GraphicsPipelineHandle skyPipeline;
        std::optional<RenderedEnvironment> applied;
    };

    /// @brief 主表面或单个阴影层的静态批次及绑定. 实例行号相对本工作集内的块, 上传批由整个槽共享.
    /// 当前 Draw 引用在安全复用时解除. 同形的常量和参数存储可在下次准备时比较或重写.
    struct StaticDrawWork
    {
        StaticDrawWork(UploadBatch &batch, ForwardEncodingCounts &counts) noexcept :
            uploads(batch), encodings(counts) {}

        std::shared_ptr<ConstantRow> view;
        std::optional<StaticAbi::ForwardView> viewValues;
        std::vector<std::shared_ptr<MaterialRow>> materialRows;
        std::vector<DrawRecord> draws;
        std::vector<DrawBatch> batches;
        std::vector<std::shared_ptr<DrawBuffer>> instanceBlocks; ///< 每块从行 0 起索引, 长度固定为生成接口的容量.
        std::shared_ptr<DrawBuffer> indirect; ///< 本 View 各批次的参数按顺序紧排, 跨块延续记录下标.
        UploadBatch &uploads;
        std::optional<Core::BoundedText> degradation;
        ForwardEncodingCounts &encodings; ///< 直接计入同槽的实际 CPU 编码, 拒绝单路时仍保留事实.
    };

    /// @brief 一路 View 的查询表及可重绘层. levels 和 queryRows 的有效前缀按实际物理层排列.
    /// depth.retained 命中后只读导入数组, 各槽仍独立持有自己的查询及灯行对应数据.
    struct ShadowFrame
    {
        std::vector<StaticDrawWork> levels;
        ShadowDepthSet depth;
        std::shared_ptr<ConstantRow> constants;
        std::optional<StaticAbi::ForwardShadowView> values;
        StaticAbi::ShadowLayersData queryRows{};
        std::shared_ptr<DrawBuffer> layerRows;
        std::shared_ptr<DrawBuffer> references;
    };

    /// @brief 同一冻结 View 的待选路径和附件准入结果. 可执行的待选路径先准备自己的原生键.
    struct OpaqueCandidate
    {
        OpaquePath path;
        ViewResourceNeeds needs;
        std::optional<ForwardFailure> failure;
    };

    /// @brief 本路不透明策略从现有槽存储出发所需的新分配及上传.
    struct OpaqueResourceDemand
    {
        ResidencyDemand residency;
        std::pair<std::size_t, std::size_t> uploads{}; ///< 字节数和写入次数.
    };

    /// @brief 安全槽中的一个 View 候选. Draw, 灯光和阴影各自在实际消费前准备.
    /// 选路先共用可见表面及实例行, 再把一个策略的角色和私有写入交给本轮图.
    /// @note 同槽完成后方可重用常量、实例块和间接缓冲.
    struct ForwardFrameData : StaticDrawWork
    {
        ForwardFrameData(UploadBatch &batch, ForwardEncodingCounts &counts) noexcept :
            StaticDrawWork(batch, counts) {}

        /// @brief 在已准备的可见材质中查找需要辅助深度和前向颜色的表面.
        [[nodiscard]] bool HasAuxiliarySurface() const noexcept
        {
            return std::ranges::any_of(materialRows, [](const auto &row)
            {
                return row->route != OpaqueSurfaceRoute::CoreDeferred;
            });
        }

        ForwardWorkingSource source;
        ViewResourceNeeds needs;
        // 附件不兼容的候选也保留在这里, 供选择阶段向对应请求报告具体失败.
        std::optional<OpaqueCandidate> alternate;
        bool usesLighting = false; ///< 当前颜色存在受光表面, 包括核心屏幕光照及辅助前向光照.
        bool opaqueResourcesReady = false; ///< 选定策略已物化私有资源, 可继续绑定同一程序代.
        std::uint64_t preparationStamp{}; ///< 与 Allocation::lastUse 配对, 零表示本槽无准备或已撤回.
        std::shared_ptr<LightTable> lightTable; ///< 本槽可复用的非空表.
        std::shared_ptr<LightTable> activeLights;
        RHI::BindingLayoutHandle lightLayout;
        RHI::BindingLayoutHandle froxelViewLayout; ///< 选定颜色角色的完整 PerView 布局, 深度仍使用普通布局.
        std::optional<ShaderPipelineRequest> deferredColor; ///< 实际有核心受光时才登记的颜色角色.
        std::optional<StaticAbi::DeferredView> deferredViewValues; ///< 本轮核心角色所需的重建值, 无法编码时为空.
        std::shared_ptr<DrawBuffer> deferredView; ///< 安全帧槽可复用的屏幕几何常量.
        bool deferredViewWriteRecorded = false; ///< 屏幕写入已尝试录制, 尚未获得完整图确认.
        RHI::GraphicsPipelineHandle deferredPipeline;
        RHI::BindingLayoutHandle deferredSurfaceLayout;
        std::optional<FroxelUpload> froxelUpload; ///< 本路拥有的上传组, 已发工作按安全槽完成保留预算凭据.
        std::pair<std::size_t, std::size_t> opaqueUploadReservation{}; ///< 有效方案尚未物化的字节数和写入数.
        std::optional<ResidencyDemand> effectiveOpaqueResidency; ///< 已有效策略尚待结算的设备驻留需求.
        EnvironmentFrame environment;
        std::optional<ShadowFrame> shadow;
        PostProcess::DisplayFrame display;
        bool displayReserved = false; ///< PlanDisplay 已为这份输入登记额度, 本轮失败时可撤回.
    };

    struct ForwardShaderGeneration;

    /// @brief 同槽的两份 Draw 各自保活, 通过 uploads 引用向一个图候选登记所需的写入.
    /// 各路登记各自的显示输入, 其中同一 atlas 只计一次. 驻留内容由图执行后的确认决定.
    struct ForwardFrameStorage
    {
        UploadBatch uploads;
        ForwardEncodingCounts encodings;
        std::array<ForwardFrameData, ForwardViewCount> views{
            ForwardFrameData(uploads, encodings), ForwardFrameData(uploads, encodings)};
        std::shared_ptr<const ForwardShaderGeneration> shaders; ///< 保留本槽已冻结的原生代直至真实完成.
        std::bitset<ForwardViewCount> planned; ///< 本路资源和工作集已准备, 等候 FinishView 绑定选定代.
        bool begun = false;
        bool hasPreparedView = false;
    };

    /// @brief 一代不透明几何、屏幕照明和天空程序的原生表. 每槽持有匹配的已准备 Pipeline.
    struct ForwardPrograms
    {
        /// @brief 取得 ABI 约定的场景布局. 程序和 Pipeline 只在帧前按需求准备.
        explicit ForwardPrograms(RHI::Device &device);
        /// @brief 从该角色的几何或天空槽取拥有型句柄. 槽为空时由帧前准备器补办.
        [[nodiscard]] RHI::GraphicsPipelineHandle FindPipeline(ShaderPipelineRequest key) const noexcept;
        /// @brief 在当前设备 context 创建缺少的角色程序, 布局与指定槽. 创建被拒时抛出 ForwardFailure.
        void PreparePipeline(ForwardDevice &owner, ShaderPipelineRequest key, const Shader::ProductPackages &packages);
        /// @brief 借用该角色已载入包的内容摘要, 尚未载入时为空. 替换程序后重新取值.
        [[nodiscard]] std::string_view ProgramContent(Shader::ProductShader role) const noexcept;
        /// @brief 接续同设备同角色, 包内容及固定状态相同的 Pipeline. 天空槽随带布局, 颜色角色共用设备布局.
        /// @note 几何程序的摘要复制失败时保留目标槽原状. 成功后两代各自保活其句柄.
        [[nodiscard]] bool ReusePipeline(const ForwardPrograms &source, ShaderPipelineRequest key,
                                         std::string_view content);
        /// @brief 清除由该角色派生的程序和 Pipeline 槽, 兼容的顶点布局和其他角色的句柄可沿用.
        void ReplaceProgram(Shader::ProductShader role) noexcept;

        std::array<Shader::RhiProgram, ForwardColorPrograms> colors;
        std::array<Shader::RhiProgram, DeferredLightingCount> deferredColors;
        std::array<Shader::RhiProgram, SurfacePreviewRoleCount> surfacePreviews; ///< 随同代 Pipeline 保存各预览角色的程序及内容摘要.
        Shader::RhiProgram depth;
        Shader::RhiProgram shadowDepth;
        Shader::RhiProgram coreGBuffer;
        Shader::RhiProgram surfaceAux;
        std::array<RHI::BindingLayoutHandle, 3> layouts;
        std::string skyContent;
        RHI::BindingLayoutHandle skyLayout;
        RHI::GraphicsPipelineHandle skyPipeline;
        std::array<RHI::InputLayoutHandle, 8> inputLayouts;
        std::array<RHI::GraphicsPipelineHandle, ScenePipelineCount> pipelines;

    private:
        /// @brief 按颜色角色核对活动绑定并取得设备的固定布局. 三种阴影诊断共用基色覆盖和数组查询配方.
        void PrepareLighting(ForwardDevice &owner, Shader::ProductShader role, const Shader::ProductPackages &packages);
        /// @brief 在当前固定角色槽准备全屏颜色程序及其三个集合, 交由冻结帧持有.
        void PrepareDeferredLighting(ForwardDevice &owner, ShaderPipelineRequest key,
                                     const Shader::ProductPackages &packages);
        /// @brief 按独立预览接口建立固定单样本颜色 Pipeline. 结果沿普通 Shader 代保活.
        void PrepareSurfacePreview(ForwardDevice &owner, ShaderPipelineRequest key,
                                   const Shader::ProductPackages &packages);
        /// @brief 准备只写工作色 RGB 的天空 Pipeline, 成功后保存其包摘要供下次换代比较.
        void PrepareSky(RHI::Device &device, const ForwardPackagePaths &paths, const Shader::ProductPackages &packages);
    };

    /// @brief 一个设备会话内原子选择的程序表. 旧表由冻结帧持有, 未变化的兼容项可在候选间共用.
    struct ForwardShaderGeneration
    {
        ForwardShaderGeneration(RHI::Device &owner, Core::BoundedText nativeIdentity)
            : device(&owner), driver(nativeIdentity), programs(owner) {}
        RHI::Device *device;
        Core::BoundedText driver;
        ForwardPrograms programs;
        PostProcess::DisplayDevice::ProgramTable display;
        std::uint64_t generation = 1;
    };

    /// @brief 带程序角色的自持诊断. 初始化或全局观察失败时, role 可为 Count.
    struct ShaderPreparationFailure
    {
        Shader::ProductShader role;
        Core::BoundedText text;
        std::optional<std::size_t> variant; ///< 原生失败的固定状态槽. 包级拒绝覆盖角色, 源诊断可不涉及槽.
    };

    /// @brief 缓存映射只拥有可复用代或本次准备候选. 单线程推进 stamp, 前一帧槽保有退役代.
    struct ForwardDevice
    {
        enum class ShaderPreparationPhase : std::uint8_t { Admission, Active, Candidate };

        ForwardDevice(RHI::Device &source, const ForwardPackagePaths &paths, std::uint32_t textureLimit,
                      std::uint32_t uniformAlignment, Core::BoundedText nativeIdentity,
                      ShaderPreparationLimits shaderLimits);
        /// @brief 登记几何或天空键并查询 Active. 缺项返回空句柄, 已登记的硬失败抛出 ForwardFailure.
        [[nodiscard]] RHI::GraphicsPipelineHandle RequestShader(ShaderPipelineRequest key);
        /// @brief 查询待切入颜色键, 带回当前角色的源及原生首因, 不改写活动表和使用统计.
        [[nodiscard]] std::optional<ForwardFailure> CheckSwitchShader(ShaderPipelineRequest key) const noexcept;
        /// @brief 登记本 View 的整条显示链. 返回 true 时, 该链可从 Active 取得全部阶段.
        [[nodiscard]] bool RequestDisplay(const ViewLayouts &layouts, DisplaySettings settings);
        /// @brief 仅在帧前 owner 阶段尝试接纳当前包并推进一次有界的原生工作.
        void AdvanceShaders(std::uint64_t request, std::optional<Shader::ReloadResult> result,
                            bool continueActive = false) noexcept;
        /// @brief 候选表及本帧显示资源就绪, 且切换后仍有空间记录旧表.
        [[nodiscard]] bool ShaderCandidateReady() const noexcept;
        /// @brief 构图前核对最新源修订, 只在已准备的候选仍具备资格时切换.
        void PublishShaders(std::uint64_t request, std::optional<Shader::ReloadResult> result) noexcept;
        /// @brief 将上下文首个错误附加到本轮刚失败的原生工作, 供同一诊断快照读取.
        void RecordShaderDiagnostic(const Core::BoundedText &reason) noexcept;
        /// @brief 分配共享图候选的准备代.
        void Begin();
        /// @brief 按当前 SceneFrame 的完整模型版本表撤销旧几何代的命中资格.
        /// @note 空模型表撤销全部几何目录项, 在途 Draw 继续持有页区间直到其槽安全释放.
        void ObserveGeometry(const SceneFrame &scene);
        /// @brief 开始独立 View 的工作集记账, 继续使用当前候选的共享写入.
        void BeginView(std::size_t index);
        /// @brief 回到先前已规划的同一路, 继续使用其额度和去重记录.
        void ResumeView(std::size_t index) noexcept;
        /// @brief 检查该代是否已接受或已在本轮登记使用. 共用待写代的调用方须确保本批已持有生产者.
        [[nodiscard]] bool Current(const Allocation &entry) const noexcept;
        /// @brief 每代每轮只计一次工作集占用, 超出单 View 硬预算时抛 ForwardFailure.
        void Use(Allocation &entry);
        /// @brief 分配前核对单 View 及全缓存容量, 尝试淘汰空闲代, 仍受占用时返回等待错误.
        void Reserve(std::size_t bytes, bool paged = false);
        /// @brief 核对一组未分配的设备驻留需求, 包括其它有效计划需要保留的余量.
        /// 仅淘汰空闲代. 实际字节和条目仍随分配对象寿命记账.
        void CheckDeviceResidency(ResidencyDemand demand);
        /// @brief 从兼容页中借出一个新代的区间. 失败由已有准备边界回收未发布区间.
        void AllocateSlice(PageSlice &slice, const Allocation &owner, GraphUsage usage, std::uint8_t layout,
                           std::size_t bytes, std::size_t alignment, std::size_t tableRows = 0);
        /// @brief 只删除缓存独占且本轮未使用的代. 帧槽保有的引用阻止在途代被提前扣账.
        void EvictUnused() noexcept;

        RHI::Device &device;
        std::uint32_t maxTextureSize;
        std::uint32_t uniformAlignment;
        const ForwardPackagePaths packagePaths;
        const Core::BoundedText shaderDevice;
        const ShaderPreparationLimits shaderLimits;
        ShaderDemand shaderDemand; ///< 本轮各路实际请求的键并集.
        ShaderPreparationStatistics shaderStatistics;
        std::shared_ptr<ForwardShaderGeneration> activeShaders;
        std::shared_ptr<ForwardShaderGeneration> candidateShaders;
        std::vector<std::weak_ptr<const ForwardShaderGeneration>> retiredShaders;
        std::vector<RHI::GraphicsPipeline *> countedPipelines; ///< 预留全部表的计数空间, 只在 owner 内借用.
        Shader::ProductPackages activePackages;
        std::optional<Shader::ReloadResult> candidateInput; ///< 最新请求的 CPU 包. 原生键失败后保留至重选或请求换代.
        bool sourcePending{};
        bool shaderCacheDirty{};
        bool retryOnReselection{}; ///< 已观察到候选的原生失败键退出计划, 同一键再出现时重试.
        std::optional<ShaderPreparationFailure> sourceFailure, displayFailure;
        std::vector<ShaderPreparationFailure> activeFailures; ///< 保留活动代仍在需求中的失败角色或单键及首因.
        std::optional<ShaderPreparationPhase> lastShaderFailure; ///< 记录本次推进失败的诊断归属.
        PostProcess::DisplayDevice display;
        std::array<RHI::BindingLayoutHandle, ForwardPerPassLayouts> lightLayouts; ///< 两个候选策略共用的固定配方.
        RHI::BindingLayoutHandle froxelViewLayout; ///< 首次需要 Froxel Pipeline 时准备, 同设备各代共用.
        RHI::BindingLayoutHandle deferredSurfaceLayout; ///< 屏幕常量和最终表面采样, 同设备各代共用.
        std::shared_ptr<ResidencyCapacity> capacity = std::make_shared<ResidencyCapacity>();
        std::uint64_t stamp = 0;
        std::uint64_t frameBegin = 0; ///< 本候选最早使用代, 各 View 再单独推进 stamp.
        /// @brief 一路 View 从规划到补办 caster 准备共用的额度. 同一存储在该 stamp 内只计一次.
        struct ViewBudget
        {
            std::uint64_t stamp{};
            std::size_t bytes{};
            std::size_t entries{};
        };
        std::array<ViewBudget, ForwardViewCount> viewBudgets;
        std::size_t preparingView = 0;
        std::uint64_t materialRequests = 0;
        std::uint64_t materialHits = 0;
        std::uint64_t materialGeneration = 0;
        std::uint64_t pageOrdinal = 0;
        std::vector<std::shared_ptr<BufferPage>> pages = std::vector<std::shared_ptr<BufferPage>>(0);
        std::map<GeometryKey, std::shared_ptr<GeometryRef>> geometry;
        Scene::SceneId geometryScene;
        std::uint64_t geometryStructure{};
        std::uint64_t geometryContent{};
        std::vector<AssetVersionKey> geometryVersions = std::vector<AssetVersionKey>(0);
        std::map<TextureKey, std::shared_ptr<ResidentTexture>> textures;
        std::map<ObjectRowKey, ObjectRow> objects;
        std::map<MaterialKey, MaterialEntry> materials;
        std::map<std::pair<std::uint64_t, SamplingKey>, std::weak_ptr<const RHI::BindingSetHandle>> materialBindings;
        std::map<SamplerKey, RHI::SamplerHandle> samplers;
        std::shared_ptr<ResidentTexture> white;
        std::shared_ptr<LightTable> emptyLights; ///< 初始化后只读, 供诊断、零灯及全 Unlit 工作集使用.
        Scene::SceneId encodedLightScene;
        std::uint64_t encodedLightStructure{};
        std::map<std::pair<std::uint32_t, std::uint64_t>, EncodedLight> encodedLights; ///< 当前 Scene 的实体槽和代.
        EnvironmentCache environmentCache;
        std::map<std::pair<EnvironmentKey, bool>, ResidentEnvironment> environmentTextures;
        std::shared_ptr<ResidentTexture> dfgTexture;
        RHI::SamplerHandle environmentSampler;
    };

    /// @brief 中止本次准备并携带有界原因, 由 ForwardRenderer::Prepare 转换为可见结果.
    [[noreturn]] void Reject(ViewSettingReason reason, std::string_view description);
    /// @brief 从三个属性位生成交错流偏移和步长, 必有 position/normal/color.
    [[nodiscard]] VertexLayout StaticVertexLayout(std::uint8_t attributes) noexcept;
    /// @brief 把实际阶段及光照角色和固定状态映射到一个无碰撞的准备槽.
    [[nodiscard]] ShaderPipelineRequest GeometryRequest(std::uint8_t layout, bool mirrored, bool doubleSided,
                                                        SurfacePhase phase, LightingProgram kind,
                                                        bool iorSpecular = false) noexcept;
    /// @brief 分配 Common 进出的普通缓冲. 只读表从结构步长定义角色, 内容尚未初始化.
    [[nodiscard]] RHI::BufferHandle AllocateBuffer(RHI::Device &device, GraphUsage usage, std::size_t size,
        std::uint32_t stride = 0, const char *name = "Forward buffer");
    /// @brief 创建并记账常量. set 有值时建立固定 View 集合, 否则由图声明实际绑定.
    [[nodiscard]] std::shared_ptr<ConstantRow> CreateConstantRow(ForwardDevice &device, std::size_t bytes,
                                                                  std::optional<std::size_t> set);
    /// @brief 为有 Draw 的颜色 Pass 选择普通灯表或共享零表, 将匹配的表留在 frame.activeLights.
    /// @param usesLighting 当前有 Draw 在直接光或物理模式受光. 为假或可见灯为空时使用零表.
    /// @pre frame 所在帧槽已可安全重写. 编码或额度失败沿 Forward 准备入口返回, 整个候选停止.
    void PrepareLightTable(ForwardDevice &device, ForwardFrameData &frame, const PreparedView &view,
                           bool usesLighting);
    /// @brief 冻结当前有效角色与曝光. 产品未准备时中止当前正常候选, 由 View 保留旧接受输出.
    void PrepareEnvironment(ForwardDevice &device, ForwardFrameData &frame, const PreparedView &view,
                            PreviewMode mode, bool litDraws);
    /// @brief 复用已接受或本批能完整生产的纹理. 从未录制的代可原地补传, 已尝试写入的未接受代另建存储.
    /// @pre image 来自固定的已发布 CPU 记录, key 包含对应版本, 颜色解释和 mip 策略.
    [[nodiscard]] std::shared_ptr<ResidentTexture> PrepareTexture(ForwardDevice &device, UploadBatch &uploads,
        const TextureKey &key, const Asset::Rgba8Image &image);
    /// @brief 为缺席槽提供共享线性白图, 首次使用仍须经同一图上传及接受确认.
    [[nodiscard]] std::shared_ptr<ResidentTexture> WhiteTexture(ForwardDevice &device, UploadBatch &uploads);
    /// @brief 对合法 glTF sampler 填入本预览 profile 的线性过滤缺省值, 保留显式环绕和 mip 选择.
    [[nodiscard]] SamplerKey EffectiveSampler(const Asset::ModelSampler &sampler) noexcept;
    /// @brief 共享合法枚举组合的 sampler, LOD 可访问范围由对应纹理层数约束.
    [[nodiscard]] RHI::SamplerHandle PrepareSampler(ForwardDevice &device, const SamplerKey &key);
    /// @brief 准备主可见集及程序键. 按诊断选择或实际受光材质决定查询范围, 再为需要重绘的层准备 caster.
    /// @param shadowLight 诊断时借用选中组. 空指针选择基础预览, 受光分支可从 view 取得多个组.
    void PlanDraws(ForwardDevice &device, ForwardFrameData &frame, const PreparedView &view, PreviewMode mode,
                   const ShadowLightPlan *shadowLight);
    /// @brief 从本帧选定代取得每个批次和天空所需的句柄. 缺项报告等待或硬失败, 不创建 Pipeline.
    void ApplyDrawShaders(ForwardDevice &device, ForwardFrameData &frame, const PreparedView &view);
    /// @brief 从同一灯光配方选择该策略的候选来源.
    [[nodiscard]] LightingProgram OpaqueColorProgram(const ForwardFrameData &frame, OpaquePath path) noexcept;
    /// @brief 核对当前可见表面对所选表面及颜色路径所需的原生键.
    [[nodiscard]] std::optional<ForwardFailure> CheckOpaqueShaders(const ForwardDevice &device,
        const ForwardFrameData &frame, const ViewResourceNeeds &needs, LightingProgram program) noexcept;
    /// @brief 从选定角色、CPU Froxel 和安全槽现有屏幕常量推导实际分配及写入需求.
    [[nodiscard]] OpaqueResourceDemand OpaqueRequirements(const ForwardDevice &device, const ForwardFrameData &frame,
        const PreparedView &view, bool coreLighting, LightingProgram program) noexcept;
    /// @brief 完成所选路径的批次划分、屏幕常量和 Froxel. 局部失败保留先前已准备的策略.
    /// @param protectedResidency 候选准入时传入其它有效策略尚未分配的容量. 有效策略直接结算实际分配.
    /// @pre 当前设备记账指向该 View, 上传批尚未封存.
    void PrepareOpaqueResources(ForwardDevice &device, ForwardFrameData &frame, const PreparedView &view,
        const ViewResourceNeeds &needs, LightingProgram program, const GraphBufferSupport &buffers,
        const std::optional<ResidencyDemand> &protectedResidency);
}
