#pragma once

/// @file
/// @brief 自持 canonical 模块与入口反射, 供 Pipeline 准备使用, 不持有 Engine 包或 GL Shader 名称.

#include "Backend.hpp"

#include <compare>
#include <span>
#include <string>
#include <vector>

namespace Physara::RHI::OpenGL
{
    /// @brief canonical set/binding 身份, 与 GL 物理单元不同.
    struct ShaderBindingKey
    {
        std::uint32_t set = 0;
        std::uint32_t binding = 0;
        auto operator<=>(const ShaderBindingKey &) const = default;
    };

    /// @brief 活动资源及其完整数值布局签名, extent/stride 的单位为字节.
    /// @note id 只属于该模块, 跨阶段以 key 合并; layout 比较数值字段而非名称或对象 padding.
    struct ShaderResource
    {
        ShaderBindingKey key;
        ResourceType type = ResourceType::None;
        std::uint32_t id = 0;
        std::uint32_t extent = 0;
        std::uint32_t recordStride = 0;
        std::vector<std::uint32_t> layout;
    };

    /// @brief 一个标量/向量 location, scalar 为 Float/Int/UInt 对应的 GL 基类型.
    /// @note interpolation 的 bit 0 为 flat, bit 1 为 noperspective, 零表示 smooth.
    struct ShaderIo
    {
        std::uint32_t location = 0;
        GLenum scalar = 0;
        std::uint32_t components = 0;
        std::uint32_t interpolation = 0;
        bool operator==(const ShaderIo &) const = default;
    };

    /// @brief IR 中实际的纹理/采样器对, dummy 为 true 时 sampler 身份不对应公共资源.
    struct ShaderSamplingPair
    {
        ShaderBindingKey texture;
        ShaderBindingKey sampler;
        bool dummy = false;
        auto operator<=>(const ShaderSamplingPair &) const = default;
    };

    /// @brief 原始模块边界完成一次解析, 之后按不可变反射准备 Pipeline.
    /// @note 仅持有 CPU 模块和 Device, 不创建 GL Shader; 反射/字节借用均不跨对象销毁.
    class OpenGLShader final : public RefCounter<RHI::Shader>
    {
    public:
        /// @brief 复制已通过工厂长度/阶段预检的字节, 解析 profile, 活动资源, IO 和采样对.
        /// @throws std::exception 解析或准备失败时由 CreateShader 转为诊断和空句柄.
        OpenGLShader(OpenGLDevice *device, const ShaderDesc &desc, const void *bytes, std::size_t size);
        [[nodiscard]] const ShaderDesc &GetDesc() const override
        {
            return m_Desc;
        }
        /// @brief 返回对象内字节借用, Shader 销毁前有效, 不引用调用方输入.
        void GetBytecode(const void **bytes, std::size_t *size) const override;
        [[nodiscard]] OpenGLDevice *Owner() const noexcept
        {
            return m_Device.Get();
        }
        [[nodiscard]] const std::vector<std::uint32_t> &Words() const noexcept
        {
            return m_Words;
        }
        [[nodiscard]] const std::vector<ShaderResource> &Resources() const noexcept
        {
            return m_Resources;
        }
        [[nodiscard]] const std::vector<ShaderSamplingPair> &Pairs() const noexcept
        {
            return m_Pairs;
        }
        [[nodiscard]] const std::vector<ShaderIo> &Inputs() const noexcept
        {
            return m_Inputs;
        }
        [[nodiscard]] const std::vector<ShaderIo> &Outputs() const noexcept
        {
            return m_Outputs;
        }
        /// @brief 活动 PointSize 输出决定原生点光栅是否消费 Shader 大小, 不改变 canonical 模块.
        [[nodiscard]] bool WritesPointSize() const noexcept
        {
            return m_WritesPointSize;
        }

    private:
        friend class OpenGLCommandList;
        RefCountPtr<OpenGLDevice> m_Device;
        InternalOwnership m_InternalOwners;
        ShaderDesc m_Desc;
        bool m_WritesPointSize = false;
        std::vector<std::uint32_t> m_Words;
        std::vector<ShaderResource> m_Resources;
        std::vector<ShaderSamplingPair> m_Pairs;
        std::vector<ShaderIo> m_Inputs;
        std::vector<ShaderIo> m_Outputs;
    };

    /// @brief 准备算法内部的失败出口, 由公开工厂捕获并转为既有空句柄/诊断.
    void RequirePreparation(bool condition, const std::string &reason);
} // namespace Physara::RHI::OpenGL
