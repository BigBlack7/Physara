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

#pragma once

// 来源与受控差异见 RHI/README.md; 此头不代表后端能力已实现.

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "RHI.hpp"

namespace Physara::RHI::Utils
{
    PHYSARA_RHI_API BlendState::RenderTarget CreateAddBlendState(BlendFactor srcBlend, BlendFactor dstBlend);

    PHYSARA_RHI_API BufferDesc CreateStaticConstantBufferDesc(uint32_t byteSize, const char *debugName);

    PHYSARA_RHI_API BufferDesc CreateVolatileConstantBufferDesc(uint32_t byteSize, const char *debugName,
                                                                uint32_t maxVersions);

    // 来源工具的自动 layout 仅覆盖每槽单元素; 数组先提供匹配 layout. 输入须已初始化,
    // PushConstants 字节数在调用前检查为有效且<=MaxPushConstantSize, 再进入窄字段构造.
    PHYSARA_RHI_API bool CreateBindingSetAndLayout(Device *device, Physara::RHI::ShaderType visibility,
                                                   uint32_t registerSpace, const BindingSetDesc &bindingSetDesc,
                                                   BindingLayoutHandle &bindingLayout, BindingSetHandle &bindingSet,
                                                   bool registerSpaceIsDescriptorSet = false);

    PHYSARA_RHI_API void ClearColorAttachment(CommandList *commandList, Framebuffer *framebuffer,
                                              uint32_t attachmentIndex, Color color);

    PHYSARA_RHI_API void ClearDepthStencilAttachment(CommandList *commandList, Framebuffer *framebuffer, float depth,
                                                     uint32_t stencil);

    PHYSARA_RHI_API void BuildBottomLevelAccelStruct(CommandList *commandList, RT::AccelStruct *as,
                                                     const RT::AccelStructDesc &desc);

    // Places a UAV barrier on the provided texture.
    // Useful when doing multiple consecutive dispatch calls with the same resources but different constants.
    // Ignored if there was a call to setEnableUavBarriersForTexrure(..., false) on this texture.
    PHYSARA_RHI_API void TextureUavBarrier(CommandList *commandList, Texture *texture);

    // Places a UAV barrier on the provided buffer.
    // Useful when doing multiple consecutive dispatch calls with the same resources but different constants.
    // Ignored if there was a call to setEnableUavBarriersForBuffer(..., false) on this buffer.
    PHYSARA_RHI_API void BufferUavBarrier(CommandList *commandList, Buffer *buffer);

    // Selects a format from the supplied list that supports all the required features on the given device.
    // The formats are tested in the same order they're provided, and the first matching one is returned.
    // If no formats are matching, Format::UNKNOWN is returned.
    PHYSARA_RHI_API Format ChooseFormat(Device *device, Physara::RHI::FormatSupport requiredFeatures,
                                        const Physara::RHI::Format *requestedFormats, size_t requestedFormatCount);

    PHYSARA_RHI_API const char *GraphicsAPIToString(GraphicsAPI api);
    PHYSARA_RHI_API const char *TextureDimensionToString(TextureDimension dimension);
    PHYSARA_RHI_API const char *DebugNameToString(const std::string &debugName);
    PHYSARA_RHI_API const char *ShaderStageToString(ShaderType stage);
    PHYSARA_RHI_API const char *ResourceTypeToString(ResourceType type);
    PHYSARA_RHI_API const char *FormatToString(Format format);
    PHYSARA_RHI_API const char *CommandQueueToString(CommandQueue queue);

    std::string GenerateHeapDebugName(const HeapDesc &desc);
    std::string GenerateTextureDebugName(const TextureDesc &desc);
    std::string GenerateBufferDebugName(const BufferDesc &desc);

    void NotImplemented();
    void NotSupported();
    void InvalidEnum();

    class BitSetAllocator
    {
      public:
        // 容量固定; capacity>INT_MAX 在分配前抛 length_error, 标准存储分配异常可传播.
        // multithreaded=true 同步 Allocate/Release; false 要求调用者串行访问. CPU 索引不表示 GPU 退休.
        explicit BitSetAllocator(size_t capacity, bool multithreaded);

        int Allocate();
        void Release(int index);
        [[nodiscard]] size_t GetCapacity() const
        {
            return m_Allocated.size();
        }

      private:
        int m_NextAvailable = 0;
        std::vector<bool> m_Allocated;
        bool m_MultiThreaded;
        std::mutex m_Mutex;
    };

    // Automatic begin/end marker for command list
    class ScopedMarker
    {
      public:
        CommandList *m_CommandList;
        ScopedMarker(CommandList *commandList, const char *markerName) : m_CommandList(commandList)
        {
            m_CommandList->BeginMarker(markerName);
        }

        ScopedMarker(CommandListHandle *commandList, const char *markerName)
            : ScopedMarker(commandList->Get(), markerName)
        {
        }

        ~ScopedMarker()
        {
            m_CommandList->EndMarker();
        }
    };

}
