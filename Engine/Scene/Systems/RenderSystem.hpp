#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <Engine/Scene/Components/MaterialComponent.hpp>
#include <Engine/Scene/EntityId.hpp>

namespace Physara::Engine
{
    class AssetManager;
    class Scene;

    struct RenderMeshSubmission
    {
        EntityId entity{NullEntity};
        std::string meshPath{};
        std::uint32_t meshIndex{0};
        std::uint32_t primitiveIndex{0};
        std::uint64_t meshKey{0};
        std::uint64_t primitiveKey{0};
        MaterialComponent material{};
        glm::mat4 model{1.f};
        glm::mat4 inverseTransposeModel{1.f};
        glm::vec3 boundsCenter{0.f};
        float boundsRadius{0.f};
        bool hasBounds{false};
        bool receiveShadows{true};
    };

    struct RenderSystemCollectScratch
    {
        std::unordered_map<std::string, MaterialComponent> resourceMaterials{};

        void Clear()
        {
            resourceMaterials.clear();
        }
    };

    class RenderSystem final
    {
    public:
        // Caller owns Scene::UpdateTransforms() so render and light collection can share one authoritative update.
        static void Collect(
            Scene &scene,
            std::vector<RenderMeshSubmission> &submissions,
            AssetManager *assetManager = nullptr,
            RenderSystemCollectScratch *scratch = nullptr);
    };
}
