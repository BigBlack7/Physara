#include "GraphCompilerInternal.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <string_view>
#include <type_traits>

#include "GraphRanges.hpp"
#include "RenderGraph.hpp"

/// @file
/// @brief 从 CPU 能力表核对存储描述与进出状态, 再为每个声明 Pass 建立用途和视图记录.
/// 用途表沿用声明索引, 最终按存活顺序移入计划. 裁剪前检查普通阶段的共享状态和图尾导出合同.

namespace Physara::Renderer::Detail
{
    using RHI::FormatSupport;
    using RHI::ResourceStates;
    using RHI::TextureDimension;

    // 声明阶段已核对正数, 维度枚举及基本范围. 此处核对 mip 与样本的关系, 以及空间尺寸和 cube 面数.
    static bool ValidTextureShape(const RHI::TextureDesc &desc) noexcept
    {
        const bool multisample = desc.dimension == TextureDimension::Texture2DMS ||
                                 desc.dimension == TextureDimension::Texture2DMSArray;
        const bool supportedSampleShape = desc.sampleCount == 2 || desc.sampleCount == 4 || desc.sampleCount == 8;
        const auto maxMips = static_cast<std::uint32_t>(std::bit_width((std::max)({desc.width, desc.height, desc.depth})));
        if (desc.sampleQuality != 0 || desc.mipLevels > maxMips ||
            (multisample ? desc.mipLevels != 1 || !supportedSampleShape : desc.sampleCount != 1))
        {
            return false;
        }
        switch (desc.dimension)
        {
        case TextureDimension::Texture1D:
        case TextureDimension::Texture1DArray:
            return desc.height == 1 && desc.depth == 1;
        case TextureDimension::TextureCube:
            return desc.width == desc.height && desc.depth == 1 && desc.arraySize == 6;
        case TextureDimension::TextureCubeArray:
            return desc.width == desc.height && desc.depth == 1 && desc.arraySize % 6 == 0;
        case TextureDimension::Texture3D:
            return true;
        default:
            return desc.depth == 1;
        }
    }

    // 按资源类型用完整状态值匹配能力表. 导入的 final 还须与 keepInitialState 的 Close 恢复一致.
    template <typename Declaration>
    static GraphFailure ValidateBoundary(const Declaration &resource, std::span<const ResourceStates> states,
                                          CompileResourceLocation location)
    {
        const auto &desc = resource.desc;
        const auto supports = [&](ResourceStates state) { return std::ranges::find(states, state) != states.end(); };
        if (!HasKnownResourceStateBits(desc.initialState) ||
            (desc.initialState == ResourceStates::Unknown && desc.keepInitialState))
        {
            return CompileError(GraphErrorCode::InvalidDescription, "Invalid initial resource state", location);
        }
        if (desc.initialState != ResourceStates::Unknown && !supports(desc.initialState))
        {
            return CompileError(GraphErrorCode::UnsupportedCapability, "Unsupported initial resource state", location);
        }
        if (resource.importState)
        {
            if (!supports(resource.importState->entry))
            {
                return CompileError(GraphErrorCode::UnsupportedCapability, "Unsupported import entry state", location);
            }
            if (!supports(resource.importState->final))
            {
                return CompileError(GraphErrorCode::UnsupportedCapability, "Unsupported import final state", location);
            }
            if (desc.keepInitialState && resource.importState->final != desc.initialState)
            {
                return CompileError(GraphErrorCode::InvalidDescription,
                                    "Import final state conflicts with initial-state restoration", location);
            }
        }
        return std::nullopt;
    }

    // 按格式, 存储维度和样本数匹配能力行. 校验成功后借用该行供本次编译检查各 Pass 的用途.
    static GraphResult<const GraphTextureSupport *> InspectTexture(const TextureDeclaration &resource,
                                                                    const GraphCompileOptions &options,
                                                                    CompileResourceLocation location)
    {
        const auto &desc = resource.desc;
        const auto &format = RHI::GetFormatInfo(desc.format);
        const RHI::ComponentSwizzle components[] = {desc.defaultComponentMapping.r, desc.defaultComponentMapping.g,
                                                    desc.defaultComponentMapping.b, desc.defaultComponentMapping.a};
        if (!ValidTextureShape(desc) || (format.blockSize > 1 && (desc.isRenderTarget || desc.sampleCount > 1)) ||
            std::ranges::any_of(components, [](auto component)
        {
            return component > RHI::ComponentSwizzle::One;
        }))
        {
            return CompileError(GraphErrorCode::InvalidDescription,
                                "Invalid texture shape, mip, sample, format role or swizzle", location);
        }
        const auto support = std::ranges::find_if(options.textures, [&](const GraphTextureSupport &entry)
        {
            return entry.format == desc.format && entry.dimension == desc.dimension &&
                   entry.sampleCount == desc.sampleCount;
        });
        const auto target = resource.range.aspect == TextureAspect::Color ? FormatSupport::RenderTarget :
                                                                            FormatSupport::DepthStencil;
        if (support == options.textures.end() || !(support->formatSupport & FormatSupport::Texture) ||
            (desc.isShaderResource && !(support->formatSupport & FormatSupport::ShaderLoad)) ||
            (desc.isRenderTarget && !(support->formatSupport & target)) ||
            desc.width > support->maxWidth || desc.height > support->maxHeight || desc.depth > support->maxDepth ||
            desc.arraySize > support->maxLayers || desc.isUAV || desc.isVirtual || desc.isTiled ||
            desc.isShadingRateSurface || desc.sharedResourceFlags != RHI::SharedResourceFlags::None)
        {
            return CompileError(GraphErrorCode::UnsupportedCapability, "Unsupported texture format, sample or storage",
                                location);
        }
        if (auto error = ValidateBoundary(resource, options.textureBoundaryStates, location))
        {
            return *error;
        }
        return &*support;
    }

    // 只规划已有普通 Buffer profile, 检查容量和结构布局后再校验用途所需的视图.
    static GraphFailure InspectBuffer(const BufferDeclaration &resource, const GraphCompileOptions &options,
                                       CompileResourceLocation location)
    {
        const auto &desc = resource.desc;
        if ((desc.structStride != 0 && desc.byteSize % desc.structStride != 0) ||
            (desc.canHaveRawViews && desc.byteSize % 4 != 0))
        {
            return CompileError(GraphErrorCode::InvalidDescription, "Invalid buffer storage layout", location);
        }
        if (desc.byteSize > options.buffers.maxBytes || desc.cpuAccess != RHI::CpuAccessMode::None ||
            desc.isVolatile || desc.isVirtual || desc.canHaveTypedViews || desc.format != RHI::Format::UNKNOWN ||
            desc.sharedResourceFlags != RHI::SharedResourceFlags::None ||
            desc.isAccelStructBuildInput || desc.isAccelStructStorage || desc.isShaderBindingTable)
        {
            return CompileError(GraphErrorCode::UnsupportedCapability, "Unsupported buffer size or storage profile",
                                location);
        }
        return ValidateBoundary(resource, options.bufferBoundaryStates, location);
    }

    // 区间合法性已由 T01 保证. 在此检查需要设备能力的绑定对齐, 容量和结构步长.
    static bool SupportsBufferUse(const RHI::BufferDesc &desc, const RHI::BufferRange &range,
                                  GraphUsage usage, const GraphBufferSupport &support) noexcept
    {
        const auto fits = [&](std::uint64_t alignment, std::uint64_t bytes)
        {
            return alignment != 0 && range.byteOffset % alignment == 0 && range.byteSize <= bytes;
        };
        if (usage == GraphUsage::ShaderRead)
        {
            return desc.structStride != 0 && range.byteOffset % desc.structStride == 0 &&
                   range.byteSize % desc.structStride == 0 &&
                   fits(support.structuredOffsetAlignment, support.maxStructuredBytes);
        }
        if (usage == GraphUsage::ConstantBuffer)
        {
            constexpr auto alignment = RHI::ConstantBufferOffsetSizeAlignment;
            const bool whole = range.byteOffset == 0 && range.byteSize == desc.byteSize;
            return (whole || (range.byteOffset % alignment == 0 && range.byteSize % alignment == 0)) &&
                   fits(support.constantOffsetAlignment, support.maxConstantBytes);
        }
        return true;
    }

    /// @brief 同一图用途对应的诊断名称和普通阶段的公共 RHI 状态.
    struct UsageInfo
    {
        std::string_view name;
        ResourceStates state;
    };

    // 深度/模板采样同时需要 DepthRead. 状态与诊断名称沿用同一用途映射.
    static UsageInfo DescribeUsage(GraphUsage usage, TextureAspect aspect = TextureAspect::Color) noexcept
    {
        switch (usage)
        {
        case GraphUsage::Sampled:
            return {"Sampled", aspect == TextureAspect::Color ? ResourceStates::ShaderResource :
                                                               ResourceStates::ShaderResource | ResourceStates::DepthRead};
        case GraphUsage::ShaderRead: return {"ShaderRead", ResourceStates::ShaderResource};
        case GraphUsage::VertexBuffer: return {"VertexBuffer", ResourceStates::VertexBuffer};
        case GraphUsage::IndexBuffer: return {"IndexBuffer", ResourceStates::IndexBuffer};
        case GraphUsage::ConstantBuffer: return {"ConstantBuffer", ResourceStates::ConstantBuffer};
        case GraphUsage::CopySource: return {"CopySource", ResourceStates::CopySource};
        case GraphUsage::CopyDestination: return {"CopyDestination", ResourceStates::CopyDest};
        case GraphUsage::ColorAttachment: return {"ColorAttachment", ResourceStates::RenderTarget};
        case GraphUsage::DepthStencilAttachment: return {"DepthStencilAttachment", ResourceStates::DepthWrite};
        case GraphUsage::DepthStencilRead: return {"DepthStencilRead", ResourceStates::DepthRead};
        }
        assert(false);
        return {{}, ResourceStates::Unknown};
    }

    /// @brief 同一 Pass 内各附件在所选 mip 上必须匹配的宽高和样本数.
    struct AttachmentShape
    {
        std::uint32_t width;
        std::uint32_t height;
        std::uint32_t samples;

        bool operator==(const AttachmentShape &) const = default;
    };

    // 按实际视图比较尺寸, 颜色槽沿声明顺序追加. 深度与模板合用同一存储位置和访问模式.
    static GraphFailure AddAttachment(PlannedGraphPass &plan, std::optional<AttachmentShape> &shape,
                                      const TextureAccess &access, const RHI::TextureDesc &desc,
                                      const PassDeclaration &pass, std::uint32_t maxColorAttachments)
    {
        const auto &range = access.range;
        const auto incompatible = [&](std::string_view reason)
        {
            return CompileError(GraphErrorCode::IncompatibleAttachments, reason,
                                LocateResource(access.resource, desc.debugName), &pass);
        };
        if (range.mipCount != 1 || range.layerCount != 1 || desc.dimension == TextureDimension::Texture3D)
        {
            return incompatible("Attachment requires a single mip and layer");
        }
        const AttachmentShape selected{(std::max)(1u, desc.width >> range.firstMip),
                                       (std::max)(1u, desc.height >> range.firstMip), desc.sampleCount};
        if (shape && *shape != selected)
        {
            return incompatible("Attachment extent or sample count mismatch");
        }
        shape = selected;
        const PlannedGraphAttachment attachment{{access.resource.identity, 0}, range, desc.format,
                                                  access.usage == GraphUsage::DepthStencilRead};
        if (access.usage == GraphUsage::ColorAttachment)
        {
            if (plan.colorAttachments.size() >= (std::min)(maxColorAttachments, RHI::MaxRenderTargets))
            {
                return incompatible("Color attachment count exceeds capability");
            }
            plan.colorAttachments.push_back(attachment);
        }
        else if (auto &depthStencil = plan.depthStencilAttachment; !depthStencil)
        {
            depthStencil = attachment;
        }
        else
        {
            if (depthStencil->resource != attachment.resource || depthStencil->readOnly != attachment.readOnly ||
                depthStencil->range.firstMip != range.firstMip || depthStencil->range.firstLayer != range.firstLayer)
            {
                return incompatible("Depth and stencil require one compatible attachment view");
            }
            depthStencil->range.aspect = static_cast<TextureAspect>(static_cast<unsigned>(depthStencil->range.aspect) |
                                                                    static_cast<unsigned>(range.aspect));
        }
        return std::nullopt;
    }

    // 采样和着色器导出交付同格式视图, 都需要实际的视图能力和范围.
    static bool SupportsSampledView(const TextureRange &range, const GraphTextureSupport &support) noexcept
    {
        return (support.formatSupport & FormatSupport::ShaderSample) != 0 &&
               range.layerCount <= support.maxSampledLayers;
    }

    // 普通复制和读回使用 TextureSlice, packed 格式的两个 aspect 一起传输.
    static bool SupportsCopyAspects(const RHI::TextureDesc &desc, TextureAspect aspect) noexcept
    {
        const auto &format = RHI::GetFormatInfo(desc.format);
        return !format.hasDepth || !format.hasStencil || aspect == TextureAspect::DepthStencil;
    }

    // 先检查清除命令所需能力, 再检查清除之后仍需使用的普通用途.
    // 附件之间的尺寸及样本相容性由 AddAttachment 核对.
    static bool SupportsTextureUse(const TextureUse &use, const GraphTextureSupport &support) noexcept
    {
        if (use.write && use.write->ops.load == LoadOp::Clear && (!support.clear || !support.copyDestination))
        {
            return false;
        }
        switch (use.access.usage)
        {
        case GraphUsage::Sampled: return SupportsSampledView(use.access.range, support);
        case GraphUsage::CopySource: return support.copySource;
        case GraphUsage::CopyDestination: return support.copyDestination;
        case GraphUsage::ColorAttachment: return bool(support.formatSupport & FormatSupport::RenderTarget);
        case GraphUsage::DepthStencilRead:
        case GraphUsage::DepthStencilAttachment: return bool(support.formatSupport & FormatSupport::DepthStencil);
        default: return false;
        }
    }

    // 普通阶段一次准备全部状态. 首发 RHI 支持图形只读位的组合及 CopySource|CopyDest.
    static bool SupportsCombinedState(ResourceStates state) noexcept
    {
        const auto reads = ResourceStates::ConstantBuffer | ResourceStates::VertexBuffer | ResourceStates::IndexBuffer |
                           ResourceStates::ShaderResource | ResourceStates::DepthRead;
        return (state & ~reads) == ResourceStates::Unknown ||
               state == (ResourceStates::CopySource | ResourceStates::CopyDest);
    }

    // Texture 状态共享 mip/layer, Buffer 状态共享整个对象. 内容依赖仍使用 aspect 和字节区间.
    static bool SharesTrackedSubresource(const TextureRange &a, const TextureRange &b) noexcept
    {
        return IntersectInterval(a.firstMip, a.mipCount, b.firstMip, b.mipCount).second != 0 &&
               IntersectInterval(a.firstLayer, a.layerCount, b.firstLayer, b.layerCount).second != 0;
    }

    // 逐项核对本用途与已登记的同一状态单元. 首发允许任意图形只读位组合, 或复制源和目标状态组合.
    // 纯传输 Clear 只在清除步骤请求状态. 诊断保留双方用途和当前 Pass/资源索引.
    template <typename Use>
    static GraphFailure CheckOrdinaryUse(const Use &use, ResourceStates state, const PlannedGraphPass &plan,
                                         bool readOnlyDepthSampling, CompileResourceLocation location,
                                         const PassDeclaration &pass)
    {
        for (const auto &planned : plan.uses)
        {
            if (!planned.state || *planned.state == state)
            {
                continue;
            }
            const auto *other = std::get_if<Use>(&planned.declaration);
            if (!other || other->access.resource.identity != use.access.resource.identity)
            {
                continue;
            }
            auto code = GraphErrorCode::UnsupportedCapability;
            std::string_view reason = "Incompatible ordinary RHI uses";
            const auto combined = *planned.state | state;
            if constexpr (std::is_same_v<Use, TextureUse>)
            {
                if (!SharesTrackedSubresource(other->access.range, use.access.range))
                {
                    continue;
                }
                const bool depthAndSampling =
                    (use.access.usage == GraphUsage::DepthStencilRead && other->access.usage == GraphUsage::Sampled) ||
                    (other->access.usage == GraphUsage::DepthStencilRead && use.access.usage == GraphUsage::Sampled);
                if (depthAndSampling && !readOnlyDepthSampling)
                {
                    reason = "Unsupported simultaneous read-only depth attachment and sampling";
                }
                else if (SupportsCombinedState(combined))
                {
                    continue;
                }
                else if ((combined & ResourceStates::ShaderResource) != 0 &&
                         (combined & (ResourceStates::RenderTarget | ResourceStates::DepthWrite)) != 0)
                {
                    code = GraphErrorCode::InvalidFeedback;
                    reason = "Sampling and writable attachment feedback";
                }
            }
            else if (SupportsCombinedState(combined))
            {
                continue;
            }

            Core::BoundedText conflict(reason);
            conflict.Append(": ");
            conflict.Append(DescribeUsage(other->access.usage).name);
            conflict.Append(" + ");
            conflict.Append(DescribeUsage(use.access.usage).name);
            return CompileError(code, conflict.View(), location, &pass);
        }
        return std::nullopt;
    }

    // 保持用途索引, 令读回和清除步骤直接引用原声明的版本与范围.
    // 附件清除后仍需要附件状态, 纯传输清除仅在清除步骤请求状态.
    static GraphFailure BuildPassPlan(const RenderGraph &graph, const PassDeclaration &pass,
                                      const GraphCompileOptions &options,
                                      std::span<const GraphTextureSupport *const> textureSupport,
                                      PlannedGraphPass &plan)
    {
        plan.id = pass.id;
        plan.readbackUses = pass.readbackUses;
        plan.uses.reserve(pass.uses.size());
        std::optional<AttachmentShape> shape;
        for (const auto &use : pass.uses)
        {
            if (const auto *texture = std::get_if<TextureUse>(&use))
            {
                const auto &access = texture->access;
                const auto index = access.resource.identity.index;
                const auto &desc = graph.Textures()[index].desc;
                const auto &support = *textureSupport[index];
                const auto location = LocateResource(access.resource, desc.debugName);
                const auto usage = access.usage;
                const bool clear = texture->write && texture->write->ops.load == LoadOp::Clear;
                if ((usage == GraphUsage::CopySource || (usage == GraphUsage::CopyDestination && !clear)) &&
                    !SupportsCopyAspects(desc, access.range.aspect))
                {
                    const auto reason = usage == GraphUsage::CopySource
                        ? "CopySource requires both packed depth/stencil aspects"
                        : "CopyDestination requires both packed depth/stencil aspects";
                    return CompileError(GraphErrorCode::InvalidUsage, reason, location, &pass);
                }
                if (!SupportsTextureUse(*texture, support))
                {
                    return CompileError(GraphErrorCode::UnsupportedCapability, "Unsupported texture use or clear",
                                        location, &pass);
                }
                std::optional state = DescribeUsage(usage, access.range.aspect).state;
                if (clear)
                {
                    plan.clears.push_back({plan.uses.size()});
                    if (usage == GraphUsage::CopyDestination)
                    {
                        state.reset();
                    }
                }
                if (state)
                {
                    if (auto error = CheckOrdinaryUse(*texture, *state, plan, support.readOnlyDepthSampling,
                                                      location, pass))
                    {
                        return error;
                    }
                }
                if (usage == GraphUsage::ColorAttachment || usage == GraphUsage::DepthStencilAttachment ||
                    usage == GraphUsage::DepthStencilRead)
                {
                    if (auto error = AddAttachment(plan, shape, access, desc, pass, options.maxColorAttachments))
                    {
                        return error;
                    }
                }
                plan.uses.push_back({use, state});
            }
            else
            {
                const auto &buffer = std::get<BufferUse>(use);
                const auto &access = buffer.access;
                const auto &desc = graph.Buffers()[access.resource.identity.index].desc;
                const auto location = LocateResource(access.resource, desc.debugName);
                if (!SupportsBufferUse(desc, access.range, access.usage, options.buffers))
                {
                    return CompileError(GraphErrorCode::UnsupportedCapability, "Unsupported buffer binding range",
                                        location, &pass);
                }
                const auto state = DescribeUsage(access.usage).state;
                if (auto error = CheckOrdinaryUse(buffer, state, plan, false, location, pass))
                {
                    return error;
                }
                plan.uses.push_back({use, state});
            }
        }
        return std::nullopt;
    }

    // 图尾阶段按导出角色查询能力. 实际外部绑定, 读回及窗口交接由后续消费方完成.
    static GraphFailure ValidateTextureExport(const TextureExport &output, const TextureDeclaration &resource,
                                               const GraphTextureSupport &support,
                                               ResourceStates presentationFinalState)
    {
        const auto &desc = resource.desc;
        const auto location = LocateResource(output.resource, desc.debugName);
        switch (output.usage)
        {
        case ExportUsage::ShaderRead:
            if (SupportsSampledView(output.range, support))
            {
                return std::nullopt;
            }
            return CompileError(GraphErrorCode::UnsupportedCapability,
                                "Unsupported texture export use: ShaderRead requires a supported sampled view",
                                location);
        case ExportUsage::Presentation:
            if (support.presentation && desc.isRenderTarget && desc.sampleCount == 1 &&
                desc.mipLevels == 1 && desc.arraySize == 1)
            {
                // 声明已保证呈现来自导入资源. 通用状态可用仍须满足宿主的完整交接值.
                assert(resource.importState);
                const auto final = resource.importState->final;
                if (final == presentationFinalState)
                {
                    return std::nullopt;
                }
                Core::BoundedText reason("Incompatible Presentation final state (actual=0x");
                AppendDiagnosticNumber(reason, static_cast<std::uint32_t>(final), 16);
                reason.Append(", required=0x");
                AppendDiagnosticNumber(reason, static_cast<std::uint32_t>(presentationFinalState), 16);
                reason.Append(")");
                return CompileError(GraphErrorCode::UnsupportedCapability, reason.View(), location);
            }
            return CompileError(GraphErrorCode::UnsupportedCapability, "Unsupported texture export use: Presentation",
                                location);
        case ExportUsage::Readback:
            if (!SupportsCopyAspects(desc, output.range.aspect))
            {
                return CompileError(GraphErrorCode::InvalidUsage, "Readback requires both packed depth/stencil aspects",
                                    location);
            }
            if (support.copySource)
            {
                return std::nullopt;
            }
            return CompileError(GraphErrorCode::UnsupportedCapability, "Unsupported texture export use: Readback",
                                location);
        }
        assert(false);
        return CompileError(GraphErrorCode::InvalidUsage, "Invalid texture export use", location);
    }

    // 纹理按图尾用途核对采样视图, 传输范围或呈现状态. Buffer 的 ShaderRead 核对结构化读视图.
    static GraphFailure ValidateExportUses(const RenderGraph &graph, const GraphCompileOptions &options,
                                           std::span<const GraphTextureSupport *const> textureSupport)
    {
        for (const auto &output : graph.Exports())
        {
            if (const auto *texture = std::get_if<TextureExport>(&output))
            {
                const auto index = texture->resource.identity.index;
                if (auto error = ValidateTextureExport(*texture, graph.Textures()[index], *textureSupport[index],
                                                        options.presentationFinalState))
                {
                    return error;
                }
            }
            else
            {
                const auto &buffer = std::get<BufferExport>(output);
                const auto &desc = graph.Buffers()[buffer.resource.identity.index].desc;
                if (buffer.usage == ExportUsage::ShaderRead &&
                    !SupportsBufferUse(desc, buffer.range, GraphUsage::ShaderRead, options.buffers))
                {
                    return CompileError(GraphErrorCode::UnsupportedCapability, "Unsupported buffer export view",
                                        LocateResource(buffer.resource, desc.debugName));
                }
            }
        }
        return std::nullopt;
    }

    // 先保存每项纹理对应的能力行, 再以已校验的资源索引组装用途. 任一声明失败即结束该阶段.
    GraphFailure BuildPassPlans(const RenderGraph &graph, const GraphCompileOptions &options,
                               std::vector<PlannedGraphPass> &passes)
    {
        std::vector<const GraphTextureSupport *> textureSupport(0);
        textureSupport.reserve(graph.Textures().size());
        for (std::uint32_t i = 0; i < graph.Textures().size(); ++i)
        {
            const auto &resource = graph.Textures()[i];
            const CompileResourceLocation location{CompileResourceLocation::Kind::Texture, i, 0,
                                                   resource.desc.debugName};
            const auto support = InspectTexture(resource, options, location);
            if (const auto *error = std::get_if<GraphError>(&support))
            {
                return *error;
            }
            textureSupport.push_back(std::get<const GraphTextureSupport *>(support));
        }
        for (std::uint32_t i = 0; i < graph.Buffers().size(); ++i)
        {
            const auto &resource = graph.Buffers()[i];
            const CompileResourceLocation location{CompileResourceLocation::Kind::Buffer, i, 0,
                                                   resource.desc.debugName};
            if (auto error = InspectBuffer(resource, options, location))
            {
                return error;
            }
        }
        for (const auto &pass : graph.Passes())
        {
            if (auto error = BuildPassPlan(graph, pass, options, textureSupport, passes[pass.id.index]))
            {
                return error;
            }
        }
        return ValidateExportUses(graph, options, textureSupport);
    }
}
