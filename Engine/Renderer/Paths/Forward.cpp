#include "Forward.hpp"

#include <algorithm>
#include <cassert>
#include <new>
#include <utility>

#include "../Materials/StaticGpu.hpp"
#include "../RenderGraph/GraphRanges.hpp"

/// @file
/// @brief 在同一图中登记共享上传及各 View 的几何, 阴影深度, 天空和显示转换.
/// 受光分支按准入组紧排深度层, 诊断只取选中组. 深度逐层写入或整体只读导入, 颜色 Pass 绑定连续数组视图.
/// 显式表面请求先写核心 MRT, 再按辅助覆盖更新 G1, G3 和 D32. Deferred 从这些最终版本着色,
/// 与 Forward+ 共用本 View 的 Froxel 上传. 天空和显示沿用各自的工作色版本.

namespace Physara::Renderer
{
    using Detail::BufferBoundary;
    using Detail::DepthFormat;
    using Detail::TextureBoundary;
    using Detail::WorkingFormat;

    ForwardFrame::ForwardFrame() noexcept = default;
    ForwardFrame::~ForwardFrame() = default;

    ForwardWorkCounts ForwardFrame::WorkCounts(std::size_t index) const noexcept
    {
        if (!m_Data)
        {
            return {};
        }
        const auto &view = m_Data->views[index];
        const auto &lights = view.activeLights;
        return {.draws = view.draws.size(),
                .lights = lights ? std::size_t(lights->directionalCount) + lights->localCount : 0};
    }

    ForwardWorkCounts ForwardFrame::ExecutedWorkCounts(std::size_t index, const ForwardOutput &output,
                                                       const CompiledGraphPlan &plan) const noexcept
    {
        auto counts = WorkCounts(index);
        const auto &view = m_Data->views[index];
        if (view.needs.surfaceOnly)
        {
            return counts;
        }
        ForwardPassWork work{};
        for (const auto &batch : view.batches)
        {
            const auto &draw = view.draws[batch.firstDraw];
            if (view.needs.deferredLighting && view.materialRows[draw.material]->route ==
                Detail::OpaqueSurfaceRoute::CoreDeferred)
            {
                continue;
            }
            ++work.batches;
            work.records += batch.commandCount;
        }
        if (IsLivePass(plan, output.depthPass))
        {
            counts.depth = work;
        }
        if (IsLivePass(plan, output.colorPass))
        {
            counts.color = work;
        }
        return counts;
    }

    OpaqueWorkStatistics ForwardFrame::ExecutedOpaqueWork(std::size_t index, const ForwardOutput &output,
        const RenderGraph &graph, const CompiledGraphPlan &plan, TextureId preview) const noexcept
    {
        OpaqueWorkStatistics work;
        const auto &view = m_Data->views[index];
        // 同一材质行可被多个实例引用. 路由占比的分母按可绘实例表面计, 每个 DrawRecord 贡献一项.
        work.surfaceCount = view.draws.size();
        for (const auto &draw : view.draws)
        {
            const auto route = view.materialRows[draw.material]->route;
            work.extensionCount += route == Detail::OpaqueSurfaceRoute::OpaqueForwardExtension;
            work.unlitCount += route == Detail::OpaqueSurfaceRoute::Unlit;
        }

        // 这里的六种表面及颜色目标都是一层、一 mip 的单样本栅格, 图版本共享各自的逻辑存储.
        const auto bytes = [&](TextureId resource) -> std::uint64_t
        {
            if (!resource.IsValid() || !std::ranges::any_of(plan.textures, [&](const auto &texture)
                { return texture.resource.Identity() == resource.Identity(); }))
            {
                return 0;
            }
            const auto &desc = graph.Textures()[resource.Identity().index].desc;
            const auto &format = RHI::GetFormatInfo(desc.format);
            assert(desc.mipLevels == 1 && desc.arraySize == 1 && desc.sampleCount == 1 && format.blockSize == 1);
            return std::uint64_t(desc.width) * desc.height * format.bytesPerBlock;
        };
        constexpr std::size_t gbufferCount = CoreGBufferOutput::Formats.size();
        std::array<TextureId, gbufferCount + 2> ids{output.depth, output.workingColor};
        if (output.coreGBuffer)
        {
            std::ranges::copy(output.coreGBuffer->colors, ids.begin() + 2);
        }
        std::array<std::uint64_t, ids.size()> attachmentBytes{};
        std::ranges::transform(ids, attachmentBytes.begin(), bytes);
        work.depthBytes = attachmentBytes[0];
        work.workingColorBytes = attachmentBytes[1];
        std::copy_n(attachmentBytes.begin() + 2, gbufferCount, work.gbufferBytes.begin());
        work.previewBytes = bytes(preview);

        for (const auto &pass : plan.passes)
        {
            if (!output.OwnsPass(pass.id) || pass.id.index > output.lastWorkingPass.index)
            {
                continue;
            }
            // 只计此生产范围实际使用的主附件. 同一 Pass 的显式读、只读深度和 Load 在同一范围合并.
            std::array<bool, ids.size()> reads{}, writes{};
            for (const auto &entry : pass.uses)
            {
                const auto *texture = std::get_if<TextureUse>(&entry.declaration);
                if (!texture)
                {
                    continue;
                }
                const auto found = std::ranges::find(ids, texture->access.resource.Identity(), &TextureId::Identity);
                if (found == ids.end())
                {
                    continue;
                }
                const auto slot = std::size_t(found - ids.begin());
                reads[slot] |= !texture->write || texture->write->ops.load == LoadOp::Load;
                writes[slot] |= texture->write.has_value();
            }
            for (std::size_t i = 0; i < ids.size(); ++i)
            {
                work.readBytes += reads[i] ? attachmentBytes[i] : 0;
                work.writeBytes += writes[i] ? attachmentBytes[i] : 0;
            }
        }
        return work;
    }

    ForwardEncodingCounts ForwardFrame::EncodingCounts() const noexcept
    {
        return m_Data ? m_Data->encodings : ForwardEncodingCounts{};
    }

    std::pair<std::size_t, std::size_t> ForwardFrame::UploadDemand() const noexcept
    {
        return m_Data ? std::pair{m_Data->uploads.writes, m_Data->uploads.bytes} : std::pair<std::size_t, std::size_t>{};
    }

    bool ForwardFrame::UsesFroxels(std::size_t index) const noexcept
    {
        return Detail::UsesFroxels(m_Data->views[index].environment.program);
    }

    bool ForwardFrame::AddsOpaqueResources(std::size_t index, std::optional<OpaquePath> effective) const noexcept
    {
        const auto &view = m_Data->views[index];
        const bool froxels = UsesFroxels(index);
        if (effective != view.source.opaquePath)
        {
            return froxels || view.deferredColor.has_value();
        }
        return view.alternate && !view.alternate->failure &&
            ((view.usesLighting && UsesFroxelCandidates(view.alternate->path) && !froxels) ||
             (view.alternate->needs.deferredLighting && view.deferredViewValues));
    }

    /// @brief 解除本轮绘制引用. 槽内保留 View 行、实例块和参数缓冲供下次准备比较.
    static void ResetDrawWork(Detail::StaticDrawWork &work) noexcept
    {
        work.batches.clear();
        work.draws.clear();
        work.materialRows.clear();
        work.degradation.reset();
    }

    /// @brief 清理本轮主表面、阴影和显示候选. 帧槽已取得安全复用后才调用.
    static void ResetView(Detail::ForwardFrameData &view) noexcept
    {
        ResetDrawWork(view);
        if (view.shadow)
        {
            for (auto &level : view.shadow->levels)
            {
                ResetDrawWork(level);
            }
            view.shadow->depth = {};
        }
        view.activeLights.reset();
        view.alternate.reset();
        view.opaqueResourcesReady = false;
        view.lightLayout = nullptr;
        view.froxelViewLayout = nullptr;
        view.deferredColor.reset();
        view.deferredViewValues.reset();
        if (std::exchange(view.deferredViewWriteRecorded, false))
        {
            // 已尝试写入却没有完整图回执, 原内容可能已被覆盖. 下次使用时重新上传.
            view.deferredView->accepted = false;
        }
        view.deferredPipeline = nullptr;
        view.deferredSurfaceLayout = nullptr;
        view.froxelUpload.reset();
        view.opaqueUploadReservation = {};
        view.effectiveOpaqueResidency.reset();
        view.environment = {};
        view.display = {};
        view.displayReserved = false;
        view.preparationStamp = 0;
    }

    void ForwardFrame::Reset() noexcept
    {
        if (m_Data)
        {
            m_Data->uploads.Clear();
            // 主表面与各层 caster 都向槽累计编码. 拒绝一路后, 另一条接受的路仍可报告已发生的工作.
            m_Data->encodings = {};
            for (auto &view : m_Data->views)
            {
                ResetView(view);
            }
            m_Data->begun = false;
            m_Data->hasPreparedView = false;
            m_Data->planned.reset();
            m_Data->shaders.reset();
        }
    }

    std::uint64_t ForwardFrame::ShaderGeneration() const noexcept
    {
        return m_Data && m_Data->shaders ? m_Data->shaders->generation : 0;
    }

    const ShadowDepthSet *ForwardFrame::PreparedShadowDepth(std::size_t index) const noexcept
    {
        assert(m_Data);
        const auto &shadow = m_Data->views[index].shadow;
        return shadow ? &shadow->depth : nullptr;
    }

    OpaqueProduction ForwardFrame::Production(std::size_t index) const noexcept
    {
        const auto &view = m_Data->views[index];
        const auto &programs = m_Data->shaders->programs;
        const bool composed = view.needs.deferredLighting;
        const bool surfaceOnly = view.needs.surfaceOnly;
        const bool auxiliary = (composed || surfaceOnly) && view.HasAuxiliarySurface();
        OpaqueProduction production{.color = surfaceOnly ? RHI::Format::UNKNOWN : WorkingFormat, .depth = DepthFormat};
        const auto store = [&](std::array<char, 64> &digest, Shader::ProductShader role)
        {
            const auto content = programs.ProgramContent(role);
            assert(content.size() == digest.size());
            std::ranges::copy(content, digest.begin());
        };
        if (view.deferredColor)
        {
            store(production.coreSurface, Shader::ProductShader::StaticCoreGBuffer);
            store(production.deferredColor, view.deferredColor->role);
        }
        else if (surfaceOnly && std::ranges::any_of(view.materialRows, [](const auto &row)
        {
            return row->route == Detail::OpaqueSurfaceRoute::CoreDeferred;
        }))
        {
            store(production.coreSurface, Shader::ProductShader::StaticCoreGBuffer);
        }
        if (auxiliary)
        {
            store(production.surfaceAux, Shader::ProductShader::StaticSurfaceAux);
        }
        // 同一 View 可以同时使用核心与扩展颜色角色. 历史键记录实际绘制的每个角色.
        if (!surfaceOnly)
        {
            for (const auto &batch : view.batches)
            {
                const auto &draw = view.draws[batch.firstDraw];
                const auto &material = *view.materialRows[draw.material];
                if (composed && material.route == Detail::OpaqueSurfaceRoute::CoreDeferred)
                {
                    continue;
                }
                auto &digest = draw.iorSpecularColor ? production.forwardExtensionColor : production.forwardColor;
                if (digest.front() == '\0')
                {
                    const auto key = Detail::GeometryRequest(0, false, false, Detail::SurfacePhase::Combined,
                                                              view.environment.program, draw.iorSpecularColor);
                    store(digest, key.role);
                }
            }
            if (view.needs.NeedsEarlyDepth() && !view.batches.empty())
            {
                store(production.earlyDepth, Shader::ProductShader::StaticDepth);
            }
        }
        if (view.environment.sky)
        {
            store(production.sky, Shader::ProductShader::ForwardSky);
        }
        if (view.shadow && view.shadow->depth.variants.any())
        {
            store(production.shadowDepth, Shader::ProductShader::StaticShadowDepth);
        }
        return production;
    }

    void ForwardFrame::ConfirmShadowDepth(View &owner, std::size_t index, const GraphRetainedResource &retained,
                                               std::uint64_t ticket) const noexcept
    {
        assert(m_Data && m_Data->views[index].source.view == owner.Id());
        const auto &frame = m_Data->views[index].shadow;
        if (frame && retained)
        {
            auto depth = frame->depth;
            depth.retained = retained;
            owner.AcceptShadowDepth(std::move(depth), ticket);
        }
    }

    const std::optional<Core::BoundedText> &ForwardFrame::Degradation(std::size_t index) const noexcept
    {
        assert(m_Data);
        return m_Data->views[index].degradation;
    }

    const std::optional<RenderedEnvironment> &ForwardFrame::Environment(std::size_t index) const noexcept
    {
        assert(m_Data);
        return m_Data->views[index].environment.applied;
    }

    std::size_t ForwardFrame::GraphStorageBytes() const noexcept
    {
        assert(m_Data);
        // 图 arena 只保存上传回调和几个 Pass 描述. Draw 和字节另由帧槽持有.
        std::size_t bytes = 4096 * m_Data->views.size() + m_Data->uploads.writes * 256;
        for (const auto &view : m_Data->views)
        {
            if (view.needs.NeedsCoreSurface())
            {
                bytes += view.HasAuxiliarySurface() ? 2048 : 1024; // 核心及可选辅助写入的 SceneRaster.
            }
            if (view.needs.deferredLighting)
            {
                bytes += 1024; // 核心颜色或背景初始化的声明.
            }
            if (view.shadow && !view.shadow->depth.retained)
            {
                bytes += view.shadow->depth.count * 1024;
            }
        }
        return bytes;
    }

    void ForwardFrame::ReleasePayload() noexcept
    {
        if (m_Data)
        {
            m_Data->uploads.Clear();
            for (auto &view : m_Data->views)
            {
                view.display.ReleasePayload();
            }
        }
    }

    ForwardRenderer::ForwardRenderer(RHI::Device &device, ForwardPackagePaths packages, std::uint32_t maxTextureSize,
                                     std::uint32_t maxAttachmentSize, std::uint32_t uniformAlignment) noexcept
        : m_Device(device), m_Packages(std::move(packages)), m_MaxTextureSize(maxTextureSize),
          m_MaxAttachmentSize(maxAttachmentSize), m_UniformAlignment(uniformAlignment)
    {
    }

    ForwardRenderer::~ForwardRenderer() = default;

    void ForwardRenderer::SetShaderPreparationLimits(Core::BoundedText deviceIdentity,
                                                       ShaderPreparationLimits limits) noexcept
    {
        assert(!m_Resources && limits.frames && limits.perStep);
        m_ShaderDevice = deviceIdentity;
        m_ShaderLimits = limits;
    }

    bool ForwardRenderer::InitializeResources() noexcept
    {
        assert(!m_Resources && m_InitialShaders);
        auto &failure = m_InitialShaders->deviceFailure;
        try
        {
            m_Resources = std::make_unique<Detail::ForwardDevice>(m_Device, m_Packages, m_MaxTextureSize,
                m_UniformAlignment, m_ShaderDevice, m_ShaderLimits);
            failure.reset();
            return true;
        }
        catch (const ForwardFailure &error)
        {
            failure = error;
        }
        catch (const std::bad_alloc &)
        {
            failure = ForwardFailure{ViewSettingReason::ScenePreparationFailed,
                                     Core::BoundedText("Shader preparation allocation failed")};
        }
        catch (const std::exception &error)
        {
            failure = ForwardFailure{ViewSettingReason::ScenePreparationFailed, Core::BoundedText(error.what())};
        }
        return false;
    }

    void ForwardRenderer::AdvanceShaderPreparation(std::uint64_t request,
                                                    std::optional<Shader::ReloadResult> result) noexcept
    {
        if (m_InitialShaders)
        {
            auto &initial = *m_InitialShaders;
            if (request < initial.request)
            {
                return;
            }
            if (request != initial.request)
            {
                initial.request = request;
                initial.result.reset();
            }
            const bool delivered = result && result->request == request;
            if (delivered)
            {
                initial.result = std::move(result);
            }
            // 首份回执可在无 View 时装配设备. 若分配失败, 空闲轮询继续持有回执, 下次有效规划可重新装配.
            if (!m_Resources && (!delivered || !InitializeResources()))
            {
                return;
            }
            // 以本轮确认过的修订交给设备准备器, 从此由它管理该请求和后续候选.
            result = std::move(initial.result);
            m_InitialShaders.reset();
        }
        m_Resources->AdvanceShaders(request, std::move(result));
    }

    void ForwardRenderer::ContinueShaderPreparation() noexcept
    {
        assert(m_Resources);
        m_Resources->AdvanceShaders(m_Resources->shaderStatistics.request, std::nullopt, true);
    }

    void ForwardRenderer::BeginShaderRequests() noexcept
    {
        if (m_Resources)
        {
            m_Resources->shaderDemand = {};
            m_Resources->displayFailure.reset();
        }
    }

    bool ForwardRenderer::ShaderCandidateReady() const noexcept
    {
        return m_Resources && m_Resources->ShaderCandidateReady();
    }

    void ForwardRenderer::PublishShaderPreparation(std::uint64_t request,
                                                    std::optional<Shader::ReloadResult> result) noexcept
    {
        if (m_Resources)
        {
            m_Resources->PublishShaders(request, std::move(result));
        }
    }

    ShaderPreparationStatistics ForwardRenderer::ShaderStatistics() const noexcept
    {
        if (!m_InitialShaders)
        {
            return m_Resources->shaderStatistics;
        }
        ShaderPreparationStatistics statistics;
        statistics.request = m_InitialShaders->request;
        statistics.budget = m_ShaderLimits.perStep;
        if (const auto &failure = m_InitialShaders->deviceFailure)
        {
            statistics.state = ShaderPreparationState::Failed;
            statistics.reason = failure->description;
        }
        return statistics;
    }

    void ForwardRenderer::RecordShaderDiagnostic(const Core::BoundedText &reason) noexcept
    {
        if (m_Resources)
        {
            m_Resources->RecordShaderDiagnostic(reason);
        }
        else if (m_InitialShaders && m_InitialShaders->deviceFailure)
        {
            auto &description = m_InitialShaders->deviceFailure->description;
            if (description.View() != reason.View())
            {
                description.Append("\n");
                description.Append(reason.View());
            }
        }
    }

    void ForwardRenderer::BeginEnvironmentRequests(const Asset::EnvironmentVersion &source) noexcept
    {
        if (m_Resources)
        {
            m_Resources->environmentCache.BeginFrame(source);
        }
    }

    void ForwardRenderer::EndEnvironmentRequests() noexcept
    {
        if (m_Resources)
        {
            m_Resources->environmentCache.EndFrame();
        }
    }

    std::optional<ForwardFailure> ForwardRenderer::ObserveModelTable(const SceneFrame &scene) noexcept
    try
    {
        if (m_Resources)
        {
            m_Resources->ObserveGeometry(scene);
        }
        return std::nullopt;
    }
    catch (const std::bad_alloc &)
    {
        return ForwardFailure{ViewSettingReason::ScenePreparationFailed,
                              Core::BoundedText("Forward model table observation allocation failed")};
    }

    ForwardResidencyCounts ForwardRenderer::ResidencyCounts() const noexcept
    {
        if (!m_Resources)
        {
            return {};
        }
        const auto &cache = *m_Resources;
        ForwardResidencyCounts result{cache.geometry.size(), cache.textures.size() + std::size_t(bool(cache.white)),
            cache.materials.size(), cache.environmentTextures.size() + std::size_t(bool(cache.dfgTexture))};
        result.textureBytes = cache.capacity->textureBytes;
        result.environmentBytes = cache.capacity->environmentBytes;
        result.samplerEntries = cache.samplers.size() + std::size_t(bool(cache.environmentSampler));
        result.materialRequests = cache.materialRequests;
        result.materialHits = cache.materialHits;
        result.materialGeneration = cache.materialGeneration;
        for (const auto &page : cache.pages)
        {
            const auto bytes = static_cast<std::size_t>(page->buffer->GetDesc().byteSize);
            if (page->usage == GraphUsage::ShaderRead)
            {
                auto &tables = page->layout == std::uint8_t(Detail::TableLayout::Materials) ?
                    result.materialPageBytes : result.objectPageBytes;
                tables += bytes;
                continue;
            }
            ++result.geometryPages;
            result.geometryPageBytes += bytes;
            result.geometryUsedBytes += page->usedBytes;
            result.geometryRetiringBytes += page->retiringBytes;
            result.largestGeometryGap = std::max(result.largestGeometryGap, page->largestGap);
        }
        return result;
    }

    std::optional<ForwardFailure> ForwardRenderer::CheckAttachments(ViewExtent render, ViewExtent output,
        DisplaySettings display, bool resample, ViewResourceNeeds needs, std::uint32_t samples,
        bool surfacePreview) const noexcept
    {
        if (samples != 1)
        {
            const auto text = needs.deferredLighting ? "Deferred MSAA is unsupported" :
                "Opaque rendering requires single-sample attachments";
            return ForwardFailure{ViewSettingReason::InvalidViewRequest, Core::BoundedText(text)};
        }
        if (render.width == 0 || render.height == 0 ||
            (!needs.surfaceOnly && (output.width == 0 || output.height == 0)))
        {
            return ForwardFailure{ViewSettingReason::InvalidExtent, Core::BoundedText("Forward extent is empty")};
        }
        const auto limit = std::min(m_MaxAttachmentSize, m_MaxTextureSize);
        if (std::max(render.width, render.height) > limit ||
            (!needs.surfaceOnly && std::max(output.width, output.height) > limit))
        {
            return ForwardFailure{ViewSettingReason::SceneAttachmentLimit,
                                  Core::BoundedText("Forward extent exceeds the attachment limit")};
        }
        if (needs.NeedsCoreSurface())
        {
            using RHI::FormatSupport;
            const auto check = [&](RHI::Format format, FormatSupport required) -> std::optional<ForwardFailure>
            {
                if ((m_Device.QueryFormatSupport(format) & required) == required)
                {
                    return std::nullopt;
                }
                Core::BoundedText detail("Opaque surface attachment unsupported: ");
                detail.Append(RHI::GetFormatInfo(format).name);
                return ForwardFailure{ViewSettingReason::SceneAttachmentLimit, detail};
            };
            for (const auto format : CoreGBufferOutput::Formats)
            {
                const auto read = format == RHI::Format::R16_UINT ? FormatSupport::ShaderLoad :
                    FormatSupport::ShaderLoad | FormatSupport::ShaderSample;
                if (auto failure = check(format, FormatSupport::Texture | FormatSupport::RenderTarget | read))
                {
                    return failure;
                }
            }
            const auto depthRead = needs.deferredLighting ? FormatSupport::ShaderSample : FormatSupport::None;
            if (auto failure = check(DepthFormat, FormatSupport::Texture | FormatSupport::DepthStencil | depthRead))
            {
                return failure;
            }
        }
        // 主 D32 为 4 字节. 最终表面另有 18 字节颜色, 或在普通颜色附带表面时合计 22 字节.
        std::size_t renderBytesPerPixel = 4 + (needs.NeedsCoreSurface() ? 18 : 0);
        if (!needs.surfaceOnly)
        {
            const bool fxaa = display.antiAliasing == AntiAliasing::Fxaa;
            renderBytesPerPixel += 8 * (1 + std::size_t(fxaa) + std::size_t(resample));
            renderBytesPerPixel += surfacePreview ? 4 : 0;
            if (!needs.deferredLighting && needs.coreGBuffer)
            {
                renderBytesPerPixel += 4;
            }
        }
        if (render.height > ForwardAttachmentBudget / renderBytesPerPixel / render.width)
        {
            const auto text = needs.NeedsCoreSurface() ? "GBuffer attachments exceed the view byte budget" :
                "Forward attachments exceed the view byte budget";
            return ForwardFailure{ViewSettingReason::SceneAttachmentLimit, Core::BoundedText(text)};
        }
        if (needs.surfaceOnly)
        {
            return std::nullopt;
        }
        const auto renderBytes = std::size_t(render.width) * render.height * renderBytesPerPixel;
        // 用余量核算显示画布的 SRGBA8. 两套栅格均先按行宽折算可容纳的高度, 避免直接相乘越界.
        if (output.height > (ForwardAttachmentBudget - renderBytes) / 4 / output.width)
        {
            return ForwardFailure{ViewSettingReason::SceneAttachmentLimit,
                                  Core::BoundedText("Display output exceeds the view byte budget")};
        }
        return std::nullopt;
    }

    /// @brief 同一槽内有任一路为待写的共享 LUT 保留额度.
    static bool HasLutReservation(const Detail::ForwardFrameStorage &frame) noexcept
    {
        return std::ranges::any_of(frame.views, [](const auto &view)
        {
            return view.displayReserved && view.display.LutUploadBytes() != 0;
        });
    }

    /// @brief 拒绝或取消一路后, 以仍在准备中的 View 或已就绪的 View 重算上传额度.
    static void DiscardViewUploads(Detail::ForwardFrameStorage &frame, std::size_t index) noexcept
    {
        auto &view = frame.views[index];
        const auto stamp = std::exchange(view.preparationStamp, 0);
        if (!stamp)
        {
            return;
        }
        std::array<std::uint64_t, ForwardViewCount> retained;
        std::ranges::transform(frame.views, retained.begin(), &Detail::ForwardFrameData::preparationStamp);
        frame.uploads.DiscardView(index, stamp, retained);
        view.froxelUpload.reset();
        view.opaqueUploadReservation = {};
        view.displayReserved = false;
        view.display.ReleasePayload();
        // DiscardView 已按实际载荷重算额度. 仍在等待收尾的不透明写入预留各补计一次,
        // 显示常量按路补计, 共享 LUT 只补一份.
        bool lutReserved = false;
        for (const auto &candidate : frame.views)
        {
            const auto [bytes, writes] = candidate.opaqueUploadReservation;
            frame.uploads.bytes += bytes;
            frame.uploads.writes += writes;
            if (candidate.displayReserved)
            {
                const auto lutBytes = candidate.display.LutUploadBytes();
                const bool sharedLut = lutReserved && lutBytes != 0;
                frame.uploads.bytes += candidate.display.UploadBytes() - (sharedLut ? lutBytes : 0);
                frame.uploads.writes += candidate.display.UploadCount() - std::size_t(sharedLut);
                lutReserved |= lutBytes != 0;
            }
        }
        assert(frame.uploads.bytes <= Detail::ForwardUploadBudget && frame.uploads.writes <= Detail::ForwardUploadItems);
    }

    /// @brief 结束失败候选的独占准备, 在批次还有其它候选时保留可共享的生产者.
    static void RejectView(Detail::ForwardFrameStorage &frame, std::size_t index) noexcept
    {
        DiscardViewUploads(frame, index);
        ResetView(frame.views[index]);
        frame.planned.reset(index);
        // 另一路可能已经登记了本批写入, 尚在等待 FinishView. 保留它所依赖的上传载荷和准备代.
        if (!frame.hasPreparedView && frame.planned.none())
        {
            // 这批写入已失去消费者. 下路重新 Begin, 使设备缓存能识别只在旧批次中拥有生产者的代.
            frame.uploads.Clear();
            frame.begun = false;
            frame.shaders.reset();
        }
    }

    // 选代前取得显示输入并预留写入额度. 两路共享的 LUT 只计一次, 实际上传由使用它的图声明.
    static void PlanDisplay(Detail::ForwardDevice &device, Detail::ForwardFrameStorage &frame,
                            Detail::ForwardFrameData &view, const ViewLayouts &layouts, DisplaySettings settings)
    {
        auto &display = view.display;
        const auto requests = PostProcess::DisplayDevice::SelectStages(layouts, settings);
        for (const auto key : requests.Stages())
        {
            device.shaderDemand.Add(key);
        }
        try
        {
            if (const auto failure = device.display.PrepareInputs(display, layouts, settings))
            {
                Detail::Reject(ViewSettingReason::DisplayUnavailable, failure->View());
            }
        }
        catch (const ForwardFailure &failure)
        {
            // 一路所需的分级或重采样输入未就绪时, 其它路可消费旧代, 本帧的候选切换继续等待.
            if (!device.displayFailure)
            {
                const auto key = settings.profile == DisplayProfile::AcesSdr
                    ? requests.Stages().front() : requests.Stages().back();
                device.displayFailure = Detail::ShaderPreparationFailure{key.role, failure.description, key.variant};
            }
            throw;
        }
        // 显示输入已就绪, 此后的批次超额由 View 准入报告. 重试可复用已登记的显示键和其它路共享的 LUT.
        const auto lutBytes = display.LutUploadBytes();
        const bool sharedLut = lutBytes != 0 && HasLutReservation(frame);
        frame.uploads.ReserveWrites(display.UploadBytes() - (sharedLut ? lutBytes : 0),
                                    display.UploadCount() - std::size_t(sharedLut));
        view.displayReserved = true;
    }

    std::optional<ForwardFailure> ForwardRenderer::Prepare(ForwardFrame &frame, const PreparedView &view,
        PreviewMode mode, ViewResourceNeeds needs, std::size_t index, const GraphBufferSupport &buffers)
    {
        if (auto failure = PlanView(frame, view, mode, needs, index))
        {
            return failure;
        }
        return FinishView(frame, view, index, buffers);
    }

    std::optional<ForwardFailure> ForwardRenderer::PlanView(ForwardFrame &frame, const PreparedView &view,
        PreviewMode mode, ViewResourceNeeds needs, std::size_t index, std::optional<OpaquePath> alternate,
        bool protectEffectiveResources)
    try
    {
        assert(view.camera && view.reason == ViewSettingReason::None && index < ForwardViewCount);
        if (needs.surfaceOnly && (needs.deferredLighting || alternate))
        {
            return ForwardFailure{ViewSettingReason::InvalidViewRequest,
                Core::BoundedText("Surface-only output cannot be combined with a color path request")};
        }
        if (needs.deferredLighting && view.previewShadow)
        {
            return ForwardFailure{ViewSettingReason::InvalidViewRequest,
                Core::BoundedText("Deferred opaque composition cannot be combined with a shadow diagnostic")};
        }
        if (needs.deferredLighting && (!IsLitPreview(mode) || alternate))
        {
            return ForwardFailure{ViewSettingReason::InvalidViewRequest,
                Core::BoundedText("DeferredLighting needs a lit view with no alternate opaque path request")};
        }
        const auto display = ResolveDisplaySettings(view.previewShadow ? PreviewMode::BaseColor : mode,
                                                    view.requestedAa);
        const bool lit = IsLitPreview(mode) && !view.previewShadow;
        auto path = needs.deferredLighting ? OpaquePath::Deferred :
            lit && !needs.surfaceOnly ? view.opaquePath : OpaquePath::Forward;
        needs.deferredLighting = path == OpaquePath::Deferred;
        const auto check = [&](ViewResourceNeeds options)
        {
            return CheckAttachments(view.layouts.render.outputExtent, view.layouts.display.outputExtent,
                                    display, view.layouts.render != view.layouts.display, options);
        };
        // 两条路径使用同一显示配置. Deferred 改变表面附件和深度用途, 因此单独核算其布局.
        const auto attachmentFailure = check(needs);
        std::optional<Detail::OpaqueCandidate> candidate;
        if (alternate)
        {
            assert(lit && *alternate != path);
            auto candidateNeeds = needs;
            candidateNeeds.deferredLighting = *alternate == OpaquePath::Deferred;
            const auto candidateFailure = candidateNeeds.deferredLighting == needs.deferredLighting
                ? attachmentFailure : check(candidateNeeds);
            candidate.emplace(Detail::OpaqueCandidate{*alternate, candidateNeeds, candidateFailure});
        }
        if (attachmentFailure)
        {
            if (!candidate || candidate->failure)
            {
                throw candidate ? *candidate->failure : *attachmentFailure;
            }
            // 旧策略的附件限制不能拦截当前仍可执行的请求.
            path = candidate->path;
            needs = candidate->needs;
            candidate.reset();
            protectEffectiveResources = false;
        }
        if (!m_Resources && !InitializeResources())
        {
            throw *m_InitialShaders->deviceFailure;
        }
        if (!frame.m_Data)
        {
            frame.m_Data = std::make_unique<Detail::ForwardFrameStorage>();
        }
        assert(!frame.m_Data->uploads.sealed);
        assert(!frame.m_Data->planned[index]);
        if (!frame.m_Data->begun)
        {
            m_Resources->ObserveGeometry(view.scene);
            m_Resources->Begin();
            frame.m_Data->begun = true;
        }
        auto &data = frame.m_Data->views[index];
        assert(data.draws.empty() && data.materialRows.empty());
        // 普通灯缓冲和 View 行都随各自帧槽跨帧保留. 更换 Renderer 时以预算 owner 识别旧缓存.
        if (data.view && data.view->capacity != m_Resources->capacity)
        {
            // 帧槽更换准备 owner 后, 原 UBO 集合的布局不再是当前 Pipeline 的布局.
            data.view.reset();
            data.viewValues.reset();
        }
        if (data.lightTable && data.lightTable->capacity != m_Resources->capacity)
        {
            data.lightTable.reset();
        }
        data.source = {.view = view.view, .scene = view.scene.scene, .contentRevision = view.scene.contentRevision,
            .mode = mode, .camera = *view.camera, .layout = view.layouts.render, .opaquePath = path};
        data.needs = needs;
        data.alternate = std::move(candidate);
        m_Resources->BeginView(index);
        data.preparationStamp = m_Resources->viewBudgets[index].stamp;
        Detail::PlanDraws(*m_Resources, data, view, mode, needs.surfaceOnly ? nullptr : view.previewShadow);
        if (!needs.surfaceOnly)
        {
            PlanDisplay(*m_Resources, *frame.m_Data, data, view.layouts, display);
        }
        if (protectEffectiveResources)
        {
            // 写入计入共享批次. 设备容量随实际分配记账, 待选策略须为尚未分配的有效计划留出余量.
            const auto demand = Detail::OpaqueRequirements(*m_Resources, data, view,
                data.deferredColor.has_value(), data.environment.program);
            data.effectiveOpaqueResidency = demand.residency;
            try
            {
                frame.m_Data->uploads.ReserveWrites(demand.uploads.first, demand.uploads.second);
                data.opaqueUploadReservation = demand.uploads;
            }
            catch (const ForwardFailure &)
            {
                if (!data.alternate)
                {
                    throw;
                }
                // 待选路径可独立取得更少的私有资源. 若仍使用旧路, 收尾按实际需求重新准入.
            }
        }
        frame.m_Data->planned.set(index);
        return std::nullopt;
    }
    catch (const ForwardFailure &failure)
    {
        if (frame.m_Data)
        {
            RejectView(*frame.m_Data, index);
        }
        return failure;
    }
    catch (const std::bad_alloc &)
    {
        if (frame.m_Data)
        {
            RejectView(*frame.m_Data, index);
        }
        return ForwardFailure{ViewSettingReason::ScenePreparationFailed,
                              Core::BoundedText("Forward plan allocation failed")};
    }

    /// @brief 汇总其它仍在准备的有效计划需要的新存储. 已物化或撤回的计划由现有容量计数反映.
    static Detail::ResidencyDemand PendingEffectiveResidency(const Detail::ForwardFrameStorage &frame,
                                                              std::size_t index) noexcept
    {
        Detail::ResidencyDemand demand{};
        for (std::size_t peer = 0; peer < frame.views.size(); ++peer)
        {
            const auto &view = frame.views[peer];
            if (peer != index && frame.planned[peer] && !view.opaqueResourcesReady && view.effectiveOpaqueResidency)
            {
                demand.bytes += view.effectiveOpaqueResidency->bytes;
                demand.entries += view.effectiveOpaqueResidency->entries;
            }
        }
        return demand;
    }

    std::optional<ForwardFailure> ForwardRenderer::SelectOpaquePath(ForwardFrame &frame, PreparedView &view,
        View &owner, std::size_t index, const GraphBufferSupport &buffers)
    {
        auto &storage = *frame.m_Data;
        auto &data = storage.views[index];
        if (data.needs.surfaceOnly)
        {
            return std::nullopt;
        }
        const auto report = [&](ViewSettingReason reason, const Core::BoundedText *detail = nullptr)
        {
            owner.ReportOpaquePreparation(view.scene.scene, view.documentRevision, reason, detail);
        };
        const bool lit = IsLitPreview(view.preview) && !view.previewShadow;
        const auto effective = owner.OpaqueSettings(view.scene.scene).effective;
        view.opaquePath = data.source.opaquePath;
        if (data.alternate)
        {
            const auto &candidate = *data.alternate;
            if (candidate.failure)
            {
                report(candidate.failure->reason, &candidate.failure->description);
                return std::nullopt;
            }
            if (!owner.CanSwitchOpaquePath(view, m_Resources->activeShaders->generation))
            {
                return std::nullopt;
            }
            const auto program = Detail::OpaqueColorProgram(data, candidate.path);
            if (const auto issue = Detail::CheckOpaqueShaders(*m_Resources, data, candidate.needs, program))
            {
                report(issue->reason, &issue->description);
                return std::nullopt;
            }
            // 局部失败仍可交付已准备的有效策略. 路径和工作集在私有资源就绪后一起切换.
            try
            {
                m_Resources->ResumeView(index);
                Detail::PrepareOpaqueResources(*m_Resources, data, view, candidate.needs, program, buffers,
                                               PendingEffectiveResidency(storage, index));
                view.opaquePath = data.source.opaquePath = candidate.path;
            }
            catch (const ForwardFailure &failure)
            {
                report(failure.reason, &failure.description);
            }
            catch (const std::bad_alloc &)
            {
                const Core::BoundedText detail("Opaque candidate allocation failed");
                report(ViewSettingReason::ScenePreparationFailed, &detail);
            }
        }
        else if (lit && effective != data.source.opaquePath)
        {
            // 没有有效计划时也可保留失败输入. 调用方撤回本路准备, 其它路继续构图.
            if (!owner.CanSwitchOpaquePath(view, m_Resources->activeShaders->generation))
            {
                const auto status = owner.OpaqueSettings(view.scene.scene);
                return ForwardFailure{status.reason, status.detail ? *status.detail :
                    Core::BoundedText("Opaque path submission was rejected")};
            }
            return Detail::CheckOpaqueShaders(*m_Resources, data, data.needs, data.environment.program);
        }
        return std::nullopt;
    }

    void ForwardRenderer::CancelPlannedView(ForwardFrame &frame, std::size_t index) noexcept
    {
        assert(frame.m_Data && frame.m_Data->planned[index]);
        RejectView(*frame.m_Data, index);
    }

    std::optional<ForwardFailure> ForwardRenderer::FinishView(ForwardFrame &frame, const PreparedView &view,
        std::size_t index, const GraphBufferSupport &buffers)
    try
    {
        assert(m_Resources && frame.m_Data && frame.m_Data->planned[index]);
        auto &data = frame.m_Data->views[index];
        assert(data.source.view == view.view && data.source.scene == view.scene.scene);
        if (!frame.m_Data->shaders)
        {
            // 同一帧的两路 View 固定使用这张表, 直至槽安全 Reset.
            frame.m_Data->shaders = m_Resources->activeShaders;
        }
        assert(frame.m_Data->shaders == m_Resources->activeShaders);
        const auto displayMode = Detail::IsShadowPreview(data.environment.program) ?
            PreviewMode::BaseColor : data.source.mode;
        const auto display = ResolveDisplaySettings(displayMode, view.requestedAa);
        if (!data.needs.surfaceOnly && !m_Resources->RequestDisplay(view.layouts, display))
        {
            Detail::Reject(ViewSettingReason::ShaderPending, "Waiting for prepared display pipelines");
        }
        m_Resources->ResumeView(index);
        if (!data.opaqueResourcesReady)
        {
            const auto protectedResidency = data.effectiveOpaqueResidency ? std::nullopt :
                std::optional{PendingEffectiveResidency(*frame.m_Data, index)};
            Detail::PrepareOpaqueResources(*m_Resources, data, view, data.needs, data.environment.program, buffers,
                                           protectedResidency);
        }
        Detail::ApplyDrawShaders(*m_Resources, data, view);
        if (!data.needs.surfaceOnly)
        {
            m_Resources->display.BindStages(data.display, frame.m_Data->shaders->display, display);
        }
        assert(Detail::UsesFroxels(data.environment.program) == bool(data.froxelViewLayout));
        assert(Detail::UsesFroxels(data.environment.program) == data.froxelUpload.has_value());
        frame.m_Data->hasPreparedView = true;
        frame.m_Data->planned.reset(index);
        return std::nullopt;
    }
    catch (const ForwardFailure &failure)
    {
        RejectView(*frame.m_Data, index);
        return failure;
    }
    catch (const std::bad_alloc &)
    {
        // 换代后的 caster 准备仍会分配. 只撤销本路, 保留另一路可能引用的共享生产者.
        RejectView(*frame.m_Data, index);
        return ForwardFailure{ViewSettingReason::ScenePreparationFailed,
                              Core::BoundedText("Forward final preparation allocation failed")};
    }

    /// @brief 将声明失败交还本次图的唯一入口, 保留图的具体错误.
    template <typename T>
    static T TakeGraph(GraphResult<T> result)
    {
        if (const auto *error = std::get_if<GraphError>(&result))
        {
            throw *error;
        }
        return std::get<T>(std::move(result));
    }

    /// @brief 仅在本图保存物理资源到最后生产版本及实际范围的映射.
    struct SceneGraphResources
    {
        std::map<RHI::Buffer *, BufferId> buffers;
        std::map<RHI::Texture *, TextureId> textures;

        /// @brief 每页只导入一次. 已接受的区间可读, 新代仍须找到本图的生产者.
        [[nodiscard]] BufferId &BufferVersion(RenderGraph &graph, const RHI::BufferHandle &buffer,
            const Detail::BufferPage *page, bool defined)
        {
            const auto [item, inserted] = buffers.try_emplace(buffer.Get());
            if (inserted)
            {
                std::vector<RHI::BufferRange> initial(0);
                if (page && !page->initialized)
                {
                    for (const auto &entry : page->occupied)
                    {
                        if (entry.owner->accepted)
                        {
                            initial.push_back(entry.range);
                        }
                    }
                }
                const bool initialContents = page ? page->initialized : defined;
                item->second = TakeGraph(graph.ImportBuffer(buffer, {BufferBoundary, BufferBoundary, initialContents},
                                                            std::nullopt, initial));
            }
            return item->second;
        }

        /// @brief 优先取本图的生产版本. 首次以读者身份导入时沿用纹理代的接受状态, 由图检查读取权限.
        [[nodiscard]] TextureId ReadTexture(RenderGraph &graph, const Detail::ResidentTexture &product)
        {
            const auto &texture = product.texture;
            const auto [item, inserted] = textures.try_emplace(texture.Get());
            if (inserted)
            {
                item->second = TakeGraph(graph.ImportTexture(texture,
                    {TextureBoundary, TextureBoundary, product.accepted}));
            }
            return item->second;
        }
    };

    /// @brief 按选入图的 View 收集行写入. 冷表生成一份整页载荷, 暖页按消费集合归并连续区间.
    /// 各行关联实际写 Pass, 供正常提交后对照编译计划确认.
    static void DeclarePageUploads(RenderGraph &graph, SceneGraphResources &resources,
        Detail::ForwardFrameStorage &frame, const std::array<bool, ForwardViewCount> &selected,
        std::vector<GraphUpload> &uploads, std::map<Detail::Allocation *, std::vector<PassId>> &storageWrites,
        std::vector<std::pair<Detail::BufferPage *, PassId>> &tableWrites)
    {
        std::map<const Detail::Allocation *, unsigned> consumers;
        for (std::size_t index = 0; index < selected.size(); ++index)
        {
            if (!selected[index])
            {
                continue;
            }
            const auto &view = frame.views[index];
            const auto collect = [&](const Detail::StaticDrawWork &work)
            {
                for (const auto &draw : work.draws)
                {
                    consumers[draw.geometry.get()] |= 1u << index;
                    consumers[draw.object.get()] |= 1u << index;
                    consumers[work.materialRows[draw.material].get()] |= 1u << index;
                }
            };
            collect(view);
            if (view.shadow && !view.shadow->depth.retained)
            {
                for (const auto &level : std::span(view.shadow->levels).first(view.shadow->depth.count))
                {
                    collect(level);
                }
            }
        }

        struct Write
        {
            const Detail::BufferUpload *source;
            unsigned consumers;
        };
        std::vector<Write> ordered(0);
        for (const auto &upload : frame.uploads.buffers)
        {
            const auto match = consumers.find(upload.revision.get());
            if (upload.page && match != consumers.end())
            {
                ordered.push_back({&upload, match->second});
            }
        }
        std::ranges::sort(ordered, [](const Write &a, const Write &b)
        {
            if (a.source->page != b.source->page)
            {
                return std::less<const Detail::BufferPage *>{}(a.source->page, b.source->page);
            }
            return a.source->offset < b.source->offset;
        });
        for (std::size_t first = 0; first < ordered.size();)
        {
            const auto &head = *ordered[first].source;
            const bool cold = head.page->usage == GraphUsage::ShaderRead && !head.page->initialized;
            auto end = head.offset + head.payload.size();
            auto last = first + 1;
            for (; last < ordered.size(); ++last)
            {
                // 已初始化的页只合并同消费条件下的实际脏区间. 冷表的完整绑定需要一次填满.
                const auto &next = ordered[last];
                if (next.source->page != head.page ||
                    (!cold && (next.consumers != ordered[first].consumers || next.source->offset != end)))
                {
                    break;
                }
                end += next.source->payload.size();
            }

            std::span<const std::byte> bytes = head.payload;
            auto offset = head.offset;
            if (cold)
            {
                // 每个已选行直接写到初始化载荷, 间隙实写零. 同页只登记一次生产和接受.
                offset = 0;
                auto &image = frame.uploads.merged.emplace_back(head.buffer->GetDesc().byteSize);
                for (const auto &part : std::span(ordered).subspan(first, last - first))
                {
                    const auto &source = *part.source;
                    std::ranges::copy(source.payload, image.begin() + source.offset);
                }
                bytes = image;
            }
            else if (last != first + 1)
            {
                // 合并载荷留在封存批中, Graph 的回调直到执行结束都借用此区段.
                auto &merged = frame.uploads.merged.emplace_back(0);
                merged.reserve(static_cast<std::size_t>(end - head.offset));
                for (auto part : std::span(ordered).subspan(first, last - first))
                {
                    merged.insert(merged.end(), part.source->payload.begin(), part.source->payload.end());
                }
                bytes = merged;
            }
            auto &version = resources.BufferVersion(graph, head.buffer, head.page, false);
            version = TakeGraph(AddBufferUpload(graph, version, bytes, uploads, offset,
                                                cold ? &head.page->initializationRecorded : nullptr));
            const auto pass = uploads.back().pass;
            uploads.back().source = GraphUploadSource::ForwardStatic;
            if (cold)
            {
                tableWrites.emplace_back(head.page, pass);
            }
            for (auto part : std::span(ordered).subspan(first, last - first))
            {
                storageWrites[part.source->revision.get()].push_back(pass);
            }
            first = last;
        }
    }

    /// @brief 按资源接续待写版本, 保留已有有效页区间与新写入的不同来源.
    static SceneGraphResources DeclareUploads(RenderGraph &graph, Detail::ForwardFrameStorage &frame,
                                               const std::array<bool, ForwardViewCount> &selected,
                                               std::vector<GraphUpload> &uploads,
                                               std::map<Detail::Allocation *, std::vector<PassId>> &storageWrites,
                                               std::vector<std::pair<Detail::BufferPage *, PassId>> &tableWrites,
                                               std::array<std::vector<PassId>, ForwardViewCount> &viewUploadPasses)
    {
        const auto &batch = frame.uploads;
        SceneGraphResources resources;
        // 一代几何可有两块缓冲, 一个 cube 可有多个面和 mip. 确认时须能逐笔找到同一代的写入.
        const auto record = [&](Detail::Allocation *storage, std::size_t first, GraphUploadSource source,
                                std::optional<std::size_t> view = {})
        {
            auto &passes = storageWrites[storage];
            for (auto &write : std::span(uploads).subspan(first))
            {
                write.source = source;
                passes.push_back(write.pass);
                if (view)
                {
                    // 灯表可按 ForwardStatic 统计, 但缓冲仍归单个 View. 用写入记录的 view 定位失败.
                    viewUploadPasses[*view].push_back(write.pass);
                }
            }
        };
        for (const auto &upload : batch.textures)
        {
            const auto first = uploads.size();
            const auto &texture = upload.revision->texture;
            auto version = TakeGraph(graph.ImportTexture(texture, {TextureBoundary, TextureBoundary, false}));
            auto width = texture->GetDesc().width;
            for (std::uint32_t mip = 0; mip < upload.levels.size(); ++mip)
            {
                version = TakeGraph(AddTextureUpload(graph, version, upload.levels[mip], std::size_t(width) * 4,
                    uploads, mip, 0, &upload.revision->uploadRecorded)).texture;
                width = std::max(1u, width / 2);
            }
            resources.textures.emplace(texture.Get(), version);
            record(upload.revision.get(), first, GraphUploadSource::ForwardStatic);
        }
        for (const auto &upload : batch.environmentTextures)
        {
            const auto first = uploads.size();
            const auto &texture = upload.revision->texture;
            const auto &desc = texture->GetDesc();
            auto version = TakeGraph(graph.ImportTexture(texture, {TextureBoundary, TextureBoundary, false}));
            if (upload.dfg)
            {
                version = TakeGraph(AddTextureUpload(graph, version, std::as_bytes(std::span(upload.dfg->pixels)),
                    std::size_t(desc.width) * sizeof(upload.dfg->pixels.front()), uploads)).texture;
            }
            else
            {
                // 每个实际面和 mip 是完整写入. 下一层读依赖同一存储的最后生产版本.
                for (std::uint32_t mip = 0; mip < desc.mipLevels; ++mip)
                {
                    const auto &level = mip == 0 ? *upload.environment->base :
                                                  *upload.environment->prefiltered[mip - 1];
                    for (std::uint32_t face = 0; face < desc.arraySize; ++face)
                    {
                        version = TakeGraph(AddTextureUpload(graph, version, std::as_bytes(level.Face(face)),
                            std::size_t(level.edge) * sizeof(Detail::EnvironmentTexel), uploads, mip, face)).texture;
                    }
                }
            }
            resources.textures.emplace(texture.Get(), version);
            record(upload.revision.get(), first, GraphUploadSource::ForwardStatic);
        }
        for (const auto &upload : batch.buffers)
        {
            if (upload.page)
            {
                continue;
            }
            const auto first = uploads.size();
            auto &version = resources.BufferVersion(graph, upload.buffer, upload.page, false);
            bool *recorded = nullptr;
            if (upload.view)
            {
                auto &view = frame.views[*upload.view];
                if (upload.revision == view.deferredView)
                {
                    recorded = &view.deferredViewWriteRecorded;
                }
            }
            version = TakeGraph(AddBufferUpload(graph, version, upload.payload, uploads, upload.offset, recorded));
            record(upload.revision.get(), first, upload.source, upload.view);
        }
        DeclarePageUploads(graph, resources, frame, selected, uploads, storageWrites, tableWrites);
        return resources;
    }

    /// @brief 本 Pass 的共享页, 原生批次状态及精确参数区间. 借用的集合由 ForwardFrame 保活.
    struct GraphBatch
    {
        BufferAccess vertices;
        BufferAccess indices;
        BufferAccess arguments;
        RHI::BindingSet *renderables;
        RHI::BindingSet *material;
        RHI::GraphicsPipeline *pipeline;
        std::uint32_t commandCount;
    };

    /// @brief 当前 View 的灯表、环境与阴影图版本, 供几何颜色和屏幕颜色共用.
    /// shadowRows 按物理层索引, 受光 Pass 从 shadowReferences 按灯行定位组首层.
    struct LightingInputs
    {
        std::optional<BufferAccess> lights;
        RHI::BindingLayout *lightLayout = nullptr;
        std::optional<BufferAccess> environment;
        std::optional<TextureAccess> dfg;
        std::optional<TextureAccess> reflection;
        RHI::Sampler *environmentSampler = nullptr;
        std::optional<BufferAccess> shadowConstants;
        std::optional<BufferAccess> shadowRows;
        std::optional<BufferAccess> shadowReferences;
        std::optional<TextureAccess> shadowImage;
    };

    /// @brief 一个几何 Pass 的图像矩形, View 集合与 Draw 顺序.
    struct SceneRaster
    {
        RHI::ViewportState viewport;
        RHI::BindingSet *view = nullptr;
        RHI::BindingLayout *froxelViewLayout = nullptr;
        std::optional<Detail::FroxelGraphInputs> froxels;
        BufferAccess viewRow{}; ///< 本图的 b0. Froxel 颜色在预检时与四个块合为一组.
        LightingInputs lighting;
        std::vector<GraphBatch> batches;
    };

    /// @brief 同一 Pass 按资源及范围去重, 同页上不同几何或材质区间仍各自保留.
    /// 表键使用本图的存储索引, 值内的 BufferUse 和 TextureAccess 仍携带相应内容版本.
    struct RasterReads
    {
        std::map<std::tuple<std::uint32_t, std::uint64_t, std::uint64_t, GraphUsage>, BufferUse> buffers;
        std::map<std::uint32_t, TextureAccess> textures;

        /// @brief 登记实际读区间. 索引表可绑定整页, 对应的图依赖仍只覆盖使用的行.
        void AddBuffer(BufferAccess access, std::optional<RHI::BufferRange> indexed = std::nullopt)
        {
            buffers.try_emplace(std::tuple{access.resource.Identity().index, access.range.byteOffset,
                                          access.range.byteSize, access.usage}, BufferUse{access, std::nullopt, indexed});
        }
    };

    /// @brief 登记整块常量或结构化缓冲的读取. 同图上传优先, 已驻留的输入沿用导入版本.
    static BufferAccess ReadColorBuffer(RenderGraph &graph, SceneGraphResources &versions, RasterReads &reads,
                                         const RHI::BufferHandle &buffer, GraphUsage usage)
    {
        const BufferAccess access{versions.BufferVersion(graph, buffer, nullptr, true),
                                  {0, buffer->GetDesc().byteSize}, usage};
        reads.AddBuffer(access);
        return access;
    }

    /// @brief 按本轮角色取得共享颜色输入. 诊断仅使用获准的阴影范围.
    static LightingInputs ReadLightingInputs(RenderGraph &graph, SceneGraphResources &versions, RasterReads &reads,
                                              const Detail::ForwardFrameData &frame, TextureId shadowDepth)
    {
        LightingInputs lighting;
        lighting.lightLayout = frame.lightLayout.Get();
        const bool preview = Detail::IsShadowPreview(frame.environment.program);
        const auto buffer = [&](const RHI::BufferHandle &handle, GraphUsage usage)
        {
            return ReadColorBuffer(graph, versions, reads, handle, usage);
        };
        if (frame.shadow)
        {
            const auto &shadow = *frame.shadow;
            lighting.shadowConstants = buffer(shadow.constants->buffer, GraphUsage::ConstantBuffer);
            lighting.shadowRows = buffer(shadow.layerRows->buffer, GraphUsage::ShaderRead);
            if (!preview)
            {
                lighting.shadowReferences = buffer(shadow.references->buffer, GraphUsage::ShaderRead);
            }
            const TextureAccess access{shadowDepth, {.layerCount = shadow.depth.count, .aspect = TextureAspect::Depth},
                                       GraphUsage::Sampled};
            lighting.shadowImage = access;
            reads.textures.try_emplace(shadowDepth.Identity().index, access);
        }
        if (!preview)
        {
            lighting.lights = buffer(frame.activeLights->buffer, GraphUsage::ShaderRead);
            const auto &environment = frame.environment;
            const auto sample = [&](const Detail::ResidentTexture &texture)
            {
                const auto version = versions.ReadTexture(graph, texture);
                const auto &desc = texture.texture->GetDesc();
                const TextureAccess access{version, {.mipCount = desc.mipLevels, .layerCount = desc.arraySize},
                                           GraphUsage::Sampled};
                reads.textures.try_emplace(version.Identity().index, access);
                return access;
            };
            if (environment.dfg)
            {
                lighting.dfg = sample(*environment.dfg);
                lighting.environmentSampler = environment.sampler.Get();
            }
            if (environment.reflection)
            {
                lighting.reflection = sample(*environment.reflection);
                lighting.environment = buffer(environment.lightingConstants->buffer, GraphUsage::ConstantBuffer);
            }
        }
        return lighting;
    }

    /// @brief 从有效图像矩形建立光栅视口及同边界的裁剪.
    static RHI::ViewportState RasterViewport(const ViewImageRect &rect)
    {
        RHI::ViewportState state;
        state.AddViewportAndScissorRect(RHI::Viewport(static_cast<float>(rect.x),
            static_cast<float>(rect.x + rect.extent.width), static_cast<float>(rect.y),
            static_cast<float>(rect.y + rect.extent.height), 0.f, 1.f));
        return state;
    }

    /// @brief 几何 Pass 对已排序表面的选择. 两个辅助 Pass 共用 Auxiliary 选择.
    enum class RasterSurfaces : std::uint8_t { All, Core, Auxiliary };

    /// @brief 将预解析的 Draw 绑定关联到本 Pass 的实际读范围.
    /// 深度和阴影诊断只取基色. 辅助表面写入另取粗糙度与法线. 几何着色由 color 选用 set 3.
    /// 阴影附件使用自己的方形 viewport.
    static SceneRaster RasterInputs(RenderGraph &graph, SceneGraphResources &versions,
        const Detail::StaticDrawWork &work, const ViewImageRect &rect, RasterReads &reads,
        const Detail::ForwardFrameData *color = nullptr, TextureId shadowDepth = {},
        const Detail::FroxelGraphInputs *froxels = nullptr, RasterSurfaces surfaces = RasterSurfaces::All)
    {
        SceneRaster raster;
        raster.viewport = RasterViewport(rect);
        const auto selected = [surfaces](const Detail::MaterialRow &material)
        {
            return surfaces == RasterSurfaces::All || (surfaces == RasterSurfaces::Core) ==
                (material.route == Detail::OpaqueSurfaceRoute::CoreDeferred);
        };
        if (work.draws.empty() || (surfaces != RasterSurfaces::All &&
            std::ranges::none_of(work.materialRows, [&](const auto &row) { return selected(*row); })))
        {
            return raster;
        }
        const auto readBuffer = [&](const RHI::BufferHandle &handle, GraphUsage usage, RHI::BufferRange range,
                                    const Detail::BufferPage *page,
                                    std::optional<RHI::BufferRange> indexedBinding = std::nullopt)
        {
            BufferAccess access{versions.BufferVersion(graph, handle, page, true), range, usage};
            reads.AddBuffer(access, indexedBinding);
            return access;
        };
        const auto slice = [&](const Detail::PageSlice &storage)
        {
            return readBuffer(storage.page->buffer, storage.page->usage, storage.range, storage.page.get());
        };
        const auto table = [&](const Detail::PageSlice &storage)
        {
            // 冷页依赖同图整页生产. 暖页仍绑定整页, 只有实际实例索引的行带来增量依赖.
            const auto &page = storage.page;
            const RHI::BufferRange binding{0, page->buffer->GetDesc().byteSize};
            const auto range = page->initialized ? storage.range : binding;
            static_cast<void>(readBuffer(page->buffer, GraphUsage::ShaderRead, range, page.get(),
                                        page->initialized ? std::optional{binding} : std::nullopt));
        };
        raster.viewRow = ReadColorBuffer(graph, versions, reads, work.view->buffer, GraphUsage::ConstantBuffer);
        if (froxels)
        {
            assert(color && color->froxelViewLayout);
            raster.froxels = *froxels;
            raster.froxelViewLayout = color->froxelViewLayout.Get();
        }
        else
        {
            raster.view = work.view->binding.Get();
        }
        if (color)
        {
            raster.lighting = ReadLightingInputs(graph, versions, reads, *color, shadowDepth);
        }
        const bool preview = color && Detail::IsShadowPreview(color->environment.program);
        // 同一批的实例可以选取不同对象行和材质行. 逐表面登记这些行及几何区间, 重复的读由 readBuffer 合并.
        for (const auto &draw : work.draws)
        {
            const auto &material = *work.materialRows[draw.material];
            if (!selected(material))
            {
                continue;
            }
            static_cast<void>(slice(draw.geometry->vertices));
            static_cast<void>(slice(draw.geometry->indices));
            table(draw.object->storage);
            table(material.storage);
        }
        const bool surfaceWrite = !color && surfaces != RasterSurfaces::All;
        constexpr auto commandBytes = sizeof(RHI::DrawIndexedIndirectArguments);
        raster.batches.reserve(work.batches.size());
        for (const auto &batch : work.batches)
        {
            const auto &draw = work.draws[batch.firstDraw];
            const auto &material = *work.materialRows[draw.material];
            if (!selected(material))
            {
                continue;
            }
            static_cast<void>(ReadColorBuffer(graph, versions, reads, work.instanceBlocks[batch.instanceBlock]->buffer,
                                             GraphUsage::ShaderRead));
            auto slots = color && draw.iorSpecularColor ? material.textures.size() : Detail::CoreMaterialTextureCount;
            if (surfaceWrite && surfaces == RasterSurfaces::Auxiliary)
            {
                // 原生 SurfaceAux 配方含 MASK 基色绑定. 独立 OPAQUE 行把该槽指向同一行的表面图像,
                // 按实际绑定登记即可在图中合并相同图像的读.
                slots = Detail::AuxSurfaceTextureCount;
            }
            else if ((!color && !surfaceWrite) || preview)
            {
                slots = Detail::CoverageTextureCount;
            }
            for (std::size_t i = 0; i < slots; ++i)
            {
                const auto &product = *material.textures[i];
                const auto version = versions.ReadTexture(graph, product);
                reads.textures.try_emplace(version.Identity().index,
                    TextureAccess{version, {.firstMip = product.sampling.baseMipLevel,
                        .mipCount = product.sampling.numMipLevels}, GraphUsage::Sampled});
            }
            const auto arguments = readBuffer(work.indirect->buffer, GraphUsage::IndirectArgument,
                {batch.firstCommand * commandBytes, batch.commandCount * commandBytes}, nullptr);
            raster.batches.push_back({slice(draw.geometry->vertices), slice(draw.geometry->indices), arguments,
                batch.renderables.Get(), material.binding->Get(),
                (surfaceWrite ? draw.surfacePipeline : (color ? draw.pipeline : draw.depthPipeline)).Get(),
                batch.commandCount});
        }
        return raster;
    }

    /// @brief 执行只使用图授权的几何、桥接附件和现成集合. RHI 对集合里的活动资源检查实际范围.
    static GraphFailure DrawScene(PassResources &resources, const SceneRaster &raster,
                                  RHI::CommandList &commands) noexcept
    {
        auto *viewBindings = raster.froxels ? resources.BindingSet(0) : raster.view;
        auto *passBindings = raster.lighting.lightLayout ? resources.BindingSet(raster.froxels ? 1 : 0) : nullptr;
        if ((raster.froxels && !viewBindings) || (raster.lighting.lightLayout && !passBindings))
        {
            // BindingSet 已将失败记入本 Pass. 停止录制, 由执行器返回该原因.
            return std::nullopt;
        }
        for (const auto &batch : raster.batches)
        {
            auto *vertices = resources.Resolve(batch.vertices.resource, batch.vertices.usage, batch.vertices.range);
            auto *indices = resources.Resolve(batch.indices.resource, batch.indices.usage, batch.indices.range);
            auto *arguments = resources.Resolve(batch.arguments.resource, batch.arguments.usage, batch.arguments.range);
            if (!vertices || !indices || !arguments)
            {
                return std::nullopt;
            }
            RHI::GraphicsState state{};
            // firstIndex 和 baseVertex 已含各自的页内起点. 这里按页基准绑定, 仅命令参数另取字节偏移.
            state.SetFramebuffer(resources.Framebuffer()).SetPipeline(batch.pipeline).SetViewport(raster.viewport)
                .AddBindingSet(viewBindings).AddBindingSet(batch.renderables).AddBindingSet(batch.material)
                .AddVertexBuffer(RHI::VertexBufferBinding().SetBuffer(vertices).SetOffset(0))
                .SetIndexBuffer(RHI::IndexBufferBinding().SetBuffer(indices).SetFormat(RHI::Format::R32_UINT)
                    .SetOffset(0)).SetIndirectParams(arguments);
            if (passBindings)
            {
                state.AddBindingSet(passBindings);
            }
            commands.SetGraphicsState(state);
            commands.DrawIndexedIndirect(static_cast<std::uint32_t>(batch.arguments.range.byteOffset),
                                          batch.commandCount);
        }
        return std::nullopt;
    }

    /// @brief 声明光栅 Pass 的几何和材质读范围, 阴影数组一次声明全部实际采样层.
    static bool DeclareRasterReads(PassBuilder &builder, const RasterReads &reads)
    {
        for (const auto &[index, use] : reads.buffers)
        {
            const auto &access = use.access;
            const bool declared = use.indexedBinding ?
                builder.ReadIndexedRows(access.resource, access.range, *use.indexedBinding) :
                builder.Read(access.resource, access.usage, access.range);
            if (!declared)
            {
                return false;
            }
        }
        for (const auto &[index, access] : reads.textures)
        {
            if (!builder.Read(access.resource, access.usage, access.range))
            {
                return false;
            }
        }
        return true;
    }

    /// @brief 将相机常量与同一图内写出的四块网格数据绑定到 set 0.
    static bool BindFroxelView(PassPreparation &preparation, BufferAccess viewRow,
                               const Detail::FroxelGraphInputs &froxels, RHI::BindingLayout &layout)
    {
        const auto constants = preparation.ReadBuffer(0, viewRow.resource, viewRow.usage, viewRow.range);
        const auto froxelItems = froxels.Bind(preparation);
        if (!constants || !froxelItems)
        {
            return false;
        }
        const std::array items{*constants, (*froxelItems)[0], (*froxelItems)[1], (*froxelItems)[2], (*froxelItems)[3]};
        return preparation.CreateBindingSet(layout, items).has_value();
    }

    /// @brief 由实际用途和共享布局绑定灯、环境及阴影集合. 纹理视图限于本 Pass 声明的范围.
    static void BindLighting(PassPreparation &preparation, const LightingInputs &data)
    {
        if (!data.lightLayout)
        {
            return;
        }
        std::array<RHI::BindingSetItem, 9> items; ///< IBL 与阴影同时启用时的完整 PerPass 配方.
        std::size_t count = 0;
        for (const auto &slot : data.lightLayout->GetDesc()->bindings)
        {
            std::optional<RHI::BindingSetItem> binding;
            if (slot.type == RHI::ResourceType::StructuredBuffer_SRV)
            {
                const auto &access = slot.slot == Detail::ShadowLayersSlot ? *data.shadowRows :
                    (slot.slot == Detail::ShadowReferencesSlot ? *data.shadowReferences : *data.lights);
                binding = preparation.ReadBuffer(slot.slot, access.resource, access.usage, access.range);
            }
            else if (slot.type == RHI::ResourceType::ConstantBuffer)
            {
                const auto &access = slot.slot == Detail::ShadowConstantsSlot ? *data.shadowConstants : *data.environment;
                binding = preparation.ReadBuffer(slot.slot, access.resource, access.usage, access.range);
            }
            else if (slot.type == RHI::ResourceType::Sampler)
            {
                binding = RHI::BindingSetItem::Sampler(slot.slot, data.environmentSampler);
            }
            else if (slot.slot == Detail::ShadowImageSlot)
            {
                const auto &access = *data.shadowImage;
                binding = preparation.SampledTexture(slot.slot, access.resource, access.range,
                                                     RHI::TextureDimension::Texture2DArray);
            }
            else
            {
                const bool cube = slot.slot == Detail::ReflectionSlot;
                assert(cube || slot.slot == Detail::DfgSlot);
                const auto &access = cube ? *data.reflection : *data.dfg;
                binding = preparation.SampledTexture(slot.slot, access.resource, access.range,
                    cube ? RHI::TextureDimension::TextureCube : RHI::TextureDimension::Texture2D);
            }
            if (!binding)
            {
                return;
            }
            assert(count < items.size());
            items[count++] = *binding;
        }
        static_cast<void>(preparation.CreateBindingSet(*data.lightLayout, std::span(items).first(count)));
    }

    /// @brief Froxel 颜色先保存 set 0, 再保存 set 3. 普通颜色直接借用已有的 set 0.
    static GraphFailure PrepareRasterBindings(PassPreparation &preparation, const SceneRaster &data)
    {
        if (!data.froxels || BindFroxelView(preparation, data.viewRow, *data.froxels, *data.froxelViewLayout))
        {
            BindLighting(preparation, data.lighting);
        }
        return std::nullopt;
    }

    static const RHI::Color OpaqueBackground(0.018f, 0.018f, 0.018f, 0.f);

    /// @brief 深度和颜色共享 Draw 顺序及声明规则. 读前置深度时使用只读附件, 从而确定 WAW/RAW 关系.
    static PassId AddGeometryPass(RenderGraph &graph, SceneGraphResources &versions,
        const Detail::ForwardFrameData &frame, TextureId &depth, TextureId *color, bool readOnlyDepth,
        TextureId shadowDepth = {}, const Detail::FroxelGraphInputs *froxels = nullptr)
    {
        RasterReads reads;
        auto raster = RasterInputs(graph, versions, frame, frame.source.layout.rect, reads,
                                   color ? &frame : nullptr, shadowDepth, froxels);
        const auto *label = color ? (froxels ? "Forward+ ShadeOpaque" : "Forward ShadeOpaque") : "Forward PrepareSurface";
        return TakeGraph(graph.AddPass<SceneRaster>(label,
            [&](PassBuilder &builder, auto &data)
            {
                data = std::move(raster);
                if (!DeclareRasterReads(builder, reads) || (data.froxels && !data.froxels->DeclareReads(builder)))
                {
                    return false;
                }
                const TextureRange depthRange{.aspect = TextureAspect::Depth};
                if (readOnlyDepth)
                {
                    if (!builder.Read(depth, GraphUsage::DepthStencilRead, depthRange))
                    {
                        return false;
                    }
                }
                else
                {
                    depth = builder.Write(depth, GraphUsage::DepthStencilAttachment,
                        {.load = LoadOp::Clear, .clearValue = DepthStencilClear{0.f, 0}}, depthRange);
                }
                if (color)
                {
                    *color = builder.Write(*color, GraphUsage::ColorAttachment,
                         {.load = LoadOp::Clear, .clearValue = OpaqueBackground});
                }
                return depth.IsValid() && (!color || color->IsValid());
            }, DrawScene, PrepareRasterBindings));
    }

    /// @brief 命中时只读导入已接受的层组. 新组按子视图顺序写连续物理层, 每次 Write 接续上一图版本.
    /// 存活的空 caster 层也执行 Clear/Store. 跨帧保留由建立颜色消费者的调用方声明.
    static TextureId DeclareShadowDepth(RenderGraph &graph, SceneGraphResources &resources,
                                             const Detail::ShadowFrame &frame)
    {
        const auto &group = frame.depth;
        const TextureRange all{.layerCount = group.count, .aspect = TextureAspect::Depth};
        if (group.retained)
        {
            return TakeGraph(graph.ImportTexture(group.retained, Detail::SampledState(all.aspect), all));
        }
        constexpr auto *label = "Shadow depth";
        auto depth = TakeGraph(graph.CreateTexture({.width = group.edge, .height = group.edge, .arraySize = group.count,
            .format = ShadowDepthFormat, .dimension = RHI::TextureDimension::Texture2DArray,
            .debugName = label, .isRenderTarget = true}));
        const ViewImageRect rect{.extent = {group.edge, group.edge}};
        for (std::uint32_t layer = 0; layer < group.count; ++layer)
        {
            RasterReads reads;
            auto raster = RasterInputs(graph, resources, frame.levels[layer], rect, reads);
            const TextureRange range{.firstLayer = layer, .aspect = TextureAspect::Depth};
            static_cast<void>(TakeGraph(graph.AddPass<SceneRaster>(label,
                [&](PassBuilder &builder, auto &data)
                {
                    data = std::move(raster);
                    if (!DeclareRasterReads(builder, reads))
                    {
                        return false;
                    }
                    depth = builder.Write(depth, GraphUsage::DepthStencilAttachment,
                        {.load = LoadOp::Clear, .clearValue = DepthStencilClear{ShadowDepthConvention::Clear, 0}}, range);
                    return depth.IsValid();
                }, DrawScene)));
        }
        return depth;
    }

    /// @brief 天空读取本图的一个完整基础 cube, 加载不透明工作色后只填其未覆盖 RGB.
    struct SkyPass
    {
        TextureId sky;
        BufferAccess view;
        RHI::Sampler *sampler;
        RHI::GraphicsPipeline *pipeline;
        RHI::BindingLayout *layout;
        RHI::ViewportState viewport;
    };

    /// @brief 在有效矩形内加载上一版工作色, 以目的 alpha 混入天空. 保留已有的深度版本.
    static void AddSky(RenderGraph &graph, SceneGraphResources &resources, const Detail::ForwardFrameData &frame,
                       TextureId &color)
    {
        const auto &environment = frame.environment;
        if (!environment.sky)
        {
            return;
        }
        const auto &view = environment.skyConstants->buffer;
        const auto cube = resources.ReadTexture(graph, *environment.sky);
        const BufferAccess constants{resources.BufferVersion(graph, view, nullptr, true), {0, view->GetDesc().byteSize},
                                     GraphUsage::ConstantBuffer};
        static_cast<void>(TakeGraph(graph.AddPass<SkyPass>("Forward Sky",
            [&](PassBuilder &builder, SkyPass &data)
            {
                data = {cube, constants, environment.sampler.Get(), environment.skyPipeline.Get(),
                        environment.skyLayout.Get(), RasterViewport(frame.source.layout.rect)};
                color = builder.Write(color, GraphUsage::ColorAttachment, {.load = LoadOp::Load});
                return color.IsValid() && builder.Read(cube, GraphUsage::Sampled) &&
                       builder.Read(constants.resource, constants.usage, constants.range);
            }, +[](PassResources &resources, const SkyPass &data, RHI::CommandList &commands) noexcept -> GraphFailure
            {
                RHI::GraphicsState state;
                state.SetPipeline(data.pipeline).SetFramebuffer(resources.Framebuffer())
                    .SetViewport(data.viewport).AddBindingSet(resources.BindingSet(0));
                commands.SetGraphicsState(state);
                commands.Draw(RHI::DrawArguments().SetVertexCount(3).SetInstanceCount(1));
                return std::nullopt;
            }, +[](PassPreparation &preparation, const SkyPass &data) -> GraphFailure
            {
                const auto view = preparation.ReadBuffer(0, data.view.resource, data.view.usage, data.view.range);
                const auto sky = preparation.SampledTexture(0, data.sky, std::nullopt, RHI::TextureDimension::TextureCube);
                if (view && sky)
                {
                    const std::array items{*view, *sky, RHI::BindingSetItem::Sampler(0, data.sampler)};
                    static_cast<void>(preparation.CreateBindingSet(*data.layout, items));
                }
                return std::nullopt;
            })));
    }

    /// @brief 以核心材质批次绘制同一份四附件快照, 深度与资格位显式初始化以支持池内存复用.
    static CoreGBufferOutput DeclareCoreSurface(RenderGraph &graph, SceneGraphResources &versions,
                                                const Detail::ForwardFrameData &frame)
    {
        CoreGBufferOutput surface{};
        surface.rect = frame.source.layout.rect;
        const auto extent = frame.source.layout.outputExtent;
        const auto texture = [&](RHI::Format format, const char *name)
        {
            return TakeGraph(graph.CreateTexture({.width = extent.width, .height = extent.height,
                .format = format, .debugName = name, .isRenderTarget = true}));
        };
        constexpr std::array names{"G0 base color and AO", "G1 normal and material", "G2 pre-exposed emission", "G3 flags"};
        for (std::size_t i = 0; i < surface.colors.size(); ++i)
        {
            surface.colors[i] = texture(surface.Formats[i], names[i]);
        }
        surface.depth = texture(DepthFormat, "Opaque surface reverse depth");
        RasterReads reads;
        auto raster = RasterInputs(graph, versions, frame, surface.rect, reads, nullptr, {}, nullptr,
                                   RasterSurfaces::Core);
        surface.producer = TakeGraph(graph.AddPass<SceneRaster>("Core GBuffer",
            [&](PassBuilder &builder, SceneRaster &data)
            {
                data = std::move(raster);
                if (!DeclareRasterReads(builder, reads))
                {
                    return false;
                }
                for (auto i : Detail::CoreSurfaceAttachments)
                {
                    TextureWriteOps ops{.load = LoadOp::Clear, .clearValue = RHI::Color(0.f)};
                    if (surface.Formats[i] == RHI::Format::R16_UINT)
                    {
                        ops.clearValue = std::uint32_t{0};
                    }
                    auto &color = surface.colors[i];
                    color = builder.Write(color, GraphUsage::ColorAttachment, ops);
                    if (!color.IsValid())
                    {
                        return false;
                    }
                }
                surface.depth = builder.Write(surface.depth, GraphUsage::DepthStencilAttachment,
                    {.load = LoadOp::Clear, .clearValue = DepthStencilClear{0.f, 0}}, TextureRange{.aspect = TextureAspect::Depth});
                return surface.depth.IsValid();
            }, DrawScene));
        return surface;
    }

    /// @brief 核心阶段初始化整个表面, 辅助批次随后以同一栅格的最近覆盖接续 G1、G3 和 D32.
    static CoreGBufferOutput PrepareDeferredSurface(RenderGraph &graph, SceneGraphResources &versions,
                                                    const Detail::ForwardFrameData &frame)
    {
        auto surface = DeclareCoreSurface(graph, versions, frame);
        if (!frame.HasAuxiliarySurface())
        {
            return surface;
        }
        RasterReads reads;
        auto raster = RasterInputs(graph, versions, frame, surface.rect, reads, nullptr, {}, nullptr,
                                   RasterSurfaces::Auxiliary);
        surface.producer = TakeGraph(graph.AddPass<SceneRaster>("SurfaceAux",
            [&](PassBuilder &builder, SceneRaster &data)
            {
                data = std::move(raster);
                if (!DeclareRasterReads(builder, reads))
                {
                    return false;
                }
                // G0 和 G2 保留核心阶段的版本. 两个 Load 加上下方深度 Load 保留孔内和遮挡后的表面.
                for (auto index : Detail::AuxSurfaceAttachments)
                {
                    auto &attachment = surface.colors[index];
                    attachment = builder.Write(attachment, GraphUsage::ColorAttachment, {.load = LoadOp::Load});
                    if (!attachment.IsValid())
                    {
                        return false;
                    }
                }
                surface.depth = builder.Write(surface.depth, GraphUsage::DepthStencilAttachment,
                    {.load = LoadOp::Load}, TextureRange{.aspect = TextureAspect::Depth});
                return surface.depth.IsValid();
            }, DrawScene));
        return surface;
    }

    /// @brief 屏幕颜色的冻结输入. 纯背景初始化时 pipeline 为空且没有采样或灯光依赖.
    struct DeferredLightingPass
    {
        CoreGBufferOutput surface;
        BufferAccess view;
        BufferAccess reconstruction;
        RHI::BindingLayout *froxelLayout = nullptr;
        RHI::BindingLayout *surfaceLayout = nullptr;
        RHI::GraphicsPipeline *pipeline = nullptr;
        Detail::FroxelGraphInputs froxels;
        LightingInputs lighting;
    };

    static constexpr TextureRange OpaqueDepthRange{.aspect = TextureAspect::Depth};

    /// @brief 按屏幕 Pipeline 的顺序保存 View、表面及共享灯光三个集合.
    /// 录制时按预检保存的顺序取回集合 0, 1, 2, 分别对应 Shader set 0, 1, 3.
    /// 资源解析或建集失败由 PassPreparation 记录, 无 Pipeline 的背景 Pass 只执行附件清除.
    static GraphFailure BindDeferredLighting(PassPreparation &preparation, const DeferredLightingPass &data)
    {
        if (!data.pipeline || !BindFroxelView(preparation, data.view, data.froxels, *data.froxelLayout))
        {
            return std::nullopt;
        }
        const auto constants = preparation.ReadBuffer(Detail::DeferredViewSlot, data.reconstruction.resource,
                                                       data.reconstruction.usage, data.reconstruction.range);
        const auto depth = preparation.SampledTexture(Detail::GBufferDepthSlot, data.surface.depth, OpaqueDepthRange);
        if (!constants || !depth)
        {
            return std::nullopt;
        }
        std::array<RHI::BindingSetItem, CoreGBufferOutput::Formats.size() + 2> items;
        items.front() = *constants;
        items.back() = *depth;
        for (std::uint32_t index = 0; index < data.surface.colors.size(); ++index)
        {
            const auto color = preparation.SampledTexture(Detail::GBufferFirstSlot + index,
                                                           data.surface.colors[index]);
            if (!color)
            {
                return std::nullopt;
            }
            items[index + 1] = *color;
        }
        if (preparation.CreateBindingSet(*data.surfaceLayout, items))
        {
            BindLighting(preparation, data.lighting);
        }
        return std::nullopt;
    }

    /// @brief 以最终表面资格遮罩屏幕颜色, 工作域自发光和各项共享受光直接写到已初始化的 HDR.
    static void ShadeDeferredOpaque(RenderGraph &graph, SceneGraphResources &versions,
        const Detail::ForwardFrameData &frame, const CoreGBufferOutput &surface, TextureId &color,
        TextureId shadowDepth, const Detail::FroxelGraphInputs *froxels)
    {
        RasterReads reads;
        DeferredLightingPass pass{};
        if (frame.deferredPipeline)
        {
            assert(froxels && frame.deferredView && frame.froxelViewLayout && frame.deferredSurfaceLayout);
            pass.surface = surface;
            pass.pipeline = frame.deferredPipeline.Get();
            pass.froxelLayout = frame.froxelViewLayout.Get();
            pass.surfaceLayout = frame.deferredSurfaceLayout.Get();
            pass.froxels = *froxels;
            pass.view = ReadColorBuffer(graph, versions, reads, frame.view->buffer, GraphUsage::ConstantBuffer);
            pass.reconstruction = ReadColorBuffer(graph, versions, reads, frame.deferredView->buffer,
                                                  GraphUsage::ConstantBuffer);
            pass.lighting = ReadLightingInputs(graph, versions, reads, frame, shadowDepth);
        }
        static_cast<void>(TakeGraph(graph.AddPass<DeferredLightingPass>(
            pass.pipeline ? "DeferredLighting" : "Deferred opaque background",
            [&](PassBuilder &builder, DeferredLightingPass &data)
            {
                data = pass;
                if (data.pipeline)
                {
                    if (!DeclareRasterReads(builder, reads) || !data.froxels.DeclareReads(builder) ||
                        !builder.Read(surface.depth, GraphUsage::Sampled, OpaqueDepthRange))
                    {
                        return false;
                    }
                    for (const auto attachment : surface.colors)
                    {
                        if (!builder.Read(attachment, GraphUsage::Sampled))
                        {
                            return false;
                        }
                    }
                }
                color = builder.Write(color, GraphUsage::ColorAttachment,
                                     {.load = LoadOp::Clear, .clearValue = OpaqueBackground});
                return color.IsValid();
            }, +[](PassResources &resources, const DeferredLightingPass &data, RHI::CommandList &commands) noexcept -> GraphFailure
            {
                if (data.pipeline)
                {
                    auto *view = resources.BindingSet(0);
                    auto *surfaceBindings = resources.BindingSet(1);
                    auto *lighting = resources.BindingSet(2);
                    if (view && surfaceBindings && lighting)
                    {
                        RHI::GraphicsState state;
                        state.SetPipeline(data.pipeline).SetFramebuffer(resources.Framebuffer())
                            .SetViewport(RasterViewport(data.surface.rect))
                            .AddBindingSet(view).AddBindingSet(surfaceBindings).AddBindingSet(lighting);
                        commands.SetGraphicsState(state);
                        commands.Draw(RHI::DrawArguments().SetVertexCount(3).SetInstanceCount(1));
                    }
                }
                return std::nullopt;
            }, BindDeferredLighting)));
    }

    /// @brief 沿 SurfaceAux 的批次次序重画已覆盖的颜色. 读取最终 D32, 保留其它像素的核心色或底色.
    static PassId AddOpaqueForwardExtensions(RenderGraph &graph, SceneGraphResources &versions,
        const Detail::ForwardFrameData &frame, const CoreGBufferOutput &surface, TextureId &color,
        TextureId shadowDepth, const Detail::FroxelGraphInputs *froxels)
    {
        if (!frame.HasAuxiliarySurface())
        {
            return {};
        }
        RasterReads reads;
        auto raster = RasterInputs(graph, versions, frame, surface.rect, reads, &frame, shadowDepth, froxels,
                                   RasterSurfaces::Auxiliary);
        return TakeGraph(graph.AddPass<SceneRaster>("Forward opaque extensions",
            [&](PassBuilder &builder, SceneRaster &data)
            {
                data = std::move(raster);
                if (!DeclareRasterReads(builder, reads) || (data.froxels && !data.froxels->DeclareReads(builder)) ||
                    !builder.Read(surface.depth, GraphUsage::DepthStencilRead, OpaqueDepthRange))
                {
                    return false;
                }
                color = builder.Write(color, GraphUsage::ColorAttachment, {.load = LoadOp::Load});
                return color.IsValid();
            }, DrawScene, PrepareRasterBindings));
    }

    /// @brief 前向深度与附加表面独立成版. 只有真实的早期深度消费者需要前置通道.
    static void PrepareForwardSurface(RenderGraph &graph, SceneGraphResources &resources,
                                      const Detail::ForwardFrameData &frame, ForwardOutput &output)
    {
        if (frame.needs.coreGBuffer)
        {
            output.coreGBuffer = PrepareDeferredSurface(graph, resources, frame);
        }
        const auto extent = frame.source.layout.outputExtent;
        output.depth = TakeGraph(graph.CreateTexture({.width = extent.width, .height = extent.height,
            .format = DepthFormat, .debugName = "Forward reverse depth", .isRenderTarget = true}));
        if (frame.needs.NeedsEarlyDepth())
        {
            output.depthPass = AddGeometryPass(graph, resources, frame, output.depth, nullptr, false);
        }
    }

    /// @brief Deferred 屏幕光照和辅助几何共用同一份末版 D32.
    static void PrepareDeferredOpaqueSurface(RenderGraph &graph, SceneGraphResources &resources,
                                             const Detail::ForwardFrameData &frame, ForwardOutput &output)
    {
        output.coreGBuffer = PrepareDeferredSurface(graph, resources, frame);
        output.depth = output.coreGBuffer->depth;
    }

    /// @brief 全灯和 Froxel 颜色共用几何入口, 按选定角色取得相应绑定.
    static void ShadeForwardOpaque(RenderGraph &graph, SceneGraphResources &resources,
        const Detail::ForwardFrameData &frame, ForwardOutput &output, const Detail::FroxelGraphInputs *froxels)
    {
        output.colorPass = AddGeometryPass(graph, resources, frame, output.depth, &output.workingColor,
                                          frame.needs.NeedsEarlyDepth(), output.shadowDepth, froxels);
    }

    /// @brief 核心屏幕颜色后接续只读等深辅助绘制. 最终 D32 继续属于表面生产者.
    static void ShadeDeferredView(RenderGraph &graph, SceneGraphResources &resources,
        const Detail::ForwardFrameData &frame, ForwardOutput &output, const Detail::FroxelGraphInputs *froxels)
    {
        const auto &surface = *output.coreGBuffer;
        ShadeDeferredOpaque(graph, resources, frame, surface, output.workingColor, output.shadowDepth, froxels);
        output.colorPass = AddOpaqueForwardExtensions(graph, resources, frame, surface, output.workingColor,
                                                      output.shadowDepth, froxels);
    }

    /// @brief 由同一个已选路径定位配对阶段. 天空和显示只消费它们交付的工作色和深度.
    struct OpaqueStrategy
    {
        decltype(&PrepareForwardSurface) prepareSurface;
        decltype(&ShadeForwardOpaque) shadeOpaque;
    };

    static constexpr std::array OpaqueStrategies{
        OpaqueStrategy{PrepareForwardSurface, ShadeForwardOpaque},
        OpaqueStrategy{PrepareForwardSurface, ShadeForwardOpaque},
        OpaqueStrategy{PrepareDeferredOpaqueSurface, ShadeDeferredView},
    };
    static_assert(OpaqueStrategies.size() == std::size_t(OpaquePath::Deferred) + 1);

    /// @brief 声明一个 View 的表面与显示目标, 私有输出不进入共享驻留映射.
    static ForwardOutput DeclareView(RenderGraph &graph, SceneGraphResources &versions,
                                     std::vector<GraphUpload> &uploads, const Detail::ForwardFrameData &frame,
                                     TextureId lut, const Detail::FroxelGraphInputs *froxels)
    {
        const auto extent = frame.source.layout.outputExtent;
        const auto firstPass = graph.Passes().size();
        ForwardOutput output;
        output.source = frame.source;
        if (frame.needs.surfaceOnly)
        {
            // 表面消费者以同一最终深度及表面写入范围交付, 无需声明颜色或显示阶段.
            PrepareDeferredOpaqueSurface(graph, versions, frame, output);
            output.firstPass = graph.Passes()[firstPass].id;
            output.lastPass = output.coreGBuffer->producer;
            return output;
        }
        const auto &strategy = OpaqueStrategies[std::size_t(frame.source.opaquePath)];
        strategy.prepareSurface(graph, versions, frame, output);
        output.workingColor = TakeGraph(graph.CreateTexture({.width = extent.width, .height = extent.height,
            .format = WorkingFormat, .debugName = "Opaque linear working color", .isRenderTarget = true}));
        if (frame.shadow)
        {
            output.shadowDepth = DeclareShadowDepth(graph, versions, *frame.shadow);
            output.shadowBytes = frame.shadow->depth.LogicalBytes();
        }
        strategy.shadeOpaque(graph, versions, frame, output, froxels);
        AddSky(graph, versions, frame, output.workingColor);
        // Deferred 可以只有核心光照而没有辅助重画. 用声明区间的终点覆盖两种情况及可选天空.
        output.lastWorkingPass = graph.Passes().back().id;
        // 后处理只读天空合成后的版本. 比较或读回仍可在同图消费原工作色和 D32, 不必从显示色反推.
        const auto first = uploads.size();
        output.displayColor = TakeGraph(frame.display.Declare(graph, uploads, output.workingColor,
                                                              frame.display.UsesLut() ? lut : TextureId{}));
        for (auto &entry : std::span(uploads).subspan(first))
        {
            entry.source = GraphUploadSource::ForwardView;
        }
        output.firstPass = graph.Passes()[firstPass].id;
        output.lastPass = graph.Passes().back().id;
        return output;
    }

    GraphResult<ForwardGraph> ForwardFrame::Declare(RenderGraph &graph, std::vector<GraphUpload> &uploads,
        const std::array<bool, ForwardViewCount> &selected)
    try
    {
        assert(m_Data && !m_Data->uploads.sealed);
        for (std::size_t index = 0; index < selected.size(); ++index)
        {
            if (!selected[index])
            {
                // 撤回未选路的准备代. 几何等共享写入只在其它路仍使用它时保留, 显示预留另行结算.
                DiscardViewUploads(*m_Data, index);
            }
        }
        m_Data->uploads.sealed = true;
        ForwardGraph output;
        TextureId lut;
        for (std::size_t index = 0; index < selected.size(); ++index)
        {
            const auto &display = m_Data->views[index].display;
            if (selected[index] && display.UsesLut())
            {
                // 两个物理 View 的同一 atlas 只导入一次. 纹理写必须排在场景缓冲之前.
                const auto first = uploads.size();
                lut = TakeGraph(display.DeclareLut(graph, uploads));
                if (uploads.size() != first)
                {
                    uploads.back().source = GraphUploadSource::ForwardStatic;
                    output.m_LutWrite = uploads.back().pass;
                    output.m_LutView = index;
                }
                break;
            }
        }
        // 生产版本只建立一次. 逐路声明读取时沿用同一映射, 因而存活的任一路都能保住共享上传.
        auto versions = DeclareUploads(graph, *m_Data, selected, uploads, output.m_StorageWrites,
                                       output.m_TableWrites, output.m_ViewUploadPasses);
        for (std::size_t index = 0; index < selected.size(); ++index)
        {
            if (selected[index])
            {
                const auto &view = m_Data->views[index];
                const auto froxels = view.froxelUpload
                    ? std::optional{TakeGraph(view.froxelUpload->Resolve(m_Data->uploads, versions.buffers))} : std::nullopt;
                output.views[index] = DeclareView(graph, versions, uploads, view, lut, froxels ? &*froxels : nullptr);
            }
        }
        return output;
    }
    catch (const GraphError &failure)
    {
        return failure;
    }

    ViewId ForwardGraph::PassOwner(PassId pass) const noexcept
    {
        if (pass.IsValid())
        {
            for (std::size_t index = 0; index < views.size(); ++index)
            {
                const auto &output = views[index];
                const auto &writes = m_ViewUploadPasses[index];
                if (output && (output->OwnsPass(pass) || std::ranges::find(writes, pass) != writes.end()))
                {
                    return output->source.view;
                }
            }
        }
        return {};
    }

    void ForwardFrame::Confirm(const ForwardGraph &output, const CompiledGraphPlan &plan) noexcept
    {
        assert(m_Data);
        if (IsLivePass(plan, output.m_LutWrite))
        {
            m_Data->views[output.m_LutView].display.Confirm();
        }
        for (const auto &[page, write] : output.m_TableWrites)
        {
            if (IsLivePass(plan, write))
            {
                page->initialized = true;
            }
        }
        // 一代几何须同时确认顶点和索引, 一个 cube 须确认所有面和 mip.
        // 材质行与其引用的纹理分别拥有写入记录, 各自依据本图的存活 Pass 取得确认.
        for (const auto &[storage, writes] : output.m_StorageWrites)
        {
            if (std::ranges::all_of(writes, [&](PassId pass) { return IsLivePass(plan, pass); }))
            {
                storage->accepted = true;
            }
        }
        for (auto &view : m_Data->views)
        {
            if (view.deferredViewWriteRecorded)
            {
                const auto write = output.m_StorageWrites.find(view.deferredView.get());
                if (write != output.m_StorageWrites.end() && IsLivePass(plan, write->second.front()))
                {
                    // 屏幕常量只有这一笔生产写入. 以完整图的回执更新安全槽可复用的字节身份.
                    const auto values = std::as_bytes(std::span(&*view.deferredViewValues, 1));
                    auto &contents = view.deferredView->contents;
                    contents.resize(values.size());
                    std::ranges::copy(values, contents.begin());
                    view.deferredViewWriteRecorded = false;
                }
            }
        }
    }
}
