#pragma once

/// @file
/// @brief 图形准备的不可变绑定计划和原生 Program owner, 不执行命令或拥有具体 framebuffer.

#include <string_view>

#include "Bindings.hpp"

namespace Physara::RHI::OpenGL
{
    /// @brief 同一逻辑资源跨 stage 合并后的记录, layoutIndex 指 Pipeline 布局位置, unit 仅对 buffer 有效.
    /// @note stages 的 bit 0/1 分别为 VS/FS; resource.id 不具有跨阶段身份意义.
    struct PlannedResource
    {
        ShaderResource resource;
        std::uint32_t layoutIndex = 0;
        std::uint32_t slot = 0;
        std::uint32_t unit = 0;
        std::uint32_t stages = 0;
    };

    /// @brief 按纹理与采样器双方身份合并的单元, stages 的位含义与 PlannedResource 相同.
    /// @note dummy 对的 sampler 来自 Device, 不能按其占位 key 查调用方 BindingSet.
    struct PlannedPair
    {
        ShaderSamplingPair pair;
        std::uint32_t unit = 0;
        std::uint32_t stages = 0;
    };

    /// @brief 描述实际 IR 活动集合, 同一物理位置可被两 stage 使用, combined 数量仍分别计数.
    /// @note resources/pairs 按完整逻辑 key 稳定排序; textureCounts 统计采样对, 不是 Texture 声明数.
    struct PipelineBindingPlan
    {
        std::vector<PlannedResource> resources;
        std::vector<PlannedPair> pairs;
        std::array<std::uint32_t, 2> uniformCounts{};
        std::array<std::uint32_t, 2> storageCounts{};
        std::array<std::uint32_t, 2> textureCounts{};
    };

    /// @brief 纯 CPU 计划计算, 接受生产查询的 caps, 失败抛准备错误且不创建 GL 对象.
    /// @pre 各 Shader/布局均为 Device 工厂已确认实现类型和设备归属的非空对象.
    /// @note 顺序布局以数组位置解释 set, descriptor-set 布局以 registerSpace 解释 set, 同一计划不可混用.
    [[nodiscard]] PipelineBindingPlan BuildBindingPlan(std::span<OpenGLShader *const> shaders,
                                                       const BindingLayoutVector &layouts, const ShaderCaps &caps);
    /// @brief 以确定计划生成一阶段 GLSL 460, 不修改 Shader 的 canonical words.
    /// @note plan 须由包含该 Shader 的活动反射建立; 失败抛异常, 本层不创建原生对象.
    [[nodiscard]] std::string LowerShader(const OpenGLShader &shader, const PipelineBindingPlan &plan);
    /// @brief lowering 与 link 后查询共用的块名, 保留完整 set/binding 身份以避免跨集合冲突.
    [[nodiscard]] std::string BlockName(ShaderBindingKey key);
    /// @brief lowering 与 link 后查询共用的组合 sampler 名, unit 来自确定的绑定计划.
    [[nodiscard]] std::string PairName(std::uint32_t unit);

    /// @brief 本次原生准备的阶段身份与完整生成文本, 全部字段仅借用到同步诊断调用结束.
    struct ProgramStageDiagnostic
    {
        const ShaderDesc &desc;
        std::string_view glsl;
    };

    /// @brief 先报告全部阶段身份, 再分块发送有界 driver log 与可按生成行号重组的完整 GLSL.
    /// @pre phase 为 Compile/Link, stages 仅包含实际参与的阶段, callback 不抛出且不重入.
    /// @note 每条消息最多 511 bytes, driver log 最多 2047 bytes; 源文本同步流出, 不建立缓存或原始源行映射.
    /// 生成 line/column 从 1 开始, column 按字节计; 身份和文本的借用均不跨 callback 返回.
    void ReportProgramFailure(MessageCallback &callback, const char *phase,
                              std::span<const ProgramStageDiagnostic> stages, std::string_view driverLog) noexcept;

    /// @brief 已链接 Program 与固定图形描述的 owner, 描述/计划借用在对象存活期间有效.
    /// @note desc 强持有 Shader/InputLayout/BindingLayout, info 只保存格式和样本, 不持有附件.
    /// 发布前失败由创建 context 回滚, 发布后最终 Release 只转交预分配节点; GPU 保活由执行消费者负责.
    class OpenGLGraphicsPipeline final : public RefCounter<RHI::GraphicsPipeline>
    {
    public:
        /// @brief 先保存固定描述和计划并预留 Program 删除节点, 原生名称由 Device 工厂随后填写.
        OpenGLGraphicsPipeline(OpenGLDevice *device, const GraphicsPipelineDesc &desc, const FramebufferInfo &info,
                               PipelineBindingPlan plan);
        ~OpenGLGraphicsPipeline() override;
        [[nodiscard]] const GraphicsPipelineDesc &GetDesc() const override
        {
            return m_Desc;
        }
        [[nodiscard]] const FramebufferInfo &GetFramebufferInfo() const override
        {
            return m_Info;
        }
        [[nodiscard]] const PipelineBindingPlan &Plan() const noexcept
        {
            return m_Plan;
        }
        [[nodiscard]] OpenGLDevice *Owner() const noexcept
        {
            return m_Device.Get();
        }
        [[nodiscard]] GLuint Name() const noexcept
        {
            return m_Deletion->name;
        }

    private:
        RefCountPtr<OpenGLDevice> m_Device;
        GraphicsPipelineDesc m_Desc;
        FramebufferInfo m_Info;
        PipelineBindingPlan m_Plan;
        std::unique_ptr<NativeDeletion> m_Deletion;
        bool m_Published = false;
        friend class OpenGLDevice;
    };
} // namespace Physara::RHI::OpenGL
