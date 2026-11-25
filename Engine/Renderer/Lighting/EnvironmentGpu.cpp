#include "../Materials/StaticGpu.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <ForwardSky/Data.hpp>

/// @file
/// @brief 将本轮实际使用的环境角色接到 Forward 驻留、上传及生成接口. 只由 owning context 的主线程访问.

namespace Physara::Renderer::Detail
{
    namespace SkyAbi = Shader::Generated::ForwardSky;

    /// @brief 将原始线性倍率及曝光分别拆分, 保留合法强度, 避免 CPU 乘积先溢出或下溢.
    struct RadianceScale
    {
        float mantissa;
        std::int32_t exponent;
    };

    /// @brief 把文件单位白, 场景强度和当帧预曝光编码成一个底数为 2 的倍率, 供两个 Pass 共用.
    static RadianceScale Scale(const Scene::SceneEnvironment &source, const CameraSnapshot &camera) noexcept
    {
        int calibrationExponent = 0;
        int intensityExponent = 0;
        int exposureExponent = 0;
        const double calibration = std::frexp(double(source.source->image.whiteLuminance.value_or(1.0f)),
                                              &calibrationExponent);
        const double intensity = std::frexp(source.settings.intensity, &intensityExponent);
        const double exposure = std::frexp(double(camera.exposure.preExposure), &exposureExponent);
        return {static_cast<float>(calibration * intensity * exposure),
                calibrationExponent + intensityExponent + exposureExponent};
    }

    /// @brief 生成 ABI 的列存布局, 每列只填可见的三个分量.
    static void EncodeMatrix(auto &target, const glm::dmat3 &value) noexcept
    {
        for (glm::length_t column = 0; column < 3; ++column)
        {
            target[std::size_t(column)].value = {float(value[column].x), float(value[column].y), float(value[column].z)};
        }
    }

    /// @brief 从场景的 xyzw 旋转派生所有查询共用的 world 到环境逆旋转.
    static glm::dmat3 InverseEnvironmentRotation(const Scene::SceneEnvironment &source) noexcept
    {
        const auto &q = source.settings.rotation;
        return glm::mat3_cast(glm::conjugate(glm::normalize(glm::dquat(q[3], q[0], q[1], q[2]))));
    }

    /// @brief 为本 View 的环境查询或天空准备常量. 其中的曝光随相机变化, 上传计入 View 写入.
    /// 槽保有缓冲, 绑定集合在图准备阶段按实际读取建立.
    static std::shared_ptr<ConstantRow> WriteConstants(ForwardDevice &device, UploadBatch &uploads,
                                                       std::span<const std::byte> payload)
    {
        device.Reserve(payload.size());
        auto row = std::make_shared<ConstantRow>(device.capacity, payload.size());
        row->buffer = AllocateBuffer(device.device, GraphUsage::ConstantBuffer, payload.size());
        device.Use(*row);
        uploads.Add(row, row->buffer, payload, GraphUploadSource::ForwardView, device.preparingView);
        return row;
    }

    /// @brief 取得驻留额度并创建尚未初始化的存储. 像素写入由消费该候选的图声明.
    static std::shared_ptr<ResidentTexture> CreateEnvironmentTexture(ForwardDevice &device, const RHI::TextureDesc &desc,
                                                                     std::size_t bytes)
    {
        device.Reserve(bytes);
        auto entry = std::make_shared<ResidentTexture>(device.capacity, bytes, true);
        entry->texture = device.device.CreateTexture(desc);
        if (!entry->texture)
        {
            Reject(ViewSettingReason::EnvironmentRejected, "Cannot allocate sampled HDR environment storage");
        }
        entry->sampling = {0, desc.mipLevels, 0, desc.arraySize};
        device.Use(*entry);
        return entry;
    }

    /// @brief DFG 与场景源独立. 只在第一份真实需要 DFG 的正常图中初始化其完整单层.
    /// @param data 命中驻留代时可为空. 冷建时由上传批持有像素到图录制结束.
    static std::shared_ptr<ResidentTexture> PrepareDfg(ForwardDevice &device, UploadBatch &uploads,
                                                       const std::shared_ptr<const DfgTable> &data)
    {
        if (!device.dfgTexture || !device.Current(*device.dfgTexture))
        {
            const auto bytes = std::as_bytes(std::span(data->pixels)).size();
            uploads.ReserveWrites(bytes, 1);
            auto entry = CreateEnvironmentTexture(device, {.width = DfgEdge, .height = DfgEdge, .format = RHI::Format::RGBA16_FLOAT, .debugName = "Shared core DFG", .initialState = TextureBoundary, .keepInitialState = true}, bytes);
            uploads.environmentTextures.push_back({entry, {}, data});
            device.dfgTexture = std::move(entry);
        }
        device.Use(*device.dfgTexture);
        return device.dfgTexture;
    }

    /// @brief 识别同烘焙键及角色下可读的代. 提前登记本轮使用, 避免后续资源准备淘汰它.
    static const ResidentEnvironment *FindCube(ForwardDevice &device, EnvironmentKey key, bool reflection)
    {
        const auto found = device.environmentTextures.find({key, reflection});
        if (found != device.environmentTextures.end() && device.Current(*found->second.texture))
        {
            device.Use(*found->second.texture);
            return &found->second;
        }
        return nullptr;
    }

    /// @brief 按完整 cube 范围准备新代. 反射产品保存 SH9, 后续查询无需访问烘焙像素.
    static const ResidentEnvironment &PrepareCube(ForwardDevice &device, UploadBatch &uploads, EnvironmentKey key,
        const std::shared_ptr<const BakedEnvironment> &data, bool reflection)
    {
        auto pixels = data->base->pixels.size();
        const auto mips = reflection ? EnvironmentMipCount : 1u;
        if (reflection)
        {
            for (const auto &mip : data->prefiltered)
            {
                pixels += mip->pixels.size();
            }
        }
        const auto bytes = pixels * sizeof(EnvironmentTexel);
        uploads.ReserveWrites(bytes, 6 * mips);
        auto entry = CreateEnvironmentTexture(device, {.width = EnvironmentEdge, .height = EnvironmentEdge, .arraySize = 6, .mipLevels = mips, .format = RHI::Format::RGBA32_FLOAT, .dimension = RHI::TextureDimension::TextureCube, .debugName = reflection ? "Environment GGX reflection" : "Environment sky", .initialState = TextureBoundary, .keepInitialState = true}, bytes);
        uploads.environmentTextures.push_back({entry, data, {}});
        return device.environmentTextures.insert_or_assign({key, reflection}, ResidentEnvironment{entry, data->sh})
            .first->second;
    }

    /// @brief 取实际过滤器, 纹理视图各自限定 mip. 同一个线性 clamp sampler 供 DFG 和 cube.
    static const RHI::SamplerHandle &EnvironmentSampler(ForwardDevice &device)
    {
        if (!device.environmentSampler)
        {
            device.environmentSampler = device.device.CreateSampler(
                RHI::SamplerDesc().SetAllFilters(true).SetAllAddressModes(RHI::SamplerAddressMode::Clamp));
            if (!device.environmentSampler)
            {
                Reject(ViewSettingReason::EnvironmentRejected, "Cannot create the environment sampler");
            }
        }
        return device.environmentSampler;
    }

    /// @brief 打包世界到环境的旋转, 曝光倍率和 SH9. 每一色通道的九项系数共用该通道的指数.
    static StaticAbi::ForwardEnvironment LightingConstants(const std::array<EnvironmentTexel, 9> &sh,
                                                           const glm::dmat3 &environmentFromWorld, RadianceScale scale) noexcept
    {
        StaticAbi::ForwardEnvironment constants{};
        EncodeMatrix(constants.environmentFromWorld, environmentFromWorld);
        constants.radianceMantissa = scale.mantissa;
        constants.radianceExponent = scale.exponent;
        constants.maxMip = float(EnvironmentMipCount - 1);
        std::array<float, 3> maxCoefficients{};
        for (const auto &term : sh)
        {
            for (std::size_t channel = 0; channel != maxCoefficients.size(); ++channel)
            {
                maxCoefficients[channel] = std::max(maxCoefficients[channel], std::abs(term[channel]));
            }
        }
        // 各通道独立归一化, 保有暗色通道. Shader 将逐通道还原指数与场景倍率和预曝光合并.
        for (std::size_t channel = 0; channel != maxCoefficients.size(); ++channel)
        {
            int exponent = 0;
            static_cast<void>(std::frexp(maxCoefficients[channel], &exponent));
            constants.shExponent[channel] = exponent;
            for (std::size_t term = 0; term != sh.size(); ++term)
            {
                constants.sh[term].value[channel] = float(std::ldexp(double(sh[term][channel]), -exponent));
            }
        }
        return constants;
    }

    /// @brief 将屏幕 clip 映射到环境中的射线. 正交使用固定前向, 透视从近端逆投影的 xyz 取方向.
    static SkyAbi::ForwardSkyView SkyConstants(const CameraSnapshot &camera, const glm::dmat3 &environmentFromWorld,
                                               RadianceScale scale)
    {
        SkyAbi::ForwardSkyView constants{};
        constants.radianceMantissa = scale.mantissa;
        constants.radianceExponent = scale.exponent;
        const auto inverseProjection = glm::make_mat4(camera.viewFromRenderClip.data());
        const auto fromView = environmentFromWorld * glm::dmat3(glm::make_mat4(camera.worldFromView.data()));
        glm::dmat3 rays(0);
        rays[2] = fromView * glm::dvec3(0, 0, -1);
        if (!camera.orthographic)
        {
            rays[0] = fromView * glm::dvec3(inverseProjection[0]);
            rays[1] = fromView * glm::dvec3(inverseProjection[1]);
            rays[2] = fromView * glm::dvec3(inverseProjection[2] + inverseProjection[3]);
        }
        // 方向查询按比例不变. 全矩阵按一个系数缩放, 保持各射线的同一透视投影关系.
        const double peak = std::max({glm::length(rays[0]), glm::length(rays[1]), glm::length(rays[2])});
        if (!(peak > 0) || !std::isfinite(peak))
        {
            Reject(ViewSettingReason::EnvironmentRejected, "Cannot encode the current sky projection");
        }
        EncodeMatrix(constants.environmentFromClip, rays / peak);
        return constants;
    }

    void PrepareEnvironment(ForwardDevice &device, ForwardFrameData &frame, const PreparedView &view,
                            PreviewMode mode, bool litDraws)
    {
        const auto &source = view.scene.environment;
        const bool physical = mode == PreviewMode::PhysicalLighting;
        const bool dfg = physical && litDraws;
        const bool ibl = dfg && source && source->settings.lighting;
        const bool sky = physical && source && source->settings.sky;
        if (!dfg && !sky)
        {
            return;
        }
        const auto key = (ibl || sky) ? EnvironmentKey{source->source->sourceFingerprint} : EnvironmentKey{};
        const auto *reflection = ibl ? FindCube(device, key, true) : nullptr;
        const auto *background = sky ? FindCube(device, key, false) : nullptr;
        // 两种角色可分别命中. 复用反射纹理时从同代保存的 SH9 生成查询常量, 仅为缺少的角色索取 CPU 产品.
        const bool needsEnvironment = (ibl && !reflection) || (sky && !background);
        const bool needsDfg = dfg && (!device.dfgTexture || !device.Current(*device.dfgTexture));
        auto prepared = device.environmentCache.Prepare(needsEnvironment ? source->source : nullptr,
                                                         ibl && !reflection, needsDfg);
        if (const auto *failure = std::get_if<EnvironmentPreparationFailure>(&prepared))
        {
            throw ForwardFailure{failure->pending ? ViewSettingReason::EnvironmentPending : ViewSettingReason::EnvironmentRejected, failure->reason};
        }
        auto &products = std::get<EnvironmentProducts>(prepared);
        auto &environment = frame.environment;
        if (dfg)
        {
            environment.program = ibl ? LightingProgram::Ibl : LightingProgram::Physical;
            environment.dfg = PrepareDfg(device, frame.uploads, products.dfg);
        }
        if (dfg || sky)
        {
            environment.sampler = EnvironmentSampler(device);
        }
        if (ibl || sky)
        {
            const auto inverse = InverseEnvironmentRotation(*source);
            const auto scale = Scale(*source, *view.camera);
            if (ibl)
            {
                if (!reflection)
                {
                    reflection = &PrepareCube(device, frame.uploads, key, products.environment, true);
                }
                environment.reflection = reflection->texture;
                const auto values = LightingConstants(reflection->sh, inverse, scale);
                environment.lightingConstants = WriteConstants(device, frame.uploads, std::as_bytes(std::span(&values, 1)));
            }
            if (sky)
            {
                // 天空资源已准备后才纳入本路计划. 选定本帧代之后再取得对应的 Pipeline 和布局.
                if (!background)
                {
                    background = &PrepareCube(device, frame.uploads, key, products.environment, false);
                }
                environment.sky = background->texture;
                const auto values = SkyConstants(*view.camera, inverse, scale);
                environment.skyConstants = WriteConstants(device, frame.uploads, std::as_bytes(std::span(&values, 1)));
            }
            // 保有本轮查询的源和参数, 角色位只记录已选入当前图的消费者. View 在提交后发布这份候选.
            environment.applied = *source;
            environment.applied->settings.lighting = ibl;
            environment.applied->settings.sky = sky;
        }
    }
}
