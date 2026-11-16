#pragma once

/// @file
/// @brief 将所选 mesh 的已验证 accessor 计划转换为本记录独有的三角形几何.

#include "GltfAccessor.hpp"
#include "GltfData.hpp"

namespace Physara::Asset::Gltf
{
    /// @brief 仅在当前解码作业内存在的 Primitive 计划. 视图借用 Resources, 输出另行分配.
    struct PrimitivePlan
    {
        Json field;
        std::uint32_t material;
        std::uint32_t mode;
        std::optional<std::uint32_t> normalUv;
        Accessor position;
        std::optional<Accessor> normal;
        std::optional<Accessor> tangent;
        std::array<std::optional<Accessor>, 2> uv;
        std::optional<Accessor> color;
        std::optional<Accessor> index;
    };

    /// @brief 先从所选节点解析唯一 mesh 和实际依赖, 再在内容指纹变化时组装拥有型几何.
    class Geometry final
    {
    public:
        /// @brief 收集所有几何/材质依赖, 将节点 mesh 引用从源下标重映射到将要构建的局部表.
        /// @note 计划借用 JSON 和资源快照; 构造成功仍不表示元素值和最终表面已经校验完成.
        Geometry(const Json &root, Resources &resources, Tables &tables, ModelRecord &record, std::stop_token stop);

        /// @brief 在单模型工作额度内构建表面, 为实际保留的几何取得共享额度.
        /// @pre 仅调用一次, retainedBytes 由候选的 RAII owner 持有并在候选/最终记录销毁时归还.
        /// @note 失败允许部分写入候选和记账, 调用者必须整体释放, 不能发布或重试该候选.
        void Build(std::size_t limit, ByteBudget &budget, std::size_t &retainedBytes);

    private:
        struct MeshPlan
        {
            std::uint32_t source;
            std::vector<PrimitivePlan> primitives;
        };

        std::vector<MeshPlan> m_Meshes;
        ModelRecord &m_Record;
        std::stop_token m_Stop;
    };
}
