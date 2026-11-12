#include "View.hpp"

#include <array>
#include <cassert>
#include <cstring>
#include <utility>

#include <Core/FrameArena.hpp>

/// @file
/// @brief 生成用于验证视口尺寸, 颜色和方向的静态诊断纹理. 接受后复用同尺寸输出.

namespace Physara::Renderer
{
    static constexpr std::size_t PixelBytes = 4;
    // 进程只有一个活动宿主. View 在该宿主 owner 线程分配身份, 后续创建不会复用旧代.
    static std::uint64_t LastViewIdentity = 0;

    // 用预算的除法上界校验尺寸, 再计算紧密 RGBA8 字节数.
    std::size_t DiagnosticPixelBytes(ViewExtent extent) noexcept
    {
        if (extent.width == 0 || extent.height == 0 ||
            extent.height > DiagnosticUploadBudget / PixelBytes / extent.width)
        {
            return 0;
        }
        return static_cast<std::size_t>(extent.width) * extent.height * PixelBytes;
    }

    std::optional<View> View::Create(RHI::Device &device) noexcept
    {
        if (!Core::AdvanceGeneration(LastViewIdentity))
        {
            return std::nullopt;
        }
        return View(device, {0, LastViewIdentity});
    }

    std::variant<NoSceneOutputs, Core::BoundedText> View::Prepare(const std::optional<ViewportRequest> &request,
                                                                std::size_t remainingUploadBytes)
    {
        NoSceneOutputs outputs;
        if (!request)
        {
            return outputs;
        }
        assert(request->view == m_Id);
        const auto extent = request->extent;
        const auto bytes = DiagnosticPixelBytes(extent);
        if (bytes == 0)
        {
            return outputs;
        }
        // 静态诊断图由实际采样尺寸确定. 相同尺寸可沿 Graphics 队列顺序直接读取已接受代.
        if (m_Diagnostic && m_Diagnostic->extent == extent)
        {
            outputs.m_Output = m_Diagnostic;
            return outputs;
        }
        // 保留无颜色结果供 Editor 解析占位. 未创建新纹理, 下次按届时布局重新判断额度.
        if (bytes > remainingUploadBytes)
        {
            outputs.m_UploadPending = true;
            return outputs;
        }

        const RHI::TextureDesc desc{.width = extent.width, .height = extent.height,
                                    .format = RHI::Format::SRGBA8_UNORM, .debugName = "Viewport diagnostic color",
                                    .initialState = RHI::ResourceStates::ShaderResource, .keepInitialState = true};
        auto texture = m_Device.CreateTexture(desc);
        if (!texture || !Core::AdvanceGeneration(m_LastGeneration))
        {
            return Core::BoundedText("Viewport diagnostic target or generation creation failed");
        }
        outputs.m_Output = ViewOutput{m_Id, ViewOutputRole::DiagnosticColor, m_LastGeneration, extent,
                                      std::move(texture)};
        outputs.m_UploadBytes = bytes;
        return outputs;
    }

    bool NoSceneOutputs::PrepareUpload(Core::FrameArena &arena) noexcept
    {
        if (m_UploadBytes == 0)
        {
            return true;
        }
        auto pixels = arena.Allocate(m_UploadBytes, 1);
        if (pixels.empty())
        {
            return false;
        }
        // 四个象限用固定的 sRGB 编码字节. 单独的颜色可观测上下行序, 不依赖窗口底色.
        constexpr std::array<std::array<std::uint8_t, PixelBytes>, 4> Colors{{
            {180, 71, 53, 255}, {41, 109, 149, 255}, {55, 127, 82, 255}, {118, 75, 161, 255}}};
        auto *pixel = pixels.data();
        const auto extent = m_Output->extent;
        for (std::uint32_t y = 0; y < extent.height; ++y)
        {
            const auto row = y < extent.height / 2 ? 0u : 2u;
            for (std::uint32_t x = 0; x < extent.width; ++x)
            {
                const auto &color = Colors[row + (x >= extent.width / 2)];
                std::memcpy(pixel, color.data(), PixelBytes);
                pixel += PixelBytes;
            }
        }
        m_Pixels = pixels;
        return true;
    }

    // 即时上传在调用内消费 arena 字节, 后端持有实际 GPU 使用的上传区及纹理依赖.
    void NoSceneOutputs::RecordUpload(RHI::CommandList &commands) const
    {
        if (!m_Pixels.empty())
        {
            commands.WriteTexture(m_Output->texture.Get(), 0, 0, m_Pixels.data(), m_Output->extent.width * PixelBytes);
        }
    }

    // 换入已接受的新代并释放旧代的 View 引用. 原有 UI 绑定由已提交的 RHI 包继续持有.
    void View::Publish(NoSceneOutputs &&outputs) noexcept
    {
        if (outputs.m_UploadBytes != 0)
        {
            assert(outputs.m_Output->view == m_Id && !outputs.m_Pixels.empty());
            m_Diagnostic = std::move(outputs.m_Output);
        }
    }
}
