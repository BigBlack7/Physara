#include "GLTFLoader.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <memory>
#include <numbers>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <tinygltf/tiny_gltf_v3.h>

#include <Engine/Core/Log.hpp>
#include <Engine/Resource/AssetManager.hpp>
#include <Engine/Resource/Loaders/TextureLoader.hpp>
#include <Engine/Resource/Types/Material.hpp>
#include <Engine/Resource/Types/Mesh.hpp>
#include <Engine/Resource/Types/Texture.hpp>
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

        const tg3_value *FindObjectValue(const tg3_value &object, const char *key)
        {
            if (object.type != TG3_VALUE_OBJECT || key == nullptr)
            {
                return nullptr;
            }

            for (std::uint32_t i = 0; i < object.object_count; ++i)
            {
                if (Equals(object.object_data[i].key, key))
                {
                    return &object.object_data[i].value;
                }
            }
            return nullptr;
        }

        const tg3_value *FindExtensionValue(const tg3_extras_ext &ext, const char *name)
        {
            for (std::uint32_t i = 0; i < ext.extensions_count; ++i)
            {
                if (Equals(ext.extensions[i].name, name))
                {
                    return &ext.extensions[i].value;
                }
            }
            return nullptr;
        }

        float ReadNumberValue(const tg3_value *value, float fallback)
        {
            if (value == nullptr)
            {
                return fallback;
            }
            if (value->type == TG3_VALUE_REAL)
            {
                return static_cast<float>(value->real_val);
            }
            if (value->type == TG3_VALUE_INT)
            {
                return static_cast<float>(value->int_val);
            }
            return fallback;
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

        const tg3_buffer_view *BufferViewAt(const tg3_model &model, int32_t index)
        {
            if (index < 0 || static_cast<std::uint32_t>(index) >= model.buffer_views_count)
            {
                return nullptr;
            }
            return &model.buffer_views[index];
        }

        const std::uint8_t *AccessorData(const tg3_model &model, const tg3_accessor &accessor)
        {
            const tg3_buffer_view *bufferView = BufferViewAt(model, accessor.buffer_view);
            if (bufferView == nullptr || bufferView->buffer < 0 || static_cast<std::uint32_t>(bufferView->buffer) >= model.buffers_count)
            {
                return nullptr;
            }

            const tg3_buffer &buffer = model.buffers[bufferView->buffer];
            const std::uint64_t offset = bufferView->byte_offset + accessor.byte_offset;
            if (offset >= buffer.data.count)
            {
                return nullptr;
            }

            return buffer.data.data + offset;
        }

        float ReadFloatComponent(const std::uint8_t *data, int32_t componentType)
        {
            if (componentType == TG3_COMPONENT_TYPE_FLOAT)
            {
                float value = 0.f;
                std::memcpy(&value, data, sizeof(float));
                return value;
            }
            if (componentType == TG3_COMPONENT_TYPE_UNSIGNED_SHORT)
            {
                std::uint16_t value = 0;
                std::memcpy(&value, data, sizeof(std::uint16_t));
                return static_cast<float>(value);
            }
            if (componentType == TG3_COMPONENT_TYPE_SHORT)
            {
                std::int16_t value = 0;
                std::memcpy(&value, data, sizeof(std::int16_t));
                return static_cast<float>(value);
            }
            if (componentType == TG3_COMPONENT_TYPE_UNSIGNED_BYTE)
            {
                return static_cast<float>(*data);
            }
            if (componentType == TG3_COMPONENT_TYPE_BYTE)
            {
                std::int8_t value = 0;
                std::memcpy(&value, data, sizeof(std::int8_t));
                return static_cast<float>(value);
            }
            return 0.f;
        }

        std::uint32_t ReadIndexComponent(const std::uint8_t *data, int32_t componentType)
        {
            if (componentType == TG3_COMPONENT_TYPE_UNSIGNED_INT)
            {
                std::uint32_t value = 0;
                std::memcpy(&value, data, sizeof(std::uint32_t));
                return value;
            }
            if (componentType == TG3_COMPONENT_TYPE_UNSIGNED_SHORT)
            {
                std::uint16_t value = 0;
                std::memcpy(&value, data, sizeof(std::uint16_t));
                return value;
            }
            if (componentType == TG3_COMPONENT_TYPE_UNSIGNED_BYTE)
            {
                return *data;
            }
            return 0u;
        }

        int32_t AccessorStride(const tg3_model &model, const tg3_accessor &accessor)
        {
            const tg3_buffer_view *bufferView = BufferViewAt(model, accessor.buffer_view);
            return tg3_accessor_byte_stride(&accessor, bufferView);
        }

        glm::vec2 ReadVec2Attribute(const tg3_model &model, const tg3_accessor *accessor, std::uint64_t vertexIndex, glm::vec2 fallback)
        {
            if (accessor == nullptr)
            {
                return fallback;
            }
            const std::uint8_t *data = AccessorData(model, *accessor);
            if (data == nullptr)
            {
                return fallback;
            }

            const int32_t componentSize = tg3_component_size(accessor->component_type);
            const std::uint8_t *element = data + vertexIndex * static_cast<std::uint64_t>(AccessorStride(model, *accessor));
            return {
                ReadFloatComponent(element, accessor->component_type),
                ReadFloatComponent(element + componentSize, accessor->component_type)};
        }

        glm::vec3 ReadVec3Attribute(const tg3_model &model, const tg3_accessor *accessor, std::uint64_t vertexIndex, glm::vec3 fallback)
        {
            if (accessor == nullptr)
            {
                return fallback;
            }
            const std::uint8_t *data = AccessorData(model, *accessor);
            if (data == nullptr)
            {
                return fallback;
            }

            const int32_t componentSize = tg3_component_size(accessor->component_type);
            const std::uint8_t *element = data + vertexIndex * static_cast<std::uint64_t>(AccessorStride(model, *accessor));
            return {
                ReadFloatComponent(element, accessor->component_type),
                ReadFloatComponent(element + componentSize, accessor->component_type),
                ReadFloatComponent(element + componentSize * 2, accessor->component_type)};
        }

        glm::vec4 ReadVec4Attribute(const tg3_model &model, const tg3_accessor *accessor, std::uint64_t vertexIndex, glm::vec4 fallback)
        {
            if (accessor == nullptr)
            {
                return fallback;
            }
            const std::uint8_t *data = AccessorData(model, *accessor);
            if (data == nullptr)
            {
                return fallback;
            }

            const int32_t componentSize = tg3_component_size(accessor->component_type);
            const std::uint8_t *element = data + vertexIndex * static_cast<std::uint64_t>(AccessorStride(model, *accessor));
            return {
                ReadFloatComponent(element, accessor->component_type),
                ReadFloatComponent(element + componentSize, accessor->component_type),
                ReadFloatComponent(element + componentSize * 2, accessor->component_type),
                ReadFloatComponent(element + componentSize * 3, accessor->component_type)};
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

        glm::vec3 BuildFallbackTangent(const glm::vec3 &normal)
        {
            const glm::vec3 axis = std::abs(normal.y) < 0.9f ? glm::vec3(0.f, 1.f, 0.f) : glm::vec3(1.f, 0.f, 0.f);
            return glm::normalize(glm::cross(axis, normal));
        }

        void GenerateNormals(MeshPrimitive &primitive)
        {
            std::vector<glm::vec3> accum(primitive.vertices.size(), glm::vec3(0.f));
            for (std::size_t i = 0; i + 2u < primitive.indices.size(); i += 3u)
            {
                const std::uint32_t ia = primitive.indices[i];
                const std::uint32_t ib = primitive.indices[i + 1u];
                const std::uint32_t ic = primitive.indices[i + 2u];
                if (ia >= primitive.vertices.size() || ib >= primitive.vertices.size() || ic >= primitive.vertices.size())
                {
                    continue;
                }

                const glm::vec3 &a = primitive.vertices[ia].position;
                const glm::vec3 &b = primitive.vertices[ib].position;
                const glm::vec3 &c = primitive.vertices[ic].position;
                const glm::vec3 normal = glm::cross(b - a, c - a);
                if (glm::dot(normal, normal) <= 0.f)
                {
                    continue;
                }

                accum[ia] += normal;
                accum[ib] += normal;
                accum[ic] += normal;
            }

            for (std::size_t i = 0; i < primitive.vertices.size(); ++i)
            {
                primitive.vertices[i].normal = glm::dot(accum[i], accum[i]) > 0.f
                                                   ? glm::normalize(accum[i])
                                                   : glm::vec3(0.f, 0.f, 1.f);
            }
        }

        void GenerateTangents(MeshPrimitive &primitive)
        {
            std::vector<glm::vec3> tangentAccum(primitive.vertices.size(), glm::vec3(0.f));
            std::vector<glm::vec3> bitangentAccum(primitive.vertices.size(), glm::vec3(0.f));

            for (std::size_t i = 0; i + 2u < primitive.indices.size(); i += 3u)
            {
                const std::uint32_t ia = primitive.indices[i];
                const std::uint32_t ib = primitive.indices[i + 1u];
                const std::uint32_t ic = primitive.indices[i + 2u];
                if (ia >= primitive.vertices.size() || ib >= primitive.vertices.size() || ic >= primitive.vertices.size())
                {
                    continue;
                }

                const MeshVertex &a = primitive.vertices[ia];
                const MeshVertex &b = primitive.vertices[ib];
                const MeshVertex &c = primitive.vertices[ic];
                const glm::vec3 edge1 = b.position - a.position;
                const glm::vec3 edge2 = c.position - a.position;
                const glm::vec2 uv1 = b.texCoord0 - a.texCoord0;
                const glm::vec2 uv2 = c.texCoord0 - a.texCoord0;
                const float determinant = uv1.x * uv2.y - uv1.y * uv2.x;
                if (std::abs(determinant) <= 1e-8f)
                {
                    continue;
                }

                const float invDet = 1.f / determinant;
                const glm::vec3 tangent = (edge1 * uv2.y - edge2 * uv1.y) * invDet;
                const glm::vec3 bitangent = (edge2 * uv1.x - edge1 * uv2.x) * invDet;
                tangentAccum[ia] += tangent;
                tangentAccum[ib] += tangent;
                tangentAccum[ic] += tangent;
                bitangentAccum[ia] += bitangent;
                bitangentAccum[ib] += bitangent;
                bitangentAccum[ic] += bitangent;
            }

            for (std::size_t i = 0; i < primitive.vertices.size(); ++i)
            {
                const glm::vec3 normal = glm::normalize(primitive.vertices[i].normal);
                glm::vec3 tangent = tangentAccum[i] - normal * glm::dot(normal, tangentAccum[i]);
                if (glm::dot(tangent, tangent) <= 0.f)
                {
                    tangent = BuildFallbackTangent(normal);
                }
                else
                {
                    tangent = glm::normalize(tangent);
                }

                const float handedness = glm::dot(glm::cross(normal, tangent), bitangentAccum[i]) < 0.f ? -1.f : 1.f;
                primitive.vertices[i].tangent = glm::vec4(tangent, handedness);
            }
        }

        void BuildPrimitiveGeometry(const tg3_model &model, const tg3_primitive &primitive, MeshPrimitive &result)
        {
            const tg3_accessor *positions = AccessorAt(model, FindAttribute(primitive, "POSITION"));
            if (positions == nullptr || positions->component_type != TG3_COMPONENT_TYPE_FLOAT || positions->type != TG3_TYPE_VEC3)
            {
                PHYSARA_CORE_WARN("GLTF primitive {} has no supported POSITION accessor.", result.primitiveIndex);
                return;
            }

            if (AccessorData(model, *positions) == nullptr)
            {
                PHYSARA_CORE_WARN("GLTF primitive {} POSITION accessor has no loaded buffer data.", result.primitiveIndex);
                return;
            }

            const tg3_accessor *normals = AccessorAt(model, FindAttribute(primitive, "NORMAL"));
            const tg3_accessor *tangents = AccessorAt(model, FindAttribute(primitive, "TANGENT"));
            const tg3_accessor *texCoords = AccessorAt(model, FindAttribute(primitive, "TEXCOORD_0"));
            const bool hasNormals = normals != nullptr && normals->component_type == TG3_COMPONENT_TYPE_FLOAT && normals->type == TG3_TYPE_VEC3;
            const bool hasTangents = tangents != nullptr && tangents->component_type == TG3_COMPONENT_TYPE_FLOAT && tangents->type == TG3_TYPE_VEC4;

            result.vertices.resize(static_cast<std::size_t>(positions->count));
            for (std::uint64_t i = 0; i < positions->count; ++i)
            {
                MeshVertex vertex{};
                vertex.position = ReadVec3Attribute(model, positions, i, glm::vec3(0.f));
                vertex.normal = hasNormals ? glm::normalize(ReadVec3Attribute(model, normals, i, glm::vec3(0.f, 0.f, 1.f))) : glm::vec3(0.f, 0.f, 1.f);
                vertex.tangent = hasTangents ? ReadVec4Attribute(model, tangents, i, glm::vec4(1.f, 0.f, 0.f, 1.f)) : glm::vec4(1.f, 0.f, 0.f, 1.f);
                vertex.texCoord0 = ReadVec2Attribute(model, texCoords, i, glm::vec2(0.f));
                result.vertices[static_cast<std::size_t>(i)] = vertex;
            }

            const tg3_accessor *indices = AccessorAt(model, primitive.indices);
            if (indices != nullptr)
            {
                const std::uint8_t *indexData = AccessorData(model, *indices);
                const int32_t componentSize = tg3_component_size(indices->component_type);
                const int32_t stride = AccessorStride(model, *indices);
                if (indexData != nullptr && componentSize > 0 && stride > 0)
                {
                    result.indices.resize(static_cast<std::size_t>(indices->count));
                    for (std::uint64_t i = 0; i < indices->count; ++i)
                    {
                        result.indices[static_cast<std::size_t>(i)] =
                            ReadIndexComponent(indexData + i * static_cast<std::uint64_t>(stride), indices->component_type);
                    }
                }
            }
            else
            {
                result.indices.resize(result.vertices.size());
                for (std::uint32_t i = 0; i < result.indices.size(); ++i)
                {
                    result.indices[i] = i;
                }
            }

            result.vertexCount = result.vertices.size();
            result.indexCount = result.indices.size();

            if (!hasNormals)
            {
                GenerateNormals(result);
            }
            if (!hasTangents)
            {
                GenerateTangents(result);
            }
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

            if (const tg3_value *transmission = FindExtensionValue(source.ext, "KHR_materials_transmission"))
            {
                const float transmissionFactor = ReadNumberValue(FindObjectValue(*transmission, "transmissionFactor"), 0.f);
                if (transmissionFactor > 0.f)
                {
                    material.alphaMode = AlphaMode::Blend;
                    material.baseColor.a = std::min(material.baseColor.a, 1.f - transmissionFactor * 0.75f);
                    material.castShadow = false;
                }
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

        void TryRegisterTexture(const std::string &texturePath, AssetManager *assetManager, std::unordered_set<std::string> &registered)
        {
            if (texturePath.empty() || assetManager == nullptr)
            {
                return;
            }

            const std::string normalizedPath = assetManager->NormalizePath(texturePath);
            if (!registered.insert(normalizedPath).second || assetManager->GetByPath<Texture>(normalizedPath) != nullptr)
            {
                return;
            }

            std::shared_ptr<Texture> texture = TextureLoader::LoadRGBA8(normalizedPath);
            if (texture == nullptr || !texture->IsLoaded())
            {
                PHYSARA_CORE_WARN("GLTF texture '{}' could not be loaded.", normalizedPath);
                return;
            }

            texture->path = normalizedPath;
            (void)assetManager->RegisterAsset<Texture>(normalizedPath, texture);
            PHYSARA_CORE_INFO("Registered GLTF texture '{}': {}x{}.", normalizedPath, texture->width, texture->height);
        }

        void RegisterMaterialTextures(const Material &material, AssetManager *assetManager, std::unordered_set<std::string> &registered)
        {
            TryRegisterTexture(material.baseColorTexture.path, assetManager, registered);
            TryRegisterTexture(material.metallicRoughnessTexture.path, assetManager, registered);
            TryRegisterTexture(material.normalTexture.path, assetManager, registered);
            TryRegisterTexture(material.occlusionTexture.path, assetManager, registered);
            TryRegisterTexture(material.emissiveTexture.path, assetManager, registered);
        }

        void ApplyBaseColorAlphaPolicy(Material &material, AssetManager *assetManager)
        {
            if (assetManager == nullptr || material.baseColorTexture.path.empty())
            {
                return;
            }

            const std::shared_ptr<Texture> texture = assetManager->GetByPath<Texture>(material.baseColorTexture.path);
            if (texture == nullptr || !texture->hasTransparentPixels)
            {
                return;
            }

            if (material.alphaMode == AlphaMode::Opaque)
            {
                material.alphaMode = AlphaMode::Mask;
                material.alphaCutoff = 0.5f;
                PHYSARA_CORE_INFO("GLTF material '{}' promoted OPAQUE -> MASK because baseColor texture '{}' contains alpha.",
                                  material.name,
                                  material.baseColorTexture.path);
            }
            else if (material.alphaMode == AlphaMode::Blend && !texture->hasPartialAlphaPixels)
            {
                material.alphaMode = AlphaMode::Mask;
                material.alphaCutoff = 0.5f;
                PHYSARA_CORE_INFO("GLTF material '{}' promoted BLEND -> MASK because baseColor texture '{}' has binary cutout alpha.",
                                  material.name,
                                  material.baseColorTexture.path);
            }
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
                PHYSARA_CORE_WARN("GLTF resource registration skipped for '{}' because AssetManager is null.", gltfPath.string());
                return;
            }

            std::unordered_set<std::string> registeredTextures;
            for (std::uint32_t materialIndex = 0; materialIndex < model.materials_count; ++materialIndex)
            {
                auto material = std::make_shared<Material>(ConvertMaterial(model, gltfPath, materialIndex, assetManager));
                RegisterMaterialTextures(*material, assetManager, registeredTextures);
                ApplyBaseColorAlphaPolicy(*material, assetManager);
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
                    MeshPrimitive primitive = BuildPrimitiveInfo(model, source.primitives[primitiveIndex], primitiveIndex);
                    BuildPrimitiveGeometry(model, source.primitives[primitiveIndex], primitive);
                    PHYSARA_CORE_INFO("Registered GLTF mesh {} primitive {}: vertices={}, indices={}, bounds={}.",
                                      meshIndex,
                                      primitiveIndex,
                                      primitive.vertices.size(),
                                      primitive.indices.size(),
                                      primitive.hasBounds ? "yes" : "no");
                    mesh->primitives.push_back(std::move(primitive));
                }
                (void)assetManager->RegisterAsset<Mesh>(mesh->path, mesh);
                PHYSARA_CORE_INFO("Registered GLTF mesh resource '{}'.", assetManager->NormalizePath(mesh->path));
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
                Material material = ConvertMaterial(model, gltfPath, materialIndex, assetManager);
                ApplyBaseColorAlphaPolicy(material, assetManager);
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

    bool GLTFLoader::LoadResources(const std::filesystem::path &path, AssetManager *assetManager)
    {
        if (assetManager == nullptr)
        {
            return false;
        }

        const std::filesystem::path gltfPath = Platform::FileSystem::ResolvePath(path.string());

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
            PHYSARA_CORE_ERROR("Failed to read GLTF resources '{}': {}", gltfPath.string(), error.what());
            return false;
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
            PHYSARA_CORE_ERROR("Failed to parse GLTF resources '{}'. Error code: {}", gltfPath.string(), static_cast<int>(result));
            return false;
        }

        GLTFLoaderDetail::RegisterResources(*model.get(), gltfPath, assetManager);
        return true;
    }

    Entity GLTFLoader::LoadToScene(Scene &scene, const std::filesystem::path &path, AssetManager *assetManager)
    {
        const std::filesystem::path gltfPath = Platform::FileSystem::ResolvePath(path.string());
        PHYSARA_CORE_INFO("Loading GLTF scene '{}' with AssetManager={}.",
                          gltfPath.string(),
                          assetManager != nullptr ? "yes" : "no");

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