# Shader 核心不消费自己的生成头, 薄驱动与业务消费者只沿此单向依赖构建.
add_library(PhysaraShader STATIC
    Types.hpp Interface.hpp Compiler.hpp Package.hpp
    BuildSupport.hpp BuildSupport.cpp
    ShaderSchema.hpp ShaderSchema.cpp ReflectionData.cpp
    Interface.cpp CompilerEnvironment.cpp Compiler.cpp
    SourceGraph.hpp SourceGraph.cpp Spirv.hpp Spirv.cpp Package.cpp
)
target_include_directories(PhysaraShader PUBLIC "${PROJECT_SOURCE_DIR}/Engine")
target_link_libraries(PhysaraShader PUBLIC PhysaraCore PRIVATE
    PhysaraRHI nlohmann_json glslang glslang-default-resource-limits
    SPIRV-Tools-static spirv-cross-core bcrypt
)
target_compile_features(PhysaraShader PUBLIC cxx_std_20)
set_target_properties(PhysaraShader PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(PhysaraShader PRIVATE /W4 /WX /utf-8 /permissive- /EHs)
target_compile_definitions(PhysaraShader PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)

# 指纹是当前保留输入的派生值, 不从 Git、绝对路径或构建时间推断版本.
# 只在 CMake 配置时扫描当前编译链, 输入改变会触发重新配置, 不逐帧/逐次运行扫描.
set(shaderFingerprintInputs)
foreach(directory IN ITEMS glslang spirv-tools spirv-headers spirv-cross ShaderToolchain)
    file(GLOB_RECURSE dependencyInputs CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/ThirdParty/${directory}/*")
    list(APPEND shaderFingerprintInputs ${dependencyInputs})
endforeach()
file(GLOB moduleInputs CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")
list(APPEND shaderFingerprintInputs ${moduleInputs}
    "${PROJECT_SOURCE_DIR}/ThirdParty/json/single_include/nlohmann/json.hpp"
    "${PROJECT_SOURCE_DIR}/ThirdParty/json/single_include/nlohmann/json_fwd.hpp"
    "${CMAKE_CURRENT_LIST_FILE}" "${PROJECT_SOURCE_DIR}/RHI/RHI.hpp"
    "${PROJECT_SOURCE_DIR}/CMakeLists.txt" "${PROJECT_SOURCE_DIR}/Engine/CMakeLists.txt"
    "${PROJECT_SOURCE_DIR}/ThirdParty/CMakeLists.txt")
list(SORT shaderFingerprintInputs)
set(shaderFingerprint "PhysaraShaderCompilerV1\n")
string(APPEND shaderFingerprint "${CMAKE_SYSTEM_VERSION}\n${CMAKE_CXX_FLAGS}\n${CMAKE_CXX_STANDARD}\n")
string(APPEND shaderFingerprint "WindowsSDK=${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}\n")
string(APPEND shaderFingerprint "${CMAKE_MSVC_RUNTIME_LIBRARY}\n")
string(APPEND shaderFingerprint "${CMAKE_CXX_COMPILER_ID}/${CMAKE_CXX_COMPILER_VERSION}\n")
string(APPEND shaderFingerprint "${CMAKE_VS_PLATFORM_TOOLSET}\n${CMAKE_SIZEOF_VOID_P}\n")
foreach(input IN LISTS shaderFingerprintInputs)
    file(RELATIVE_PATH logicalPath "${PROJECT_SOURCE_DIR}" "${input}")
    file(SHA256 "${input}" inputHash)
    string(APPEND shaderFingerprint "${logicalPath}:${inputHash}\n")
endforeach()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${shaderFingerprintInputs})
string(SHA256 shaderInputHash "${shaderFingerprint}")
set(shaderFlagHash)
foreach(configuration IN LISTS CMAKE_CONFIGURATION_TYPES)
    string(TOUPPER "${configuration}" upperConfiguration)
    string(SHA256 flagHash "${CMAKE_CXX_FLAGS_${upperConfiguration}}")
    string(APPEND shaderFlagHash "$<$<CONFIG:${configuration}>:${flagHash}>")
endforeach()
set(shaderBuildIdentity "#pragma once\n#include <string_view>\nnamespace Physara::Shader\n{\n")
string(APPEND shaderBuildIdentity "    inline constexpr std::string_view CompilerInputHash = \"${shaderInputHash}\";\n")
string(APPEND shaderBuildIdentity "    inline constexpr std::string_view CompilerFlagHash = \"${shaderFlagHash}\";\n")
string(APPEND shaderBuildIdentity "    inline constexpr std::string_view BuildConfiguration = \"$<CONFIG>\";\n}\n")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/Private/$<CONFIG>/BuildIdentity.hpp" CONTENT "${shaderBuildIdentity}")
target_include_directories(PhysaraShader PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/Private/$<CONFIG>")

add_executable(PhysaraShaderBuild BuildDriver.cpp)
target_link_libraries(PhysaraShaderBuild PRIVATE PhysaraShader nlohmann_json)
target_compile_options(PhysaraShaderBuild PRIVATE /W4 /WX /utf-8 /permissive-)
set_target_properties(PhysaraShaderBuild PROPERTIES CXX_EXTENSIONS OFF)

# 调用方显式提供实际输入, 本模块不注册开发测试或默认样例.
# 每个接口/配置只有一个命令写入全部产物; 消费者显式依赖其生成 target.
function(physara_add_shader_interface name schema)
    get_filename_component(schema "${schema}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    set(outputRoot "${PROJECT_BINARY_DIR}/Generated/Shader/$<CONFIG>/${name}")
    set(outputs)
    foreach(file IN ITEMS Data.hpp Bindings.hpp Vertex.glsl Fragment.glsl Expected.json Manifest.json)
        list(APPEND outputs "${outputRoot}/${file}")
    endforeach()
    add_custom_command(
        OUTPUT "${outputRoot}/Interface.stamp" ${outputs}
        COMMAND PhysaraShaderBuild generate "${schema}" "${outputRoot}" "${outputRoot}/Interface.stamp"
        DEPENDS PhysaraShaderBuild "${schema}"
        COMMENT "Generating Shader interface ${name}"
        VERBATIM
    )
    # 多个程序共用此 target 生成接口. 程序规则另依赖具体输出, 用其变更决定是否重新编译.
    add_custom_target(PhysaraShaderInterface${name} DEPENDS "${outputRoot}/Interface.stamp" ${outputs})
    set(${name}Outputs ${outputs} PARENT_SCOPE)
endfunction()

# 规范选项由核心的版本化 profile 决定; 请求是配置输入, 不透传无法进入身份的 flags.
function(physara_add_shader_program name interfaceName program sourceRoot)
    get_filename_component(program "${program}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    get_filename_component(sourceRoot "${sourceRoot}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    set(shaderPackageRoot "${PROJECT_BINARY_DIR}/Shaders/$<CONFIG>")
    add_custom_command(
        # VS 的 BYPRODUCTS 不会因单独丢失 depfile 而重跑; 三份必要输出都显式列为 OUTPUT.
        # 首输出与 depfile 左侧都锚定最后发布的包, 辅助 stamp 不能覆盖包发布失败的状态.
        OUTPUT "${shaderPackageRoot}/${name}.pshader" "${shaderPackageRoot}/${name}.stamp"
               "${shaderPackageRoot}/${name}.d"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${shaderPackageRoot}"
        COMMAND PhysaraShaderBuild build "${program}"
                "${sourceRoot}" "${PROJECT_BINARY_DIR}/Generated/Shader/$<CONFIG>/${interfaceName}"
                "${shaderPackageRoot}/${name}.pshader" "${shaderPackageRoot}/${name}.d"
                "${shaderPackageRoot}/${name}.stamp"
        DEPENDS PhysaraShaderBuild "PhysaraShaderInterface${interfaceName}" ${${interfaceName}Outputs} "${program}"
        DEPFILE "${shaderPackageRoot}/${name}.d"
        COMMENT "Compiling canonical Shader package ${name}"
        VERBATIM
    )
    # 命令只挂到这个包的唯一生成 target. 消费者共享 target 前置, 包路径只用于定位数据.
    set(packageTarget "PhysaraShaderPackage${name}")
    add_custom_target(${packageTarget}
        DEPENDS "${shaderPackageRoot}/${name}.pshader" "${shaderPackageRoot}/${name}.stamp"
                "${shaderPackageRoot}/${name}.d")
    set(${name}Package "${shaderPackageRoot}/${name}.pshader" PARENT_SCOPE)
    set(${name}PackageTarget "${packageTarget}" PARENT_SCOPE)
endfunction()
