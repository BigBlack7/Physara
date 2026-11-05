#include "ShaderSchema.hpp"

namespace Physara::Shader
{
    void to_json(Json &json, const FieldLayout &value)
    {
        json = {{"name", value.name},
                {"type", value.type},
                {"offset", value.offsetBytes},
                {"extent", value.extentBytes},
                {"arrayCount", value.arrayCount},
                {"arrayStride", value.arrayStrideBytes},
                {"matrixStride", value.matrixStrideBytes}};
    }

    void from_json(const Json &json, FieldLayout &value)
    {
        CheckFields(json, {"name", "type", "offset", "extent", "arrayCount", "arrayStride", "matrixStride"}, "field");
        value.name = Identifier(json.at("name"), "field/name");
        value.type = json.at("type").get<std::string>();
        (void)ParseDataType(value.type);
        value.offsetBytes = UInt(json.at("offset"), MaxBlockBytes, "field/offset");
        value.extentBytes = UInt(json.at("extent"), MaxBlockBytes, "field/extent");
        value.arrayCount = UInt(json.at("arrayCount"), MaxBlockBytes, "field/arrayCount");
        value.arrayStrideBytes = UInt(json.at("arrayStride"), MaxBlockBytes, "field/arrayStride");
        value.matrixStrideBytes = UInt(json.at("matrixStride"), 16, "field/matrixStride");
        if (value.arrayCount == 0 || value.extentBytes == 0 ||
            std::uint64_t(value.offsetBytes) + value.extentBytes > MaxBlockBytes)
        {
            throw BuildError(ErrorCode::CorruptPackage, "Invalid reflected field range.");
        }
    }

    void to_json(Json &json, const BufferLayout &value)
    {
        json = {{"name", value.name},      {"layout", value.layout},      {"alignment", value.alignmentBytes},
                {"size", value.sizeBytes}, {"extent", value.extentBytes}, {"fields", value.fields}};
    }

    void from_json(const Json &json, BufferLayout &value)
    {
        CheckFields(json, {"name", "layout", "alignment", "size", "extent", "fields"}, "buffer");
        value.name = Identifier(json.at("name"), "buffer/name");
        value.layout = json.at("layout").get<std::string>();
        value.alignmentBytes = UInt(json.at("alignment"), 16, "buffer/alignment");
        value.sizeBytes = UInt(json.at("size"), MaxBlockBytes, "buffer/size");
        value.extentBytes = UInt(json.at("extent"), MaxBlockBytes, "buffer/extent");
        value.fields = json.at("fields").get<std::vector<FieldLayout>>();
        if ((value.layout != "std140" && value.layout != "std430") || value.fields.empty() ||
            (value.alignmentBytes != 4 && value.alignmentBytes != 8 && value.alignmentBytes != 16) ||
            value.extentBytes > value.sizeBytes || value.sizeBytes % value.alignmentBytes != 0)
        {
            throw BuildError(ErrorCode::CorruptPackage, "Invalid reflected buffer layout.");
        }
        std::uint32_t end = 0;
        for (const auto &field : value.fields)
        {
            if (field.offsetBytes < end || field.offsetBytes + field.extentBytes > value.extentBytes)
            {
                throw BuildError(ErrorCode::CorruptPackage, "Overlapping/out-of-range reflected fields.");
            }
            end = field.offsetBytes + field.extentBytes;
        }
        Json fields = Json::array();
        for (const auto &field : value.fields)
        {
            Json item{{"name", field.name}, {"type", field.type}};
            if (field.arrayStrideBytes != 0)
            {
                item["array"] = field.arrayCount;
            }
            fields.push_back(std::move(item));
        }
        const auto canonical = ParseBlockLayout({{"name", value.name}, {"layout", value.layout}, {"fields", fields}});
        if (Json(canonical) != json)
        {
            throw BuildError(ErrorCode::CorruptPackage, "Reflected layout is not a canonical std140/std430 layout.");
        }
    }

    void to_json(Json &json, const ResourceReflection &value)
    {
        json = {{"name", value.name},
                {"kind", ResourceKindName(value.kind)},
                {"set", value.set},
                {"slot", value.slot},
                {"binding", value.binding}};
        if (value.kind == ResourceKind::ConstantBuffer || value.kind == ResourceKind::StructuredBuffer)
        {
            json["buffer"] = value.buffer;
        }
        if (value.kind == ResourceKind::StructuredBuffer)
        {
            json["capacity"] = value.capacity;
            json["recordStride"] = value.recordStrideBytes;
        }
    }

    void from_json(const Json &json, ResourceReflection &value)
    {
        CheckFields(json, {"name", "kind", "set", "slot", "binding", "buffer", "capacity", "recordStride"}, "resource");
        value.name = Identifier(json.at("name"), "resource/name");
        value.kind = ParseResourceKind(json.at("kind").get<std::string>());
        value.set = UInt(json.at("set"), 3, "resource/set");
        value.slot = UInt(json.at("slot"), 127, "resource/slot");
        value.binding = UInt(json.at("binding"), 511, "resource/binding");
        if (value.binding != CanonicalBinding(value.kind, value.slot))
        {
            throw BuildError(ErrorCode::CorruptPackage, "Reflected logical/canonical binding mismatch.");
        }
        if (value.kind == ResourceKind::ConstantBuffer || value.kind == ResourceKind::StructuredBuffer)
        {
            value.buffer = json.at("buffer").get<BufferLayout>();
            if (value.buffer.layout != (value.kind == ResourceKind::ConstantBuffer ? "std140" : "std430"))
            {
                throw BuildError(ErrorCode::CorruptPackage, "Resource class/layout mismatch.");
            }
        }
        else if (json.contains("buffer"))
        {
            throw BuildError(ErrorCode::CorruptPackage, "Non-buffer resource contains a buffer layout.");
        }
        if (value.kind == ResourceKind::StructuredBuffer)
        {
            value.capacity = UInt(json.at("capacity"), MaxBlockBytes, "resource/capacity");
            value.recordStrideBytes = UInt(json.at("recordStride"), MaxBlockBytes, "resource/recordStride");
            if (value.capacity == 0 || value.recordStrideBytes != value.buffer.sizeBytes ||
                std::uint64_t(value.capacity) * value.recordStrideBytes > MaxBlockBytes)
            {
                throw BuildError(ErrorCode::CorruptPackage, "Invalid structured buffer record range.");
            }
        }
        else if (json.contains("capacity") || json.contains("recordStride"))
        {
            throw BuildError(ErrorCode::CorruptPackage, "Non-structured resource contains record data.");
        }
    }

    void to_json(Json &json, const IoReflection &value)
    {
        json = {{"name", value.name},
                {"type", value.type},
                {"interpolation", value.interpolation},
                {"location", value.location}};
    }

    void from_json(const Json &json, IoReflection &value)
    {
        CheckFields(json, {"name", "type", "interpolation", "location"}, "IO");
        value.name = Identifier(json.at("name"), "IO/name");
        value.type = json.at("type").get<std::string>();
        value.interpolation = json.at("interpolation").get<std::string>();
        value.location = UInt(json.at("location"), 31, "IO/location");
        if (ParseDataType(value.type).columns != 1 ||
            (value.interpolation != "smooth" && value.interpolation != "flat" &&
             value.interpolation != "noperspective"))
        {
            throw BuildError(ErrorCode::CorruptPackage, "Invalid reflected IO.");
        }
    }
}
