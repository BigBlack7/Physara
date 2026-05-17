#include "TextureLoader.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <string>
#include <vector>

#include <stb/stb_image.h>
#include <tinyexr/tinyexr_c.h>

#include <Engine/Core/Log.hpp>
#include <Platform/FileSystem/FileSystem.hpp>

namespace Physara::Engine
{
    namespace TextureLoaderDetail
    {
        template <typename T>
        constexpr T MaxValue(T lhs, T rhs)
        {
            return lhs < rhs ? rhs : lhs;
        }

        TextureSourceFormat DetectFormat(const std::filesystem::path &path)
        {
            const std::string extension = Platform::FileSystem::GetExtensionLower(path.string());
            if (extension == ".png")
            {
                return TextureSourceFormat::PNG;
            }
            if (extension == ".jpg" || extension == ".jpeg")
            {
                return TextureSourceFormat::JPG;
            }
            if (extension == ".exr")
            {
                return TextureSourceFormat::EXR;
            }
            return TextureSourceFormat::Unknown;
        }

        const char *ExrErrorString(ExrResult result)
        {
            const char *message = exr_result_to_string(result);
            return message != nullptr ? message : "Unknown EXR error";
        }

        int FindChannelIndex(const std::vector<std::string> &channels, char name)
        {
            for (std::size_t i = 0; i < channels.size(); ++i)
            {
                if (!channels[i].empty() && static_cast<char>(std::toupper(static_cast<unsigned char>(channels[i][0]))) == name)
                {
                    return static_cast<int>(i);
                }
            }
            return -1;
        }

        std::shared_ptr<Texture> LoadEXR32F(const std::filesystem::path &path, const std::vector<std::uint8_t> &fileData)
        {
            ExrContextCreateInfo contextInfo{};
            contextInfo.api_version = TINYEXR_C_API_VERSION;
            contextInfo.flags = EXR_CONTEXT_SINGLE_THREADED;

            ExrContext context = nullptr;
            ExrResult result = exr_context_create(&contextInfo, &context);
            if (EXR_FAILED(result))
            {
                PHYSARA_CORE_WARN("Failed to create EXR context for '{}': {}", path.string(), ExrErrorString(result));
                return {};
            }

            ExrDataSource source{};
            result = exr_data_source_from_memory(fileData.data(), fileData.size(), &source);
            if (EXR_FAILED(result))
            {
                PHYSARA_CORE_WARN("Failed to create EXR data source for '{}': {}", path.string(), ExrErrorString(result));
                exr_context_destroy(context);
                return {};
            }

            ExrDecoderCreateInfo decoderInfo{};
            decoderInfo.source = source;

            ExrDecoder decoder = nullptr;
            result = exr_decoder_create(context, &decoderInfo, &decoder);
            if (EXR_FAILED(result))
            {
                PHYSARA_CORE_WARN("Failed to create EXR decoder for '{}': {}", path.string(), ExrErrorString(result));
                exr_context_destroy(context);
                return {};
            }

            ExrImage image = nullptr;
            result = exr_decoder_parse_header(decoder, &image);
            if (EXR_FAILED(result))
            {
                PHYSARA_CORE_WARN("Failed to parse EXR header '{}': {}", path.string(), ExrErrorString(result));
                exr_decoder_destroy(decoder);
                exr_context_destroy(context);
                return {};
            }

            ExrPart part = nullptr;
            result = exr_image_get_part(image, 0, &part);
            if (EXR_FAILED(result))
            {
                PHYSARA_CORE_WARN("Failed to read EXR part '{}': {}", path.string(), ExrErrorString(result));
                exr_image_destroy(image);
                exr_decoder_destroy(decoder);
                exr_context_destroy(context);
                return {};
            }

            ExrPartInfo partInfo{};
            result = exr_part_get_info(part, &partInfo);
            if (EXR_FAILED(result) || partInfo.width <= 0 || partInfo.height <= 0 || partInfo.num_channels == 0)
            {
                PHYSARA_CORE_WARN("Invalid EXR image '{}'.", path.string());
                exr_image_destroy(image);
                exr_decoder_destroy(decoder);
                exr_context_destroy(context);
                return {};
            }

            std::vector<std::string> channelNames;
            channelNames.reserve(partInfo.num_channels);
            for (std::uint32_t i = 0; i < partInfo.num_channels; ++i)
            {
                ExrChannelInfo channel{};
                if (EXR_FAILED(exr_part_get_channel(part, i, &channel)) || channel.name == nullptr)
                {
                    channelNames.emplace_back();
                }
                else
                {
                    channelNames.emplace_back(channel.name);
                }
            }

            std::vector<float> raw(static_cast<std::size_t>(partInfo.width) * static_cast<std::size_t>(partInfo.height) * partInfo.num_channels);

            ExrCommandBufferCreateInfo commandInfo{};
            commandInfo.decoder = decoder;
            commandInfo.max_commands = 1u;
            commandInfo.flags = EXR_CMD_ONE_TIME_SUBMIT;

            ExrCommandBuffer command = nullptr;
            result = exr_command_buffer_create(context, &commandInfo, &command);
            if (!EXR_FAILED(result))
            {
                result = exr_command_buffer_begin(command);
            }
            if (!EXR_FAILED(result))
            {
                ExrFullImageRequest request{};
                request.part = part;
                request.output = ExrBuffer{raw.data(), raw.size() * sizeof(float), 0u};
                request.channels_mask = 0u;
                request.output_pixel_type = EXR_PIXEL_FLOAT;
                request.output_layout = EXR_LAYOUT_INTERLEAVED;
                request.target_level = 0;
                result = exr_cmd_request_full_image(command, &request);
            }
            if (!EXR_FAILED(result))
            {
                result = exr_command_buffer_end(command);
            }
            if (!EXR_FAILED(result))
            {
                const ExrCommandBuffer commands[]{command};
                ExrSubmitInfo submit{};
                submit.command_buffer_count = 1u;
                submit.command_buffers = commands;
                result = exr_submit(decoder, &submit);
            }

            if (command != nullptr)
            {
                exr_command_buffer_destroy(command);
            }

            if (EXR_FAILED(result))
            {
                PHYSARA_CORE_WARN("Failed to load EXR pixels '{}': {}", path.string(), ExrErrorString(result));
                exr_image_destroy(image);
                exr_decoder_destroy(decoder);
                exr_context_destroy(context);
                return {};
            }

            const int r = FindChannelIndex(channelNames, 'R');
            const int g = FindChannelIndex(channelNames, 'G');
            const int b = FindChannelIndex(channelNames, 'B');
            const int a = FindChannelIndex(channelNames, 'A');

            auto texture = std::make_shared<Texture>();
            texture->path = path.lexically_normal().generic_string();
            texture->width = static_cast<std::uint32_t>(partInfo.width);
            texture->height = static_cast<std::uint32_t>(partInfo.height);
            texture->channels = 4u;
            texture->sourceFormat = TextureSourceFormat::EXR;
            texture->rgba32fPixels.resize(static_cast<std::size_t>(partInfo.width) * static_cast<std::size_t>(partInfo.height) * 4u);

            for (int y = 0; y < partInfo.height; ++y)
            {
                for (int x = 0; x < partInfo.width; ++x)
                {
                    const std::size_t srcBase = (static_cast<std::size_t>(y) * static_cast<std::size_t>(partInfo.width) + static_cast<std::size_t>(x)) * partInfo.num_channels;
                    const std::size_t dstBase = (static_cast<std::size_t>(y) * static_cast<std::size_t>(partInfo.width) + static_cast<std::size_t>(x)) * 4u;
                    texture->rgba32fPixels[dstBase + 0u] = r >= 0 ? raw[srcBase + static_cast<std::size_t>(r)] : 0.f;
                    texture->rgba32fPixels[dstBase + 1u] = g >= 0 ? raw[srcBase + static_cast<std::size_t>(g)] : texture->rgba32fPixels[dstBase + 0u];
                    texture->rgba32fPixels[dstBase + 2u] = b >= 0 ? raw[srcBase + static_cast<std::size_t>(b)] : texture->rgba32fPixels[dstBase + 0u];
                    texture->rgba32fPixels[dstBase + 3u] = a >= 0 ? raw[srcBase + static_cast<std::size_t>(a)] : 1.f;
                }
            }

            exr_image_destroy(image);
            exr_decoder_destroy(decoder);
            exr_context_destroy(context);
            return texture;
        }
    }

    std::shared_ptr<Texture> TextureLoader::LoadRGBA8(const std::filesystem::path &path)
    {
        std::vector<std::uint8_t> fileData;
        try
        {
            fileData = Platform::FileSystem::ReadBinaryFile(path.string());
        }
        catch (const std::exception &error)
        {
            PHYSARA_CORE_WARN("Failed to read texture '{}': {}", path.string(), error.what());
            return {};
        }

        int width = 0;
        int height = 0;
        int channels = 0;
        unsigned char *pixels = stbi_load_from_memory(fileData.data(),
                                                      static_cast<int>(fileData.size()),
                                                      &width,
                                                      &height,
                                                      &channels,
                                                      4);
        if (pixels == nullptr)
        {
            return {};
        }

        auto texture = std::make_shared<Texture>();
        texture->path = path.lexically_normal().generic_string();
        texture->width = static_cast<std::uint32_t>(TextureLoaderDetail::MaxValue(width, 0));
        texture->height = static_cast<std::uint32_t>(TextureLoaderDetail::MaxValue(height, 0));
        texture->channels = 4;
        texture->sourceFormat = TextureLoaderDetail::DetectFormat(path);
        texture->rgba8Pixels.assign(pixels, pixels + static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u);
        for (std::size_t i = 3u; i < texture->rgba8Pixels.size(); i += 4u)
        {
            const std::uint8_t alpha = texture->rgba8Pixels[i];
            if (alpha < 250u)
            {
                texture->hasTransparentPixels = true;
            }
            if (alpha > 5u && alpha < 250u)
            {
                texture->hasPartialAlphaPixels = true;
            }
        }

        stbi_image_free(pixels);
        return texture;
    }

    std::shared_ptr<Texture> TextureLoader::LoadRGBA32F(const std::filesystem::path &path)
    {
        std::vector<std::uint8_t> fileData;
        try
        {
            fileData = Platform::FileSystem::ReadBinaryFile(path.string());
        }
        catch (const std::exception &error)
        {
            PHYSARA_CORE_WARN("Failed to read HDR texture '{}': {}", path.string(), error.what());
            return {};
        }

        const TextureSourceFormat sourceFormat = TextureLoaderDetail::DetectFormat(path);
        if (sourceFormat == TextureSourceFormat::EXR)
        {
            return TextureLoaderDetail::LoadEXR32F(path, fileData);
        }

        int width = 0;
        int height = 0;
        int channels = 0;
        float *pixels = stbi_loadf_from_memory(fileData.data(),
                                               static_cast<int>(fileData.size()),
                                               &width,
                                               &height,
                                               &channels,
                                               4);
        if (pixels == nullptr)
        {
            PHYSARA_CORE_WARN("Failed to load HDR texture '{}'.", path.string());
            return {};
        }

        auto texture = std::make_shared<Texture>();
        texture->path = path.lexically_normal().generic_string();
        texture->width = static_cast<std::uint32_t>(TextureLoaderDetail::MaxValue(width, 0));
        texture->height = static_cast<std::uint32_t>(TextureLoaderDetail::MaxValue(height, 0));
        texture->channels = 4u;
        texture->sourceFormat = sourceFormat;
        texture->rgba32fPixels.assign(pixels, pixels + static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u);

        stbi_image_free(pixels);
        return texture;
    }
}