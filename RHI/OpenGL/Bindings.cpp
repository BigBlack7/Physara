#include "Bindings.hpp"

/// @file
/// @brief 由逻辑布局与调用方条目建立不可变集合. 保存原存储的区间和引用, 需要重编号时创建独立视图.
/// 创建过程保留 context 的活动绑定. Pipeline 另外解释 slot 到原生单元的映射.

#include <algorithm>
#include <cstdio>
#include <limits>
#include <set>
#include <stdexcept>

#include "../Utils.hpp"

namespace Physara::RHI::OpenGL
{
    // 先按资源类别选择 canonical offset, 再与逻辑 slot 合成 binding; 此值尚不是 GL 物理单元.
    std::uint32_t CanonicalBinding(const BindingLayoutDesc &desc, const BindingLayoutItem &item)
    {
        std::uint32_t offset = 0;
        switch (item.type)
        {
        case ResourceType::ConstantBuffer:
        case ResourceType::VolatileConstantBuffer:
            offset = desc.bindingOffsets.constantBuffer;
            break;
        case ResourceType::StructuredBuffer_SRV:
        case ResourceType::Texture_SRV:
            offset = desc.bindingOffsets.shaderResource;
            break;
        case ResourceType::StructuredBuffer_UAV:
        case ResourceType::Texture_UAV:
            offset = desc.bindingOffsets.unorderedAccess;
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

    // 布局只准备不可变 CPU 描述; 这里拒绝类别合成后的冲突, 实际 set 解释和单元分配留给 Pipeline.
    BindingLayoutHandle OpenGLDevice::CreateBindingLayout(const BindingLayoutDesc &desc)
    {
        if (!RequireContext("CreateBindingLayout"))
        {
            return nullptr;
        }
        try
        {
            const auto allowed = ShaderType::Vertex | ShaderType::Pixel |
                (m_Caps.shader.compute ? ShaderType::Compute : ShaderType::None);
            RequirePreparation(desc.visibility != ShaderType::None && (desc.visibility & ~allowed) == ShaderType::None,
                               "Unsupported: binding visibility");
            RequirePreparation(desc.registerSpace < 4 && (desc.registerSpaceIsDescriptorSet || desc.registerSpace == 0),
                               "InvalidArgument: layout register-space strategy");
            std::set<std::uint32_t> bindings;
            std::optional<std::uint32_t> pushSlot;
            std::uint32_t volatileCount = 0;
            for (const auto &item : desc.bindings)
            {
                if (item.type == ResourceType::PushConstants)
                {
                    RequirePreparation(!pushSlot && item.size != 0 && item.size <= MaxPushConstantSize &&
                                           item.size % 4 == 0 && item.slot < 128,
                                       "InvalidArgument: push-constant layout count, size or slot");
                    pushSlot = item.slot;
                    continue;
                }
                RequirePreparation(item.size == 1 && item.slot < 128, "Unsupported: descriptor count or slot");
                RequirePreparation(item.type != ResourceType::VolatileConstantBuffer ||
                                       ++volatileCount <= MaxVolatileConstantBuffersPerLayout,
                                   "InvalidArgument: too many volatile constants in one layout");
                RequirePreparation((item.type != ResourceType::StructuredBuffer_UAV &&
                                    item.type != ResourceType::Texture_UAV) || desc.visibility == ShaderType::Compute,
                                   "Unsupported: storage binding requires compute-only layout visibility");
                RequirePreparation(bindings.insert(CanonicalBinding(desc, item)).second,
                                   "InvalidArgument: colliding canonical layout bindings");
            }
            RequirePreparation(!pushSlot || std::ranges::none_of(desc.bindings, [&](const auto &item)
            {
                return item.slot == *pushSlot && (item.type == ResourceType::ConstantBuffer ||
                                                 item.type == ResourceType::VolatileConstantBuffer);
            }), "InvalidArgument: push constants reuse a constant-buffer slot");
            return BindingLayoutHandle::Create(new OpenGLBindingLayout(this, desc));
        }
        catch (const std::exception &error)
        {
            Report("CreateBindingLayout", error.what());
            return nullptr;
        }
    }

    // 按输入顺序保活资源, 准备解析记录和删除节点. 原生 view 由工厂随后创建.
    // 空向量用可抛的 count 构造. Debug 代理分配失败时已构造成员随展开释放, 由工厂报告.
    OpenGLBindingSet::OpenGLBindingSet(OpenGLDevice *device, const BindingSetDesc &desc, OpenGLBindingLayout *layout)
        : m_Device(device), m_Desc(desc), m_Layout(layout), m_Resources(0), m_Bindings(0), m_Views(0)
    {
        m_Resources.reserve(desc.bindings.size());
        m_Bindings.reserve(desc.bindings.size());
        m_Views.reserve(desc.bindings.size());
        for (const auto &item : desc.bindings)
        {
            m_Resources.emplace_back(item.resourceHandle);
            m_Bindings.emplace_back();
            m_Views.emplace_back();
        }
    }

    // 发布与否决定立即回滚或排队退休; 两条路径都先交出自有 view, 再撤销实际 storage 的强引用.
    OpenGLBindingSet::~OpenGLBindingSet()
    {
        for (auto &view : m_Views)
        {
            m_Device->ReleaseAllocation(std::move(view), m_Published);
        }
        // view 先入队, storage 依赖随后释放, FIFO 保持 view 在 storage 之前退休.
        m_Resources.clear();
    }

    /// @brief 将显式、零长度或全一哨兵范围解析为精确范围, 不截断调用方给出的越界输入.
    /// @note 此处只证明资源用途与设备 range 合法, 不证明它覆盖任意 Shader 所需的块 extent.
    static BufferRange ResolveBuffer(const BindingSetItem &item, OpenGLBuffer &buffer, const ShaderCaps &caps)
    {
        const auto &desc = buffer.GetDesc();
        const bool isVolatile = item.type == ResourceType::VolatileConstantBuffer;
        const bool constant = isVolatile || item.type == ResourceType::ConstantBuffer;
        RequirePreparation(item.type != ResourceType::StructuredBuffer_UAV || desc.canHaveUAVs,
                           "InvalidArgument: buffer missing structured UAV role");
        RequirePreparation(desc.isVolatile == isVolatile && (constant ? desc.isConstantBuffer : desc.structStride > 0),
                           "InvalidArgument: buffer binding usage");
        const auto offset = item.range.byteOffset;
        RequirePreparation(offset < desc.byteSize, "InvalidArgument: buffer binding offset");
        // 零长度与全一哨兵均表示剩余范围; 显式长度不经公共 Resolve 的 clamp, 留给下面拒绝越界.
        const auto size = (item.range.byteSize == 0 || item.range.byteSize == ~0ull)
                              ? desc.byteSize - offset
                              : item.range.byteSize;
        const auto alignment = constant ? caps.uniformAlignment : caps.storageAlignment;
        const auto maximum = constant ? caps.uniformBlockBytes : caps.storageBlockBytes;
        // 公共 RHI 对 partial CBV 的 offset/size 另有 256-byte 合同, 不能只按较小的 GL 对齐放宽.
        const bool whole = offset == 0 && size == desc.byteSize;
        RequirePreparation(!isVolatile || whole, "InvalidArgument: volatile binding must cover the logical buffer");
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

    // 一次创建尝试的视图计划, texture 借用由未发布 BindingSet 已取得的强引用保活.
    struct SamplingView
    {
        OpenGLTexture *texture = nullptr;
        TextureDimension dimension = TextureDimension::Unknown;
        GLenum format = 0;
        TextureSubresourceSet range;
        std::array<GLint, 4> swizzle{};
        bool usesStorage = false;
    };

    /// @brief 仅在拒绝时格式化可用槽身份及实际输入. 非 descriptor 布局的 set 由后续 Pipeline 分配.
    [[noreturn]] static void RejectSamplingView(const char *reason, const BindingLayoutDesc &layout,
        std::uint32_t slot, TextureDimension dimension, const TextureSubresourceSet &range, const TextureDesc &desc)
    {
        char set[32]{};
        if (layout.registerSpaceIsDescriptorSet)
        {
            std::snprintf(set, sizeof(set), "set=%u ", layout.registerSpace);
        }
        char text[384]{};
        std::snprintf(text, sizeof(text),
            "%s (%sslot=%u requested=%s storage=%s, baseMip=%u mipCount=%u, baseLayer=%u layerCount=%u, "
            "storageMips=%u storageLayers=%u)%s",
            reason, set, slot, Utils::TextureDimensionToString(dimension),
            Utils::TextureDimensionToString(desc.dimension), range.baseMipLevel, range.numMipLevels,
            range.baseArraySlice, range.numArraySlices, desc.mipLevels, desc.arraySize,
            dimension == TextureDimension::TextureCube ? ", cube requires baseLayer=0 layerCount=6" : "");
        throw std::runtime_error(text);
    }

    /// @brief 选择二维单层、连续层数组或完整 cube, 保留实际 mip、layer 范围和视图维度.
    static SamplingView InspectTexture(OpenGLDevice &device, const BindingSetItem &item, const BindingLayoutDesc &layout)
    {
        auto *texture = dynamic_cast<OpenGLTexture *>(item.resourceHandle);
        RequirePreparation(texture && texture->Owner() == &device, "InvalidResource: foreign texture binding");
        const auto &desc = texture->GetDesc();
        const auto dimension = item.dimension == TextureDimension::Unknown ? TextureDimension::Texture2D :
                                                                           item.dimension;
        const bool cube = dimension == TextureDimension::TextureCube;
        const bool array = dimension == TextureDimension::Texture2DArray;
        const bool layerStorage = desc.dimension == TextureDimension::Texture2D ||
                                  desc.dimension == TextureDimension::Texture2DArray ||
                                  desc.dimension == TextureDimension::TextureCube ||
                                  desc.dimension == TextureDimension::TextureCubeArray;
        auto range = item.subresources;
        const bool compatible = (dimension == TextureDimension::Texture2D && layerStorage) ||
            ((cube || array) && desc.dimension == dimension);
        if (!desc.isShaderResource || desc.sampleCount != 1 || !compatible)
        {
            RejectSamplingView("Unsupported: incompatible sampled view dimension", layout, item.slot, dimension,
                               range, desc);
        }
        if (range.baseMipLevel >= desc.mipLevels || range.baseArraySlice >= desc.arraySize)
        {
            RejectSamplingView("InvalidArgument: sampling view base range", layout, item.slot, dimension, range, desc);
        }
        if (range.numMipLevels == TextureSubresourceSet::AllMipLevels)
        {
            range.numMipLevels = desc.mipLevels - range.baseMipLevel;
        }
        if (range.numArraySlices == TextureSubresourceSet::AllArraySlices)
        {
            range.numArraySlices = desc.arraySize - range.baseArraySlice;
        }
        // 单层数组的层坐标仍相对视图起点. 保留调用者的数组维度, 供活动 Shader 核对.
        const bool layersValid = array ? range.numArraySlices > 0 &&
            range.numArraySlices <= desc.arraySize - range.baseArraySlice :
            (cube ? range.baseArraySlice == 0 && range.numArraySlices == 6 : range.numArraySlices == 1);
        if (range.numMipLevels == 0 || range.numMipLevels > desc.mipLevels - range.baseMipLevel || !layersValid)
        {
            RejectSamplingView("InvalidArgument: sampling view mip/layer range", layout, item.slot, dimension,
                               range, desc);
        }
        const auto format = item.format == Format::UNKNOWN ? desc.format : item.format;
        RequirePreparation(format > Format::UNKNOWN && format < Format::COUNT, "InvalidArgument: sampling view format");
        if (array && format != desc.format)
        {
            RejectSamplingView("Unsupported: sampled 2D array view must match the storage format", layout, item.slot,
                               dimension, range, desc);
        }
        const auto &info = GetFormatInfo(format);
        const auto &storage = device.GetFormatCaps(desc.format, desc.dimension);
        const auto &view = device.GetFormatCaps(format, dimension);
        // 按视图的真实目标核对 ShaderSample. cube 基线只接受颜色, 二维 packed 仍按深度分量解释.
        RequirePreparation((view.support & FormatSupport::ShaderSample) != 0 &&
                               (info.kind != FormatKind::DepthStencil || format == desc.format) &&
                               (format == desc.format ||
                                (desc.isTypeless && storage.viewClass != 0 && storage.viewClass == view.viewClass)),
                           "Unsupported: sampling view interpretation or format capability");
        const auto mapping = ResolveComponentMapping(item.overrideComponentMapping, desc.defaultComponentMapping);
        constexpr std::array<GLint, 6> Swizzles{GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA, GL_ZERO, GL_ONE};
        const std::array<ComponentSwizzle, 4> components{mapping.r, mapping.g, mapping.b, mapping.a};
        SamplingView result{texture, dimension, view.internalFormat, range, {}};
        for (std::size_t index = 0; index < components.size(); ++index)
        {
            RequirePreparation(std::size_t(components[index]) < Swizzles.size(), "InvalidArgument: component mapping");
            result.swizzle[index] = Swizzles[std::size_t(components[index])];
        }
        const auto defaults = ResolveComponentMapping(std::nullopt, desc.defaultComponentMapping);
        // 完全相同的视图直接共享现有 native 名字. 独立 view 仅用于实际解释或范围变化.
        result.usesStorage = dimension == desc.dimension && format == desc.format && range.baseMipLevel == 0 &&
            range.numMipLevels == desc.mipLevels && range.baseArraySlice == 0 &&
            range.numArraySlices == desc.arraySize && mapping == defaults;
        return result;
    }

    // image 只选一个 mip. 二维视图取一个实际层, 数组视图可覆盖一段连续层.
    // 选区需要新视图时, 原生 image 绑定从该视图的 level/layer 0 寻址.
    static SamplingView InspectImage(OpenGLDevice &device, const BindingSetItem &item)
    {
        auto *texture = dynamic_cast<OpenGLTexture *>(item.resourceHandle);
        RequirePreparation(texture && texture->Owner() == &device, "InvalidResource: foreign image binding");
        const auto &desc = texture->GetDesc();
        const auto dimension = item.dimension == TextureDimension::Unknown ? desc.dimension : item.dimension;
        const bool array = dimension == TextureDimension::Texture2DArray;
        auto range = item.subresources;
        RequirePreparation(desc.isUAV && (desc.dimension == TextureDimension::Texture2D ||
                           desc.dimension == TextureDimension::Texture2DArray) &&
                           (array ? desc.dimension == dimension : dimension == TextureDimension::Texture2D) &&
                           (item.format == Format::UNKNOWN || item.format == desc.format) &&
                           range.baseMipLevel < desc.mipLevels && range.baseArraySlice < desc.arraySize,
                           "InvalidArgument: storage image role, format, dimension or base range");
        if (range.numArraySlices == TextureSubresourceSet::AllArraySlices)
        {
            range.numArraySlices = desc.arraySize - range.baseArraySlice;
        }
        if (range.numMipLevels == TextureSubresourceSet::AllMipLevels)
        {
            range.numMipLevels = desc.mipLevels - range.baseMipLevel;
        }
        RequirePreparation(range.numMipLevels == 1 && range.numArraySlices > 0 &&
                               range.numArraySlices <= desc.arraySize - range.baseArraySlice &&
                               (array || range.numArraySlices == 1),
                           "InvalidArgument: storage image needs one mip and an exact layer span");
        const auto &caps = device.GetFormatCaps(desc.format, dimension);
        RequirePreparation((caps.support & (FormatSupport::ShaderUavLoad | FormatSupport::ShaderUavStore)) != 0,
                           "Unsupported: storage image view capability");
        return {texture, dimension, caps.internalFormat, range, {},
                dimension == desc.dimension && range.baseMipLevel == 0 && range.baseArraySlice == 0 &&
                range.numArraySlices == desc.arraySize};
    }

    // 先由未发布的集合持有布局和资源, 再为需要重编号的条目物化原生视图. 中途返回由本地 owner 回收前缀.
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
            std::set<const BindingLayoutItem *> occupied;
            // 完整输入预检不发原生工作, 所有成功记录仍由未发布的 set 拥有.
            // 数量一致还不足以证明匹配, type/slot 与 occupied 共同拒绝缺项被重复项替代的情况.
            for (std::size_t index = 0; index < desc.bindings.size(); ++index)
            {
                const auto &item = desc.bindings[index];
                const auto found = std::ranges::find_if(
                    items, [&](const auto &entry) { return entry.slot == item.slot && entry.type == item.type; });
                RequirePreparation(found != items.end() && item.arrayElement == 0 && occupied.insert(&*found).second &&
                                       (item.type == ResourceType::PushConstants ? !item.resourceHandle
                                                                                 : item.resourceHandle != nullptr),
                                   "InvalidArgument: missing, duplicated or mismatched binding item");
                auto &record = set->m_Bindings[index];
                record.slot = item.slot;
                record.type = item.type;
                record.resource = item.resourceHandle;
                if (item.type == ResourceType::PushConstants)
                {
                    // 集合只保存 push 占位和大小, 当前 Draw 或 Dispatch 的数据来自最近一次被接受的状态设置后的写入.
                    RequirePreparation(item.range.byteOffset == 0 && item.range.byteSize == found->size,
                                       "InvalidArgument: push constant set size");
                    record.range = item.range;
                }
                else if (item.type == ResourceType::Texture_SRV || item.type == ResourceType::Texture_UAV)
                {
                    if (item.type == ResourceType::Texture_UAV)
                    {
                        views[index] = InspectImage(*this, item);
                        record.imageFormat = views[index].texture->GetDesc().format;
                        record.imageInternalFormat = views[index].format;
                    }
                    else
                    {
                        views[index] = InspectTexture(*this, item, *nativeLayout->GetDesc());
                    }
                    // 保存 storage 空间中的实际范围, view 的 mip/layer 重编号不能用于用途或 feedback 判断.
                    record.dimension = views[index].dimension;
                    record.subresources = views[index].range;
                    if (views[index].usesStorage)
                    {
                        record.name = views[index].texture->Name();
                    }
                    else
                    {
                        // 需要独立 view 时先预留删除节点, 后面的原生创建不再分配它.
                        set->m_Views[index] = std::make_unique<NativeDeletion>(NativeKind::Texture);
                    }
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
                if (!set->m_Views[index])
                {
                    continue;
                }
                auto &name = set->m_Views[index]->name;
                glGenTextures(1, &name);
                RequirePreparation(name != 0, "NativeFailure: zero sampling view name");
                // 每个 view 有独立 mip/layer 与 swizzle 状态, 不改 storage 的参数或当前 texture unit.
                glTextureView(name, TextureTarget(view.dimension), view.texture->Name(), view.format,
                              view.range.baseMipLevel, view.range.numMipLevels, view.range.baseArraySlice,
                              view.range.numArraySlices);
                if (set->m_Bindings[index].type == ResourceType::Texture_SRV)
                {
                    glTextureParameteriv(name, GL_TEXTURE_SWIZZLE_RGBA, view.swizzle.data());
                }
                else
                {
                    SetStorageImageFilters(name, set->m_Bindings[index].imageFormat);
                }
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
