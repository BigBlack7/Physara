#include "Pipeline.hpp"

/// @file
/// @brief 从图形阶段或单个计算阶段准备原生 Program. 核对活动绑定后发布持有依赖的 Pipeline.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <exception>
#include <utility>

#include "InputLayout.hpp"

namespace Physara::RHI::OpenGL
{
    OpenGLGraphicsPipeline::OpenGLGraphicsPipeline(OpenGLDevice *device, const GraphicsPipelineDesc &desc,
                                                   const FramebufferInfo &info, PipelineBindingPlan plan)
        : m_Device(device), m_Desc(desc), m_Info(info), m_Plan(std::move(plan)),
          m_Deletion(std::make_unique<NativeDeletion>(NativeKind::Program))
    {
    }

    OpenGLGraphicsPipeline::~OpenGLGraphicsPipeline()
    {
        m_Device->ReleaseAllocation(std::move(m_Deletion), m_Published);
    }

    /// @brief 每次准备的临时 GL Shader 仅在创建线程/context 栈上拥有, 不发布到外部.
    /// @note 作用域结束先置删除标记, 已附加对象仍由 Program 保活到 detach 或 Program 回滚.
    struct TemporaryShader
    {
        OpenGLDevice &device;
        std::unique_ptr<NativeDeletion> deletion = std::make_unique<NativeDeletion>(NativeKind::Shader);
        ~TemporaryShader()
        {
            device.ReleaseAllocation(std::move(deletion), false);
        }
    };

    // 固定大小片段保留 UTF-8 边界与生成文本坐标, 不解析供应商各异的 driver log 行号语法.
    // line/column 从 1 开始, column 按字节推进; phase/stage/field 由内部固定标签提供, 片段不超过 256 bytes.
    static void EmitProgramText(MessageCallback &callback, const char *operation, const char *phase,
                                const char *profile, const char *stage, const char *field,
                                std::string_view text) noexcept
    {
        std::size_t offset = 0;
        std::size_t line = 1;
        std::size_t column = 1;
        do
        {
            auto end = std::min(offset + 256, text.size());
            const auto byteEnd = end;
            while (end > offset && end < text.size() && (static_cast<unsigned char>(text[end]) & 0xc0u) == 0x80u)
            {
                --end;
            }
            if (end == offset)
            {
                end = byteEnd;
            }
            const auto part = text.substr(offset, end - offset);
            char message[512]{};
            std::snprintf(message, sizeof(message),
                          "OpenGL.%s.%s: profile=%s, stage=%s, %s byte=%zu line=%zu column=%zu\n%.*s",
                          operation, phase, profile, stage, field, offset, line, column, int(part.size()),
                          part.empty() ? "" : part.data());
            callback.Message(MessageSeverity::Error, message);
            for (char byte : part)
            {
                if (byte == '\n')
                {
                    ++line;
                    column = 1;
                }
                else
                {
                    ++column;
                }
            }
            offset = end;
        } while (offset < text.size());
    }

    void ReportProgramFailure(MessageCallback &callback, const char *operation, const char *phase,
                              std::span<const ProgramStageDiagnostic> stages, std::string_view driverLog) noexcept
    {
        // 身份先于可能很长的日志/源码. 多阶段链接的驱动日志归程序, 每份输入仍标出实际阶段.
        const auto first = DescribeShaderStage(stages.front().desc.shaderType);
        const auto *logStage = stages.size() == 1 ? first.stage : "program";
        const auto emit = [&](const char *stage, const char *field, std::string_view text)
        {
            EmitProgramText(callback, operation, phase, first.profile, stage, field, text);
        };
        for (const auto &stage : stages)
        {
            const auto *name = DescribeShaderStage(stage.desc.shaderType).stage;
            emit(name, "entry", stage.desc.entryName);
            emit(name, "source", stage.desc.debugName);
        }
        emit(logStage, "driver-log", driverLog.substr(0, 2047));
        if (driverLog.size() > 2047)
        {
            emit(logStage, "driver-log", "<truncated after 2047 bytes>");
        }
        for (const auto &stage : stages)
        {
            emit(DescribeShaderStage(stage.desc.shaderType).stage, "GLSL", stage.glsl);
        }
    }

    /// @brief 编译已生成的单阶段并附加到未发布 Program, 文本由外层保留到 link 完成以支持失败定位.
    static bool CompileStage(OpenGLDevice &device, const OpenGLShader &shader, const std::string &source,
                             GLuint program, const char *operation)
    {
        const ProgramStageDiagnostic diagnostic{shader.GetDesc(), source};
        const auto *text = source.c_str();
        const auto type = shader.GetDesc().shaderType;
        const GLenum stage = type == ShaderType::Vertex ? GL_VERTEX_SHADER :
                            (type == ShaderType::Pixel ? GL_FRAGMENT_SHADER : GL_COMPUTE_SHADER);
        TemporaryShader temporary{device};
        auto &name = temporary.deletion->name;
        name = glCreateShader(stage);
        if (!device.CheckNative("CreateProgram.Shader", name) || name == 0)
        {
            return false;
        }
        glShaderSource(name, 1, &text, nullptr);
        glCompileShader(name);
        GLint compiled = GL_FALSE;
        glGetShaderiv(name, GL_COMPILE_STATUS, &compiled);
        if (!device.CheckNative("CreateProgram.Compile", name, shader.GetDesc().debugName.c_str()))
        {
            ReportProgramFailure(*device.GetMessageCallback(), operation, "Compile", {&diagnostic, 1},
                                 "Native compilation call failed; see preceding GL diagnostic.");
            return false;
        }
        if (compiled != GL_TRUE)
        {
            std::array<char, 2048> log{};
            glGetShaderInfoLog(name, GLsizei(log.size()), nullptr, log.data());
            ReportProgramFailure(*device.GetMessageCallback(), operation, "Compile", {&diagnostic, 1}, log.data());
            return false;
        }
        glAttachShader(program, name);
        // 删除标记不销毁仍被 Program 持有的 Shader, Program 失败/退休时会释放最后依赖.
        return device.CheckNative("CreateProgram.Attach", program);
    }

    /// @brief 检查链接后仍活动的 native 接口, 允许驱动移除未消费资源, 不允许保留资源绑定错位.
    static void CheckLinkedBindings(GLuint program, PipelineBindingPlan &plan, const ShaderCaps &caps)
    {
        // 核对仍活动的块和 sampler 的绑定位置. 动态 UBO 还须取得驱动报告的完整块大小以确定绑定范围.
        for (auto &entry : plan.resources)
        {
            const auto type = entry.resource.type;
            const bool uniform = type == ResourceType::ConstantBuffer || type == ResourceType::PushConstants;
            if (type == ResourceType::Texture_UAV)
            {
                const auto name = BlockName(entry.resource.key);
                const auto location = glGetUniformLocation(program, name.c_str());
                if (location >= 0)
                {
                    GLint binding = -1;
                    glGetUniformiv(program, location, &binding);
                    RequirePreparation(binding == GLint(entry.unit), "InterfaceMismatch: linked image " + name);
                }
                continue;
            }
            if (!uniform && type != ResourceType::StructuredBuffer_SRV &&
                type != ResourceType::StructuredBuffer_UAV)
            {
                continue;
            }
            const auto interfaceType = uniform ? GL_UNIFORM_BLOCK : GL_SHADER_STORAGE_BLOCK;
            const auto name = BlockName(entry.resource.key);
            const auto index = glGetProgramResourceIndex(program, interfaceType, name.c_str());
            if (index == GL_INVALID_INDEX)
            {
                continue;
            }
            constexpr GLenum Property = GL_BUFFER_BINDING;
            GLint binding = -1;
            glGetProgramResourceiv(program, interfaceType, index, 1, &Property, 1, nullptr, &binding);
            RequirePreparation(binding == GLint(entry.unit), "InterfaceMismatch: linked block " + name);
            if (type == ResourceType::PushConstants || entry.bindingType == ResourceType::VolatileConstantBuffer)
            {
                constexpr GLenum Size = GL_BUFFER_DATA_SIZE;
                GLint extent = 0;
                glGetProgramResourceiv(program, interfaceType, index, 1, &Size, 1, nullptr, &extent);
                RequirePreparation(extent > 0 && extent <= caps.uniformBlockBytes,
                                   "InterfaceMismatch: native dynamic block size " + name);
                entry.nativeExtent = (std::max)(entry.resource.extent, static_cast<std::uint32_t>(extent));
                if (type == ResourceType::PushConstants)
                {
                    plan.pushBackingBytes = (std::max)(plan.pushBytes, entry.nativeExtent);
                }
            }
        }
        for (const auto &entry : plan.pairs)
        {
            const auto name = PairName(entry.unit);
            const auto location = glGetUniformLocation(program, name.c_str());
            if (location < 0)
            {
                continue;
            }
            GLint unit = -1;
            glGetUniformiv(program, location, &unit);
            RequirePreparation(unit == GLint(entry.unit), "InterfaceMismatch: linked sampler " + name);
        }
    }

    /// @brief 链接本次实际阶段并解除临时 Shader 附件, 调用者保管未发布的 Program.
    static bool LinkProgram(OpenGLDevice &device, GLuint program, std::span<const ProgramStageDiagnostic> stages,
                            const char *operation)
    {
        glLinkProgram(program);
        GLint linked = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (!device.CheckNative("CreateProgram.Link", program) || linked != GL_TRUE)
        {
            std::array<char, 2048> log{};
            glGetProgramInfoLog(program, GLsizei(log.size()), nullptr, log.data());
            ReportProgramFailure(*device.GetMessageCallback(), operation, "Link", stages, log.data());
            return false;
        }
        GLint count = 0;
        std::array<GLuint, 2> attached{};
        glGetAttachedShaders(program, GLsizei(attached.size()), &count, attached.data());
        for (GLint index = 0; index < count; ++index)
        {
            glDetachShader(program, attached[std::size_t(index)]);
        }
        return true;
    }

    /// @brief 检查普通状态数值和基线扩展限制, 不能以 Validation 包装存在为裸 Device 的安全前提.
    static void CheckRenderState(const RenderState &state, const FramebufferInfo &info)
    {
        const auto &raster = state.rasterState;
        const auto &depth = state.depthStencilState;
        RequirePreparation(!state.singlePassStereo.enabled && !raster.conservativeRasterEnable &&
                               !raster.forcedSampleCount && !raster.programmableSamplePositionsEnable &&
                               !raster.quadFillEnable && !raster.multisampleEnable && !raster.antialiasedLineEnable,
                           "Unsupported: raster extension/profile");
        RequirePreparation(raster.fillMode <= RasterFillMode::Wireframe && raster.cullMode <= RasterCullMode::None &&
                               std::isfinite(raster.depthBiasClamp) && std::isfinite(raster.slopeScaledDepthBias) &&
                               depth.depthFunc >= ComparisonFunc::Never && depth.depthFunc <= ComparisonFunc::Always,
                           "InvalidArgument: raster/depth state");
        for (const auto &face : {depth.frontFaceStencil, depth.backFaceStencil})
        {
            RequirePreparation(
                face.failOp >= StencilOp::Keep && face.failOp <= StencilOp::DecrementAndWrap &&
                    face.depthFailOp >= StencilOp::Keep && face.depthFailOp <= StencilOp::DecrementAndWrap &&
                    face.passOp >= StencilOp::Keep && face.passOp <= StencilOp::DecrementAndWrap &&
                    face.stencilFunc >= ComparisonFunc::Never && face.stencilFunc <= ComparisonFunc::Always,
                "InvalidArgument: stencil state");
        }
        RequirePreparation((!depth.depthTestEnable && !depth.stencilEnable) || info.depthFormat != Format::UNKNOWN,
                           "InvalidArgument: depth/stencil state needs attachment");
        RequirePreparation(!depth.stencilEnable || GetFormatInfo(info.depthFormat).hasStencil,
                           "InvalidArgument: stencil state requires stencil format");
        const auto validFactor = [](BlendFactor value) {
            const auto number = std::uint32_t(value);
            return (number >= 1 && number <= 11) || (number >= 14 && number <= 19);
        };
        for (std::size_t index = 0; index < info.colorFormats.size(); ++index)
        {
            const auto &target = state.blendState.targets[index];
            // 枚举合法不等于本 profile 能执行; inactive 因子仍保留在描述中, 应用时再统一为无害值.
            RequirePreparation(validFactor(target.srcBlend) && validFactor(target.destBlend) &&
                                   validFactor(target.srcBlendAlpha) && validFactor(target.destBlendAlpha) &&
                                   target.blendOp >= BlendOp::Add && target.blendOp <= BlendOp::Max &&
                                   target.blendOpAlpha >= BlendOp::Add && target.blendOpAlpha <= BlendOp::Max &&
                                   (std::uint32_t(target.colorWriteMask) & ~0xfu) == 0,
                               "InvalidArgument: blend state");
            const auto use = GetBlendFactorUse(target, info.colorFormats[index]);
            const auto dualSource = [](BlendFactor value)
            { return value >= BlendFactor::Src1Color && value <= BlendFactor::InvSrc1Alpha; };
            // 基线没有 index=1 的第二颜色输出, 只拒绝本次真正需要该输入的配置.
            RequirePreparation(
                (!use.color || (!dualSource(target.srcBlend) && !dualSource(target.destBlend))) &&
                    (!use.alpha || (!dualSource(target.srcBlendAlpha) && !dualSource(target.destBlendAlpha))),
                "Unsupported: active dual-source blend requires a second shader color output");
        }
    }

    /// @brief 对比活动阶段 IO、顶点类型与目标数值类型, 具体 framebuffer 身份不进入 Pipeline.
    static void CheckInterfaces(const OpenGLShader &vertex, const OpenGLShader *fragment,
                                const OpenGLInputLayout *input, const FramebufferInfo &info)
    {
        for (const auto &value : vertex.Inputs())
        {
            // 顶点存储到 Shader 的转换按数值类别核对, 跨阶段 varying 则在下方要求形状/插值完全一致.
            RequirePreparation(input != nullptr, "InterfaceMismatch: missing vertex input layout");
            const auto found = std::ranges::find(input->Inputs(), value.location, &VertexInput::location);
            RequirePreparation(found != input->Inputs().end() && found->scalar == value.scalar,
                               "InterfaceMismatch: vertex input location/type");
        }
        if (!fragment)
        {
            return;
        }
        for (const auto &value : fragment->Inputs())
        {
            const auto found = std::ranges::find(vertex.Outputs(), value.location, &ShaderIo::location);
            RequirePreparation(found != vertex.Outputs().end() && *found == value,
                               "InterfaceMismatch: VS/FS location/type/interpolation");
        }
        for (const auto &value : fragment->Outputs())
        {
            RequirePreparation(value.location < info.colorFormats.size(),
                               "InterfaceMismatch: fragment output location");
            const auto &format = GetFormatInfo(info.colorFormats[value.location]);
            const auto scalar =
                format.kind == FormatKind::Integer ? (format.isSigned ? GL_INT : GL_UNSIGNED_INT) : GL_FLOAT;
            RequirePreparation(value.scalar == GLenum(scalar), "InterfaceMismatch: fragment output numeric type");
        }
    }

    GraphicsPipelineHandle OpenGLDevice::CreateGraphicsPipeline(const GraphicsPipelineDesc &desc,
                                                                const FramebufferInfo &info)
    {
        constexpr auto Operation = "CreateGraphicsPipeline";
        if (!RequireContext(Operation))
        {
            return nullptr;
        }
        try
        {
            // 先完成对象归属, 固定状态和 IO/目标预检, 本阶段不创建原生对象.
            auto *vertex = dynamic_cast<OpenGLShader *>(desc.VS.Get());
            auto *fragment = dynamic_cast<OpenGLShader *>(desc.PS.Get());
            auto *input = dynamic_cast<OpenGLInputLayout *>(desc.inputLayout.Get());
            RequirePreparation(vertex && vertex->Owner() == this &&
                                   vertex->GetDesc().shaderType == ShaderType::Vertex &&
                                   (!desc.PS || (fragment && fragment->Owner() == this &&
                                                 fragment->GetDesc().shaderType == ShaderType::Pixel)) &&
                                   (!desc.inputLayout || (input && input->Owner() == this)),
                               "InvalidResource: pipeline shader/input ownership");
            RequirePreparation(!desc.HS && !desc.DS && !desc.GS && !desc.shadingRateState.enabled &&
                                   desc.primType < PrimitiveType::PatchList && desc.patchControlPoints == 0,
                               "Unsupported: graphics stage/primitive/extension");
            RequirePreparation(info.sampleCount == 1 && info.sampleQuality == 0 &&
                                   info.colorFormats.size() <= std::size_t(m_Caps.maxColorAttachments) &&
                                   info.colorFormats.size() <= std::size_t(m_Caps.maxDrawBuffers) &&
                                   (!info.colorFormats.empty() || info.depthFormat != Format::UNKNOWN) &&
                                   (fragment || info.colorFormats.empty()),
                               "Unsupported: pipeline target/sample profile");
            for (std::size_t index = 0; index < info.colorFormats.size(); ++index)
            {
                const auto &format = GetFormatCaps(info.colorFormats[index], TextureDimension::Texture2D);
                const auto required = desc.renderState.blendState.targets[index].blendEnable
                                          ? FormatSupport::RenderTarget | FormatSupport::Blendable
                                          : FormatSupport::RenderTarget;
                RequirePreparation((format.support & required) == required && (format.sampleMask & 2u) != 0 &&
                                       !GetFormatInfo(info.colorFormats[index]).hasDepth,
                                   "Unsupported: color/blend target format");
            }
            if (info.depthFormat != Format::UNKNOWN)
            {
                const auto &format = GetFormatCaps(info.depthFormat, TextureDimension::Texture2D);
                RequirePreparation((format.support & FormatSupport::DepthStencil) != 0 && (format.sampleMask & 2u) != 0,
                                   "Unsupported: depth target format");
            }
            CheckRenderState(desc.renderState, info);
            CheckInterfaces(*vertex, fragment, input, info);
            for (const auto &layout : desc.bindingLayouts)
            {
                const auto *native = dynamic_cast<OpenGLBindingLayout *>(layout.Get());
                RequirePreparation(native && native->Owner() == this, "InvalidResource: pipeline binding layout");
            }
            std::array<OpenGLShader *, 2> shaders{vertex, fragment};
            // 计划与 CPU owner 均先于 GL 创建, 任何准备异常都不会发布半成品.
            auto plan = BuildBindingPlan({shaders.data(), fragment ? 2u : 1u}, desc.bindingLayouts, m_Caps.shader);
            auto pipeline = std::make_unique<OpenGLGraphicsPipeline>(this, desc, info, std::move(plan));
            // 两阶段生成文本保留到本次 link 结束; 失败同步发送, 成功即随栈释放, 不增加持久缓存.
            std::array<std::string, 2> sources;
            sources[0] = LowerShader(*vertex, pipeline->m_Plan);
            if (fragment)
            {
                sources[1] = LowerShader(*fragment, pipeline->m_Plan);
            }
            const std::array<ProgramStageDiagnostic, 2> diagnostics{{
                {vertex->GetDesc(), sources[0]},
                {fragment ? fragment->GetDesc() : vertex->GetDesc(), sources[1]},
            }};
            const std::span stages{diagnostics.data(), fragment ? 2u : 1u};
            if (!BeginNative("CreateGraphicsPipeline"))
            {
                return nullptr;
            }
            if (std::ranges::any_of(pipeline->m_Plan.pairs, [](const auto &pair) { return pair.pair.dummy; }) &&
                PrepareDummySampler() == 0)
            {
                return nullptr;
            }
            auto &program = pipeline->m_Deletion->name;
            program = glCreateProgram();
            if (!CheckNative("CreateGraphicsPipeline.Program", program) || program == 0)
            {
                return nullptr;
            }
            if (!CompileStage(*this, *vertex, sources[0], program, Operation) ||
                (fragment && !CompileStage(*this, *fragment, sources[1], program, Operation)))
            {
                return nullptr;
            }
            if (!LinkProgram(*this, program, stages, Operation))
            {
                return nullptr;
            }
            CheckLinkedBindings(program, pipeline->m_Plan, m_Caps.shader);
            if (!CheckNative("CreateGraphicsPipeline.Interfaces", program))
            {
                return nullptr;
            }
            // 所有原生步骤与活动绑定查询通过后, Program 才进入对外可见的排队退休寿命.
            pipeline->m_Published = true;
            return GraphicsPipelineHandle::Create(pipeline.release());
        }
        catch (const std::exception &error)
        {
            Report("CreateGraphicsPipeline", error.what());
            return nullptr;
        }
    }

    GraphicsPipelineHandle OpenGLDevice::CreateGraphicsPipeline(const GraphicsPipelineDesc &desc,
                                                                Framebuffer *framebuffer)
    {
        if (!RequireContext("CreateGraphicsPipeline.Framebuffer"))
        {
            return nullptr;
        }
        auto *native = dynamic_cast<OpenGLFramebuffer *>(framebuffer);
        if (!native || native->Owner() != this)
        {
            Report("CreateGraphicsPipeline", "InvalidResource: framebuffer ownership");
            return nullptr;
        }
        // 裸 Device 在丢弃附件身份前核对归属; 兼容性重载只接收格式/样本值, 不保活 framebuffer.
        return CreateGraphicsPipeline(desc, native->GetFramebufferInfo());
    }
    OpenGLComputePipeline::OpenGLComputePipeline(OpenGLDevice *device, const ComputePipelineDesc &desc,
                                                  PipelineBindingPlan plan)
        : m_Device(device), m_Desc(desc), m_Plan(std::move(plan)),
          m_Deletion(std::make_unique<NativeDeletion>(NativeKind::Program))
    {
    }

    OpenGLComputePipeline::~OpenGLComputePipeline()
    {
        m_Device->ReleaseAllocation(std::move(m_Deletion), m_Published);
    }

    ComputePipelineHandle OpenGLDevice::CreateComputePipeline(const ComputePipelineDesc &desc)
    {
        constexpr auto Operation = "CreateComputePipeline";
        if (!RequireContext(Operation))
        {
            return nullptr;
        }
        const ShaderDesc *shaderDesc = nullptr;
        try
        {
            const auto &caps = m_Caps.shader;
            RequirePreparation(caps.compute, "Unsupported: compute native entries");
            auto *shader = dynamic_cast<OpenGLShader *>(desc.CS.Get());
            RequirePreparation(shader && shader->Owner() == this && shader->GetDesc().shaderType == ShaderType::Compute,
                               "InvalidResource: compute shader ownership or stage");
            shaderDesc = &shader->GetDesc();
            for (const auto &layout : desc.bindingLayouts)
            {
                const auto *native = dynamic_cast<OpenGLBindingLayout *>(layout.Get());
                RequirePreparation(native && native->Owner() == this, "InvalidResource: compute binding layout");
            }
            // 三轴 local size 来自同一 CS. 逐轴检查并在相乘前核对总量, 避免乘积溢出掩盖设备限制.
            const auto &size = shader->LocalSize();
            std::uint64_t invocations = 1;
            for (std::size_t axis = 0; axis < size.size(); ++axis)
            {
                RequirePreparation(size[axis] <= caps.groupSize[axis] && caps.workGroupInvocations > 0 &&
                    invocations <= std::uint64_t(caps.workGroupInvocations) / size[axis],
                    "LimitExceeded: compute local size axis " + std::to_string(axis) +
                    ", local=" + std::to_string(size[axis]) + ", axis limit=" + std::to_string(caps.groupSize[axis]) +
                    ", invocation limit=" + std::to_string(caps.workGroupInvocations));
                invocations *= size[axis];
            }
            auto plan = BuildBindingPlan({&shader, 1}, desc.bindingLayouts, caps);
            auto source = LowerShader(*shader, plan);
            auto pipeline = std::make_unique<OpenGLComputePipeline>(this, desc, std::move(plan));
            if (!BeginNative("CreateComputePipeline") ||
                (std::ranges::any_of(pipeline->m_Plan.pairs, [](const auto &pair) { return pair.pair.dummy; }) &&
                 PrepareDummySampler() == 0))
            {
                return nullptr;
            }
            auto &program = pipeline->m_Deletion->name;
            program = glCreateProgram();
            const ProgramStageDiagnostic stage{*shaderDesc, source};
            if (!CheckNative("CreateComputePipeline.Program", program) || program == 0 ||
                !CompileStage(*this, *shader, source, program, Operation) ||
                !LinkProgram(*this, program, {&stage, 1}, Operation))
            {
                return nullptr;
            }
            CheckLinkedBindings(program, pipeline->m_Plan, caps);
            if (!CheckNative("CreateComputePipeline.Interfaces", program))
            {
                return nullptr;
            }
            pipeline->m_Published = true;
            return ComputePipelineHandle::Create(pipeline.release());
        }
        catch (const std::exception &error)
        {
            // shaderDesc 只在归属和阶段检查通过后赋值. 调用方的 desc.CS 在异常展开期间仍持有该 Shader.
            if (shaderDesc)
            {
                const auto context = FormatShaderDiagnosticContext(*shaderDesc);
                Report(Operation, error.what(), context.data());
            }
            else
            {
                Report(Operation, error.what());
            }
            return nullptr;
        }
    }
} // namespace Physara::RHI::OpenGL
