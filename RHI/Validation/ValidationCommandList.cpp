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

#include "Common/Misc.hpp"
#include "Utils.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace Physara::RHI::Validation
{

    CommandListWrapper::CommandListWrapper(DeviceWrapper *device, CommandList *commandList, bool isImmediate,
                                           CommandQueue queueType)
        : m_CommandList(commandList), m_Device(device), m_MessageCallback(device->GetMessageCallback()),
          m_IsImmediate(isImmediate), m_QueueType(queueType)
    {
    }

    CommandListWrapper::~CommandListWrapper()
    {
        if (m_HasImmediateClaim)
        {
            --m_Device->m_NumOpenImmediateCommandLists;
            m_HasImmediateClaim = false;
        }
        // 底层列表的释放可能访问设备; 父包装及其 Device 引用此时仍存活.
        m_CommandList.Reset();
    }

    void CommandListWrapper::Error(const std::string &messageText, std::source_location location) const
    {
        Report(m_MessageCallback, MessageSeverity::Error, messageText, location);
    }

    void CommandListWrapper::Warning(const std::string &messageText, std::source_location location) const
    {
        Report(m_MessageCallback, MessageSeverity::Warning, messageText, location);
    }

    static const char *CommandListStateToString(CommandListState state)
    {
        switch (state)
        {
        case CommandListState::Initial:
            return "INITIAL";
        case CommandListState::Open:
            return "OPEN";
        case CommandListState::Closed:
            return "CLOSED";
        default:
            return "<INVALID>";
        }
    }

    static const char *CommandQueueTypeToString(CommandQueue type)
    {
        switch (type)
        {
        case CommandQueue::Graphics:
            return "GRAPHICS";
        case CommandQueue::Compute:
            return "COMPUTE";
        case CommandQueue::Copy:
            return "COPY";
        case CommandQueue::Count:
        default:
            return "<INVALID>";
        }
    }

    bool CommandListWrapper::RequireOpenState(std::source_location location) const
    {
        if (m_State == CommandListState::Open)
        {
            return true;
        }

        std::stringstream ss;
        ss << "InvalidState: A command list must be opened before any rendering commands can be executed. "
              "Actual state: "
           << CommandListStateToString(m_State);
        Error(ss.str(), location);

        return false;
    }

    bool CommandListWrapper::RequireExecuteState(std::source_location location) const
    {

        if (m_State != CommandListState::Closed)
        {
            Error("InvalidState: command list must be opened and closed before submission", location);
            return false;
        }
        return true;
    }

    bool CommandListWrapper::RequireType(CommandQueue queueType, const char *operation) const
    {
        if ((int)m_QueueType > (int)queueType)
        {
            std::stringstream ss;
            ss << "This command list has type " << CommandQueueTypeToString(m_QueueType) << ", but the '" << operation
               << "' operation requires at least " << CommandQueueTypeToString(queueType);
            Error(ss.str());

            return false;
        }

        return true;
    }

    Object CommandListWrapper::GetNativeObject(ObjectType objectType)
    {
        return m_CommandList->GetNativeObject(objectType);
    }

    void CommandListWrapper::Open()
    {
        switch (m_State)
        {
        case CommandListState::Open:
            Error("InvalidState: Cannot open a command list that is already open");
            return;
        case CommandListState::Closed:
            if (m_IsImmediate)
            {
                Error("InvalidState: An immediate command list cannot be abandoned and must be executed before it is "
                      "re-opened");
                return;
            }
            else
            {
                Warning("A command list should be executed before it is reopened");
                break;
            }
        case CommandListState::Initial:
        default:
            break;
        }

        if (m_IsImmediate)
        {
            unsigned int expected = 0;
            if (!m_Device->m_NumOpenImmediateCommandLists.compare_exchange_strong(expected, 1))
            {
                Error("InvalidState: Two or more immediate command lists cannot be open at the same time");
                return;
            }
            m_HasImmediateClaim = true;
        }

        m_CommandList->Open();

        m_State = CommandListState::Open;
        m_GraphicsStateSet = false;
        m_ComputeStateSet = false;
        m_MeshletStateSet = false;
        m_RayTracingStateSet = false;
        m_PushConstantsSet = false;
        m_PipelinePushConstantSize = 0;
    }

    void CommandListWrapper::Close()
    {
        switch (m_State)
        {
        case CommandListState::Initial:
            Error("InvalidState: Cannot close a command list before it is opened");
            return;
        case CommandListState::Closed:
            Error("InvalidState: Cannot close a command list that is already closed");
            return;
        case CommandListState::Open:
        default:
            break;
        }

        if (m_HasImmediateClaim)
        {
            --m_Device->m_NumOpenImmediateCommandLists;
            m_HasImmediateClaim = false;
        }

        m_CommandList->Close();

        m_State = CommandListState::Closed;
        m_GraphicsStateSet = false;
        m_ComputeStateSet = false;
        m_MeshletStateSet = false;
        m_RayTracingStateSet = false;
        m_PushConstantsSet = false;
        m_PipelinePushConstantSize = 0;
    }

    void CommandListWrapper::ClearTextureFloat(Texture *t, TextureSubresourceSet subresources, const Color &clearColor)
    {
        if (!RequireOpenState())
        {
            return;
        }
        if (!Input::CheckTextureRange(m_Device, t, subresources))
        {
            return;
        }

        if (!RequireType(CommandQueue::Compute, "clearTextureFloat"))
        {
            return;
        }

        const TextureDesc &textureDesc = t->GetDesc();

        const FormatInfo &formatInfo = GetFormatInfo(textureDesc.format);
        if (formatInfo.hasDepth || formatInfo.hasStencil)
        {
            std::stringstream ss;
            ss << "Texture " << Utils::DebugNameToString(textureDesc.debugName)
               << " cannot be cleared with "
                  "clearTextureFloat because it's a depth-stencil texture. Use clearDepthStencilTexture instead.";
            Error(ss.str());
            return;
        }

        if (formatInfo.kind == FormatKind::Integer)
        {
            std::stringstream ss;
            ss << "Texture " << Utils::DebugNameToString(textureDesc.debugName)
               << " cannot be cleared with "
                  "clearTextureFloat because it's an integer texture. Use clearTextureUInt instead.";
            Error(ss.str());
            return;
        }

        if (!textureDesc.isRenderTarget && !textureDesc.isUAV)
        {
            std::stringstream ss;
            ss << "Texture " << Utils::DebugNameToString(textureDesc.debugName)
               << " cannot be cleared with "
                  "clearTextureFloat because it was created with both isRenderTarget = false and isUAV = false.";
            Error(ss.str());
            return;
        }

        m_CommandList->ClearTextureFloat(t, subresources, clearColor);
    }

    void CommandListWrapper::ClearDepthStencilTexture(Texture *t, TextureSubresourceSet subresources, bool clearDepth,
                                                      float depth, bool clearStencil, uint8_t stencil)
    {
        if (!RequireOpenState())
        {
            return;
        }
        if (!Input::CheckTextureRange(m_Device, t, subresources))
        {
            return;
        }
        const auto &clearFormat = GetFormatInfo(t->GetDesc().format);
        if ((clearDepth && !clearFormat.hasDepth) || (clearStencil && !clearFormat.hasStencil))
        {
            Error("InvalidArgument: selected depth/stencil aspect is absent");
            return;
        }

        if (!RequireType(CommandQueue::Graphics, "clearDepthStencilTexture"))
        {
            return;
        }

        const FormatInfo &formatInfo = GetFormatInfo(t->GetDesc().format);
        if (!formatInfo.hasDepth && !formatInfo.hasStencil)
        {
            std::stringstream ss;
            ss << "Texture " << Utils::DebugNameToString(t->GetDesc().debugName)
               << " cannot be cleared with "
                  "clearDepthStencilTexture because it's not a depth-stencil texture. Use clearTextureFloat or "
                  "clearTextureUInt instead.";
            Error(ss.str());
            return;
        }

        if (!t->GetDesc().isRenderTarget)
        {
            std::stringstream ss;
            ss << "Texture " << Utils::DebugNameToString(t->GetDesc().debugName)
               << " cannot be cleared with "
                  "clearDepthStencilTexture because it was created with isRenderTarget = false.";
            Error(ss.str());
            return;
        }

        m_CommandList->ClearDepthStencilTexture(t, subresources, clearDepth, depth, clearStencil, stencil);
    }

    void CommandListWrapper::ClearTextureUInt(Texture *t, TextureSubresourceSet subresources, uint32_t clearColor)
    {
        if (!RequireOpenState())
        {
            return;
        }
        if (!Input::CheckTextureRange(m_Device, t, subresources))
        {
            return;
        }

        if (!RequireType(CommandQueue::Compute, "clearTextureUInt"))
        {
            return;
        }

        const TextureDesc &textureDesc = t->GetDesc();

        const FormatInfo &formatInfo = GetFormatInfo(textureDesc.format);
        if (formatInfo.hasDepth || formatInfo.hasStencil)
        {
            std::stringstream ss;
            ss << "Texture " << Utils::DebugNameToString(textureDesc.debugName)
               << " cannot be cleared with "
                  "clearTextureUInt because it's a depth-stencil texture. Use clearDepthStencilTexture instead.";
            Error(ss.str());
            return;
        }

        if (formatInfo.kind != FormatKind::Integer)
        {
            std::stringstream ss;
            ss << "Texture " << Utils::DebugNameToString(textureDesc.debugName)
               << " cannot be cleared with "
                  "clearTextureUInt because it's not an integer texture. Use clearTextureFloat instead.";
            Error(ss.str());
            return;
        }

        if (!textureDesc.isRenderTarget && !textureDesc.isUAV)
        {
            std::stringstream ss;
            ss << "Texture " << Utils::DebugNameToString(textureDesc.debugName)
               << " cannot be cleared with "
                  "clearTextureUInt because it was created with both isRenderTarget = false and isUAV = false.";
            Error(ss.str());
            return;
        }

        m_CommandList->ClearTextureUInt(t, subresources, clearColor);
    }

    void CommandListWrapper::CopyTexture(Texture *dest, const TextureSlice &destSlice, Texture *src,
                                         const TextureSlice &srcSlice)
    {
        if (!RequireOpenState())
        {
            return;
        }
        if (!dest || !src)
        {
            Error("InvalidArgument: texture copy requires source and destination");
            return;
        }
        if (!Input::CheckTextureCopy(m_Device, dest->GetDesc(), destSlice, src->GetDesc(), srcSlice))
        {
            return;
        }

        m_CommandList->CopyTexture(dest, destSlice, src, srcSlice);
    }

    void CommandListWrapper::CopyTexture(StagingTexture *dest, const TextureSlice &destSlice, Texture *src,
                                         const TextureSlice &srcSlice)
    {
        if (!RequireOpenState())
        {
            return;
        }
        if (!dest || !src)
        {
            Error("InvalidArgument: texture copy requires source and destination");
            return;
        }
        if (!Input::CheckTextureCopy(m_Device, dest->GetDesc(), destSlice, src->GetDesc(), srcSlice))
        {
            return;
        }

        m_CommandList->CopyTexture(dest, destSlice, src, srcSlice);
    }

    void CommandListWrapper::CopyTexture(Texture *dest, const TextureSlice &destSlice, StagingTexture *src,
                                         const TextureSlice &srcSlice)
    {
        if (!RequireOpenState())
        {
            return;
        }
        if (!dest || !src)
        {
            Error("InvalidArgument: texture copy requires source and destination");
            return;
        }
        if (!Input::CheckTextureCopy(m_Device, dest->GetDesc(), destSlice, src->GetDesc(), srcSlice))
        {
            return;
        }

        m_CommandList->CopyTexture(dest, destSlice, src, srcSlice);
    }

    void CommandListWrapper::WriteTexture(Texture *dest, uint32_t arraySlice, uint32_t mipLevel, const void *data,
                                          size_t rowPitch, size_t depthPitch)
    {
        if (!RequireOpenState())
        {
            return;
        }
        if (!dest || !data)
        {
            Error("InvalidArgument: texture and data are required");
            return;
        }
        TextureSlice writeSlice;
        writeSlice.arraySlice = arraySlice;
        writeSlice.mipLevel = mipLevel;
        if (!Input::CheckTextureSlice(m_Device, dest->GetDesc(), writeSlice))
        {
            return;
        }
        const auto &writeDesc = dest->GetDesc();
        const auto resolvedWrite = writeSlice.Resolve(writeDesc);
        const auto &writeFormat = GetFormatInfo(writeDesc.format);
        const size_t columns =
            resolvedWrite.width / writeFormat.blockSize + (resolvedWrite.width % writeFormat.blockSize != 0);
        const size_t rows =
            resolvedWrite.height / writeFormat.blockSize + (resolvedWrite.height % writeFormat.blockSize != 0);
        if (columns > std::numeric_limits<size_t>::max() / writeFormat.bytesPerBlock)
        {
            Error("InvalidArgument: texture row size overflows");
            return;
        }
        const size_t rowBytes = columns * writeFormat.bytesPerBlock;
        if ((rowPitch != 0 && rowPitch < rowBytes) || (rows > 1 && rowPitch == 0) || writeDesc.sampleCount != 1)
        {
            Error("InvalidArgument: invalid texture row pitch or multisample write");
            return;
        }
        const size_t rowStride = rowPitch == 0 ? rowBytes : rowPitch;
        if (rows > 1 && rowStride > (std::numeric_limits<size_t>::max() - rowBytes) / (rows - 1))
        {
            Error("InvalidArgument: texture slice pitch/footprint overflows");
            return;
        }
        const size_t sliceBytes = (rows - 1) * rowStride + rowBytes;
        if (resolvedWrite.depth > 1 &&
            (depthPitch < sliceBytes ||
             depthPitch > (std::numeric_limits<size_t>::max() - sliceBytes) / (resolvedWrite.depth - 1)))
        {
            Error("InvalidArgument: texture volume pitch/footprint overflows");
            return;
        }

        m_CommandList->WriteTexture(dest, arraySlice, mipLevel, data, rowPitch, depthPitch);
    }

    void CommandListWrapper::ResolveTexture(Texture *dest, const TextureSubresourceSet &dstSubresources, Texture *src,
                                            const TextureSubresourceSet &srcSubresources)
    {
        if (!RequireOpenState())
        {
            return;
        }
        if (!Input::CheckTextureRange(m_Device, dest, dstSubresources) ||
            !Input::CheckTextureRange(m_Device, src, srcSubresources))
        {
            return;
        }

        if (!RequireType(CommandQueue::Graphics, "resolveTexture"))
        {
            return;
        }

        bool anyErrors = false;

        if (!dest)
        {
            Error("resolveTexture: dest is NULL");
            anyErrors = true;
        }

        if (!src)
        {
            Error("resolveTexture: src is NULL");
            anyErrors = true;
        }

        if (anyErrors)
        {
            return;
        }

        const TextureDesc &dstDesc = dest->GetDesc();
        const TextureDesc &srcDesc = src->GetDesc();

        TextureSubresourceSet dstSR = dstSubresources.Resolve(dstDesc, false);
        TextureSubresourceSet srcSR = srcSubresources.Resolve(srcDesc, false);

        if (dstSR.numArraySlices != srcSR.numArraySlices || dstSR.numMipLevels != srcSR.numMipLevels)
        {
            Error("resolveTexture: source and destination subresource sets must resolve to sets of the same size");
            anyErrors = true;
        }

        const uint32_t srcMipWidth = std::max(srcDesc.width >> srcSR.baseMipLevel, 1u);
        const uint32_t srcMipHeight = std::max(srcDesc.height >> srcSR.baseMipLevel, 1u);
        const uint32_t dstMipWidth = std::max(dstDesc.width >> dstSR.baseMipLevel, 1u);
        const uint32_t dstMipHeight = std::max(dstDesc.height >> dstSR.baseMipLevel, 1u);
        if (srcMipWidth != dstMipWidth || srcMipHeight != dstMipHeight)
        {
            Error("resolveTexture: referenced mip levels of source and destination textures must have the same "
                  "dimensions");
            anyErrors = true;
        }

        if (dstDesc.sampleCount != 1)
        {
            Error("resolveTexture: destination texture must not be multi-sampled");
            anyErrors = true;
        }

        if (srcDesc.sampleCount <= 1)
        {
            Error("resolveTexture: source texture must be multi-sampled");
            anyErrors = true;
        }

        if (srcDesc.format != dstDesc.format)
        {
            Error("resolveTexture: source and destination textures must have the same format");
            anyErrors = true;
        }

        if (anyErrors)
        {
            return;
        }

        m_CommandList->ResolveTexture(dest, dstSubresources, src, srcSubresources);
    }

    void CommandListWrapper::WriteBuffer(Buffer *b, const void *data, size_t dataSize, uint64_t destOffsetBytes)
    {
        if (!RequireOpenState())
        {
            return;
        }
        if (!Input::CheckBufferRange(m_Device, b, destOffsetBytes, dataSize))
        {
            return;
        }
        if (dataSize != 0 && !data)
        {
            Error("InvalidArgument: nonempty buffer write requires data");
            return;
        }

        if (destOffsetBytes > 0 && b->GetDesc().isVolatile)
        {
            Error("writeBuffer: cannot write into volatile buffers with an offset");
            return;
        }

        if (dataSize > 0x10000 && b->GetDesc().isVolatile)
        {
            Error("writeBuffer: cannot write more than 65535 bytes into volatile buffers");
            return;
        }

        m_CommandList->WriteBuffer(b, data, dataSize, destOffsetBytes);
    }

    void CommandListWrapper::ClearBufferUInt(Buffer *b, uint32_t clearValue)
    {
        if (!RequireOpenState())
        {
            return;
        }
        if (!b || !Input::CheckBufferDesc(m_Device, b->GetDesc()))
        {
            if (!b)
            {
                Error("InvalidArgument: null clear buffer");
            }
            return;
        }
        if (!b->GetDesc().canHaveUAVs || b->GetDesc().byteSize % 4 != 0)
        {
            Error("InvalidArgument: clear buffer requires aligned UAV storage");
            return;
        }

        if (!RequireType(CommandQueue::Compute, "clearBufferUInt"))
        {
            return;
        }

        m_CommandList->ClearBufferUInt(b, clearValue);
    }

    void CommandListWrapper::CopyBuffer(Buffer *dest, uint64_t destOffsetBytes, Buffer *src, uint64_t srcOffsetBytes,
                                        uint64_t dataSizeBytes)
    {
        if (!RequireOpenState())
        {
            return;
        }
        if (!Input::CheckBufferRange(m_Device, dest, destOffsetBytes, dataSizeBytes) ||
            !Input::CheckBufferRange(m_Device, src, srcOffsetBytes, dataSizeBytes))
        {
            return;
        }
        if (dest == src && dataSizeBytes != 0 &&
            (destOffsetBytes <= srcOffsetBytes ? srcOffsetBytes - destOffsetBytes : destOffsetBytes - srcOffsetBytes) <
                dataSizeBytes)
        {
            Error("InvalidArgument: overlapping self buffer copy");
            return;
        }
        if (dest->GetDesc().isVolatile || src->GetDesc().isVolatile)
        {
            Error("InvalidArgument: volatile buffers cannot participate in copies");
            return;
        }

        m_CommandList->CopyBuffer(dest, destOffsetBytes, src, srcOffsetBytes, dataSizeBytes);
    }

    void CommandListWrapper::ClearSamplerFeedbackTexture(SamplerFeedbackTexture *texture)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return;
        }

        if (!RequireOpenState())
        {
            return;
        }
        m_CommandList->ClearSamplerFeedbackTexture(texture);
    }

    void CommandListWrapper::DecodeSamplerFeedbackTexture(Buffer *buffer, SamplerFeedbackTexture *texture,
                                                          Physara::RHI::Format format)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return;
        }

        if (!RequireOpenState())
        {
            return;
        }
        m_CommandList->DecodeSamplerFeedbackTexture(buffer, texture, format);
    }

    void CommandListWrapper::SetSamplerFeedbackTextureState(SamplerFeedbackTexture *texture, ResourceStates stateBits)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return;
        }

        if (!RequireOpenState())
        {
            return;
        }
        m_CommandList->SetSamplerFeedbackTextureState(texture, stateBits);
    }

    bool CommandListWrapper::ValidateBindingSetsAgainstLayouts(
        const StaticVector<BindingLayoutHandle, MaxBindingLayouts> &layouts,
        const StaticVector<BindingSet *, MaxBindingLayouts> &sets, std::source_location location) const
    {

        if (layouts.size() != sets.size())
        {
            Error("InvalidArgument: binding set count does not match pipeline layouts", location);
            return false;
        }
        for (size_t index = 0; index < layouts.size(); ++index)
        {
            if (!layouts[index] || !sets[index] || !sets[index]->GetLayout())
            {
                Error("InvalidArgument: null pipeline layout or binding set/layout", location);
                return false;
            }
            auto *expected = layouts[index].Get();
            auto *actual = sets[index]->GetLayout();
            const bool bindless = expected->GetBindlessDesc() != nullptr;
            const auto *desc = sets[index]->GetDesc();
            if ((!bindless && actual != expected) || (bindless && desc) || (!bindless && !desc))
            {
                Error("InvalidArgument: binding set/layout identity or kind mismatch", location);
                return false;
            }
            if (desc && !m_Device->ValidateBindingSetDesc(*desc, actual, location))
            {
                return false;
            }
        }
        return true;
    }

    void CommandListWrapper::SetPushConstants(const void *data, size_t byteSize)
    {
        if (!RequireOpenState())
        {
            return;
        }
        if (!data || byteSize == 0 || byteSize % 4 != 0)
        {
            Error("InvalidArgument: push constant data/size/alignment is invalid");
            return;
        }

        if (!m_GraphicsStateSet && !m_ComputeStateSet && !m_MeshletStateSet && !m_RayTracingStateSet)
        {
            Error("setPushConstants is only valid when a graphics, compute, meshlet, or ray tracing state is set");
            return;
        }

        if (byteSize > MaxPushConstantSize)
        {
            std::stringstream ss;
            ss << "Push constant size (" << byteSize << ") cannot exceed " << MaxPushConstantSize << " bytes";
            Error(ss.str());
            return;
        }

        if (byteSize != m_PipelinePushConstantSize)
        {
            std::stringstream ss;

            if (m_PipelinePushConstantSize == 0)
            {
                ss << "The current pipeline does not expect any push constants, so the setPushConstants call is "
                      "invalid.";
            }
            else
            {
                ss << "Push constant size (" << byteSize << " bytes) doesn't match the size expected by the pipeline ("
                   << m_PipelinePushConstantSize << " bytes)";
            }

            Error(ss.str());
            return;
        }

        m_PushConstantsSet = true;

        m_CommandList->SetPushConstants(data, byteSize);
    }

    void CommandListWrapper::SetGraphicsState(const GraphicsState &state)
    {
        if (state.shadingRateState.enabled && Profile::RejectOpenGL(m_Device))
        {
            return;
        }
        if (state.pipeline && Profile::RejectGraphicsPipeline(m_Device, state.pipeline->GetDesc()))
        {
            return;
        }
        for (auto *bindings : state.bindings)
        {
            if (Profile::RejectBindingSet(m_Device, bindings))
            {
                return;
            }
        }

        if (!RequireOpenState())
        {
            return;
        }

        if (!RequireType(CommandQueue::Graphics, "setGraphicsState"))
        {
            return;
        }

        if (!state.pipeline || !state.framebuffer)
        {
            Error("InvalidArgument: graphics pipeline/framebuffer is null");
            return;
        }
        const auto &pipelineDesc = state.pipeline->GetDesc();
        if (!pipelineDesc.VS || pipelineDesc.primType > PrimitiveType::PatchList)
        {
            Error("InvalidArgument: invalid graphics pipeline description");
            return;
        }
        if (!m_Device->ValidateShaderType(ShaderType::Vertex, pipelineDesc.VS->GetDesc(), "SetGraphicsState") ||
            !m_Device->ValidateRenderState(pipelineDesc.renderState, state.pipeline->GetFramebufferInfo()))
        {
            return;
        }
        for (const auto &attachment : state.framebuffer->GetDesc().colorAttachments)
        {
            if (!Input::CheckTextureRange(m_Device, attachment.texture, attachment.subresources, true))
            {
                return;
            }
        }
        if (state.framebuffer->GetDesc().depthAttachment.texture &&
            !Input::CheckTextureRange(m_Device, state.framebuffer->GetDesc().depthAttachment.texture,
                                      state.framebuffer->GetDesc().depthAttachment.subresources, true))
        {
            return;
        }
        for (const auto &vb : state.vertexBuffers)
        {
            if (!Input::CheckBufferRange(m_Device, vb.buffer, vb.offset, 0))
            {
                return;
            }
        }
        if (state.indexBuffer.buffer)
        {
            if (!Input::CheckBufferRange(m_Device, state.indexBuffer.buffer, state.indexBuffer.offset, 0))
            {
                return;
            }
            if ((state.indexBuffer.format != Format::R16_UINT && state.indexBuffer.format != Format::R32_UINT) ||
                state.indexBuffer.offset % (state.indexBuffer.format == Format::R16_UINT ? 2u : 4u) != 0)
            {
                Error("InvalidArgument: index buffer format/offset is invalid");
                return;
            }
        }
        for (auto *buffer : {state.indirectParams, state.indirectCountBuffer})
        {
            if (buffer && !Input::CheckBufferDesc(m_Device, buffer->GetDesc()))
            {
                return;
            }
            if (buffer && !buffer->GetDesc().isDrawIndirectArgs)
            {
                Error("InvalidArgument: invalid indirect argument/count buffer");
                return;
            }
        }
        for (const auto &viewport : state.viewport.viewports)
        {
            if (!std::isfinite(viewport.minX) || !std::isfinite(viewport.maxX) || !std::isfinite(viewport.minY) ||
                !std::isfinite(viewport.maxY) || !std::isfinite(viewport.minZ) || !std::isfinite(viewport.maxZ) ||
                viewport.minX > viewport.maxX || viewport.minY > viewport.maxY || viewport.minZ < 0.f ||
                viewport.maxZ > 1.f || viewport.maxZ < 0.f || viewport.minZ > 1.f ||
                double(viewport.maxX) - viewport.minX > std::numeric_limits<float>::max() ||
                double(viewport.maxY) - viewport.minY > std::numeric_limits<float>::max())
            {
                Error("InvalidArgument: invalid viewport interval");
                return;
            }
        }
        for (const auto &rect : state.viewport.scissorRects)
        {
            const int64_t width = int64_t(rect.maxX) - rect.minX;
            const int64_t height = int64_t(rect.maxY) - rect.minY;
            if (width < 0 || height < 0 || width > std::numeric_limits<int>::max() ||
                height > std::numeric_limits<int>::max())
            {
                Error("InvalidArgument: scissor extent is not representable");
                return;
            }
        }
        bool anyErrors = false;
        std::stringstream ss;
        ss << "setGraphicsState: " << std::endl;

        if (!state.pipeline)
        {
            ss << "pipeline is NULL." << std::endl;
            anyErrors = true;
        }

        if (!state.framebuffer)
        {
            ss << "framebuffer is NULL." << std::endl;
            anyErrors = true;
        }

        if (state.indexBuffer.buffer && !state.indexBuffer.buffer->GetDesc().isIndexBuffer)
        {
            ss << "Cannot use buffer '" << Utils::DebugNameToString(state.indexBuffer.buffer->GetDesc().debugName)
               << "' as an index buffer because it does not have the isIndexBuffer flag set." << std::endl;
            anyErrors = true;
        }

        for (size_t index = 0; index < state.vertexBuffers.size(); index++)
        {
            const VertexBufferBinding &vb = state.vertexBuffers[index];

            if (!vb.buffer)
            {
                ss << "Vertex buffer at index " << index << " is NULL." << std::endl;
                anyErrors = true;
            }
            else if (!vb.buffer->GetDesc().isVertexBuffer)
            {
                ss << "Buffer '" << Utils::DebugNameToString(vb.buffer->GetDesc().debugName)
                   << "' bound to vertex buffer slot " << index
                   << " cannot be used as a vertex buffer because it does not have the isVertexBuffer flag set."
                   << std::endl;
                anyErrors = true;
            }

            if (vb.slot >= MaxVertexAttributes)
            {
                ss << "Vertex buffer binding at index " << index << " uses an invalid slot " << vb.slot << "."
                   << std::endl;
                anyErrors = true;
            }
        }

        if (state.indirectParams && !state.indirectParams->GetDesc().isDrawIndirectArgs)
        {
            ss << "Cannot use buffer '" << Utils::DebugNameToString(state.indirectParams->GetDesc().debugName)
               << "' as a DrawIndirect argument buffer because it does not have the isDrawIndirectArgs flag set."
               << std::endl;
            anyErrors = true;
        }

        if (anyErrors)
        {
            Error(ss.str());
            return;
        }

        if (!ValidateBindingSetsAgainstLayouts(state.pipeline->GetDesc().bindingLayouts, state.bindings))
        {
            return;
        }

        if (state.framebuffer->GetFramebufferInfo() != state.pipeline->GetFramebufferInfo())
        {
            ss << "The framebuffer used in the draw call does not match the framebuffer used to create the pipeline."
               << std::endl
               << "Formats and sample counts of the framebuffers must match." << std::endl;
            anyErrors = true;
        }

        if (anyErrors)
        {
            Error(ss.str());
            return;
        }

        EvaluatePushConstantSize(state.pipeline->GetDesc().bindingLayouts);

        m_CommandList->SetGraphicsState(state);

        m_GraphicsStateSet = true;
        m_ComputeStateSet = false;
        m_MeshletStateSet = false;
        m_RayTracingStateSet = false;
        m_PushConstantsSet = false;
        m_CurrentGraphicsState = state;
    }

    void CommandListWrapper::Draw(const DrawArguments &args)
    {
        if (!RequireOpenState())
        {
            return;
        }

        if (!RequireType(CommandQueue::Graphics, "draw"))
        {
            return;
        }

        if (!m_GraphicsStateSet)
        {
            Error("Graphics state is not set before a draw call.\n"
                  "Note that setting compute state invalidates the graphics state.");
            return;
        }

        if (!ValidatePushConstants("graphics", "setGraphicsState"))
        {
            return;
        }

        m_CommandList->Draw(args);
    }

    void CommandListWrapper::DrawIndexed(const DrawArguments &args)
    {
        if (!RequireOpenState())
        {
            return;
        }

        if (!RequireType(CommandQueue::Graphics, "drawIndexed"))
        {
            return;
        }

        if (!m_GraphicsStateSet)
        {
            Error("Graphics state is not set before a drawIndexed call.\n"
                  "Note that setting compute state invalidates the graphics state.");
            return;
        }

        if (m_CurrentGraphicsState.indexBuffer.buffer == nullptr)
        {
            Error("Index buffer is not set before a drawIndexed call");
            return;
        }

        if (!ValidatePushConstants("graphics", "setGraphicsState"))
        {
            return;
        }

        const auto &index = m_CurrentGraphicsState.indexBuffer;
        const uint64_t stride = index.format == Format::R16_UINT ? 2u : 4u;
        const uint64_t start = uint64_t(index.offset) + uint64_t(args.startIndexLocation) * stride;
        if (!Input::CheckBufferRange(m_Device, index.buffer, start, uint64_t(args.vertexCount) * stride))
        {
            return;
        }
        m_CommandList->DrawIndexed(args);
    }

    bool CommandListWrapper::ValidateIndirectBuffer(Buffer *buffer, uint64_t offset, uint64_t count,
                                                    size_t stride) const
    {
        if (offset % 4 != 0 || (count != 0 && stride > std::numeric_limits<uint64_t>::max() / count))
        {
            Error("InvalidArgument: indirect offset/record size is invalid");
            return false;
        }
        if (!Input::CheckBufferRange(m_Device, buffer, offset, count * stride))
        {
            return false;
        }
        if (!buffer->GetDesc().isDrawIndirectArgs)
        {
            Error("InvalidArgument: indirect argument usage is missing");
            return false;
        }
        return true;
    }
    static_assert(sizeof(DrawIndirectArguments) == 16 && sizeof(DrawIndexedIndirectArguments) == 20 &&
                  sizeof(DispatchIndirectArguments) == 12);
    void CommandListWrapper::DrawIndirect(uint32_t offsetBytes, uint32_t drawCount)
    {
        if (!RequireOpenState())
        {
            return;
        }

        if (!RequireType(CommandQueue::Graphics, "drawIndirect"))
        {
            return;
        }

        if (!m_GraphicsStateSet)
        {
            Error("Graphics state is not set before a drawIndirect call.\n"
                  "Note that setting compute state invalidates the graphics state.");
            return;
        }

        if (!m_CurrentGraphicsState.indirectParams)
        {
            Error("Indirect params buffer is not set before a drawIndirect call.");
            return;
        }

        if (!ValidatePushConstants("graphics", "setGraphicsState"))
        {
            return;
        }

        if (!ValidateIndirectBuffer(m_CurrentGraphicsState.indirectParams, offsetBytes, drawCount,
                                    sizeof(DrawIndirectArguments)))
        {
            return;
        }

        m_CommandList->DrawIndirect(offsetBytes, drawCount);
    }

    void CommandListWrapper::DrawIndexedIndirect(uint32_t offsetBytes, uint32_t drawCount)
    {
        if (!RequireOpenState())
        {
            return;
        }

        if (!RequireType(CommandQueue::Graphics, "drawIndexedIndirect"))
        {
            return;
        }

        if (!m_GraphicsStateSet)
        {
            Error("Graphics state is not set before a drawIndexedIndirect call.\n"
                  "Note that setting compute state invalidates the graphics state.");
            return;
        }

        if (!m_CurrentGraphicsState.indirectParams)
        {
            Error("Indirect params buffer is not set before a drawIndexedIndirect call.");
            return;
        }

        if (!ValidatePushConstants("graphics", "setGraphicsState"))
        {
            return;
        }

        if (!ValidateIndirectBuffer(m_CurrentGraphicsState.indirectParams, offsetBytes, drawCount,
                                    sizeof(DrawIndexedIndirectArguments)))
        {
            return;
        }
        if (!m_CurrentGraphicsState.indexBuffer.buffer)
        {
            Error("InvalidArgument: indexed indirect draw requires an index buffer");
            return;
        }

        m_CommandList->DrawIndexedIndirect(offsetBytes, drawCount);
    }

    void CommandListWrapper::DrawIndexedIndirectCount(uint32_t paramOffsetBytes, uint32_t countOffsetBytes,
                                                      uint32_t maxDrawCount)
    {
        if (!RequireOpenState())
        {
            return;
        }

        if (!RequireType(CommandQueue::Graphics, "drawIndexedIndirectCount"))
        {
            return;
        }

        if (!m_GraphicsStateSet)
        {
            Error("Graphics state is not set before a drawIndexedIndirectCount call.\n"
                  "Note that setting compute state invalidates the graphics state.");
            return;
        }

        if (!m_CurrentGraphicsState.indirectParams)
        {
            Error("Indirect params buffer is not set before a drawIndexedIndirectCount call.");
            return;
        }

        if (!m_CurrentGraphicsState.indirectCountBuffer)
        {
            Error("Indirect count buffer is not set before a drawIndexedIndirectCount call.");
            return;
        }

        if (!ValidatePushConstants("graphics", "setGraphicsState"))
        {
            return;
        }

        if (!ValidateIndirectBuffer(m_CurrentGraphicsState.indirectParams, paramOffsetBytes, maxDrawCount,
                                    sizeof(DrawIndexedIndirectArguments)))
        {
            return;
        }
        if (!m_CurrentGraphicsState.indexBuffer.buffer)
        {
            Error("InvalidArgument: indexed indirect draw requires an index buffer");
            return;
        }
        if (!ValidateIndirectBuffer(m_CurrentGraphicsState.indirectCountBuffer, countOffsetBytes, 1, sizeof(uint32_t)))
        {
            return;
        }

        m_CommandList->DrawIndexedIndirectCount(paramOffsetBytes, countOffsetBytes, maxDrawCount);
    }

    void CommandListWrapper::SetComputeState(const ComputeState &state)
    {
        if (state.pipeline && Profile::RejectComputePipeline(m_Device, state.pipeline->GetDesc()))
        {
            return;
        }
        for (auto *bindings : state.bindings)
        {
            if (Profile::RejectBindingSet(m_Device, bindings))
            {
                return;
            }
        }
        if (!RequireOpenState())
        {
            return;
        }

        if (!RequireType(CommandQueue::Compute, "setComputeState"))
        {
            return;
        }

        if (!state.pipeline || !state.pipeline->GetDesc().CS)
        {
            Error("InvalidArgument: compute pipeline/shader is null");
            return;
        }
        if (!m_Device->ValidateShaderType(ShaderType::Compute, state.pipeline->GetDesc().CS->GetDesc(),
                                          "SetComputeState"))
        {
            return;
        }
        if (state.indirectParams && !Input::CheckBufferDesc(m_Device, state.indirectParams->GetDesc()))
        {
            return;
        }
        bool anyErrors = false;
        std::stringstream ss;
        ss << "setComputeState: " << std::endl;

        if (!state.pipeline)
        {
            ss << "pipeline is NULL." << std::endl;
            anyErrors = true;
        }

        if (state.indirectParams && !state.indirectParams->GetDesc().isDrawIndirectArgs)
        {
            ss << "Cannot use buffer '" << Utils::DebugNameToString(state.indirectParams->GetDesc().debugName)
               << "' as a DispatchIndirect argument buffer because it does not have the isDrawIndirectArgs flag set."
               << std::endl;
            anyErrors = true;
        }

        if (anyErrors)
        {
            Error(ss.str());
            return;
        }

        if (anyErrors)
        {
            return;
        }

        if (!ValidateBindingSetsAgainstLayouts(state.pipeline->GetDesc().bindingLayouts, state.bindings))
        {
            anyErrors = true;
        }

        if (anyErrors)
        {
            return;
        }

        EvaluatePushConstantSize(state.pipeline->GetDesc().bindingLayouts);

        m_CommandList->SetComputeState(state);

        m_GraphicsStateSet = false;
        m_ComputeStateSet = true;
        m_MeshletStateSet = false;
        m_RayTracingStateSet = false;
        m_PushConstantsSet = false;
        m_CurrentComputeState = state;
    }

    void CommandListWrapper::Dispatch(uint32_t groupsX, uint32_t groupsY /*= 1*/, uint32_t groupsZ /*= 1*/)
    {
        if (!RequireOpenState())
        {
            return;
        }

        if (!RequireType(CommandQueue::Compute, "dispatch"))
        {
            return;
        }

        if (!m_ComputeStateSet)
        {
            Error("Compute state is not set before a dispatch call.\n"
                  "Note that setting graphics state invalidates the compute state.");
            return;
        }

        if (!ValidatePushConstants("compute", "setComputeState"))
        {
            return;
        }

        m_CommandList->Dispatch(groupsX, groupsY, groupsZ);
    }

    void CommandListWrapper::DispatchIndirect(uint32_t offsetBytes)
    {
        if (!RequireOpenState())
        {
            return;
        }

        if (!RequireType(CommandQueue::Compute, "dispatchIndirect"))
        {
            return;
        }

        if (!m_ComputeStateSet)
        {
            Error("Compute state is not set before a dispatchIndirect call.\n"
                  "Note that setting graphics state invalidates the compute state.");
            return;
        }

        if (!m_CurrentComputeState.indirectParams)
        {
            Error("Indirect params buffer is not set before a dispatchIndirect call.");
            return;
        }

        if (!ValidatePushConstants("compute", "setComputeState"))
        {
            return;
        }

        if (!ValidateIndirectBuffer(m_CurrentComputeState.indirectParams, offsetBytes, 1,
                                    sizeof(DispatchIndirectArguments)))
        {
            return;
        }

        m_CommandList->DispatchIndirect(offsetBytes);
    }

    void CommandListWrapper::SetMeshletState(const MeshletState &state)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return;
        }

        if (!RequireOpenState())
        {
            return;
        }

        if (!RequireType(CommandQueue::Graphics, "setMeshletState"))
        {
            return;
        }

        bool anyErrors = false;
        if (!state.pipeline)
        {
            Error("MeshletState::pipeline is NULL");
            anyErrors = true;
        }

        if (anyErrors)
        {
            return;
        }

        if (!ValidateBindingSetsAgainstLayouts(state.pipeline->GetDesc().bindingLayouts, state.bindings))
        {
            anyErrors = true;
        }

        if (anyErrors)
        {
            return;
        }

        EvaluatePushConstantSize(state.pipeline->GetDesc().bindingLayouts);

        m_CommandList->SetMeshletState(state);

        m_GraphicsStateSet = false;
        m_ComputeStateSet = false;
        m_MeshletStateSet = true;
        m_RayTracingStateSet = false;
        m_PushConstantsSet = false;
        m_CurrentMeshletState = state;
    }

    void CommandListWrapper::DispatchMesh(uint32_t groupsX, uint32_t groupsY /*= 1*/, uint32_t groupsZ /*= 1*/)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return;
        }

        if (!RequireOpenState())
        {
            return;
        }

        if (!RequireType(CommandQueue::Graphics, "dispatchMesh"))
        {
            return;
        }

        if (!m_MeshletStateSet)
        {
            Error("Meshlet state is not set before a dispatchMesh call.\n"
                  "Note that setting graphics or compute state invalidates the meshlet state.");
            return;
        }

        if (!ValidatePushConstants("meshlet", "setMeshletState"))
        {
            return;
        }

        m_CommandList->DispatchMesh(groupsX, groupsY, groupsZ);
    }

    void CommandListWrapper::DispatchMeshIndirect(uint32_t offsetBytes, uint32_t maxDrawCount)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return;
        }

        if (!RequireOpenState())
        {
            return;
        }

        if (!RequireType(CommandQueue::Graphics, "dispatchMesh"))
        {
            return;
        }

        if (!m_MeshletStateSet)
        {
            Error("Meshlet state is not set before a dispatchMesh call.\n"
                  "Note that setting graphics or compute state invalidates the meshlet state.");
            return;
        }

        if (!ValidatePushConstants("meshlet", "setMeshletState"))
        {
            return;
        }

        if (!m_CurrentMeshletState.indirectParams)
        {
            Error("Indirect params buffer is not set before a dispatchMeshIndirect call.");
            return;
        }

        m_CommandList->DispatchMeshIndirect(offsetBytes, maxDrawCount);
    }

    void CommandListWrapper::DispatchMeshIndirectCount(uint32_t paramOffsetBytes, uint32_t countOffsetBytes,
                                                       uint32_t maxDrawCount)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return;
        }

        if (!RequireOpenState())
        {
            return;
        }

        if (!RequireType(CommandQueue::Graphics, "dispatchMesh"))
        {
            return;
        }

        if (!m_MeshletStateSet)
        {
            Error("Meshlet state is not set before a dispatchMesh call.\n"
                  "Note that setting graphics or compute state invalidates the meshlet state.");
            return;
        }

        if (!ValidatePushConstants("meshlet", "setMeshletState"))
        {
            return;
        }

        if (!m_CurrentMeshletState.indirectParams)
        {
            Error("Indirect params buffer is not set before a dispatchMeshIndirectCount call.");
            return;
        }

        if (!m_CurrentMeshletState.indirectCountBuffer)
        {
            Error("Indirect count buffer is not set before a dispatchMeshIndirectCount call.");
            return;
        }

        m_CommandList->DispatchMeshIndirectCount(paramOffsetBytes, countOffsetBytes, maxDrawCount);
    }

    void CommandListWrapper::BeginTimerQuery(TimerQuery *query)
    {
        if (!RequireOpenState())
        {
            return;
        }
        if (!query)
        {
            Error("InvalidArgument: timer query is null");
            return;
        }

        m_CommandList->BeginTimerQuery(query);
    }

    void CommandListWrapper::EndTimerQuery(TimerQuery *query)
    {
        if (!RequireOpenState())
        {
            return;
        }
        if (!query)
        {
            Error("InvalidArgument: timer query is null");
            return;
        }

        m_CommandList->EndTimerQuery(query);
    }

    void CommandListWrapper::BeginMarker(const char *name)
    {
        if (!RequireOpenState())
        {
            return;
        }
        if (!name)
        {
            Error("InvalidArgument: marker name is null");
            return;
        }

        m_CommandList->BeginMarker(name);
    }

    void CommandListWrapper::EndMarker()
    {
        if (!RequireOpenState())
        {
            return;
        }

        m_CommandList->EndMarker();
    }

    void CommandListWrapper::SetEnableAutomaticBarriers(bool enable)
    {
        if (!RequireOpenState())
        {
            return;
        }

        m_CommandList->SetEnableAutomaticBarriers(enable);
    }

    void CommandListWrapper::SetResourceStatesForBindingSet(BindingSet *bindingSet)
    {
        if (Profile::RejectBindingSet(m_Device, bindingSet))
        {
            return;
        }
        if (!RequireOpenState())
        {
            return;
        }

        if (!bindingSet || !bindingSet->GetLayout())
        {
            Error("InvalidArgument: binding set/layout is null");
            return;
        }
        if (const auto *desc = bindingSet->GetDesc())
        {
            if (!m_Device->ValidateBindingSetDesc(*desc, bindingSet->GetLayout()))
            {
                return;
            }
        }
        m_CommandList->SetResourceStatesForBindingSet(bindingSet);
    }

    void CommandListWrapper::SetEnableUavBarriersForTexture(Texture *texture, bool enableBarriers)
    {
        if (!RequireOpenState())
        {
            return;
        }
        if (!Input::CheckTextureRange(m_Device, texture, AllSubresources))
        {
            return;
        }

        if (!RequireType(CommandQueue::Compute, "setEnableUavBarriersForTexture"))
        {
            return;
        }

        m_CommandList->SetEnableUavBarriersForTexture(texture, enableBarriers);
    }

    void CommandListWrapper::SetEnableUavBarriersForBuffer(Buffer *buffer, bool enableBarriers)
    {
        if (!RequireOpenState())
        {
            return;
        }
        if (!buffer)
        {
            Error("InvalidArgument: buffer is null");
            return;
        }
        if (!Input::CheckBufferDesc(m_Device, buffer->GetDesc()))
        {
            return;
        }

        if (!RequireType(CommandQueue::Compute, "setEnableUavBarriersForBuffer"))
        {
            return;
        }

        m_CommandList->SetEnableUavBarriersForBuffer(buffer, enableBarriers);
    }

    void CommandListWrapper::BeginTrackingTextureState(Texture *texture, TextureSubresourceSet subresources,
                                                       ResourceStates stateBits)
    {
        if ((stateBits & Profile::OpenGLExcludedStates) != 0 && Profile::RejectOpenGL(m_Device))
        {
            return;
        }

        if (!RequireOpenState())
        {
            return;
        }
        if (!Input::CheckTextureRange(m_Device, texture, subresources))
        {
            return;
        }
        if (!Input::CheckState(m_Device, stateBits))
        {
            return;
        }

        m_CommandList->BeginTrackingTextureState(texture, subresources, stateBits);
    }

    void CommandListWrapper::BeginTrackingBufferState(Buffer *buffer, ResourceStates stateBits)
    {
        if ((stateBits & Profile::OpenGLExcludedStates) != 0 && Profile::RejectOpenGL(m_Device))
        {
            return;
        }

        if (!RequireOpenState())
        {
            return;
        }
        if (!buffer)
        {
            Error("InvalidArgument: buffer is null");
            return;
        }
        if (!Input::CheckBufferDesc(m_Device, buffer->GetDesc()))
        {
            return;
        }
        if (!Input::CheckState(m_Device, stateBits))
        {
            return;
        }

        m_CommandList->BeginTrackingBufferState(buffer, stateBits);
    }

    void CommandListWrapper::SetTextureState(Texture *texture, TextureSubresourceSet subresources,
                                             ResourceStates stateBits)
    {
        if ((stateBits & Profile::OpenGLExcludedStates) != 0 && Profile::RejectOpenGL(m_Device))
        {
            return;
        }

        if (!RequireOpenState())
        {
            return;
        }
        if (!Input::CheckTextureRange(m_Device, texture, subresources))
        {
            return;
        }
        if (!Input::CheckState(m_Device, stateBits))
        {
            return;
        }

        m_CommandList->SetTextureState(texture, subresources, stateBits);
    }

    void CommandListWrapper::SetBufferState(Buffer *buffer, ResourceStates stateBits)
    {
        if ((stateBits & Profile::OpenGLExcludedStates) != 0 && Profile::RejectOpenGL(m_Device))
        {
            return;
        }

        if (!RequireOpenState())
        {
            return;
        }
        if (!buffer)
        {
            Error("InvalidArgument: buffer is null");
            return;
        }
        if (!Input::CheckBufferDesc(m_Device, buffer->GetDesc()))
        {
            return;
        }
        if (!Input::CheckState(m_Device, stateBits))
        {
            return;
        }

        m_CommandList->SetBufferState(buffer, stateBits);
    }

    void CommandListWrapper::SetAccelStructState(RT::AccelStruct *as, ResourceStates stateBits)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return;
        }

        if (!RequireOpenState())
        {
            return;
        }

        m_CommandList->SetAccelStructState(CheckedCast<RT::AccelStruct *>(UnwrapResource(as)), stateBits);
    }

    void CommandListWrapper::SetPermanentTextureState(Texture *texture, ResourceStates stateBits)
    {
        if ((stateBits & Profile::OpenGLExcludedStates) != 0 && Profile::RejectOpenGL(m_Device))
        {
            return;
        }

        if (!RequireOpenState())
        {
            return;
        }
        if (!Input::CheckTextureRange(m_Device, texture, AllSubresources))
        {
            return;
        }
        if (!Input::CheckState(m_Device, stateBits))
        {
            return;
        }

        m_CommandList->SetPermanentTextureState(texture, stateBits);
    }

    void CommandListWrapper::SetPermanentBufferState(Buffer *buffer, ResourceStates stateBits)
    {
        if ((stateBits & Profile::OpenGLExcludedStates) != 0 && Profile::RejectOpenGL(m_Device))
        {
            return;
        }

        if (!RequireOpenState())
        {
            return;
        }
        if (!buffer)
        {
            Error("InvalidArgument: buffer is null");
            return;
        }
        if (!Input::CheckBufferDesc(m_Device, buffer->GetDesc()))
        {
            return;
        }
        if (!Input::CheckState(m_Device, stateBits))
        {
            return;
        }

        m_CommandList->SetPermanentBufferState(buffer, stateBits);
    }

    void CommandListWrapper::CommitBarriers()
    {
        if (!RequireOpenState())
        {
            return;
        }

        m_CommandList->CommitBarriers();
    }

    ResourceStates CommandListWrapper::GetTextureSubresourceState(Texture *texture, ArraySlice arraySlice,
                                                                  MipLevel mipLevel)
    {
        if (!RequireOpenState())
        {
            return ResourceStates::Unknown;
        }
        if (!Input::CheckTextureRange(m_Device, texture, TextureSubresourceSet(mipLevel, 1, arraySlice, 1)))
        {
            return ResourceStates::Unknown;
        }

        return m_CommandList->GetTextureSubresourceState(texture, arraySlice, mipLevel);
    }

    ResourceStates CommandListWrapper::GetBufferState(Buffer *buffer)
    {
        if (!RequireOpenState())
        {
            return ResourceStates::Unknown;
        }
        if (!buffer)
        {
            Error("InvalidArgument: buffer is null");
            return ResourceStates::Unknown;
        }
        if (!Input::CheckBufferDesc(m_Device, buffer->GetDesc()))
        {
            return ResourceStates::Unknown;
        }

        return m_CommandList->GetBufferState(buffer);
    }

    void CommandListWrapper::ClearState()
    {
        if (!RequireOpenState())
        {
            return;
        }

        m_GraphicsStateSet = false;
        m_ComputeStateSet = false;
        m_MeshletStateSet = false;
        m_RayTracingStateSet = false;
        m_PushConstantsSet = false;

        m_CommandList->ClearState();
    }

    Device *CommandListWrapper::GetDevice()
    {
        return m_Device;
    }

    const CommandListParameters &CommandListWrapper::GetDesc()
    {
        return m_CommandList->GetDesc();
    }

    void CommandListWrapper::SetRayTracingState(const RT::State &state)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return;
        }

        if (!RequireOpenState())
        {
            return;
        }

        if (!RequireType(CommandQueue::Compute, "setRayTracingState"))
        {
            return;
        }

        EvaluatePushConstantSize(state.shaderTable->GetPipeline()->GetDesc().globalBindingLayouts);

        m_CommandList->SetRayTracingState(state);

        m_GraphicsStateSet = false;
        m_ComputeStateSet = false;
        m_MeshletStateSet = true;
        m_RayTracingStateSet = true;
        m_PushConstantsSet = false;
        m_CurrentRayTracingState = state;
    }

    void CommandListWrapper::DispatchRays(const RT::DispatchRaysArguments &args)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return;
        }

        if (!RequireOpenState())
        {
            return;
        }

        if (!RequireType(CommandQueue::Compute, "dispatchRays"))
        {
            return;
        }

        if (!m_RayTracingStateSet)
        {
            Error("Ray tracing state is not set before a dispatchRays call.\n"
                  "Note that setting graphics or compute state invalidates the ray tracing state.");
            return;
        }

        if (!ValidatePushConstants("ray tracing", "setRayTracingState"))
        {
            return;
        }

        m_CommandList->DispatchRays(args);
    }

    void CommandListWrapper::CompactBottomLevelAccelStructs()
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return;
        }

        if (!RequireOpenState())
        {
            return;
        }

        if (!RequireType(CommandQueue::Compute, "compactBottomLevelAccelStructs"))
        {
            return;
        }

        m_CommandList->CompactBottomLevelAccelStructs();
    }

    void CommandListWrapper::CopyRaytracingAccelerationStructure(RT::AccelStruct *destination, RT::AccelStruct *source)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return;
        }

        if (!RequireOpenState())
        {
            return;
        }

        if (!RequireType(CommandQueue::Compute, "copyRaytracingAccelerationStructure"))
        {
            return;
        }

        RT::AccelStruct *underlyingDst = destination;
        RT::AccelStruct *underlyingSrc = source;

        AccelStructWrapper *dstWrapper = dynamic_cast<AccelStructWrapper *>(destination);
        if (dstWrapper)
        {
            underlyingDst = dstWrapper->GetUnderlyingObject();
        }

        AccelStructWrapper *srcWrapper = dynamic_cast<AccelStructWrapper *>(source);
        if (srcWrapper)
        {
            underlyingSrc = srcWrapper->GetUnderlyingObject();
        }

        m_CommandList->CopyRaytracingAccelerationStructure(underlyingDst, underlyingSrc);
    }

    void CommandListWrapper::BuildOpacityMicromap(RT::OpacityMicromap *omm, const RT::OpacityMicromapDesc &desc)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return;
        }

        if (!RequireOpenState())
        {
            return;
        }

        if (!RequireType(CommandQueue::Compute, "buildOpacityMicromap"))
        {
            return;
        }

        m_CommandList->BuildOpacityMicromap(omm, desc);
    }

    void CommandListWrapper::BuildBottomLevelAccelStruct(RT::AccelStruct *as, const RT::GeometryDesc *pGeometries,
                                                         size_t numGeometries, RT::AccelStructBuildFlags buildFlags)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return;
        }

        if (!RequireOpenState())
        {
            return;
        }

        if (!RequireType(CommandQueue::Compute, "buildBottomLevelAccelStruct"))
        {
            return;
        }

        RT::AccelStruct *underlyingAS = as;

        AccelStructWrapper *wrapper = dynamic_cast<AccelStructWrapper *>(as);
        if (wrapper)
        {
            underlyingAS = wrapper->GetUnderlyingObject();

            if (wrapper->isTopLevel)
            {
                Error("Cannot perform buildBottomLevelAccelStruct on a top-level AS");
                return;
            }

            for (size_t i = 0; i < numGeometries; i++)
            {
                const auto &geom = pGeometries[i];

                if (geom.geometryType == RT::GeometryType::Triangles)
                {
                    const auto &triangles = geom.geometryData.triangles;

                    if (triangles.indexFormat != Format::UNKNOWN)
                    {
                        switch (triangles.indexFormat) // NOLINT(clang-diagnostic-switch-enum)
                        {
                        case Format::R8_UINT:
                            if (m_Device->GetGraphicsAPI() != GraphicsAPI::VULKAN)
                            {
                                std::stringstream ss;
                                ss << "BLAS " << Utils::DebugNameToString(as->GetDesc().debugName) << " build geometry "
                                   << i << " has index format R8_UINT which is only supported on Vulkan";
                                Error(ss.str());
                                return;
                            }
                            break;
                        case Format::R16_UINT:
                        case Format::R32_UINT:
                            break;
                        default:
                        {
                            std::stringstream ss;
                            ss << "BLAS " << Utils::DebugNameToString(as->GetDesc().debugName) << " build geometry "
                               << i
                               << " has unsupported index format: " << Utils::FormatToString(triangles.indexFormat);
                            Error(ss.str());
                            return;
                        }
                        }

                        if (triangles.indexBuffer == nullptr)
                        {
                            std::stringstream ss;
                            ss << "BLAS " << Utils::DebugNameToString(as->GetDesc().debugName) << " build geometry "
                               << i << " has a NULL index buffer but indexFormat is "
                               << Utils::FormatToString(triangles.indexFormat);
                            Error(ss.str());
                            return;
                        }

                        const BufferDesc &indexBufferDesc = triangles.indexBuffer->GetDesc();
                        if (!indexBufferDesc.isAccelStructBuildInput)
                        {
                            std::stringstream ss;
                            ss << "BLAS " << Utils::DebugNameToString(as->GetDesc().debugName) << " build geometry "
                               << i << " has index buffer = " << Utils::DebugNameToString(indexBufferDesc.debugName)
                               << " which does not have the isAccelStructBuildInput flag set";
                            Error(ss.str());
                            return;
                        }

                        const size_t indexSize =
                            triangles.indexCount * GetFormatInfo(triangles.indexFormat).bytesPerBlock;
                        if (triangles.indexOffset + indexSize > indexBufferDesc.byteSize)
                        {
                            std::stringstream ss;
                            ss << "BLAS " << Utils::DebugNameToString(as->GetDesc().debugName) << " build geometry "
                               << i << " points at " << indexSize << " bytes of index data at offset "
                               << triangles.indexOffset << " in buffer "
                               << Utils::DebugNameToString(indexBufferDesc.debugName) << " whose size is "
                               << indexBufferDesc.byteSize << ", which will result in a buffer overrun";
                            Error(ss.str());
                            return;
                        }

                        if ((triangles.indexCount % 3) != 0)
                        {
                            std::stringstream ss;
                            ss << "BLAS " << Utils::DebugNameToString(as->GetDesc().debugName) << " build geometry "
                               << i << " has indexCount = " << triangles.indexCount << ", which is not a multiple of 3";
                            Error(ss.str());
                            return;
                        }
                    }
                    else
                    {
                        if (triangles.indexCount != 0 || triangles.indexBuffer != nullptr)
                        {
                            std::stringstream ss;
                            ss << "BLAS " << Utils::DebugNameToString(as->GetDesc().debugName) << " build geometry "
                               << i << " has indexFormat = UNKNOWN but nonzero indexCount = " << triangles.indexCount;
                            Error(ss.str());
                            return;
                        }

                        if (triangles.indexBuffer != nullptr)
                        {
                            std::stringstream ss;
                            ss << "BLAS " << Utils::DebugNameToString(as->GetDesc().debugName) << " build geometry "
                               << i << " has indexFormat = UNKNOWN but non-NULL indexBuffer = "
                               << Utils::DebugNameToString(triangles.indexBuffer->GetDesc().debugName);
                            Error(ss.str());
                            return;
                        }
                    }

                    switch (triangles.vertexFormat) // NOLINT(clang-diagnostic-switch-enum)
                    {
                    case Format::RG32_FLOAT:
                    case Format::RGB32_FLOAT:
                    case Format::RGBA32_FLOAT:
                    case Format::RG16_FLOAT:
                    case Format::RGBA16_FLOAT:
                    case Format::RG16_SNORM:
                    case Format::RGBA16_SNORM:
                    case Format::RGBA16_UNORM:
                    case Format::RG16_UNORM:
                    case Format::R10G10B10A2_UNORM:
                    case Format::RGBA8_UNORM:
                    case Format::RG8_UNORM:
                    case Format::RGBA8_SNORM:
                    case Format::RG8_SNORM:
                        break;
                    default:
                    {
                        std::stringstream ss;
                        ss << "BLAS " << Utils::DebugNameToString(as->GetDesc().debugName) << " build geometry " << i
                           << " has unsupported vertex format: " << Utils::FormatToString(triangles.vertexFormat);
                        Error(ss.str());
                        return;
                    }
                    }

                    if (triangles.vertexBuffer == nullptr)
                    {
                        std::stringstream ss;
                        ss << "BLAS " << Utils::DebugNameToString(as->GetDesc().debugName) << " build geometry " << i
                           << " has NULL vertex buffer";
                        Error(ss.str());
                        return;
                    }

                    if (triangles.vertexStride == 0)
                    {
                        std::stringstream ss;
                        ss << "BLAS " << Utils::DebugNameToString(as->GetDesc().debugName) << " build geometry " << i
                           << " has vertexStride = 0";
                        Error(ss.str());
                        return;
                    }

                    if ((triangles.indexFormat == Format::UNKNOWN) && (triangles.vertexCount % 3) != 0)
                    {
                        std::stringstream ss;
                        ss << "BLAS " << Utils::DebugNameToString(as->GetDesc().debugName) << " build geometry " << i
                           << " has indexFormat = UNKNOWN and vertexCount = " << triangles.vertexCount
                           << ", which is not a multiple of 3";
                        Error(ss.str());
                        return;
                    }

                    const BufferDesc &vertexBufferDesc = triangles.vertexBuffer->GetDesc();
                    if (!vertexBufferDesc.isAccelStructBuildInput)
                    {
                        std::stringstream ss;
                        ss << "BLAS " << Utils::DebugNameToString(as->GetDesc().debugName) << " build geometry " << i
                           << " has vertex buffer = " << Utils::DebugNameToString(vertexBufferDesc.debugName)
                           << " which does not have the isAccelStructBuildInput flag set";
                        Error(ss.str());
                        return;
                    }

                    const size_t vertexDataSize = triangles.vertexCount * triangles.vertexStride;
                    if (triangles.vertexOffset + vertexDataSize > vertexBufferDesc.byteSize)
                    {
                        std::stringstream ss;
                        ss << "BLAS " << Utils::DebugNameToString(as->GetDesc().debugName) << " build geometry " << i
                           << " points at " << vertexDataSize << " bytes of vertex data at offset "
                           << triangles.vertexOffset << " in buffer "
                           << Utils::DebugNameToString(vertexBufferDesc.debugName) << " whose size is "
                           << vertexBufferDesc.byteSize << ", which will result in a buffer overrun";
                        Error(ss.str());
                        return;
                    }
                }
                else if (geom.geometryType == RT::GeometryType::AABBs)
                {
                    const auto &aabbs = geom.geometryData.aabbs;

                    if (aabbs.buffer == nullptr)
                    {
                        std::stringstream ss;
                        ss << "BLAS " << Utils::DebugNameToString(as->GetDesc().debugName) << " build geometry " << i
                           << " has NULL AABB data buffer";
                        Error(ss.str());
                        return;
                    }

                    const BufferDesc &aabbBufferDesc = aabbs.buffer->GetDesc();
                    if (!aabbBufferDesc.isAccelStructBuildInput)
                    {
                        std::stringstream ss;
                        ss << "BLAS " << Utils::DebugNameToString(as->GetDesc().debugName) << " build geometry " << i
                           << " has AABB data buffer = " << Utils::DebugNameToString(aabbBufferDesc.debugName)
                           << " which does not have the isAccelStructBuildInput flag set";
                        Error(ss.str());
                        return;
                    }

                    if (aabbs.count > 1 && aabbs.stride < sizeof(RT::GeometryAABB))
                    {
                        std::stringstream ss;
                        ss << "BLAS " << Utils::DebugNameToString(as->GetDesc().debugName) << " build geometry " << i
                           << " has AABB stride = " << aabbs.stride << " which is less than the size of one AABB ("
                           << sizeof(RT::GeometryAABB) << " bytes)";
                        Error(ss.str());
                        return;
                    }

                    const size_t aabbDataSize = aabbs.count * aabbs.stride;
                    if (aabbs.offset + aabbDataSize > aabbBufferDesc.byteSize)
                    {
                        std::stringstream ss;
                        ss << "BLAS " << Utils::DebugNameToString(as->GetDesc().debugName) << " build geometry " << i
                           << " points at " << aabbDataSize << " bytes of AABB data at offset " << aabbs.offset
                           << " in buffer " << Utils::DebugNameToString(aabbBufferDesc.debugName) << " whose size is "
                           << aabbBufferDesc.byteSize << ", which will result in a buffer overrun";
                        Error(ss.str());
                        return;
                    }

                    if (geom.useTransform)
                    {
                        std::stringstream ss;
                        ss << "BLAS " << Utils::DebugNameToString(as->GetDesc().debugName) << " build geometry " << i
                           << " is of type AABB but has useTransform = true, "
                              "which is unsupported, and the transform will be ignored";
                        m_MessageCallback->Message(MessageSeverity::Warning, ss.str().c_str());
                    }
                }
                else if (geom.geometryType == RT::GeometryType::Spheres)
                {
                    const auto &spheres = geom.geometryData.spheres;

                    if (spheres.vertexBuffer == nullptr)
                    {
                        std::stringstream ss;
                        ss << "BLAS " << Utils::DebugNameToString(as->GetDesc().debugName) << " build geometry " << i
                           << " has NULL vertex buffer";
                        Error(ss.str());
                        return;
                    }

                    // TODO: Add more validation
                }
                else if (geom.geometryType == RT::GeometryType::Lss)
                {
                    const auto &lss = geom.geometryData.lss;

                    if (lss.vertexBuffer == nullptr)
                    {
                        std::stringstream ss;
                        ss << "BLAS " << Utils::DebugNameToString(as->GetDesc().debugName) << " build geometry " << i
                           << " has NULL vertex buffer";
                        Error(ss.str());
                        return;
                    }

                    // TODO: Add more validation
                }
            }

            if ((buildFlags & RT::AccelStructBuildFlags::PerformUpdate) != 0)
            {
                if (!wrapper->allowUpdate)
                {
                    std::stringstream ss;
                    ss << "Cannot perform an update on BLAS " << Utils::DebugNameToString(as->GetDesc().debugName)
                       << " that was not created with the AllowUpdate flag";
                    Error(ss.str());
                    return;
                }

                if (!wrapper->wasBuilt)
                {
                    std::stringstream ss;
                    ss << "Cannot perform an update on BLAS " << Utils::DebugNameToString(as->GetDesc().debugName)
                       << " before the same BLAS was initially built";
                    Error(ss.str());
                    return;
                }

                if (numGeometries != wrapper->buildGeometries.size())
                {
                    std::stringstream ss;
                    ss << "Cannot perform an update on BLAS " << Utils::DebugNameToString(as->GetDesc().debugName)
                       << " with " << numGeometries
                       << " geometries "
                          "when this BLAS was built with "
                       << wrapper->buildGeometries.size() << " geometries";
                    Error(ss.str());
                    return;
                }

                for (size_t i = 0; i < numGeometries; i++)
                {
                    const auto &before = wrapper->buildGeometries[i];
                    const auto &after = pGeometries[i];

                    if (before.geometryType != after.geometryType)
                    {
                        std::stringstream ss;
                        ss << "Cannot perform an update on BLAS " << Utils::DebugNameToString(as->GetDesc().debugName)
                           << " with mismatching geometry types in slot " << i;
                        Error(ss.str());
                        return;
                    }

                    if (before.geometryType == RT::GeometryType::Triangles)
                    {
                        uint32_t primitivesBefore = (before.geometryData.triangles.vertexFormat == Format::UNKNOWN)
                                                        ? before.geometryData.triangles.vertexCount
                                                        : before.geometryData.triangles.indexCount;

                        uint32_t primitivesAfter = (after.geometryData.triangles.vertexFormat == Format::UNKNOWN)
                                                       ? after.geometryData.triangles.vertexCount
                                                       : after.geometryData.triangles.indexCount;

                        primitivesBefore /= 3;
                        primitivesAfter /= 3;

                        if (primitivesBefore != primitivesAfter)
                        {
                            std::stringstream ss;
                            ss << "Cannot perform an update on BLAS "
                               << Utils::DebugNameToString(as->GetDesc().debugName)
                               << " with mismatching triangle counts in geometry slot " << i
                               << ": "
                                  "built with "
                               << primitivesBefore << " triangles, updating with " << primitivesAfter << " triangles";
                            Error(ss.str());
                            return;
                        }
                    }
                    else // AABBs
                    {
                        uint32_t aabbsBefore = before.geometryData.aabbs.count;
                        uint32_t aabbsAfter = after.geometryData.aabbs.count;

                        if (aabbsBefore != aabbsAfter)
                        {
                            std::stringstream ss;
                            ss << "Cannot perform an update on BLAS "
                               << Utils::DebugNameToString(as->GetDesc().debugName)
                               << " with mismatching AABB counts in geometry slot " << i
                               << ": "
                                  "built with "
                               << aabbsBefore << " AABBs, updating with " << aabbsAfter << " AABBs";
                            Error(ss.str());
                            return;
                        }
                    }
                }
            }

            if (wrapper->allowCompaction && wrapper->wasBuilt)
            {
                std::stringstream ss;
                ss << "Cannot rebuild BLAS " << Utils::DebugNameToString(as->GetDesc().debugName)
                   << " that has the AllowCompaction flag set";
                Error(ss.str());
                return;
            }

            wrapper->wasBuilt = true;
            wrapper->buildGeometries.assign(pGeometries, pGeometries + numGeometries);
        }

        m_CommandList->BuildBottomLevelAccelStruct(underlyingAS, pGeometries, numGeometries, buildFlags);
    }

    bool CommandListWrapper::ValidateBuildTopLevelAccelStruct(AccelStructWrapper *wrapper, size_t numInstances,
                                                              RT::AccelStructBuildFlags buildFlags) const
    {
        if (!wrapper->isTopLevel)
        {
            std::stringstream ss;
            ss << "Cannot perform buildTopLevelAccelStruct on a bottom-level AS "
               << Utils::DebugNameToString(wrapper->GetDesc().debugName);
            Error(ss.str());
            return false;
        }

        if (numInstances > wrapper->maxInstances)
        {
            std::stringstream ss;
            ss << "Cannot build TLAS " << Utils::DebugNameToString(wrapper->GetDesc().debugName) << " with "
               << numInstances
               << " instances which is greater than topLevelMaxInstances "
                  " specified at creation ("
               << wrapper->maxInstances << ")";
            Error(ss.str());
            return false;
        }

        if ((buildFlags & RT::AccelStructBuildFlags::PerformUpdate) != 0)
        {
            if (!wrapper->allowUpdate)
            {
                std::stringstream ss;
                ss << "Cannot perform an update on TLAS " << Utils::DebugNameToString(wrapper->GetDesc().debugName)
                   << " that was not created with the ALLOW_UPDATE flag";
                Error(ss.str());
                return false;
            }

            if (!wrapper->wasBuilt)
            {
                std::stringstream ss;
                ss << "Cannot perform an update on TLAS " << Utils::DebugNameToString(wrapper->GetDesc().debugName)
                   << " before the same TLAS was initially built";
                Error(ss.str());
                return false;
            }

            if (wrapper->buildInstances != numInstances)
            {
                std::stringstream ss;
                ss << "Cannot perform an update on TLAS " << Utils::DebugNameToString(wrapper->GetDesc().debugName)
                   << " with " << numInstances << " instances when this TLAS was built with " << wrapper->buildInstances
                   << " instances";
                Error(ss.str());
                return false;
            }
        }

        return true;
    }

    void CommandListWrapper::BuildTopLevelAccelStruct(RT::AccelStruct *as, const RT::InstanceDesc *pInstances,
                                                      size_t numInstances, RT::AccelStructBuildFlags buildFlags)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return;
        }

        if (!RequireOpenState())
        {
            return;
        }

        if (!RequireType(CommandQueue::Compute, "buildTopLevelAccelStruct"))
        {
            return;
        }

        if (!as)
        {
            Error("buildTopLevelAccelStruct: 'as' is NULL");
            return;
        }

        std::vector<RT::InstanceDesc> patchedInstances;
        patchedInstances.assign(pInstances, pInstances + numInstances);

        for (auto &instance : patchedInstances)
        {
            instance.bottomLevelAS = CheckedCast<RT::AccelStruct *>(UnwrapResource(instance.bottomLevelAS));
        }

        RT::AccelStruct *underlyingAS = as;

        AccelStructWrapper *wrapper = dynamic_cast<AccelStructWrapper *>(as);
        if (wrapper)
        {
            underlyingAS = wrapper->GetUnderlyingObject();

            if (!ValidateBuildTopLevelAccelStruct(wrapper, numInstances, buildFlags))
            {
                return;
            }

            const bool allowEmptyInstances = (buildFlags & RT::AccelStructBuildFlags::AllowEmptyInstances) != 0;

            for (size_t i = 0; i < numInstances; i++)
            {
                const auto &instance = pInstances[i];

                if (instance.bottomLevelAS == nullptr)
                {
                    if (allowEmptyInstances)
                    {
                        continue;
                    }
                    else
                    {
                        std::stringstream ss;
                        ss << "TLAS " << Utils::DebugNameToString(as->GetDesc().debugName) << " build instance " << i
                           << " has a NULL bottomLevelAS";
                        Error(ss.str());
                        return;
                    }
                }

                AccelStructWrapper *blasWrapper = dynamic_cast<AccelStructWrapper *>(instance.bottomLevelAS);
                if (blasWrapper)
                {
                    if (blasWrapper->isTopLevel)
                    {
                        std::stringstream ss;
                        ss << "TLAS " << Utils::DebugNameToString(as->GetDesc().debugName) << " build instance " << i
                           << " refers to another TLAS, which is unsupported";
                        Error(ss.str());
                        return;
                    }

                    if (!blasWrapper->wasBuilt)
                    {
                        std::stringstream ss;
                        ss << "TLAS " << Utils::DebugNameToString(as->GetDesc().debugName) << " build instance " << i
                           << " refers to a BLAS which was never built";
                        Error(ss.str());
                        return;
                    }
                }

                if (instance.instanceMask == 0 && !allowEmptyInstances)
                {
                    std::stringstream ss;
                    ss << "TLAS " << Utils::DebugNameToString(as->GetDesc().debugName) << " build instance " << i
                       << " has instanceMask = 0, which means the instance "
                          "will never be included in any ray intersections";
                    m_MessageCallback->Message(MessageSeverity::Warning, ss.str().c_str());
                }
            }

            wrapper->wasBuilt = true;
            wrapper->buildInstances = numInstances;
        }
        m_CommandList->BuildTopLevelAccelStruct(underlyingAS, patchedInstances.data(),
                                                uint32_t(patchedInstances.size()), buildFlags);
    }

    void CommandListWrapper::BuildTopLevelAccelStructFromBuffer(RT::AccelStruct *as,
                                                                Physara::RHI::Buffer *instanceBuffer,
                                                                uint64_t instanceBufferOffset, size_t numInstances,
                                                                RT::AccelStructBuildFlags buildFlags)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return;
        }

        if (!RequireOpenState())
        {
            return;
        }

        if (!RequireType(CommandQueue::Compute, "buildTopLevelAccelStruct"))
        {
            return;
        }

        if (!as)
        {
            Error("buildTopLevelAccelStructFromBuffer: 'as' is NULL");
            return;
        }

        if (!instanceBuffer)
        {
            Error("buildTopLevelAccelStructFromBuffer: 'instanceBuffer' is NULL");
            return;
        }

        RT::AccelStruct *underlyingAS = as;

        AccelStructWrapper *wrapper = dynamic_cast<AccelStructWrapper *>(as);
        if (wrapper)
        {
            underlyingAS = wrapper->GetUnderlyingObject();

            if (!ValidateBuildTopLevelAccelStruct(wrapper, numInstances, buildFlags))
            {
                return;
            }
        }

        auto bufferDesc = instanceBuffer->GetDesc();
        if (!bufferDesc.isAccelStructBuildInput)
        {
            std::stringstream ss;
            ss << "Buffer " << Utils::DebugNameToString(bufferDesc.debugName)
               << " used in buildTopLevelAccelStructFromBuffer "
                  "doesn't have the 'isAccelStructBuildInput' flag set";
            Error(ss.str());
            return;
        }

        uint64_t sizeOfData = numInstances * sizeof(RT::InstanceDesc);
        if (bufferDesc.byteSize < instanceBufferOffset + sizeOfData)
        {
            std::stringstream ss;
            ss << "Buffer " << Utils::DebugNameToString(bufferDesc.debugName)
               << " used in buildTopLevelAccelStructFromBuffer "
                  "is smaller than the referenced instance data: "
               << sizeOfData << " bytes used at offset " << instanceBufferOffset << ", buffer size is "
               << bufferDesc.byteSize << " bytes";
            Error(ss.str());
            return;
        }

        m_CommandList->BuildTopLevelAccelStructFromBuffer(underlyingAS, instanceBuffer, instanceBufferOffset,
                                                          numInstances, buildFlags);
    }

    void CommandListWrapper::ExecuteMultiIndirectClusterOperation(const RT::Cluster::OperationDesc &desc)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return;
        }

        if (!RequireOpenState())
        {
            return;
        }

        if (!RequireType(CommandQueue::Compute, "executeMultiIndirectClusterOperation"))
        {
            return;
        }

        if (!m_Device->ValidateClusterOperationParams(desc.params))
        {
            return;
        }

        if (desc.inIndirectArgCountBuffer == nullptr && desc.params.maxArgCount == 0)
        {
            Error("executeMultiIndirectClusterOperation: 'inIndirectArgCountBuffer' is NULL and maxArgCount is 0");
            return;
        }

        if (desc.inIndirectArgsBuffer == nullptr)
        {
            Error("executeMultiIndirectClusterOperation: 'inIndirectArgsBuffer' is NULL");
            return;
        }

        if (desc.scratchSizeInBytes == 0)
        {
            Error("executeMultiIndirectClusterOperation: 'scratchSizeInBytes' is 0");
            return;
        }

        if (desc.params.mode == RT::Cluster::OperationMode::ImplicitDestinations)
        {
            if (desc.inOutAddressesBuffer == nullptr)
            {
                Error("executeMultiIndirectClusterOperation (cluster::OperationMode::ImplicitDestinations): "
                      "'inOutAddressesBuffer' is NULL");
                return;
            }
            if (desc.outAccelerationStructuresBuffer == nullptr)
            {
                Error("executeMultiIndirectClusterOperation (cluster::OperationMode::ImplicitDestinations): "
                      "'outAccelerationStructuresBuffer' is NULL");
                return;
            }
        }
        else if (desc.params.mode == RT::Cluster::OperationMode::ExplicitDestinations)
        {
            if (desc.inOutAddressesBuffer == nullptr)
            {
                Error("executeMultiIndirectClusterOperation (cluster::OperationMode::ExplicitDestinations): "
                      "'inOutAddressesBuffer' is NULL");
                return;
            }
        }
        else if (desc.params.mode == RT::Cluster::OperationMode::GetSizes)
        {
            if (desc.outSizesBuffer == nullptr)
            {
                Error("executeMultiIndirectClusterOperation (cluster::OperationMode::GetSizes): 'outSizesBuffer' is "
                      "NULL");
                return;
            }
        }

        m_CommandList->ExecuteMultiIndirectClusterOperation(desc);
    }

    void CommandListWrapper::ConvertCoopVecMatrices(CoopVec::ConvertMatrixLayoutDesc const *convertDescs,
                                                    size_t numDescs)
    {
        if (Profile::RejectOpenGL(m_Device))
        {
            return;
        }

        if (!m_Device->QueryFeatureSupport(Feature::CooperativeVectorInferencing))
        {
            Error("convertCoopVecMatrices: Cooperative Vectors are not supported by the device");
            return;
        }

        for (size_t i = 0; i < numDescs; i++)
        {
            CoopVec::ConvertMatrixLayoutDesc const &desc = convertDescs[i];

            if (desc.src.buffer == nullptr)
            {
                std::stringstream ss;
                ss << "convertCoopVecMatrices: src.buffer is NULL for convertDescs[" << i << "]";
                Error(ss.str());
                return;
            }

            if (desc.dst.buffer == nullptr)
            {
                std::stringstream ss;
                ss << "convertCoopVecMatrices: dst.buffer is NULL for convertDescs[" << i << "]";
                Error(ss.str());
                return;
            }

            if (!desc.dst.buffer->GetDesc().canHaveUAVs && m_Device->GetGraphicsAPI() == GraphicsAPI::D3D12)
            {
                std::stringstream ss;
                ss << "convertCoopVecMatrices: dst.buffer "
                   << Utils::DebugNameToString(desc.dst.buffer->GetDesc().debugName)
                   << " does not have the canHaveUAVs flag set for convertDescs[" << i << "]";
                Error(ss.str());
                return;
            }

            if (desc.src.size == 0 || desc.dst.size == 0)
            {
                std::stringstream ss;
                ss << "convertCoopVecMatrices: src.size (" << desc.src.size << ") and dst.size (" << desc.dst.size
                   << ") must be non-zero for convertDescs[" << i << "]";
                Error(ss.str());
                return;
            }
        }

        m_CommandList->ConvertCoopVecMatrices(convertDescs, numDescs);
    }

    void CommandListWrapper::EvaluatePushConstantSize(const Physara::RHI::BindingLayoutVector &bindingLayouts)
    {
        m_PipelinePushConstantSize = 0;

        // Find the first PushConstants entry.
        // Assumes that the binding layout vector has been validated for duplicated push constants entries.

        for (const auto &layout : bindingLayouts)
        {
            const BindingLayoutDesc *layoutDesc = layout->GetDesc();

            if (!layoutDesc) // bindless layouts have null desc
            {
                continue;
            }

            for (const auto &item : layoutDesc->bindings)
            {
                if (item.type == ResourceType::PushConstants)
                {
                    m_PipelinePushConstantSize = item.size;
                    return;
                }
            }
        }
    }

    bool CommandListWrapper::ValidatePushConstants(const char *pipelineType, const char *stateFunctionName) const
    {
        if (m_PipelinePushConstantSize != 0 && !m_PushConstantsSet)
        {
            std::stringstream ss;
            ss << "The " << pipelineType << " pipeline expects push constants (" << m_PipelinePushConstantSize
               << " bytes) that were not set." << std::endl;
            ss << "Push constants must be set after each call to " << stateFunctionName << ".";

            Error(ss.str());

            return false;
        }

        return true;
    }

} // namespace Physara::RHI::Validation
