#include "Forward.hpp"

#include <algorithm>
#include <cassert>
#include <new>
#include <utility>

#include "../Materials/StaticGpu.hpp"

/// @file
/// @brief 在同一图登记共享上传和各 View 的几何、天空与显示转换.
/// 回调只访问已声明版本, 提交后按每个存储代的实际生产者确认上传.

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
        ForwardEncodingCounts counts;
        if (m_Data)
        {
            for (const auto &view : m_Data->views)
            {
                counts.objects += view.encodings.objects;
                counts.materials += view.encodings.materials;
                counts.lights += view.encodings.lights;
            }
        }
        return counts;
    }

    std::pair<std::size_t, std::size_t> ForwardFrame::UploadDemand() const noexcept
    {
        return m_Data ? std::pair{m_Data->uploads.writes, m_Data->uploads.bytes} : std::pair<std::size_t, std::size_t>{};
    }

    /// @brief 解除本轮着色引用. 槽内保留 View 行, 普通灯表和批次缓冲, 下轮准备时再比较其内容.
    static void ResetView(Detail::ForwardFrameData &view) noexcept
    {
        view.batches.clear();
        view.draws.clear();
        view.materialRows.clear();
        view.activeLights.reset();
        view.lightLayout = nullptr;
        view.environment = {};
        view.display = {};
        view.degradation.reset();
    }

    void ForwardFrame::Reset() noexcept
    {
        if (m_Data)
        {
            m_Data->uploads.Clear();
            for (auto &view : m_Data->views)
            {
                ResetView(view);
                view.encodings = {};
            }
            m_Data->begun = m_Data->lutReserved = false;
            m_Data->hasPreparedView = false;
            m_Data->planned.reset();
            m_Data->shaders.reset();
        }
    }

    std::uint64_t ForwardFrame::ShaderGeneration() const noexcept
    {
        return m_Data && m_Data->shaders ? m_Data->shaders->generation : 0;
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
        return 4096 * m_Data->views.size() + m_Data->uploads.writes * 256;
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

    /// @brief 已有计划可能依赖共享生产者. 只在没有后续消费者时丢弃批次并归还写入额度.
    static void RejectView(Detail::ForwardFrameStorage &frame, std::size_t index) noexcept
    {
        ResetView(frame.views[index]);
        frame.planned.reset(index);
        // 另一路可能已经登记了本批写入, 尚在等待 FinishView. 保留它所依赖的上传载荷和准备代.
        if (!frame.hasPreparedView && frame.planned.none())
        {
            // 这批写入已失去消费者. 下路重新 Begin, 使设备缓存能识别只在旧批次中拥有生产者的代.
            frame.uploads.Clear();
            frame.begun = frame.lutReserved = false;
            frame.shaders.reset();
        }
    }

    // 选代前取得显示输入并预留写入额度. 两路共享的 LUT 只计一次, 实际上传由使用它的图声明.
    static void PlanDisplay(Detail::ForwardDevice &device, Detail::ForwardFrameStorage &frame,
                            PostProcess::DisplayFrame &display, const ViewLayouts &layouts, DisplaySettings settings)
    {
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
            const auto lutBytes = display.LutUploadBytes();
            const bool sharedLut = lutBytes != 0 && frame.lutReserved;
            frame.uploads.ReserveWrites(display.UploadBytes() - (sharedLut ? lutBytes : 0),
                                        display.UploadCount() - std::size_t(sharedLut));
            frame.lutReserved |= lutBytes != 0;
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
    }

    std::optional<ForwardFailure> ForwardRenderer::Prepare(ForwardFrame &frame, const PreparedView &view,
        PreviewMode mode, ViewResourceNeeds needs, std::size_t index)
    {
        if (auto failure = PlanView(frame, view, mode, needs, index))
        {
            return failure;
        }
        return FinishView(frame, view, index);
    }

    std::optional<ForwardFailure> ForwardRenderer::PlanView(ForwardFrame &frame, const PreparedView &view,
        PreviewMode mode, ViewResourceNeeds needs, std::size_t index)
    try
    {
        assert(view.camera && view.reason == ViewSettingReason::None && index < ForwardViewCount);
        const auto display = ResolveDisplaySettings(mode, view.requestedAa);
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
        data.source = {view.view, view.scene.scene, view.scene.contentRevision, mode, *view.camera, view.layouts.render};
        data.needs = needs;
        m_Resources->BeginView();
        Detail::PlanDraws(*m_Resources, data, view, mode);
        PlanDisplay(*m_Resources, *frame.m_Data, data.display, view.layouts, display);
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

    std::optional<ForwardFailure> ForwardRenderer::FinishView(ForwardFrame &frame, const PreparedView &view,
                                                              std::size_t index)
    try
    {
        assert(m_Resources && frame.m_Data && frame.m_Data->planned[index]);
        auto &data = frame.m_Data->views[index];
        assert(data.source.view == view.view);
        if (!frame.m_Data->shaders)
        {
            // 同一帧的两路 View 固定使用这张表, 直至槽安全 Reset.
            frame.m_Data->shaders = m_Resources->activeShaders;
        }
        assert(frame.m_Data->shaders == m_Resources->activeShaders);
        const auto display = ResolveDisplaySettings(data.source.mode, view.requestedAa);
        if (!m_Resources->RequestDisplay(view.layouts, display))
        {
            Detail::Reject(ViewSettingReason::ShaderPending, "Waiting for prepared display pipelines");
        }
        Detail::ApplyDrawShaders(*m_Resources, data);
        m_Resources->display.BindStages(data.display, frame.m_Data->shaders->display, display);
        frame.m_Data->hasPreparedView = true;
        frame.m_Data->planned.reset(index);
        return std::nullopt;
    }
    catch (const ForwardFailure &failure)
    {
        RejectView(*frame.m_Data, index);
        return failure;
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
            for (const auto &draw : view.draws)
            {
                consumers[draw.geometry.get()] |= 1u << index;
                consumers[draw.object.get()] |= 1u << index;
                consumers[view.materialRows[draw.material].get()] |= 1u << index;
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
                                               std::vector<std::pair<Detail::BufferPage *, PassId>> &tableWrites)
    {
        const auto &batch = frame.uploads;
        SceneGraphResources resources;
        // 一代几何可有两块缓冲, 一个 cube 可有多个面和 mip. 确认时须能逐笔找到同一代的写入.
        const auto record = [&](Detail::Allocation *storage, std::size_t first, GraphUploadSource source)
        {
            auto &passes = storageWrites[storage];
            for (auto &write : std::span(uploads).subspan(first))
            {
                write.source = source;
                passes.push_back(write.pass);
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
            record(upload.revision.get(), first, upload.source);
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
    /// lights 存在时携带本图授权的版本和范围, lightLayout 借用本槽保有的颜色布局.
    struct SceneRaster
    {
        RHI::ViewportState viewport;
        RHI::BindingSet *view = nullptr;
        std::optional<BufferAccess> lights;
        RHI::BindingLayout *lightLayout = nullptr;
        std::optional<BufferAccess> environment;
        std::optional<TextureAccess> dfg;
        std::optional<TextureAccess> reflection;
        RHI::Sampler *environmentSampler = nullptr;
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

    /// @brief 将预解析的绑定关联到本图授权. 同一 Pass 的重复资源只声明一次, Depth 只取基色槽.
    static SceneRaster RasterInputs(RenderGraph &graph, SceneGraphResources &versions,
                                    const Detail::ForwardFrameData &frame, bool depthOnly, RasterReads &reads)
    {
        SceneRaster raster;
        raster.viewport = RasterViewport(frame.source.layout.rect);
        if (frame.draws.empty())
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
        static_cast<void>(buffer(frame.view->buffer, GraphUsage::ConstantBuffer));
        raster.view = frame.view->binding.Get();
        if (!depthOnly)
        {
            raster.lights = buffer(frame.activeLights->buffer, GraphUsage::ShaderRead);
            raster.lightLayout = frame.lightLayout.Get();
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
        for (const auto &draw : frame.draws)
        {
            static_cast<void>(slice(draw.geometry->vertices));
            static_cast<void>(slice(draw.geometry->indices));
            table(draw.object->storage);
            table(frame.materialRows[draw.material]->storage);
        }
        constexpr auto commandBytes = sizeof(RHI::DrawIndexedIndirectArguments);
        raster.batches.reserve(frame.batches.size());
        for (const auto &batch : frame.batches)
        {
            const auto &draw = frame.draws[batch.firstDraw];
            const auto &material = *frame.materialRows[draw.material];
            static_cast<void>(buffer(frame.instanceBlocks[batch.instanceBlock]->buffer, GraphUsage::ShaderRead));
            const auto slots = depthOnly ? 1 : material.textures.size();
            for (std::size_t i = 0; i < slots; ++i)
            {
                const auto &product = *material.textures[i];
                const auto version = versions.ReadTexture(graph, product);
                reads.textures.try_emplace(version.Identity().index,
                    TextureAccess{version, {.firstMip = product.sampling.baseMipLevel,
                        .mipCount = product.sampling.numMipLevels}, GraphUsage::Sampled});
            }
            const auto arguments = readBuffer(frame.indirect->buffer, GraphUsage::IndirectArgument,
                {batch.firstCommand * commandBytes, batch.commandCount * commandBytes}, nullptr);
            raster.batches.push_back({slice(draw.geometry->vertices), slice(draw.geometry->indices), arguments,
                batch.renderables.Get(), material.binding->Get(),
                (depthOnly ? draw.depthPipeline : draw.pipeline).Get(), batch.commandCount});
        }
        return raster;
    }

    /// @brief 执行只使用图授权的几何、桥接附件和现成集合. RHI 对集合里的活动资源检查实际范围.
    static GraphFailure DrawScene(PassResources &resources, const SceneRaster &raster,
                                  RHI::CommandList &commands) noexcept
    {
        auto *lights = raster.lights ? resources.BindingSet(0) : nullptr;
        if (raster.lights && !lights)
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
                .AddBindingSet(raster.view).AddBindingSet(batch.renderables).AddBindingSet(batch.material)
                .AddVertexBuffer(RHI::VertexBufferBinding().SetBuffer(vertices).SetOffset(0))
                .SetIndexBuffer(RHI::IndexBufferBinding().SetBuffer(indices).SetFormat(RHI::Format::R32_UINT)
                    .SetOffset(0)).SetIndirectParams(arguments);
            if (lights)
            {
                state.AddBindingSet(lights);
            }
            commands.SetGraphicsState(state);
            commands.DrawIndexedIndirect(static_cast<std::uint32_t>(batch.arguments.range.byteOffset),
                                          batch.commandCount);
        }
        return std::nullopt;
    }

    /// @brief 深度和颜色共享 Draw 顺序及声明规则. 读前置深度时使用只读附件, 从而确定 WAW/RAW 关系.
    static PassId AddGeometryPass(RenderGraph &graph, SceneGraphResources &versions,
        const Detail::ForwardFrameData &frame, TextureId &depth, TextureId *color, bool readOnlyDepth)
    {
        RasterReads reads;
        auto raster = RasterInputs(graph, versions, frame, !color, reads);
        return TakeGraph(graph.AddPass<SceneRaster>(color ? "Forward ShadeOpaque" : "Forward PrepareSurface",
            [&](PassBuilder &builder, auto &data)
            {
                data = std::move(raster);
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
            }, DrawScene, +[](PassPreparation &preparation, const SceneRaster &data) -> GraphFailure
            {
                if (!data.lights)
                {
                    return std::nullopt;
                }
                // 沿生成集合枚举实际使用的字段, 读取必须来自 setup 给出的同一图权限.
                std::array<RHI::BindingSetItem, 5> items;
                std::size_t count = 0;
                for (const auto &slot : data.lightLayout->GetDesc()->bindings)
                {
                    std::optional<RHI::BindingSetItem> binding;
                    if (slot.type == RHI::ResourceType::StructuredBuffer_SRV)
                    {
                        const auto &access = *data.lights;
                        binding = preparation.ReadBuffer(slot.slot, access.resource, access.usage, access.range);
                    }
                    else if (slot.type == RHI::ResourceType::ConstantBuffer)
                    {
                        const auto &access = *data.environment;
                        binding = preparation.ReadBuffer(slot.slot, access.resource, access.usage, access.range);
                    }
                    else if (slot.type == RHI::ResourceType::Sampler)
                    {
                        binding = RHI::BindingSetItem::Sampler(slot.slot, data.environmentSampler);
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
                    items[count++] = *binding;
                }
                static_cast<void>(preparation.CreateBindingSet(*data.lightLayout, std::span(items).first(count)));
                return std::nullopt;
            }));
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
                            TextureId &depth, TextureId &color)
    {
        return AddGeometryPass(graph, resources, frame, depth, &color, frame.needs.earlyDepth);
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
                                     TextureId lut)
    {
        const auto extent = frame.source.layout.outputExtent;
        ForwardOutput output;
        output.source = frame.source;
        output.depth = TakeGraph(graph.CreateTexture({.width = extent.width, .height = extent.height,
            .format = DepthFormat, .debugName = "Forward reverse depth", .isRenderTarget = true}));
        output.workingColor = TakeGraph(graph.CreateTexture({.width = extent.width, .height = extent.height,
            .format = WorkingFormat, .debugName = "Forward linear working color", .isRenderTarget = true}));
        output.depthPass = PrepareSurface(graph, versions, frame, output.depth);
        output.colorPass = ShadeOpaque(graph, versions, frame, output.depth, output.workingColor);
        AddSky(graph, versions, frame, output.workingColor);
        // 后处理只读天空合成后的版本. 比较或读回仍可在同图消费原工作色和 D32, 不必从显示色反推.
        const auto first = uploads.size();
        output.displayColor = TakeGraph(frame.display.Declare(graph, uploads, output.workingColor,
                                                              frame.display.UsesLut() ? lut : TextureId{}));
        for (auto &entry : std::span(uploads).subspan(first))
        {
            entry.source = GraphUploadSource::ForwardView;
        }
        return output;
    }

    GraphResult<ForwardGraph> ForwardFrame::Declare(RenderGraph &graph, std::vector<GraphUpload> &uploads,
        const std::array<bool, ForwardViewCount> &selected)
    try
    {
        assert(m_Data && !m_Data->uploads.sealed);
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
        auto versions = DeclareUploads(graph, *m_Data, selected, uploads, output.m_StorageWrites, output.m_TableWrites);
        for (std::size_t index = 0; index < selected.size(); ++index)
        {
            if (selected[index])
            {
                output.views[index] = DeclareView(graph, versions, uploads, m_Data->views[index], lut);
            }
        }
        return output;
    }
    catch (const GraphError &failure)
    {
        return failure;
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
