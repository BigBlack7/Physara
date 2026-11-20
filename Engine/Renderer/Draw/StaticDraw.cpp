#include "../Materials/StaticGpu.hpp"
#include "../Lighting/PunctualLights.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <numeric>
#include <string_view>
#include <variant>

#include <glm/gtc/type_ptr.hpp>

#include <Asset/StaticTangents.hpp>

/// @file
/// @brief 从 PreparedView 解析静态表面, 排序后构造间接批次及拥有型待写字节.
/// 收齐可绘材质后选择环境角色和颜色管线, 灯表计数, 观察量及曝光取自同轮 View.

namespace Physara::Renderer::Detail
{
    /// @brief Scene 和实体的完整分配代隔离资源, 防止文档替换或 ECS 槽复用后命中旧行.
    static ObjectKey ObjectIdentity(Scene::SceneId scene, Scene::EntityHandle entity) noexcept
    {
        return {scene.index, scene.generation, entity.index, entity.generation};
    }

    /// @brief 按生成接口的列布局写入矩阵. 任一 double 分量超出有限 float 范围时返回 false.
    template <typename Columns>
    static bool StoreMatrix(Columns &output, const glm::dmat4 &matrix) noexcept
    {
        for (std::uint32_t column = 0; column < 4; ++column)
        {
            for (std::uint32_t row = 0; row < 4; ++row)
            {
                const auto value = matrix[column][row];
                if (!(std::abs(value) <= (std::numeric_limits<float>::max)()))
                {
                    return false;
                }
                output[column].value[row] = static_cast<float>(value);
            }
        }
        return true;
    }

    /// @brief 编码对象与法线矩阵, 以及在图元世界基点附近求光照所需的仿射系数.
    /// @param anchor 已由固定图元的局部范围选出的光照基点.
    /// @return 完整 CPU 对象行或首个失败原因. 调用者可用源图元身份记录被跳过的表面.
    static std::variant<StaticAbi::ForwardObject, std::string_view>
    EncodeObject(const SceneObject &object, const std::array<float, 3> &anchor) noexcept
    {
        constexpr std::string_view invalidFrame = "instance transform cannot form a GPU surface frame";
        StaticAbi::ForwardObject row{};
        const auto worldFromLocal = glm::make_mat4(object.worldFromLocal.data());
        if (object.orientation == 0 || !StoreMatrix(row.worldFromLocal, worldFromLocal))
        {
            return invalidFrame;
        }
        // 选取靠近几何的世界基点并窄化一次. 它只作为光照坐标的原点, 裁剪矩阵沿用现有路径.
        const auto worldAnchor = worldFromLocal * glm::dvec4(anchor[0], anchor[1], anchor[2], 1.0);
        const std::array position{worldAnchor.x, worldAnchor.y, worldAnchor.z};
        if (!StoreGpuVector(row.lightingAnchor, position))
        {
            return "lighting anchor exceeds the GPU position range";
        }

        // A 取对象行里已经舍入的世界基点. 顶点计算 M * p + (T - A), 片元从灯位或眼点减去同一个 A.
        std::array<double, 3> shift{};
        for (std::uint32_t axis = 0; axis < shift.size(); ++axis)
        {
            shift[axis] = worldFromLocal[3][axis] - row.lightingAnchor[axis];
        }
        if (!StoreGpuVector(row.lightingShiftHigh, shift))
        {
            return "lighting translation relative to the anchor exceeds the GPU position range";
        }
        for (std::uint32_t axis = 0; axis < shift.size(); ++axis)
        {
            const auto translation = worldFromLocal[3][axis];
            const auto base = row.lightingAnchor[axis];
            // 低位合并 double 计算 T - A 时的舍入余量, 以及将结果窄化为 FP32 时的余量.
            const auto roundedBase = translation - shift[axis];
            const auto error = (translation - (shift[axis] + roundedBase)) + (roundedBase - base);
            row.lightingShiftLow[axis] =
                static_cast<float>((shift[axis] - row.lightingShiftHigh[axis]) + error);
        }

        // 法线沿用 GPU 矩阵. 光照另存线性系数窄化的余量以恢复仿射顶点位置.
        glm::dmat3 linear(1.0);
        for (std::uint32_t c = 0; c < 3; ++c)
        {
            const auto &column = row.worldFromLocal[c].value;
            linear[c] = glm::dvec3(column[0], column[1], column[2]);
            for (std::uint32_t r = 0; r < 3; ++r)
            {
                row.lightingLinearLow[c].value[r] = static_cast<float>(worldFromLocal[c][r] - linear[c][r]);
            }
        }
        const auto determinant = glm::determinant(linear);
        if (determinant == 0 || (determinant < 0) != (object.orientation < 0))
        {
            return invalidFrame;
        }
        const auto normal = glm::transpose(glm::inverse(linear));
        // 等比缩放法线矩阵保留单位方向.
        double scale = 0.0;
        for (std::uint32_t c = 0; c < 3; ++c)
        {
            for (std::uint32_t r = 0; r < 3; ++r)
            {
                scale = std::max(scale, std::abs(normal[c][r]));
            }
        }
        if (!(scale > 0) || !StoreMatrix(row.normalFromLocal, glm::dmat4(normal / scale)))
        {
            return invalidFrame;
        }
        row.orientation = static_cast<float>(object.orientation);
        return row;
    }

    /// @brief 依次表示可复用的 GPU 行, 尚待分配的 CPU 编码, 或该图元的静态拒绝原因.
    using ObjectPreparation = std::variant<std::shared_ptr<ObjectStorage>, StaticAbi::ForwardObject, std::string_view>;

    /// @brief 在转换矩阵前查已编码行. 冷行先留 CPU 值, 几何可绘后才创建缓冲.
    static ObjectPreparation ResolveObject(const ForwardDevice &device, const ObjectRowKey &key,
                                           const SceneObject &source, ForwardEncodingCounts &encodings)
    {
        if (const auto found = device.objects.find(key);
            found != device.objects.end() && device.Current(*found->second.resident) &&
            source.transformRevision == found->second.transformRevision)
        {
            return found->second.resident;
        }
        ++encodings.objects;
        const auto encoded = EncodeObject(source, key.second);
        if (const auto *failure = std::get_if<std::string_view>(&encoded))
        {
            return *failure;
        }
        return std::get<StaticAbi::ForwardObject>(encoded);
    }

    /// @brief 比较实际 ABI 字段, 避免把 padding 或场景全局版本当成材质变化.
    static bool SameMaterial(const StaticAbi::ForwardMaterial &a, const StaticAbi::ForwardMaterial &b) noexcept
    {
        if (a.baseColor != b.baseColor || a.emissive != b.emissive || a.surface != b.surface ||
            a.normalBasis != b.normalBasis || a.alphaCutoff != b.alphaCutoff || a.masked != b.masked ||
            a.unlit != b.unlit)
        {
            return false;
        }
        for (std::size_t i = 0; i < a.uvX.size(); ++i)
        {
            if (a.uvX[i].value != b.uvX[i].value || a.uvY[i].value != b.uvY[i].value)
            {
                return false;
            }
        }
        return true;
    }

    /// @brief 从已验证的合成材质编码标准因子和每槽 UV 仿射. 缺席用负集合号表示.
    static StaticAbi::ForwardMaterial EncodeMaterial(const Scene::EffectiveMaterial &material) noexcept
    {
        const auto &source = material.values;
        StaticAbi::ForwardMaterial out{};
        out.baseColor = source.baseColor;
        out.emissive = {source.emissive[0], source.emissive[1], source.emissive[2], source.emissiveStrength};
        out.surface = {source.metallic, source.roughness, source.occlusionStrength, source.normalScale};
        out.alphaCutoff = source.alphaCutoff;
        out.masked = source.alphaMode == Asset::ModelAlpha::Mask ? 1u : 0u;
        out.unlit = source.unlit ? 1u : 0u;
        for (std::size_t i = 0; i < material.textures.size(); ++i)
        {
            const auto &texture = material.textures[i];
            if (!texture)
            {
                out.uvX[i].value[3] = -1.f;
                continue;
            }
            const auto &slot = texture->slot;
            const auto c = std::cos(slot.rotationRadians);
            const auto s = std::sin(slot.rotationRadians);
            out.uvX[i].value = {c * slot.scale[0], -s * slot.scale[1], slot.offset[0], float(slot.texCoord)};
            out.uvY[i].value = {s * slot.scale[0], c * slot.scale[1], slot.offset[1], 0.f};
            if (i == NormalSlot)
            {
                // 此预览 profile 用 Q=R*diag(signX,signY) 的逆确定框架朝向.
                // scale 的绝对值只改变采样频率, 零轴沿正向作常量坐标采样, 不在此对完整 UV 仿射求逆.
                const auto sx = slot.scale[0] < 0.f ? -1.f : 1.f;
                const auto sy = slot.scale[1] < 0.f ? -1.f : 1.f;
                out.normalBasis = {sx * c, -sy * s, sx * sy, 0.f};
            }
        }
        return out;
    }

    /// @brief 内嵌图片由模型及源 image 下标解释, 外置图片按自身固定的身份/版本解释.
    static std::shared_ptr<ResidentTexture> PrepareSlot(ForwardDevice &device, UploadBatch &uploads,
        const AssetVersionKey &model, const Scene::EffectiveTexture &texture, bool mips)
    {
        if (const auto *image = std::get_if<const Asset::ModelImage *>(&texture.image))
        {
            return PrepareTexture(device, uploads, {false, model, (*image)->sourceIndex, texture.slot.domain, mips},
                                  (*image)->pixels);
        }
        const auto &image = *std::get<Asset::ImageVersion>(texture.image);
        return PrepareTexture(device, uploads, {true, Version(image.asset, image.revision), 0, texture.slot.domain, mips},
                              image.image);
    }

    /// @brief 取得能按当前覆盖代直接用于路由和绘制的已准备行.
    static const MaterialEntry *FindMaterial(const ForwardDevice &device, const MaterialKey &key,
                                             std::uint64_t revision) noexcept
    {
        const auto found = device.materials.find(key);
        if (found != device.materials.end() && found->second.revision == revision &&
            device.Current(*found->second.resident))
        {
            return &found->second;
        }
        return nullptr;
    }

    /// @brief 由有效纹理槽一次确定几何所需属性和双面状态.
    static MaterialGeometry MaterialRequirements(const Scene::EffectiveMaterial &material) noexcept
    {
        MaterialGeometry result{.doubleSided = material.values.doubleSided};
        for (const auto &slot : material.textures)
        {
            if (slot)
            {
                result.attributes |= slot->slot.texCoord == 0 ? VertexUv0 : VertexUv1;
            }
        }
        if (const auto &normal = material.textures[NormalSlot])
        {
            result.attributes |= VertexTangent;
            result.normalUv = static_cast<std::int8_t>(normal->slot.texCoord);
        }
        return result;
    }

    /// @brief 按表页与完整采样集合共用预解析绑定. 每个纹理代固定拥有一个实际视图和采样范围.
    static void PrepareMaterialBinding(ForwardDevice &device, MaterialRow &row)
    {
        for (std::size_t i = 0; i < row.textures.size(); ++i)
        {
            row.sampling[2 * i] = reinterpret_cast<std::uintptr_t>(row.textures[i].get());
            row.sampling[2 * i + 1] = reinterpret_cast<std::uintptr_t>(row.samplers[i].Get());
        }
        auto &cached = device.materialBindings[{row.storage.page->ordinal, row.sampling}];
        row.binding = cached.lock();
        if (row.binding)
        {
            return;
        }
        // 集合绑定整张材质页. 即使实例的数值行不同, 页和采样组合相同时也可共用该集合.
        RHI::BindingSetDesc bindings;
        bindings.AddItem(RHI::BindingSetItem::StructuredBuffer_SRV(MaterialTableSlot, row.storage.page->buffer.Get()));
        for (std::uint32_t i = 0; i < row.textures.size(); ++i)
        {
            bindings.AddItem(RHI::BindingSetItem::Texture_SRV(i, row.textures[i]->texture.Get())
                .SetSubresources(row.textures[i]->sampling));
            bindings.AddItem(RHI::BindingSetItem::Sampler(i, row.samplers[i].Get()));
        }
        auto binding = device.device.CreateBindingSet(bindings, device.programs.layouts[2].Get());
        if (!binding)
        {
            Reject(ViewSettingReason::ScenePreparationFailed, "Cannot prepare Forward material table binding");
        }
        row.binding = std::make_shared<RHI::BindingSetHandle>(std::move(binding));
        cached = row.binding;
    }

    /// @brief 准备新覆盖代或补齐缓存行未获接受的纹理. 相同字段及绑定沿用原存储.
    static std::shared_ptr<MaterialRow> PrepareMaterial(ForwardDevice &device, ForwardFrameData &frame,
        const MaterialKey &key, std::uint64_t revision, const Scene::EffectiveMaterial &material,
        const MaterialGeometry &geometry, const MaterialRow *cachedRow)
    {
        auto &uploads = frame.uploads;
        const auto values = cachedRow ? cachedRow->values : EncodeMaterial(material);
        if (!cachedRow)
        {
            ++frame.encodings.materials;
        }
        // 覆盖行除 ABI 数值外还比较实际纹理代和 sampler, 同一路径的新像素也须建立新行.
        std::array<std::shared_ptr<ResidentTexture>, Scene::MaterialSlotCount> textures;
        std::array<RHI::SamplerHandle, Scene::MaterialSlotCount> samplers;
        for (std::size_t i = 0; i < textures.size(); ++i)
        {
            const auto &active = material.textures[i];
            const auto sampler = EffectiveSampler(active ? active->sampler : Asset::ModelSampler{});
            samplers[i] = PrepareSampler(device, sampler);
            textures[i] = active ? PrepareSlot(device, uploads, std::get<0>(key), *active, sampler[0] >= 9984)
                                 : WhiteTexture(device, uploads);
        }
        if (const auto found = device.materials.find(key);
            found != device.materials.end() && device.Current(*found->second.resident))
        {
            auto &entry = found->second;
            auto &row = *entry.resident;
            if (SameMaterial(row.values, values) && row.textures == textures && row.samplers == samplers)
            {
                // 这些字段只参与后续准备. 在途 Draw 已保存自己的路由和 Pipeline.
                entry.revision = revision;
                entry.geometry = geometry;
                device.Use(row);
                ++device.materialHits;
                return entry.resident;
            }
        }
        constexpr auto rowBytes = sizeof(StaticAbi::ForwardMaterial);
        device.Reserve(rowBytes, true);
        auto row = std::make_shared<MaterialRow>(device.capacity, rowBytes, true);
        row->values = values;
        row->textures = std::move(textures);
        row->samplers = std::move(samplers);
        device.AllocateSlice(row->storage, *row, GraphUsage::ShaderRead, std::uint8_t(TableLayout::Materials),
                             rowBytes, rowBytes, StaticAbi::MaterialRowsCapacity);
        row->index = static_cast<std::size_t>(row->storage.range.byteOffset / rowBytes);
        PrepareMaterialBinding(device, *row);
        device.Use(*row);
        uploads.Add(row, row->storage, std::as_bytes(std::span(&values, 1)));
        if (const auto previous = device.materials.find(key); previous != device.materials.end())
        {
            previous->second.resident->storage.Retire();
        }
        device.materials.insert_or_assign(key, MaterialEntry{revision, geometry, row});
        ++device.materialGeneration;
        return row;
    }

    /// @brief GPU 顶点只打包选中的流. 静态索引的源空间和绕序保持不变.
    static void PackVertices(std::span<std::byte> target, const Asset::ModelPrimitive &primitive,
                             std::uint8_t attributes, const VertexLayout &layout) noexcept
    {
        for (const auto &vertex : primitive.vertices)
        {
            const auto store = [&](std::size_t slot, const auto &field)
            {
                std::memcpy(target.data() + layout.offsets[slot], field.data(), sizeof(field));
            };
            store(0, vertex.position);
            store(1, vertex.normal);
            store(2, vertex.color);
            if ((attributes & VertexUv0) != 0)
            {
                store(3, vertex.uv[0]);
            }
            if ((attributes & VertexUv1) != 0)
            {
                store(4, vertex.uv[1]);
            }
            if ((attributes & VertexTangent) != 0)
            {
                store(5, vertex.tangent);
            }
            target = target.subspan(layout.stride);
        }
    }

    using GeometryResult = std::variant<std::shared_ptr<GeometryRef>, Core::BoundedText>;

    /// @brief 优先重用同版及同布局几何. 有效 normal UV 变化时用共享导入算法生成新的接缝索引.
    static GeometryResult PrepareGeometry(ForwardDevice &device, UploadBatch &uploads, const GeometryKey &key,
                                          const Asset::ModelPrimitive &source)
    {
        if (const auto found = device.geometry.find(key);
            found != device.geometry.end() && device.Current(*found->second))
        {
            device.Use(*found->second);
            return found->second;
        }
        std::optional<Asset::ModelPrimitive> rebuilt;
        const auto tangentUv = std::get<4>(key);
        if (tangentUv >= 0)
        {
            auto result = Asset::BuildNormalUvGeometry(source, static_cast<std::uint32_t>(tangentUv),
                                                       ForwardUploadBudget);
            if (const auto *error = std::get_if<Asset::AssetFailure>(&result))
            {
                if (error->reason == Asset::AssetError::ModelByteLimit)
                {
                    throw ForwardFailure{ViewSettingReason::SceneResidencyLimit, error->description};
                }
                return error->description;
            }
            rebuilt = std::get<Asset::ModelPrimitive>(std::move(result));
        }
        const auto &primitive = rebuilt ? *rebuilt : source;
        const auto attributes = std::get<3>(key);
        const auto layout = StaticVertexLayout(attributes);
        const auto indexBytes = std::as_bytes(std::span(primitive.indices));
        if (primitive.vertices.size() > ForwardBufferLimit / layout.stride || indexBytes.size() > ForwardBufferLimit)
        {
            Reject(ViewSettingReason::SceneResidencyLimit, "Primitive vertex or index storage exceeds the buffer limit");
        }
        const auto vertexBytes = primitive.vertices.size() * layout.stride;
        uploads.ReserveWrites(vertexBytes + indexBytes.size(), 2);
        device.Reserve(vertexBytes + indexBytes.size(), true);
        auto geometry = std::make_shared<GeometryRef>(device.capacity, vertexBytes + indexBytes.size(), true);
        geometry->key = key;
        geometry->indexCount = static_cast<std::uint32_t>(primitive.indices.size());
        // 共享顶点绑定取页起点, 页内区间须能换算成整数 baseVertex.
        device.AllocateSlice(geometry->vertices, *geometry, GraphUsage::VertexBuffer, attributes, vertexBytes,
                             layout.stride);
        device.AllocateSlice(geometry->indices, *geometry, GraphUsage::IndexBuffer, 0, indexBytes.size(),
                             sizeof(std::uint32_t));
        std::vector<std::byte> packed(vertexBytes);
        PackVertices(packed, primitive, attributes, layout);
        const auto &vertices = geometry->vertices;
        const auto &indices = geometry->indices;
        // 两项写入共用几何代, 各自携带目的页偏移. 确认时须同时找到顶点和索引的生产 Pass.
        uploads.buffers.push_back({geometry, vertices.page->buffer, std::move(packed),
                                   GraphUploadSource::ForwardStatic, vertices.range.byteOffset, vertices.page.get()});
        uploads.buffers.push_back({geometry, indices.page->buffer, {indexBytes.begin(), indexBytes.end()},
                                   GraphUploadSource::ForwardStatic, indices.range.byteOffset, indices.page.get()});
        device.Use(*geometry);
        if (const auto previous = device.geometry.find(key); previous != device.geometry.end())
        {
            previous->second->Retire();
        }
        device.geometry.insert_or_assign(key, geometry);
        return geometry;
    }

    /// @brief 在有界信息中记录跳过项的源身份. 混合 profile 仍可绘制已支持部分.
    static void DescribeSkipped(ForwardFrameData &frame, const Asset::ModelRecord &model, const PrimitiveRef &ref,
                                std::string_view reason)
    {
        if (!frame.degradation)
        {
            frame.degradation.emplace();
        }
        else
        {
            frame.degradation->Append(" | ");
        }
        auto &text = *frame.degradation;
        if (text.WasTruncated())
        {
            return;
        }
        text.Append("Skipped glTF mesh ");
        text.Append(std::to_string(model.meshes[ref.mesh].sourceIndex));
        text.Append(" primitive ");
        text.Append(std::to_string(ref.sourceSlot));
        text.Append(": ");
        text.Append(reason);
    }

    /// @brief 编码当前相机, 请求模式及活动灯表的两段计数. 只在槽安全且字段已改变或上传未接受时排入写入.
    static void PrepareViewRow(ForwardDevice &device, ForwardFrameData &frame, const PreparedView &view,
                               PreviewMode mode, bool usesLighting)
    {
        StaticAbi::ForwardView value{};
        if (!StoreMatrix(value.clipFromWorld, glm::make_mat4(view.camera->renderClipFromView.data()) *
                                              glm::make_mat4(view.camera->viewFromWorld.data())))
        {
            Reject(ViewSettingReason::InvalidProjection, "Forward clip transform exceeds the GPU float range");
        }
        value.mode = static_cast<std::uint32_t>(mode);
        value.preExposure = view.camera->exposure.preExposure;
        value.directionalCount = frame.activeLights->directionalCount;
        value.localCount = frame.activeLights->localCount;
        if (usesLighting)
        {
            const std::span basis(view.camera->worldFromView);
            value.orthographic = view.camera->orthographic ? 1u : 0u;
            // 透视使用眼点, 正交使用相机世界 +Z, 即朝向观察者的固定单位向量.
            const auto viewer = view.camera->orthographic ? basis.subspan<8, 3>() : basis.subspan<12, 3>();
            if (!StoreGpuVector(value.viewer, viewer))
            {
                Reject(ViewSettingReason::InvalidProjection, "Forward viewer exceeds the GPU float range");
            }
            if (!view.camera->orthographic)
            {
                value.viewerLow = GpuPositionLow(viewer, value.viewer);
            }
        }
        if (!frame.view)
        {
            frame.view = CreateConstantRow(device, sizeof(value), 0);
        }
        device.Use(*frame.view);
        const auto same = [&]
        {
            // 曝光, 灯计数或观察向量均可独立改变. 比较只读取实际字段.
            if (!frame.viewValues || frame.viewValues->mode != value.mode ||
                frame.viewValues->preExposure != value.preExposure || frame.viewValues->viewer != value.viewer ||
                frame.viewValues->viewerLow != value.viewerLow ||
                frame.viewValues->orthographic != value.orthographic ||
                frame.viewValues->directionalCount != value.directionalCount ||
                frame.viewValues->localCount != value.localCount)
            {
                return false;
            }
            for (std::size_t i = 0; i < value.clipFromWorld.size(); ++i)
            {
                if (value.clipFromWorld[i].value != frame.viewValues->clipFromWorld[i].value)
                {
                    return false;
                }
            }
            return true;
        };
        if (!frame.view->accepted || !same())
        {
            // 先撤销已上传资格. 图未接受或上传准备失败时保留 false, 本槽安全复用后仍会重试该行.
            frame.view->accepted = false;
            frame.viewValues = value;
            frame.uploads.Add(frame.view, frame.view->buffer, std::as_bytes(std::span(&value, 1)),
                              GraphUploadSource::ForwardView);
        }
    }

    /// @brief 准备当前可见 Primitive 的几何和材质行. 记录光栅键, 颜色管线在全部可绘材质确定后选择.
    static void PrepareDraw(ForwardDevice &device, ForwardFrameData &frame, const PreparedView &view,
        std::uint32_t index, std::map<MaterialRow *, std::size_t> &rows)
    {
        const auto &ref = view.scene.primitives[index];
        const auto &model = *view.scene.models[ref.model];
        const auto &source = model.meshes[ref.mesh].primitives[ref.sourceSlot];
        const auto &object = view.scene.objects[ref.object];
        const auto version = Version(model.asset, model.revision);
        const auto entity = ObjectIdentity(view.scene.scene, object.entity);
        const MaterialKey key{version, ref.material, ref.materialOverride ? entity : ObjectKey{},
                              ref.materialOverride ? ref.sourceSlot : 0};
        ++device.materialRequests;
        const auto *cached = FindMaterial(device, key, ref.materialRevision);
        std::shared_ptr<MaterialRow> row;
        std::optional<Scene::EffectiveMaterial> sourceMaterial;
        const Scene::EffectiveMaterial *material = nullptr;
        MaterialGeometry requirements;
        if (cached)
        {
            requirements = cached->geometry;
            row = cached->resident;
        }
        // 深度图可能只确认材质行和基色纹理. 其余槽须各自有已接受内容或本批生产者, 才能直接使用原集合.
        const bool texturesReady = row && std::ranges::all_of(row->textures, [&](const auto &texture)
        {
            return frame.uploads.HasMaterialTexture(*texture);
        });
        if (!texturesReady)
        {
            if (ref.materialOverride)
            {
                material = &view.scene.materialOverrides[*ref.materialOverride];
            }
            else
            {
                material = &sourceMaterial.emplace(Scene::ResolveMaterial(model, source, nullptr));
            }
            if (!row && material->values.alphaMode == Asset::ModelAlpha::Blend)
            {
                DescribeSkipped(frame, model, ref, "alphaMode BLEND is unsupported");
                return;
            }
            if (!row)
            {
                requirements = MaterialRequirements(*material);
            }
        }

        // 固定源局部基点构成缓存键. 世界 FP32 基点及余量只在对象真实换代时编码.
        assert(source.bounds);
        std::array<float, 3> anchor{};
        for (std::size_t axis = 0; axis < anchor.size(); ++axis)
        {
            anchor[axis] = std::midpoint(source.bounds->minimum[axis], source.bounds->maximum[axis]);
        }
        const ObjectRowKey objectKey{entity, anchor};
        auto objectCandidate = ResolveObject(device, objectKey, object, frame.encodings);
        if (const auto *error = std::get_if<std::string_view>(&objectCandidate))
        {
            DescribeSkipped(frame, model, ref, *error);
            return;
        }
        const auto attributes = requirements.attributes;
        std::int8_t tangentUv = -1;
        // 原切线只有在记录的 UV 与有效 normal 槽匹配时复用, 替换框架另建几何而不修改资产.
        const auto &sourceNormal = model.materials[ref.material].slots[NormalSlot];
        if (requirements.normalUv >= 0 &&
            (!source.hasTangent || !sourceNormal || sourceNormal->texCoord != std::uint32_t(requirements.normalUv)))
        {
            tangentUv = requirements.normalUv;
        }
        auto geometry = PrepareGeometry(device, frame.uploads, {version, ref.mesh, ref.sourceSlot, attributes, tangentUv},
                                        source);
        if (const auto *error = std::get_if<Core::BoundedText>(&geometry))
        {
            DescribeSkipped(frame, model, ref, error->View());
            return;
        }
        std::shared_ptr<ObjectStorage> instance;
        if (auto *resident = std::get_if<std::shared_ptr<ObjectStorage>>(&objectCandidate))
        {
            instance = std::move(*resident);
            device.Use(*instance);
        }
        else
        {
            const auto &encoded = std::get<StaticAbi::ForwardObject>(objectCandidate);
            device.Reserve(sizeof(encoded), true);
            instance = std::make_shared<ObjectStorage>(device.capacity, sizeof(encoded), true);
            device.AllocateSlice(instance->storage, *instance, GraphUsage::ShaderRead,
                std::uint8_t(TableLayout::Objects), sizeof(encoded), sizeof(encoded), StaticAbi::ObjectRowsCapacity);
            device.Use(*instance);
            frame.uploads.Add(instance, instance->storage, std::as_bytes(std::span(&encoded, 1)));
            if (const auto previous = device.objects.find(objectKey); previous != device.objects.end())
            {
                previous->second.resident->storage.Retire();
            }
            device.objects.insert_or_assign(objectKey, ObjectRow{object.transformRevision, instance});
        }
        if (texturesReady)
        {
            // 快速命中跳过了逐槽准备. 本 View 的驻留账单仍须包含绑定集合引用的每个纹理代.
            ++device.materialHits;
            for (const auto &texture : row->textures)
            {
                device.Use(*texture);
            }
            device.Use(*row);
        }
        else
        {
            row = PrepareMaterial(device, frame, key, ref.materialRevision, *material, requirements, row.get());
        }
        const auto [slot, newRow] = rows.try_emplace(row.get(), frame.materialRows.size());
        if (newRow)
        {
            frame.materialRows.push_back(std::move(row));
        }
        const auto phase = frame.needs.earlyDepth ? SurfacePhase::ReadOnlyDepth : SurfacePhase::Combined;
        const bool mirror = object.orientation < 0;
        const bool doubleSided = requirements.doubleSided;
        const auto &matrix = view.camera->viewFromWorld;
        const auto &bounds = *ref.bounds;
        auto z = matrix[14];
        for (std::size_t i = 0; i < 3; ++i)
        {
            z += matrix[4 * i + 2] * (bounds.minimum[i] * 0.5 + bounds.maximum[i] * 0.5);
        }
        if (!std::isfinite(z))
        {
            Reject(ViewSettingReason::ScenePreparationFailed, "Forward sort depth is not finite");
        }
        frame.draws.push_back({std::get<std::shared_ptr<GeometryRef>>(std::move(geometry)), std::move(instance),
            slot->second, {}, {}, PipelineKey(attributes, mirror, doubleSided, phase),
            -z, index, attributes, mirror, doubleSided});
    }

    /// @brief 给实际采样集合编号用于排序. 相同冻结输入以首次可见顺序编号, 资源地址只参与相等查找.
    static void SortDraws(ForwardFrameData &frame)
    {
        std::map<SamplingKey, std::size_t> groups;
        std::vector<std::size_t> samplingOrder;
        samplingOrder.reserve(frame.materialRows.size());
        for (const auto &row : frame.materialRows)
        {
            samplingOrder.push_back(groups.try_emplace(row->sampling, groups.size()).first->second);
        }
        const auto order = [&](const DrawRecord &draw)
        {
            return std::tie(draw.pipelineKey, samplingOrder[draw.material], draw.geometry->vertices.page->ordinal,
                            draw.geometry->indices.page->ordinal, draw.viewDepth, draw.source);
        };
        std::ranges::sort(frame.draws, [&](const DrawRecord &a, const DrawRecord &b)
        {
            return order(a) < order(b);
        });
    }

    /// @brief 比较同一 View 和实例块内仍可能变化的状态. 两个 Pipeline 含布局和光栅状态,
    /// 材质集合含表页和采样组合. 同一顶点页固定布局和步长, 本路径的索引页固定为 R32.
    static bool SameBatch(const ForwardFrameData &frame, const DrawRecord &a, const DrawRecord &b) noexcept
    {
        return a.pipeline == b.pipeline && a.depthPipeline == b.depthPipeline &&
               a.geometry->vertices.page == b.geometry->vertices.page &&
               a.geometry->indices.page == b.geometry->indices.page &&
               a.object->storage.page == b.object->storage.page &&
               frame.materialRows[a.material]->binding == frame.materialRows[b.material]->binding;
    }

    /// @brief 将已验证的页区间换算为元素单位. 首实例是当前绑定块内的行号.
    [[nodiscard]] static RHI::DrawIndexedIndirectArguments EncodeCommand(const GeometryRef &geometry,
        std::uint32_t instance, std::uint32_t count)
    {
        const auto stride = StaticVertexLayout(geometry.vertices.page->layout).stride;
        const auto vertexOffset = geometry.vertices.range.byteOffset;
        const auto indexOffset = geometry.indices.range.byteOffset;
        const auto baseVertex = vertexOffset / stride;
        const auto firstIndex = indexOffset / sizeof(std::uint32_t);
        if (vertexOffset % stride != 0 || indexOffset % sizeof(std::uint32_t) != 0 ||
            baseVertex > std::uint64_t(std::numeric_limits<std::int32_t>::max()) ||
            firstIndex > std::numeric_limits<std::uint32_t>::max())
        {
            Reject(ViewSettingReason::SceneResidencyLimit, "Forward indirect geometry offsets are not representable");
        }
        return {geometry.indexCount, count, static_cast<std::uint32_t>(firstIndex),
                static_cast<std::int32_t>(baseVertex), instance};
    }

    /// @brief 在当前安全 View 槽复用等形缓冲. 已接受且字节相同的版本可直接导入后继图.
    static void PrepareDrawBuffer(ForwardDevice &device, ForwardFrameData &frame,
        std::shared_ptr<DrawBuffer> &storage, GraphUsage usage, std::span<const std::byte> values,
        std::uint32_t stride = 0)
    {
        if (storage && (storage->capacity != device.capacity || storage->bytes != values.size()))
        {
            storage.reset();
        }
        if (!storage)
        {
            device.Reserve(values.size());
            auto next = std::make_shared<DrawBuffer>(device.capacity, values.size());
            next->buffer = AllocateBuffer(device.device, usage, values.size(), stride);
            storage = std::move(next);
        }
        device.Use(*storage);
        if (!storage->accepted || !std::ranges::equal(values, storage->contents))
        {
            storage->accepted = false;
            frame.uploads.Add(storage, storage->buffer, values, GraphUploadSource::ForwardDraw);
            storage->contents.assign(values.begin(), values.end());
        }
    }

    /// @brief 为有序表面分配实例块并编码紧排的参数. 大工作集按块分段, 避免跨越 Shader 表容量.
    /// @pre draws 已排序且非空. 生成的首实例相对于绑定块, 参数偏移相对于整份 View 参数缓冲.
    static void PackBatches(ForwardDevice &device, ForwardFrameData &frame)
    {
        constexpr auto blockCapacity = StaticAbi::DrawInstancesCapacity;
        using Argument = RHI::DrawIndexedIndirectArguments;
        std::vector<Argument> arguments;
        const auto drawCount = frame.draws.size();
        const auto blockCount = (drawCount - 1) / blockCapacity + 1;
        frame.instanceBlocks.resize(blockCount);
        std::array<StaticAbi::DrawInstanceData, blockCapacity> instances{};
        for (std::size_t block = 0; block < blockCount; ++block)
        {
            const auto first = block * blockCapacity;
            const auto count = std::min(blockCapacity, drawCount - first);
            const auto draws = std::span(frame.draws).subspan(first, count);
            for (std::size_t i = 0; i < draws.size(); ++i)
            {
                const auto &draw = draws[i];
                const auto objectRow = draw.object->storage.range.byteOffset / sizeof(StaticAbi::ForwardObject);
                const auto materialRow = frame.materialRows[draw.material]->index;
                assert(objectRow < StaticAbi::ObjectRowsCapacity && materialRow < StaticAbi::MaterialRowsCapacity);
                instances[i] = {static_cast<std::uint32_t>(objectRow), static_cast<std::uint32_t>(materialRow)};
            }
            // Shader 绑定整个定长块. 末块的尾部也随载荷定义, 只有前 count 行可被命令索引.
            std::fill(instances.begin() + count, instances.end(), StaticAbi::DrawInstanceData{});
            auto &storage = frame.instanceBlocks[block];
            PrepareDrawBuffer(device, frame, storage, GraphUsage::ShaderRead, std::as_bytes(std::span(instances)),
                              sizeof(StaticAbi::DrawInstanceData));
            std::map<std::uint64_t, RHI::BindingSetHandle> sets;
            for (std::size_t begin = 0; begin < draws.size();)
            {
                const auto &head = draws[begin];
                const auto &objects = *head.object->storage.page;
                auto &renderables = sets[objects.ordinal];
                if (!renderables)
                {
                    RHI::BindingSetDesc bindings;
                    bindings.AddItem(RHI::BindingSetItem::StructuredBuffer_SRV(0, objects.buffer.Get()));
                    bindings.AddItem(RHI::BindingSetItem::StructuredBuffer_SRV(1, storage->buffer.Get()));
                    renderables = device.device.CreateBindingSet(bindings, device.programs.layouts[1].Get());
                    if (!renderables)
                    {
                        Reject(ViewSettingReason::ScenePreparationFailed, "Cannot prepare Forward instance binding");
                    }
                }
                const auto firstCommand = static_cast<std::uint32_t>(arguments.size());
                auto current = begin;
                // 相邻的同段几何扩展 instanceCount. 换几何时追加记录, 直到需要切换绑定或 Pipeline.
                do
                {
                    const auto &geometry = draws[current].geometry;
                    auto end = current + 1;
                    while (end < draws.size() && draws[end].geometry == geometry &&
                           SameBatch(frame, head, draws[end]))
                    {
                        ++end;
                    }
                    if (arguments.size() == ForwardBufferLimit / sizeof(Argument))
                    {
                        Reject(ViewSettingReason::SceneUploadLimit, "Forward indirect arguments exceed the buffer limit");
                    }
                    arguments.push_back(EncodeCommand(*geometry, static_cast<std::uint32_t>(current),
                                                     static_cast<std::uint32_t>(end - current)));
                    current = end;
                } while (current < draws.size() && SameBatch(frame, head, draws[current]));
                frame.batches.push_back({first + begin, block, firstCommand,
                    static_cast<std::uint32_t>(arguments.size()) - firstCommand, renderables});
                begin = current;
            }
        }
        PrepareDrawBuffer(device, frame, frame.indirect, GraphUsage::IndirectArgument,
                          std::as_bytes(std::span(arguments)));
    }

    void PrepareDraws(ForwardDevice &device, ForwardFrameData &frame, const PreparedView &view, PreviewMode mode)
    {
        // 先收集几何和材质写入, 据实际可绘项选择后续灯表和 View 常量. 各路准备结束后统一封存.
        std::map<MaterialRow *, std::size_t> rows;
        for (const auto index : view.primitives)
        {
            PrepareDraw(device, frame, view, index, rows);
        }
        if (frame.draws.empty() && frame.degradation)
        {
            throw ForwardFailure{ViewSettingReason::SceneUnsupported, *frame.degradation};
        }
        const bool litDraws = std::ranges::any_of(frame.materialRows,
            [](const auto &row) { return row->values.unlit == 0; });
        PrepareEnvironment(device, frame, view, mode, litDraws);
        if (frame.draws.empty())
        {
            return;
        }
        // 知道实际可绘材质后才选择活动资源形状, 纯 Unlit 工作集不需要 DFG 或环境照明.
        const auto kind = frame.environment.program;
        device.programs.PrepareLighting(device.device, kind);
        frame.lightLayout = device.programs.lightLayouts[std::size_t(kind)];
        const auto phase = frame.needs.earlyDepth ? SurfacePhase::ReadOnlyDepth : SurfacePhase::Combined;
        for (auto &draw : frame.draws)
        {
            draw.pipeline = device.programs.GeometryPipeline(device.device, draw.attributes, draw.mirrored,
                                                             draw.doubleSided, phase, kind);
            if (frame.needs.earlyDepth)
            {
                draw.depthPipeline = device.programs.GeometryPipeline(device.device, draw.attributes, draw.mirrored,
                                                                      draw.doubleSided, SurfacePhase::Depth, kind);
            }
        }
        const bool usesLighting = litDraws && (mode == PreviewMode::DirectLighting || mode == PreviewMode::PhysicalLighting);
        PrepareLightTable(device, frame, view, usesLighting);
        PrepareViewRow(device, frame, view, mode, usesLighting);
        SortDraws(frame);
        PackBatches(device, frame);
    }
}
