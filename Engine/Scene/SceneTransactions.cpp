#include "SceneState.hpp"

/// @file
/// @brief 在私有记录和链接上预检事务. 世界更新也在预检阶段完成, 发布只移动已经准备好的值.

#include <algorithm>
#include <cassert>
#include <charconv>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace Physara::Scene
{
    /// @brief 单次编辑的临时拥有者. 未提交的 ECS 槽在退出时回收, 活动记录尚未被改写.
    class StagedChange final
    {
    public:
        StagedChange(SceneState &state, std::stop_token stop)
            : m_State(state), m_Stop(stop), m_Generation(state.lastGeneration), m_Revision(state.revisions),
              m_LastId(state.lastEntityId) {}

        ~StagedChange()
        {
            for (const auto token : m_Created)
            {
                m_State.registry.destroy(token);
            }
        }

        /// @brief 在临时存储完成引用, 层级, 世界数据和版号预检, 失败不改写活动记录.
        void Prepare(const Change &change)
        {
            CheckStop();
            if (change.environment)
            {
                const auto &environment = *change.environment;
                if (environment.before != m_State.environment)
                {
                    throw Core::BoundedText("Environment history no longer matches the scene");
                }
                if (environment.after)
                {
                    if (const auto error = CheckEnvironment(*environment.after))
                    {
                        throw *error;
                    }
                }
                if (environment.after != environment.before)
                {
                    // 地址标记本次需要发布环境. 即使 after 为空也要保留这个地址, 以便 Publish 移除旧源.
                    m_Environment = &environment.after;
                    m_RenderChanged = true;
                }
            }
            if (change.before.empty() && change.after.empty() && !m_Environment)
            {
                return;
            }
            TrackRecords(change);
            for (auto &[id, entry] : m_Entries)
            {
                CheckStop();
                if (entry.after)
                {
                    Resolve(entry);
                }
                Classify(entry);
                PlanLinks(entry);
            }
            ValidateHierarchy();
            PrepareLinks();
            PrepareWorld();
            if (!Core::AdvanceGeneration(m_Revision.document) ||
                (m_RenderChanged && !Core::AdvanceGeneration(m_Revision.renderContent)) ||
                (m_CameraChanged && !Core::AdvanceGeneration(m_Revision.camera)) ||
                (!m_World.empty() && !Core::AdvanceGeneration(m_Revision.transforms)))
            {
                throw Core::BoundedText("Scene revision exhausted");
            }
            CheckStop();
        }

        /// @brief 消费准备好的记录和容器, 不再分配或执行可失败的领域校验.
        void Publish() noexcept
        {
            // 先安装活节点的属性和边, 等所有引用切换后再退休删除的 ECS 槽.
            for (auto &[id, entry] : m_Entries)
            {
                if (entry.after)
                {
                    entry.node->record = std::move(*entry.after);
                    entry.node->parent = entry.parent;
                    entry.node->instance = entry.instance;
                    entry.node->sourceNode = entry.source;
                }
            }
            for (auto &[node, links] : m_Links)
            {
                node->children = std::move(links.children);
                node->sourceNodes = std::move(links.sources);
            }
            for (auto &update : m_World)
            {
                update.node->world = std::move(update.world);
            }
            m_State.entities.merge(m_NewEntries);
            for (const auto &[id, entry] : m_Entries)
            {
                if (!entry.after)
                {
                    m_State.entities.erase(id);
                    m_State.registry.destroy(entry.node->native);
                }
            }
            m_State.lastEntityId = m_LastId;
            m_State.lastGeneration = m_Generation;
            if (m_Environment)
            {
                m_State.environment = *m_Environment;
            }
            m_State.revisions = m_Revision;
            m_Created.clear();
        }

    private:
        /// @brief 本次事务对一个实体的替换, after 为空表示删除; 新槽尚未进入公开 ID 索引.
        struct Entry
        {
            SceneNode *node{};
            std::optional<EntityRecord> after;
            SceneNode *parent{};
            SceneNode *instance{};
            const Asset::ModelNode *source{};
            bool created{};
        };

        /// @brief 仅为受影响 owner 准备的新关系表, 发布前仍使用活动表解释旧状态.
        struct Links
        {
            std::vector<SceneNode *> children;
            std::vector<SceneNode *> sources;
        };

        /// @brief 发布前独立持有的一项派生数据, 包括完整矩阵、刚体位姿及网格范围.
        struct WorldUpdate
        {
            SceneNode *node;
            WorldState world;
        };

        /// @brief 候选构建在阶段和遍历边界合作取消, 由同一事务析构路径释放临时槽.
        void CheckStop() const
        {
            if (m_Stop.stop_requested())
            {
                throw Core::BoundedText("Scene validation was cancelled");
            }
        }

        /// @brief 只覆盖领域变更涉及的字段和实体, 冷路径从持久 ID 定位.
        [[nodiscard]] const Entry *Proposed(const SceneNode *node) const noexcept
        {
            const auto found = m_Entries.find(node->record.id);
            return found == m_Entries.end() ? nullptr : &found->second;
        }

        [[nodiscard]] SceneNode *Find(std::string_view id) const noexcept
        {
            const auto found = m_Entries.find(id);
            if (found != m_Entries.end())
            {
                return found->second.after ? found->second.node : nullptr;
            }
            return m_State.Find(id);
        }

        [[nodiscard]] bool Alive(const SceneNode *node) const noexcept
        {
            const auto *entry = Proposed(node);
            return !entry || entry->after.has_value();
        }

        [[nodiscard]] SceneNode *Parent(const SceneNode *node) const noexcept
        {
            const auto *entry = Proposed(node);
            return entry ? entry->parent : node->parent;
        }

        [[nodiscard]] SceneNode *Instance(const SceneNode *node) const noexcept
        {
            const auto *entry = Proposed(node);
            return entry ? entry->instance : node->instance;
        }

        [[nodiscard]] const EntityRecord &Record(const SceneNode *node) const noexcept
        {
            const auto *entry = Proposed(node);
            return entry ? *entry->after : node->record;
        }

        /// @brief 记录此 Scene 自动 ID 的已使用上界. 删除后也保持退役.
        void NoteId(std::string_view id) noexcept
        {
            if (id.starts_with(m_State.generatedPrefix))
            {
                id.remove_prefix(m_State.generatedPrefix.size());
                std::uint64_t number{};
                const auto result = std::from_chars(id.data(), id.data() + id.size(), number);
                if (result.ec == std::errc{} && result.ptr == id.data() + id.size())
                {
                    m_LastId = std::max(m_LastId, number);
                }
            }
        }

        /// @brief 合并前后记录并检查身份冲突, 预留新 ECS 槽和稳定 ID 索引节点.
        void TrackRecords(const Change &change)
        {
            for (const auto &record : change.before)
            {
                auto *node = m_State.Find(record.id);
                auto [found, first] = m_Entries.try_emplace(record.id);
                if (!node || !first)
                {
                    Reject(record.id, "id", "Missing or repeated edit target");
                }
                found->second.node = node;
            }
            std::size_t newCount = 0;
            for (const auto &record : change.after)
            {
                CheckStop();
                auto [found, fresh] = m_Entries.try_emplace(record.id);
                if (record.id.empty() || found->second.after || (fresh && m_State.Find(record.id)))
                {
                    Reject(record.id, "id", "Empty or duplicate entity identity");
                }
                found->second.after = record;
                newCount += static_cast<std::size_t>(fresh);
            }
            const auto available = static_cast<std::size_t>(entt::entt_traits<entt::entity>::entity_mask) -
                m_State.entities.size();
            if (newCount > available || newCount > std::numeric_limits<std::uint64_t>::max() - m_Generation)
            {
                throw Core::BoundedText("Scene entity capacity exhausted");
            }
            m_Created.reserve(newCount);
            for (auto &[id, entry] : m_Entries)
            {
                if (!entry.node)
                {
                    const auto token = m_State.registry.create();
                    m_Created.push_back(token);
                    const auto handle = EntityHandle{m_State.id, entt::to_entity(token), ++m_Generation};
                    entry.node = &m_State.registry.emplace<SceneNode>(token, token, handle, id);
                    entry.created = true;
                    m_NewEntries.emplace(id, entry.node);
                    NoteId(id);
                }
            }
        }

        /// @brief 每个参与新绑定的模型只索引一次原始下标. 常规编辑复用已经绑定的节点.
        [[nodiscard]] const Asset::ModelNode *SourceNode(const Asset::ModelRecord &model, std::uint32_t index)
        {
            auto [table, first] = m_Sources.try_emplace(&model);
            if (first)
            {
                table->second.reserve(model.nodes.size());
                for (const auto &node : model.nodes)
                {
                    table->second.emplace(node.sourceIndex, &node);
                }
            }
            const auto found = table->second.find(index);
            return found == table->second.end() ? nullptr : found->second;
        }

        /// @brief 按事务终态解析父级和固定来源, 已存在实体不允许隐式换绑模型版本.
        void Resolve(Entry &entry)
        {
            const auto &next = *entry.after;
            const auto *prev = entry.created ? nullptr : &entry.node->record;
            if (next.parent)
            {
                entry.parent = Find(*next.parent);
                if (!entry.parent)
                {
                    Reject(next.id, "parent", "Missing parent");
                }
            }
            if (prev && (next.source != prev->source || next.model != prev->model))
            {
                Reject(next.id, "sourceRef", "An existing entity keeps its source binding");
            }
            if (next.model && (next.source || !next.model->asset.IsValid() || next.model->revision.value == 0))
            {
                Reject(next.id, "modelInstance", "Expected a published model on an instance root");
            }
            if (next.source)
            {
                entry.instance = Find(next.source->instanceEntityId);
                if (!entry.instance || !Record(entry.instance).model)
                {
                    Reject(next.id, "sourceRef", "Missing model instance");
                }
                entry.source = prev ? entry.node->sourceNode :
                    SourceNode(*Record(entry.instance).model, next.source->node);
                if (!entry.source)
                {
                    Reject(next.id, "sourceRef.node", "Node is outside the active model version");
                }
            }
            ValidateProperties(next, prev, entry.source);
            if (!prev || next.overrides != prev->overrides)
            {
                ValidateOverrides(entry, next);
            }
        }

        /// @brief 仅重验变化的数值, 同时确认网格组件及覆盖确有可解析的源网格.
        static void ValidateProperties(const EntityRecord &next, const EntityRecord *prev,
                                       const Asset::ModelNode *source)
        {
            const auto &value = next.properties;
            if ((!prev || value.local != prev->properties.local) && !ValidLocal(value.local))
            {
                Reject(next.id, "local", "Invalid local TRS");
            }
            if (value.camera && (!prev || value.camera != prev->properties.camera) && !ValidCamera(*value.camera))
            {
                Reject(next.id, "camera", "Invalid camera parameters");
            }
            if (value.light && (!prev || value.light != prev->properties.light))
            {
                if (const auto *error = LightError(*value.light))
                {
                    Reject(next.id, "light", error);
                }
            }
            if (value.castsShadow && (!source || !source->mesh))
            {
                Reject(next.id, "meshRenderer", "Source node has no mesh");
            }
            if (!value.castsShadow && !next.overrides.empty())
            {
                Reject(next.id, "primitiveOverrides", "Missing MeshRenderer");
            }
        }

        /// @brief 检查覆盖的源槽顺序与有效采样语义, 不重新解码已发布的模型或图像.
        void ValidateOverrides(const Entry &entry, const EntityRecord &next) const
        {
            std::optional<std::uint32_t> last;
            for (const auto &value : next.overrides)
            {
                const auto &model = *Record(entry.instance).model;
                const auto &mesh = model.meshes[*entry.source->mesh];
                const auto *primitive = FindPrimitive(mesh, value.primitiveSlot);
                if (!primitive || (last && value.primitiveSlot <= *last))
                {
                    RejectPrimitiveOverride(next.id, value.primitiveSlot, "Invalid or repeated Primitive slot");
                }
                if (auto error = ValidateOverride(model, *primitive, value))
                {
                    RejectPrimitiveOverride(next.id, value.primitiveSlot, error->View());
                }
                last = value.primitiveSlot;
            }
        }

        [[nodiscard]] static bool Renderable(const EntityProperties &props) noexcept
        {
            return props.castsShadow.has_value() || props.light.has_value();
        }

        /// @brief 将字段变化分配给文档, 内容和相机版本, 需要世界求值的实体登记为脏根.
        void Classify(const Entry &entry)
        {
            const auto *before = entry.created ? nullptr : &entry.node->record;
            const auto *after = entry.after ? &*entry.after : nullptr;
            if (!before || !after)
            {
                const auto &value = before ? before->properties : after->properties;
                m_RenderChanged |= Renderable(value);
                m_CameraChanged |= value.camera.has_value();
                if (after)
                {
                    m_Dirty.insert(entry.node);
                }
                return;
            }
            const auto &a = before->properties;
            const auto &b = after->properties;
            m_RenderChanged |= a.castsShadow != b.castsShadow || a.light != b.light ||
                before->overrides != after->overrides || (a.visible != b.visible && Renderable(b));
            m_CameraChanged |= a.camera != b.camera;
            if (entry.parent != entry.node->parent || a.local != b.local ||
                a.castsShadow.has_value() != b.castsShadow.has_value())
            {
                m_Dirty.insert(entry.node);
            }
        }

        /// @brief 标记失去链接的旧 owner, 并把新增链接暂存到新 owner 的候选表.
        void PlanLinks(const Entry &entry)
        {
            auto *node = entry.node;
            if (node->parent != entry.parent)
            {
                if (node->parent)
                {
                    m_Links.try_emplace(node->parent);
                }
                if (entry.parent)
                {
                    m_Links[entry.parent].children.push_back(node);
                }
            }
            if (node->instance != entry.instance)
            {
                if (node->instance)
                {
                    m_Links.try_emplace(node->instance);
                }
                if (entry.instance)
                {
                    m_Links[entry.instance].sources.push_back(node);
                }
            }
        }

        /// @brief 从变化的边查找祖先, 共享已检查的路径, 同一事务可接入任意记录顺序.
        void ValidateHierarchy() const
        {
            std::unordered_map<const SceneNode *, bool> complete;
            std::vector<const SceneNode *> path;
            for (const auto &[id, entry] : m_Entries)
            {
                if (!entry.after || entry.parent == entry.node->parent)
                {
                    continue;
                }
                for (const auto *node = entry.node; node; node = Parent(node))
                {
                    const auto [flag, fresh] = complete.emplace(node, false);
                    if (!fresh)
                    {
                        if (!flag->second)
                        {
                            Reject(id, "parent", "Hierarchy contains a cycle");
                        }
                        break;
                    }
                    path.push_back(node);
                }
                for (const auto *node : path)
                {
                    complete.find(node)->second = true;
                }
                path.clear();
            }
        }

        /// @brief 合并仍存活的旧链接, 拒绝同实例重复源节点和删除后残留的引用.
        void PrepareLinks()
        {
            // 待更新的链接只遍历一次. 原关系仍用于所有未触及的实体.
            for (auto &[node, links] : m_Links)
            {
                for (auto *child : node->children)
                {
                    if (Alive(child) && Parent(child) == node)
                    {
                        links.children.push_back(child);
                    }
                }
                for (auto *source : node->sourceNodes)
                {
                    if (Alive(source) && Instance(source) == node)
                    {
                        links.sources.push_back(source);
                    }
                }
                if (!links.sources.empty())
                {
                    std::unordered_set<std::uint32_t> seen;
                    seen.reserve(links.sources.size());
                    for (auto *source : links.sources)
                    {
                        const auto &record = Record(source);
                        if (!seen.insert(record.source->node).second)
                        {
                            Reject(record.id, "sourceRef.node", "Repeated source node in one instance");
                        }
                    }
                }
            }
            for (const auto &[id, entry] : m_Entries)
            {
                if (!entry.after)
                {
                    const auto links = m_Links.find(entry.node);
                    const bool empty = links == m_Links.end() ?
                        entry.node->children.empty() && entry.node->sourceNodes.empty() :
                        links->second.children.empty() && links->second.sources.empty();
                    if (!empty)
                    {
                        Reject(id, "id", "Removal would leave a dangling entity reference");
                    }
                }
            }
        }

        /// @brief 从已验证的事务终态取得网格, 无 MeshRenderer 时返回空.
        [[nodiscard]] const Asset::ModelMesh *ProposedMesh(SceneNode *node, const EntityRecord &record) const noexcept
        {
            if (!record.properties.castsShadow)
            {
                return nullptr;
            }
            const auto *entry = Proposed(node);
            const auto *source = entry ? entry->source : node->sourceNode;
            return &Record(Instance(node)).model->meshes[*source->mesh];
        }

        /// @brief 几何看完整仿射变换, 相机和灯分别看自己使用的刚体分量.
        void ClassifyWorld(const EntityProperties &props, const WorldState &before, const WorldState &after) noexcept
        {
            // 零缩放可能掩盖矩阵中的旋转变化, 因此相机和灯不能只比较 worldFromLocal.
            const bool translated = before.pose.translation != after.pose.translation;
            const bool rotated = before.pose.rotation != after.pose.rotation;
            m_CameraChanged |= props.camera.has_value() && (translated || rotated);
            m_RenderChanged |= props.castsShadow.has_value() &&
                (before.worldFromLocal != after.worldFromLocal || before.orientation != after.orientation);
            if (props.light)
            {
                // 方向灯不消费位置, 点光不消费朝向; 定向灯绕自身光轴旋转也不改变发射方向.
                const auto type = props.light->type;
                m_RenderChanged |= (translated && type != Asset::ModelLightKind::Directional) ||
                    (rotated && type != Asset::ModelLightKind::Point && before.Forward() != after.Forward());
            }
        }

        /// @brief 合并被脏祖先覆盖的根, 以父结果索引传递世界数据, 避免 vector 扩容使指针失效.
        void PrepareWorld()
        {
            static constexpr auto UseActiveParent = std::numeric_limits<std::size_t>::max();
            struct Visit
            {
                SceneNode *node;
                std::size_t parentResult;
            };
            std::vector<Visit> pending;
            // 空脏根直接跳过全部实体. 结构校验已经保证 Parent 链终止且所有链接存活.
            for (auto *root : m_Dirty)
            {
                auto *ancestor = Parent(root);
                while (ancestor && !m_Dirty.contains(ancestor))
                {
                    ancestor = Parent(ancestor);
                }
                if (!ancestor)
                {
                    pending.push_back({root, UseActiveParent});
                }
            }
            while (!pending.empty())
            {
                CheckStop();
                const auto [node, parentResult] = pending.back();
                pending.pop_back();
                const auto *parent = Parent(node);
                const auto *parentWorld = parentResult == UseActiveParent ? (parent ? &parent->world : nullptr) :
                    &m_World[parentResult].world;
                const auto &record = Record(node);
                auto world = ComputeWorld(record.properties.local, parentWorld, ProposedMesh(node, record));
                if (!world)
                {
                    Reject(record.id, "local", "World transform or bounds exceeds representation");
                }
                ClassifyWorld(record.properties, node->world, *world);
                const auto index = m_World.size();
                m_World.push_back({node, std::move(*world)});
                const auto updated = m_Links.find(node);
                const auto &children = updated == m_Links.end() ? node->children : updated->second.children;
                for (auto *child : children)
                {
                    pending.push_back({child, index});
                }
            }
        }

        SceneState &m_State;
        std::stop_token m_Stop;
        std::uint64_t m_Generation;
        Revisions m_Revision;
        std::uint64_t m_LastId;
        bool m_RenderChanged{};
        bool m_CameraChanged{};
        const std::optional<SceneEnvironment> *m_Environment{}; ///< 借用提交输入, 仅在同次 Prepare/Publish 使用.
        std::map<EntityId, Entry, std::less<>> m_Entries;
        std::vector<entt::entity> m_Created;
        std::map<EntityId, SceneNode *, std::less<>> m_NewEntries;
        std::unordered_map<SceneNode *, Links> m_Links;
        std::unordered_set<SceneNode *> m_Dirty;
        std::unordered_map<const Asset::ModelRecord *, std::unordered_map<std::uint32_t, const Asset::ModelNode *>>
            m_Sources;
        std::vector<WorldUpdate> m_World;
    };

    std::optional<Core::BoundedText> SceneState::Commit(const Change &change, std::stop_token stop)
    {
        if (change.Empty())
        {
            return std::nullopt;
        }
        try
        {
            StagedChange pending(*this, stop);
            pending.Prepare(change);
            pending.Publish();
            return std::nullopt;
        }
        catch (const Core::BoundedText &error)
        {
            return error;
        }
    }
}
