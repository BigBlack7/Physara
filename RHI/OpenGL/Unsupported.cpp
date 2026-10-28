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

#include "Backend.hpp"

#include <stdexcept>

namespace Physara::RHI::OpenGL
{
    HeapHandle OpenGLDevice::CreateHeap(const HeapDesc &)
    {
        if (RequireContext("CreateHeap"))
        {
            Report("CreateHeap", "Unsupported: resource-only device");
        }
        return nullptr;
    }

    bool OpenGLDevice::QueryTopLevelAccelStructPrebuildInfo(const RT::AccelStructDesc &, uint32_t,
                                                            RT::AccelStructPrebuildInfo &)
    {
        if (RequireContext("QueryTopLevelAccelStructPrebuildInfo"))
        {
            Report("QueryTopLevelAccelStructPrebuildInfo", "Unsupported: resource-only device");
        }
        return false;
    }

    MemoryRequirements OpenGLDevice::GetTextureMemoryRequirements(Texture *)
    {
        if (RequireContext("GetTextureMemoryRequirements"))
        {
            Report("GetTextureMemoryRequirements", "Unsupported: resource-only device");
        }
        return {};
    }

    bool OpenGLDevice::BindTextureMemory(Texture *, Heap *, uint64_t)
    {
        if (RequireContext("BindTextureMemory"))
        {
            Report("BindTextureMemory", "Unsupported: resource-only device");
        }
        return false;
    }

    TextureHandle OpenGLDevice::CreateHandleForNativeTexture(ObjectType, Object, const TextureDesc &)
    {
        if (RequireContext("CreateHandleForNativeTexture"))
        {
            Report("CreateHandleForNativeTexture", "Unsupported: resource-only device");
        }
        return nullptr;
    }

    StagingTextureHandle OpenGLDevice::CreateStagingTexture(const TextureDesc &, CpuAccessMode)
    {
        if (RequireContext("CreateStagingTexture"))
        {
            Report("CreateStagingTexture", "Unsupported: resource-only device");
        }
        return nullptr;
    }

    void *OpenGLDevice::MapStagingTexture(StagingTexture *, const TextureSlice &, CpuAccessMode, size_t *)
    {
        if (RequireContext("MapStagingTexture"))
        {
            Report("MapStagingTexture", "Unsupported: resource-only device");
        }
        return nullptr;
    }

    void OpenGLDevice::UnmapStagingTexture(StagingTexture *)
    {
        if (RequireContext("UnmapStagingTexture"))
        {
            Report("UnmapStagingTexture", "Unsupported: resource-only device");
        }
    }

    void OpenGLDevice::GetTextureTiling(Texture *, uint32_t *, PackedMipDesc *, TileShape *, uint32_t *,
                                        SubresourceTiling *)
    {
        if (RequireContext("GetTextureTiling"))
        {
            Report("GetTextureTiling", "Unsupported: resource-only device");
        }
    }

    void OpenGLDevice::UpdateTextureTileMappings(Texture *, const TextureTilesMapping *, uint32_t, CommandQueue)
    {
        if (RequireContext("UpdateTextureTileMappings"))
        {
            Report("UpdateTextureTileMappings", "Unsupported: resource-only device");
        }
    }

    SamplerFeedbackTextureHandle OpenGLDevice::CreateSamplerFeedbackTexture(Texture *,
                                                                            const SamplerFeedbackTextureDesc &)
    {
        if (RequireContext("CreateSamplerFeedbackTexture"))
        {
            Report("CreateSamplerFeedbackTexture", "Unsupported: resource-only device");
        }
        return nullptr;
    }

    SamplerFeedbackTextureHandle OpenGLDevice::CreateSamplerFeedbackForNativeTexture(ObjectType, Object, Texture *)
    {
        if (RequireContext("CreateSamplerFeedbackForNativeTexture"))
        {
            Report("CreateSamplerFeedbackForNativeTexture", "Unsupported: resource-only device");
        }
        return nullptr;
    }

    void *OpenGLDevice::MapBuffer(Buffer *, CpuAccessMode)
    {
        if (RequireContext("MapBuffer"))
        {
            Report("MapBuffer", "Unsupported: resource-only device");
        }
        return nullptr;
    }

    void OpenGLDevice::UnmapBuffer(Buffer *)
    {
        if (RequireContext("UnmapBuffer"))
        {
            Report("UnmapBuffer", "Unsupported: resource-only device");
        }
    }

    MemoryRequirements OpenGLDevice::GetBufferMemoryRequirements(Buffer *)
    {
        if (RequireContext("GetBufferMemoryRequirements"))
        {
            Report("GetBufferMemoryRequirements", "Unsupported: resource-only device");
        }
        return {};
    }

    bool OpenGLDevice::BindBufferMemory(Buffer *, Heap *, uint64_t)
    {
        if (RequireContext("BindBufferMemory"))
        {
            Report("BindBufferMemory", "Unsupported: resource-only device");
        }
        return false;
    }

    BufferHandle OpenGLDevice::CreateHandleForNativeBuffer(ObjectType, Object, const BufferDesc &)
    {
        if (RequireContext("CreateHandleForNativeBuffer"))
        {
            Report("CreateHandleForNativeBuffer", "Unsupported: resource-only device");
        }
        return nullptr;
    }

    ShaderHandle OpenGLDevice::CreateShader(const ShaderDesc &, const void *, size_t)
    {
        if (RequireContext("CreateShader"))
        {
            Report("CreateShader", "Unsupported: resource-only device");
        }
        return nullptr;
    }

    ShaderHandle OpenGLDevice::CreateShaderSpecialization(Shader *, const ShaderSpecialization *, uint32_t)
    {
        if (RequireContext("CreateShaderSpecialization"))
        {
            Report("CreateShaderSpecialization", "Unsupported: resource-only device");
        }
        return nullptr;
    }

    ShaderLibraryHandle OpenGLDevice::CreateShaderLibrary(const void *, size_t)
    {
        if (RequireContext("CreateShaderLibrary"))
        {
            Report("CreateShaderLibrary", "Unsupported: resource-only device");
        }
        return nullptr;
    }

    InputLayoutHandle OpenGLDevice::CreateInputLayout(const VertexAttributeDesc *, uint32_t, Shader *)
    {
        if (RequireContext("CreateInputLayout"))
        {
            Report("CreateInputLayout", "Unsupported: resource-only device");
        }
        return nullptr;
    }

    EventQueryHandle OpenGLDevice::CreateEventQuery()
    {
        if (RequireContext("CreateEventQuery"))
        {
            Report("CreateEventQuery", "Unsupported: resource-only device");
        }
        return nullptr;
    }

    void OpenGLDevice::SetEventQuery(EventQuery *, CommandQueue)
    {
        if (RequireContext("SetEventQuery"))
        {
            Report("SetEventQuery", "Unsupported: resource-only device");
        }
    }

    bool OpenGLDevice::PollEventQuery(EventQuery *)
    {
        if (RequireContext("PollEventQuery"))
        {
            Report("PollEventQuery", "Unsupported: resource-only device");
        }
        return false;
    }

    void OpenGLDevice::WaitEventQuery(EventQuery *)
    {
        if (RequireContext("WaitEventQuery"))
        {
            Report("WaitEventQuery", "Unsupported: resource-only device");
        }
    }

    void OpenGLDevice::ResetEventQuery(EventQuery *)
    {
        if (RequireContext("ResetEventQuery"))
        {
            Report("ResetEventQuery", "Unsupported: resource-only device");
        }
    }

    TimerQueryHandle OpenGLDevice::CreateTimerQuery()
    {
        if (RequireContext("CreateTimerQuery"))
        {
            Report("CreateTimerQuery", "Unsupported: resource-only device");
        }
        return nullptr;
    }

    bool OpenGLDevice::PollTimerQuery(TimerQuery *)
    {
        if (RequireContext("PollTimerQuery"))
        {
            Report("PollTimerQuery", "Unsupported: resource-only device");
        }
        return false;
    }

    float OpenGLDevice::GetTimerQueryTime(TimerQuery *)
    {
        if (RequireContext("GetTimerQueryTime"))
        {
            Report("GetTimerQueryTime", "Unsupported: resource-only device");
        }
        return 0;
    }

    void OpenGLDevice::ResetTimerQuery(TimerQuery *)
    {
        if (RequireContext("ResetTimerQuery"))
        {
            Report("ResetTimerQuery", "Unsupported: resource-only device");
        }
    }

    GraphicsPipelineHandle OpenGLDevice::CreateGraphicsPipeline(const GraphicsPipelineDesc &, FramebufferInfo const &)
    {
        if (RequireContext("CreateGraphicsPipeline"))
        {
            Report("CreateGraphicsPipeline", "Unsupported: resource-only device");
        }
        return nullptr;
    }

    GraphicsPipelineHandle OpenGLDevice::CreateGraphicsPipeline(const GraphicsPipelineDesc &, Framebuffer *)
    {
        if (RequireContext("CreateGraphicsPipeline"))
        {
            Report("CreateGraphicsPipeline", "Unsupported: resource-only device");
        }
        return nullptr;
    }

    ComputePipelineHandle OpenGLDevice::CreateComputePipeline(const ComputePipelineDesc &)
    {
        if (RequireContext("CreateComputePipeline"))
        {
            Report("CreateComputePipeline", "Unsupported: resource-only device");
        }
        return nullptr;
    }

    MeshletPipelineHandle OpenGLDevice::CreateMeshletPipeline(const MeshletPipelineDesc &, FramebufferInfo const &)
    {
        if (RequireContext("CreateMeshletPipeline"))
        {
            Report("CreateMeshletPipeline", "Unsupported: resource-only device");
        }
        return nullptr;
    }

    MeshletPipelineHandle OpenGLDevice::CreateMeshletPipeline(const MeshletPipelineDesc &, Framebuffer *)
    {
        if (RequireContext("CreateMeshletPipeline"))
        {
            Report("CreateMeshletPipeline", "Unsupported: resource-only device");
        }
        return nullptr;
    }

    RT::PipelineHandle OpenGLDevice::CreateRayTracingPipeline(const RT::PipelineDesc &)
    {
        if (RequireContext("CreateRayTracingPipeline"))
        {
            Report("CreateRayTracingPipeline", "Unsupported: resource-only device");
        }
        return nullptr;
    }

    BindingLayoutHandle OpenGLDevice::CreateBindingLayout(const BindingLayoutDesc &)
    {
        if (RequireContext("CreateBindingLayout"))
        {
            Report("CreateBindingLayout", "Unsupported: resource-only device");
        }
        return nullptr;
    }

    BindingLayoutHandle OpenGLDevice::CreateBindlessLayout(const BindlessLayoutDesc &)
    {
        if (RequireContext("CreateBindlessLayout"))
        {
            Report("CreateBindlessLayout", "Unsupported: resource-only device");
        }
        return nullptr;
    }

    BindingSetHandle OpenGLDevice::CreateBindingSet(const BindingSetDesc &, BindingLayout *)
    {
        if (RequireContext("CreateBindingSet"))
        {
            Report("CreateBindingSet", "Unsupported: resource-only device");
        }
        return nullptr;
    }

    DescriptorTableHandle OpenGLDevice::CreateDescriptorTable(BindingLayout *)
    {
        if (RequireContext("CreateDescriptorTable"))
        {
            Report("CreateDescriptorTable", "Unsupported: resource-only device");
        }
        return nullptr;
    }

    void OpenGLDevice::ResizeDescriptorTable(DescriptorTable *, uint32_t, bool)
    {
        if (RequireContext("ResizeDescriptorTable"))
        {
            Report("ResizeDescriptorTable", "Unsupported: resource-only device");
        }
    }

    bool OpenGLDevice::WriteDescriptorTable(DescriptorTable *, const BindingSetItem &)
    {
        if (RequireContext("WriteDescriptorTable"))
        {
            Report("WriteDescriptorTable", "Unsupported: resource-only device");
        }
        return false;
    }

    RT::OpacityMicromapHandle OpenGLDevice::CreateOpacityMicromap(const RT::OpacityMicromapDesc &)
    {
        if (RequireContext("CreateOpacityMicromap"))
        {
            Report("CreateOpacityMicromap", "Unsupported: resource-only device");
        }
        return nullptr;
    }

    RT::AccelStructHandle OpenGLDevice::CreateAccelStruct(const RT::AccelStructDesc &)
    {
        if (RequireContext("CreateAccelStruct"))
        {
            Report("CreateAccelStruct", "Unsupported: resource-only device");
        }
        return nullptr;
    }

    MemoryRequirements OpenGLDevice::GetAccelStructMemoryRequirements(RT::AccelStruct *)
    {
        if (RequireContext("GetAccelStructMemoryRequirements"))
        {
            Report("GetAccelStructMemoryRequirements", "Unsupported: resource-only device");
        }
        return {};
    }

    RT::Cluster::OperationSizeInfo OpenGLDevice::GetClusterOperationSizeInfo(const RT::Cluster::OperationParams &)
    {
        if (RequireContext("GetClusterOperationSizeInfo"))
        {
            Report("GetClusterOperationSizeInfo", "Unsupported: resource-only device");
        }
        return {};
    }

    bool OpenGLDevice::BindAccelStructMemory(RT::AccelStruct *, Heap *, uint64_t)
    {
        if (RequireContext("BindAccelStructMemory"))
        {
            Report("BindAccelStructMemory", "Unsupported: resource-only device");
        }
        return false;
    }

    void OpenGLDevice::QueueWaitForCommandList(CommandQueue, CommandQueue, uint64_t)
    {
        if (RequireContext("QueueWaitForCommandList"))
        {
            Report("QueueWaitForCommandList", "Unsupported: resource-only device");
        }
    }

    CommandListLifetimeTrackerHandle OpenGLDevice::CreateCommandListLifetimeTracker(CommandQueue)
    {
        if (RequireContext("CreateCommandListLifetimeTracker"))
        {
            Report("CreateCommandListLifetimeTracker", "Unsupported: resource-only device");
        }
        return nullptr;
    }

    CoopVec::DeviceFeatures OpenGLDevice::QueryCoopVecFeatures()
    {
        if (RequireContext("QueryCoopVecFeatures"))
        {
            Report("QueryCoopVecFeatures", "Unsupported: resource-only device");
        }
        return {};
    }

    CoopVec::MatMulFormatSupport OpenGLDevice::QueryCoopVecMatMulFormatSupport(const CoopVec::MatMulFormatCombo &)
    {
        if (RequireContext("QueryCoopVecMatMulFormatSupport"))
        {
            Report("QueryCoopVecMatMulFormatSupport", "Unsupported: resource-only device");
        }
        return {};
    }

    CoopVec::TrainingFormatSupport OpenGLDevice::QueryCoopVecTrainingFormatSupport(CoopVec::DataType)
    {
        if (RequireContext("QueryCoopVecTrainingFormatSupport"))
        {
            Report("QueryCoopVecTrainingFormatSupport", "Unsupported: resource-only device");
        }
        return {};
    }

    size_t OpenGLDevice::GetCoopVecMatrixSize(CoopVec::DataType, CoopVec::MatrixLayout, int, int)
    {
        if (RequireContext("GetCoopVecMatrixSize"))
        {
            Report("GetCoopVecMatrixSize", "Unsupported: resource-only device");
        }
        return 0;
    }

    Object OpenGLDevice::GetNativeQueue(ObjectType, CommandQueue)
    {
        if (RequireContext("GetNativeQueue"))
        {
            Report("GetNativeQueue", "Unsupported: resource-only device");
        }
        return nullptr;
    }

    bool OpenGLDevice::IsAftermathEnabled()
    {
        if (RequireContext("IsAftermathEnabled"))
        {
            Report("IsAftermathEnabled", "Unsupported: resource-only device");
        }
        return false;
    }

    AftermathCrashDumpHelper &OpenGLDevice::GetAftermathCrashDumpHelper()
    {
        Report("GetAftermathCrashDumpHelper", "Unsupported: reference-only facility");
        throw std::logic_error("OpenGL crash helper is unavailable");
    }

}
