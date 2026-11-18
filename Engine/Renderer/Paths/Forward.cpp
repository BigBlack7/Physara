#include "Forward.hpp"

#include <algorithm>
#include <cassert>
#include <new>
#include <utility>

#include "../Materials/StaticGpu.hpp"

/// @file
/// @brief 把已拥有的上传和静态 Draw 组装为同一 View 的表面、颜色和显示子图.
/// 图内回调只访问已声明版本; 预制场景集合由 RHI 按实际活动绑定再次核对访问权限.

namespace Physara::Renderer
{
    using Detail::BufferBoundary;
    using Detail::DepthFormat;
    using Detail::DisplayFormat;
    using Detail::TextureBoundary;
    using Detail::WorkingFormat;

    ForwardFrame::ForwardFrame() noexcept = default;
    ForwardFrame::~ForwardFrame() = default;

    bool ForwardOutput::HasLiveUploads(const CompiledGraphPlan &plan) const noexcept
    {
        return std::ranges::any_of(m_Uploads, [&](PassId pass) { return IsLivePass(plan, pass); });
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
            m_Data->displayPipeline = nullptr;
            m_Data->displayLayout = nullptr;
            m_Data->degradation.reset();
        }
    }

    const std::optional<Core::BoundedText> &ForwardFrame::Degradation() const noexcept
    {
        assert(m_Data);
        return m_Data->degradation;
    }

    std::size_t ForwardFrame::GraphStorageBytes() const noexcept
    {
        assert(m_Data);
        // 图 arena 只保存上传回调和几个 Pass 描述. Draw 和字节另由帧槽持有.
        return 2048 + m_Data->uploads.writes * 256;
    }

    void ForwardFrame::ReleasePayload() noexcept
    {
        if (m_Data)
        {
            m_Data->uploads.Clear();
        }
    }

    ForwardRenderer::ForwardRenderer(RHI::Device &device, ForwardPackagePaths packages, std::uint32_t maxTextureSize,
                                     std::uint32_t maxAttachmentSize) noexcept
        : m_Device(device), m_Packages(std::move(packages)), m_MaxTextureSize(maxTextureSize),
          m_MaxAttachmentSize(maxAttachmentSize)
    {
    }

    ForwardRenderer::~ForwardRenderer() = default;

    std::optional<ForwardFailure> ForwardRenderer::Prepare(ForwardFrame &frame, const PreparedView &view,
                                                            PreviewMode mode, ViewResourceNeeds needs)
    try
    {
        assert(view.camera && view.reason == ViewSettingReason::None);
        const auto extent = view.layout.outputExtent;
        if (extent.width > m_MaxAttachmentSize || extent.height > m_MaxAttachmentSize)
        {
            Detail::Reject(ViewSettingReason::SceneAttachmentLimit, "Forward extent exceeds the attachment limit");
        }
        // RGBA16F、D32 和 SRGBA8 均为单样本同尺寸, 三者有短时重叠寿命.
        constexpr std::size_t bytesPerPixel = 8 + 4 + 4;
        if (extent.height > ForwardAttachmentBudget / bytesPerPixel / extent.width)
        {
            Detail::Reject(ViewSettingReason::SceneAttachmentLimit, "Forward attachments exceed the view byte budget");
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
        data.layout = view.layout;
        data.needs = needs;
        m_Resources->Begin();
        Detail::PrepareDraws(*m_Resources, data, view, mode);
        data.displayPipeline = m_Resources->programs.displayPipeline;
        data.displayLayout = m_Resources->programs.displayLayout;
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
        std::vector<GraphDraw> draws;
    };

    /// @brief 同一 Pass 按物理资源身份去重的精确读取, 在 setup 时交给图建立依赖与执行权限.
    struct RasterReads
    {
        std::map<std::uint32_t, BufferAccess> buffers;
        std::map<std::uint32_t, TextureAccess> textures;
    };

    /// @brief 将预解析的绑定关联到本图授权. 同一 Pass 的重复资源只声明一次, Depth 只取基色槽.
    static SceneRaster RasterInputs(RenderGraph &graph, SceneGraphResources &versions,
                                    const Detail::ForwardFrameData &frame, bool depthOnly, RasterReads &reads)
    {
        SceneRaster raster;
        const auto &rect = frame.layout.rect;
        const RHI::Viewport viewport(static_cast<float>(rect.x), static_cast<float>(rect.x + rect.extent.width),
            static_cast<float>(rect.y), static_cast<float>(rect.y + rect.extent.height), 0.f, 1.f);
        raster.viewport.AddViewportAndScissorRect(viewport);
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
                        {.load = LoadOp::Clear, .clearValue = RHI::Color(0.018f, 0.018f, 0.018f, 1.f)});
                }
                return depth.IsValid() && (!color || color->IsValid());
            }, DrawScene, +[](PassPreparation &preparation, const SceneRaster &data) -> GraphFailure
            {
                if (data.lights)
                {
                    // 沿 setup 声明的版本和整块范围创建集合. 0 是本 Pass 的首个准备结果,
                    // DrawScene 把它接在 View, Object, Material 三组之后, 对应 Shader 的 set 3.
                    const auto &access = *data.lights;
                    const auto slot = data.lightLayout->GetDesc()->bindings.front().slot;
                    if (auto binding = preparation.ReadBuffer(slot, access.resource, access.usage, access.range))
                    {
                        static_cast<void>(preparation.CreateBindingSet(*data.lightLayout, std::span(&*binding, 1)));
                    }
                }
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

    /// @brief 显示转换只读同图工作颜色, 布局和 Pipeline 借用本槽持有的固定对象.
    struct DisplayPass
    {
        TextureId workingColor;
        RHI::GraphicsPipeline *pipeline;
        RHI::BindingLayout *layout;
    };

    /// @brief 工作色是瞬态图资源. 只在其物化后用授权的实际纹理产生显示转换绑定.
    static TextureId AddDisplay(RenderGraph &graph, const Detail::ForwardFrameData &frame, TextureId input,
                                 TextureId target)
    {
        TextureId display;
        static_cast<void>(TakeGraph(graph.AddPass<DisplayPass>("Forward display preview",
            [&](PassBuilder &builder, auto &data)
            {
                data = {input, frame.displayPipeline.Get(), frame.displayLayout.Get()};
                display = builder.Write(target, GraphUsage::ColorAttachment,
                    {.load = LoadOp::Discard, .coverage = WriteCoverage::Full});
                return display.IsValid() && builder.Read(input, GraphUsage::Sampled);
            }, +[](PassResources &resources, const DisplayPass &data, RHI::CommandList &commands) noexcept -> GraphFailure
            {
                RHI::GraphicsState state;
                auto *framebuffer = resources.Framebuffer();
                state.SetFramebuffer(framebuffer).SetPipeline(data.pipeline).AddBindingSet(resources.BindingSet(0));
                state.viewport.AddViewport(framebuffer->GetFramebufferInfo().GetViewport());
                commands.SetGraphicsState(state);
                commands.Draw(RHI::DrawArguments().SetVertexCount(3).SetInstanceCount(1));
                return std::nullopt;
            }, +[](PassPreparation &preparation, const DisplayPass &data) -> GraphFailure
            {
                if (auto binding = preparation.SampledTexture(0, data.workingColor))
                {
                    static_cast<void>(preparation.CreateBindingSet(*data.layout, std::span(&*binding, 1)));
                }
                return std::nullopt;
            })));
        return display;
    }

    GraphResult<ForwardOutput> ForwardFrame::Declare(RenderGraph &graph, std::vector<GraphUpload> &uploads) const
    try
    {
        assert(m_Data && m_Data->displayPipeline);
        const auto &frame = *m_Data;
        const auto firstWrite = uploads.size();
        auto versions = DeclareUploads(graph, frame.uploads, uploads);
        const auto extent = frame.layout.outputExtent;
        ForwardOutput output;
        output.depth = TakeGraph(graph.CreateTexture({.width = extent.width, .height = extent.height,
            .format = DepthFormat, .debugName = "Forward reverse depth", .isRenderTarget = true}));
        output.workingColor = TakeGraph(graph.CreateTexture({.width = extent.width, .height = extent.height,
            .format = WorkingFormat, .debugName = "Forward linear working color", .isRenderTarget = true}));
        const auto display = TakeGraph(graph.CreateTexture({.width = extent.width, .height = extent.height,
            .format = DisplayFormat, .debugName = "Forward viewport color", .isRenderTarget = true,
            .initialState = RHI::ResourceStates::ShaderResource, .keepInitialState = true}));
        PrepareSurface(graph, versions, frame, output.depth);
        ShadeOpaque(graph, versions, frame, output.depth, output.workingColor);
        output.displayColor = AddDisplay(graph, frame, output.workingColor, display);
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
        if (output.displayColor.identity.generation != plan.graphGeneration ||
            !std::ranges::all_of(output.m_Uploads, [&](PassId pass) { return IsLivePass(plan, pass); }))
        {
            return;
        }
        // 每个新增区段都属于本次 Draw 的必需资源. 含多层的纹理只以完整被接受的上传包提交.
        for (const auto &upload : m_Data->uploads.textures)
        {
            upload.revision->accepted = true;
        }
        for (const auto &upload : m_Data->uploads.buffers)
        {
            upload.revision->accepted = true;
        }
    }
}
