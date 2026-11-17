#pragma once

/// @file
/// @brief 追踪单个活动文档的模型实例意图. CPU 解码与共享请求归 Asset, 实例提交归 Scene/历史.

#include "SceneDocumentTypes.hpp"

#include <Asset/AssetManager.hpp>

namespace Physara::Renderer
{
    class View;
}

namespace Physara::Editor::Detail
{
    class EditHistory;

    /// @brief owner 线程上最多一份待实例化意图. 只跟踪准确请求代, 不拥有共享解码的取消权.
    class SceneImport final
    {
    public:
        /// @brief 在可编辑文档中准入一个实例意图, 不修改 Scene.
        /// @note 调用者已核对活动文档和冻结状态; 准入失败记录本次原因, 不使用旧 published 记录.
        [[nodiscard]] std::optional<Core::BoundedText> Begin(Asset::AssetManager &assets, Scene::SceneId scene,
            const std::filesystem::path &path, Asset::ModelImportOptions options);
        /// @brief 在 Asset 发布之后推进匹配请求, 只在历史边界空闲时提交完整实例.
        /// @pre 调用者已处理本轮文件结果, scene/view 是当前可编辑文档及其主 View.
        /// @note 换文档, 请求代失效或失败均退休本意图; 成功只生成一笔可撤销 Scene 事务.
        void Advance(Asset::AssetManager &assets, Scene::Scene &scene, Renderer::View &view, EditHistory &history);
        /// @brief 放弃当前意图的实例资格. 合并的 Asset 请求可继续完成.
        void Retire() noexcept;
        [[nodiscard]] const SceneImportStatus &Status() const noexcept { return m_Status; }

    private:
        void Fail(const Core::BoundedText &error) noexcept;

        struct Pending
        {
            Scene::SceneId scene;
            Asset::AssetRequest request;
        };

        std::optional<Pending> m_Pending;
        SceneImportStatus m_Status;
    };
}
