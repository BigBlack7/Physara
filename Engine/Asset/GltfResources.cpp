#include "GltfResources.hpp"

/// @file
/// @brief 根据 URI/容器类型取得受控快照, 并校验消费范围与模型输入额度.

#include <algorithm>
#include <array>
#include <cassert>
#include <limits>
#include <utility>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace Physara::Asset::Gltf
{
    /// @brief 解析 URI 百分号转义中的单个十六进制位, 非法字符返回 -1.
    static int Hex(unsigned char c)
    {
        if (c >= '0' && c <= '9')
        {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f')
        {
            return c - 'a' + 10;
        }
        return c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
    }

    /// @brief 相对 URI 段转 Windows 路径. 只解码一次, 后续由 Core 审核资源根和实际打开对象.
    static std::filesystem::path RelativeUri(const Json &field)
    {
        const auto text = field.Text();
        if (text.empty() || text.size() > 32768 || text.front() == '/' ||
            text.find_first_of(":#?\\") != std::string_view::npos)
        {
            field.Error("Expected a local relative resource URI", AssetError::InvalidPath);
        }
        std::string decoded;
        decoded.reserve(text.size());
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            auto current = static_cast<unsigned char>(text[i]);
            if (current == '%')
            {
                if (text.size() - i < 3 || Hex(text[i + 1]) < 0 || Hex(text[i + 2]) < 0)
                {
                    field.Error("Invalid percent escape in model URI", AssetError::InvalidPath);
                }
                current = static_cast<unsigned char>(16 * Hex(text[i + 1]) + Hex(text[i + 2]));
                i += 2;
                if (current == '/' || current == '\\' || current == ':')
                {
                    field.Error("Escaped path separator in model URI", AssetError::InvalidPath);
                }
            }
            else if (current == ' ')
            {
                field.Error("Unescaped URI space", AssetError::InvalidPath);
            }
            if (current < 0x20 || current == 0x7f)
            {
                field.Error("Control character in model URI", AssetError::InvalidPath);
            }
            decoded.push_back(static_cast<char>(current));
        }
        const auto size = static_cast<int>(decoded.size());
        const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, decoded.data(), size, nullptr, 0);
        if (count <= 0)
        {
            field.Error("Invalid UTF-8 resource URI", AssetError::InvalidPath);
        }
        std::wstring wide(static_cast<std::size_t>(count), L'\0');
        const int written = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, decoded.data(), size, wide.data(), count);
        if (written != count)
        {
            field.Error("Cannot convert resource URI", AssetError::InvalidPath);
        }
        return std::filesystem::path(std::move(wide));
    }

    /// @brief glTF 支持的图像 MIME 类别. 随后核对真实解码类型.
    static ImageEncoding Encoding(const Json &field, std::string_view mime)
    {
        if (mime == "image/png")
        {
            return ImageEncoding::Png;
        }
        if (mime == "image/jpeg")
        {
            return ImageEncoding::Jpeg;
        }
        field.Error("Unsupported model image MIME", AssetError::UnsupportedEncoding);
    }

    /// @brief 将标准 base64 字符映射到六位值, 64 表示非法字符或需单独处理的填充.
    static unsigned Digit(char c)
    {
        if (c >= 'A' && c <= 'Z')
        {
            return static_cast<unsigned>(c - 'A');
        }
        if (c >= 'a' && c <= 'z')
        {
            return 26u + static_cast<unsigned>(c - 'a');
        }
        if (c >= '0' && c <= '9')
        {
            return 52u + static_cast<unsigned>(c - '0');
        }
        return c == '+' ? 62u : (c == '/' ? 63u : 64u);
    }

    /// @brief 输入已按四元组计量, 填充只能出现在末尾且保留位必须为零.
    static void DecodeBase64(const Json &field, std::string_view text, std::span<std::byte> output,
                             std::stop_token stop)
    {
        std::size_t written = 0;
        for (std::size_t i = 0; i < text.size(); i += 4)
        {
            if ((i & 65535u) == 0)
            {
                CheckStop(stop);
            }
            const unsigned a = Digit(text[i]);
            const unsigned b = Digit(text[i + 1]);
            if (a >= 64 || b >= 64)
            {
                field.Error("Invalid base64 digit");
            }
            const auto remaining = output.size() - written;
            output[written++] = static_cast<std::byte>(((a << 2) | (b >> 4)) & 0xff);
            if (remaining == 1)
            {
                if (text[i + 2] != '=' || text[i + 3] != '=' || (b & 15u) != 0)
                {
                    field.Error("Invalid base64 padding");
                }
                continue;
            }
            const unsigned c = Digit(text[i + 2]);
            if (c >= 64)
            {
                field.Error("Invalid base64 digit");
            }
            output[written++] = static_cast<std::byte>(((b << 4) | (c >> 2)) & 0xff);
            if (remaining == 2)
            {
                if (text[i + 3] != '=' || (c & 3u) != 0)
                {
                    field.Error("Invalid base64 padding");
                }
                continue;
            }
            const unsigned d = Digit(text[i + 3]);
            if (d >= 64)
            {
                field.Error("Invalid base64 digit");
            }
            output[written++] = static_cast<std::byte>(((c << 6) | d) & 0xff);
        }
    }

    /// @brief 按元数据表预留每源对象的载荷槽, 主文件快照字节从一开始计入累计输入.
    Resources::Resources(const Document &doc, const std::filesystem::path &root, const std::filesystem::path &source,
                         const ImportLimits &limits, std::size_t sourceBytes, std::stop_token stop)
        : m_Document(doc), m_Root(root), m_Directory(source.parent_path()), m_Limits(limits), m_Stop(stop),
          m_InputBytes(sourceBytes), m_Buffers(doc.Metadata().buffers_count),
          m_Images(doc.Metadata().images_count), m_ViewUses(doc.Metadata().buffer_views_count)
    {
    }

    /// @brief 记入新增依赖或内嵌展开字节, 超限在下一次载荷分配前终止作业.
    void Resources::Charge(std::size_t bytes, const Json &field)
    {
        if (bytes > m_Limits.modelInputBytes - m_InputBytes)
        {
            field.Error("Model exceeds cumulative input bytes", AssetError::ModelByteLimit);
        }
        m_InputBytes += bytes;
    }

    /// @brief 从模型目录解析单个文件引用, 复用已取得的同路径快照, 新读入由 Core 校验实际句柄.
    std::span<const std::byte> Resources::ReadExternal(const Json &uri)
    {
        constexpr std::array<std::wstring_view, 1> Allowed{L"Models"};
        auto parsed = Core::ResourcePath::Parse(m_Directory / RelativeUri(uri), Allowed);
        if (const auto *error = std::get_if<Core::ResourceFailure>(&parsed))
        {
            uri.Error(error->description.View(), AssetError::InvalidPath);
        }
        const auto &path = std::get<Core::ResourcePath>(parsed);
        if (auto found = m_External.find(path.Relative()); found != m_External.end())
        {
            return found->second.bytes;
        }
        const auto room = m_Limits.modelInputBytes - m_InputBytes;
        auto read = Core::ReadResourceFile(m_Root, path, std::min(m_Limits.sourceBytes, room), m_Stop);
        CheckStop(m_Stop);
        if (const auto *error = std::get_if<Core::ResourceFailure>(&read))
        {
            switch (error->reason)
            {
            case Core::ResourceError::InvalidPath:
                uri.Error(error->description.View(), AssetError::InvalidPath);
            case Core::ResourceError::ByteLimit:
                uri.Error(error->description.View(),
                          room <= m_Limits.sourceBytes ? AssetError::ModelByteLimit : AssetError::SourceByteLimit);
            case Core::ResourceError::IncompleteRead:
                uri.Error(error->description.View(), AssetError::IncompleteRead);
            case Core::ResourceError::HashFailed:
                uri.Error(error->description.View(), AssetError::HashFailed);
            case Core::ResourceError::Cancelled:
                throw ImportCancelled{};
            case Core::ResourceError::CannotOpen:
                uri.Error(error->description.View(), AssetError::CannotOpen);
            case Core::ResourceError::IncompleteWrite:
                uri.Error(error->description.View(), AssetError::UnexpectedFailure);
            }
        }
        auto snapshot = std::get<Core::ResourceBytes>(std::move(read));
        Charge(snapshot.bytes.size(), uri);
        return m_External.emplace(path.Relative(), std::move(snapshot)).first->second.bytes;
    }

    /// @brief 外部路径借用缓存快照, data URI 则在记账后拥有解码字节; MIME 随载荷保留.
    void Resources::ReadUri(const Json &uri, bool image, Backing &backing)
    {
        const auto text = uri.Text();
        if (!text.starts_with("data:"))
        {
            backing.bytes = ReadExternal(uri);
            return;
        }
        const auto comma = text.find(',');
        if (comma == std::string_view::npos || comma < 5 || !text.substr(5, comma - 5).ends_with(";base64"))
        {
            uri.Error("Expected a base64 data URI");
        }
        const auto mime = text.substr(5, comma - 5 - 7);
        if (image)
        {
            backing.encoding = Encoding(uri, mime);
        }
        else if (mime != "application/octet-stream" && mime != "application/gltf-buffer")
        {
            uri.Error("Unsupported buffer data URI MIME", AssetError::UnsupportedEncoding);
        }
        const auto payload = text.substr(comma + 1);
        if (payload.empty() || (payload.size() % 4) != 0)
        {
            uri.Error("Invalid base64 data URI length");
        }
        const std::size_t padding = payload.ends_with("==") ? 2 : (payload.ends_with('=') ? 1 : 0);
        const auto bytes = payload.size() / 4 * 3 - padding;
        Charge(bytes, uri);
        backing.owned.resize(bytes);
        DecodeBase64(uri, payload, backing.owned, m_Stop);
        backing.bytes = backing.owned;
    }

    /// @brief 延迟装载实际使用的 buffer, 对外仅暴露声明长度, 不把文件尾部当作可寻址数据.
    std::span<const std::byte> Resources::Buffer(std::uint32_t index)
    {
        const auto field = m_Document.Root().Field("buffers").At(index);
        auto &backing = m_Buffers[index];
        if (backing.bytes.empty())
        {
            const auto &descriptor = m_Document.Metadata().buffers[index];
            const auto uri = field.Field("uri");
            if (uri)
            {
                ReadUri(uri, false, backing);
            }
            else if (descriptor.data.data && descriptor.data.count != 0)
            {
                // GLB BIN 的借用在 C11 元数据阶段已限定到声明大小.
                backing.bytes = std::as_bytes(std::span(descriptor.data.data,
                                                       static_cast<std::size_t>(descriptor.data.count)));
            }
            if (descriptor.byte_length == 0 || descriptor.byte_length > backing.bytes.size())
            {
                field.Field("byteLength").Error("Buffer has no payload or is smaller than its declared length");
            }
            backing.bytes = backing.bytes.first(static_cast<std::size_t>(descriptor.byte_length));
        }
        return backing.bytes;
    }

    ByteView Resources::View(std::uint32_t index, ViewRole role, std::optional<std::uint32_t> vertexAccessor)
    {
        assert((role == ViewRole::Vertex) == vertexAccessor.has_value());
        const auto field = m_Document.Root().Field("bufferViews").At(index);
        auto &use = m_ViewUses[index];
        if (use.role && *use.role != role)
        {
            field.Error("bufferView mixes incompatible uses");
        }
        const auto strideField = field.Field("byteStride");
        const auto stride = strideField ? strideField.Index() : 0;
        const auto target = field.Field("target");
        if (strideField && (role != ViewRole::Vertex || stride < 4 || stride > 252 || (stride % 4) != 0))
        {
            strideField.Error("Invalid stride for this bufferView use");
        }
        // 无步长时以源 accessor 下标判定共享, 同一 accessor 可被多个 Primitive 引用.
        if (!strideField && use.packedVertexAccessor && use.packedVertexAccessor != vertexAccessor)
        {
            strideField.Error("A bufferView shared by distinct vertex accessors requires byteStride");
        }
        if (target && !((role == ViewRole::Vertex && target.Index() == 34962) ||
                        (role == ViewRole::Index && target.Index() == 34963)))
        {
            target.Error("Invalid target for this bufferView use");
        }
        const auto bytes = Buffer(field.Field("buffer").Index());
        const auto offsetField = field.Field("byteOffset");
        const auto offset = offsetField ? offsetField.Unsigned() : 0;
        const auto length = field.Field("byteLength").Unsigned();
        if (length == 0 || offset > bytes.size() || length > bytes.size() - offset)
        {
            field.Error("bufferView exceeds the actual declared buffer bytes");
        }
        use.role = role;
        if (!strideField)
        {
            use.packedVertexAccessor = vertexAccessor;
        }
        return {bytes.subspan(static_cast<std::size_t>(offset), static_cast<std::size_t>(length)), offset, stride};
    }

    ImageInput Resources::Image(std::uint32_t index)
    {
        const auto field = m_Document.Root().Field("images").At(index);
        auto &backing = m_Images[index];
        if (backing.bytes.empty())
        {
            const auto uri = field.Field("uri");
            const auto view = field.Field("bufferView");
            const auto mime = field.Field("mimeType");
            if (static_cast<bool>(uri) == static_cast<bool>(view))
            {
                field.Error("Expected exactly one core image source", AssetError::UnsupportedModel);
            }
            if (uri)
            {
                ReadUri(uri, true, backing);
            }
            else
            {
                backing.bytes = View(view.Index(), ViewRole::Image).bytes;
                backing.encoding = Encoding(mime, mime.Text());
            }
            if (mime)
            {
                const auto declared = Encoding(mime, mime.Text());
                if (backing.encoding && *backing.encoding != declared)
                {
                    mime.Error("Image MIME conflicts with its data URI");
                }
                backing.encoding = declared;
            }
            if (backing.bytes.empty())
            {
                field.Error("Empty image payload");
            }
        }
        return {backing.bytes, backing.encoding};
    }

    Core::Sha256Digest Resources::Fingerprint(const Core::Sha256Digest &source, std::uint32_t scene) const
    {
        // 固定标签、长度前缀和小端整数使拼接无歧义. map 按规范路径排序, 与依赖发现次序无关.
        constexpr std::string_view Tag = "Physara/glTF/sourceVersion/1";
        std::vector<std::byte> manifest;
        auto append = [&manifest](std::span<const std::byte> bytes)
        {
            manifest.insert(manifest.end(), bytes.begin(), bytes.end());
        };
        auto value = [&manifest](std::uint64_t n)
        {
            for (unsigned shift = 0; shift < 64; shift += 8)
            {
                manifest.push_back(static_cast<std::byte>((n >> shift) & 0xff));
            }
        };
        append(std::as_bytes(std::span(Tag)));
        append(std::as_bytes(std::span(source)));
        value(scene);
        value(m_External.size());
        for (const auto &[path, snapshot] : m_External)
        {
            CheckStop(m_Stop);
            const auto text = path.generic_u8string();
            value(text.size());
            append(std::as_bytes(std::span(text)));
            append(std::as_bytes(std::span(snapshot.fingerprint)));
        }
        const auto hash = Core::ComputeSha256(manifest);
        if (const auto *digest = std::get_if<Core::Sha256Digest>(&hash))
        {
            return *digest;
        }
        Reject(AssetError::HashFailed, "/", "Cannot compute model sourceVersion");
    }

    void Resources::AppendInputFiles(std::vector<ModelInputFile> &inputs) const
    {
        inputs.reserve(inputs.size() + m_External.size());
        for (const auto &[path, file] : m_External)
        {
            CheckStop(m_Stop);
            inputs.push_back({path, file.fingerprint, file.bytes.size()});
        }
    }
}
