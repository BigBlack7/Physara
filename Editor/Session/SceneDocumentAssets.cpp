#include "SceneDocumentAssets.hpp"

/// @file
/// @brief 同一打开操作合并资源引用. 旧版只能在本次受控读证据匹配后参与恢复.

#include <algorithm>
#include <cassert>
#include <map>
#include <utility>

#include <Asset/SourceVerification.hpp>

namespace Physara::Editor::Detail
{
    /// @brief 将资源根相对路径附到拒绝原因, 由文件操作边界记录一次失败.
    [[noreturn]] static void RejectAsset(const std::filesystem::path &path, std::string_view reason)
    {
        const auto utf8 = path.generic_u8string();
        Core::BoundedText error(std::string_view(reinterpret_cast<const char *>(utf8.data()), utf8.size()));
        error.Append(": ");
        error.Append(reason);
        throw error;
    }

    SceneOpenInputs::SceneOpenInputs(ParsedSceneDocument document, std::span<const Asset::ModelVersion> pinned)
        : m_Document(std::move(document))
    {
        using Key = std::pair<std::filesystem::path, std::uint32_t>;
        std::map<Key, Asset::ModelVersion> published;
        for (const auto &model : pinned)
        {
            published.try_emplace({model->source, model->sourceScene}, model);
        }
        std::map<Key, std::size_t> models;
        for (const auto &file : m_Document.models)
        {
            const Key key{file.path, file.scene};
            auto [found, first] = models.try_emplace(key, m_Dependencies.size());
            if (first)
            {
                SceneDependency need{.path = file.path, .scene = file.scene, .version = file.version};
                const auto old = published.find(key);
                if (old != published.end() && old->second->sourceVersion == file.version)
                {
                    need.resolved = old->second;
                }
                m_Dependencies.push_back(std::move(need));
            }
            else if (m_Dependencies[found->second].version != file.version)
            {
                RejectAsset(file.path, "One scene document refers to conflicting model source versions");
            }
            m_ModelIndices.push_back(found->second);
        }
        std::map<std::filesystem::path, std::size_t> images;
        for (const auto &file : m_Document.images)
        {
            auto [found, first] = images.try_emplace(file.path, m_Dependencies.size());
            if (first)
            {
                m_Dependencies.push_back(SceneDependency{.path = file.path});
            }
            m_ImageIndices.push_back(found->second);
        }
    }

    /// @brief 保留请求票据和真正的 CpuReady 值. Failed 中的旧 published 不可用于打开.
    static bool AcceptStatus(SceneDependency &need, const auto &status)
    {
        if (status.request != need.request->request)
        {
            RejectAsset(need.path, "Asset request changed while opening the scene");
        }
        if (status.state == Asset::AssetState::Loading)
        {
            return false;
        }
        if (status.state != Asset::AssetState::CpuReady || !status.published)
        {
            RejectAsset(need.path, status.failure ? status.failure->description.View() :
                                                   "Required source did not become CPU ready");
        }
        need.resolved = status.published;
        // 经失效后重取的记录已在本操作中读取实际字节, 包括同内容复用的结果.
        need.verified = need.refresh;
        return true;
    }

    bool SceneOpenInputs::Advance(Asset::AssetManager &assets)
    {
        bool ready = true;
        for (auto &need : m_Dependencies)
        {
            if (!std::holds_alternative<std::monostate>(need.resolved))
            {
                continue;
            }
            if (!need.request)
            {
                const auto submitted = need.scene ? assets.RequestModel(need.path, {*need.scene}) :
                    assets.RequestImage(need.path);
                if (const auto *failure = std::get_if<Asset::AssetFailure>(&submitted))
                {
                    if (failure->reason == Asset::AssetError::QueueFull)
                    {
                        return false;
                    }
                    RejectAsset(need.path, failure->description.View());
                }
                need.request = std::get<Asset::AssetRequest>(submitted);
            }
            const auto id = need.request->asset;
            ready &= need.scene ? AcceptStatus(need, assets.StatusModel(id)) : AcceptStatus(need, assets.Status(id));
        }
        return ready;
    }

    std::vector<std::size_t> SceneOpenInputs::Verify(const std::filesystem::path &root, std::stop_token stop)
    {
        std::vector<std::size_t> stale;
        for (std::size_t i = 0; i < m_Dependencies.size(); ++i)
        {
            auto &need = m_Dependencies[i];
            const auto *model = std::get_if<Asset::ModelVersion>(&need.resolved);
            const bool expected = !model || (*model)->sourceVersion == need.version;
            if (!need.verified)
            {
                auto match = model ? Asset::VerifySource(root, **model, stop) :
                    Asset::VerifySource(root, *std::get<Asset::ImageVersion>(need.resolved), stop);
                if (match.result == Asset::SourceMatch::Unavailable)
                {
                    RejectAsset(need.path, match.detail.View());
                }
                if (match.result == Asset::SourceMatch::Changed)
                {
                    // 旧 CPU 版本恰好就是文件要求的版本时, 磁盘变化意味着文档冲突, 不能自动换绑.
                    // 只有缓存与文件要求本就不同, 或无固定版本的外置图像, 才需要重取当前源.
                    if (model && expected)
                    {
                        Core::BoundedText conflict("Model sourceVersion conflict: ");
                        conflict.Append(match.detail.View());
                        RejectAsset(need.path, conflict.View());
                    }
                    stale.push_back(i);
                    continue;
                }
                need.verified = true;
            }
            if (!expected)
            {
                RejectAsset(need.path, "Saved model sourceVersion does not match the current source");
            }
        }
        return stale;
    }

    /// @brief 已查明旧字节时才推进源代. 若其他消费者已更换票据, 重新观察其当前请求.
    void SceneOpenInputs::Refresh(Asset::AssetManager &assets, std::span<const std::size_t> stale)
    {
        for (const auto i : stale)
        {
            auto &need = m_Dependencies[i];
            assert(need.request);
            const auto request = *need.request;
            const bool stillCurrent = need.scene ?
                assets.StatusModel(request.asset).request == request.request :
                assets.Status(request.asset).request == request.request;
            if (stillCurrent)
            {
                if (auto failure = assets.InvalidateSource(request.asset))
                {
                    RejectAsset(need.path, failure->description.View());
                }
                need.refresh = true;
            }
            need.request.reset();
            need.resolved = std::monostate{};
        }
    }

    std::unique_ptr<Scene::Scene> SceneOpenInputs::Build(std::stop_token stop)
    {
        // 文件层只恢复引用位置, Scene 再按实际固定版本检查源 node, Primitive 和有效 UV.
        for (std::size_t i = 0; i < m_ModelIndices.size(); ++i)
        {
            const auto &file = m_Document.models[i];
            m_Document.entities[file.entity].model =
                std::get<Asset::ModelVersion>(m_Dependencies[m_ModelIndices[i]].resolved);
        }
        for (std::size_t i = 0; i < m_ImageIndices.size(); ++i)
        {
            const auto &file = m_Document.images[i];
            auto &overrides = m_Document.entities[file.entity].overrides;
            const auto primitive = std::ranges::lower_bound(overrides, file.primitive, {},
                                                            &Scene::PrimitiveOverride::primitiveSlot);
            assert(primitive != overrides.end() && primitive->primitiveSlot == file.primitive &&
                   primitive->textures[file.slot]);
            primitive->textures[file.slot]->image =
                std::get<Asset::ImageVersion>(m_Dependencies[m_ImageIndices[i]].resolved);
        }
        auto result = Scene::Scene::Create(m_Document.entities, stop);
        if (const auto *failure = std::get_if<Core::BoundedText>(&result))
        {
            throw *failure;
        }
        return std::get<std::unique_ptr<Scene::Scene>>(std::move(result));
    }

    Renderer::ViewDocumentRequests SceneOpenInputs::TakeView() noexcept
    {
        return std::move(m_Document.view);
    }
}
