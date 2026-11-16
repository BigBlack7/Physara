#pragma once

/// @file
/// @brief glTF 几何 accessor 的已校验视图. 只借用当前导入作业的字节.

#include "GltfResources.hpp"

namespace Physara::Asset::Gltf
{
    /// @brief 当前实际消费的核心属性语义, 决定允许的类型、分量数与 normalized 组合.
    enum class Attribute
    {
        Position,
        Normal,
        Tangent,
        TexCoord,
        Color,
        Index
    };

    /// @brief 已验证角色和字节范围的 accessor, 保留有序 sparse 视图而不展开稠密副本.
    /// 仅在 Document/Resources 存活期间有效; 浮点值和索引值在实际读取时继续校验.
    class Accessor final
    {
    public:
        /// @brief 建立基底及稀疏替换视图, 校验元素范围、组件对齐和稀疏索引严格递增.
        /// @param sourceIndex 文档 accessors 数组下标, 用于读取字段及登记顶点基底的共享关系.
        Accessor(const Json &accessors, std::uint32_t sourceIndex, Resources &resources,
                 Attribute attribute, std::stop_token stop);

        [[nodiscard]] std::uint32_t Count() const noexcept { return m_Count; }
        [[nodiscard]] bool HasStorage() const noexcept { return !m_Base.empty() || m_SparseCount != 0; }
        [[nodiscard]] const Json &Field() const noexcept { return m_Field; }
        /// @brief 读取顶点属性并应用归一化/稀疏替换, 三分量属性的补充 w 为 1.
        /// @pre index < Count(), 本视图不是 Attribute::Index; 非有限源浮点抛出导入失败.
        [[nodiscard]] std::array<float, 4> Read(std::uint32_t index) const;
        /// @brief 读取原始无符号索引, 拒绝越过顶点数或命中该组件类型的保留最大值.
        /// @pre index < Count(), 本视图为 Attribute::Index.
        [[nodiscard]] std::uint32_t ReadIndex(std::uint32_t index, std::uint32_t vertexCount) const;

    private:
        [[nodiscard]] const std::byte *Element(std::uint32_t index) const noexcept;

        Json m_Field;
        std::span<const std::byte> m_Base;
        std::span<const std::byte> m_SparseIndices;
        std::span<const std::byte> m_SparseValues;
        std::uint32_t m_Count;
        std::uint32_t m_Component;
        std::uint32_t m_Components;
        std::uint32_t m_Width;
        std::uint32_t m_Stride;
        std::uint32_t m_SparseCount = 0;
        std::uint32_t m_SparseWidth = 0;
    };
}
