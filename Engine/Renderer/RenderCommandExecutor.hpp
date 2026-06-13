#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include <Engine/RHI/Descriptors/RHIIndirectDrawCommand.hpp>
#include <Engine/RHI/Resource/RHIBuffer.hpp>

namespace Physara::RHI
{
    class RHICommandList;
    class RHIDevice;
}

namespace Physara::Engine
{
    class AssetManager;
    class MeshGPUCache;
    struct FrameStatistics;
    struct MeshGPUPrimitive;
    struct RenderCommand;

    enum class RenderCommandSubmitMode : std::uint32_t
    {
        Direct = 0,
        Indirect = 1
    };

    struct RenderCommandExecutorContext
    {
        RHI::RHIDevice *device{nullptr};
        RHI::RHICommandList *commandList{nullptr};
        MeshGPUCache *meshCache{nullptr};
        AssetManager *assetManager{nullptr};
        FrameStatistics *stats{nullptr};
    };

    using RenderCommandMergeCallback = bool (*)(const RenderCommand &lhs, const RenderCommand &rhs);
    using RenderCommandBindCallback = bool (*)(void *userData, const RenderCommand &command);
    using RenderCommandRecordCallback = void (*)(
        void *userData,
        const RenderCommand &command,
        const MeshGPUPrimitive &primitive,
        RenderCommandSubmitMode mode);

    struct RenderCommandSubmitCallbacks
    {
        void *userData{nullptr};
        RenderCommandMergeCallback canMergeIndirectRun{nullptr};
        RenderCommandBindCallback bindCommand{nullptr};
        RenderCommandRecordCallback recordCommand{nullptr};
    };

    class RenderCommandExecutor final
    {
    public:
        void BeginFrame();
        void Submit(
            const RenderCommandExecutorContext &context,
            std::span<const RenderCommand> commands,
            const RenderCommandSubmitCallbacks &callbacks);
        void Reset();

    private:
        struct IndirectRun
        {
            std::uint32_t commandIndex{0};
            std::uint32_t commandCount{0};
            std::uint32_t indirectCommandOffset{0};
            RHI::RHIBuffer *indirectBuffer{nullptr};
        };

        void PreparePrimitives(const RenderCommandExecutorContext &context, std::span<const RenderCommand> commands);
        void BuildIndirectRuns(
            const RenderCommandExecutorContext &context,
            std::span<const RenderCommand> commands,
            RenderCommandMergeCallback canMerge);
        [[nodiscard]] RHI::RHIBuffer *UploadIndirectCommands(const RenderCommandExecutorContext &context);
        [[nodiscard]] static bool CanUseIndirectRun(const MeshGPUPrimitive &first, const MeshGPUPrimitive &next);
        [[nodiscard]] static bool CanMergeDefault(const RenderCommand &lhs, const RenderCommand &rhs);
        [[nodiscard]] static bool BindCommand(const RenderCommandSubmitCallbacks &callbacks, const RenderCommand &command);
        static void RecordCommand(
            const RenderCommandSubmitCallbacks &callbacks,
            const RenderCommand &command,
            const MeshGPUPrimitive &primitive,
            RenderCommandSubmitMode mode);
        void SubmitDirectCommand(
            const RenderCommandExecutorContext &context,
            std::span<const RenderCommand> commands,
            std::uint32_t commandIndex,
            const RenderCommandSubmitCallbacks &callbacks);
        void SubmitIndirectRun(
            const RenderCommandExecutorContext &context,
            std::span<const RenderCommand> commands,
            const IndirectRun &run,
            const RenderCommandSubmitCallbacks &callbacks);

    private:
        std::vector<std::unique_ptr<RHI::RHIBuffer>> m_IndirectCommandBuffers{};
        std::vector<RHI::RHIDrawIndexedIndirectCommand> m_IndirectCommandScratch{};
        std::vector<MeshGPUPrimitive *> m_CommandPrimitiveScratch{};
        std::vector<IndirectRun> m_IndirectRuns{};
        std::uint32_t m_IndirectBufferCursor{0};
    };
}