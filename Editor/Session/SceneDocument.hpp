#pragma once

/// @file
/// @brief 场景 v1 编解码与文件作业交换拥有型记录. JSON 类型留在实现中.

#include <array>
#include <filesystem>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <Core/ResourceIO.hpp>
#include <Renderer/View.hpp>

namespace Physara::Editor::Detail
{
    inline constexpr std::size_t DocumentByteLimit = 16 * 1024 * 1024;
    inline constexpr int DocumentDepthLimit = 32;
    inline constexpr std::size_t DocumentNodeLimit = 2'000'000;

    /// @brief v1 的 textures 对象按材质槽寻址. 下标与 Scene 的稀疏覆盖数组一致.
    inline constexpr std::array<std::string_view, Scene::MaterialSlotCount> DocumentTextureSlots{
        "baseColor", "metallicRoughness", "normal", "occlusion", "emissive", "specular", "specularColor",
        "clearcoat", "clearcoatRoughness", "clearcoatNormal"};
    /// @brief 外置图像引用另存 use. 遮蔽与金属粗糙度两个槽共用同一线性数据用途.
    inline constexpr std::array<std::pair<std::string_view, Asset::ImageUse>, 9> DocumentTextureUses{{
        {"baseColor", Asset::ImageUse::BaseColor},
        {"occlusionRoughnessMetallic", Asset::ImageUse::OcclusionRoughnessMetallic},
        {"normal", Asset::ImageUse::Normal}, {"emissive", Asset::ImageUse::Emissive},
        {"specular", Asset::ImageUse::Specular}, {"specularColor", Asset::ImageUse::SpecularColor},
        {"clearcoat", Asset::ImageUse::Clearcoat}, {"clearcoatRoughness", Asset::ImageUse::ClearcoatRoughness},
        {"clearcoatNormal", Asset::ImageUse::ClearcoatNormal}
    }};

    /// @brief 指向 entities 中实例根的未解析模型引用. scene 和摘要共同限定源节点的解释版本.
    struct ModelFileReference
    {
        std::size_t entity;
        std::filesystem::path path;
        std::uint32_t scene;
        Core::Sha256Digest version;
    };

    /// @brief 指向一个实体的源 Primitive 槽及材质纹理槽, path 始终相对项目资源根.
    struct ImageFileReference
    {
        std::size_t entity;
        std::uint32_t primitive;
        std::size_t slot;
        std::filesystem::path path;
    };

    /// @brief 可省略的场景级环境请求. Open 解析的文件路径在候选建成时由 CPU 版本接管.
    struct EnvironmentFileReference
    {
        std::filesystem::path path;
        Scene::EnvironmentSettings settings;
    };

    /// @brief 已校验格式的 v1 记录. 实体引用指向模型或覆盖图, 场景环境单独固定其源.
    struct ParsedSceneDocument
    {
        std::vector<Scene::EntityRecord> entities;
        Renderer::ViewDocumentRequests view;
        std::vector<ModelFileReference> models;
        std::vector<ImageFileReference> images;
        std::optional<EnvironmentFileReference> environment;
    };

    /// @brief 只接受项目相对目录和 .scene.json 后缀. Core 在 I/O 时确认物理位置.
    [[nodiscard]] std::variant<Core::ResourcePath, Core::ResourceFailure>
        ParseScenePath(const std::filesystem::path &path);

    /// @brief 冷路径验证文件预算、类型、ID 和层级. 失败抛出自持的有界错误, 分配错误可传播.
    [[nodiscard]] ParsedSceneDocument DecodeSceneDocument(std::span<const std::byte> bytes, std::stop_token stop);
    /// @brief 从 Scene/View 的自持值写出 v1. 超预算或无法编码时抛出自持的有界错误.
    /// @param environment 随保存快照持有的环境. 存在时写入源路径和创作值, 重开时解析届时的源.
    [[nodiscard]] std::string EncodeSceneDocument(std::span<const Scene::EntityRecord> entities,
                                                   const Renderer::ViewDocumentRequests &view,
                                                   std::stop_token stop,
                                                   const std::optional<Scene::SceneEnvironment> &environment = {});
}
