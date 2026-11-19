#include "PhysicalInspector.hpp"

/// @file
/// @brief 单字段提交和资源路径输入, 有效值由 Scene 和所选固定资产解析.

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <numbers>
#include <span>
#include <string_view>

#include <glm/gtc/quaternion.hpp>
#include <imgui.h>

#include "Workspace.hpp"

namespace Physara::Editor::Detail
{
    static constexpr auto Enter = ImGuiInputTextFlags_EnterReturnsTrue;
    static constexpr std::array TextureNames{"Base color texture [sRGB]", "Texture [Roughness = G, Metallic = B]",
        "Normal texture [linear]", "Occlusion texture [R]", "Emissive texture [sRGB]"};
    static_assert(TextureNames.size() == Scene::MaterialSlotCount);
    static constexpr double DegreesPerRadian = 180.0 / std::numbers::pi;

    /// @brief 每个文档/源版本拆成两个 ID, 防止延续上一目标的 ImGui 草稿.
    static void PushIdentity(std::uint64_t value)
    {
        ImGui::PushID(static_cast<int>(value));
        ImGui::PushID(static_cast<int>(value >> 32));
    }

    static void PopIdentity()
    {
        ImGui::PopID();
        ImGui::PopID();
    }

    // 数值用浮点领域类型显示和提交, Enter 之前的文本由 ImGui 保存.
    static bool Input(const char *label, float &value)
    {
        return ImGui::InputFloat(label, &value, 0.0f, 0.0f, "%.9g", Enter);
    }

    static bool Input(const char *label, double &value)
    {
        return ImGui::InputDouble(label, &value, 0.0, 0.0, "%.17g", Enter);
    }

    static bool Input(const char *label, std::array<float, 2> &value)
    {
        return ImGui::InputFloat2(label, value.data(), "%.9g", Enter);
    }

    static bool Input(const char *label, std::array<float, 3> &value)
    {
        return ImGui::InputFloat3(label, value.data(), "%.9g", Enter);
    }

    static bool Input(const char *label, std::array<float, 4> &value)
    {
        return ImGui::InputFloat4(label, value.data(), "%.9g", Enter);
    }

    static bool Input(const char *label, bool &value)
    {
        return ImGui::Checkbox(label, &value);
    }

    /// @brief 为覆盖的单字段提供还原入口, 并明确当前数值来自源或实例.
    static bool RestoreInherited(const char *label, bool overridden)
    {
        ImGui::SameLine();
        if (overridden)
        {
            ImGui::PushID(label);
            const bool clicked = ImGui::SmallButton("Reset override");
            ImGui::PopID();
            return clicked;
        }
        ImGui::TextDisabled("Inherited");
        return false;
    }

    /// @brief 控件显示求值后的因子. 重置提交空成员值, 让该字段继续随源材质解析.
    template <class T>
    static void DrawMaterialField(const char *label, SurfaceTarget target,
        std::optional<T> Scene::MaterialOverride::*field, const Scene::MaterialOverride &overrides,
        T effective, WorkspaceFrame &frame)
    {
        auto next = effective;
        if (Input(label, next) && next != effective)
        {
            frame.physicalEdits.push_back({MaterialEdit{target, MaterialField<T>{field, next}}, label});
        }
        if (RestoreInherited(label, (overrides.*field).has_value()))
        {
            frame.physicalEdits.push_back({MaterialEdit{target, MaterialField<T>{field, std::nullopt}}, label});
        }
    }

    /// @brief 允许从导入的未绘制 BLEND 切到已支持的 profile; 恢复时仍沿其原语义显示.
    static void DrawAlpha(SurfaceTarget target, const Scene::MaterialOverride &overrides,
                          Asset::ModelAlpha current, WorkspaceFrame &frame)
    {
        using enum Asset::ModelAlpha;
        constexpr auto label = "Alpha mode";
        if (ImGui::BeginCombo(label, current == Mask ? "MASK" : current == Opaque ? "OPAQUE" : "BLEND (unsupported)"))
        {
            for (const auto choice : {Opaque, Mask})
            {
                if (ImGui::Selectable(choice == Mask ? "MASK" : "OPAQUE", current == choice) && current != choice)
                {
                    frame.physicalEdits.push_back({MaterialEdit{target,
                        MaterialField<Asset::ModelAlpha>{&Scene::MaterialOverride::alphaMode, choice}}, label});
                }
            }
            ImGui::EndCombo();
        }
        if (RestoreInherited(label, overrides.alphaMode.has_value()))
        {
            frame.physicalEdits.push_back({MaterialEdit{target,
                MaterialField<Asset::ModelAlpha>{&Scene::MaterialOverride::alphaMode, std::nullopt}}, label});
        }
    }

    /// @brief 当前路径用于初始化可编辑草稿; 光标激活期间, ImGui 保存未提交的输入.
    static void DrawSource(const char *hint, const std::filesystem::path *source, PhysicalResourceTarget target,
                           WorkspaceFrame &frame, std::optional<std::uint32_t> initialUv = {})
    {
        std::array<char, 2048> text{};
        if (source)
        {
            const auto utf8 = source->generic_u8string();
            if (utf8.size() < text.size())
            {
                std::copy(utf8.begin(), utf8.end(), text.begin());
            }
        }
        if (ImGui::InputTextWithHint("Source path", hint, text.data(), text.size(), Enter) && text[0] != '\0')
        {
            frame.physicalSource = PhysicalResourceIntent{target, text.data(), initialUv};
        }
    }

    /// @brief 选项的数值沿用 glTF sampler 枚举, 展示文字只用于本面板.
    struct SamplerChoice
    {
        std::uint32_t value;
        const char *name;
    };

    static constexpr std::array MinFilters{SamplerChoice{9728, "Nearest"}, SamplerChoice{9729, "Linear"},
        SamplerChoice{9984, "Nearest / nearest mip"}, SamplerChoice{9985, "Linear / nearest mip"},
        SamplerChoice{9986, "Nearest / linear mip"}, SamplerChoice{9987, "Linear / linear mip"}};
    static constexpr std::array WrapModes{SamplerChoice{10497, "Repeat"}, SamplerChoice{33071, "Clamp to edge"},
        SamplerChoice{33648, "Mirrored repeat"}};

    /// @brief sampler 的可选成员继承源设置, 选择表与 Scene 使用相同的 glTF 数值.
    static void DrawSampler(const char *label, TextureTarget target, std::span<const SamplerChoice> choices,
        std::optional<std::uint32_t> Scene::SamplerOverride::*field, std::optional<std::uint32_t> effective,
        std::optional<std::uint32_t> override, WorkspaceFrame &frame)
    {
        const auto active = std::ranges::find(choices, effective.value_or(0), &SamplerChoice::value);
        if (ImGui::BeginCombo(label, active == choices.end() ? "glTF default" : active->name))
        {
            if (ImGui::Selectable("Inherit sampler field", !override) && override)
            {
                frame.physicalEdits.push_back({TextureEdit{target,
                    SetField<Scene::SamplerOverride, std::optional<std::uint32_t>>{field, std::nullopt}}, label});
            }
            for (const auto &choice : choices)
            {
                if (ImGui::Selectable(choice.name, effective == choice.value) && effective != choice.value)
                {
                    frame.physicalEdits.push_back({TextureEdit{target,
                        SetField<Scene::SamplerOverride, std::optional<std::uint32_t>>{field, choice.value}}, label});
                }
            }
            ImGui::EndCombo();
        }
    }

    /// @brief 从有效 UV 变换开始编辑, 确认时只记录所选向量或角度成员.
    template <class T>
    static void DrawTextureValue(const char *label, TextureTarget target, T effective,
        std::optional<T> Scene::TextureOverride::*field, WorkspaceFrame &frame)
    {
        auto next = effective;
        if (Input(label, next) && next != effective)
        {
            frame.physicalEdits.push_back({TextureEdit{target,
                SetField<Scene::TextureOverride, std::optional<T>>{field, next}}, label});
        }
    }

    /// @brief 只有源 Primitive 的实际 UV 可供选择. 继承图像也能保留自己的 sampler 和变换覆盖.
    static void DrawTexture(TextureTarget target, const Asset::ModelPrimitive &primitive,
        const std::optional<Scene::EffectiveTexture> &effective, const std::optional<Scene::TextureOverride> &sparse,
        WorkspaceFrame &frame)
    {
        const auto index = static_cast<std::size_t>(target.slot);
        if (!ImGui::TreeNode(TextureNames[index]))
        {
            return;
        }
        const auto *source = sparse && sparse->image ? &sparse->image->source : nullptr;
        ImGui::TextUnformatted(source ? "Project image override" : effective ? "Inherited glTF image" : "No image");
        if (primitive.hasUv[0] || primitive.hasUv[1])
        {
            const auto initialUv = !effective && !primitive.hasUv[0] ? std::optional<std::uint32_t>(1) : std::nullopt;
            DrawSource("Assets/Textures/...", source, target, frame, initialUv);
        }
        else
        {
            ImGui::TextDisabled("This Primitive has no supported UV set.");
        }
        if (sparse && ImGui::SmallButton("Reset texture slot"))
        {
            frame.physicalEdits.push_back({TextureEdit{target, std::monostate{}}, TextureNames[index]});
        }
        if (effective)
        {
            const auto &slot = effective->slot;
            if (ImGui::BeginCombo("UV set", slot.texCoord == 0 ? "UV0" : "UV1"))
            {
                for (std::uint32_t i = 0; i < primitive.hasUv.size(); ++i)
                {
                    if (primitive.hasUv[i] && ImGui::Selectable(i == 0 ? "UV0" : "UV1", slot.texCoord == i) &&
                        slot.texCoord != i)
                    {
                        frame.physicalEdits.push_back({TextureEdit{target,
                            SetField<Scene::TextureOverride, std::optional<std::uint32_t>>{
                                &Scene::TextureOverride::texCoord, i}}, "UV set"});
                    }
                }
                ImGui::EndCombo();
            }
            const auto sampler = sparse ? sparse->sampler : Scene::SamplerOverride{};
            const auto &active = effective->sampler;
            DrawSampler("Min filter", target, MinFilters, &Scene::SamplerOverride::minFilter,
                        active.minFilter, sampler.minFilter, frame);
            DrawSampler("Mag filter", target, std::span(MinFilters).first(2), &Scene::SamplerOverride::magFilter,
                        active.magFilter, sampler.magFilter, frame);
            DrawSampler("Wrap S", target, WrapModes, &Scene::SamplerOverride::wrapS,
                        active.wrapS, sampler.wrapS, frame);
            DrawSampler("Wrap T", target, WrapModes, &Scene::SamplerOverride::wrapT,
                        active.wrapT, sampler.wrapT, frame);
            DrawTextureValue("UV offset", target, slot.offset, &Scene::TextureOverride::offset, frame);
            DrawTextureValue("UV scale", target, slot.scale, &Scene::TextureOverride::scale, frame);
            DrawTextureValue("UV rotation (rad)", target, slot.rotationRadians,
                             &Scene::TextureOverride::rotationRadians, frame);
        }
        ImGui::TreePop();
    }

    /// @brief 逐槽读取有效表面, 同源的其它实例不参与本次意图构建.
    static void DrawSurface(const Scene::Scene &scene, SurfaceTarget target, const Scene::EntityRecord &record,
                            const Asset::ModelPrimitive &primitive, WorkspaceFrame &frame)
    {
        const auto effective = scene.Material(target.entity, target.primitive);
        if (!effective)
        {
            return;
        }
        const auto *over = FindSurfaceOverride(record, target.primitive);
        const auto &value = effective->values;
        const auto sparse = over ? over->material : Scene::MaterialOverride{};
        using Material = Scene::MaterialOverride;
        DrawMaterialField("Base color (linear RGBA)", target, &Material::baseColor, sparse, value.baseColor, frame);
        if (!value.unlit)
        {
            DrawMaterialField("Metallic", target, &Material::metallic, sparse, value.metallic, frame);
            DrawMaterialField("Roughness", target, &Material::roughness, sparse, value.roughness, frame);
            DrawMaterialField("Normal scale", target, &Material::normalScale, sparse, value.normalScale, frame);
            DrawMaterialField("Occlusion strength", target, &Material::occlusionStrength, sparse,
                              value.occlusionStrength, frame);
            DrawMaterialField("Emission (linear RGB)", target, &Material::emissive, sparse, value.emissive, frame);
            DrawMaterialField("Emissive strength", target, &Material::emissiveStrength, sparse,
                              value.emissiveStrength, frame);
        }
        DrawAlpha(target, sparse, value.alphaMode, frame);
        if (value.alphaMode == Asset::ModelAlpha::Mask)
        {
            DrawMaterialField("Alpha cutoff", target, &Material::alphaCutoff, sparse, value.alphaCutoff, frame);
        }
        DrawMaterialField("Double sided", target, &Material::doubleSided, sparse, value.doubleSided, frame);
        for (std::size_t i = 0; i < (value.unlit ? 1 : TextureNames.size()); ++i)
        {
            DrawTexture({target, static_cast<Asset::MaterialSlot>(i)}, primitive, effective->textures[i],
                        over ? over->textures[i] : std::nullopt, frame);
        }
    }

    /// @brief 确认光度字段的完整数值草稿, 并以控件的单位标注解析失败.
    /// @return 有实际变化的显示单位值. 范围缺席时显示占位, 领域值域仍由 Scene 校验.
    static std::optional<double> DrawLightNumber(const char *label, std::optional<double> current,
                                                  WorkspaceFrame &frame)
    {
        std::array<char, 64> draft{};
        if (current)
        {
            std::to_chars(draft.data(), draft.data() + draft.size() - 1, *current);
        }
        constexpr auto flags = Enter | ImGuiInputTextFlags_AutoSelectAll;
        if (!ImGui::InputTextWithHint(label, current ? nullptr : "Unbounded range", draft.data(), draft.size(), flags))
        {
            return std::nullopt;
        }
        const std::string_view number(draft.data());
        const auto *end = number.data() + number.size();
        const auto *start = number.data() + (number.starts_with('+') ? 1 : 0);
        double value{};
        const auto parsed = std::from_chars(start, end, value);
        // 缓冲写满时无法区分完整输入与粘贴后留下的前缀, 交由用户重新确认较短的表示.
        if (number.size() == draft.size() - 1 || number.starts_with("+-") ||
            parsed.ec != std::errc{} || parsed.ptr != end || !std::isfinite(value))
        {
            auto &error = frame.physicalInputError.emplace(label);
            error.Append(": enter one complete finite number");
            return std::nullopt;
        }
        return current == value ? std::nullopt : std::optional(value);
    }

    /// @brief 有限范围与清除范围分别提交, 无截断由空值表达.
    static void DrawLightRange(Scene::EntityHandle target, std::optional<double> range, WorkspaceFrame &frame)
    {
        constexpr auto label = "Range (m)";
        if (const auto value = DrawLightNumber(label, range, frame))
        {
            frame.physicalEdits.push_back({LightEdit{target, SetField{&Scene::Light::range, value}}, label});
        }
        if (range && ImGui::SmallButton("Use unbounded range"))
        {
            frame.physicalEdits.push_back({LightEdit{target,
                SetField{&Scene::Light::range, std::optional<double>{}}}, label});
        }
    }

    /// @brief 按光型选择单位和适用字段. 锥角在控件边界从度换回弧度.
    static void DrawLight(Scene::EntityHandle target, const Scene::Light &light, WorkspaceFrame &frame)
    {
        using Kind = Asset::ModelLightKind;
        ImGui::SeparatorText("Punctual light");
        const bool directional = light.type == Kind::Directional;
        const bool spot = light.type == Kind::Spot;
        ImGui::TextUnformatted(directional ? "Directional" : spot ? "Spot" : "Point");
        auto color = light.color;
        constexpr auto colorLabel = "Light color (linear RGB)";
        if (Input(colorLabel, color) && color != light.color)
        {
            frame.physicalEdits.push_back({LightEdit{target, SetField{&Scene::Light::color, color}}, colorLabel});
        }
        const auto editDouble = [&](const char *label, double Scene::Light::*field, double displayScale)
        {
            if (const auto value = DrawLightNumber(label, light.*field * displayScale, frame))
            {
                frame.physicalEdits.push_back({LightEdit{target, SetField{field, *value / displayScale}}, label});
            }
        };
        editDouble(directional ? "Intensity (lux)" : "Intensity (cd)", &Scene::Light::intensity, 1.0);
        if (!directional)
        {
            DrawLightRange(target, light.range, frame);
            if (spot)
            {
                editDouble("Inner cone (deg)", &Scene::Light::innerConeAngle, DegreesPerRadian);
                editDouble("Outer cone (deg)", &Scene::Light::outerConeAngle, DegreesPerRadian);
            }
        }
    }

    /// @brief 环境与所选实体独立. 角度转为环境到世界的单位旋转, 灯光与天空单独提交.
    static void DrawEnvironment(const Scene::Scene &scene, WorkspaceFrame &frame)
    {
        ImGui::SeparatorText("Scene environment");
        const auto &current = scene.Environment();
        DrawSource("Assets/Environments/...exr", current ? &current->source->source : nullptr, scene.Id(), frame);
        if (!current)
        {
            ImGui::TextDisabled("No environment configured");
            return;
        }
        if (ImGui::SmallButton("Remove environment"))
        {
            frame.physicalEdits.push_back({EnvironmentEdit{scene.Id(), std::monostate{}}, "Environment"});
        }
        // 尺寸和单位白来自已接受的 EXR 版本. 候选文件路径在独立的来源状态区展示.
        const auto &image = current->source->image;
        if (const auto white = image.whiteLuminance)
        {
            ImGui::Text("EXR %ux%u, source white %.6g cd/m2", image.width, image.height, static_cast<double>(*white));
        }
        else
        {
            ImGui::Text("EXR %ux%u, relative radiance", image.width, image.height);
        }
        using Settings = Scene::EnvironmentSettings;
        const auto &settings = current->settings;
        const auto &q = settings.rotation;
        const auto euler = glm::degrees(glm::eulerAngles(glm::dquat(q[3], q[0], q[1], q[2])));
        const std::array previous{euler.x, euler.y, euler.z};
        auto angles = previous;
        if (ImGui::InputScalarN("Environment rotation (deg)", ImGuiDataType_Double, angles.data(), 3,
                                nullptr, nullptr, "%.17g", Enter) && angles != previous)
        {
            const glm::dquat rotation(glm::radians(glm::dvec3(angles[0], angles[1], angles[2])));
            frame.physicalEdits.push_back({EnvironmentEdit{scene.Id(), SetField{&Settings::rotation,
                std::array{rotation.x, rotation.y, rotation.z, rotation.w}}}, "Environment rotation"});
        }
        auto intensity = settings.intensity;
        if (Input("Environment intensity", intensity) && intensity != settings.intensity)
        {
            frame.physicalEdits.push_back({EnvironmentEdit{scene.Id(), SetField{&Settings::intensity, intensity}},
                                           "Environment intensity"});
        }
        bool lighting = settings.lighting;
        bool sky = settings.sky;
        if (ImGui::Checkbox("Indirect lighting", &lighting))
        {
            frame.physicalEdits.push_back({EnvironmentEdit{scene.Id(), SetField{&Settings::lighting, lighting}},
                                           "Indirect lighting"});
        }
        if (ImGui::Checkbox("Sky background", &sky))
        {
            frame.physicalEdits.push_back({EnvironmentEdit{scene.Id(), SetField{&Settings::sky, sky}},
                                           "Sky background"});
        }
    }

    /// @brief 请求状态属于当前会话, 即使用户已经查看其它实体也标出它的原目标.
    static void DrawResourceStatus(const Scene::Scene &scene, const PhysicalResourceStatus &resource,
                                   WorkspaceFrame &frame)
    {
        using enum PhysicalResourcePhase;
        const char *phase = nullptr;
        switch (resource.phase)
        {
        case Idle: return;
        case Pending: phase = "Pending CPU asset"; break;
        case Applied: phase = "CPU asset applied to Scene"; break;
        case Inactive: phase = "Previous source edit (inactive)"; break;
        case Failed: phase = "Failed"; break;
        case Cancelled: phase = "Cancelled"; break;
        }
        ImGui::TextWrapped("%s - %s", phase, resource.path.View().data());
        if (const auto *texture = std::get_if<TextureTarget>(&resource.target))
        {
            const auto entity = scene.Get(texture->surface.entity);
            ImGui::TextWrapped("%s / Primitive %u / %s", entity ? entity->record.id.c_str() : "Retired entity",
                               texture->surface.primitive, TextureNames[static_cast<std::size_t>(texture->slot)]);
        }
        if (resource.phase == Failed)
        {
            ImGui::TextWrapped("%s", resource.reason.View().data());
        }
        if (resource.phase == Pending && ImGui::SmallButton("Cancel source edit"))
        {
            frame.cancelPhysicalSource = true;
        }
    }

    void DrawPhysicalInspector(const Scene::Scene &scene, std::optional<Scene::EntityHandle> selected,
                               const PhysicalResourceStatus &resource, bool allowEdits, WorkspaceFrame &frame)
    {
        PushIdentity(scene.Id().generation);
        ImGui::BeginDisabled(!allowEdits);
        if (const auto entity = selected ? scene.Get(*selected) : std::nullopt)
        {
            PushIdentity(selected->generation);
            if (entity->record.properties.light)
            {
                DrawLight(*selected, *entity->record.properties.light, frame);
            }
            if (entity->model)
            {
                const auto &model = **entity->model;
                const auto report = model.supportReport.View();
                ImGui::TextWrapped("Model revision %llu: %s", static_cast<unsigned long long>(model.revision.value),
                                   report.empty() ? "Static core profile" : report.data());
                if (entity->record.properties.castsShadow && entity->sourceNode && entity->sourceNode->mesh)
                {
                    PushIdentity(model.asset.generation);
                    PushIdentity(model.revision.value);
                    ImGui::SeparatorText("Instance surfaces");
                    for (const auto &primitive : model.meshes[*entity->sourceNode->mesh].primitives)
                    {
                        ImGui::PushID(static_cast<int>(primitive.sourceSlot));
                        if (ImGui::TreeNode("Surface", "Primitive %u", primitive.sourceSlot))
                        {
                            DrawSurface(scene, {*selected, model.asset, model.revision, primitive.sourceSlot},
                                        entity->record, primitive, frame);
                            ImGui::TreePop();
                        }
                        ImGui::PopID();
                    }
                    PopIdentity();
                    PopIdentity();
                }
            }
            PopIdentity();
        }
        DrawEnvironment(scene, frame);
        DrawResourceStatus(scene, resource, frame);
        ImGui::EndDisabled();
        PopIdentity();
    }
}
