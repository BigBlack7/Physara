#include "SceneState.hpp"

/// @file
/// @brief Scene 公开入口将类型化命令转为最小记录改动, 由同一事务入口安装及重放.

#include <algorithm>
#include <atomic>
#include <cassert>
#include <limits>
#include <unordered_set>
#include <utility>

namespace Physara::Scene
{
    /// @brief 场景候选可由后台线程建立, 身份单独分配且禁止回绕.
    static SceneId AllocateSceneId() noexcept
    {
        static std::atomic<std::uint64_t> last{0};
        auto previous = last.load(std::memory_order_relaxed);
        while (previous != std::numeric_limits<std::uint64_t>::max())
        {
            if (last.compare_exchange_weak(previous, previous + 1, std::memory_order_relaxed))
            {
                return {0, previous + 1};
            }
        }
        return {};
    }

    void Reject(std::string_view id, std::string_view field, std::string_view reason)
    {
        Core::BoundedText error("Entity ");
        error.Append(id);
        error.Append(".");
        error.Append(field);
        error.Append(": ");
        error.Append(reason);
        throw error;
    }

    SceneNode::SceneNode(entt::entity token, EntityHandle identity, EntityId id)
        : native(token), handle(identity)
    {
        record.id = std::move(id);
    }

    const Asset::ModelVersion *SceneNode::Model() const noexcept
    {
        if (instance)
        {
            return &instance->record.model;
        }
        return record.model ? &record.model : nullptr;
    }

    SceneNode *SceneState::Find(std::string_view entityId) const noexcept
    {
        const auto found = entities.find(entityId);
        return found == entities.end() ? nullptr : found->second;
    }

    const SceneNode *SceneState::Find(EntityHandle handle) const noexcept
    {
        using Traits = entt::entt_traits<entt::entity>;
        if (handle.scene != id || handle.generation == 0 || handle.index >= Traits::entity_mask)
        {
            return nullptr;
        }
        const auto token = Traits::construct(handle.index, registry.current(static_cast<entt::entity>(handle.index)));
        if (!registry.valid(token))
        {
            return nullptr;
        }
        const auto &node = registry.get<SceneNode>(token);
        return node.handle.generation == handle.generation ? &node : nullptr;
    }

    /// @brief 为新 ID 选取与文件中全部 ID 不重叠的前缀. 任意原生文件 ID 均可保留.
    static std::string SelectGeneratedPrefix(std::span<const EntityRecord> records)
    {
        std::unordered_set<std::string_view> occupied;
        for (const auto &record : records)
        {
            const std::string_view id = record.id;
            if (const auto separator = id.rfind('-'); separator != id.npos)
            {
                occupied.insert(id.substr(0, separator + 1));
            }
        }
        std::string prefix("entity-");
        for (std::size_t i = 1; occupied.contains(prefix); ++i)
        {
            prefix = "entity-" + std::to_string(i) + "-";
        }
        return prefix;
    }

    Scene::Scene(SceneId id, std::string generatedPrefix)
        : m_State(std::make_unique<SceneState>(id, std::move(generatedPrefix)))
    {
    }

    Scene::~Scene() = default;

    Scene::Creation Scene::Create(std::span<const EntityRecord> records, std::stop_token stop)
    {
        const auto id = AllocateSceneId();
        if (!id.IsValid())
        {
            return Core::BoundedText("Scene identity exhausted");
        }
        std::unique_ptr<Scene> candidate(new Scene(id, SelectGeneratedPrefix(records)));
        if (auto error = candidate->m_State->Commit({id, {}, {records.begin(), records.end()}}, stop))
        {
            return *error;
        }
        return candidate;
    }

    SceneId Scene::Id() const noexcept { return m_State->id; }
    Revisions Scene::Versions() const noexcept { return m_State->revisions; }

    std::optional<EntityHandle> Scene::Find(std::string_view id) const noexcept
    {
        const auto *node = m_State->Find(id);
        return node ? std::optional(node->handle) : std::nullopt;
    }

    std::optional<EntityView> Scene::Get(EntityHandle entity) const noexcept
    {
        const auto *node = m_State->Find(entity);
        return node ? std::optional(EntityView{node->record, node->world, node->Model(), node->sourceNode}) :
            std::nullopt;
    }

    std::optional<CameraState> Scene::FindCamera(std::string_view id) const noexcept
    {
        const auto *node = m_State->Find(id);
        if (!node || !node->record.properties.camera)
        {
            return std::nullopt;
        }
        // 位姿和投影来自同次已接受状态, 按值交付后不再借用 ECS 组件.
        return CameraState{node->handle, node->world.pose, *node->record.properties.camera};
    }

    void Scene::VisitEntities(const std::function<void(EntityHandle, const EntityView &)> &visit) const
    {
        for (const auto &[id, node] : m_State->entities)
        {
            visit(node->handle, EntityView{node->record, node->world, node->Model(), node->sourceNode});
        }
    }

    std::vector<EntityRecord> Scene::Snapshot() const
    {
        std::vector<EntityRecord> records;
        records.reserve(m_State->entities.size());
        for (const auto &[id, node] : m_State->entities)
        {
            records.push_back(node->record);
        }
        return records;
    }

    std::vector<Asset::ModelVersion> Scene::PinnedModels() const
    {
        std::vector<Asset::ModelVersion> versions;
        std::unordered_set<const Asset::ModelRecord *> seen;
        for (const auto &[id, node] : m_State->entities)
        {
            const auto &model = node->record.model;
            if (model && seen.insert(model.get()).second)
            {
                versions.push_back(model);
            }
        }
        return versions;
    }

    /// @brief 从当前实体取得固定版本内的网格, 分离创作组件是否存在与源记录.
    static const Asset::ModelMesh *Mesh(const SceneNode &node) noexcept
    {
        if (!node.record.properties.castsShadow)
        {
            return nullptr;
        }
        assert(node.instance && node.sourceNode && node.sourceNode->mesh);
        return &node.instance->record.model->meshes[*node.sourceNode->mesh];
    }

    std::optional<EffectiveMaterial> Scene::Material(EntityHandle entity, std::uint32_t slot) const noexcept
    {
        const auto *node = m_State->Find(entity);
        const auto *mesh = node ? Mesh(*node) : nullptr;
        const auto *primitive = mesh ? FindPrimitive(*mesh, slot) : nullptr;
        if (!primitive)
        {
            return std::nullopt;
        }
        const auto &values = node->record.overrides;
        const auto found = std::ranges::lower_bound(values, slot, {}, &PrimitiveOverride::primitiveSlot);
        const auto *value = found != values.end() && found->primitiveSlot == slot ? &*found : nullptr;
        return ResolveMaterial(*node->instance->record.model, *primitive, value);
    }

    /// @brief 与活动组件无共享写入, 命令准备完成后才交给 SceneState 提交.
    class CommandPlan final
    {
    public:
        explicit CommandPlan(const SceneState &state)
            : m_State(state), m_NextId(state.lastEntityId), m_Change{state.id, {}, {}} {}

        /// @brief 生成受影响记录, 值域, 关系与世界数据的最终预检留给提交入口.
        [[nodiscard]] Change Build(const Command &command)
        {
            std::visit([this](const auto &value) { Prepare(value); }, command);
            return std::move(m_Change);
        }

    private:
        /// @brief 在复制记录前核对运行期身份, 旧 Scene 或已退役 generation 均拒绝.
        [[nodiscard]] const SceneNode &Target(EntityHandle target) const
        {
            const auto *node = m_State.Find(target);
            if (!node)
            {
                throw Core::BoundedText("Scene edit targets an expired entity");
            }
            return *node;
        }

        /// @brief 在本计划内产生新稳定 ID, 只有事务提交后才推进 owner 的已用上界.
        [[nodiscard]] EntityId Allocate()
        {
            if (!Core::AdvanceGeneration(m_NextId))
            {
                throw Core::BoundedText("Persistent entity identity exhausted");
            }
            return m_State.generatedPrefix + std::to_string(m_NextId);
        }

        /// @brief 只记录实际不同的前后值, 同值命令不产生历史或 revision.
        void Replace(const SceneNode &node, EntityRecord after)
        {
            if (node.record != after)
            {
                m_Change.before.push_back(node.record);
                m_Change.after.push_back(std::move(after));
            }
        }

        /// @brief 普通实体只复制创作值和经身份校验的父引用, 不隐式绑定模型.
        void Prepare(const CreateEntity &command)
        {
            EntityRecord record;
            record.id = Allocate();
            if (command.parent)
            {
                record.parent = Target(*command.parent).record.id;
            }
            record.properties = command.properties;
            m_Change.after.push_back(std::move(record));
        }

        /// @brief 先保有固定模型, 再按导入器的父先子后顺序生成独立实体及来源映射.
        void Prepare(const Instantiate &command)
        {
            const auto &model = command.model;
            if (!model || !model->asset.IsValid() || model->revision.value == 0)
            {
                throw Core::BoundedText("Instancing requires a published CPU model version");
            }
            EntityRecord root;
            root.id = Allocate();
            if (command.parent)
            {
                root.parent = Target(*command.parent).record.id;
            }
            root.properties.name = command.name;
            root.model = model;
            const auto instanceId = root.id;
            m_Change.after.reserve(model->nodes.size() + 1);
            m_Change.after.push_back(std::move(root));

            // 导入器保证根和内部下标属于所选静态 scene, 这里生成创作身份和可编辑值.
            std::vector<std::size_t> parents(model->nodes.size(), 0);
            for (std::size_t i = 0; i < model->nodes.size(); ++i)
            {
                for (const auto child : model->nodes[i].children)
                {
                    parents[child] = i + 1;
                }
            }
            for (std::size_t i = 0; i < model->nodes.size(); ++i)
            {
                const auto &source = model->nodes[i];
                EntityRecord node;
                node.id = Allocate();
                assert(parents[i] < m_Change.after.size());
                node.parent = m_Change.after[parents[i]].id;
                node.source = SourceRef{instanceId, source.sourceIndex};
                node.properties.name = source.name;
                const auto local = ImportLocal(source.local);
                if (!local)
                {
                    Reject(node.id, "local", "Source transform cannot be represented as TRS");
                }
                node.properties.local = *local;
                if (source.mesh)
                {
                    node.properties.castsShadow = true;
                }
                if (source.camera)
                {
                    node.properties.camera = model->cameras[*source.camera].projection;
                }
                if (source.light)
                {
                    const auto &light = model->lights[*source.light];
                    node.properties.light = Light{light.kind, light.color, light.intensity, light.range,
                                                  light.innerConeAngle, light.outerConeAngle};
                }
                m_Change.after.push_back(std::move(node));
            }
        }

        /// @brief 同时沿层级和实例来源收集删除闭包, 去重后交提交入口一次退役.
        void Prepare(const EraseSubtree &command)
        {
            std::unordered_set<const SceneNode *> visited;
            std::vector<const SceneNode *> pending{&Target(command.entity)};
            while (!pending.empty())
            {
                const auto *node = pending.back();
                pending.pop_back();
                if (visited.insert(node).second)
                {
                    m_Change.before.push_back(node->record);
                    pending.insert(pending.end(), node->children.begin(), node->children.end());
                    pending.insert(pending.end(), node->sourceNodes.begin(), node->sourceNodes.end());
                }
            }
        }

        /// @brief 检查重挂根是否还需约束 local 旋转. 保住根的刚体朝向即可保住其后代的朝向.
        [[nodiscard]] static bool HasOrientedComponent(const SceneNode &root)
        {
            std::vector<const SceneNode *> pending{&root};
            while (!pending.empty())
            {
                const auto *node = pending.back();
                pending.pop_back();
                const auto &props = node->record.properties;
                if (props.camera || (props.light && props.light->type != Asset::ModelLightKind::Point))
                {
                    return true;
                }
                pending.insert(pending.end(), node->children.begin(), node->children.end());
            }
            return false;
        }

        /// @brief 以当前已接受的 world 推导候选 local, 环及最终组合范围在提交时检查.
        void Prepare(const Reparent &command)
        {
            const auto &node = Target(command.entity);
            const auto *parent = command.parent ? &Target(*command.parent) : nullptr;
            if (parent == node.parent)
            {
                return;
            }
            auto after = node.record;
            after.parent = parent ? std::optional(parent->record.id) : std::nullopt;
            if (command.policy == ReparentPolicy::KeepWorld)
            {
                // 约束覆盖实际层级子树. 无相机或定向灯时仍允许原有的几何 TRS 分解自由度.
                const bool preservePose = HasOrientedComponent(node);
                const auto local = RelativeLocal(node.world, parent ? &parent->world : nullptr, preservePose);
                if (!local)
                {
                    Reject(after.id, "parent", "World pose cannot be preserved with a local TRS");
                }
                after.properties.local = *local;
            }
            Replace(node, std::move(after));
        }

        /// @brief 替换创作值; 移除 MeshRenderer 后同步清掉依附它的实例覆盖.
        void Prepare(const EditProperties &command)
        {
            const auto &node = Target(command.entity);
            auto after = node.record;
            after.properties = command.properties;
            if (!after.properties.castsShadow)
            {
                after.overrides.clear();
            }
            Replace(node, std::move(after));
        }

        /// @brief 按源槽替换稀疏记录, 空纹理和全空 Primitive 修改归并为清除覆盖.
        void Prepare(const EditOverride &command)
        {
            const auto &node = Target(command.entity);
            const auto *mesh = Mesh(node);
            if (!mesh || !FindPrimitive(*mesh, command.primitiveSlot) ||
                (command.value && command.value->primitiveSlot != command.primitiveSlot))
            {
                Reject(node.record.id, "primitiveOverrides", "Unknown Primitive slot");
            }
            auto after = node.record;
            auto found = std::ranges::lower_bound(after.overrides, command.primitiveSlot, {},
                                                  &PrimitiveOverride::primitiveSlot);
            const bool exists = found != after.overrides.end() && found->primitiveSlot == command.primitiveSlot;
            auto value = command.value;
            if (value)
            {
                for (auto &texture : value->textures)
                {
                    if (texture && *texture == TextureOverride{})
                    {
                        texture.reset();
                    }
                }
                if (value->material == MaterialOverride{} &&
                    std::ranges::none_of(value->textures, [](const auto &texture) { return texture.has_value(); }))
                {
                    value.reset();
                }
            }
            if (value)
            {
                if (exists)
                {
                    *found = std::move(*value);
                }
                else
                {
                    after.overrides.insert(found, std::move(*value));
                }
            }
            else if (exists)
            {
                after.overrides.erase(found);
            }
            Replace(node, std::move(after));
        }

        const SceneState &m_State;
        std::uint64_t m_NextId;
        Change m_Change;
    };

    Scene::EditResult Scene::Apply(const Command &command)
    {
        auto planned = Plan(command);
        if (const auto *failure = std::get_if<Core::BoundedText>(&planned))
        {
            return *failure;
        }
        if (const auto error = m_State->Commit(std::get<Change>(planned)))
        {
            return *error;
        }
        return planned;
    }

    Scene::EditResult Scene::Plan(const Command &command) const
    {
        try
        {
            return CommandPlan(*m_State).Build(command);
        }
        catch (const Core::BoundedText &error)
        {
            return error;
        }
    }

    std::optional<Core::BoundedText> Scene::Replay(const Change &change, bool undo)
    {
        if (change.scene != Id())
        {
            return Core::BoundedText("Scene history belongs to an expired scene");
        }
        const auto &expected = undo ? change.after : change.before;
        for (const auto &record : expected)
        {
            const auto *node = m_State->Find(record.id);
            if (!node || node->record != record)
            {
                return Core::BoundedText("Scene history no longer matches its target");
            }
        }
        return undo ? m_State->Commit({change.scene, change.after, change.before}) : m_State->Commit(change);
    }
}
