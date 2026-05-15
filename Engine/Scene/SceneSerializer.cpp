#include "SceneSerializer.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/trigonometric.hpp>
#include <nlohmann/json.hpp>

#include <Engine/Core/Log.hpp>
#include <Engine/Scene/Components/CameraComponent.hpp>
#include <Engine/Scene/Components/LightComponent.hpp>
#include <Engine/Scene/Components/RelationshipComponent.hpp>
#include <Engine/Scene/Components/TagComponent.hpp>
#include <Engine/Scene/Components/TransformComponent.hpp>
#include <Engine/Scene/Entity.hpp>
#include <Engine/Scene/Scene.hpp>
#include <Platform/FileSystem/FileSystem.hpp>

namespace Physara::Engine
{
    namespace SceneSerializerDetail
    {
        using json = nlohmann::json;

        json Vec2ToJson(const glm::vec2 &v)
        {
            return json::array({v.x, v.y});
        }

        json Vec3ToJson(const glm::vec3 &v)
        {
            return json::array({v.x, v.y, v.z});
        }

        json Vec4ToJson(const glm::vec4 &v)
        {
            return json::array({v.x, v.y, v.z, v.w});
        }

        glm::vec2 JsonToVec2(const json &value, const glm::vec2 &fallback = glm::vec2(0.f))
        {
            if (!value.is_array() || value.size() < 2)
            {
                return fallback;
            }

            return {value.at(0).get<float>(), value.at(1).get<float>()};
        }

        glm::vec3 JsonToVec3(const json &value, const glm::vec3 &fallback = glm::vec3(0.f))
        {
            if (!value.is_array() || value.size() < 3)
            {
                return fallback;
            }

            return {value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>()};
        }

        glm::vec4 JsonToVec4(const json &value, const glm::vec4 &fallback = glm::vec4(0.f))
        {
            if (!value.is_array() || value.size() < 4)
            {
                return fallback;
            }

            return {value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>(), value.at(3).get<float>()};
        }

        void CollectEntityRecursive(const Scene &scene, EntityId entity, std::vector<EntityId> &out)
        {
            const auto &registry = scene.GetRegistry();
            if (!registry.valid(entity))
            {
                return;
            }

            out.push_back(entity);

            const auto *relationship = registry.try_get<RelationshipComponent>(entity);
            if (relationship == nullptr)
            {
                return;
            }

            for (EntityId child = relationship->firstChild; child != NullEntity;)
            {
                const auto *childRelationship = registry.try_get<RelationshipComponent>(child);
                const EntityId next = childRelationship != nullptr ? childRelationship->nextSibling : NullEntity;
                CollectEntityRecursive(scene, child, out);
                child = next;
            }
        }
    }

    bool SceneSerializer::Serialize(const Scene &scene, const std::filesystem::path &path)
    {
        try
        {
            std::vector<EntityId> orderedEntities;
            const auto &rootRegistry = scene.GetRegistry();
            auto rootView = rootRegistry.view<RelationshipComponent>();
            for (EntityId rootEntity : rootView)
            {
                const auto &relationship = rootView.get<RelationshipComponent>(rootEntity);
                if (relationship.IsRoot())
                {
                    SceneSerializerDetail::CollectEntityRecursive(scene, rootEntity, orderedEntities);
                }
            }

            std::unordered_map<EntityId, std::uint64_t> ids;
            ids.reserve(orderedEntities.size());
            for (std::size_t i = 0; i < orderedEntities.size(); ++i)
            {
                ids.emplace(orderedEntities[i], static_cast<std::uint64_t>(i + 1));
            }

            SceneSerializerDetail::json root;
            root["format"] = "PhysaraScene";
            root["version"] = 1;
            root["entities"] = SceneSerializerDetail::json::array();

            const auto &registry = scene.GetRegistry();
            for (EntityId entity : orderedEntities)
            {
                SceneSerializerDetail::json e;
                e["id"] = ids.at(entity);

                if (const auto *tag = registry.try_get<TagComponent>(entity))
                {
                    e["tag"] = {{"name", tag->name}};
                }

                if (const auto *relationship = registry.try_get<RelationshipComponent>(entity))
                {
                    e["relationship"] = {
                        {"parent", relationship->parent != NullEntity && ids.contains(relationship->parent)
                                       ? ids.at(relationship->parent)
                                       : 0u}};
                }

                if (const auto *transform = registry.try_get<TransformComponent>(entity))
                {
                    e["transform"] = {
                        {"position", SceneSerializerDetail::Vec3ToJson(transform->localPosition)},
                        {"rotationDegrees", SceneSerializerDetail::Vec3ToJson(glm::degrees(transform->GetLocalEulerRotation()))},
                        {"scale", SceneSerializerDetail::Vec3ToJson(transform->localScale)}};
                }

                if (const auto *camera = registry.try_get<CameraComponent>(entity))
                {
                    SceneSerializerDetail::json cameraJson;
                    cameraJson["projection"] = std::string(ToString(camera->projectionType));
                    cameraJson["primary"] = camera->primary;
                    cameraJson["sensorWidthMillimeters"] = camera->sensorWidthMillimeters;
                    cameraJson["sensorHeightMillimeters"] = camera->sensorHeightMillimeters;
                    cameraJson["focalLengthMillimeters"] = camera->focalLengthMillimeters;
                    cameraJson["apertureFStop"] = camera->apertureFStop;
                    cameraJson["shutterTimeSeconds"] = camera->shutterTimeSeconds;
                    cameraJson["iso"] = camera->iso;
                    cameraJson["nearClipMeters"] = camera->nearClipMeters;
                    cameraJson["farClipMeters"] = camera->farClipMeters;
                    cameraJson["orthographicHeightMeters"] = camera->orthographicHeightMeters;
                    e["camera"] = std::move(cameraJson);
                }

                if (const auto *light = registry.try_get<LightComponent>(entity))
                {
                    SceneSerializerDetail::json lightJson;
                    lightJson["type"] = std::string(ToString(light->type));
                    lightJson["color"] = SceneSerializerDetail::Vec3ToJson(light->color);
                    lightJson["colorTemperatureKelvin"] = light->colorTemperatureKelvin;
                    lightJson["useColorTemperature"] = light->useColorTemperature;
                    lightJson["directionalIlluminanceLux"] = light->directionalIlluminanceLux;
                    lightJson["pointLuminousPowerLumens"] = light->pointLuminousPowerLumens;
                    lightJson["spotLuminousIntensityCandela"] = light->spotLuminousIntensityCandela;
                    lightJson["areaLuminanceCandelaPerSquareMeter"] = light->areaLuminanceCandelaPerSquareMeter;
                    lightJson["rangeMeters"] = light->rangeMeters;
                    lightJson["sourceRadiusMeters"] = light->sourceRadiusMeters;
                    lightJson["areaSizeMeters"] = SceneSerializerDetail::Vec2ToJson(light->areaSizeMeters);
                    lightJson["innerConeAngleDegrees"] = glm::degrees(light->innerConeAngleRadians);
                    lightJson["outerConeAngleDegrees"] = glm::degrees(light->outerConeAngleRadians);
                    lightJson["castsShadow"] = light->castsShadow;
                    lightJson["shadowBias"] = light->shadowBias;
                    lightJson["iesProfilePath"] = light->iesProfilePath;
                    e["light"] = std::move(lightJson);
                }

                root["entities"].push_back(std::move(e));
            }

            const std::string sceneText = root.dump(4);
            Platform::FileSystem::WriteFile(path.string(),
                                            std::vector<std::uint8_t>(sceneText.begin(), sceneText.end()));
            return true;
        }
        catch (const std::exception &e)
        {
            PHYSARA_CORE_ERROR("Scene serialization failed: {}", e.what());
            return false;
        }
    }

    bool SceneSerializer::Deserialize(Scene &scene, const std::filesystem::path &path)
    {
        try
        {
            const std::string sceneText = Platform::FileSystem::ReadTextFile(path.string());
            const SceneSerializerDetail::json root = SceneSerializerDetail::json::parse(sceneText);
            if (root.value("format", std::string{}) != "PhysaraScene")
            {
                PHYSARA_CORE_ERROR("Unsupported scene file format: {}", path.string());
                return false;
            }

            const SceneSerializerDetail::json &entities = root.at("entities");
            if (!entities.is_array())
            {
                PHYSARA_CORE_ERROR("Scene file has invalid entities array: {}", path.string());
                return false;
            }

            scene.Clear();

            std::unordered_map<std::uint64_t, Entity> idToEntity;
            idToEntity.reserve(entities.size());

            for (const SceneSerializerDetail::json &serialized : entities)
            {
                const std::uint64_t id = serialized.at("id").get<std::uint64_t>();
                const std::string name = serialized.value("tag", SceneSerializerDetail::json::object()).value("name", std::string{"Entity"});
                Entity entity = scene.CreateEntity(name);

                if (serialized.contains("transform"))
                {
                    const SceneSerializerDetail::json &t = serialized.at("transform");
                    auto &transform = entity.GetComponent<TransformComponent>();
                    transform.SetLocalTRS(SceneSerializerDetail::JsonToVec3(t.value("position", SceneSerializerDetail::json::array())),
                                          glm::radians(SceneSerializerDetail::JsonToVec3(t.value("rotationDegrees", SceneSerializerDetail::json::array()))),
                                          SceneSerializerDetail::JsonToVec3(t.value("scale", SceneSerializerDetail::json::array()), glm::vec3(1.f)));
                }

                if (serialized.contains("camera"))
                {
                    const SceneSerializerDetail::json &c = serialized.at("camera");
                    CameraComponent camera{};
                    camera.projectionType = CameraProjectionTypeFromString(c.value("projection", std::string{"Perspective"}));
                    camera.primary = c.value("primary", false);
                    camera.sensorWidthMillimeters = c.value("sensorWidthMillimeters", camera.sensorWidthMillimeters);
                    camera.sensorHeightMillimeters = c.value("sensorHeightMillimeters", camera.sensorHeightMillimeters);
                    camera.focalLengthMillimeters = c.value("focalLengthMillimeters", camera.focalLengthMillimeters);
                    camera.apertureFStop = c.value("apertureFStop", camera.apertureFStop);
                    camera.shutterTimeSeconds = c.value("shutterTimeSeconds", camera.shutterTimeSeconds);
                    camera.iso = c.value("iso", camera.iso);
                    camera.nearClipMeters = c.value("nearClipMeters", camera.nearClipMeters);
                    camera.farClipMeters = c.value("farClipMeters", camera.farClipMeters);
                    camera.orthographicHeightMeters = c.value("orthographicHeightMeters", camera.orthographicHeightMeters);
                    camera.Sanitize();
                    entity.AddOrReplaceComponent<CameraComponent>(camera);
                }

                if (serialized.contains("light"))
                {
                    const SceneSerializerDetail::json &l = serialized.at("light");
                    LightComponent light{};
                    light.type = LightTypeFromString(l.value("type", std::string{"Directional"}));
                    light.color = SceneSerializerDetail::JsonToVec3(l.value("color", SceneSerializerDetail::json::array()), glm::vec3(1.f));
                    light.colorTemperatureKelvin = l.value("colorTemperatureKelvin", light.colorTemperatureKelvin);
                    light.useColorTemperature = l.value("useColorTemperature", light.useColorTemperature);
                    light.directionalIlluminanceLux = l.value("directionalIlluminanceLux", light.directionalIlluminanceLux);
                    light.pointLuminousPowerLumens = l.value("pointLuminousPowerLumens", light.pointLuminousPowerLumens);
                    light.spotLuminousIntensityCandela = l.value("spotLuminousIntensityCandela", light.spotLuminousIntensityCandela);
                    light.areaLuminanceCandelaPerSquareMeter =
                        l.value("areaLuminanceCandelaPerSquareMeter", light.areaLuminanceCandelaPerSquareMeter);
                    light.rangeMeters = l.value("rangeMeters", light.rangeMeters);
                    light.sourceRadiusMeters = l.value("sourceRadiusMeters", light.sourceRadiusMeters);
                    light.areaSizeMeters = SceneSerializerDetail::JsonToVec2(l.value("areaSizeMeters", SceneSerializerDetail::json::array()), light.areaSizeMeters);
                    light.innerConeAngleRadians = glm::radians(l.value("innerConeAngleDegrees", glm::degrees(light.innerConeAngleRadians)));
                    light.outerConeAngleRadians = glm::radians(l.value("outerConeAngleDegrees", glm::degrees(light.outerConeAngleRadians)));
                    light.castsShadow = l.value("castsShadow", light.castsShadow);
                    light.shadowBias = l.value("shadowBias", light.shadowBias);
                    light.iesProfilePath = l.value("iesProfilePath", light.iesProfilePath);
                    light.Sanitize();
                    entity.AddOrReplaceComponent<LightComponent>(light);
                }

                idToEntity.emplace(id, entity);
            }

            for (const SceneSerializerDetail::json &serialized : entities)
            {
                const std::uint64_t id = serialized.at("id").get<std::uint64_t>();
                const std::uint64_t parentId =
                    serialized.value("relationship", SceneSerializerDetail::json::object()).value("parent", std::uint64_t{0});

                if (parentId != 0 && idToEntity.contains(id) && idToEntity.contains(parentId))
                {
                    scene.SetParent(idToEntity.at(id), idToEntity.at(parentId));
                }
            }

            scene.EnsureSceneCamera();
            scene.UpdateTransforms();
            return true;
        }
        catch (const std::exception &e)
        {
            PHYSARA_CORE_ERROR("Scene deserialization failed: {}", e.what());
            return false;
        }
    }
}