#include "CommandList.hpp"

/// @file
/// @brief 预检单阶段计算候选, 在实际调度时消费活动范围并沿用即时列表的保活.

#include "Pipeline.hpp"

namespace Physara::RHI::OpenGL
{
    // 两个已解析条目已指向同一存储. 按原始字节或 mip/layer 判断两个独立绑定是否相交.
    static bool StorageOverlap(const NativeBinding &left, const NativeBinding &right) noexcept
    {
        if (left.dimension != TextureDimension::Unknown)
        {
            const auto &a = left.subresources;
            const auto &b = right.subresources;
            return a.baseMipLevel < b.baseMipLevel + b.numMipLevels && b.baseMipLevel < a.baseMipLevel + a.numMipLevels &&
                   a.baseArraySlice < b.baseArraySlice + b.numArraySlices &&
                   b.baseArraySlice < a.baseArraySlice + a.numArraySlices;
        }
        return left.range.byteOffset < right.range.byteOffset + right.range.byteSize &&
               right.range.byteOffset < left.range.byteOffset + left.range.byteSize;
    }

    bool OpenGLCommandList::ValidateComputeBindings(const ComputeState &state, const OpenGLComputePipeline &pipeline)
    {
        const auto &layouts = pipeline.GetDesc().bindingLayouts;
        if (state.bindings.size() != layouts.size())
        {
            m_Device->Report("SetComputeState", "InterfaceMismatch: compute binding set count");
            return false;
        }
        for (std::size_t index = 0; index < state.bindings.size(); ++index)
        {
            const auto *set = dynamic_cast<const OpenGLBindingSet *>(state.bindings[index]);
            if (!set || set->Owner() != m_Device.Get() || set->GetLayout() != layouts[index].Get())
            {
                m_Device->Report("SetComputeState", "InterfaceMismatch: compute binding owner/layout");
                return false;
            }
        }
        const auto &resources = pipeline.Plan().resources;
        for (std::size_t index = 0; index < resources.size(); ++index)
        // 活动项来自已链接 Pipeline 的计划. 先核对该项的 ABI 和方向能力, 再与先前活动项比较物理别名.
        {
            const auto &entry = resources[index];
            const auto &binding = BindingFor(state.bindings, entry);
            if ((entry.resource.extent != 0 && binding.range.byteSize < entry.resource.extent) ||
                (entry.resource.recordStride != 0 && binding.recordStride != entry.resource.recordStride) ||
                (entry.resource.dimension != TextureDimension::Unknown && binding.dimension != entry.resource.dimension))
            {
                m_Device->Report("SetComputeState", "InterfaceMismatch: compute active range, stride or dimension");
                return false;
            }
            if (binding.type == ResourceType::Texture_SRV && GetFormatInfo(binding.imageFormat).kind == FormatKind::Integer)
            {
                m_Device->Report("SetComputeState", "InterfaceMismatch: compute sampled texture requires float view");
                return false;
            }
            if (binding.type == ResourceType::Texture_UAV)
            {
                if (binding.imageFormat != entry.resource.imageFormat)
                {
                    m_Device->Report("SetComputeState", "InterfaceMismatch: compute storage image format");
                    return false;
                }
                const auto &texture = static_cast<const OpenGLTexture *>(binding.resource)->GetDesc();
                const auto storage = m_Device->GetFormatCaps(texture.format, texture.dimension).support;
                const auto view = m_Device->GetFormatCaps(texture.format, binding.dimension).support;
                auto required = FormatSupport::None;
                if (CoversStorageAccess(entry.resource.access, StorageAccess::Read))
                {
                    required = required | FormatSupport::ShaderUavLoad;
                }
                if (CoversStorageAccess(entry.resource.access, StorageAccess::Write))
                {
                    required = required | FormatSupport::ShaderUavStore;
                }
                if ((storage & required) != required || (view & required) != required)
                {
                    m_Device->Report("SetComputeState", "Unsupported: compute image access direction");
                    return false;
                }
            }
            for (std::size_t prior = 0; prior < index; ++prior)
            {
                const auto &other = BindingFor(state.bindings, resources[prior]);
                if (!binding.resource || binding.resource != other.resource)
                {
                    continue;
                }
                const bool write = CoversStorageAccess(entry.resource.access, StorageAccess::Write) ||
                    CoversStorageAccess(resources[prior].resource.access, StorageAccess::Write);
                // Buffer 的公共状态粒度是整对象. 图像可以在不相交的 mip/layer 上分别使用采样和存储视图.
                const bool mixedBufferState = (binding.type == ResourceType::StructuredBuffer_UAV) !=
                                              (other.type == ResourceType::StructuredBuffer_UAV);
                const bool mixedImageState = (binding.type == ResourceType::Texture_UAV) !=
                                             (other.type == ResourceType::Texture_UAV);
                if (mixedBufferState || ((write || mixedImageState) && StorageOverlap(binding, other)))
                {
                    m_Device->Report("SetComputeState", "InvalidState: incompatible aliased compute bindings");
                    return false;
                }
            }
        }
        return true;
    }

    void OpenGLCommandList::SetComputeState(const ComputeState &state)
    {
        constexpr auto Operation = "SetComputeState";
        auto *packet = Recording(Operation);
        if (!packet)
        {
            return;
        }
        if (packet->useMode == UseMode::GraphManaged)
        {
            ResetPipelineState();
        }
        auto *pipeline = dynamic_cast<OpenGLComputePipeline *>(state.pipeline);
        if (!pipeline || pipeline->Owner() != m_Device.Get() || state.indirectParams)
        {
            m_Device->Report(Operation, "InvalidResource or Unsupported: compute pipeline owner or indirect params");
            return;
        }
        // 先取得 Pipeline 和选择自动保活的集合. 全部成功后才发布新快照并开启新的 push 区间.
        if (!ValidateComputeBindings(state, *pipeline) || !RetainResource(pipeline))
        {
            return;
        }
        for (auto *set : state.bindings)
        {
            if (set->GetDesc()->trackLiveness && !RetainResource(set))
            {
                return;
            }
        }
        packet->compute = state;
        packet->computeSet = true;
        packet->graphicsSet = false;
        packet->push = {};
    }

    bool OpenGLCommandList::ComputeUses(const BindingSetVector &bindings, const PipelineBindingPlan &plan)
    {
        constexpr auto Operation = "Dispatch";
        // 存储条目按 IR 方向请求图授权. 采样, 普通常量和结构化 SRV 各自登记实际后继域.
        for (const auto &entry : plan.resources)
        {
            const auto &binding = BindingFor(bindings, entry);
            switch (binding.type)
            {
            case ResourceType::Texture_UAV:
                if (!UseTexture(*static_cast<OpenGLTexture *>(binding.resource), binding.subresources,
                                TextureAspect::Color, ResourceStates::UnorderedAccess, Operation,
                                ResourceStates::Unknown, entry.resource.access, GL_SHADER_IMAGE_ACCESS_BARRIER_BIT))
                {
                    return false;
                }
                break;
            case ResourceType::Texture_SRV:
            {
                auto &texture = *static_cast<OpenGLTexture *>(binding.resource);
                const bool depth = GetFormatInfo(texture.GetDesc().format).hasDepth;
                const auto state = ResourceStates::NonPixelShaderResource |
                                   (depth ? ResourceStates::DepthRead : ResourceStates::Unknown);
                if (!UseTexture(texture, binding.subresources, depth ? TextureAspect::Depth : TextureAspect::Color,
                                state, Operation, ResourceStates::NonPixelShaderResource, StorageAccess::None,
                                GL_TEXTURE_FETCH_BARRIER_BIT))
                {
                    return false;
                }
                break;
            }
            case ResourceType::StructuredBuffer_UAV:
            case ResourceType::StructuredBuffer_SRV:
            case ResourceType::ConstantBuffer:
            {
                const bool unordered = binding.type == ResourceType::StructuredBuffer_UAV;
                const auto state = unordered ? ResourceStates::UnorderedAccess :
                    (binding.type == ResourceType::ConstantBuffer ? ResourceStates::ConstantBuffer :
                                                                     ResourceStates::NonPixelShaderResource);
                if (!UseBuffer(*static_cast<OpenGLBuffer *>(binding.resource), state, Operation, binding.range,
                               binding.type == ResourceType::StructuredBuffer_SRV,
                               unordered ? entry.resource.access : StorageAccess::None,
                               binding.type == ResourceType::ConstantBuffer ? GL_UNIFORM_BARRIER_BIT :
                                                                            GL_SHADER_STORAGE_BARRIER_BIT))
                {
                    return false;
                }
                break;
            }
            case ResourceType::VolatileConstantBuffer:
                if (!CheckGraphBufferUse(*static_cast<OpenGLBuffer *>(binding.resource), ResourceStates::ConstantBuffer,
                                         Operation, binding.range))
                {
                    return false;
                }
                break;
            default:
                break;
            }
        }
        return true;
    }

    void OpenGLCommandList::Dispatch(std::uint32_t x, std::uint32_t y, std::uint32_t z)
    {
        constexpr auto Operation = "Dispatch";
        auto *packet = Recording(Operation);
        if (!packet)
        {
            return;
        }
        const auto &limit = m_Device->m_Caps.shader.groupCount;
        if (!packet->computeSet || x > limit[0] || y > limit[1] || z > limit[2])
        {
            m_Device->Report(Operation, "InvalidState or LimitExceeded: active compute state or group count");
            return;
        }
        if (x == 0 || y == 0 || z == 0)
        {
            return;
        }
        // 有效非空调度才选择动态版本和登记访问. 此前的 SetComputeState 或重用同一集合都不能代替本次检查.
        const auto &pipeline = *static_cast<const OpenGLComputePipeline *>(packet->compute.pipeline);
        const auto &plan = pipeline.Plan();
        const auto &bindings = packet->compute.bindings;
        if (!CheckConstants(bindings, plan, Operation) || !ComputeUses(bindings, plan) ||
            !m_Device->BeginNative(Operation) || !PrepareNativeAccesses(Operation))
        {
            return;
        }
        m_Device->m_ContextState.ApplyProgram(pipeline.Name(), false);
        ApplyProgramBindings(bindings, plan);
        ApplyConstants(bindings, plan);
        if (!m_Device->CheckNative("Dispatch.Bindings"))
        {
            packet->invalid = true;
            m_Device->m_ContextState.Invalidate();
            return;
        }
        // 原生调用只在这里发出一次. 包持有可能已发的前缀, 成功后再把本次访问发布给后继命令.
        glDispatchCompute(x, y, z);
        packet->issuedWork = true;
        if (m_Device->CheckNative(Operation))
        {
            RecordNativeAccesses();
        }
        else
        {
            packet->invalid = true;
        }
    }
} // namespace Physara::RHI::OpenGL
