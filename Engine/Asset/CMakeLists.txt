# CPU 资产服务拥有图片、环境和静态模型解码、内容版本和作业发布. Core 的值类型进入公开合同.
# stb 只提供私有头搜索路径, 实现宏限定在 ImageDecode.cpp, 不把解码库类型传播给消费者.
# TinyGLTF 的 C11 元数据/原字段和 MikkTSpace 回调只由 Gltf 实现消费, 公共记录不暴露其类型.
# StaticTangents 复用同一 CPU 几何算法供有效 UV 覆盖调用, Asset 仍不依赖 Renderer 或 RHI.
add_library(PhysaraAsset STATIC
    AssetManager.hpp AssetManager.cpp
    ByteBudget.hpp
    ImageDecode.hpp ImageDecode.cpp
    EnvironmentDecode.hpp EnvironmentDecode.cpp
    ModelAsset.hpp
    SourceVerification.hpp SourceVerification.cpp
    GltfDocument.hpp GltfDocument.cpp
    GltfScene.hpp GltfScene.cpp
    GltfResources.hpp GltfResources.cpp
    GltfData.hpp GltfData.cpp
    GltfAccessor.hpp GltfAccessor.cpp
    GltfGeometry.hpp GltfGeometry.cpp
    GltfTangents.hpp GltfTangents.cpp StaticTangents.hpp
    GltfImport.hpp GltfImport.cpp
)
# TinyEXR 只由环境解码实现消费, 对外发布自持浮点像素.
target_link_libraries(PhysaraAsset PUBLIC PhysaraCore PRIVATE stb tinygltf mikktspace tinyexr)
target_compile_features(PhysaraAsset PUBLIC cxx_std_20)
set_target_properties(PhysaraAsset PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(PhysaraAsset PRIVATE /W4 /WX /utf-8 /permissive-)
