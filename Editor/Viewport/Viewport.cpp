#include "Viewport.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>

#include <imgui_internal.h>

#include "ImGui/UiTextures.hpp"

/// @file
/// @brief 视口以逻辑坐标布局和命中, 以同批窗口事实计算独立的离屏像素请求.
/// 记录本轮图像位置, 供合成器根据实际解析结果生成可见状态.

namespace Physara::Editor::Detail
{
    // 布局期预备字形和合成期绘制使用同一组原因文字. 旧输出标签只解释来源, 不确认当前 View 请求.
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
        case UiFallback::CameraUnavailable: return "Scene camera unavailable";
        case UiFallback::InvalidProjection: return "Invalid camera projection";
        case UiFallback::InvalidImageRect: return "Camera image does not fit the viewport";
        case UiFallback::InvalidSampling: return "Invalid camera sampling profile";
        case UiFallback::ScenePreparationFailed: return "Scene preparation failed";
        case UiFallback::ScenePipelineUnavailable: return "Forward pipeline unavailable";
        case UiFallback::ShaderPending: return "Preparing Shader pipelines";
        case UiFallback::SceneResidencyPending: return "Waiting for scene residency";
        case UiFallback::SceneResidencyLimit: return "Scene residency budget exceeded";
        case UiFallback::SceneAttachmentLimit: return "Forward attachment limit exceeded";
        case UiFallback::SceneUnsupported: return "Unsupported scene geometry";
        case UiFallback::SceneUploadLimit: return "Forward upload limit exceeded";
        case UiFallback::SceneLightTableRejected: return "Visible light table rejected";
        case UiFallback::EnvironmentPending: return "Preparing HDR environment or DFG";
        case UiFallback::EnvironmentRejected: return "HDR environment unavailable";
        case UiFallback::DisplayUnavailable: return "Display processing unavailable";
        case UiFallback::RetainedOutput: return "Frame failed";
        case UiFallback::FrameFailed: return "Frame failed - no accepted output";
        case UiFallback::None: return "";
        }
        return "";
    }

    const ImDrawCmd *ViewportImage::FindDraw(const ImDrawData &data) const noexcept
    {
        if (!firstIndex || list == nullptr || std::ranges::find(data.CmdLists, list) == data.CmdLists.end())
        {
            return nullptr;
        }
        // ImGui 可合并相邻命令. 用图像首个索引在封存的绘制区间内定位.
        for (const auto &draw : list->CmdBuffer)
        {
            if (draw.UserCallback == nullptr && draw.IdxOffset <= *firstIndex &&
                *firstIndex - draw.IdxOffset < draw.ElemCount)
            {
                return &draw;
            }
        }
        return nullptr;
    }

    std::optional<ViewportRegion> ViewportImage::Region(const ImDrawData &data) const noexcept
    {
        const auto *draw = FindDraw(data);
        if (!draw)
        {
            return std::nullopt;
        }
        const auto right = origin.x + size.x;
        const auto bottom = origin.y + size.y;
        const ViewportRegion region{{origin.x, origin.y, right, bottom},
            {std::max(origin.x, draw->ClipRect.x), std::max(origin.y, draw->ClipRect.y),
             std::min(right, draw->ClipRect.z), std::min(bottom, draw->ClipRect.w)}};
        return region.clip[0] < region.clip[2] && region.clip[1] < region.clip[3] ?
            std::optional(region) : std::nullopt;
    }

    // 逻辑画布只换算一次, View 的有效像素区域继续负责留边和半开边界.
    std::optional<std::array<double, 2>> ViewportRegion::ImageUv(const Renderer::ViewImageLayout &layout,
        double x, double y) const noexcept
    {
        if (!(x >= clip[0] && x < clip[2] && y >= clip[1] && y < clip[3]))
        {
            return std::nullopt;
        }
        return layout.ImageUv((x - canvas[0]) * layout.outputExtent.width / (canvas[2] - canvas[0]),
                              (y - canvas[1]) * layout.outputExtent.height / (canvas[3] - canvas[1]));
    }

    // 按事件发生位置查询当轮窗口遮挡. 不复用批末 HoveredWindow, 也不因正在拖窗口强行命中视口.
    bool ViewportLayout::OwnsPointer(Platform::LogicalPoint position) const noexcept
    {
        ImGuiWindow *hovered = nullptr;
        ImGui::FindHoveredWindowEx(ImVec2(static_cast<float>(position.x), static_cast<float>(position.y)),
                                  true, &hovered, nullptr);
        return hovered && hovered->ID == window;
    }

    // 在视口图像和原绘制裁剪的交集内生成状态标签, 供合成器紧接图像使用.
    void ViewportImage::DrawStatus(ImDrawList &status, const ImDrawCmd &draw, UiFallback reason,
                                   const Renderer::ViewOutput *output, bool retained) const
    {
        Core::BoundedText label(StatusText(reason));
        if (retained)
        {
            // 等待或拒绝原因属于当前候选, 后缀说明图像仍来自旧代.
            label.Append(" - showing last accepted output");
            if (output && output->scene)
            {
                label.Append(" - ");
                label.Append(ScenePreviewName(*output->scene));
            }
        }
        else if (reason == UiFallback::None && output)
        {
            if (output->scene)
            {
                // 窄视口先显示降级标识. 完整摘要由接受后的 Frame status 菜单显示.
                label = Core::BoundedText(output->scene->degradation ? "Degraded - " : "Forward preview - ");
                label.Append(ScenePreviewName(*output->scene));
            }
            else
            {
                label = Core::BoundedText("Test viewport - no scene output");
            }
        }
        DrawLabel(status, draw.ClipRect, label.View(), false);
    }

    void ViewportImage::DrawLabel(ImDrawList &status, ImVec4 clipRect, std::string_view label, bool wrap) const
    {
        const ImVec2 min(std::max(origin.x, clipRect.x), std::max(origin.y, clipRect.y));
        const ImVec2 max(std::min(origin.x + size.x, clipRect.z), std::min(origin.y + size.y, clipRect.w));
        if (min.x >= max.x || min.y >= max.y || label.empty())
        {
            return;
        }
        constexpr float Inset = 4.0f;
        // 标签沿用封存图像的裁剪、字号与 atlas. 布局阶段已为固定的 ASCII 状态文字预备字形.
        status._ResetForNewFrame();
        status.PushClipRect(min, max);
        status.PushTexture(font->OwnerAtlas->TexRef);
        const auto wrapWidth = wrap ? std::max(fontSize, max.x - min.x - 2 * Inset) : 0.f;
        const auto height = 2 * Inset + (wrap ? font->CalcTextSizeA(fontSize, (std::numeric_limits<float>::max)(),
            wrapWidth, label.data(), label.data() + label.size()).y : fontSize);
        status.AddRectFilled(min, ImVec2(max.x, std::min(max.y, min.y + height)),
                             IM_COL32(30, 24, 32, 255));
        status.AddText(font, fontSize, ImVec2(min.x + Inset, min.y + Inset), IM_COL32_WHITE,
                       label.data(), label.data() + label.size(), wrapWidth);
    }

    Core::BoundedText SurfacePreviewLabel(std::string_view text) noexcept
    {
        Core::BoundedText label;
        while (!text.empty() && !label.WasTruncated())
        {
            unsigned codepoint = static_cast<unsigned char>(text.front());
            const auto bytes = codepoint < 128 ? 1 :
                ImTextCharFromUtf8(&codepoint, text.data(), text.data() + text.size());
            if (codepoint >= 32 && codepoint <= 126)
            {
                label.Append(text.substr(0, 1));
            }
            else
            {
                char digits[8];
                const auto written = std::to_chars(digits, digits + sizeof(digits), codepoint, 16);
                label.Append("\\u{");
                label.Append({digits, static_cast<std::size_t>(written.ptr - digits)});
                label.Append("}");
            }
            text.remove_prefix(static_cast<std::size_t>(bytes));
        }
        return label;
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

    // 两套尺寸从同一未取整的逻辑范围独立换算, 不能把已取整的显示宽高再乘比例.
    // 换算失败仍封存请求身份, 由 View 报告原因.
    std::optional<Renderer::ViewportRequest> Viewport::Request(const ViewportLayout &layout,
                                                                 const Platform::WindowState &window,
                                                                 const Renderer::ViewSettings &settings) const noexcept
    {
        if (!layout.image.list || settings.view != m_View)
        {
            return std::nullopt;
        }
        const auto extent = MeasureViewport(layout.image.size.x, layout.image.size.y, window, settings.requestedScale);
        const auto output = settings.requestedScale == 1.0f ? extent :
            MeasureViewport(layout.image.size.x, layout.image.size.y, window, 1.0f);
        return Renderer::ViewportRequest{m_View, extent.value_or(Renderer::ViewExtent{}),
                                         output.value_or(Renderer::ViewExtent{}), settings.requestRevision};
    }

    // 只封存逻辑图像和状态文字需要的字形. 正常候选与安全 UI 随后可在同一布局上分别解析.
    ViewportLayout Viewport::Draw() const
    {
        const auto size = ImGui::GetContentRegionAvail();
        ViewportLayout result;
        if (!m_View.IsValid() || size.x <= 0 || size.y <= 0)
        {
            return result;
        }
        // 先封存逻辑图像. 比例应用后, 本轮准备会解析实际颜色或在同一位置合成原因.
        result.image = ViewportImage::Draw(EncodeUiTexture(m_Texture), size);
        if (result.image.list)
        {
            result.window = ImGui::GetCurrentWindowRead()->ID;
        }
        return result;
    }

    // 两种画布均在封存前取得本轮裁剪与字体, 诊断纹理的顶点由合成器随后选择.
    static ViewportImage CaptureCanvas(ImTextureID token, ImVec2 size)
    {
        const auto min = ImGui::GetCursorScreenPos();
        const auto *list = ImGui::GetWindowDrawList();
        const auto clipMin = list->GetClipRectMin();
        const auto clipMax = list->GetClipRectMax();
        // 预备状态正文和路径转义需要的 ASCII 字形, 保持封存后的 atlas 集合稳定.
        auto *baked = ImGui::GetFontBaked();
        for (ImWchar character = 32; character <= 126; ++character)
        {
            static_cast<void>(baked->FindGlyph(character));
        }
        return {list, {}, token, min, size, {clipMin.x, clipMin.y, clipMax.x, clipMax.y},
                ImGui::GetFont(), ImGui::GetFontSize()};
    }

    ViewportImage ViewportImage::Draw(ImTextureID token, ImVec2 size)
    {
        auto image = CaptureCanvas(token, size);
        const auto first = image.list->IdxBuffer.Size;
        ImGui::Image(ImTextureRef(token), size);
        if (image.list->IdxBuffer.Size == first)
        {
            return {};
        }
        image.firstIndex = static_cast<unsigned>(first);
        return image;
    }

    ViewportImage ViewportImage::Reserve(ImTextureID token, ImVec2 size)
    {
        auto image = CaptureCanvas(token, size);
        ImGui::Dummy(size);
        return image;
    }
}
