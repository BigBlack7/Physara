#include "RenderSystem.hpp"

#include <algorithm>
#include <functional>
#include <memory>
#include <string_view>

#include <glm/vec4.hpp>

#include <Engine/Scene/Components/MeshComponent.hpp>
#include <Engine/Scene/Components/TransformComponent.hpp>
#include <Engine/Resource/AssetManager.hpp>
#include <Engine/Resource/Types/Material.hpp>
#include <Engine/Scene/Scene.hpp>

namespace Physara::Engine
{
    namespace RenderSystemDetail
    {
        float GetMaxScaleAxis(const glm::mat4 &matrix)
        {
            const float x = glm::length(glm::vec3(matrix[0]));
            const float y = glm::length(glm::vec3(matrix[1]));
            const float z = glm::length(glm::vec3(matrix[2]));
            return std::max(x, std::max(y, z));
        }

        MaterialComponent ToComponent(const Material &material)
        {
            MaterialComponent component{};
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
            return component;
        }

        bool ResolveResourceMaterial(
            AssetManager *assetManager,
            const std::string &materialPath,
            RenderSystemCollectScratch *scratch,
            MaterialComponent &outMaterial)
        {
            if (assetManager == nullptr || materialPath.empty())
            {
                return false;
            }

            if (scratch != nullptr)
            {
                const auto cached = scratch->resourceMaterials.find(materialPath);
                if (cached != scratch->resourceMaterials.end())
                {
                    outMaterial = cached->second;
                    return true;
                }
            }

            const std::shared_ptr<Material> resource = assetManager->GetByPath<Material>(materialPath);
            if (resource == nullptr)
            {
                return false;
            }

            outMaterial = ToComponent(*resource);
            if (scratch != nullptr)
            {
                scratch->resourceMaterials.emplace(materialPath, outMaterial);
            }
            return true;
        }

        void ApplyResourceTransparency(MaterialComponent &material, const MaterialComponent &resource)
        {
            if (resource.alphaMode != AlphaMode::Blend)
            {
                return;
            }

            material.alphaMode = AlphaMode::Blend;
            material.baseColor.a = std::min(material.baseColor.a, resource.baseColor.a);
            material.castShadow = false;
        }

        const MaterialSlotRef *FindMaterialSlot(const MeshComponent &mesh)
        {
            const auto exact = std::find_if(mesh.materialSlots.begin(), mesh.materialSlots.end(), [&mesh](const MaterialSlotRef &slot)
                                            {
                                                return slot.slotIndex == mesh.primitive.primitiveIndex;
                                            });
            if (exact != mesh.materialSlots.end())
            {
                return &*exact;
            }

            const auto fallback = std::find_if(mesh.materialSlots.begin(), mesh.materialSlots.end(), [](const MaterialSlotRef &slot)
                                               {
                                                   return slot.slotIndex == 0u;
                                               });
            return fallback != mesh.materialSlots.end() ? &*fallback : nullptr;
        }

        MaterialComponent GetMaterial(
            const entt::registry &registry,
            EntityId entity,
            const MeshComponent &mesh,
            AssetManager *assetManager,
            RenderSystemCollectScratch *scratch)
        {
            MaterialComponent material{};
            const auto *component = registry.try_get<MaterialComponent>(entity);
            if (component != nullptr)
            {
                material = *component;
            }

            const MaterialSlotRef *slot = FindMaterialSlot(mesh);
            if (slot != nullptr && slot->HasOverride())
            {
                material.materialPath = slot->materialPath;
            }

            MaterialComponent resourceMaterial{};
            if (ResolveResourceMaterial(assetManager, material.materialPath, scratch, resourceMaterial))
            {
                if (component == nullptr)
                {
                    material = resourceMaterial;
                }
                else
                {
                    ApplyResourceTransparency(material, resourceMaterial);
                }
            }

            material.Sanitize();
            return material;
        }

        std::uint64_t HashString(std::string_view value)
        {
            return static_cast<std::uint64_t>(std::hash<std::string_view>{}(value));
        }

        void HashCombine(std::uint64_t &seed, std::uint64_t value)
        {
            seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6u) + (seed >> 2u);
        }

        std::uint64_t BuildMeshKey(const MeshPrimitiveRef &primitive)
        {
            std::uint64_t seed = HashString(primitive.assetPath);
            HashCombine(seed, static_cast<std::uint64_t>(primitive.meshIndex));
            return seed;
        }

        std::uint64_t BuildPrimitiveKey(const MeshPrimitiveRef &primitive, std::uint64_t meshKey)
        {
            std::uint64_t seed = meshKey;
            HashCombine(seed, static_cast<std::uint64_t>(primitive.primitiveIndex));
            return seed;
        }
    }

    void RenderSystem::Collect(
        Scene &scene,
        std::vector<RenderMeshSubmission> &submissions,
        AssetManager *assetManager,
        RenderSystemCollectScratch *scratch)
    {
        auto &registry = scene.GetRegistry();
        auto view = registry.view<MeshComponent, TransformComponent>();

        submissions.clear();
        submissions.reserve(view.size_hint());
        if (scratch != nullptr)
        {
            scratch->Clear();
            scratch->resourceMaterials.reserve(view.size_hint());
        }

        view.each([&submissions, &registry, assetManager, scratch](EntityId entity, const MeshComponent &mesh, const TransformComponent &transform)
        {
            if (!mesh.visible || !mesh.HasMesh())
            {
                return;
            }

            RenderMeshSubmission submission{};
            submission.entity = entity;
            submission.meshPath = mesh.primitive.assetPath;
            submission.meshIndex = mesh.primitive.meshIndex;
            submission.primitiveIndex = mesh.primitive.primitiveIndex;
            submission.meshKey = RenderSystemDetail::BuildMeshKey(mesh.primitive);
            submission.primitiveKey = RenderSystemDetail::BuildPrimitiveKey(mesh.primitive, submission.meshKey);
            submission.material = RenderSystemDetail::GetMaterial(registry, entity, mesh, assetManager, scratch);
            submission.model = transform.GetWorldMatrix();
            submission.inverseTransposeModel = transform.GetInverseTransposeWorldMatrix();
            submission.receiveShadows = mesh.receiveShadows;

            if (mesh.localBounds.valid)
            {
                submission.boundsCenter = glm::vec3(submission.model * glm::vec4(mesh.localBounds.center, 1.f));
                submission.boundsRadius = mesh.localBounds.radius * RenderSystemDetail::GetMaxScaleAxis(submission.model);
                submission.hasBounds = true;
            }

            submissions.push_back(std::move(submission));
        });
    }
}
