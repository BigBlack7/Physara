#include "EditorTheme.hpp"

#include <array>
#include <filesystem>

#include <imgui/imgui.h>

namespace Physara::Editor
{
    namespace Internal
    {
        constexpr float FontSize = 17.f;

        ImVec4 Color(float r, float g, float b, float a = 1.f)
        {
            return ImVec4(r / 255.f, g / 255.f, b / 255.f, a);
        }

        bool AddFontIfExists(ImFontAtlas &fonts, const char *path, float size, const ImFontConfig *config,
                             const ImWchar *ranges)
        {
            if (!std::filesystem::exists(path))
            {
                return false;
            }

            return fonts.AddFontFromFileTTF(path, size, config, ranges) != nullptr;
        }

        void ConfigureFonts()
        {
            ImGuiIO &io = ImGui::GetIO();
            ImFontAtlas &fonts = *io.Fonts;

            fonts.Clear();

            ImFontConfig latinConfig{};
            latinConfig.OversampleH = 3;
            latinConfig.OversampleV = 2;
            latinConfig.PixelSnapH = true;

            const std::array<const char *, 3> latinFonts{
                "C:/Windows/Fonts/segoeui.ttf",
                "C:/Windows/Fonts/SegoeUI.ttf",
                "C:/Windows/Fonts/arial.ttf",
            };

            bool hasBaseFont = false;
            for (const char *path : latinFonts)
            {
                if (AddFontIfExists(fonts, path, FontSize, &latinConfig, fonts.GetGlyphRangesDefault()))
                {
                    hasBaseFont = true;
                    break;
                }
            }

            if (!hasBaseFont)
            {
                fonts.AddFontDefault();
                return;
            }

            ImFontConfig cjkConfig{};
            cjkConfig.MergeMode = true;
            cjkConfig.OversampleH = 2;
            cjkConfig.OversampleV = 2;
            cjkConfig.PixelSnapH = true;

            const std::array<const char *, 4> cjkFonts{
                "C:/Windows/Fonts/msyh.ttc",
                "C:/Windows/Fonts/msyh.ttf",
                "C:/Windows/Fonts/Deng.ttf",
                "C:/Windows/Fonts/simsun.ttc",
            };

            for (const char *path : cjkFonts)
            {
                if (AddFontIfExists(fonts, path, FontSize, &cjkConfig, fonts.GetGlyphRangesChineseSimplifiedCommon()))
                {
                    break;
                }
            }
        }

        void ConfigureStyle()
        {
            ImGuiStyle &style = ImGui::GetStyle();
            style = ImGuiStyle{};

            style.WindowPadding = ImVec2(12.f, 10.f);
            style.FramePadding = ImVec2(10.f, 6.f);
            style.CellPadding = ImVec2(8.f, 5.f);
            style.ItemSpacing = ImVec2(8.f, 7.f);
            style.ItemInnerSpacing = ImVec2(7.f, 5.f);
            style.TouchExtraPadding = ImVec2(0.f, 0.f);
            style.IndentSpacing = 5.f;
            style.ScrollbarSize = 13.f;
            style.GrabMinSize = 12.f;

            style.WindowBorderSize = 1.f;
            style.ChildBorderSize = 1.f;
            style.PopupBorderSize = 1.f;
            style.FrameBorderSize = 1.f;
            style.TabBorderSize = 0.f;

            style.WindowRounding = 9.f;
            style.ChildRounding = 8.f;
            style.FrameRounding = 7.f;
            style.PopupRounding = 8.f;
            style.ScrollbarRounding = 10.f;
            style.GrabRounding = 7.f;
            style.LogSliderDeadzone = 4.f;
            style.TabRounding = 8.f;

            style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
            style.ColorButtonPosition = ImGuiDir_Right;
            style.SeparatorTextBorderSize = 1.f;
            style.SeparatorTextAlign = ImVec2(0.f, 0.5f);
            style.SeparatorTextPadding = ImVec2(20.f, 3.f);

            ImVec4 *colors = style.Colors;
            colors[ImGuiCol_Text] = Color(31, 43, 38);
            colors[ImGuiCol_TextDisabled] = Color(103, 119, 110);
            colors[ImGuiCol_WindowBg] = Color(218, 230, 213);
            colors[ImGuiCol_ChildBg] = Color(226, 236, 221);
            colors[ImGuiCol_PopupBg] = Color(232, 240, 226);
            colors[ImGuiCol_Border] = Color(170, 190, 164);
            colors[ImGuiCol_BorderShadow] = Color(255, 255, 255, 0.f);
            colors[ImGuiCol_FrameBg] = Color(206, 222, 201);
            colors[ImGuiCol_FrameBgHovered] = Color(194, 215, 188);
            colors[ImGuiCol_FrameBgActive] = Color(176, 202, 170);
            colors[ImGuiCol_TitleBg] = Color(198, 216, 193);
            colors[ImGuiCol_TitleBgActive] = Color(184, 207, 179);
            colors[ImGuiCol_TitleBgCollapsed] = Color(208, 224, 203);
            colors[ImGuiCol_MenuBarBg] = Color(203, 221, 198);
            colors[ImGuiCol_ScrollbarBg] = Color(215, 228, 210);
            colors[ImGuiCol_ScrollbarGrab] = Color(150, 176, 143);
            colors[ImGuiCol_ScrollbarGrabHovered] = Color(125, 160, 121);
            colors[ImGuiCol_ScrollbarGrabActive] = Color(94, 135, 101);
            colors[ImGuiCol_CheckMark] = Color(58, 106, 86);
            colors[ImGuiCol_SliderGrab] = Color(99, 147, 115);
            colors[ImGuiCol_SliderGrabActive] = Color(70, 122, 99);
            colors[ImGuiCol_Button] = Color(198, 217, 192);
            colors[ImGuiCol_ButtonHovered] = Color(181, 207, 175);
            colors[ImGuiCol_ButtonActive] = Color(151, 185, 147);
            colors[ImGuiCol_Header] = Color(194, 214, 188);
            colors[ImGuiCol_HeaderHovered] = Color(175, 203, 169);
            colors[ImGuiCol_HeaderActive] = Color(149, 184, 145);
            colors[ImGuiCol_Separator] = Color(176, 195, 170);
            colors[ImGuiCol_SeparatorHovered] = Color(120, 156, 115);
            colors[ImGuiCol_SeparatorActive] = Color(88, 129, 94);
            colors[ImGuiCol_ResizeGrip] = Color(130, 166, 124, 0.45f);
            colors[ImGuiCol_ResizeGripHovered] = Color(109, 150, 107, 0.75f);
            colors[ImGuiCol_ResizeGripActive] = Color(78, 125, 88, 0.95f);
            colors[ImGuiCol_Tab] = Color(203, 222, 198);
            colors[ImGuiCol_TabHovered] = Color(177, 205, 171);
            colors[ImGuiCol_TabActive] = Color(226, 236, 221);
            colors[ImGuiCol_TabUnfocused] = Color(202, 219, 198);
            colors[ImGuiCol_TabUnfocusedActive] = Color(216, 229, 211);
            colors[ImGuiCol_DockingPreview] = Color(86, 134, 101, 0.45f);
            colors[ImGuiCol_DockingEmptyBg] = Color(209, 224, 204);
            colors[ImGuiCol_PlotLines] = Color(80, 120, 101);
            colors[ImGuiCol_PlotLinesHovered] = Color(61, 101, 84);
            colors[ImGuiCol_PlotHistogram] = Color(151, 112, 62);
            colors[ImGuiCol_PlotHistogramHovered] = Color(128, 93, 49);
            colors[ImGuiCol_TableHeaderBg] = Color(199, 217, 193);
            colors[ImGuiCol_TableBorderStrong] = Color(158, 182, 152);
            colors[ImGuiCol_TableBorderLight] = Color(188, 207, 182);
            colors[ImGuiCol_TableRowBg] = Color(226, 236, 221, 0.f);
            colors[ImGuiCol_TableRowBgAlt] = Color(205, 221, 200, 0.52f);
            colors[ImGuiCol_TextSelectedBg] = Color(137, 178, 134, 0.45f);
            colors[ImGuiCol_DragDropTarget] = Color(91, 139, 98, 0.85f);
            colors[ImGuiCol_NavHighlight] = Color(76, 122, 93, 0.60f);
            colors[ImGuiCol_NavWindowingHighlight] = Color(255, 255, 255, 0.70f);
            colors[ImGuiCol_NavWindowingDimBg] = Color(55, 70, 62, 0.20f);
            colors[ImGuiCol_ModalWindowDimBg] = Color(55, 70, 62, 0.28f);
        }
    }

    void EditorTheme::Apply()
    {
        Internal::ConfigureFonts();
        Internal::ConfigureStyle();
    }
}