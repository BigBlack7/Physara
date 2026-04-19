#include <iostream>
#include <filesystem>
#include <glm/glm.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <entt/entt.hpp>
#include <imgui/imgui.h>
#include <imgui/ImGuizmo.h>
#include <nlohmann/json.hpp>
#include <tinygltf/tiny_gltf_v3.h>

int main()
{
    glm::mat4 l(1.f);
    entt::registry registry;
    ImGui::CreateContext();
    GLFWwindow *window = glfwCreateWindow(800, 600, "Hello World", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    nlohmann::json json;
    std::cout << l[0][0] << std::endl;
    return 0;
}