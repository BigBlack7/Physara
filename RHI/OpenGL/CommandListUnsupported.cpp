#include "CommandList.hpp"

namespace Physara::RHI::OpenGL
{
    void OpenGLCommandList::ClearTextureFloat(Texture *t, TextureSubresourceSet subresources, const Color &clearColor)
    {
        static_cast<void>(t);
        static_cast<void>(subresources);
        static_cast<void>(clearColor);
        Unsupported("ClearTextureFloat");
    }

    void OpenGLCommandList::ClearDepthStencilTexture(Texture *t, TextureSubresourceSet subresources, bool clearDepth,
                                                     float depth, bool clearStencil, uint8_t stencil)
    {
        static_cast<void>(t);
        static_cast<void>(subresources);
        static_cast<void>(clearDepth);
        static_cast<void>(depth);
        static_cast<void>(clearStencil);
        static_cast<void>(stencil);
        Unsupported("ClearDepthStencilTexture");
    }

    void OpenGLCommandList::ClearTextureUInt(Texture *t, TextureSubresourceSet subresources, uint32_t clearColor)
    {
        static_cast<void>(t);
        static_cast<void>(subresources);
        static_cast<void>(clearColor);
        Unsupported("ClearTextureUInt");
    }

    void OpenGLCommandList::CopyTexture(Texture *dest, const TextureSlice &destSlice, Texture *src,
                                        const TextureSlice &srcSlice)
    {
        static_cast<void>(dest);
        static_cast<void>(destSlice);
        static_cast<void>(src);
        static_cast<void>(srcSlice);
        Unsupported("CopyTexture");
    }

    void OpenGLCommandList::CopyTexture(StagingTexture *dest, const TextureSlice &destSlice, Texture *src,
                                        const TextureSlice &srcSlice)
    {
        static_cast<void>(dest);
        static_cast<void>(destSlice);
        static_cast<void>(src);
        static_cast<void>(srcSlice);
        Unsupported("CopyTexture");
    }

    void OpenGLCommandList::CopyTexture(Texture *dest, const TextureSlice &destSlice, StagingTexture *src,
                                        const TextureSlice &srcSlice)
    {
        static_cast<void>(dest);
        static_cast<void>(destSlice);
        static_cast<void>(src);
        static_cast<void>(srcSlice);
        Unsupported("CopyTexture");
    }

    void OpenGLCommandList::ResolveTexture(Texture *dest, const TextureSubresourceSet &dstSubresources, Texture *src,
                                           const TextureSubresourceSet &srcSubresources)
    {
        static_cast<void>(dest);
        static_cast<void>(dstSubresources);
        static_cast<void>(src);
        static_cast<void>(srcSubresources);
        Unsupported("ResolveTexture");
    }

    void OpenGLCommandList::ClearBufferUInt(Buffer *b, uint32_t clearValue)
    {
        static_cast<void>(b);
        static_cast<void>(clearValue);
        Unsupported("ClearBufferUInt");
    }

    void OpenGLCommandList::CopyBuffer(Buffer *dest, uint64_t destOffsetBytes, Buffer *src, uint64_t srcOffsetBytes,
                                       uint64_t dataSizeBytes)
    {
        static_cast<void>(dest);
        static_cast<void>(destOffsetBytes);
        static_cast<void>(src);
        static_cast<void>(srcOffsetBytes);
        static_cast<void>(dataSizeBytes);
        Unsupported("CopyBuffer");
    }

    void OpenGLCommandList::ClearSamplerFeedbackTexture(SamplerFeedbackTexture *texture)
    {
        static_cast<void>(texture);
        Unsupported("ClearSamplerFeedbackTexture");
    }

    void OpenGLCommandList::DecodeSamplerFeedbackTexture(Buffer *buffer, SamplerFeedbackTexture *texture,
                                                         Physara::RHI::Format format)
    {
        static_cast<void>(buffer);
        static_cast<void>(texture);
        static_cast<void>(format);
        Unsupported("DecodeSamplerFeedbackTexture");
    }

    void OpenGLCommandList::SetSamplerFeedbackTextureState(SamplerFeedbackTexture *texture, ResourceStates stateBits)
    {
        static_cast<void>(texture);
        static_cast<void>(stateBits);
        Unsupported("SetSamplerFeedbackTextureState");
    }

    void OpenGLCommandList::SetPushConstants(const void *data, size_t byteSize)
    {
        static_cast<void>(data);
        static_cast<void>(byteSize);
        Unsupported("SetPushConstants");
    }

    void OpenGLCommandList::SetGraphicsState(const GraphicsState &state)
    {
        static_cast<void>(state);
        Unsupported("SetGraphicsState");
    }

    void OpenGLCommandList::Draw(const DrawArguments &args)
    {
        static_cast<void>(args);
        Unsupported("Draw");
    }

    void OpenGLCommandList::DrawIndexed(const DrawArguments &args)
    {
        static_cast<void>(args);
        Unsupported("DrawIndexed");
    }

    void OpenGLCommandList::DrawIndirect(uint32_t offsetBytes, uint32_t drawCount)
    {
        static_cast<void>(offsetBytes);
        static_cast<void>(drawCount);
        Unsupported("DrawIndirect");
    }

    void OpenGLCommandList::DrawIndexedIndirect(uint32_t offsetBytes, uint32_t drawCount)
    {
        static_cast<void>(offsetBytes);
        static_cast<void>(drawCount);
        Unsupported("DrawIndexedIndirect");
    }

    void OpenGLCommandList::DrawIndexedIndirectCount(uint32_t paramOffsetBytes, uint32_t countOffsetBytes,
                                                     uint32_t maxDrawCount)
    {
        static_cast<void>(paramOffsetBytes);
        static_cast<void>(countOffsetBytes);
        static_cast<void>(maxDrawCount);
        Unsupported("DrawIndexedIndirectCount");
    }

    void OpenGLCommandList::SetComputeState(const ComputeState &state)
    {
        static_cast<void>(state);
        Unsupported("SetComputeState");
    }

    void OpenGLCommandList::Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ)
    {
        static_cast<void>(groupsX);
        static_cast<void>(groupsY);
        static_cast<void>(groupsZ);
        Unsupported("Dispatch");
    }

    void OpenGLCommandList::DispatchIndirect(uint32_t offsetBytes)
    {
        static_cast<void>(offsetBytes);
        Unsupported("DispatchIndirect");
    }

    void OpenGLCommandList::SetMeshletState(const MeshletState &state)
    {
        static_cast<void>(state);
        Unsupported("SetMeshletState");
    }

    void OpenGLCommandList::DispatchMesh(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ)
    {
        static_cast<void>(groupsX);
        static_cast<void>(groupsY);
        static_cast<void>(groupsZ);
        Unsupported("DispatchMesh");
    }

    void OpenGLCommandList::DispatchMeshIndirect(uint32_t offsetBytes, uint32_t maxDrawCount)
    {
        static_cast<void>(offsetBytes);
        static_cast<void>(maxDrawCount);
        Unsupported("DispatchMeshIndirect");
    }

    void OpenGLCommandList::DispatchMeshIndirectCount(uint32_t paramOffsetBytes, uint32_t countOffsetBytes,
                                                      uint32_t maxDrawCount)
    {
        static_cast<void>(paramOffsetBytes);
        static_cast<void>(countOffsetBytes);
        static_cast<void>(maxDrawCount);
        Unsupported("DispatchMeshIndirectCount");
    }

    void OpenGLCommandList::SetRayTracingState(const RT::State &state)
    {
        static_cast<void>(state);
        Unsupported("SetRayTracingState");
    }

    void OpenGLCommandList::DispatchRays(const RT::DispatchRaysArguments &args)
    {
        static_cast<void>(args);
        Unsupported("DispatchRays");
    }

    void OpenGLCommandList::BuildOpacityMicromap(RT::OpacityMicromap *omm, const RT::OpacityMicromapDesc &desc)
    {
        static_cast<void>(omm);
        static_cast<void>(desc);
        Unsupported("BuildOpacityMicromap");
    }

    void OpenGLCommandList::BuildBottomLevelAccelStruct(RT::AccelStruct *as, const RT::GeometryDesc *pGeometries,
                                                        size_t numGeometries, RT::AccelStructBuildFlags buildFlags)
    {
        static_cast<void>(as);
        static_cast<void>(pGeometries);
        static_cast<void>(numGeometries);
        static_cast<void>(buildFlags);
        Unsupported("BuildBottomLevelAccelStruct");
    }

    void OpenGLCommandList::CompactBottomLevelAccelStructs()
    {
        Unsupported("CompactBottomLevelAccelStructs");
    }

    void OpenGLCommandList::CopyRaytracingAccelerationStructure(RT::AccelStruct *destination, RT::AccelStruct *source)
    {
        static_cast<void>(destination);
        static_cast<void>(source);
        Unsupported("CopyRaytracingAccelerationStructure");
    }

    void OpenGLCommandList::BuildTopLevelAccelStruct(RT::AccelStruct *as, const RT::InstanceDesc *pInstances,
                                                     size_t numInstances, RT::AccelStructBuildFlags buildFlags)
    {
        static_cast<void>(as);
        static_cast<void>(pInstances);
        static_cast<void>(numInstances);
        static_cast<void>(buildFlags);
        Unsupported("BuildTopLevelAccelStruct");
    }

    void OpenGLCommandList::ExecuteMultiIndirectClusterOperation(const RT::Cluster::OperationDesc &desc)
    {
        static_cast<void>(desc);
        Unsupported("ExecuteMultiIndirectClusterOperation");
    }

    void OpenGLCommandList::BuildTopLevelAccelStructFromBuffer(RT::AccelStruct *as,
                                                               Physara::RHI::Buffer *instanceBuffer,
                                                               uint64_t instanceBufferOffset, size_t numInstances,
                                                               RT::AccelStructBuildFlags buildFlags)
    {
        static_cast<void>(as);
        static_cast<void>(instanceBuffer);
        static_cast<void>(instanceBufferOffset);
        static_cast<void>(numInstances);
        static_cast<void>(buildFlags);
        Unsupported("BuildTopLevelAccelStructFromBuffer");
    }

    void OpenGLCommandList::ConvertCoopVecMatrices(CoopVec::ConvertMatrixLayoutDesc const *convertDescs,
                                                   size_t numDescs)
    {
        static_cast<void>(convertDescs);
        static_cast<void>(numDescs);
        Unsupported("ConvertCoopVecMatrices");
    }

    void OpenGLCommandList::BeginTimerQuery(TimerQuery *query)
    {
        static_cast<void>(query);
        Unsupported("BeginTimerQuery");
    }

    void OpenGLCommandList::EndTimerQuery(TimerQuery *query)
    {
        static_cast<void>(query);
        Unsupported("EndTimerQuery");
    }

    void OpenGLCommandList::BeginMarker(const char *name)
    {
        static_cast<void>(name);
        Unsupported("BeginMarker");
    }

    void OpenGLCommandList::EndMarker()
    {
        Unsupported("EndMarker");
    }

    void OpenGLCommandList::SetResourceStatesForBindingSet(BindingSet *bindingSet)
    {
        static_cast<void>(bindingSet);
        Unsupported("SetResourceStatesForBindingSet");
    }

    void OpenGLCommandList::SetEnableUavBarriersForTexture(Texture *texture, bool enableBarriers)
    {
        static_cast<void>(texture);
        static_cast<void>(enableBarriers);
        Unsupported("SetEnableUavBarriersForTexture");
    }

    void OpenGLCommandList::SetEnableUavBarriersForBuffer(Buffer *buffer, bool enableBarriers)
    {
        static_cast<void>(buffer);
        static_cast<void>(enableBarriers);
        Unsupported("SetEnableUavBarriersForBuffer");
    }

    void OpenGLCommandList::SetAccelStructState(RT::AccelStruct *as, ResourceStates stateBits)
    {
        static_cast<void>(as);
        static_cast<void>(stateBits);
        Unsupported("SetAccelStructState");
    }

    void OpenGLCommandList::SetPermanentTextureState(Texture *texture, ResourceStates stateBits)
    {
        static_cast<void>(texture);
        static_cast<void>(stateBits);
        Unsupported("SetPermanentTextureState");
    }

    void OpenGLCommandList::SetPermanentBufferState(Buffer *buffer, ResourceStates stateBits)
    {
        static_cast<void>(buffer);
        static_cast<void>(stateBits);
        Unsupported("SetPermanentBufferState");
    }

}
