#include "GltfAccessor.hpp"

/// @file
/// @brief 将已受控的 buffer 切片解释为核心属性元素, 统一处理对齐、稀疏替换和小端数值.

#include <bit>
#include <cassert>
#include <cmath>
#include <optional>

namespace Physara::Asset::Gltf
{
    static constexpr std::uint32_t U8 = 5121, U16 = 5123, U32 = 5125, F32 = 5126;

    /// @brief 返回本 profile 消费的组件字节宽度, 其它枚举由语义校验拒绝.
    static std::uint32_t ByteWidth(std::uint32_t type) noexcept
    {
        switch (type)
        {
        case U8:
            return 1;
        case U16:
            return 2;
        case U32:
        case F32:
            return 4;
        default:
            return 0;
        }
    }

    /// @brief 从已校验范围读取一到四字节的小端整数, 不依赖宿主指针对齐或别名转换.
    static std::uint32_t LittleEndian(const std::byte *data, std::uint32_t width) noexcept
    {
        std::uint32_t value = 0;
        for (std::uint32_t i = 0; i < width; ++i)
        {
            value |= std::to_integer<std::uint32_t>(data[i]) << (8 * i);
        }
        return value;
    }

    /// @brief 在已验证 bufferView 上核对完整的元素范围, 分别验证相对及绝对组件对齐.
    static std::span<const std::byte> Elements(const Json &field, ByteView view, std::uint32_t count,
                                               std::uint32_t elementBytes, std::uint32_t stride,
                                               std::uint32_t alignment, bool vertex)
    {
        const auto fieldOffset = field.Field("byteOffset");
        const auto offset = fieldOffset ? fieldOffset.Unsigned() : 0;
        if (offset % alignment != 0 || view.offset % alignment != 0 ||
            (vertex && (offset % 4 != 0 || (count > 1 && stride % 4 != 0))))
        {
            field.Error("Misaligned accessor offset or stride");
        }
        const auto bytes = view.bytes.size();
        if (stride < elementBytes || offset > bytes || elementBytes > bytes - offset ||
            count - 1 > (bytes - offset - elementBytes) / stride)
        {
            field.Error("Accessor elements exceed their bufferView or stride");
        }
        const auto size = static_cast<std::size_t>(count - 1) * stride + elementBytes;
        return view.bytes.subspan(static_cast<std::size_t>(offset), size);
    }

    /// @brief 将角色允许的类型/归一化组合映射到分量数, 不将量化扩展冒充核心属性支持.
    static std::uint32_t Components(const Json &field, Attribute attribute, std::uint32_t type, bool normalized)
    {
        const auto shape = field.Field("type").Text();
        const bool floats = type == F32 && !normalized;
        const bool unitIntegers = (type == U8 || type == U16) && normalized;
        switch (attribute)
        {
        case Attribute::Position:
        case Attribute::Normal:
            if (floats && shape == "VEC3")
            {
                return 3;
            }
            break;
        case Attribute::Tangent:
            if (floats && shape == "VEC4")
            {
                return 4;
            }
            break;
        case Attribute::TexCoord:
            if ((floats || unitIntegers) && shape == "VEC2")
            {
                return 2;
            }
            break;
        case Attribute::Color:
            if (floats || unitIntegers)
            {
                if (shape == "VEC3")
                {
                    return 3;
                }
                if (shape == "VEC4")
                {
                    return 4;
                }
            }
            break;
        case Attribute::Index:
            if (!normalized && (type == U8 || type == U16 || type == U32) && shape == "SCALAR")
            {
                return 1;
            }
            break;
        }
        field.Error("Invalid componentType, normalized or type for the consumed semantic");
    }

    Accessor::Accessor(const Json &accessors, std::uint32_t sourceIndex, Resources &resources,
                       Attribute attribute, std::stop_token stop)
        : m_Field(accessors.At(sourceIndex)), m_Count(m_Field.Field("count").Index()),
          m_Component(m_Field.Field("componentType").Index())
    {
        if (m_Count == 0)
        {
            m_Field.Field("count").Error("Accessor must contain elements");
        }
        const auto normalized = m_Field.Field("normalized");
        m_Components = Components(m_Field, attribute, m_Component, normalized && normalized.Boolean());
        m_Width = ByteWidth(m_Component);
        const auto elementBytes = m_Width * m_Components;
        m_Stride = elementBytes;
        const auto view = m_Field.Field("bufferView");
        const auto sparse = m_Field.Field("sparse");
        if (view)
        {
            const bool vertex = attribute != Attribute::Index;
            const auto bytes = resources.View(view.Index(), vertex ? ViewRole::Vertex : ViewRole::Index,
                                              vertex ? std::optional{sourceIndex} : std::nullopt);
            if (bytes.stride != 0)
            {
                m_Stride = bytes.stride;
            }
            m_Base = Elements(m_Field, bytes, m_Count, elementBytes, m_Stride, m_Width, vertex);
        }
        else if (const auto offset = m_Field.Field("byteOffset"); offset)
        {
            offset.Error("Accessor offset requires a base bufferView");
        }

        // 缺少基底时保留 glTF 的零初值, sparse 可以覆盖选定元素.
        if (!sparse)
        {
            return;
        }
        m_SparseCount = sparse.Field("count").Index();
        if (m_SparseCount == 0 || m_SparseCount > m_Count)
        {
            sparse.Field("count").Error("Invalid sparse accessor count");
        }
        const auto indices = sparse.Field("indices");
        const auto type = indices.Field("componentType").Index();
        if (type != U8 && type != U16 && type != U32)
        {
            indices.Field("componentType").Error("Expected unsigned sparse indices");
        }
        m_SparseWidth = ByteWidth(type);
        m_SparseIndices = Elements(indices, resources.View(indices.Field("bufferView").Index(), ViewRole::Sparse),
                                    m_SparseCount, m_SparseWidth, m_SparseWidth, m_SparseWidth, false);
        const auto values = sparse.Field("values");
        m_SparseValues = Elements(values, resources.View(values.Field("bufferView").Index(), ViewRole::Sparse),
                                   m_SparseCount, elementBytes, elementBytes, m_Width, false);
        std::uint32_t previous = 0;
        for (std::uint32_t i = 0; i < m_SparseCount; ++i)
        {
            if (i % 4096 == 0)
            {
                CheckStop(stop);
            }
            const auto index = LittleEndian(m_SparseIndices.data() + static_cast<std::size_t>(i) * m_SparseWidth,
                                            m_SparseWidth);
            if (index >= m_Count || (i != 0 && index <= previous))
            {
                indices.Error("Sparse indices must be strictly increasing and smaller than accessor.count");
            }
            previous = index;
        }
    }

    /// @brief 优先查有序 sparse 替换, 未命中时使用基底; 返回空指针表示标准零初值.
    const std::byte *Accessor::Element(std::uint32_t index) const noexcept
    {
        assert(index < m_Count);
        // 在已验证的有序索引里查找替换项, 无须为稀疏数据分配稠密临时副本.
        std::uint32_t low = 0;
        std::uint32_t high = m_SparseCount;
        while (low < high)
        {
            const auto mid = low + (high - low) / 2;
            const auto candidate = LittleEndian(m_SparseIndices.data() + static_cast<std::size_t>(mid) * m_SparseWidth,
                                                m_SparseWidth);
            if (candidate == index)
            {
                return m_SparseValues.data() + static_cast<std::size_t>(mid) * m_Width * m_Components;
            }
            if (candidate < index)
            {
                low = mid + 1;
            }
            else
            {
                high = mid;
            }
        }
        return m_Base.empty() ? nullptr : m_Base.data() + static_cast<std::size_t>(index) * m_Stride;
    }

    std::array<float, 4> Accessor::Read(std::uint32_t index) const
    {
        assert(m_Component != U32);
        std::array<float, 4> out{0, 0, 0, 1};
        const auto *element = Element(index);
        for (std::uint32_t i = 0; i < m_Components; ++i)
        {
            const auto value = element ? LittleEndian(element + i * m_Width, m_Width) : 0;
            if (m_Component == F32)
            {
                out[i] = std::bit_cast<float>(value);
                if (!std::isfinite(out[i]))
                {
                    m_Field.Error("Accessor contains a non-finite float");
                }
            }
            else
            {
                out[i] = static_cast<float>(value) / (m_Component == U8 ? 255.0f : 65535.0f);
            }
        }
        return out;
    }

    std::uint32_t Accessor::ReadIndex(std::uint32_t index, std::uint32_t vertexCount) const
    {
        assert(m_Component != F32 && m_Components == 1);
        const auto *element = Element(index);
        const auto result = element ? LittleEndian(element, m_Width) : 0;
        const auto reserved = 0xffffffffu >> (8 * (4 - m_Width));
        if (result >= vertexCount || result == reserved)
        {
            m_Field.Error("Index is outside the primitive vertex range or uses a reserved value");
        }
        return result;
    }
}
