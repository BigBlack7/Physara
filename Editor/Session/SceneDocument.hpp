#pragma once

/// @file
/// @brief 场景 v1 编解码与文件作业交换拥有型记录. JSON 类型留在实现中.

#include <filesystem>
#include <span>
#include <stop_token>
#include <string>
#include <vector>

#include <Core/ResourceIO.hpp>
#include <Renderer/View.hpp>

namespace Physara::Editor::Detail
{
    inline constexpr std::size_t DocumentByteLimit = 16 * 1024 * 1024;
    inline constexpr int DocumentDepthLimit = 32;
    inline constexpr std::size_t DocumentNodeLimit = 2'000'000;

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

    /// @brief 已校验格式的记录. 文件引用表定位尚未绑定的 model / image 字段.
    struct ParsedSceneDocument
    {
        std::vector<Scene::EntityRecord> entities;
        Renderer::ViewDocumentRequests view;
        std::vector<ModelFileReference> models;
        std::vector<ImageFileReference> images;
    };

    /// @brief 只接受项目相对目录和 .scene.json 后缀. Core 在 I/O 时确认物理位置.
    [[nodiscard]] std::variant<Core::ResourcePath, Core::ResourceFailure>
        ParseScenePath(const std::filesystem::path &path);

    /// @brief 冷路径验证文件预算、类型、ID 和层级. 失败抛出自持的有界错误, 分配错误可传播.
    [[nodiscard]] ParsedSceneDocument DecodeSceneDocument(std::span<const std::byte> bytes, std::stop_token stop);
    /// @brief 从 Scene/View 的自持值写出 v1. 超预算或无法编码时抛出自持的有界错误.
    [[nodiscard]] std::string EncodeSceneDocument(std::span<const Scene::EntityRecord> entities,
                                                   const Renderer::ViewDocumentRequests &view,
                                                   std::stop_token stop);
}
