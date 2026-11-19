#pragma once

/// @file
/// @brief 从已提交 Scene 提取帧槽内的只读对象、几何、材质引用和灯.
/// 资产正文由固定 CPU 版本保活, 每个 View 可独立索引同一份记录.

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
#include <variant>
#include <vector>

#include <Scene/Scene.hpp>

namespace Physara::Renderer
{
    /// @brief 每个网格实体只存一个完整仿射变换. orientation 是有符号绕序, 0 表示奇异.
    struct SceneObject
    {
        Scene::EntityHandle entity;
        std::array<double, 16> worldFromLocal;
        int orientation;
        std::uint64_t transformRevision{};
    };

    /// @brief 模型、对象与覆盖索引只在同一 SceneFrame 内使用.
    /// mesh/material 索引固定模型的紧凑表, sourceSlot 保持源 Primitive 顺序.
    /// materialOverride 索引帧内有效材质; 缺省时直接读取模型材质. 空源表面的 bounds 为空.
    struct PrimitiveRef
    {
        std::uint32_t object;
        std::uint32_t model;
        std::uint32_t mesh;
        std::uint32_t sourceSlot;
        std::uint32_t material;
        std::optional<std::uint32_t> materialOverride;
        std::optional<Scene::Bounds> bounds;
        bool visible;
        bool castsShadow;
        std::uint64_t materialRevision{}; ///< 仅实例覆盖使用, 源材质随固定模型代解释.
    };

    /// @brief 灯的世界描述. 方向灯只用 direction, 点光只用 position, 聚光灯使用二者.
    /// 位置和 range 用米, direction 为发射方向, 颜色与强度保留导入合同.
    struct LightRecord
    {
        Scene::EntityHandle entity;
        Scene::Light light;
        Asset::ModelLightUnit intensityUnit;
        std::array<double, 3> position{};
        std::array<double, 3> direction{};
        bool visible;
        std::uint64_t lightRevision{};
    };

    enum class SceneExtraction : std::uint8_t { Skipped, Reused, Updated, Rebuilt };

    /// @brief 本次共享准备的用时及实际写入条数, 一条 Primitive 的多个字段只计一次.
    struct SceneExtractionStatistics
    {
        SceneExtraction mode{};
        double seconds{};
        std::size_t objects{};
        std::size_t primitives{};
        std::size_t lights{};
    };

    /// @brief 借用同一安全帧槽中的连续记录, 仅在该槽本次使用内有效.
    /// @note 读资产正文用 models 中的固定版本, 覆盖中借用的模型图像也由该表保活.
    /// 活动 Scene 可在提取后被替换, 但不能据此延长本值对槽内数组的借用期.
    struct SceneFrame
    {
        Scene::SceneId scene;
        std::uint64_t contentRevision = 0;
        std::span<const SceneObject> objects;
        std::span<const PrimitiveRef> primitives;
        std::span<const LightRecord> lights;
        std::span<const Asset::ModelVersion> models;
        std::span<const Scene::EffectiveMaterial> materialOverrides;
        std::optional<Scene::Bounds> drawableBounds;
        std::optional<Scene::SceneEnvironment> environment; ///< 按值复制查询参数并持有源版本, 与上方的槽内 span 寿命独立.
        std::uint64_t structureRevision{};
        bool hasGeometry{}; ///< 源几何范围存在, 包括隐藏或被当前相机剔除的图元.
        SceneExtractionStatistics extraction;
    };

    /// @brief 一个 FrameSlot 的拥有型提取存储, 独立于每次图尝试的 arena.
    class SceneFrameStorage final
    {
    public:
        SceneFrameStorage() = default;
        SceneFrameStorage(const SceneFrameStorage &) = delete;
        SceneFrameStorage &operator=(const SceneFrameStorage &) = delete;

        /// @brief 合并新提交的变更. 可在槽在途时调用, 只更新分开存储的待消费索引.
        /// @note 结构不匹配的槽仅标记重建. 调用不保留 Scene 借用, 不分配存储.
        void Observe(const Scene::RenderChanges &changes) noexcept;

        /// @brief 复用、增量更新或重建记录, 成功后交付一份完整只读快照.
        /// @pre 槽已安全取得. 旧借用须在换 Scene 或内容版本前结束.
        /// @return 成功时数组借用本槽, 环境随返回值固定. 失败时交付原因并清除部分提取结果.
        /// @note 失败清除本槽的旧缓存及资产引用, 保留数组容量; 其它槽不受影响.
        [[nodiscard]] std::variant<SceneFrame, Core::BoundedText> Prepare(const Scene::Scene &scene);

    private:
        /// @brief 一个结构代中的实体映射. 网格有连续的源 Primitive 区间, 纯灯实体只有灯索引.
        /// pending 合并实体级修改, 单槽材质修改另以 Primitive 下标去重.
        struct EntityIndices
        {
            Scene::EntityHandle entity;
            std::optional<std::uint32_t> object;
            std::optional<std::uint32_t> light;
            std::uint32_t firstPrimitive{};
            std::uint32_t primitiveCount{};
            std::uint8_t pending{};
        };

        /// @brief 使缓存身份失效并释放记录中的引用, 不归还连续数组的容量.
        void Clear() noexcept;
        void Rebuild(const Scene::Scene &scene);
        void Update(const Scene::Scene &scene, SceneExtractionStatistics &statistics);
        void UpdateMaterial(std::uint32_t index, const Scene::EntityView &entity);
        void UpdateBounds(std::uint32_t object, std::optional<Scene::Bounds> bounds) noexcept;

        Scene::SceneId m_Scene;
        // Content 记录已写入数组的版本. Observed 记录已收到的终点, 槽在途时可先于数组推进.
        std::uint64_t m_ContentRevision = 0;
        std::uint64_t m_StructureRevision = 0;
        std::uint64_t m_ObservedRevision = 0;
        bool m_Rebuild = true;
        bool m_HasGeometry = false;
        std::vector<SceneObject> m_Objects;
        std::vector<PrimitiveRef> m_Primitives;
        std::vector<LightRecord> m_Lights;
        std::vector<Asset::ModelVersion> m_Models;
        std::vector<Scene::EffectiveMaterial> m_MaterialOverrides;
        std::vector<std::uint32_t> m_FreeOverrides;
        std::vector<std::optional<Scene::Bounds>> m_Bounds; ///< 叶从 objects.size() 开始, 根在下标 1.
        std::unordered_map<std::uint32_t, EntityIndices> m_Indices;
        std::vector<EntityIndices *> m_PendingEntities;
        std::vector<std::uint8_t> m_MaterialPending;
        std::vector<std::uint32_t> m_PendingMaterials;
    };
}
