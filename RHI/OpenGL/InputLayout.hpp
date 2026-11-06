#pragma once

/// @file
/// @brief 保存顶点属性位置和 GL 数据解释, 不创建 VAO 或绑定本帧 VB/IB.

#include "Shader.hpp"

namespace Physara::RHI::OpenGL
{
    /// @brief 一项展开后的顶点属性, location 按公开描述顺序计算, normalized 仅用于浮点输入转换.
    /// @note offset/stride 单位为字节; scalar 是 Shader 的输入类别, storage 是顶点数据的存储类型.
    struct VertexInput
    {
        std::uint32_t location = 0;
        std::uint32_t bufferIndex = 0;
        std::uint32_t offset = 0;
        std::uint32_t stride = 0;
        bool instanced = false;
        GLenum scalar = 0;
        GLenum storage = 0;
        std::uint32_t components = 0;
        bool normalized = false;
    };

    /// @brief GL core 可承接的顶点格式映射, false 不修改输出, 不从纹理格式元数据推断支持.
    [[nodiscard]] bool VertexFormat(Format format, VertexInput &input) noexcept;

    /// @brief 不可变 InputLayout, 描述和展开记录只借出到对象销毁, 强持有 Device.
    class OpenGLInputLayout final : public RefCounter<RHI::InputLayout>
    {
    public:
        /// @brief 复制原始描述, 受检展开 arraySize, 并核对共享 buffer 的 stride/实例频率与真实设备 limits.
        /// @note 原始属性与展开 location 数量可不同; 活动 VS 输入的一致性在 Pipeline 组合时核对.
        OpenGLInputLayout(OpenGLDevice *device, std::span<const VertexAttributeDesc> attributes);
        /// @brief 返回原始描述数量, 不等同于 arraySize 展开后的 location 总数.
        [[nodiscard]] std::uint32_t GetNumAttributes() const override;
        /// @brief 借用指定原始描述到对象销毁, 越界时返回空.
        [[nodiscard]] const VertexAttributeDesc *GetAttributeDesc(std::uint32_t index) const override;
        [[nodiscard]] const std::vector<VertexInput> &Inputs() const noexcept
        {
            return m_Inputs;
        }
        [[nodiscard]] OpenGLDevice *Owner() const noexcept
        {
            return m_Device.Get();
        }

    private:
        RefCountPtr<OpenGLDevice> m_Device;
        std::vector<VertexAttributeDesc> m_Attributes;
        std::vector<VertexInput> m_Inputs;
    };
} // namespace Physara::RHI::OpenGL
