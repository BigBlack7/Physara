#pragma once

/// @file
/// @brief glTF 静态模型的拥有型 CPU 数据. 索引只在所属模型版本内有效.
/// sourceIndex/sourceSlot 保留源文件身份, 其它索引指向本记录的紧凑表; 不引用解析器或 GPU 对象.

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <Asset/AssetManager.hpp>

namespace Physara::Asset
{
    /// @brief 源局部变换. matrix 存在时使用列主序仿射矩阵, 否则使用 TRS (旋转为 xyzw).
    struct ModelTransform
    {
        std::array<double, 3> translation{};
        std::array<double, 4> rotation{0, 0, 0, 1};
        std::array<double, 3> scale{1, 1, 1};
        std::optional<std::array<double, 16>> matrix;
    };

    /// @brief 网格局部空间的静态顶点, 保留源右手系和 UV 原点约定. color 是线性乘数.
    /// normal 对有效表面可用; tangent/uv 的有效性由所属 Primitive 的标志决定.
    struct ModelVertex
    {
        std::array<float, 3> position{};
        std::array<float, 3> normal{};
        std::array<float, 4> tangent{};
        std::array<std::array<float, 2>, 2> uv{};
        std::array<float, 4> color{1, 1, 1, 1};
    };

    /// @brief 有效三角形所用顶点的局部 AABB, 不包含被剔除的面或未引用顶点.
    struct ModelBounds
    {
        std::array<float, 3> minimum;
        std::array<float, 3> maximum;
    };

    /// @brief 三角形以模型局部绕序存储. 空表面保留源槽, 且不具备 bounds.
    /// indices 索引本 Primitive 的 vertices, material 索引 ModelRecord::materials.
    /// 拆顶点后源顶点下标不再稳定, 实例负缩放的绕序和 handedness 调整留给下游.
    struct ModelPrimitive
    {
        std::uint32_t sourceSlot{};
        std::uint32_t material{};
        std::array<bool, 2> hasUv{};
        bool hasTangent{};
        bool hasSourceTangent{}; ///< 源 NORMAL/TANGENT 配对有效, 可独立于基底 normal 槽提供框架.
        std::optional<ModelBounds> bounds;
        std::vector<ModelVertex> vertices;
        std::vector<std::uint32_t> indices;
    };

    /// @brief 一个被选中节点引用的源 mesh, primitives 保持源槽顺序, 多个节点可共享此表项.
    struct ModelMesh
    {
        std::uint32_t sourceIndex{};
        std::vector<ModelPrimitive> primitives;
    };

    /// @brief 保留 glTF 的采样枚举和未指定的 filter. 具体 GPU 采样器由下游创建.
    struct ModelSampler
    {
        std::optional<std::uint32_t> sourceIndex;
        std::optional<std::uint32_t> minFilter;
        std::optional<std::uint32_t> magFilter;
        std::uint32_t wrapS = 10497;
        std::uint32_t wrapT = 10497;
    };

    /// @brief 由源 image 下标去重的单层像素, 颜色域由各材质槽决定而非图像本身决定.
    struct ModelImage
    {
        std::uint32_t sourceIndex;
        Rgba8Image pixels;
    };

    /// @brief glTF texture 到本记录 image/sampler 表的关联, 不合并不同采样器的纹理身份.
    struct ModelTexture
    {
        std::uint32_t sourceIndex;
        std::uint32_t image;
        std::uint32_t sampler;
    };

    /// @brief 材质槽顺序. 扩展槽接在核心槽之后, sheen 颜色使用 sRGB RGB, 粗糙度使用线性 A.
    enum class MaterialSlot
    {
        BaseColor,
        MetallicRoughness,
        Normal,
        Occlusion,
        Emissive,
        Specular,
        SpecularColor,
        Clearcoat,
        ClearcoatRoughness,
        ClearcoatNormal,
        SheenColor,
        SheenRoughness,
        Anisotropy,
        Count
    };

    /// @brief texture 和 texCoord 引用所属模型的纹理与顶点集. UV 变换不烘进几何.
    /// texCoord 已采用 KHR_texture_transform 的覆盖值; 变换顺序为 scale, rotation, offset.
    /// domain 只描述 RGB 解释. 覆盖 A 结合 alphaMode, specular 强度和 sheen 粗糙度的 A 是独立数据.
    struct ModelTextureSlot
    {
        std::uint32_t texture{};
        std::uint32_t texCoord{};
        ImageUse use{};
        ImageDomain domain{};
        AlphaMeaning alpha{};
        std::array<float, 2> offset{};
        std::array<float, 2> scale{1, 1};
        float rotationRadians{};
    };

    enum class ModelAlpha
    {
        Opaque,
        Mask,
        Blend
    };

    /// @brief 固定源材质省略可选各向异性时的冲突层. 清漆与 sheen 都冲突时优先记录清漆.
    enum class AnisotropySourceFallback : std::uint8_t
    {
        None,
        Clearcoat,
        Sheen
    };

    /// @brief 核心材质及本阶段支持的扩展值. 因子在线性域, slots 缺席时按 glTF 缺省采样值解释.
    /// sourceIndex 缺席表示未指定材质的标准缺省; unlit 只保留参与采样的基础颜色槽.
    struct ModelMaterial
    {
        std::optional<std::uint32_t> sourceIndex;
        std::array<float, 4> baseColor{1, 1, 1, 1};
        std::array<float, 3> emissive{};
        float metallic = 1;
        float roughness = 1;
        float normalScale = 1;
        float occlusionStrength = 1;
        float emissiveStrength = 1;
        ModelAlpha alphaMode = ModelAlpha::Opaque;
        float alphaCutoff = 0.5f;
        bool doubleSided = false;
        bool unlit = false;
        std::array<std::optional<ModelTextureSlot>, static_cast<std::size_t>(MaterialSlot::Count)> slots;
        float ior = 1.5f; ///< 源中的零值保留兼容模式, 大于等于 1 的值是物理折射率.
        float specularFactor = 1;
        std::array<float, 3> specularColorFactor{1, 1, 1};
        bool hasIorExtension = false; ///< 空扩展对象仍表示启用相应材质模型.
        bool hasSpecularExtension = false;
        float clearcoatFactor{};
        float clearcoatRoughnessFactor{};
        float clearcoatNormalScale = 1;
        bool hasClearcoatExtension = false; ///< 空扩展保留声明身份, 缺省层权重为零.
        bool hasSpecularGlossinessExtension = false; ///< 核心回退仍保留用于实例扩展排斥的源声明.
        std::array<float, 3> sheenColorFactor{};
        float sheenRoughnessFactor{};
        bool hasSheenExtension = false; ///< 保留源声明, 有效颜色因子决定是否准备 sheen 受光.
        float anisotropyStrength{}; ///< [0, 1] 的源因子. 方向图 B 在片元采样后才参与强度计算.
        float anisotropyRotation{}; ///< 源弧度, 正向从切线转向副切线.
        bool hasAnisotropyExtension = false; ///< 通过组合检查的源声明, 包含零强度.
        AnisotropySourceFallback anisotropySourceFallback{}; ///< 随固定模型保留, 独立于实例覆盖和摘要容量.
    };

    /// @brief 原始透视参数, yfov 为弧度, 裁剪距离为米. 空 zfar 为无限远, 空 aspectRatio 由 View 决定.
    struct ModelPerspective
    {
        double yfov{};
        double znear{};
        std::optional<double> zfar;
        std::optional<double> aspectRatio;

        bool operator==(const ModelPerspective &) const = default;
    };

    /// @brief 正交半宽/半高和有限裁剪区间, 单位为米; 导入 profile 要求正 magnification.
    struct ModelOrthographic
    {
        double xmag{};
        double ymag{};
        double znear{};
        double zfar{};

        bool operator==(const ModelOrthographic &) const = default;
    };

    /// @brief 可由多个节点引用的源投影描述, 位姿来自引用节点, 不预生成 GPU 投影矩阵.
    struct ModelCamera
    {
        std::uint32_t sourceIndex;
        std::variant<ModelPerspective, ModelOrthographic> projection;
    };

    enum class ModelLightKind
    {
        Directional,
        Point,
        Spot
    };

    enum class ModelLightUnit
    {
        Lux,
        Candela
    };

    /// @brief 保留发布者的强度和单位. range 缺席表示不作距离截断.
    struct ModelLight
    {
        std::uint32_t sourceIndex{};
        ModelLightKind kind{};
        ModelLightUnit intensityUnit{};
        std::array<float, 3> color{1, 1, 1};
        double intensity = 1;
        std::optional<double> range;
        double innerConeAngle{};
        double outerConeAngle{};
    };

    /// @brief 所选层级中的一个源节点. children 和组件引用均为 ModelRecord 局部索引.
    /// 节点按源根/子节点顺序深度优先存储, local 相对父节点; 动画不改变初始姿态.
    struct ModelNode
    {
        std::uint32_t sourceIndex{};
        std::string name;
        ModelTransform local;
        std::optional<std::uint32_t> mesh;
        std::optional<std::uint32_t> camera;
        std::optional<std::uint32_t> light;
        std::vector<std::uint32_t> children;
    };

    /// @brief 模型源校验所需的规范相对路径、字节上界和摘要. 仅保留实际消费的输入.
    struct ModelInputFile
    {
        std::filesystem::path source;
        Core::Sha256Digest fingerprint{};
        std::size_t byteCount{};
    };

    /// @brief 源位置与局部索引均以当前记录为基准. sourceVersion 是持久指纹, 与进程内 revision 分开.
    /// source 相对项目资源根, roots 索引 nodes; 指纹含模型正文、选定 scene 和实际外部依赖快照.
    /// AssetManager 发布后仅交付 const 共享引用, 旧版本可跨请求和 manager 寿命持有.
    /// supportReport 是可能截断的支持说明, CpuReady 不表示 Scene 实例化或 GPU 驻留已经完成.
    struct ModelRecord
    {
        AssetId asset;
        ContentRevision revision;
        std::filesystem::path source;
        Core::Sha256Digest sourceVersion{};
        std::uint32_t sourceScene{};
        Core::BoundedText supportReport;
        std::vector<std::uint32_t> roots;
        std::vector<ModelNode> nodes;
        std::vector<ModelMesh> meshes;
        std::vector<ModelMaterial> materials;
        std::vector<ModelImage> images;
        std::vector<ModelTexture> textures;
        std::vector<ModelSampler> samplers;
        std::vector<ModelCamera> cameras;
        std::vector<ModelLight> lights;
        std::vector<ModelInputFile> inputFiles; ///< 首项为模型主文件, 后续为去重后的外部文件.
    };
}
