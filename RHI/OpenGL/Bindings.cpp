#include "Bindings.hpp"

/// @file
/// @brief 先核对整组逻辑输入, 再物化独立 texture view; 创建不改变 context 的活动绑定.

#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>

namespace Physara::RHI::OpenGL
{
    std::uint32_t CanonicalBinding(const BindingLayoutDesc &desc, const BindingLayoutItem &item)
    {
        std::uint32_t offset = 0;
        switch (item.type)
        {
        case ResourceType::ConstantBuffer:
            offset = desc.bindingOffsets.constantBuffer;
            break;
        case ResourceType::StructuredBuffer_SRV:
        case ResourceType::Texture_SRV:
            offset = desc.bindingOffsets.shaderResource;
            break;
        case ResourceType::Sampler:
            offset = desc.bindingOffsets.sampler;
            break;
        default:
            throw std::runtime_error("Unsupported: binding type outside GraphicsBaselineV1");
        }
        RequirePreparation(item.slot <= std::numeric_limits<std::uint32_t>::max() - offset,
                           "InvalidArgument: canonical binding overflow");
        return offset + item.slot;
    }

    BindingLayoutHandle OpenGLDevice::CreateBindingLayout(const BindingLayoutDesc &desc)
    {
        if (!RequireContext("CreateBindingLayout"))
        {
            return nullptr;
        }
        try
        {
            const auto allowed = ShaderType::Vertex | ShaderType::Pixel;
            RequirePreparation(desc.visibility != ShaderType::None && (desc.visibility & ~allowed) == ShaderType::None,
                               "Unsupported: binding visibility");
            RequirePreparation(desc.registerSpace < 4 && (desc.registerSpaceIsDescriptorSet || desc.registerSpace == 0),
                               "InvalidArgument: layout register-space strategy");
            std::set<std::uint32_t> bindings;
            for (const auto &item : desc.bindings)
            {
                RequirePreparation(item.size == 1 && item.slot < 128, "Unsupported: descriptor count or slot");
                RequirePreparation(bindings.insert(CanonicalBinding(desc, item)).second,
                                   "InvalidArgument: colliding canonical layout bindings");
            }
            return BindingLayoutHandle::Create(new OpenGLBindingLayout(this, desc));
        }
        catch (const std::exception &error)
        {
            Report("CreateBindingLayout", error.what());
            return nullptr;
        }
    }

    OpenGLBindingSet::OpenGLBindingSet(OpenGLDevice *device, const BindingSetDesc &desc, OpenGLBindingLayout *layout)
        : m_Device(device), m_Desc(desc), m_Layout(layout)
    {
        m_Resources.reserve(desc.bindings.size());
        m_Bindings.reserve(desc.bindings.size());
        m_Views.reserve(desc.bindings.size());
        for (const auto &item : desc.bindings)
        {
            m_Resources.emplace_back(item.resourceHandle);
            m_Bindings.emplace_back();
            // 所有删除节点先于任何 GL 创建分配, 析构只交接这些节点.
            m_Views.push_back(item.type == ResourceType::Texture_SRV
                                  ? std::make_unique<NativeDeletion>(NativeKind::Texture)
                                  : nullptr);
        }
    }

    OpenGLBindingSet::~OpenGLBindingSet()
    {
        for (auto &view : m_Views)
        {
            m_Device->ReleaseAllocation(std::move(view), m_Published);
        }
        // view 先入队, storage 依赖随后释放, FIFO 保持 view 在 storage 之前退休.
        m_Resources.clear();
    }

    /// @brief 将显式或 whole-buffer 范围解析为精确范围, 不截断调用方给出的越界输入.
    /// @note 此处只证明资源用途与设备 range 合法, 不证明它覆盖任意 Shader 所需的块 extent.
    static BufferRange ResolveBuffer(const BindingSetItem &item, OpenGLBuffer &buffer, const ShaderCaps &caps)
    {
        const auto &desc = buffer.GetDesc();
        const bool constant = item.type == ResourceType::ConstantBuffer;
        RequirePreparation(!desc.isVolatile && (constant ? desc.isConstantBuffer : desc.structStride > 0),
                           "InvalidArgument: buffer binding usage");
        const auto offset = item.range.byteOffset;
        RequirePreparation(offset < desc.byteSize, "InvalidArgument: buffer binding offset");
        const auto size = item.range.byteSize == ~0ull ? desc.byteSize - offset : item.range.byteSize;
        const auto alignment = constant ? caps.uniformAlignment : caps.storageAlignment;
        const auto maximum = constant ? caps.uniformBlockBytes : caps.storageBlockBytes;
        // 公共 RHI 对 partial CBV 的 offset/size 另有 256-byte 合同, 不能只按较小的 GL 对齐放宽.
        const bool whole = offset == 0 && size == desc.byteSize;
        RequirePreparation(
            !constant || whole ||
                (offset % ConstantBufferOffsetSizeAlignment == 0 && size % ConstantBufferOffsetSizeAlignment == 0),
            "InvalidArgument: partial constant-buffer alignment");
        RequirePreparation(size > 0 && size <= desc.byteSize - offset && alignment > 0 &&
                               offset % std::uint64_t(alignment) == 0 && size <= std::uint64_t(maximum) &&
                               (constant || (offset % desc.structStride == 0 && size % desc.structStride == 0)),
                           "InvalidArgument: buffer binding range/alignment/block limit");
        return {offset, size};
    }

    // 一次创建尝试的二维视图计划, texture 借用由未发布 BindingSet 已取得的强引用保活.
    struct SamplingView
    {
        OpenGLTexture *texture = nullptr;
        GLenum format = 0;
        TextureSubresourceSet range;
        std::array<GLint, 4> swizzle{};
    };

    /// @brief 验证真正的二维单层采样视图, format 重解释沿用 storage 的 typeless/view-class 合同.
    static SamplingView InspectTexture(OpenGLDevice &device, const BindingSetItem &item)
    {
        auto *texture = dynamic_cast<OpenGLTexture *>(item.resourceHandle);
        RequirePreparation(texture && texture->Owner() == &device, "InvalidResource: foreign texture binding");
        const auto &desc = texture->GetDesc();
        RequirePreparation(
            desc.isShaderResource && desc.sampleCount == 1 &&
                (desc.dimension == TextureDimension::Texture2D || desc.dimension == TextureDimension::Texture2DArray ||
                 desc.dimension == TextureDimension::TextureCube ||
                 desc.dimension == TextureDimension::TextureCubeArray) &&
                (item.dimension == TextureDimension::Unknown || item.dimension == TextureDimension::Texture2D),
            "Unsupported: binding requires a two-dimensional sampled view");
        auto range = item.subresources;
        RequirePreparation(range.baseMipLevel < desc.mipLevels && range.baseArraySlice < desc.arraySize,
                           "InvalidArgument: sampling view base range");
        if (range.numMipLevels == TextureSubresourceSet::AllMipLevels)
        {
            range.numMipLevels = desc.mipLevels - range.baseMipLevel;
        }
        if (range.numArraySlices == TextureSubresourceSet::AllArraySlices)
        {
            range.numArraySlices = desc.arraySize - range.baseArraySlice;
        }
        RequirePreparation(range.numMipLevels > 0 && range.numMipLevels <= desc.mipLevels - range.baseMipLevel &&
                               range.numArraySlices == 1,
                           "InvalidArgument: sampling view mip/layer range");
        const auto format = item.format == Format::UNKNOWN ? desc.format : item.format;
        RequirePreparation(format > Format::UNKNOWN && format < Format::COUNT, "InvalidArgument: sampling view format");
        const auto &info = GetFormatInfo(format);
        const auto &storage = device.GetFormatCaps(desc.format, desc.dimension);
        const auto &view = device.GetFormatCaps(format, TextureDimension::Texture2D);
        RequirePreparation(info.kind != FormatKind::Integer && info.kind != FormatKind::DepthStencil &&
                               (view.support & FormatSupport::ShaderSample) != 0 && view.internalFormat != 0 &&
                               (format == desc.format ||
                                (desc.isTypeless && storage.viewClass != 0 && storage.viewClass == view.viewClass)),
                           "Unsupported: sampling view interpretation or format capability");
        const auto mapping = ResolveComponentMapping(item.overrideComponentMapping, desc.defaultComponentMapping);
        constexpr std::array<GLint, 6> Swizzles{GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA, GL_ZERO, GL_ONE};
        const std::array<ComponentSwizzle, 4> components{mapping.r, mapping.g, mapping.b, mapping.a};
        SamplingView result{texture, view.internalFormat, range, {}};
        for (std::size_t index = 0; index < components.size(); ++index)
        {
            RequirePreparation(std::size_t(components[index]) < Swizzles.size(), "InvalidArgument: component mapping");
            result.swizzle[index] = Swizzles[std::size_t(components[index])];
        }
        return result;
    }

    BindingSetHandle OpenGLDevice::CreateBindingSet(const BindingSetDesc &desc, BindingLayout *layout)
    {
        if (!RequireContext("CreateBindingSet"))
        {
            return nullptr;
        }
        try
        {
            auto *nativeLayout = dynamic_cast<OpenGLBindingLayout *>(layout);
            RequirePreparation(nativeLayout && nativeLayout->Owner() == this,
                               "InvalidResource: foreign binding layout");
            const auto &items = nativeLayout->GetDesc()->bindings;
            RequirePreparation(desc.bindings.size() == items.size(), "InvalidArgument: binding set item count");
            auto set = std::make_unique<OpenGLBindingSet>(this, desc, nativeLayout);
            std::vector<SamplingView> views(desc.bindings.size());
            std::set<std::uint32_t> occupied;
            // 完整输入预检不发原生工作, 所有成功记录仍由未发布的 set 拥有.
            // 数量一致还不足以证明匹配, type/slot 与 occupied 共同拒绝缺项被重复项替代的情况.
            for (std::size_t index = 0; index < desc.bindings.size(); ++index)
            {
                const auto &item = desc.bindings[index];
                const auto found = std::ranges::find_if(
                    items, [&](const auto &entry) { return entry.slot == item.slot && entry.type == item.type; });
                RequirePreparation(found != items.end() && item.arrayElement == 0 && item.resourceHandle &&
                                       occupied.insert(CanonicalBinding(*nativeLayout->GetDesc(), *found)).second,
                                   "InvalidArgument: missing, duplicated or mismatched binding item");
                auto &record = set->m_Bindings[index];
                record.slot = item.slot;
                record.type = item.type;
                record.resource = item.resourceHandle;
                if (item.type == ResourceType::Texture_SRV)
                {
                    views[index] = InspectTexture(*this, item);
                    // 保存 storage 空间中的实际范围, view 的 mip/layer 重编号不能用于用途或 feedback 判断.
                    record.subresources = views[index].range;
                }
                else if (item.type == ResourceType::Sampler)
                {
                    auto *sampler = dynamic_cast<OpenGLSampler *>(item.resourceHandle);
                    RequirePreparation(sampler && sampler->Owner() == this &&
                                           sampler->GetDesc().reductionType == SamplerReductionType::Standard,
                                       "InvalidResource or Unsupported: sampler binding");
                    record.name = sampler->Name();
                }
                else
                {
                    auto *buffer = dynamic_cast<OpenGLBuffer *>(item.resourceHandle);
                    RequirePreparation(buffer && buffer->Owner() == this, "InvalidResource: foreign buffer binding");
                    record.name = buffer->Name();
                    record.range = ResolveBuffer(item, *buffer, m_Caps.shader);
                    record.recordStride = buffer->GetDesc().structStride;
                }
            }
            if (!BeginNative("CreateBindingSet"))
            {
                return nullptr;
            }
            for (std::size_t index = 0; index < views.size(); ++index)
            {
                const auto &view = views[index];
                if (!view.texture)
                {
                    continue;
                }
                auto &name = set->m_Views[index]->name;
                glGenTextures(1, &name);
                RequirePreparation(name != 0, "NativeFailure: zero sampling view name");
                // 每个 view 有独立 mip/layer 与 swizzle 状态, 不改 storage 的参数或当前 texture unit.
                glTextureView(name, GL_TEXTURE_2D, view.texture->Name(), view.format, view.range.baseMipLevel,
                              view.range.numMipLevels, view.range.baseArraySlice, 1);
                glTextureParameteriv(name, GL_TEXTURE_SWIZZLE_RGBA, view.swizzle.data());
                if (!CheckNative("CreateBindingSet.View", name))
                {
                    return nullptr;
                }
                set->m_Bindings[index].name = name;
            }
            // 此前任一步失败均由未发布 owner 回滚, 到这里才转为发布后的排队退休语义.
            set->m_Published = true;
            return BindingSetHandle::Create(set.release());
        }
        catch (const std::exception &error)
        {
            Report("CreateBindingSet", error.what());
            return nullptr;
        }
    }
} // namespace Physara::RHI::OpenGL
