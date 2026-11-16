#pragma once

/// @file
/// @brief 构建模型的标准数据表. 图片先定位输入, 在确定版本后再解码.

#include "GltfResources.hpp"
#include "ImageDecode.hpp"

#include <Asset/ModelAsset.hpp>

namespace Physara::Asset::Gltf
{
    /// @brief 只为所选场景引用的记录分配局部索引. 暂存的源字节由 Resources 持有.
    /// 所有写入都针对未发布候选, 任一阶段失败后整体丢弃, 不承诺回滚局部容器.
    class Tables final
    {
    public:
        Tables(Json root, Resources &resources, ModelRecord &record);

        /// @brief 将源材质索引或未指定材质映射到唯一局部表项, 同步收集有效纹理/图像依赖.
        [[nodiscard]] std::uint32_t Material(const Json &source);
        /// @brief 校验相机/灯并把节点暂存的源下标改为本记录索引, mesh 由 Geometry 处理.
        void PrepareNodes(std::stop_token stop);
        /// @brief 依赖指纹确认变化后逐图解码, 再补充基础颜色 alpha 描述; 失败由候选析构统一释放像素.
        void DecodeImages(const ImportLimits &limits, const std::shared_ptr<PixelBudget> &budget,
                          std::stop_token stop);

    private:
        [[nodiscard]] std::uint32_t Texture(const Json &source);
        [[nodiscard]] std::uint32_t Sampler(const Json &source);
        [[nodiscard]] ModelTextureSlot Slot(const Json &field, ImageUse usage);
        [[nodiscard]] ModelMaterial ReadMaterial(const Json &field);

        Json m_Root;
        Resources &m_Resources;
        ModelRecord &m_Record;
        std::map<std::optional<std::uint32_t>, std::uint32_t> m_Materials;
        std::map<std::optional<std::uint32_t>, std::uint32_t> m_Samplers;
        std::map<std::uint32_t, std::uint32_t> m_Textures;
        std::map<std::uint32_t, std::uint32_t> m_Images;
        std::vector<std::pair<std::uint32_t, ImageInput>> m_ImageInputs;
    };
}
