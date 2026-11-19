#include "Forward.hpp"

#include <algorithm>
#include <cassert>
#include <new>
#include <utility>

#include "../Materials/StaticGpu.hpp"

/// @file
/// @brief 将同一 View 的上传, 不透明几何, 可选天空和显示转换组装为有版本依赖的子图.
/// 图内回调只访问已声明版本; 预制场景集合由 RHI 按实际活动绑定再次核对访问权限.

namespace Physara::Renderer
{
    using Detail::BufferBoundary;
    using Detail::DepthFormat;
    using Detail::TextureBoundary;
    using Detail::WorkingFormat;

    ForwardFrame::ForwardFrame() noexcept = default;
    ForwardFrame::~ForwardFrame() = default;

    ForwardWorkCounts ForwardFrame::WorkCounts() const noexcept
    {
        if (!m_Data)
        {
            return {};
        }
        const auto &lights = m_Data->activeLights;
        return {m_Data->draws.size(), lights ? std::size_t(lights->directionalCount) + lights->localCount : 0};
    }

    bool ForwardOutput::HasLiveUploads(const CompiledGraphPlan &plan) const noexcept
    {
        return std::ranges::any_of(m_Uploads, [&](PassId pass) { return IsLivePass(plan, pass); });
    }

    bool ForwardOutput::HasCompleteUploads(const CompiledGraphPlan &plan) const noexcept
    {
        return displayColor.identity.generation == plan.graphGeneration &&
               std::ranges::all_of(m_Uploads, [&](PassId pass) { return IsLivePass(plan, pass); });
    }

    void ForwardFrame::Reset() noexcept
    {
        if (m_Data)
        {
            // 此槽已可复用. 解除本轮着色引用, 保留槽内的 View 行和普通灯缓冲供下次准备判定.
            m_Data->draws.clear();
            m_Data->materialRows.clear();
            m_Data->uploads.Clear();
            m_Data->activeLights.reset();
            m_Data->lightLayout = nullptr;
            m_Data->environment = {};
            m_Data->display = {};
            m_Data->degradation.reset();
        }
    }

    const std::optional<Core::BoundedText> &ForwardFrame::Degradation() const noexcept
    {
        assert(m_Data);
        return m_Data->degradation;
    }

    const std::optional<RenderedEnvironment> &ForwardFrame::Environment() const noexcept
    {
        assert(m_Data);
        return m_Data->environment.applied;
    }

    std::size_t ForwardFrame::GraphStorageBytes() const noexcept
    {
        assert(m_Data);
        // 图 arena 只保存上传回调和几个 Pass 描述. Draw 和字节另由帧槽持有.
        return 4096 + m_Data->uploads.writes * 256;
    }

    void ForwardFrame::ReleasePayload() noexcept
    {
        if (m_Data)
        {
            m_Data->uploads.Clear();
            m_Data->display.ReleasePayload();
        }
    }

    ForwardRenderer::ForwardRenderer(RHI::Device &device, ForwardPackagePaths packages, std::uint32_t maxTextureSize,
                                     std::uint32_t maxAttachmentSize) noexcept
        : m_Device(device), m_Packages(std::move(packages)), m_MaxTextureSize(maxTextureSize),
          m_MaxAttachmentSize(maxAttachmentSize)
    {
    }

    ForwardRenderer::~ForwardRenderer() = default;

    ForwardResidencyCounts ForwardRenderer::ResidencyCounts() const noexcept
    {
        if (!m_Resources)
        {
            return {};
        }
        // 每个天空或反射 cube 缓存键算一项. 共享 DFG 和白纹理由单独的句柄持有.
        return {m_Resources->geometry.size(), m_Resources->textures.size(), m_Resources->materials.size(),
                m_Resources->environmentTextures.size()};
    }

    std::optional<ForwardFailure> ForwardRenderer::Prepare(ForwardFrame &frame, const PreparedView &view,
                                                            PreviewMode mode, ViewResourceNeeds needs)
    try
    {
        assert(view.camera && view.reason == ViewSettingReason::None);
        const auto output = view.layouts.display.outputExtent;
        const auto render = view.layouts.render.outputExtent;
        const auto limit = std::min(m_MaxAttachmentSize, m_MaxTextureSize);
        if (output.width > limit || output.height > limit || render.width > limit || render.height > limit)
        {
            Detail::Reject(ViewSettingReason::SceneAttachmentLimit, "Forward extent exceeds the attachment limit");
        }
        const auto display = ResolveDisplaySettings(mode, view.requestedAa);
        const bool fxaa = display.antiAliasing == AntiAliasing::Fxaa;
        const bool scale = view.layouts.display != view.layouts.render;
        // HDR/D32 常驻本图. 各后继阶段才增加线性中间色, 最终 SRGBA8 按输出尺寸计费.
        const std::size_t renderBytesPerPixel = 12 + 8 * (std::size_t(fxaa) + std::size_t(scale));
        if (render.height > ForwardAttachmentBudget / renderBytesPerPixel / render.width)
        {
            Detail::Reject(ViewSettingReason::SceneAttachmentLimit, "Forward attachments exceed the view byte budget");
        }
        const auto renderBytes = std::size_t(render.width) * render.height * renderBytesPerPixel;
        if (output.height > (ForwardAttachmentBudget - renderBytes) / 4 / output.width)
        {
            Detail::Reject(ViewSettingReason::SceneAttachmentLimit, "Display output exceeds the view byte budget");
        }
        if (!m_Resources)
        {
            m_Resources = std::make_unique<Detail::ForwardDevice>(m_Device, m_Packages, m_MaxTextureSize);
        }
        if (!frame.m_Data)
        {
            frame.m_Data = std::make_unique<Detail::ForwardFrameData>();
        }
        auto &data = *frame.m_Data;
        assert(data.draws.empty() && data.materialRows.empty() && data.uploads.writes == 0);
        // 普通灯缓冲和 View 行都随各自帧槽跨帧保留. 更换 Renderer 时以预算 owner 识别旧缓存.
        if (data.view && data.view->capacity != m_Resources->capacity)
        {
            // 帧槽更换准备 owner 后, 原 UBO 集合的布局不再是当前 Pipeline 的布局.
            data.view.reset();
            data.viewValues.reset();
        }
        if (data.lightTable && data.lightTable->capacity != m_Resources->capacity)
        {
            data.lightTable.reset();
        }
        data.source = {view.view, view.scene.scene, view.scene.contentRevision, mode, *view.camera, view.layouts.render};
        data.needs = needs;
        m_Resources->Begin();
        Detail::PrepareDraws(*m_Resources, data, view, mode);
        if (const auto failure = m_Resources->display.Prepare(data.display, view.layouts, display))
        {
            Detail::Reject(ViewSettingReason::DisplayUnavailable, failure->View());
        }
        data.uploads.ReserveWrites(data.display.UploadBytes(), data.display.UploadCount());
        return std::nullopt;
    }
    catch (const ForwardFailure &failure)
    {
        frame.Reset();
        return failure;
    }
    catch (const std::bad_alloc &)
    {
        frame.Reset();
        return ForwardFailure{ViewSettingReason::ScenePreparationFailed, Core::BoundedText("Forward preparation allocation failed")};
    }

    /// @brief 将声明失败交还本次图的唯一入口, 保留图的具体错误.
    template <typename T>
    static T TakeGraph(GraphResult<T> result)
    {
        if (const auto *error = std::get_if<GraphError>(&result))
        {
            throw *error;
        }
        return std::get<T>(std::move(result));
    }

    /// @brief 仅在本图保存物理资源到最后生产版本及实际范围的映射.
    struct SceneGraphResources
    {
        std::map<RHI::Buffer *, BufferId> buffers;
        std::map<RHI::Texture *, TextureId> textures;

        /// @brief 待写缓冲优先取得本图的最终写版本, 其余仅从已经接受的驻留代导入已定义内容.
        [[nodiscard]] BufferId ReadBuffer(RenderGraph &graph, const RHI::BufferHandle &buffer)
        {
            const auto [item, inserted] = buffers.try_emplace(buffer.Get());
            if (inserted)
            {
                item->second = TakeGraph(graph.ImportBuffer(buffer, {BufferBoundary, BufferBoundary, true}));
            }
            return item->second;
        }

        /// @brief 所有待写 mip 已由 DeclareUploads 形成最终版本, 这里为共享读取保持唯一导入身份.
        [[nodiscard]] TextureId ReadTexture(RenderGraph &graph, const RHI::TextureHandle &texture)
        {
            const auto [item, inserted] = textures.try_emplace(texture.Get());
            if (inserted)
            {
                item->second = TakeGraph(graph.ImportTexture(texture, {TextureBoundary, TextureBoundary, true}));
            }
            return item->second;
        }
    };

    /// @brief 初次导入的存储未初始化. 消费者只能读取按 mip 或完整缓冲写后的版本.
    static SceneGraphResources DeclareUploads(RenderGraph &graph, const Detail::UploadBatch &batch,
                                               std::vector<GraphUpload> &uploads)
    {
        SceneGraphResources resources;
        for (const auto &upload : batch.textures)
        {
            const auto &texture = upload.revision->texture;
            auto version = TakeGraph(graph.ImportTexture(texture, {TextureBoundary, TextureBoundary, false}));
            auto width = texture->GetDesc().width;
            for (std::uint32_t mip = 0; mip < upload.levels.size(); ++mip)
            {
                version = TakeGraph(AddTextureUpload(graph, version, upload.levels[mip], std::size_t(width) * 4,
                                                     uploads, mip)).texture;
                width = std::max(1u, width / 2);
            }
            resources.textures.emplace(texture.Get(), version);
        }
        for (const auto &upload : batch.environmentTextures)
        {
            const auto &texture = upload.revision->texture;
            const auto &desc = texture->GetDesc();
            auto version = TakeGraph(graph.ImportTexture(texture, {TextureBoundary, TextureBoundary, false}));
            if (upload.dfg)
            {
                version = TakeGraph(AddTextureUpload(graph, version, std::as_bytes(std::span(upload.dfg->pixels)),
                    std::size_t(desc.width) * sizeof(upload.dfg->pixels.front()), uploads)).texture;
            }
            else
            {
                // 每个实际面和 mip 是完整写入. 下一层读依赖同一存储的最后生产版本.
                for (std::uint32_t mip = 0; mip < desc.mipLevels; ++mip)
                {
                    const auto &level = mip == 0 ? *upload.environment->base :
                                                  *upload.environment->prefiltered[mip - 1];
                    for (std::uint32_t face = 0; face < desc.arraySize; ++face)
                    {
                        version = TakeGraph(AddTextureUpload(graph, version, std::as_bytes(level.Face(face)),
                            std::size_t(level.edge) * sizeof(Detail::EnvironmentTexel), uploads, mip, face)).texture;
                    }
                }
            }
            resources.textures.emplace(texture.Get(), version);
        }
        for (const auto &upload : batch.buffers)
        {
            const auto initial = TakeGraph(graph.ImportBuffer(upload.buffer, {BufferBoundary, BufferBoundary, false}));
            resources.buffers.emplace(upload.buffer.Get(),
                TakeGraph(AddBufferUpload(graph, initial, upload.payload, uploads)));
        }
        return resources;
    }

    /// @brief 本 Pass 的几何权限与已准备绘制值. 原生指针借用 ForwardFrame 保有的对象至执行结束.
    struct GraphDraw
    {
        BufferAccess vertices;
        BufferAccess indices;
        RHI::BindingSet *object;
        RHI::BindingSet *material;
        RHI::GraphicsPipeline *pipeline;
        std::uint32_t indexCount;
    };

    /// @brief 一个几何 Pass 的图像矩形, View 集合与 Draw 顺序.
    /// lights 存在时携带本图授权的版本和范围, lightLayout 借用本槽保有的颜色布局.
    struct SceneRaster
    {
        RHI::ViewportState viewport;
        RHI::BindingSet *view = nullptr;
        std::optional<BufferAccess> lights;
        RHI::BindingLayout *lightLayout = nullptr;
        std::optional<BufferAccess> environment;
        std::optional<TextureAccess> dfg;
        std::optional<TextureAccess> reflection;
        RHI::Sampler *environmentSampler = nullptr;
        std::vector<GraphDraw> draws;
    };

    /// @brief 同一 Pass 按物理资源身份去重的精确读取, 在 setup 时交给图建立依赖与执行权限.
    struct RasterReads
    {
        std::map<std::uint32_t, BufferAccess> buffers;
        std::map<std::uint32_t, TextureAccess> textures;
    };

    /// @brief 从有效图像矩形建立光栅视口及同边界的裁剪.
    static RHI::ViewportState RasterViewport(const ViewImageRect &rect)
    {
        RHI::ViewportState state;
        state.AddViewportAndScissorRect(RHI::Viewport(static_cast<float>(rect.x),
            static_cast<float>(rect.x + rect.extent.width), static_cast<float>(rect.y),
            static_cast<float>(rect.y + rect.extent.height), 0.f, 1.f));
        return state;
    }

    /// @brief 将预解析的绑定关联到本图授权. 同一 Pass 的重复资源只声明一次, Depth 只取基色槽.
    static SceneRaster RasterInputs(RenderGraph &graph, SceneGraphResources &versions,
                                    const Detail::ForwardFrameData &frame, bool depthOnly, RasterReads &reads)
    {
        SceneRaster raster;
        raster.viewport = RasterViewport(frame.source.layout.rect);
        if (frame.draws.empty())
        {
            return raster;
        }
        const auto buffer = [&](const RHI::BufferHandle &handle, GraphUsage usage)
        {
            BufferAccess access{versions.ReadBuffer(graph, handle), {0, handle->GetDesc().byteSize}, usage};
            reads.buffers.try_emplace(access.resource.identity.index, access);
            return access;
        };
        static_cast<void>(buffer(frame.view->buffer, GraphUsage::ConstantBuffer));
        raster.view = frame.view->binding.Get();
        if (!depthOnly)
        {
            raster.lights = buffer(frame.activeLights->buffer, GraphUsage::ShaderRead);
            raster.lightLayout = frame.lightLayout.Get();
            const auto &environment = frame.environment;
            const auto sample = [&](const RHI::TextureHandle &texture)
            {
                const auto version = versions.ReadTexture(graph, texture);
                const auto &desc = texture->GetDesc();
                const TextureAccess access{version, {.mipCount = desc.mipLevels, .layerCount = desc.arraySize},
                                           GraphUsage::Sampled};
                reads.textures.try_emplace(version.identity.index, access);
                return access;
            };
            if (environment.dfg)
            {
                raster.dfg = sample(environment.dfg->texture);
                raster.environmentSampler = environment.sampler.Get();
            }
            if (environment.reflection)
            {
                raster.reflection = sample(environment.reflection->texture);
                raster.environment = buffer(environment.lightingConstants->buffer, GraphUsage::ConstantBuffer);
            }
        }
        raster.draws.reserve(frame.draws.size());
        for (const auto &draw : frame.draws)
        {
            const auto &material = *frame.materialRows[draw.material];
            static_cast<void>(buffer(draw.object->buffer, GraphUsage::ConstantBuffer));
            static_cast<void>(buffer(material.buffer, GraphUsage::ConstantBuffer));
            const auto slots = depthOnly ? 1 : material.textures.size();
            for (std::size_t i = 0; i < slots; ++i)
            {
                const auto &texture = material.textures[i]->texture;
                const auto version = versions.ReadTexture(graph, texture);
                reads.textures.try_emplace(version.identity.index,
                    TextureAccess{version, {.mipCount = texture->GetDesc().mipLevels}, GraphUsage::Sampled});
            }
            raster.draws.push_back({buffer(draw.geometry->vertices, GraphUsage::VertexBuffer),
                buffer(draw.geometry->indices, GraphUsage::IndexBuffer), draw.object->binding.Get(),
                material.binding.Get(), (depthOnly ? draw.depthPipeline : draw.pipeline).Get(), draw.geometry->indexCount});
        }
        return raster;
    }

    /// @brief 执行只使用图授权的几何、桥接附件和现成集合. RHI 对集合里的活动资源检查实际范围.
    static GraphFailure DrawScene(PassResources &resources, const SceneRaster &raster,
                                  RHI::CommandList &commands) noexcept
    {
        auto *lights = raster.lights ? resources.BindingSet(0) : nullptr;
        if (raster.lights && !lights)
        {
            // BindingSet 已将失败记入本 Pass. 停止录制, 由执行器返回该原因.
            return std::nullopt;
        }
        for (const auto &draw : raster.draws)
        {
            auto *vertices = resources.Resolve(draw.vertices.resource, draw.vertices.usage, draw.vertices.range);
            auto *indices = resources.Resolve(draw.indices.resource, draw.indices.usage, draw.indices.range);
            if (!vertices || !indices)
            {
                return std::nullopt;
            }
            RHI::GraphicsState state;
            state.SetFramebuffer(resources.Framebuffer()).SetPipeline(draw.pipeline).SetViewport(raster.viewport)
                .AddBindingSet(raster.view).AddBindingSet(draw.object).AddBindingSet(draw.material)
                .AddVertexBuffer(RHI::VertexBufferBinding().SetBuffer(vertices))
                .SetIndexBuffer(RHI::IndexBufferBinding().SetBuffer(indices).SetFormat(RHI::Format::R32_UINT));
            if (lights)
            {
                state.AddBindingSet(lights);
            }
            commands.SetGraphicsState(state);
            commands.DrawIndexed(RHI::DrawArguments().SetVertexCount(draw.indexCount).SetInstanceCount(1));
        }
        return std::nullopt;
    }

    /// @brief 深度和颜色共享 Draw 顺序及声明规则. 读前置深度时使用只读附件, 从而确定 WAW/RAW 关系.
    static void AddGeometryPass(RenderGraph &graph, SceneGraphResources &versions,
        const Detail::ForwardFrameData &frame, TextureId &depth, TextureId *color, bool readOnlyDepth)
    {
        RasterReads reads;
        auto raster = RasterInputs(graph, versions, frame, !color, reads);
        static_cast<void>(TakeGraph(graph.AddPass<SceneRaster>(color ? "Forward ShadeOpaque" : "Forward PrepareSurface",
            [&](PassBuilder &builder, auto &data)
            {
                data = std::move(raster);
                for (const auto &[index, access] : reads.buffers)
                {
                    if (!builder.Read(access.resource, access.usage, access.range))
                    {
                        return false;
                    }
                }
                for (const auto &[index, access] : reads.textures)
                {
                    if (!builder.Read(access.resource, access.usage, access.range))
                    {
                        return false;
                    }
                }
                const TextureRange depthRange{.aspect = TextureAspect::Depth};
                if (readOnlyDepth)
                {
                    if (!builder.Read(depth, GraphUsage::DepthStencilRead, depthRange))
                    {
                        return false;
                    }
                }
                else
                {
                    depth = builder.Write(depth, GraphUsage::DepthStencilAttachment,
                        {.load = LoadOp::Clear, .clearValue = DepthStencilClear{0.f, 0}}, depthRange);
                }
                if (color)
                {
                    *color = builder.Write(*color, GraphUsage::ColorAttachment,
                         {.load = LoadOp::Clear, .clearValue = RHI::Color(0.018f, 0.018f, 0.018f, 0.f)});
                }
                return depth.IsValid() && (!color || color->IsValid());
            }, DrawScene, +[](PassPreparation &preparation, const SceneRaster &data) -> GraphFailure
            {
                if (!data.lights)
                {
                    return std::nullopt;
                }
                // 沿生成集合枚举实际使用的字段, 读取必须来自 setup 给出的同一图权限.
                std::array<RHI::BindingSetItem, 5> items;
                std::size_t count = 0;
                for (const auto &slot : data.lightLayout->GetDesc()->bindings)
                {
                    std::optional<RHI::BindingSetItem> binding;
                    if (slot.type == RHI::ResourceType::StructuredBuffer_SRV)
                    {
                        const auto &access = *data.lights;
                        binding = preparation.ReadBuffer(slot.slot, access.resource, access.usage, access.range);
                    }
                    else if (slot.type == RHI::ResourceType::ConstantBuffer)
                    {
                        const auto &access = *data.environment;
                        binding = preparation.ReadBuffer(slot.slot, access.resource, access.usage, access.range);
                    }
                    else if (slot.type == RHI::ResourceType::Sampler)
                    {
                        binding = RHI::BindingSetItem::Sampler(slot.slot, data.environmentSampler);
                    }
                    else
                    {
                        const bool cube = slot.slot == Detail::ReflectionSlot;
                        assert(cube || slot.slot == Detail::DfgSlot);
                        const auto &access = cube ? *data.reflection : *data.dfg;
                        binding = preparation.SampledTexture(slot.slot, access.resource, access.range,
                            cube ? RHI::TextureDimension::TextureCube : RHI::TextureDimension::Texture2D);
                    }
                    if (!binding)
                    {
                        return std::nullopt;
                    }
                    items[count++] = *binding;
                }
                static_cast<void>(preparation.CreateBindingSet(*data.lightLayout, std::span(items).first(count)));
                return std::nullopt;
            })));
    }

    /// @brief 只有请求真实早期深度的下游才新增几何 Pass.
    static void PrepareSurface(RenderGraph &graph, SceneGraphResources &resources,
                               const Detail::ForwardFrameData &frame, TextureId &depth)
    {
        if (frame.needs.earlyDepth)
        {
            AddGeometryPass(graph, resources, frame, depth, nullptr, false);
        }
    }

    /// @brief 按准备的路径同时产生颜色和深度, 或读取早期深度并只写颜色.
    static void ShadeOpaque(RenderGraph &graph, SceneGraphResources &resources, const Detail::ForwardFrameData &frame,
                            TextureId &depth, TextureId &color)
    {
        AddGeometryPass(graph, resources, frame, depth, &color, frame.needs.earlyDepth);
    }

    /// @brief 天空读取本图的一个完整基础 cube, 加载不透明工作色后只填其未覆盖 RGB.
    struct SkyPass
    {
        TextureId sky;
        BufferAccess view;
        RHI::Sampler *sampler;
        RHI::GraphicsPipeline *pipeline;
        RHI::BindingLayout *layout;
        RHI::ViewportState viewport;
    };

    /// @brief 在有效矩形内加载上一版工作色, 以目的 alpha 混入天空. 保留已有的深度版本.
    static void AddSky(RenderGraph &graph, SceneGraphResources &resources, const Detail::ForwardFrameData &frame,
                       TextureId &color)
    {
        const auto &environment = frame.environment;
        if (!environment.sky)
        {
            return;
        }
        const auto &view = environment.skyConstants->buffer;
        const auto cube = resources.ReadTexture(graph, environment.sky->texture);
        const BufferAccess constants{resources.ReadBuffer(graph, view), {0, view->GetDesc().byteSize},
                                     GraphUsage::ConstantBuffer};
        static_cast<void>(TakeGraph(graph.AddPass<SkyPass>("Forward Sky",
            [&](PassBuilder &builder, SkyPass &data)
            {
                data = {cube, constants, environment.sampler.Get(), environment.skyPipeline.Get(),
                        environment.skyLayout.Get(), RasterViewport(frame.source.layout.rect)};
                color = builder.Write(color, GraphUsage::ColorAttachment, {.load = LoadOp::Load});
                return color.IsValid() && builder.Read(cube, GraphUsage::Sampled) &&
                       builder.Read(constants.resource, constants.usage, constants.range);
            }, +[](PassResources &resources, const SkyPass &data, RHI::CommandList &commands) noexcept -> GraphFailure
            {
                RHI::GraphicsState state;
                state.SetPipeline(data.pipeline).SetFramebuffer(resources.Framebuffer())
                    .SetViewport(data.viewport).AddBindingSet(resources.BindingSet(0));
                commands.SetGraphicsState(state);
                commands.Draw(RHI::DrawArguments().SetVertexCount(3).SetInstanceCount(1));
                return std::nullopt;
            }, +[](PassPreparation &preparation, const SkyPass &data) -> GraphFailure
            {
                const auto view = preparation.ReadBuffer(0, data.view.resource, data.view.usage, data.view.range);
                const auto sky = preparation.SampledTexture(0, data.sky, std::nullopt, RHI::TextureDimension::TextureCube);
                if (view && sky)
                {
                    const std::array items{*view, *sky, RHI::BindingSetItem::Sampler(0, data.sampler)};
                    static_cast<void>(preparation.CreateBindingSet(*data.layout, items));
                }
                return std::nullopt;
            })));
    }

    GraphResult<ForwardOutput> ForwardFrame::Declare(RenderGraph &graph, std::vector<GraphUpload> &uploads) const
    try
    {
        assert(m_Data);
        const auto &frame = *m_Data;
        const auto firstWrite = uploads.size();
        // LUT 属于纹理写, 设备准入要求它在本图所有缓冲上传之前声明.
        const auto lut = TakeGraph(frame.display.DeclareLut(graph, uploads));
        auto versions = DeclareUploads(graph, frame.uploads, uploads);
        const auto extent = frame.source.layout.outputExtent;
        ForwardOutput output;
        output.source = frame.source;
        output.depth = TakeGraph(graph.CreateTexture({.width = extent.width, .height = extent.height,
            .format = DepthFormat, .debugName = "Forward reverse depth", .isRenderTarget = true}));
        output.workingColor = TakeGraph(graph.CreateTexture({.width = extent.width, .height = extent.height,
            .format = WorkingFormat, .debugName = "Forward linear working color", .isRenderTarget = true}));
        PrepareSurface(graph, versions, frame, output.depth);
        ShadeOpaque(graph, versions, frame, output.depth, output.workingColor);
        AddSky(graph, versions, frame, output.workingColor);
        // 后处理只读天空合成后的版本. 比较或读回仍可在同图消费原工作色和 D32, 不必从显示色反推.
        output.displayColor = TakeGraph(frame.display.Declare(graph, uploads, output.workingColor, lut));
        output.m_Uploads.reserve(frame.uploads.writes);
        for (const auto &entry : std::span(uploads).subspan(firstWrite))
        {
            output.m_Uploads.push_back(entry.pass);
        }
        return output;
    }
    catch (const GraphError &failure)
    {
        return failure;
    }

    void ForwardFrame::Confirm(const ForwardOutput &output, const CompiledGraphPlan &plan) noexcept
    {
        assert(m_Data && output.m_Uploads.size() == m_Data->uploads.writes);
        if (!output.HasCompleteUploads(plan))
        {
            return;
        }
        m_Data->display.Confirm();
        // 按整批确认驻留. 环境纹理的所有面和 mip 共用一个 accepted 位, 前缀提交保活但没有确认资格.
        for (const auto &upload : m_Data->uploads.textures)
        {
            upload.revision->accepted = true;
        }
        for (const auto &upload : m_Data->uploads.environmentTextures)
        {
            upload.revision->accepted = true;
        }
        for (const auto &upload : m_Data->uploads.buffers)
        {
            upload.revision->accepted = true;
        }
    }
}
