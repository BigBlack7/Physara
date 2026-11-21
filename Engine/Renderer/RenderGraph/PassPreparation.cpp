#include "GraphExecutor.hpp"

/// @file
/// @brief 由当前 Pass 的精确授权生成 RHI 视图条目, 执行回调只借用已成功创建的集合.

#include <algorithm>
#include <utility>
#include <vector>

#include "GraphRanges.hpp"

namespace Physara::Renderer
{
    PassPreparation::PassPreparation(RHI::Device &device, PassResources &resources)
        : m_Device(device), m_Resources(resources), m_Authorized(0), m_BindingSets(0)
    {
    }

    std::optional<RHI::BindingSetItem> PassPreparation::SampledTexture(
        std::uint32_t slot, TextureId id, std::optional<TextureRange> range, RHI::TextureDimension dimension)
    {
        TextureRange resolved;
        auto *texture = m_Resources.ResolveTexture(id, GraphUsage::Sampled, range, &resolved);
        if (!texture)
        {
            return std::nullopt;
        }
        // 同格式 packed 视图只交付深度分量, 与编译的普通采样及导出判定一致.
        if (!Detail::HasSameFormatSampledAspect(resolved.aspect))
        {
            m_Resources.Fail(GraphErrorCode::UnsupportedCapability, "CreateBindingSet",
                             texture->GetDesc().debugName, "Same-format sampled view cannot expose requested aspects",
                             id.version);
            return std::nullopt;
        }
        const auto item = RHI::BindingSetItem::Texture_SRV(slot, texture, texture->GetDesc().format,
                                                           Detail::RhiSubresources(resolved), dimension);
        m_Authorized.push_back(item);
        return item;
    }

    std::optional<RHI::BindingSetItem> PassPreparation::ReadBuffer(
        std::uint32_t slot, BufferId id, GraphUsage usage, std::optional<RHI::BufferRange> range)
    {
        if (usage != GraphUsage::ConstantBuffer && usage != GraphUsage::ShaderRead)
        {
            m_Resources.Fail(GraphErrorCode::InvalidUsage, "CreateBindingSet", {},
                             "Buffer binding requires ConstantBuffer or ShaderRead", id.version);
            return std::nullopt;
        }
        RHI::BufferRange resolved;
        auto *buffer = m_Resources.ResolveBuffer(id, usage, range, &resolved);
        if (!buffer)
        {
            return std::nullopt;
        }
        const auto item = usage == GraphUsage::ConstantBuffer
            ? RHI::BindingSetItem::ConstantBuffer(slot, buffer, resolved)
            : RHI::BindingSetItem::StructuredBuffer_SRV(slot, buffer, RHI::Format::UNKNOWN, resolved);
        m_Authorized.push_back(item);
        return item;
    }

    std::optional<RHI::BindingSetItem> PassPreparation::StorageImage(
        std::uint32_t slot, TextureId id, GraphUsage usage, std::optional<TextureRange> range,
        RHI::TextureDimension dimension)
    {
        if (!IsStorageUsage(usage))
        {
            m_Resources.Fail(GraphErrorCode::InvalidUsage, "CreateBindingSet", {},
                             "Image binding requires a storage declaration", id.version);
            return std::nullopt;
        }
        TextureRange resolved;
        auto *texture = m_Resources.ResolveTexture(id, usage, range, &resolved);
        if (!texture)
        {
            return std::nullopt;
        }
        // 条目携带原存储坐标. 范围内的读写权限另随图声明提交, 执行时再与活动 Shader 对照.
        const auto item = RHI::BindingSetItem::Texture_UAV(slot, texture, texture->GetDesc().format,
                                                           Detail::RhiSubresources(resolved), dimension);
        m_Authorized.push_back(item);
        return item;
    }

    std::optional<RHI::BindingSetItem> PassPreparation::StorageBuffer(
        std::uint32_t slot, BufferId id, GraphUsage usage, std::optional<RHI::BufferRange> range)
    {
        if (!IsStorageUsage(usage))
        {
            m_Resources.Fail(GraphErrorCode::InvalidUsage, "CreateBindingSet", {},
                             "Structured UAV binding requires a storage declaration", id.version);
            return std::nullopt;
        }
        RHI::BufferRange resolved;
        auto *buffer = m_Resources.ResolveBuffer(id, usage, range, &resolved);
        if (!buffer)
        {
            return std::nullopt;
        }
        const auto item = RHI::BindingSetItem::StructuredBuffer_UAV(slot, buffer, RHI::Format::UNKNOWN, resolved);
        m_Authorized.push_back(item);
        return item;
    }

    std::optional<std::size_t> PassPreparation::CreateBindingSet(
        RHI::BindingLayout &layout, std::span<const RHI::BindingSetItem> items)
    {
        if (m_Resources.m_Failure)
        {
            return std::nullopt;
        }
        // 按条目的语义字段查找本次授权的资源和范围. Sampler 由布局调用方提供, RHI 检查实际归属.
        for (const auto &item : items)
        {
            if (item.type != RHI::ResourceType::Sampler &&
                std::ranges::find(m_Authorized, item) == m_Authorized.end())
            {
                m_Resources.Fail(GraphErrorCode::AccessDenied, "CreateBindingSet", {},
                                 "Binding item was not prepared with this Pass permission");
                return std::nullopt;
            }
        }
        // 直接从授权范围构造拥有型列表, 使 MSVC Debug 的迭代器代理分配处于可抛构造路径.
        // 分配失败沿 prepare 传播, 由执行器保留先前诊断或返回 OutOfStorage.
        const RHI::BindingSetDesc desc{.bindings = std::vector<RHI::BindingSetItem>(items.begin(), items.end())};
        // 设备核对格式与布局后创建实际视图. 本 Pass 在准备成功后才会清除和执行.
        auto binding = m_Device.CreateBindingSet(desc, &layout);
        if (!binding || !m_Resources.m_Diagnostics.IsClear())
        {
            m_Resources.Fail(GraphErrorCode::ExecutionFailed, "CreateBindingSet", {}, "BindingSet creation failed");
            return std::nullopt;
        }
        // 只有工厂与诊断均成功的集合才取得索引. owner 跨回调边界保留到本 Pass 返回.
        m_BindingSets.push_back(std::move(binding));
        return m_BindingSets.size() - 1;
    }
}
