# CPU 资产服务拥有图片解码、内容版本和作业发布. Core 的值类型进入公开合同.
# stb 只提供私有头搜索路径, 实现宏限定在 ImageDecode.cpp, 不把解码库类型传播给消费者.
add_library(PhysaraAsset STATIC
    AssetManager.hpp AssetManager.cpp
    ImageDecode.hpp ImageDecode.cpp
)
target_link_libraries(PhysaraAsset PUBLIC PhysaraCore PRIVATE stb)
target_compile_features(PhysaraAsset PUBLIC cxx_std_20)
set_target_properties(PhysaraAsset PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(PhysaraAsset PRIVATE /W4 /WX /utf-8 /permissive-)
