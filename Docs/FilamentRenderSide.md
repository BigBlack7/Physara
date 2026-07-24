# Filament 渲染侧物理模型架构：材质、光照与相机

> 本文只讨论 Filament 渲染侧的材质、光照和相机，不展开 Vulkan、Metal、OpenGL 等后端 API 实现。
>
> 分析基线：`google/filament` 主分支提交
> [`fe53857a8c0505069dfe3aca97870b56039709c2`](https://github.com/google/filament/commit/fe53857a8c0505069dfe3aca97870b56039709c2)，提交日期为 2026-06-17。
> 文中的“当前实现”均指该提交，避免把旧版 Filament 文档中的设计讨论误认为现有代码行为。

---

## 1. 总览

Filament 的 PBR 渲染可以概括为以下数据流：

```text
材质定义 .mat
    |
    v
matc / filamat::MaterialBuilder
    |- 分析实际使用的 MaterialInputs
    |- 生成着色模型与功能变体
    |- 编译各阶段 Shader
    `- 打包参数、Sampler、Raster State 和 Shader Program
    |
    v
Material（不可变模板）
    |
    v
MaterialInstance（每实例参数、纹理和少量光栅状态）
    |
    +------------------------------+
    |                              |
    v                              v
相机物理参数                   直接光 / IBL / 阴影
N、t、ISO、焦距、近远平面       lux、lm、cd、SH、预过滤环境图
    |                              |
    `------------+-----------------'
                 v
         View / Renderer 准备阶段
    |- 构造 CameraInfo
    |- 计算曝光与预曝光
    |- 场景裁剪
    |- Froxel 光源分配
    |- 阴影图与级联阴影准备
    `- Frame / View / Material GPU 数据提交
                 |
                 v
            Surface Shader
    |- 构造几何与材质参数
    |- 评估 IBL
    |- 评估方向光
    |- 遍历当前 Froxel 的点光/聚光
    |- 阴影、AO、SSR、折射
    `- 输出预曝光线性 HDR
                 |
                 v
        后处理、色调映射和显示变换
```

三套物理模型之间的核心契约是：

1. **材质**描述表面如何把入射辐射转换为出射辐射。
2. **光照**尽量使用物理单位和几何衰减产生入射照度或辐亮度。
3. **相机**用光圈、快门和 ISO 建立曝光，把大范围的物理亮度预缩放到稳定的 HDR 计算范围。

Filament 并不是离线渲染器式的完全光谱、全局路径追踪模型。它是一套以实时性能为目标的混合体系：物理参数化与能量关系构成基础，同时大量使用预积分、分裂和近似来压缩运行成本。

---

## 2. 材质系统

### 2.1 编译期和运行期架构

#### 2.1.1 Material 是编译后的不可变模板

Filament 材质通常以 `.mat` 文件描述，由 `matc` 或 `filamat::MaterialBuilder` 编译为 Material Package。编译器负责：

- 解析材质 DSL、参数、Sampler、着色模型、混合和光栅状态。
- 对 `material()` 函数进行语义分析，确定真正读写了哪些 `MaterialInputs`。
- 根据材质域、着色模型和功能生成 Shader。
- 生成方向光、动态光、阴影、雾、蒙皮、VSM、SSR、立体渲染等变体。
- 把 Shader Program、参数布局和固定状态打包。

源码：

- [`MaterialBuilder.cpp`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/libs/filamat/src/MaterialBuilder.cpp)
- [`MaterialBuilder.h`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/libs/filamat/include/filamat/MaterialBuilder.h)
- [`MaterialEnums.h`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/filament/include/filament/MaterialEnums.h)

编译器在分析阶段先让所有材质属性在 AST 中可见，再从用户代码的实际访问结果反推出 `mProperties`。因此，一个可选属性只要被材质代码使用，即使赋的是常量默认值，也可能启用对应宏、插值量、Shader 分支和变体。材质作者应避免“为了完整而无条件写入所有字段”。

`variantFilter` 可在编译时删除确定不会使用的变体。例如 Unlit 材质会自动过滤大部分照明变体。这是 Filament 控制 Shader 数量和运行时切换成本的重要手段。

#### 2.1.2 MaterialInstance 是可变参数集合

`Material` 持有不可变的程序和参数布局；`MaterialInstance` 持有：

- 每实例 Uniform 数据。
- 纹理和 Sampler。
- 可覆盖的双面、剔除、深度、透明、模板等状态。
- 参数脏标记与 Descriptor 提交状态。

Uniform 位于材质 UBO 中，Sampler 位于每材质 Descriptor Set 中。MaterialInstance 只在数据变脏时更新；后端还可对小 UBO 做批处理。即使材质没有用户 Uniform，布局也会保留最小存储空间以满足后端约束。

源码：

- [`Material.h`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/filament/include/filament/Material.h)
- [`MaterialInstance.h`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/filament/include/filament/MaterialInstance.h)
- [`details/Material.cpp`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/filament/src/details/Material.cpp)
- [`details/MaterialInstance.cpp`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/filament/src/details/MaterialInstance.cpp)

### 2.2 Surface Shader 执行流程

表面片元的主流程位于
[`surface_main.fs`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/shaders/src/surface_main.fs)：

```text
初始化对象和每帧数据
    |
computeShadingParams()
    |- 世界空间位置
    |- 几何法线与切线标架
    |- 视线方向
    `- 顶点/片元插值属性
    |
initMaterial(inputs)
    |- 写入当前模型的默认材质值
    |
material(inputs)
    |- 执行用户编写的材质函数
    |
透明遮罩 / Alpha Test
    |
prepareMaterial(inputs)
    |- 将切线空间法线变换到世界空间
    |- 计算 NoV、反射向量和 Bent Normal
    `- 准备 Clear Coat 独立法线等派生量
    |
evaluateMaterial(inputs)
    |- IBL
    |- 方向光
    |- Froxel 点光/聚光
    |- 阴影、AO、SSR、折射
    `- Emissive
    |
Post-lighting Color
    |
Fog
    |
输出预曝光线性 HDR
```

`initMaterial()` 的默认值来自
[`surface_material_inputs.fs`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/shaders/src/surface_material_inputs.fs)。
用户的 `material()` 只需要覆盖真正需要的字段。

`prepareMaterial()` 是重要的阶段边界：用户材质先写切线空间 Normal、Bent Normal、Clear Coat Normal，之后 Filament 才建立最终世界空间着色状态。直接光和间接光共享这套准备后的表面参数。

### 2.3 着色模型

当前 `ShadingModel` 包含：

| 模型 | 定位 | 核心模型 |
|---|---|---|
| `LIT` | 默认金属度-粗糙度 PBR | Lambert Diffuse + GGX Specular，可叠加 Clear Coat、Sheen、Anisotropy、Transmission |
| `SUBSURFACE` | 近似次表面材质 | 标准表面高光 + 前向/背向透射近似 |
| `CLOTH` | 布料 | Lambert Diffuse + Charlie Sheen NDF + Neubelt Visibility |
| `UNLIT` | 无光照 | Base Color + Emissive，可选接收阴影 |
| `SPECULAR_GLOSSINESS` | 旧式兼容模型 | Specular Color + Glossiness，内部适配到统一像素参数 |

对应 Shader：

- [`surface_shading_model_standard.fs`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/shaders/src/surface_shading_model_standard.fs)
- [`surface_shading_model_subsurface.fs`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/shaders/src/surface_shading_model_subsurface.fs)
- [`surface_shading_model_cloth.fs`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/shaders/src/surface_shading_model_cloth.fs)
- [`surface_shading_unlit.fs`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/shaders/src/surface_shading_unlit.fs)

### 2.4 MaterialInputs 属性

以下是当前源码中的完整材质属性集合。并非每个着色模型都开放全部字段。

#### 2.4.1 基础表面参数

| 属性 | 典型范围/单位 | 含义 | 默认值 |
|---|---:|---|---:|
| `baseColor` | 线性 RGB `[0,1]`，Alpha `[0,1]` | 介质漫反射色或金属 F0 色；透明模式下通常为预乘 Alpha | `(1,1,1,1)` |
| `roughness` | `[0,1]` | 感知粗糙度 `p`，0 光滑，1 粗糙 | `1` |
| `metallic` | `[0,1]` | 介质与导体之间的艺术可插值参数 | `0` |
| `reflectance` | `[0,1]` | 介质法线入射反射率的感知参数 | `0.5` |
| `ambientOcclusion` | `[0,1]` | 材质微观/局部遮蔽，主要作用于间接光 | `1` |
| `normal` | 切线空间方向 | 最终着色法线 | `(0,0,1)` |
| `bentNormal` | 切线空间方向 | 非遮蔽方向，用于更合理的镜面 AO | `(0,0,1)` |
| `emissive` | 线性 RGB + 曝光权重 | 自发光；Alpha 控制是否随曝光缩放 | `(0,0,0,1)` |

#### 2.4.2 镜面工作流与表面层

| 属性 | 典型范围 | 含义 | 默认值 |
|---|---:|---|---:|
| `specularFactor` | `[0,1]` | 镜面反射强度缩放 | `1` |
| `specularColorFactor` | RGB `[0,1]` | 介质镜面颜色缩放 | `(1,1,1)` |
| `sheenColor` | RGB `[0,1]` | 掠射角绒毛层颜色 | `0` |
| `sheenRoughness` | `[0,1]` | Sheen 粗糙度 | `0` |
| `clearCoat` | `[0,1]`，建议接近 0/1 | 透明清漆层权重 | 功能启用时为 `1` |
| `clearCoatRoughness` | `[0,1]` | 清漆层粗糙度 | `0` |
| `clearCoatNormal` | 切线空间方向 | 清漆层独立法线 | `(0,0,1)` |
| `anisotropy` | `[-1,1]` | 各向异性强度和方向符号 | `0` |
| `anisotropyDirection` | 切线空间方向 | 拉丝方向 | `(1,0,0)` |

#### 2.4.3 次表面、透射和折射

| 属性 | 典型范围/单位 | 含义 | 默认值 |
|---|---:|---|---:|
| `thickness` | 非负，材质约定单位 | 实体透射/次表面的厚度 | `0.5` |
| `microThickness` | 非负，材质约定单位 | Thin Refraction 的微小吸收路径 | `0` |
| `subsurfacePower` | 非负 | 次表面散射指数 | `12.234` |
| `subsurfaceColor` | RGB `[0,1]` | 散射/透射染色 | 标准模型为白，Cloth 为黑 |
| `transmission` | `[0,1]` | 透射能量比例 | `1` |
| `ior` | `>= 1` | 相对折射率参数 | `1.5` |
| `absorption` | 非负，逆长度 | Beer-Lambert 吸收系数 | `0` |
| `dispersion` | 非负，通常 `[0,1]` | 波长相关折射强度 | `0` |

#### 2.4.4 旧式模型和渲染控制

| 属性 | 含义 | 默认值 |
|---|---|---:|
| `specularColor` | Specular-Glossiness 模型的 F0 | `0` |
| `glossiness` | Specular-Glossiness 光泽度，内部转换为 `1 - glossiness` | `0` |
| `postLightingColor` | 光照后的附加颜色 | `0` |
| `postLightingMixFactor` | 光照结果与 Post-lighting Color 混合权重 | `1` |
| `clipSpaceTransform` | 顶点阶段裁剪空间变换 | 单位变换语义 |
| `shadowStrength` | 材质对接收阴影可见度的调制 | `0` |

完整枚举见
[`MaterialEnums.h`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/filament/include/filament/MaterialEnums.h)。

### 2.5 标准金属度-粗糙度模型

#### 2.5.1 从艺术参数到物理参数

设：

- `c = baseColor.rgb`
- `m = metallic`
- `r = reflectance`
- `p = perceptualRoughness`

Filament 构造：

```text
diffuseColor = c * (1 - m)

dielectricF0 = 0.16 * r^2

f0 = mix(dielectricF0, c, m)

alpha = p^2
```

`reflectance = 0.5` 时：

```text
F0 = 0.16 * 0.5^2 = 0.04
```

即常见介质约 4% 的法线入射反射率。金属度为 1 时，漫反射被关闭，`baseColor` 直接成为有色金属 F0。

若材质直接使用 IOR，介质 F0 为：

```text
F0 = ((eta_t - eta_i) / (eta_t + eta_i))^2
```

空气入射且 `eta_i = 1` 时：

```text
F0 = ((ior - 1) / (ior + 1))^2
ior = (1 + sqrt(F0)) / (1 - sqrt(F0))
```

源码入口：

- [`surface_material.fs`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/shaders/src/surface_material.fs)
- [`surface_shading_parameters.fs`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/shaders/src/surface_shading_parameters.fs)
- [`common_shading.fs`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/shaders/src/common_shading.fs)

#### 2.5.2 粗糙度下限

代码对感知粗糙度设置下限，防止 GGX 在极光滑表面产生除零、过亮尖峰和 FP16 溢出：

```text
Desktop / 高精度：p_min = 0.045，alpha_min = 0.002025
Mobile / mediump： p_min = 0.089，alpha_min = 0.007921
```

这意味着用户输入 `roughness = 0` 也不会得到数学上的 Dirac 镜面。

#### 2.5.3 BRDF 分解

标准表面的直接光 BRDF 可写为：

```text
f_r(v, l) = f_d(v, l) + f_s(v, l)
```

当前默认漫反射使用 Lambert：

```text
f_d = diffuseColor / pi
```

Shader 内仍保留 Burley Diffuse 函数，但当前默认编译路径选择 Lambert，不应仅根据旧理论文档断言运行时使用 Disney/Burley Diffuse。

镜面项是 Cook-Torrance 微表面模型：

```text
f_s = D_GGX(h) * V_SmithGGXCorrelated(v, l) * F_Schlick(v, h)
```

其中：

- `D`：Trowbridge-Reitz GGX 法线分布。
- `V`：Smith GGX Correlated 可见性项。
- `F`：Schlick Fresnel。
- `h = normalize(v + l)`。

Filament 的 `V` 函数已经把标准 Cook-Torrance 分母归入可见性定义，因此阅读代码时不要再额外乘一次 `1 / (4 NoV NoL)`。

低/普通质量路径使用快速 correlated visibility 近似；高质量路径使用更精确形式。移动路径的 GGX NDF 通过叉积形式提高 `NoH` 接近 1 时的精度，并限制峰值，避免半精度上溢。

Fresnel：

```text
F = f0 + (f90 - f0) * (1 - VoH)^5
```

低质量路径直接取 `f90 = 1`；较高质量路径从 `f0` 估算掠射反射。

核心实现：

- [`surface_brdf.fs`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/shaders/src/surface_brdf.fs)
- [`surface_shading_model_standard.fs`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/shaders/src/surface_shading_model_standard.fs)

#### 2.5.4 直接光积分

对离散光源，Shader 计算：

```text
L_o = f_r(v, l) * E * saturate(NoL)
```

这里 `E` 是光源在表面的照度或等效辐照量。方向光直接提供照度；点光和聚光先从光强与距离计算照度。阴影、光通道、聚光角衰减和 AO 微阴影会继续调制结果。

#### 2.5.5 高粗糙度能量补偿

单次散射 GGX 会在高粗糙度时损失多重散射能量。Filament 使用预积分 DFG LUT 进行补偿：

```text
energyCompensation = 1 + f0 * (1 / dfg.y - 1)
```

然后将补偿乘到镜面项。它不是显式追踪微表面间的多次反射，而是以预积分近似恢复总体能量和高粗糙金属的亮度。

DFG LUT 是运行时加载的 RGB 半浮点纹理：

- RG：Filament 多次散射 DFG 编码。R 保存 Fresnel 权重贡献 `B`，G 保存总积分 `A + B`；运行时用 `mix(R, G, f0)` 得到 `f0 * A + B`。
- B：Cloth/Sheen 相关预积分项。

这与文献中常见的经典 Split-Sum 可视化编码不同。经典图通常直接把 `A` 放在 R、`B` 放在 G，因此整体偏红；Filament 多次散射运行时表把 `B` 放在 R、`A + B` 放在 G，因此原始 RG 可视化整体偏绿。两者可无损互换：

```text
classic.A = multiscatter.G - multiscatter.R
classic.B = multiscatter.R
```

Physara 的 CPU 预积分严格使用与 Filament `DFV_Multiscatter` 相同的 GGX 重要性采样、Smith-GGX correlated visibility 和 `4 / sampleCount` 归一化。DFG 采样不复用环境 cubemap 预滤波的最小粗糙度钳制，避免低粗糙度区域被人为展宽。

源码：

- [`DFG.cpp`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/filament/src/DFG.cpp)
- [`surface_light_indirect.fs`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/shaders/src/surface_light_indirect.fs)

#### 2.5.6 Specular Anti-Aliasing

高频法线贴图和小三角形会在低粗糙度材质上产生闪烁。Filament 通过屏幕空间法线导数估计法线分布方差，并提高有效粗糙度：

```text
kernelRoughness = variance * normalDerivativeEnergy
filteredAlpha = min(alpha + kernelRoughness, threshold^2)
```

材质可配置方差和阈值。其本质是把无法由像素解析的法线变化折叠为更宽的高光，而不是依靠后处理模糊。

### 2.6 表面层与特殊模型

#### 2.6.1 Clear Coat

Clear Coat 是位于基础材质之上的第二个介质镜面层：

- 固定 IOR 约 1.5，对应 `F0 = 0.04`。
- 使用独立粗糙度。
- 可使用独立 Clear Coat Normal。
- NDF 为 GGX。
- Visibility 使用 Kelemen 形式。
- Fresnel 使用 Schlick。

组合近似为：

```text
baseAttenuation = 1 - F_clearCoat

f = baseAttenuation * f_base + clearCoatWeight * f_clearCoat
```

当启用 IOR 调整时，代码还会把观察到的基础层 F0 反推到清漆介质内部，减少两层介质界面重复计算造成的错误。

#### 2.6.2 Sheen

Sheen 表示布料纤维或绒毛在掠射角形成的柔和反射层：

- 使用 Charlie 分布。
- 使用 Neubelt Visibility。
- 颜色由 `sheenColor` 给出。
- 粗糙度由 `sheenRoughness` 给出。

Filament 用 DFG LUT 估算 Sheen 能量，并压低基础层：

```text
sheenScaling = 1 - max(sheenColor) * sheenDFG
```

然后：

```text
f = sheenScaling * f_base + f_sheen
```

#### 2.6.3 Anisotropy

直接光使用各向异性 GGX。若各向异性参数为 `a`：

```text
alpha_t = alpha * (1 + a)
alpha_b = alpha * (1 - a)
```

切线与副切线方向使用不同粗糙度，并采用 anisotropic GGX NDF 与 correlated visibility。

IBL 不进行完整的各向异性卷积，而是弯曲反射向量后采样普通预过滤 Cubemap。这是明显的实时近似：直接光高光形状较准确，环境高光只是方向近似。

#### 2.6.4 Cloth

Cloth 模型使用：

```text
Diffuse = Lambert
Specular/Sheen = Charlie NDF * Neubelt Visibility * sheenColor
```

它没有标准介质 Fresnel 项，`sheenColor` 直接承担纤维高光颜色。可选的 Cloth Subsurface 使用 Wrap Diffuse 和染色近似，代码明确将其视为艺术性近似，并非严格物理散射。

#### 2.6.5 Subsurface

Subsurface 模型保留标准表面的镜面反射，并对漫反射加入：

- 面向光源的前向散射指数项。
- 背面入射的透射/回散射项。
- `thickness`、`subsurfacePower` 和 `subsurfaceColor` 调制。

实现没有求解扩散方程，也没有屏幕空间多层厚度积分；源码明确指出该 BTDF 不是严格物理模型。它适合蜡、皮肤、叶片等实时视觉近似。

#### 2.6.6 Specular-Glossiness

旧式模型转换为统一像素参数：

```text
f0 = specularColor
perceptualRoughness = 1 - glossiness
metallicApprox = max(specularColor)
diffuseColor = baseColor * (1 - metallicApprox)
```

这里的 `metallicApprox` 只是兼容适配，不代表从任意 Specular Color 都能物理正确地恢复金属度。

#### 2.6.7 Unlit

Unlit 输出主要是：

```text
color = baseColor + emissive
```

它绕过 BRDF 和场景光照，但仍可启用透明、雾和后处理。针对 AR 接收平面，还可选择把场景阴影乘到 Unlit 结果上。

### 2.7 透射、折射、吸收与色散

Filament 支持：

```text
RefractionMode:
    NONE
    CUBEMAP
    SCREEN_SPACE

RefractionType:
    SOLID
    THIN
```

#### 2.7.1 Solid

Solid 模式：

1. 用 Snell 定律和 `ior` 计算进入介质的折射方向。
2. 用近似球体求第二交点和离开介质的方向。
3. 依据粗糙度选择 Cubemap 或屏幕空间颜色的 LOD。
4. 用几何路径长度计算吸收。

近似球体并不等同于真实网格背面，因此厚玻璃的轮廓折射只能近似正确。

#### 2.7.2 Thin

Thin 模式假设薄片两侧近似平行，出射方向接近原始方向。`microThickness` 可提供一个很短的内部吸收路径，并使用简化的无限内部反射级数补偿多次界面反射。

#### 2.7.3 Beer-Lambert 吸收

吸收为：

```text
T(lambda) = exp(-absorption(lambda) * distance)
```

最终透射能量还要乘：

```text
(1 - Fresnel) * transmission * diffuseColor
```

基础漫反射则按 `(1 - transmission)` 衰减，避免同一份能量同时被反射和透射两次。

#### 2.7.4 色散

当前 Shader 使用四个代表波长近似色散：

```text
486.1 nm
546.1 nm
589.3 nm
656.3 nm
```

每个波长使用预计算颜色变换矩阵和不同折射率采样，再重组为 RGB；同时加入像素级抖动来缓解有限样本条带。该路径仅用于 Solid Refraction。它是 RGB 渲染器中的多波长近似，不是完整光谱渲染。

实现见
[`surface_light_reflections.fs`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/shaders/src/surface_light_reflections.fs)。

### 2.8 AO、Bent Normal、SSR 与混合

#### 2.8.1 AO

材质 AO 与 SSAO 组合后主要用于间接光：

```text
combinedAO = min(materialAO, ssao)
```

镜面 AO 可选择：

- Lagarde 经验公式。
- 基于 Bent Normal 和可见圆锥交集的 Jimenez 近似。

GTAO Multibounce 可用多项式近似恢复被传统 AO 过度压暗的彩色反弹。

方向直接光还可使用 Micro-shadowing：根据 AO 推导微遮蔽孔径，再用 `NoL` 估算直接光在微结构中的可见性。它不是额外阴影图，而是 AO 对直接光的局部调制。

#### 2.8.2 SSR

Screen-space Reflection 可在低粗糙度时替代或混合环境镜面。它只包含屏幕中可见的信息，缺失区域仍由 IBL 补足。Screen-space Refraction 同样属于后缓冲复用，不是真实的二次场景追踪。

#### 2.8.3 混合模式

表面材质支持：

- `OPAQUE`
- `TRANSPARENT`
- `ADD`
- `MASKED`
- `FADE`
- `MULTIPLY`
- `SCREEN`
- `CUSTOM`

透明工作流使用预乘 Alpha 语义。`MASKED` 使用 Alpha Test，可结合 Alpha-to-Coverage。透明材质还支持默认、双 Pass 单面和双 Pass 双面等策略。

物理 BRDF 的能量关系只描述单个表面样本；一旦进入 Add、Screen、Custom 等混合，最终合成可能是明确的艺术效果，而不是物理介质叠加。

---

## 3. 光照系统

### 3.1 光源类型与物理单位

`LightManager` 支持：

| 类型 | 物理强度接口 | 说明 |
|---|---|---|
| `DIRECTIONAL` | lux | 无限远平行光 |
| `SUN` | lux | 方向光加太阳角半径、光盘和光晕参数 |
| `POINT` | lumen 或 candela | 各向同性点光 |
| `FOCUSED_SPOT` | lumen 或 candela | 光通量与外锥角物理耦合 |
| `SPOT` | lumen 或 candela | 强度与锥角解耦，便于艺术控制，但不严格物理 |

颜色使用线性 sRGB。API 提供 CCT 和 D 系列日光辅助函数，把色温转换为颜色；色温只改变色度，光源强度仍由 lux、lumen 或 candela 控制。

源码：

- [`LightManager.h`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/filament/include/filament/LightManager.h)
- [`components/LightManager.cpp`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/filament/src/components/LightManager.cpp)
- [`Color.cpp`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/filament/src/Color.cpp)

### 3.2 光度学转换

#### 3.2.1 功率到光通量

API 支持按光源功率和发光效率设置强度。`efficiency` 在代码中按 `[0,1]` 比例使用，例如 LED 内置参考值约为 `0.1171`：

```text
luminousPower(lm) = efficiency * 683 * radiantPower(W)
```

683 lm/W 是 555 nm 单色光的最大明视觉光效。`efficiency` 把真实光源的光谱和电光转换效率折入标量。

#### 3.2.2 Point

各向同性点光由总光通量 `Phi` 转换为光强：

```text
I = Phi / (4 pi)    [cd]
```

表面照度：

```text
E = I / d^2         [lux]
```

#### 3.2.3 Focused Spot

设外锥半角为 `theta_o`，圆锥立体角：

```text
Omega = 2 pi (1 - cos(theta_o))
```

则：

```text
I = Phi / Omega
```

改变外锥角会重新计算 candela，保持总 lumen 不变。这正是 `FOCUSED_SPOT` 的物理含义：光束越窄，中心照度越高。

#### 3.2.4 Spot

普通 `SPOT` 使用：

```text
I = Phi / pi
```

并故意不随锥角变化。它方便灯光师独立调节范围和亮度，但不保持总光通量，因此属于艺术控制模型。

#### 3.2.5 Directional 与 Sun

方向光直接以垂直于光线方向的照度 `E` 表示，单位 lux。太阳默认强度为 100000 lux，接近晴天太阳直射的现实数量级。

### 3.3 点光和聚光衰减

点光/聚光 Shader 位于
[`surface_light_punctual.fs`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/shaders/src/surface_light_punctual.fs)。

设距离平方为 `d2`，用户影响半径为 `r`：

```text
x = d2 / r^2
window = saturate(1 - x^2)^2
attenuation = window / max(d2, 1e-4)
```

因此：

- 主体遵循物理逆平方衰减。
- 到用户设定半径时，用平滑窗口收敛到 0，获得有限影响范围。
- `1e-4 m^2` 相当于约 1 cm 的最小距离，避免理想点光在中心奇异。

有限半径窗口不是无限空间中的严格物理衰减，而是实时灯光裁剪和性能预算所需的工程修正。

聚光角衰减使用内外锥余弦：

```text
scale = 1 / (cosInner - cosOuter)
offset = -cosOuter * scale
spot = saturate(dot(-lightDirection, L) * scale + offset)^2
```

平方让锥边过渡更平滑。

### 3.4 方向光与太阳面积近似

普通方向光没有距离衰减。高质量路径可启用 `SUN_AS_AREA_LIGHT`，根据太阳角半径把反射方向限制到有限太阳盘上，从而得到具有有限宽度的太阳高光。

这不是对圆盘面积光的完整积分，而是针对极远、角半径很小的太阳建立的专用近似。`SUN` 的可见光盘和 Halo 参数还用于天空太阳外观；直接照明仍沿方向光路径完成。

### 3.5 Froxel 动态光分配

Filament 不让每个片元遍历场景全部点光和聚光，而是将相机视锥分成 Froxel，即 Frustum Voxel：

- X/Y 按屏幕 Tile 划分。
- Z 按视空间深度进行近似对数分层。
- CPU 计算光源球体或聚光锥与 Froxel 的相交关系。
- 将每个 Froxel 的光索引压缩到记录缓冲。
- 片元根据屏幕坐标和深度定位 Froxel，只遍历该 Froxel 的灯。

源码：

- [`Froxelizer.cpp`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/filament/src/Froxelizer.cpp)
- [`surface_lighting.fs`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/shaders/src/surface_lighting.fs)

当前记录格式对单个 Froxel 的点光和聚光索引分别采用 8 位计数/范围语义，源码注释给出的上限是每类 255 个。实际可用数量还受全局光源缓冲、内存和性能限制。

场景通常有一个主方向光通过每帧 Uniform 单独求值；点光和聚光通过 Froxel 动态列表求值。光通道可进一步过滤 Renderable 与 Light 是否相互作用。

### 3.6 阴影

#### 3.6.1 阴影图组织

Filament 使用二维纹理数组作为阴影 Atlas：

- 方向光：1 至 4 级 Cascaded Shadow Maps。
- Spot：一张投影阴影图。
- Point：立方体六个方向对应六张独立阴影图。

虽然主相机渲染投影使用无限远平面，阴影和裁剪仍使用 Camera 的有限 `far`，因此 `far` 会直接影响级联划分、可见光源和阴影覆盖。

#### 3.6.2 过滤技术

代码支持多种阴影路径：

- 硬阴影/低成本 PCF。
- DPCF，使用多 Tap 旋转或分布采样。
- PCSS，先搜索遮挡者，再按遮挡者距离扩大过滤半径。
- VSM/EVSM 路径，配合模糊和 Mipmap。
- Contact Shadows，使用屏幕空间短距离追踪补充接触处细节。

当前 PCSS 路径的典型采样预算为 16 次 blocker search 加 16 次过滤采样；DPCF 使用约 12 Tap。具体编译宏和质量级别可改变路径。

#### 3.6.3 Bias

为减少 Shadow Acne 和 Peter Panning，Filament 使用：

- Constant Depth Bias。
- Slope/Receiver-plane Depth Bias。
- 沿几何法线的 Normal Bias。
- 针对投影方向的各向异性 Normal Offset。

VSM 路径不采用同样的 Normal Bias 配置。材质的 `shadowStrength` 还可调制最终接收阴影的强度。

相关源码：

- [`ShadowMapManager.cpp`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/filament/src/ShadowMapManager.cpp)
- [`surface_shadowing.fs`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/shaders/src/surface_shadowing.fs)
- [`surface_shadowing.glsl`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/shaders/src/surface_shadowing.glsl)

### 3.7 Image-Based Lighting

#### 3.7.1 IndirectLight 数据

一个 Scene 当前只绑定一个远距离 `IndirectLight`。它包含：

- 预过滤的镜面反射 Cubemap Mipmap 链。
- 1、2 或 3 个球谐 Band，即最高到 0、1 或 2 阶，对应 1、4 或 9 个 SH 系数。
- 或用于漫反射的低频 Irradiance Cubemap。
- IBL 强度，单位 lux。
- 环境旋转。

默认 IBL 强度为 30000 lux。

源码：

- [`IndirectLight.h`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/filament/include/filament/IndirectLight.h)
- [`details/IndirectLight.cpp`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/filament/src/details/IndirectLight.cpp)
- [`surface_light_indirect.fs`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/shaders/src/surface_light_indirect.fs)

CPU 会把输入 SH Radiance 系数转换为适合 Shader 直接求值的 Irradiance 系数，包括余弦卷积、Lambert `1/pi` 和多项式基系数。

#### 3.7.2 漫反射 IBL

漫反射 Irradiance 由法线方向评估 SH：

```text
irradiance = evaluateSH(normal)
Fd = diffuseColor * irradiance
```

结果会经过 AO、GTAO Multibounce、次表面或透射调制。没有 SH 时也可从最高粗糙度的环境 Mip 近似漫反射环境。

#### 3.7.3 镜面 IBL：Split-Sum

实时中无法逐像素对环境图积分完整 GGX BRDF，因此 Filament 使用 Split-Sum：

```text
SpecularIBL ~= PrefilteredEnvironment(R, roughness)
             * DFG(NoV, roughness, f0)
```

环境图预先按 GGX 粗糙度卷积到 Mip 链，运行时根据感知粗糙度选择：

```text
lod = roughnessOneLevel * p * (2 - p)
```

该映射比简单线性 Mip 更好地匹配 GGX 视觉变化。

DFG LUT 通过 `NoV` 和粗糙度给出积分系数：

```text
dfg.R = B
dfg.G = A + B
E = mix(dfg.R, dfg.G, f0)
  = f0 * A + B
Fr = E * prefilteredRadiance
Fd = diffuseColor * irradiance * (1 - E)
```

`(1 - E)` 用于在漫反射和镜面反射间近似保持能量。随后叠加能量补偿、Clear Coat、Sheen、SSR、AO 和 Refraction。

调试输出同时保留两种表达：

- `brdf_integrate.exr`：经典 Split-Sum `(A, B)`，用于和常见技术文章及 Filament 文档中的红色主导图直接比较。
- `brdf_multiscatter_runtime.exr`：GPU 实际上传的 `(B, A + B)`，用于检查运行时纹理契约；该图偏绿属于正常编码特征。

#### 3.7.4 Dominant Direction

粗糙反射的主要贡献方向并不总是理想反射向量。Filament 随粗糙度把反射向量向法线偏移，再采样环境图。该技巧改善高粗糙度表面的环境高光位置，但仍是单方向采样近似。

### 3.8 当前光照边界

当前提交有几个容易被旧文档掩盖的限制：

1. **没有通用面积光实时求值。** Shader 的动态光循环明确只处理方向光、点光和聚光，并注明 Area Light 尚不支持。
2. **IES 尚未接入实际灯光。** `isIESLight()` 当前返回 `false` 并带有 TODO；旧理论文档对 IES 的讨论不是当前实现能力。
3. **IBL 是单个远距离探针。** 不是内建的多局部 Probe 混合系统。
4. **实时光照不是全局光照求解器。** 多次反射主要来自离线生成的环境、AO 多反弹近似和 BRDF 能量补偿。

---

## 4. 相机系统

### 4.1 坐标系和变换

Filament Camera 的约定：

- 相机朝局部 `-Z` 方向观察。
- `+Y` 为上。
- `+X` 为右。

Camera Entity 的 Transform 表示相机 Model Matrix；View Matrix 是其逆矩阵：

```text
view = inverse(cameraModel)
```

`lookAt()` 最终也是构造 Camera Model Transform。Camera 与 TransformManager 共用实体变换，因此相机移动和普通场景节点遵循同一套世界变换体系。

源码：

- [`Camera.h`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/filament/include/filament/Camera.h)
- [`details/Camera.cpp`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/filament/src/details/Camera.cpp)

### 4.2 投影模型

Camera 支持：

- Perspective。
- Orthographic。
- 直接设置 Custom Projection。
- Stereo 每眼投影。
- 按水平或垂直 FOV 设置透视。
- 按焦距设置透视。

#### 4.2.1 FOV 投影

设近裁剪面为 `n`，垂直 FOV 为 `fovY`：

```text
halfHeight = tan(fovY / 2) * n
halfWidth = halfHeight * aspect
```

水平 FOV 路径则先求 `halfWidth`，再除以宽高比得到 `halfHeight`。

#### 4.2.2 焦距投影

Filament 的焦距接口使用 35 mm 全画幅相机的 24 mm 垂直传感器尺寸：

```text
sensorHeight = 0.024 m
halfHeight = 0.5 * near * sensorHeight / focalLength
```

等价关系：

```text
fovY = 2 * atan(sensorHeight / (2 * focalLength))
```

焦距输入以毫米表示，内部转换时注意单位。

#### 4.2.3 无限远投影与有限裁剪投影

Filament 同时维护两套投影语义：

- **渲染投影**：透视 Camera 强制使用无限远平面。
- **裁剪投影**：保留用户设置的有限 `far`。

无限远渲染投影提高深度分布稳定性，并避免远平面切掉天空和远景；有限 `far` 仍用于：

- CPU/GPU 视锥裁剪。
- Froxel 深度范围。
- 阴影级联划分。
- 光源影响范围判断。

因此 `far` 虽不直接形成透视深度的远裁剪面，仍然是重要性能和阴影质量参数。

#### 4.2.4 Reverse-Z

用户 Custom Projection 按 OpenGL Clip Space 约定输入。Filament 内部把 Z 映射为反向的 DirectX 风格深度：

```text
z' = -0.5 * z + 0.5 * w
```

近处映射到较大的深度值，远处趋近 0。配合浮点深度缓冲，Reverse-Z 把更多有效精度分配给近处，同时无限远投影不再依赖巨大 `far/near` 比值。

裁剪矩阵仍保留适于视锥平面提取的约定，不应把渲染深度矩阵直接当作 CPU Culling Matrix。

### 4.3 CameraInfo

Renderer 在每帧捕获 Camera 状态为 `CameraInfo`，其中包括：

- 每眼渲染投影矩阵。
- Culling Projection。
- Model 和 View Matrix。
- `near`、`far`。
- `ev100`。
- 焦距 `f`。
- 光圈物理直径 `A = f / N`。
- 对焦距离 `d = max(near, focusDistance)`。

这些数据随后进入 View Uniform、裁剪、Froxel、阴影以及景深等后处理。相机原对象因此是用户配置层，`CameraInfo` 是单帧一致快照。

### 4.4 物理曝光

#### 4.4.1 参数

Camera 提供：

- Aperture `N`：f-number。
- Shutter Speed `t`：秒。
- Sensitivity `S`：ISO。

默认值：

```text
N = 16
t = 1 / 125 s
S = 100
```

API 会钳制到：

```text
aperture:       [0.5, 64]
shutter speed:  [1/25000 s, 60 s]
ISO:            [10, 204800]
```

实现：

- [`Exposure.h`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/filament/include/filament/Exposure.h)
- [`Exposure.cpp`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/filament/src/Exposure.cpp)

#### 4.4.2 EV100

ISO 100 标准曝光值：

```text
EV100 = log2((N^2 / t) * (100 / S))
```

默认参数得到：

```text
EV100 = log2(16^2 * 125) = log2(32000) ~= 14.97
```

#### 4.4.3 曝光乘数

Filament 使用饱和基准常数 1.2：

```text
exposure = 1 / (1.2 * 2^EV100)
```

等价为：

```text
exposure = 1 / (1.2 * (N^2 / t) * (100 / S))
```

默认相机：

```text
exposure ~= 1 / 38400
```

场景的物理亮度先乘该因子，再进入 HDR 渲染目标。

#### 4.4.4 EV、亮度和照度关系

按反射式测光常数 `K = 12.5`：

```text
luminance(EV100) = 2^(EV100 - 3) cd/m^2
```

按入射式测光常数 `C = 250`：

```text
illuminance(EV100) = 2.5 * 2^EV100 lux
```

这些辅助函数让应用可以把现实测光、灯光照度与 Camera EV 放入同一数量体系。

### 4.5 预曝光

#### 4.5.1 为什么需要预曝光

现实太阳约 100000 lux，点光中心还可能更高。如果直接把这些数值写入 RGB16F 中间缓冲，容易：

- 超出半浮点范围。
- 在 mediump Shader 中丢失精度。
- 迫使全部光照使用 highp，增加移动 GPU 成本。

Filament 在光照累加前乘 Camera Exposure：

```text
preExposedLight = physicalLight * exposure
```

因此 HDR Buffer 中存储的是相对于当前相机曝光的线性场景值，不是原始 lux 或 cd/m² 数值。

#### 4.5.2 各类光的预曝光位置

- Directional Light：CPU 准备每帧 Uniform 时预乘曝光。
- IBL：CPU 将环境强度预乘曝光。
- Punctual Light：Shader 读取物理光强后乘每帧曝光。
- Emissive：通过 Emissive Alpha 在“随曝光”和“不随曝光”之间混合。

Emissive 衰减近似为：

```text
emissiveAttenuation = mix(1, exposure, emissive.a)
```

- `emissive.a = 1`：像真实发光体一样随相机曝光变化。
- `emissive.a = 0`：保持曝光后亮度，适合 UI、Bloom 或艺术性恒亮效果。

预曝光不会改变 BRDF 的相对能量关系，只是把整个照明问题乘一个公共标量。后处理中的色调映射再把预曝光 HDR 转换到显示空间。

### 4.6 相机单帧渲染流程

当前相机相关主流程可归纳为：

```text
1. 应用配置 Camera
   |- Transform / lookAt
   |- Projection / focal length
   |- near / far
   |- aperture / shutter / ISO
   `- focus distance

2. Renderer 捕获 CameraInfo
   |- view/model
   |- render projection
   |- culling projection
   |- EV100
   `- lens / DoF parameters

3. 调整单帧投影
   |- Guard Band
   |- Dynamic Resolution
   `- TAA Jitter

4. View::prepare
   |- 计算曝光与预曝光灯光
   |- 更新 Camera / View Uniform
   |- 视锥裁剪
   |- Froxelization
   |- 阴影图和级联
   `- IBL / Fog / AO / SSR 等资源准备

5. Surface Pass
   |- 材质几何参数
   |- IBL
   |- 方向光
   |- Punctual Lights
   |- Shadow / AO / Reflection / Refraction
   `- 输出预曝光线性 HDR

6. Post Processing
   |- Bloom / DoF / TAA 等
   |- Color Grading
   |- Tone Mapping
   `- 输出目标显示空间
```

关联源码：

- [`details/Renderer.cpp`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/filament/src/details/Renderer.cpp)
- [`details/View.cpp`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/filament/src/details/View.cpp)
- [`details/Camera.cpp`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/filament/src/details/Camera.cpp)

TAA Jitter 修改的是本帧颜色投影，不应污染稳定的 Culling Projection。类似地，动态分辨率和 Guard Band 是 View/Renderer 的单帧调整，而不是永久改写用户 Camera。

### 4.7 相机模型的边界

1. **核心 Camera API 是手动物理曝光。** 当前分析路径没有在 Camera 内部形成“亮度测量 -> 自动调整 EV”的闭环。
2. 官方理论文档讨论了平均亮度、直方图和测光模式，但那是自动曝光的设计方法，不应直接写成当前 Camera 的内建行为。
3. 应用若需要自动曝光，可以从前帧亮度统计计算目标 EV，再平滑更新光圈、快门、ISO 或直接曝光值。
4. `shutterSpeed` 在这里首先参与曝光计算；不能仅因存在快门参数就断言它自动驱动物理运动模糊。
5. 光圈、焦距和对焦距离会进入景深相关参数；景深本身属于后处理，而不是 Surface BRDF。

---

## 5. 三套模型如何闭环

以太阳照射粗糙介质为例：

```text
Sun intensity = 100000 lux

Camera:
    N = 16
    t = 1/125 s
    ISO = 100
    exposure ~= 1/38400

Pre-exposed illuminance:
    E' ~= 100000 / 38400 ~= 2.604

Material:
    baseColor = c
    metallic = 0
    reflectance = 0.5 -> F0 = 0.04
    roughness = p -> alpha = p^2

Direct shading:
    Lo' = [Lambert(c) + GGX(F0, alpha)] * E' * NoL * visibility
```

这说明：

- 灯光资产可以按现实 lux 设置。
- 材质使用可测量或可解释的 F0、IOR、粗糙度。
- 相机决定同一场景在不同曝光下的最终亮度。
- Shader 内的数值因为预曝光保持在适合实时 HDR 的范围。
- 色调映射只负责把相机曝光后的 HDR 压缩到显示设备，不替代物理光照。

点光示例则多一步：

```text
Phi(lm)
    -> I = Phi / 4pi (cd)
    -> E = I / d^2 (lux)
    -> E' = E * exposure
    -> BRDF * E' * NoL
```

这条链路正是 Filament “基于物理”的核心：资产参数、灯光单位、表面响应和相机曝光具有一致含义，而不是任意的颜色乘法。

---

## 6. 哪些部分是物理模型，哪些是实时近似

| 模块 | 物理基础 | 实时近似或限制 |
|---|---|---|
| 标准材质 | 金属/介质分离、Fresnel、GGX、Smith、能量分配 | Lambert Diffuse；多次微表面散射用 LUT 补偿 |
| 粗糙反射 | GGX 微表面统计 | 极低粗糙度钳制；Specular AA 扩宽 |
| Clear Coat | 双层介质和独立 Fresnel | 固定约 1.5 IOR，层间多次散射近似 |
| Sheen/Cloth | Charlie 分布与纤维掠射高光 | Cloth Subsurface 和层能量为经验近似 |
| Subsurface | 前向/背向透射概念 | 非扩散方程、非厚度场积分 |
| Refraction | Snell、Fresnel、Beer-Lambert | Solid 用近似球体；屏幕空间仅采已有颜色 |
| Dispersion | 波长相关 IOR | 4 波长 RGB 重建，不是光谱渲染 |
| Point/Spot | lumen、candela、逆平方 | 有限影响半径平滑窗口；1 cm 奇异点截断 |
| Sun | lux 和有限角半径 | 太阳面积高光是专用方向修正 |
| IBL | 环境辐射卷积 | SH 漫反射、Split-Sum 镜面、单 Probe |
| 阴影 | 几何遮挡 | Shadow Map、PCF/PCSS/VSM 与 Bias |
| AO | 局部可见性概念 | SSAO/GTAO 和微阴影经验式 |
| 相机曝光 | 光圈、快门、ISO、EV100 | 标量 RGB 曝光，不包含真实传感器光谱和噪声 |
| HDR 稳定性 | 曝光是全局线性缩放 | 预曝光让中间值稳定，但依赖后续 Tone Mapping |

评价 Filament 是否“物理”时，正确结论不是“所有步骤完全精确”，而是：

> 它用物理单位、可解释参数、微表面 BRDF、Fresnel、几何衰减和相机曝光建立一致框架，再用适合实时 GPU 的预积分和有限采样近似昂贵积分。

---

## 7. 对自研渲染器的架构启示

### 7.1 材质层

1. 将 `Material` 设计为不可变 Shader/布局模板，将 `MaterialInstance` 设计为轻量参数和资源实例。
2. 在材质编译期分析实际使用属性，按 Feature 生成宏和变体，避免运行时万能 Uber Shader 承担所有功能。
3. 统一内部 `PixelParams`，让 Metal-Roughness、Spec-Gloss、Cloth 等输入模型最终接入共享直接光和 IBL 框架。
4. 把 `prepareMaterial()` 作为明确阶段：用户参数写入结束后，集中生成世界空间法线、NoV、反射向量和分层法线。
5. 材质属性应记录单位、颜色空间、合法范围和默认值；纹理导入阶段必须正确处理 sRGB 与线性数据。

### 7.2 光照层

1. Directional 使用 lux，Point/Spot 对外使用 lumen/candela，内部统一到可直接计算照度的量。
2. 保留逆平方主体，只在用户半径附近应用平滑窗口；不要用随意的线性距离衰减替代。
3. 区分物理聚光与艺术聚光：是否在改变锥角时保持总 lumen 是清晰的 API 语义。
4. 大量局部光使用 Froxel/Clustered Lighting，只把当前空间单元的灯提交给片元。
5. 将主方向光、动态局部光、IBL 和阴影组织成独立但共享 BSDF 的评估阶段。

### 7.3 相机层

1. 同时维护渲染投影与裁剪投影：渲染可使用 Reverse-Z 无限远透视，裁剪和阴影保留有限 far。
2. 用 CameraInfo 捕获单帧快照，避免渲染过程中读取可变 Camera 状态。
3. 让灯光资产保持现实强度，通过 Camera EV 做全局预曝光，而不是为了防止溢出而压低灯光单位。
4. 把曝光、Tone Mapping 和自动曝光分层：曝光是物理线性缩放，Tone Mapping 是显示变换，自动曝光是跨帧控制器。
5. TAA Jitter、Guard Band 和动态分辨率只修饰单帧渲染投影，不进入稳定裁剪矩阵。

---

## 8. 关键源码导航

### 材质

- Material API：
  [`Material.h`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/filament/include/filament/Material.h)
- MaterialInstance API：
  [`MaterialInstance.h`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/filament/include/filament/MaterialInstance.h)
- 材质编译：
  [`MaterialBuilder.cpp`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/libs/filamat/src/MaterialBuilder.cpp)
- 表面主流程：
  [`surface_main.fs`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/shaders/src/surface_main.fs)
- BRDF：
  [`surface_brdf.fs`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/shaders/src/surface_brdf.fs)
- 标准模型：
  [`surface_shading_model_standard.fs`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/shaders/src/surface_shading_model_standard.fs)
- 材质输入默认值：
  [`surface_material_inputs.fs`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/shaders/src/surface_material_inputs.fs)

### 光照

- LightManager：
  [`LightManager.h`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/filament/include/filament/LightManager.h)
- 光度学转换：
  [`components/LightManager.cpp`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/filament/src/components/LightManager.cpp)
- 方向光：
  [`surface_light_directional.fs`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/shaders/src/surface_light_directional.fs)
- 点光/聚光：
  [`surface_light_punctual.fs`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/shaders/src/surface_light_punctual.fs)
- IBL：
  [`surface_light_indirect.fs`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/shaders/src/surface_light_indirect.fs)
- Froxel：
  [`Froxelizer.cpp`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/filament/src/Froxelizer.cpp)
- 阴影：
  [`ShadowMapManager.cpp`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/filament/src/ShadowMapManager.cpp)

### 相机

- Camera API：
  [`Camera.h`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/filament/include/filament/Camera.h)
- Camera 实现：
  [`details/Camera.cpp`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/filament/src/details/Camera.cpp)
- 曝光公式：
  [`Exposure.cpp`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/filament/src/Exposure.cpp)
- View 准备：
  [`details/View.cpp`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/filament/src/details/View.cpp)
- Renderer 帧流程：
  [`details/Renderer.cpp`](https://github.com/google/filament/blob/fe53857a8c0505069dfe3aca97870b56039709c2/filament/src/details/Renderer.cpp)

---

## 9. 官方资料

- Filament 官方 PBR 设计文档：
  [Filament - Physically Based Rendering in Filament](https://google.github.io/filament/Filament.html)
- Filament 官方材质系统文档：
  [Filament Materials Guide](https://google.github.io/filament/Materials.html)
- 项目仓库：
  [google/filament](https://github.com/google/filament)
- 本文固定分析提交：
  [`fe53857a8c0505069dfe3aca97870b56039709c2`](https://github.com/google/filament/tree/fe53857a8c0505069dfe3aca97870b56039709c2)

本文优先以固定提交的代码为准。官方长篇 PBR 文档包含非常有价值的推导和历史设计，但某些章节描述的实验方向、旧默认项或未来能力，可能与当前 Shader 和 C++ 实现不同。
