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

/// @file
/// @brief 描述构造, 命令便利操作和固定容量 CPU 索引分配, 不拥有 Device/context 或原生资源缓存.

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "RHI.hpp"

namespace Physara::RHI::Utils
{
    /// @brief 构造使用指定因子的 Add 混合描述, 不创建或验证设备对象.
    PHYSARA_RHI_API BlendState::RenderTarget CreateAddBlendState(BlendFactor srcBlend, BlendFactor dstBlend);

    /// @brief 构造常量缓冲描述; Volatile 变体另设置版本预算, 设备仍需支持相应用途.
    PHYSARA_RHI_API BufferDesc CreateStaticConstantBufferDesc(uint32_t byteSize, const char *debugName);

    PHYSARA_RHI_API BufferDesc CreateVolatileConstantBufferDesc(uint32_t byteSize, const char *debugName,
                                                                uint32_t maxVersions);

    // 来源工具的自动 layout 仅覆盖每槽单元素; 数组先提供匹配 layout. 输入须已初始化,
    // PushConstants 字节数在调用前检查为有效且<=MaxPushConstantSize, 再进入窄字段构造.
    /// @brief 缺少 layout 时按每槽单元素推导, 再按需创建 set, 已有输出直接复用.
    /// @return 任一步失败返回 false, 此前成功的 layout 可保留; 输入须初始化, 数组须先提供匹配 layout.
    PHYSARA_RHI_API bool CreateBindingSetAndLayout(Device *device, Physara::RHI::ShaderType visibility,
                                                   uint32_t registerSpace, const BindingSetDesc &bindingSetDesc,
                                                   BindingLayoutHandle &bindingLayout, BindingSetHandle &bindingSet,
                                                   bool registerSpaceIsDescriptorSet = false);

    /// @brief 从实际 framebuffer 附件提取纹理/子资源并转发 clear, 不扩大底层支持范围.
    PHYSARA_RHI_API void ClearColorAttachment(CommandList *commandList, Framebuffer *framebuffer,
                                              uint32_t attachmentIndex, Color color);

    /// @brief 仅在存在深度附件时转发深度/模板清除, 深度值按调用方约定提供.
    PHYSARA_RHI_API void ClearDepthStencilAttachment(CommandList *commandList, Framebuffer *framebuffer, float depth,
                                                     uint32_t stencil);

    /// @brief 将描述中的几何数组转发给底层 BLAS 构建, 资源与录制寿命由调用方保证.
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

    /// @brief 枚举名称查询返回静态文本; DebugNameToString 对非空名字借用其 c_str, 不跨名字修改/销毁保留.
    PHYSARA_RHI_API const char *GraphicsAPIToString(GraphicsAPI api);
    PHYSARA_RHI_API const char *TextureDimensionToString(TextureDimension dimension);
    PHYSARA_RHI_API const char *DebugNameToString(const std::string &debugName);
    PHYSARA_RHI_API const char *ShaderStageToString(ShaderType stage);
    PHYSARA_RHI_API const char *ResourceTypeToString(ResourceType type);
    PHYSARA_RHI_API const char *FormatToString(Format format);
    PHYSARA_RHI_API const char *CommandQueueToString(CommandQueue queue);

    /// @brief 从描述生成自持诊断名, 三类 Generate 函数均只格式化 CPU 值, 可传播分配异常.
    std::string GenerateHeapDebugName(const HeapDesc &desc);
    std::string GenerateTextureDebugName(const TextureDesc &desc);
    std::string GenerateBufferDebugName(const BufferDesc &desc);

    /// @brief 以下三个固定来源辅助入口仅提供 Debug 断言, 不承担 Release 外部输入拒绝.
    void NotImplemented();
    void NotSupported();
    void InvalidEnum();

    /// @brief 固定容量 CPU 索引占用表, 不推断资源或 GPU 退休, 使用者自己决定何时释放.
    class BitSetAllocator
    {
      public:
        // 容量固定; capacity>INT_MAX 在分配前抛 length_error, 标准存储分配异常可传播.
        // multithreaded=true 同步 Allocate/Release; false 要求调用者串行访问. CPU 索引不表示 GPU 退休.
        explicit BitSetAllocator(size_t capacity, bool multithreaded);

        /// @brief 从候选位置环形搜索一个空槽并占用, 耗尽包括零容量时返回 -1.
        int Allocate();
        /// @brief 释放合法索引并调整搜索候选, 范围外索引忽略, 不等待关联工作完成.
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
    /// @brief 在作用域内配对命令 marker, 借用列表须覆盖析构, 且列表仍须满足 recording 合同.
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
