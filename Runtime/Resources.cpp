#include "Resources.hpp"

/// @file
/// @brief Windows 主模块路径的冷启动查询, 为 CPU 资产和生成包提供一致的位置基准.

#include <array>
#include <string_view>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include "RuntimeResources.hpp"

namespace Physara::Runtime
{
    std::variant<ResourceLocations, Core::BoundedText> LocateResources()
    {
        // 用宽字符保留 Windows 主模块路径. 缓冲区填满按截断处理, 防止从不完整目录解析包.
        std::array<wchar_t, 32768> module{};
        const auto length = GetModuleFileNameW(nullptr, module.data(), static_cast<DWORD>(module.size()));
        if (length == 0 || length >= module.size())
        {
            return Core::BoundedText("Cannot locate the product executable directory");
        }
        const std::filesystem::path executable(std::wstring_view(module.data(), length));
        // 各配置相对 exe 定位包, Renderer 按实际表面和受光角色加载.
        const auto directory = executable.parent_path();
        ResourceLocations result{directory, (directory / EditorUiPackageRelative).lexically_normal(),
                                {(directory / StaticForwardPackageRelative).lexically_normal(),
                                 (directory / StaticDepthPackageRelative).lexically_normal(),
                                 (directory / ForwardDisplayPackageRelative).lexically_normal(),
                                 (directory / StaticForwardPhysicalPackageRelative).lexically_normal(),
                                 (directory / StaticForwardIblPackageRelative).lexically_normal(),
                                 (directory / ForwardSkyPackageRelative).lexically_normal(),
                                 (directory / DisplayColorGradingPackageRelative).lexically_normal(),
                                 (directory / DisplayFxaaPackageRelative).lexically_normal(),
                                 (directory / DisplayScalePackageRelative).lexically_normal(),
                                 (directory / StaticShadowDepthPackageRelative).lexically_normal(),
                                 (directory / StaticCascadePreviewPackageRelative).lexically_normal(),
                                 (directory / StaticSpotShadowPreviewPackageRelative).lexically_normal(),
                                 (directory / StaticPointShadowPreviewPackageRelative).lexically_normal(),
                                 (directory / StaticForwardShadowsPackageRelative).lexically_normal(),
                                 (directory / StaticForwardPhysicalShadowsPackageRelative).lexically_normal(),
                                 (directory / StaticForwardIblShadowsPackageRelative).lexically_normal(),
                                 {(directory / StaticForwardPlusPackageRelative).lexically_normal(),
                                  (directory / StaticForwardPlusPhysicalPackageRelative).lexically_normal(),
                                  (directory / StaticForwardPlusIblPackageRelative).lexically_normal(),
                                  (directory / StaticForwardPlusShadowsPackageRelative).lexically_normal(),
                                  (directory / StaticForwardPlusPhysicalShadowsPackageRelative).lexically_normal(),
                                  (directory / StaticForwardPlusIblShadowsPackageRelative).lexically_normal()},
                                 (directory / StaticCoreGBufferPackageRelative).lexically_normal(),
                                 (directory / StaticSurfaceAuxPackageRelative).lexically_normal(),
                                 {(directory / DeferredLightingPackageRelative).lexically_normal(),
                                  (directory / DeferredLightingPhysicalPackageRelative).lexically_normal(),
                                  (directory / DeferredLightingIblPackageRelative).lexically_normal(),
                                  (directory / DeferredLightingShadowsPackageRelative).lexically_normal(),
                                  (directory / DeferredLightingPhysicalShadowsPackageRelative).lexically_normal(),
                                  (directory / DeferredLightingIblShadowsPackageRelative).lexically_normal()},
                                 {(directory / SurfaceDepthPreviewPackageRelative).lexically_normal(),
                                  (directory / SurfaceGBufferPreviewPackageRelative).lexically_normal()}}};
        static_assert(std::size(IorSpecularPackageRelatives) == Shader::IorSpecularProgramCount);
        for (std::size_t i = 0; i < result.forward.iorSpecularColors.size(); ++i)
        {
            result.forward.iorSpecularColors[i] = (directory / IorSpecularPackageRelatives[i]).lexically_normal();
        }
        static_assert(std::size(ClearcoatPackageRelatives) == Shader::ClearcoatProgramCount);
        for (std::size_t i = 0; i < result.forward.clearcoatColors.size(); ++i)
        {
            result.forward.clearcoatColors[i] = (directory / ClearcoatPackageRelatives[i]).lexically_normal();
        }
        static_assert(std::size(SheenPackageRelatives) == Shader::SheenProgramCount);
        for (std::size_t i = 0; i < result.forward.sheenColors.size(); ++i)
        {
            result.forward.sheenColors[i] = (directory / SheenPackageRelatives[i]).lexically_normal();
        }
        return result;
    }
}
