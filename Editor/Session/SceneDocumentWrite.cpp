#include "SceneDocument.hpp"

/// @file
/// @brief 独立快照的场景 v1 编码. 模型数据和图像像素以受控引用表示.
/// 主 View 只写捕获时的请求, 不写派生镜头, 曝光或最后渲染值; 安全文件替换由调用方负责.

#include <cassert>
#include <type_traits>
#include <utility>

#include <nlohmann/json.hpp>

namespace Physara::Editor::Detail
{
    using Json = nlohmann::json;

    /// @brief 用 UTF-8 和通用分隔符保存项目相对路径, 不写入本机资源根.
    static std::string PathText(const std::filesystem::path &path)
    {
        const auto utf8 = path.generic_u8string();
        return {reinterpret_cast<const char *>(utf8.data()), utf8.size()};
    }

    /// @brief 将持久源身份写为固定长度的小写十六进制字符串.
    static std::string FormatVersion(const Core::Sha256Digest &version)
    {
        constexpr std::string_view Digits = "0123456789abcdef";
        std::string text;
        text.reserve(version.size() * 2);
        for (const auto value : version)
        {
            text.push_back(Digits[value >> 4]);
            text.push_back(Digits[value & 15]);
        }
        return text;
    }

    /// @brief 只写创作平移和 xyzw 四元数, 不保存求值后的矩阵.
    static Json WritePose(const Scene::Pose &pose)
    {
        return {{"translation", pose.translation}, {"rotation", pose.rotation}};
    }

    /// @brief 可选字段缺席时省略成员, 不用 null 混淆继承与无限值.
    template <typename T>
    static void Set(Json &obj, std::string_view key, const std::optional<T> &value)
    {
        if (value)
        {
            obj[std::string(key)] = *value;
        }
    }

    /// @brief 分别写出两种投影, 透视的无限远面明确编码为 null.
    static Json WriteCamera(const Scene::Camera &camera)
    {
        return std::visit([](const auto &value) -> Json
        {
            if constexpr (std::is_same_v<std::decay_t<decltype(value)>, Asset::ModelPerspective>)
            {
                Json json{{"type", "perspective"}, {"yfov", value.yfov}, {"near", value.znear},
                    {"far", value.zfar ? Json(*value.zfar) : Json(nullptr)}};
                Set(json, "aspectRatio", value.aspectRatio);
                return json;
            }
            else
            {
                return {{"type", "orthographic"}, {"xmag", value.xmag}, {"ymag", value.ymag},
                    {"near", value.znear}, {"far", value.zfar}};
            }
        }, camera);
    }

    /// @brief 保留领域层已验证的物理参数, 只为聚光灯写入锥角.
    static Json WriteLight(const Scene::Light &light)
    {
        using Kind = Asset::ModelLightKind;
        const auto *type = light.type == Kind::Directional ? "directional" :
            (light.type == Kind::Point ? "point" : "spot");
        Json value{{"type", type}, {"color", light.color}, {"intensity", light.intensity}};
        Set(value, "range", light.range);
        if (light.type == Kind::Spot)
        {
            value["innerConeAngle"] = light.innerConeAngle;
            value["outerConeAngle"] = light.outerConeAngle;
        }
        return value;
    }

    /// @brief 仅编码实际存在的实例字段, 未修改的共享材质仍由模型提供.
    static Json WriteMaterial(const Scene::MaterialOverride &value)
    {
        auto node = Json::object();
        Set(node, "baseColor", value.baseColor);
        Set(node, "emissive", value.emissive);
        Set(node, "metallic", value.metallic);
        Set(node, "roughness", value.roughness);
        Set(node, "normalScale", value.normalScale);
        Set(node, "occlusionStrength", value.occlusionStrength);
        Set(node, "emissiveStrength", value.emissiveStrength);
        Set(node, "alphaCutoff", value.alphaCutoff);
        Set(node, "doubleSided", value.doubleSided);
        if (value.alphaMode)
        {
            using Mode = Asset::ModelAlpha;
            node["alphaMode"] = *value.alphaMode == Mode::Opaque ? "OPAQUE" :
                (*value.alphaMode == Mode::Mask ? "MASK" : "BLEND");
        }
        return node;
    }

    /// @brief 映射 Scene 已验证的四类采样用途, 保持 v1 的稳定字段拼写.
    static const char *TextureUse(Asset::ImageUse use) noexcept
    {
        switch (use)
        {
        case Asset::ImageUse::BaseColor:
            return "baseColor";
        case Asset::ImageUse::OcclusionRoughnessMetallic:
            return "occlusionRoughnessMetallic";
        case Asset::ImageUse::Normal:
            return "normal";
        case Asset::ImageUse::Emissive:
            return "emissive";
        case Asset::ImageUse::EditorColor:
        case Asset::ImageUse::LinearData:
            break;
        }
        assert(false);
        return "";
    }

    /// @brief 外置图像写资源引用, sampler 和 UV 只写被修改的字段.
    static Json WriteTexture(const Scene::TextureOverride &value)
    {
        auto node = Json::object();
        if (value.image)
        {
            assert(value.use);
            node["path"] = PathText(value.image->source);
            node["use"] = TextureUse(*value.use);
        }
        Set(node, "texCoord", value.texCoord);
        Set(node, "offset", value.offset);
        Set(node, "scale", value.scale);
        Set(node, "rotation", value.rotationRadians);
        if (value.sampler != Scene::SamplerOverride{})
        {
            auto sampler = Json::object();
            Set(sampler, "minFilter", value.sampler.minFilter);
            Set(sampler, "magFilter", value.sampler.magFilter);
            Set(sampler, "wrapS", value.sampler.wrapS);
            Set(sampler, "wrapT", value.sampler.wrapT);
            node["sampler"] = std::move(sampler);
        }
        return node;
    }

    /// @brief 按源槽保存一份稀疏覆盖, 没有修改的材质或纹理组不生成对象.
    static Json WritePrimitive(const Scene::PrimitiveOverride &value)
    {
        Json node{{"primitiveSlot", value.primitiveSlot}};
        if (value.material != Scene::MaterialOverride{})
        {
            node["material"] = WriteMaterial(value.material);
        }
        constexpr std::array<std::string_view, Scene::MaterialSlotCount> Slots{
            "baseColor", "metallicRoughness", "normal", "occlusion", "emissive"};
        auto textures = Json::object();
        for (std::size_t i = 0; i < value.textures.size(); ++i)
        {
            if (value.textures[i])
            {
                textures[std::string(Slots[i])] = WriteTexture(*value.textures[i]);
            }
        }
        if (!textures.empty())
        {
            node["textures"] = std::move(textures);
        }
        return node;
    }

    /// @brief 只编码创作字段及稳定引用, 世界数据由恢复后的 Scene 再求值.
    static Json WriteEntity(const Scene::EntityRecord &record)
    {
        const auto &props = record.properties;
        auto local = WritePose(props.local);
        local["scale"] = props.local.scale;
        Json entity{{"id", record.id}, {"name", props.name}, {"visible", props.visible},
                    {"parent", record.parent ? Json(*record.parent) : Json(nullptr)}, {"local", std::move(local)}};
        if (record.model)
        {
            entity["modelInstance"] = {{"path", PathText(record.model->source)}, {"scene", record.model->sourceScene},
                                       {"sourceVersion", FormatVersion(record.model->sourceVersion)}};
        }
        if (record.source)
        {
            entity["sourceRef"] = {{"instanceEntityId", record.source->instanceEntityId},
                                   {"node", record.source->node}};
        }
        if (props.castsShadow)
        {
            entity["meshRenderer"] = {{"castsShadow", *props.castsShadow}};
        }
        if (props.camera)
        {
            entity["camera"] = WriteCamera(*props.camera);
        }
        if (props.light)
        {
            entity["light"] = WriteLight(*props.light);
        }
        if (!record.overrides.empty())
        {
            auto overrides = Json::array();
            for (const auto &value : record.overrides)
            {
                overrides.push_back(WritePrimitive(value));
            }
            entity["primitiveOverrides"] = std::move(overrides);
        }
        return entity;
    }

    /// @brief 已知 v1 结构深度有界. 计数与读取端的 JSON 值预算一致.
    static void CountValues(const Json &value, std::size_t &remaining)
    {
        if (remaining == 0)
        {
            throw Core::BoundedText("Scene document structure budget exceeded");
        }
        --remaining;
        if (value.is_structured())
        {
            for (const auto &child : value)
            {
                CountValues(child, remaining);
            }
        }
    }

    // 输入是保存边界已捕获的合法快照. 编码失败或取消时只丢弃局部文本, 不接触目标文件.
    std::string EncodeSceneDocument(std::span<const Scene::EntityRecord> entities,
                                    const Renderer::ViewDocumentRequests &view, std::stop_token stop,
                                    const std::optional<Scene::SceneEnvironment> &environment)
    {
        std::string output = "{\"version\":1,\"entities\":[";
        std::size_t remaining = DocumentNodeLimit - 3; // 顶层对象、version 及 entities 数组.
        const auto append = [&output](std::string_view piece)
        {
            if (piece.size() > DocumentByteLimit - output.size())
            {
                throw Core::BoundedText("Scene document byte limit exceeded");
            }
            output.append(piece);
        };
        bool first = true;
        // 每次只构造一个实体的 JSON 临时树, 结构计数累计到整份文档, 输出追加另受字节预算约束.
        for (const auto &record : entities)
        {
            if (stop.stop_requested())
            {
                throw Core::BoundedText("Document encoding was cancelled");
            }
            auto entity = WriteEntity(record);
            CountValues(entity, remaining);
            append(first ? "\n" : ",\n");
            append(entity.dump());
            first = false;
        }
        // 物理配置来自捕获的请求, 保持秒, 米及档数等领域单位, 即使对应场景颜色尚未获确认也要保存.
        const auto &camera = view.physicalCamera;
        Json physicalCamera{{"fNumber", camera.fNumber},
                            {"shutterSeconds", camera.shutterSeconds},
                            {"iso", camera.iso},
                            {"exposureCompensationEv", camera.exposureCompensationEv},
                            {"sensorHeightMeters", camera.sensorHeightMeters},
                            {"navigationFocalLengthMeters", camera.navigationFocalLengthMeters},
                            {"focusDistanceMeters", camera.focusDistanceMeters}};
        Json mainView{{"cameraPose", WritePose(view.cameraPose)},
                      {"cameraEntityId", view.cameraEntityId ? Json(*view.cameraEntityId) : Json(nullptr)},
                      {"requested", {{"renderScale", view.renderScale}, {"physicalCamera", std::move(physicalCamera)}}}};
        CountValues(mainView, remaining);
        append("\n],\"mainView\":");
        append(mainView.dump());
        if (environment)
        {
            const auto &settings = environment->settings;
            const Json value{{"path", PathText(environment->source->source)}, {"rotation", settings.rotation},
                             {"intensity", settings.intensity}, {"lighting", settings.lighting}, {"sky", settings.sky}};
            CountValues(value, remaining);
            append(",\"environment\":");
            append(value.dump());
        }
        append("}\n");
        return output;
    }
}
