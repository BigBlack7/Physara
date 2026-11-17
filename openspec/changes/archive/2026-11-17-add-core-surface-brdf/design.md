# Design

## Context

`EncodeMaterial` 已从有效实例材质打包标准因子和槽 UV 变换。`StaticForward` 与 `StaticDepth` 共用生成接口、静态顶点入口和基色遮罩，`StaticPreview.glsl` 在颜色 Pass 中按模式解释材质字段。当前 per-view ABI 提供投影变换和预览模式。灯来自现有 `PreparedView`，实际光照消费在 D07 后续工作包接入。 [材质行](../../../../Engine/Renderer/Draw/StaticDraw.cpp) · [共享接口](../../../../Engine/Shader/Interface/StaticForward.json) · [现有预览](../../../../Shaders/Material/StaticPreview.glsl) · [本次范围](proposal.md)

## Goals / Non-Goals

共享 GLSL 交付可组合的表面参数和核心反射响应。生产 Forward 的现有预览验证纹理与表面几何链路，开发探针用显式方向与 DFG 样点验证实际 BRDF。物理相机、灯列表、正式 DFG 纹理、IBL、显示及编辑按 D07 各自的工作包交付，材质扩展与其他渲染路径按后续阶段接入。 [开发计划](../../../../Docs/Developments/D06-D07-SceneAndForward.md#dev-d07) · [行为合同](specs/static-surface-evaluation/spec.md)

## Decisions

### 1. 分离取样、表面几何与派生参数

- **Material** 从已生成的材质行和有效 UV 取样。MaterialInputs 持有线性基色与 alpha、原始感知粗糙度和金属度、AO。切线空间 normal 和自发光由同一模块按消费需求独立取样。沿用槽存在判定、UV 选择、变换、颜色格式和缺席槽默认值。颜色入口共享已经取得的基色，Unlit 通过覆盖后直接输出基色。Emissive 的 FP16 存储约束由现有预览写出边界承担。
- **Geometry** 复用静态 vertex fetch、CPU 的有效 normal UV 框架和仿射变换。共享表面准备区分逆转置后规范化的几何法线 `Ng` 与施加 normal map 的着色法线 `N`。贴图 XY 缩放、切线重正交和副切线手性按现有合同。双面在着色法线形成后按有效面向翻转一次。像素插值退化沿用几何法线回退。位置与覆盖仍由相同顶点入口和 `SampleBaseColor` / `ApplyCoverage` 决定。
- **PixelParams 与 BRDF** 消费已取样字段和准备后的几何，交付 `diffuseColor`、`F0/F90`、过滤后的 `p` 与 `alpha`。传入单位方向和匹配的 DFG 值时准备视角相关能量项。波瓣函数只接收所需数值，可独立于绑定和灯实现验证。通用数学函数有实际共享消费者时才放进 Common。

直接沿用生成的材质数据、场景覆盖和绑定。派生参数属于本像素，法线过滤结果由当前光栅采样决定。这也确定了以后 Deferred 的交接点，编码过滤后的核心参数，解码时完成视角相关项。正式 Depth 仅在 MASK 消费基色 alpha，Color 调用现有模式所需的取样。 [分层合同](../../../../Docs/Architecture.md#shader-runtime) · [表面与派生参数](../../../../Docs/Architecture.md#materials) · [现有覆盖](../../../../Shaders/Material/StaticCoverage.glsl) · [现有切线框架](../../../../Shaders/Geometry/SurfaceFrame.glsl)

### 2. 固定桌面 GGX 与几何粗糙度过滤

- 采用 Filament 的几何镜面抗锯齿公式，T01 的 footprint 以当前光栅像素为单位。对单位 `Ng` 的 `dFdx`、`dFdy` 平方长度求和，按 `variance = 0.15 × (dot(dx,dx) + dot(dy,dy))` 和 `kernel = min(2 × variance, 0.2²)` 计算。取 `p = clamp(saturate(pRaw⁴ + kernel)^(1/4), 0.045, 1)`，再求 `alpha = p²`。权重和进入 kernel 的阈值来自固定 Filament 默认参数及阈值平方约定。导数在片元覆盖分歧前求取，过滤只作用于 BRDF 参数。法线贴图及 mip 的解释由现有材质和采样合同承担。
- 对单位视向 `V` 和入射方向 `L`，取 `NoV = clamp(dot(N,V), 1e-4, 1)`、`NoL = clamp(dot(N,L), 0, 1)`。`NoL = 0` 时返回零贡献。半角向量 `V+L` 先稳定归一化，零向量和位于着色半球外的半角贡献零镜面。用 FP32 计算 GGX 和完整 Smith，正的 alpha 和 NoV 约束有效分母。Schlick 使用显式 `F90 = 1`，包括低基色金属。
- 漫反射项为 `diffuseColor / π`，单次散射镜面项为 `D × visibility × F`。visibility 含 `1/(4NoVNoL)` 的约定，光照或积分层对整个 BRDF 乘一次入射余弦。选择完整 Smith 与 FP32 运算以匹配参考积分并保留窄高光，Filament 参考中的 FP16 峰值保护属于其精度配置。导入因子与几何继续从既有验证边界进入共享计算。

[GGX 与两种 Smith 路径](https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/shaders/src/surface_brdf.fs) · [p 与 NoV 下限](https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/shaders/src/surface_material.fs#L1) · [几何过滤](https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/shaders/src/surface_shading_lit.fs#L24) · [过滤默认参数](https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/libs/filamat/include/filamat/MaterialBuilder.h#L956) · [阈值平方](https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/details/MaterialInstance.cpp#L398)

### 3. 先固定能量合同，再接正式 DFG 生产者

核心能量函数接收同一 `NoV` 与过滤后 `p` 的有效 DFG R/G 样点。`R=B`，`G=A+B`，其中 G 是令 Fresnel 为 1 的单次散射方向反照率。以有效的正有限 G 求 `C=1+F0×(1/G−1)`，向环境镜面交付 `E=mix(R,G,F0)`，向环境漫反射交付 `1−E`。补偿镜面项的消费者为直接光的 `D×visibility×F×C` 与预滤波环境的 `E×C`。AO、自发光、入射光物理单位和预曝光交给对应消费层。

本包用白炉及独立积分产生的样点验证能量项，T04 在正式 LUT 的生成和发布边界落实匹配模型、编码以及正有限 G。共享函数使用已验证值。实时补偿对白金属做白炉闭合，核心介质和混合材质的直接 Lambert 加镜面与间接漫反射预算属于 Filament 的实时近似，记录数值偏差。

| 工作包 | 对共享表面与 BRDF 的交接 |
|---|---|
| D07-T02 | 以现有 View 和相机准备交付预曝光与物理参数，用生成接口表达数据。 |
| D07-T03 | 正式直接光先以明确标识的单次散射切片消费本包的 BRDF，验收灯单位、方向、衰减和曝光。用插值的片元位置和已准备的相机求视向，透视 `V` 指向眼点，正交 `V` 指向固定反视线。 |
| D07-T04 | 固定 DFG 与环境生产接入后，直接光和 IBL 共同消费补偿。DFG 用同一模型的 `NoV,p` 查表。真正的 cubemap 采样需贯通生成接口、SPIR-V 反射和 GL 绑定，属于环境消费者的实现范围。 |
| D07-T05 / T06 | 接收同一预曝光 HDR 域，按各自计划验收显示链和可编辑的参考场景。 |

这一顺序把 DFG 的生产、版本和资源绑定集中在正式环境消费者出现时交付。T01 的开发读回直接提供已知样点，使共享数学先获得独立证据。 [光照合同](../../../../Docs/Architecture.md#lighting) · [参考积分编码](https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/libs/ibl/src/CubemapIBL.cpp#L789) · [直接光参考与近似](https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/shaders/src/surface_shading_model_standard.fs#L118) · [阶段前置](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d07-t02)

## Risks / Trade-offs

- 高光或能量异常的来源可能是重复平方、错误余弦、错读 DFG 通道或精度下限。CPU 参考以独立半球积分和粗糙极限解析值交叉核对，GL 探针分别读出派生参数和实际波瓣，再验证补偿响应。
- 重用预览代码可能改变有效 UV、颜色域、背面方向或 MASK 深度。用同一非对称场景在两个 Forward 深度组合中读回覆盖和各预览工作色，几何导数另用具有已知梯度的浮点探针。
- 生产预览与 BRDF 的首次消费时点不同。开发探针通过正常 Shader 包生成和原生 Pipeline 链路编译同一份受测库，记录包内容身份以及原生读回。D07-T03 的直接光接入承担首次正式光照消费的验收。 [最小验证计划](tasks.md#verification)

## Migration Plan

由正式包构建发布更新后的 `StaticForward` 和受传递依赖影响的 `StaticDepth`，核对构建配置、包内容身份及实际消费记录。需要回退时恢复对应源码代并按原目标重建匹配包。模型、场景与材质数据沿用当前读写入口。 [构建入口](../../../../Docs/Build.md#shader-构建)
