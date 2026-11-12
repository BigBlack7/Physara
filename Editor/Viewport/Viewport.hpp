#pragma once

/// @file
/// @brief 保存主视口逻辑身份. 布局封存图像区域, 编辑应用后再换算离屏请求.

#include <cstdint>
#include <optional>

#include <imgui.h>

#include <Input.hpp>

#include "Session/UiFrame.hpp"

namespace Physara::Editor::Detail
{
    enum class UiFallback : std::uint8_t;

    /// @brief 本轮视口图像的逻辑矩形及 token. 记录索引以便定位合并后的封存命令.
    /// @note 列表和字体来自本轮 ImGui. 图像命令与 token 在准备期一起验证.
    struct ViewportImage
    {
        const ImDrawList *list = nullptr;
        unsigned firstIndex = 0; ///< 图像写入的首个索引, 供封存后定位实际采样命令.
        ImTextureID token = ImTextureID_Invalid;
        ImVec2 origin;
        ImVec2 size;
        ImFont *font = nullptr;
        float fontSize = 0;

        /// @brief 在参与合成的列表中按索引定位图像命令. 无对应列表或索引范围时为空.
        [[nodiscard]] const ImDrawCmd *FindDraw(const ImDrawData &data) const noexcept;
        /// @brief 用布局期间已准备的字体绘制原因, 裁剪在图像及其命令的可见范围内.
        void DrawStatus(ImDrawList &status, const ImDrawCmd &draw, UiFallback reason) const;
    };

    /// @brief 单次布局的逻辑命中及图像区域. 隐藏或被裁掉时没有采样命令.
    struct ViewportLayout
    {
        bool hit = false;
        ViewportImage image;
    };

    /// @brief 从逻辑内容宽高换算离屏像素. 各轴使用同批 framebuffer 比例, 乘比例后再取整.
    /// @return 无法使用窗口度量或结果超出有效像素范围时为空.
    [[nodiscard]] std::optional<Renderer::ViewExtent> MeasureViewport(double width, double height,
                                                                       const Platform::WindowState &window,
                                                                       float renderScale) noexcept;

    /// @brief 持有主 View 的逻辑纹理身份, 将布局与像素请求分成两个 CPU 阶段.
    class Viewport final
    {
    public:
        Viewport() = default;
        Viewport(Renderer::ViewId view, UiTextureId texture) noexcept
            : m_View(view), m_Texture(texture)
        {
        }
        [[nodiscard]] Renderer::ViewId Id() const noexcept { return m_View; }

        /// @brief 为可见面板测量逻辑图像区域, 记录命中和图像命令.
        /// @pre Workspace 已开始当前可见的 ImGui 面板.
        [[nodiscard]] ViewportLayout Draw() const;

        /// @brief 用本轮封存区域和应用后的比例生成请求.
        /// @param settings 同一 View 的快照, 其比例和请求代随本次布局一起消费.
        /// @return 无图像或身份失配时为空. 可见区域无法换算时用零范围表示拒绝.
        [[nodiscard]] std::optional<Renderer::ViewportRequest> Request(const ViewportLayout &layout,
                                                                        const Platform::WindowState &window,
                                                                        const Renderer::ViewSettings &settings) const noexcept;

    private:
        Renderer::ViewId m_View;
        UiTextureId m_Texture;
    };
}
