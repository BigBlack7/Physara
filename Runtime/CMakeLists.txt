# 唯一产品入口装配诊断, 原生宿主和 CPU Editor, 按发布顺序推进输入与请求.
# Runtime 通过各模块的公开接口装配. GLFW 和 ImGui 头留在各自的私有实现.
add_executable(Physara Main.cpp)
target_link_libraries(Physara PRIVATE PhysaraPlatform PhysaraCore PhysaraEditor)
target_compile_features(Physara PRIVATE cxx_std_20)
set_target_properties(Physara PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(Physara PRIVATE /W4 /WX /utf-8 /permissive-)
