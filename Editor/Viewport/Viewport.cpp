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
        case UiFallback::InvalidExtent: return "Invalid viewport pixel extent";
        case UiFallback::TextureSizeLimit: return "Device texture size limit";
        case UiFallback::DiagnosticBudget: return "Diagnostic texture budget exceeded";
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

    // 在视口图像和原绘制裁剪的交集内生成状态标签, 供合成器紧接图像使用.
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
                                                        const Platform::WindowState &window, float renderScale) noexcept
    {
        const auto logical = window.clientSizeLogical;
        const auto framebuffer = window.framebufferSizePixels;
        if (logical.width == 0 || logical.height == 0)
        {
            return std::nullopt;
        }
        // 内容缩放已用于 UI 样式, 这里仅使用实际 framebuffer 与逻辑窗口的比例.
        const double x = std::floor(width * framebuffer.width / logical.width * renderScale);
        const double y = std::floor(height * framebuffer.height / logical.height * renderScale);
        constexpr auto MaxDimension = static_cast<double>(std::numeric_limits<std::uint32_t>::max());
        if (!std::isfinite(x) || !std::isfinite(y) || x < 1 || y < 1 || x > MaxDimension || y > MaxDimension)
        {
            return std::nullopt;
        }
        return Renderer::ViewExtent{static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y)};
    }

    // 像素范围与用于换算的比例请求代一起封存. 换算失败也保留身份, 供可见占位解释原因.
    std::optional<Renderer::ViewportRequest> Viewport::Request(const ViewportLayout &layout,
                                                                 const Platform::WindowState &window,
                                                                 const Renderer::ViewSettings &settings) const noexcept
    {
        if (!layout.image.list || settings.view != m_View)
        {
            return std::nullopt;
        }
        const auto extent = MeasureViewport(layout.image.size.x, layout.image.size.y, window, settings.requestedScale);
        return Renderer::ViewportRequest{m_View, extent.value_or(Renderer::ViewExtent{}), settings.requestRevision};
    }

    ViewportLayout Viewport::Draw() const
    {
        // 先保留标签高度, 剩余区域同时用于图像布局, 命中和离屏尺寸计算.
        ImGui::TextUnformatted(m_View.IsValid() ? "Test viewport | No scene loaded." : "Viewport output pending.");
        const auto min = ImGui::GetCursorScreenPos();
        const auto size = ImGui::GetContentRegionAvail();
        const ImVec2 max{min.x + size.x, min.y + size.y};
        ViewportLayout result;
        // 命中服务输入路由. 图像及尺寸准备依据面板的实际绘制区域.
        result.hit = size.x > 0 && size.y > 0 && ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(min, max);
        if (!m_View.IsValid() || size.x <= 0 || size.y <= 0)
        {
            return result;
        }
        // 先封存逻辑图像. 比例应用后, 本轮准备会解析实际颜色或在同一位置合成原因.
        auto *list = ImGui::GetWindowDrawList();
        const auto firstIndex = list->IdxBuffer.Size;
        const auto token = EncodeUiTexture(m_Texture);
        ImGui::Image(ImTextureRef(token), size);
        if (list->IdxBuffer.Size != firstIndex)
        {
            result.image = {list, static_cast<unsigned>(firstIndex), token, min, size,
                            ImGui::GetFont(), ImGui::GetFontSize()};
            // 封存前先准备各原因的字形, 避免 GPU 准备时重新修改 atlas 请求.
            auto *baked = ImGui::GetFontBaked();
            for (auto reason : {UiFallback::InvalidIdentity, UiFallback::MissingOutput, UiFallback::UploadPending,
                                UiFallback::InvalidExtent, UiFallback::TextureSizeLimit, UiFallback::DiagnosticBudget})
            {
                for (const char *character = StatusText(reason); *character != '\0'; ++character)
                {
                    static_cast<void>(baked->FindGlyph(static_cast<ImWchar>(*character)));
                }
            }
        }
        return result;
    }
}
