#pragma once

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <typeinfo>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec4.hpp>
#include <imgui/imgui.h>

#include <Editor/Core/EditorContext.hpp>
#include <Engine/Resource/AssetManager.hpp>
#include <Engine/Resource/AssetPath.hpp>
#include <Engine/Resource/Types/Material.hpp>
#include <Engine/Scene/Components/TagComponent.hpp>
#include <Engine/Scene/Components/CameraComponent.hpp>
#include <Engine/Scene/Components/LightComponent.hpp>
#include <Engine/Scene/Components/MaterialComponent.hpp>
#include <Engine/Scene/Components/MeshComponent.hpp>
#include <Engine/Scene/Components/TransformComponent.hpp>
#include <Engine/Scene/Entity.hpp>
#include <Engine/Scene/Scene.hpp>

namespace Physara::Editor
{
    namespace ComponentDrawerDetail
    {
        inline std::vector<std::string> CollectTextureAssets(const std::filesystem::path &assetsRoot)
        {
            static std::filesystem::path cachedRoot;
            static std::vector<std::string> cachedTextures;
            if (cachedRoot == assetsRoot)
            {
                return cachedTextures;
            }

            cachedRoot = assetsRoot;
            cachedTextures.clear();
            std::vector<std::string> result;
            if (assetsRoot.empty() || !std::filesystem::exists(assetsRoot))
            {
                return result;
            }

            std::error_code error;
            for (const std::filesystem::directory_entry &entry : std::filesystem::recursive_directory_iterator(assetsRoot, error))
            {
                if (error)
                {
                    break;
                }
                if (!entry.is_regular_file(error) || !Engine::AssetPath::IsTextureFile(entry.path()))
                {
                    continue;
                }

                std::filesystem::path relative = std::filesystem::relative(entry.path(), assetsRoot, error);
                result.push_back((error ? entry.path() : relative).generic_string());
            }

            std::sort(result.begin(), result.end());
            cachedTextures = result;
            return cachedTextures;
        }

        inline std::vector<std::string> CollectCachedMaterialResources(const Engine::AssetManager *assetManager)
        {
            std::vector<std::string> result;
            if (assetManager == nullptr)
            {
                return result;
            }

            const std::string materialType = typeid(Engine::Material).name();
            for (const Engine::AssetRecordInfo &record : assetManager->GetCachedAssets())
            {
                if (record.loaded && record.typeName == materialType)
                {
                    result.push_back(record.normalizedPath);
                }
            }

            std::sort(result.begin(), result.end());
            return result;
        }

        inline bool DrawAssetPopup(const char *popupId, std::string &path, const std::vector<std::string> &candidates)
        {
            bool changed = false;
            if (ImGui::BeginPopup(popupId))
            {
                if (candidates.empty())
                {
                    ImGui::TextDisabled("No matching assets are available.");
                }
                else
                {
                    for (const std::string &candidate : candidates)
                    {
                        const bool selected = path == candidate;
                        if (ImGui::Selectable(candidate.c_str(), selected))
                        {
                            path = candidate;
                            changed = true;
                            ImGui::CloseCurrentPopup();
                        }
                    }
                }
                ImGui::EndPopup();
            }

            return changed;
        }

        inline bool DrawReadOnlyPathSelector(const char *label, std::string &path, const char *popupId, const std::vector<std::string> &candidates, const char *emptyText)
        {
            bool changed = false;
            ImGui::TextUnformatted(label);
            const char *displayPath = path.empty() ? emptyText : path.c_str();
            std::array<char, 384> displayBuffer{};
            std::snprintf(displayBuffer.data(), displayBuffer.size(), "%s", displayPath);
            ImGui::BeginDisabled();
            ImGui::InputText("##Path", displayBuffer.data(), displayBuffer.size(), ImGuiInputTextFlags_ReadOnly);
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::SmallButton("Select"))
            {
                ImGui::OpenPopup(popupId);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Clear"))
            {
                path.clear();
                changed = true;
            }

            changed |= DrawAssetPopup(popupId, path, candidates);
            return changed;
        }

        inline bool DrawTextureSlot(const char *label, Engine::TextureSlot &slot, const EditorContext &context)
        {
            bool changed = false;
            ImGui::PushID(label);
            std::vector<std::string> textures = CollectTextureAssets(context.assetsRootPath);
            changed |= DrawReadOnlyPathSelector(label, slot.path, "SelectTexture", textures, "<none>");

            int texCoord = static_cast<int>(slot.texCoord);
            if (ImGui::SliderInt("UV Set", &texCoord, 0, 1, "UV%d"))
            {
                slot.texCoord = static_cast<std::uint32_t>(texCoord);
                changed = true;
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Chooses which mesh texture coordinate attribute this texture samples.");
            }
            ImGui::PopID();
            return changed;
        }

        inline bool DrawTextureControlledScalar(
            const char *name,
            bool textureBound,
            float &value,
            float &textureInfluence,
            float valueMin,
            float valueMax)
        {
            bool changed = false;
            ImGui::PushID(name);
            if (textureBound)
            {
                const std::string fallbackLabel = std::string(name) + " Fallback";
                changed |= ImGui::SliderFloat(fallbackLabel.c_str(), &value, valueMin, valueMax, "%.3f");
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Used when texture influence is reduced or the texture slot is cleared.");
                }

                const std::string influenceLabel = std::string(name) + " Texture Influence";
                changed |= ImGui::SliderFloat(influenceLabel.c_str(), &textureInfluence, 0.f, 1.f, "%.3f");
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("1 uses the texture channel; 0 uses the fallback value.");
                }
            }
            else
            {
                changed |= ImGui::SliderFloat(name, &value, valueMin, valueMax, "%.3f");
            }
            ImGui::PopID();
            return changed;
        }

        inline Engine::MaterialComponent ToComponent(const Engine::Material &material)
        {
            Engine::MaterialComponent component{};
            component.materialPath = material.path;
            component.shadingModel = material.shadingModel;
            component.alphaMode = material.alphaMode;
            component.doubleSided = material.doubleSided;
            component.castShadow = material.castShadow;
            component.baseColor = material.baseColor;
            component.metallic = material.metallic;
            component.roughness = material.roughness;
            component.reflectance = material.reflectance;
            component.ambientOcclusion = material.ambientOcclusion;
            component.alphaCutoff = material.alphaCutoff;
            component.metallicTextureInfluence = material.metallicTextureInfluence;
            component.roughnessTextureInfluence = material.roughnessTextureInfluence;
            component.ambientOcclusionTextureInfluence = material.ambientOcclusionTextureInfluence;
            component.emissiveColor = material.emissiveColor;
            component.normalScale = material.normalScale;
            component.flipNormalY = material.flipNormalY;
            component.baseColorTexture = material.baseColorTexture;
            component.metallicRoughnessTexture = material.metallicRoughnessTexture;
            component.normalTexture = material.normalTexture;
            component.occlusionTexture = material.occlusionTexture;
            component.emissiveTexture = material.emissiveTexture;
            component.Sanitize();
            return component;
        }

        inline bool ApplyMaterialResourceToEntity(Engine::Entity entity, Engine::AssetManager *assetManager, const std::string &materialPath)
        {
            if (assetManager == nullptr || materialPath.empty())
            {
                return false;
            }

            const std::shared_ptr<Engine::Material> material = assetManager->GetByPath<Engine::Material>(materialPath);
            if (material == nullptr)
            {
                return false;
            }

            entity.AddOrReplaceComponent<Engine::MaterialComponent>(ToComponent(*material));
            return true;
        }
    }

    template <typename T>
    bool DrawComponent(Engine::Entity entity, EditorContext &context, Engine::AssetManager *assetManager)
    {
        (void)entity;
        (void)context;
        (void)assetManager;
        return false;
    }

    template <>
    inline bool DrawComponent<Engine::TagComponent>(Engine::Entity entity, EditorContext &context, Engine::AssetManager *assetManager)
    {
        (void)context;
        (void)assetManager;
        auto &tag = entity.GetComponent<Engine::TagComponent>();

        std::array<char, 128> buffer{};
        std::snprintf(buffer.data(), buffer.size(), "%s", tag.name.c_str());

        if (ImGui::InputText("Name", buffer.data(), buffer.size()))
        {
            tag.name = buffer.data();
            return true;
        }

        return false;
    }

    template <>
    inline bool DrawComponent<Engine::TransformComponent>(Engine::Entity entity, EditorContext &context, Engine::AssetManager *assetManager)
    {
        (void)assetManager;
        auto &transform = entity.GetComponent<Engine::TransformComponent>();
        if (context.activeScene == nullptr)
        {
            return false;
        }

        context.activeScene->UpdateTransforms();
        glm::vec3 position{};
        glm::quat rotation{};
        glm::vec3 scale{};
        if (!transform.GetWorldTRS(position, rotation, scale))
        {
            ImGui::TextDisabled("World transform is not decomposable.");
            return false;
        }

        bool changed = ImGui::DragFloat3("World Position", &position.x, 0.05f);

        glm::vec3 rotationDegrees = glm::degrees(glm::eulerAngles(rotation));
        if (ImGui::DragFloat3("World Rotation", &rotationDegrees.x, 0.25f))
        {
            changed = true;
        }

        if (ImGui::DragFloat3("World Scale", &scale.x, 0.02f, 0.001f, 0.f))
        {
            changed = true;
        }

        if (changed)
        {
            context.activeScene->SetWorldTransform(
                entity.GetHandle(),
                position,
                glm::quat(glm::radians(rotationDegrees)),
                scale);
        }

        return changed;
    }

    template <>
    inline bool DrawComponent<Engine::CameraComponent>(Engine::Entity entity, EditorContext &context, Engine::AssetManager *assetManager)
    {
        (void)context;
        (void)assetManager;
        auto &camera = entity.GetComponent<Engine::CameraComponent>();
        bool changed = false;

        int projectionIndex = static_cast<int>(camera.projectionType);
        const char *projectionLabels[] = {"Perspective", "Orthographic"};
        if (ImGui::Combo("Projection", &projectionIndex, projectionLabels, IM_ARRAYSIZE(projectionLabels)))
        {
            camera.projectionType = static_cast<Engine::CameraProjectionType>(projectionIndex);
            changed = true;
        }

        camera.primary = true;
        changed |= ImGui::DragFloat("Sensor Width", &camera.sensorWidthMillimeters, 0.1f, 1.f, 100.f, "%.1f mm");
        changed |= ImGui::DragFloat("Sensor Height", &camera.sensorHeightMillimeters, 0.1f, 1.f, 100.f, "%.1f mm");
        changed |= ImGui::DragFloat("Focal Length", &camera.focalLengthMillimeters, 0.25f, 1.f, 1000.f, "%.1f mm");
        changed |= ImGui::DragFloat("Aperture", &camera.apertureFStop, 0.05f, 0.5f, 64.f, "f/%.1f");
        changed |= ImGui::DragFloat("Shutter", &camera.shutterTimeSeconds, 0.0005f, 1.f / 25000.f, 60.f, "%.4f s");
        changed |= ImGui::DragFloat("ISO", &camera.iso, 1.f, 10.f, 204800.f, "%.0f", ImGuiSliderFlags_Logarithmic);
        changed |= ImGui::SliderFloat("Exposure Compensation EV", &camera.exposureCompensationEV, -16.f, 16.f, "%.2f");
        changed |= ImGui::SliderFloat("Navigation Speed", &camera.navigationSpeedMetersPerSecond, 0.1f, 100.f, "%.1f m/s", ImGuiSliderFlags_Logarithmic);
        changed |= ImGui::DragFloat("Near Clip", &camera.nearClipMeters, 0.01f, 0.001f, 100.f, "%.3f m");
        changed |= ImGui::DragFloat("Far Clip", &camera.farClipMeters, 1.f, 0.01f, 100000.f, "%.1f m");

        if (camera.projectionType == Engine::CameraProjectionType::Orthographic)
        {
            changed |= ImGui::DragFloat("Ortho Height", &camera.orthographicHeightMeters, 0.1f, 0.001f, 10000.f, "%.2f m");
        }

        if (changed)
        {
            camera.Sanitize();
        }

        ImGui::Text("EV100: %.2f", camera.GetEV100());
        ImGui::TextDisabled("Viewport navigation and capture use this camera.");
        return changed;
    }

    template <>
    inline bool DrawComponent<Engine::MeshComponent>(Engine::Entity entity, EditorContext &context, Engine::AssetManager *assetManager)
    {
        (void)context;
        auto &mesh = entity.GetComponent<Engine::MeshComponent>();
        bool changed = false;

        ImGui::TextWrapped("Asset: %s", mesh.primitive.assetPath.c_str());
        ImGui::Text("Mesh: %u", mesh.primitive.meshIndex);
        ImGui::Text("Primitive: %u", mesh.primitive.primitiveIndex);
        changed |= ImGui::Checkbox("Visible", &mesh.visible);
        changed |= ImGui::Checkbox("Receive Shadows", &mesh.receiveShadows);

        if (mesh.localBounds.valid)
        {
            ImGui::Text("Bounds Min: %.3f, %.3f, %.3f", mesh.localBounds.min.x, mesh.localBounds.min.y, mesh.localBounds.min.z);
            ImGui::Text("Bounds Max: %.3f, %.3f, %.3f", mesh.localBounds.max.x, mesh.localBounds.max.y, mesh.localBounds.max.z);
            ImGui::Text("Radius: %.3f", mesh.localBounds.radius);
        }

        if (!mesh.materialSlots.empty())
        {
            ImGui::SeparatorText("Material Slots");
            for (Engine::MaterialSlotRef &slot : mesh.materialSlots)
            {
                ImGui::PushID(static_cast<int>(slot.slotIndex));
                ImGui::Text("Slot %u Resource Override", slot.slotIndex);
                ImGui::TextDisabled("Normally edit the entity Material component below. This only overrides the mesh slot with another cached material resource.");
                const std::vector<std::string> materials = ComponentDrawerDetail::CollectCachedMaterialResources(assetManager);
                const bool slotChanged = ComponentDrawerDetail::DrawReadOnlyPathSelector(
                    "Material Resource",
                    slot.materialPath,
                    "SelectMaterialResource",
                    materials,
                    "<use entity material>");
                changed |= slotChanged;
                if (slotChanged && !slot.materialPath.empty())
                {
                    ComponentDrawerDetail::ApplyMaterialResourceToEntity(entity, assetManager, slot.materialPath);
                }
                ImGui::PopID();
            }
        }

        return changed;
    }

    template <>
    inline bool DrawComponent<Engine::MaterialComponent>(Engine::Entity entity, EditorContext &context, Engine::AssetManager *assetManager)
    {
        auto &material = entity.GetComponent<Engine::MaterialComponent>();
        bool changed = false;

        ImGui::TextWrapped("Material: %s", material.materialPath.empty() ? "<embedded>" : material.materialPath.c_str());
        if (!material.materialPath.empty())
        {
            const bool canReload = assetManager != nullptr && assetManager->GetByPath<Engine::Material>(material.materialPath) != nullptr;
            if (!canReload)
            {
                ImGui::BeginDisabled();
            }
            if (ImGui::SmallButton("Reload From Material Resource"))
            {
                return ComponentDrawerDetail::ApplyMaterialResourceToEntity(entity, assetManager, material.materialPath);
            }
            if (!canReload)
            {
                ImGui::EndDisabled();
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                {
                    ImGui::SetTooltip("The material resource is not currently cached. Reimport or load the source asset first.");
                }
            }
            else if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Copies the cached material resource back onto this entity material instance.");
            }
        }

        int shadingModelIndex = static_cast<int>(material.shadingModel);
        const char *shadingModelLabels[] = {"Lit", "Unlit"};
        if (ImGui::Combo("Shading Model", &shadingModelIndex, shadingModelLabels, IM_ARRAYSIZE(shadingModelLabels)))
        {
            material.shadingModel = static_cast<Engine::ShadingModel>(shadingModelIndex);
            changed = true;
        }

        int alphaModeIndex = static_cast<int>(material.alphaMode);
        const char *alphaModeLabels[] = {"Opaque", "Mask", "Blend"};
        if (ImGui::Combo("Alpha Mode", &alphaModeIndex, alphaModeLabels, IM_ARRAYSIZE(alphaModeLabels)))
        {
            material.alphaMode = static_cast<Engine::AlphaMode>(alphaModeIndex);
            changed = true;
        }
        if (material.alphaMode == Engine::AlphaMode::Mask)
        {
            changed |= ImGui::SliderFloat("Alpha Cutoff", &material.alphaCutoff, 0.f, 1.f);
        }

        changed |= ImGui::ColorEdit4("Base Color", &material.baseColor.x);
        if (material.alphaMode != Engine::AlphaMode::Opaque)
        {
            changed |= ImGui::SliderFloat("Alpha", &material.baseColor.a, 0.f, 1.f, "%.3f");
        }
        if (material.shadingModel == Engine::ShadingModel::Lit)
        {
            const bool hasMetallicRoughnessTexture = material.metallicRoughnessTexture.IsBound();
            const bool hasOcclusionTexture = material.occlusionTexture.IsBound();
            changed |= ComponentDrawerDetail::DrawTextureControlledScalar(
                "Metallic",
                hasMetallicRoughnessTexture,
                material.metallic,
                material.metallicTextureInfluence,
                0.f,
                1.f);
            changed |= ComponentDrawerDetail::DrawTextureControlledScalar(
                "Roughness",
                hasMetallicRoughnessTexture,
                material.roughness,
                material.roughnessTextureInfluence,
                0.045f,
                1.f);
            changed |= ImGui::SliderFloat("Reflectance", &material.reflectance, 0.f, 1.f);
            changed |= ComponentDrawerDetail::DrawTextureControlledScalar(
                "AO",
                hasOcclusionTexture,
                material.ambientOcclusion,
                material.ambientOcclusionTextureInfluence,
                0.f,
                1.f);
            changed |= ImGui::SliderFloat("Normal Scale", &material.normalScale, 0.f, 4.f);
            changed |= ImGui::Checkbox("Flip Normal Y", &material.flipNormalY);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Default enabled for Physara's current tangent-space convention. Disable only if a normal map looks inverted.");
            }
        }
        changed |= ImGui::ColorEdit3("Emissive", &material.emissiveColor.x);
        changed |= ImGui::DragFloat("Emissive Luminance", &material.emissiveLuminance, 1.f, 0.f, 100000.f, "%.1f cd/m2");
        changed |= ImGui::Checkbox("Double Sided", &material.doubleSided);
        changed |= ImGui::Checkbox("Cast Shadow", &material.castShadow);

        if (changed)
        {
            material.Sanitize();
        }

        if (ImGui::CollapsingHeader("Texture Slots", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TextDisabled("Texture paths are stored relative to Assets. UV Set selects UV0 or UV1 from the mesh.");
            changed |= ComponentDrawerDetail::DrawTextureSlot("Base Color", material.baseColorTexture, context);
            const bool hadMetallicRoughnessTexture = material.metallicRoughnessTexture.IsBound();
            changed |= ComponentDrawerDetail::DrawTextureSlot("ARM / Metallic Roughness", material.metallicRoughnessTexture, context);
            if (!hadMetallicRoughnessTexture && material.metallicRoughnessTexture.IsBound())
            {
                material.metallic = 0.f;
                material.roughness = 0.8f;
                material.metallicTextureInfluence = 1.f;
                material.roughnessTextureInfluence = 1.f;
                changed = true;
            }
            if (material.metallicRoughnessTexture.IsBound())
            {
                if (ImGui::SmallButton("Use ARM Red Channel as AO"))
                {
                    material.occlusionTexture.path = material.metallicRoughnessTexture.path;
                    material.occlusionTexture.texCoord = material.metallicRoughnessTexture.texCoord;
                    material.ambientOcclusion = 1.f;
                    material.ambientOcclusionTextureInfluence = 1.f;
                    changed = true;
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("ARM packing uses R=AO, G=Roughness, B=Metallic. This assigns the same image to the Occlusion slot.");
                }
            }
            changed |= ComponentDrawerDetail::DrawTextureSlot("Normal", material.normalTexture, context);
            const bool hadOcclusionTexture = material.occlusionTexture.IsBound();
            changed |= ComponentDrawerDetail::DrawTextureSlot("Occlusion", material.occlusionTexture, context);
            if (!hadOcclusionTexture && material.occlusionTexture.IsBound())
            {
                material.ambientOcclusion = 1.f;
                material.ambientOcclusionTextureInfluence = 1.f;
                changed = true;
            }
            changed |= ComponentDrawerDetail::DrawTextureSlot("Emissive", material.emissiveTexture, context);
        }

        return changed;
    }

    template <>
    inline bool DrawComponent<Engine::LightComponent>(Engine::Entity entity, EditorContext &context, Engine::AssetManager *assetManager)
    {
        (void)context;
        (void)assetManager;
        auto &light = entity.GetComponent<Engine::LightComponent>();
        bool changed = false;

        int typeIndex = static_cast<int>(light.type);
        const char *typeLabels[] = {"Directional", "Point", "Spot", "Area (Legacy / Unsupported)"};
        if (ImGui::Combo("Type", &typeIndex, typeLabels, IM_ARRAYSIZE(typeLabels)))
        {
            light.type = static_cast<Engine::LightType>(typeIndex);
            changed = true;
        }

        changed |= ImGui::ColorEdit3("Color", &light.color.x);
        changed |= ImGui::Checkbox("Use Color Temperature", &light.useColorTemperature);
        if (light.useColorTemperature)
        {
            changed |= ImGui::DragFloat("Temperature", &light.colorTemperatureKelvin, 25.f, 1000.f, 40000.f, "%.0f K");
        }

        if (light.type == Engine::LightType::Directional || light.type == Engine::LightType::Spot)
        {
            ImGui::TextDisabled("Direction comes from Transform rotation (-Z forward).");
            if (entity.HasComponent<Engine::TransformComponent>())
            {
                const glm::mat4 &world = entity.GetComponent<Engine::TransformComponent>().GetWorldMatrix();
                const glm::vec3 direction = glm::normalize(glm::vec3(world * glm::vec4(0.f, 0.f, -1.f, 0.f)));
                ImGui::Text("Direction: %.2f, %.2f, %.2f", direction.x, direction.y, direction.z);
            }
        }

        switch (light.type)
        {
        case Engine::LightType::Directional:
            changed |= ImGui::DragFloat("Illuminance", &light.directionalIlluminanceLux, 100.f, 0.f, 200000.f, "%.0f lx");
            break;
        case Engine::LightType::Point:
            changed |= ImGui::DragFloat("Power", &light.pointLuminousPowerLumens, 10.f, 0.f, 200000.f, "%.0f lm");
            changed |= ImGui::DragFloat("Range", &light.rangeMeters, 0.1f, 0.001f, 1000.f, "%.2f m");
            changed |= ImGui::DragFloat("Source Radius", &light.sourceRadiusMeters, 0.01f, 0.f, 100.f, "%.3f m");
            ImGui::Text("Intensity: %.2f cd", light.GetEffectiveLuminousIntensityCandela());
            break;
        case Engine::LightType::Spot:
        {
            changed |= ImGui::DragFloat("Intensity", &light.spotLuminousIntensityCandela, 10.f, 0.f, 200000.f, "%.0f cd");
            changed |= ImGui::DragFloat("Range", &light.rangeMeters, 0.1f, 0.001f, 1000.f, "%.2f m");
            float innerDegrees = glm::degrees(light.innerConeAngleRadians);
            float outerDegrees = glm::degrees(light.outerConeAngleRadians);
            if (ImGui::DragFloat("Inner Cone", &innerDegrees, 0.25f, 0.f, 90.f, "%.1f deg"))
            {
                light.innerConeAngleRadians = glm::radians(innerDegrees);
                changed = true;
            }
            if (ImGui::DragFloat("Outer Cone", &outerDegrees, 0.25f, 0.f, 90.f, "%.1f deg"))
            {
                light.outerConeAngleRadians = glm::radians(outerDegrees);
                changed = true;
            }
            break;
        }
        case Engine::LightType::Area:
            ImGui::TextDisabled("Area lights are retained for scene compatibility but are not submitted by the renderer.");
            changed |= ImGui::DragFloat("Luminance", &light.areaLuminanceCandelaPerSquareMeter, 10.f, 0.f, 200000.f, "%.0f cd/m2");
            changed |= ImGui::DragFloat2("Size", &light.areaSizeMeters.x, 0.01f, 0.001f, 100.f, "%.2f m");
            break;
        }

        if (light.type == Engine::LightType::Directional)
        {
            changed |= ImGui::Checkbox("Cast Cascaded Shadows", &light.castsShadow);
            if (light.castsShadow)
            {
                changed |= ImGui::DragFloat("Receiver Depth Bias", &light.shadowBias, 0.00001f, 0.f, 0.05f, "%.5f");
            }
        }
        else
        {
            ImGui::TextDisabled("Punctual shadow maps are not implemented; CSM is directional-only.");
        }

        if (light.type == Engine::LightType::Spot)
        {
            ImGui::TextDisabled("IES profiles are serialized for compatibility but are not evaluated yet.");
        }

        if (changed)
        {
            light.Sanitize();
        }

        return changed;
    }

    template <typename T>
    bool TryDrawComponent(Engine::Entity entity, const char *label, EditorContext &context, Engine::AssetManager *assetManager)
    {
        if (!entity.HasComponent<T>())
        {
            return false;
        }

        bool changed = false;
        if (ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::PushID(label);
            changed = DrawComponent<T>(entity, context, assetManager);
            ImGui::PopID();
        }

        return changed;
    }
}
