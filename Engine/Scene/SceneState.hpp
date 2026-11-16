#pragma once

/// @file
/// @brief Scene 实现中的 ECS 实体及事务共享入口. EnTT 类型由此文件隔离.

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

        SceneNode(entt::entity native, EntityHandle handle, EntityId id);
        [[nodiscard]] const Asset::ModelVersion *Model() const noexcept;
    };

    /// @brief Scene 的唯一可写存储. ID 索引和 ECS 组件在一次提交中更新, 世界数据由创作值派生.
    struct SceneState
    {
        SceneState(SceneId id, std::string prefix) : id(id), generatedPrefix(std::move(prefix)) {}
        SceneId id;
        std::string generatedPrefix;
        Revisions revisions;
        std::uint64_t lastGeneration{};
        std::uint64_t lastEntityId{};
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
    /// @brief 检查支持的投影及物理灯参数, 包括可空远面和各灯类型的字段约束.
    [[nodiscard]] bool ValidCamera(const Camera &camera) noexcept;
    [[nodiscard]] bool ValidLight(const Light &light) noexcept;
    /// @brief 将已导入的矩阵或 TRS 转为 Scene local; 无法表示为有限 TRS 时返回空.
    [[nodiscard]] std::optional<LocalTransform> ImportLocal(const Asset::ModelTransform &value) noexcept;
    /// @brief 从当前世界数据反解新父级下的 local. 空父级表示根, 奇异或含 shear 的结果被拒绝.
    [[nodiscard]] std::optional<LocalTransform> RelativeLocal(const WorldState &world,
                                                              const WorldState *parent) noexcept;
    /// @brief 合成 world, 反射方向及可选网格范围, 非有限结果返回空; 输入只在调用中借用.
    [[nodiscard]] std::optional<WorldState> ComputeWorld(const LocalTransform &local, const WorldState *parent,
                                                         const Asset::ModelMesh *mesh);

    /// @brief 源 Primitive 槽在导入时保持连续, 越界返回空, 不以材质或顶点下标定位.
    [[nodiscard]] const Asset::ModelPrimitive *FindPrimitive(const Asset::ModelMesh &mesh,
                                                              std::uint32_t slot) noexcept;
    /// @brief 按固定源叠加语义检查材质值, 纹理用途和 UV, 返回首个拒绝原因.
    [[nodiscard]] std::optional<Core::BoundedText> ValidateOverride(const Asset::ModelRecord &model,
                                                                    const Asset::ModelPrimitive &primitive,
                                                                    const PrimitiveOverride &value);
    /// @brief 解析已验证的稀疏覆盖. 模型图像仍借用 model, 外置图像由返回值保有.
    [[nodiscard]] EffectiveMaterial ResolveMaterial(const Asset::ModelRecord &model,
                                                     const Asset::ModelPrimitive &primitive,
                                                     const PrimitiveOverride *value) noexcept;
}
