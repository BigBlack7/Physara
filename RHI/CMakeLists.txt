# 公共合同由真实头文件组成; Common/后端尚未接入, 此目标不产生 .lib.
add_library(PhysaraRHI INTERFACE)

target_sources(PhysaraRHI INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/RHI.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Resource.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Containers.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ShaderTypes.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Utils.hpp"
)

# 只传播当前模块入口和语言要求; 不暴露参考源码、业务模块或原生依赖.
target_include_directories(PhysaraRHI INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(PhysaraRHI INTERFACE cxx_std_20)
