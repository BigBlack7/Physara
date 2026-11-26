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
/// Forward+ 的颜色 Pass 从本图上传版本取得所属 View 的 Froxel 数据, 后继天空和显示沿用同一工作色.

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
        ForwardPassWork work{view.batches.size(), 0};
        for (const auto &batch : view.batches)
        {
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

    bool ForwardFrame::AddsFroxels(std::size_t index) const noexcept
    {
        const auto &view = m_Data->views[index];
        return view.usesLighting && view.alternatePath == OpaquePath::ForwardPlus && !UsesFroxels(index);
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
        view.lightLayout = nullptr;
        view.froxelViewLayout = nullptr;
        view.froxelUpload.reset();
        view.froxelReserved = false;
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
        OpaqueProduction production{.color = WorkingFormat, .depth = DepthFormat};
        const auto store = [&](std::size_t slot, Shader::ProductShader role)
        {
            const auto content = programs.ProgramContent(role);
            auto &digest = production.programContents[slot];
            assert(content.size() == digest.size());
            std::ranges::copy(content, digest.begin());
        };
        // 顶点布局和绕序选择同一角色的不同 Pipeline 槽. 历史比较使用角色的包摘要.
        if (!view.batches.empty())
        {
            const auto key = Detail::GeometryRequest(0, false, false, Detail::SurfacePhase::Combined,
                                                     view.environment.program);
            store(0, key.role);
            if (view.needs.earlyDepth)
            {
                store(2, Shader::ProductShader::StaticDepth);
            }
        }
        if (view.environment.sky)
        {
            store(1, Shader::ProductShader::ForwardSky);
        }
        if (view.shadow && view.shadow->depth.variants.any())
        {
            store(3, Shader::ProductShader::StaticShadowDepth);
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
        DisplaySettings display, bool resample) const noexcept
    {
        if (render.width == 0 || render.height == 0 || output.width == 0 || output.height == 0)
        {
            return ForwardFailure{ViewSettingReason::InvalidExtent, Core::BoundedText("Forward extent is empty")};
        }
        const auto limit = std::min(m_MaxAttachmentSize, m_MaxTextureSize);
        if (std::max({render.width, render.height, output.width, output.height}) > limit)
        {
            return ForwardFailure{ViewSettingReason::SceneAttachmentLimit,
                                  Core::BoundedText("Forward extent exceeds the attachment limit")};
        }
        const bool fxaa = display.antiAliasing == AntiAliasing::Fxaa;
        // RGBA16F 工作色和 D32 每个渲染像素合计 12 字节. FXAA 和重采样各需一个 8 字节的线性中间目标.
        const std::size_t renderBytesPerPixel = 12 + 8 * (std::size_t(fxaa) + std::size_t(resample));
        if (render.height > ForwardAttachmentBudget / renderBytesPerPixel / render.width)
        {
            return ForwardFailure{ViewSettingReason::SceneAttachmentLimit,
                                  Core::BoundedText("Forward attachments exceed the view byte budget")};
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
        view.froxelReserved = false;
        view.displayReserved = false;
        view.display.ReleasePayload();
        // DiscardView 已按实际载荷重算额度. 仍在等待收尾的 Froxel 写组各补计一次,
        // 显示常量按路补计, 共享 LUT 只补一份.
        bool lutReserved = false;
        for (const auto &candidate : frame.views)
        {
            if (candidate.froxelReserved)
            {
                const auto [bytes, writes] = Detail::FroxelUpload::UploadRequirements();
                frame.uploads.bytes += bytes;
                frame.uploads.writes += writes;
            }
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
        bool holdFroxelWrites)
    try
    {
        assert(view.camera && view.reason == ViewSettingReason::None && index < ForwardViewCount);
        const auto display = ResolveDisplaySettings(view.previewShadow ? PreviewMode::BaseColor : mode,
                                                    view.requestedAa);
        if (const auto failure = CheckAttachments(view.layouts.render.outputExtent, view.layouts.display.outputExtent,
                                                  display, view.layouts.render != view.layouts.display))
        {
            throw *failure;
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
            .mode = mode, .camera = *view.camera, .layout = view.layouts.render, .opaquePath = view.opaquePath};
        data.needs = needs;
        data.alternatePath = alternate;
        m_Resources->BeginView(index);
        data.preparationStamp = m_Resources->viewBudgets[index].stamp;
        Detail::PlanDraws(*m_Resources, data, view, mode, view.previewShadow);
        PlanDisplay(*m_Resources, *frame.m_Data, data, view.layouts, display);
        if (holdFroxelWrites && Detail::UsesFroxels(data.environment.program) && view.froxels)
        {
            // 有效计划保留已知的写入需求. 首次受光和额外候选在实际收尾时准入.
            const auto [bytes, writes] = Detail::FroxelUpload::UploadRequirements();
            try
            {
                frame.m_Data->uploads.ReserveWrites(bytes, writes);
                data.froxelReserved = true;
            }
            catch (const ForwardFailure &)
            {
                if (!alternate)
                {
                    throw;
                }
                // 待选全灯可在 Froxel 额度不足时独立就绪. 若仍使用旧路, 收尾尝试取得实际所需额度.
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

    /// @brief 列表准备失败仍保留全灯工作集, 由需要 Froxel 的实际策略报告首因.
    static ForwardFailure MissingFroxelLists(const PreparedView &view) noexcept
    {
        Core::BoundedText issue("Cannot prepare Forward+ Froxel grid: ");
        issue.Append(FroxelStatusName(view.froxelStatus));
        return {ViewSettingReason::ScenePreparationFailed, issue};
    }

    std::optional<ForwardFailure> ForwardRenderer::SelectOpaquePath(ForwardFrame &frame, PreparedView &view,
        View &owner, std::size_t index, const GraphBufferSupport &buffers)
    {
        auto &storage = *frame.m_Data;
        auto &data = storage.views[index];
        const auto report = [&](ViewSettingReason reason, const Core::BoundedText *detail = nullptr)
        {
            owner.ReportOpaquePreparation(view.scene.scene, view.documentRevision, reason, detail);
        };
        const bool lit = IsLitPreview(view.preview) && !view.previewShadow;
        const auto effective = owner.OpaqueSettings(view.scene.scene).effective;
        if (data.alternatePath && owner.CanSwitchOpaquePath(view, m_Resources->activeShaders->generation))
        {
            auto program = Detail::FullListProgram(data.environment.program);
            const bool froxels = data.usesLighting && *data.alternatePath == OpaquePath::ForwardPlus;
            if (froxels)
            {
                program = Detail::WithFroxels(program);
            }
            // 等待或拒绝只影响待选路径. 共用的绘制及上传仍可由已接受策略提交.
            std::optional<ForwardFailure> candidate;
            if (data.usesLighting)
            {
                candidate = Detail::CheckAlternateColors(*m_Resources, data, program);
            }
            if (!candidate && froxels)
            {
                if (!view.froxels)
                {
                    candidate = MissingFroxelLists(view);
                }
                else
                {
                    try
                    {
                        m_Resources->ResumeView(index);
                        data.froxelUpload = Detail::FroxelUpload::Prepare(*m_Resources, storage.uploads, view, buffers);
                    }
                    catch (const ForwardFailure &failure)
                    {
                        candidate = failure;
                    }
                    catch (const std::bad_alloc &)
                    {
                        candidate = ForwardFailure{ViewSettingReason::ScenePreparationFailed,
                                                    Core::BoundedText("Forward+ upload allocation failed")};
                    }
                }
            }
            if (candidate)
            {
                report(candidate->reason, &candidate->description);
            }
            else
            {
                if (data.froxelReserved)
                {
                    const auto [bytes, writes] = Detail::FroxelUpload::UploadRequirements();
                    storage.uploads.ReleaseWrites(bytes, writes);
                    data.froxelReserved = false;
                }
                // PreparePreview 和 Declare 分别从这两份记录读取路径, 必须指向同一次选择.
                view.opaquePath = data.source.opaquePath = *data.alternatePath;
                data.environment.program = program;
            }
        }
        else if (!data.alternatePath && lit && !effective)
        {
            // 没有有效计划时也可保留失败输入. 调用方撤回本路准备, 其它路继续构图.
            if (!owner.CanSwitchOpaquePath(view, m_Resources->activeShaders->generation))
            {
                const auto status = owner.OpaqueSettings(view.scene.scene);
                return ForwardFailure{status.reason, status.detail ? *status.detail :
                    Core::BoundedText("Opaque path submission was rejected")};
            }
            if (data.usesLighting)
            {
                return Detail::CheckAlternateColors(*m_Resources, data, data.environment.program);
            }
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
        assert(data.source.view == view.view && data.source.opaquePath == view.opaquePath);
        if (!frame.m_Data->shaders)
        {
            // 同一帧的两路 View 固定使用这张表, 直至槽安全 Reset.
            frame.m_Data->shaders = m_Resources->activeShaders;
        }
        assert(frame.m_Data->shaders == m_Resources->activeShaders);
        const auto displayMode = Detail::IsShadowPreview(data.environment.program) ?
            PreviewMode::BaseColor : data.source.mode;
        const auto display = ResolveDisplaySettings(displayMode, view.requestedAa);
        if (!m_Resources->RequestDisplay(view.layouts, display))
        {
            Detail::Reject(ViewSettingReason::ShaderPending, "Waiting for prepared display pipelines");
        }
        m_Resources->ResumeView(index);
        Detail::ApplyDrawShaders(*m_Resources, data, view);
        m_Resources->display.BindStages(data.display, frame.m_Data->shaders->display, display);
        if (data.froxelViewLayout && !data.froxelUpload)
        {
            if (!view.froxels)
            {
                throw MissingFroxelLists(view);
            }
            // 角色已就绪, 取用本 View 的逻辑预留, 实际写入由上传组按同样的额度登记.
            if (data.froxelReserved)
            {
                const auto [bytes, writes] = Detail::FroxelUpload::UploadRequirements();
                frame.m_Data->uploads.ReleaseWrites(bytes, writes);
                data.froxelReserved = false;
            }
            data.froxelUpload = Detail::FroxelUpload::Prepare(*m_Resources, frame.m_Data->uploads, view, buffers);
            assert(data.froxelUpload);
        }
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
            version = TakeGraph(AddBufferUpload(graph, version, upload.payload, uploads, upload.offset));
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

    /// @brief 一个几何 Pass 的图像矩形, View 集合与 Draw 顺序.
    /// lightLayout 借用本槽的颜色布局. shadowRows 按物理层索引, 普通受光另用 shadowReferences 从灯行找到组首层.
    struct SceneRaster
    {
        RHI::ViewportState viewport;
        RHI::BindingSet *view = nullptr;
        RHI::BindingLayout *froxelViewLayout = nullptr;
        std::optional<Detail::FroxelGraphInputs> froxels;
        BufferAccess viewRow{}; ///< 本图的 b0. Froxel 颜色在预检时与四个块合为一组.
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
        std::vector<GraphBatch> batches;
    };

    /// @brief 同一 Pass 按资源及范围去重, 同页上不同几何或材质区间仍各自保留.
    /// 表键使用本图的存储索引, 值内的 BufferUse 和 TextureAccess 仍携带相应内容版本.
    struct RasterReads
    {
        std::map<std::tuple<std::uint32_t, std::uint64_t, std::uint64_t, GraphUsage>, BufferUse> buffers;
        std::map<std::uint32_t, TextureAccess> textures;
    };

    /// @brief 从有效图像矩形建立光栅视口及同边界的裁剪.
    static RHI::ViewportState RasterViewport(const ViewImageRect &rect)
    {
        RHI::ViewportState state;
        state.AddViewportAndScissorRect(RHI::Viewport(static_cast<float>(rect.x),
            static_cast<float>(rect.x + rect.extent.width), static_cast<float>(rect.y),
            static_cast<float>(rect.y + rect.extent.height), 0.f, 1.f));
        return state;
    }

    /// @brief 将预解析的 Draw 绑定关联到本 Pass 的读范围. 主深度, 阴影深度和阴影诊断只取材质基色槽.
    /// color 指向主着色工作时才选用 set 3. 阴影附件使用自己的方形 viewport.
    static SceneRaster RasterInputs(RenderGraph &graph, SceneGraphResources &versions,
        const Detail::StaticDrawWork &work, const ViewImageRect &rect, RasterReads &reads,
        const Detail::ForwardFrameData *color = nullptr, TextureId shadowDepth = {},
        const Detail::FroxelGraphInputs *froxels = nullptr)
    {
        SceneRaster raster;
        raster.viewport = RasterViewport(rect);
        if (work.draws.empty())
        {
            return raster;
        }
        const auto readBuffer = [&](const RHI::BufferHandle &handle, GraphUsage usage, RHI::BufferRange range,
                                    const Detail::BufferPage *page,
                                    std::optional<RHI::BufferRange> indexedBinding = std::nullopt)
        {
            BufferAccess access{versions.BufferVersion(graph, handle, page, true), range, usage};
            reads.buffers.try_emplace(std::tuple{access.resource.Identity().index, range.byteOffset, range.byteSize,
                                                 usage}, BufferUse{access, std::nullopt, indexedBinding});
            return access;
        };
        const auto buffer = [&](const RHI::BufferHandle &handle, GraphUsage usage)
        {
            return readBuffer(handle, usage, {0, handle->GetDesc().byteSize}, nullptr);
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
        raster.viewRow = buffer(work.view->buffer, GraphUsage::ConstantBuffer);
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
            raster.lightLayout = color->lightLayout.Get();
        }
        const bool preview = color && Detail::IsShadowPreview(color->environment.program);
        if (color && color->shadow)
        {
            const auto &shadow = *color->shadow;
            raster.shadowConstants = buffer(shadow.constants->buffer, GraphUsage::ConstantBuffer);
            raster.shadowRows = buffer(shadow.layerRows->buffer, GraphUsage::ShaderRead);
            if (!preview)
            {
                raster.shadowReferences = buffer(shadow.references->buffer, GraphUsage::ShaderRead);
            }
            const TextureAccess access{shadowDepth, {.layerCount = shadow.depth.count, .aspect = TextureAspect::Depth},
                                       GraphUsage::Sampled};
            raster.shadowImage = access;
            reads.textures.try_emplace(shadowDepth.Identity().index, access);
        }
        if (color && !preview)
        {
            raster.lights = buffer(color->activeLights->buffer, GraphUsage::ShaderRead);
            const auto &environment = color->environment;
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
                raster.dfg = sample(*environment.dfg);
                raster.environmentSampler = environment.sampler.Get();
            }
            if (environment.reflection)
            {
                raster.reflection = sample(*environment.reflection);
                raster.environment = buffer(environment.lightingConstants->buffer, GraphUsage::ConstantBuffer);
            }
        }
        // 同一批的实例可以选取不同对象行和材质行. 逐表面登记这些行及几何区间, 重复的读由 readBuffer 合并.
        for (const auto &draw : work.draws)
        {
            static_cast<void>(slice(draw.geometry->vertices));
            static_cast<void>(slice(draw.geometry->indices));
            table(draw.object->storage);
            table(work.materialRows[draw.material]->storage);
        }
        constexpr auto commandBytes = sizeof(RHI::DrawIndexedIndirectArguments);
        raster.batches.reserve(work.batches.size());
        for (const auto &batch : work.batches)
        {
            const auto &draw = work.draws[batch.firstDraw];
            const auto &material = *work.materialRows[draw.material];
            static_cast<void>(buffer(work.instanceBlocks[batch.instanceBlock]->buffer, GraphUsage::ShaderRead));
            const auto slots = !color || preview ? Detail::CoverageTextureCount : material.textures.size();
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
                (color ? draw.pipeline : draw.depthPipeline).Get(), batch.commandCount});
        }
        return raster;
    }

    /// @brief 执行只使用图授权的几何、桥接附件和现成集合. RHI 对集合里的活动资源检查实际范围.
    static GraphFailure DrawScene(PassResources &resources, const SceneRaster &raster,
                                  RHI::CommandList &commands) noexcept
    {
        auto *viewBindings = raster.froxels ? resources.BindingSet(0) : raster.view;
        auto *passBindings = raster.lightLayout ? resources.BindingSet(raster.froxels ? 1 : 0) : nullptr;
        if ((raster.froxels && !viewBindings) || (raster.lightLayout && !passBindings))
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

    /// @brief 生成诊断或受光 Pass 所需的集合. 每个纹理视图都限于本 Pass 声明的范围.
    static GraphFailure PrepareRasterBindings(PassPreparation &preparation, const SceneRaster &data)
    {
        // 预检按创建顺序保存集合. Froxel 颜色先保存 set 0, 再保存 set 3. 普通颜色直接借用已有的 set 0.
        if (data.froxels)
        {
            const auto constants = preparation.ReadBuffer(0, data.viewRow.resource, data.viewRow.usage, data.viewRow.range);
            const auto froxelItems = data.froxels->Bind(preparation);
            if (!constants || !froxelItems)
            {
                return std::nullopt;
            }
            const std::array items{*constants, (*froxelItems)[0], (*froxelItems)[1], (*froxelItems)[2], (*froxelItems)[3]};
            if (!preparation.CreateBindingSet(*data.froxelViewLayout, items))
            {
                return std::nullopt;
            }
        }
        if (!data.lightLayout)
        {
            return std::nullopt;
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
                return std::nullopt;
            }
            assert(count < items.size());
            items[count++] = *binding;
        }
        static_cast<void>(preparation.CreateBindingSet(*data.lightLayout, std::span(items).first(count)));
        return std::nullopt;
    }

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
                         {.load = LoadOp::Clear, .clearValue = RHI::Color(0.018f, 0.018f, 0.018f, 0.f)});
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

    /// @brief 只有请求真实早期深度的下游才新增几何 Pass.
    static PassId PrepareSurface(RenderGraph &graph, SceneGraphResources &resources,
                               const Detail::ForwardFrameData &frame, TextureId &depth)
    {
        if (frame.needs.earlyDepth)
        {
            return AddGeometryPass(graph, resources, frame, depth, nullptr, false);
        }
        return {};
    }

    /// @brief 按准备的路径同时产生颜色和深度, 或读取早期深度并只写颜色.
    static PassId ShadeOpaque(RenderGraph &graph, SceneGraphResources &resources, const Detail::ForwardFrameData &frame,
        TextureId &depth, TextureId &color, TextureId shadowDepth, const Detail::FroxelGraphInputs *froxels)
    {
        return AddGeometryPass(graph, resources, frame, depth, &color, frame.needs.earlyDepth, shadowDepth, froxels);
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

    /// @brief 声明一个 View 的表面与显示目标, 私有输出不进入共享驻留映射.
    static ForwardOutput DeclareView(RenderGraph &graph, SceneGraphResources &versions,
                                     std::vector<GraphUpload> &uploads, const Detail::ForwardFrameData &frame,
                                     TextureId lut, const Detail::FroxelGraphInputs *froxels)
    {
        const auto extent = frame.source.layout.outputExtent;
        const auto firstPass = graph.Passes().size();
        ForwardOutput output;
        output.source = frame.source;
        output.depth = TakeGraph(graph.CreateTexture({.width = extent.width, .height = extent.height,
            .format = DepthFormat, .debugName = "Forward reverse depth", .isRenderTarget = true}));
        output.workingColor = TakeGraph(graph.CreateTexture({.width = extent.width, .height = extent.height,
            .format = WorkingFormat, .debugName = "Forward linear working color", .isRenderTarget = true}));
        if (frame.shadow)
        {
            output.shadowDepth = DeclareShadowDepth(graph, versions, *frame.shadow);
            output.shadowBytes = frame.shadow->depth.LogicalBytes();
        }
        output.depthPass = PrepareSurface(graph, versions, frame, output.depth);
        output.colorPass = ShadeOpaque(graph, versions, frame, output.depth, output.workingColor,
                                       output.shadowDepth, froxels);
        AddSky(graph, versions, frame, output.workingColor);
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
    }
}
