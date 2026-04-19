add_executable(Physara Main.cpp)

target_link_libraries(Physara PRIVATE glm imgui glad glfw3 entt nlohmann tinygltf)