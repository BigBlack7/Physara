# Scene 拥有静态领域状态. EnTT 与矩阵求值仅供实现使用, 公共合同使用项目值类型和只读资产.
add_library(PhysaraScene STATIC
    Scene.hpp Scene.cpp
    SceneState.hpp SceneTransactions.cpp
    SceneTransform.cpp
    SceneMaterial.hpp SceneMaterial.cpp
)
target_link_libraries(PhysaraScene PUBLIC PhysaraAsset PRIVATE entt glm)
target_compile_features(PhysaraScene PUBLIC cxx_std_20)
set_target_properties(PhysaraScene PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(PhysaraScene PRIVATE /W4 /WX /utf-8 /permissive-)
