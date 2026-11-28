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
/// @brief 从同一 SceneFrame 为主表面和阴影投射物准备静态行, 实例块及间接参数.
/// 每个阴影子视图单独分批, 各层共享几何和材质驻留. 同槽各路的待写字节由一个上传批持有.

namespace Physara::Renderer::Detail
{
    /// @brief Scene 和实体的完整分配代隔离资源, 防止文档替换或 ECS 槽复用后命中旧行.
    static ObjectKey ObjectIdentity(Scene::SceneId scene, Scene::EntityHandle entity) noexcept
    {
        return {scene.index, scene.generation, entity.index, entity.generation};
    }

    /// @brief 按生成接口的列布局写入矩阵. 任一 double 分量超出有限 float 范围时返回 false.
    template <typename Columns, glm::length_t Size>
    static bool StoreMatrix(Columns &output, const glm::mat<Size, Size, double> &matrix) noexcept
    {
        for (glm::length_t column = 0; column < Size; ++column)
        {
            for (glm::length_t row = 0; row < Size; ++row)
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

    /// @brief 比较光栅所用的投影与三段原点. 仅尾数变化也会使安全帧槽中的旧常量失效.
    static bool SameRasterTransform(const StaticAbi::ForwardView &a, const StaticAbi::ForwardView &b) noexcept
    {
        return a.originHigh == b.originHigh && a.originLow == b.originLow && a.originResidual == b.originResidual &&
            std::ranges::equal(a.clipFromOrigin, b.clipFromOrigin,
                               [](const auto &left, const auto &right) { return left.value == right.value; });
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
        // 选取靠近几何的世界基点并窄化一次. 光栅、眼点和局部灯从同一基点消去世界平移.
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
            a.unlit != b.unlit || a.ior != b.ior || a.specular != b.specular || a.clearcoat != b.clearcoat ||
            a.coatNormalBasis != b.coatNormalBasis || a.sheen != b.sheen)
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
        out.ior = source.ior;
        out.specular = {source.specularColorFactor[0], source.specularColorFactor[1], source.specularColorFactor[2],
                        source.specularFactor};
        out.clearcoat = {source.clearcoatFactor, source.clearcoatRoughnessFactor, source.clearcoatNormalScale,
                         material.iorSpecular ? 1.f : 0.f};
        out.sheen = {source.sheenColorFactor[0], source.sheenColorFactor[1], source.sheenColorFactor[2],
                     source.sheenRoughnessFactor};
        static_assert(out.uvX.size() == Scene::MaterialSlotCount && out.uvY.size() == Scene::MaterialSlotCount);
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
            if (i == NormalSlot || i == CoatNormalSlot)
            {
                // 此预览 profile 用 Q=R*diag(signX,signY) 的逆确定框架朝向.
                // scale 的绝对值只改变采样频率, 零轴沿正向作常量坐标采样, 不在此对完整 UV 仿射求逆.
                const auto sx = slot.scale[0] < 0.f ? -1.f : 1.f;
                const auto sy = slot.scale[1] < 0.f ? -1.f : 1.f;
                auto &basis = i == NormalSlot ? out.normalBasis : out.coatNormalBasis;
                basis = {sx * c, -sy * s, sx * sy, 0.f};
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

    /// @brief 由有效纹理槽一次确定各表面角色所需的属性和双面状态.
    static MaterialGeometry MaterialRequirements(const Scene::EffectiveMaterial &material) noexcept
    {
        MaterialGeometry result{.doubleSided = material.values.doubleSided};
        const bool masked = material.values.alphaMode == Asset::ModelAlpha::Mask;
        const bool coatEnabled = material.values.clearcoatFactor > 0.f;
        for (std::size_t i = 0; i < material.textures.size(); ++i)
        {
            if ((i >= IorSpecularTextureCount && i < ClearcoatTextureCount && !coatEnabled) ||
                (i >= ClearcoatTextureCount && !material.sheen))
            {
                continue;
            }
            if (const auto &slot = material.textures[i])
            {
                const auto uv = slot->slot.texCoord == 0 ? VertexUv0 : VertexUv1;
                auto &attributes = i < CoreMaterialTextureCount ? result.attributes :
                    i < IorSpecularTextureCount ? result.specularAttributes :
                    i < ClearcoatTextureCount ? result.coatAttributes : result.sheenAttributes;
                attributes |= uv;
                if (i < AuxSurfaceTextureCount && (i != 0 || masked))
                {
                    result.surfaceAttributes |= uv;
                }
            }
        }
        if (const auto &normal = material.textures[NormalSlot])
        {
            result.attributes |= VertexTangent;
            result.surfaceAttributes |= VertexTangent;
            result.normalUv = static_cast<std::int8_t>(normal->slot.texCoord);
        }
        if (const auto &normal = material.textures[CoatNormalSlot]; coatEnabled && normal)
        {
            result.coatAttributes |= VertexTangent;
            result.coatNormalUv = static_cast<std::int8_t>(normal->slot.texCoord);
            if (result.normalUv >= 0 && result.normalUv != result.coatNormalUv)
            {
                result.coatAttributes |= VertexCoatTangent;
            }
        }
        result.iorSpecularBase = material.iorSpecular;
        using Color = MaterialColorProfile;
        result.color = material.sheen ? (material.clearcoat ? Color::SheenClearcoat : Color::Sheen) :
            material.clearcoat ? Color::Clearcoat : material.iorSpecular ? Color::IorSpecular : Color::Core;
        const auto &baseColor = material.textures.front();
        if (masked && baseColor)
        {
            result.shadowAttributes = baseColor->slot.texCoord == 0 ? VertexUv0 : VertexUv1;
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
        // UV 数组按材质槽连续编号. 扩展的图像和 sampler 绑定跳过固定的 t5 材质表.
        RHI::BindingSetDesc bindings;
        bindings.AddItem(RHI::BindingSetItem::StructuredBuffer_SRV(StaticAbi::MaterialTableSlot,
                                                                  row.storage.page->buffer.Get()));
        for (std::uint32_t i = 0; i < row.textures.size(); ++i)
        {
            static_assert(StaticAbi::SpecularTextureSlot == StaticAbi::SpecularTextureIndex + 1 &&
                          StaticAbi::SpecularColorTextureSlot == StaticAbi::SpecularColorTextureIndex + 1 &&
                          StaticAbi::ClearcoatTextureSlot == StaticAbi::ClearcoatTextureIndex + 1 &&
                          StaticAbi::ClearcoatRoughnessTextureSlot == StaticAbi::ClearcoatRoughnessTextureIndex + 1 &&
                          StaticAbi::ClearcoatNormalTextureSlot == StaticAbi::ClearcoatNormalTextureIndex + 1 &&
                          StaticAbi::SheenColorTextureSlot == StaticAbi::SheenColorTextureIndex + 1 &&
                          StaticAbi::SheenRoughnessTextureSlot == StaticAbi::SheenRoughnessTextureIndex + 1);
            const auto slot = i + (i >= StaticAbi::MaterialTableSlot ? 1u : 0u);
            bindings.AddItem(RHI::BindingSetItem::Texture_SRV(slot, row.textures[i]->texture.Get())
                .SetSubresources(row.textures[i]->sampling));
            bindings.AddItem(RHI::BindingSetItem::Sampler(slot, row.samplers[i].Get()));
        }
        auto binding = device.device.CreateBindingSet(bindings, device.activeShaders->programs.layouts[2].Get());
        if (!binding)
        {
            Reject(ViewSettingReason::ScenePreparationFailed, "Cannot prepare Forward material table binding");
        }
        row.binding = std::make_shared<RHI::BindingSetHandle>(std::move(binding));
        cached = row.binding;
    }

    /// @brief 路由只取决于冻结的有效材质, 可以供不同 View 的表面和颜色角色复用.
    static OpaqueSurfaceRoute SurfaceRoute(const Scene::EffectiveMaterial &material) noexcept
    {
        if (material.values.unlit)
        {
            return OpaqueSurfaceRoute::Unlit;
        }
        return material.iorSpecular || material.clearcoat || material.sheen ?
            OpaqueSurfaceRoute::OpaqueForwardExtension : OpaqueSurfaceRoute::CoreDeferred;
    }

    /// @brief 准备新覆盖代或补齐缓存行的实际采样槽. 相同字段及绑定沿用原存储.
    static std::shared_ptr<MaterialRow> PrepareMaterial(ForwardDevice &device, StaticDrawWork &frame,
        const MaterialKey &key, std::uint64_t revision, const Scene::EffectiveMaterial &material,
        const MaterialGeometry &geometry, const MaterialRow *cachedRow, OpaqueSurfaceRoute route,
        std::uint32_t sampledSlots)
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
        const auto first = std::get<4>(key) == MaterialUse::OpaqueAuxiliary ? CoverageTextureCount : 0;
        assert((sampledSlots & (1u << first)) != 0);
        for (std::size_t i = first; i < textures.size(); ++i)
        {
            const bool unusedSpecular = i >= CoreMaterialTextureCount && i < IorSpecularTextureCount &&
                !geometry.iorSpecularBase;
            const bool unusedCoat = i >= IorSpecularTextureCount && i < ClearcoatTextureCount &&
                                    values.clearcoat[0] == 0.f;
            if ((sampledSlots & (1u << i)) == 0 || unusedSpecular || unusedCoat)
            {
                // 实际颜色角色之外的槽及未生效的子层借用本行已有视图, 只补原生集合.
                textures[i] = textures[first];
                samplers[i] = samplers[first];
                continue;
            }
            const auto &active = material.textures[i];
            const auto sampler = EffectiveSampler(active ? active->sampler : Asset::ModelSampler{});
            samplers[i] = PrepareSampler(device, sampler);
            textures[i] = active ? PrepareSlot(device, uploads, std::get<0>(key), *active, sampler[0] >= 9984)
                                 : WhiteTexture(device, uploads);
        }
        // SurfaceAux 的基色绑定服务 MASK 分支. OPAQUE 时借用该行已准备的表面图像,
        // 使原生配方与图读登记同一已准备的资源.
        std::ranges::fill(std::span(textures).first(first), textures[first]);
        std::ranges::fill(std::span(samplers).first(first), samplers[first]);
        if (const auto found = device.materials.find(key);
            found != device.materials.end() && device.Current(*found->second.resident))
        {
            auto &entry = found->second;
            auto &row = *entry.resident;
            if (row.route == route && row.sampledSlots == sampledSlots && SameMaterial(row.values, values) &&
                row.textures == textures && row.samplers == samplers)
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
        row->route = route;
        row->sampledSlots = sampledSlots;
        row->textures = std::move(textures);
        row->samplers = std::move(samplers);
        device.AllocateSlice(row->storage, *row, GraphUsage::ShaderRead, std::uint8_t(TableLayout::Materials),
                             rowBytes, rowBytes, StaticAbi::MaterialRowsCapacity);
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
                             std::uint8_t attributes, const VertexLayout &layout,
                             std::span<const std::array<float, 4>> coatTangents) noexcept
    {
        for (std::size_t i = 0; i < primitive.vertices.size(); ++i)
        {
            const auto &vertex = primitive.vertices[i];
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
            if (!coatTangents.empty())
            {
                store(6, coatTangents[i]);
            }
            target = target.subspan(layout.stride);
        }
    }

    using GeometryResult = std::variant<std::shared_ptr<GeometryRef>, Core::BoundedText>;

    /// @brief 优先重用同版及同布局几何. 有效 normal UV 变化时用共享导入算法生成新的接缝索引.
    static GeometryResult PrepareGeometry(ForwardDevice &device, UploadBatch &uploads, const GeometryKey &key,
                                          const Asset::ModelPrimitive &source, bool primaryIsCoat)
    {
        if (const auto found = device.geometry.find(key);
            found != device.geometry.end() && device.Current(*found->second))
        {
            device.Use(*found->second);
            return found->second;
        }
        std::optional<Asset::NormalUvGeometry> rebuilt;
        const auto primary = static_cast<Asset::TangentSource>(std::get<4>(key));
        const auto secondary = static_cast<Asset::TangentSource>(std::get<5>(key));
        if (primary >= Asset::TangentSource::Uv0 || secondary != Asset::TangentSource::Absent)
        {
            auto result = Asset::BuildNormalUvGeometry(source, primary, secondary, Asset::StaticTangentWorkLimit,
                primaryIsCoat ? "clearcoatNormalTexture.texCoord" : "normalTexture.texCoord");
            if (const auto *error = std::get_if<Asset::AssetFailure>(&result))
            {
                if (error->reason == Asset::AssetError::ModelByteLimit)
                {
                    throw ForwardFailure{ViewSettingReason::SceneResidencyLimit, error->description};
                }
                return error->description;
            }
            rebuilt = std::get<Asset::NormalUvGeometry>(std::move(result));
        }
        const auto &primitive = rebuilt ? rebuilt->primitive : source;
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
        PackVertices(packed, primitive, attributes, layout,
                     rebuilt ? std::span<const std::array<float, 4>>(rebuilt->coatTangents) :
                         std::span<const std::array<float, 4>>{});
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
    static void DescribeSkipped(StaticDrawWork &frame, const Asset::ModelRecord &model, const PrimitiveRef &ref,
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

    /// @brief 以当前眼点为主光栅原点, 编码投影、模式及活动灯表的两段计数.
    /// 只在槽安全且字段已改变或上传未接受时排入写入.
    static void PrepareViewRow(ForwardDevice &device, ForwardFrameData &frame, const PreparedView &view,
                               PreviewMode mode)
    {
        StaticAbi::ForwardView value{};
        const auto &camera = *view.camera;
        const std::span basis(camera.worldFromView);
        const auto eye = basis.subspan<12, 3>();
        if (!StoreGpuPosition(value.originHigh, value.originLow, value.originResidual, eye))
        {
            Reject(ViewSettingReason::InvalidProjection, "Forward raster origin exceeds the GPU float range");
        }
        // 光栅与屏幕恢复共用这三段眼点. 矩阵只旋转相对位移并应用本次渲染投影, 保留实际抖动.
        const auto viewFromOrigin = glm::dmat4(glm::dmat3(glm::make_mat4(camera.viewFromWorld.data())));
        if (!StoreMatrix(value.clipFromOrigin, glm::make_mat4(camera.renderClipFromView.data()) * viewFromOrigin))
        {
            Reject(ViewSettingReason::InvalidProjection, "Forward clip transform exceeds the GPU float range");
        }
        value.mode = static_cast<std::uint32_t>(mode);
        value.preExposure = camera.exposure.preExposure;
        if (frame.activeLights)
        {
            value.directionalCount = frame.activeLights->directionalCount;
            value.localCount = frame.activeLights->localCount;
        }
        value.orthographic = camera.orthographic ? 1u : 0u;
        if (camera.orthographic)
        {
            // 正交 BRDF 使用相机世界 +Z. 光栅原点仍为眼点.
            if (!StoreGpuVector(value.orthographicDirection, basis.subspan<8, 3>()))
            {
                Reject(ViewSettingReason::InvalidProjection, "Forward view direction exceeds the GPU float range");
            }
        }
        if (!frame.view)
        {
            frame.view = CreateConstantRow(device, sizeof(value), 0);
        }
        device.Use(*frame.view);
        const auto &previous = frame.viewValues;
        if (!frame.view->accepted || !previous || previous->mode != value.mode ||
            previous->preExposure != value.preExposure || previous->orthographic != value.orthographic ||
            previous->orthographicDirection != value.orthographicDirection ||
            previous->directionalCount != value.directionalCount || previous->localCount != value.localCount ||
            !SameRasterTransform(value, *previous))
        {
            // 先撤销已上传资格. 图未接受或上传准备失败时保留 false, 本槽安全复用后仍会重试该行.
            frame.view->accepted = false;
            frame.viewValues = value;
            frame.uploads.Add(frame.view, frame.view->buffer, std::as_bytes(std::span(&value, 1)),
                              GraphUploadSource::ForwardView, device.preparingView);
        }
    }

    /// @brief 将主可见集或当前阴影子视图中的一个 Primitive 加入调用方的批次输入.
    /// 按覆盖、独立表面或颜色的消费范围准备纹理. Shader 句柄由 View 完成规划后的选代阶段填写.
    static void PrepareDraw(ForwardDevice &device, StaticDrawWork &frame, const PreparedView &view,
        std::uint32_t index, std::map<MaterialRow *, std::size_t> &rows, SurfacePhase phase, bool coverageOnly,
        bool litColor = false, bool surfaceOnly = false)
    {
        const auto &ref = view.scene.primitives[index];
        const auto &model = *view.scene.models[ref.model];
        const auto &source = model.meshes[ref.mesh].primitives[ref.sourceSlot];
        const auto &object = view.scene.objects[ref.object];
        const auto version = Version(model.asset, model.revision);
        const auto entity = ObjectIdentity(view.scene.scene, object.entity);
        MaterialKey key{version, ref.material, ref.materialOverride ? entity : ObjectKey{},
                        ref.materialOverride ? ref.sourceSlot : 0,
                        surfaceOnly ? MaterialUse::OpaqueAuxiliary : MaterialUse::Main};
        ++device.materialRequests;
        const auto *cached = FindMaterial(device, key, ref.materialRevision);
        if (surfaceOnly && !cached)
        {
            // 先查独立辅助绑定. 从颜色行可复用有效参数和几何要求, 仍须为 OPAQUE 准备占位绑定.
            std::get<4>(key) = MaterialUse::Main;
            cached = FindMaterial(device, key, ref.materialRevision);
        }
        if (coverageOnly && (!cached || !frame.uploads.HasMaterialTexture(*cached->resident->textures.front())))
        {
            // 已有完整材质时复用其覆盖. 仅为阴影准备的材质另按实际使用的基色资源记账.
            std::get<4>(key) = MaterialUse::Coverage;
            cached = FindMaterial(device, key, ref.materialRevision);
        }
        std::optional<Scene::EffectiveMaterial> sourceMaterial;
        const auto resolveMaterial = [&]() -> const Scene::EffectiveMaterial *
        {
            if (ref.materialOverride)
            {
                return &view.scene.materialOverrides[*ref.materialOverride];
            }
            return &sourceMaterial.emplace(Scene::ResolveMaterial(model, source, nullptr));
        };
        const auto *material = cached ? nullptr : resolveMaterial();
        if (material && material->values.alphaMode == Asset::ModelAlpha::Blend)
        {
            DescribeSkipped(frame, model, ref, "alphaMode BLEND is unsupported");
            return;
        }
        auto row = cached ? cached->resident : nullptr;
        const auto route = row ? row->route : SurfaceRoute(*material);
        const bool auxiliaryOnly = surfaceOnly && route != OpaqueSurfaceRoute::CoreDeferred;
        const bool masked = row ? row->values.masked != 0 : material->values.alphaMode == Asset::ModelAlpha::Mask;
        const auto cachedUse = std::get<4>(key);
        if (auxiliaryOnly && !masked)
        {
            std::get<4>(key) = MaterialUse::OpaqueAuxiliary;
        }
        const auto requirements = cached ? cached->geometry : MaterialRequirements(*material);
        const auto color = litColor ? requirements.color : MaterialColorProfile::Core;
        auto sampled = ColorTextureSlots(color);
        if (coverageOnly)
        {
            sampled = MaterialTexturePrefix(CoverageTextureCount);
        }
        else if (auxiliaryOnly)
        {
            sampled = MaterialTexturePrefix(AuxSurfaceTextureCount);
        }
        // 仅复用同一用途的有效绑定. 颜色行真实基色与独立 OPAQUE 辅助行的占位各自就绪.
        bool texturesReady = row && cachedUse == std::get<4>(key) && (row->sampledSlots & sampled) == sampled;
        for (std::size_t i = 0; texturesReady && i < row->textures.size(); ++i)
        {
            if ((sampled & (1u << i)) != 0)
            {
                texturesReady = frame.uploads.HasMaterialTexture(*row->textures[i]);
            }
        }
        if (!texturesReady)
        {
            if (!material)
            {
                material = resolveMaterial();
            }
            if (std::get<4>(key) == MaterialUse::Coverage)
            {
                if (ref.materialOverride)
                {
                    sourceMaterial = *material;
                }
                auto &slots = sourceMaterial->textures;
                const auto firstUnused = masked ? CoverageTextureCount : 0;
                for (std::size_t slot = firstUnused; slot < slots.size(); ++slot)
                {
                    slots[slot].reset();
                }
                material = &*sourceMaterial;
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
            if (phase == SurfacePhase::ShadowDepth)
            {
                throw ForwardFailure{ViewSettingReason::SceneUnsupported, *frame.degradation};
            }
            return;
        }
        // 按实际角色取顶点流. 独立辅助表面无需颜色 UV, 即使本行已由颜色帧补齐.
        const bool coatColor = UsesClearcoat(color);
        auto attributes = std::uint8_t(requirements.attributes |
            (litColor && requirements.iorSpecularBase ? requirements.specularAttributes : 0) |
            (coatColor ? requirements.coatAttributes : 0) |
            (UsesSheen(color) ? requirements.sheenAttributes : 0));
        if (coverageOnly)
        {
            attributes = requirements.shadowAttributes;
        }
        else if (auxiliaryOnly)
        {
            attributes = requirements.surfaceAttributes;
        }
        const auto &sourceSlots = model.materials[ref.material].slots;
        const auto &sourceNormal = sourceSlots[NormalSlot] ? sourceSlots[NormalSlot] : sourceSlots[CoatNormalSlot];
        const auto selectTangent = [&](std::int8_t uv)
        {
            return source.hasTangent && sourceNormal && sourceNormal->texCoord == static_cast<std::uint32_t>(uv) ?
                Asset::TangentSource::Original : static_cast<Asset::TangentSource>(uv);
        };
        const bool primaryIsCoat = requirements.normalUv < 0 && coatColor;
        const auto primary = (attributes & VertexTangent) != 0 ?
            selectTangent(primaryIsCoat ? requirements.coatNormalUv : requirements.normalUv) :
            Asset::TangentSource::Absent;
        const auto secondary = (attributes & VertexCoatTangent) != 0 ?
            selectTangent(requirements.coatNormalUv) : Asset::TangentSource::Absent;
        // 只有清漆法线时沿主切线地址取样. 两槽共用 UV 时也只存一条流, 变换仍由各自的材质槽提供.
        auto geometry = PrepareGeometry(device, frame.uploads, {version, ref.mesh, ref.sourceSlot, attributes,
                                        static_cast<std::int8_t>(primary), static_cast<std::int8_t>(secondary)},
                                        source, primaryIsCoat);
        if (const auto *error = std::get_if<Core::BoundedText>(&geometry))
        {
            DescribeSkipped(frame, model, ref, error->View());
            if (phase == SurfacePhase::ShadowDepth)
            {
                throw ForwardFailure{ViewSettingReason::SceneUnsupported, *frame.degradation};
            }
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
            // 缓存行可能含更多纹理. 按当前角色的槽位集合登记驻留, 含原生配方要求的占位.
            ++device.materialHits;
            for (std::size_t i = 0; i < row->textures.size(); ++i)
            {
                if ((sampled & (1u << i)) != 0)
                {
                    device.Use(*row->textures[i]);
                }
            }
            device.Use(*row);
        }
        else
        {
            row = PrepareMaterial(device, frame, key, ref.materialRevision, *material, requirements, row.get(),
                                  route, sampled);
        }
        const auto [slot, newRow] = rows.try_emplace(row.get(), frame.materialRows.size());
        if (newRow)
        {
            frame.materialRows.push_back(std::move(row));
        }
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
            slot->second, {}, {}, {}, static_cast<std::uint32_t>(
                GeometryRequest(attributes, mirror, doubleSided, phase, LightingProgram::Legacy).variant +
                std::size_t(color) * GeometryPipelineSlots),
            -z, index, attributes, mirror, doubleSided, color});
    }

    /// @brief 核心和阴影按状态排序, 采样集合按首次可见顺序编号.
    /// 主视图的辅助 Draw 在核心之后按冻结的 Primitive 顺序绘制, 与颜色角色及缓存命中无关.
    static void SortDraws(StaticDrawWork &frame, bool orderAuxiliary = false)
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
            if (orderAuxiliary)
            {
                const auto auxiliary = [&](const DrawRecord &draw)
                {
                    return frame.materialRows[draw.material]->route != OpaqueSurfaceRoute::CoreDeferred;
                };
                const bool aAux = auxiliary(a);
                const bool bAux = auxiliary(b);
                if (aAux != bAux)
                {
                    return bAux;
                }
                if (aAux)
                {
                    // 两者都写辅助表面. 深度相等时后画者获胜, 批次状态只用于合并已经相邻的 Draw.
                    return a.source < b.source;
                }
            }
            return order(a) < order(b);
        });
    }

    /// @brief 比较同一 View 和实例块内仍可能变化的状态. 固定状态槽区分材质颜色 profile、顶点布局及光栅组合.
    /// 材质集合含表页和采样组合. 灯表和前置深度模式由本 View 统一选择.
    /// splitRoutes 使每批首项能代表整批的表面路由. 在相邻段上分批, 保留相等深度处的覆盖顺序.
    static bool SameBatch(const StaticDrawWork &frame, const DrawRecord &a, const DrawRecord &b,
                          bool splitRoutes) noexcept
    {
        return a.pipelineKey == b.pipelineKey &&
               (!splitRoutes || frame.materialRows[a.material]->route == frame.materialRows[b.material]->route) &&
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

    /// @brief 同一设备 owner 和字节数的私有缓冲可复用物理存储.
    static bool FitsDrawBuffer(const ForwardDevice &device, const DrawBuffer *storage, std::size_t bytes) noexcept
    {
        return storage && storage->capacity == device.capacity && storage->bytes == bytes;
    }

    /// @brief 在当前安全 View 槽取得等形缓冲, 按本轮使用登记驻留.
    static void UseDrawBuffer(ForwardDevice &device, std::shared_ptr<DrawBuffer> &storage,
                              GraphUsage usage, std::size_t bytes, std::uint32_t stride = 0)
    {
        if (!FitsDrawBuffer(device, storage.get(), bytes))
        {
            storage.reset();
            device.Reserve(bytes);
            auto next = std::make_shared<DrawBuffer>(device.capacity, bytes);
            next->buffer = AllocateBuffer(device.device, usage, bytes, stride);
            storage = std::move(next);
        }
        device.Use(*storage);
    }

    /// @brief 已接受且字节相同的版本可直接导入后继图.
    static bool HasDrawBufferContents(const DrawBuffer &storage, std::span<const std::byte> values) noexcept
    {
        return storage.accepted && std::ranges::equal(values, storage.contents);
    }

    /// @brief 准备当前安全 View 槽的普通绘制缓冲, 必要时在本图登记写入.
    static void PrepareDrawBuffer(ForwardDevice &device, StaticDrawWork &frame,
        std::shared_ptr<DrawBuffer> &storage, GraphUsage usage, std::span<const std::byte> values,
        std::uint32_t stride = 0, GraphUploadSource source = GraphUploadSource::ForwardDraw)
    {
        UseDrawBuffer(device, storage, usage, values.size(), stride);
        if (!HasDrawBufferContents(*storage, values))
        {
            storage->accepted = false;
            frame.uploads.Add(storage, storage->buffer, values, source, device.preparingView);
            storage->contents.assign(values.begin(), values.end());
        }
    }

    /// @brief 打包最终 D32 所在栅格的逆投影和相机旋转. 屏幕着色另从主光栅的 View 常量取得三段眼点.
    /// renderRect 以渲染画布像素计, 包含留边偏移. 逆矩阵对应实际写深度时带抖动的渲染投影.
    static std::optional<StaticAbi::DeferredView> EncodeDeferredView(const ForwardFrameData &frame,
                                                                     const PreparedView &view) noexcept
    {
        StaticAbi::DeferredView value{};
        const auto &camera = *view.camera;
        if (!StoreMatrix(value.viewFromRenderClip, glm::make_mat4(camera.viewFromRenderClip.data())) ||
            !StoreMatrix(value.worldBasis, glm::dmat3(glm::make_mat4(camera.worldFromView.data()))))
        {
            return std::nullopt;
        }
        const auto &rect = frame.source.layout.rect;
        value.renderRect = {rect.x, rect.y, rect.extent.width, rect.extent.height};
        return value;
    }

    /// @brief 为有序表面分配实例块. 页内行号及顺序在不同的表面路由之间共用.
    static void PrepareInstanceBlocks(ForwardDevice &device, StaticDrawWork &frame)
    {
        constexpr auto blockCapacity = StaticAbi::DrawInstancesCapacity;
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
                const auto materialRow = frame.materialRows[draw.material]->storage.range.byteOffset /
                    sizeof(StaticAbi::ForwardMaterial);
                assert(objectRow < StaticAbi::ObjectRowsCapacity && materialRow < StaticAbi::MaterialRowsCapacity);
                instances[i] = {static_cast<std::uint32_t>(objectRow), static_cast<std::uint32_t>(materialRow)};
            }
            // Shader 绑定整个定长块. 末块的尾部也随载荷定义, 只有前 count 行可被命令索引.
            std::fill(instances.begin() + count, instances.end(), StaticAbi::DrawInstanceData{});
            auto &storage = frame.instanceBlocks[block];
            PrepareDrawBuffer(device, frame, storage, GraphUsage::ShaderRead, std::as_bytes(std::span(instances)),
                              sizeof(StaticAbi::DrawInstanceData));
        }
    }

    /// @brief 由已准备的实例块编码相邻批次. prior 借用同一有序工作集的旧批次, 实例块和对象页身份一致.
    static void BuildBatches(ForwardDevice &device, const StaticDrawWork &frame, bool splitRoutes,
        std::vector<DrawBatch> &batches, std::vector<RHI::DrawIndexedIndirectArguments> &arguments,
        std::span<const DrawBatch> prior = {})
    {
        constexpr auto blockCapacity = StaticAbi::DrawInstancesCapacity;
        auto *layout = device.activeShaders->programs.layouts[1].Get();
        auto previous = prior.begin();
        for (std::size_t block = 0; block < frame.instanceBlocks.size(); ++block)
        {
            const auto first = block * blockCapacity;
            const auto draws = std::span(frame.draws).subspan(first, std::min(blockCapacity, frame.draws.size() - first));
            const auto &storage = frame.instanceBlocks[block];
            std::map<std::uint64_t, RHI::BindingSetHandle> sets;
            // 重分批沿用同一对象页和实例块的集合. 原生布局身份变化时按当前布局建立集合.
            for (; previous != prior.end() && previous->instanceBlock == block; ++previous)
            {
                if (previous->renderables->GetLayout() == layout)
                {
                    const auto &objects = frame.draws[previous->firstDraw].object->storage.page;
                    sets.try_emplace(objects->ordinal, previous->renderables);
                }
            }
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
                    renderables = device.device.CreateBindingSet(bindings, layout);
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
                           SameBatch(frame, head, draws[end], splitRoutes))
                    {
                        ++end;
                    }
                    if (arguments.size() == ForwardBufferLimit / sizeof(RHI::DrawIndexedIndirectArguments))
                    {
                        Reject(ViewSettingReason::SceneUploadLimit, "Forward indirect arguments exceed the buffer limit");
                    }
                    arguments.push_back(EncodeCommand(*geometry, static_cast<std::uint32_t>(current),
                                                     static_cast<std::uint32_t>(end - current)));
                    current = end;
                } while (current < draws.size() && SameBatch(frame, head, draws[current], splitRoutes));
                batches.push_back({first + begin, block, firstCommand,
                    static_cast<std::uint32_t>(arguments.size()) - firstCommand, renderables});
                begin = current;
            }
        }
    }

    /// @brief 按当前策略准备有序实例和参数. 深度与颜色共享这些实例行.
    static void PackBatches(ForwardDevice &device, StaticDrawWork &frame, bool splitRoutes = false)
    {
        PrepareInstanceBlocks(device, frame);
        std::vector<RHI::DrawIndexedIndirectArguments> arguments;
        BuildBatches(device, frame, splitRoutes, frame.batches, arguments);
        PrepareDrawBuffer(device, frame, frame.indirect, GraphUsage::IndirectArgument,
                          std::as_bytes(std::span(arguments)));
    }

    // 来自本路实际驻留的表面及环境配置, 两个准备阶段共用固定状态选择.
    static ShaderPipelineRequest DrawShader(const ForwardFrameData &frame, const DrawRecord &draw,
                                            SurfacePhase phase) noexcept
    {
        return GeometryRequest(draw.attributes, draw.mirrored, draw.doubleSided, phase, frame.environment.program,
                               draw.color);
    }

    /// @brief 按批首材质选择表面写入角色. 取余保留布局, 绕序及剔面, 去掉颜色阶段的等深只读状态.
    static ShaderPipelineRequest SurfaceShader(OpaqueSurfaceRoute route, const DrawRecord &draw) noexcept
    {
        const auto role = route == OpaqueSurfaceRoute::CoreDeferred ? Shader::ProductShader::StaticCoreGBuffer
                                                                  : Shader::ProductShader::StaticSurfaceAux;
        return {role, draw.pipelineKey % GeometryPhaseSlots};
    }

    /// @brief 前置深度和最终 Deferred 表面均先于几何颜色消费.
    static SurfacePhase ColorPhase(const ViewResourceNeeds &needs) noexcept
    {
        return needs.NeedsEarlyDepth() || needs.deferredLighting ? SurfacePhase::ReadOnlyDepth : SurfacePhase::Combined;
    }

    /// @brief 普通颜色使用所有批次. Deferred 的前向颜色只消费辅助几何. 独立表面没有颜色批次.
    static bool UsesForwardColor(const ViewResourceNeeds &needs, OpaqueSurfaceRoute route) noexcept
    {
        return !needs.surfaceOnly && (!needs.deferredLighting || route != OpaqueSurfaceRoute::CoreDeferred);
    }

    LightingProgram OpaqueColorProgram(const ForwardFrameData &frame, OpaquePath path) noexcept
    {
        const auto program = FullListProgram(frame.environment.program);
        return frame.usesLighting && UsesFroxelCandidates(path) ? WithFroxels(program) : program;
    }

    /// @brief 只有可绘核心材质实际需要屏幕光照程序. 空表面和 Unlit 仍能完成构图.
    static std::optional<ShaderPipelineRequest> DeferredColor(const ForwardFrameData &frame,
        const ViewResourceNeeds &needs, LightingProgram program) noexcept
    {
        if (needs.deferredLighting && frame.usesLighting && std::ranges::any_of(frame.materialRows, [](const auto &row)
            {
                return row->route == OpaqueSurfaceRoute::CoreDeferred;
            }))
        {
            return DeferredLightingRequest(program);
        }
        return std::nullopt;
    }

    /// @brief 同一份表面和颜色配方用于需求登记与就绪判断. 以实际材质路由选择各几何键.
    static void VisitOpaqueDrawShaders(const ForwardFrameData &frame, const DrawRecord &draw,
        const ViewResourceNeeds &needs, LightingProgram program, const auto &visit)
    {
        const auto route = frame.materialRows[draw.material]->route;
        if (needs.NeedsCoreSurface())
        {
            visit(SurfaceShader(route, draw));
        }
        if (UsesForwardColor(needs, route))
        {
            visit(GeometryRequest(draw.attributes, draw.mirrored, draw.doubleSided, ColorPhase(needs), program,
                                  draw.color));
        }
        if (needs.NeedsEarlyDepth())
        {
            visit(DrawShader(frame, draw, SurfacePhase::Depth));
        }
    }

    /// @brief 同次 CPU 投影编码为相对原点的列主序 clip 矩阵. caster 常量偏移单独追加.
    static glm::dmat4 CascadeClip(const DirectionalShadowProjection &projection) noexcept
    {
        glm::dmat4 clip(1);
        const std::array scale{1 / projection.halfExtent, 1 / projection.halfExtent, 1 / projection.depthRange};
        for (std::uint32_t column = 0; column < 3; ++column)
        {
            for (std::uint32_t row = 0; row < 3; ++row)
            {
                clip[column][row] = projection.axes[row][column] * scale[row];
            }
        }
        return clip;
    }

    /// @brief 在安全槽内复用阴影 View 常量, 修改仅登记本轮的图上传.
    static void PrepareCasterConstants(ForwardDevice &device, StaticDrawWork &work, const auto &value)
    {
        if (!work.view || work.view->capacity != device.capacity)
        {
            work.view = CreateConstantRow(device, sizeof(value), 0);
            work.viewValues.reset();
        }
        device.Use(*work.view);
        const auto &previous = work.viewValues;
        if (!work.view->accepted || !previous || !SameRasterTransform(value, *previous))
        {
            work.view->accepted = false;
            work.uploads.Add(work.view, work.view->buffer, std::as_bytes(std::span(&value, 1)),
                            GraphUploadSource::ForwardView, device.preparingView);
            work.viewValues = value;
        }
    }

    static constexpr auto ShadowVariantBase = std::size_t(SurfacePhase::ShadowDepth) * GeometryPhaseSlots;

    /// @brief 按当前 View 的查询设置更新常量. 过滤切换只改变查询输入.
    static void PrepareShadowConstants(ForwardDevice &device, ForwardFrameData &frame, const PreparedView &view)
    {
        auto &shadow = *frame.shadow;
        StaticAbi::ForwardShadowView value{};
        // 方向光若获准总是首组. 级联选级使用这里的前向和主 View 行的眼点.
        if (shadow.queryRows.front().kind == 0)
        {
            const std::span camera(view.camera->worldFromView);
            for (std::size_t axis = 0; axis < 3; ++axis)
            {
                value.cameraForward[axis] = static_cast<float>(-camera[8 + axis]);
            }
        }
        value.edge = shadow.depth.edge;
        value.layerCount = shadow.depth.count;
        value.pcf = shadow.depth.filter == ShadowFilter::Pcf;
        value.innerScale = static_cast<float>(ShadowInnerScale(value.edge));
        static_assert(sizeof(value) <= RHI::ConstantBufferOffsetSizeAlignment);
        if (!shadow.constants || shadow.constants->capacity != device.capacity)
        {
            shadow.constants = CreateConstantRow(device, sizeof(value), std::nullopt);
        }
        device.Use(*shadow.constants);
        const auto &previous = shadow.values;
        if (!shadow.constants->accepted || !previous || previous->cameraForward != value.cameraForward ||
            previous->edge != value.edge || previous->layerCount != value.layerCount || previous->pcf != value.pcf)
        {
            shadow.constants->accepted = false;
            frame.uploads.Add(shadow.constants, shadow.constants->buffer, std::as_bytes(std::span(&value, 1)),
                              GraphUploadSource::ForwardView, device.preparingView);
            shadow.values = value;
        }
    }

    /// @brief 组首行同时交付组范围和覆盖参数, 每一行对应一个物理层的无偏投影.
    static void PackShadowQuery(ShadowFrame &shadow, std::size_t first, const ShadowLightPlan &light)
    {
        auto &rows = shadow.queryRows;
        auto &group = rows[first];
        group.count = light.regionCount;
        static_assert(std::uint32_t(Asset::ModelLightKind::Directional) == 0 &&
                      std::uint32_t(Asset::ModelLightKind::Point) == 1 && std::uint32_t(Asset::ModelLightKind::Spot) == 2);
        if (const auto *plan = std::get_if<DirectionalShadowPlan>(&light.views))
        {
            group.nearDepth = plan->nearDepth;
            for (std::size_t i = 0; i < plan->cascades.size(); ++i)
            {
                auto &row = rows[first + i];
                const auto &cascade = plan->cascades[i];
                const auto &projection = cascade.projection;
                if (!StoreGpuPosition(row.originHigh, row.originLow, row.originResidual, projection.origin) ||
                    !StoreMatrix(row.clipFromOrigin, CascadeClip(projection)))
                {
                    Reject(ViewSettingReason::InvalidProjection, "Directional shadow projection exceeds float range");
                }
                row.worldTexel = static_cast<float>(projection.halfExtent * (2.0 / shadow.depth.edge));
                row.farDepth = cascade.farDepth;
                row.transition = cascade.transition;
            }
            const auto &axis = plan->cascades.front().projection.axes[2];
            std::ranges::transform(axis, group.lightDirection.begin(), [](double value)
            {
                return static_cast<float>(-value);
            });
        }
        else
        {
            const auto &local = std::get<LocalShadowPlan>(light.views);
            const auto &volume = local.volume;
            group.kind = std::uint32_t(volume.outerCone > 0 ? Asset::ModelLightKind::Spot : Asset::ModelLightKind::Point);
            if (!StoreGpuPosition(group.originHigh, group.originLow, group.originResidual, volume.position))
            {
                Reject(ViewSettingReason::InvalidProjection, "Local shadow projection exceeds float range");
            }
            group.nearDepth = static_cast<float>(volume.nearPlane);
            group.farPlane = static_cast<float>(volume.farPlane);
            group.receiverOffsetRatio = local.receiverOffsetRatio;
            for (std::size_t i = 0; i < light.regionCount; ++i)
            {
                auto &matrix = rows[first + i].clipFromOrigin;
                const auto &projection = local.projections[i].clipFromLight;
                for (std::size_t column = 0; column < projection.size(); ++column)
                {
                    matrix[column].value = projection[column];
                }
            }
        }
    }

    /// @brief 深度与查询共用同一编码, 写入沿常规深度方向追加 caster 偏移.
    static StaticAbi::ForwardView ShadowRasterView(const ShadowFrame &shadow, const ShadowLightPlan &light,
                                                  std::size_t first, std::size_t i) noexcept
    {
        const auto &row = shadow.queryRows[first + i];
        StaticAbi::ForwardView raster{};
        const auto *directional = std::get_if<DirectionalShadowPlan>(&light.views);
        const auto &origin = directional ? row : shadow.queryRows[first];
        raster.originHigh = origin.originHigh;
        raster.originLow = origin.originLow;
        raster.originResidual = origin.originResidual;
        for (std::size_t column = 0; column < row.clipFromOrigin.size(); ++column)
        {
            raster.clipFromOrigin[column].value = row.clipFromOrigin[column].value;
        }
        if (directional)
        {
            const auto &projection = directional->cascades[i].projection;
            const double texel = projection.halfExtent * (2.0 / shadow.depth.edge);
            raster.clipFromOrigin[3].value[2] =
                static_cast<float>(texel * ShadowDepthConvention::CasterBiasTexels / projection.depthRange);
        }
        else
        {
            raster.clipFromOrigin[3].value[2] = std::get<LocalShadowPlan>(light.views).projections[i].casterClipZ;
        }
        return raster;
    }

    /// @brief 同一数组需生产时才逐层准备静态 caster. 各层按原计划组顺序编号.
    static void PrepareShadowCasters(ForwardDevice &device, ForwardFrameData &frame, const PreparedView &view,
                                     std::span<const ShadowLightPlan> groups)
    {
        auto &shadow = *frame.shadow;
        auto &depth = shadow.depth;
        depth.variants.reset();
        depth.degradation.reset();
        depth.drawBatches = 0;
        shadow.levels.reserve(depth.count);
        while (shadow.levels.size() < depth.count)
        {
            shadow.levels.emplace_back(frame.uploads, frame.encodings);
        }
        std::size_t first = 0;
        for (const auto &light : groups)
        {
            for (std::size_t i = 0; i < light.regionCount; ++i)
            {
                auto &level = shadow.levels[first + i];
                std::map<MaterialRow *, std::size_t> rows;
                for (const auto index : light.Casters(i))
                {
                    PrepareDraw(device, level, view, index, rows, SurfacePhase::ShadowDepth, true);
                }
                if (level.degradation && !depth.degradation)
                {
                    depth.degradation = level.degradation;
                }
                if (level.draws.empty())
                {
                    continue;
                }
                PrepareCasterConstants(device, level, ShadowRasterView(shadow, light, first, i));
                SortDraws(level);
                PackBatches(device, level);
                depth.drawBatches += level.batches.size();
                for (const auto &batch : level.batches)
                {
                    const auto key = DrawShader(frame, level.draws[batch.firstDraw], SurfacePhase::ShadowDepth);
                    depth.variants.set(key.variant - ShadowVariantBase);
                }
            }
            first += light.regionCount;
        }
    }

    /// @brief 计划的 lights 顺序源自本次可见集合. 用与灯表相同的分段顺序发布组首层引用.
    static void PrepareShadowReferences(ForwardDevice &device, ForwardFrameData &frame, const PreparedView &view)
    {
        static_assert(StaticAbi::ShadowReferencesCapacity == StaticAbi::LightRowsCapacity);
        StaticAbi::ShadowReferencesData references{};
        std::array<std::uint32_t, MaxShadowLayers> firstReference{};
        const auto &plan = *view.shadows;
        std::uint32_t first = 1;
        for (std::size_t group = 0; group < plan.admitted.size(); ++group)
        {
            firstReference[group] = first;
            first += plan.admitted[group].regionCount;
        }
        // 组引用从 1 起算. 两表使用相同的分段游标, 未准入的灯也推进游标并保留零引用.
        LightRowCursor cursor{.local = frame.activeLights->directionalCount};
        for (const auto &link : plan.lights)
        {
            const auto row = cursor.Next(view.scene.lights[link.sceneLightIndex].light.type);
            if (link.plan)
            {
                references[row].group = firstReference[*link.plan];
            }
        }
        PrepareDrawBuffer(device, frame, frame.shadow->references, GraphUsage::ShaderRead,
                          std::as_bytes(std::span(references)), sizeof(StaticAbi::ForwardShadowReference),
                          GraphUploadSource::ForwardView);
    }

    /// @brief 按组枚举次序紧排物理层, 同步生成查询行. 导入候选须有可读出口且逐层身份和内容代匹配.
    /// 命中时仍登记旧深度使用过的程序键, 待本帧选代后由 ApplyShadowShaders 决定是否补画.
    static void PlanShadowDraws(ForwardDevice &device, ForwardFrameData &frame, const PreparedView &view,
                                 std::span<const ShadowLightPlan> groups)
    {
        if (!frame.shadow)
        {
            frame.shadow.emplace();
        }
        auto &shadow = *frame.shadow;
        auto &depth = shadow.depth;
        const auto &retained = view.acceptedShadowDepth;
        if (retained && retained->device == &device.device && retained->retained.Readable() &&
            retained->Matches(view.view, view.scene.scene, *view.shadows, groups))
        {
            depth = *retained;
        }
        else
        {
            depth = {.view = view.view, .scene = view.scene.scene, .edge = view.shadows->edge,
                     .groups = static_cast<std::uint32_t>(groups.size()), .device = &device.device};
            for (const auto &group : groups)
            {
                for (const auto &region : group.Regions())
                {
                    depth.layers[depth.count++] = {group.light, region};
                }
            }
        }
        depth.drawBatches = 0;
        depth.filter = view.shadowStatistics.filter;
        shadow.queryRows = {};
        std::size_t first = 0;
        for (const auto &group : groups)
        {
            PackShadowQuery(shadow, first, group);
            first += group.regionCount;
        }
        static_assert(StaticAbi::ShadowLayersCapacity == MaxShadowLayers);
        PrepareShadowConstants(device, frame, view);
        PrepareDrawBuffer(device, frame, shadow.layerRows, GraphUsage::ShaderRead,
                          std::as_bytes(std::span(shadow.queryRows)), sizeof(StaticAbi::ForwardShadowLayer),
                          GraphUploadSource::ForwardView);
        if (!depth.retained)
        {
            PrepareShadowCasters(device, frame, view, groups);
        }
        static_assert(decltype(depth.variants){}.size() == GeometryPhaseSlots);
        for (std::size_t slot = 0; slot < depth.variants.size(); ++slot)
        {
            if (depth.variants[slot])
            {
                device.shaderDemand.Add({Shader::ProductShader::StaticShadowDepth, ShadowVariantBase + slot});
            }
        }
    }

    void PlanDraws(ForwardDevice &device, ForwardFrameData &frame, const PreparedView &view, PreviewMode mode,
                   const ShadowLightPlan *shadowLight)
    {
        // 先收集几何和材质写入, 据实际可绘项选择后续灯表和 View 常量. 各路准备结束后统一封存.
        std::map<MaterialRow *, std::size_t> rows;
        const auto phase = ColorPhase(frame.needs);
        const bool needsSurface = frame.needs.NeedsCoreSurface();
        const bool needsColor = !frame.needs.surfaceOnly;
        const bool litColor = needsColor && !shadowLight && IsLitPreview(mode);
        const bool surfaceOnly = needsSurface && (!needsColor || shadowLight);
        for (const auto index : view.primitives)
        {
            PrepareDraw(device, frame, view, index, rows, phase, shadowLight != nullptr && !needsSurface, litColor,
                        surfaceOnly);
        }
        if (frame.draws.empty() && frame.degradation)
        {
            throw ForwardFailure{ViewSettingReason::SceneUnsupported, *frame.degradation};
        }
        // 核心与辅助受光共享本 View 的灯表、Froxel、阴影及环境准备.
        const bool usesLighting = litColor &&
            std::ranges::any_of(frame.materialRows, [](const auto &row) { return row->values.unlit == 0; });
        frame.usesLighting = usesLighting;
        if (shadowLight)
        {
            if (const auto *local = std::get_if<LocalShadowSelection>(&view.shadowDiagnostic))
            {
                frame.source.localShadow = *local;
                frame.environment.program = local->kind == Asset::ModelLightKind::Spot ?
                    LightingProgram::SpotPreview : LightingProgram::PointPreview;
            }
            else
            {
                frame.source.directionalCascades = shadowLight->regionCount;
                frame.environment.program = LightingProgram::CascadePreview;
            }
        }
        else if (needsColor)
        {
            PrepareEnvironment(device, frame, view, mode, usesLighting);
        }
        const auto groups = needsColor ? view.ShadowGroups(usesLighting) : std::span<const ShadowLightPlan>{};
        if (!groups.empty() && !frame.draws.empty())
        {
            if (usesLighting)
            {
                // 三个受光布局对应相同顺序的数组变体. 查询档在常量中切换.
                frame.environment.program = static_cast<LightingProgram>(
                    std::size_t(LightingProgram::LegacyShadows) + std::size_t(frame.environment.program));
            }
            PlanShadowDraws(device, frame, view, groups);
        }
        else
        {
            frame.shadow.reset();
        }
        frame.environment.program = OpaqueColorProgram(frame, frame.source.opaquePath);
        frame.deferredColor = DeferredColor(frame, frame.needs, frame.environment.program);
        if (frame.deferredColor)
        {
            frame.deferredViewValues = EncodeDeferredView(frame, view);
            device.shaderDemand.Add(*frame.deferredColor);
        }
        if (!frame.draws.empty())
        {
            // 只为实际核心或前向颜色准备灯表, 无光照的前向配方沿用共享零表.
            if (needsColor && !shadowLight)
            {
                PrepareLightTable(device, frame, view, usesLighting);
                if (frame.shadow)
                {
                    PrepareShadowReferences(device, frame, view);
                }
            }
            PrepareViewRow(device, frame, view, mode);
            // 固定等深辅助覆盖的顺序后, 仅合并相邻且兼容的 Draw. 句柄在两路完成规划后统一选代.
            SortDraws(frame, true);
            // GBuffer 按路由拆分批次, 两个辅助 Pass 沿相同的有序批次绘制.
            PackBatches(device, frame, needsSurface);
        }
        if (frame.environment.sky)
        {
            device.shaderDemand.Add({Shader::ProductShader::ForwardSky, 0});
        }
        for (const auto &batch : frame.batches)
        {
            VisitOpaqueDrawShaders(frame, frame.draws[batch.firstDraw], frame.needs, frame.environment.program,
                [&](ShaderPipelineRequest key) { device.shaderDemand.Add(key); });
        }
        if (frame.alternate && !frame.alternate->failure)
        {
            const auto &candidate = *frame.alternate;
            const auto program = OpaqueColorProgram(frame, candidate.path);
            if (const auto core = DeferredColor(frame, candidate.needs, program))
            {
                frame.deferredViewValues = EncodeDeferredView(frame, view);
                device.shaderDemand.Add(*core, false);
            }
            // 有效批次可能混合两种路由. 待选表面角色从其真正会处理的 Draw 收集.
            for (const auto &draw : frame.draws)
            {
                VisitOpaqueDrawShaders(frame, draw, candidate.needs, program,
                    [&](ShaderPipelineRequest key) { device.shaderDemand.Add(key, false); });
            }
        }
    }

    std::optional<ForwardFailure> CheckOpaqueShaders(const ForwardDevice &device,
        const ForwardFrameData &frame, const ViewResourceNeeds &needs, LightingProgram program) noexcept
    {
        ShaderDemand checked;
        std::optional<ForwardFailure> issue;
        const auto visit = [&](ShaderPipelineRequest key)
        {
            if ((issue && issue->reason != ViewSettingReason::ShaderPending) || checked.Contains(key))
            {
                return;
            }
            checked.Add(key);
            if (auto rejected = device.CheckSwitchShader(key))
            {
                issue = std::move(rejected);
            }
        };
        if (const auto core = DeferredColor(frame, needs, program))
        {
            visit(*core);
        }
        for (const auto &draw : frame.draws)
        {
            VisitOpaqueDrawShaders(frame, draw, needs, program, visit);
            if (issue && issue->reason != ViewSettingReason::ShaderPending)
            {
                break;
            }
        }
        return issue;
    }

    OpaqueResourceDemand OpaqueRequirements(const ForwardDevice &device, const ForwardFrameData &frame,
        const PreparedView &view, bool coreLighting, LightingProgram program) noexcept
    {
        const bool froxels = UsesFroxels(program);
        if ((froxels && !view.froxels) || (coreLighting && !frame.deferredViewValues))
        {
            return {};
        }
        OpaqueResourceDemand demand{};
        if (froxels)
        {
            demand.uploads = FroxelUpload::UploadRequirements();
            // 四块 GPU 缓冲共用一个 Allocation, 按合计字节和一个条目计入设备额度.
            demand.residency = {demand.uploads.first, 1};
        }
        if (coreLighting)
        {
            const auto values = std::as_bytes(std::span(&*frame.deferredViewValues, 1));
            const auto *screen = frame.deferredView.get();
            const bool resident = FitsDrawBuffer(device, screen, values.size());
            if (!resident)
            {
                demand.residency.bytes += values.size();
                ++demand.residency.entries;
            }
            if (!resident || !HasDrawBufferContents(*screen, values))
            {
                demand.uploads.first += values.size();
                ++demand.uploads.second;
            }
        }
        return demand;
    }

    void PrepareOpaqueResources(ForwardDevice &device, ForwardFrameData &frame, const PreparedView &view,
        const ViewResourceNeeds &needs, LightingProgram program, const GraphBufferSupport &buffers,
        const std::optional<ResidencyDemand> &protectedResidency)
    {
        const auto core = DeferredColor(frame, needs, program);
        if (protectedResidency)
        {
            const auto demand = OpaqueRequirements(device, frame, view, core.has_value(), program).residency;
            if (demand.entries != 0)
            {
                // 私有资源在同一 context 顺序分配. 先核对整组及其它有效计划的余量, 再开始本路尝试.
                device.CheckDeviceResidency({demand.bytes + protectedResidency->bytes,
                                             demand.entries + protectedResidency->entries});
            }
        }
        const bool rebatch = needs.NeedsCoreSurface() != frame.needs.NeedsCoreSurface() && !frame.draws.empty();
        std::vector<DrawBatch> batches;
        std::vector<RHI::DrawIndexedIndirectArguments> arguments;
        if (rebatch)
        {
            BuildBatches(device, frame, needs.NeedsCoreSurface(), batches, arguments, frame.batches);
            // 本轮有效材质和颜色角色随 Draw 冻结. 这里只调整批次边界, 复用原间接参数.
            assert(std::ranges::equal(std::as_bytes(std::span(arguments)), frame.indirect->contents));
        }
        auto &uploads = frame.uploads;
        const auto index = device.preparingView;
        const auto savedBudget = device.viewBudgets[index];
        const auto savedBytes = uploads.bytes;
        const auto savedWrites = uploads.writes;
        const auto savedBuffers = uploads.buffers.size();
        const auto savedScreenUse = frame.deferredView ? frame.deferredView->lastUse[index] : 0;

        // 候选写入先由上传批持有. 图取得正常回执前, 已接受的屏幕常量仍描述上一份内容.
        auto screen = frame.deferredView;
        std::optional<FroxelUpload> froxels;
        try
        {
            const auto [bytes, writes] = frame.opaqueUploadReservation;
            uploads.ReleaseWrites(bytes, writes);
            if (core)
            {
                if (!frame.deferredViewValues)
                {
                    Reject(ViewSettingReason::InvalidProjection,
                           "DeferredLighting inverse projection exceeds the GPU float range");
                }
                const auto values = std::as_bytes(std::span(&*frame.deferredViewValues, 1));
                UseDrawBuffer(device, screen, GraphUsage::ConstantBuffer, values.size());
                if (!HasDrawBufferContents(*screen, values))
                {
                    // 为正常回执预留 CPU 字节空间, 使确认阶段只需复制实际接受的行.
                    screen->contents.reserve(values.size());
                    uploads.Add(screen, screen->buffer, values, GraphUploadSource::ForwardView, index);
                }
            }
            if (UsesFroxels(program))
            {
                if (!view.froxels)
                {
                    Core::BoundedText issue("Cannot prepare Froxel grid: ");
                    issue.Append(FroxelStatusName(view.froxelStatus));
                    Reject(ViewSettingReason::ScenePreparationFailed, issue.View());
                }
                froxels = FroxelUpload::Prepare(device, uploads, view, buffers);
            }
        }
        catch (...)
        {
            // 撤回私有写入和使用证据. 缓存仍描述尝试前的内容及接受状态.
            uploads.buffers.resize(savedBuffers);
            uploads.bytes = savedBytes;
            uploads.writes = savedWrites;
            device.viewBudgets[index] = savedBudget;
            if (frame.deferredView)
            {
                frame.deferredView->lastUse[index] = savedScreenUse;
            }
            throw;
        }
        if (rebatch)
        {
            frame.batches = std::move(batches);
        }
        frame.needs = needs;
        frame.environment.program = program;
        frame.deferredColor = core;
        frame.deferredView = std::move(screen);
        frame.froxelUpload = std::move(froxels);
        frame.opaqueUploadReservation = {};
        frame.opaqueResourcesReady = true;
    }

    /// @brief 从冻结代确认只读导入. 程序变化时才为原计划补办重绘, 沿用该 View 的准备预算.
    static void ApplyShadowShaders(ForwardDevice &device, ForwardFrameData &frame, const PreparedView &view)
    {
        auto &shadow = *frame.shadow;
        auto &depth = shadow.depth;
        // 仅清除的层组与深度程序独立. 含绘制的组用该角色的包摘要识别会改变阴影写入的换代.
        const auto program = depth.variants.any() ?
            device.activeShaders->programs.ProgramContent(Shader::ProductShader::StaticShadowDepth) : std::string_view{};
        if (depth.retained)
        {
            if (depth.program.View() == program)
            {
                return;
            }
            depth.retained = {};
            PrepareShadowCasters(device, frame, view, view.ShadowGroups(true));
        }
        for (auto &level : std::span(shadow.levels).first(depth.count))
        {
            for (const auto &batch : level.batches)
            {
                auto &draw = level.draws[batch.firstDraw];
                draw.depthPipeline = device.RequestShader(DrawShader(frame, draw, SurfacePhase::ShadowDepth));
                if (!draw.depthPipeline)
                {
                    Reject(ViewSettingReason::ShaderPending, "Waiting for prepared shadow depth pipelines");
                }
            }
        }
        depth.program.Assign(program);
    }

    void ApplyDrawShaders(ForwardDevice &device, ForwardFrameData &frame, const PreparedView &view)
    {
        bool shaderReady = true;
        if (frame.environment.sky)
        {
            frame.environment.skyPipeline = device.RequestShader({Shader::ProductShader::ForwardSky, 0});
            frame.environment.skyLayout = device.activeShaders->programs.skyLayout;
            shaderReady = bool(frame.environment.skyPipeline);
        }
        if (frame.deferredColor)
        {
            frame.deferredPipeline = device.RequestShader(*frame.deferredColor);
            frame.deferredSurfaceLayout = device.deferredSurfaceLayout;
            shaderReady &= bool(frame.deferredPipeline);
        }
        const auto phase = ColorPhase(frame.needs);
        // 本帧只从已选代取得各批次使用的句柄. 实例及间接参数已按固定状态分组.
        for (const auto &batch : frame.batches)
        {
            auto &draw = frame.draws[batch.firstDraw];
            const auto route = frame.materialRows[draw.material]->route;
            if (UsesForwardColor(frame.needs, route))
            {
                draw.pipeline = device.RequestShader(DrawShader(frame, draw, phase));
                shaderReady &= bool(draw.pipeline);
            }
            if (frame.needs.NeedsCoreSurface())
            {
                draw.surfacePipeline = device.RequestShader(SurfaceShader(route, draw));
                shaderReady &= bool(draw.surfacePipeline);
            }
            if (frame.needs.NeedsEarlyDepth())
            {
                draw.depthPipeline = device.RequestShader(DrawShader(frame, draw, SurfacePhase::Depth));
                shaderReady &= bool(draw.depthPipeline);
            }
        }
        if (!shaderReady)
        {
            Reject(ViewSettingReason::ShaderPending, "Waiting for prepared Forward pipelines");
        }
        if (frame.shadow)
        {
            ApplyShadowShaders(device, frame, view);
            if (const auto &skipped = frame.shadow->depth.degradation)
            {
                // 新产出与只读导入都取该深度组的实际跳过摘要. 主表面的摘要来自本轮绘制准备.
                if (frame.degradation)
                {
                    frame.degradation->Append(" | Shadow casters - ");
                    frame.degradation->Append(skipped->View());
                }
                else
                {
                    frame.degradation = skipped;
                }
            }
        }
        if (!frame.needs.surfaceOnly)
        {
            const auto program = frame.environment.program;
            frame.lightLayout = device.lightLayouts[LightLayoutIndex(program)];
            frame.sheenLightLayout = device.lightLayouts[LightLayoutIndex(program, true)];
            if (UsesFroxels(program))
            {
                assert(device.froxelViewLayout);
                frame.froxelViewLayout = device.froxelViewLayout;
            }
        }
    }
}
