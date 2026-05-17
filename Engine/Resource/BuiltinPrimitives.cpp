#include "BuiltinPrimitives.hpp"

#include <cmath>
#include <cstdint>
#include <memory>
#include <numbers>
#include <string_view>
#include <utility>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <Engine/Resource/AssetManager.hpp>
#include <Engine/Resource/Types/Mesh.hpp>

namespace Physara::Engine
{
    namespace BuiltinPrimitivesDetail
    {
        MeshVertex Vertex(const glm::vec3 &position, const glm::vec3 &normal, const glm::vec4 &tangent, const glm::vec2 &uv)
        {
            MeshVertex vertex{};
            vertex.position = position;
            vertex.normal = normal;
            vertex.tangent = tangent;
            vertex.texCoord0 = uv;
            return vertex;
        }

        void SetBounds(MeshPrimitive &primitive, const glm::vec3 &min, const glm::vec3 &max)
        {
            primitive.boundsMin = min;
            primitive.boundsMax = max;
            primitive.hasBounds = true;
            primitive.vertexCount = primitive.vertices.size();
            primitive.indexCount = primitive.indices.size();
        }

        std::shared_ptr<Mesh> MakeMesh(std::string_view path, std::string_view name, MeshPrimitive primitive)
        {
            auto mesh = std::make_shared<Mesh>();
            mesh->path = std::string(path);
            mesh->meshIndex = 0u;
            mesh->name = std::string(name);
            mesh->primitives.push_back(std::move(primitive));
            return mesh;
        }

        MeshPrimitive MakeCubePrimitive()
        {
            MeshPrimitive primitive{};
            primitive.primitiveIndex = 0u;
            primitive.materialIndex = 0u;

            const glm::vec3 p000{-0.5f, -0.5f, -0.5f};
            const glm::vec3 p001{-0.5f, -0.5f, 0.5f};
            const glm::vec3 p010{-0.5f, 0.5f, -0.5f};
            const glm::vec3 p011{-0.5f, 0.5f, 0.5f};
            const glm::vec3 p100{0.5f, -0.5f, -0.5f};
            const glm::vec3 p101{0.5f, -0.5f, 0.5f};
            const glm::vec3 p110{0.5f, 0.5f, -0.5f};
            const glm::vec3 p111{0.5f, 0.5f, 0.5f};

            auto addFace = [&primitive](const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c, const glm::vec3 &d,
                                        const glm::vec3 &normal, const glm::vec4 &tangent)
            {
                const std::uint32_t base = static_cast<std::uint32_t>(primitive.vertices.size());
                primitive.vertices.push_back(Vertex(a, normal, tangent, {0.f, 0.f}));
                primitive.vertices.push_back(Vertex(b, normal, tangent, {1.f, 0.f}));
                primitive.vertices.push_back(Vertex(c, normal, tangent, {1.f, 1.f}));
                primitive.vertices.push_back(Vertex(d, normal, tangent, {0.f, 1.f}));
                primitive.indices.insert(primitive.indices.end(), {base, base + 1u, base + 2u, base, base + 2u, base + 3u});
            };

            addFace(p001, p101, p111, p011, {0.f, 0.f, 1.f}, {1.f, 0.f, 0.f, 1.f});
            addFace(p100, p000, p010, p110, {0.f, 0.f, -1.f}, {-1.f, 0.f, 0.f, 1.f});
            addFace(p000, p001, p011, p010, {-1.f, 0.f, 0.f}, {0.f, 0.f, 1.f, 1.f});
            addFace(p101, p100, p110, p111, {1.f, 0.f, 0.f}, {0.f, 0.f, -1.f, 1.f});
            addFace(p010, p011, p111, p110, {0.f, 1.f, 0.f}, {1.f, 0.f, 0.f, -1.f});
            addFace(p000, p100, p101, p001, {0.f, -1.f, 0.f}, {1.f, 0.f, 0.f, 1.f});

            SetBounds(primitive, {-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f});
            return primitive;
        }

        MeshPrimitive MakePlanePrimitive()
        {
            MeshPrimitive primitive{};
            primitive.primitiveIndex = 0u;
            primitive.materialIndex = 0u;
            const glm::vec3 normal{0.f, 1.f, 0.f};
            const glm::vec4 tangent{1.f, 0.f, 0.f, -1.f};
            primitive.vertices = {
                Vertex({-0.5f, 0.f, -0.5f}, normal, tangent, {0.f, 0.f}),
                Vertex({0.5f, 0.f, -0.5f}, normal, tangent, {1.f, 0.f}),
                Vertex({0.5f, 0.f, 0.5f}, normal, tangent, {1.f, 1.f}),
                Vertex({-0.5f, 0.f, 0.5f}, normal, tangent, {0.f, 1.f})};
            primitive.indices = {0u, 2u, 1u, 0u, 3u, 2u};
            SetBounds(primitive, {-0.5f, -0.001f, -0.5f}, {0.5f, 0.001f, 0.5f});
            return primitive;
        }

        MeshPrimitive MakeSpherePrimitive()
        {
            MeshPrimitive primitive{};
            primitive.primitiveIndex = 0u;
            primitive.materialIndex = 0u;

            constexpr std::uint32_t stacks = 24u;
            constexpr std::uint32_t slices = 48u;
            constexpr float radius = 0.5f;

            for (std::uint32_t y = 0u; y <= stacks; ++y)
            {
                const float v = static_cast<float>(y) / static_cast<float>(stacks);
                const float phi = v * std::numbers::pi_v<float>;
                const float sinPhi = std::sin(phi);
                const float cosPhi = std::cos(phi);

                for (std::uint32_t x = 0u; x <= slices; ++x)
                {
                    const float u = static_cast<float>(x) / static_cast<float>(slices);
                    const float theta = u * std::numbers::pi_v<float> * 2.f;
                    const float sinTheta = std::sin(theta);
                    const float cosTheta = std::cos(theta);

                    const glm::vec3 normal{sinPhi * cosTheta, cosPhi, sinPhi * sinTheta};
                    const glm::vec3 position = normal * radius;
                    const glm::vec3 tangent = glm::normalize(glm::vec3(-sinTheta, 0.f, cosTheta));
                    primitive.vertices.push_back(Vertex(position, normal, glm::vec4(tangent, 1.f), {u, v}));
                }
            }

            for (std::uint32_t y = 0u; y < stacks; ++y)
            {
                for (std::uint32_t x = 0u; x < slices; ++x)
                {
                    const std::uint32_t row0 = y * (slices + 1u);
                    const std::uint32_t row1 = (y + 1u) * (slices + 1u);
                    const std::uint32_t a = row0 + x;
                    const std::uint32_t b = row0 + x + 1u;
                    const std::uint32_t c = row1 + x + 1u;
                    const std::uint32_t d = row1 + x;
                    primitive.indices.insert(primitive.indices.end(), {a, b, c, a, c, d});
                }
            }

            SetBounds(primitive, {-radius, -radius, -radius}, {radius, radius, radius});
            return primitive;
        }
    }

    void RegisterBuiltinPrimitives(AssetManager &assetManager)
    {
        (void)assetManager.RegisterAsset<Mesh>(
            "Builtin/Primitives/Cube#mesh/0",
            BuiltinPrimitivesDetail::MakeMesh("Builtin/Primitives/Cube#mesh/0", "Cube", BuiltinPrimitivesDetail::MakeCubePrimitive()));
        (void)assetManager.RegisterAsset<Mesh>(
            "Builtin/Primitives/Sphere#mesh/0",
            BuiltinPrimitivesDetail::MakeMesh("Builtin/Primitives/Sphere#mesh/0", "Sphere", BuiltinPrimitivesDetail::MakeSpherePrimitive()));
        (void)assetManager.RegisterAsset<Mesh>(
            "Builtin/Primitives/Plane#mesh/0",
            BuiltinPrimitivesDetail::MakeMesh("Builtin/Primitives/Plane#mesh/0", "Plane", BuiltinPrimitivesDetail::MakePlanePrimitive()));
    }
}