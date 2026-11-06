#include "Pipeline.hpp"

/// @file
/// @brief 使用 SPIRV-Cross 的 IR/virtual emission 扩展点生成 GL 4.6 源码, 不做字符串修补.

#include <algorithm>

#include <spirv_glsl.hpp>

namespace Physara::RHI::OpenGL
{
    /// @brief 固定 core 460 的实例索引生成, 避免上游 ARB/uniform 回退成为逐 Draw 隐式合同.
    class Glsl460Compiler final : public spirv_cross::CompilerGLSL
    {
    public:
        explicit Glsl460Compiler(const std::vector<std::uint32_t> &words) : CompilerGLSL(words) {}

    protected:
        /// @brief SPIRV-Cross CompilerGLSL 的固定 virtual 协议要求保留 builtin_to_glsl 拼写.
        /// @note 上游固定 commit 为 f11ba9f0b21ba8fc15153d50a2a1ae31ab1cf8f7, 仅覆盖 InstanceIndex emission.
        /// @note support_nonzero_base_instance=false 抑制上游模拟 uniform, 非零语义由本覆盖显式提供.
        std::string builtin_to_glsl(spv::BuiltIn builtin, spv::StorageClass storage) override
        {
            return builtin == spv::BuiltInInstanceIndex ? "(gl_InstanceID + gl_BaseInstance)"
                                                        : CompilerGLSL::builtin_to_glsl(builtin, storage);
        }
    };

    /// @brief 让不同阶段同一块使用相同字段名, 字段字节布局仍保留 IR 的原始 decorations.
    static void NormalizeMembers(spirv_cross::Compiler &compiler, const spirv_cross::SPIRType &type)
    {
        for (std::uint32_t index = 0; index < type.member_types.size(); ++index)
        {
            compiler.set_member_name(type.self, index, "m" + std::to_string(index));
            const auto &member = compiler.get_type(type.member_types[index]);
            if (member.basetype == spirv_cross::SPIRType::Struct)
            {
                NormalizeMembers(compiler, member);
            }
        }
    }

    std::string LowerShader(const OpenGLShader &shader, const PipelineBindingPlan &plan)
    {
        Glsl460Compiler compiler(shader.Words());
        spirv_cross::CompilerGLSL::Options options;
        options.version = 460;
        options.es = false;
        options.vulkan_semantics = false;
        // Device 已建立 upper-left/zero-to-one, 不再套用旧 GL 的深度修正或顶点 Y 翻转.
        options.vertex.fixup_clipspace = false;
        options.vertex.flip_vert_y = false;
        options.vertex.support_nonzero_base_instance = false;
        // 当前 canonical 编译不生成源行调试信息; 若 IR 带行信息, 交给上游的行指令发射处理.
        options.emit_line_directives = true;
        compiler.set_common_options(options);
        // 先按原始 IR 身份配置 buffer, dummy 分析之后再计算活动变量并建立 combined samplers.
        const auto resources = compiler.get_shader_resources();
        for (const auto &blocks : {resources.uniform_buffers, resources.storage_buffers})
        {
            for (const auto &block : blocks)
            {
                ShaderBindingKey key{compiler.get_decoration(block.id, spv::DecorationDescriptorSet),
                                     compiler.get_decoration(block.id, spv::DecorationBinding)};
                const auto found =
                    std::ranges::find_if(plan.resources, [&](const auto &entry) { return entry.resource.key == key; });
                if (found == plan.resources.end())
                {
                    continue;
                }
                compiler.set_decoration(block.id, spv::DecorationBinding, found->unit);
                compiler.unset_decoration(block.id, spv::DecorationDescriptorSet);
                compiler.set_name(block.base_type_id, BlockName(key));
                compiler.set_name(block.id,
                                  "PhysaraResource_" + std::to_string(key.set) + "_" + std::to_string(key.binding));
                NormalizeMembers(compiler, compiler.get_type(block.base_type_id));
            }
        }
        const auto dummy = compiler.build_dummy_sampler_for_combined_images();
        compiler.set_enabled_interface_variables(compiler.get_active_interface_variables());
        compiler.build_combined_image_samplers();
        const auto key = [&](std::uint32_t id) -> ShaderBindingKey {
            return {compiler.get_decoration(id, spv::DecorationDescriptorSet),
                    compiler.get_decoration(id, spv::DecorationBinding)};
        };
        // 同一 Texture 配不同 Sampler 保留独立 unit, dummy key 不对应调用方的逻辑 sampler.
        for (const auto &combined : compiler.get_combined_image_samplers())
        {
            const ShaderSamplingPair pair{key(combined.image_id),
                                          combined.sampler_id == dummy ? ShaderBindingKey{} : key(combined.sampler_id),
                                          combined.sampler_id == dummy};
            const auto found = std::ranges::find(plan.pairs, pair, &PlannedPair::pair);
            RequirePreparation(found != plan.pairs.end(), "InterfaceMismatch: lowering pair missing from plan");
            compiler.set_name(combined.combined_id, PairName(found->unit));
            compiler.set_decoration(combined.combined_id, spv::DecorationBinding, found->unit);
        }
        return compiler.compile();
    }
} // namespace Physara::RHI::OpenGL
