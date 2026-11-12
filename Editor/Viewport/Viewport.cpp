#include "Viewport.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "ImGui/UiTextures.hpp"

/// @file
/// @brief 视口以逻辑坐标布局和命中, 以同批窗口事实计算独立的离屏像素请求.
/// 记录本轮图像位置, 供合成器根据实际解析结果生成可见状态.

namespace Physara::Editor::Detail
{
    // 布局期预备字形和合成期绘制使用同一组原因文字.
    static const char *StatusText(UiFallback reason) noexcept
    {
        switch (reason)
        {
        case UiFallback::InvalidIdentity: return "Texture expired";
        case UiFallback::MissingOutput: return "Output unavailable";
        case UiFallback::UploadPending: return "Waiting for upload";
        case UiFallback::None: return "";
        }
        return "";
    }

    const ImDrawCmd *ViewportImage::FindDraw(const ImDrawData &data) const noexcept
    {
        if (list == nullptr || std::ranges::find(data.CmdLists, list) == data.CmdLists.end())
        {
            return nullptr;
        }
        // ImGui 可合并相邻命令. 用图像首个索引在封存的绘制区间内定位.
        for (const auto &draw : list->CmdBuffer)
        {
            if (draw.UserCallback == nullptr && draw.IdxOffset <= firstIndex &&
                firstIndex - draw.IdxOffset < draw.ElemCount)
            {
                return &draw;
            }
        }
        return nullptr;
    }

    void ViewportImage::DrawStatus(ImDrawList &status, const ImDrawCmd &draw, UiFallback reason) const
    {
        const auto clip = draw.ClipRect;
        const ImVec2 min(std::max(origin.x, clip.x), std::max(origin.y, clip.y));
        const ImVec2 max(std::min(origin.x + size.x, clip.z), std::min(origin.y + size.y, clip.w));
        if (min.x >= max.x || min.y >= max.y)
        {
            return;
        }
        constexpr float Inset = 4.0f;
        // 独立列表沿用视口图像的裁剪. 字形已在布局期进入本帧 atlas, 此处只生成顶点.
        status._ResetForNewFrame();
        status.PushClipRect(min, max);
        status.PushTexture(font->OwnerAtlas->TexRef);
        status.AddRectFilled(min, ImVec2(max.x, std::min(max.y, min.y + fontSize + 2 * Inset)),
                             IM_COL32(30, 24, 32, 255));
        status.AddText(font, fontSize, ImVec2(min.x + Inset, min.y + Inset), IM_COL32_WHITE, StatusText(reason));
    }

    std::optional<Renderer::ViewExtent> MeasureViewport(double width, double height,
                                                        const Platform::WindowState &window) noexcept
    {
        const auto logical = window.clientSizeLogical;
        const auto framebuffer = window.framebufferSizePixels;
        if (logical.width == 0 || logical.height == 0)
        {
            return std::nullopt;
        }
        // 内容缩放已用于 UI 样式, 这里仅使用实际 framebuffer 与逻辑窗口的比例.
        const double x = std::floor(width * framebuffer.width / logical.width);
        const double y = std::floor(height * framebuffer.height / logical.height);
        constexpr auto MaxDimension = static_cast<double>(std::numeric_limits<std::uint32_t>::max());
        if (!std::isfinite(x) || !std::isfinite(y) || x < 1 || y < 1 || x > MaxDimension || y > MaxDimension)
        {
            return std::nullopt;
        }
        return Renderer::ViewExtent{static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y)};
    }

    ViewportLayout Viewport::Draw(const Platform::WindowState &window) const
    {
        // 先保留标签高度, 剩余区域同时用于图像布局, 命中和离屏尺寸计算.
        ImGui::TextUnformatted(m_View.IsValid() ? "Test viewport | No scene loaded." : "Viewport output pending.");
        const auto min = ImGui::GetCursorScreenPos();
        const auto size = ImGui::GetContentRegionAvail();
        const ImVec2 max{min.x + size.x, min.y + size.y};
        ViewportLayout result;
        result.hit = size.x > 0 && size.y > 0 && ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(min, max);
        if (!m_View.IsValid() || size.x <= 0 || size.y <= 0)
        {
            return result;
        }
        const auto extent = MeasureViewport(size.x, size.y, window);
        if (!extent || extent->width > m_MaxTextureSize || extent->height > m_MaxTextureSize)
        {
            ImGui::TextUnformatted("Viewport unavailable: invalid pixel extent.");
        }
        else if (Renderer::DiagnosticPixelBytes(*extent) == 0)
        {
            ImGui::TextUnformatted("Viewport unavailable: diagnostic texture budget exceeded.");
        }
        else
        {
            // ImGui 只记录会话 token. Renderer 准备的本轮颜色代留到 UI 绑定阶段解析.
            result.request = Renderer::ViewportRequest{m_View, *extent};
            auto *list = ImGui::GetWindowDrawList();
            const auto firstIndex = list->IdxBuffer.Size;
            const auto token = EncodeUiTexture(m_Texture);
            ImGui::Image(ImTextureRef(token), size);
            if (list->IdxBuffer.Size != firstIndex)
            {
                result.image = {list, static_cast<unsigned>(firstIndex), token, min, size,
                                ImGui::GetFont(), ImGui::GetFontSize()};
                // 原因在封存后才确定. 在 ImGui 收集纹理请求前准备三种短标签所需的字形.
                auto *baked = ImGui::GetFontBaked();
                for (auto reason : {UiFallback::InvalidIdentity, UiFallback::MissingOutput, UiFallback::UploadPending})
                {
                    for (const char *character = StatusText(reason); *character != '\0'; ++character)
                    {
                        static_cast<void>(baked->FindGlyph(static_cast<ImWchar>(*character)));
                    }
                }
            }
        }
        return result;
    }
}
