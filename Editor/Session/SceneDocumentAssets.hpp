#pragma once

/// @file
/// @brief 场景打开的去重资产请求及固定版本校验. 只有主线程接触 AssetManager.

#include "SceneDocument.hpp"

namespace Physara::Editor::Detail
{
    /// @brief 一个去重来源在本次 Open 中的准入和核对状态.
    /// resolved 固定 CPU 版本; verified 只证明本次操作取得的读快照, 不承诺文件随后不会变化.
    struct SceneDependency
    {
        std::filesystem::path path;
        std::optional<std::uint32_t> scene;
        Core::Sha256Digest version{};
        std::variant<std::monostate, Asset::ModelVersion, Asset::ImageVersion, Asset::EnvironmentVersion> resolved;
        std::optional<Asset::AssetRequest> request;
        bool verified{};
        bool refresh{}; ///< 本操作已使旧请求失效, 后继 CpuReady 含本次重新读取的证据.
        bool environment{}; ///< 非模型依赖中的独立 HDR 角色.
    };

    /// @brief Open 的阶段载荷. 主线程请求资产, worker 校验并构建, 交接期间不并发访问本对象.
    class SceneOpenInputs final
    {
    public:
        /// @brief 合并同路径和 scene 的需求, 复制可匹配的活动模型引用, 不借用 pinned 容器.
        SceneOpenInputs(ParsedSceneDocument document, std::span<const Asset::ModelVersion> pinned);

        /// @brief 非阻塞跟进准入和发布. true 表示全部就绪, false 表示后续 CPU 轮次继续.
        [[nodiscard]] bool Advance(Asset::AssetManager &assets);
        /// @brief worker 核对实际读证据. 返回待重取的依赖索引, 可直接使用的版本保留.
        /// @note 全部需求已取得 CPU 版本后调用. 文档源冲突或不可读时抛出定位到资源的有界错误.
        [[nodiscard]] std::vector<std::size_t> Verify(const std::filesystem::path &root, std::stop_token stop);
        /// @brief 主线程消费 Verify 的过期索引, 仅失效仍匹配的资产请求, 然后交回 Advance 准入.
        void Refresh(Asset::AssetManager &assets, std::span<const std::size_t> stale);
        /// @brief 所有依赖已完成时, 将固定版本交给 Scene 的类型化候选构建.
        [[nodiscard]] std::unique_ptr<Scene::Scene> Build(std::stop_token stop);
        [[nodiscard]] Renderer::ViewDocumentRequests TakeView() noexcept;

    private:
        ParsedSceneDocument m_Document;
        std::vector<SceneDependency> m_Dependencies;
        std::vector<std::size_t> m_ModelIndices;
        std::vector<std::size_t> m_ImageIndices;
        std::optional<std::size_t> m_EnvironmentIndex;
    };
}
