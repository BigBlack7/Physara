/*
* Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
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

#include <memory>
#include <optional>
#include <source_location>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "RHI.hpp"

namespace Physara::RHI
{
    struct BufferStateExtension
    {
        const BufferDesc &descRef;
        ResourceStates permanentState = ResourceStates::Unknown;

        explicit BufferStateExtension(const BufferDesc &desc) : descRef(desc)
        {
        }
    };

    struct TextureStateExtension
    {
        const TextureDesc &descRef;
        ResourceStates permanentState = ResourceStates::Unknown;
        bool stateInitialized = false;
        bool isSamplerFeedback = false;

        explicit TextureStateExtension(const TextureDesc &desc) : descRef(desc)
        {
        }
    };

    struct TextureState
    {
        std::vector<ResourceStates> subresourceStates;
        ResourceStates state = ResourceStates::Unknown;
        bool enableUavBarriers = true;
        bool firstUavBarrierPlaced = false;
        bool permanentTransition = false;
    };

    struct BufferState
    {
        ResourceStates state = ResourceStates::Unknown;
        bool enableUavBarriers = true;
        bool firstUavBarrierPlaced = false;
        bool permanentTransition = false;
    };

    struct TextureBarrier
    {
        TextureStateExtension *texture = nullptr;
        MipLevel mipLevel = 0;
        ArraySlice arraySlice = 0;
        bool entireTexture = false;
        ResourceStates stateBefore = ResourceStates::Unknown;
        ResourceStates stateAfter = ResourceStates::Unknown;
    };

    struct BufferBarrier
    {
        BufferStateExtension *buffer = nullptr;
        ResourceStates stateBefore = ResourceStates::Unknown;
        ResourceStates stateAfter = ResourceStates::Unknown;
    };

    // 单写者 recording 工作集. descRef/extension/callback 均借用, 地址和内容须稳定至 Submitted/Reset/析构.
    // ClearBarriers 仅消费抽象依赖, Submitted 仅发布接受提交的 metadata; 两者均不证明 GPU 完成.
    class CommandListResourceStateTracker
    {
      public:
          explicit CommandListResourceStateTracker(MessageCallback *messageCallback)
              : m_MessageCallback(messageCallback), m_PermanentTextureStates(0), m_PermanentBufferStates(0),
                m_TextureBarriers(0), m_BufferBarriers(0)
          {
              // MSVC Debug 的默认 vector 构造为 noexcept 却分配 proxy; count 构造允许 owner 捕获 OOM.
              m_HasErrors = messageCallback == nullptr;
          }

        // ICommandList-like interface

        void SetEnableUavBarriersForTexture(TextureStateExtension *texture, bool enableBarriers);
        void SetEnableUavBarriersForBuffer(BufferStateExtension *buffer, bool enableBarriers);

        void BeginTrackingTextureState(TextureStateExtension *texture, TextureSubresourceSet subresources,
                                       ResourceStates stateBits);
        void BeginTrackingBufferState(BufferStateExtension *buffer, ResourceStates stateBits);

        void SetPermanentTextureState(TextureStateExtension *texture, TextureSubresourceSet subresources,
                                      ResourceStates stateBits);
        void SetPermanentBufferState(BufferStateExtension *buffer, ResourceStates stateBits);

        ResourceStates GetTextureSubresourceState(TextureStateExtension *texture, ArraySlice arraySlice,
                                                  MipLevel mipLevel);
        ResourceStates GetBufferState(BufferStateExtension *buffer);

        // Internal interface

        void RequireTextureState(TextureStateExtension *texture, TextureSubresourceSet subresources,
                                 ResourceStates state);
        void RequireBufferState(BufferStateExtension *buffer, ResourceStates state);

        void KeepBufferInitialStates();
        void KeepTextureInitialStates();
        void CommandListSubmitted();

        // 错误为本次 recording 的 sticky 状态; 必须先检查再发射. Reset 放弃本地工作, 不发布 metadata.
        [[nodiscard]] bool HasErrors() const
        {
            return m_HasErrors;
        }
        void Reset();

        [[nodiscard]] const std::vector<TextureBarrier> &GetTextureBarriers() const
        {
            return m_TextureBarriers;
        }
        [[nodiscard]] const std::vector<BufferBarrier> &GetBufferBarriers() const
        {
            return m_BufferBarriers;
        }
        void ClearBarriers()
        {
            m_TextureBarriers.clear();
            m_BufferBarriers.clear();
        }

      private:
        MessageCallback *m_MessageCallback;
        bool m_HasErrors = false;

        void Error(const char *reason, const std::string *debugName = nullptr,
                   std::source_location location = std::source_location::current());
        bool CheckTexture(TextureStateExtension *texture, const TextureSubresourceSet *subresources = nullptr);
        bool CheckBuffer(BufferStateExtension *buffer);
        bool CheckState(ResourceStates state);

        std::optional<std::unordered_map<TextureStateExtension *, std::unique_ptr<TextureState>>> m_TextureStates;
        std::optional<std::unordered_map<BufferStateExtension *, std::unique_ptr<BufferState>>> m_BufferStates;

        // Deferred transitions of textures and buffers to permanent states.
        // They are executed only when the command list is executed, not when the app calls setPermanentTextureState or setPermanentBufferState.
        std::vector<std::pair<TextureStateExtension *, ResourceStates>> m_PermanentTextureStates;
        std::vector<std::pair<BufferStateExtension *, ResourceStates>> m_PermanentBufferStates;

        std::vector<TextureBarrier> m_TextureBarriers;
        std::vector<BufferBarrier> m_BufferBarriers;

        TextureState *GetTextureStateTracking(TextureStateExtension *texture, bool allowCreate);
        BufferState *GetBufferStateTracking(BufferStateExtension *buffer, bool allowCreate);
    };

    bool VerifyPermanentResourceState(ResourceStates permanentState, ResourceStates requiredState, bool isTexture,
                                      const std::string &debugName, MessageCallback *messageCallback);

    // Derives the state for SRV type resources used in binding sets.
    // We don't want to include the PixelShaderResource bit everywhere because it cannot be used in a compute queue.
    // So, use the binding layout's visibility mask to determine if a pixel shader might be necessary.
    ResourceStates GetShaderResourceStateForBindingLayout(BindingLayout *bindingLayout);

} // namespace Physara::RHI
