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

/// @file
/// @brief 单 recording 的抽象依赖与提交元数据合同, extension 由后端保活, 本层不发原生命令或等待 GPU.

#include <map>
#include <memory>
#include <optional>
#include <source_location>
#include <string>
#include <utility>
#include <vector>

#include "RHI.hpp"

namespace Physara::RHI
{
    /// @brief 资源 wrapper 中的持久跟踪元数据, 借用不可变 desc, permanentState 仅在接受提交后发布.
    struct BufferStateExtension
    {
        // descRef 借用资源 wrapper 内的不可变描述; permanentState 只由已接受提交发布.
        const BufferDesc &descRef;
        ResourceStates permanentState = ResourceStates::Unknown;

        explicit BufferStateExtension(const BufferDesc &desc) : descRef(desc)
        {
        }
    };

    /// @brief 纹理持久元数据, stateInitialized 只承诺 keepInitialState 提交协议, 不证明 texel 有效.
    struct TextureStateExtension
    {
        // stateInitialized 只表示 keepInitialState 的提交协议已建立, 不保证任意 mip/layer 含有效 texel.
        const TextureDesc &descRef;
        ResourceStates permanentState = ResourceStates::Unknown;
        bool stateInitialized = false;
        bool isSamplerFeedback = false;

        explicit TextureStateExtension(const TextureDesc &desc) : descRef(desc)
        {
        }
    };

    /// @brief 单纹理的 recording 本地状态, 整图与逐子资源表示互斥, 不立即修改持久 extension.
    struct TextureState
    {
        // subresourceStates 为空时用 state 表示整图; 展开后由 mip + layer*mipLevels 索引各子资源.
        // count=0 构造允许 Debug proxy 分配失败传播到 tracker 的 catch, 不使用 noexcept 默认构造.
        std::vector<ResourceStates> subresourceStates = std::vector<ResourceStates>(0);
        ResourceStates state = ResourceStates::Unknown;
        bool enableUavBarriers = true;
        bool firstUavBarrierPlaced = false;
        bool permanentTransition = false;
    };

    /// @brief Buffer 的 recording 本地用途与 UAV 顺序策略, permanentTransition 约束后续用途.
    struct BufferState
    {
        ResourceStates state = ResourceStates::Unknown;
        bool enableUavBarriers = true;
        bool firstUavBarrierPlaced = false;
        bool permanentTransition = false;
    };

    /// @brief 待后端消费的抽象纹理依赖, 指针借用 extension, entireTexture 决定范围解释.
    struct TextureBarrier
    {
        TextureStateExtension *texture = nullptr;
        MipLevel mipLevel = 0;
        ArraySlice arraySlice = 0;
        bool entireTexture = false;
        ResourceStates stateBefore = ResourceStates::Unknown;
        ResourceStates stateAfter = ResourceStates::Unknown;
    };

    /// @brief Buffer 的前后访问用途依赖, 不携带 GPU 完成票据.
    struct BufferBarrier
    {
        BufferStateExtension *buffer = nullptr;
        ResourceStates stateBefore = ResourceStates::Unknown;
        ResourceStates stateAfter = ResourceStates::Unknown;
    };

    /// @brief 单写者 recording 工作集, 声明前态, 生成依赖, 提交发布与放弃分别执行.
    /// @note descRef/extension/callback 均借用, 地址与内容稳定至 Submitted/Reset/析构;
    /// ClearBarriers 仅消费依赖, Submitted 仅发布接受提交的 metadata, 两者不证明 GPU 完成.
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

        /// @brief 改变该资源在本 recording 中的同态 UAV 顺序策略, Buffer 重载同义.
        void SetEnableUavBarriersForTexture(TextureStateExtension *texture, bool enableBarriers);
        void SetEnableUavBarriersForBuffer(BufferStateExtension *buffer, bool enableBarriers);

        /// @brief 声明已知前态, Buffer 重载同义; 只改本地记录, 不生成前态转换.
        void BeginTrackingTextureState(TextureStateExtension *texture, TextureSubresourceSet subresources,
                                       ResourceStates stateBits);
        void BeginTrackingBufferState(BufferStateExtension *buffer, ResourceStates stateBits);

        /// @brief 要求不可再改变的目标用途并暂存发布, Buffer 重载同义, 真正提交前不改 extension.
        void SetPermanentTextureState(TextureStateExtension *texture, TextureSubresourceSet subresources,
                                      ResourceStates stateBits);
        void SetPermanentBufferState(BufferStateExtension *buffer, ResourceStates stateBits);

        /// @brief 查询本 recording 的已知用途, 无记录时可为 Unknown; Buffer 查询遵循同一规则.
        ResourceStates GetTextureSubresourceState(TextureStateExtension *texture, ArraySlice arraySlice,
                                                  MipLevel mipLevel);
        ResourceStates GetBufferState(BufferStateExtension *buffer);

        /// @brief 要求后继访问并生成必要依赖, Buffer 重载同义, sticky error 后不得据此发原生工作.
        void RequireTextureState(TextureStateExtension *texture, TextureSubresourceSet subresources,
                                 ResourceStates state);
        void RequireBufferState(BufferStateExtension *buffer, ResourceStates state);

        /// @brief 为 keepInitialState 资源恢复初态, Texture 版本按同一提交协议处理.
        void KeepBufferInitialStates();
        void KeepTextureInitialStates();
        /// @brief 接受提交后整体预检并发布持久 metadata, 再释放本地记录; 失败或放弃用 Reset.
        void CommandListSubmitted();

        // 错误为本次 recording 的 sticky 状态; 必须先检查再发射. Reset 放弃本地工作, 不发布 metadata.
        [[nodiscard]] bool HasErrors() const
        {
            return m_HasErrors;
        }
        /// @brief 放弃本地状态, 借用和依赖并清 sticky error, 不发布资源持久 metadata.
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

        // 锁存本次 recording 失败并以有界正文报告, 诊断不为 OOM 再次分配.
        void Error(const char *reason, const std::string *debugName = nullptr,
                   std::source_location location = std::source_location::current());
        // 核对 extension 与描述可表示性及可选范围, 原生能力和实际 Device 归属仍由后端检查.
        bool CheckTexture(TextureStateExtension *texture, const TextureSubresourceSet *subresources = nullptr);
        // 核对 Buffer extension 与描述的一致前提, 不推断资源已初始化.
        bool CheckBuffer(BufferStateExtension *buffer);
        // 检查非零且仅含已知的用途位, 发现非法值锁存本次 recording 错误.
        bool CheckState(ResourceStates state);

        // 有序索引避开 MSVC Debug hash 容器的 noexcept proxy 分配; 建表/插入失败由录制入口锁存.
        // key 只标识稳定的 extension 地址, 默认 std::less 提供指针全序; map 不拥有 extension.
        std::optional<std::map<TextureStateExtension *, std::unique_ptr<TextureState>>> m_TextureStates;
        std::optional<std::map<BufferStateExtension *, std::unique_ptr<BufferState>>> m_BufferStates;

        // Deferred transitions of textures and buffers to permanent states.
        // They are executed only when the command list is executed, not when the app calls setPermanentTextureState or setPermanentBufferState.
        std::vector<std::pair<TextureStateExtension *, ResourceStates>> m_PermanentTextureStates;
        std::vector<std::pair<BufferStateExtension *, ResourceStates>> m_PermanentBufferStates;

        std::vector<TextureBarrier> m_TextureBarriers;
        std::vector<BufferBarrier> m_BufferBarriers;

        // 按需建立本地纹理记录, 首次 keepInitialState 从 Common 开始, 已接受代沿用承诺初态.
        TextureState *GetTextureStateTracking(TextureStateExtension *texture, bool allowCreate);
        // 按 extension 地址取得或创建本地 Buffer 记录, 表只拥有 tracking, 不拥有 extension.
        BufferState *GetBufferStateTracking(BufferStateExtension *buffer, bool allowCreate);
    };

    /// @brief 检查所需用途是否被永久状态包含, 冲突报告但不修改资源或生成转换.
    bool VerifyPermanentResourceState(ResourceStates permanentState, ResourceStates requiredState, bool isTexture,
                                      const std::string &debugName, MessageCallback *messageCallback);

    // Derives the state for SRV type resources used in binding sets.
    // We don't want to include the PixelShaderResource bit everywhere because it cannot be used in a compute queue.
    // So, use the binding layout's visibility mask to determine if a pixel shader might be necessary.
    ResourceStates GetShaderResourceStateForBindingLayout(BindingLayout *bindingLayout);

} // namespace Physara::RHI
