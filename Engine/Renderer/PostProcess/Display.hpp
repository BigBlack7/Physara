#pragma once

/// @file
/// @brief 准备设备级显示资源, 并按实际请求组装本 View 的后处理图.
/// 共享 LUT 只有在正常合成接受后才作为已定义内容供后继帧导入.

#include <array>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include <DisplayPost/Data.hpp>
#include <Shader/RhiProgram.hpp>

#include "../View.hpp"
#include "ColorLut.hpp"

namespace Physara::Renderer::PostProcess
{
    namespace Abi = Shader::Generated::DisplayPost;

    inline constexpr auto LinearFormat = RHI::Format::RGBA16_FLOAT;
    inline constexpr auto OutputFormat = RHI::Format::SRGBA8_UNORM;

    /// @brief 同一设备共享的固定 atlas. 首次可靠接受后清除设备的待写引用.
    /// 已准备的帧各自保有上传字节, GPU 纹理仍沿提交寿命保活.
    struct ResidentLut
    {
        RHI::TextureHandle texture;
        std::shared_ptr<const std::vector<LutTexel>> pendingPixels;
    };

    /// @brief 由帧槽持有的程序和布局, 覆盖本帧的所有记录及提交访问.
    struct DisplayStage
    {
        std::string_view name;
        RHI::BindingLayoutHandle layout;
        RHI::GraphicsPipelineHandle pipeline;
    };

    class DisplayDevice;

    /// @brief 一次正常候选的固定选择和图输入, 不保存上次图的版本.
    class DisplayFrame final
    {
    public:
        /// @brief 本候选要上传的逻辑字节数和笔数, 包括首次 LUT 和按需使用的矩形常量, 不预留设备容量.
        [[nodiscard]] std::size_t UploadBytes() const noexcept;
        [[nodiscard]] std::size_t UploadCount() const noexcept;
        /// @brief 本候选仍持有的 atlas 上传字节数. Forward 用它扣除另一 View 已预留的同一笔写入.
        [[nodiscard]] std::size_t LutUploadBytes() const noexcept;
        [[nodiscard]] bool UsesLut() const noexcept { return bool(m_Lut); }
        /// @brief 在本帧缓冲写入前声明 LUT 上传或导入. 诊断模式返回空纹理代.
        /// @note 上传 Pass 借用本帧的不可变像素. 首次接受后准备的新帧直接导入 atlas.
        [[nodiscard]] GraphResult<TextureId> DeclareLut(RenderGraph &graph, std::vector<GraphUpload> &uploads) const;
        /// @brief 按准备好的顺序消费工作色, 只有最终图目标执行 sRGB 存储编码.
        /// @pre working 和 DeclareLut 返回的版本来自同一张图. 声明失败时放弃整张候选图.
        [[nodiscard]] GraphResult<TextureId> Declare(RenderGraph &graph, std::vector<GraphUpload> &uploads,
                                                      TextureId working, TextureId lut) const;
        /// @brief 确认设备可供新帧导入的 atlas 内容, 不修改其他已准备帧.
        /// @pre 当前图完整执行且获可靠提交, 此 atlas 的上传 Pass 在计划中存活.
        /// 同图任一使用该 LUT 的显示出口都可使这一笔上传存活.
        void Confirm() noexcept;
        /// @brief 放下本槽的 LUT 像素引用, 不影响其他已准备槽. 正常发布先 Confirm, 取消则不确认.
        /// @pre 图回调已结束或图已放弃, 不再借用本槽像素. 释放无需等待 GPU 完成.
        void ReleasePayload() noexcept;

    private:
        friend class DisplayDevice;
        ViewLayouts m_Layouts;
        Abi::DisplayRects m_Rects{};
        std::shared_ptr<ResidentLut> m_Lut;
        std::shared_ptr<const std::vector<LutTexel>> m_LutPixels;
        RHI::SamplerHandle m_Sampler;
        std::array<DisplayStage, 3> m_Stages;
        std::size_t m_StageCount = 0;
    };

    /// @brief 设备持有包, 布局, 按附件格式缓存的 Pipeline 及固定 LUT; 帧只选择本次要用的阶段.
    class DisplayDevice final
    {
    public:
        /// @brief 借用设备并保存按 Clamp, Grade, Fxaa, Scale 排列的包路径; 构造时不读包.
        DisplayDevice(RHI::Device &device, std::array<std::filesystem::path, 4> paths);
        /// @brief 按两套布局及已解析的实际配置组装候选; 失败返回原因, 不替换传入的 frame.
        /// 已取得的设备资源可保留供重试, 不等同于图已接受或上传已完成.
        [[nodiscard]] std::optional<Core::BoundedText> Prepare(DisplayFrame &frame, const ViewLayouts &layouts,
                                                               DisplaySettings settings);

    private:
        enum class Stage : std::uint8_t { Clamp, Grade, Fxaa, Scale };
        /// @brief 同一阶段共用 Shader 和布局. 两个 Pipeline 槽分别写线性中间附件和 sRGB 终点.
        struct Program
        {
            Shader::RhiProgram shaders;
            RHI::BindingLayoutHandle layout;
            std::array<RHI::GraphicsPipelineHandle, 2> variants;
        };

        [[nodiscard]] DisplayStage PrepareStage(Stage stage, RHI::Format format);
        void PrepareLut();

        RHI::Device &m_Device;
        std::array<std::filesystem::path, 4> m_Paths;
        std::array<Program, 4> m_Programs;
        std::shared_ptr<ResidentLut> m_Lut;
        RHI::SamplerHandle m_Sampler;
    };
}
