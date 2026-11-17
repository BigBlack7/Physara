#include "../Materials/StaticGpu.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>

#include <glm/gtc/type_ptr.hpp>

#include <Asset/StaticTangents.hpp>

/// @file
/// @brief 使用 PreparedView 构造可直接录制的静态 Draw. 本阶段仅分配对象并拥有待写字节.

namespace Physara::Renderer::Detail
{
    /// @brief 资源键只由发布身份组成, 不使用路径、进程内地址或绘制顺序.
    static AssetVersionKey Version(const Asset::AssetId &id, Asset::ContentRevision revision) noexcept
    {
        return {id.index, id.generation, revision.value};
    }

    /// @brief 对象行以 Scene 和实体的完整分配代隔离, 文档替换或 ECS 槽复用不会命中旧行.
    static ObjectKey ObjectIdentity(Scene::SceneId scene, Scene::EntityHandle entity) noexcept
    {
        return {scene.index, scene.generation, entity.index, entity.generation};
    }

    /// @brief 生成布局中的矩阵列支持显式 std140 打包. 检查 double 到 GPU float 的窄化.
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

    /// @brief 用真正送入 GPU 的线性变换计算法线矩阵. 等比缩放逆转置不会改变单位方向.
    static std::optional<StaticAbi::ForwardObject> EncodeObject(const SceneObject &object) noexcept
    {
        StaticAbi::ForwardObject row{};
        if (object.orientation == 0 || !StoreMatrix(row.worldFromLocal, glm::make_mat4(object.worldFromLocal.data())))
        {
            return std::nullopt;
        }
        glm::dmat3 linear(1.0);
        for (std::uint32_t c = 0; c < 3; ++c)
        {
            const auto &column = row.worldFromLocal[c].value;
            linear[c] = glm::dvec3(column[0], column[1], column[2]);
        }
        const auto determinant = glm::determinant(linear);
        if (determinant == 0 || (determinant < 0) != (object.orientation < 0))
        {
            return std::nullopt;
        }
        const auto normal = glm::transpose(glm::inverse(linear));
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
            return std::nullopt;
        }
        row.orientation = static_cast<float>(object.orientation);
        return row;
    }

    /// @brief 只改相机时重用既有对象行. 变换变化在新行写入, 原行保留给在途槽.
    static std::shared_ptr<ConstantRow> PrepareObject(ForwardDevice &device, UploadBatch &uploads,
        const ObjectKey &key, const SceneObject &source, const StaticAbi::ForwardObject &value)
    {
        if (const auto found = device.objects.find(key);
            found != device.objects.end() && device.Current(*found->second.resident) &&
            source.worldFromLocal == found->second.worldFromLocal)
        {
            device.Use(*found->second.resident);
            return found->second.resident;
        }
        auto row = CreateConstantRow(device, sizeof(value), 1);
        uploads.Add(row, row->buffer, std::as_bytes(std::span(&value, 1)));
        device.objects.insert_or_assign(key, ObjectRow{source.worldFromLocal, row});
        return row;
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

    /// @brief 创建一次有效材质集合, 后继 Draw 直接使用已解析的行与纹理/采样器.
    static std::shared_ptr<MaterialRow> PrepareMaterial(ForwardDevice &device, UploadBatch &uploads,
        const MaterialKey &key, const Scene::EffectiveMaterial &material)
    {
        if (std::get<2>(key)[1] == 0)
        {
            // 共享源材质完全由固定模型代决定. 命中时可直接保留绑定依赖.
            const auto found = device.materials.find(key);
            if (found != device.materials.end() && device.Current(*found->second))
            {
                for (const auto &texture : found->second->textures)
                {
                    device.Use(*texture);
                }
                device.Use(*found->second);
                return found->second;
            }
        }
        const auto values = EncodeMaterial(material);
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
            found != device.materials.end() && device.Current(*found->second))
        {
            auto &row = *found->second;
            if (SameMaterial(row.values, values) && row.textures == textures && row.samplers == samplers)
            {
                device.Use(row);
                return found->second;
            }
        }
        device.Reserve(sizeof(values));
        auto row = std::make_shared<MaterialRow>(device.capacity, sizeof(values));
        row->values = values;
        row->textures = std::move(textures);
        row->samplers = std::move(samplers);
        row->buffer = AllocateBuffer(device.device, GraphUsage::ConstantBuffer, sizeof(values));
        RHI::BindingSetDesc bindings;
        bindings.AddItem(RHI::BindingSetItem::ConstantBuffer(0, row->buffer.Get()));
        for (std::uint32_t i = 0; i < row->textures.size(); ++i)
        {
            bindings.AddItem(RHI::BindingSetItem::Texture_SRV(i, row->textures[i]->texture.Get()));
            bindings.AddItem(RHI::BindingSetItem::Sampler(i, row->samplers[i].Get()));
        }
        row->binding = device.device.CreateBindingSet(bindings, device.programs.layouts[2].Get());
        if (!row->binding)
        {
            Reject(ViewSettingReason::ScenePreparationFailed, "Cannot prepare effective material binding");
        }
        device.Use(*row);
        uploads.Add(row, row->buffer, std::as_bytes(std::span(&values, 1)));
        device.materials.insert_or_assign(key, row);
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
        device.Reserve(vertexBytes + indexBytes.size());
        auto geometry = std::make_shared<GeometryRef>(device.capacity, vertexBytes + indexBytes.size());
        geometry->key = key;
        geometry->indexCount = static_cast<std::uint32_t>(primitive.indices.size());
        geometry->vertices = AllocateBuffer(device.device, GraphUsage::VertexBuffer, vertexBytes);
        geometry->indices = AllocateBuffer(device.device, GraphUsage::IndexBuffer, indexBytes.size());
        std::vector<std::byte> packed(vertexBytes);
        PackVertices(packed, primitive, attributes, layout);
        uploads.buffers.push_back({geometry, geometry->vertices, std::move(packed)});
        uploads.buffers.push_back({geometry, geometry->indices, {indexBytes.begin(), indexBytes.end()}});
        device.Use(*geometry);
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

    /// @brief 帧槽的 per-view 行在安全复用后更新. 比例、模式或相机变化只改这份常量数据.
    static void PrepareViewRow(ForwardDevice &device, ForwardFrameData &frame, const PreparedView &view,
                               PreviewMode mode)
    {
        StaticAbi::ForwardView value{};
        if (!StoreMatrix(value.clipFromWorld, glm::make_mat4(view.camera->renderClipFromView.data()) *
                                              glm::make_mat4(view.camera->viewFromWorld.data())))
        {
            Reject(ViewSettingReason::InvalidProjection, "Forward clip transform exceeds the GPU float range");
        }
        value.mode = static_cast<std::uint32_t>(mode);
        if (!frame.view)
        {
            frame.view = CreateConstantRow(device, sizeof(value), 0);
        }
        device.Use(*frame.view);
        const auto same = [&]
        {
            if (!frame.viewValues || frame.viewValues->mode != value.mode)
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
            frame.view->accepted = false;
            frame.viewValues = value;
            frame.uploads.Add(frame.view, frame.view->buffer, std::as_bytes(std::span(&value, 1)));
        }
    }

    /// @brief 准备当前可见 Primitive 的固定布局、索引、材质行和 Pipeline.
    static void PrepareDraw(ForwardDevice &device, ForwardFrameData &frame, const PreparedView &view,
        std::uint32_t index, std::map<MaterialRow *, std::size_t> &rows)
    {
        const auto &ref = view.scene.primitives[index];
        const auto &model = *view.scene.models[ref.model];
        const auto &source = model.meshes[ref.mesh].primitives[ref.sourceSlot];
        const auto &object = view.scene.objects[ref.object];
        auto material = ref.materialOverride ? view.scene.materialOverrides[*ref.materialOverride]
                                             : Scene::ResolveMaterial(model, source, nullptr);
        if (material.values.alphaMode == Asset::ModelAlpha::Blend)
        {
            DescribeSkipped(frame, model, ref, "alphaMode BLEND is unsupported");
            return;
        }
        const auto objectValue = EncodeObject(object);
        if (!objectValue)
        {
            DescribeSkipped(frame, model, ref, "instance transform cannot form a GPU surface frame");
            return;
        }
        std::uint8_t attributes = 0;
        for (const auto &slot : material.textures)
        {
            if (slot)
            {
                assert(slot->slot.texCoord < source.hasUv.size() && source.hasUv[slot->slot.texCoord]);
                attributes |= slot->slot.texCoord == 0 ? VertexUv0 : VertexUv1;
            }
        }
        std::int8_t tangentUv = -1;
        // 原切线只有在记录的 UV 与有效 normal 槽匹配时复用, 替换框架另建几何而不修改资产.
        if (const auto &normal = material.textures[NormalSlot])
        {
            attributes |= VertexTangent;
            if (!source.hasTangent || material.sourceTangentUv != normal->slot.texCoord)
            {
                tangentUv = static_cast<std::int8_t>(normal->slot.texCoord);
            }
        }
        const auto version = Version(model.asset, model.revision);
        auto geometry = PrepareGeometry(device, frame.uploads, {version, ref.mesh, ref.sourceSlot, attributes, tangentUv},
                                        source);
        if (const auto *error = std::get_if<Core::BoundedText>(&geometry))
        {
            DescribeSkipped(frame, model, ref, error->View());
            return;
        }
        const auto entity = ObjectIdentity(view.scene.scene, object.entity);
        auto instance = PrepareObject(device, frame.uploads, entity, object, *objectValue);
        auto row = PrepareMaterial(device, frame.uploads, {version, ref.material, ref.materialOverride ? entity :
            ObjectKey{}, ref.materialOverride ? ref.sourceSlot : 0}, material);
        const auto [slot, newRow] = rows.try_emplace(row.get(), frame.materialRows.size());
        if (newRow)
        {
            frame.materialRows.push_back(std::move(row));
        }
        const auto phase = frame.needs.earlyDepth ? SurfacePhase::ReadOnlyDepth : SurfacePhase::Combined;
        const bool mirror = object.orientation < 0;
        const bool doubleSided = material.values.doubleSided;
        auto pipeline = device.programs.GeometryPipeline(device.device, attributes, mirror, doubleSided, phase);
        RHI::GraphicsPipelineHandle depth;
        if (frame.needs.earlyDepth)
        {
            depth = device.programs.GeometryPipeline(device.device, attributes, mirror, doubleSided, SurfacePhase::Depth);
        }
        const auto &matrix = view.camera->viewFromWorld;
        const auto &bounds = *ref.bounds;
        auto z = matrix[14];
        for (std::size_t i = 0; i < 3; ++i)
        {
            z += matrix[4 * i + 2] * (bounds.minimum[i] * 0.5 + bounds.maximum[i] * 0.5);
        }
        frame.draws.push_back({std::get<std::shared_ptr<GeometryRef>>(std::move(geometry)), std::move(instance),
            slot->second, std::move(pipeline), std::move(depth), PipelineKey(attributes, mirror, doubleSided, phase),
            -z, index});
    }

    void PrepareDraws(ForwardDevice &device, ForwardFrameData &frame, const PreparedView &view, PreviewMode mode)
    {
        // 先封存所有可绘项及静态上传, 再追加 per-view 常量; 后续图执行不再解析活动 Scene.
        std::map<MaterialRow *, std::size_t> rows;
        for (const auto index : view.primitives)
        {
            PrepareDraw(device, frame, view, index, rows);
        }
        if (frame.draws.empty())
        {
            if (frame.degradation)
            {
                throw ForwardFailure{ViewSettingReason::SceneUnsupported, *frame.degradation};
            }
            return;
        }
        PrepareViewRow(device, frame, view, mode);
        // 两种深度组合使用同一顺序, 相等深度下保留相同的绘制优先级.
        std::ranges::sort(frame.draws, [](const DrawRecord &a, const DrawRecord &b)
        {
            return std::tie(a.pipelineKey, a.material, a.geometry->key, a.viewDepth, a.source) <
                   std::tie(b.pipelineKey, b.material, b.geometry->key, b.viewDepth, b.source);
        });
    }
}
