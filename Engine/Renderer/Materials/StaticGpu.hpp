#pragma once

/// @file
/// @brief Forward 的设备缓存、拥有型写区段与可绘记录. 类型仅在 Renderer 实现内共享.

#include <array>
#include <map>
#include <tuple>
#include <utility>

#include <Shader/RhiProgram.hpp>
#include <StaticForward/Data.hpp>

#include "../Paths/Forward.hpp"
#include "../Lighting/EnvironmentCache.hpp"

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
    /// @brief 模型版本, 紧凑 mesh, 源 Primitive 槽, 属性位和重建切线的 UV. UV=-1 表示沿用源几何.
    using GeometryKey = std::tuple<AssetVersionKey, std::uint32_t, std::uint32_t, std::uint8_t, std::int8_t>;
    /// @brief 外置标志, 固定资产版本, 源 image 下标, 颜色域及完整 mip 策略, sampler 另行共享.
    using TextureKey = std::tuple<bool, AssetVersionKey, std::uint32_t, Asset::ImageDomain, bool>;
    using ObjectKey = std::array<std::uint64_t, 4>; ///< Scene 与对象各自的槽和分配代.
    /// @brief 以对象身份和传给 Shader 的 FP32 世界基点定位对象行. 命中后核对原始世界矩阵,
    /// 只有同基点且仿射系数仍匹配的图元才复用这一行及其编码余量.
    using ObjectRowKey = std::pair<ObjectKey, std::array<float, 3>>;
    /// @brief 模型版本, 源材质, 覆盖对象及源 Primitive 槽. 无覆盖时后两项为零, 行按源材质共享.
    using MaterialKey = std::tuple<AssetVersionKey, std::uint32_t, ObjectKey, std::uint32_t>;
    using SamplerKey = std::array<std::uint32_t, 4>; ///< min、mag、wrapS、wrapT, 已填充缺省值.

    /// @brief 记账附着于资源实际寿命, 被替换而仍由槽保活的代继续占用额度.
    struct ResidencyCapacity
    {
        std::size_t bytes = 0;
        std::size_t entries = 0;
    };

    /// @brief 一代存储的逻辑预算与上传事实. accepted 允许后继有序读取, 不表示 GPU 已完成.
    /// @note 由派生类型的 shared_ptr 销毁, 最后一个缓存或帧槽引用释放时归还预算.
    struct Allocation
    {
        Allocation(std::shared_ptr<ResidencyCapacity> quota, std::size_t size) noexcept;
        ~Allocation();
        Allocation(const Allocation &) = delete;
        Allocation &operator=(const Allocation &) = delete;

        std::shared_ptr<ResidencyCapacity> capacity;
        std::size_t bytes;
        std::uint64_t lastUse = 0; ///< ForwardDevice 的准备轮次, 不是 CPU 帧号或提交票据.
        bool accepted = false;
    };

    /// @brief 仅存此布局实际消费的 UV 与切线. 缺席的输入位置绑定到不会被材质读取的字段.
    struct VertexLayout
    {
        std::array<std::uint32_t, 6> offsets{};
        std::uint32_t stride = 0;
    };

    /// @brief 一项固定布局的独占顶点/索引存储. 首版直接绘制从索引 0 开始的完整区间.
    struct GeometryRef final : Allocation
    {
        using Allocation::Allocation;
        GeometryKey key;
        RHI::BufferHandle vertices;
        RHI::BufferHandle indices;
        std::uint32_t indexCount = 0;
    };

    /// @brief 指定颜色解释及 mip 策略的图像代, 层级内容由 UploadBatch 完整生产.
    struct ResidentTexture final : Allocation
    {
        using Allocation::Allocation;
        RHI::TextureHandle texture;
    };

    /// @brief 一个普通 UBO 及其布局匹配的集合. 对象行换代, per-view 行只在安全槽内重写.
    struct ConstantRow final : Allocation
    {
        using Allocation::Allocation;
        RHI::BufferHandle buffer;
        RHI::BindingSetHandle binding;
    };

    /// @brief 一块定长灯缓冲及其 CPU 复用依据. indices 保存裁剪所得原序, 缓冲中的行按灯类型分段.
    /// 两个计数随同槽 View 常量交付. 接受前只可依赖本图的上传版本, 同槽 GPU 完成后才可重写.
    struct LightTable final : Allocation
    {
        using Allocation::Allocation;
        RHI::BufferHandle buffer;
        Scene::SceneId scene;
        std::uint64_t contentRevision = 0;
        std::vector<std::uint32_t> indices;
        std::uint32_t directionalCount = 0;
        std::uint32_t localCount = 0;
    };

    /// @brief 缓存完整世界变换以判定对象行是否仍匹配, 不以全场景版本触发无关重传.
    struct ObjectRow
    {
        std::array<double, 16> worldFromLocal;
        std::shared_ptr<ConstantRow> resident;
    };

    /// @brief GPU 材质行与预解析集合. 缺席槽绑定共享白图, Shader 仍由有效 slot 判定是否采样.
    struct MaterialRow final : Allocation
    {
        using Allocation::Allocation;
        std::array<std::shared_ptr<ResidentTexture>, Scene::MaterialSlotCount> textures;
        std::array<RHI::SamplerHandle, Scene::MaterialSlotCount> samplers;
        RHI::BufferHandle buffer;
        RHI::BindingSetHandle binding;
        StaticAbi::ForwardMaterial values{};
    };

    /// @brief 拥有缓冲和稳定字节, revision 用于接受后确认所属存储代, 不以声明成功确认.
    struct BufferUpload
    {
        std::shared_ptr<Allocation> revision;
        RHI::BufferHandle buffer;
        std::vector<std::byte> payload;
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

    /// @brief 帧拥有的写入内容. 每个普通缓冲从 0 写精确范围, 纹理按完整 mip 层写入.
    /// @note 仅在准备阶段追加, Declare 后保持不变至图执行结束. 预算失败中止整份候选.
    struct UploadBatch
    {
        std::vector<BufferUpload> buffers;
        std::vector<TextureUpload> textures;
        std::vector<EnvironmentUpload> environmentTextures;
        std::size_t bytes = 0;
        std::size_t writes = 0;

        /// @brief 在复制载荷前累计逻辑字节和命令数, 超限抛 ForwardFailure; 不预留设备上传池.
        void ReserveWrites(std::size_t byteCount, std::size_t count);
        /// @brief 保有目标及所属代, 复制 source 全部字节. 调用返回后不再借用源区段.
        void Add(const std::shared_ptr<Allocation> &revision, const RHI::BufferHandle &buffer,
                 std::span<const std::byte> source);
        /// @brief 结束 CPU 载荷与上传记录的所有权, 不修改资源代的 accepted 状态.
        void Clear() noexcept;
    };

    /// @brief 三种光栅组合: 合并写深度/颜色, 只写前置深度, 相等深度测试的只读深度着色.
    enum class SurfacePhase : std::uint8_t { Combined, Depth, ReadOnlyDepth };
    /// @brief 按实际资源形状选择颜色变体, 与每轮连续创作参数独立.
    enum class LightingProgram : std::uint8_t { Legacy, Physical, Ibl };
    inline constexpr std::uint32_t DfgSlot = 1; ///< StaticForward 的 PerPass t1.
    inline constexpr std::uint32_t ReflectionSlot = 2; ///< StaticForward 的 PerPass t2.

    /// @brief 索引及资源范围来自固定几何代; material 是本槽的有效 GPU 材质行索引.
    struct DrawRecord
    {
        std::shared_ptr<GeometryRef> geometry;
        std::shared_ptr<ConstantRow> object;
        std::size_t material;
        RHI::GraphicsPipelineHandle pipeline;
        RHI::GraphicsPipelineHandle depthPipeline;
        std::uint32_t pipelineKey;
        double viewDepth;
        std::uint32_t source; ///< 本 SceneFrame 的 Primitive 下标, 用作最终稳定排序键.
        std::uint8_t attributes;
        bool mirrored;
        bool doubleSided;
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

    /// @brief 同槽本轮的只读 Draw, 绑定和上传. view/viewValues 和普通灯表跨安全复用保留.
    /// Reset 解除本次 Draw, 材质, 活动灯表和显示引用, layout/needs 在下次准备时替换.
    /// @note 图回调借用这里保有的 RHI 对象; graph 析构后才可释放载荷, 同槽完成后才可重置 Draw.
    struct ForwardFrameData
    {
        ViewImageLayout layout;
        ViewResourceNeeds needs;
        std::shared_ptr<ConstantRow> view;
        std::optional<StaticAbi::ForwardView> viewValues;
        std::shared_ptr<LightTable> lightTable; ///< 本槽可复用的非空表.
        std::shared_ptr<LightTable> activeLights; ///< 当前颜色 Pass 实际借用的普通表或设备空表.
        RHI::BindingLayoutHandle lightLayout;
        EnvironmentFrame environment;
        std::vector<std::shared_ptr<MaterialRow>> materialRows;
        std::vector<DrawRecord> draws;
        UploadBatch uploads;
        RHI::GraphicsPipelineHandle displayPipeline;
        RHI::BindingLayoutHandle displayLayout;
        std::optional<Core::BoundedText> degradation;
    };

    /// @brief 初始化完整程序代, 按真实布局和阶段从固定数量的 Pipeline 中准备所需变体.
    struct ForwardPrograms
    {
        /// @brief 一次取得三份 ABI 匹配的程序及显示 Pipeline, 任一失败由构造展开释放局部资源.
        ForwardPrograms(RHI::Device &device, const ForwardPackagePaths &paths);
        /// @brief 按属性, 镜像, 双面和阶段惰性创建并缓存 Pipeline, 失败抛 ForwardFailure.
        [[nodiscard]] RHI::GraphicsPipelineHandle GeometryPipeline(RHI::Device &device, std::uint8_t layout,
            bool mirrored, bool doubleSided, SurfacePhase phase, LightingProgram kind);
        /// @brief 按颜色角色准备程序和 set 3 布局. 物理变体的片元活动资源须与该角色完全匹配.
        /// @note 包或布局被拒绝时抛出 ForwardFailure, 只有完整就绪的角色才可命中缓存.
        void PrepareLighting(RHI::Device &device, LightingProgram kind);
        /// @brief 首次实际需要天空时准备独立颜色混合管线.
        void PrepareSky(RHI::Device &device);

        std::array<Shader::RhiProgram, 3> colors;
        Shader::RhiProgram depth;
        std::array<RHI::BindingLayoutHandle, 3> layouts;
        std::array<RHI::BindingLayoutHandle, 3> lightLayouts;
        std::array<std::filesystem::path, 2> physicalPaths;
        std::filesystem::path skyPath;
        RHI::BindingLayoutHandle skyLayout;
        RHI::GraphicsPipelineHandle skyPipeline;
        RHI::BindingLayoutHandle displayLayout;
        RHI::GraphicsPipelineHandle displayPipeline;
        std::array<RHI::InputLayoutHandle, 8> inputLayouts;
        std::array<RHI::GraphicsPipelineHandle, 8 * 4 * 3 * 3> pipelines;
    };

    /// @brief 缓存映射只拥有可复用代或本次准备候选. 单线程推进 stamp, 前一帧槽保有退役代.
    struct ForwardDevice
    {
        ForwardDevice(RHI::Device &source, const ForwardPackagePaths &paths, std::uint32_t textureLimit);
        /// @brief 分配本轮准备身份并清零工作集预算, 不清除旧缓存或上传确认事实.
        void Begin();
        /// @brief 已接受代可复用; 本轮尚未接受的代也可共享, 其写入已由同一批次拥有.
        [[nodiscard]] bool Current(const Allocation &entry) const noexcept;
        /// @brief 每代每轮只计一次工作集占用, 超出单 View 硬预算时抛 ForwardFailure.
        void Use(Allocation &entry);
        /// @brief 分配前核对单 View 及全缓存容量, 尝试淘汰空闲代, 仍受占用时返回等待错误.
        void Reserve(std::size_t bytes);
        /// @brief 只删除缓存独占且本轮未使用的代. 帧槽保有的引用阻止在途代被提前扣账.
        void EvictUnused() noexcept;

        RHI::Device &device;
        std::uint32_t maxTextureSize;
        ForwardPrograms programs;
        std::shared_ptr<ResidencyCapacity> capacity = std::make_shared<ResidencyCapacity>();
        std::uint64_t stamp = 0;
        std::size_t preparedBytes = 0;
        std::size_t preparedEntries = 0;
        std::map<GeometryKey, std::shared_ptr<GeometryRef>> geometry;
        std::map<TextureKey, std::shared_ptr<ResidentTexture>> textures;
        std::map<ObjectRowKey, ObjectRow> objects;
        std::map<MaterialKey, std::shared_ptr<MaterialRow>> materials;
        std::map<SamplerKey, RHI::SamplerHandle> samplers;
        std::shared_ptr<ResidentTexture> white;
        std::shared_ptr<LightTable> emptyLights; ///< 初始化后只读, 供诊断、零灯及全 Unlit 工作集使用.
        EnvironmentCache environmentCache;
        std::map<std::pair<EnvironmentKey, bool>, std::shared_ptr<ResidentTexture>> environmentTextures;
        std::shared_ptr<ResidentTexture> dfgTexture;
        RHI::SamplerHandle environmentSampler;
    };

    /// @brief 中止本次准备并携带有界原因, 由 ForwardRenderer::Prepare 转换为可见结果.
    [[noreturn]] void Reject(ViewSettingReason reason, std::string_view description);
    /// @brief 从三个属性位生成交错流偏移和步长, 必有 position/normal/color.
    [[nodiscard]] VertexLayout StaticVertexLayout(std::uint8_t attributes) noexcept;
    /// @brief 把已验证的属性位与光栅组合映射到固定 96 项表, 不截断或哈希状态.
    [[nodiscard]] std::uint32_t PipelineKey(std::uint8_t layout, bool mirrored, bool doubleSided,
                                           SurfacePhase phase) noexcept;
    /// @brief 分配 Common 进出的普通缓冲, usage 只取顶点, 索引或常量; 内容尚未初始化.
    [[nodiscard]] RHI::BufferHandle AllocateBuffer(RHI::Device &device, GraphUsage usage, std::size_t size);
    /// @brief 创建并记账 view/object UBO 及对应集合, 字节由调用者追加到上传包.
    [[nodiscard]] std::shared_ptr<ConstantRow> CreateConstantRow(ForwardDevice &device, std::size_t bytes,
                                                                  std::size_t set);
    /// @brief 为有 Draw 的颜色 Pass 选择普通灯表或共享零表, 将匹配的表留在 frame.activeLights.
    /// @param usesLighting 当前有 Draw 在直接光或物理模式受光. 为假或可见灯为空时使用零表.
    /// @pre frame 所在帧槽已可安全重写. 编码或额度失败沿 Forward 准备入口返回, 整个候选停止.
    void PrepareLightTable(ForwardDevice &device, ForwardFrameData &frame, const PreparedView &view,
                           bool usesLighting);
    /// @brief 冻结当前有效角色与曝光. 产品未准备时中止当前正常候选, 由 View 保留旧接受输出.
    void PrepareEnvironment(ForwardDevice &device, ForwardFrameData &frame, const PreparedView &view,
                            PreviewMode mode, bool litDraws);
    /// @brief 复用匹配且有效的纹理代, 或创建新存储并把所有层级字节交给本轮上传.
    /// @pre image 来自固定的已发布 CPU 记录, key 包含对应版本, 颜色解释和 mip 策略.
    [[nodiscard]] std::shared_ptr<ResidentTexture> PrepareTexture(ForwardDevice &device, UploadBatch &uploads,
        const TextureKey &key, const Asset::Rgba8Image &image);
    /// @brief 为缺席槽提供共享线性白图, 首次使用仍须经同一图上传及接受确认.
    [[nodiscard]] std::shared_ptr<ResidentTexture> WhiteTexture(ForwardDevice &device, UploadBatch &uploads);
    /// @brief 对合法 glTF sampler 填入本预览 profile 的线性过滤缺省值, 保留显式环绕和 mip 选择.
    [[nodiscard]] SamplerKey EffectiveSampler(const Asset::ModelSampler &sampler) noexcept;
    /// @brief 共享合法枚举组合的 sampler, LOD 可访问范围由对应纹理层数约束.
    [[nodiscard]] RHI::SamplerHandle PrepareSampler(ForwardDevice &device, const SamplerKey &key);
    /// @brief 解析可见集, 记录局部不支持项并排序. 全部可见项不可绘时失败, 视锥空集可输出背景.
    void PrepareDraws(ForwardDevice &device, ForwardFrameData &frame, const PreparedView &view, PreviewMode mode);
}
