#pragma once

#include <cstdint>
#include <vector>

#include <Engine/RHI/RHIDefinitions.hpp>

namespace Physara::RHI
{
    class RHIShader;
    struct RHIRenderPassDesc;

    struct RHIVertexAttribute
    {
        std::uint32_t location = 0; // shader location
        std::uint32_t binding = 0;  // 对应哪个顶点流绑定点
        TextureFormat format = TextureFormat::RGBA32F;
        std::uint32_t offset = 0; // 顶点结构体内字节偏移
    };

    struct RHIVertexBinding
    {
        std::uint32_t binding = 0;
        std::uint32_t stride = 0; // 单顶点字节跨度
    };

    struct RHIRasterizerState
    {
        CullMode cullMode = CullMode::Back;
        PolygonMode polygonMode = PolygonMode::Fill;
        float depthBias = 0.f;
        float depthBiasSlope = 0.f;
    };

    struct RHIDepthStencilState
    {
        bool depthTest = true;
        bool depthWrite = true;
        DepthCompareOp compareOp = DepthCompareOp::Less;
        bool stencilTest = false; // 模板阶段后续再扩展
    };

    struct RHIBlendState
    {
        bool blendEnable = false;
        BlendFactor srcColor = BlendFactor::One;
        BlendFactor dstColor = BlendFactor::Zero;
        BlendOp colorOp = BlendOp::Add;
        BlendFactor srcAlpha = BlendFactor::One;
        BlendFactor dstAlpha = BlendFactor::Zero;
        BlendOp alphaOp = BlendOp::Add;
    };

    struct RHIPipelineStateDesc
    {
        // Shader
        RHIShader *vertexShader = nullptr;
        RHIShader *fragmentShader = nullptr;
        RHIShader *computeShader = nullptr; // Compute Pipeline使用,图形管线时为空

        // 顶点输入
        std::vector<RHIVertexAttribute> vertexAttributes;
        std::vector<RHIVertexBinding> vertexBindings;

        // 固定状态
        RHIRasterizerState rasterizerState;
        RHIDepthStencilState depthStencilState;
        std::vector<RHIBlendState> blendStates; // 每个颜色附件一个
        PrimitiveTopology topology = PrimitiveTopology::Triangles;

        // Vulkan创建Pipeline时的兼容RenderPass布局, OpenGL可忽略
        const RHIRenderPassDesc *renderPassDesc = nullptr;
    };

    class RHIPipelineState
    {
    public:
        virtual ~RHIPipelineState() = default;
        virtual bool IsValid() const = 0;
    };
}