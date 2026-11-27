#pragma once

/// @file
/// @brief 保存主视口逻辑身份. 布局封存图像区域, 编辑应用后再换算离屏请求.
/// View 准备提供画布内的有效矩形, 输入映射再排除裁剪区域与相机留边.

#include <array>
#include <cstdint>
#include <optional>

#include <imgui.h>

#include <Input.hpp>

#include "Session/UiFrame.hpp"

namespace Physara::Editor::Detail
{
    enum class UiFallback : std::uint8_t;
    /// @brief 按 Renderer::PreviewMode 顺序排列的基础预览名, 也是选择框的请求选项.
    inline constexpr const char *PreviewNames[] = {
        "Base Color", "World Normal", "Material Channels", "Emissive", "Direct Lighting", "Physical Lighting"};

    /// @brief 以实际诊断光源类型命名当前视口颜色.
    [[nodiscard]] inline const char *LocalShadowPreviewName(Asset::ModelLightKind kind) noexcept
    {
        return kind == Asset::ModelLightKind::Spot ? "Spot shadow" : "Point shadow";
    }

    /// @brief 按颜色的实际来源命名. 基础预览请求可能同时保留以供恢复.
    [[nodiscard]] inline const char *ScenePreviewName(const Renderer::RenderedScene &scene) noexcept
    {
        if (scene.localShadow)
        {
            return LocalShadowPreviewName(scene.localShadow->kind);
        }
        return scene.directionalCascades ? "Directional CSM" : PreviewNames[static_cast<std::size_t>(scene.mode)];
    }

    /// @brief 逻辑窗口中的画布及可见裁剪, 可跨帧比较而不借用 ImGui 数据.
    struct ViewportRegion
    {
        std::array<float, 4> canvas; ///< 左、上、右、下.
        std::array<float, 4> clip; ///< 画布与绘制裁剪的交集.
        bool operator==(const ViewportRegion &) const = default;

        /// @brief 在封存裁剪内把逻辑位置映射到有效图像 UV. 颜色版本与输入归属须由调用者先核对.
        [[nodiscard]] std::optional<std::array<double, 2>> ImageUv(const Renderer::ViewImageLayout &layout,
            double x, double y) const noexcept;
    };

    /// @brief 本轮图像画布及 token. 普通视口还记录封存索引, 诊断画布供准备阶段拟合.
    /// @note 列表和字体来自本轮 ImGui, 不跨帧持有.
    struct ViewportImage
    {
        const ImDrawList *list = nullptr;
        std::optional<unsigned> firstIndex; ///< 仅普通视口在布局时已有图像索引.
        ImTextureID token = ImTextureID_Invalid;
        ImVec2 origin;
        ImVec2 size;
        ImVec4 clip; ///< 布局期裁剪, 供诊断图像拟合使用.
        ImFont *font = nullptr;
        float fontSize = 0;

        /// @brief 在当前窗口封存带逻辑身份的 Image, 并预备后续状态合成需要的 ASCII 字形.
        [[nodiscard]] static ViewportImage Draw(ImTextureID token, ImVec2 size);
        /// @brief 封存诊断画布, 实际拟合图像及原因由同轮 UI 准备产生.
        [[nodiscard]] static ViewportImage Reserve(ImTextureID token, ImVec2 size);
        /// @brief 在参与合成的列表中按索引定位图像命令. 无对应列表或索引范围时为空.
        [[nodiscard]] const ImDrawCmd *FindDraw(const ImDrawData &data) const noexcept;
        /// @brief 用封存字体绘制输出来源, 实际预览模式或失败原因, 裁剪在本图像命令内.
        /// @note output 只在调用期间借用, 模式标签来自该候选而非之后的 View 请求.
        /// retained 表示本命令确实采样旧代, 可以与等待或拒绝的原因同时显示.
        void DrawStatus(ImDrawList &status, const ImDrawCmd &draw, UiFallback reason,
                        const Renderer::ViewOutput *output, bool retained) const;
        /// @brief 在同帧图像裁剪内合成有界状态文字. 调用方选择匹配当前采样的标签.
        void DrawLabel(ImDrawList &status, ImVec4 clipRect, std::string_view label, bool wrap = true) const;
        /// @brief 从同轮实际图像命令取得非空逻辑画布与裁剪, 不依赖颜色资源是否已经发布.
        [[nodiscard]] std::optional<ViewportRegion> Region(const ImDrawData &data) const noexcept;
    };

    /// @brief UTF-8 路径按可打印 ASCII 表示, 让封存后生成的标签只消费本轮已预备的字形.
    [[nodiscard]] Core::BoundedText SurfacePreviewLabel(std::string_view text) noexcept;

    /// @brief 单次面板布局. 窗口身份用于界面归属, 有效图像命中还需匹配正常已接受输出.
    struct ViewportLayout
    {
        ImGuiID window = 0; ///< 当前视口所在的 ImGui 窗口, 供顺序按钮事件判定遮挡.
        ViewportImage image;

        /// @brief 在当前 UI 的窗口层级内判断实际按钮位置的顶层输入归属.
        [[nodiscard]] bool OwnsPointer(Platform::LogicalPoint position) const noexcept;
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

        /// @brief 为可见面板测量逻辑图像区域, 记录窗口身份和图像命令.
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
