#include "GltfScene.hpp"

/// @file
/// @brief 保留所选 scene 的层级、初始局部姿态和支持报告.

#include <algorithm>
#include <array>
#include <cmath>
#include <set>

namespace Physara::Asset::Gltf
{
    /// @brief 扩展声明是文件级合同, 不随当前选择的 scene 变更.
    static void ReportSupport(const Json &root, Core::BoundedText &report)
    {
        constexpr std::array<std::string_view, 6> Supported{
            "KHR_lights_punctual", "KHR_materials_unlit", "KHR_materials_emissive_strength", "KHR_texture_transform",
            "KHR_materials_ior", "KHR_materials_specular"};
        const auto used = root.Field("extensionsUsed");
        std::set<std::string_view> names;
        for (std::size_t i = 0; i < used.Size(); ++i)
        {
            const auto field = used.At(i);
            const auto name = field.Text();
            if (name.empty() || !names.insert(name).second)
            {
                field.Error("Empty or duplicate extension name");
            }
        }
        const auto required = root.Field("extensionsRequired");
        std::set<std::string_view> unique;
        for (std::size_t i = 0; i < required.Size(); ++i)
        {
            const auto field = required.At(i);
            const auto name = field.Text();
            if (!names.contains(name) || !unique.insert(name).second)
            {
                field.Error("Required extensions must be unique and declared in extensionsUsed");
            }
            if (std::ranges::find(Supported, name) == Supported.end())
            {
                Core::BoundedText detail("Unsupported required extension ");
                detail.Append(name);
                field.Error(detail.View(), AssetError::UnsupportedModel);
            }
        }
        if (root.Field("animations").Size() != 0)
        {
            report.Append("Animation channels are not played; using initial node transforms");
        }
        for (const auto name : names)
        {
            if (!report.View().empty())
            {
                report.Append("\n");
            }
            report.Append(std::ranges::find(Supported, name) != Supported.end() ?
                "Supported CPU extension: " : "Optional extension ignored; using core fallback: ");
            report.Append(name);
        }
    }

    /// @brief 仿射矩阵的非零列应正交. 保留原矩阵, 允许可表达的负缩放和零缩放.
    static void CheckAffine(const Json &field, const std::array<double, 16> &m)
    {
        if (m[3] != 0 || m[7] != 0 || m[11] != 0 || m[15] != 1)
        {
            field.Error("Expected an affine local transform");
        }
        std::array<std::array<double, 3>, 3> basis{};
        for (std::size_t col = 0; col < 3; ++col)
        {
            const auto length = std::hypot(m[4 * col], m[4 * col + 1], m[4 * col + 2]);
            if (!std::isfinite(length))
            {
                field.Error("Matrix column exceeds representable scale");
            }
            if (length > 0)
            {
                for (std::size_t row = 0; row < 3; ++row)
                {
                    basis[col][row] = m[4 * col + row] / length;
                }
            }
            for (std::size_t previous = 0; previous < col; ++previous)
            {
                double dot = 0;
                for (std::size_t row = 0; row < 3; ++row)
                {
                    dot += basis[col][row] * basis[previous][row];
                }
                if (std::abs(dot) > 1e-5)
                {
                    field.Error("Local matrix contains shear", AssetError::UnsupportedModel);
                }
            }
        }
    }

    /// @brief 从源字段校验互斥和形状, 不让默认补齐掩盖错误的数组.
    static ModelTransform ReadTransform(const Json &node)
    {
        ModelTransform out;
        const auto matrix = node.Field("matrix");
        const auto translation = node.Field("translation");
        const auto rotation = node.Field("rotation");
        const auto scale = node.Field("scale");
        if (matrix)
        {
            if (translation || rotation || scale)
            {
                matrix.Error("matrix and TRS are mutually exclusive");
            }
            out.matrix = matrix.Numbers<16>();
            CheckAffine(matrix, *out.matrix);
            return out;
        }
        if (translation)
        {
            out.translation = translation.Numbers<3>();
        }
        if (scale)
        {
            out.scale = scale.Numbers<3>();
        }
        if (rotation)
        {
            out.rotation = rotation.Numbers<4>();
            const auto &q = out.rotation;
            const auto length = std::hypot(std::hypot(q[0], q[1]), std::hypot(q[2], q[3]));
            if (std::abs(length - 1) > 1e-5)
            {
                rotation.Error("Expected a unit quaternion");
            }
        }
        return out;
    }

    /// @brief 先解析所选根, 按源顺序迭代遍历. 任何二次到达均构成环或多父.
    void SelectScene(const Json &root, ModelImportOptions options, std::stop_token stop, ModelRecord &record)
    {
        ReportSupport(root, record.supportReport);
        const auto scenes = root.Field("scenes");
        const auto defaultScene = root.Field("scene");
        if (options.scene)
        {
            record.sourceScene = *options.scene;
        }
        else if (defaultScene)
        {
            record.sourceScene = defaultScene.Index();
        }
        else if (scenes.Size() != 1)
        {
            scenes.Error(scenes.Size() == 0 ? "No scene is declared" : "Select a scene explicitly",
                         AssetError::UnsupportedModel);
        }
        const auto nodes = root.Field("nodes");
        const auto roots = scenes.At(record.sourceScene).Field("nodes");
        const auto meshes = root.Field("meshes");
        const auto cameras = root.Field("cameras");
        const auto lights = root.Field("extensions").Field("KHR_lights_punctual").Field("lights");
        // 显式栈避免源层级深度消耗 C++ 调用栈. parent 已是输出表下标, source 仍是源 node 下标.
        struct Pending
        {
            std::uint32_t source{};
            std::optional<std::uint32_t> parent;
        };
        std::vector<Pending> pending;
        std::vector<bool> visited(nodes.Size(), false);
        std::vector<bool> checkedMeshes(meshes.Size(), false);
        // 逆序压栈保证出栈和记录中的根/子节点次序与源文档一致.
        auto enqueue = [&pending](const Json &list, std::optional<std::uint32_t> parent)
        {
            for (auto i = list.Size(); i > 0; --i)
            {
                pending.push_back({list.At(i - 1).Index(), parent});
            }
        };
        enqueue(roots, {});

        while (!pending.empty())
        {
            CheckStop(stop);
            const auto work = pending.back();
            pending.pop_back();
            const auto node = nodes.At(work.source);
            if (visited[work.source])
            {
                node.Error("A selected node has multiple entries or is cyclic");
            }
            visited[work.source] = true;
            if (node.Field("skin"))
            {
                node.Field("skin").Error("Skin is bound in the selected scene", AssetError::UnsupportedModel);
            }
            ModelNode out;
            out.sourceIndex = work.source;
            if (const auto name = node.Field("name"))
            {
                out.name = name.Text();
            }
            out.local = ReadTransform(node);
            if (const auto mesh = node.Field("mesh"))
            {
                out.mesh = mesh.Index();
                const auto sourceMesh = meshes.At(*out.mesh);
                if (!checkedMeshes[*out.mesh])
                {
                    const auto primitives = sourceMesh.Field("primitives");
                    if (primitives.Size() == 0)
                    {
                        primitives.Error("Expected a mesh with at least one primitive");
                    }
                    for (std::size_t i = 0; i < primitives.Size(); ++i)
                    {
                        const auto targets = primitives.At(i).Field("targets");
                        if (targets.Size() != 0)
                        {
                            targets.Error("Morph targets are used by a selected node", AssetError::UnsupportedModel);
                        }
                    }
                    checkedMeshes[*out.mesh] = true;
                }
            }
            if (const auto camera = node.Field("camera"))
            {
                out.camera = camera.Index();
                static_cast<void>(cameras.At(*out.camera));
            }
            if (const auto light = node.Field("extensions").Field("KHR_lights_punctual"))
            {
                out.light = light.Field("light").Index();
                static_cast<void>(lights.At(*out.light));
            }
            // 至多与经过有界解析的源节点数相同, C11 元数据的计数已限制在 uint32 范围内.
            const auto index = static_cast<std::uint32_t>(record.nodes.size());
            if (work.parent)
            {
                record.nodes[*work.parent].children.push_back(index);
            }
            else
            {
                record.roots.push_back(index);
            }
            record.nodes.push_back(std::move(out));
            enqueue(node.Field("children"), index);
        }
    }
}
