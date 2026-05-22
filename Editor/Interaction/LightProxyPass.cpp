#include "LightProxyPass.hpp"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <Engine/Scene/Components/LightComponent.hpp>
#include <Engine/Scene/Components/TransformComponent.hpp>
#include <Engine/Scene/Scene.hpp>

namespace Physara::Editor
{
    namespace LightProxyPassDetail
    {
        constexpr float IconSize = 24.f;
        constexpr float MinScreenRadius = 12.f;
        constexpr float MaxScreenRadius = 260.f;

        glm::vec3 Forward(const glm::mat4 &world)
        {
            return glm::normalize(glm::vec3(world * glm::vec4(0.f, 0.f, -1.f, 0.f)));
        }

        glm::vec3 Right(const glm::mat4 &world)
        {
            return glm::normalize(glm::vec3(world * glm::vec4(1.f, 0.f, 0.f, 0.f)));
        }

        glm::vec3 Up(const glm::mat4 &world)
        {
            return glm::normalize(glm::vec3(world * glm::vec4(0.f, 1.f, 0.f, 0.f)));
        }
    }

    void LightProxyPass::Draw(EditorContext &context, const ImVec2 &viewportOrigin, float width, float height)
    {
        if (context.activeScene == nullptr || context.ui.cleanSceneView)
        {
            return;
        }

        ImDrawList *drawList = ImGui::GetWindowDrawList();
        auto &registry = context.activeScene->GetRegistry();
        auto view = registry.view<Engine::LightComponent, Engine::TransformComponent>();

        view.each([&](Engine::EntityId entity, const Engine::LightComponent &light, Engine::TransformComponent &transform)
        {
            const glm::mat4 &world = transform.GetWorldMatrix();
            const glm::vec3 position = glm::vec3(world[3]);
            ImVec2 screen{};
            if (!Project(context, viewportOrigin, width, height, position, screen))
            {
                return;
            }

            const bool selected = IsSelected(context, entity);
            DrawBillboard(*drawList, screen, selected);

            if (!selected)
            {
                return;
            }

            const ImU32 proxyColor = IM_COL32(250, 224, 128, 230);
            const ImU32 fillColor = IM_COL32(250, 224, 128, 26);
            if (light.type == Engine::LightType::Point)
            {
                ImVec2 edge{};
                const glm::vec3 radiusPoint = position + LightProxyPassDetail::Right(world) * light.rangeMeters;
                if (Project(context, viewportOrigin, width, height, radiusPoint, edge))
                {
                    const float radius = std::clamp(std::hypot(edge.x - screen.x, edge.y - screen.y),
                                                    LightProxyPassDetail::MinScreenRadius,
                                                    LightProxyPassDetail::MaxScreenRadius);
                    drawList->AddCircleFilled(screen, radius, fillColor, 64);
                    drawList->AddCircle(screen, radius, proxyColor, 64, 1.5f);
                }
                return;
            }

            if (light.type == Engine::LightType::Spot)
            {
                const glm::vec3 forward = LightProxyPassDetail::Forward(world);
                const glm::vec3 right = LightProxyPassDetail::Right(world);
                const glm::vec3 up = LightProxyPassDetail::Up(world);
                const float coneRadius = std::tan(light.outerConeAngleRadians) * light.rangeMeters;
                const glm::vec3 center = position + forward * light.rangeMeters;
                const glm::vec3 edge0 = center + right * coneRadius;
                const glm::vec3 edge1 = center - right * coneRadius;
                const glm::vec3 edge2 = center + up * coneRadius;
                const glm::vec3 edge3 = center - up * coneRadius;
                ImVec2 centerScreen{};
                ImVec2 screens[4]{};
                if (Project(context, viewportOrigin, width, height, center, centerScreen) &&
                    Project(context, viewportOrigin, width, height, edge0, screens[0]) &&
                    Project(context, viewportOrigin, width, height, edge1, screens[1]) &&
                    Project(context, viewportOrigin, width, height, edge2, screens[2]) &&
                    Project(context, viewportOrigin, width, height, edge3, screens[3]))
                {
                    for (const ImVec2 &edge : screens)
                    {
                        drawList->AddLine(screen, edge, proxyColor, 1.4f);
                    }
                    const float radiusX = std::max(std::abs(screens[0].x - screens[1].x) * 0.5f, 4.f);
                    const float radiusY = std::max(std::abs(screens[2].y - screens[3].y) * 0.5f, 4.f);
                    drawList->AddCircle(centerScreen, std::max(radiusX, radiusY), proxyColor, 48, 1.4f);
                }
                return;
            }

            if (light.type == Engine::LightType::Directional)
            {
                const glm::vec3 endpoint = position + LightProxyPassDetail::Forward(world) * 1.5f;
                ImVec2 endScreen{};
                if (Project(context, viewportOrigin, width, height, endpoint, endScreen))
                {
                    drawList->AddLine(screen, endScreen, proxyColor, 2.f);
                    drawList->AddCircleFilled(endScreen, 4.f, proxyColor, 16);
                }
            }
        });
    }

    bool LightProxyPass::Project(const EditorContext &context, const ImVec2 &origin, float width, float height, const glm::vec3 &world, ImVec2 &screen) const
    {
        const glm::vec4 clip = context.sceneView.lastRenderView.viewProjection * glm::vec4(world, 1.f);
        if (clip.w <= 0.0001f)
        {
            return false;
        }

        const glm::vec3 ndc = glm::vec3(clip) / clip.w;
        if (ndc.z < -1.f || ndc.z > 1.f)
        {
            return false;
        }

        screen.x = origin.x + (ndc.x * 0.5f + 0.5f) * width;
        screen.y = origin.y + (0.5f - ndc.y * 0.5f) * height;
        return true;
    }

    bool LightProxyPass::IsSelected(const EditorContext &context, Engine::EntityId entity) const
    {
        if (context.selectedEntity == entity)
        {
            return true;
        }

        return std::find(context.selectedEntities.begin(), context.selectedEntities.end(), entity) != context.selectedEntities.end();
    }

    void LightProxyPass::DrawBillboard(ImDrawList &drawList, const ImVec2 &screen, bool selected) const
    {
        const float half = LightProxyPassDetail::IconSize * 0.5f;
        const ImVec2 min(screen.x - half, screen.y - half);
        const ImVec2 max(screen.x + half, screen.y + half);
        const ImU32 tint = selected ? IM_COL32(255, 238, 158, 255) : IM_COL32(214, 226, 185, 230);

        if (m_BillboardIcon != 0)
        {
            drawList.AddImage(static_cast<ImTextureID>(m_BillboardIcon), min, max, ImVec2(0.f, 0.f), ImVec2(1.f, 1.f), tint);
        }
        else
        {
            drawList.AddCircleFilled(screen, half * 0.58f, IM_COL32(255, 229, 126, 210), 18);
            drawList.AddCircle(screen, half * 0.58f, IM_COL32(80, 63, 20, 240), 18, 1.2f);
        }

        if (selected)
        {
            drawList.AddRect(min, max, IM_COL32(255, 244, 178, 245), 4.f, 0, 1.5f);
        }
    }
}