# 唯一产品入口只装配已经实现的 Core/Platform; 原生头与检查目标不进入产品接口.
add_executable(Physara Main.cpp)
target_link_libraries(Physara PRIVATE PhysaraPlatform PhysaraCore)
target_compile_features(Physara PRIVATE cxx_std_20)
set_target_properties(Physara PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(Physara PRIVATE /W4 /WX /utf-8 /permissive-)
