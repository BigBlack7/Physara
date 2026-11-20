#include "StaticGpu.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <utility>

/// @file
/// @brief 把固定 CPU 图像和 glTF sampler 映射到显式驻留层级, CPU 字节由本轮上传包持有.

namespace Physara::Renderer::Detail
{
    static constexpr std::size_t Channels = 4;

    /// @brief RGB 的 mip 在 Rec.709 线性域过滤. alpha 和数据纹理直接按字节比例解释.
    static const std::array<double, 256> SrgbDecode = []
    {
        std::array<double, 256> values{};
        for (std::size_t i = 0; i < values.size(); ++i)
        {
            const auto signal = static_cast<double>(i) / 255.0;
            values[i] = signal <= 0.04045 ? signal / 12.92 : std::pow((signal + 0.055) / 1.055, 2.4);
        }
        return values;
    }();

    /// @brief 单轴 box 覆盖. 用整数刻度保留非二次幂图像的边沿与分数权重.
    static std::uint64_t Overlap(std::uint64_t from, std::uint64_t to, std::uint64_t pixel, std::uint64_t scale)
    {
        return std::min(to, (pixel + 1) * scale) - std::max(from, pixel * scale);
    }

    /// @brief 一个目标像素最多覆盖 3x3 源像素. 分别累计线性颜色与不作 gamma 的数据.
    static std::vector<std::byte> Downsample(std::span<const std::byte> source, std::uint32_t width,
                                             std::uint32_t height, bool srgb)
    {
        const auto w = std::max(1u, width / 2);
        const auto h = std::max(1u, height / 2);
        std::vector<std::byte> result(std::size_t(w) * h * Channels);
        const auto area = static_cast<double>(width) * height;
        for (std::uint32_t y = 0; y < h; ++y)
        {
            const std::uint64_t top = std::uint64_t(y) * height, bottom = top + height;
            for (std::uint32_t x = 0; x < w; ++x)
            {
                const std::uint64_t left = std::uint64_t(x) * width, right = left + width;
                std::array<double, Channels> sums{};
                for (auto sy = top / h; sy * h < bottom; ++sy)
                {
                    for (auto sx = left / w; sx * w < right; ++sx)
                    {
                        const auto weight = static_cast<double>(Overlap(top, bottom, sy, h) *
                                                               Overlap(left, right, sx, w));
                        const auto start = (std::size_t(sy) * width + std::size_t(sx)) * Channels;
                        for (std::size_t c = 0; c < Channels; ++c)
                        {
                            const auto value = std::to_integer<unsigned char>(source[start + c]);
                            sums[c] += weight * (srgb && c < 3 ? SrgbDecode[value] : value / 255.0);
                        }
                    }
                }
                for (std::size_t c = 0; c < Channels; ++c)
                {
                    auto value = sums[c] / area;
                    if (srgb && c < 3)
                    {
                        value = value <= 0.0031308 ? 12.92 * value : 1.055 * std::pow(value, 1.0 / 2.4) - 0.055;
                    }
                    result[(std::size_t(y) * w + x) * Channels + c] =
                        static_cast<std::byte>(std::lround(std::clamp(value, 0.0, 1.0) * 255.0));
                }
            }
        }
        return result;
    }

    bool UploadBatch::HasMaterialTexture(const ResidentTexture &texture) const noexcept
    {
        return texture.accepted || std::ranges::any_of(textures, [&](const TextureUpload &upload)
        {
            return upload.revision.get() == &texture;
        });
    }

    /// @brief 检查所有 mip 的逻辑存储和准入, 再登记完整的拥有型层级.
    /// @param entry 可重试的同版存储. 非空时, 此代未被先前录制访问, 可沿用原绑定.
    static std::shared_ptr<ResidentTexture> PrepareImage(ForwardDevice &device, UploadBatch &batch,
        std::uint32_t width, std::uint32_t height, std::span<const std::byte> pixels, bool srgb, bool mips,
        std::shared_ptr<ResidentTexture> entry)
    {
        if (width > device.maxTextureSize || height > device.maxTextureSize)
        {
            Reject(ViewSettingReason::TextureSizeLimit, "Material image exceeds the device texture size limit");
        }
        const auto levels = mips ? static_cast<std::uint32_t>(std::bit_width(std::max(width, height))) : 1u;
        // 四通道 RGBA8 按各级实际宽高累计字节. 上传批据此核对本图剩余额度, 通过后才创建存储和生成 mip.
        std::size_t bytes = 0;
        constexpr auto maxBytes = std::numeric_limits<std::size_t>::max();
        for (auto w = width, h = height, level = 0u; level < levels; ++level)
        {
            if (h > (maxBytes - bytes) / Channels / w)
            {
                Reject(ViewSettingReason::SceneUploadLimit, "Material mip chain byte count is unrepresentable");
            }
            bytes += std::size_t(w) * h * Channels;
            w = std::max(1u, w / 2);
            h = std::max(1u, h / 2);
        }
        batch.ReserveWrites(bytes, levels);
        if (!entry)
        {
            const auto format = srgb ? RHI::Format::SRGBA8_UNORM : RHI::Format::RGBA8_UNORM;
            if ((device.device.QueryFormatSupport(format) & RHI::FormatSupport::ShaderSample) == 0)
            {
                Reject(ViewSettingReason::ScenePipelineUnavailable, "Material image format cannot be sampled");
            }
            device.Reserve(bytes);
            entry = std::make_shared<ResidentTexture>(device.capacity, bytes);
            const RHI::TextureDesc desc{.width = width, .height = height, .mipLevels = levels, .format = format,
                .debugName = "Forward material image", .initialState = TextureBoundary, .keepInitialState = true};
            entry->texture = device.device.CreateTexture(desc);
            if (!entry->texture)
            {
                Reject(ViewSettingReason::ScenePreparationFailed, "Cannot allocate material image storage");
            }
            // 材质集合和图读取共用这个视图范围. 每个可采样层在下方都有一份拥有型载荷.
            entry->sampling = {0, levels, 0, 1};
        }
        device.Use(*entry);
        TextureUpload upload{entry, {}};
        upload.levels.reserve(levels);
        upload.levels.emplace_back(pixels.begin(), pixels.end());
        for (std::uint32_t i = 1; i < levels; ++i)
        {
            upload.levels.push_back(Downsample(upload.levels.back(), width, height, srgb));
            width = std::max(1u, width / 2);
            height = std::max(1u, height / 2);
        }
        // 最后才把整条 mip 链登记为本批生产者. 后续材质或 View 命中时可直接借用这些层级.
        batch.textures.push_back(std::move(upload));
        return entry;
    }

    std::shared_ptr<ResidentTexture> PrepareTexture(ForwardDevice &device, UploadBatch &uploads,
        const TextureKey &key, const Asset::Rgba8Image &image)
    {
        const auto found = device.textures.find(key);
        std::shared_ptr<ResidentTexture> reusable;
        if (found != device.textures.end())
        {
            if (uploads.HasMaterialTexture(*found->second))
            {
                device.Use(*found->second);
                return found->second;
            }
            if (!found->second->uploadRecorded)
            {
                reusable = found->second;
            }
        }
        auto texture = PrepareImage(device, uploads, image.width, image.height, std::as_bytes(image.Pixels()),
                                    std::get<3>(key) == Asset::ImageDomain::Srgb, std::get<4>(key),
                                    std::move(reusable));
        device.textures.insert_or_assign(key, texture);
        return texture;
    }

    std::shared_ptr<ResidentTexture> WhiteTexture(ForwardDevice &device, UploadBatch &uploads)
    {
        if (!device.white || !uploads.HasMaterialTexture(*device.white))
        {
            constexpr std::array white{std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}};
            auto reusable = device.white && !device.white->uploadRecorded ? device.white : nullptr;
            device.white = PrepareImage(device, uploads, 1, 1, white, false, false, std::move(reusable));
            return device.white;
        }
        device.Use(*device.white);
        return device.white;
    }

    SamplerKey EffectiveSampler(const Asset::ModelSampler &sampler) noexcept
    {
        return {sampler.minFilter.value_or(9729), sampler.magFilter.value_or(9729), sampler.wrapS, sampler.wrapT};
    }

    /// @brief 枚举已由导入和场景覆盖校验, 此处只映射到独立公共 RHI 采样状态.
    RHI::SamplerHandle PrepareSampler(ForwardDevice &device, const SamplerKey &key)
    {
        if (const auto found = device.samplers.find(key); found != device.samplers.end())
        {
            return found->second;
        }
        const auto address = [](std::uint32_t mode)
        {
            return mode == 10497 ? RHI::SamplerAddressMode::Wrap :
                   mode == 33648 ? RHI::SamplerAddressMode::Mirror : RHI::SamplerAddressMode::Clamp;
        };
        const auto min = key[0];
        const auto desc = RHI::SamplerDesc().SetAddressU(address(key[2])).SetAddressV(address(key[3]))
            .SetMinFilter(min == 9729 || min == 9985 || min == 9987).SetMagFilter(key[1] == 9729)
            .SetMipFilter(min == 9986 || min == 9987);
        auto sampler = device.device.CreateSampler(desc);
        if (!sampler)
        {
            Reject(ViewSettingReason::ScenePreparationFailed, "Cannot prepare material sampler");
        }
        device.samplers.emplace(key, sampler);
        return sampler;
    }
}
