#include "BuildSupport.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <set>

#include <glslang/Public/ShaderLang.h>
#include <Windows.h>
#include <bcrypt.h>

namespace Physara::Shader
{
    Failure MakeFailure(ErrorCode code, std::string_view message) noexcept
    {
        Failure failure;
        failure.code = code;
        failure.context.description.Assign(message);
        return failure;
    }

    std::string PathText(const std::filesystem::path &path)
    {
        const auto text = path.generic_u8string();
        return {reinterpret_cast<const char *>(text.data()), text.size()};
    }

    std::vector<std::byte> ReadBytes(const std::filesystem::path &path, std::size_t limit)
    {
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input)
        {
            throw BuildError(ErrorCode::IoFailure, "Cannot open input: " + PathText(path));
        }
        const auto size = input.tellg();
        if (size < 0 || static_cast<std::uint64_t>(size) > limit)
        {
            throw BuildError(ErrorCode::InvalidInput, "Input exceeds byte budget: " + PathText(path));
        }
        std::vector<std::byte> bytes(static_cast<std::size_t>(size));
        input.seekg(0);
        if (!bytes.empty() && !input.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(size)))
        {
            throw BuildError(ErrorCode::IoFailure, "Cannot read complete input: " + PathText(path));
        }
        return bytes;
    }

    std::string ReadText(const std::filesystem::path &path, std::size_t limit)
    {
        const auto bytes = ReadBytes(path, limit);
        if (bytes.empty())
        {
            return {};
        }
        return {reinterpret_cast<const char *>(bytes.data()), bytes.size()};
    }

    Json ParseJson(std::string_view text, std::string_view origin)
    {
        // 固定 JSON 库把原始 NUL 当成 EOF. 在共享字节边界拒绝它, 使解析覆盖完整输入.
        // JSON 文本中的转义序列不含原始 NUL, 仍由解析器正常解码.
        if (text.find('\0') != std::string_view::npos)
        {
            throw BuildError(ErrorCode::InvalidInput, std::string(origin) + ": raw NUL is not valid JSON input");
        }
        // nlohmann 默认保留最后一个重复 key; 在对象开始/结束之间分别维护 key 集合以拒绝歧义.
        std::vector<std::set<std::string>> keys;
        try
        {
            return Json::parse(text,
                               [&](int depth, Json::parse_event_t event, Json &value)
                               {
                                   if (depth > 64)
                                   {
                                       throw BuildError(ErrorCode::InvalidInput,
                                                        std::string(origin) + ": JSON nesting exceeds budget");
                                   }
                                   if (event == Json::parse_event_t::object_start)
                                   {
                                       keys.emplace_back();
                                   }
                                   else if (event == Json::parse_event_t::key)
                                   {
                                       if (!keys.back().insert(value.get<std::string>()).second)
                                       {
                                           throw BuildError(ErrorCode::InvalidInput, std::string(origin) +
                                                                                         ": duplicate JSON key " +
                                                                                         value.get<std::string>());
                                       }
                                   }
                                   else if (event == Json::parse_event_t::object_end)
                                   {
                                       keys.pop_back();
                                   }
                                   return true;
                               });
        }
        catch (const Json::exception &error)
        {
            throw BuildError(ErrorCode::InvalidInput, std::string(origin) + ": " + error.what());
        }
    }

    void CheckFields(const Json &object, std::initializer_list<std::string_view> allowed, std::string_view origin)
    {
        if (!object.is_object())
        {
            throw BuildError(ErrorCode::InvalidInput, std::string(origin) + ": expected object");
        }
        for (auto item = object.begin(); item != object.end(); ++item)
        {
            if (std::find(allowed.begin(), allowed.end(), item.key()) == allowed.end())
            {
                throw BuildError(ErrorCode::InvalidInput, std::string(origin) + ": unknown field " + item.key());
            }
        }
    }

    std::uint32_t UInt(const Json &value, std::uint32_t maximum, std::string_view origin)
    {
        if (!value.is_number_integer() ||
            (value.is_number_integer() && !value.is_number_unsigned() && value.get<std::int64_t>() < 0) ||
            value.get<std::uint64_t>() > maximum)
        {
            throw BuildError(ErrorCode::InvalidInput, std::string(origin) + ": unsigned integer out of range");
        }
        return static_cast<std::uint32_t>(value.get<std::uint64_t>());
    }

    std::string Identifier(const Json &value, std::string_view origin)
    {
        if (!value.is_string())
        {
            throw BuildError(ErrorCode::InvalidInput, std::string(origin) + ": expected identifier");
        }
        auto name = value.get<std::string>();
        // C++/既有接口保留名在此检查; GLSL 的完整词汇直接查询固定编译器, 不另抄易遗漏的名单.
        static constexpr std::string_view Reserved[]{
            "alignas",      "alignof",    "and",       "and_eq",     "asm",           "auto",
            "bitand",       "bitor",      "bool",      "break",      "buffer",        "case",
            "catch",        "char",       "char8_t",   "char16_t",   "char32_t",      "class",
            "co_await",     "co_return",  "co_yield",  "compl",      "concept",       "const",
            "consteval",    "constexpr",  "constinit", "const_cast", "continue",      "decltype",
            "default",      "delete",     "discard",   "do",         "double",        "dynamic_cast",
            "else",         "enum",       "explicit",  "export",     "extern",        "false",
            "flat",         "float",      "for",       "friend",     "goto",          "if",
            "in",           "inline",     "inout",     "int",        "layout",        "long",
            "mutable",      "namespace",  "new",       "noexcept",   "not",           "not_eq",
            "nullptr",      "operator",   "or",        "or_eq",      "out",           "precision",
            "private",      "protected",  "public",    "readonly",   "register",      "reinterpret_cast",
            "requires",     "restrict",   "return",    "sampler",    "sampler2D",     "short",
            "signed",       "sizeof",     "smooth",    "static",     "static_assert", "static_cast",
            "struct",       "subroutine", "switch",    "template",   "texture2D",     "this",
            "thread_local", "throw",      "true",      "try",        "typedef",       "typeid",
            "typename",     "uint",       "uniform",   "union",      "unsigned",      "using",
            "varying",      "virtual",    "void",      "volatile",   "wchar_t",       "while",
            "writeonly",    "xor",        "xor_eq",    "vec2",       "vec3",          "vec4",
            "ivec2",        "ivec3",      "ivec4",     "uvec2",      "uvec3",         "uvec4",
            "mat2",         "mat3",       "mat4"};
        const auto first = [](char character)
        {
            return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z');
        };
        if (name.empty() || name.size() > 96 || !first(name.front()) || name.starts_with("gl_") ||
            name.starts_with("GL_") || std::ranges::find(Reserved, name) != std::end(Reserved) ||
            glslang::IsReservedIdentifier(name.c_str()) || name.find("__") != std::string::npos ||
            !std::all_of(name.begin(), name.end(),
                         [&](char character)
                         {
                             return first(character) || character == '_' || (character >= '0' && character <= '9');
                         }))
        {
            throw BuildError(ErrorCode::InvalidInput, std::string(origin) + ": invalid or reserved identifier " + name);
        }
        return name;
    }

    // CNG 句柄的作用域与本次摘要计算一致, 任何失败/分配异常均先关闭句柄再向公共边界传播.
    struct HashHandles final
    {
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        // 显式 count 构造可抛出 MSVC Debug proxy 分配失败, 避免默认构造的 noexcept.
        std::vector<unsigned char> storage = std::vector<unsigned char>(0);
        HashHandles() = default;
        HashHandles(const HashHandles &) = delete;
        HashHandles &operator=(const HashHandles &) = delete;
        HashHandles(HashHandles &&) = delete;
        HashHandles &operator=(HashHandles &&) = delete;
        ~HashHandles()
        {
            if (hash)
            {
                BCryptDestroyHash(hash);
            }
            if (algorithm)
            {
                BCryptCloseAlgorithmProvider(algorithm, 0);
            }
        }
    };

    std::string Sha256(std::span<const std::byte> bytes)
    {
        if (bytes.size() > std::numeric_limits<ULONG>::max())
        {
            throw BuildError(ErrorCode::InvalidInput, "SHA-256 input exceeds CNG length range.");
        }
        HashHandles handles;
        ULONG objectBytes = 0;
        ULONG written = 0;
        if (BCryptOpenAlgorithmProvider(&handles.algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 ||
            BCryptGetProperty(handles.algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectBytes),
                              sizeof(objectBytes), &written, 0) < 0)
        {
            throw BuildError(ErrorCode::SystemFailure, "Cannot initialize CNG SHA-256.");
        }
        handles.storage.resize(objectBytes);
        std::array<unsigned char, 32> digest{};
        if (BCryptCreateHash(handles.algorithm, &handles.hash, handles.storage.data(), objectBytes, nullptr, 0, 0) <
                0 ||
            BCryptHashData(handles.hash, reinterpret_cast<PUCHAR>(const_cast<std::byte *>(bytes.data())),
                           static_cast<ULONG>(bytes.size()), 0) < 0 ||
            BCryptFinishHash(handles.hash, digest.data(), static_cast<ULONG>(digest.size()), 0) < 0)
        {
            throw BuildError(ErrorCode::SystemFailure, "CNG SHA-256 operation failed.");
        }
        // HashHandles 的析构体先销毁 hash, 再由成员析构释放其外部 storage.
        static constexpr char Hex[] = "0123456789abcdef";
        std::string result(64, '0');
        for (std::size_t index = 0; index < digest.size(); ++index)
        {
            result[index * 2] = Hex[digest[index] >> 4];
            result[index * 2 + 1] = Hex[digest[index] & 15];
        }
        return result;
    }

    std::string HashText(std::string_view text)
    {
        return Sha256(std::as_bytes(std::span(text.data(), text.size())));
    }

    bool WriteAtomically(const std::filesystem::path &path, std::span<const std::byte> bytes,
                         std::optional<std::filesystem::file_time_type> outputTime)
    {
        if (bytes.size() > MaxPackageBytes)
        {
            throw BuildError(ErrorCode::InvalidInput, "Output exceeds byte budget.");
        }
        auto temporary = path;
        temporary += L".tmp";
        std::error_code cleanupError;
        std::filesystem::remove(temporary, cleanupError);
        if (std::filesystem::exists(path))
        {
            const auto previous = ReadBytes(path, MaxPackageBytes);
            if (std::ranges::equal(previous, bytes))
            {
                if (outputTime)
                {
                    std::filesystem::last_write_time(path, *outputTime);
                }
                return false;
            }
        }
        if (!path.parent_path().empty())
        {
            std::filesystem::create_directories(path.parent_path());
        }
        try
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output || (!bytes.empty() && !output.write(reinterpret_cast<const char *>(bytes.data()),
                                                            static_cast<std::streamsize>(bytes.size()))))
            {
                throw BuildError(ErrorCode::IoFailure, "Cannot write output: " + PathText(path));
            }
            output.close();
            if (!output)
            {
                throw BuildError(ErrorCode::IoFailure, "Cannot close output: " + PathText(path));
            }
            if (outputTime)
            {
                // 先关闭写流再设时间, 原子替换前便满足辅助输出的完成时间合同.
                std::filesystem::last_write_time(temporary, *outputTime);
            }
            if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            {
                const auto nativeError = GetLastError();
                throw BuildError(ErrorCode::IoFailure,
                                 "Cannot publish output, Win32 " + std::to_string(nativeError) + ": " + PathText(path));
            }
        }
        catch (...)
        {
            std::filesystem::remove(temporary, cleanupError);
            throw;
        }
        return true;
    }

    bool WriteTextAtomically(const std::filesystem::path &path, std::string_view text,
                             std::optional<std::filesystem::file_time_type> outputTime)
    {
        return WriteAtomically(path, std::as_bytes(std::span(text.data(), text.size())), outputTime);
    }

    std::filesystem::path CanonicalInputPath(const std::filesystem::path &path)
    {
        const auto absolute = std::filesystem::absolute(path).lexically_normal();
        auto current = absolute.root_path();
        for (const auto &component : absolute.relative_path())
        {
            current /= component;
            const auto attributes = GetFileAttributesW(current.c_str());
            if (attributes == INVALID_FILE_ATTRIBUTES)
            {
                throw BuildError(ErrorCode::IoFailure, "Cannot inspect input path: " + PathText(current));
            }
            if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
            {
                throw BuildError(ErrorCode::InvalidInput,
                                 "Redirectable input paths are unsupported: " + PathText(current));
            }
        }
        return std::filesystem::canonical(absolute);
    }

    bool IsWithin(const std::filesystem::path &root, const std::filesystem::path &file)
    {
        auto first = root.begin();
        auto second = file.begin();
        for (; first != root.end(); ++first, ++second)
        {
            if (second == file.end())
            {
                return false;
            }
            const auto &left = first->native();
            const auto &right = second->native();
            if (CompareStringOrdinal(left.data(), static_cast<int>(left.size()), right.data(),
                                     static_cast<int>(right.size()), TRUE) != CSTR_EQUAL)
            {
                return false;
            }
        }
        return true;
    }
}
