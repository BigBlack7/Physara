#pragma once

/// @file
/// @brief CPU 静态场景的实体身份、编辑事务和拥有型记录.
/// 活动 Scene 在 owner 线程编辑. 候选可在 worker 独立构建, 移交之后由接收方独占.

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <Core/Identity.hpp>

#include "SceneMaterial.hpp"

namespace Physara::Scene
{
    struct SceneTag;
    using SceneId = Core::Identity<SceneTag>;
    using EntityId = std::string;

    /// @brief 仅在关联 Scene 中解析的运行期实体身份. 删除后原代永久失效.
    struct EntityHandle
    {
        SceneId scene;
        std::uint32_t index{};
        std::uint64_t generation{};

        bool operator==(const EntityHandle &) const = default;
    };

    /// @brief 右手系米制平移和 xyzw 单位四元数, 与保存导航位姿共用.
    struct Pose
    {
        std::array<double, 3> translation{};
        std::array<double, 4> rotation{0, 0, 0, 1};

        bool operator==(const Pose &) const = default;
    };

    /// @brief 按 T*R*S 作用于列向量. 有符号缩放含零轴.
    struct LocalTransform : Pose
    {
        std::array<double, 3> scale{1, 1, 1};

        bool operator==(const LocalTransform &) const = default;
    };

    /// @brief 检查有限平移与允许误差内的单位四元数, 不改写或归一化输入.
    [[nodiscard]] bool IsValidPose(const Pose &pose) noexcept;
    using Camera = std::variant<Asset::ModelPerspective, Asset::ModelOrthographic>;

    /// @brief 方向灯强度为 lux, 点光和聚光强度为 candela. range 缺省为无截断.
    struct Light
    {
        Asset::ModelLightKind type{};
        std::array<float, 3> color{1, 1, 1};
        double intensity = 1;
        std::optional<double> range;
        double innerConeAngle{};
        double outerConeAngle{};

        bool operator==(const Light &) const = default;
    };

    /// @brief 源 node 在该实例根固定的模型版本内有效. 层级重挂后仍保留来源.
    struct SourceRef
    {
        EntityId instanceEntityId;
        std::uint32_t node{};

        bool operator==(const SourceRef &) const = default;
    };

    /// @brief 编辑器可修改的实体值. castsShadow 缺席表示没有 MeshRenderer.
    struct EntityProperties
    {
        std::string name;
        LocalTransform local;
        bool visible = true;
        std::optional<bool> castsShadow;
        std::optional<Camera> camera;
        std::optional<Light> light;

        bool operator==(const EntityProperties &) const = default;
    };

    /// @brief 自持的实体记录, 可用于快照、候选或历史. model 只在实例根存在.
    struct EntityRecord
    {
        EntityId id;
        std::optional<EntityId> parent;
        EntityProperties properties;
        Asset::ModelVersion model;
        std::optional<SourceRef> source;
        std::vector<PrimitiveOverride> overrides;

        bool operator==(const EntityRecord &) const = default;
    };

    /// @brief 世界空间的轴对齐包围盒. 无几何范围由外层 optional 表达.
    struct Bounds
    {
        std::array<double, 3> minimum;
        std::array<double, 3> maximum;
    };

    /// @brief 实例世界数据, 矩阵按列主序. orientation 表达 -1/0/+1 的绕序变换.
    /// 几何使用完整仿射矩阵; 相机与定向灯使用 pose, 缩放只经层级矩阵影响其原点.
    struct WorldState
    {
        std::array<double, 16> worldFromLocal{};
        Pose pose; ///< 世界原点及忽略缩放的单位朝向, 与几何矩阵随同一事务提交.
        int orientation = 1;
        std::vector<std::optional<Bounds>> primitives;
        std::optional<Bounds> bounds;

        /// @brief 相机和定向灯使用的世界单位方向, 对应局部 -Z.
        [[nodiscard]] std::array<double, 3> Forward() const noexcept;
    };

    /// @brief 分开标识保存内容, 非相机渲染内容, 相机输入和世界数据更新.
    /// 初始空场景均为零, 只在事务接受时递增; 名称编辑不推进后三项.
    struct Revisions
    {
        std::uint64_t document{};
        std::uint64_t renderContent{};
        std::uint64_t camera{};
        std::uint64_t transforms{};

        bool operator==(const Revisions &) const = default;
    };

    /// @brief 场景环境的创作值. rotation 是环境空间至世界空间的单位 xyzw 四元数.
    /// intensity 为有限非负倍率. lighting 和 sky 至少有一项开启, 分别请求间接光和背景.
    struct EnvironmentSettings
    {
        std::array<double, 4> rotation{0, 0, 0, 1};
        double intensity = 1; ///< 无量纲倍率, 场景曝光与源标定另由实际消费者取得.
        bool lighting = false;
        bool sky = false;

        bool operator==(const EnvironmentSettings &) const = default;
    };

    /// @brief Scene 固定当前环境的源版本, 用途与创作值随文档整体发布.
    struct SceneEnvironment
    {
        Asset::EnvironmentVersion source;
        EnvironmentSettings settings;

        bool operator==(const SceneEnvironment &) const = default;
    };

    /// @brief 同步的只读借用, 有效至下一次场景编辑或替换.
    struct EntityView
    {
        const EntityRecord &record;
        const WorldState &world;
        const Asset::ModelVersion *model; ///< 实例根或源节点解析到的模型. 普通实体为空.
        const Asset::ModelNode *sourceNode;
    };

    /// @brief 当前 Scene 相机的独立值, 可在借用结束后用于同帧准备.
    struct CameraState
    {
        EntityHandle entity;
        Pose pose;
        Camera projection;
    };

    enum class ReparentPolicy : std::uint8_t
    {
        KeepLocal,
        KeepWorld
    };

    /// @brief 新建普通实体. 模型网格组件须通过 Instantiate 的源节点建立.
    struct CreateEntity
    {
        std::optional<EntityHandle> parent;
        EntityProperties properties;
    };

    /// @brief 从已发布模型的所选 scene 建立独立实例根及节点, 整体成功或拒绝.
    struct Instantiate
    {
        std::optional<EntityHandle> parent;
        Asset::ModelVersion model;
        std::string name;
    };

    /// @brief 删除该实体及后代, 同时覆盖被删实例根在层级外关联的源节点.
    struct EraseSubtree
    {
        EntityHandle entity;
    };

    /// @brief 按显式策略重挂实体. KeepWorld 对不可逆父级或不可表示的 local TRS 返回失败.
    /// 子树含相机或定向灯时同时保留其刚体朝向, 与几何无法兼容则整体拒绝.
    struct Reparent
    {
        EntityHandle entity;
        std::optional<EntityHandle> parent;
        ReparentPolicy policy;
    };

    /// @brief 替换实体创作值. 移除网格组件同时移除该实体的全部 Primitive 覆盖.
    struct EditProperties
    {
        EntityHandle entity;
        EntityProperties properties;
    };

    /// @brief 替换或清除一个源 Primitive 槽的稀疏覆盖, 不改写共享模型.
    struct EditOverride
    {
        EntityHandle entity;
        std::uint32_t primitiveSlot;
        std::optional<PrimitiveOverride> value; ///< 空值清除该 Primitive 的修改.
    };

    /// @brief 为当前 Scene 设置完整环境请求. 空值移除环境, 不影响实体和主 View.
    /// @note 非空值须引用 Asset 已发布的环境版本. 编辑入口负责把延迟意图定位到所属 Scene.
    struct SetEnvironment
    {
        std::optional<SceneEnvironment> value;
    };

    using Command = std::variant<CreateEntity, Instantiate, EraseSubtree, Reparent, EditProperties, EditOverride,
                                 SetEnvironment>;

    /// @brief 环境历史固定变更两端的 CPU 资源版本. 空端表示该端未配置环境, 两端都随历史项保活.
    struct EnvironmentChange
    {
        std::optional<SceneEnvironment> before;
        std::optional<SceneEnvironment> after;
    };

    /// @brief 一次已接受的变更. 历史保有受影响记录及其版本, 重放解析同一 Scene 内的稳定 ID.
    struct Change
    {
        SceneId scene;
        std::vector<EntityRecord> before;
        std::vector<EntityRecord> after;
        std::optional<EnvironmentChange> environment{};

        [[nodiscard]] bool Empty() const noexcept { return before.empty() && after.empty() && !environment; }
    };

    struct SceneState;

    /// @brief 静态领域 owner. 成功提交后提供一致的层级、世界数据和版本.
    class Scene final
    {
    public:
        using Creation = std::variant<std::unique_ptr<Scene>, Core::BoundedText>;
        using EditResult = std::variant<Change, Core::BoundedText>;

        /// @brief 校验并构造独立候选, 恢复记录的层级与 ID. 默认建立空 Scene.
        /// @return 成功值拥有新 Scene 身份; 拒绝或合作取消返回原因, 不交付部分候选.
        /// @note records 只在调用中借用. 模型, 图像和可选环境均使用已发布的 CPU 版本.
        [[nodiscard]] static Creation Create(std::span<const EntityRecord> records = {}, std::stop_token stop = {},
                                             std::optional<SceneEnvironment> environment = std::nullopt);
        ~Scene();
        Scene(const Scene &) = delete;
        Scene &operator=(const Scene &) = delete;

        [[nodiscard]] SceneId Id() const noexcept;
        [[nodiscard]] Revisions Versions() const noexcept;
        /// @brief 借用活动 Scene 固定的环境. 复制该值可使场景替换后的读者继续持有源.
        [[nodiscard]] const std::optional<SceneEnvironment> &Environment() const noexcept;
        [[nodiscard]] std::optional<EntityHandle> Find(std::string_view id) const noexcept;
        [[nodiscard]] std::optional<EntityView> Get(EntityHandle entity) const noexcept;
        /// @brief 从本 Scene 解析相机资产实例或自建相机. 缺少实体或组件时返回空.
        [[nodiscard]] std::optional<CameraState> FindCamera(std::string_view id) const noexcept;
        /// @brief 在 owner 线程同步访问已提交实体. 回调不得编辑本 Scene 或保留借用跨越下一次编辑.
        /// @note 回调异常向调用者传播, 遍历本身不修改 Scene; 接收方负责丢弃未完成的提取结果.
        void VisitEntities(const std::function<void(EntityHandle, const EntityView &)> &visit) const;
        /// @brief 复制当前创作记录及固定资产引用, 不含运行期 handle 和可再计算的世界数据.
        [[nodiscard]] std::vector<EntityRecord> Snapshot() const;
        /// @brief 文件操作可重新核对并复用这些只读模型, 引用独立于活动 Scene 的寿命.
        [[nodiscard]] std::vector<Asset::ModelVersion> PinnedModels() const;

        /// @brief 解析该实体源槽的材质. 模型图像借用至下一次编辑, 外部图像引用由结果保有.
        [[nodiscard]] std::optional<EffectiveMaterial> Material(EntityHandle entity, std::uint32_t slot) const noexcept;

        /// @brief 应用一个领域事务. 拒绝和同值操作保留活动状态及版本.
        /// @return 拥有型历史记录或有界错误. 普通分配失败可传播, 提交前回收所有临时状态.
        [[nodiscard]] EditResult Apply(const Command &command);

        /// @brief 为跨 owner 事务准备受影响的记录, 由 Replay 完成校验和应用.
        /// @note 不预留 ID 或修改 Scene. 计划须在活动状态仍满足原记录前提时应用.
        [[nodiscard]] EditResult Plan(const Command &command) const;

        /// @brief 将已有事务反向或正向应用. 只接受原 Scene 和同一历史前提.
        [[nodiscard]] std::optional<Core::BoundedText> Replay(const Change &change, bool undo);

    private:
        Scene(SceneId id, std::string generatedPrefix);
        std::unique_ptr<SceneState> m_State;
    };
}
