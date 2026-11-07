#pragma once

/// @file
/// @brief 常规逻辑布局和资源集合, 创建时解析 view/range, Pipeline 单独决定物理绑定位置.

#include "Shader.hpp"

namespace Physara::RHI::OpenGL
{
    /// @brief 按布局的资源类别 offset 计算 canonical binding, 溢出或基线外类型抛准备错误.
    [[nodiscard]] std::uint32_t CanonicalBinding(const BindingLayoutDesc &desc, const BindingLayoutItem &item);

    /// @brief 不占原生单元的逻辑布局, 描述借用在对象存活期间有效.
    /// @note 创建只保存类别/slot/offset 和 set 策略; 实际 set 与紧凑单元在 Pipeline 中确定.
    class OpenGLBindingLayout final : public RefCounter<RHI::BindingLayout>
    {
    public:
        OpenGLBindingLayout(OpenGLDevice *device, const BindingLayoutDesc &desc) : m_Device(device), m_Desc(desc) {}
        [[nodiscard]] const BindingLayoutDesc *GetDesc() const override
        {
            return &m_Desc;
        }
        [[nodiscard]] const BindlessLayoutDesc *GetBindlessDesc() const override
        {
            return nullptr;
        }
        [[nodiscard]] OpenGLDevice *Owner() const noexcept
        {
            return m_Device.Get();
        }

    private:
        friend class OpenGLCommandList;
        RefCountPtr<OpenGLDevice> m_Device;
        InternalOwnership m_InternalOwners;
        BindingLayoutDesc m_Desc;
    };

    /// @brief 已解析的单项绑定, name 是 buffer/view/sampler, range 仅用于 buffer, slot 仍为逻辑身份.
    /// @note recordStride 取自 Buffer 描述; 与具体 Shader extent/stride 的一致性仍需在组合消费时核对.
    struct NativeBinding
    {
        std::uint32_t slot = 0;
        ResourceType type = ResourceType::None;
        GLuint name = 0;
        BufferRange range;
        std::uint32_t recordStride = 0;
        Resource *resource = nullptr;       ///< 借用集合强持有的实际 storage/sampler, 不以 view 名称推断资源身份.
        TextureSubresourceSet subresources; ///< SRV 已解析的 mip/layer 范围, 供实际用途和 feedback 检查.
    };

    /// @brief 集合拥有布局/资源与其自有 view, trackLiveness 只影响即时列表是否自动保活整个集合.
    /// @note trackLiveness=false 不改变集合自身的依赖所有权, 调用方须将集合及依赖保留到工作完成.
    /// @note 描述与解析记录均只读, 按集合输入顺序保存; 取项时以 type/slot 解释, 不借用布局项的序号.
    /// 发布后 CPU 最终 Release 可在 worker 执行, 自有 view 仅交给 Device 的 owner-context 删除队列.
    class OpenGLBindingSet final : public RefCounter<RHI::BindingSet>
    {
    public:
        /// @brief 先保存全部依赖并预分配 view 删除节点, 原生视图由 Device 工厂随后物化.
        OpenGLBindingSet(OpenGLDevice *device, const BindingSetDesc &desc, OpenGLBindingLayout *layout);
        ~OpenGLBindingSet() override;
        [[nodiscard]] const BindingSetDesc *GetDesc() const override
        {
            return &m_Desc;
        }
        [[nodiscard]] BindingLayout *GetLayout() const override
        {
            return m_Layout.Get();
        }
        [[nodiscard]] OpenGLDevice *Owner() const noexcept
        {
            return m_Device.Get();
        }
        [[nodiscard]] const std::vector<NativeBinding> &Bindings() const noexcept
        {
            return m_Bindings;
        }

    private:
        friend class OpenGLCommandList;
        RefCountPtr<OpenGLDevice> m_Device;
        InternalOwnership m_InternalOwners;
        BindingSetDesc m_Desc;
        RefCountPtr<OpenGLBindingLayout> m_Layout;
        std::vector<ResourceHandle> m_Resources;
        std::vector<NativeBinding> m_Bindings;
        std::vector<std::unique_ptr<NativeDeletion>> m_Views;
        bool m_Published = false;
        friend class OpenGLDevice;
    };
} // namespace Physara::RHI::OpenGL
