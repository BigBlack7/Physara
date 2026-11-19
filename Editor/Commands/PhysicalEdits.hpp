#pragma once

/// @file
/// @brief Inspector 的物理字段意图与单个待发布的来源编辑.
/// 体积较大的资产由版本引用持有, 会话在 CPU 编辑边界解析目标并提交现有历史.

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

#include "EditHistory.hpp"

namespace Physara::Editor::Detail
{
    /// @brief 源槽只由目标实体当前固定的模型版本解释.
    struct SurfaceTarget
    {
        Scene::EntityHandle entity;
        Asset::AssetId model;
        Asset::ContentRevision revision;
        std::uint32_t primitive;
        bool operator==(const SurfaceTarget &) const = default;
    };

    /// @brief 纹理槽属于指定实例的源 Primitive, 同一张图可在不同槽取得各自的采样语义.
    struct TextureTarget
    {
        SurfaceTarget surface;
        Asset::MaterialSlot slot;
        bool operator==(const TextureTarget &) const = default;
    };

    /// @brief 面板选择已知成员并交付领域值, 应用时从 owner 合并对应的一个字段.
    template <class Owner, class Value>
    struct SetField
    {
        Value Owner::*field;
        Value value;
    };

    template <class Value>
    using MaterialField = SetField<Scene::MaterialOverride, std::optional<Value>>;

    /// @brief 成员指针标识要改的材质字段, 空 optional 恢复该字段的源值.
    struct MaterialEdit
    {
        SurfaceTarget target;
        std::variant<MaterialField<float>, MaterialField<std::array<float, 3>>, MaterialField<std::array<float, 4>>,
                     MaterialField<Asset::ModelAlpha>, MaterialField<bool>> field;
    };

    struct TextureEdit
    {
        TextureTarget target;
        // 空字段恢复整个槽的继承值. sampler 子字段保持自己的成员类型.
        std::variant<std::monostate, SetField<Scene::TextureOverride, std::optional<std::uint32_t>>,
                     SetField<Scene::SamplerOverride, std::optional<std::uint32_t>>,
                     SetField<Scene::TextureOverride, std::optional<std::array<float, 2>>>,
                     SetField<Scene::TextureOverride, std::optional<float>>> field;
    };

    /// @brief 灯沿实体代定位, 命令值已采用 Scene 的米, 弧度和光度单位.
    struct LightEdit
    {
        Scene::EntityHandle target;
        std::variant<SetField<Scene::Light, double>, SetField<Scene::Light, std::array<float, 3>>,
                     SetField<Scene::Light, std::optional<double>>> field;
    };

    struct EnvironmentEdit
    {
        Scene::SceneId target;
        // 空字段移除整个请求. 旋转值采用环境空间到世界的单位 xyzw.
        std::variant<std::monostate, SetField<Scene::EnvironmentSettings, std::array<double, 4>>,
                     SetField<Scene::EnvironmentSettings, double>, SetField<Scene::EnvironmentSettings, bool>> field;
    };

    /// @brief 本批次的离散创作意图. label 指向控件的静态文字, 在提交失败时定位输入.
    struct PhysicalEdit
    {
        std::variant<MaterialEdit, TextureEdit, LightEdit, EnvironmentEdit> value;
        const char *label; ///< 固定控件名, 用于字段级诊断.
    };

    using PhysicalResourceTarget = std::variant<Scene::SceneId, TextureTarget>;

    /// @brief 路径仅在用户确认后解析. SceneId 目标表示场景环境.
    struct PhysicalResourceIntent
    {
        PhysicalResourceTarget target;
        std::string path;
        std::optional<std::uint32_t> initialUv{}; ///< 新槽只有 UV1 时的起始采样集.
    };

    enum class PhysicalResourcePhase { Idle, Pending, Applied, Inactive, Failed, Cancelled };

    /// @brief 最近一次来源编辑的状态. Applied 表示该版本仍是 Scene 请求, GPU 由 View 独立确认.
    /// Inactive 保留上次成功操作的路径, 当前采用的创作值由对应 Inspector 字段显示.
    struct PhysicalResourceStatus
    {
        PhysicalResourcePhase phase = PhysicalResourcePhase::Idle;
        PhysicalResourceTarget target{};
        Core::BoundedText path;
        Core::BoundedText reason;
    };

    /// @brief 只在 record 的当前借用期间读取稀疏槽, 不保留容器元素指针.
    [[nodiscard]] const Scene::PrimitiveOverride *FindSurfaceOverride(const Scene::EntityRecord &record,
                                                                      std::uint32_t primitive) noexcept;

    /// @brief 每个会话至多有一个待发布来源. 保留原目标和源, 等到可提交边界才与当前其它字段合并.
    class PhysicalEdits final
    {
    public:
        [[nodiscard]] const PhysicalResourceStatus &Status() const noexcept { return m_Status; }
        /// @brief 提交前由 Session 核对主 View 与文档冻结; 到域入口再次确认实体和固定模型版本.
        [[nodiscard]] std::optional<Core::BoundedText> Apply(Scene::Scene &scene, Renderer::View &view,
                                                           EditHistory &history, const PhysicalEdit &edit);
        /// @brief 应用会话的类型化 Scene 命令. 明确清除时也撤销同一目标的候选, 包括领域值已为空的情况.
        /// EditOverride 在此替换指定 Primitive 的整份覆盖. Inspector 的单字段意图由 Apply 合并.
        [[nodiscard]] std::optional<Core::BoundedText> ApplyScene(Scene::Scene &scene, Renderer::View &view,
            EditHistory &history, const Scene::Command &command);
        /// @brief 用户确认路径后替换会话候选. 准入时记住原来源和资产请求代, 拒绝原因留在 Status.
        void Begin(Asset::AssetManager &assets, const Scene::Scene &scene, const PhysicalResourceIntent &intent);
        /// @brief 在资产发布后的可编辑边界推进一次. CpuReady 且历史空闲时合入当前字段, 失败保留原创作值.
        void Advance(Asset::AssetManager &assets, Scene::Scene &scene, Renderer::View &view, EditHistory &history);
        /// @brief 编辑或历史移动后核对候选资格, 并根据实际版本更新上次成功编辑的展示.
        void RefreshSourceStatus(const Scene::Scene &scene) noexcept;
        /// @brief 只撤销会话资格, 共享资产任务及已发布内容继续由 AssetManager 管理.
        void Cancel() noexcept;
        /// @brief 文档替换或会话结束时同时释放候选引用和面板上的上一条来源记录.
        void Reset() noexcept;

    private:
        using Source = std::variant<Asset::ImageVersion, Asset::EnvironmentVersion>;

        struct Pending
        {
            Asset::AssetRequest asset;
            Source before;
            std::optional<std::uint32_t> initialUv;
        };

        /// @brief 展示历史只保存已应用版本的身份, 不延长资产寿命.
        struct SourceIdentity
        {
            Asset::AssetId asset;
            Asset::ContentRevision revision;
            bool operator==(const SourceIdentity &) const = default;
        };

        /// @brief 无结果表示目标已退役. 有结果但版本指针为空表示目标仍可接收它的首个外部源.
        [[nodiscard]] static std::optional<Source> CurrentSource(const Scene::Scene &scene,
                                                                 const PhysicalResourceTarget &target) noexcept;
        /// @brief 仅撤销仍在等待的同一来源目标. 已成功操作的展示由 RefreshSourceStatus 按当前版本更新.
        void CancelSource(const PhysicalResourceTarget &target) noexcept;
        /// @brief 退役候选并保留目标, 路径和拒绝原因, 供下一次布局显示.
        void Fail(Core::BoundedText failure) noexcept;

        PhysicalResourceStatus m_Status;
        std::optional<Pending> m_Pending;
        std::optional<SourceIdentity> m_AppliedSource;
    };
}
