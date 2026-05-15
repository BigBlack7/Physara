#include "GLTFLoader.hpp"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <memory>
#include <numbers>
#include <string>
#include <unordered_set>
#include <vector>

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <tinygltf/tiny_gltf_v3.h>

#include <Engine/Core/Log.hpp>
#include <Engine/Resource/AssetManager.hpp>
#include <Engine/Resource/Types/Material.hpp>
#include <Engine/Resource/Types/Mesh.hpp>
#include <Engine/Scene/Components/LightComponent.hpp>
#include <Engine/Scene/Components/MaterialComponent.hpp>
#include <Engine/Scene/Components/MeshComponent.hpp>
#include <Engine/Scene/Components/TagComponent.hpp>
#include <Engine/Scene/Components/TransformComponent.hpp>
#include <Engine/Scene/Scene.hpp>
#include <Platform/FileSystem/FileSystem.hpp>

namespace Physara::Engine
{
    namespace GLTFLoaderDetail
    {
        std::string ToString(tg3_str value)
        {
            return value.data != nullptr && value.len > 0 ? std::string(value.data, value.len) : std::string{};
        }

        bool Equals(tg3_str value, const char *text)
        {
            // 比较 tg3_str 与 C 字符串是否相等
            return tg3_str_equals_cstr(value, text) != 0;
        }

        std::string NormalizeAssetPath(const std::filesystem::path &path, AssetManager *assetManager)
        {
            if (assetManager != nullptr)
            {
                return assetManager->NormalizePath(path);
            }
            return Platform::FileSystem::NormalizeForCompare(Platform::FileSystem::ToAssetsRelativePath(path.string()));
        }

        // 生成glTF内嵌材质的合成路径
        std::string SyntheticMaterialPath(const std::filesystem::path &gltfPath, std::uint32_t materialIndex,
                                          AssetManager *assetManager)
        {
            return NormalizeAssetPath(gltfPath, assetManager) + "#material/" + std::to_string(materialIndex);
        }

        glm::vec3 ReadVec3(const double *values, glm::vec3 fallback)
        {
            if (values == nullptr)
            {
                return fallback;
            }
            return glm::vec3(static_cast<float>(values[0]), static_cast<float>(values[1]), static_cast<float>(values[2]));
        }

        glm::vec4 ReadVec4(const double *values, glm::vec4 fallback)
        {
            if (values == nullptr)
            {
                return fallback;
            }
            return glm::vec4(static_cast<float>(values[0]), static_cast<float>(values[1]),
                             static_cast<float>(values[2]), static_cast<float>(values[3]));
        }

        glm::quat ReadQuat(const double *values)
        {
            return glm::normalize(glm::quat(static_cast<float>(values[3]),
                                            static_cast<float>(values[0]),
                                            static_cast<float>(values[1]),
                                            static_cast<float>(values[2])));
        }

        void ApplyNodeTransform(TransformComponent &transform, const tg3_node &node)
        {
            if (node.has_matrix)
            {
                glm::mat4 matrix{1.f};
                for (int column = 0; column < 4; ++column)
                {
                    for (int row = 0; row < 4; ++row)
                    {
                        matrix[column][row] = static_cast<float>(node.matrix[column * 4 + row]);
                    }
                }

                const glm::vec3 position = glm::vec3(matrix[3]); // 平移
                glm::vec3 scale{
                    glm::length(glm::vec3(matrix[0])),
                    glm::length(glm::vec3(matrix[1])),
                    glm::length(glm::vec3(matrix[2]))}; // 缩放

                glm::mat3 rotationMatrix{1.f};
                if (scale.x > 0.f)
                {
                    rotationMatrix[0] = glm::vec3(matrix[0]) / scale.x;
                }
                if (scale.y > 0.f)
                {
                    rotationMatrix[1] = glm::vec3(matrix[1]) / scale.y;
                }
                if (scale.z > 0.f)
                {
                    rotationMatrix[2] = glm::vec3(matrix[2]) / scale.z;
                }

                transform.SetLocalTRS(position, glm::quat_cast(rotationMatrix), scale);
                return;
            }

            transform.SetLocalTRS(ReadVec3(node.translation, glm::vec3(0.f)),
                                  ReadQuat(node.rotation),
                                  ReadVec3(node.scale, glm::vec3(1.f)));
        }

        // 根据索引安全地获取访问器指针
        const tg3_accessor *AccessorAt(const tg3_model &model, int32_t index)
        {
            if (index < 0 || static_cast<std::uint32_t>(index) >= model.accessors_count)
            {
                return nullptr;
            }
            return &model.accessors[index];
        }

        // 在Primitive的属性列表中查找指定名称的属性, 并返回其索引
        int32_t FindAttribute(const tg3_primitive &primitive, const char *name)
        {
            for (std::uint32_t i = 0; i < primitive.attributes_count; ++i)
            {
                if (Equals(primitive.attributes[i].key, name))
                {
                    return primitive.attributes[i].value;
                }
            }
            return TG3_INDEX_NONE;
        }

        // 构建MeshPrimitive信息, 包括顶点数、索引数、包围盒等
        MeshPrimitive BuildPrimitiveInfo(const tg3_model &model, const tg3_primitive &primitive, std::uint32_t primitiveIndex)
        {
            MeshPrimitive result{};
            result.primitiveIndex = primitiveIndex;
            result.materialIndex = primitive.material >= 0 ? static_cast<std::uint32_t>(primitive.material) : 0u;

            if (const tg3_accessor *indices = AccessorAt(model, primitive.indices))
            {
                result.indexCount = indices->count;
            }

            const int32_t positionAccessorIndex = FindAttribute(primitive, "POSITION");
            if (const tg3_accessor *positions = AccessorAt(model, positionAccessorIndex))
            {
                result.vertexCount = positions->count;
                if (positions->min_values_count >= 3 && positions->max_values_count >= 3)
                {
                    result.boundsMin = ReadVec3(positions->min_values, glm::vec3(0.f));
                    result.boundsMax = ReadVec3(positions->max_values, glm::vec3(0.f));
                    result.hasBounds = true;
                }
            }

            return result;
        }

        std::string ImageUriFromTextureIndex(const tg3_model &model, int32_t textureIndex)
        {
            if (textureIndex < 0 || static_cast<std::uint32_t>(textureIndex) >= model.textures_count)
            {
                return {};
            }

            const tg3_texture &texture = model.textures[textureIndex];
            if (texture.source < 0 || static_cast<std::uint32_t>(texture.source) >= model.images_count)
            {
                return {};
            }

            return ToString(model.images[texture.source].uri);
        }

        // 从纹理信息结构体生成纹理的完整路径
        std::string TexturePathFromInfo(const tg3_model &model, const std::filesystem::path &gltfPath,
                                        const tg3_texture_info &info, AssetManager *assetManager)
        {
            const std::string uri = ImageUriFromTextureIndex(model, info.index);
            return uri.empty() ? std::string{} : NormalizeAssetPath(gltfPath.parent_path() / uri, assetManager);
        }

        // 从纹理索引生成纹理的完整路径
        std::string TexturePathFromIndex(const tg3_model &model, const std::filesystem::path &gltfPath,
                                         int32_t textureIndex, AssetManager *assetManager)
        {
            const std::string uri = ImageUriFromTextureIndex(model, textureIndex);
            return uri.empty() ? std::string{} : NormalizeAssetPath(gltfPath.parent_path() / uri, assetManager);
        }

        Material ConvertMaterial(const tg3_model &model, const std::filesystem::path &gltfPath,
                                 std::uint32_t materialIndex, AssetManager *assetManager)
        {
            Material material{};
            material.materialIndex = materialIndex;
            material.path = SyntheticMaterialPath(gltfPath, materialIndex, assetManager);

            if (materialIndex >= model.materials_count)
            {
                material.name = "Default Material";
                return material;
            }

            const tg3_material &source = model.materials[materialIndex];
            material.name = ToString(source.name);
            if (material.name.empty())
            {
                material.name = "Material " + std::to_string(materialIndex);
            }

            material.baseColor = ReadVec4(source.pbr_metallic_roughness.base_color_factor, glm::vec4(1.f));
            material.metallic = static_cast<float>(source.pbr_metallic_roughness.metallic_factor);
            material.roughness = static_cast<float>(source.pbr_metallic_roughness.roughness_factor);
            material.doubleSided = source.double_sided != 0;
            material.alphaCutoff = static_cast<float>(source.alpha_cutoff);
            material.emissiveColor = ReadVec3(source.emissive_factor, glm::vec3(0.f));
            material.normalScale = static_cast<float>(source.normal_texture.scale);

            if (Equals(source.alpha_mode, "MASK"))
            {
                material.alphaMode = AlphaMode::Mask;
            }
            else if (Equals(source.alpha_mode, "BLEND"))
            {
                material.alphaMode = AlphaMode::Blend;
                material.castShadow = false;
            }

            material.baseColorTexture = TextureSlot(
                TexturePathFromInfo(model, gltfPath, source.pbr_metallic_roughness.base_color_texture, assetManager),
                static_cast<std::uint32_t>(std::max(source.pbr_metallic_roughness.base_color_texture.tex_coord, 0)));
            material.metallicRoughnessTexture = TextureSlot(
                TexturePathFromInfo(model, gltfPath, source.pbr_metallic_roughness.metallic_roughness_texture, assetManager),
                static_cast<std::uint32_t>(std::max(source.pbr_metallic_roughness.metallic_roughness_texture.tex_coord, 0)));
            material.normalTexture = TextureSlot(
                TexturePathFromIndex(model, gltfPath, source.normal_texture.index, assetManager),
                static_cast<std::uint32_t>(std::max(source.normal_texture.tex_coord, 0)));
            material.occlusionTexture = TextureSlot(
                TexturePathFromIndex(model, gltfPath, source.occlusion_texture.index, assetManager),
                static_cast<std::uint32_t>(std::max(source.occlusion_texture.tex_coord, 0)));
            material.emissiveTexture = TextureSlot(
                TexturePathFromInfo(model, gltfPath, source.emissive_texture, assetManager),
                static_cast<std::uint32_t>(std::max(source.emissive_texture.tex_coord, 0)));

            return material;
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
            component.ambientOcclusion = material.ambientOcclusion;
            component.alphaCutoff = material.alphaCutoff;
            component.emissiveColor = material.emissiveColor;
            component.normalScale = material.normalScale;
            component.baseColorTexture = TextureSlot(material.baseColorTexture.path, material.baseColorTexture.texCoord);
            component.metallicRoughnessTexture = TextureSlot(material.metallicRoughnessTexture.path, material.metallicRoughnessTexture.texCoord);
            component.normalTexture = TextureSlot(material.normalTexture.path, material.normalTexture.texCoord);
            component.occlusionTexture = TextureSlot(material.occlusionTexture.path, material.occlusionTexture.texCoord);
            component.emissiveTexture = TextureSlot(material.emissiveTexture.path, material.emissiveTexture.texCoord);
            component.Sanitize();
            return component;
        }

        void ApplyLight(const tg3_model &model, const tg3_node &node, Entity entity)
        {
            if (node.light < 0 || static_cast<std::uint32_t>(node.light) >= model.lights_count)
            {
                return;
            }

            const tg3_light &source = model.lights[node.light];
            LightComponent light{};
            if (Equals(source.type, "point"))
            {
                light.type = LightType::Point;
                light.pointLuminousPowerLumens = static_cast<float>(source.intensity * 4.0 * std::numbers::pi_v<double>);
            }
            else if (Equals(source.type, "spot"))
            {
                light.type = LightType::Spot;
                light.spotLuminousIntensityCandela = static_cast<float>(source.intensity);
                light.innerConeAngleRadians = static_cast<float>(source.spot.inner_cone_angle);
                light.outerConeAngleRadians = static_cast<float>(source.spot.outer_cone_angle);
            }
            else
            {
                light.type = LightType::Directional;
                light.directionalIlluminanceLux = static_cast<float>(source.intensity);
            }

            light.color = ReadVec3(source.color, glm::vec3(1.f));
            if (source.range > 0.0)
            {
                light.rangeMeters = static_cast<float>(source.range);
            }
            light.Sanitize();
            entity.AddOrReplaceComponent<LightComponent>(light);
        }

        void RegisterResources(const tg3_model &model, const std::filesystem::path &gltfPath, AssetManager *assetManager)
        {
            if (assetManager == nullptr)
            {
                return;
            }

            for (std::uint32_t materialIndex = 0; materialIndex < model.materials_count; ++materialIndex)
            {
                auto material = std::make_shared<Material>(ConvertMaterial(model, gltfPath, materialIndex, assetManager));
                (void)assetManager->RegisterAsset<Material>(material->path, material);
            }

            const std::string assetPath = assetManager->NormalizePath(gltfPath);
            for (std::uint32_t meshIndex = 0; meshIndex < model.meshes_count; ++meshIndex)
            {
                const tg3_mesh &source = model.meshes[meshIndex];
                auto mesh = std::make_shared<Mesh>();
                mesh->path = assetPath + "#mesh/" + std::to_string(meshIndex);
                mesh->meshIndex = meshIndex;
                mesh->name = ToString(source.name);
                for (std::uint32_t primitiveIndex = 0; primitiveIndex < source.primitives_count; ++primitiveIndex)
                {
                    mesh->primitives.push_back(BuildPrimitiveInfo(model, source.primitives[primitiveIndex], primitiveIndex));
                }
                (void)assetManager->RegisterAsset<Mesh>(mesh->path, mesh);
            }
        }

        void ApplyPrimitive(Scene &scene, Entity owner, const tg3_model &model, const std::filesystem::path &gltfPath,
                            const std::string &assetPath, std::uint32_t meshIndex, std::uint32_t primitiveIndex,
                            AssetManager *assetManager)
        {
            const tg3_mesh &mesh = model.meshes[meshIndex];
            const tg3_primitive &primitive = mesh.primitives[primitiveIndex];

            MeshComponent meshComponent(assetPath, meshIndex, primitiveIndex);
            const MeshPrimitive primitiveInfo = BuildPrimitiveInfo(model, primitive, primitiveIndex);
            if (primitiveInfo.hasBounds)
            {
                meshComponent.localBounds.min = primitiveInfo.boundsMin;
                meshComponent.localBounds.max = primitiveInfo.boundsMax;
                meshComponent.localBounds.center = (primitiveInfo.boundsMin + primitiveInfo.boundsMax) * 0.5f;
                meshComponent.localBounds.radius = glm::length(primitiveInfo.boundsMax - meshComponent.localBounds.center);
                meshComponent.localBounds.valid = true;
            }

            if (primitive.material >= 0)
            {
                const std::uint32_t materialIndex = static_cast<std::uint32_t>(primitive.material);
                const Material material = ConvertMaterial(model, gltfPath, materialIndex, assetManager);
                meshComponent.materialSlots.emplace_back(0u, material.path);
                owner.AddOrReplaceComponent<MaterialComponent>(ToComponent(material));
            }

            owner.AddOrReplaceComponent<MeshComponent>(std::move(meshComponent));
            (void)scene;
        }

        Entity ImportNode(Scene &scene, const tg3_model &model, const std::filesystem::path &gltfPath,
                          const std::string &assetPath, int32_t nodeIndex, Entity parent,
                          AssetManager *assetManager)
        {
            if (nodeIndex < 0 || static_cast<std::uint32_t>(nodeIndex) >= model.nodes_count)
            {
                return {};
            }

            const tg3_node &node = model.nodes[nodeIndex];
            std::string nodeName = ToString(node.name);
            if (nodeName.empty())
            {
                nodeName = "Node " + std::to_string(nodeIndex);
            }

            Entity entity = scene.CreateEntity(nodeName);
            ApplyNodeTransform(entity.GetComponent<TransformComponent>(), node);
            if (parent)
            {
                scene.SetParent(entity, parent);
            }

            ApplyLight(model, node, entity);

            if (node.mesh >= 0 && static_cast<std::uint32_t>(node.mesh) < model.meshes_count)
            {
                const std::uint32_t meshIndex = static_cast<std::uint32_t>(node.mesh);
                const tg3_mesh &mesh = model.meshes[meshIndex];
                if (mesh.primitives_count == 1)
                {
                    ApplyPrimitive(scene, entity, model, gltfPath, assetPath, meshIndex, 0, assetManager);
                }
                else
                {
                    for (std::uint32_t primitiveIndex = 0; primitiveIndex < mesh.primitives_count; ++primitiveIndex)
                    {
                        const std::string primitiveName =
                            (ToString(mesh.name).empty() ? "Primitive" : ToString(mesh.name)) + " " + std::to_string(primitiveIndex);
                        Entity primitiveEntity = scene.CreateEntity(primitiveName);
                        scene.SetParent(primitiveEntity, entity);
                        ApplyPrimitive(scene, primitiveEntity, model, gltfPath, assetPath, meshIndex, primitiveIndex, assetManager);
                    }
                }
            }

            for (std::uint32_t childIndex = 0; childIndex < node.children_count; ++childIndex)
            {
                ImportNode(scene, model, gltfPath, assetPath, node.children[childIndex], entity, assetManager);
            }

            return entity;
        }

        // 当glTF文件没有定义任何场景时, 自动找出所有没有父节点的节点作为根节点
        std::vector<int32_t> FindFallbackRootNodes(const tg3_model &model)
        {
            std::unordered_set<int32_t> children;
            for (std::uint32_t nodeIndex = 0; nodeIndex < model.nodes_count; ++nodeIndex)
            {
                const tg3_node &node = model.nodes[nodeIndex];
                for (std::uint32_t childIndex = 0; childIndex < node.children_count; ++childIndex)
                {
                    children.insert(node.children[childIndex]);
                }
            }

            std::vector<int32_t> roots;
            for (std::uint32_t nodeIndex = 0; nodeIndex < model.nodes_count; ++nodeIndex)
            {
                if (!children.contains(static_cast<int32_t>(nodeIndex)))
                {
                    roots.push_back(static_cast<int32_t>(nodeIndex));
                }
            }
            return roots;
        }
    }

    Entity GLTFLoader::LoadToScene(Scene &scene, const std::filesystem::path &path, AssetManager *assetManager)
    {
        const std::filesystem::path gltfPath = path.lexically_normal();

        tinygltf3::Model model;
        tinygltf3::ErrorStack errors;
        tg3_parse_options options{};
        tg3_parse_options_init(&options);
        options.images_as_is = 1;

        std::vector<std::uint8_t> fileData;
        try
        {
            fileData = Platform::FileSystem::ReadBinaryFile(gltfPath.string());
        }
        catch (const std::exception &error)
        {
            PHYSARA_CORE_ERROR("Failed to read GLTF '{}': {}", gltfPath.string(), error.what());
            return {};
        }

        const std::string baseDir = Platform::FileSystem::NormalizePath(gltfPath.parent_path().string());
        const tg3_error_code result = tinygltf3::parse(model,
                                                       errors,
                                                       fileData.data(),
                                                       static_cast<std::uint64_t>(fileData.size()),
                                                       baseDir.c_str(),
                                                       &options);
        if (result != TG3_OK)
        {
            PHYSARA_CORE_ERROR("Failed to parse GLTF '{}'. Error code: {}", gltfPath.string(), static_cast<int>(result));
            for (std::uint32_t i = 0; i < errors.count(); ++i)
            {
                const tg3_error_entry *entry = errors.entry(i);
                if (entry != nullptr && entry->message != nullptr)
                {
                    PHYSARA_CORE_ERROR("GLTF parser: {}", entry->message);
                }
            }
            return {};
        }

        const tg3_model &gltf = *model.get();
        GLTFLoaderDetail::RegisterResources(gltf, gltfPath, assetManager);

        const std::string rootName = gltfPath.stem().string().empty() ? "GLTF Scene" : gltfPath.stem().string();
        Entity importRoot = scene.CreateEntity(rootName);

        const std::string assetPath = GLTFLoaderDetail::NormalizeAssetPath(gltfPath, assetManager);
        std::vector<int32_t> rootNodes;
        if (gltf.default_scene >= 0 && static_cast<std::uint32_t>(gltf.default_scene) < gltf.scenes_count)
        {
            const tg3_scene &defaultScene = gltf.scenes[gltf.default_scene];
            rootNodes.assign(defaultScene.nodes, defaultScene.nodes + defaultScene.nodes_count);
        }
        else if (gltf.scenes_count > 0)
        {
            const tg3_scene &firstScene = gltf.scenes[0];
            rootNodes.assign(firstScene.nodes, firstScene.nodes + firstScene.nodes_count);
        }
        else
        {
            rootNodes = GLTFLoaderDetail::FindFallbackRootNodes(gltf);
        }

        for (int32_t nodeIndex : rootNodes)
        {
            GLTFLoaderDetail::ImportNode(scene, gltf, gltfPath, assetPath, nodeIndex, importRoot, assetManager);
        }

        scene.UpdateTransforms();
        PHYSARA_CORE_INFO("Imported GLTF '{}': {} nodes, {} meshes, {} materials.",
                          gltfPath.string(), gltf.nodes_count, gltf.meshes_count, gltf.materials_count);
        return importRoot;
    }
}