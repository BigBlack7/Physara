# Design

## Context

基准为 `v2.0@c58ecb9`，当前 T01 到 T03 已验收。导入通过 `GltfData`、`GltfGeometry` 和 `GltfScene` 完成字段、框架与能力判定。Scene 合成固定模型版本和 Primitive 稀疏覆盖。Renderer 按颜色与表面用途选择几何属性、材质槽和程序，现有扩展颜色家族可复用核心或 IOR/specular 基底。环境库已有 GGX 预滤波、DFG、SH 以及独立的 Charlie 需求。 [提案](proposal.md) · [开发包](../../../../Docs/Developments/D12-D13-Materials.md#d12-t04) · [T03 验收](../2026-11-28-add-sheen-material-profile/tasks.md#final-verification)

本次遵循几何、材质、光照与计划就绪四项合同。方向映射以固定 Khronos 规范及 schema 为准，GGX D/V 与间接光方向取锁定的 Filament 实现。 [静态几何](../../../../Docs/Architecture.md#geometry) · [标准参数](../../../../Docs/Architecture.md#materials) · [统一光照](../../../../Docs/Architecture.md#lighting) · [准备与就绪](../../../../Docs/Architecture.md#plan-readiness) · [Khronos 扩展](https://github.com/KhronosGroup/glTF/blob/7cee0fcddd5730e90d488bfb5aa2e52171483dde/extensions/2.0/Khronos/KHR_materials_anisotropy/README.md) · [Khronos schema](https://github.com/KhronosGroup/glTF/blob/7cee0fcddd5730e90d488bfb5aa2e52171483dde/extensions/2.0/Khronos/KHR_materials_anisotropy/schema/material.KHR_materials_anisotropy.schema.json)

## Goals / Non-Goals

**Goals**

- 用已有固定资产与场景实例所有权导入、编辑和持久化标准输入。沿用基底 normal 的几何选择，让变换后的各向异性轴和实际着色半球一致。
- 用同一份派生参数给直接镜面和环境查询。扩展资源跟随颜色用途，Depth、Shadow、SurfaceAux 及诊断保持各自的就绪条件。
- 界定有限产品角色及其原生验收。通过匹配实测输入和读回证明 T04，向 T06 提供本 profile 的可复验基线。

**Non-Goals**

- 清漆或 sheen 与各向异性的联合颜色家族及 D12 阶段组合矩阵归 T06，薄膜归 T05，透明及透射归 D13。
- 本 profile 的环境镜面采用既有预滤波的单次方向近似。轴相关预积分、高阶环境卷积、通用材质组合系统及扩展拾取系统属于后续能力。

## Decisions

<a id="input-and-scope"></a>
### 1. 以有效材质决定颜色家族

`ModelMaterial` 保存标准强度、角度、源声明和线性数据槽，加入类型化 `MaterialSlot` 和 `ImageUse`。采样设置沿用 `Tables::Slot` 的 glTF 解析路径、图像版本固定和外部依赖。线性 RGB 使用现有线性图像存储和 mip 规则，采样后的 RG 归一和 B 乘数由材质 Shader 应用。sampler、有效 texCoord 和 UV 仿射都是槽的属性。 [材质解析](../../../../Engine/Asset/GltfData.cpp) · [图像用途](../../../../Engine/Asset/AssetManager.hpp) · [纹理实现](../../../../Engine/Renderer/Materials/StaticTextures.cpp) · [纹理变换规范](https://github.com/KhronosGroup/glTF/blob/7cee0fcddd5730e90d488bfb5aa2e52171483dde/extensions/2.0/Khronos/KHR_texture_transform/README.md)

正的有效材质强度选择一组各向异性颜色角色，核心与 IOR/specular 共用这组包。若方向图的 B 局部为零，色角色仍由 Draw 的有效材质决定，该像素取等向基底。有效材质强度为零时，恢复当时对应的核心、IOR/specular、清漆或 sheen 角色。沿用现有清漆家族的声明/覆盖判据与 sheen 的有效颜色判据，包括已选清漆而强度为零的情况。 [当前有效材质](../../../../Engine/Scene/SceneMaterial.cpp) · [颜色和表面需求](../../../../Engine/Renderer/Draw/StaticDraw.cpp)

`GltfScene` 继续验证全文件的扩展声明，所选场景的实际可达材质负责判定能否交付。先判定标准相斥项，再按已选家族核对有限组合。必需的 anisotropy 遇到未支持组合时返回定位到实际材质的失败。可选的 anisotropy 遇到同类组合且其余已支持响应可用时，记录该材质省略了 anisotropy 并导入其余响应。被省略的方向输入没有活动采样及几何需求。正常导入的方向槽即使因子为零，也保留原始声明和值并执行适用资产校验。已发布报告显示实际 CPU 支持、适用组合及可见降级。 [扩展报告入口](../../../../Engine/Asset/GltfScene.cpp) · [导入规格](specs/static-gltf-import/spec.md)

<a id="geometry-frame"></a>
### 2. 从基底框架取得方向

- 源有合法 NORMAL/TANGENT 时保留框架和 `tangent.w`。若没有可用源框架，基底 normal 槽的有效未变换 UV 提供 MikkTSpace 输入。缺 NORMAL 的模型仍按既有规则生成逐面法线并忽略源 TANGENT。仅有方向纹理、无基底 normal 且无有效源框架的材质在导入或实例编辑边界拒绝。角点帧的生成、拆点、有效 UV 验证及几何预算复用已有路径。 [几何准备](../../../../Engine/Asset/GltfGeometry.cpp) · [MikkTSpace 适配](../../../../Engine/Asset/GltfTangents.cpp)
- 方向图和基底 normal 是同一基础切线空间中的向量数据，允许各用自己的有效 UV。方向槽的 `KHR_texture_transform` 变换取样坐标和梯度。方向分量先解码、归一，再用材质的正弧度旋转，直接在基础 T/B 内解释。基底 normal 仍以自己的采样仿射处理法线纹理方向。这样同一组显式 TANGENT 配合烘入 texCoord 的坐标或等价采样变换时，方向槽取到同一 texel 就得到同一材料方向。 [现有 UV/覆盖入口](../../../../Shaders/Material/StaticCoverage.glsl) · [方向和 UV 规格](specs/static-surface-evaluation/spec.md)
- 仅色角色读取当前框架所需的原始切线流。对没有 normal 贴图且有源切线的 Primitive 直接选择源切线流。normal 有效 UV 变更时沿用现有的源框架匹配或角点重建。色角色从局部 N/T 与 `w` 得到原始 T/B，分别施加实例线性矩阵并插值，在片元内以旋转后的方向组合两轴再正交化。这保留沿斜轴的仿射方向，负 determinant 已体现在原始轴的世界变换及既有绕序中。 [几何驻留键](../../../../Engine/Renderer/Materials/StaticGpu.hpp) · [共享顶点取用](../../../../Shaders/Geometry/StaticVertexFetch.glsl)
- 基底着色法线沿已有 normal map 与双面规则生成。将方向投影到该已定面向的着色切平面，垂直轴由该法线和投影方向重建。镜面使用的是轴，沿轴反向在 GGX 和弯曲反射中等价。映射后的 RG 退化时使用 +T 再应用材质旋转。两轴无法在片元建立时局部使用等向响应。这样可区分资产缺失框架和合法输入在个别像素退化。 [表面框架](../../../../Shaders/Geometry/SurfaceFrame.glsl)

<a id="lighting-model"></a>
### 3. 共用基底能量项和一对粗糙度

MaterialInputs 提供有效 `s` 和局部方向，SurfaceGeometry 提供面向一致的世界法线和框架，PixelParams 只保留派生的轴和微表面参数。先沿共享几何方差、粗糙度过滤与下限取得 `p`，再以 `alpha=p²`、`alphaT=mix(alpha,1,s²)`、`alphaB=alpha` 交给各向异性 GGX D 和完整的高度相关 Smith V。Fresnel、Lambert、金属混合及 direct/physical 的原有差异沿基底处理，镜面求值共用相同的半角和能量项。零强度、相等粗糙度及退化方向使用等向基底求值。 [共享粗糙度](../../../../Shaders/Material/CoreSurface.glsl) · [现有 BRDF](../../../../Shaders/BRDF/CoreBrdf.glsl) · [固定 Filament D/V](https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/shaders/src/surface_brdf.fs#L81)

IBL 以增粗方向垂直的单位轴 B 为参照，将视向投影到垂直于 B 的平面。按固定 Filament 的弯曲权重 `s × clamp(5p,0,1)` 将着色法线向该投影混合并归一，然后用其求反射向量。投影或混合退化时取原着色法线。按现有 dominant direction 处理，以 `alpha²` 将反射方向向原着色法线混合，并按现有 `p × (2−p) × maxMip` 计算 LOD。这里的 s 只用于非负弯曲幅度，双轴粗糙度始终来自 Khronos 映射。 [Filament 弯曲反射](https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/shaders/src/surface_lighting.fs#L100) · [Filament 主导方向](https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/shaders/src/surface_light_indirect.fs#L131) · [Physara 环境 Shader](../../../../Shaders/Lighting/Environment.glsl)

现有 DFG R/G、能量补偿和材质 AO 继续以原着色法线的 NoV 和基底粗糙度计算。环境 SH 使用原着色法线。活动各向异性基底按架构完整保留环境镜面，SSR 的置信度留给支持的等向基底。Direct、无环境 Physical 和含环境 Physical 复用各自既有 DFG 与照明配方。该近似可用带方向的 HDR 环境验证采样轴，以单位环境验证 split-sum 能量与粗糙度极限。 [环境规格](specs/environment-lighting/spec.md)

<a id="roles-and-delivery"></a>
### 4. 延续生成 ABI 和有限颜色目录

扩展 `StaticForward.json`，保留材质表 t5 和既有槽位，使用 0 基索引 12 及 set 2 的 t13/s13。两组 UV 行各增一个 `float4`，另增一个 `float4` 编码强度、旋转的 cos/sin 与 IOR/specular 基底标记。预计材质行从 528 B 到 576 B，按生成的 offset、stride 及绑定反射核对行比较、上传和缓存键。共享行增加 48 B，可继续走同一个材质表、行页及上传所有权。把各向异性专用的两轴插值、取样及 BRDF 保持在结构颜色变体内。等向颜色以及独立表面仍只上传并绑定其实际所需图像和顶点流。 [接口 schema](../../../../Engine/Shader/Interface/StaticForward.json) · [配方与槽掩码](../../../../Engine/Renderer/Materials/StaticGpu.hpp)

增加 12 个受光角色，全灯/Froxel × Direct/Physical/IBL × 有无阴影。正式目录由 Renderer 的 80 增至 92 个角色，合计含 Editor UI 的产品包预期由 81 增至 93。基底 Fresnel 通过数据分辨，旋转、方向纹理可选及纹理 UV 由材质值驱动。按既有封闭家族索引和 VariantKey 加入新角色，显式收紧 sheen 家族当前使用 `Count` 的尾界。Deferred 复用 Froxel 颜色程序；GBuffer 格式和 G3 编码按既有合同。 [产品目录](../../../../Engine/Shader/ProductVariants.hpp) · [Shader 准入规格](specs/shader-generation-reload/spec.md)

冷包和开发候选沿既有程序身份、ABI、阶段资源、阶段 IO 及原生绑定上限检查。候选准入包含未选角色并与 Pipeline 准备共用帧前预算。T03 实测活动与候选共驻 20,217,372 B，已接近 20 MiB 限额。以增加后的正式包集合运行真实 `SourceReload`，若超限，按实际共驻峰值与必要余量确定有界容量。缺项或失败保持旧代，合法恢复后由下一正常提交发布实际颜色身份。 [冷包与候选准备](../../../../Engine/Renderer/Paths/ForwardShaderPreparation.cpp) · [当前缓存默认值](../../../../Engine/Shader/Reload.hpp) · [既有共驻验证](../2026-11-28-add-sheen-material-profile/tasks.md#final-verification)

<a id="editing-and-routing"></a>
### 5. 用同一实例和 View 身份交付状态

实例编辑使用 Scene 类型化字段及纹理用途，通过 Inspector、EditorSession 与 v1 的既有编辑和恢复事务。保存保留稀疏字段，包括显式零和原弧度。有效配置校验覆盖方向图的图像用途、UV 与采样设置，以及基底 normal 更换后可达的框架和颜色家族。沿对应模型换源及文档事务重验相同目标。 [Scene 有效材质](../../../../Engine/Scene/SceneMaterial.cpp) · [Inspector](../../../../Editor/Panels/PhysicalInspector.cpp) · [v1 读取](../../../../Editor/Session/SceneDocumentRead.cpp) · [v1 写出](../../../../Editor/Session/SceneDocumentWrite.cpp)

色需求、辅助表面需求与仅覆盖的需求沿同一 Draw 准备入口分流。Deferred 的扩展赢面按现有次序交付基底 G1、G3=4 和 D32，ForwardExtension 等深合成。方向图及其专用属性只服务各向异性颜色，Depth/Shadow 的 MASK 仍读基色 alpha，SurfaceAux 只读基底表面所需属性和纹理。实际接受的各向异性颜色用当前 View、固定源版本、实例与 Primitive 匹配现有输出身份，等待、失败或诊断沿既有保留规则。 [Draw 准备](../../../../Engine/Renderer/Draw/StaticDraw.cpp) · [View 提交](../../../../Engine/Renderer/Paths/Forward.cpp) · [颜色与表面规格](specs/static-forward-rendering/spec.md)

## Risks / Trade-offs

| 风险 | 对应验证 |
|---|---|
| 方向纹理色彩域、UV 选择、切线角度与轴的变换顺序都能改变高光轴 | 同一标样用非对称 RGB、斜向旋转和独立镜像/非均匀变换，按几何向量及选定纹素计算参考方向 |
| 高各向异性、掠射与粗糙度下限可能暴露数值不稳定，单次 IBL 近似对细窄反射的角分布表达有限 | 用双精度 D/V 定向样点及单位环境约束极限，用方向环境和旋转对照检验实际查询方向 |
| 共用大行和增加产品包影响上传/cache 预算，错误包可能只在未选角色暴露 | 核对反射与真实设备限制，记录普通工作集资源差分并用活动代在场的真实 SourceReload 证明完整候选 |
| 可选组合降级、无切线或后续编辑可能被误报为已受光 | 同时检查导入定位、编辑拒绝、场景版本、独立表面 readback 与实际 View 正常接受票据 |

## Migration Plan

在同一产品构建中生成 Shader 接口、消费它的 CPU 二进制与全量正式包。ABI 不匹配沿现有准入拒绝。v1 读写增加可省略字段，普通文档沿现有默认和继承恢复。工作包的最终 verify 使用 B 签收的源、包和注册输入，之后同步增量并正常归档。 [A/B 交接](../../../../AGENTS.md#review-loop) · [开发计划](../../../../Docs/Developments/Developments.md#current-stage)
