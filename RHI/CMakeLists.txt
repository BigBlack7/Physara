# 通用定义形成同一正式静态库; 原生后端随真实消费者接入.
add_library(PhysaraRHI STATIC
    Common/Misc.hpp
    Common/Misc.cpp
    Common/FormatInfo.cpp
    Common/Utils.cpp
    Common/StateTracking.hpp
    Common/StateTracking.cpp
    Validation/ValidationBackend.hpp
    # 宿主短借验证列表的私有声明. 实现与包装阶段检查同属 Validation.
    Validation/CommandListInterop.hpp
    Validation/ValidationDevice.cpp
    Validation/ValidationCommandList.cpp
)

target_sources(PhysaraRHI PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/RHI.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Resource.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Containers.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ShaderTypes.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Utils.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Validation.hpp"
)

# 只传播当前模块入口和语言要求; 不暴露参考源码、业务模块或原生依赖.
target_include_directories(PhysaraRHI PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(PhysaraRHI PUBLIC cxx_std_20)
set_target_properties(PhysaraRHI PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(PhysaraRHI PRIVATE /W4 /WX /utf-8 /permissive-)
