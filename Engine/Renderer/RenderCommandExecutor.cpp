#include "RenderCommandExecutor.hpp"

#include <algorithm>

#include <Engine/Renderer/FrameData.hpp>
#include <Engine/Renderer/MeshGPUCache.hpp>
#include <Engine/Renderer/RenderProxy.hpp>
#include <Engine/Resource/AssetManager.hpp>
#include <Engine/RHI/Command/RHICommandList.hpp>
#include <Engine/RHI/Core/RHIDevice.hpp>
#include <Engine/RHI/Descriptors/RHIBufferDesc.hpp>

namespace Physara::Engine
{
    namespace RenderCommandExecutorDetail
    {
        constexpr std::uint32_t MinIndirectRunCommandCount = 2u;
    }

    void RenderCommandExecutor::BeginFrame()
    {
        m_IndirectBufferCursor = 0;
        m_IndirectCommandScratch.clear();
        m_CommandPrimitiveScratch.clear();
        m_IndirectRuns.clear();
    }

    void RenderCommandExecutor::Submit(
        const RenderCommandExecutorContext &context,
        std::span<const RenderCommand> commands,
        const RenderCommandSubmitCallbacks &callbacks)
    {
        if (context.commandList == nullptr || context.device == nullptr || context.meshCache == nullptr ||
            commands.empty())
        {
            return;
        }

        PreparePrimitives(context, commands);
        BuildIndirectRuns(context, commands, callbacks);
        std::uint32_t runIndex = 0;
        for (std::uint32_t commandIndex = 0; commandIndex < static_cast<std::uint32_t>(commands.size());)
        {
            if (runIndex < m_IndirectRuns.size() && m_IndirectRuns[runIndex].commandIndex == commandIndex)
            {
                const IndirectRun &run = m_IndirectRuns[runIndex++];
                SubmitIndirectRun(context, commands, run, callbacks);
                commandIndex += run.commandCount;
                continue;
            }

            SubmitDirectCommand(context, commands, commandIndex, callbacks);
            ++commandIndex;
        }
    }

    void RenderCommandExecutor::Reset()
    {
        m_IndirectCommandBuffers.clear();
        BeginFrame();
    }

    void RenderCommandExecutor::PreparePrimitives(
        const RenderCommandExecutorContext &context,
        std::span<const RenderCommand> commands)
    {
        m_CommandPrimitiveScratch.clear();
        m_CommandPrimitiveScratch.reserve(commands.size());
        for (const RenderCommand &command : commands)
        {
            m_CommandPrimitiveScratch.push_back(
                context.meshCache->GetOrCreate(context.device, context.assetManager, command, context.stats));
        }
    }

    void RenderCommandExecutor::BuildIndirectRuns(
        const RenderCommandExecutorContext &context,
        std::span<const RenderCommand> commands,
        const RenderCommandSubmitCallbacks &callbacks)
    {
        m_IndirectRuns.clear();
        m_IndirectCommandScratch.clear();
        if (commands.size() < RenderCommandExecutorDetail::MinIndirectRunCommandCount)
        {
            return;
        }

        const RenderCommandMergeCallback mergeCallback = callbacks.canMergeIndirectRun != nullptr
                                                             ? callbacks.canMergeIndirectRun
                                                             : &RenderCommandExecutor::CanMergeDefault;
        for (std::uint32_t commandIndex = 0; commandIndex < static_cast<std::uint32_t>(commands.size());)
        {
            const RenderCommand &firstCommand = commands[commandIndex];
            MeshGPUPrimitive *firstPrimitive = m_CommandPrimitiveScratch[commandIndex];
            if (firstPrimitive == nullptr || firstPrimitive->indexCount == 0)
            {
                if (context.stats != nullptr)
                {
                    ++context.stats->indirectInvalidBreaks;
                }
                ++commandIndex;
                continue;
            }

            std::uint32_t runCommandCount = 1u;
            while (commandIndex + runCommandCount < static_cast<std::uint32_t>(commands.size()))
            {
                if (!mergeCallback(callbacks.userData, firstCommand, commands[commandIndex + runCommandCount]))
                {
                    if (context.stats != nullptr)
                    {
                        ++context.stats->indirectMergeBreaks;
                    }
                    break;
                }
                MeshGPUPrimitive *nextPrimitive = m_CommandPrimitiveScratch[commandIndex + runCommandCount];
                if (nextPrimitive == nullptr || nextPrimitive->indexCount == 0 || !CanUseIndirectRun(*firstPrimitive, *nextPrimitive))
                {
                    if (context.stats != nullptr)
                    {
                        if (nextPrimitive == nullptr || nextPrimitive->indexCount == 0)
                        {
                            ++context.stats->indirectInvalidBreaks;
                        }
                        else
                        {
                            ++context.stats->indirectGeometryBreaks;
                        }
                    }
                    break;
                }
                ++runCommandCount;
            }

            if (runCommandCount < RenderCommandExecutorDetail::MinIndirectRunCommandCount)
            {
                if (context.stats != nullptr)
                {
                    ++context.stats->indirectShortRuns;
                }
                commandIndex += runCommandCount;
                continue;
            }

            const std::uint32_t indirectCommandOffset =
                static_cast<std::uint32_t>(m_IndirectCommandScratch.size() * sizeof(RHI::RHIDrawIndexedIndirectCommand));
            for (std::uint32_t i = 0; i < runCommandCount; ++i)
            {
                const std::uint32_t currentIndex = commandIndex + i;
                const RenderCommand &command = commands[currentIndex];
                MeshGPUPrimitive *primitive = m_CommandPrimitiveScratch[currentIndex];

                RHI::RHIDrawIndexedIndirectCommand indirectCommand{};
                indirectCommand.indexCount = primitive->indexCount;
                indirectCommand.instanceCount = command.instanceCount;
                indirectCommand.firstIndex = primitive->firstIndex;
                indirectCommand.vertexOffset = primitive->vertexOffset;
                indirectCommand.firstInstance = command.firstInstanceIndex;
                m_IndirectCommandScratch.push_back(indirectCommand);
            }

            m_IndirectRuns.push_back(IndirectRun{commandIndex, runCommandCount, indirectCommandOffset, nullptr});
            commandIndex += runCommandCount;
        }

        RHI::RHIBuffer *indirectBuffer = UploadIndirectCommands(context);
        if (indirectBuffer == nullptr)
        {
            m_IndirectRuns.clear();
            m_IndirectCommandScratch.clear();
            return;
        }

        for (IndirectRun &run : m_IndirectRuns)
        {
            run.indirectBuffer = indirectBuffer;
        }
    }

    RHI::RHIBuffer *RenderCommandExecutor::UploadIndirectCommands(const RenderCommandExecutorContext &context)
    {
        if (context.device == nullptr || m_IndirectCommandScratch.empty())
        {
            return nullptr;
        }

        if (m_IndirectBufferCursor >= m_IndirectCommandBuffers.size())
        {
            m_IndirectCommandBuffers.push_back(nullptr);
        }

        const std::uint32_t requiredBytes =
            static_cast<std::uint32_t>(m_IndirectCommandScratch.size() * sizeof(RHI::RHIDrawIndexedIndirectCommand));
        std::unique_ptr<RHI::RHIBuffer> &buffer = m_IndirectCommandBuffers[m_IndirectBufferCursor++];
        if (buffer == nullptr || buffer->GetSize() < requiredBytes)
        {
            std::uint32_t commandCapacity = 32u;
            const std::uint32_t requiredCommands = static_cast<std::uint32_t>(m_IndirectCommandScratch.size());
            while (commandCapacity < requiredCommands)
            {
                commandCapacity *= 2u;
            }

            RHI::RHIBufferDesc desc{};
            desc.size = commandCapacity * static_cast<std::uint32_t>(sizeof(RHI::RHIDrawIndexedIndirectCommand));
            desc.usage = RHI::BufferUsage::Indirect;
            desc.dynamic = true;
            buffer = context.device->CreateBuffer(desc);
        }

        if (buffer == nullptr)
        {
            return nullptr;
        }

        buffer->UploadData(m_IndirectCommandScratch.data(), requiredBytes);
        if (context.stats != nullptr)
        {
            context.stats->bufferUploadBytes += requiredBytes;
            ++context.stats->bufferUploadChunks;
        }
        return buffer.get();
    }

    bool RenderCommandExecutor::CanUseIndirectRun(const MeshGPUPrimitive &first, const MeshGPUPrimitive &next)
    {
        return first.indexBinding.buffer == next.indexBinding.buffer &&
               first.indexBinding.offset == next.indexBinding.offset &&
               first.vertexBindings[0].buffer == next.vertexBindings[0].buffer &&
               first.vertexBindings[0].offset == next.vertexBindings[0].offset &&
               first.vertexBindings[0].slot == next.vertexBindings[0].slot;
    }

    bool RenderCommandExecutor::CanMergeDefault(void *userData, const RenderCommand &lhs, const RenderCommand &rhs)
    {
        (void)userData;
        return lhs.meshKey == rhs.meshKey;
    }

    bool RenderCommandExecutor::BindCommand(
        const RenderCommandSubmitCallbacks &callbacks,
        const RenderCommand &command)
    {
        return callbacks.bindCommand == nullptr || callbacks.bindCommand(callbacks.userData, command);
    }

    void RenderCommandExecutor::RecordCommand(
        const RenderCommandSubmitCallbacks &callbacks,
        const RenderCommand &command,
        const MeshGPUPrimitive &primitive,
        RenderCommandSubmitMode mode)
    {
        if (callbacks.recordCommand != nullptr)
        {
            callbacks.recordCommand(callbacks.userData, command, primitive, mode);
        }
    }

    void RenderCommandExecutor::SubmitDirectCommand(
        const RenderCommandExecutorContext &context,
        std::span<const RenderCommand> commands,
        std::uint32_t commandIndex,
        const RenderCommandSubmitCallbacks &callbacks)
    {
        MeshGPUPrimitive *primitive = m_CommandPrimitiveScratch[commandIndex];
        if (primitive == nullptr || primitive->indexCount == 0 || !BindCommand(callbacks, commands[commandIndex]))
        {
            return;
        }

        context.commandList->SetRenderPrimitive(primitive->AsRHIRenderPrimitive());
        context.commandList->DrawIndexed(
            primitive->indexCount,
            commands[commandIndex].instanceCount,
            primitive->firstIndex,
            primitive->vertexOffset,
            commands[commandIndex].firstInstanceIndex);
        if (context.stats != nullptr)
        {
            ++context.stats->directSubmittedCommands;
        }
        RecordCommand(callbacks, commands[commandIndex], *primitive, RenderCommandSubmitMode::Direct);
    }

    void RenderCommandExecutor::SubmitIndirectRun(
        const RenderCommandExecutorContext &context,
        std::span<const RenderCommand> commands,
        const IndirectRun &run,
        const RenderCommandSubmitCallbacks &callbacks)
    {
        if (run.indirectBuffer == nullptr || run.commandIndex >= commands.size() ||
            !BindCommand(callbacks, commands[run.commandIndex]))
        {
            return;
        }

        MeshGPUPrimitive *firstPrimitive = m_CommandPrimitiveScratch[run.commandIndex];
        if (firstPrimitive == nullptr || firstPrimitive->indexCount == 0)
        {
            return;
        }

        context.commandList->SetRenderPrimitive(firstPrimitive->AsRHIRenderPrimitive());
        context.commandList->DrawIndexedIndirect(
            run.indirectBuffer,
            run.commandCount,
            static_cast<std::uint32_t>(sizeof(RHI::RHIDrawIndexedIndirectCommand)),
            run.indirectCommandOffset);
        if (context.stats != nullptr)
        {
            ++context.stats->indirectRuns;
            context.stats->indirectRunCommands += run.commandCount;
            context.stats->maxIndirectRunCommands =
                std::max<std::uint64_t>(context.stats->maxIndirectRunCommands, run.commandCount);
        }

        for (std::uint32_t i = 0; i < run.commandCount; ++i)
        {
            const std::uint32_t commandIndex = run.commandIndex + i;
            MeshGPUPrimitive *primitive = m_CommandPrimitiveScratch[commandIndex];
            if (primitive == nullptr)
            {
                continue;
            }
            RecordCommand(callbacks, commands[commandIndex], *primitive, RenderCommandSubmitMode::Indirect);
        }
    }
}
