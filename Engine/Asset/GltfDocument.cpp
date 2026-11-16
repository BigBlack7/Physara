#include "GltfDocument.hpp"

/// @file
/// @brief 保留 JSON 字段存在性与 C11 元数据, 不在容器阶段读取外部资源.

#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <utility>

namespace Physara::Asset::Gltf
{
    void Reject(AssetError reason, std::string_view location, std::string_view message)
    {
        Core::BoundedText text(location.empty() ? "/" : location);
        text.Append(": ");
        text.Append(message);
        throw ImportFailure{{reason, text}};
    }

    void CheckStop(std::stop_token stop)
    {
        if (stop.stop_requested())
        {
            throw ImportCancelled{};
        }
    }

    Json::Json(const tg3json_value *value, std::string location)
        : m_Value(value), m_Location(std::move(location))
    {
    }

    void Json::Error(std::string_view message, AssetError reason) const
    {
        Reject(reason, m_Location, message);
    }

    void Json::Object() const
    {
        if (m_Value && m_Value->type != TG3JSON_OBJECT)
        {
            Error("Expected an object");
        }
    }

    // 使用原 JSON 树保留显式字段, 使 C 数值的缺省哨兵不会掩盖错误.
    Json Json::Field(std::string_view name) const
    {
        Object();
        auto location = m_Location;
        location.append("/").append(name);
        return {m_Value ? tg3json_object_get_n(m_Value, name.data(), name.size()) : nullptr, std::move(location)};
    }

    Json Json::At(std::size_t index) const
    {
        Json result(m_Value ? tg3json_array_get(m_Value, index) : nullptr,
                    m_Location + "/" + std::to_string(index));
        if (!result)
        {
            result.Error("Missing or invalid array index");
        }
        return result;
    }

    std::size_t Json::Size() const
    {
        if (!m_Value)
        {
            return 0;
        }
        if (m_Value->type != TG3JSON_ARRAY)
        {
            Error("Expected an array");
        }
        return m_Value->u.array.count;
    }

    std::string_view Json::Text() const
    {
        if (!m_Value || m_Value->type != TG3JSON_STRING)
        {
            Error("Expected a string");
        }
        return {m_Value->u.string.ptr, m_Value->u.string.len};
    }

    double Json::Number() const
    {
        if (m_Value && m_Value->type == TG3JSON_INT)
        {
            return static_cast<double>(m_Value->u.integer);
        }
        if (!m_Value || m_Value->type != TG3JSON_REAL || !std::isfinite(m_Value->u.real))
        {
            Error("Expected a finite number");
        }
        return m_Value->u.real;
    }

    float Json::Float() const
    {
        const auto value = Number();
        if (std::abs(value) > std::numeric_limits<float>::max())
        {
            Error("Number exceeds float32 range");
        }
        return static_cast<float>(value);
    }

    bool Json::Boolean() const
    {
        if (!m_Value || m_Value->type != TG3JSON_BOOL)
        {
            Error("Expected a boolean");
        }
        return m_Value->u.boolean != 0;
    }

    std::uint64_t Json::Unsigned() const
    {
        if (m_Value && m_Value->type == TG3JSON_INT && m_Value->u.integer >= 0)
        {
            return static_cast<std::uint64_t>(m_Value->u.integer);
        }
        const auto value = Number();
        if (value < 0 || value >= 0x1p64 || value != std::trunc(value))
        {
            Error("Expected a nonnegative integer");
        }
        return static_cast<std::uint64_t>(value);
    }

    std::uint32_t Json::Index() const
    {
        const auto value = Unsigned();
        if (value > std::numeric_limits<std::uint32_t>::max())
        {
            Error("Index exceeds uint32 range");
        }
        return static_cast<std::uint32_t>(value);
    }

    Document::~Document()
    {
        tg3json_value_free(&m_Json);
        tg3_error_stack_free(&m_Errors);
        tg3_model_free(&m_Model);
    }

    /// @brief 解析 glTF JSON asset.version 的 major.minor 格式, 不使用容器版本代替它.
    static std::array<unsigned, 2> Version(const Json &field)
    {
        const auto value = field.Text();
        const auto separator = value.find('.');
        std::array<unsigned, 2> parts{};
        if (separator != std::string_view::npos)
        {
            const auto first = std::from_chars(value.data(), value.data() + separator, parts[0]);
            const auto second = std::from_chars(value.data() + separator + 1, value.data() + value.size(), parts[1]);
            if (first.ec == std::errc{} && first.ptr == value.data() + separator &&
                second.ec == std::errc{} && second.ptr == value.data() + value.size())
            {
                return parts;
            }
        }
        field.Error("Expected a glTF major.minor version");
    }

    void Document::Parse(std::span<const std::byte> source, std::size_t budget, std::stop_token stop)
    {
        CheckStop(stop);
        tg3_parse_options options;
        tg3_parse_options_init(&options);
        options.defer_uri_buffers = 1;
        options.metadata_json = &m_Json;
        options.borrow_input_buffers = 1;
        options.images_as_is = 1;
        options.skip_extras_values = 1;
        options.memory.memory_budget = budget;
        options.memory.max_single_alloc = budget;
        // 小额度也可容纳小文档, 不必取得默认大小的 arena 块.
        options.memory.arena_block_size = static_cast<std::uint32_t>((std::clamp)(
            budget / 4, std::size_t{1}, static_cast<std::size_t>(options.memory.arena_block_size)));
        const auto result = tg3_parse_auto(&m_Model, &m_Errors,
            reinterpret_cast<const std::uint8_t *>(source.data()), source.size(), nullptr, 0, &options);
        CheckStop(stop);
        if (result != TG3_OK)
        {
            const auto reason = result == TG3_ERR_GLB_INVALID_VERSION ? AssetError::UnsupportedModel :
                (result == TG3_ERR_OUT_OF_MEMORY ? AssetError::ModelByteLimit : AssetError::InvalidModel);
            for (std::uint32_t i = 0; i < m_Errors.count; ++i)
            {
                const auto &error = m_Errors.entries[i];
                if (error.severity == TG3_SEVERITY_ERROR && error.message &&
                    (result != TG3_ERR_OUT_OF_MEMORY || error.code == TG3_ERR_OUT_OF_MEMORY))
                {
                    Reject(reason, error.json_path ? error.json_path : "/", error.message);
                }
            }
            Reject(reason, "/", "Cannot parse model metadata");
        }
        // 容器版本只说明 GLB 外壳, glTF asset 的版本与最低要求仍从同次保留的 JSON 读取.
        const auto asset = Root().Field("asset");
        const auto version = Version(asset.Field("version"));
        const auto minimumField = asset.Field("minVersion");
        constexpr std::array<unsigned, 2> Supported{2, 0};
        if (version[0] != 2)
        {
            asset.Field("version").Error("Expected glTF major version 2", AssetError::UnsupportedModel);
        }
        if (minimumField)
        {
            const auto minimum = Version(minimumField);
            if (minimum > version)
            {
                minimumField.Error("minVersion exceeds asset.version");
            }
            if (minimum > Supported)
            {
                minimumField.Error("glTF minVersion exceeds the supported profile", AssetError::UnsupportedModel);
            }
        }
    }
}
