# Filament · 材质响应与光照求值

[Filament 技术目录](../Filament_Rendering_Pipeline_Analysis.md) · [源码索引](SourceIndex.md#fa)

<a id="f11"></a>
## 11. 光照求值：材质响应、直接光和 IBL 的完整调用链

### 11.1 模块架构：所有灯共用一次材质参数准备

第 10 节的 MaterialInputs 是用户接口，PixelParams 是光照计算接口。前者保留 metallic、reflectance、感知粗糙度等创作参数；后者保存 F0、微表面粗糙度、DFG、切线和各层响应。转换只做一次，然后由各类光共享。[F29](SourceIndex.md#f29)

```cpp
vec4 evaluateMaterial(MaterialInputs M) {
    PixelParams P;
    getPixelParams(M, P);                // 不是每盏灯各转换一次
    vec3 C = 0;

    evaluateIBL(M, P, C);                // 包括环境反射、漫反射、可选折射/SSR

    if (runtimeHasDirectionalLighting) {
        evaluateDirectionalLight(M, P, C);       // 主方向光，可含阴影
        evaluateExtraDirectionalLights(M, P, C); // 额外方向光
    }
    if (runtimeHasDynamicLighting)
        evaluatePunctualLights(M, P, C); // 只遍历当前 Froxel 的索引

    if (blendMode == FADE) C *= M.baseColor.a;
    vec4 result(C, computeDiffuseAlpha(M.baseColor.a));
    result.rgb += emissive(M, result.a);
    return clampPositiveInfinityToMediumpMax(result);
}
```

这个顺序把前面各模块接起来：PerMaterial 提供 M；PerView 提供曝光、环境和灯；PerRenderable 提供 light channel、接收阴影等对象属性；Froxel 索引决定逐灯循环长度；第 9 节的纹理提供 visibility 和屏幕空间补充。

### 11.2 从材质工作流转换到微表面参数

标准金属度工作流的核心关系是：

$$
c_d=c_{\mathrm{base}}(1-m),\quad
F_0=(1-m)\,0.16r^2+m\,c_{\mathrm{base}},\quad
\alpha=p^2.
$$

这里 r 是 reflectance，p 是 perceptual roughness，α 才是 BRDF 使用的粗糙度。r=0.5 得到介质 F0=0.04。若材质使用 IOR 且未显式提供 reflectance，则以 $((n-1)/(n+1))^2$ 求介质 F0；Specular-Glossiness 分支直接使用 specularColor，并令 p=1-glossiness。[F29](SourceIndex.md#f29) [F37](SourceIndex.md#f37)

```cpp
getPixelParams(M, P) {
    getSpecularPixelParams(M, P);
    getCommonPixelParams(M, P);       // diffuseColor、F0、IOR、transmission 等
    getIridescencePixelParams(M, P);  // 修改基底 F0，必须在 clear coat 接口变换前

    float kernel = geometricSpecularAA ? computeKernelRoughness(geometricN) : 0;
    getSheenPixelParams(M, P, kernel);
    getClearCoatPixelParams(M, P, kernel);
    getRoughnessPixelParams(M, P, kernel);
    getSubsurfacePixelParams(M, P);
    getAnisotropyPixelParams(M, P);
    getEnergyCompensationPixelParams(P); // 采样 DFG，供直接光和 IBL 共用
}
```

几何镜面抗锯齿不改变屏幕采样数量，而是由几何法线导数估算子像素法线方差，扩大粗糙度：

```cpp
variance = aaVariance *
    dot(vec2(dot(dFdx(Ng), dFdx(Ng)), dot(dFdy(Ng), dFdy(Ng))),
        square(frame.derivativesScale));
kernel = min(2 * variance, aaThreshold); // threshold 在 CPU 端已按接口约定处理
alpha = p * p;
filteredAlpha = sqrt(saturate(alpha * alpha + kernel));
filteredP = sqrt(filteredAlpha);
p = clamp(filteredP, MIN_PERCEPTUAL_ROUGHNESS, 1);
```

高精度/中精度的最小 p 分别约为 0.045 / 0.089，以避免峰值和除零问题。清漆、sheen、第二波瓣也各自经过相应过滤。基底粗糙度还会按清漆强度限制到不小于清漆粗糙度，这是层间模糊的工程近似。[F29](SourceIndex.md#f29) [F37](SourceIndex.md#f37)

### 11.3 标准 BRDF：D、V、F 的约定必须一起理解

标准模型以 GGX 分布、Smith 高度相关可见性和 Schlick Fresnel 为主。默认漫反射为 Lambert；源码同时保留其他 BRDF 辅助实现与质量分支，不能把“函数存在”当作默认配置。[F38](SourceIndex.md#f38) [F73](SourceIndex.md#f73)

$$
D_{\mathrm{GGX}}=
\frac{\alpha^2}
{\pi\left[(N\!\cdot H)^2(\alpha^2-1)+1\right]^2},\qquad
F=F_0+(F_{90}-F_0)(1-L\!\cdot H)^5.
$$

源码使用的 V 已包含微表面公式中的 $1/(4N\!\cdot V\,N\!\cdot L)$ 约定，因此镜面 BRDF 写成 D×V×F，不能在调用侧再除一次。实际 D 实现还利用向量叉积等变形提高中精度稳定性。

```cpp
surfaceShading(P, light, visibility) {
    H = normalize(V + light.L);
    NoL = saturate(dot(N, light.L));

    Fr = D_GGX(P.alpha, N, H)
       * V_SmithGGXCorrelated(P.alpha, NoV, NoL)
       * F_Schlick(P.F0, P.F90, dot(light.L, H));
    Fr *= P.energyCompensation;

    Fd = P.diffuseColor / PI;
    if (hasRefraction) Fd *= 1 - P.transmission;

    // 可选的第二波瓣、sheen、清漆在下一小节插入。
    return (Fd + Fr) * light.color * light.intensity
         * light.attenuation * NoL * visibility;
}
```

DFG 的 y 通道参与近似多次散射能量补偿：
`energyCompensation = 1 + F0 * (1 / dfg.y - 1)`。这不是逐路径多次散射追踪。标准直接光代码还保留“衰减漫反射以避免能量增益”的 TODO；因此对现有实现更准确的描述是物理驱动的实时近似，而非所有组合下严格能量守恒。[F29](SourceIndex.md#f29) [F73](SourceIndex.md#f73)

### 11.4 扩展层怎样进入同一个 surfaceShading

| 扩展 | 在参数阶段准备什么 | 在直接光阶段怎样接入 | 在 IBL 阶段怎样接入 |
|---|---|---|---|
| 各向异性 | anisotropicT/B、anisotropy | GGX 的 D、V 改用切线和副切线投影 | 调整反射方向/弯曲法线的近似 |
| 第二镜面波瓣 | 第二粗糙度、DFG、补偿系数、权重 | 第一与第二 Fr 做 mix，第二为各向同性 | 两个不同 LOD 的反射做 mix |
| Sheen | sheenColor、粗糙度、DFG.z、sheenScaling | 衰减基底后加 Charlie/Neubelt 响应 | 衰减基底，加入环境 sheen |
| 清漆 | 独立法线/粗糙度、clearCoat、可选基底 IOR 重映射 | 基底乘 1-Fcc，再加 GGX/Kelemen 清漆 | 环境基底衰减，再加独立清漆反射 |
| 薄膜干涉 | 由视角、薄膜 IOR/厚度改写 F0 | 后续 Fresnel 使用已修改 F0 | 同一 F0 进入 DFG 近似 |

各向异性粗糙度由单参数导出：
`αt=max(α(1+a), MIN_ROUGHNESS)`，`αb=max(α(1-a), MIN_ROUGHNESS)`。a 的符号改变拉伸方向，不表示负的物理粗糙度。[F73](SourceIndex.md#f73)

```cpp
// 标准模型中层的插入顺序，省略编译条件。
Fr0 = specularLobe(P, light) * P.energyCompensation;
if (hasSecondLobe)
    Fr0 = mix(Fr0, isotropicLobe(P.secondAlpha, P.F0) *
                   P.secondEnergyCompensation, P.secondWeight);

base = Fd + Fr0;
if (hasSheen)
    base = base * P.sheenScaling + sheenLobe(P, light);

if (hasClearCoat) {
    Fcc = schlick(0.04, 1, LoH) * P.clearCoat;
    coat = D_GGX(P.coatAlpha) * V_Kelemen(LoH) * Fcc;
    // 有独立清漆法线时，两个 NoL 必须分别计算。
    shaded = base * (1-Fcc) * NoL + coat * coatNoL;
} else {
    shaded = base * NoL;
}
return shaded * incidentRadiance * attenuation * shadowVisibility;
```

第二波瓣的混合避免简单相加导致响应无约束地增大；它受材质 API level 和模型条件限制。清漆固定使用对应 IOR≈1.5 的 F0=0.04，不能解释成任意多层介质求解器。[F22](SourceIndex.md#f22) [F29](SourceIndex.md#f29) [F73](SourceIndex.md#f73)

薄膜干涉位于参数转换层：以薄膜光程差约 `2*nFilm*thicknessNm*cosThetaFilm`，通过光谱灵敏度拟合及有限阶干涉项得到 RGB Fresnel，再反推等效 F0，与基底 F0 混合。代码使用两阶干涉近似；由 RGB F0 也不能完整恢复金属复数折射率。它不会为每盏灯分别执行完整光谱积分。[F38](SourceIndex.md#f38)

### 11.5 其他着色模型：改变共用循环中的响应函数

| 模型 | 与标准模型共用 | 发生变化的求值 |
|---|---|---|
| Lit / Specular-Glossiness | 灯、阴影、Froxel、IBL、输出流程 | 工作流参数化不同，进入同类微表面计算 |
| Cloth | 逐灯和环境框架 | Charlie 分布 + Neubelt 可见性；sheenColor 作为布料镜面颜色 |
| Subsurface | 标准表面高光和灯输入 | 增加厚度/背面透光的便宜散射近似 |
| Unlit | 顶点、材质、混合、雾/输出选项 | 不走常规 BRDF 灯循环；可选 shadow multiplier |
| Custom surface shading | 参数/灯输入框架和 Shader 生成 | 用户提供 surfaceShading，承担每灯响应的实现 |

Cloth 保留 Lambert 漫反射，使用 Charlie/Neubelt 高光，通常不使用标准 GGX 多次散射补偿；可选 subsurfaceColor 通过 wrap diffuse 等方式模拟柔软边缘。[F51](SourceIndex.md#f51)

Subsurface 不是离线 BSSRDF，也没有把光在物体内部多次随机游走。实际每灯流程的关键式如下：[F52](SourceIndex.md#f52)

```cpp
forwardScatter = exp2((saturate(dot(V, -L)) - 1) * subsurfacePower);
backScatter = (NoL * thickness + (1 - thickness)) * 0.5;
subsurface = mix(backScatter, 1, forwardScatter) * (1 - thickness);
C = subsurfaceColor * subsurface / PI;

if (NoL > 0) {
    Fd = diffuseColor / PI;
    Fr = standardSpecularLobe(...) * energyCompensation;
    // 阴影边缘处用散射颜色替代部分常规漫反射。
    Fd = mix(subsurfaceColor * subsurface / PI, Fd, pow4(visibility));
    C += (Fd + Fr) * NoL * visibility;
}
return C * light.color * light.intensity * light.attenuation;
```

透射散射项没有与正面项一样整体乘 NoL；源码对其阴影遮挡仍有 TODO。Custom surface shading 也不自动获得一个新的物理模型，它是把该函数的实现交给材质作者的扩展点。[F48](SourceIndex.md#f48) [F52](SourceIndex.md#f52) [F53](SourceIndex.md#f53)

### 11.6 直接光：CPU 物理单位如何变成片元入射量

第 4 节已把灯转换并压缩到 GPU 表达。方向光按方向给出空间常量入射；SUN 在方向光基础上增加太阳角半径有关的镜面近似。局部光需要距离、范围、聚光角和对象 light channel 过滤。[F30](SourceIndex.md#f30) [F54](SourceIndex.md#f54) [F56](SourceIndex.md#f56)

```cpp
evaluatePunctualLights(M, P, inout C) {
    uint froxel = findFroxel(fragmentXY, viewSpaceDepth);
    Record rec = froxelRecords[froxel];
    for (uint k = 0; k < rec.lightCount; ++k) {
        LightData D = lights[lightIndices[rec.offset + k]];
        if ((D.channels & object.channels) == 0) continue;

        delta = D.position - worldPosition;
        d2 = dot(delta, delta);
        L = delta * inversesqrt(d2);
        rangeWindow = square(saturate(1 - square(d2 * D.inverseRadiusSquared)));
        attenuation = rangeWindow / max(d2, 1e-4);

        if (D.isSpot)
            attenuation *= square(saturate(dot(D.direction, -L) *
                                            D.spotScale + D.spotOffset));
        attenuation *= viewDistanceFade(...);
        if (attenuation <= 0) continue;

        Light light = makeLight(L, D.color, D.intensity * frame.exposure, attenuation);
        visibility = D.hasShadow ? sampleLocalShadow(D, ...) : 1;
        visibility *= optionalScreenSpaceContactShadow(...);
        C += surfaceShading(P, light, visibility);
    }
}
```

rangeWindow 让有限半径外严格归零，半径内仍保留反平方形式；局部灯影响范围和光度强度是两个不同参数。Point 的流明转换为坎德拉时除 4π；Focused Spot 使用聚光立体角，Spot 的用户强度约定不同。Shader 不能再把流明直接塞进 `I/d²`。[F54](SourceIndex.md#f54) [F56](SourceIndex.md#f56)

环境和方向光已在 CPU 相关参数中结合曝光；局部光在 Shader 求值时乘曝光，不能再对整个 HDR 结果重复乘一次。现有主路径没有通用面积光积分。LightManager 中的 IES 判定仍是 TODO，不能仅因存在相关字段就认为已经实现完整 IES 配光。[F29](SourceIndex.md#f29) [F69](SourceIndex.md#f69)

### 11.7 IBL：离线卷积、运行时 DFG 和环境采样必须配套

运行时的主要输入是一个 IndirectLight 的远场环境、漫反射 SH 系数和 DFG LUT。它不是局部探针混合系统，也不是场景几何上的实时 GI。[F36](SourceIndex.md#f36) [F58](SourceIndex.md#f58)

| 输入 | 生成/准备 | 运行时用途 |
|---|---|---|
| 环境立方图的粗糙度层级 | cmgen / CubemapIBL 的重要性采样卷积 | 近似 GGX 高光积分 |
| 漫反射 SH | 1、4 或 9 个系数；CPU 完成对应卷积/归一化 | 由法线或 bent normal 重建低频环境 |
| DFG LUT | DFG::init 上传预积分查找表 | 结合 NoV、p、F0 估计方向反照率 |
| SSAO/材质 AO/bent normal | 第 9、10 节 | 局部遮蔽近似 |
| SSR 颜色和置信度 | 第 9 节 | 替换可见的部分环境镜面反射 |

DFG 不是任意“BRDF 两通道贴图”。当前表的 R 对应经典 split-sum 的 B，G 对应 A+B，蓝通道用于 cloth/sheen；因此代码写成：

$$
E=\operatorname{mix}(\mathrm{DFG}.r,\mathrm{DFG}.g,F_0)
 = B+F_0A.
$$

若换成常见的 (A,B) LUT 而不改 Shader，这个混合会错误。`LOD = maxMip * p * (2-p)` 也与 Filament 的环境预滤波约定配套；普通 box-filter mip 不能等价替代该资源。[F36](SourceIndex.md#f36) [F39](SourceIndex.md#f39) [F57](SourceIndex.md#f57)

```cpp
evaluateIBL(M, P, inout C) {
    E = mix(P.dfg.x, P.dfg.y, P.F0);
    ssr = sampleValidSSRIfEnabled(...); // RGB 已乘贡献置信度，A 保存同一权重

    if (ssr.a < 1) {
        R = getReflectedVector(P, N);   // 各向异性可调整反射方向
        Fr = E * samplePrefilteredEnvironment(R, lod(P.p));
        Fr *= P.energyCompensation;
        if (hasSecondLobe)
            Fr = mix(Fr, evaluateSecondPrefilteredLobe(P), P.secondWeight);
    } else Fr = 0;

    aoD = min(M.ambientOcclusion, sampleSSAO());
    aoS = specularAO(NoV, aoD, P.alpha, bentNormalData);
    Fr *= singleBounceAO(aoS);

    irradiance = evaluateSH(hasBentNormal ? bentN : N);
    // Lambert 的 1/PI 已在 SH 准备约定中，不能再除一次。
    Fd = P.diffuseColor * irradiance * (1-E) * singleBounceAO(aoD);

    evaluateSubsurfaceIBL(P, irradiance, Fd, Fr);
    applyMultiBounceAO(aoD, aoS, P, Fd, Fr); // 颜色相关的能量补偿近似
    evaluateSheenIBL(P, ..., Fd, Fr);
    evaluateClearCoatIBL(P, ..., Fd, Fr);
    Fd *= frame.iblLuminance;             // 已含曝光
    Fr *= frame.iblLuminance;

    if (hasRefraction) {
        Ft = evaluateRefraction(P, N, E) * P.transmission;
        Fd *= 1-P.transmission;
    }
    Fr = Fr * (1-ssr.a) + E * (ssr.rgb * singleBounceAO(aoS));
    C += Fd + Fr + optional(Ft);
}
```

SSAO 主要进入间接光遮蔽，不能直接当作每盏灯的阴影因子。SSR 已经是加权的屏幕空间反射入射量，在合成处应用相应 DFG 响应；不能先把它当最终高光、再乘一次完整 BRDF，也不能把 confidence 重复乘入。固定源码的 SSR 分支没有再次乘环境亮度，且不与环境分支完全共用所有能量补偿操作；移植时应明确所选共同模型，不能仅凭函数名称认定两分支代数相同。没有有效 SSR 的区域自然保留环境反射。[F36](SourceIndex.md#f36) [F59](SourceIndex.md#f59)

源码还有运行时重要性采样 IBL 的编译分支，但包含未完成项；本文的完整链路以默认的预滤波环境方案为准。接下来分析上面尚未展开的 Ft，以及颜色怎样写入 HDR 目标。

### 11.8 标准 glTF 输入与 Filament 参数的适配边界

同名材质属性不保证参数化相同。glTF 字段、默认值、纹理通道、颜色域与扩展交互属于资产语义；规范中明确标为非规范性的 Implementation 示例提供可选择的实时算法，不要求逐行复制。下列差异解释移植边界；Physara 的支持范围和固定求值 profile 仍见架构。

**核心、IOR 与 specular。** glTF metallic-roughness 的默认 metallic、roughness 都是 1，不采用 Filament 通用材质接口的 metallic=0 初始化。保留 glTF 的 IOR、specular weight/color，派生介质 F0/F90 后再按 metallic 与金属响应混合：

```text
weight = specularFactor * specularTexture.a
color = specularColorFactor * DecodeSrgb(specularColorTexture.rgb)
dielectricF0 = min(((ior-1)/(ior+1))² * color, 1) * weight
dielectricF90 = weight
```

默认 IOR=1.5、weight=1、color=1 恢复 F0=0.04。specular 只改变介质响应，不缩放金属高光；只修改 F0 而始终保留 F90=1 也不符合该扩展的响应定义。固定 Filament 在相应 feature 分支中有独立 F0/F90 字段，可以复用数据组织。IOR=0 是 Khronos 定义的兼容模式，基底 Fresnel 必须恒为 1，再按该模式组合 specular/color；不能将它按普通折射率直接代入折射函数或像固定 Filament 通用入口那样简单 clamp 到 1。[F29](SourceIndex.md#f29) [G01] [G02] [G09]

**各向异性。** 第 11.4 节的 `alpha*(1±a)` 是 Filament 自有参数化。KHR_materials_anisotropy 的强度 s 及纹理 B、方向 RG/rotation 需要先按扩展解释，再由 `alphaT=mix(alpha,1,s²)`、`alphaB=alpha` 产生两个方向粗糙度；它不等于给 Filament 的 a 直接赋 s。可以复用接受 alphaT/alphaB 的 GGX D 与各向异性 Smith visibility 基础函数，IBL 则明确采用其弯曲反射方向近似，不改写原始 glTF 参数。[F38](SourceIndex.md#f38) [F73](SourceIndex.md#f73) [G03]

**清漆、sheen 与发射。** KHR clearcoat 的独立法线缺省为几何法线，不继承基底 normal map；清漆和基底的 IOR 分开解释。标准层定义覆盖整个基底，包括发射；其非规范性实现示例用清漆透过权重衰减 emission。固定 Filament 的可选基底 IOR 重映射、以及灯光求值后直接相加的 emissive 不能不经适配便视为标准叠层。Sheen 也不等于切换整个 Cloth shading model；Charlie/visibility、albedo scaling 和 DFG 必须配套。KHR 示例允许用 NoV 单次查询的 albedo scaling 换取性能，因此 Filament 式简化可作为明确的实时 profile，而不是任意组合严格能量守恒的保证。[F29](SourceIndex.md#f29) [F36](SourceIndex.md#f36) [F73](SourceIndex.md#f73) [G04] [G05]

**透射与体积。** 固定 Filament 的直接光只把基底漫反射乘 `1-transmission`，透射 Ft 在 IBL/背景函数加入；这不自动提供逐灯 BTDF。KHR transmission 定义包含微表面透射分量，官方 Sample Renderer 的 [逐灯循环](https://github.com/KhronosGroup/glTF-Sample-Renderer/blob/5650e2c8958801445ac0b38937a83984e3a6b278/source/Renderer/shaders/pbr.frag#L353) 也求透射。选择实现完整的直接/间接响应时，应复用同一灯循环并补齐透射分量，不能仅展示背景折射便认为两者等价。厚度的 mesh space、吸收距离的 world space，以及实例变换的处理见第 12.2 节。[F36](SourceIndex.md#f36) [F73](SourceIndex.md#f73) [G06] [G07] [G08]

**薄膜。** KHR iridescenceFactor 乘强度纹理 R；厚度由纹理 G 在 minimum/maximum 间插值，单位 nm，无纹理时取 maximum，且 minimum 允许大于 maximum。原始薄膜 IOR 默认 1.3，不与场景米单位或基底 IOR 混淆。第 11.4 节的有限阶干涉和等效 F0 是具体实时求值近似，不替代这些输入语义。[F38](SourceIndex.md#f38) [G11]
