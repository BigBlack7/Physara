/*
* Copyright (c) 2014-2021, NVIDIA CORPORATION. All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*/

#include "ValidationBackend.hpp"

#include <algorithm>
#include <cassert>
#include <bit>
#include <cmath>
#include <cstdio>
#include <limits>
#include <new>
#include <optional>
#include <sstream>
#include <stdexcept>

#include "Common/Misc.hpp"
#include "Utils.hpp"

namespace Physara::RHI::Validation
{
    namespace Input
    {
        bool Reject(Device *device, const char *reason, const std::string *name, std::source_location location)
        {
            char message[1024];
            const char *prefix = std::string_view(reason).starts_with("Unsupported:") ? "" : "InvalidArgument: ";
            std::snprintf(message, sizeof(message), "%s%s [%s]", prefix, reason,
                          name ? Utils::DebugNameToString(*name) : "<input>");
            Report(device->GetMessageCallback(), MessageSeverity::Error, message, location);
            return false;
        }

        bool CheckState(Device *device, ResourceStates state, bool allowUnknown, std::source_location location)
        {
            if ((!allowUnknown && !state) || (uint32_t(state) & ~0x03ffffffu) != 0)
            {
                return Reject(device, "Unknown or invalid resource state bits", nullptr, location);
            }
            if ((state & Profile::OpenGLExcludedStates) != 0 && Profile::RejectOpenGL(device, location))
            {
                return false;
            }
            return true;
        }

        bool CheckTextureDesc(Device *device, const TextureDesc &desc, bool querySupport, std::source_location location)
        {
            if (Profile::IsOpenGL(device) &&
                (desc.isVirtual || desc.isShadingRateSurface || desc.sharedResourceFlags != SharedResourceFlags::None ||
                 desc.sampleQuality != 0 || (desc.initialState & Profile::OpenGLExcludedStates) != 0))
            {
                Profile::RejectOpenGL(device, location);
                return false;
            }
            const auto dimension = desc.dimension;
            if (dimension < TextureDimension::Texture1D || dimension > TextureDimension::Texture3D || desc.width == 0 ||
                desc.height == 0 || desc.depth == 0 || desc.arraySize == 0 || desc.mipLevels == 0 ||
                desc.mipLevels >
                    static_cast<uint32_t>(std::bit_width(std::max({desc.width, desc.height, desc.depth}))) ||
                desc.format == Format::UNKNOWN || desc.format >= Format::COUNT ||
                (uint32_t(desc.sharedResourceFlags) & ~7u) != 0)
            {
                return Reject(device, "invalid texture dimensions, mips, format or flags", &desc.debugName, location);
            }
            const bool array =
                dimension == TextureDimension::Texture1DArray || dimension == TextureDimension::Texture2DArray ||
                dimension == TextureDimension::TextureCube || dimension == TextureDimension::TextureCubeArray ||
                dimension == TextureDimension::Texture2DMSArray;
            const bool ms =
                dimension == TextureDimension::Texture2DMS || dimension == TextureDimension::Texture2DMSArray;
            const bool cube =
                dimension == TextureDimension::TextureCube || dimension == TextureDimension::TextureCubeArray;
            if ((!array && desc.arraySize != 1) ||
                ((dimension == TextureDimension::Texture1D || dimension == TextureDimension::Texture1DArray) &&
                 desc.height != 1) ||
                (dimension != TextureDimension::Texture3D && desc.depth != 1) ||
                (cube && (desc.width != desc.height || desc.arraySize % 6 != 0)) ||
                (dimension == TextureDimension::TextureCube && desc.arraySize != 6) ||
                (ms && (desc.mipLevels != 1 || desc.isUAV ||
                        (desc.sampleCount != 2 && desc.sampleCount != 4 && desc.sampleCount != 8))) ||
                (!ms && desc.sampleCount != 1) ||
                size_t(desc.arraySize) > std::vector<ResourceStates>().max_size() / desc.mipLevels)
            {
                return Reject(device, "incompatible texture shape, samples or subresource count", &desc.debugName,
                              location);
            }
            if (!CheckState(device, desc.initialState, !desc.keepInitialState, location))
            {
                return false;
            }
            if (desc.isVirtual && !Profile::QueryFeatureSupport(device, Feature::VirtualResources))
            {
                Report(device->GetMessageCallback(), MessageSeverity::Error, "Unsupported: virtual textures", location);
                return false;
            }
            const auto &format = GetFormatInfo(desc.format);
            if ((format.blockSize > 1 && (desc.isRenderTarget || desc.isUAV || ms)) ||
                (format.kind == FormatKind::DepthStencil && desc.isUAV))
            {
                return Reject(device, "format is incompatible with texture usage", &desc.debugName, location);
            }
            if (querySupport)
            {
                const auto support = device->QueryFormatSupport(desc.format);
                FormatSupport required = FormatSupport::Texture;
                if (desc.isRenderTarget)
                {
                    required = required | (format.kind == FormatKind::DepthStencil ? FormatSupport::DepthStencil
                                                                                   : FormatSupport::RenderTarget);
                }
                if (desc.isUAV)
                {
                    required = required | FormatSupport::ShaderUavStore;
                }
                if ((support & required) != required ||
                    (desc.isShaderResource && !(support & (FormatSupport::ShaderLoad | FormatSupport::ShaderSample))))
                {
                    return Reject(device, "Unsupported: underlying format query lacks requested texture usage",
                                  &desc.debugName, location);
                }
            }
            return true;
        }

        bool CheckBufferDesc(Device *device, const BufferDesc &desc, bool querySupport, std::source_location location)
        {
            if (Profile::IsOpenGL(device) &&
                (desc.isVirtual || desc.isAccelStructBuildInput || desc.isAccelStructStorage ||
                 desc.isShaderBindingTable || desc.sharedResourceFlags != SharedResourceFlags::None ||
                 (desc.initialState & Profile::OpenGLExcludedStates) != 0))
            {
                Profile::RejectOpenGL(device, location);
                return false;
            }
            if (desc.byteSize == 0 || desc.cpuAccess > CpuAccessMode::Write || desc.format >= Format::COUNT ||
                desc.structStride > desc.byteSize || (uint32_t(desc.sharedResourceFlags) & ~7u) != 0 ||
                (desc.isVolatile &&
                 (!desc.isConstantBuffer || desc.maxVersions == 0 || desc.cpuAccess != CpuAccessMode::None ||
                  desc.isVertexBuffer || desc.isIndexBuffer || desc.isDrawIndirectArgs || desc.canHaveUAVs ||
                  desc.isAccelStructBuildInput || desc.isAccelStructStorage || desc.isShaderBindingTable ||
                  desc.isVirtual)))
            {
                return Reject(device, "invalid buffer size, stride, flags or CPU access", &desc.debugName, location);
            }
            if (!CheckState(device, desc.initialState, !desc.keepInitialState, location))
            {
                return false;
            }
            if (desc.isVirtual && !Profile::QueryFeatureSupport(device, Feature::VirtualResources))
            {
                Report(device->GetMessageCallback(), MessageSeverity::Error, "Unsupported: virtual buffers", location);
                return false;
            }
            if (desc.format != Format::UNKNOWN)
            {
                const auto &format = GetFormatInfo(desc.format);
                if (format.blockSize != 1 || format.kind == FormatKind::DepthStencil || format.bytesPerBlock == 0)
                {
                    return Reject(device, "invalid typed buffer format", &desc.debugName, location);
                }
                if (querySupport && desc.canHaveTypedViews &&
                    !(device->QueryFormatSupport(desc.format) & FormatSupport::Buffer))
                {
                    return Reject(device, "Unsupported: underlying format query lacks buffer support", &desc.debugName,
                                  location);
                }
            }
            return true;
        }

        bool CheckTextureRange(Device *device, Texture *texture, const TextureSubresourceSet &range, bool singleMip,
                               std::source_location location)
        {
            if (!texture)
            {
                return Reject(device, "null required texture", nullptr, location);
            }
            const auto &desc = texture->GetDesc();
            if (!CheckTextureDesc(device, desc, false, location))
            {
                return false;
            }
            const auto resolved = range.Resolve(desc, singleMip);
            if (resolved.numMipLevels == 0 || resolved.numArraySlices == 0)
            {
                return Reject(device, "empty or out-of-range texture subresources", &desc.debugName, location);
            }
            return true;
        }

        bool CheckTextureSlice(Device *device, const TextureDesc &desc, const TextureSlice &slice,
                               std::source_location location)
        {
            if (!CheckTextureDesc(device, desc, false, location))
            {
                return false;
            }
            if (slice.mipLevel >= desc.mipLevels || slice.mipLevel >= 32 || slice.arraySlice >= desc.arraySize)
            {
                return Reject(device, "out-of-range texture slice mip or layer", &desc.debugName, location);
            }
            const auto resolved = slice.Resolve(desc);
            const auto width = std::max(desc.width >> slice.mipLevel, 1u);
            const auto height = std::max(desc.height >> slice.mipLevel, 1u);
            const auto depth =
                desc.dimension == TextureDimension::Texture3D ? std::max(desc.depth >> slice.mipLevel, 1u) : 1u;
            if (resolved.width == 0 || resolved.height == 0 || resolved.depth == 0 || resolved.x > width ||
                resolved.width > width - resolved.x || resolved.y > height || resolved.height > height - resolved.y ||
                resolved.z > depth || resolved.depth > depth - resolved.z)
            {
                return Reject(device, "texture slice exceeds mip extent", &desc.debugName, location);
            }
            const auto block = GetFormatInfo(desc.format).blockSize;
            if (block > 1 && (resolved.x % block != 0 || resolved.y % block != 0 ||
                              (resolved.width % block != 0 && resolved.width != width - resolved.x) ||
                              (resolved.height % block != 0 && resolved.height != height - resolved.y)))
            {
                return Reject(device, "compressed texture slice is not block-aligned", &desc.debugName, location);
            }
            return true;
        }

        bool CheckTextureCopy(Device *device, const TextureDesc &dest, const TextureSlice &destSlice,
                              const TextureDesc &src, const TextureSlice &srcSlice, std::source_location location)
        {
            if (!CheckTextureSlice(device, dest, destSlice, location) ||
                !CheckTextureSlice(device, src, srcSlice, location))
            {
                return false;
            }
            const auto dst = destSlice.Resolve(dest);
            const auto source = srcSlice.Resolve(src);
            const auto &df = GetFormatInfo(dest.format);
            const auto &sf = GetFormatInfo(src.format);
            const uint32_t dw = dst.width / df.blockSize + (dst.width % df.blockSize != 0);
            const uint32_t dh = dst.height / df.blockSize + (dst.height % df.blockSize != 0);
            const uint32_t sw = source.width / sf.blockSize + (source.width % sf.blockSize != 0);
            const uint32_t sh = source.height / sf.blockSize + (source.height % sf.blockSize != 0);
            if (dest.sampleCount != src.sampleCount || df.bytesPerBlock != sf.bytesPerBlock || dw != sw || dh != sh ||
                dst.depth != source.depth ||
                ((df.kind == FormatKind::DepthStencil || sf.kind == FormatKind::DepthStencil) &&
                 dest.format != src.format))
            {
                return Reject(device, "incompatible texture copy extents, samples or formats", nullptr, location);
            }
            return true;
        }

        bool CheckBufferRange(Device *device, Buffer *buffer, uint64_t offset, uint64_t size,
                              std::source_location location)
        {
            if (!buffer)
            {
                return Reject(device, "null required buffer", nullptr, location);
            }
            const auto &desc = buffer->GetDesc();
            if (!CheckBufferDesc(device, desc, false, location))
            {
                return false;
            }
            if (offset > desc.byteSize || size > desc.byteSize - offset)
            {
                return Reject(device, "buffer interval overflows or exceeds capacity", &desc.debugName, location);
            }
            return true;
        }

        bool CheckViewFormat(Device *device, const TextureDesc &desc, Format view, FormatSupport required,
                             std::source_location location)
        {
            if (view == Format::UNKNOWN)
            {
                view = desc.format;
            }
            if (view == Format::UNKNOWN || view >= Format::COUNT)
            {
                return Reject(device, "invalid texture view format", &desc.debugName, location);
            }
            const auto &resource = GetFormatInfo(desc.format);
            const auto &format = GetFormatInfo(view);
            // 来源的 stencil SRV 只选择模板平面, hasDepth 与完整资源不同.
            const bool stencilSrv =
                required == FormatSupport::Texture && ((desc.format == Format::D24S8 && view == Format::X24G8_UINT) ||
                                                       (desc.format == Format::D32S8 && view == Format::X32G8_UINT));
            if ((view != desc.format && !desc.isTypeless) || format.bytesPerBlock != resource.bytesPerBlock ||
                format.blockSize != resource.blockSize ||
                (!stencilSrv && (format.hasDepth != resource.hasDepth || format.hasStencil != resource.hasStencil)))
            {
                return Reject(device, "incompatible texture view format", &desc.debugName, location);
            }
            const auto support = device->QueryFormatSupport(view);
            if ((support & required) != required)
            {
                return Reject(device, "Unsupported: underlying format query lacks requested view usage",
                              &desc.debugName, location);
            }
            return true;
        }

        bool CheckBindingLayoutDesc(Device *device, const BindingLayoutDesc &desc, std::source_location location)
        {
            if (desc.visibility == ShaderType::None || (uint16_t(desc.visibility) & ~uint16_t(ShaderType::All)) != 0 ||
                desc.bindings.size() > std::numeric_limits<uint32_t>::max())
            {
                return Reject(device, "invalid binding layout visibility or count", nullptr, location);
            }
            std::unordered_set<uint64_t> locations;
            uint32_t volatileCount = 0;
            uint32_t pushCount = 0;
            for (const auto &item : desc.bindings)
            {
                if (Profile::IsOpenGL(device) && (item.type == ResourceType::RayTracingAccelStruct ||
                                                  item.type == ResourceType::SamplerFeedbackTexture_UAV))
                {
                    Profile::RejectOpenGL(device, location);
                    return false;
                }
                if (item.type == ResourceType::None || item.type >= ResourceType::Count || item.size == 0)
                {
                    return Reject(device, "invalid binding layout type or size", nullptr, location);
                }
                uint32_t group = 0;
                uint32_t offset = 0;
                switch (item.type)
                {
                case ResourceType::Texture_SRV:
                case ResourceType::TypedBuffer_SRV:
                case ResourceType::StructuredBuffer_SRV:
                case ResourceType::RawBuffer_SRV:
                case ResourceType::RayTracingAccelStruct:
                    group = 0;
                    offset = desc.bindingOffsets.shaderResource;
                    break;
                case ResourceType::Texture_UAV:
                case ResourceType::TypedBuffer_UAV:
                case ResourceType::StructuredBuffer_UAV:
                case ResourceType::RawBuffer_UAV:
                case ResourceType::SamplerFeedbackTexture_UAV:
                    group = 1;
                    offset = desc.bindingOffsets.unorderedAccess;
                    break;
                case ResourceType::Sampler:
                    group = 2;
                    offset = desc.bindingOffsets.sampler;
                    break;
                default:
                    group = 3;
                    offset = desc.bindingOffsets.constantBuffer;
                    break;
                }
                if (!locations.insert((uint64_t(group) << 32) | item.slot).second)
                {
                    return Reject(device, "duplicate binding layout register location", nullptr, location);
                }
                if (item.type == ResourceType::VolatileConstantBuffer &&
                    (item.size != 1 || ++volatileCount > MaxVolatileConstantBuffersPerLayout))
                {
                    return Reject(device, "invalid volatile constant buffer array/count", nullptr, location);
                }
                if (item.type == ResourceType::PushConstants &&
                    (++pushCount > 1 || item.size > MaxPushConstantSize || item.size % 4 != 0))
                {
                    return Reject(device, "invalid push constant block count/size/alignment", nullptr, location);
                }
                if (device->GetGraphicsAPI() == GraphicsAPI::VULKAN &&
                    item.slot > std::numeric_limits<uint32_t>::max() - offset)
                {
                    return Reject(device, "Vulkan register offset overflows", nullptr, location);
                }
            }
            if (device->GetGraphicsAPI() == GraphicsAPI::VULKAN && desc.registerSpace != 0 &&
                !desc.registerSpaceIsDescriptorSet)
            {
                return Reject(device, "nonzero Vulkan register space requires descriptor-set semantics", nullptr,
                              location);
            }
            return true;
        }
    }
    namespace Profile
    {
        bool HasExcludedShaderExtensions(const ShaderDesc &desc)
        {
            return (desc.shaderType & (ShaderType::AllRayTracing | ShaderType::Mesh | ShaderType::Amplification)) !=
                       0 ||
                   desc.hlslExtensionsUAV >= 0 || desc.useSpecificShaderExt || desc.numCustomSemantics != 0 ||
                   desc.pCustomSemantics != nullptr || desc.fastGSFlags != FastGeometryShaderFlags(0) ||
                   desc.pCoordinateSwizzling != nullptr;
        }

        bool HasExcludedRenderExtensions(const RenderState &state)
        {
            return state.singlePassStereo.enabled || state.rasterState.conservativeRasterEnable ||
                   state.rasterState.forcedSampleCount != 0 || state.rasterState.programmableSamplePositionsEnable ||
                   state.rasterState.quadFillEnable;
        }

        bool RejectBindingLayout(Device *device, BindingLayout *layout, std::source_location location)
        {
            if (!IsOpenGL(device) || !layout)
            {
                return false;
            }
            const auto *desc = layout->GetDesc();
            if (!desc)
            {
                return RejectOpenGL(device, location);
            }
            for (const auto &item : desc->bindings)
            {
                if (item.type == ResourceType::RayTracingAccelStruct ||
                    item.type == ResourceType::SamplerFeedbackTexture_UAV)
                {
                    return RejectOpenGL(device, location);
                }
            }
            return false;
        }

        bool RejectBindingSet(Device *device, BindingSet *bindings, std::source_location location)
        {
            if (!IsOpenGL(device) || !bindings)
            {
                return false;
            }
            if (RejectBindingLayout(device, bindings->GetLayout(), location))
            {
                return true;
            }
            const auto *desc = bindings->GetDesc();
            if (!desc)
            {
                return RejectOpenGL(device, location);
            }
            for (const auto &item : desc->bindings)
            {
                if (item.type == ResourceType::RayTracingAccelStruct ||
                    item.type == ResourceType::SamplerFeedbackTexture_UAV)
                {
                    return RejectOpenGL(device, location);
                }
            }
            return false;
        }

        bool RejectGraphicsPipeline(Device *device, const GraphicsPipelineDesc &desc, std::source_location location)
        {
            if (!IsOpenGL(device))
            {
                return false;
            }
            if (desc.shadingRateState.enabled || HasExcludedRenderExtensions(desc.renderState))
            {
                return RejectOpenGL(device, location);
            }
            for (Shader *shader : {desc.VS.Get(), desc.HS.Get(), desc.DS.Get(), desc.GS.Get(), desc.PS.Get()})
            {
                if (shader && HasExcludedShaderExtensions(shader->GetDesc()))
                {
                    return RejectOpenGL(device, location);
                }
            }
            for (const auto &layout : desc.bindingLayouts)
            {
                if (RejectBindingLayout(device, layout, location))
                {
                    return true;
                }
            }
            return false;
        }

        bool RejectComputePipeline(Device *device, const ComputePipelineDesc &desc, std::source_location location)
        {
            if (!IsOpenGL(device))
            {
                return false;
            }
            if (desc.CS && HasExcludedShaderExtensions(desc.CS->GetDesc()))
            {
                return RejectOpenGL(device, location);
            }
            for (const auto &layout : desc.bindingLayouts)
            {
                if (RejectBindingLayout(device, layout, location))
                {
                    return true;
                }
            }
            return false;
        }

        bool IsOpenGL(Device *device)
        {
            return device->GetGraphicsAPI() == GraphicsAPI::OpenGL;
        }

        bool RejectOpenGL(Device *device, std::source_location location)
        {
            if (!IsOpenGL(device))
            {
                return false;
            }
            Report(device->GetMessageCallback(), MessageSeverity::Error,
                   "Unsupported: operation is outside the initial OpenGL policy", location);
            return true;
        }

        bool CheckQueue(Device *device, CommandQueue queue, std::source_location location)
        {
            if (queue >= CommandQueue::Count)
            {
                Report(device->GetMessageCallback(), MessageSeverity::Error, "InvalidArgument: unknown command queue",
                       location);
                return false;
            }
            if (IsOpenGL(device) && queue != CommandQueue::Graphics)
            {
                RejectOpenGL(device, location);
                return false;
            }
            return true;
        }

        bool QueryFeatureSupport(Device *device, Feature feature, void *info, size_t infoSize,
                                 std::source_location location)
        {
            if (feature > Feature::EnhancedBarriers)
            {
                Report(device->GetMessageCallback(), MessageSeverity::Error, "InvalidArgument: unknown Feature",
                       location);
                return false;
            }
            if (IsOpenGL(device))
            {
                switch (feature)
                {
                case Feature::ConstantBufferRanges:
                    break;
                case Feature::ComputeQueue:
                case Feature::ConservativeRasterization:
                case Feature::CopyQueue:
                case Feature::DeferredCommandLists:
                case Feature::FastGeometryShader:
                case Feature::HeapDirectlyIndexed:
                case Feature::HlslExtensionUAV:
                case Feature::LinearSweptSpheres:
                case Feature::Meshlets:
                case Feature::RayQuery:
                case Feature::RayTracingAccelStruct:
                case Feature::RayTracingClusters:
                case Feature::RayTracingOpacityMicromap:
                case Feature::RayTracingPipeline:
                case Feature::RayTracingPositionFetch:
                case Feature::SamplerFeedback:
                case Feature::ShaderExecutionReordering:
                case Feature::ShaderSpecializations:
                case Feature::SinglePassStereo:
                case Feature::Spheres:
                case Feature::VariableRateShading:
                case Feature::VirtualResources:
                case Feature::WaveLaneCountMinMax:
                case Feature::CooperativeVectorInferencing:
                case Feature::CooperativeVectorTraining:
                case Feature::EnhancedBarriers:
                default:
                    return false;
                }
            }
            return device->QueryFeatureSupport(feature, info, infoSize);
        }
    }
    void Report(MessageCallback *callback, MessageSeverity severity, std::string_view message,
                std::source_location location)
    {
        // 错误路径不分配, 因此也能报告工厂/预检自身的 bad_alloc. 文本只在同步 callback 调用期间借用.
        char text[2048];
        const char *reason = severity == MessageSeverity::Warning ? "Warning" : "InvalidArgument";
        if (message.starts_with("InvalidState:") || message.starts_with("Unsupported:") ||
            message.starts_with("InvalidArgument:"))
        {
            reason = "Validation";
        }
        std::snprintf(text, sizeof(text), "%s: %s: %.*s at %s:%u", reason, location.function_name(),
                      static_cast<int>(std::min(message.size(), size_t{1400})), message.data(), location.file_name(),
                      location.line());
        callback->Message(severity, text);
    }
    template <typename T>
    static std::unordered_set<T> SetDifference(std::unordered_set<T> const &a, std::unordered_set<T> const &b)
    {
        std::unordered_set<T> result = a;
        for (T const &item : b)
        {
            result.erase(item);
        }
        return result;
    }

    template <typename T>
    static std::unordered_set<T> SetIntersection(std::unordered_set<T> const &a, std::unordered_set<T> const &b)
    {
        std::unordered_set<T> result;
        for (T const &item : a)
        {
            if (b.find(item) != b.end())
            {
                result.insert(item);
            }
        }
        return result;
    }

    template <typename T> static void SetUnionInplace(std::unordered_set<T> &a, std::unordered_set<T> const &b)
    {
        for (T const &item : b)
        {
            a.insert(item);
        }
    }

    DeviceHandle CreateValidationLayer(Device *underlyingDevice)
    {
        if (!underlyingDevice)
        {
            return nullptr;
        }
        auto *callback = underlyingDevice->GetMessageCallback();
        if (!callback)
        {
            return nullptr;
        }
        switch (underlyingDevice->GetGraphicsAPI())
        {
        case GraphicsAPI::D3D11:
        case GraphicsAPI::D3D12:
        case GraphicsAPI::VULKAN:
        case GraphicsAPI::OpenGL:
            break;
        default:
            Report(callback, MessageSeverity::Error, "InvalidArgument: unknown GraphicsAPI");
            return nullptr;
        }
        try
        {
            return DeviceHandle::Create(new DeviceWrapper(underlyingDevice));
        }
        catch (const std::bad_alloc &)
        {
            Report(callback, MessageSeverity::Error, "CreateValidationLayer: CPU allocation failed");
            return nullptr;
        }
    }

    DeviceWrapper::DeviceWrapper(Device *device) : m_Device(device), m_MessageCallback(device->GetMessageCallback())
    {
    }

    void DeviceWrapper::Error(const std::string &messageText, std::source_location location) const
    {
        Report(m_MessageCallback, MessageSeverity::Error, messageText, location);
    }

    void DeviceWrapper::Warning(const std::string &messageText, std::source_location location) const
    {
        Report(m_MessageCallback, MessageSeverity::Warning, messageText, location);
    }

    Object DeviceWrapper::GetNativeObject(ObjectType objectType)
    {
        return m_Device->GetNativeObject(objectType);
    }

    HeapHandle DeviceWrapper::CreateHeap(const HeapDesc &d)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return nullptr;
        }
        if (d.type < HeapType::DeviceLocal || d.type > HeapType::Readback)
        {
            Error("InvalidArgument: invalid heap type");
            return nullptr;
        }

        if (d.capacity == 0)
        {
            Error("Cannot create a Heap with capacity = 0");
            return nullptr;
        }

        HeapDesc patchedDesc = d;
        if (patchedDesc.debugName.empty())
        {
            patchedDesc.debugName = Utils::GenerateHeapDebugName(patchedDesc);
        }

        return m_Device->CreateHeap(patchedDesc);
    }

    TextureHandle DeviceWrapper::CreateTexture(const TextureDesc &d)
    {
        if (!Input::CheckTextureDesc(m_Device, d, true))
        {
            return nullptr;
        }
        TextureDesc patchedDesc = d;
        if (patchedDesc.debugName.empty())
        {
            patchedDesc.debugName = Utils::GenerateTextureDebugName(patchedDesc);
        }
        return m_Device->CreateTexture(patchedDesc);
    }

    void DeviceWrapper::GetTextureTiling(Texture *texture, uint32_t *numTiles, PackedMipDesc *desc,
                                         TileShape *tileShape, uint32_t *subresourceTilingsNum,
                                         SubresourceTiling *subresourceTilings)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return;
        }

        m_Device->GetTextureTiling(texture, numTiles, desc, tileShape, subresourceTilingsNum, subresourceTilings);
    }

    void DeviceWrapper::UpdateTextureTileMappings(Texture *texture, const TextureTilesMapping *tileMappings,
                                                  uint32_t numTileMappings, CommandQueue executionQueue)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return;
        }

        m_Device->UpdateTextureTileMappings(texture, tileMappings, numTileMappings, executionQueue);
    }

    SamplerFeedbackTextureHandle DeviceWrapper::CreateSamplerFeedbackTexture(Texture *pairedTexture,
                                                                             const SamplerFeedbackTextureDesc &desc)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return nullptr;
        }
        if (!pairedTexture)
        {
            Error("InvalidArgument: paired texture is null");
            return nullptr;
        }
        if (!Input::CheckTextureDesc(m_Device, pairedTexture->GetDesc()))
        {
            return nullptr;
        }

        const GraphicsAPI graphicsApi = m_Device->GetGraphicsAPI();
        if (graphicsApi != GraphicsAPI::D3D12)
        {
            std::stringstream ss;
            ss << "The current graphics API (" << Utils::GraphicsAPIToString(m_Device->GetGraphicsAPI())
               << ") "
                  "doesn't support createSamplerFeedbackTexture";
            Error(ss.str());
            return nullptr;
        }

        return m_Device->CreateSamplerFeedbackTexture(pairedTexture, desc);
    }

    SamplerFeedbackTextureHandle DeviceWrapper::CreateSamplerFeedbackForNativeTexture(ObjectType objectType,
                                                                                      Object texture,
                                                                                      Texture *pairedTexture)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return nullptr;
        }
        if (!pairedTexture)
        {
            Error("InvalidArgument: paired texture is null");
            return nullptr;
        }

        const GraphicsAPI graphicsApi = m_Device->GetGraphicsAPI();
        if (graphicsApi != GraphicsAPI::D3D12)
        {
            std::stringstream ss;
            ss << "The current graphics API (" << Utils::GraphicsAPIToString(m_Device->GetGraphicsAPI())
               << ") "
                  "doesn't support createSamplerFeedbackForNativeTexture";
            Error(ss.str());
            return nullptr;
        }

        return m_Device->CreateSamplerFeedbackForNativeTexture(objectType, texture, pairedTexture);
    }

    MemoryRequirements DeviceWrapper::GetTextureMemoryRequirements(Texture *texture)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return {};
        }

        if (texture == nullptr)
        {
            Error("getTextureMemoryRequirements: texture is NULL");
            return MemoryRequirements();
        }

        const MemoryRequirements memReq = m_Device->GetTextureMemoryRequirements(texture);

        if (memReq.size == 0)
        {
            std::stringstream ss;
            ss << "Invalid texture " << Utils::DebugNameToString(texture->GetDesc().debugName)
               << ": "
                  "getTextureMemoryRequirements returned zero size";

            Error(ss.str());
        }

        return memReq;
    }

    bool DeviceWrapper::BindTextureMemory(Texture *texture, Heap *heap, uint64_t offset)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return false;
        }

        if (texture == nullptr)
        {
            Error("bindTextureMemory: texture is NULL");
            return false;
        }

        if (heap == nullptr)
        {
            Error("bindTextureMemory: heap is NULL");
            return false;
        }

        const HeapDesc &heapDesc = heap->GetDesc();
        const TextureDesc &textureDesc = texture->GetDesc();

        if (!textureDesc.isVirtual)
        {
            std::stringstream ss;
            ss << "Cannot perform bindTextureMemory on texture " << Utils::DebugNameToString(textureDesc.debugName)
               << " because it was created with isVirtual = false";

            Error(ss.str());
            return false;
        }

        MemoryRequirements memReq = m_Device->GetTextureMemoryRequirements(texture);

        if (memReq.size == 0 || memReq.alignment == 0 || offset > heapDesc.capacity ||
            memReq.size > heapDesc.capacity - offset)
        {
            std::stringstream ss;
            ss << "Texture " << Utils::DebugNameToString(textureDesc.debugName) << " does not fit into heap "
               << Utils::DebugNameToString(heapDesc.debugName) << " at offset " << offset << " because it requires "
               << memReq.size << " bytes,"
               << " and the heap capacity is " << heapDesc.capacity << " bytes";

            Error(ss.str());
            return false;
        }

        if (memReq.alignment != 0 && (offset % memReq.alignment) != 0)
        {
            std::stringstream ss;
            ss << "Texture " << Utils::DebugNameToString(textureDesc.debugName) << " is placed in heap "
               << Utils::DebugNameToString(heapDesc.debugName) << " at invalid alignment: required alignment to "
               << memReq.alignment << " bytes,"
               << " actual offset is " << offset << " bytes";

            Error(ss.str());
            return false;
        }

        return m_Device->BindTextureMemory(texture, heap, offset);
    }

    TextureHandle DeviceWrapper::CreateHandleForNativeTexture(ObjectType objectType, Object texture,
                                                              const TextureDesc &desc)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return nullptr;
        }

        return m_Device->CreateHandleForNativeTexture(objectType, texture, desc);
    }

    StagingTextureHandle DeviceWrapper::CreateStagingTexture(const TextureDesc &d, CpuAccessMode cpuAccess)
    {
        if (Profile::IsOpenGL(m_Device) &&
            (d.isVirtual || d.isShadingRateSurface || d.sharedResourceFlags != SharedResourceFlags::None ||
             (d.initialState & Profile::OpenGLExcludedStates) != 0))
        {
            Profile::RejectOpenGL(m_Device);
            return nullptr;
        }
        if (!Input::CheckTextureDesc(m_Device, d))
        {
            return nullptr;
        }
        if (cpuAccess == CpuAccessMode::None || cpuAccess > CpuAccessMode::Write)
        {
            Error("InvalidArgument: staging texture requires Read or Write CPU access");
            return nullptr;
        }

        TextureDesc patchedDesc = d;
        if (patchedDesc.debugName.empty())
        {
            patchedDesc.debugName = Utils::GenerateTextureDebugName(patchedDesc);
        }

        return m_Device->CreateStagingTexture(patchedDesc, cpuAccess);
    }

    void *DeviceWrapper::MapStagingTexture(StagingTexture *tex, const TextureSlice &slice, CpuAccessMode cpuAccess,
                                           size_t *outRowPitch)
    {
        if (!tex || !outRowPitch || cpuAccess == CpuAccessMode::None || cpuAccess > CpuAccessMode::Write)
        {
            Error("InvalidArgument: required staging texture/pitch output or CPU access is invalid");
            return nullptr;
        }
        if (!Input::CheckTextureSlice(m_Device, tex->GetDesc(), slice))
        {
            return nullptr;
        }

        return m_Device->MapStagingTexture(tex, slice, cpuAccess, outRowPitch);
    }

    void DeviceWrapper::UnmapStagingTexture(StagingTexture *tex)
    {
        if (!tex)
        {
            Error("InvalidArgument: null staging texture");
            return;
        }

        m_Device->UnmapStagingTexture(tex);
    }

    BufferHandle DeviceWrapper::CreateBuffer(const BufferDesc &d)
    {
        if (!Input::CheckBufferDesc(m_Device, d, true))
        {
            return nullptr;
        }
        BufferDesc patchedDesc = d;
        if (patchedDesc.debugName.empty())
        {
            patchedDesc.debugName = Utils::GenerateBufferDebugName(patchedDesc);
        }
        return m_Device->CreateBuffer(patchedDesc);
    }

    void *DeviceWrapper::MapBuffer(Buffer *b, CpuAccessMode mapFlags)
    {
        if (!b || mapFlags == CpuAccessMode::None || mapFlags > CpuAccessMode::Write)
        {
            Error("InvalidArgument: buffer and Read/Write access are required");
            return nullptr;
        }
        if (!Input::CheckBufferDesc(m_Device, b->GetDesc()))
        {
            return nullptr;
        }
        if (b->GetDesc().cpuAccess != mapFlags)
        {
            Error("InvalidArgument: map access differs from buffer CPU access");
            return nullptr;
        }

        return m_Device->MapBuffer(b, mapFlags);
    }

    void DeviceWrapper::UnmapBuffer(Buffer *b)
    {
        if (!b || b->GetDesc().cpuAccess == CpuAccessMode::None)
        {
            Error("InvalidArgument: CPU-visible buffer is required");
            return;
        }
        if (!Input::CheckBufferDesc(m_Device, b->GetDesc()))
        {
            return;
        }

        m_Device->UnmapBuffer(b);
    }

    MemoryRequirements DeviceWrapper::GetBufferMemoryRequirements(Buffer *buffer)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return {};
        }

        if (buffer == nullptr)
        {
            Error("getBufferMemoryRequirements: buffer is NULL");
            return MemoryRequirements();
        }

        const MemoryRequirements memReq = m_Device->GetBufferMemoryRequirements(buffer);

        if (memReq.size == 0)
        {
            std::stringstream ss;
            ss << "Invalid buffer " << Utils::DebugNameToString(buffer->GetDesc().debugName)
               << ": "
                  "getBufferMemoryRequirements returned zero size";

            Error(ss.str());
        }

        return memReq;
    }

    bool DeviceWrapper::BindBufferMemory(Buffer *buffer, Heap *heap, uint64_t offset)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return false;
        }

        if (buffer == nullptr)
        {
            Error("bindBufferMemory: texture is NULL");
            return false;
        }

        if (heap == nullptr)
        {
            Error("bindBufferMemory: heap is NULL");
            return false;
        }

        const HeapDesc &heapDesc = heap->GetDesc();
        const BufferDesc &bufferDesc = buffer->GetDesc();

        if (!bufferDesc.isVirtual)
        {
            std::stringstream ss;
            ss << "Cannot perform bindBufferMemory on buffer " << Utils::DebugNameToString(bufferDesc.debugName)
               << " because it was created with isVirtual = false";

            Error(ss.str());
            return false;
        }

        MemoryRequirements memReq = m_Device->GetBufferMemoryRequirements(buffer);

        if (memReq.size == 0 || memReq.alignment == 0 || offset > heapDesc.capacity ||
            memReq.size > heapDesc.capacity - offset)
        {
            std::stringstream ss;
            ss << "Buffer " << Utils::DebugNameToString(bufferDesc.debugName) << " does not fit into heap "
               << Utils::DebugNameToString(heapDesc.debugName) << " at offset " << offset << " because it requires "
               << memReq.size << " bytes,"
               << " and the heap capacity is " << heapDesc.capacity << " bytes";

            Error(ss.str());
            return false;
        }

        if (memReq.alignment != 0 && (offset % memReq.alignment) != 0)
        {
            std::stringstream ss;
            ss << "Buffer " << Utils::DebugNameToString(bufferDesc.debugName) << " is placed in heap "
               << Utils::DebugNameToString(heapDesc.debugName) << " at invalid alignment: required alignment to "
               << memReq.alignment << " bytes,"
               << " actual offset is " << offset << " bytes";

            Error(ss.str());
            return false;
        }

        return m_Device->BindBufferMemory(buffer, heap, offset);
    }

    BufferHandle DeviceWrapper::CreateHandleForNativeBuffer(ObjectType objectType, Object buffer,
                                                            const BufferDesc &desc)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return nullptr;
        }

        return m_Device->CreateHandleForNativeBuffer(objectType, buffer, desc);
    }

    ShaderHandle DeviceWrapper::CreateShader(const ShaderDesc &d, const void *binary, const size_t binarySize)
    {
        if (Profile::IsOpenGL(m_Device) && Profile::HasExcludedShaderExtensions(d))
        {
            Profile::RejectOpenGL(m_Device);
            return nullptr;
        }
        switch (d.shaderType)
        {
        case ShaderType::Compute:
        case ShaderType::Vertex:
        case ShaderType::Hull:
        case ShaderType::Domain:
        case ShaderType::Geometry:
        case ShaderType::Pixel:
        case ShaderType::Amplification:
        case ShaderType::Mesh:
        case ShaderType::RayGeneration:
        case ShaderType::AnyHit:
        case ShaderType::ClosestHit:
        case ShaderType::Miss:
        case ShaderType::Intersection:
        case ShaderType::Callable:
            break;
        default:
            Error("InvalidArgument: shader creation requires a single valid stage");
            return nullptr;
        }
        if (!binary || binarySize == 0 || (d.numCustomSemantics != 0 && !d.pCustomSemantics))
        {
            Error("InvalidArgument: shader binary/custom semantic storage is missing");
            return nullptr;
        }

        return m_Device->CreateShader(d, binary, binarySize);
    }

    ShaderHandle DeviceWrapper::CreateShaderSpecialization(Shader *baseShader, const ShaderSpecialization *constants,
                                                           uint32_t numConstants)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return nullptr;
        }

        if (!Profile::QueryFeatureSupport(m_Device, Feature::ShaderSpecializations))
        {
            std::stringstream ss;
            ss << "The current graphics API (" << Utils::GraphicsAPIToString(m_Device->GetGraphicsAPI())
               << ") "
                  "doesn't support shader specializations";
            Error(ss.str());
            return nullptr;
        }
        if (!baseShader || !constants || numConstants == 0)
        {
            Error("InvalidArgument: shader and specialization storage are required");
            return nullptr;
        }

        if (constants == nullptr || numConstants == 0)
        {
            Error("Both 'constants' and 'numConstatns' must be non-zero in createShaderSpecialization");
            return nullptr;
        }

        if (baseShader == nullptr)
        {
            Error("baseShader must be non-null in createShaderSpecialization");
            return nullptr;
        }

        return m_Device->CreateShaderSpecialization(baseShader, constants, numConstants);
    }

    Physara::RHI::ShaderLibraryHandle DeviceWrapper::CreateShaderLibrary(const void *binary, const size_t binarySize)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return nullptr;
        }
        if (!binary || binarySize == 0)
        {
            Error("InvalidArgument: shader library binary is required");
            return nullptr;
        }

        return m_Device->CreateShaderLibrary(binary, binarySize);
    }

    SamplerHandle DeviceWrapper::CreateSampler(const SamplerDesc &d)
    {
        if (d.addressU > SamplerAddressMode::MirrorOnce || d.addressV > SamplerAddressMode::MirrorOnce ||
            d.addressW > SamplerAddressMode::MirrorOnce || d.reductionType > SamplerReductionType::Maximum ||
            !std::isfinite(d.maxAnisotropy) || d.maxAnisotropy < 1.f || !std::isfinite(d.mipBias))
        {
            Error("InvalidArgument: sampler enum/anisotropy/bias is invalid");
            return nullptr;
        }

        return m_Device->CreateSampler(d);
    }

    InputLayoutHandle DeviceWrapper::CreateInputLayout(const VertexAttributeDesc *d, uint32_t attributeCount,
                                                       Shader *vertexShader)
    {
        if ((attributeCount != 0 && !d) || attributeCount > MaxVertexAttributes)
        {
            Error("InvalidArgument: vertex attribute storage/count is invalid");
            return nullptr;
        }
        uint32_t totalAttributes = 0;
        for (uint32_t index = 0; index < attributeCount; ++index)
        {
            const auto &attribute = d[index];
            if (attribute.format == Format::UNKNOWN || attribute.format >= Format::COUNT || attribute.arraySize == 0 ||
                attribute.bufferIndex >= MaxVertexAttributes ||
                attribute.arraySize > MaxVertexAttributes - totalAttributes)
            {
                Error("InvalidArgument: vertex attribute format/array/buffer slot is invalid");
                return nullptr;
            }
            totalAttributes += attribute.arraySize;
            const auto &format = GetFormatInfo(attribute.format);
            if (format.blockSize != 1 || format.kind == FormatKind::DepthStencil || format.bytesPerBlock == 0 ||
                uint64_t(attribute.offset) + uint64_t(attribute.arraySize) * format.bytesPerBlock >
                    std::numeric_limits<uint32_t>::max())
            {
                Error("InvalidArgument: vertex attribute offset/extent is not representable");
                return nullptr;
            }
            if (!(QueryFormatSupport(attribute.format) & FormatSupport::VertexBuffer))
            {
                Error("Unsupported: vertex format");
                return nullptr;
            }
            for (uint32_t prior = 0; prior < index; ++prior)
            {
                if (d[prior].bufferIndex == attribute.bufferIndex &&
                    (d[prior].elementStride != attribute.elementStride ||
                     d[prior].isInstanced != attribute.isInstanced))
                {
                    Error("InvalidArgument: vertex buffer attributes disagree on stride or frequency");
                    return nullptr;
                }
            }
        }
        if (m_Device->GetGraphicsAPI() == GraphicsAPI::D3D11 && !vertexShader)
        {
            Error("InvalidArgument: D3D11 input layout requires a vertex shader");
            return nullptr;
        }
        if (vertexShader && !ValidateShaderType(ShaderType::Vertex, vertexShader->GetDesc(), "CreateInputLayout"))
        {
            return nullptr;
        }

        return m_Device->CreateInputLayout(d, attributeCount, vertexShader);
    }

    EventQueryHandle DeviceWrapper::CreateEventQuery()
    {
        return m_Device->CreateEventQuery();
    }

    void DeviceWrapper::SetEventQuery(EventQuery *query, CommandQueue queue)
    {
        if (!Profile::CheckQueue(m_Device, queue))
        {
            return;
        }
        if (!query)
        {
            Error("InvalidArgument: query is null");
            return;
        }

        m_Device->SetEventQuery(query, queue);
    }

    bool DeviceWrapper::PollEventQuery(EventQuery *query)
    {
        if (!query)
        {
            Error("InvalidArgument: query is null");
            return false;
        }

        return m_Device->PollEventQuery(query);
    }

    void DeviceWrapper::WaitEventQuery(EventQuery *query)
    {
        if (!query)
        {
            Error("InvalidArgument: query is null");
            return;
        }

        m_Device->WaitEventQuery(query);
    }

    void DeviceWrapper::ResetEventQuery(EventQuery *query)
    {
        if (!query)
        {
            Error("InvalidArgument: query is null");
            return;
        }

        m_Device->ResetEventQuery(query);
    }

    TimerQueryHandle DeviceWrapper::CreateTimerQuery()
    {
        return m_Device->CreateTimerQuery();
    }

    bool DeviceWrapper::PollTimerQuery(TimerQuery *query)
    {
        if (!query)
        {
            Error("InvalidArgument: query is null");
            return false;
        }

        return m_Device->PollTimerQuery(query);
    }

    float DeviceWrapper::GetTimerQueryTime(TimerQuery *query)
    {
        if (!query)
        {
            Error("InvalidArgument: query is null");
            return 0.f;
        }

        return m_Device->GetTimerQueryTime(query);
    }

    void DeviceWrapper::ResetTimerQuery(TimerQuery *query)
    {
        if (!query)
        {
            Error("InvalidArgument: query is null");
            return;
        }

        return m_Device->ResetTimerQuery(query);
    }

    GraphicsAPI DeviceWrapper::GetGraphicsAPI()
    {
        return m_Device->GetGraphicsAPI();
    }

    FramebufferHandle DeviceWrapper::CreateFramebuffer(const FramebufferDesc &desc)
    {
        if (desc.shadingRateAttachment.texture && Profile::RejectOpenGL(m_Device))
        {
            return nullptr;
        }
        for (const auto &attachment : desc.colorAttachments)
        {
            if (!Input::CheckTextureRange(m_Device, attachment.texture, attachment.subresources, true))
            {
                return nullptr;
            }
            if (!Input::CheckViewFormat(m_Device, attachment.texture->GetDesc(), attachment.format,
                                        FormatSupport::RenderTarget))
            {
                return nullptr;
            }
        }
        if (desc.depthAttachment.texture &&
            (!Input::CheckTextureRange(m_Device, desc.depthAttachment.texture, desc.depthAttachment.subresources,
                                       true) ||
             !Input::CheckViewFormat(m_Device, desc.depthAttachment.texture->GetDesc(), desc.depthAttachment.format,
                                     FormatSupport::DepthStencil)))
        {
            return nullptr;
        }
        if (desc.shadingRateAttachment.texture &&
            !Input::CheckTextureRange(m_Device, desc.shadingRateAttachment.texture,
                                      desc.shadingRateAttachment.subresources, true))
        {
            return nullptr;
        }

        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t arraySize = 0;
        uint32_t sampleCount = 0;
        uint32_t sampleQuality = 0;

        if (desc.depthAttachment.texture)
        {
            TextureDesc const &d = desc.depthAttachment.texture->GetDesc();

            TextureSubresourceSet const subresources = desc.depthAttachment.subresources.Resolve(d, true);

            uint32_t const mipWidth = std::max(d.width >> subresources.baseMipLevel, 1u);
            uint32_t const mipHeight = std::max(d.height >> subresources.baseMipLevel, 1u);

            width = mipWidth;
            height = mipHeight;
            arraySize = subresources.numArraySlices;
            sampleCount = d.sampleCount;
            sampleQuality = d.sampleQuality;

            if (!d.isRenderTarget)
            {
                std::stringstream ss;
                ss << "Depth attachment texture " << Utils::DebugNameToString(d.debugName)
                   << " must be created with isRenderTarget = true";
                Error(ss.str());
                return nullptr;
            }

            FormatInfo const &formatInfo = GetFormatInfo(d.format);
            if (formatInfo.kind != FormatKind::DepthStencil)
            {
                std::stringstream ss;
                ss << "Depth attachment texture " << Utils::DebugNameToString(d.debugName) << " has a format "
                   << formatInfo.name << " that does not support depth or stencil";
                Error(ss.str());
                return nullptr;
            }
        }

        for (size_t i = 0; i < desc.colorAttachments.size(); ++i)
        {
            FramebufferAttachment const &att = desc.colorAttachments[i];
            if (!att.texture)
            {
                std::stringstream ss;
                ss << "Color attachment " << i << " is NULL";
                Error(ss.str());
                return nullptr;
            }

            TextureDesc const &d = att.texture->GetDesc();
            if (!d.isRenderTarget)
            {
                std::stringstream ss;
                ss << "Color attachment  " << i << " texture " << Utils::DebugNameToString(d.debugName)
                   << " must be created with isRenderTarget = true";
                Error(ss.str());
                return nullptr;
            }

            TextureSubresourceSet const subresources = att.subresources.Resolve(d, true);

            uint32_t const mipWidth = std::max(d.width >> subresources.baseMipLevel, 1u);
            uint32_t const mipHeight = std::max(d.height >> subresources.baseMipLevel, 1u);

            if (!width || !height || !arraySize || !sampleCount)
            {
                width = mipWidth;
                height = mipHeight;
                arraySize = subresources.numArraySlices;
                sampleCount = d.sampleCount;
                sampleQuality = d.sampleQuality;
            }
            else
            {
                if (width != mipWidth || height != mipHeight || arraySize != subresources.numArraySlices ||
                    sampleCount != d.sampleCount || sampleQuality != d.sampleQuality)
                {
                    std::stringstream ss;
                    ss << "Color attachment " << i << " texture " << Utils::DebugNameToString(d.debugName)
                       << " has dimensions " << mipWidth << "x" << mipHeight << " with " << subresources.numArraySlices
                       << " array slice(s) and " << sampleCount
                       << " sample(s), which do not match the dimensions of depth"
                       << " or previous color attachments " << width << "x" << height << " with " << arraySize
                       << " array slice(s) and " << sampleCount << " sample(s)";
                    Error(ss.str());
                    return nullptr;
                }
            }

            FormatInfo const &formatInfo = GetFormatInfo(d.format);
            if (!formatInfo.hasRed)
            {
                std::stringstream ss;
                ss << "Color attachment " << i << " texture " << Utils::DebugNameToString(d.debugName)
                   << " has a format " << formatInfo.name << " that does not have color or alpha channels";
                Error(ss.str());
                return nullptr;
            }

            if (formatInfo.blockSize > 1)
            {
                std::stringstream ss;
                ss << "Color attachment " << i << " texture " << Utils::DebugNameToString(d.debugName)
                   << " has a format " << formatInfo.name
                   << " that is block-compressed. Block-compressed formats are not supported for render targets.";
                Error(ss.str());
                return nullptr;
            }
        }

        if (desc.shadingRateAttachment.texture)
        {
            TextureDesc const &d = desc.shadingRateAttachment.texture->GetDesc();

            // The shading rate attachment must be an R8_UINT format and have a single sample.
            // Its dimensions may be different (smaller) from the color attachments.
            // Not sure about array size or how VRS works with rendering into a texture array.

            if (d.format != Format::R8_UINT)
            {
                FormatInfo const &formatInfo = GetFormatInfo(d.format);

                std::stringstream ss;
                ss << "Shading rate attachment texture " << Utils::DebugNameToString(d.debugName) << " has a format "
                   << formatInfo.name << " that is not R8_UINT";
                Error(ss.str());
                return nullptr;
            }

            if (d.sampleCount != 1)
            {
                std::stringstream ss;
                ss << "Shading rate attachment texture " << Utils::DebugNameToString(d.debugName)
                   << " has a sample count " << d.sampleCount << " that is not 1";
                Error(ss.str());
                return nullptr;
            }

            if (!d.isShadingRateSurface)
            {
                std::stringstream ss;
                ss << "Shading rate attachment texture " << Utils::DebugNameToString(d.debugName)
                   << " must be created with isShadingRateSurface = true";
                Error(ss.str());
                return nullptr;
            }
        }

        return m_Device->CreateFramebuffer(desc);
    }

    static void UpdateBindingSummaryWithLocation(MessageCallback *messageCallback, ResourceType type,
                                                 BindingLocation location, BindingSummary &bindings,
                                                 BindingLocationSet &duplicates)
    {
        switch (type)
        {
        case ResourceType::Texture_SRV:
        case ResourceType::TypedBuffer_SRV:
        case ResourceType::StructuredBuffer_SRV:
        case ResourceType::RawBuffer_SRV:
        case ResourceType::RayTracingAccelStruct:
            location.type = GraphicsResourceType::SRV;
            bindings.rangeSRV.Add(location.slot);
            break;

        case ResourceType::Texture_UAV:
        case ResourceType::TypedBuffer_UAV:
        case ResourceType::StructuredBuffer_UAV:
        case ResourceType::RawBuffer_UAV:
        case ResourceType::SamplerFeedbackTexture_UAV:
            location.type = GraphicsResourceType::UAV;
            bindings.rangeUAV.Add(location.slot);
            break;

        case ResourceType::ConstantBuffer:
        case ResourceType::VolatileConstantBuffer:
        case ResourceType::PushConstants:
            location.type = GraphicsResourceType::CB;
            bindings.rangeCB.Add(location.slot);
            if (type == ResourceType::VolatileConstantBuffer)
            {
                ++bindings.numVolatileCBs;
            }
            break;

        case ResourceType::Sampler:
            location.type = GraphicsResourceType::Sampler;
            bindings.rangeSampler.Add(location.slot);
            break;

        case ResourceType::None:
        case ResourceType::Count:
        default:
        {
            std::stringstream ss;
            ss << "Invalid layout item type " << (int)type;
            messageCallback->Message(MessageSeverity::Error, ss.str().c_str());
            break;
        }
        }

        if (bindings.locations.find(location) != bindings.locations.end())
        {
            duplicates.insert(location);
        }
        else
        {
            bindings.locations.insert(location);
        }
    }

    static void FillBindingLayoutSummary(MessageCallback *messageCallback, BindingLayoutDesc const &desc,
                                         bool ignoreRegisterSpaces, BindingSummary &bindings,
                                         BindingLocationSet &duplicates)
    {
        for (const auto &item : desc.bindings)
        {
            BindingLocation location;
            location.registerSpace = ignoreRegisterSpaces ? 0 : desc.registerSpace;
            location.slot = item.slot;
            uint32_t arraySize = item.GetArraySize();
            for (location.arrayElement = 0; location.arrayElement < arraySize; ++location.arrayElement)
            {
                UpdateBindingSummaryWithLocation(messageCallback, item.type, location, bindings, duplicates);
            }
        }
    }

    static void FillBindingSetSummary(MessageCallback *messageCallback, BindingSetDesc const &desc,
                                      bool ignoreRegisterSpaces, uint32_t registerSpace, BindingSummary &bindings,
                                      BindingLocationSet &duplicates)
    {
        for (const auto &item : desc.bindings)
        {
            BindingLocation location;
            location.registerSpace = ignoreRegisterSpaces ? 0 : registerSpace;
            location.slot = item.slot;
            location.arrayElement = item.arrayElement;
            UpdateBindingSummaryWithLocation(messageCallback, item.type, location, bindings, duplicates);
        }
    }

    template <typename ItemType, typename DescType>
    static const ItemType *SelectShaderStage(const DescType &desc, ShaderType stage)
    {
        switch (stage)
        {
        case ShaderType::Vertex:
            return &desc.VS;
        case ShaderType::Hull:
            return &desc.HS;
        case ShaderType::Domain:
            return &desc.DS;
        case ShaderType::Geometry:
            return &desc.GS;
        case ShaderType::Pixel:
            return &desc.PS;
        case ShaderType::Compute:
            return &desc.CS;
        default:
            Utils::InvalidEnum();
            return nullptr;
        }
    }

    template <typename ItemType, typename DescType>
    static const ItemType *SelectGraphicsShaderStage(const DescType &desc, ShaderType stage)
    {
        switch (stage) // NOLINT(clang-diagnostic-switch-enum)
        {
        case ShaderType::Vertex:
            return &desc.VS;
        case ShaderType::Hull:
            return &desc.HS;
        case ShaderType::Domain:
            return &desc.DS;
        case ShaderType::Geometry:
            return &desc.GS;
        case ShaderType::Pixel:
            return &desc.PS;
        default:
            Utils::InvalidEnum();
            return nullptr;
        }
    }

    template <typename ItemType, typename DescType>
    static const ItemType *SelectMeshletShaderStage(const DescType &desc, ShaderType stage)
    {
        switch (stage) // NOLINT(clang-diagnostic-switch-enum)
        {
        case ShaderType::Amplification:
            return &desc.AS;
        case ShaderType::Mesh:
            return &desc.MS;
        case ShaderType::Pixel:
            return &desc.PS;
        default:
            Utils::InvalidEnum();
            return nullptr;
        }
    }

    static const ShaderType GraphicsShaderStages[] = {ShaderType::Vertex, ShaderType::Hull, ShaderType::Domain,
                                                      ShaderType::Geometry, ShaderType::Pixel};

    static const ShaderType MeshletShaderStages[] = {ShaderType::Amplification, ShaderType::Mesh, ShaderType::Pixel};

    bool DeviceWrapper::ValidatePipelineBindingLayouts(
        const StaticVector<BindingLayoutHandle, MaxBindingLayouts> &bindingLayouts,
        const std::vector<Shader *> &shaders) const
    {
        for (const auto &layout : bindingLayouts)
        {
            if (!layout)
            {
                Error("InvalidArgument: null pipeline binding layout");
                return false;
            }
            if (const auto *desc = layout->GetDesc())
            {
                if (!Input::CheckBindingLayoutDesc(m_Device, *desc))
                {
                    return false;
                }
            }
            else if (Profile::RejectOpenGL(m_Device))
            {
                return false;
            }
        }
        const int numBindingLayouts = int(bindingLayouts.size());
        bool anyErrors = false;
        bool anyDuplicateBindings = false;
        bool anyOverlappingBindings = false;
        std::stringstream ssDuplicateBindings;
        std::stringstream ssOverlappingBindings;

        bool const ignoreRegisterSpaces = (m_Device->GetGraphicsAPI() == GraphicsAPI::D3D11);

        for (Shader *shader : shaders)
        {
            ShaderType stage = shader->GetDesc().shaderType;

            StaticVector<BindingSummary, MaxBindingLayouts> bindingsPerLayout;
            bindingsPerLayout.resize(numBindingLayouts);

            // Accumulate binding information about the stage from all layouts

            for (int layoutIndex = 0; layoutIndex < numBindingLayouts; layoutIndex++)
            {
                if (bindingLayouts[layoutIndex] == nullptr)
                {
                    std::stringstream ss;
                    ss << "Binding layout in slot " << layoutIndex << " is NULL";
                    Error(ss.str());
                    anyErrors = true;
                }
                else
                {
                    const BindingLayoutDesc *layoutDesc = bindingLayouts[layoutIndex]->GetDesc();

                    if (layoutDesc)
                    {
                        if (!(layoutDesc->visibility & stage))
                        {
                            continue;
                        }

                        BindingLocationSet duplicates;
                        FillBindingLayoutSummary(m_MessageCallback, *layoutDesc, ignoreRegisterSpaces,
                                                 bindingsPerLayout[layoutIndex], duplicates);

                        // Layouts with duplicates should not have passed validation in createBindingLayout
                        assert(duplicates.empty());
                    }
                }
            }

            // Check for multiple layouts declaring the same bindings

            if (numBindingLayouts > 1)
            {
                BindingSummary bindings = bindingsPerLayout[0];
                BindingSummary duplicates;

                for (int layoutIndex = 1; layoutIndex < numBindingLayouts; layoutIndex++)
                {
                    SetUnionInplace(duplicates.locations,
                                    SetIntersection(bindings.locations, bindingsPerLayout[layoutIndex].locations));
                    SetUnionInplace(bindings.locations, bindingsPerLayout[layoutIndex].locations);
                }

                if (duplicates.Any())
                {
                    if (!anyDuplicateBindings)
                    {
                        ssDuplicateBindings << "Same bindings defined by more than one layout in this pipeline:";
                    }

                    ssDuplicateBindings << std::endl
                                        << Utils::ShaderStageToString(stage) << ": " << duplicates.locations;

                    anyDuplicateBindings = true;
                }
                else if (m_Device->GetGraphicsAPI() == GraphicsAPI::D3D11)
                {
                    // Check for overlapping layouts on DX11, because the backend implements each binding set as a single
                    // call to a function like PSSetShaderResources. If binding sets overlap, a set with higher index
                    // will overwrite bindings from the lower-indexed sets, even if they are on different slots.
                    // Do this only when there are no duplicates, as with duplicates the layouts will always overlap.

                    bool overlapSRV = false;
                    bool overlapSampler = false;
                    bool overlapUAV = false;
                    bool overlapCB = false;

                    for (int i = 0; i < numBindingLayouts - 1; i++)
                    {
                        const BindingSummary &set1 = bindingsPerLayout[i];

                        for (int j = i + 1; j < numBindingLayouts; j++)
                        {
                            const BindingSummary &set2 = bindingsPerLayout[j];

                            overlapSRV = overlapSRV || set1.rangeSRV.OverlapsWith(set2.rangeSRV);
                            overlapSampler = overlapSampler || set1.rangeSampler.OverlapsWith(set2.rangeSampler);
                            overlapUAV = overlapUAV || set1.rangeUAV.OverlapsWith(set2.rangeUAV);
                            overlapCB = overlapCB || set1.rangeCB.OverlapsWith(set2.rangeCB);
                        }
                    }

                    if (overlapSRV || overlapSampler || overlapUAV || overlapCB)
                    {
                        if (!anyOverlappingBindings)
                        {
                            ssOverlappingBindings << "Binding layouts have overlapping register ranges:";
                        }

                        ssOverlappingBindings << std::endl << Utils::ShaderStageToString(stage) << ": ";

                        bool first = true;
                        auto append = [&first, &ssOverlappingBindings](bool value, const char *text)
                        {
                            if (value)
                            {
                                if (!first)
                                {
                                    ssOverlappingBindings << ", ";
                                }
                                ssOverlappingBindings << text;
                                first = false;
                            }
                        };

                        append(overlapSRV, "SRV");
                        append(overlapSampler, "Sampler");
                        append(overlapUAV, "UAV");
                        append(overlapCB, "CB");

                        anyOverlappingBindings = true;
                    }
                }
            }
        }

        if (anyDuplicateBindings)
        {
            Error(ssDuplicateBindings.str());
            anyErrors = true;
        }

        if (anyOverlappingBindings)
        {
            Error(ssOverlappingBindings.str());
            anyErrors = true;
        }

        int pushConstantCount = 0;
        uint32_t pushConstantSize = 0;
        enum class RegisterSpaceIsDescriptorSet
        {
            False,
            True,
            Undetermined,
            Mixed
        } registerSpaceIsDescriptorSet = RegisterSpaceIsDescriptorSet::Undetermined;
        StaticVector<int, MaxBindingLayouts> registerSpaceToLayoutIdx(MaxBindingLayouts);
        registerSpaceToLayoutIdx.fill(-1);

        for (int layoutIndex = 0; layoutIndex < numBindingLayouts; layoutIndex++)
        {
            const BindingLayoutDesc *layoutDesc = bindingLayouts[layoutIndex]->GetDesc();
            if (layoutDesc)
            {
                for (const auto &item : layoutDesc->bindings)
                {
                    if (item.type == ResourceType::PushConstants)
                    {
                        pushConstantCount++;
                        pushConstantSize = std::max(pushConstantSize, uint32_t(item.size));
                    }
                }

                if (layoutDesc->registerSpaceIsDescriptorSet)
                {
                    if (layoutDesc->registerSpace >= MaxBindingLayouts)
                    {
                        std::stringstream errorStream;
                        errorStream << "Binding layout at index " << layoutIndex
                                    << " has registerSpace = " << layoutDesc->registerSpace
                                    << ". Largest supported registerSpace index is " << (MaxBindingLayouts - 1);
                        Error(errorStream.str());
                        anyErrors = true;
                    }
                    else if (registerSpaceToLayoutIdx[layoutDesc->registerSpace] != -1)
                    {
                        std::stringstream errorStream;
                        errorStream << "Binding layout at index " << layoutIndex
                                    << " has registerSpace = " << layoutDesc->registerSpace
                                    << ". That register space has already been used in layout index "
                                    << registerSpaceToLayoutIdx[layoutDesc->registerSpace];
                        Error(errorStream.str());
                        anyErrors = true;
                    }
                    registerSpaceToLayoutIdx[layoutDesc->registerSpace] = layoutIndex;
                    if (registerSpaceIsDescriptorSet == RegisterSpaceIsDescriptorSet::Undetermined)
                    {
                        registerSpaceIsDescriptorSet = RegisterSpaceIsDescriptorSet::True;
                    }
                    else if (registerSpaceIsDescriptorSet == RegisterSpaceIsDescriptorSet::False)
                    {
                        registerSpaceIsDescriptorSet = RegisterSpaceIsDescriptorSet::Mixed;
                    }
                }
                else
                {
                    if (registerSpaceIsDescriptorSet == RegisterSpaceIsDescriptorSet::Undetermined)
                    {
                        registerSpaceIsDescriptorSet = RegisterSpaceIsDescriptorSet::False;
                    }
                    else if (registerSpaceIsDescriptorSet == RegisterSpaceIsDescriptorSet::True)
                    {
                        registerSpaceIsDescriptorSet = RegisterSpaceIsDescriptorSet::Mixed;
                    }
                }
            }
        }
        if (registerSpaceIsDescriptorSet == RegisterSpaceIsDescriptorSet::Mixed)
        {
            Error("Pipeline contains Binding layouts with differing values of `registerSpaceIsDescriptorSet`");
            anyErrors = true;
        }

        if (pushConstantCount > 1)
        {
            std::stringstream errorStream;
            errorStream << "Binding layout contains more than one (" << pushConstantCount << ") push constant blocks";
            Error(errorStream.str());
            anyErrors = true;
        }

        if (pushConstantSize > MaxPushConstantSize)
        {
            std::stringstream errorStream;
            errorStream << "Binding layout declares " << pushConstantSize
                        << " bytes of push constant data, "
                           "which exceeds the limit of "
                        << MaxPushConstantSize << " bytes";
            Error(errorStream.str());
            anyErrors = true;
        }

        return !anyErrors;
    }

    bool DeviceWrapper::ValidateShaderType(ShaderType expected, const ShaderDesc &shaderDesc,
                                           const char *function) const
    {
        if (Profile::IsOpenGL(m_Device) && Profile::HasExcludedShaderExtensions(shaderDesc))
        {
            Profile::RejectOpenGL(m_Device);
            return false;
        }
        if (expected == shaderDesc.shaderType)
        {
            return true;
        }

        std::stringstream ss;
        ss << "Unexpected shader type used in " << function
           << ": expected shaderType = " << Utils::ShaderStageToString(expected)
           << ", actual shaderType = " << Utils::ShaderStageToString(shaderDesc.shaderType) << " in "
           << Utils::DebugNameToString(shaderDesc.debugName) << ":" << shaderDesc.entryName;
        Error(ss.str());
        return false;
    }

    bool DeviceWrapper::ValidateRenderState(const RenderState &renderState, FramebufferInfo const &fbinfo) const
    {
        if (fbinfo.sampleCount != 1 && fbinfo.sampleCount != 2 && fbinfo.sampleCount != 4 && fbinfo.sampleCount != 8)
        {
            Error("InvalidArgument: invalid framebuffer sample count");
            return false;
        }
        if (Profile::IsOpenGL(m_Device) && fbinfo.sampleQuality != 0)
        {
            Profile::RejectOpenGL(m_Device);
            return false;
        }
        const auto &raster = renderState.rasterState;
        const auto &depth = renderState.depthStencilState;
        if (raster.fillMode > RasterFillMode::Wireframe || raster.cullMode > RasterCullMode::None ||
            !std::isfinite(raster.depthBiasClamp) || !std::isfinite(raster.slopeScaledDepthBias) ||
            depth.depthFunc < ComparisonFunc::Never || depth.depthFunc > ComparisonFunc::Always)
        {
            Error("InvalidArgument: invalid raster/depth state");
            return false;
        }
        for (const auto &face : {depth.frontFaceStencil, depth.backFaceStencil})
        {
            if (face.failOp < StencilOp::Keep || face.failOp > StencilOp::DecrementAndWrap ||
                face.depthFailOp < StencilOp::Keep || face.depthFailOp > StencilOp::DecrementAndWrap ||
                face.passOp < StencilOp::Keep || face.passOp > StencilOp::DecrementAndWrap ||
                face.stencilFunc < ComparisonFunc::Never || face.stencilFunc > ComparisonFunc::Always)
            {
                Error("InvalidArgument: invalid stencil state");
                return false;
            }
        }
        if (fbinfo.depthFormat != Format::UNKNOWN &&
            (fbinfo.depthFormat >= Format::COUNT || GetFormatInfo(fbinfo.depthFormat).kind != FormatKind::DepthStencil))
        {
            Error("InvalidArgument: invalid framebuffer depth format");
            return false;
        }
        auto validFactor = [](BlendFactor factor)
        {
            const auto value = uint8_t(factor);
            return (value >= 1 && value <= 11) || (value >= 14 && value <= 19);
        };
        for (size_t index = 0; index < fbinfo.colorFormats.size(); ++index)
        {
            const auto format = fbinfo.colorFormats[index];
            if (format != Format::UNKNOWN &&
                (format >= Format::COUNT || !GetFormatInfo(format).hasRed || GetFormatInfo(format).blockSize != 1))
            {
                Error("InvalidArgument: invalid framebuffer color format");
                return false;
            }
            const auto &target = renderState.blendState.targets[index];
            if (!validFactor(target.srcBlend) || !validFactor(target.destBlend) || !validFactor(target.srcBlendAlpha) ||
                !validFactor(target.destBlendAlpha) || target.blendOp < BlendOp::Add || target.blendOp > BlendOp::Max ||
                target.blendOpAlpha < BlendOp::Add || target.blendOpAlpha > BlendOp::Max ||
                (uint8_t(target.colorWriteMask) & ~0xfu) != 0)
            {
                Error("InvalidArgument: invalid blend state");
                return false;
            }
            if (format != Format::UNKNOWN)
            {
                const auto required = target.blendEnable ? (FormatSupport::RenderTarget | FormatSupport::Blendable)
                                                         : FormatSupport::RenderTarget;
                if ((m_Device->QueryFormatSupport(format) & required) != required)
                {
                    Error("Unsupported: framebuffer color/blend format usage");
                    return false;
                }
            }
        }

        if (renderState.depthStencilState.depthTestEnable || renderState.depthStencilState.stencilEnable)
        {
            if (fbinfo.depthFormat == Format::UNKNOWN)
            {
                Error("The depth-stencil state indicates that depth or stencil operations are used, "
                      "but the framebuffer info has no depth format.");
                return false;
            }
        }

        if (renderState.rasterState.conservativeRasterEnable &&
            !Profile::QueryFeatureSupport(m_Device, Feature::ConservativeRasterization))
        {
            Warning("Conservative rasterization is not supported on this device");
            return false;
        }

        return true;
    }

    GraphicsPipelineHandle DeviceWrapper::CreateGraphicsPipeline(const GraphicsPipelineDesc &pipelineDesc,
                                                                 FramebufferInfo const &fbinfo)
    {
        if (Profile::RejectGraphicsPipeline(m_Device, pipelineDesc))
        {
            return nullptr;
        }

        if (pipelineDesc.primType > PrimitiveType::PatchList ||
            (pipelineDesc.primType == PrimitiveType::PatchList && pipelineDesc.patchControlPoints == 0))
        {
            Error("InvalidArgument: invalid primitive/patch count");
            return nullptr;
        }
        if (!pipelineDesc.VS)
        {
            Error("InvalidArgument: graphics pipeline requires a vertex shader");
            return nullptr;
        }
        std::vector<Shader *> shaders;

        for (ShaderType stage : GraphicsShaderStages)
        {
            Shader *shader = *SelectGraphicsShaderStage<ShaderHandle, GraphicsPipelineDesc>(pipelineDesc, stage);
            if (shader)
            {
                shaders.push_back(shader);

                if (!ValidateShaderType(stage, shader->GetDesc(), "createGraphicsPipeline"))
                {
                    return nullptr;
                }
            }
        }

        if (!ValidatePipelineBindingLayouts(pipelineDesc.bindingLayouts, shaders))
        {
            return nullptr;
        }

        if (!ValidateRenderState(pipelineDesc.renderState, fbinfo))
        {
            return nullptr;
        }

        return m_Device->CreateGraphicsPipeline(pipelineDesc, fbinfo);
    }

    GraphicsPipelineHandle DeviceWrapper::CreateGraphicsPipeline(const GraphicsPipelineDesc &pipelineDesc,
                                                                 Framebuffer *fb)
    {
        if (Profile::RejectGraphicsPipeline(m_Device, pipelineDesc))
        {
            return nullptr;
        }

        if (!fb)
        {
            Error("framebuffer is NULL");
            return nullptr;
        }

        return CreateGraphicsPipeline(pipelineDesc, fb->GetFramebufferInfo());
    }

    ComputePipelineHandle DeviceWrapper::CreateComputePipeline(const ComputePipelineDesc &pipelineDesc)
    {
        if (Profile::RejectComputePipeline(m_Device, pipelineDesc))
        {
            return nullptr;
        }
        if (!pipelineDesc.CS)
        {
            Error("createComputePipeline: CS = NULL");
            return nullptr;
        }

        std::vector<Shader *> shaders = {pipelineDesc.CS};

        if (!ValidatePipelineBindingLayouts(pipelineDesc.bindingLayouts, shaders))
        {
            return nullptr;
        }

        if (!ValidateShaderType(ShaderType::Compute, pipelineDesc.CS->GetDesc(), "createComputePipeline"))
        {
            return nullptr;
        }

        return m_Device->CreateComputePipeline(pipelineDesc);
    }

    MeshletPipelineHandle DeviceWrapper::CreateMeshletPipeline(const MeshletPipelineDesc &pipelineDesc,
                                                               FramebufferInfo const &fbinfo)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return nullptr;
        }

        std::vector<Shader *> shaders;

        for (ShaderType stage : MeshletShaderStages)
        {
            Shader *shader = *SelectMeshletShaderStage<ShaderHandle, MeshletPipelineDesc>(pipelineDesc, stage);
            if (shader)
            {
                shaders.push_back(shader);

                if (!ValidateShaderType(stage, shader->GetDesc(), "createMeshletPipeline"))
                {
                    return nullptr;
                }
            }
        }

        if (!ValidatePipelineBindingLayouts(pipelineDesc.bindingLayouts, shaders))
        {
            return nullptr;
        }

        if (!ValidateRenderState(pipelineDesc.renderState, fbinfo))
        {
            return nullptr;
        }

        return m_Device->CreateMeshletPipeline(pipelineDesc, fbinfo);
    }

    MeshletPipelineHandle DeviceWrapper::CreateMeshletPipeline(const MeshletPipelineDesc &pipelineDesc, Framebuffer *fb)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return nullptr;
        }

        if (!fb)
        {
            Error("framebuffer is NULL");
            return nullptr;
        }

        return CreateMeshletPipeline(pipelineDesc, fb->GetFramebufferInfo());
    }

    Physara::RHI::RT::PipelineHandle DeviceWrapper::CreateRayTracingPipeline(const RT::PipelineDesc &desc)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return nullptr;
        }

        return m_Device->CreateRayTracingPipeline(desc);
    }

    BindingLayoutHandle DeviceWrapper::CreateBindingLayout(const BindingLayoutDesc &desc)
    {

        if (!Input::CheckBindingLayoutDesc(m_Device, desc))
        {
            return nullptr;
        }
        return m_Device->CreateBindingLayout(desc);
    }

    BindingLayoutHandle DeviceWrapper::CreateBindlessLayout(const BindlessLayoutDesc &desc)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return nullptr;
        }

        std::stringstream errorStream;
        bool anyErrors = false;

        if (desc.visibility == ShaderType::None)
        {
            errorStream << "Cannot create a bindless layout with visibility = None" << std::endl;
            anyErrors = true;
        }

        if (desc.registerSpaces.empty() && desc.layoutType == BindlessLayoutDesc::LayoutType::Immutable)
        {
            errorStream << "Bindless layout has no register spaces assigned" << std::endl;
            anyErrors = true;
        }

        if (!desc.registerSpaces.empty() && desc.layoutType != BindlessLayoutDesc::LayoutType::Immutable)
        {
            errorStream << "Bindless mutable layout has register spaces assigned" << std::endl;
            anyErrors = true;
        }

        if (desc.maxCapacity == 0)
        {
            errorStream << "Bindless layout has maxCapacity = 0" << std::endl;
            anyErrors = true;
        }

        for (const BindingLayoutItem &item : desc.registerSpaces)
        {
            switch (item.type)
            {
            case ResourceType::Texture_SRV:
            case ResourceType::TypedBuffer_SRV:
            case ResourceType::StructuredBuffer_SRV:
            case ResourceType::RawBuffer_SRV:
            case ResourceType::RayTracingAccelStruct:
            case ResourceType::ConstantBuffer:
            case ResourceType::Texture_UAV:
            case ResourceType::TypedBuffer_UAV:
            case ResourceType::StructuredBuffer_UAV:
            case ResourceType::RawBuffer_UAV:
                continue;
            case ResourceType::VolatileConstantBuffer:
                errorStream << "Volatile CBs cannot be placed into a bindless layout (slot " << item.slot << ")"
                            << std::endl;
                anyErrors = true;
                break;
            case ResourceType::Sampler:
                errorStream << "Bindless samplers are not implemented (slot " << item.slot << ")" << std::endl;
                anyErrors = true;
                break;
            case ResourceType::PushConstants:
                errorStream << "Push constants cannot be placed into a bindless layout (slot " << item.slot << ")"
                            << std::endl;
                anyErrors = true;
                break;

            case ResourceType::None:
            case ResourceType::Count:
            default:
                errorStream << "Invalid resource type " << int(item.type) << " in slot " << item.slot << std::endl;
                anyErrors = true;
                break;
            }
        }

        if (anyErrors)
        {
            Error(errorStream.str());
            return nullptr;
        }

        return m_Device->CreateBindlessLayout(desc);
    }

    static bool TextureDimensionsCompatible(TextureDimension resourceDimension, TextureDimension viewDimension)
    {
        if (resourceDimension == viewDimension)
        {
            return true;
        }

        if (resourceDimension == TextureDimension::Texture3D)
        {
            return viewDimension == TextureDimension::Texture2DArray;
        }

        if (resourceDimension == TextureDimension::TextureCube ||
            resourceDimension == TextureDimension::TextureCubeArray)
        {
            return viewDimension == TextureDimension::Texture2DArray;
        }

        return false;
    }

    bool DeviceWrapper::ValidateBindingSetItem(const BindingSetItem &binding, DescriptorTable *pOptDescriptorTable,
                                               std::stringstream &errorStream, std::source_location location)
    {

        if (binding.type >= ResourceType::Count || binding.format >= Format::COUNT ||
            binding.dimension > TextureDimension::Texture3D)
        {
            errorStream << "Invalid binding enum";
            return false;
        }
        if ((binding.overrideComponentMapping & ~uint16_t(ComponentMappingExplicit | 0x0fffu)) != 0)
        {
            errorStream << "Invalid component mapping bits";
            return false;
        }
        if (binding.overrideComponentMapping & ComponentMappingExplicit)
        {
            const auto mapping = ComponentMapping::Unpack(binding.overrideComponentMapping);
            if (mapping.r > ComponentSwizzle::One || mapping.g > ComponentSwizzle::One ||
                mapping.b > ComponentSwizzle::One || mapping.a > ComponentSwizzle::One)
            {
                errorStream << "Invalid component swizzle";
                return false;
            }
        }
        bool isDescriptorTable = pOptDescriptorTable != nullptr;
        BindingLayout *pBindingLayout = pOptDescriptorTable ? pOptDescriptorTable->GetLayout() : nullptr;
        const BindlessLayoutDesc *pBindlessLayoutDesc = pBindingLayout ? pBindingLayout->GetBindlessDesc() : nullptr;

        if (pBindlessLayoutDesc)
        {
            if (pBindlessLayoutDesc->layoutType == BindlessLayoutDesc::LayoutType::MutableSrvUavCbv)
            {
                switch (binding.type)
                {
                case ResourceType::None:
                case ResourceType::Texture_SRV:
                case ResourceType::Texture_UAV:
                case ResourceType::TypedBuffer_SRV:
                case ResourceType::TypedBuffer_UAV:
                case ResourceType::StructuredBuffer_SRV:
                case ResourceType::StructuredBuffer_UAV:
                case ResourceType::RawBuffer_SRV:
                case ResourceType::RawBuffer_UAV:
                case ResourceType::ConstantBuffer:
                    // Valid descriptors
                    break;
                case ResourceType::Sampler:
                    errorStream
                        << "ResourceType::Sampler is not allowed for BindlessLayoutDesc::LayoutType::MutableSrvUavCbv."
                        << std::endl;
                    return false;
                case ResourceType::SamplerFeedbackTexture_UAV:
                    errorStream << "ResourceType::SamplerFeedbackTexture_UAV is not allowed for bindless layouts with "
                                   "LayoutType::MutableSrvUavCbv."
                                << std::endl;
                    return false;
                case ResourceType::VolatileConstantBuffer:
                    errorStream << "ResourceType::VolatileConstantBuffer is not allowed for bindless layouts with "
                                   "LayoutType::MutableSrvUavCbv."
                                << std::endl;
                    return false;
                case ResourceType::PushConstants:
                    errorStream << "ResourceType::PushConstants is not allowed for bindless layouts with "
                                   "LayoutType::MutableSrvUavCbv."
                                << std::endl;
                    return false;
                case ResourceType::RayTracingAccelStruct:
                    errorStream << "ResourceType::RayTracingAccelStruct is not allowed for bindless layouts with "
                                   "LayoutType::MutableSrvUavCbv."
                                << std::endl;
                    return false;
                default:
                    // Invalid resource types will be dealt with later in this function.
                    break;
                }
            }
            else if (pBindlessLayoutDesc->layoutType == BindlessLayoutDesc::LayoutType::MutableCounters)
            {
                if (binding.type != ResourceType::StructuredBuffer_UAV)
                {
                    errorStream << "Only ResourceType::StructuredBuffer_UAV can be used for bindless layouts with "
                                   "LayoutType::MutableCounters."
                                << std::endl;
                    return false;
                }
            }
            else if (pBindlessLayoutDesc->layoutType == BindlessLayoutDesc::LayoutType::MutableSampler)
            {
                if (binding.type != ResourceType::Sampler)
                {
                    errorStream << "Only ResourceType::Sampler can be used for bindless layouts with "
                                   "LayoutType::MutableSampler."
                                << std::endl;
                    return false;
                }
            }
        }

        if ((binding.overrideComponentMapping & ComponentMappingExplicit) && binding.type != ResourceType::Texture_SRV)
        {
            errorStream << "Component mappings are only supported on ResourceType::Texture_SRV bindings." << std::endl;
            return false;
        }

        switch (binding.type)
        {
        case ResourceType::None:
            if (!isDescriptorTable)
            {
                errorStream << "ResourceType::None bindings are not allowed in binding sets." << std::endl;
                return false;
            }
            break;

        case ResourceType::Texture_SRV:
        case ResourceType::Texture_UAV:
        {
            Texture *texture = dynamic_cast<Texture *>(binding.resourceHandle);

            if (texture == nullptr)
            {
                errorStream << "Null resource bindings are not allowed for textures." << std::endl;
                return false;
            }

            if (!Input::CheckTextureRange(m_Device, texture, binding.subresources, false, location))
            {
                return false;
            }
            const auto &textureDesc = texture->GetDesc();
            if (!Input::CheckViewFormat(m_Device, textureDesc, binding.format,
                                        binding.type == ResourceType::Texture_UAV ? FormatSupport::ShaderUavStore
                                                                                  : FormatSupport::Texture,
                                        location))
            {
                return false;
            }
            if (binding.type == ResourceType::Texture_SRV)
            {
                const Format view = binding.format == Format::UNKNOWN ? textureDesc.format : binding.format;
                if (!textureDesc.isShaderResource ||
                    !(QueryFormatSupport(view) & (FormatSupport::ShaderLoad | FormatSupport::ShaderSample)))
                {
                    errorStream << "Texture lacks SRV usage/format support";
                    return false;
                }
            }
            if (binding.type == ResourceType::Texture_UAV &&
                binding.subresources.Resolve(textureDesc, false).numMipLevels != 1)
            {
                errorStream << "Texture UAV must select exactly one mip";
                return false;
            }
            const TextureDesc &desc = texture->GetDesc();

            TextureSubresourceSet subresources = binding.subresources.Resolve(desc, false);
            if (subresources.numArraySlices == 0 || subresources.numMipLevels == 0)
            {
                errorStream << "The specified subresource set (BaseMipLevel = " << binding.subresources.baseMipLevel
                            << ", NumMipLevels = " << binding.subresources.numMipLevels
                            << ", BaseArraySlice = " << binding.subresources.baseArraySlice
                            << ", NumArraySlices = " << binding.subresources.numArraySlices
                            << ") does not intersect with the texture being bound ("
                            << Utils::DebugNameToString(desc.debugName) << ", MipLevels = " << desc.mipLevels
                            << ", ArraySize = " << desc.arraySize << ")" << std::endl;

                return false;
            }

            if ((binding.type == ResourceType::Texture_UAV) && !desc.isUAV)
            {
                errorStream << "Texture " << Utils::DebugNameToString(desc.debugName)
                            << " cannot be used as a UAV because it does not have the isUAV flag set." << std::endl;
                return false;
            }

            if (binding.dimension != TextureDimension::Unknown)
            {
                if (!TextureDimensionsCompatible(desc.dimension, binding.dimension))
                {
                    errorStream << "Requested binding dimension (" << Utils::TextureDimensionToString(binding.dimension)
                                << ") "
                                   "is incompatible with the dimension ("
                                << Utils::TextureDimensionToString(desc.dimension)
                                << ") "
                                   "of texture "
                                << Utils::DebugNameToString(desc.debugName) << std::endl;
                    return false;
                }
            }

            break;
        }

        case ResourceType::SamplerFeedbackTexture_UAV:
            // 来源允许 NULL; 非空对象仍须具有匹配的活对象类型.
            if (binding.resourceHandle && !dynamic_cast<SamplerFeedbackTexture *>(binding.resourceHandle))
            {
                errorStream << "Incompatible sampler feedback resource type";
                return false;
            }
            break;

        case ResourceType::TypedBuffer_SRV:
        case ResourceType::TypedBuffer_UAV:
        case ResourceType::StructuredBuffer_SRV:
        case ResourceType::StructuredBuffer_UAV:
        case ResourceType::RawBuffer_SRV:
        case ResourceType::RawBuffer_UAV:
        case ResourceType::ConstantBuffer:
        case ResourceType::VolatileConstantBuffer:
        {
            Buffer *buffer = dynamic_cast<Buffer *>(binding.resourceHandle);
            if (binding.resourceHandle && !buffer)
            {
                errorStream << "Buffer binding carries an incompatible live resource type";
                return false;
            }
            if (!buffer && Profile::IsOpenGL(m_Device))
            {
                errorStream << "OpenGL buffer binding requires a resource";
                return false;
            }

            if (buffer == nullptr && binding.type != ResourceType::TypedBuffer_SRV &&
                binding.type != ResourceType::TypedBuffer_UAV && m_Device->GetGraphicsAPI() != GraphicsAPI::VULKAN)
            {
                errorStream << "Null resource bindings are not allowed for buffers, unless it's a "
                               "TypedBuffer_SRV or TypedBuffer_UAV type binding on DX11 or DX12."
                            << std::endl;
                return false;
            }

            if (buffer == nullptr)
            {
                return true;
            }

            if (!Input::CheckBufferDesc(m_Device, buffer->GetDesc(), false, location))
            {
                return false;
            }
            const BufferDesc &desc = buffer->GetDesc();

            bool isTypedView =
                (binding.type == ResourceType::TypedBuffer_SRV) || (binding.type == ResourceType::TypedBuffer_UAV);
            bool isStructuredView = (binding.type == ResourceType::StructuredBuffer_SRV) ||
                                    (binding.type == ResourceType::StructuredBuffer_UAV);
            bool isRawView =
                (binding.type == ResourceType::RawBuffer_SRV) || (binding.type == ResourceType::RawBuffer_UAV);
            bool isUAV = (binding.type == ResourceType::TypedBuffer_UAV) ||
                         (binding.type == ResourceType::StructuredBuffer_UAV) ||
                         (binding.type == ResourceType::RawBuffer_UAV);
            bool isConstantView = (binding.type == ResourceType::ConstantBuffer) ||
                                  (binding.type == ResourceType::VolatileConstantBuffer);

            const auto resolvedRange = binding.range.Resolve(desc);
            if (resolvedRange.byteSize == 0)
            {
                errorStream << "Empty buffer view range";
                return false;
            }
            uint64_t alignment = isRawView ? 4u : (isStructuredView ? desc.structStride : 1u);
            if (isTypedView)
            {
                const Format view = binding.format == Format::UNKNOWN ? desc.format : binding.format;
                if (view == Format::UNKNOWN || view >= Format::COUNT)
                {
                    errorStream << "Invalid typed buffer view format";
                    return false;
                }
                const auto &format = GetFormatInfo(view);
                if (format.blockSize != 1 || format.bytesPerBlock == 0 || format.kind == FormatKind::DepthStencil)
                {
                    errorStream << "Incompatible typed buffer view format";
                    return false;
                }
                const auto required =
                    FormatSupport::Buffer | (isUAV ? FormatSupport::ShaderUavStore : FormatSupport::ShaderLoad);
                if ((QueryFormatSupport(view) & required) != required)
                {
                    errorStream << "Typed buffer format lacks required device support";
                    return false;
                }
                alignment = format.bytesPerBlock;
            }
            if (alignment == 0 || resolvedRange.byteOffset % alignment != 0 || resolvedRange.byteSize % alignment != 0)
            {
                errorStream << "Buffer view range is not aligned to its element layout";
                return false;
            }
            if (isTypedView && !desc.canHaveTypedViews)
            {
                errorStream << "Cannot bind buffer " << Utils::DebugNameToString(desc.debugName) << " as "
                            << Utils::ResourceTypeToString(binding.type)
                            << " because it doesn't support typed views (BufferDesc::canHaveTypedViews)." << std::endl;
                return false;
            }

            if (isStructuredView && desc.structStride == 0)
            {
                errorStream << "Cannot bind buffer " << Utils::DebugNameToString(desc.debugName) << " as "
                            << Utils::ResourceTypeToString(binding.type)
                            << " because it doesn't have structStride specified at creation." << std::endl;
                return false;
            }

            if (isRawView && !desc.canHaveRawViews)
            {
                errorStream << "Cannot bind buffer " << Utils::DebugNameToString(desc.debugName) << " as "
                            << Utils::ResourceTypeToString(binding.type)
                            << " because it doesn't support raw views (BufferDesc::canHaveRawViews)." << std::endl;
                return false;
            }

            if (isUAV && !desc.canHaveUAVs)
            {
                errorStream << "Cannot bind buffer " << Utils::DebugNameToString(desc.debugName) << " as "
                            << Utils::ResourceTypeToString(binding.type)
                            << " because it doesn't support unordeded access views (BufferDesc::canHaveUAVs)."
                            << std::endl;
                return false;
            }

            if (isConstantView && !desc.isConstantBuffer)
            {
                errorStream << "Cannot bind buffer " << Utils::DebugNameToString(desc.debugName) << " as "
                            << Utils::ResourceTypeToString(binding.type)
                            << " because it doesn't support constant buffer views (BufferDesc::isConstantBuffer)."
                            << std::endl;
                return false;
            }

            if (binding.type == ResourceType::ConstantBuffer && desc.isVolatile)
            {
                errorStream << "Cannot bind buffer " << Utils::DebugNameToString(desc.debugName)
                            << " as a regular ConstantBuffer because it's a VolatileConstantBuffer." << std::endl;
                return false;
            }

            if (binding.type == ResourceType::VolatileConstantBuffer && !desc.isVolatile)
            {
                errorStream << "Cannot bind buffer " << Utils::DebugNameToString(desc.debugName)
                            << " as a VolatileConstantBuffer because it's a regular ConstantBuffer." << std::endl;
                return false;
            }

            if (isTypedView && (binding.format == Format::UNKNOWN && desc.format == Format::UNKNOWN))
            {
                errorStream << "Both binding for typed buffer " << Utils::DebugNameToString(desc.debugName)
                            << " and its BufferDesc have format == UNKNOWN." << std::endl;
                return false;
            }

            if (binding.type == ResourceType::ConstantBuffer && !binding.range.IsEntireBuffer(desc))
            {
                if (!QueryFeatureSupport(Feature::ConstantBufferRanges))
                {
                    errorStream << "Partial binding of constant buffers is not supported by the device (used for "
                                << Utils::DebugNameToString(desc.debugName) << ")";
                    return false;
                }

                const BufferRange range = binding.range.Resolve(desc);
                if ((range.byteOffset % ConstantBufferOffsetSizeAlignment) != 0)
                {
                    errorStream << "Constant buffer offsets must be a multiple of " << ConstantBufferOffsetSizeAlignment
                                << " bytes. Buffer " << Utils::DebugNameToString(desc.debugName)
                                << " is bound with effective byteOffset = " << range.byteOffset << ".";
                    return false;
                }

                if (range.byteSize == 0 || (range.byteSize % ConstantBufferOffsetSizeAlignment) != 0)
                {
                    errorStream << "Constant buffer bindings must have nonzero byteSize that is a multiple of "
                                << ConstantBufferOffsetSizeAlignment << " bytes. Buffer "
                                << Utils::DebugNameToString(desc.debugName)
                                << " is bound with effective byteSize = " << range.byteSize << ".";
                    return false;
                }
            }

            if (binding.type == ResourceType::VolatileConstantBuffer && !binding.range.IsEntireBuffer(desc))
            {
                const BufferRange range = binding.range.Resolve(desc);
                errorStream << "Volatile constant buffers cannot be partially bound. Buffer "
                            << Utils::DebugNameToString(desc.debugName)
                            << " is bound with effective byteOffset = " << range.byteOffset
                            << ", byteSize = " << range.byteSize << ".";
                return false;
            }

            break;
        }

        case ResourceType::Sampler:
            if (binding.resourceHandle && !dynamic_cast<Sampler *>(binding.resourceHandle))
            {
                errorStream << "Sampler binding carries an incompatible live resource type";
                return false;
            }
            if (binding.resourceHandle == nullptr)
            {
                errorStream << "Null resource bindings are not allowed for samplers." << std::endl;
                return false;
            }
            break;

        case ResourceType::RayTracingAccelStruct:
            if (binding.resourceHandle && !dynamic_cast<RT::AccelStruct *>(binding.resourceHandle))
            {
                errorStream << "AccelStruct binding carries an incompatible live resource type";
                return false;
            }
            if (binding.resourceHandle == nullptr)
            {
                errorStream << "Null resource bindings are not allowed for ray tracing acceleration structures."
                            << std::endl;
                return false;
            }
            break;

        case ResourceType::PushConstants:
            if (isDescriptorTable)
            {
                errorStream << "Push constants cannot be used in a descriptor table." << std::endl;
                return false;
            }
            if (binding.resourceHandle != nullptr)
            {
                errorStream << "Push constants cannot have a resource specified." << std::endl;
                return false;
            }
            if (binding.range.byteOffset != 0 || binding.range.byteSize == 0 ||
                binding.range.byteSize > MaxPushConstantSize || binding.range.byteSize % 4 != 0)
            {
                errorStream << "Push constants must have nonzero size specified." << std::endl;
                return false;
            }
            break;

        case ResourceType::Count:
        default:
            errorStream << "Unrecognized resourceType = " << uint32_t(binding.type) << std::endl;
            return false;
        }

        return true;
    }

    bool DeviceWrapper::ValidateBindingSetDesc(const BindingSetDesc &desc, BindingLayout *layout,
                                               std::source_location location)
    {
        if (Profile::IsOpenGL(m_Device))
        {
            for (const auto &item : desc.bindings)
            {
                if (item.type == ResourceType::RayTracingAccelStruct ||
                    item.type == ResourceType::SamplerFeedbackTexture_UAV)
                {
                    Profile::RejectOpenGL(m_Device, location);
                    return false;
                }
            }
        }

        if (layout == nullptr)
        {
            Error("Cannot create a binding set without a valid layout", location);
            return false;
        }

        const BindingLayoutDesc *layoutDesc = layout->GetDesc();
        if (!layoutDesc)
        {
            Error("Cannot create a binding set from a bindless layout", location);
            return false;
        }

        std::stringstream errorStream;
        bool anyErrors = false;
        bool const ignoreRegisterSpaces = (m_Device->GetGraphicsAPI() == GraphicsAPI::D3D11);

        if (!Input::CheckBindingLayoutDesc(m_Device, *layoutDesc, location))
        {
            return false;
        }
        for (const auto &binding : desc.bindings)
        {
            if (!ValidateBindingSetItem(binding, nullptr, errorStream, location))
            {
                if (!errorStream.str().empty())
                {
                    Error(errorStream.str(), location);
                }
                return false;
            }
            bool matches = false;
            for (const auto &item : layoutDesc->bindings)
            {
                if (binding.slot == item.slot && binding.type == item.type &&
                    binding.arrayElement < item.GetArraySize())
                {
                    matches = item.type != ResourceType::PushConstants || binding.range.byteSize == item.size;
                    break;
                }
            }
            if (!matches)
            {
                Error("InvalidArgument: binding type/slot/arrayElement/size does not match layout", location);
                return false;
            }
        }
        BindingSummary layoutBindings;
        BindingLocationSet layoutDuplicates;

        FillBindingLayoutSummary(m_MessageCallback, *layoutDesc, ignoreRegisterSpaces, layoutBindings,
                                 layoutDuplicates);

        BindingSummary setBindings;
        BindingLocationSet setDuplicates;

        FillBindingSetSummary(m_MessageCallback, desc, ignoreRegisterSpaces, layoutDesc->registerSpace, setBindings,
                              setDuplicates);

        BindingLocationSet declaredNotBound;
        BindingLocationSet boundNotDeclared;

        declaredNotBound = SetDifference(layoutBindings.locations, setBindings.locations);
        boundNotDeclared = SetDifference(setBindings.locations, layoutBindings.locations);

        if (!declaredNotBound.empty())
        {
            errorStream << "Bindings declared in the layout are not present in the binding set: " << declaredNotBound
                        << std::endl;
            anyErrors = true;
        }

        if (!boundNotDeclared.empty())
        {
            errorStream << "Bindings in the binding set are not declared in the layout: " << boundNotDeclared
                        << std::endl;
            anyErrors = true;
        }

        if (!setDuplicates.empty())
        {
            errorStream << "Binding set contains duplicate bindings: " << setDuplicates << std::endl;
            anyErrors = true;
        }

        if (anyErrors)
        {
            Error(errorStream.str(), location);
            return false;
        }

        return true;
    }

    BindingSetHandle DeviceWrapper::CreateBindingSet(const BindingSetDesc &desc, BindingLayout *layout)
    {
        if (!ValidateBindingSetDesc(desc, layout))
        {
            return nullptr;
        }
        // Unwrap the resources
        BindingSetDesc patchedDesc = desc;
        for (auto &binding : patchedDesc.bindings)
        {
            binding.resourceHandle = UnwrapResource(binding.resourceHandle);
        }

        return m_Device->CreateBindingSet(patchedDesc, layout);
    }

    DescriptorTableHandle DeviceWrapper::CreateDescriptorTable(BindingLayout *layout)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return nullptr;
        }
        if (!layout)
        {
            Error("InvalidArgument: descriptor table layout is null");
            return nullptr;
        }

        if (!layout->GetBindlessDesc())
        {
            Error("Descriptor tables can only be created with bindless layouts");
            return nullptr;
        }

        return m_Device->CreateDescriptorTable(layout);
    }

    void DeviceWrapper::ResizeDescriptorTable(DescriptorTable *descriptorTable, uint32_t newSize, bool keepContents)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return;
        }
        if (!descriptorTable)
        {
            Error("InvalidArgument: descriptor table is null");
            return;
        }

        m_Device->ResizeDescriptorTable(descriptorTable, newSize, keepContents);
    }

    bool DeviceWrapper::WriteDescriptorTable(DescriptorTable *descriptorTable, const BindingSetItem &item)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return false;
        }
        if (!descriptorTable)
        {
            Error("InvalidArgument: descriptor table is null");
            return false;
        }

        std::stringstream errorStream;

        if (!ValidateBindingSetItem(item, descriptorTable, errorStream))
        {
            if (!errorStream.str().empty())
            {
                Error(errorStream.str());
            }
            return false;
        }

        BindingSetItem patchedItem = item;
        patchedItem.resourceHandle = UnwrapResource(patchedItem.resourceHandle);

        return m_Device->WriteDescriptorTable(descriptorTable, patchedItem);
    }

    RT::OpacityMicromapHandle DeviceWrapper::CreateOpacityMicromap(const RT::OpacityMicromapDesc &desc)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return nullptr;
        }

        if (desc.inputBuffer == nullptr)
        {
            Error("OpacityMicromapDesc:inputBuffer is nullptr");
            return nullptr;
        }

        if (desc.perOmmDescs == nullptr)
        {
            Error("OpacityMicromapDesc:perOmmDescs is nullptr");
            return nullptr;
        }

        RT::OpacityMicromapHandle omm = m_Device->CreateOpacityMicromap(desc);
        if (!omm)
        {
            Error("createOpacityMicromap returned nullptr");
            return nullptr;
        }
        return omm;
    }

    RT::AccelStructHandle DeviceWrapper::CreateAccelStruct(const RT::AccelStructDesc &desc)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return nullptr;
        }

        RT::AccelStructHandle as = m_Device->CreateAccelStruct(desc);

        if (!as)
        {
            return nullptr;
        }

        if ((desc.buildFlags & RT::AccelStructBuildFlags::AllowCompaction) != 0 && desc.isTopLevel)
        {
            std::stringstream ss;
            ss << "Cannot create TLAS " << Utils::DebugNameToString(desc.debugName)
               << " with the AllowCompaction flag set: compaction is not supported for TLAS'es";
            Error(ss.str());
            return nullptr;
        }

        if ((desc.buildFlags & RT::AccelStructBuildFlags::AllowUpdate) != 0 &&
            (desc.buildFlags & RT::AccelStructBuildFlags::AllowCompaction) != 0)
        {
            std::stringstream ss;
            ss << "Cannot create AccelStruct " << Utils::DebugNameToString(desc.debugName)
               << " with incompatible flags: AllowUpdate and AllowCompaction";
            Error(ss.str());
            return nullptr;
        }

        AccelStructWrapper *wrapper = new AccelStructWrapper(as);
        wrapper->isTopLevel = desc.isTopLevel;
        wrapper->allowUpdate = !!(desc.buildFlags & RT::AccelStructBuildFlags::AllowUpdate);
        wrapper->allowCompaction = !!(desc.buildFlags & RT::AccelStructBuildFlags::AllowCompaction);
        wrapper->maxInstances = desc.topLevelMaxInstances;

        return RT::AccelStructHandle::Create(wrapper);
    }

    bool DeviceWrapper::QueryTopLevelAccelStructPrebuildInfo(const RT::AccelStructDesc &desc, uint32_t instanceCount,
                                                             RT::AccelStructPrebuildInfo &outInfo)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return false;
        }

        return m_Device->QueryTopLevelAccelStructPrebuildInfo(desc, instanceCount, outInfo);
    }

    MemoryRequirements DeviceWrapper::GetAccelStructMemoryRequirements(RT::AccelStruct *as)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return {};
        }

        if (as == nullptr)
        {
            Error("getAccelStructMemoryRequirements: as is NULL");
            return MemoryRequirements();
        }

        AccelStructWrapper *wrapper = dynamic_cast<AccelStructWrapper *>(as);
        if (wrapper)
        {
            as = wrapper->GetUnderlyingObject();
        }

        const MemoryRequirements memReq = m_Device->GetAccelStructMemoryRequirements(as);

        return memReq;
    }

    static const char *OperationTypeStrings[] = {"Move", "ClasBuild", "ClasBuildTemplates", "ClasInstantiateTemplates",
                                                 "BlasBuild"};
    static_assert(std::size(OperationTypeStrings) == uint32_t(RT::Cluster::OperationType::BlasBuild) + 1);

    bool DeviceWrapper::ValidateClusterOperationParams(const RT::Cluster::OperationParams &params) const
    {
        bool isValid = true;

        const char *operationType = "Unknown";
        if (uint32_t(params.type) < std::size(OperationTypeStrings))
        {
            operationType = OperationTypeStrings[uint32_t(params.type)];
        }

        switch (params.mode)
        {
        case RT::Cluster::OperationMode::ImplicitDestinations:
        case RT::Cluster::OperationMode::ExplicitDestinations:
        case RT::Cluster::OperationMode::GetSizes:
            break;
        default:
            isValid = false;
            Error("cluster::OperationParams " + std::string(operationType) + " unknown cluster::OperationMode");
            break;
        }

        bool validateClasParams = false;
        switch (params.type)
        {
        case RT::Cluster::OperationType::Move:
            break;
        case RT::Cluster::OperationType::ClasBuild:
        case RT::Cluster::OperationType::ClasBuildTemplates:
        case RT::Cluster::OperationType::ClasInstantiateTemplates:
            validateClasParams = true;
            break;
        case RT::Cluster::OperationType::BlasBuild:
            break;
        }

        if (validateClasParams)
        {
            Physara::RHI::Format vertexFormat = params.clas.vertexFormat;
            const bool validVertexFormat = (vertexFormat == Physara::RHI::Format::RGBA32_FLOAT) ||
                                           (vertexFormat == Physara::RHI::Format::RGB32_FLOAT) ||
                                           (vertexFormat == Physara::RHI::Format::RG32_FLOAT) ||
                                           (vertexFormat == Physara::RHI::Format::RGBA16_FLOAT) ||
                                           (vertexFormat == Physara::RHI::Format::RG16_FLOAT) ||
                                           (vertexFormat == Physara::RHI::Format::RGBA16_SNORM) ||
                                           (vertexFormat == Physara::RHI::Format::RG16_SNORM) ||
                                           (vertexFormat == Physara::RHI::Format::RGBA8_SNORM) ||
                                           (vertexFormat == Physara::RHI::Format::RG8_SNORM) ||
                                           (vertexFormat == Physara::RHI::Format::RGBA16_UNORM) ||
                                           (vertexFormat == Physara::RHI::Format::RG16_UNORM) ||
                                           (vertexFormat == Physara::RHI::Format::RGBA8_UNORM) ||
                                           (vertexFormat == Physara::RHI::Format::RG8_UNORM) ||
                                           (vertexFormat == Physara::RHI::Format::R10G10B10A2_UNORM);
            if (!validVertexFormat)
            {
                Error("cluster::OperationParams " + std::string(operationType) +
                      " does not have a valid vertex format");
                isValid = false;
            }

            if (params.clas.maxGeometryIndex > Physara::RHI::RT::Cluster::MaxGeometryIndex)
            {
                Error("cluster::OperationParams " + std::string(operationType) + " has a maxGeometryIndex over " +
                      std::to_string(Physara::RHI::RT::Cluster::MaxGeometryIndex));
                isValid = false;
            }

            if (params.clas.minPositionTruncateBitCount > 32)
            {
                Error("cluster::OperationParams " + std::string(operationType) + " minPositionTruncateBitCount over " +
                      std::to_string(32));
                isValid = false;
            }

            if (params.clas.maxTriangleCount > Physara::RHI::RT::Cluster::ClasMaxTriangles)
            {
                Error("cluster::OperationParams " + std::string(operationType) + " maxTriangleCount over " +
                      std::to_string(Physara::RHI::RT::Cluster::ClasMaxTriangles));
                isValid = false;
            }

            if (params.clas.maxVertexCount > Physara::RHI::RT::Cluster::ClasMaxVertices)
            {
                Error("cluster::OperationParams " + std::string(operationType) + " maxVertexCount over " +
                      std::to_string(Physara::RHI::RT::Cluster::ClasMaxVertices));
                isValid = false;
            }

            if (params.clas.maxTriangleCount > params.clas.maxTotalTriangleCount)
            {
                Error("cluster::OperationParams " + std::string(operationType) +
                      " maxTriangleCount over maxTotalTriangleCount. maxTotalTriangleCount must be greater than the "
                      "sum of all triangles in the operation");
                isValid = false;
            }

            if (params.clas.maxVertexCount > params.clas.maxTotalVertexCount)
            {
                Error("cluster::OperationParams " + std::string(operationType) +
                      " maxVertexCount over maxTotalVertexCount. maxTotalVertexCount must be greater than the sum of "
                      "all vertices in the operation");
                isValid = false;
            }

            if (params.clas.maxUniqueGeometryCount > params.clas.maxTriangleCount)
            {
                Error("cluster::OperationParams " + std::string(operationType) +
                      " maxUniqueGeometryCount over maxTriangleCount. Maximum 1 geometry per triangle");
                isValid = false;
            }
        }

        return isValid;
    }

    RT::Cluster::OperationSizeInfo DeviceWrapper::GetClusterOperationSizeInfo(
        const RT::Cluster::OperationParams &params)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return {};
        }

        if (!ValidateClusterOperationParams(params))
        {
            return RT::Cluster::OperationSizeInfo{};
        }

        return m_Device->GetClusterOperationSizeInfo(params);
    }

    bool DeviceWrapper::BindAccelStructMemory(RT::AccelStruct *as, Heap *heap, uint64_t offset)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return false;
        }

        if (as == nullptr)
        {
            Error("bindAccelStructMemory: texture is NULL");
            return false;
        }

        if (heap == nullptr)
        {
            Error("bindAccelStructMemory: heap is NULL");
            return false;
        }

        AccelStructWrapper *wrapper = dynamic_cast<AccelStructWrapper *>(as);
        if (wrapper)
        {
            as = wrapper->GetUnderlyingObject();
        }

        const HeapDesc &heapDesc = heap->GetDesc();
        const RT::AccelStructDesc &asDesc = as->GetDesc();

        if (!asDesc.isVirtual)
        {
            std::stringstream ss;
            ss << "Cannot perform bindAccelStructMemory on AccelStruct " << Utils::DebugNameToString(asDesc.debugName)
               << " because it was created with isVirtual = false";

            Error(ss.str());
            return false;
        }

        MemoryRequirements memReq = m_Device->GetAccelStructMemoryRequirements(as);

        if (memReq.size == 0 || memReq.alignment == 0 || offset > heapDesc.capacity ||
            memReq.size > heapDesc.capacity - offset)
        {
            std::stringstream ss;
            ss << "AccelStruct " << Utils::DebugNameToString(asDesc.debugName) << " does not fit into heap "
               << Utils::DebugNameToString(heapDesc.debugName) << " at offset " << offset << " because it requires "
               << memReq.size << " bytes,"
               << " and the heap capacity is " << heapDesc.capacity << " bytes";

            Error(ss.str());
            return false;
        }

        if (memReq.alignment != 0 && (offset % memReq.alignment) != 0)
        {
            std::stringstream ss;
            ss << "AccelStruct " << Utils::DebugNameToString(asDesc.debugName) << " is placed in heap "
               << Utils::DebugNameToString(heapDesc.debugName) << " at invalid alignment: required alignment to "
               << memReq.alignment << " bytes,"
               << " actual offset is " << offset << " bytes";

            Error(ss.str());
            return false;
        }

        return m_Device->BindAccelStructMemory(as, heap, offset);
    }

    CommandListHandle DeviceWrapper::CreateCommandList(const CommandListParameters &params)
    {
        if (!Profile::CheckQueue(m_Device, params.queueType))
        {
            return nullptr;
        }
        if (!params.enableImmediateExecution && Profile::RejectOpenGL(m_Device))
        {
            return nullptr;
        }

        switch (params.queueType)
        {
        case CommandQueue::Graphics:
            // Assume the graphics queue always exists
            break;

        case CommandQueue::Compute:
            if (!Profile::QueryFeatureSupport(m_Device, Feature::ComputeQueue))
            {
                Error("Compute queue is not supported or initialized in this device");
                return nullptr;
            }
            break;

        case CommandQueue::Copy:
            if (!Profile::QueryFeatureSupport(m_Device, Feature::CopyQueue))
            {
                Error("Copy queue is not supported or initialized in this device");
                return nullptr;
            }
            break;

        case CommandQueue::Count:
        default:
            Utils::InvalidEnum();
            return nullptr;
        }

        CommandListHandle commandList = m_Device->CreateCommandList(params);

        if (commandList == nullptr)
        {
            return nullptr;
        }

        try
        {
            return CommandListHandle::Create(
                new CommandListWrapper(this, commandList, params.enableImmediateExecution, params.queueType));
        }
        catch (const std::bad_alloc &)
        {
            Report(m_MessageCallback, MessageSeverity::Error, "CreateCommandList: CPU wrapper allocation failed");
            return nullptr;
        }
    }

    uint64_t DeviceWrapper::ExecuteCommandLists(CommandList *const *pCommandLists, size_t numCommandLists,
                                                CommandQueue executionQueue)
    {

        if (numCommandLists == 0)
        {
            return 0;
        }
        if (numCommandLists > std::numeric_limits<uint32_t>::max())
        {
            Error("InvalidArgument: command list count exceeds uint32_t");
            return 0;
        }
        if (!Profile::CheckQueue(m_Device, executionQueue))
        {
            return 0;
        }
        if (!pCommandLists)
        {
            Error("InvalidArgument: command list array is null");
            return 0;
        }
        std::optional<std::vector<CommandList *>> unwrappedCommandLists;
        try
        {
            // MSVC Debug 的容器构造也可能分配; 连同构造一起纳入预检失败边界.
            unwrappedCommandLists.emplace(numCommandLists);
            std::unordered_set<CommandList *> unique;
            unique.reserve(numCommandLists);
            // 全批只读预检. 输出暂存与去重分配也在任何状态消耗之前完成.
            for (size_t index = 0; index < numCommandLists; ++index)
            {
                auto *wrapper = dynamic_cast<CommandListWrapper *>(pCommandLists[index]);
                if (!wrapper || wrapper->m_Device.Get() != this)
                {
                    Error("InvalidArgument: null, foreign or unwrapped command list");
                    return 0;
                }
                if (wrapper->m_QueueType != executionQueue)
                {
                    Error("InvalidArgument: command list queue does not match execution queue");
                    return 0;
                }
                if (!unique.insert(wrapper).second)
                {
                    Error("InvalidArgument: duplicate command list identity in batch");
                    return 0;
                }
                if (!wrapper->RequireExecuteState())
                {
                    return 0;
                }
                (*unwrappedCommandLists)[index] = wrapper->GetUnderlyingCommandList();
            }
        }
        catch (const std::bad_alloc &)
        {
            Report(m_MessageCallback, MessageSeverity::Error, "ExecuteCommandLists: CPU preflight allocation failed");
            return 0;
        }
        for (size_t index = 0; index < numCommandLists; ++index)
        {
            static_cast<CommandListWrapper *>(pCommandLists[index])->m_State = CommandListState::Initial;
        }
        // 委托后不可假定 native 能回滚; 0/异常均不自动恢复 Closed 或重放.
        return m_Device->ExecuteCommandLists(unwrappedCommandLists->data(), unwrappedCommandLists->size(),
                                             executionQueue);
    }

    void DeviceWrapper::QueueWaitForCommandList(CommandQueue waitQueue, CommandQueue executionQueue, uint64_t instance)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return;
        }

        m_Device->QueueWaitForCommandList(waitQueue, executionQueue, instance);
    }

    bool DeviceWrapper::WaitForIdle()
    {
        return m_Device->WaitForIdle();
    }

    CommandListLifetimeTrackerHandle DeviceWrapper::CreateCommandListLifetimeTracker(CommandQueue executionQueue)
    {
        if (!Profile::CheckQueue(m_Device, executionQueue))
        {
            return nullptr;
        }

        return m_Device->CreateCommandListLifetimeTracker(executionQueue);
    }

    void DeviceWrapper::RunGarbageCollection()
    {
        m_Device->RunGarbageCollection();
    }

    bool DeviceWrapper::QueryFeatureSupport(Feature feature, void *pInfo, size_t infoSize)
    {
        return Profile::QueryFeatureSupport(m_Device, feature, pInfo, infoSize);
    }

    FormatSupport DeviceWrapper::QueryFormatSupport(Format format)
    {
        if (format == Format::UNKNOWN)
        {
            return FormatSupport::None;
        }
        if (format >= Format::COUNT)
        {
            Error("InvalidArgument: unknown format");
            return FormatSupport::None;
        }
        return m_Device->QueryFormatSupport(format);
    }

    CoopVec::DeviceFeatures DeviceWrapper::QueryCoopVecFeatures()
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return {};
        }

        return m_Device->QueryCoopVecFeatures();
    }

    CoopVec::MatMulFormatSupport DeviceWrapper::QueryCoopVecMatMulFormatSupport(
        const CoopVec::MatMulFormatCombo &combination)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return {};
        }

        return m_Device->QueryCoopVecMatMulFormatSupport(combination);
    }

    CoopVec::TrainingFormatSupport DeviceWrapper::QueryCoopVecTrainingFormatSupport(CoopVec::DataType componentType)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return {};
        }

        return m_Device->QueryCoopVecTrainingFormatSupport(componentType);
    }

    size_t DeviceWrapper::GetCoopVecMatrixSize(CoopVec::DataType type, CoopVec::MatrixLayout layout, int rows,
                                               int columns)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return {};
        }

        if (!Profile::QueryFeatureSupport(m_Device, Feature::CooperativeVectorInferencing))
        {
            Error("getCoopVecMatrixSize: Cooperative Vectors are not supported by the device");
            return 0;
        }

        if (rows <= 0 || columns <= 0)
        {
            Error("getCoopVecMatrixSize: rows and columns must be positive");
            return 0;
        }

        return m_Device->GetCoopVecMatrixSize(type, layout, rows, columns);
    }

    Object DeviceWrapper::GetNativeQueue(ObjectType objectType, CommandQueue queue)
    {
        if (!Profile::CheckQueue(m_Device, queue))
        {
            return nullptr;
        }

        return m_Device->GetNativeQueue(objectType, queue);
    }

    MessageCallback *DeviceWrapper::GetMessageCallback()
    {
        return m_MessageCallback;
    }

    bool DeviceWrapper::IsAftermathEnabled()
    {
        if (Profile::IsOpenGL(m_Device))
        {
            return false;
        }
        return m_Device->IsAftermathEnabled();
    }

    AftermathCrashDumpHelper &DeviceWrapper::GetAftermathCrashDumpHelper()
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            throw std::logic_error("Unsupported OpenGL Aftermath crash helper");
        }
        return m_Device->GetAftermathCrashDumpHelper();
    }

    void Range::Add(uint32_t item)
    {
        min = std::min(min, item);
        max = std::max(max, item);
    }

    bool Range::Empty() const
    {
        return min > max;
    }

    bool Range::OverlapsWith(const Range &other) const
    {
        return !Empty() && !other.Empty() && max >= other.min && min <= other.max;
    }

    bool BindingSummary::Any() const
    {
        return !locations.empty();
    }

    bool BindingSummary::OverlapsWith(const BindingSummary &other) const
    {
        return rangeSRV.OverlapsWith(other.rangeSRV) || rangeSampler.OverlapsWith(other.rangeSampler) ||
               rangeUAV.OverlapsWith(other.rangeUAV) || rangeCB.OverlapsWith(other.rangeCB);
    }

    Resource *UnwrapResource(Resource *resource)
    {
        if (!resource)
        {
            return nullptr;
        }

        AccelStructWrapper *asWrapper = dynamic_cast<AccelStructWrapper *>(resource);

        if (asWrapper)
        {
            return asWrapper->GetUnderlyingObject();
        }

        // More resource types to be added here when their wrappers are implemented

        return resource;
    }

    std::ostream &operator<<(std::ostream &os, const BindingLocationSet &set)
    {
        bool first = true;
        for (BindingLocation const &item : set)
        {
            if (!first)
            {
                os << ", ";
            }

            if (item.registerSpace != 0)
            {
                os << "space" << item.registerSpace << ".";
            }

            char const *prefix = "?";
            switch (item.type)
            {
            case GraphicsResourceType::SRV:
                prefix = "t";
                break;
            case GraphicsResourceType::Sampler:
                prefix = "s";
                break;
            case GraphicsResourceType::UAV:
                prefix = "u";
                break;
            case GraphicsResourceType::CB:
                prefix = "b";
                break;
            }
            os << prefix << item.slot;

            if (item.arrayElement != 0)
            {
                os << "[" << item.arrayElement << "]";
            }

            first = false;
        }
        return os;
    }
} // namespace Physara::RHI::Validation
