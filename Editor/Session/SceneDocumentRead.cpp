#include "SceneDocument.hpp"

/// @file
/// @brief 将受限 UTF-8 JSON 解码为 v1 文档值, 资产引用暂不绑定.
/// 文件层检查字段形状和数值表示, View 校验物理值及派生曝光; 失败不发布部分候选.

#include <algorithm>
#include <cmath>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <unordered_map>
#include <unordered_set>

#include <nlohmann/json.hpp>

namespace Physara::Editor::Detail
{
    using Json = nlohmann::json;

    // 先限定 Scenes 下的资源路径与文档后缀, 实际文件落点仍由受控 I/O 检查.
    std::variant<Core::ResourcePath, Core::ResourceFailure> ParseScenePath(const std::filesystem::path &path)
    {
        constexpr std::array<std::wstring_view, 1> Allowed{L"Scenes"};
        auto result = Core::ResourcePath::Parse(path, Allowed);
        if (const auto *parsed = std::get_if<Core::ResourcePath>(&result))
        {
            const auto name = parsed->Relative().filename().native();
            constexpr std::wstring_view Suffix = L".scene.json";
            if (name.size() <= Suffix.size() || !name.ends_with(Suffix))
            {
                return Core::ResourceFailure{Core::ResourceError::InvalidPath,
                    Core::BoundedText("Scene document requires a name ending in .scene.json")};
            }
        }
        return result;
    }

    /// @brief 文件定位只随冷路径的字段访问构造, 类型错误能指向具体值.
    class Field final
    {
    public:
        Field(const Json &value, std::string path) : m_Value(value), m_Path(std::move(path)) {}

        /// @brief 形成当前字段的有界错误, 中止整份候选解码.
        [[noreturn]] void Error(std::string_view reason) const
        {
            Core::BoundedText message(m_Path);
            message.Append(": ");
            message.Append(reason);
            throw message;
        }

        /// @brief 限定 v1 对象成员集合; 必填字段由随后的 Required 单独检查.
        void Object(std::initializer_list<std::string_view> keys) const
        {
            if (!m_Value.is_object())
            {
                Error("Expected an object");
            }
            for (auto item = m_Value.begin(); item != m_Value.end(); ++item)
            {
                if (std::ranges::find(keys, item.key()) == keys.end())
                {
                    Field(item.value(), m_Path + "/" + item.key()).Error("Unknown v1 field");
                }
            }
        }

        /// @brief 区分字段缺席与显式 null, 返回的定位继续借用同一 JSON 树.
        [[nodiscard]] std::optional<Field> Optional(std::string_view key) const
        {
            const auto item = m_Value.find(key);
            return item == m_Value.end() ? std::nullopt :
                std::optional(Field(*item, m_Path + "/" + std::string(key)));
        }

        /// @brief 缺席时报告带成员名的错误, 不以缺省值替代必填字段.
        [[nodiscard]] Field Required(std::string_view key) const
        {
            const auto field = Optional(key);
            if (!field)
            {
                Field(m_Value, m_Path + "/" + std::string(key)).Error("Missing required field");
            }
            return *field;
        }

        /// @brief 校验数组类型并取得长度, 后续 Item 只在此范围内访问.
        [[nodiscard]] std::size_t Size() const
        {
            if (!m_Value.is_array())
            {
                Error("Expected an array");
            }
            return m_Value.size();
        }

        [[nodiscard]] Field Item(std::size_t at) const
        {
            return {m_Value[at], m_Path + "/" + std::to_string(at)};
        }

        [[nodiscard]] bool IsNull() const noexcept { return m_Value.is_null(); }

        /// @brief 读取严格类型的标量. 字符串的 UTF-8 合法性由 JSON 解析器确认.
        [[nodiscard]] std::string Text() const
        {
            if (!m_Value.is_string())
            {
                Error("Expected UTF-8 text");
            }
            return m_Value.get<std::string>();
        }

        [[nodiscard]] bool Flag() const
        {
            if (!m_Value.is_boolean())
            {
                Error("Expected a boolean");
            }
            return m_Value.get<bool>();
        }

        /// @brief 在窄化之前拒绝负数, 非整数和超出 v1 下标表示范围的数值.
        [[nodiscard]] std::uint32_t Index() const
        {
            if (!m_Value.is_number_integer() || (!m_Value.is_number_unsigned() && m_Value.get<std::int64_t>() < 0))
            {
                Error("Expected an unsigned integer");
            }
            const auto value = m_Value.get<std::uint64_t>();
            if (value > std::numeric_limits<std::uint32_t>::max())
            {
                Error("Integer exceeds the v1 range");
            }
            return static_cast<std::uint32_t>(value);
        }

        /// @brief 接受有限 JSON 数字, 转成字段类型前检查其可表示的绝对值上界.
        template <typename T = double>
        [[nodiscard]] T Number() const
        {
            if (!m_Value.is_number())
            {
                Error("Expected a finite number");
            }
            const auto value = m_Value.get<double>();
            if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<T>::max())
            {
                Error("Number exceeds the field representation");
            }
            return static_cast<T>(value);
        }

        /// @brief 按固定长度读取数值向量, 错误保留具体元素下标.
        template <typename T, std::size_t N>
        [[nodiscard]] std::array<T, N> Vector() const
        {
            if (Size() != N)
            {
                Error("Invalid vector length");
            }
            std::array<T, N> result{};
            for (std::size_t i = 0; i < N; ++i)
            {
                result[i] = Item(i).Number<T>();
            }
            return result;
        }

        /// @brief 将 UTF-8 路径变为受控项目相对路径, 物理落点留待实际 I/O 核验.
        [[nodiscard]] std::filesystem::path Resource(std::wstring_view directory) const
        {
            const auto text = Text();
            std::u8string utf8(text.size(), u8'\0');
            std::memcpy(utf8.data(), text.data(), text.size());
            const std::array allowed{directory};
            const auto result = Core::ResourcePath::Parse(std::filesystem::path(utf8), allowed);
            if (const auto *error = std::get_if<Core::ResourceFailure>(&result))
            {
                Error(error->description.View());
            }
            return std::get<Core::ResourcePath>(result).Relative();
        }

    private:
        const Json &m_Value;
        std::string m_Path;
    };

    /// @brief 解码固定长度 SHA-256 十六进制身份, 不将进程内 revision 解释为源版本.
    static Core::Sha256Digest ReadVersion(const Field &field)
    {
        const auto text = field.Text();
        Core::Sha256Digest version{};
        if (text.size() != version.size() * 2)
        {
            field.Error("Expected a SHA-256 sourceVersion");
        }
        for (std::size_t i = 0; i < version.size(); ++i)
        {
            unsigned value = 0;
            for (std::size_t digit = 0; digit != 2; ++digit)
            {
                const auto c = text[i * 2 + digit];
                if (c >= '0' && c <= '9')
                {
                    value = value * 16 + static_cast<unsigned>(c - '0');
                }
                else if ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
                {
                    value = value * 16 + static_cast<unsigned>(c >= 'a' ? c - 'a' + 10 : c - 'A' + 10);
                }
                else
                {
                    field.Error("Expected hexadecimal sourceVersion");
                }
            }
            version[i] = static_cast<unsigned char>(value);
        }
        return version;
    }

    /// @brief 读取平移和 xyzw 旋转的表示, 单位旋转约束由所属领域入口检查.
    static Scene::Pose ReadPose(const Field &field)
    {
        return {field.Required("translation").Vector<double, 3>(), field.Required("rotation").Vector<double, 4>()};
    }

    /// @brief 按投影种类读取不同字段, 仅透视 far 的 null 表示无限远.
    static Scene::Camera ReadCamera(const Field &field)
    {
        const auto type = field.Required("type").Text();
        if (type == "perspective")
        {
            field.Object({"type", "yfov", "near", "far", "aspectRatio"});
            Asset::ModelPerspective result;
            result.yfov = field.Required("yfov").Number();
            result.znear = field.Required("near").Number();
            const auto far = field.Required("far");
            if (!far.IsNull())
            {
                result.zfar = far.Number();
            }
            if (const auto aspect = field.Optional("aspectRatio"))
            {
                result.aspectRatio = aspect->Number();
            }
            return result;
        }
        if (type == "orthographic")
        {
            field.Object({"type", "xmag", "ymag", "near", "far"});
            return Asset::ModelOrthographic{field.Required("xmag").Number(), field.Required("ymag").Number(),
                                             field.Required("near").Number(), field.Required("far").Number()};
        }
        field.Error("Unsupported camera type");
    }

    /// @brief 依据灯类型限制成员集合, range 缺席保留无截断语义.
    static Scene::Light ReadLight(const Field &field)
    {
        const auto type = field.Required("type").Text();
        Scene::Light value;
        if (type == "directional")
        {
            field.Object({"type", "color", "intensity"});
        }
        else if (type == "point")
        {
            field.Object({"type", "color", "intensity", "range"});
            value.type = Asset::ModelLightKind::Point;
        }
        else if (type == "spot")
        {
            field.Object({"type", "color", "intensity", "range", "innerConeAngle", "outerConeAngle"});
            value.type = Asset::ModelLightKind::Spot;
            value.innerConeAngle = field.Required("innerConeAngle").Number();
            value.outerConeAngle = field.Required("outerConeAngle").Number();
        }
        else
        {
            field.Error("Unsupported light type");
        }
        value.color = field.Required("color").Vector<float, 3>();
        value.intensity = field.Required("intensity").Number();
        if (const auto range = field.Optional("range"))
        {
            value.range = range->Number();
        }
        return value;
    }

    /// @brief 缺席表示继承, 显式 null 仍通过数值类型检查被拒绝.
    static std::optional<float> OptionalFloat(const Field &field, std::string_view key)
    {
        const auto value = field.Optional(key);
        return value ? std::optional(value->Number<float>()) : std::nullopt;
    }

    /// @brief 读取已修改的核心材质字段, 不在文件层展开源材质的缺省值.
    static Scene::MaterialOverride ReadMaterial(const Field &field)
    {
        field.Object({"baseColor", "metallic", "roughness", "emissive", "emissiveStrength", "normalScale",
                      "occlusionStrength", "alphaMode", "alphaCutoff", "doubleSided"});
        Scene::MaterialOverride value;
        if (const auto color = field.Optional("baseColor"))
        {
            value.baseColor = color->Vector<float, 4>();
        }
        if (const auto color = field.Optional("emissive"))
        {
            value.emissive = color->Vector<float, 3>();
        }
        value.metallic = OptionalFloat(field, "metallic");
        value.roughness = OptionalFloat(field, "roughness");
        value.normalScale = OptionalFloat(field, "normalScale");
        value.occlusionStrength = OptionalFloat(field, "occlusionStrength");
        value.emissiveStrength = OptionalFloat(field, "emissiveStrength");
        value.alphaCutoff = OptionalFloat(field, "alphaCutoff");
        if (const auto mode = field.Optional("alphaMode"))
        {
            const auto text = mode->Text();
            if (text == "OPAQUE")
            {
                value.alphaMode = Asset::ModelAlpha::Opaque;
            }
            else if (text == "MASK")
            {
                value.alphaMode = Asset::ModelAlpha::Mask;
            }
            else if (text == "BLEND")
            {
                value.alphaMode = Asset::ModelAlpha::Blend;
            }
            else
            {
                mode->Error("Invalid alphaMode");
            }
        }
        if (const auto sided = field.Optional("doubleSided"))
        {
            value.doubleSided = sided->Flag();
        }
        return value;
    }

    /// @brief 将 v1 的纹理用途名映射为 Asset 语义, 不接受编辑器专用或任意数据用途.
    static Asset::ImageUse ReadUse(const Field &field)
    {
        const auto name = field.Text();
        constexpr std::array Names{"baseColor", "occlusionRoughnessMetallic", "normal", "emissive"};
        constexpr std::array Values{Asset::ImageUse::BaseColor, Asset::ImageUse::OcclusionRoughnessMetallic,
                                    Asset::ImageUse::Normal, Asset::ImageUse::Emissive};
        const auto found = std::ranges::find(Names, name);
        if (found == Names.end())
        {
            field.Error("Unsupported scene texture use");
        }
        return Values[static_cast<std::size_t>(found - Names.begin())];
    }

    /// @brief 读取可省略的下标或采样枚举, 有值时仍须满足整数表示约束.
    static std::optional<std::uint32_t> OptionalIndex(const Field &field, std::string_view key)
    {
        const auto value = field.Optional(key);
        return value ? std::optional(value->Index()) : std::nullopt;
    }

    /// @brief 将外置图片加入未绑定引用表, 其余采样修改保留稀疏值供 Scene 校验.
    static Scene::TextureOverride ReadTexture(const Field &field, ParsedSceneDocument &document,
                                              std::uint32_t primitive, std::size_t slot)
    {
        field.Object({"path", "use", "texCoord", "sampler", "offset", "scale", "rotation"});
        Scene::TextureOverride value;
        if (const auto path = field.Optional("path"))
        {
            value.use = ReadUse(field.Required("use"));
            document.images.push_back({document.entities.size(), primitive, slot, path->Resource(L"Textures")});
        }
        else if (const auto use = field.Optional("use"))
        {
            use->Error("External texture use requires a path");
        }
        value.texCoord = OptionalIndex(field, "texCoord");
        if (const auto sampler = field.Optional("sampler"))
        {
            sampler->Object({"minFilter", "magFilter", "wrapS", "wrapT"});
            value.sampler = {OptionalIndex(*sampler, "minFilter"), OptionalIndex(*sampler, "magFilter"),
                             OptionalIndex(*sampler, "wrapS"), OptionalIndex(*sampler, "wrapT")};
        }
        if (const auto offset = field.Optional("offset"))
        {
            value.offset = offset->Vector<float, 2>();
        }
        if (const auto scale = field.Optional("scale"))
        {
            value.scale = scale->Vector<float, 2>();
        }
        value.rotationRadians = OptionalFloat(field, "rotation");
        return value;
    }

    /// @brief 一个覆盖按源 Primitive 槽定位, 仅保留当前格式支持的材质和纹理成员.
    static Scene::PrimitiveOverride ReadPrimitive(const Field &field, ParsedSceneDocument &document)
    {
        field.Object({"primitiveSlot", "material", "textures"});
        Scene::PrimitiveOverride value{.primitiveSlot = field.Required("primitiveSlot").Index()};
        if (const auto material = field.Optional("material"))
        {
            value.material = ReadMaterial(*material);
        }
        if (const auto textures = field.Optional("textures"))
        {
            constexpr std::array<std::string_view, Scene::MaterialSlotCount> Slots{
                "baseColor", "metallicRoughness", "normal", "occlusion", "emissive"};
            textures->Object({Slots[0], Slots[1], Slots[2], Slots[3], Slots[4]});
            for (std::size_t i = 0; i < Slots.size(); ++i)
            {
                if (const auto texture = textures->Optional(Slots[i]))
                {
                    value.textures[i] = ReadTexture(*texture, document, value.primitiveSlot, i);
                }
            }
        }
        return value;
    }

    /// @brief 读取显式实体终态并建立资产引用位置, 不按模型源层级自动补回被删节点.
    static Scene::EntityRecord ReadEntity(const Field &field, ParsedSceneDocument &document)
    {
        field.Object({"id", "name", "parent", "local", "visible", "modelInstance", "sourceRef", "meshRenderer",
                      "camera", "light", "primitiveOverrides"});
        Scene::EntityRecord record;
        record.id = field.Required("id").Text();
        record.properties.name = field.Required("name").Text();
        record.properties.visible = field.Required("visible").Flag();
        const auto parent = field.Required("parent");
        if (!parent.IsNull())
        {
            record.parent = parent.Text();
        }
        const auto local = field.Required("local");
        local.Object({"translation", "rotation", "scale"});
        record.properties.local = {ReadPose(local), local.Required("scale").Vector<double, 3>()};
        if (!Scene::IsValidPose(record.properties.local))
        {
            local.Error("Invalid local pose");
        }
        if (const auto model = field.Optional("modelInstance"))
        {
            model->Object({"path", "scene", "sourceVersion"});
            document.models.push_back({document.entities.size(), model->Required("path").Resource(L"Models"),
                model->Required("scene").Index(), ReadVersion(model->Required("sourceVersion"))});
        }
        if (const auto source = field.Optional("sourceRef"))
        {
            source->Object({"instanceEntityId", "node"});
            record.source = Scene::SourceRef{source->Required("instanceEntityId").Text(),
                                             source->Required("node").Index()};
        }
        if (const auto mesh = field.Optional("meshRenderer"))
        {
            mesh->Object({"castsShadow"});
            record.properties.castsShadow = mesh->Required("castsShadow").Flag();
        }
        if (const auto light = field.Optional("light"))
        {
            record.properties.light = ReadLight(*light);
        }
        if (const auto camera = field.Optional("camera"))
        {
            record.properties.camera = ReadCamera(*camera);
        }
        if (const auto overrides = field.Optional("primitiveOverrides"))
        {
            const auto count = overrides->Size();
            record.overrides.reserve(count);
            for (std::size_t i = 0; i < count; ++i)
            {
                record.overrides.push_back(ReadPrimitive(overrides->Item(i), document));
            }
            std::ranges::sort(record.overrides, {}, &Scene::PrimitiveOverride::primitiveSlot);
            const auto repeated = std::ranges::adjacent_find(record.overrides, {},
                                                             &Scene::PrimitiveOverride::primitiveSlot);
            if (repeated != record.overrides.end())
            {
                overrides->Error("Repeated Primitive slot");
            }
        }
        return record;
    }

    /// @brief 读取明确存在的物理对象. 七项参数完整且使用领域单位, 值域由 View 统一检查.
    static Renderer::PhysicalCameraRequest ReadPhysicalCamera(const Field &field)
    {
        field.Object({"fNumber", "shutterSeconds", "iso", "exposureCompensationEv", "sensorHeightMeters",
                      "navigationFocalLengthMeters", "focusDistanceMeters"});
        return {.fNumber = field.Required("fNumber").Number<double>(),
                .shutterSeconds = field.Required("shutterSeconds").Number<double>(),
                .iso = field.Required("iso").Number<double>(),
                .exposureCompensationEv = field.Required("exposureCompensationEv").Number<double>(),
                .sensorHeightMeters = field.Required("sensorHeightMeters").Number<double>(),
                .navigationFocalLengthMeters = field.Required("navigationFocalLengthMeters").Number<double>(),
                .focusDistanceMeters = field.Required("focusDistanceMeters").Number<double>()};
    }

    /// @brief 恢复主 View 的持久请求并复用值域校验, 实体相机的存在性稍后统一检查.
    static Renderer::ViewDocumentRequests ReadView(const Field &field)
    {
        field.Object({"cameraPose", "cameraEntityId", "requested"});
        const auto pose = field.Required("cameraPose");
        pose.Object({"translation", "rotation"});
        const auto requested = field.Required("requested");
        requested.Object({"renderScale", "physicalCamera"});
        const auto camera = field.Required("cameraEntityId");
        Renderer::ViewDocumentRequests view{.cameraPose = ReadPose(pose),
                                            .renderScale = requested.Required("renderScale").Number<float>()};
        if (!camera.IsNull())
        {
            view.cameraEntityId = camera.Text();
        }
        if (const auto physical = requested.Optional("physicalCamera"))
        {
            // 仅整个对象缺席时沿用 View 缺省; 显式 null, 缺字段和未知字段都交给严格读取拒绝.
            view.physicalCamera = ReadPhysicalCamera(*physical);
        }
        if (auto error = Renderer::View::ValidateDocumentRequests(view))
        {
            field.Error(error->View());
        }
        return view;
    }

    /// @brief 在资产准入之前检查文档自己的 ID、引用及环. GPU/Asset 状态无需参与这些判断.
    static void ValidateLinks(const ParsedSceneDocument &doc, const Field &entities, const Field &view)
    {
        const auto count = doc.entities.size();
        std::unordered_map<std::string_view, std::size_t> ids;
        ids.reserve(count);
        std::vector<bool> isModel(count, false);
        for (const auto &model : doc.models)
        {
            isModel[model.entity] = true;
        }
        for (std::size_t i = 0; i < count; ++i)
        {
            const auto &id = doc.entities[i].id;
            if (id.empty() || !ids.try_emplace(id, i).second)
            {
                entities.Item(i).Required("id").Error("Empty or repeated entity ID");
            }
        }
        if (doc.view.cameraEntityId)
        {
            const auto camera = ids.find(*doc.view.cameraEntityId);
            if (camera == ids.end() || !doc.entities[camera->second].properties.camera)
            {
                view.Required("cameraEntityId").Error("Expected an entity with a camera");
            }
        }
        std::vector<std::size_t> parents(count, count);
        for (std::size_t i = 0; i < count; ++i)
        {
            const auto &record = doc.entities[i];
            if (record.parent)
            {
                const auto parent = ids.find(*record.parent);
                if (parent == ids.end())
                {
                    entities.Item(i).Required("parent").Error("Missing parent entity");
                }
                parents[i] = parent->second;
            }
            if (record.source)
            {
                const auto root = ids.find(record.source->instanceEntityId);
                if (root == ids.end() || !isModel[root->second] || isModel[i])
                {
                    entities.Item(i).Required("sourceRef").Error("Invalid instance root");
                }
            }
        }
        std::vector<std::size_t> visited(count, count);
        for (std::size_t i = 0; i < count; ++i)
        {
            if (visited[i] != count)
            {
                continue;
            }
            auto node = i;
            while (node != count && visited[node] == count)
            {
                visited[node] = i;
                node = parents[node];
            }
            if (node != count && visited[node] == i)
            {
                entities.Item(node).Required("parent").Error("Hierarchy contains a cycle");
            }
        }
    }

    // 有界解析, 类型解码和引用检查全部成功后才返回拥有型候选, 不在此访问活动 Scene/View.
    ParsedSceneDocument DecodeSceneDocument(std::span<const std::byte> bytes, std::stop_token stop)
    {
        if (bytes.empty() || bytes.size() > DocumentByteLimit)
        {
            throw Core::BoundedText("Empty scene document or document byte limit exceeded");
        }
        std::vector<std::unordered_set<std::string>> keys;
        std::size_t nodes = 0;
        // 在建树过程中拒绝重复键和超预算输入, 不能等 DOM 覆盖同名字段后再判重.
        auto event = [&](int depth, Json::parse_event_t type, Json &value)
        {
            if (stop.stop_requested())
            {
                throw Core::BoundedText("Document parsing was cancelled");
            }
            const bool node = type == Json::parse_event_t::object_start || type == Json::parse_event_t::array_start ||
                type == Json::parse_event_t::value;
            if (depth > DocumentDepthLimit || (node && ++nodes > DocumentNodeLimit))
            {
                throw Core::BoundedText("Scene document structure budget exceeded");
            }
            if (type == Json::parse_event_t::object_start)
            {
                keys.emplace_back();
            }
            else if (type == Json::parse_event_t::object_end)
            {
                keys.pop_back();
            }
            else if (type == Json::parse_event_t::key && !keys.back().insert(value.get<std::string>()).second)
            {
                Core::BoundedText error("Repeated JSON object key: ");
                error.Append(value.get_ref<const std::string &>());
                throw error;
            }
            return true;
        };
        const auto *first = reinterpret_cast<const char *>(bytes.data());
        const auto json = Json::parse(first, first + bytes.size(), event);
        // JSON 结构通过后按 v1 类型解码, 再在资产请求之前检查跨实体引用与环.
        const Field root(json, "");
        root.Object({"version", "entities", "mainView", "environment"});
        const auto version = root.Required("version");
        if (version.Index() != 1)
        {
            version.Error("Unsupported scene document version");
        }
        ParsedSceneDocument doc;
        if (const auto field = root.Optional("environment"))
        {
            field->Object({"path", "rotation", "intensity", "lighting", "sky"});
            const auto rotation = field->Required("rotation").Vector<double, 4>();
            if (!Scene::IsValidPose(Scene::Pose{{}, rotation}))
            {
                field->Required("rotation").Error("Expected a unit xyzw rotation");
            }
            const auto intensity = field->Required("intensity").Number<double>();
            if (intensity < 0)
            {
                field->Required("intensity").Error("Expected nonnegative environment intensity");
            }
            const bool lighting = field->Required("lighting").Flag();
            const bool sky = field->Required("sky").Flag();
            if (!lighting && !sky)
            {
                field->Error("Enable at least one environment role");
            }
            doc.environment = EnvironmentFileReference{field->Required("path").Resource(L"Environments"),
                                                       {rotation, intensity, lighting, sky}};
        }
        const auto view = root.Required("mainView");
        doc.view = ReadView(view);
        const auto list = root.Required("entities");
        const auto count = list.Size();
        doc.entities.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            if (stop.stop_requested())
            {
                throw Core::BoundedText("Document decoding was cancelled");
            }
            doc.entities.push_back(ReadEntity(list.Item(i), doc));
        }
        ValidateLinks(doc, list, view);
        return doc;
    }
}
