#pragma once

/// @file
/// @brief Asset 私有的 C11 元数据适配. 字段视图只在 GltfDocument 和源快照存活期间使用.

#include <array>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>

#include <tiny_gltf_v3.h>
#include <tinygltf_json_c.h>

#include <Asset/AssetManager.hpp>

namespace Physara::Asset::Gltf
{
    /// @brief 私有阶段通过异常中止组装, worker 边界将自持错误交还 owner.
    struct ImportFailure
    {
        AssetFailure error;
    };

    /// @brief 合作取消的控制结果, 与格式错误分开, 不携带部分模型.
    struct ImportCancelled {};

    /// @brief 将内部导入阶段失败转为带源字段位置的有界错误.
    [[noreturn]] void Reject(AssetError reason, std::string_view location, std::string_view message);
    /// @brief 在阶段或循环边界观察取消, 请求已停止时抛出 ImportCancelled.
    void CheckStop(std::stop_token stop);

    /// @brief 已解析 JSON 的借用视图. 缺席与显式 null 不相等.
    /// 路径文字自持, 底层值借用 Document. 字段类型/数值错误抛出 ImportFailure.
    class Json final
    {
    public:
        Json(const tg3json_value *value, std::string location);

        [[nodiscard]] explicit operator bool() const noexcept { return m_Value != nullptr; }
        [[nodiscard]] std::string_view Location() const noexcept { return m_Location; }
        /// @brief 缺席父项可继续取得缺席子项, 已存在的非对象父项拒绝; 不制造默认 JSON 值.
        [[nodiscard]] Json Field(std::string_view name) const;
        /// @brief 取得已有数组元素, 类型不符或越界时拒绝; 不返回伪造的缺省元素.
        [[nodiscard]] Json At(std::size_t index) const;
        /// @brief 缺席数组的大小为零, 已存在的非数组拒绝.
        [[nodiscard]] std::size_t Size() const;
        /// @brief 严格读取所需类型; Text 借用原树, 数值必须有限且能由目标类型表达.
        [[nodiscard]] std::string_view Text() const;
        [[nodiscard]] double Number() const;
        [[nodiscard]] float Float() const;
        [[nodiscard]] bool Boolean() const;
        [[nodiscard]] std::uint32_t Index() const;
        [[nodiscard]] std::uint64_t Unsigned() const;
        /// @brief 校验存在项是对象, 缺席允许交给调用者决定是否必需.
        void Object() const;

        /// @brief 读取恰有 N 项的有限数值数组, 不截断或补齐源字段.
        template <std::size_t N>
        [[nodiscard]] std::array<double, N> Numbers() const
        {
            if (Size() != N)
            {
                Error("Unexpected array length");
            }
            std::array<double, N> values{};
            for (std::size_t i = 0; i < N; ++i)
            {
                values[i] = At(i).Number();
            }
            return values;
        }

        /// @brief 将当前字段路径并入有界失败, 终止本次导入.
        [[noreturn]] void Error(std::string_view message, AssetError reason = AssetError::InvalidModel) const;

    private:
        const tg3json_value *m_Value;
        std::string m_Location;
    };

    /// @brief 单次有界元数据解析. 规范输入保活到此对象销毁, GLB BIN 借用原快照.
    class Document final
    {
    public:
        Document() = default;
        ~Document();
        Document(const Document &) = delete;
        Document &operator=(const Document &) = delete;

        /// @brief 解析一次容器并保留同次 JSON/元数据, 检查 glTF 版本, 不读取 URI 载荷.
        /// @pre 本对象尚未解析, source 必须活到本对象销毁; budget 分别约束 JSON 树和 arena.
        /// @note 失败时只能析构本对象, 不消费部分元数据; 释放由本对象统一完成.
        void Parse(std::span<const std::byte> source, std::size_t budget, std::stop_token stop);
        [[nodiscard]] Json Root() const { return Json(&m_Json, ""); }
        [[nodiscard]] const tg3_model &Metadata() const noexcept { return m_Model; }

    private:
        tg3_model m_Model{};
        tg3_error_stack m_Errors{};
        tg3json_value m_Json{};
    };
}
