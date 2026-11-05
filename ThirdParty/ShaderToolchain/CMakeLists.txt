# Shader 工具链只编译当前实际使用的源码, 不加载上游工程或创建测试/安装/CLI 目标.
# 固定版本头与语法表作为源码快照保留, 普通构建不运行 Python 或读取外层 Git.

add_library(glslang STATIC
    ../glslang/glslang/GenericCodeGen/CodeGen.cpp
    ../glslang/glslang/GenericCodeGen/Link.cpp
    ../glslang/glslang/OSDependent/Windows/ossource.cpp
    ../glslang/glslang/MachineIndependent/glslang_tab.cpp
    ../glslang/glslang/MachineIndependent/attribute.cpp
    ../glslang/glslang/MachineIndependent/Constant.cpp
    ../glslang/glslang/MachineIndependent/iomapper.cpp
    ../glslang/glslang/MachineIndependent/InfoSink.cpp
    ../glslang/glslang/MachineIndependent/Initialize.cpp
    ../glslang/glslang/MachineIndependent/IntermTraverse.cpp
    ../glslang/glslang/MachineIndependent/Intermediate.cpp
    ../glslang/glslang/MachineIndependent/ParseContextBase.cpp
    ../glslang/glslang/MachineIndependent/ParseHelper.cpp
    ../glslang/glslang/MachineIndependent/PoolAlloc.cpp
    ../glslang/glslang/MachineIndependent/RemoveTree.cpp
    ../glslang/glslang/MachineIndependent/Scan.cpp
    ../glslang/glslang/MachineIndependent/ShaderLang.cpp
    ../glslang/glslang/MachineIndependent/SpirvIntrinsics.cpp
    ../glslang/glslang/MachineIndependent/SymbolTable.cpp
    ../glslang/glslang/MachineIndependent/Versions.cpp
    ../glslang/glslang/MachineIndependent/intermOut.cpp
    ../glslang/glslang/MachineIndependent/limits.cpp
    ../glslang/glslang/MachineIndependent/linkValidate.cpp
    ../glslang/glslang/MachineIndependent/parseConst.cpp
    ../glslang/glslang/MachineIndependent/reflection.cpp
    ../glslang/glslang/MachineIndependent/preprocessor/Pp.cpp
    ../glslang/glslang/MachineIndependent/preprocessor/PpAtom.cpp
    ../glslang/glslang/MachineIndependent/preprocessor/PpContext.cpp
    ../glslang/glslang/MachineIndependent/preprocessor/PpScanner.cpp
    ../glslang/glslang/MachineIndependent/preprocessor/PpTokens.cpp
    ../glslang/glslang/MachineIndependent/propagateNoContraction.cpp
    ../glslang/SPIRV/GlslangToSpv.cpp
    ../glslang/SPIRV/InReadableOrder.cpp
    ../glslang/SPIRV/Logger.cpp
    ../glslang/SPIRV/SpvBuilder.cpp
    ../glslang/SPIRV/SpvPostProcess.cpp
    ../glslang/SPIRV/doc.cpp
    ../glslang/SPIRV/SpvTools.cpp
    ../glslang/SPIRV/disassemble.cpp
)

add_library(glslang-default-resource-limits STATIC
    ../glslang/glslang/ResourceLimits/ResourceLimits.cpp
)

add_library(SPIRV-Tools-static STATIC
    ../spirv-tools/source/util/bit_vector.cpp
    ../spirv-tools/source/util/parse_number.cpp
    ../spirv-tools/source/util/string_utils.cpp
    ../spirv-tools/source/assembly_grammar.cpp
    ../spirv-tools/source/binary.cpp
    ../spirv-tools/source/diagnostic.cpp
    ../spirv-tools/source/disassemble.cpp
    ../spirv-tools/source/ext_inst.cpp
    ../spirv-tools/source/extensions.cpp
    ../spirv-tools/source/libspirv.cpp
    ../spirv-tools/source/name_mapper.cpp
    ../spirv-tools/source/opcode.cpp
    ../spirv-tools/source/operand.cpp
    ../spirv-tools/source/parsed_operand.cpp
    ../spirv-tools/source/print.cpp
    ../spirv-tools/source/software_version.cpp
    ../spirv-tools/source/spirv_endian.cpp
    ../spirv-tools/source/spirv_target_env.cpp
    ../spirv-tools/source/spirv_validator_options.cpp
    ../spirv-tools/source/table.cpp
    ../spirv-tools/source/table2.cpp
    ../spirv-tools/source/text.cpp
    ../spirv-tools/source/text_handler.cpp
    ../spirv-tools/source/to_string.cpp
    ../spirv-tools/source/val/validate.cpp
    ../spirv-tools/source/val/validate_adjacency.cpp
    ../spirv-tools/source/val/validate_annotation.cpp
    ../spirv-tools/source/val/validate_arithmetics.cpp
    ../spirv-tools/source/val/validate_atomics.cpp
    ../spirv-tools/source/val/validate_barriers.cpp
    ../spirv-tools/source/val/validate_bitwise.cpp
    ../spirv-tools/source/val/validate_builtins.cpp
    ../spirv-tools/source/val/validate_capability.cpp
    ../spirv-tools/source/val/validate_cfg.cpp
    ../spirv-tools/source/val/validate_composites.cpp
    ../spirv-tools/source/val/validate_constants.cpp
    ../spirv-tools/source/val/validate_conversion.cpp
    ../spirv-tools/source/val/validate_debug.cpp
    ../spirv-tools/source/val/validate_decorations.cpp
    ../spirv-tools/source/val/validate_derivatives.cpp
    ../spirv-tools/source/val/validate_dot_product.cpp
    ../spirv-tools/source/val/validate_extensions.cpp
    ../spirv-tools/source/val/validate_execution_limitations.cpp
    ../spirv-tools/source/val/validate_explicit_layout.cpp
    ../spirv-tools/source/val/validate_function.cpp
    ../spirv-tools/source/val/validate_graph.cpp
    ../spirv-tools/source/val/validate_group.cpp
    ../spirv-tools/source/val/validate_id.cpp
    ../spirv-tools/source/val/validate_image.cpp
    ../spirv-tools/source/val/validate_interfaces.cpp
    ../spirv-tools/source/val/validate_instruction.cpp
    ../spirv-tools/source/val/validate_layout.cpp
    ../spirv-tools/source/val/validate_literals.cpp
    ../spirv-tools/source/val/validate_logicals.cpp
    ../spirv-tools/source/val/validate_logical_pointers.cpp
    ../spirv-tools/source/val/validate_memory.cpp
    ../spirv-tools/source/val/validate_memory_semantics.cpp
    ../spirv-tools/source/val/validate_mesh_shading.cpp
    ../spirv-tools/source/val/validate_misc.cpp
    ../spirv-tools/source/val/validate_mode_setting.cpp
    ../spirv-tools/source/val/validate_non_uniform.cpp
    ../spirv-tools/source/val/validate_pipe.cpp
    ../spirv-tools/source/val/validate_primitives.cpp
    ../spirv-tools/source/val/validate_ray_query.cpp
    ../spirv-tools/source/val/validate_ray_tracing.cpp
    ../spirv-tools/source/val/validate_ray_tracing_reorder.cpp
    ../spirv-tools/source/val/validate_scopes.cpp
    ../spirv-tools/source/val/validate_small_type_uses.cpp
    ../spirv-tools/source/val/validate_tensor_layout.cpp
    ../spirv-tools/source/val/validate_tensor.cpp
    ../spirv-tools/source/val/validate_type.cpp
    ../spirv-tools/source/val/validate_invalid_type.cpp
    ../spirv-tools/source/val/basic_block.cpp
    ../spirv-tools/source/val/construct.cpp
    ../spirv-tools/source/val/function.cpp
    ../spirv-tools/source/val/instruction.cpp
    ../spirv-tools/source/val/validation_state.cpp
)

add_library(spirv-cross-core STATIC
    ../spirv-cross/spirv_cross.cpp
    ../spirv-cross/spirv_parser.cpp
    ../spirv-cross/spirv_cross_parsed_ir.cpp
    ../spirv-cross/spirv_cfg.cpp
)

# 公共 include 只提供当前库接口, SYSTEM 将第三方诊断与第一方门槛隔离.
target_include_directories(glslang SYSTEM PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../glslang")
target_include_directories(glslang-default-resource-limits SYSTEM PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../glslang")
target_include_directories(spirv-cross-core SYSTEM PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../spirv-cross")
target_include_directories(SPIRV-Tools-static SYSTEM PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../spirv-tools/include"
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../spirv-tools"
            "${CMAKE_CURRENT_LIST_DIR}/../spirv-tools/source/generated"
            "${CMAKE_CURRENT_LIST_DIR}/../spirv-headers/include")

# 与原固定配置一致: Windows GLSL/SPIR-V, 无 HLSL/optimizer; 保留 C ABI 桥接的异常展开.
foreach(target IN ITEMS glslang glslang-default-resource-limits)
    target_compile_features(${target} PRIVATE cxx_std_17)
    target_compile_definitions(${target} PRIVATE ENABLE_SPIRV GLSLANG_OSINCLUDE_WIN32 ENABLE_OPT=0)
    target_compile_options(${target} PRIVATE /w14146 /w14308 /w14700 /w14703)
endforeach()

# 仅保留基础验证实现; Debug context 诊断及私有 PCH 沿用上游实际配置.
target_compile_features(SPIRV-Tools-static PRIVATE cxx_std_17)
target_compile_definitions(SPIRV-Tools-static PRIVATE
    _CRT_SECURE_NO_WARNINGS _SCL_SECURE_NO_WARNINGS SPIRV_WINDOWS SPIRV_COLOR_TERMINAL
    $<$<CONFIG:Debug>:SPIRV_CHECK_CONTEXT>)
target_compile_options(SPIRV-Tools-static PRIVATE /wd4800 /wd4819 /wd4251)
target_precompile_headers(SPIRV-Tools-static PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../spirv-tools/source/pch_source.h")
set_target_properties(SPIRV-Tools-static PROPERTIES OUTPUT_NAME SPIRV-Tools)

target_compile_features(spirv-cross-core PRIVATE cxx_std_11)
target_compile_options(spirv-cross-core PRIVATE /wd4267 /wd4996 $<$<CONFIG:Debug>:/bigobj>)
foreach(target IN ITEMS glslang glslang-default-resource-limits SPIRV-Tools-static spirv-cross-core)
    target_compile_options(${target} PRIVATE /EHs /GR /utf-8)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
endforeach()
