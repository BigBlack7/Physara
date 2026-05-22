#pragma once

#include <imgui/imgui.h>

#include <Editor/Core/EditorContext.hpp>
#include <Engine/RHI/Core/IImGuiBackend.hpp>
#include <Engine/Scene/EntityId.hpp>
#include <glm/vec3.hpp>

namespace Physara::Editor
{
    class LightProxyPass final
    {
    public:
        void SetBillboardIcon(RHI::ImGuiTextureHandle icon) { m_BillboardIcon = icon; }
        void Draw(EditorContext &context, const ImVec2 &viewportOrigin, float width, float height);

    private:
        [[nodiscard]] bool Project(const EditorContext &context, const ImVec2 &origin, float width, float height, const glm::vec3 &world, ImVec2 &screen) const;
        [[nodiscard]] bool IsSelected(const EditorContext &context, Engine::EntityId entity) const;
        void DrawBillboard(ImDrawList &drawList, const ImVec2 &screen, bool selected) const;

    private:
        RHI::ImGuiTextureHandle m_BillboardIcon{0};
    };
}