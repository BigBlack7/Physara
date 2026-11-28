#pragma once

/// @file
/// @brief Scene 实现中的 ECS 实体及事务共享入口. EnTT 类型由此文件隔离.
/// 命令计划借用活动节点, 提交先准备记录、关系与世界数据, 成功后才共同发布及推进版本.

#include "Scene.hpp"

#include <map>
#include <utility>

#include <entt/entt.hpp>

namespace Physara::Scene
{
    /// @brief 地址稳定的 ECS 组件. 关系只借用同一 registry 中仍存在的节点.
    struct SceneNode
    {
        static constexpr bool in_place_delete = true; ///< EnTT 组件删除协议, 保证其他节点地址稳定.

        entt::entity native;
        EntityHandle handle;
        EntityRecord record;
        SceneNode *parent{}; ///< 创作层级, 与 instance 来源关系分别维护.
        std::vector<SceneNode *> children;
        SceneNode *instance{};
        std::vector<SceneNode *> sourceNodes; ///< 实例根的反向来源索引, 覆盖已移出其层级的节点.
        const Asset::ModelNode *sourceNode{};
        WorldState world;
        // 保存各自最近一次变更的内容代. 覆盖版本与 record.overrides 按同一源槽顺序发布.
        std::uint64_t transformRevision{};
        std::uint64_t lightRevision{};
        std::vector<std::uint64_t> overrideRevisions;

        SceneNode(entt::entity native, EntityHandle handle, EntityId id);
        [[nodiscard]] const Asset::ModelVersion *Model() const noexcept;
        [[nodiscard]] EntityView Read() const noexcept;
    };

    /// @brief Scene 的唯一可写存储. ID 索引和 ECS 组件在一次提交中更新, 世界数据由创作值派生.
    struct SceneState
    {
        SceneState(SceneId id, std::string prefix) : id(id), generatedPrefix(std::move(prefix)) {}
        SceneId id;
        std::string generatedPrefix;
        Revisions revisions;
        std::optional<SceneEnvironment> environment;
        std::uint64_t lastGeneration{};
        std::uint64_t lastEntityId{};
        // 明细只覆盖上次分发之后的事务. 结构换代清除旧键, 由各槽在安全复用时重建.
        std::uint64_t deliveredRenderContent{};
        std::set<RenderChange> renderChanges;
        entt::registry registry;
        std::map<EntityId, SceneNode *, std::less<>> entities;

        [[nodiscard]] SceneNode *Find(std::string_view entityId) const noexcept;
        /// @brief 先核对 Scene 与 ECS 槽存活, 再比较独立的非回绕 generation, 拒绝删除前的 handle.
        [[nodiscard]] const SceneNode *Find(EntityHandle handle) const noexcept;

        /// @brief 预检变更集和受影响的世界数据, 一次提交关系、记录及版号.
        [[nodiscard]] std::optional<Core::BoundedText> Commit(const Change &change, std::stop_token stop = {});
    };

    /// @brief 领域输入的值域检查, 只返回是否合法, 不修正调用者的创作值.
    [[nodiscard]] bool ValidLocal(const LocalTransform &value) noexcept;
    /// @brief 以实体和字段补齐定位并抛出有界错误, 由事务入口统一转为失败值.
    [[noreturn]] void Reject(std::string_view id, std::string_view field, std::string_view reason);
    /// @brief 定位到固定模型的源 Primitive, 保留该覆盖字段或纹理槽的拒绝原因.
    [[noreturn]] void RejectPrimitiveOverride(std::string_view id, std::uint32_t slot, std::string_view reason);
    /// @brief 检查支持的投影参数, 包括透视相机的可空远面.
    [[nodiscard]] bool ValidCamera(const Camera &camera) noexcept;
    /// @brief 检查灯的线性色值, 光度, 米制范围和锥角. 合法时返回空指针, 失败正文为静态字符串.
    [[nodiscard]] const char *LightError(const Light &light) noexcept;
    /// @brief 候选文档与编辑共用的环境源及创作字段校验.
    [[nodiscard]] std::optional<Core::BoundedText> CheckEnvironment(const SceneEnvironment &environment) noexcept;
    /// @brief 将已导入的矩阵或 TRS 转为 Scene local; 无法表示为有限 TRS 时返回空.
    [[nodiscard]] std::optional<LocalTransform> ImportLocal(const Asset::ModelTransform &value) noexcept;
    /// @brief 反解新父级下可保持原 world 的 local. 空父级表示根.
    /// @param preservePose 同时要求保留刚体朝向, 任一要求无法表示为有限 TRS 时返回空.
    [[nodiscard]] std::optional<LocalTransform> RelativeLocal(const WorldState &world, const WorldState *parent,
                                                              bool preservePose) noexcept;
    /// @brief 合成 world、刚体位姿、绕序及可选网格范围, 非有限结果返回空.
    [[nodiscard]] std::optional<WorldState> ComputeWorld(const LocalTransform &local, const WorldState *parent,
                                                         const Asset::ModelMesh *mesh);

    /// @brief 源 Primitive 槽在导入时保持连续, 越界返回空, 不以材质或顶点下标定位.
    [[nodiscard]] const Asset::ModelPrimitive *FindPrimitive(const Asset::ModelMesh &mesh,
                                                              std::uint32_t slot) noexcept;
    /// @brief 按固定源叠加语义检查材质值, 纹理用途和 UV, 返回首个拒绝原因.
    /// @param previous 同一固定模型及 Primitive 的已提交覆盖, 用于复用输入相同的切线校验.
    [[nodiscard]] std::optional<Core::BoundedText> ValidateOverride(const Asset::ModelRecord &model,
                                                                    const Asset::ModelPrimitive &primitive,
                                                                    const PrimitiveOverride &value,
                                                                    const PrimitiveOverride *previous = nullptr);
}
