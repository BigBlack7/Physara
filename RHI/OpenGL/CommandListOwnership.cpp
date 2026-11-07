#include "CommandList.hpp"

/// @file
/// @brief 将本次执行对象和实际复合强引用接到原物理包, 不建立全局对象注册表或第二份完成队列.

#include <cassert>
#include <new>
#include <utility>

#include "InputLayout.hpp"
#include "Pipeline.hpp"

namespace Physara::RHI::OpenGL
{
    InternalOwnership *OpenGLCommandList::RetainResource(Resource *resource)
    {
        // 本包已保活的根不再展开; 依赖记录描述父对象已有的强引用边, 不增加第二组子引用.
        for (const auto &lease : m_Packet->resources)
        {
            if (lease.Get() == resource)
            {
                return lease.Owners();
            }
        }
        try
        {
            InternalOwnership *owners = nullptr;
            std::vector<InternalOwnership *> dependencies(0);
            // 先取得子对象 lease, 再发布父对象 lease; 失败留下的准备也由原包统一退休.
            const auto add = [&](Resource *child)
            {
                if (child == nullptr)
                {
                    return true;
                }
                auto *childOwners = RetainResource(child);
                if (!childOwners)
                {
                    return false;
                }
                dependencies.push_back(childOwners);
                return true;
            };
            if (auto *buffer = dynamic_cast<OpenGLBuffer *>(resource))
            {
                owners = &buffer->m_InternalOwners;
            }
            else if (auto *texture = dynamic_cast<OpenGLTexture *>(resource))
            {
                owners = &texture->m_InternalOwners;
            }
            else if (auto *sampler = dynamic_cast<OpenGLSampler *>(resource))
            {
                owners = &sampler->m_InternalOwners;
            }
            else if (auto *shader = dynamic_cast<OpenGLShader *>(resource))
            {
                owners = &shader->m_InternalOwners;
            }
            else if (auto *layout = dynamic_cast<OpenGLBindingLayout *>(resource))
            {
                owners = &layout->m_InternalOwners;
            }
            else if (auto *input = dynamic_cast<OpenGLInputLayout *>(resource))
            {
                owners = &input->m_InternalOwners;
            }
            else if (auto *set = dynamic_cast<OpenGLBindingSet *>(resource))
            {
                owners = &set->m_InternalOwners;
                if (!add(set->m_Layout))
                {
                    return nullptr;
                }
                for (const auto &item : set->m_Resources)
                {
                    if (!add(item))
                    {
                        return nullptr;
                    }
                }
            }
            else if (auto *framebuffer = dynamic_cast<OpenGLFramebuffer *>(resource))
            {
                owners = &framebuffer->m_InternalOwners;
                for (const auto &attachmentTexture : framebuffer->m_Textures)
                {
                    if (!add(attachmentTexture))
                    {
                        return nullptr;
                    }
                }
            }
            else if (auto *pipeline = dynamic_cast<OpenGLGraphicsPipeline *>(resource))
            {
                owners = &pipeline->m_InternalOwners;
                const auto &desc = pipeline->GetDesc();
                if (!add(desc.VS) || !add(desc.PS) || !add(desc.inputLayout))
                {
                    return nullptr;
                }
                for (const auto &bindingLayout : desc.bindingLayouts)
                {
                    if (!add(bindingLayout))
                    {
                        return nullptr;
                    }
                }
            }
            // 所有入口及复合子对象已由创建/候选边界确认实际实现, 不再重复解析描述.
            assert(owners != nullptr);
            if (m_Packet->resources.size() == UploadItemLimit)
            {
                m_Device->ReportBudget("RetainExecutionResource", 1, UploadItemLimit, UploadItemLimit);
                return nullptr;
            }
            // 保留边的重数, 同一子对象被父对象多个字段持有时也须逐边计账.
            m_Packet->resources.emplace_back(*m_Device, resource, *owners, std::move(dependencies));
            return owners;
        }
        catch (const std::bad_alloc &)
        {
            m_Device->Report("RetainExecutionResource", "CPU allocation failed before native operation");
            return nullptr;
        }
    }
} // namespace Physara::RHI::OpenGL
