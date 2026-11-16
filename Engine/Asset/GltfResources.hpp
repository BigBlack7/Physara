#pragma once

/// @file
/// @brief 记录同一工作请求实际读取的模型依赖. 提供已验证的借用字节及持久内容指纹.

#include "GltfDocument.hpp"

#include <filesystem>
#include <map>
#include <optional>
#include <vector>

#include <Core/ResourceIO.hpp>
#include <Asset/ModelAsset.hpp>

namespace Physara::Asset::Gltf
{
    /// @brief 同一 bufferView 的消费类别, 用于拒绝索引/顶点/稀疏/图像之间的混用.
    enum class ViewRole
    {
        Vertex,
        Index,
        Sparse,
        Image
    };

    /// @brief 受限到声明范围的 bufferView, offset 是其在 buffer 中的位置, stride 为零表示未声明.
    struct ByteView
    {
        std::span<const std::byte> bytes;
        std::uint64_t offset;
        std::uint32_t stride;
    };

    /// @brief 图像的未解码字节及可选 MIME 期望, 真实编码须在解码后核对.
    struct ImageInput
    {
        std::span<const std::byte> bytes;
        std::optional<ImageEncoding> encoding;
    };

    /// @brief 属于单一 worker. 对同一规范外部路径只读一次, 借用值有效到本对象销毁.
    /// Document/源快照/root/limits 须覆盖本对象寿命. 错误或取消终止整个候选, 不继续复用部分资源状态.
    class Resources final
    {
    public:
        Resources(const Document &doc, const std::filesystem::path &root, const std::filesystem::path &source,
                  const ImportLimits &limits, std::size_t sourceBytes, std::stop_token stop);

        /// @brief 按源 bufferView 下标取得切片, 在读取前核对角色、步长与声明/实际字节范围.
        /// @pre Vertex 须传入源 accessor 下标, 其它角色保持空值; 内部调用者负责该参数与角色一致.
        /// @note 无步长时只允许同一源 accessor 重复引用, 不同 accessor 的共享在 Release 同样拒绝.
        [[nodiscard]] ByteView View(std::uint32_t index, ViewRole role,
                                    std::optional<std::uint32_t> vertexAccessor = {});
        /// @brief 按源 image 下标取得 URI 或 bufferView 载荷, 检查来源互斥和 MIME 声明.
        [[nodiscard]] ImageInput Image(std::uint32_t index);
        /// @brief 对主源摘要、实际 scene 和已读取外部快照生成稳定指纹; 调用前须收集全部有效依赖.
        [[nodiscard]] Core::Sha256Digest Fingerprint(const Core::Sha256Digest &source, std::uint32_t scene) const;
        /// @brief 保留已经验证的外部路径和摘要, 供固定版本直接核对当前磁盘输入.
        /// @pre 已收集全部实际依赖; 主文件由调用者另行加入, 本入口不再次读取文件.
        void AppendInputFiles(std::vector<ModelInputFile> &inputs) const;

    private:
        struct Backing
        {
            std::vector<std::byte> owned;
            std::span<const std::byte> bytes;
            std::optional<ImageEncoding> encoding;
        };

        /// @brief 同一作业内按源 bufferView 共享的使用记录, 跨 mesh/Primitive 保留首次顶点 accessor 身份.
        struct ViewUse
        {
            std::optional<ViewRole> role;
            std::optional<std::uint32_t> packedVertexAccessor; ///< 无 byteStride 的 view 已使用的源 accessor.
        };

        void Charge(std::size_t bytes, const Json &field);
        [[nodiscard]] std::span<const std::byte> ReadExternal(const Json &uri);
        void ReadUri(const Json &uri, bool image, Backing &backing);
        [[nodiscard]] std::span<const std::byte> Buffer(std::uint32_t index);

        const Document &m_Document;
        const std::filesystem::path &m_Root;
        const std::filesystem::path m_Directory;
        const ImportLimits &m_Limits;
        std::stop_token m_Stop;
        std::size_t m_InputBytes;
        std::map<std::filesystem::path, Core::ResourceBytes> m_External;
        std::vector<Backing> m_Buffers;
        std::vector<Backing> m_Images;
        std::vector<ViewUse> m_ViewUses;
    };
}
