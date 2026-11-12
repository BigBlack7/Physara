#pragma once

/// @file
/// @brief 主视口的布局请求. 保存逻辑身份, 仅在当前可见内容区域发布像素尺寸.

#include <cstdint>
#include <optional>

#include <imgui.h>

#include <Input.hpp>

#include "Session/UiFrame.hpp"

namespace Physara::Editor::Detail
{
    enum class UiFallback : std::uint8_t;

    /// @brief 本轮视口图像的布局来源. 按索引定位封存命令, 以逻辑矩形放置本帧状态文字.
    /// 列表和字体借用止于本轮封存帧准备结束.
    struct ViewportImage
    {
        const ImDrawList *list = nullptr;
        unsigned firstIndex = 0; ///< 图像写入的首个索引, 供封存后定位实际采样命令.
        ImTextureID token = ImTextureID_Invalid;
        ImVec2 origin;
        ImVec2 size;
        ImFont *font = nullptr;
        float fontSize = 0;

        /// @brief 在参与合成的列表中定位图像的实际命令. 被裁掉的图像没有命令.
        [[nodiscard]] const ImDrawCmd *FindDraw(const ImDrawData &data) const noexcept;
        /// @brief 用布局期间已准备的字体绘制原因, 裁剪在图像及其命令的可见范围内.
        void DrawStatus(ImDrawList &status, const ImDrawCmd &draw, UiFallback reason) const;
    };

    /// @brief 单次布局的逻辑命中和独立的离屏像素请求. 隐藏或不可绘制时请求为空.
    struct ViewportLayout
    {
        bool hit = false;
        std::optional<Renderer::ViewportRequest> request;
        ViewportImage image;
    };

    /// @brief 用同一次窗口发布的像素/逻辑比例换算内容大小. 向下取整, 无有效范围时返回空.
    [[nodiscard]] std::optional<Renderer::ViewExtent> MeasureViewport(double width, double height,
                                                                       const Platform::WindowState &window) noexcept;

    /// @brief 持有主 View 的逻辑纹理身份, 绘制仅发布逻辑 token 和实际像素请求.
    class Viewport final
    {
    public:
        Viewport() = default;
        Viewport(Renderer::ViewId view, UiTextureId texture, std::uint32_t maxTextureSize) noexcept
            : m_View(view), m_Texture(texture), m_MaxTextureSize(maxTextureSize)
        {
        }

        /// @brief 为可见面板测量图像区域, 提交本轮布局请求和逻辑图像 token.
        /// @pre Workspace 已开始当前可见的 ImGui 面板, window 来自本次输入发布.
        /// @return 逻辑区域参与输入命中. 尺寸或诊断预算校验失败时展示原因并保持请求为空.
        [[nodiscard]] ViewportLayout Draw(const Platform::WindowState &window) const;

    private:
        Renderer::ViewId m_View;
        UiTextureId m_Texture;
        std::uint32_t m_MaxTextureSize = 0;
    };
}
