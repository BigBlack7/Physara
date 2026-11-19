#include "PhysicalEdits.hpp"

/// @file
/// @brief 在编辑边界另外解析物理字段及待交付的纹理/HDR 来源.
/// 请求只持有发布身份和前一源值, 所有实际修改交还 Scene 事务和共享历史.

#include <algorithm>
#include <cassert>
#include <exception>
#include <filesystem>
#include <type_traits>
#include <utility>

#include <Core/ResourceIO.hpp>

namespace Physara::Editor::Detail
{
    const Scene::PrimitiveOverride *FindSurfaceOverride(const Scene::EntityRecord &record,
                                                         std::uint32_t primitive) noexcept
    {
        const auto found = std::ranges::lower_bound(record.overrides, primitive, {},
                                                   &Scene::PrimitiveOverride::primitiveSlot);
        return found != record.overrides.end() && found->primitiveSlot == primitive ? &*found : nullptr;
    }

    /// @brief 源下标在网格组件或模型换代后失去资格, 即使实体仍存活.
    static const Scene::EntityRecord *SurfaceRecord(const Scene::Scene &scene, const SurfaceTarget &target) noexcept
    {
        const auto entity = scene.Get(target.entity);
        if (!entity || !entity->record.properties.castsShadow || !entity->model || !entity->sourceNode ||
            !entity->sourceNode->mesh)
        {
            return nullptr;
        }
        const auto &model = **entity->model;
        return model.asset == target.model && model.revision == target.revision &&
            target.primitive < model.meshes[*entity->sourceNode->mesh].primitives.size() ? &entity->record : nullptr;
    }

    static std::size_t Index(TextureTarget target) noexcept
    {
        const auto index = static_cast<std::size_t>(target.slot);
        assert(index < Scene::MaterialSlotCount);
        return index;
    }

    /// @brief 在当前稀疏记录中合并局部字段. Scene 对合并后的完整采样组合做校验和归并.
    template <class Update>
    static std::optional<Core::BoundedText> WriteSurface(Scene::Scene &scene, Renderer::View &view,
        EditHistory &history, const SurfaceTarget &target, Update update)
    {
        const auto *record = SurfaceRecord(scene, target);
        if (!record)
        {
            return Core::BoundedText("The instance or its fixed Primitive source expired");
        }
        const auto *before = FindSurfaceOverride(*record, target.primitive);
        // 同轮先接受的其它字段已在 record 中. 以此刻的稀疏记录为基底, 清除后由 Scene 归并空槽.
        auto after = before ? *before : Scene::PrimitiveOverride{.primitiveSlot = target.primitive};
        update(after);
        return history.ApplyScene(scene, view, Scene::EditOverride{target.entity, target.primitive, std::move(after)});
    }

    /// @brief 一次访问更新一个成员. 空值仍由调用方的字段类型决定其领域含义.
    template <class Owner, class Fields>
    static void WriteField(Owner &owner, const Fields &fields)
    {
        std::visit([&owner](const auto &edit) { owner.*edit.field = edit.value; }, fields);
    }

    /// @brief 清除槽或合并 sampler/UV 字段, 保留可能已接受的其它采样值.
    static void WriteTexture(std::optional<Scene::TextureOverride> &slot, const TextureEdit &edit)
    {
        std::visit([&](const auto &field)
        {
            using Field = std::decay_t<decltype(field)>;
            if constexpr (std::is_same_v<Field, std::monostate>)
            {
                slot.reset();
            }
            else
            {
                if (!slot)
                {
                    slot.emplace();
                }
                if constexpr (std::is_same_v<Field, SetField<Scene::SamplerOverride, std::optional<std::uint32_t>>>)
                {
                    slot->sampler.*field.field = field.value;
                }
                else
                {
                    (*slot).*field.field = field.value;
                }
            }
        }, edit.field);
    }

    /// @brief 以当前场景环境为基底应用字段, 过期场景或已移除的环境不能接收旧数值.
    static std::optional<Core::BoundedText> ApplyEnvironment(Scene::Scene &scene, Renderer::View &view,
        EditHistory &history, const EnvironmentEdit &edit)
    {
        if (edit.target != scene.Id())
        {
            return Core::BoundedText("The environment edit belongs to an expired scene");
        }
        auto value = scene.Environment();
        const bool removed = std::holds_alternative<std::monostate>(edit.field);
        if (!value && !removed)
        {
            return Core::BoundedText("Choose a ready HDR source before editing environment settings");
        }
        std::visit([&value](const auto &field)
        {
            if constexpr (std::is_same_v<std::decay_t<decltype(field)>, std::monostate>)
            {
                value.reset();
            }
            else
            {
                value->settings.*field.field = field.value;
            }
        }, edit.field);
        return history.ApplyScene(scene, view, Scene::SetEnvironment{std::move(value)});
    }

    // 每项从当前领域值合并. 清除意图即使没有改动已存字段, 也会撤销同一目标的迟到回填.
    std::optional<Core::BoundedText> PhysicalEdits::Apply(Scene::Scene &scene, Renderer::View &view,
        EditHistory &history, const PhysicalEdit &edit)
    {
        std::optional<PhysicalResourceTarget> cleared;
        const auto error = std::visit([&](const auto &input) -> std::optional<Core::BoundedText>
        {
            using Input = std::decay_t<decltype(input)>;
            if constexpr (std::is_same_v<Input, MaterialEdit>)
            {
                return WriteSurface(scene, view, history, input.target, [&](auto &value)
                {
                    WriteField(value.material, input.field);
                });
            }
            else if constexpr (std::is_same_v<Input, TextureEdit>)
            {
                if (std::holds_alternative<std::monostate>(input.field))
                {
                    cleared = input.target;
                }
                return WriteSurface(scene, view, history, input.target.surface, [&](auto &value)
                {
                    WriteTexture(value.textures[Index(input.target)], input);
                });
            }
            else if constexpr (std::is_same_v<Input, LightEdit>)
            {
                const auto entity = scene.Get(input.target);
                if (!entity || !entity->record.properties.light)
                {
                    return Core::BoundedText("The punctual light expired");
                }
                auto properties = entity->record.properties;
                WriteField(*properties.light, input.field);
                return history.ApplyScene(scene, view, Scene::EditProperties{input.target, std::move(properties)});
            }
            else
            {
                if (std::holds_alternative<std::monostate>(input.field))
                {
                    cleared = input.target;
                }
                return ApplyEnvironment(scene, view, history, input);
            }
        }, edit.value);
        if (error)
        {
            Core::BoundedText description(edit.label);
            description.Append(": ");
            description.Append(error->View());
            return description;
        }
        if (cleared)
        {
            CancelSource(*cleared);
        }
        RefreshSourceStatus(scene);
        return std::nullopt;
    }

    std::optional<Core::BoundedText> PhysicalEdits::ApplyScene(Scene::Scene &scene, Renderer::View &view,
        EditHistory &history, const Scene::Command &command)
    {
        if (const auto error = history.ApplyScene(scene, view, command))
        {
            return error;
        }
        // 此时命令已被接受. 同值清除也表达了用户的选择, 仅查看 Scene revision 无法识别它.
        if (const auto *environment = std::get_if<Scene::SetEnvironment>(&command); environment && !environment->value)
        {
            CancelSource(scene.Id());
        }
        else if (const auto *target = std::get_if<TextureTarget>(&m_Status.target); m_Pending && target)
        {
            // EditOverride 全量替换指定 Primitive. 其它实体和 Primitive 的来源意图各自保留.
            const auto *over = std::get_if<Scene::EditOverride>(&command);
            if (over && over->entity == target->surface.entity && over->primitiveSlot == target->surface.primitive)
            {
                const auto *slot = over->value ? &over->value->textures[Index(*target)] : nullptr;
                if (!slot || !*slot || **slot == Scene::TextureOverride{})
                {
                    CancelSource(*target);
                }
            }
        }
        RefreshSourceStatus(scene);
        return std::nullopt;
    }

    std::optional<PhysicalEdits::Source> PhysicalEdits::CurrentSource(const Scene::Scene &scene,
        const PhysicalResourceTarget &target) noexcept
    {
        if (const auto *environment = std::get_if<Scene::SceneId>(&target))
        {
            if (*environment != scene.Id())
            {
                return std::nullopt;
            }
            return Source(scene.Environment() ? scene.Environment()->source : Asset::EnvironmentVersion{});
        }
        const auto &texture = std::get<TextureTarget>(target);
        // 未设置外部图像时, 继承图像的身份已包含在固定模型版本中. 采样器和 UV 另由当前槽提供.
        const auto *record = SurfaceRecord(scene, texture.surface);
        if (!record)
        {
            return std::nullopt;
        }
        const auto *value = FindSurfaceOverride(*record, texture.surface.primitive);
        const auto index = Index(texture);
        return Source(value && value->textures[index] ? value->textures[index]->image : Asset::ImageVersion{});
    }

    // 先替换会话候选, 再把项目路径提交给对应的资产入口. 路径只保留有界的显示副本.
    void PhysicalEdits::Begin(Asset::AssetManager &assets, const Scene::Scene &scene,
                              const PhysicalResourceIntent &intent)
    {
        Reset();
        m_Status.target = intent.target;
        m_Status.path.Assign(intent.path);
        const auto before = CurrentSource(scene, intent.target);
        if (!before)
        {
            Fail(Core::BoundedText("The resource target expired"));
            return;
        }
        try
        {
            const auto *utf8 = reinterpret_cast<const char8_t *>(intent.path.data());
            const std::filesystem::path path(utf8, utf8 + intent.path.size());
            const bool environment = std::holds_alternative<Scene::SceneId>(intent.target);
            if (!environment)
            {
                // 通用图像入口还接收 Editor 图像, 场景纹理只允许正式 Textures 目录.
                constexpr std::array<std::wstring_view, 1> textureRoot{L"Textures"};
                if (!std::holds_alternative<Core::ResourcePath>(Core::ResourcePath::Parse(path, textureRoot)))
                {
                    Fail(Core::BoundedText("Choose a relative Assets/Textures image"));
                    return;
                }
            }
            const auto result = environment ? assets.RequestEnvironment(path) : assets.RequestImage(path);
            if (const auto *error = std::get_if<Asset::AssetFailure>(&result))
            {
                Fail(error->description);
                return;
            }
            m_Pending = Pending{std::get<Asset::AssetRequest>(result), *before, intent.initialUv};
            m_Status.phase = PhysicalResourcePhase::Pending;
        }
        catch (const std::exception &error)
        {
            Fail(Core::BoundedText(error.what()));
        }
    }

    // 当前请求代和固定源匹配后才消费 CpuReady. 活动手势结束前保留候选, 不插入历史.
    void PhysicalEdits::Advance(Asset::AssetManager &assets, Scene::Scene &scene, Renderer::View &view,
                               EditHistory &history)
    {
        RefreshSourceStatus(scene);
        if (!m_Pending)
        {
            return;
        }
        const auto request = m_Pending->asset;
        const auto apply = [&]<class Status, class Publish>(const Status &status, Publish publish)
        {
            if (status.request != request.request)
            {
                Fail(Core::BoundedText("The asset request was superseded"));
            }
            else if (status.state == Asset::AssetState::CpuReady)
            {
                if (!history.HasGesture())
                {
                    try
                    {
                        if (const auto error = publish(status.published))
                        {
                            Fail(*error);
                        }
                        else
                        {
                            m_Pending.reset();
                            m_AppliedSource = SourceIdentity{status.published->asset, status.published->revision};
                            m_Status.phase = PhysicalResourcePhase::Applied;
                        }
                    }
                    catch (const std::exception &error)
                    {
                        Fail(Core::BoundedText(error.what()));
                    }
                }
            }
            else if (status.state != Asset::AssetState::Loading)
            {
                Fail(status.failure ? status.failure->description : Core::BoundedText("The asset is unavailable"));
            }
        };
        if (const auto *texture = std::get_if<TextureTarget>(&m_Status.target))
        {
            apply(assets.Status(request.asset), [&](const Asset::ImageVersion &image)
            {
                return WriteSurface(scene, view, history, texture->surface, [&](auto &value)
                {
                    auto &slot = value.textures[Index(*texture)];
                    if (!slot)
                    {
                        slot.emplace();
                    }
                    slot->image = image;
                    slot->use = Scene::TextureUse(texture->slot);
                    // 只有初次建槽且源网格仅提供 UV1 时才需起始覆盖. 等待期间已接受的 UV 优先.
                    if (!slot->texCoord)
                    {
                        slot->texCoord = m_Pending->initialUv;
                    }
                });
            });
        }
        else
        {
            apply(assets.StatusEnvironment(request.asset), [&](const Asset::EnvironmentVersion &source)
            {
                const auto &current = scene.Environment();
                const auto settings = current ? current->settings :
                    Scene::EnvironmentSettings{.lighting = true, .sky = true};
                return history.ApplyScene(scene, view,
                                          Scene::SetEnvironment{Scene::SceneEnvironment{source, settings}});
            });
        }
    }

    void PhysicalEdits::RefreshSourceStatus(const Scene::Scene &scene) noexcept
    {
        if (!m_Pending && !m_AppliedSource)
        {
            return;
        }
        const auto source = CurrentSource(scene, m_Status.target);
        if (m_Pending)
        {
            if (source != m_Pending->before)
            {
                Cancel();
            }
            return;
        }
        // 重放可让同一来源版本再次成为创作值. 保留它的身份即可恢复显示, 资产寿命仍归 Scene 或历史管理.
        const bool applied = source && std::visit([&](const auto &value)
        {
            return value && SourceIdentity{value->asset, value->revision} == *m_AppliedSource;
        }, *source);
        m_Status.phase = applied ? PhysicalResourcePhase::Applied : PhysicalResourcePhase::Inactive;
    }

    void PhysicalEdits::CancelSource(const PhysicalResourceTarget &target) noexcept
    {
        if (target == m_Status.target)
        {
            Cancel();
        }
    }

    void PhysicalEdits::Fail(Core::BoundedText failure) noexcept
    {
        m_Pending.reset();
        m_Status.phase = PhysicalResourcePhase::Failed;
        m_Status.reason = failure;
    }

    void PhysicalEdits::Cancel() noexcept
    {
        if (m_Pending)
        {
            m_Pending.reset();
            m_Status.phase = PhysicalResourcePhase::Cancelled;
            m_Status.reason = {};
        }
    }

    void PhysicalEdits::Reset() noexcept
    {
        m_Pending.reset();
        m_AppliedSource.reset();
        m_Status = {};
    }
}
