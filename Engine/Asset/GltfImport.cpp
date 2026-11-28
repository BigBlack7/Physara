#include "GltfImport.hpp"

/// @file
/// @brief 先收集静态模型全部有效输入并比较指纹, 内容变化后才分配几何和像素候选.

#include <algorithm>
#include <utility>

#include "GltfGeometry.hpp"
#include "GltfScene.hpp"

namespace Physara::Asset::Gltf
{
    /// @brief 保有额度跟随实际记录. 逆序析构先释放记录, 再归还该记录占用的几何容量.
    struct GeometryReservation
    {
        std::shared_ptr<ByteBudget> budget;
        std::size_t bytes = 0;

        ~GeometryReservation() { budget->Release(bytes); }
    };

    /// @brief 别名共享引用的控制块拥有记录及几何额度, 不让读者绕过记录寿命提前归还容量.
    struct ModelStorage
    {
        explicit ModelStorage(const std::shared_ptr<ByteBudget> &budget) : reservation{budget} {}

        GeometryReservation reservation;
        ModelRecord record;
    };

    std::shared_ptr<ModelRecord>
    DecodeModel(const Core::ResourceBytes &source, const std::filesystem::path &root,
                const Core::ResourcePath &path, ModelImportOptions options, const ImportLimits &limits,
                const std::optional<Core::Sha256Digest> &previous, const std::shared_ptr<ByteBudget> &pixels,
                const std::shared_ptr<ByteBudget> &geometry, std::stop_token stop)
    {
        Document document;
        document.Parse(source.bytes, limits.modelParseBytes, stop);
        const auto json = document.Root();
        auto storage = std::make_shared<ModelStorage>(geometry);
        auto &record = storage->record;
        record.source = path.Relative();
        SelectScene(json, options, stop, record);

        // 先固定层级和所有实际引用的资源. Geometry 此时只生成计划, 不展开最终顶点或解码像素.
        Resources resources(document, root, record.source, limits, source.bytes.size(), stop);
        Tables tables(json, resources, record);
        tables.PrepareNodes(stop);
        Geometry mesh(json, resources, tables, record, stop);
        record.sourceVersion = resources.Fingerprint(source.fingerprint, record.sourceScene);
        CheckStop(stop);
        if (previous == record.sourceVersion)
        {
            // 只有已选 buffer 和图像快照全部复核后才能复用, 避免重解码占用旧版本容量.
            return {};
        }

        // 完整构建后才交还候选. 任一步抛出时先销毁局部记录, 再退还已取得的几何/像素额度.
        // 依赖已在 Fingerprint 前全部固定, 文档核对信息与内容版本使用同一批真实字节.
        record.inputFiles.push_back({record.source, source.fingerprint, source.bytes.size()});
        resources.AppendInputFiles(record.inputFiles);
        mesh.Build(limits.modelGeometryBytes, *geometry, storage->reservation.bytes);
        tables.DecodeImages(limits, pixels, stop);
        if (std::ranges::any_of(record.materials, &ModelMaterial::hasAnisotropyExtension))
        {
            record.supportReport.Append(record.supportReport.View().empty() ? "" : "\n");
            record.supportReport.Append("Supported CPU extension: KHR_materials_anisotropy (metallic-roughness base)");
        }
        CheckStop(stop);
        return {std::move(storage), &record};
    }
}
