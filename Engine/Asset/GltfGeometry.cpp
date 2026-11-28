#include "GltfGeometry.hpp"
#include "GltfTangents.hpp"
#include "StaticTangents.hpp"

/// @file
/// @brief 分两阶段收集 Primitive 输入并构建静态三角形, 处理逐面法线、切线拆点和保有预算.
/// Renderer 的有效 normal UV 覆盖复用末尾的类型化入口, 不重新解析 glTF 或改写已发布资产.

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <numeric>
#include <tuple>
#include <utility>

namespace Physara::Asset::Gltf
{
    static constexpr std::uint32_t Triangles = 4, Strip = 5, Fan = 6;
    static constexpr auto Missing = (std::numeric_limits<std::uint32_t>::max)();

    /// @brief 建立所用属性的读视图, 确认材质有效 UV 与核心回退, 缺法线时不接纳源切线.
    static PrimitivePlan PreparePrimitive(const Json &field, const Json &accessors, Resources &resources,
                                          Tables &tables, const ModelRecord &record, std::stop_token stop)
    {
        const auto mode = field.Field("mode");
        const auto topology = mode ? mode.Index() : Triangles;
        if (topology < Triangles || topology > Fan)
        {
            mode.Error("Only triangle topology is supported", AssetError::UnsupportedModel);
        }
        const auto attributes = field.Field("attributes");
        const auto position = attributes.Field("POSITION");
        if (!position)
        {
            position.Error("Primitive has no core POSITION fallback", AssetError::UnsupportedModel);
        }
        const auto material = tables.Material(field.Field("material"));
        PrimitivePlan out{field, material, topology, {},
                          Accessor(accessors, position.Index(), resources, Attribute::Position, stop),
                          {}, {}, {}, {}, {}, {}};
        const bool draco = static_cast<bool>(field.Field("extensions").Field("KHR_draco_mesh_compression"));
        auto requireFallback = [draco](const Accessor &value)
        {
            if (draco && !value.HasStorage())
            {
                value.Field().Error("Compressed primitive has no core accessor payload", AssetError::UnsupportedModel);
            }
        };
        requireFallback(out.position);
        const auto count = out.position.Count();
        auto add = [&](std::optional<Accessor> &target, const Json &source, Attribute semantic)
        {
            if (source)
            {
                target.emplace(accessors, source.Index(), resources, semantic, stop);
                requireFallback(*target);
                if (semantic != Attribute::Index && target->Count() != count)
                {
                    source.Error("Attribute count differs from POSITION count");
                }
            }
        };
        add(out.normal, attributes.Field("NORMAL"), Attribute::Normal);
        if (out.normal)
        {
            add(out.tangent, attributes.Field("TANGENT"), Attribute::Tangent);
        }
        add(out.uv[0], attributes.Field("TEXCOORD_0"), Attribute::TexCoord);
        add(out.uv[1], attributes.Field("TEXCOORD_1"), Attribute::TexCoord);
        add(out.color, attributes.Field("COLOR_0"), Attribute::Color);
        add(out.index, field.Field("indices"), Attribute::Index);

        const auto &surface = record.materials[material];
        constexpr std::array<std::string_view, static_cast<std::size_t>(MaterialSlot::Count)> Slots{
            "baseColorTexture", "metallicRoughnessTexture", "normalTexture", "occlusionTexture", "emissiveTexture",
            "specularTexture", "specularColorTexture", "clearcoatTexture", "clearcoatRoughnessTexture",
            "clearcoatNormalTexture", "sheenColorTexture", "sheenRoughnessTexture"};
        for (std::size_t i = 0; i < surface.slots.size(); ++i)
        {
            if (const auto &slot = surface.slots[i]; slot)
            {
                if (slot->texCoord >= out.uv.size() || !out.uv[slot->texCoord])
                {
                    Core::BoundedText reason(Slots[i]);
                    reason.Append(" requires effective TEXCOORD_");
                    reason.Append(std::to_string(slot->texCoord));
                    reason.Append(" in this primitive's attributes");
                    attributes.Error(reason.View(), AssetError::UnsupportedModel);
                }
                if (i == static_cast<std::size_t>(MaterialSlot::Normal))
                {
                    out.normalUv = slot->texCoord;
                }
                else if (i == static_cast<std::size_t>(MaterialSlot::ClearcoatNormal))
                {
                    out.coatNormalUv = slot->texCoord;
                }
            }
        }
        if (out.coatNormalUv && !out.tangent && !out.normalUv)
        {
            field.Field("clearcoatNormalTexture").Error("Clearcoat normal requires NORMAL and TANGENT or a base normal map",
                                                        AssetError::UnsupportedModel);
        }
        return out;
    }

    Geometry::Geometry(const Json &root, Resources &resources, Tables &tables, ModelRecord &record, std::stop_token stop)
        : m_Record(record), m_Stop(stop)
    {
        const auto meshes = root.Field("meshes");
        const auto accessors = root.Field("accessors");
        std::map<std::uint32_t, std::uint32_t> meshMap;
        for (auto &node : record.nodes)
        {
            CheckStop(stop);
            if (!node.mesh)
            {
                continue;
            }
            const auto source = *node.mesh;
            const auto [it, inserted] = meshMap.emplace(source, static_cast<std::uint32_t>(m_Meshes.size()));
            if (inserted)
            {
                const auto primitives = meshes.At(source).Field("primitives");
                auto &mesh = m_Meshes.emplace_back(source, std::vector<PrimitivePlan>{});
                mesh.primitives.reserve(primitives.Size());
                for (std::size_t i = 0; i < primitives.Size(); ++i)
                {
                    CheckStop(stop);
                    mesh.primitives.push_back(PreparePrimitive(primitives.At(i), accessors, resources, tables, record, stop));
                }
            }
            node.mesh = it->second;
        }
    }

    /// @brief 将一个已验证的局部位置并入当前 AABB.
    static void Include(ModelBounds &bounds, const std::array<float, 3> &p) noexcept
    {
        for (std::size_t i = 0; i < p.size(); ++i)
        {
            bounds.minimum[i] = (std::min)(bounds.minimum[i], p[i]);
            bounds.maximum[i] = (std::max)(bounds.maximum[i], p[i]);
        }
    }

    /// @brief 按源顶点次序读取全部活动属性, 核对单位框架、颜色范围和 POSITION 声明 extrema.
    /// 此处 bounds 包含全部 accessor 元素, 与后续仅保留有效三角形的表面 bounds 分开.
    static std::vector<ModelVertex> ReadVertices(const PrimitivePlan &plan, std::stop_token stop)
    {
        const auto &pos = plan.position;
        const auto min = pos.Field().Field("min").Numbers<3>();
        const auto max = pos.Field().Field("max").Numbers<3>();
        std::vector<ModelVertex> vertices(pos.Count());
        const auto first = pos.Read(0);
        ModelBounds sourceBounds{{first[0], first[1], first[2]}, {first[0], first[1], first[2]}};
        for (std::uint32_t i = 0; i < pos.Count(); ++i)
        {
            if (i % 4096 == 0)
            {
                CheckStop(stop);
            }
            auto &vertex = vertices[i];
            const auto p = pos.Read(i);
            vertex.position = {p[0], p[1], p[2]};
            Include(sourceBounds, vertex.position);
            if (plan.normal)
            {
                const auto n = plan.normal->Read(i);
                vertex.normal = {n[0], n[1], n[2]};
                if (!IsUnit(vertex.normal))
                {
                    plan.normal->Field().Error("NORMAL must be a unit vector");
                }
            }
            if (plan.tangent)
            {
                vertex.tangent = plan.tangent->Read(i);
                if (!IsTangentFrame(vertex))
                {
                    plan.tangent->Field().Error("TANGENT does not form a valid frame with NORMAL");
                }
            }
            for (std::size_t channel = 0; channel < plan.uv.size(); ++channel)
            {
                if (plan.uv[channel])
                {
                    const auto uv = plan.uv[channel]->Read(i);
                    vertex.uv[channel] = {uv[0], uv[1]};
                }
            }
            if (plan.color)
            {
                vertex.color = plan.color->Read(i);
                if (std::ranges::any_of(vertex.color, [](float c) { return c < 0 || c > 1; }))
                {
                    plan.color->Field().Error("COLOR_0 components must be in [0, 1]");
                }
            }
        }
        constexpr auto MaxFloat = (std::numeric_limits<float>::max)();
        for (std::size_t axis = 0; axis < min.size(); ++axis)
        {
            if (min[axis] > max[axis] || min[axis] < -MaxFloat || max[axis] > MaxFloat ||
                static_cast<float>(min[axis]) != sourceBounds.minimum[axis] ||
                static_cast<float>(max[axis]) != sourceBounds.maximum[axis])
            {
                pos.Field().Error("POSITION extrema differ from decoded accessor values");
            }
        }
        return vertices;
    }

    /// @brief 用双精度计算有向面积向量, 避免有限 float32 位置的差与叉乘在 float32 中溢出.
    static std::array<double, 3> FaceCross(const ModelVertex &a, const ModelVertex &b, const ModelVertex &c) noexcept
    {
        std::array<double, 3> u{}, v{};
        for (std::size_t i = 0; i < u.size(); ++i)
        {
            u[i] = static_cast<double>(b.position[i]) - a.position[i];
            v[i] = static_cast<double>(c.position[i]) - a.position[i];
        }
        return {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
    }

    /// @brief 转换拓扑, 条带奇偶使用源面号. bounds 只接纳确实发出的表面顶点.
    static void BuildTriangles(const PrimitivePlan &plan, std::span<const ModelVertex> vertices,
                               std::size_t faceCount, ModelPrimitive &out, std::stop_token stop)
    {
        auto index = [&plan](std::uint32_t i)
        {
            return plan.index ? plan.index->ReadIndex(i, plan.position.Count()) : i;
        };
        out.indices.reserve(faceCount * 3);
        for (std::uint32_t face = 0; face < faceCount; ++face)
        {
            if (face % 4096 == 0)
            {
                CheckStop(stop);
            }
            const auto first = plan.mode == Triangles ? face * 3 : face;
            auto a = index(plan.mode == Fan ? 0 : first);
            auto b = index(first + 1);
            const auto c = index(first + 2);
            if (plan.mode == Strip && face % 2 != 0)
            {
                std::swap(a, b);
            }
            if (FaceCross(vertices[a], vertices[b], vertices[c]) == std::array<double, 3>{})
            {
                continue;
            }
            if (!out.bounds)
            {
                out.bounds = ModelBounds{vertices[a].position, vertices[a].position};
            }
            for (auto corner : {a, b, c})
            {
                Include(*out.bounds, vertices[corner].position);
                out.indices.push_back(corner);
            }
        }
    }

    /// @brief 单模型工作空间与跨版本保有容量分开计量, retained 的归还由外层记录 owner 负责.
    struct GeometryBudget
    {
        std::size_t limit;
        ByteBudget &shared;
        std::size_t &retained;

        /// @brief 从单模型剩余额度扣除计划的同时驻留空间, 返回可供下一阶段使用的余额.
        [[nodiscard]] std::size_t Scratch(std::uint64_t bytes, const Json &where) const
        {
            if (bytes > limit - retained)
            {
                where.Error("Primitive exceeds the model geometry budget", AssetError::ModelByteLimit);
            }
            return limit - retained - static_cast<std::size_t>(bytes);
        }

        /// @brief 在实际输出分配前取得保有额度, 成功后的记账即使遇到后续异常也须统一退还.
        void Retain(std::size_t bytes, const Json &where) const
        {
            static_cast<void>(Scratch(bytes, where));
            if (!shared.TryReserve(bytes))
            {
                where.Error("CPU model geometry budget is occupied", AssetError::ModelByteLimit);
            }
            retained += bytes;
        }
    };

    /// @brief 保留源框架时删除未使用的顶点并重映射索引, 源顶点全部使用时直接移交存储.
    static void CompactVertices(std::vector<ModelVertex> &source, ModelPrimitive &out,
                                const GeometryBudget &budget, const Json &where, std::stop_token stop)
    {
        std::vector<std::uint32_t> remap(source.size(), Missing);
        for (auto index : out.indices)
        {
            remap[index] = 0;
        }
        std::uint32_t count = 0;
        for (auto &index : remap)
        {
            if (index != Missing)
            {
                index = count++;
            }
        }
        budget.Retain(count * sizeof(ModelVertex), where);
        if (count == source.size())
        {
            out.vertices = std::move(source);
            return;
        }
        out.vertices.resize(count);
        for (std::size_t i = 0; i < source.size(); ++i)
        {
            if (i % 4096 == 0)
            {
                CheckStop(stop);
            }
            if (remap[i] != Missing)
            {
                out.vertices[remap[i]] = source[i];
            }
        }
        for (auto &index : out.indices)
        {
            index = remap[index];
        }
    }

    /// @brief 以源下标和两套角点切线为联合键. 任一层的方向或手性有接缝就拆点, 两条流共用一份索引.
    /// 空切线 span 从源顶点取值. coatVertices 为空时只按第一套切线拆点.
    /// @note reserve 在分配输出顶点前检查调用方预算, 导入与 Renderer 重建各自承担保有责任.
    template <typename Reserve>
    static void SplitTangents(std::span<const ModelVertex> source, std::span<const std::array<float, 4>> tangents,
                              ModelPrimitive &out, Reserve &&reserve, std::stop_token stop,
                              std::span<const std::array<float, 4>> coat = {},
                              std::vector<std::array<float, 4>> *coatVertices = nullptr)
    {
        std::vector<std::uint32_t> order(out.indices.size());
        std::iota(order.begin(), order.end(), std::uint32_t{0});
        auto key = [&](std::uint32_t corner)
        {
            const auto index = out.indices[corner];
            const auto first = tangents.empty() ? source[index].tangent : tangents[corner];
            const auto second = coatVertices ? (coat.empty() ? source[index].tangent : coat[corner]) :
                std::array<float, 4>{};
            return std::tuple(index, first, second);
        };
        std::ranges::sort(order, [&](auto a, auto b) { return key(a) < key(b); });
        CheckStop(stop);

        // 先在源索引尚未修改时计数. 写回阶段只读取尚未处理角点的键, previous 保留值副本.
        std::uint32_t count = 1;
        for (std::size_t i = 1; i < order.size(); ++i)
        {
            if (key(order[i - 1]) != key(order[i]))
            {
                ++count;
            }
        }
        reserve(count * (sizeof(ModelVertex) + (coatVertices ? sizeof(coatVertices->front()) : 0)));
        out.vertices.resize(count);
        if (coatVertices)
        {
            coatVertices->resize(count);
        }
        auto store = [&](std::uint32_t index, const auto &value)
        {
            const auto &[sourceIndex, first, second] = value;
            out.vertices[index] = source[sourceIndex];
            out.vertices[index].tangent = first;
            if (coatVertices)
            {
                (*coatVertices)[index] = second;
            }
        };
        auto previous = key(order.front());
        std::uint32_t index = 0;
        store(index, previous);
        for (std::size_t i = 0; i < order.size(); ++i)
        {
            if (i % 4096 == 0)
            {
                CheckStop(stop);
            }
            const auto corner = order[i];
            const auto current = key(corner);
            if (current != previous)
            {
                ++index;
                store(index, current);
                previous = current;
            }
            out.indices[corner] = index;
        }
    }

    /// @brief 为每个有效面角点独占顶点, 由非零面积叉乘生成平面法线并复制其余活动属性.
    static void MakeFlatVertices(std::span<const ModelVertex> source, ModelPrimitive &out,
                                  const GeometryBudget &budget, const Json &where, std::stop_token stop)
    {
        budget.Retain(out.indices.size() * sizeof(ModelVertex), where);
        out.vertices.resize(out.indices.size());
        for (std::size_t i = 0; i < out.indices.size(); i += 3)
        {
            if (i % (4096 * 3) == 0)
            {
                CheckStop(stop);
            }
            const auto cross = FaceCross(source[out.indices[i]], source[out.indices[i + 1]], source[out.indices[i + 2]]);
            const auto length = std::hypot(cross[0], cross[1], cross[2]);
            const std::array normal{static_cast<float>(cross[0] / length), static_cast<float>(cross[1] / length),
                                    static_cast<float>(cross[2] / length)};
            for (std::size_t corner = i; corner < i + 3; ++corner)
            {
                out.vertices[corner] = source[out.indices[corner]];
                out.vertices[corner].normal = normal;
                out.indices[corner] = static_cast<std::uint32_t>(corner);
            }
        }
    }

    /// @brief 先检查最坏展开量, 再读取顶点/转换拓扑, 最后按源法线和材质需求选择框架生成路径.
    /// 完全退化时只交付空源槽; 任一数值或容量错误中止整个模型, 不发布半份几何.
    static ModelPrimitive BuildPrimitive(const PrimitivePlan &plan, std::uint32_t slot,
                                          const GeometryBudget &budget, std::stop_token stop)
    {
        const auto count = plan.index ? plan.index->Count() : plan.position.Count();
        if (count < 3 || (plan.mode == Triangles && count % 3 != 0))
        {
            plan.field.Error("Vertex or index count does not match triangle topology");
        }
        const std::uint64_t faces = plan.mode == Triangles ? count / 3 : count - 2;
        const auto corners = faces * 3;
        if (corners >= Missing)
        {
            plan.field.Error("Expanded triangle indices cannot be represented", AssetError::ModelByteLimit);
        }

        const bool expand = !plan.normal || (plan.normalUv && !plan.tangent);
        const std::uint64_t vertexCount = plan.position.Count();
        const auto worstVertices = expand ? corners : (std::min)(vertexCount, corners);
        const auto baseBytes = vertexCount * (sizeof(ModelVertex) + sizeof(std::uint32_t)) +
                               corners * sizeof(std::uint32_t) + worstVertices * sizeof(ModelVertex);
        const auto tangentScratch = budget.Scratch(baseBytes, plan.field);

        ModelPrimitive out;
        out.sourceSlot = slot;
        out.material = plan.material;
        out.hasUv = {plan.uv[0].has_value(), plan.uv[1].has_value()};
        out.hasSourceTangent = plan.tangent.has_value();
        auto source = ReadVertices(plan, stop);
        BuildTriangles(plan, source, static_cast<std::size_t>(faces), out, stop);
        if (out.indices.empty())
        {
            out.indices = std::vector<std::uint32_t>{};
            return out;
        }
        budget.Retain(out.indices.capacity() * sizeof(std::uint32_t), plan.field);
        const auto normalSlot = plan.field.Field("normalTexture");
        // 源切线仅与源法线一起生效. 缺法线时先建立逐面顶点, 再按需要生成切线.
        if (!plan.normal)
        {
            MakeFlatVertices(source, out, budget, plan.field, stop);
            if (plan.normalUv)
            {
                const auto tangents = GenerateTangents(out.vertices, out.indices, *plan.normalUv, normalSlot.Location(),
                                                       tangentScratch, stop);
                for (std::size_t i = 0; i < tangents.size(); ++i)
                {
                    out.vertices[i].tangent = tangents[i];
                }
            }
        }
        else if (plan.normalUv && !plan.tangent)
        {
            const auto tangents = GenerateTangents(source, out.indices, *plan.normalUv, normalSlot.Location(),
                                                   tangentScratch, stop);
            SplitTangents(source, tangents, out, [&](std::size_t bytes) { budget.Retain(bytes, normalSlot); }, stop);
        }
        else
        {
            CompactVertices(source, out, budget, plan.field, stop);
        }
        out.hasTangent = plan.tangent.has_value() || plan.normalUv.has_value();
        if (plan.coatNormalUv && plan.normalUv && plan.coatNormalUv != plan.normalUv)
        {
            // 源/生成切线属于基底 UV. 在发布前确认另一组 UV 可生成清漆框架.
            const auto coatSlot = plan.field.Field("clearcoatNormalTexture");
            (void)GenerateTangents(out.vertices, out.indices, *plan.coatNormalUv, coatSlot.Location(),
                                   tangentScratch, stop);
        }
        return out;
    }

    void Geometry::Build(std::size_t limit, ByteBudget &budget, std::size_t &retainedBytes)
    {
        const GeometryBudget quota{limit, budget, retainedBytes};
        quota.Retain(m_Meshes.size() * sizeof(ModelMesh), Json(nullptr, "/meshes"));
        m_Record.meshes.reserve(m_Meshes.size());
        for (const auto &mesh : m_Meshes)
        {
            CheckStop(m_Stop);
            auto &out = m_Record.meshes.emplace_back(mesh.source, std::vector<ModelPrimitive>{});
            quota.Retain(mesh.primitives.size() * sizeof(ModelPrimitive), mesh.primitives.front().field);
            out.primitives.reserve(mesh.primitives.size());
            for (std::uint32_t i = 0; i < mesh.primitives.size(); ++i)
            {
                CheckStop(m_Stop);
                out.primitives.push_back(BuildPrimitive(mesh.primitives[i], i, quota, m_Stop));
            }
        }
    }
}

namespace Physara::Asset
{
    std::variant<NormalUvGeometry, AssetFailure> BuildNormalUvGeometry(
        const ModelPrimitive &primitive, TangentSource primary, TangentSource secondary, std::size_t workLimit,
        std::string_view primaryLocation)
    {
        assert(primary != TangentSource::Absent && !primitive.indices.empty());
        constexpr std::string_view coatLocation = "clearcoatNormalTexture.texCoord";
        try
        {
            const auto generate = [&](TangentSource source, std::string_view field, std::size_t limit)
            {
                if (source < TangentSource::Uv0)
                {
                    return std::vector<std::array<float, 4>>{};
                }
                const auto uv = static_cast<std::uint32_t>(source);
                assert(uv < primitive.hasUv.size() && primitive.hasUv[uv]);
                return Gltf::GenerateTangents(primitive.vertices, primitive.indices, uv, field, limit, {});
            };
            const auto primaryTangents = generate(primary, primaryLocation, workLimit);
            const auto primaryBytes = primaryTangents.size() * sizeof(std::array<float, 4>);
            const auto coatTangents = generate(secondary, coatLocation, workLimit - primaryBytes);
            NormalUvGeometry result;
            auto &out = result.primitive;
            out.indices = primitive.indices;
            // 两份索引分别用于输出及排序. 角点数组在整个拆点阶段存活.
            const auto working = primaryBytes + coatTangents.size() * sizeof(std::array<float, 4>) +
                out.indices.size() * (2 * sizeof(std::uint32_t));
            Gltf::SplitTangents(primitive.vertices, primaryTangents, out, [&](std::size_t bytes)
            {
                if (working > workLimit || bytes > workLimit - working)
                {
                    Gltf::Reject(AssetError::ModelByteLimit, secondary == TangentSource::Absent ?
                        primaryLocation : coatLocation, "Tangent split exceeds the geometry work budget");
                }
            }, {}, coatTangents, secondary == TangentSource::Absent ? nullptr : &result.coatTangents);
            out.sourceSlot = primitive.sourceSlot;
            out.material = primitive.material;
            out.hasUv = primitive.hasUv;
            out.hasTangent = true;
            out.bounds = primitive.bounds;
            return result;
        }
        catch (const Gltf::ImportFailure &failure)
        {
            return failure.error;
        }
    }
}
