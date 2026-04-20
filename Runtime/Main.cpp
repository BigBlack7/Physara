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
#include <stb/stb_image.h>
#include <tinyexr/tinyexr_v3.hh>
#include <spdlog/spdlog.h>

int main()
{
    glm::mat4 l(1.f);
    entt::registry registry;
    ImGui::CreateContext();
    GLFWwindow *window = glfwCreateWindow(800, 600, "Hello World", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    nlohmann::json json;
    auto stbloader = []()
    {
        stbi_set_flip_vertically_on_load(true);
    };
    spdlog::info("Hello, {}!", "world");
    const std::vector<uint8_t> data(10, 0.f);
    tinyexr::v3::load(data);
    std::cout << l[0][0] << std::endl;
    return 0;
}