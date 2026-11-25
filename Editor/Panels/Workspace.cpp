#include "Workspace.hpp"

#include <algorithm>
#include <charconv>
#include <string>
#include <string_view>
#include <variant>

#include <imgui.h>
#include <imgui_internal.h>

#include "PhysicalInspector.hpp"
#include "Session/ActionRouter.hpp"

/// @file
/// @brief 在主窗口绘制停靠面板, 收集手势, 菜单及快捷键动作.
/// 编辑意图交给 EditorSession, 标题和 VSync 请求随本轮结果交给 Runtime.

namespace Physara::Editor::Detail
{
    /// @brief 面板菜单项及固定的 ImGui 窗口身份.
    struct Panel
    {
        ActionId action;
        const char *menu;
        const char *window;
        const char *shortcut;
    };

    // 稳定 ID 保留隐藏前的停靠关系. 首次停靠组选择最后加入的 Viewport 页.
    static constexpr std::array Panels{
        Panel{ActionId::ToggleHierarchy, "Hierarchy", "Hierarchy###Physara.Hierarchy", nullptr},
        Panel{ActionId::ToggleInspector, "Inspector", "Inspector###Physara.Inspector", "Ctrl+Shift+I"},
        Panel{ActionId::ToggleViewSettings, "View Settings", "View Settings###Physara.ViewSettings", nullptr},
        Panel{ActionId::ToggleStatistics, "Frame Statistics", "Frame Statistics###Physara.FrameStatistics", nullptr},
        Panel{ActionId::ToggleViewport, "Viewport", "Viewport###Physara.Viewport", nullptr}};

    constexpr auto Control = static_cast<unsigned>(Platform::Modifier::Control);
    constexpr auto Shift = static_cast<unsigned>(Platform::Modifier::Shift);
    constexpr auto ShortcutModifiers = Control | Shift | static_cast<unsigned>(Platform::Modifier::Alt) |
                                       static_cast<unsigned>(Platform::Modifier::Super);

    /// @brief 文档和编辑动作的菜单标签与键盘映射共用同一描述.
    struct CommandItem
    {
        ActionId action;
        const char *label;
        const char *shortcut;
        Platform::Key key;
        unsigned modifiers;
    };

    static constexpr std::array FileItems{
        CommandItem{ActionId::NewScene, "New Scene", "Ctrl+N", Platform::Key::N, Control},
        CommandItem{ActionId::OpenScene, "Open Scene", "Ctrl+O", Platform::Key::O, Control},
        CommandItem{ActionId::SaveScene, "Save Scene", "Ctrl+S", Platform::Key::S, Control},
        CommandItem{ActionId::SaveSceneAs, "Save Scene As", "Ctrl+Shift+S", Platform::Key::S, Control | Shift},
        CommandItem{ActionId::ImportModel, "Import Model", nullptr, Platform::Key::Unknown, 0}};

    static constexpr std::array EditItems{
        CommandItem{ActionId::Undo, "Undo", "Ctrl+Z", Platform::Key::Z, Control},
        CommandItem{ActionId::Redo, "Redo", "Ctrl+Y", Platform::Key::Y, Control},
        CommandItem{ActionId::DeleteSelection, "Delete", "Delete", Platform::Key::Delete, 0}};

    // 将 View 提供的等待和拒绝原因映射为 View Settings 标签.
    static const char *ViewStatus(Renderer::ViewSettingReason reason) noexcept
    {
        using enum Renderer::ViewSettingReason;
        switch (reason)
        {
        case None: return "Applied";
        case LayoutPending: return "Pending - waiting for viewport layout";
        case SlotPending: return "Pending - waiting for a frame slot";
        case SubmissionPending: return "Pending - waiting for accepted output";
        case ViewportSamplingPending: return "Pending - waiting for viewport sampling";
        case UploadPending: return "Pending - waiting for upload";
        case InvalidExtent: return "Failed - invalid viewport pixel extent";
        case TextureSizeLimit: return "Failed - device texture size limit";
        case DiagnosticBudget: return "Failed - diagnostic texture budget";
        case CameraUnavailable: return "Failed - scene camera unavailable";
        case InvalidProjection: return "Failed - invalid camera projection";
        case InvalidImageRect: return "Failed - camera image does not fit the viewport";
        case InvalidSampling: return "Failed - invalid camera sampling profile";
        case ScenePreparationFailed: return "Failed - scene preparation";
        case ScenePipelineUnavailable: return "Failed - Forward pipeline unavailable";
        case ShaderPending: return "Pending - preparing Shader pipelines";
        case SceneResidencyPending: return "Pending - waiting for scene residency";
        case SceneResidencyLimit: return "Failed - scene residency budget";
        case SceneAttachmentLimit: return "Failed - Forward attachment limit";
        case SceneUnsupported: return "Failed - unsupported scene geometry";
        case SceneUploadLimit: return "Failed - Forward upload limit";
        case SceneLightTableRejected: return "Failed - visible light table rejected";
        case EnvironmentPending: return "Pending - preparing HDR environment or DFG";
        case EnvironmentRejected: return "Failed - HDR environment unavailable";
        case DisplayUnavailable: return "Failed - display processing unavailable";
        case InvalidViewRequest: return "Failed - stale or duplicate view request";
        case ViewCapacityLimit: return "Failed - offscreen view capacity";
        case SceneOutputUnavailable: return "No scene output";
        }
        return "";
    }

    /// @brief CPU 阴影准备的状态及额度用语. 其结果可与上一接受场景同时显示.
    static const char *ShadowStatus(Renderer::ShadowPlanReason reason) noexcept
    {
        using enum Renderer::ShadowPlanReason;
        switch (reason)
        {
        case Planned: return "Planned";
        case Disabled: return "Disabled";
        case ViewUnavailable: return "View unavailable";
        case DepthArrayUnavailable: return "Depth array unsupported";
        case DeviceSizeLimit: return "Device size limit";
        case NoLights: return "No visible lights";
        case NoReceiverCoverage: return "Outside receiver coverage";
        case InvalidVolume: return "Unrepresentable shadow volume";
        case DirectionalLimit: return "Primary directional light limit";
        case Budget: return "Layer or byte budget";
        case StorageUnavailable: return "CPU allocation failed";
        case RevisionExhausted: return "Plan revision exhausted";
        case LightUnavailable: return "Selected light unavailable";
        case Count: break;
        }
        return "";
    }

    /// @brief 用名称及灯型显示选择, 由实体 handle 单独标识同名灯.
    static std::string ShadowLightLabel(const Scene::EntityRecord &record, Asset::ModelLightKind kind)
    {
        std::string label(LocalShadowPreviewName(kind));
        label.append(" - ");
        label.append(record.properties.name.empty() ? record.id : record.properties.name);
        return label;
    }

    /// @brief 在当前请求的副本上编辑过滤和预算等字段. 调用者在帧边界一次提交, 保留未显示的级联和距离设置.
    static void EditShadowSettings(Renderer::ShadowSettings &settings)
    {
        const char *filters[] = {"Hard", "PCF 3x3"};
        int filter = settings.filter == Renderer::ShadowFilter::Hard ? 0 : 1;
        ImGui::Checkbox("Shadows", &settings.enabled);
        if (ImGui::Combo("Shadow filter", &filter, filters, IM_ARRAYSIZE(filters)))
        {
            settings.filter = filter == 0 ? Renderer::ShadowFilter::Hard : Renderer::ShadowFilter::Pcf;
        }
        constexpr std::array<std::uint32_t, 3> Edges{1024, 512, 256};
        const char *edgeNames[] = {"1024 texels", "512 texels", "256 texels"};
        int edge = static_cast<int>(std::ranges::find(Edges, settings.edge) - Edges.begin());
        if (ImGui::Combo("Shadow resolution", &edge, edgeNames, IM_ARRAYSIZE(edgeNames)))
        {
            settings.edge = Edges[edge];
        }
        constexpr std::uint64_t MiB = 1024 * 1024;
        constexpr std::array Budgets{4 * MiB, 8 * MiB, 16 * MiB, 32 * MiB};
        const char *budgetNames[] = {"4 MiB", "8 MiB", "16 MiB", "32 MiB"};
        int budget = static_cast<int>(std::ranges::find(Budgets, settings.byteBudget) - Budgets.begin());
        if (ImGui::Combo("Shadow budget per view", &budget, budgetNames, IM_ARRAYSIZE(budgetNames)))
        {
            settings.byteBudget = Budgets[budget];
        }
    }

    static const char *ShadowFilterName(Renderer::ShadowFilter filter) noexcept
    {
        return filter == Renderer::ShadowFilter::Hard ? "Hard" : "PCF 3x3";
    }

    /// @brief 记录本 View 的选择意图, 同时区分当前 CPU 计划、显示退路和最后接受的诊断.
    static void DrawViewShadows(const WorkspaceState &state, const Renderer::RenderedScene *accepted,
                                bool allowEdits, WorkspaceFrame &frame)
    {
        using namespace Renderer;
        const auto &view = *state.view;
        const auto &selection = view.RequestedShadowDiagnostic();
        const auto *local = std::get_if<LocalShadowSelection>(&selection);
        const bool cascades = std::holds_alternative<CascadeDiagnostic>(selection);
        const auto entity = local && state.scene ? state.scene->Get(local->light) : std::nullopt;
        const std::string label = local ? (entity && entity->record.properties.light &&
            entity->record.properties.light->type == local->kind ? ShadowLightLabel(entity->record, local->kind) :
            "Selected light unavailable") : (cascades ? "Directional CSM" : "Off");

        const auto requested = view.RequestedShadows();
        auto shadows = requested;
        ImGui::BeginDisabled(!allowEdits || !state.scene);
        EditShadowSettings(shadows);
        if (shadows != requested)
        {
            frame.shadowSettings = ShadowSettingsIntent{view.Id(), shadows};
        }
        if (ImGui::BeginCombo("Shadow diagnostic", label.c_str()))
        {
            if (ImGui::Selectable("Off", !local && !cascades))
            {
                frame.shadowDiagnostic = ShadowDiagnosticIntent{view.Id(), {}};
            }
            if (ImGui::Selectable("Directional CSM", cascades))
            {
                frame.shadowDiagnostic = ShadowDiagnosticIntent{view.Id(), CascadeDiagnostic{}};
            }
            if (state.scene)
            {
                state.scene->VisitEntities([&](Scene::EntityHandle handle, const Scene::EntityView &candidate)
                {
                    const auto &light = candidate.record.properties.light;
                    if (light && light->type != Asset::ModelLightKind::Directional)
                    {
                        const LocalShadowSelection option{handle, light->type};
                        const auto name = ShadowLightLabel(candidate.record, light->type);
                        ImGui::PushID(static_cast<int>(handle.index));
                        if (ImGui::Selectable(name.c_str(), local && *local == option))
                        {
                            frame.shadowDiagnostic = ShadowDiagnosticIntent{view.Id(), option};
                        }
                        ImGui::PopID();
                    }
                });
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();

        if (const auto &last = view.LastShadowDiagnostic(); last && state.scene && last->scene == state.scene->Id())
        {
            if (const auto *past = std::get_if<LocalShadowSelection>(&last->selection))
            {
                ImGui::Text("Last accepted %s / light %u:%llu / ticket %llu", LocalShadowPreviewName(past->kind),
                            past->light.index, static_cast<unsigned long long>(past->light.generation),
                            static_cast<unsigned long long>(last->submission));
            }
            else
            {
                ImGui::Text("Last accepted CSM / ticket %llu", static_cast<unsigned long long>(last->submission));
            }
        }
        if (!state.scene)
        {
            return;
        }
        const auto settings = view.Settings();
        const auto revisions = state.scene->Versions();
        const auto scene = state.scene->Id();
        const auto *planned = state.statistics && state.statistics->shadows[0] ? &*state.statistics->shadows[0] : nullptr;
        // 计划和颜色分别核对各自的来源. 相机移动时仍可查看上次接受的诊断和票据.
        if (planned && planned->view == view.Id() && planned->layoutRevision == settings.requestRevision &&
            planned->plan.scene == scene && planned->plan.contentRevision == revisions.renderContent &&
            planned->plan.requestRevision == view.ShadowRequestRevision() &&
            planned->sceneCameraRevision == revisions.camera && planned->documentRevision == view.DocumentRevision())
        {
            const auto &plan = planned->plan;
            ImGui::Text("Shadow plan: %s / %u requested, %u planned texels",
                        ShadowStatus(local || cascades ? plan.diagnosticReason : plan.reason),
                        plan.requestedEdge, plan.effectiveEdge);
            const auto budgetRejected = plan.unallocated[static_cast<std::size_t>(ShadowPlanReason::Budget)];
            if (budgetRejected != 0)
            {
                ImGui::Text("%zu light groups exceed the shadow budget", budgetRejected);
            }
        }
        if (settings.reason != ViewSettingReason::None && (requested.enabled || local || cascades))
        {
            ImGui::Text("Shadow output: %s", ViewStatus(settings.reason));
        }
        else if (accepted && accepted->scene == scene && accepted->contentRevision == revisions.renderContent &&
                 accepted->shadowRequestRevision == view.ShadowRequestRevision() &&
                 accepted->camera.sceneCameraRevision == revisions.camera &&
                 accepted->documentRevision == view.DocumentRevision() &&
                 (local ? accepted->localShadow == *local :
                          (cascades ? accepted->directionalCascades != 0 : accepted->shadows.has_value())))
        {
            ImGui::TextUnformatted("Shadow output: accepted");
            if (const auto &work = accepted->shadows)
            {
                ImGui::Text("%s / %u texels / %u layers", ShadowFilterName(work->filter), work->edge, work->layers);
            }
        }
    }

    /// @brief 用调用方选定的场景请求或已接受帧显示同一路径可对照的源代、查询参数和角色.
    static void DrawEnvironmentSummary(const char *label, const Scene::SceneEnvironment &environment)
    {
        const auto source = environment.source->source.generic_u8string();
        ImGui::TextWrapped("%s - %s", label, reinterpret_cast<const char *>(source.c_str()));
        constexpr char Hex[] = "0123456789abcdef";
        char fingerprint[13]{};
        for (std::size_t index = 0; index < 6; ++index)
        {
            const auto value = environment.source->sourceFingerprint[index];
            fingerprint[index * 2] = Hex[value >> 4];
            fingerprint[index * 2 + 1] = Hex[value & 15];
        }
        ImGui::TextWrapped("Source revision %llu, SHA-256 %.12s",
                           static_cast<unsigned long long>(environment.source->revision.value), fingerprint);
        const auto &settings = environment.settings;
        const auto &rotation = settings.rotation;
        ImGui::TextWrapped("Rotation xyzw (%.9g, %.9g, %.9g, %.9g)",
                           rotation[0], rotation[1], rotation[2], rotation[3]);
        const auto *roles = settings.lighting ? (settings.sky ? "Lighting and sky" : "Lighting") : "Sky";
        if (const auto white = environment.source->image.whiteLuminance)
        {
            ImGui::TextWrapped("%s, file white %.4g cd/m2, intensity %.4g", roles, double(*white), settings.intensity);
        }
        else
        {
            ImGui::TextWrapped("%s, relative radiance, intensity %.4g", roles, settings.intensity);
        }
    }

    /// @brief 显示单次共享提取和各 View 的实际裁剪, 状态及开销来自同一正常提交.
    static void DrawPreparationStatistics(const ScenePreparationStatistics &data)
    {
        constexpr const char *extraction[]{"Skipped", "Reused", "Updated", "Rebuilt"};
        ImGui::Text("Slot %zu / structure %llu / extraction %s %.3f ms", data.slot,
                    static_cast<unsigned long long>(data.structureRevision),
                    extraction[static_cast<std::size_t>(data.extraction.mode)], data.extraction.seconds * 1000);
        ImGui::Text("Scene records: objects %zu / primitives %zu / lights %zu",
                    data.extraction.objects, data.extraction.primitives, data.extraction.lights);
        ImGui::Text("Static encodes: objects %zu / materials %zu / lights %zu",
                    data.encodings.objects, data.encodings.materials, data.encodings.lights);
        for (const auto &view : data.views)
        {
            if (view.view.IsValid())
            {
                ImGui::Text("View #%llu %s / cull %.3f ms / primitives %zu of %zu / lights %zu of %zu",
                            static_cast<unsigned long long>(view.view.generation),
                            view.accepted ? "accepted" : "prepared", view.seconds * 1000,
                            view.visiblePrimitives, view.primitiveCandidates, view.visibleLights, view.lightCandidates);
            }
        }
        ImGui::Text("Graph Forward uploads: static %zu / %zu B, view data %zu / %zu B",
                    data.staticUploads.writes, data.staticUploads.bytes, data.viewUploads.writes, data.viewUploads.bytes);
        ImGui::Text("Graph Forward instance and command uploads: %zu writes / %zu B",
                    data.drawUploads.writes, data.drawUploads.bytes);
        ImGui::Text("Payload arena: peak %zu of %zu B / %zu growths",
                    data.payload.highWater, data.payload.capacity, data.payload.growths);
        ImGui::Text("Graph arena: peak %zu of %zu B / %zu growths",
                    data.graph.highWater, data.graph.capacity, data.graph.growths);
    }

    /// @brief 当期图和池来自值快照, 可以在首个正常场景前显示安全图或准入压力.
    static void DrawGraphStatistics(const FrameStatistics &stats)
    {
        // 池的总量与峰值覆盖整个设备会话. 下方图规模和原生创建数只覆盖最近一张已编译的图.
        const auto &pool = stats.graphPool;
        const auto bytes = [](std::uint64_t value) { return static_cast<unsigned long long>(value); };
        ImGui::Text("Graph pool: %zu / %zu objects, %llu / %llu logical B",
                    pool.used.objects, pool.limit.objects, bytes(pool.used.bytes), bytes(pool.limit.bytes));
        ImGui::Text("Peak: %zu objects / %llu B; idle limit: %zu / %llu B",
                    pool.highWater.objects, bytes(pool.highWater.bytes), pool.idleLimit.objects, bytes(pool.idleLimit.bytes));
        ImGui::Text("Reserved: %zu / %llu B; in flight: %zu / %llu B", pool.reserved.objects,
                    bytes(pool.reserved.bytes), pool.inFlight.objects, bytes(pool.inFlight.bytes));
        ImGui::Text("Retained: %zu / %llu B; idle: %zu / %llu B", pool.retained.objects, bytes(pool.retained.bytes),
                    pool.idle.objects, bytes(pool.idle.bytes));
        ImGui::Text("Pool hits %llu / created %llu / evicted %llu / denied %llu",
                    bytes(pool.hits), bytes(pool.created), bytes(pool.evicted), bytes(pool.rejected));
        if (stats.latestGraph)
        {
            const auto &graph = *stats.latestGraph;
            ImGui::Text("Graph %llu (%s) / %s %llu", bytes(graph.generation), graph.safeUi ? "safe UI" : "normal",
                        graph.accepted ? "accepted ticket" : graph.submission ? "prefix ticket" : "no ticket",
                        bytes(graph.submission));
            ImGui::Text("Passes declared %zu / live %zu / culled %zu; imports %zu / exports %zu",
                        graph.declared, graph.live, graph.declared - graph.live, graph.imports, graph.exports);
            ImGui::Text("Physical slots %zu / %llu logical B; native storage creates %llu texture / %llu buffer",
                        graph.slots, bytes(graph.plannedBytes), bytes(graph.nativeTextures), bytes(graph.nativeBuffers));
        }
        if (stats.graphPressure)
        {
            const auto reason = stats.graphPressure->View();
            ImGui::TextWrapped("%.*s", static_cast<int>(reason.size()), reason.data());
        }
        ImGui::Separator();
    }

    // 面板只读取 Runtime 已复制的准备计数和失败原因. 下方的场景代与票据取自独立的提交快照.
    static void DrawShaderStatistics(const FrameStatistics &stats)
    {
        using Renderer::ShaderPreparationState;
        const auto &shader = stats.shader;
        const auto &cpu = stats.shaderCpu;
        const auto counts = [](auto value) { return static_cast<unsigned long long>(value); };
        const char *status = shader.state == ShaderPreparationState::Active ? "Active" :
            (shader.state == ShaderPreparationState::Failed ? "Failed" : "Pending");
        ImGui::Text("Shader active %llu / request %llu %s", counts(shader.active), counts(shader.request), status);
        ImGui::Text("Programs %zu / Pipeline demand %zu / step %zu of %zu", Shader::ProductShaderCount,
                    shader.demand, shader.started, shader.budget);
        ImGui::Text("Pipeline cache %zu / %zu, hits %llu, created %llu, CPU %.3f ms",
                    shader.inCache, shader.cacheLimit, counts(shader.nativeHits), counts(shader.nativeCreated),
                    shader.nativeSeconds * 1000);
        if (shader.request)
        {
            ImGui::Text("CPU packages %zu / %zu, bytes %zu / %zu, hits %llu",
                cpu.entries, cpu.limit.entries, cpu.bytes, cpu.limit.bytes, counts(cpu.hits));
            ImGui::Text("Package attempts %llu / %.3f ms, GLSL stages %llu / %.3f ms",
                counts(cpu.builds), cpu.buildSeconds * 1000, counts(cpu.compilation.stages),
                cpu.compilation.seconds * 1000);
            if (cpu.discarded)
            {
                ImGui::Text("Superseded shader results %llu", counts(cpu.discarded));
            }
        }
        if (shader.reason)
        {
            if (const auto *program = Shader::FindProductProgram(shader.role))
            {
                ImGui::Text("%.*s / variant %u", static_cast<int>(program->name.size()), program->name.data(),
                            program->key.physicalLighting);
            }
            const auto reason = shader.reason->View();
            ImGui::PushTextWrapPos();
            ImGui::TextUnformatted(reason.data(), reason.data() + reason.size());
            ImGui::PopTextWrapPos();
        }
    }

    /// @brief 显示每路 CPU 规划和最近接受的阴影工作. 接受票据来自对应 View 的场景颜色.
    static void DrawShadowStatistics(const FrameStatistics &statistics)
    {
        for (const auto &shadow : statistics.shadows)
        {
            if (!shadow)
            {
                continue;
            }
            const auto &plan = shadow->plan;
            ImGui::Text("CPU shadows / View #%llu / Scene #%llu: %s",
                        static_cast<unsigned long long>(shadow->view.generation),
                        static_cast<unsigned long long>(plan.scene.generation), ShadowStatus(plan.reason));
            if (!plan.enabled)
            {
                continue;
            }
            if (plan.reason == Renderer::ShadowPlanReason::ViewUnavailable)
            {
                ImGui::TextUnformatted(ViewStatus(shadow->viewReason));
            }
            ImGui::Text("Request %llu / plan %llu / allocation %llu",
                        static_cast<unsigned long long>(plan.requestRevision),
                        static_cast<unsigned long long>(plan.planRevision),
                        static_cast<unsigned long long>(plan.allocationRevision));
            ImGui::Text("%s / %u requested / %u planned texels", ShadowFilterName(plan.filter),
                        plan.requestedEdge, plan.effectiveEdge);
            if (plan.qualityReason != Renderer::ShadowPlanReason::Planned)
            {
                // 降档时 Budget 指单层所需的逻辑字节. 下方逐灯拒绝仍可能来自层数上限.
                const auto *reason = plan.qualityReason == Renderer::ShadowPlanReason::Budget ?
                    "Logical byte budget" : ShadowStatus(plan.qualityReason);
                ImGui::Text("Lower resolution: %s", reason);
            }
            ImGui::Text("%u regions / %u layer span of %u / %llu of %llu logical B",
                        plan.regions, plan.layerSpan, plan.layerLimit, static_cast<unsigned long long>(plan.logicalBytes),
                        static_cast<unsigned long long>(plan.byteLimit));
            ImGui::Text("Directional cascades: %u (%s)", plan.directionalCascades,
                        ShadowStatus(plan.directionalReason));
            ImGui::Text("%zu lights planned / %u dirty / %u released%s", plan.plannedLights, plan.dirtyRegions,
                        plan.releasedRegions, plan.reused ? " / cache hit" : "");
            for (std::size_t reason = 0; reason < plan.unallocated.size(); ++reason)
            {
                if (plan.unallocated[reason] != 0)
                {
                    ImGui::Text("%zu lights: %s", plan.unallocated[reason],
                                ShadowStatus(static_cast<Renderer::ShadowPlanReason>(reason)));
                }
            }
        }
        for (const auto &entry : statistics.acceptedShadows)
        {
            if (!entry)
            {
                continue;
            }
            ImGui::Text("Accepted shadows / View #%llu / Scene #%llu / ticket %llu",
                        static_cast<unsigned long long>(entry->view.generation),
                        static_cast<unsigned long long>(entry->scene.generation),
                        static_cast<unsigned long long>(entry->submission));
            if (const auto &work = entry->work)
            {
                ImGui::Text("  %s / %s / %u texels / %u groups / %u layers",
                            work->diagnostic ? "Diagnostic" : "Lighting", ShadowFilterName(work->filter),
                            work->edge, work->groups, work->layers);
                ImGui::Text("  %u produced / %u reused / %zu depth batches / %llu array B", work->producedLayers,
                            work->reusedLayers, work->drawBatches, static_cast<unsigned long long>(work->logicalBytes));
            }
            else
            {
                ImGui::TextUnformatted("  No shadow consumption");
            }
        }
    }

    /// @brief 描述 CPU 网格结果. 溢出仍是可用网格, 其实际数量由调用方单独显示.
    static const char *FroxelStatusName(Renderer::FroxelStatus status) noexcept
    {
        switch (status)
        {
        case Renderer::FroxelStatus::Ready: return "Ready";
        case Renderer::FroxelStatus::ViewUnavailable: return "View unavailable";
        case Renderer::FroxelStatus::InvalidGrid: return "Invalid grid";
        case Renderer::FroxelStatus::UnrepresentableGrid: return "Grid cannot be represented";
        case Renderer::FroxelStatus::HeaderCapacity: return "Header budget exceeded";
        case Renderer::FroxelStatus::LightCapacity: return "Light row capacity exceeded";
        case Renderer::FroxelStatus::StorageUnavailable: return "CPU storage unavailable";
        case Renderer::FroxelStatus::NotRequested: return "Not requested";
        }
        return "Unknown";
    }

    /// @brief 显示各路本次显式请求的准备摘要. 退路灯行数描述单个查询的工作量, 帧上传及 GPU 时间另按票据显示.
    static void DrawFroxelStatistics(const FrameStatistics &statistics)
    {
        for (const auto &entry : statistics.froxels)
        {
            if (!entry)
            {
                continue;
            }
            const auto &plan = entry->plan;
            ImGui::Text("CPU Froxel / View #%llu / Scene #%llu: %s",
                        static_cast<unsigned long long>(entry->view.generation),
                        static_cast<unsigned long long>(plan.scene.generation), FroxelStatusName(entry->status));
            if (entry->status == Renderer::FroxelStatus::ViewUnavailable)
            {
                ImGui::TextUnformatted(ViewStatus(entry->viewReason));
            }
            if (entry->deliveryFailure)
            {
                ImGui::TextWrapped("%s", entry->deliveryFailure->View().data());
            }
            ImGui::Text("%u of %u index entries / %u overflow of %u cells",
                        plan.indexCount, plan.indexBudget, plan.overflowCellCount, plan.cellCount);
            if (plan.fallbackLocalCount)
            {
                ImGui::Text("%zu local light rows per fallback query", *plan.fallbackLocalCount);
            }
        }
    }

    /// @brief 展示最近一次场景提交. 表面, 批次和参数记录取自 source.view, 原生调用数还包括同图 UI 等消费者.
    /// GPU 时长可以稍后就绪, 使用它自己的场景和票据标注.
    static void DrawFrameStatistics(const WorkspaceState &state)
    {
        if (state.statistics)
        {
            // CPU 计划和图准入可以早于首个接受场景, 因此先显示它们, 再判断是否已有场景票据.
            DrawGraphStatistics(*state.statistics);
            DrawShaderStatistics(*state.statistics);
            DrawShadowStatistics(*state.statistics);
            DrawFroxelStatistics(*state.statistics);
        }
        if (!state.statistics || !state.statistics->latestScene)
        {
            ImGui::TextUnformatted("Awaiting accepted scene frame");
            return;
        }
        const auto &frame = *state.statistics->latestScene;
        const auto &source = frame.source;
        // 只接受附加出口的帧也属于当前文档. 场景标签取自 Scene 身份, View 和票据取自这次提交.
        const bool current = state.scene && source.scene == state.scene->Id();
        ImGui::Text("%s scene #%llu / View #%llu / ticket %llu", current ? "Accepted" : "Previous",
                    static_cast<unsigned long long>(source.scene.generation),
                    static_cast<unsigned long long>(source.view.generation),
                    static_cast<unsigned long long>(source.submission));
        ImGui::Text("Accepted shader generation %llu", static_cast<unsigned long long>(source.shaderGeneration));
        ImGui::Text("%s / %s / %s", source.localShadow ? LocalShadowPreviewName(source.localShadow->kind) :
                    source.directionalCascades ? "Directional CSM" :
                    PreviewNames[static_cast<std::size_t>(source.preview)],
                    source.display.profile == Renderer::DisplayProfile::AcesSdr ? "Filament ACES SDR" : "Linear clamp",
                    source.display.antiAliasing == Renderer::AntiAliasing::Fxaa ? "FXAA" : "None");
        ImGui::Text("Render %u x %u / Output %u x %u", source.render.outputExtent.width,
                    source.render.outputExtent.height, source.displayLayout.outputExtent.width,
                    source.displayLayout.outputExtent.height);
        ImGui::Text("CPU (ms) UI %.3f / Scene & View %.3f", frame.cpu.ui * 1000, frame.cpu.scenePreparation * 1000);
        ImGui::Text("CPU (ms) graph declare %.3f / compile %.3f / execute %.3f", frame.cpu.graphDeclare * 1000,
                    frame.cpu.graphCompile * 1000, frame.cpu.graphExecute * 1000);
        DrawPreparationStatistics(frame.preparation);
        if (frame.presentWaitSeconds)
        {
            ImGui::Text("Present wait (ms) %.3f", *frame.presentWaitSeconds * 1000);
        }
        else
        {
            ImGui::TextUnformatted("Window exchange skipped");
        }
        ImGui::Text("View surfaces %zu / lights %zu", frame.forward.draws, frame.forward.lights);
        ImGui::Text("Depth: %zu batches / %zu records. Color: %zu batches / %zu records",
                    frame.forward.depth.batches, frame.forward.depth.records,
                    frame.forward.color.batches, frame.forward.color.records);
        ImGui::Text("Graph: %zu passes / %llu native draws / %llu binds / %llu barriers",
                    frame.livePasses,
                    static_cast<unsigned long long>(frame.native.draws),
                    static_cast<unsigned long long>(frame.native.bindings),
                    static_cast<unsigned long long>(frame.native.barriers));
        ImGui::Text("Graph native uploads %llu / %llu bytes", static_cast<unsigned long long>(frame.native.uploads),
                    static_cast<unsigned long long>(frame.native.uploadBytes));
        ImGui::Text("Forward cache: geometry %zu / textures %zu / materials %zu / environment %zu",
                    frame.residency.geometry, frame.residency.textures, frame.residency.materials,
                    frame.residency.environment);
        // 待退休区间仍占用页容量. 总空闲字节可分布在多页, 最大空洞表示单次连续分配的上界.
        const auto &resident = frame.residency;
        ImGui::Text("Geometry: %zu pages, %zu used / %zu free B, %zu retiring / largest gap %zu B",
                    resident.geometryPages, resident.geometryUsedBytes, resident.geometryPageBytes -
                    resident.geometryUsedBytes, resident.geometryRetiringBytes, resident.largestGeometryGap);
        ImGui::Text("Object pages %zu B / material pages %zu B / material hits %llu of %llu / generation %llu",
                    resident.objectPageBytes, resident.materialPageBytes,
                    static_cast<unsigned long long>(resident.materialHits),
                    static_cast<unsigned long long>(resident.materialRequests),
                    static_cast<unsigned long long>(resident.materialGeneration));
        ImGui::Text("Texture %zu B / Environment %zu B / samplers %zu",
                    resident.textureBytes, resident.environmentBytes, resident.samplerEntries);
        const auto &mapping = frame.uploadStorage;
        ImGui::Text("Upload pool %llu of %llu B / available %llu B / peak %llu B / volatile %llu B",
                    static_cast<unsigned long long>(mapping.capacity), static_cast<unsigned long long>(mapping.limit),
                    static_cast<unsigned long long>(mapping.available), static_cast<unsigned long long>(mapping.highWater),
                    static_cast<unsigned long long>(mapping.volatileCapacity));
        ImGui::Text("Upload chunks %llu new / %llu reused / prepared %zu writes (%zu B)",
                    static_cast<unsigned long long>(frame.native.uploadChunksAllocated),
                    static_cast<unsigned long long>(frame.native.uploadChunksReused),
                    frame.preparation.uploadDemand.writes, frame.preparation.uploadDemand.bytes);
        constexpr const char *timingStates[]{"Unavailable", "Skipped - query capacity", "Pending", "Ready", "Failed"};
        ImGui::Text("GPU graph (scene + UI): %s", timingStates[static_cast<std::size_t>(frame.gpu)]);
        // 面板可能先看到新场景票据, 后收到旧区间的时长. 两行分别使用各自快照中的身份.
        if (const auto &gpu = state.statistics->latestGpu)
        {
            ImGui::Text("GPU %.3f ms / Scene #%llu / ticket %llu", gpu->seconds * 1000,
                        static_cast<unsigned long long>(gpu->source.scene.generation),
                        static_cast<unsigned long long>(gpu->source.submission));
        }
    }

    // 用 View 的请求比例初始化控件, 由 ImGui 的交互状态产生下一轮比例意图.
    static void DrawViewSettings(const Renderer::ViewSettings &view, bool allowEdits, WorkspaceFrame &frame)
    {
        constexpr char scaleLabel[] = "Render scale";
        auto scale = view.requestedScale;
        const auto &context = *ImGui::GetCurrentContext();
        const auto slider = ImGui::GetID(scaleLabel);
        const bool sampleRelease = allowEdits && context.ActiveId == slider &&
                                   context.ActiveIdSource == ImGuiInputSource_Mouse &&
                                   ImGui::IsMouseReleased(ImGuiMouseButton_Left) && ImGui::IsMousePosValid();
        auto &mouseDown = ImGui::GetIO().MouseDown[ImGuiMouseButton_Left];
        if (sampleRelease)
        {
            // 固定版 SliderBehavior 在释放时先清除活动项. 暂时保持本滑块的按下状态,
            // 让它用原有抓取偏移和舍入计算末段位置. 真实的释放边沿仍供手势判断.
            mouseDown = true;
        }
        ImGui::BeginDisabled(!allowEdits);
        const bool changed = ImGui::SliderFloat(scaleLabel, &scale, Renderer::MinRenderScale,
                                                Renderer::MaxRenderScale, "%.2fx", ImGuiSliderFlags_NoInput);
        ImGui::EndDisabled();
        if (sampleRelease)
        {
            // 先恢复真实按钮状态并结束活动项, 再读取项状态以提交本次末段采样.
            mouseDown = false;
            if (context.ActiveId == slider)
            {
                ImGui::ClearActiveID();
            }
        }
        if (allowEdits)
        {
            // 释放和导航确认作为正常结束信号, Escape 通过手势的取消标志处理.
            const bool began = ImGui::IsItemActivated();
            const bool active = ImGui::IsItemActive();
            const bool deactivated = ImGui::IsItemDeactivated();
            const bool finished = deactivated && (ImGui::IsMouseReleased(ImGuiMouseButton_Left) ||
                ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_Space, false) ||
                ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown, false));
            const auto preview = changed ? std::optional<FieldValue>(scale) : std::nullopt;
            if (finished && slider == context.ActiveIdPreviousFrame)
            {
                frame.gestures.completed = FieldGesture{view.view, preview, false, true};
            }
            else if (active || began)
            {
                frame.gestures.current = {view.view, preview, began, false};
            }
            else if (changed && !deactivated)
            {
                frame.edits.emplace_back(SetViewScale{view.view, scale});
            }
        }
        ImGui::Text("Requested %.2fx", view.requestedScale);
        if (view.effective)
        {
            // 显示最后已接受的整张画布尺寸, 不是当前请求或留边后的相机采样尺寸.
            const auto extent = view.effective->layout.outputExtent;
            ImGui::Text("Effective %.2fx (%u x %u)", view.effective->scale,
                        extent.width, extent.height);
        }
        else
        {
            ImGui::TextUnformatted("Effective - awaiting confirmation");
        }
        ImGui::TextUnformatted(ViewStatus(view.reason));
    }

    // 标题正文仍以原始 UTF-8 编辑和提交. 缺字及控制码用可区分的码点显示, 反斜杠转义.
    static std::string DisplayWindowTitle(std::string_view title)
    {
        std::string display;
        display.reserve(title.size());
        auto *font = ImGui::GetFont();
        for (auto it = title.data(), end = it + title.size(); it != end;)
        {
            unsigned int codepoint = 0;
            const auto bytes = ImTextCharFromUtf8(&codepoint, it, end);
            if (codepoint == '\\')
            {
                display += "\\\\";
            }
            else if (codepoint >= 0x20 && !(codepoint >= 0x7f && codepoint <= 0x9f) &&
                     font->IsGlyphInFont(static_cast<ImWchar>(codepoint)))
            {
                display.append(it, bytes);
            }
            else
            {
                char digits[6]; // Unicode 标量至多需要六位十六进制数字.
                const auto converted = std::to_chars(digits, digits + sizeof(digits), codepoint, 16);
                display += "\\u{";
                display.append(digits, converted.ptr);
                display += '}';
            }
            it += bytes;
        }
        return display;
    }

    // 菜单读已发布的宿主设置. 同一轮可产生两个各自携带窗口身份的值请求.
    void Workspace::DrawWindowMenu(const Platform::WindowSettings *window, WorkspaceFrame &frame, bool allowActions)
    {
        if (!window || !ImGui::BeginMenu("Window"))
        {
            m_TitleEditing = false;
            return;
        }
        const bool requested = window->requestedSwapInterval != 0;
        if (ImGui::MenuItem("VSync", nullptr, requested) && allowActions)
        {
            frame.vsync = Platform::SetWindowVSync{window->window, !requested};
        }
        if (window->effectiveSwapInterval)
        {
            ImGui::Text("Effective interval %d", *window->effectiveSwapInterval);
        }
        else
        {
            ImGui::TextUnformatted("Effective interval unavailable");
        }
        if (window->swapIntervalFailure)
        {
            ImGui::TextUnformatted(window->swapIntervalFailure->View().data());
        }

        // 草稿只在输入项持有键盘期间保留. Enter 提交, 失焦或关闭菜单丢弃草稿.
        if (!m_TitleEditing || !allowActions)
        {
            m_TitleDraft.fill('\0');
            if (window->requestedTitle.size() < m_TitleDraft.size())
            {
                std::ranges::copy(window->requestedTitle, m_TitleDraft.begin());
            }
        }
        ImGui::SetNextItemWidth(240.0f);
        if (ImGui::InputText("Title", m_TitleDraft.data(), m_TitleDraft.size(), ImGuiInputTextFlags_EnterReturnsTrue) &&
            allowActions)
        {
            frame.title = Platform::SetWindowTitle{window->window, m_TitleDraft.data()};
        }
        m_TitleEditing = allowActions && ImGui::IsItemActive();
        // 草稿需要转义时补一行可核对的预览. 草稿预览和宿主回执采用同一显示规则.
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 280.0f);
        if (m_TitleEditing)
        {
            const std::string_view draft(m_TitleDraft.data());
            const auto display = DisplayWindowTitle(draft);
            if (display != draft)
            {
                ImGui::TextUnformatted("Draft title");
                ImGui::TextUnformatted(display.c_str());
            }
        }
        ImGui::Text("Requested title %s", DisplayWindowTitle(window->requestedTitle).c_str());
        if (window->effectiveTitle)
        {
            ImGui::Text("Effective title %s", DisplayWindowTitle(*window->effectiveTitle).c_str());
        }
        else
        {
            ImGui::TextUnformatted("Effective title unavailable");
        }
        ImGui::PopTextWrapPos();
        if (window->titleFailure)
        {
            ImGui::TextUnformatted(window->titleFailure->View().data());
        }
        ImGui::EndMenu();
    }

    // 主视口使用会话快照. 其余面板的展示开关由 Workspace 持有.
    bool Workspace::Visible(ActionId action, EditState edits) const noexcept
    {
        return action == ActionId::ToggleViewport ? edits.viewportVisible : m_Visible[static_cast<std::size_t>(action)];
    }

    // 菜单与面板关闭共享此入口. 主视口操作交由帧边界的历史应用处理.
    void Workspace::SetVisible(ActionId action, bool visible, WorkspaceFrame &frame)
    {
        if (action == ActionId::ToggleViewport)
        {
            frame.edits.emplace_back(SetViewportVisibility{visible});
        }
        else
        {
            m_Visible[static_cast<std::size_t>(action)] = visible;
        }
    }

    // 文件操作、历史和删除统一使用当前快照的启用条件. 实际应用仍由 owner 核对身份.
    bool Workspace::CanActivate(ActionId action, const WorkspaceState &state) noexcept
    {
        if (!state.allowActions)
        {
            return false;
        }
        switch (action)
        {
        case ActionId::Undo: return state.edits.canUndo && !state.navigating;
        case ActionId::Redo: return state.edits.canRedo && !state.navigating;
        case ActionId::DeleteSelection:
            return state.scene && state.view && state.selection && !state.frozen && !state.navigating;
        case ActionId::ImportModel:
            return state.document && !state.frozen && !state.navigating && !state.model.Pending();
        case ActionId::NewScene:
        case ActionId::OpenScene:
        case ActionId::SaveScene:
        case ActionId::SaveSceneAs:
            return state.document && state.document->operation == 0 && !state.navigating;
        default:
            return true;
        }
    }

    // 从统一身份生成布局、历史或文件意图. 菜单和快捷键对相同快照遵循同一入口.
    void Workspace::Activate(ActionId action, const WorkspaceState &state, WorkspaceFrame &frame)
    {
        if (!CanActivate(action, state))
        {
            return;
        }
        using enum ActionId;
        switch (action)
        {
        case ToggleHierarchy:
        case ToggleInspector:
        case ToggleViewSettings:
        case ToggleStatistics:
        case ToggleViewport:
            SetVisible(action, !Visible(action, state.edits), frame);
            break;
        case NewScene: m_Documents.Request(SceneFileAction::New, *state.document, frame); break;
        case OpenScene: m_Documents.Request(SceneFileAction::Open, *state.document, frame); break;
        case SaveScene: m_Documents.Request(SceneFileAction::Save, *state.document, frame); break;
        case SaveSceneAs: m_Documents.Request(SceneFileAction::SaveAs, *state.document, frame); break;
        case ImportModel: m_Documents.RequestImport(state.document->version.scene); break;
        case DeleteSelection: frame.edits.emplace_back(Scene::EraseSubtree{*state.selection}); break;
        case Undo: frame.edits.emplace_back(HistoryAction::Undo); break;
        case Redo: frame.edits.emplace_back(HistoryAction::Redo); break;
        }
    }

    // 使用动作表生成菜单, 由状态快照统一控制可用性.
    void Workspace::DrawActions(const WorkspaceState &state, WorkspaceFrame &frame)
    {
        const auto menu = [&](const char *name, std::span<const CommandItem> items)
        {
            if (ImGui::BeginMenu(name))
            {
                for (const auto &item : items)
                {
                    if (ImGui::MenuItem(item.label, item.shortcut, false, CanActivate(item.action, state)))
                    {
                        Activate(item.action, state, frame);
                    }
                }
                ImGui::EndMenu();
            }
        };
        menu("File", FileItems);
        menu("Edit", EditItems);
        if (ImGui::BeginMenu("View"))
        {
            for (const auto &panel : Panels)
            {
                const bool visible = Visible(panel.action, state.edits);
                if (ImGui::MenuItem(panel.menu, panel.shortcut, visible, CanActivate(panel.action, state)))
                {
                    Activate(panel.action, state, frame);
                }
            }
            ImGui::EndMenu();
        }
    }

    // 布局和诊断只读. 封存后由会话应用字段、文件和视口意图.
    WorkspaceFrame Workspace::Draw(const WorkspaceState &state, const Viewport &viewport)
    {
        WorkspaceFrame frame;
        const auto *acceptedPreview = state.view ? state.view->AcceptedScenePreview() : nullptr;
        const auto &frameDiagnostic = state.frameDiagnostic;
        if (ImGui::BeginMainMenuBar())
        {
            DrawActions(state, frame);
            DrawWindowMenu(state.window, frame, state.allowActions);
            if (state.document)
            {
                m_Documents.DrawStatus(*state.document, state.model, state.fileError, state.allowActions, frame);
            }
            const bool degraded = acceptedPreview && acceptedPreview->degradation.has_value();
            if ((frameDiagnostic || degraded) && ImGui::BeginMenu("Frame status"))
            {
                // 使用窗口内容范围为正文换行, 不受 Viewport 面板宽度约束. 高度超出时沿菜单滚动.
                const auto &style = ImGui::GetStyle();
                const float available = ImGui::GetWindowViewport()->WorkSize.x -
                    2.0f * (style.DisplaySafeAreaPadding.x + style.WindowPadding.x) - style.ScrollbarSize;
                const float width = std::max(1.0f, std::min(40.0f * ImGui::GetFontSize(), available));
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);
                if (frameDiagnostic)
                {
                    const auto recovery = frameDiagnostic->recovery;
                    ImGui::TextUnformatted(recovery == FrameRecovery::Normal ? "Normal rendering recovered" :
                        recovery == FrameRecovery::SafeUi ? "Safe UI - retaining the last accepted output" :
                        recovery == FrameRecovery::Cancelled ? "Frame cancelled - retrying on the next iteration" :
                                                              "Rendering stopped");
                    ImGui::Separator();
                    ImGui::TextUnformatted(frameDiagnostic->primary.View().data());
                    if (frameDiagnostic->additional)
                    {
                        ImGui::TextUnformatted(frameDiagnostic->additional->View().data());
                    }
                }
                if (degraded)
                {
                    if (frameDiagnostic)
                    {
                        ImGui::Separator();
                    }
                    // 通过接受的像素代读取摘要和实际模式. 帧失败诊断仍沿原入口保留.
                    ImGui::Text("Last accepted Forward preview - %s", ScenePreviewName(*acceptedPreview));
                    const auto summary = acceptedPreview->degradation->View();
                    ImGui::TextUnformatted(summary.data(), summary.data() + summary.size());
                }
                ImGui::PopTextWrapPos();
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // 路径与未保存确认也在当前 UI 帧参与归属判定, 不从快捷键回调直接启动后台任务.
        m_Documents.DrawDialog(state.document, state.allowActions, frame);
        const bool allowEdits = state.allowActions && !state.frozen && !state.navigating;
        const auto drawEditError = [&]()
        {
            if (state.editError)
            {
                ImGui::TextWrapped("%s", state.editError->View().data());
            }
        };
        // 本轮主视口布局沿用传入快照, 显隐编辑在封存后生效.
        const auto dock = ImGui::DockSpaceOverViewport();
        bool inspectorDrawn = false;
        for (const auto &panel : Panels)
        {
            bool visible = Visible(panel.action, state.edits);
            if (!visible)
            {
                continue;
            }
            ImGui::SetNextWindowDockID(dock, ImGuiCond_FirstUseEver);
            if (ImGui::Begin(panel.window, &visible) && visible)
            {
                switch (panel.action)
                {
                case ActionId::ToggleViewport:
                    frame.viewport = viewport.Draw();
                    break;
                case ActionId::ToggleHierarchy:
                    m_ScenePanels.DrawHierarchy(state.scene, state.selection, state.allowActions, frame);
                    break;
                case ActionId::ToggleStatistics:
                    DrawFrameStatistics(state);
                    break;
                case ActionId::ToggleInspector:
                    inspectorDrawn = true;
                    m_ScenePanels.DrawInspector(state.scene, state.selection, allowEdits, frame);
                    if (state.scene)
                    {
                        DrawPhysicalInspector(*state.scene, state.selection, state.physicalResource,
                                              allowEdits && state.view, frame);
                    }
                    if (state.view)
                    {
                        const auto &last = state.view->LastRenderedScene();
                        ImGui::TextWrapped("Scene output - %s", ViewStatus(state.view->Settings().reason));
                        if (last && last->environment)
                        {
                            const bool currentScene = state.scene && last->scene == state.scene->Id();
                            const auto *label = currentScene ? "Last scene environment" : "Previous scene environment";
                            DrawEnvironmentSummary(label, *last->environment);
                        }
                        ScenePanels::DrawExposure(state.scene, *state.view, allowEdits, frame);
                    }
                    drawEditError();
                    break;
                case ActionId::ToggleViewSettings:
                    if (state.view)
                    {
                        m_ScenePanels.DrawCamera(state.scene, *state.view, allowEdits, frame);
                        if (!state.view->DocumentRequests().cameraEntityId)
                        {
                            ImGui::TextWrapped("Right mouse + WASD/QE: Fly. Alt + left mouse: Orbit. Middle mouse: Pan.");
                        }
                        const auto settings = state.view->Settings();
                        // 选择框说明当前请求的着色方式. 本次实际合成的模式由视口按颜色代读取.
                        int preview = static_cast<int>(settings.preview);
                        ImGui::BeginDisabled(!state.allowActions);
                        const bool changed = ImGui::Combo("Preview", &preview, PreviewNames, IM_ARRAYSIZE(PreviewNames));
                        ImGui::EndDisabled();
                        if (changed)
                        {
                            frame.preview = static_cast<Renderer::PreviewMode>(preview);
                        }
                        constexpr const char *aaNames[]{"None", "FXAA"};
                        int antiAliasing = static_cast<int>(settings.requestedAa);
                        ImGui::BeginDisabled(!allowEdits);
                        const bool aaChanged = ImGui::Combo("Anti-aliasing", &antiAliasing, aaNames,
                                                           IM_ARRAYSIZE(aaNames));
                        ImGui::EndDisabled();
                        if (aaChanged)
                        {
                            frame.antiAliasing = static_cast<Renderer::AntiAliasing>(antiAliasing);
                        }
                        if (antiAliasing == static_cast<int>(Renderer::AntiAliasing::Fxaa) &&
                            preview != static_cast<int>(Renderer::PreviewMode::PhysicalLighting))
                        {
                            ImGui::TextUnformatted("FXAA applies in Physical Lighting.");
                        }
                        const auto &rendered = state.view->LastRenderedScene();
                        const auto *lastScene = rendered && state.scene && rendered->scene == state.scene->Id() ?
                            &*rendered : nullptr;
                        if (lastScene)
                        {
                            // 同一文档换到测试图时仍显示最后一次场景实绩, 两个尺寸来自同一个场景记录.
                            if (lastScene->directionalCascades)
                            {
                                ImGui::Text("Last scene preview - %s (%u cascades)", ScenePreviewName(*lastScene),
                                            lastScene->directionalCascades);
                            }
                            else
                            {
                                ImGui::Text("Last scene preview - %s", ScenePreviewName(*lastScene));
                            }
                            const auto display = lastScene->display;
                            const auto render = lastScene->renderLayout.outputExtent;
                            const auto output = lastScene->outputExtent;
                            ImGui::Text("Last scene display - %s, AA %s",
                                        display.profile == Renderer::DisplayProfile::AcesSdr ?
                                            "Filament ACES SDR" : "Linear clamp",
                                        aaNames[static_cast<std::size_t>(display.antiAliasing)]);
                            ImGui::Text("Last scene render %u x %u", render.width, render.height);
                            ImGui::Text("Last scene output %u x %u", output.width, output.height);
                        }
                        else
                        {
                            ImGui::TextUnformatted("Last scene preview - awaiting scene output");
                        }
                        if (settings.effective && !acceptedPreview)
                        {
                            ImGui::TextUnformatted("Current accepted output - test image");
                        }
                        if (preview == static_cast<int>(Renderer::PreviewMode::DirectLighting))
                        {
                            ImGui::TextWrapped("Punctual lights and emission. The viewport uses a linear clamp display.");
                        }
                        if (preview == static_cast<int>(Renderer::PreviewMode::PhysicalLighting))
                        {
                            if (state.scene && state.scene->Environment())
                            {
                                DrawEnvironmentSummary("Requested environment", *state.scene->Environment());
                            }
                            else
                            {
                                ImGui::TextUnformatted("Requested environment - none");
                            }
                        }
                        if (lastScene && lastScene->environment)
                        {
                            DrawEnvironmentSummary("Last scene environment", *lastScene->environment);
                        }
                        else if (lastScene && !lastScene->directionalCascades && !lastScene->localShadow &&
                                 lastScene->mode == Renderer::PreviewMode::PhysicalLighting)
                        {
                            ImGui::TextUnformatted("Last scene environment - unused");
                        }
                        DrawViewSettings(settings, allowEdits, frame);
                        DrawViewShadows(state, acceptedPreview, allowEdits, frame);
                        drawEditError();
                    }
                    break;
                default:
                    break;
                }
            }
            ImGui::End();
            if (!visible && state.allowActions)
            {
                SetVisible(panel.action, false, frame);
            }
        }
        if (!inspectorDrawn)
        {
            EndRotationInput();
        }
        return frame;
    }

    // 只派发有资格的 Press. 事件次序来自会话, 文件输入和控件不在这里重新消费.
    void Workspace::ApplyShortcut(const Platform::InputEvent &event, const InputClaims &claims,
                                  const WorkspaceState &state, WorkspaceFrame &frame)
    {
        if (ResolveInputOwner(claims, event) != InputOwner::Workspace)
        {
            return;
        }
        const auto &key = std::get<Platform::KeyEvent>(event);
        if (key.action != Platform::KeyAction::Press || key.key == Platform::Key::Unknown)
        {
            return;
        }
        const auto modifiers = static_cast<unsigned>(key.modifiers) & ShortcutModifiers;
        const auto dispatch = [&](std::span<const CommandItem> items)
        {
            const auto item = std::ranges::find_if(items, [&](const CommandItem &command)
            {
                return command.key == key.key && command.modifiers == modifiers;
            });
            if (item != items.end())
            {
                Activate(item->action, state, frame);
                return true;
            }
            return false;
        };
        if (modifiers == (Control | Shift) && key.key == Platform::Key::I)
        {
            Activate(ActionId::ToggleInspector, state, frame);
        }
        else if (!dispatch(FileItems))
        {
            static_cast<void>(dispatch(EditItems));
        }
    }
}
