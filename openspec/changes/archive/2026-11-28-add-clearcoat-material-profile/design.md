# Design

## Context

D12-T01 的资产和 v1 编辑已经按固定源 Primitive 解析稀疏覆盖。当前生成的 `ForwardMaterial` 为 320 B，材质表在 t5，IOR/specular 在 t6/s6 与 t7/s7。受光角色在核心与 IOR/specular 间选择，前向扩展共用 SurfaceAux 和已冻结的 Draw 次序。当前静态几何只准备一个基底法线切线框架，逐灯入口也会在基底 NoL 为零时结束该灯的表面求值。双法线和新受光层需要从这两个边界接入。 [提案](proposal.md) · [生成接口](../../../../Engine/Shader/Interface/StaticForward.json) · [Draw 准备](../../../../Engine/Renderer/Draw/StaticDraw.cpp) · [逐灯入口](../../../../Shaders/Lighting/Punctual.glsl)

层与数据域按架构 8.2、8.5、8.6 执行。标准输入对照架构锁定的 Khronos 修订，DFG、GGX 及预曝光沿工程已验收的模型。项目使用清漆视向 Fresnel 配对两层与 emission。 [静态几何](../../../../Docs/Architecture.md#geometry) · [材质](../../../../Docs/Architecture.md#materials) · [光照](../../../../Docs/Architecture.md#lighting) · [Khronos clearcoat](https://github.com/KhronosGroup/glTF/blob/7cee0fcddd5730e90d488bfb5aa2e52171483dde/extensions/2.0/Khronos/KHR_materials_clearcoat/README.md)

## Goals / Non-Goals

**Goals**

- 冻结的有效材质驱动可重复的三路径颜色、覆盖和历史身份。两套法线输入分别贯通导入、实例编辑、顶点框架、光照和参考读回。
- 用明确的 GGX/DFG 近似表达清漆层和完整基底的组合。有限受光程序、可追溯绑定和独立表面足以交付本工作包。

**Non-Goals**

- D12-T03 至 T06 分别承接其余不透明材质层和阶段级组合验收。D13 承接透明覆盖及透射。 [后续工作包](../../../../Docs/Developments/D12-D13-Materials.md#d12-t03)

## Decisions

<a id="source-and-geometry"></a>
### 固定源数据与各自的切线框架

`GltfData` 解析清漆声明、两个 [0,1] 因子、法线 scale 和三个源槽。把 KHR 的同材质排斥检查接在 Unlit 专用解析之前，扩展声明加入 `GltfScene` 的 CPU 支持报告。强度和粗糙度按线性 R/G 解释，可以引用同一打包图像。法线槽沿现有线性向量与法线 mip 语义处理。图像依赖、sampler、有效 UV 和变换共用已有资产边界。CPU 保留合法清漆数据，由 Renderer 按 lit OPAQUE/MASK 交付支持，BLEND 沿用可见的透明限制。 [导入规格](specs/static-gltf-import/spec.md) · [图像语义](../../../../Docs/Architecture.md#asset-images)

带清漆法线的导入先确定源 Primitive 是否有成对的源 NORMAL/TANGENT，或者可由有效的底层 normal 槽建立切线空间。保留足以区分源框架与生成框架的几何信息，供 Scene 合成实例后按实际槽校验。例如，只有基底法线生成的切线时，清漆槽依赖有效基底法线和其可用框架。轮廓与 MASK 仍由冻结的三角形和基色 alpha 决定。

色路径在两槽使用同一有效 UV 时复用该 UV 的未变换切线基础，各自应用对应 normal 槽的 UV 变换，再完成世界变换、重正交和面向处理。UV 不同时复用现有角点切线构造，在需要两个框架的几何变体里按两组接缝的并集拆分顶点。两组手性、有效 UV 和必要的顶点布局进入现有几何身份与预算，由同一个几何所有者准备。带源切线的槽使用匹配的源框架，其 UV 以源基底 normal 槽、仅有的源清漆 normal 槽确定。需要新框架的槽使用已有 UV 的角点构造。角点或布局准备失败返回对应槽的原因。该安排允许一对任意旋转或镜像的槽，并维持原共享位置表达式。 [角点构造入口](../../../../Engine/Asset/StaticTangents.hpp) · [顶点取用](../../../../Shaders/Geometry/StaticVertexFetch.glsl) · [表面框架](../../../../Shaders/Geometry/SurfaceFrame.glsl)

基底正常着色使用基底法线。清漆使用独立清漆法线，缺席纹理时取带面向的原几何法线。几何法线和实际面向用于约束上层可见性，原有阴影查询沿用自己的几何输入。共享粗糙度过滤使用同一个几何 footprint，分别作用于基底与清漆的感知粗糙度一次。清漆所需的额外框架、UV 和顶点流按颜色消费者准备。 [表面规格](specs/static-surface-evaluation/spec.md)

### Scene 拥有有效清漆与基底身份

源声明和稀疏实例字段沿现有 `SceneMaterial` 合成，Inspector 按值提供继承与还原。v1 在 `material` 保存 `clearcoatFactor`、`clearcoatRoughnessFactor`、`clearcoatNormalScale`，在 `textures` 保存槽名及同名外部图像用途 `clearcoat`、`clearcoatRoughness`、`clearcoatNormal`。资产固定、验证、编辑提交、恢复文档和异步图像候选沿各自的现有边界。清漆材质有声明或实例覆盖时选择清漆颜色角色，其中的有效基底显式记录为核心或 IOR/specular。强度为零时层透过率为 1，保留有效声明和编辑身份。清除最后一项覆盖后按源解析角色。 [Scene 材质](../../../../Engine/Scene/SceneMaterial.cpp) · [编辑规格](specs/editor-domain-edits/spec.md) · [v1 规格](specs/scene-document-persistence/spec.md)

<a id="abi-and-roles"></a>
### 同一个生成接口与一组清漆颜色角色

扩展 `StaticForward.json` 的权威结构，生成材质行、槽位、顶点输入及阶段反射。沿用一张材质表和 t5，保留既有 t0–t4 和 t6/t7 的绑定语义，将三个清漆槽加入 t8/s8、t9/s9、t10/s10。材质行编码清漆值、各槽的 UV 变换和清漆法线基底参数。更新所有共同消费的行比较、stride、上传大小、绑定及缓存身份。材质的实际图像与采样器从同一个槽配方取得，适用的占位绑定复用当前行已经准备的合法资源。 [权威接口](../../../../Engine/Shader/Interface/StaticForward.json) · [原生配方](../../../../Engine/Renderer/Materials/StaticGpu.hpp)

受光结构增加一组 Clearcoat，覆盖 2 种灯候选方式、3 种受光配方、2 种阴影选择，共 12 个角色。Deferred 扩展重用与 Forward+ 匹配的色程序。两个基底在同一组清漆角色以材质行中的基底身份选择，复用已验收的核心和 IOR/specular 公式。核心在直接光和 DFG 有自己的响应，选择核心基底时应交付该响应。保留原有核心和 IOR/specular 程序的活动绑定。参数、采样选择、清漆槽存在性及基底组合通过材质值和实际几何布局表达。

完整环境加阴影的清漆角色预计有 13 个活动片元纹理，依据 T01 的 10 项加三个清漆槽。用编译后的各阶段反射和实机限制确认，包含清漆法线框架确实读取的顶点材质和属性。色角色、实际几何布局、绑定、所需包和 Shader 代进入同一个绘制准备及原生键。CPU 包缓存按实际编码字节验收兼容共享接口重载时的活动代与完整候选，测得当前默认 16 MiB 不足时按实测容量校正默认值，显式限额继续由同一准入规则执行。 [产品角色](../../../../Engine/Shader/ProductVariants.hpp) · [ReloadLimits](../../../../Engine/Shader/Reload.hpp) · [准入规格](specs/shader-generation-reload/spec.md)

<a id="coat-lighting"></a>
### 对应清漆视角的直接光、DFG 和发射

共享逐灯入口取得相同的入射方向、辐亮度、距离衰减与阴影。记 `Ng` 为带面向的未扰动几何法线，`Nb` 和 `Nc` 为各自着色法线，`V` 指向眼点，`L` 指向光。采用 Khronos 示例的出射角层权重，将下列值在表面准备时确定。

```text
w   = clearcoatFactor * R
p_c = FilterRoughness(clearcoatRoughnessFactor * G, geometricFootprint)
a_c = p_c * p_c
Fv  = 0.04 + 0.96 * (1 - clamp(abs(dot(Nc, V)), 0, 1))^5
S   = w * Fv
T   = 1 - S
```

对于一盏灯，基底以已有模型和自己的 NoL 求出 `B`，包含该模式的镜面补偿。清漆使用同一 GGX 分布和完整 Smith visibility，取自己的 NoV、NoL、NoH 与粗糙度。实际光线与视线同时满足几何半球和清漆半球时，清漆项为 `S × D × Vis × NoL_coat × Ccoat`。将它与 `T × B` 相加再乘该灯的入射量与共享阴影。基底 NoL 为零时基底项归零，合法的上层高光仍参与。衰减与衰减后的发射复用 `T`，`emission × T` 在工作域预曝光一次。单次散射 Direct 取 `Ccoat=1`。 [共享 GGX](../../../../Shaders/BRDF/CoreBrdf.glsl) · [现有发射入口](../../../../Shaders/Passes/Forward/Color.glsl)

Physical 按 `Nc·V` 和 `p_c` 读取已有 DFG。由于 `S` 已含视向 Fresnel，单位 Fresnel 的方向反照率取现有 DFG 的 G 分量 `A+B`，并取 `Ccoat = 1 + 0.04 × (1/G − 1)`。带灯的 Physical 直接镜面应用 `Ccoat` 一次。环境清漆项把 `S × G × Ccoat` 乘同代预滤波环境和按清漆波瓣参数算出的镜面 AO。复用现有反射方向的粗糙度修正与 LOD 映射，输入改为清漆的 `Nc` 与 `p_c`。视向须落在几何及清漆的可见半球。完整基底间接光使用各自法线、DFG 和 AO，乘 `T` 后与清漆项相加。现有环境标定和预曝光对每项应用一次。没有环境请求的 Physical 仍消费匹配的 DFG。

这套实时近似把角度权重固定于表面的出射方向。单位环境下，清漆项的积分预计为 `S × (G + 0.04 × (1−G))`，基底按其独立响应乘 `T`。配对的数值参考先以高精度半球积分求单位 Fresnel 的期望，再按相同输出量纲比较两种 roughness 和视角，以区分误用基底 DFG、重复 Fresnel/补偿、以及漏衰减 emission。 [Filament 参考与标准适配](../../../../Docs/Reference/Filament/Lighting.md) · [间接光规格](specs/environment-lighting/spec.md)

### 沿现有图和提交路径交付清漆

使用当轮有效材质选择颜色角色并以现有 `OpaqueForwardExtension` 路由承载清漆。沿 D12-T01 的冻结辅助排序，前置深度和阴影按几何与 MASK 配方，SurfaceAux 写基底法线和过滤粗糙度及 G3=4，颜色按最终 D32 等深交付，以实际当前 View 灯表、Froxel、阴影、DFG 和环境代求值。Core GBuffer 四附件且 D32 独立，屏幕读者沿用当前表面语义。材质诊断交付基底通道，包括其线性自发光输入。 [图和覆盖](../../../../Docs/Architecture.md#geometry) · [三路径规格](specs/static-forward-rendering/spec.md)

独立 OPAQUE SurfaceAux 沿 T01 已验证的 `OpaqueAuxiliary` 准备，绑定和图读取从实际存活配方确定。MASK 表面和阴影覆盖使用真实基色 alpha，清漆颜色按其需求读取清漆槽和附加几何。兼容代准备、角色错误定位、回退到可读输出和正常接受票据沿原 View 提交合同。新版有效材质经 Scene 的静态内容版本更新历史资格。 [T01 独立表面整改](../2026-11-28-add-ior-specular-material-profile/tasks.md#a-correction-2) · [View 就绪](../../../../Docs/Architecture.md#plan-readiness)

## Risks / Trade-offs

| 风险 | 处理与证据 |
|---|---|
| 两槽共用 UV 或共享源时，手性接缝和单边翻转可能在合并几何时错配 | 使用按 UV 派生的角点框架和联合接缝，保留非对称相邻样点在不同 UV、镜像与双面上的几何和光照见证 |
| 多一个清漆镜面查询可能复用错误法线、DFG 或基底近似 | 用独立积分、非对称 HDR 环境、无光发射及零强度对照判定。参考覆盖冻结设备、模型、光源和实际 FP16 工作色精度 |
| 活动材质绑定、原生布局和包大小共同影响准入 | 枚举真正可达的角色和阶段反射，确认设备限制和两代包容量。复用现有待就绪与整代拒绝回放，覆盖清漆绑定的具体故障 |
| 共面的清漆颜色与 SurfaceAux 可能选择不同的赢面 | 在同一 MASK/等深见证中配对三路径的基底 G1、G3、D32、阴影与正常色票据，沿 D11/D12-T01 的精度判据比较 |

## Migration Plan

用同一版生成接口重建 Debug、Release 产品和全部直接消费者，运行时通过现有 ABI 准入验证包与程序布局。热候选连同其完整配方通过后在帧边界发布。场景 v1 以缺省项兼容旧文档，新增稀疏字段由固定源解析。A 记录实际输入及验收，B 独立签收后进行最终验证、规格同步与归档。 [构建入口](../../../../Docs/Build.md) · [独立审查闭环](../../../../AGENTS.md#review-loop)
