#include "ScenePanels.hpp"

/// @file
/// @brief 将 Scene 的身份、父链接和展示名投影到可裁剪行, 直接读取选中实体的本地字段.
/// View 面板从同一 owner 读取物理请求和渲染记录, 单位换算后只提交字段意图.
/// 欧拉角只用于活动控件, 领域入口消费 xyzw 四元数; 所有写入均留给 Session 的编辑边界.

#include <algorithm>
#include <cmath>
#include <numbers>
#include <unordered_map>
#include <utility>

#include <glm/gtc/quaternion.hpp>
#include <imgui.h>
#include <imgui_internal.h>

#include "Workspace.hpp"

namespace Physara::Editor::Detail
{
    // 保留同一文档中仍存活节点的展开状态, 只投影 UI 所需的字段.
    void ScenePanels::RefreshHierarchy(const Scene::Scene &scene)
    {
        const auto version = scene.Versions().document;
        if (m_Scene == scene.Id() && m_Revision == version)
        {
            return;
        }
        std::unordered_map<std::uint32_t, const Node *> previous;
        if (m_Scene == scene.Id())
        {
            previous.reserve(m_Nodes.size());
            for (const auto &node : m_Nodes)
            {
                previous.emplace(node.handle.index, &node);
            }
        }
        std::vector<Node> nodes;
        std::vector<std::optional<Scene::EntityId>> parents;
        scene.VisitEntities([&](Scene::EntityHandle handle, const Scene::EntityView &entity)
        {
            const auto old = previous.find(handle.index);
            const bool known = old != previous.end() && old->second->handle == handle;
            nodes.push_back({handle, entity.record.id, entity.record.properties.name, {},
                             !known || old->second->expanded});
            parents.push_back(entity.record.parent);
        });
        std::unordered_map<std::string_view, std::size_t> byId;
        byId.reserve(nodes.size());
        for (std::size_t i = 0; i < nodes.size(); ++i)
        {
            byId.emplace(nodes[i].id, i);
        }
        std::vector<std::size_t> roots;
        for (std::size_t i = 0; i < nodes.size(); ++i)
        {
            if (parents[i])
            {
                nodes[byId.at(*parents[i])].children.push_back(i);
            }
            else
            {
                roots.push_back(i);
            }
        }
        m_Nodes = std::move(nodes);
        m_Roots = std::move(roots);
        m_Scene = scene.Id();
        m_Revision = version;
        m_RowsDirty = true;
    }

    // 用显式栈展开可见分支, 允许文档中合法的深层级, 裁剪发生在绘制阶段.
    void ScenePanels::RebuildVisibleRows()
    {
        std::vector<Row> pending;
        const auto push = [&](const std::vector<std::size_t> &children, std::size_t depth)
        {
            for (auto i = children.rbegin(); i != children.rend(); ++i)
            {
                pending.push_back({*i, depth});
            }
        };
        m_Rows.clear();
        push(m_Roots, 0);
        while (!pending.empty())
        {
            const auto row = pending.back();
            pending.pop_back();
            m_Rows.push_back(row);
            const auto &node = m_Nodes[row.node];
            if (node.expanded)
            {
                push(node.children, row.depth + 1);
            }
        }
        m_RowsDirty = false;
    }

    // 组合槽位与代, 避免同名实体或已删除槽位共享 ImGui 控件状态.
    static void PushEntityId(Scene::EntityHandle id)
    {
        ImGui::PushID(static_cast<int>(id.scene.generation));
        ImGui::PushID(static_cast<int>(id.scene.generation >> 32));
        ImGui::PushID(static_cast<int>(id.index));
        ImGui::PushID(static_cast<int>(id.generation));
        ImGui::PushID(static_cast<int>(id.generation >> 32));
    }

    // 与 PushEntityId 成对结束完整实体身份作用域, 简单子控件共用该前缀.
    static void PopEntityId()
    {
        for (int i = 0; i < 5; ++i)
        {
            ImGui::PopID();
        }
    }

    // 先刷新有版本的树投影, 再只绘制展开分支中的可见行; 选择意图保留运行期完整身份.
    void ScenePanels::DrawHierarchy(const Scene::Scene *scene, const std::optional<Scene::EntityHandle> &selection,
                                    bool allowActions, WorkspaceFrame &frame)
    {
        if (!scene)
        {
            ImGui::TextUnformatted("No scene document.");
            return;
        }
        RefreshHierarchy(*scene);
        if (m_RowsDirty)
        {
            RebuildVisibleRows();
        }
        if (m_Rows.empty())
        {
            ImGui::TextUnformatted("Empty scene. Use File > Import Model.");
            return;
        }
        ImGui::BeginDisabled(!allowActions);
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(m_Rows.size()));
        while (clipper.Step())
        {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
            {
                const auto &row = m_Rows[static_cast<std::size_t>(i)];
                auto &node = m_Nodes[row.node];
                const float indent = static_cast<float>(row.depth) * ImGui::GetStyle().IndentSpacing;
                if (indent > 0)
                {
                    ImGui::Indent(indent);
                }
                PushEntityId(node.handle);
                auto flags = ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth |
                             ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
                if (node.children.empty())
                {
                    flags |= ImGuiTreeNodeFlags_Leaf;
                }
                if (selection == node.handle)
                {
                    flags |= ImGuiTreeNodeFlags_Selected;
                }
                ImGui::SetNextItemOpen(node.expanded, ImGuiCond_Always);
                const bool open = ImGui::TreeNodeEx("Entity", flags, "%s",
                                                     node.name.empty() ? "(Unnamed)" : node.name.c_str());
                if (allowActions && ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
                {
                    frame.selection = node.handle;
                }
                if (allowActions && !node.children.empty() && open != node.expanded)
                {
                    node.expanded = open;
                    m_RowsDirty = true;
                }
                PopEntityId();
                if (indent > 0)
                {
                    ImGui::Unindent(indent);
                }
            }
        }
        ImGui::EndDisabled();
    }

    // 将 XYZ 度转换为 Rz * Ry * Rx 的单位四元数, 沿当前值的半球保持表示连续.
    static FieldValue EncodeField(TransformField field, const std::array<double, 3> &numbers,
                                  const std::array<double, 4> &currentRotation)
    {
        if (field != TransformField::Rotation)
        {
            return numbers;
        }
        auto next = glm::normalize(glm::dquat(glm::radians(glm::dvec3(numbers[0], numbers[1], numbers[2]))));
        const auto &q = currentRotation;
        if (glm::dot(next, glm::dquat{q[3], q[0], q[1], q[2]}) < 0)
        {
            next = -next;
        }
        return std::array{next.x, next.y, next.z, next.w};
    }

    // 逐轴读 ImGui 状态. 分组只能汇总活动和失活, 无法把末值及新激活归到同一行的两个控件.
    static bool DrawLocalField(const char *label, LocalFieldTarget target, std::array<double, 3> &numbers,
                               const std::array<double, 4> &rotation, bool allowEdits, FieldGestures &gestures)
    {
        const auto before = numbers;
        const auto &context = *ImGui::GetCurrentContext();
        const bool rotating = target.field == TransformField::Rotation;
        std::optional<FieldGesture> current;
        bool active = false;
        bool changedCurrent = false;
        ImGui::BeginGroup();
        ImGui::PushID(label);
        ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
        for (int axis = 0; axis < 3; ++axis)
        {
            if (axis != 0)
            {
                ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
            }
            ImGui::PushID(axis);
            const auto index = static_cast<std::size_t>(axis);
            const bool changed = ImGui::DragScalar("", ImGuiDataType_Double, &numbers[index], rotating ? 0.25f : 0.02f,
                                                   nullptr, nullptr, rotating ? "%.3f" : "%.4f");
            if (allowEdits)
            {
                const bool deactivated = ImGui::IsItemDeactivated();
                if (deactivated && ImGui::GetItemID() == context.ActiveIdPreviousFrame)
                {
                    // 只消费上轮活动轴的末值. 该轴可以在新轴之后绘制.
                    auto final = before;
                    final[index] = numbers[index];
                    gestures.completed = FieldGesture{target,
                        changed ? std::optional(EncodeField(target.field, final, rotation)) : std::nullopt,
                        false, true};
                }
                else if (ImGui::IsItemActive() || (changed && !deactivated))
                {
                    active = ImGui::IsItemActive();
                    current = FieldGesture{target, std::nullopt, ImGui::IsItemActivated() || !active, !active};
                    changedCurrent = changed;
                }
            }
            ImGui::PopID();
            ImGui::PopItemWidth();
        }
        ImGui::PopID();
        ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::EndGroup();
        if (current)
        {
            if (changedCurrent)
            {
                current->preview = EncodeField(target.field, numbers, rotation);
            }
            gestures.current = std::move(*current);
        }
        return active;
    }

    // 每帧从权威实体读取本地值. 数值编辑只生成字段意图, 在 UI 封存前不修改领域或世界变换.
    void ScenePanels::DrawInspector(const Scene::Scene *scene, const std::optional<Scene::EntityHandle> &selection,
                                    bool allowEdits, WorkspaceFrame &frame)
    {
        const auto entity = scene && selection ? scene->Get(*selection) : std::nullopt;
        if (!entity)
        {
            EndRotationInput();
            ImGui::TextUnformatted("No selection.");
            return;
        }
        const auto &value = entity->record.properties;
        ImGui::TextUnformatted(value.name.empty() ? "(Unnamed)" : value.name.c_str());
        ImGui::SeparatorText("Local Transform");
        PushEntityId(*selection);
        ImGui::BeginDisabled(!allowEdits);
        const auto &q = value.local.rotation;
        auto translation = value.local.translation;
        DrawLocalField("Translation (m)", {*selection, TransformField::Translation}, translation, q, allowEdits,
                       frame.gestures);

        // 非活跃帧从四元数派生展示值, 未编辑旋转不回写舍入结果.
        const auto xyz = glm::degrees(glm::eulerAngles(glm::dquat{q[3], q[0], q[1], q[2]}));
        auto angles = m_Rotation && m_Rotation->entity == *selection ? m_Rotation->angles :
            std::array{xyz.x, xyz.y, xyz.z};
        const bool activeRotation = DrawLocalField("Rotation (deg)", {*selection, TransformField::Rotation},
            angles, q, allowEdits, frame.gestures);
        if (activeRotation && std::ranges::all_of(angles, [](double angle) { return std::isfinite(angle); }))
        {
            m_Rotation = RotationDraft{*selection, angles};
        }
        else
        {
            EndRotationInput();
        }
        auto scale = value.local.scale;
        DrawLocalField("Scale", {*selection, TransformField::Scale}, scale, q, allowEdits, frame.gestures);
        ImGui::EndDisabled();
        PopEntityId();
    }

    /// @brief 以面板单位呈现已接受的值, 确认时只交付本字段的领域值.
    /// 输入草稿由 ImGui 保有, Enter 才产生提交; 值域及派生失败由 View 事务统一报告.
    static void DrawPhysicalField(const char *label, const Renderer::PhysicalCameraRequest &camera,
                                  PhysicalCameraEdit::Field field, double displayScale, WorkspaceFrame &frame)
    {
        auto displayed = (camera.*field) * displayScale;
        if (!std::isfinite(displayed))
        {
            // 米制请求可能可表示而毫米显示溢出. 零只作输入框占位, 不替换 View 的已接受值.
            ImGui::TextWrapped("%s: current value exceeds the display range", label);
            displayed = 0.0;
        }
        // 可往返的精度和显示单位下的同值判断, 避免只按 Enter 时重写领域长度.
        const auto initial = displayed;
        if (ImGui::InputDouble(label, &displayed, 0.0, 0.0, "%.17g", ImGuiInputTextFlags_EnterReturnsTrue) &&
            displayed != initial)
        {
            frame.physicalCamera = PhysicalCameraEdit{field, displayed / displayScale};
        }
    }

    /// @brief 渲染 View 的物理请求及对应的当前、已接受场景曝光. 几何来源由当前相机选择决定.
    static void DrawPhysicalCamera(const Scene::Scene *scene, const Scene::Camera *projection,
                                   const Renderer::View &view, bool allowEdits, WorkspaceFrame &frame)
    {
        const auto &requests = view.DocumentRequests();
        const auto &physical = requests.physicalCamera;
        const bool navigation = !requests.cameraEntityId;
        constexpr double MillimetersPerMeter = 1000.0;
        using Request = Renderer::PhysicalCameraRequest;

        // 场景替换后使用新的控件 ID, 旧文档尚未提交的输入不能写入新文档.
        const auto generation = scene ? scene->Id().generation : 0;
        ImGui::PushID(static_cast<int>(generation));
        ImGui::PushID(static_cast<int>(generation >> 32));
        ImGui::BeginDisabled(!allowEdits || !scene);
        DrawPhysicalField("Aperture (f-number)", physical, &Request::fNumber, 1.0, frame);
        DrawPhysicalField("Shutter (s)", physical, &Request::shutterSeconds, 1.0, frame);
        DrawPhysicalField("ISO", physical, &Request::iso, 1.0, frame);
        DrawPhysicalField("Exposure compensation (EV)", physical, &Request::exposureCompensationEv, 1.0, frame);
        DrawPhysicalField("Sensor height (mm)", physical, &Request::sensorHeightMeters, MillimetersPerMeter, frame);
        if (navigation)
        {
            DrawPhysicalField("Navigation focal length (mm)", physical, &Request::navigationFocalLengthMeters,
                              MillimetersPerMeter, frame);
        }
        DrawPhysicalField("Focus distance (m)", physical, &Request::focusDistanceMeters, 1.0, frame);
        ImGui::EndDisabled();
        ImGui::PopID();
        ImGui::PopID();

        if (navigation || projection)
        {
            const auto lens = view.ResolveLens(projection);
            if (lens.yfov)
            {
                ImGui::Text("Vertical FOV %.6g deg", *lens.yfov * (180.0 / std::numbers::pi));
                if (!navigation)
                {
                    const double focalMm = lens.focalLengthMeters.value_or(0.0) * MillimetersPerMeter;
                    if (focalMm > 0.0 && std::isfinite(focalMm))
                    {
                        ImGui::Text("Effective focal length %.9g mm", focalMm);
                    }
                    else
                    {
                        ImGui::TextUnformatted("Effective focal length unavailable");
                    }
                }
            }
            else
            {
                ImGui::TextUnformatted("Orthographic projection");
            }
        }

        const auto exposure = view.Exposure();
        // 当前值可以先于新场景帧生效. 最后渲染记录保留原场景归属, 不能把测试图当成曝光确认.
        ImGui::Text("Requested EV100 %.6g", exposure.ev100);
        ImGui::Text("Requested pre-exposure %.9g", static_cast<double>(exposure.preExposure));
        if (const auto &last = view.LastRenderedScene())
        {
            const auto *label = scene && last->scene == scene->Id() ? "Last rendered pre-exposure" :
                "Previous scene pre-exposure";
            ImGui::Text("%s %.9g", label, static_cast<double>(last->camera.exposure.preExposure));
        }
        else
        {
            ImGui::TextUnformatted("Last rendered pre-exposure: pending");
        }
    }

    // 下拉框打开时才遍历相机实体. 标签可重名, 控件身份用 handle, 提交引用用持久化稳定 ID.
    void ScenePanels::DrawCamera(const Scene::Scene *scene, const Renderer::View &view, bool allowEdits,
                                  WorkspaceFrame &frame)
    {
        const auto &source = view.DocumentRequests().cameraEntityId;
        const auto handle = source && scene ? scene->Find(*source) : std::nullopt;
        const auto entity = handle ? scene->Get(*handle) : std::nullopt;
        const char *label = !source ? "Navigation camera" : !entity ? "Camera unavailable" :
            entity->record.properties.name.empty() ? "(Unnamed camera)" : entity->record.properties.name.c_str();
        ImGui::BeginDisabled(!allowEdits || !scene);
        if (ImGui::BeginCombo("Camera", label))
        {
            if (ImGui::Selectable("Navigation camera", !source) && allowEdits)
            {
                frame.camera = CameraSource{std::nullopt};
            }
            if (scene)
            {
                scene->VisitEntities([&](Scene::EntityHandle id, const Scene::EntityView &candidate)
                {
                    if (candidate.record.properties.camera)
                    {
                        PushEntityId(id);
                        const auto &name = candidate.record.properties.name;
                        if (ImGui::Selectable(name.empty() ? "(Unnamed camera)" : name.c_str(),
                                              source == candidate.record.id) && allowEdits)
                        {
                            frame.camera = CameraSource{candidate.record.id};
                        }
                        PopEntityId();
                    }
                });
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
        const auto *projection = entity && entity->record.properties.camera ? &*entity->record.properties.camera :
            nullptr;
        DrawPhysicalCamera(scene, projection, view, allowEdits, frame);
    }
}
