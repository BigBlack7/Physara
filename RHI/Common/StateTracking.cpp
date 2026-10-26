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

#include "StateTracking.hpp"

#include <algorithm>
#include <bit>
#include <cstdio>
#include <limits>
#include <new>
#include <sstream>
#include <stdexcept>

#include "Utils.hpp"

namespace Physara::RHI
{
    void CommandListResourceStateTracker::Error(const char *reason, const std::string *debugName,
                                                std::source_location location)
    {
        if (m_HasErrors)
        {
            return;
        }
        m_HasErrors = true;
        if (m_MessageCallback)
        {
            // 错误处理也覆盖分配失败, 诊断本身不申请存储. callback 必须不抛出且不重入.
            char message[1024];
            std::snprintf(message, sizeof(message), "InvalidState: %s: %s [%s] at %s:%u", location.function_name(),
                          reason, debugName ? Utils::DebugNameToString(*debugName) : "<recording>",
                          location.file_name(), location.line());
            m_MessageCallback->Message(MessageSeverity::Error, message);
        }
    }

    bool CommandListResourceStateTracker::CheckState(ResourceStates state)
    {
        constexpr uint32_t validBits = 0x03ffffffu;
        if (m_HasErrors)
        {
            return false;
        }
        if (!state || (uint32_t(state) & ~validBits) != 0)
        {
            Error("Unknown or invalid resource state bits");
            return false;
        }
        return true;
    }

    bool CommandListResourceStateTracker::CheckTexture(TextureStateExtension *texture,
                                                       const TextureSubresourceSet *subresources)
    {
        if (m_HasErrors)
        {
            return false;
        }
        if (!texture)
        {
            Error("null texture extension");
            return false;
        }
        const auto &desc = texture->descRef;
        const auto dimension = desc.dimension;
        const bool array =
            dimension == TextureDimension::Texture1DArray || dimension == TextureDimension::Texture2DArray ||
            dimension == TextureDimension::TextureCube || dimension == TextureDimension::TextureCubeArray ||
            dimension == TextureDimension::Texture2DMSArray;
        if (dimension < TextureDimension::Texture1D || dimension > TextureDimension::Texture3D || desc.width == 0 ||
            desc.height == 0 || desc.depth == 0 || desc.arraySize == 0 || desc.mipLevels == 0 ||
            desc.mipLevels > static_cast<uint32_t>(std::bit_width(std::max({desc.width, desc.height, desc.depth}))) ||
            (!array && desc.arraySize != 1) || (dimension != TextureDimension::Texture3D && desc.depth != 1) ||
            ((dimension == TextureDimension::Texture1D || dimension == TextureDimension::Texture1DArray) &&
             desc.height != 1))
        {
            Error("invalid texture dimensions/mip count", &desc.debugName);
            return false;
        }
        if (size_t(desc.arraySize) > std::vector<ResourceStates>().max_size() / desc.mipLevels)
        {
            Error("unrepresentable texture subresource count", &desc.debugName);
            return false;
        }
        if (subresources)
        {
            const auto resolved = subresources->Resolve(desc, false);
            if (resolved.numMipLevels == 0 || resolved.numArraySlices == 0)
            {
                Error("empty or out-of-range texture subresources", &desc.debugName);
                return false;
            }
        }
        if ((desc.keepInitialState && !CheckState(desc.initialState)) ||
            (texture->permanentState != ResourceStates::Unknown && !CheckState(texture->permanentState)))
        {
            return false;
        }
        return true;
    }

    bool CommandListResourceStateTracker::CheckBuffer(BufferStateExtension *buffer)
    {
        if (m_HasErrors)
        {
            return false;
        }
        if (!buffer || buffer->descRef.byteSize == 0)
        {
            Error("null buffer extension or empty buffer");
            return false;
        }
        return (!buffer->descRef.keepInitialState || CheckState(buffer->descRef.initialState)) &&
               (buffer->permanentState == ResourceStates::Unknown || CheckState(buffer->permanentState));
    }

    void CommandListResourceStateTracker::Reset()
    {
        // 每次 recording 结束释放借用指针和工作存储, 不留下无预算的跨录制高水位缓存.
        m_TextureStates.reset();
        m_BufferStates.reset();
        // 当前 MSVC profile 对空 vector 的 shrink_to_fit 直接释放数据, 保留原 Debug proxy.
        m_PermanentTextureStates.clear();
        m_PermanentTextureStates.shrink_to_fit();
        m_PermanentBufferStates.clear();
        m_PermanentBufferStates.shrink_to_fit();
        m_TextureBarriers.clear();
        m_TextureBarriers.shrink_to_fit();
        m_BufferBarriers.clear();
        m_BufferBarriers.shrink_to_fit();
        m_HasErrors = m_MessageCallback == nullptr;
    }

    bool VerifyPermanentResourceState(ResourceStates permanentState, ResourceStates requiredState, bool isTexture,
                                      const std::string &debugName, MessageCallback *messageCallback)
    {

        if ((permanentState & requiredState) != requiredState)
        {
            if (messageCallback)
            {
                char message[1024];
                const auto location = std::source_location::current();
                std::snprintf(
                    message, sizeof(message),
                    "InvalidState: VerifyPermanentResourceState: %s [%s], required=0x%x present=0x%x at %s:%u",
                    isTexture ? "texture" : "buffer", Utils::DebugNameToString(debugName), uint32_t(requiredState),
                    uint32_t(permanentState), location.file_name(), location.line());
                messageCallback->Message(MessageSeverity::Error, message);
            }
            return false;
        }
        return true;
    }

    static size_t CalcSubresource(MipLevel mipLevel, ArraySlice arraySlice, const TextureDesc &desc)
    {
        return size_t(mipLevel) + size_t(arraySlice) * desc.mipLevels;
    }
    // namespace

    void CommandListResourceStateTracker::SetEnableUavBarriersForTexture(TextureStateExtension *texture,
                                                                         bool enableBarriers)
    try
    {
        if (!CheckTexture(texture))
        {
            return;
        }

        TextureState *tracking = GetTextureStateTracking(texture, true);

        tracking->enableUavBarriers = enableBarriers;
        tracking->firstUavBarrierPlaced = false;
    }
    catch (const std::bad_alloc &)
    {
        Error("recording allocation failed");
        return;
    }
    catch (const std::length_error &)
    {
        Error("recording storage size is not representable");
        return;
    }

    void CommandListResourceStateTracker::SetEnableUavBarriersForBuffer(BufferStateExtension *buffer,
                                                                        bool enableBarriers)
    try
    {
        if (!CheckBuffer(buffer))
        {
            return;
        }

        BufferState *tracking = GetBufferStateTracking(buffer, true);

        tracking->enableUavBarriers = enableBarriers;
        tracking->firstUavBarrierPlaced = false;
    }
    catch (const std::bad_alloc &)
    {
        Error("recording allocation failed");
        return;
    }
    catch (const std::length_error &)
    {
        Error("recording storage size is not representable");
        return;
    }

    void CommandListResourceStateTracker::BeginTrackingTextureState(TextureStateExtension *texture,
                                                                    TextureSubresourceSet subresources,
                                                                    ResourceStates stateBits)
    try
    {
        if (!CheckTexture(texture, &subresources) || !CheckState(stateBits))
        {
            return;
        }

        const TextureDesc &desc = texture->descRef;

        TextureState *tracking = GetTextureStateTracking(texture, true);

        subresources = subresources.Resolve(desc, false);

        if (subresources.IsEntireTexture(desc))
        {
            tracking->state = stateBits;
            tracking->subresourceStates.clear();
        }
        else
        {
            tracking->subresourceStates.resize(size_t(desc.mipLevels) * desc.arraySize, tracking->state);
            tracking->state = ResourceStates::Unknown;

            for (MipLevel mipLevel = subresources.baseMipLevel;
                 mipLevel < subresources.baseMipLevel + subresources.numMipLevels; mipLevel++)
            {
                for (ArraySlice arraySlice = subresources.baseArraySlice;
                     arraySlice < subresources.baseArraySlice + subresources.numArraySlices; arraySlice++)
                {
                    size_t subresource = CalcSubresource(mipLevel, arraySlice, desc);
                    tracking->subresourceStates[subresource] = stateBits;
                }
            }
        }
    }
    catch (const std::bad_alloc &)
    {
        Error("recording allocation failed");
        return;
    }
    catch (const std::length_error &)
    {
        Error("recording storage size is not representable");
        return;
    }

    void CommandListResourceStateTracker::BeginTrackingBufferState(BufferStateExtension *buffer,
                                                                   ResourceStates stateBits)
    try
    {
        if (!CheckBuffer(buffer) || !CheckState(stateBits))
        {
            return;
        }

        BufferState *tracking = GetBufferStateTracking(buffer, true);

        tracking->state = stateBits;
    }
    catch (const std::bad_alloc &)
    {
        Error("recording allocation failed");
        return;
    }
    catch (const std::length_error &)
    {
        Error("recording storage size is not representable");
        return;
    }

    void CommandListResourceStateTracker::SetPermanentTextureState(TextureStateExtension *texture,
                                                                   TextureSubresourceSet subresources,
                                                                   ResourceStates stateBits)
    try
    {
        if (!CheckTexture(texture, &subresources) || !CheckState(stateBits))
        {
            return;
        }

        const TextureDesc &desc = texture->descRef;
        subresources = subresources.Resolve(desc, false);
        if (!subresources.IsEntireTexture(desc))
        {
            Error("permanent transition requires the entire texture", &desc.debugName);
            return;
        }
        if (texture->permanentState != ResourceStates::Unknown && texture->permanentState != stateBits)
        {
            Error("conflicting existing permanent texture state", &desc.debugName);
            return;
        }
        for (const auto &[resource, state] : m_PermanentTextureStates)
        {
            if (resource == texture)
            {
                if (state != stateBits)
                {
                    Error("conflicting pending permanent texture state", &desc.debugName);
                }
                return;
            }
        }
        RequireTextureState(texture, subresources, stateBits);
        if (HasErrors())
        {
            return;
        }
        m_PermanentTextureStates.emplace_back(texture, stateBits);
        GetTextureStateTracking(texture, true)->permanentTransition = true;
    }
    catch (const std::bad_alloc &)
    {
        Error("recording allocation failed");
        return;
    }
    catch (const std::length_error &)
    {
        Error("recording storage size is not representable");
        return;
    }

    void CommandListResourceStateTracker::SetPermanentBufferState(BufferStateExtension *buffer,
                                                                  ResourceStates stateBits)
    try
    {
        if (!CheckBuffer(buffer) || !CheckState(stateBits))
        {
            return;
        }

        if (buffer->permanentState != ResourceStates::Unknown && buffer->permanentState != stateBits)
        {
            Error("conflicting existing permanent buffer state", &buffer->descRef.debugName);
            return;
        }
        for (const auto &[resource, state] : m_PermanentBufferStates)
        {
            if (resource == buffer)
            {
                if (state != stateBits)
                {
                    Error("conflicting pending permanent buffer state", &buffer->descRef.debugName);
                }
                return;
            }
        }
        RequireBufferState(buffer, stateBits);
        if (HasErrors())
        {
            return;
        }
        m_PermanentBufferStates.emplace_back(buffer, stateBits);
        GetBufferStateTracking(buffer, true)->permanentTransition = true;
    }
    catch (const std::bad_alloc &)
    {
        Error("recording allocation failed");
        return;
    }
    catch (const std::length_error &)
    {
        Error("recording storage size is not representable");
        return;
    }

    ResourceStates CommandListResourceStateTracker::GetTextureSubresourceState(TextureStateExtension *texture,
                                                                               ArraySlice arraySlice, MipLevel mipLevel)
    try
    {
        if (!CheckTexture(texture))
        {
            return ResourceStates::Unknown;
        }
        if (arraySlice >= texture->descRef.arraySize || mipLevel >= texture->descRef.mipLevels)
        {
            Error("texture subresource index out of range", &texture->descRef.debugName);
            return ResourceStates::Unknown;
        }

        TextureState *tracking = GetTextureStateTracking(texture, false);
        if (!tracking)
        {
            return texture->descRef.keepInitialState
                       ? (texture->stateInitialized ? texture->descRef.initialState : ResourceStates::Common)
                       : ResourceStates::Unknown;
        }

        // whole resource
        if (tracking->subresourceStates.empty())
        {
            return tracking->state;
        }

        size_t subresource = CalcSubresource(mipLevel, arraySlice, texture->descRef);
        return tracking->subresourceStates[subresource];
    }
    catch (const std::bad_alloc &)
    {
        Error("recording allocation failed");
        return ResourceStates::Unknown;
    }
    catch (const std::length_error &)
    {
        Error("recording storage size is not representable");
        return ResourceStates::Unknown;
    }

    ResourceStates CommandListResourceStateTracker::GetBufferState(BufferStateExtension *buffer)
    try
    {
        if (!CheckBuffer(buffer))
        {
            return ResourceStates::Unknown;
        }

        BufferState *tracking = GetBufferStateTracking(buffer, false);

        if (!tracking)
        {
            return ResourceStates::Unknown;
        }

        return tracking->state;
    }
    catch (const std::bad_alloc &)
    {
        Error("recording allocation failed");
        return ResourceStates::Unknown;
    }
    catch (const std::length_error &)
    {
        Error("recording storage size is not representable");
        return ResourceStates::Unknown;
    }

    void CommandListResourceStateTracker::RequireTextureState(TextureStateExtension *texture,
                                                              TextureSubresourceSet subresources, ResourceStates state)
    try
    {
        if (!CheckTexture(texture, &subresources) || !CheckState(state))
        {
            return;
        }
        for (const auto &[pendingResource, pendingState] : m_PermanentTextureStates)
        {
            if (pendingResource == texture)
            {
                if ((pendingState & state) != state)
                {
                    Error("request conflicts with pending permanent state", &texture->descRef.debugName);
                }
                return;
            }
        }

        if (texture->permanentState != 0)
        {
            if (!VerifyPermanentResourceState(texture->permanentState, state, true, texture->descRef.debugName,
                                              m_MessageCallback))
            {
                m_HasErrors = true;
            }
            return;
        }

        subresources = subresources.Resolve(texture->descRef, false);

        TextureState *tracking = GetTextureStateTracking(texture, true);

        if (tracking->subresourceStates.empty() && tracking->state == ResourceStates::Unknown)
        {
            std::stringstream ss;
            ss << "Unknown prior state of texture " << Utils::DebugNameToString(texture->descRef.debugName)
               << ". "
                  "Call CommandList::BeginTrackingTextureState(...) before using the texture or use the "
                  "keepInitialState and initialState members of TextureDesc.";
            Error(ss.str().c_str());
            return;
        }

        if (subresources.IsEntireTexture(texture->descRef) && tracking->subresourceStates.empty())
        {
            // We're requiring state for the entire texture, and it's been tracked as entire texture too

            bool transitionNecessary = tracking->state != state;
            bool uavNecessary = ((state & ResourceStates::UnorderedAccess) != 0) &&
                                (tracking->enableUavBarriers || !tracking->firstUavBarrierPlaced);

            if (transitionNecessary || uavNecessary)
            {
                TextureBarrier barrier;
                barrier.texture = texture;
                barrier.entireTexture = true;
                barrier.stateBefore = tracking->state;
                barrier.stateAfter = state;
                m_TextureBarriers.push_back(barrier);
            }

            tracking->state = state;

            if (uavNecessary && !transitionNecessary)
            {
                tracking->firstUavBarrierPlaced = true;
            }
        }
        else
        {
            // Transition individual subresources

            // Make sure that we're tracking the texture on subresource level
            bool stateExpanded = false;
            if (tracking->subresourceStates.empty())
            {
                tracking->subresourceStates.resize(size_t(texture->descRef.mipLevels) * texture->descRef.arraySize,
                                                   tracking->state);
                tracking->state = ResourceStates::Unknown;
                stateExpanded = true;
            }

            bool anyUavBarrier = false;

            for (ArraySlice arraySlice = subresources.baseArraySlice;
                 arraySlice < subresources.baseArraySlice + subresources.numArraySlices; arraySlice++)
            {
                for (MipLevel mipLevel = subresources.baseMipLevel;
                     mipLevel < subresources.baseMipLevel + subresources.numMipLevels; mipLevel++)
                {
                    size_t subresourceIndex = CalcSubresource(mipLevel, arraySlice, texture->descRef);

                    auto priorState = tracking->subresourceStates[subresourceIndex];

                    if (priorState == ResourceStates::Unknown && !stateExpanded)
                    {
                        std::stringstream ss;
                        ss << "Unknown prior state of texture " << Utils::DebugNameToString(texture->descRef.debugName)
                           << " subresource (MipLevel = " << mipLevel << ", ArraySlice = " << arraySlice
                           << "). "
                              "Call CommandList::BeginTrackingTextureState(...) before using the texture or use the "
                              "keepInitialState and initialState members of TextureDesc.";
                        Error(ss.str().c_str());
                        return;
                    }

                    bool transitionNecessary = priorState != state;
                    bool uavNecessary = ((state & ResourceStates::UnorderedAccess) != 0) && !anyUavBarrier &&
                                        (tracking->enableUavBarriers || !tracking->firstUavBarrierPlaced);

                    if (transitionNecessary || uavNecessary)
                    {
                        TextureBarrier barrier;
                        barrier.texture = texture;
                        barrier.entireTexture = false;
                        barrier.mipLevel = mipLevel;
                        barrier.arraySlice = arraySlice;
                        barrier.stateBefore = priorState;
                        barrier.stateAfter = state;
                        m_TextureBarriers.push_back(barrier);
                    }

                    tracking->subresourceStates[subresourceIndex] = state;

                    if (uavNecessary && !transitionNecessary)
                    {
                        anyUavBarrier = true;
                        tracking->firstUavBarrierPlaced = true;
                    }
                }
            }
        }
    }
    catch (const std::bad_alloc &)
    {
        Error("recording allocation failed");
        return;
    }
    catch (const std::length_error &)
    {
        Error("recording storage size is not representable");
        return;
    }

    void CommandListResourceStateTracker::RequireBufferState(BufferStateExtension *buffer, ResourceStates state)
    try
    {
        if (!CheckBuffer(buffer) || !CheckState(state))
        {
            return;
        }
        for (const auto &[pendingResource, pendingState] : m_PermanentBufferStates)
        {
            if (pendingResource == buffer)
            {
                if ((pendingState & state) != state)
                {
                    Error("request conflicts with pending permanent state", &buffer->descRef.debugName);
                }
                return;
            }
        }

        if (buffer->descRef.isVolatile)
        {
            return;
        }

        if (buffer->permanentState != 0)
        {
            if (!VerifyPermanentResourceState(buffer->permanentState, state, false, buffer->descRef.debugName,
                                              m_MessageCallback))
            {
                m_HasErrors = true;
            }

            return;
        }

        if (buffer->descRef.cpuAccess != CpuAccessMode::None)
        {
            // CPU-visible buffers can't change state
            return;
        }

        BufferState *tracking = GetBufferStateTracking(buffer, true);

        if (tracking->state == ResourceStates::Unknown)
        {
            std::stringstream ss;
            ss << "Unknown prior state of buffer " << Utils::DebugNameToString(buffer->descRef.debugName)
               << ". "
                  "Call CommandList::BeginTrackingBufferState(...) before using the buffer or use the "
                  "keepInitialState and initialState members of BufferDesc.";
            Error(ss.str().c_str());
            return;
        }

        bool transitionNecessary = tracking->state != state;
        bool uavNecessary = ((state & ResourceStates::UnorderedAccess) != 0) &&
                            (tracking->enableUavBarriers || !tracking->firstUavBarrierPlaced);

        if (transitionNecessary)
        {
            // See if this buffer is already used for a different purpose in this batch.
            // If it is, combine the state bits.
            // Example: same buffer used as index and vertex buffer, or as SRV and indirect arguments.
            for (BufferBarrier &barrier : m_BufferBarriers)
            {
                if (barrier.buffer == buffer)
                {
                    barrier.stateAfter = ResourceStates(barrier.stateAfter | state);
                    tracking->state = barrier.stateAfter;
                    return;
                }
            }
        }

        if (transitionNecessary || uavNecessary)
        {
            BufferBarrier barrier;
            barrier.buffer = buffer;
            barrier.stateBefore = tracking->state;
            barrier.stateAfter = state;
            m_BufferBarriers.push_back(barrier);
        }

        if (uavNecessary && !transitionNecessary)
        {
            tracking->firstUavBarrierPlaced = true;
        }

        tracking->state = state;
    }
    catch (const std::bad_alloc &)
    {
        Error("recording allocation failed");
        return;
    }
    catch (const std::length_error &)
    {
        Error("recording storage size is not representable");
        return;
    }

    void CommandListResourceStateTracker::KeepBufferInitialStates()
    try
    {
        if (HasErrors() || !m_BufferStates)
        {
            return;
        }

        for (auto &[buffer, tracking] : *m_BufferStates)
        {
            if (buffer->descRef.keepInitialState && !buffer->permanentState && !buffer->descRef.isVolatile &&
                !tracking->permanentTransition)
            {
                RequireBufferState(buffer, buffer->descRef.initialState);
            }
        }
    }
    catch (const std::bad_alloc &)
    {
        Error("recording allocation failed");
        return;
    }
    catch (const std::length_error &)
    {
        Error("recording storage size is not representable");
        return;
    }

    void CommandListResourceStateTracker::KeepTextureInitialStates()
    try
    {
        if (HasErrors() || !m_TextureStates)
        {
            return;
        }

        for (auto &[texture, tracking] : *m_TextureStates)
        {
            if (texture->descRef.keepInitialState && !texture->permanentState && !tracking->permanentTransition)
            {
                RequireTextureState(texture, AllSubresources, texture->descRef.initialState);
            }
        }
    }
    catch (const std::bad_alloc &)
    {
        Error("recording allocation failed");
        return;
    }
    catch (const std::length_error &)
    {
        Error("recording storage size is not representable");
        return;
    }

    void CommandListResourceStateTracker::CommandListSubmitted()
    {

        if (HasErrors())
        {
            return;
        }
        if (!m_TextureBarriers.empty() || !m_BufferBarriers.empty())
        {
            Error("CommandListSubmitted requires consumed dependencies");
            return;
        }
        // 提交通知前再次整体预检, 不逐项写入后才发现冲突. 无分配的发布阶段不会部分成功.
        for (const auto &[texture, state] : m_PermanentTextureStates)
        {
            if (texture->permanentState != ResourceStates::Unknown && texture->permanentState != state)
            {
                Error("permanent texture state changed before submission", &texture->descRef.debugName);
                return;
            }
        }
        for (const auto &[buffer, state] : m_PermanentBufferStates)
        {
            if (buffer->permanentState != ResourceStates::Unknown && buffer->permanentState != state)
            {
                Error("permanent buffer state changed before submission", &buffer->descRef.debugName);
                return;
            }
        }
        for (const auto &[texture, state] : m_PermanentTextureStates)
        {
            texture->permanentState = state;
        }
        for (const auto &[buffer, state] : m_PermanentBufferStates)
        {
            buffer->permanentState = state;
        }
        if (m_TextureStates)
        {
            for (const auto &[texture, stateTracking] : *m_TextureStates)
            {
                if (texture->descRef.keepInitialState && !texture->stateInitialized)
                {
                    texture->stateInitialized = true;
                }
            }
        }
        Reset();
    }

    TextureState *CommandListResourceStateTracker::GetTextureStateTracking(TextureStateExtension *texture,
                                                                           bool allowCreate)
    {
        if (!m_TextureStates)
        {
            if (!allowCreate)
            {
                return nullptr;
            }
            m_TextureStates.emplace();
        }
        auto it = m_TextureStates->find(texture);

        if (it != m_TextureStates->end())
        {
            return it->second.get();
        }

        if (!allowCreate)
        {
            return nullptr;
        }

        std::unique_ptr<TextureState> trackingRef = std::make_unique<TextureState>();

        TextureState *tracking = trackingRef.get();
        m_TextureStates->insert(std::make_pair(texture, std::move(trackingRef)));

        if (texture->descRef.keepInitialState)
        {
            tracking->state = texture->stateInitialized ? texture->descRef.initialState : ResourceStates::Common;
        }

        return tracking;
    }

    BufferState *CommandListResourceStateTracker::GetBufferStateTracking(BufferStateExtension *buffer, bool allowCreate)
    {
        if (!m_BufferStates)
        {
            if (!allowCreate)
            {
                return nullptr;
            }
            m_BufferStates.emplace();
        }
        auto it = m_BufferStates->find(buffer);

        if (it != m_BufferStates->end())
        {
            return it->second.get();
        }

        if (!allowCreate)
        {
            return nullptr;
        }

        std::unique_ptr<BufferState> trackingRef = std::make_unique<BufferState>();

        BufferState *tracking = trackingRef.get();
        m_BufferStates->insert(std::make_pair(buffer, std::move(trackingRef)));

        if (buffer->descRef.keepInitialState)
        {
            tracking->state = buffer->descRef.initialState;
        }

        return tracking;
    }

    ResourceStates GetShaderResourceStateForBindingLayout(BindingLayout *bindingLayout)
    {
        ShaderType const visibility = bindingLayout->GetDesc()->visibility;

        bool const hasPixelShader = (visibility & ShaderType::Pixel) != 0;

        return hasPixelShader ? ResourceStates::ShaderResource : ResourceStates::NonPixelShaderResource;
    }

} // namespace Physara::RHI
