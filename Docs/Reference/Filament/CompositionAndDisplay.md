# Filament · 颜色合成、时域与图像输出

[Filament 技术目录](../Filament_Rendering_Pipeline_Analysis.md) · [源码索引](SourceIndex.md#fa)

<a id="f12"></a>
## 12. 颜色合成：折射、透明、天空、雾与自发光

### 12.1 模块架构：先有不透明背景，才能采样屏幕空间透射

折射横跨 CPU 图组织和 GPU 材质求值，不能只在“材质模型”中孤立讲一个 refract。第 6 节的排序结果找出折射命令分界；RendererUtils 将颜色执行划为不透明背景、可采样背景准备、折射部分等阶段。Shader 的屏幕空间透射输入来自背景颜色层级，和 SSR 反射使用的层在用途上区分。[F08](SourceIndex.md#f08) [F36](SourceIndex.md#f36) [F60](SourceIndex.md#f60)

```cpp
// CPU 图组织的依赖关系概要，不表示所有透明物体都分配独立背景。
background = renderOpaqueColor(...);
if (commandsContainScreenSpaceRefraction) {
    refractiveInput = resolveAndBuildRefractionLevels(background);
    background = renderRefractiveRange(background, refractiveInput, ...);
}
color = renderRemainingSortedBlendedCommands(background, ...);
```

这保证读背景和写当前颜色有明确资源关系；但屏幕外、被遮住的背景并不会因此变得可见。立方图折射模式则从环境采样，不依赖完整的场景几何求交。

### 12.2 折射：近似几何、Snell 定律、滤波和吸收

折射函数产生三项：出口位置、出口方向、内部路径长度 d。Solid 主分支使用球体厚度近似；Thin 分支以薄层近似处理。源码虽有 solid-box 辅助函数，当前类型选择路径并未因此暴露一个通用可选的盒体折射模型。[F36](SourceIndex.md#f36)

```cpp
Refraction solidSphereApprox(P, N, incomingRay) {
    etaIn = 1 / P.ior;
    NoRin = dot(N, incomingRay);
    rr = etaIn * incomingRay
       - (etaIn * NoRin + sqrt(max(1-etaIn*etaIn*(1-NoRin*NoRin), 0))) * N;

    NoR = dot(N, rr);
    d = P.thickness * (-NoR);
    exitPosition = worldPosition + rr * d;
    exitNormal = normalize(NoR * rr - N * 0.5); // Sphere fudge，非求交得到
    exitDirection = refract(rr, exitNormal, P.ior);
    return { exitPosition, exitDirection, d };
}

Refraction thinApprox(P, N, incomingRay) {
    d = 0;
    pos = worldPosition;
    if (hasMicroThickness) {
        rr = refract(incomingRay, N, 1/P.ior);
        d = P.microThickness / max(-dot(N, rr), 0.001);
        pos += rr * d;
    }
    return { pos, incomingRay, d };
}

vec3 evaluateTransmission(P, E) {
    ray = selectRefractionModel(P);
    lod = refractionLod(P.unclampedRoughness, P.ior, frame.refractionLodOffset);
    if (mode == CUBEMAP)
        t = environment(ray.direction, lod) * frame.iblLuminance;
    else
        t = refractiveBackground(projectToUv(ray.position), lod);

    t *= exp(-P.absorption * ray.d); // Beer-Lambert；absorption 为逆场景长度
    E = optionalThinLayerInternalReflectionCorrection(E, P.transmission);
    t *= (1-E) * P.diffuseColor;
    return t; // 外层 evaluateIBL 仅在这里返回后乘一次 transmission。
}
```

环境采样须结合环境亮度和曝光，屏幕背景已经预曝光，不能重复乘。粗糙透射通过不同 LOD 的颜色采样近似，没有逐微表面追踪。薄层还用一个收敛级数近似前后界面的内部反射：`E *= 1 + transmission*(1-E.g)/(1+E.g)`，随后再计算透射边界的 1-E。[F36](SourceIndex.md#f36)

glTF volume 的 `thicknessFactor` 和 thicknessTexture.G 给出 mesh-space 厚度，节点变换也作用于厚度；`attenuationDistance` 则是 world-space 距离。计算 Beer–Lambert 时，吸收系数的逆长度必须与实际 world-space 路径长度配套，不能因为两个字段都是长度就一同乘实例缩放。官方 Sample Renderer 从 model matrix 提取轴向尺度并构造近似透射路径，这是示例算法，不是规范强制的完整几何求交。球体、薄层或该尺度近似应有明确适用条件，非均匀缩放/剪切不能不加说明地视为精确体积传输。[G07] [G08]

色散进一步拆出四条 Solid Sphere 近似射线，对应约 486.1、546.1、589.3、656.3 nm；以 dispersion/20 和基础 IOR 的偏移生成各波段 IOR，加入像素抖动，各采一次环境或屏幕背景，最后由四个预计算 3×3 矩阵重建 RGB。吸收统一使用其中 r1 的代表路径长度。这是四样本光谱积分近似，不是只把 RGB 三个通道作屏幕平移，也不是任意精度光谱追踪。[F36](SourceIndex.md#f36)

KHR dispersion 的字段语义是 `20/Abbe`，默认零表示无色散。其非规范性示例提供更轻的 RGB 三次透射：`halfSpread=(ior-1)*0.025*dispersion`，三个 IOR 为 `ior-halfSpread, ior, ior+halfSpread`；极端值需避免红通道 IOR 小于 1。这是一种可选近似，并非资产保存三个 IOR，也不是固定 Filament 当前四波长实现。采纳哪种 profile 都应保持相同标准输入，分别说明采样成本、极值和体积路径近似。[G10]

### 12.3 透明：混合公式取决于材质约定和 Draw 排序

| 模式/策略 | Shader/光栅端逻辑 | 与前面阶段的关系 |
|---|---|---|
| Opaque | 视为不透明写颜色和深度 | 形成主要背景与遮挡 |
| Masked | alpha 阈值裁剪；可配 alpha-to-coverage | 深度/阴影变体必须采用兼容裁剪 |
| Transparent | 预乘约定；镜面反射保持相应表面表现 | 依赖命令排序和固定混合状态 |
| Fade | 参数阶段反预乘 baseColor，光照完成后整体彩色乘 alpha | 漫反射与镜面一起淡出 |
| Add / Multiply / Screen / Custom | 独立 blend equation / factors | 由 PipelineState 驱动，并非不同 BRDF |
| Two passes one side | 先深度、后颜色的策略 | 限制自身可见层 |
| Two passes two sides | 背面/正面两次绘制策略 | 改变自身表面的合成次序 |

这些模式由 Material/RenderPass 共同生成具体光栅状态。它们并不形成通用 order-independent transparency；相交透明物体的排序限制依然存在。折射的 transmission、透明混合 alpha、几何 thickness 是三个不同控制量，不能合并成一个“透明度”。[F09](SourceIndex.md#f09) [F19](SourceIndex.md#f19) [F29](SourceIndex.md#f29)

### 12.4 自发光、天空与雾进入同一 HDR 域

自发光在灯光求值后加入：
`emissive.rgb * mix(1, exposure, emissive.w)`；Transparent/Fade 还结合 alpha。emissive.w 是曝光权重，不是强度单位本身。它可以形成 Bloom 的高亮输入，但不会自动把这个网格注册成照亮其他物体的面积光。[F29](SourceIndex.md#f29)

Skybox 是环境背景的绘制。它与 IndirectLight 的照明数据相互独立：场景可以看见某个天空，却由另一份环境光资源照明。两者都要遵守曝光和颜色空间约定。[F58](SourceIndex.md#f58) [F62](SourceIndex.md#f62)

Fog 结合相机到表面的距离、垂直高度的密度变化和环境/太阳散射颜色，近似沿视线的透射与入射散射：

$$
C_{\mathrm{fogged}}=T\,C_{\mathrm{surface}}+(1-T)\,C_{\mathrm{scatter}},
\qquad T=e^{-\int \sigma_t(s)\,ds}.
$$

这里的积分用解析/工程近似处理，不是网格体积介质中的多次散射求解。按选项，雾可进入表面 Shader 或对应图像处理路径；必须识别所在路径的线性 HDR、alpha 和背景约定。[F63](SourceIndex.md#f63)

颜色至此仍是预曝光的线性 HDR。下一阶段处理图像采样、镜头效果和显示映射，而不是再次计算场景 BRDF。

<a id="f13"></a>
## 13. 图像输出：Resolve、TAA、镜头和显示变换

### 13.1 模块架构：同一 FrameGraph 上的后处理子图

后处理不是固定启用的直线清单。Renderer 依照 View 选项、MSAA、透明输出、分辨率和后端能力选择子图；内置后处理材质通常通过全屏 Raster Draw 执行。[F08](SourceIndex.md#f08) [F34](SourceIndex.md#f34)

```cpp
hdr = colorPassOutput;
depth = resolveDepthIfNecessary(colorPassDepth);

if (taaEnabled) {
    hdr = taa(hdr, depth, history, stableProjection, jitteredProjection);
    prepareCurrentTaaHistory(...);
}
hdr = resolveColorIfNecessary(hdr);

if (postProcessingEnabled) {
    if (dofEnabled) hdr = dof(hdr, depth, cameraLens);
    if (bloomEnabled || lensFlareEnabled)
        effects = bloomAndFlare(hdr);

    if (!colorGradingDoneInSubpass)
        ldr = colorGrading(hdr, effects, colorGradingLut);
    else ldr = previouslyProducedLdr;

    if (fxaaEnabled) ldr = fxaa(ldr);
    if (spatialUpscaleNeeded) ldr = upscale(ldr, outputViewport, ...);
}
exportOrBlitToOutput(selectedColor, ...);
```

Resolve 是多样本到单样本资源的转换，TAA 是跨帧采样重建，FXAA 是当前图像的边缘过滤；三者不应混成同一种抗锯齿。源码还存在定制 Resolve 和 color grading subpass 的条件路径：没有 Bloom、DoF、TAA 等后续 HDR 依赖，且样本/后端 framebuffer-fetch 等条件满足时，可以减少中间图像读写。[F08](SourceIndex.md#f08)

### 13.2 TAA：投影、历史图像和重新投影组成闭环

第 3 节对颜色采样加 jitter，同时保留稳定投影。`TaaJitterCamera` 在加入 jitter 前保存当前 `projection*view`，jitter 以渲染像素为单位记录；Shader 接收的 sample jitter 与投影偏移符号相反。所选模式可使用 Halton(2,3) 16 样本。TAA 的输出经当前邻域重建后位于 stable 输出栅格；原始 depth 与 depthHistory 仍在各帧 jittered 光栅栅格。这三种坐标不能当成同一 UV。[F34](SourceIndex.md#f34) [F64](SourceIndex.md#f64) [F78](SourceIndex.md#f78) [F79](SourceIndex.md#f79)

```cpp
// 对应上游 1:1 路径；UvToClip 同时包含实际后端的 UV/clip 约定。
taa(current, depth, previous, matrices, sampleJitter) {
    uvCurrent = currentPixelUv();
    z = depth(uvCurrent);
    R = previousStableClipFromWorld * inverse(currentStableClipFromWorld) * UvToClip;
    q = R * vec4(uvCurrent, z, 1);
    uvPrev = ClipToUv(q.xy / q.w);

    cur = reconstructLanczos2Neighborhood(current, sampleJitter);
    hist = sampleHistoryWithSelectedFilter(uvPrev);
    cur, hist = compressHdrForTemporalFiltering(cur, hist);

    stats = computeNeighborhoodStatistics(cur);
    hist = clipOrClampHistory(hist, stats, taaOptions);
    historyDepthUv = uvPrev + (sampleJitter - previousSampleJitter) / renderExtent;
    expectedHistoryDepth = q.z / q.w;
    valid = optionalDepthRejectionWithJitterNeighborhoodTolerance(
        expectedHistoryDepth, depthHistory(historyDepthUv), depthNeighborhood);
    weight = historyWeightAdjustedByMotionAndValidity(valid, ...);
    return expandHdrAfterTemporalFiltering(blendCurrentAndHistory(cur, hist, weight));
}
```

上述伪代码表示资源齐全的普通路径：没有历史时，上游以当前颜色/深度作为输入、采用当前矩阵并令 jitter 差为零，不读取未初始化历史。历史颜色可选 bilinear 或带 deringing 的 5-tap Catmull–Rom；当前颜色以 3×3 Lanczos-2 重建并限制负瓣 overshoot。历史限制可使用邻域 min/max 与九点均值/方差框的交集，clip 与 clamp 是可选策略，不是固定 0.9/0.1 混色。[F34](SourceIndex.md#f34) [F64](SourceIndex.md#f64) [F78](SourceIndex.md#f78)

HDR 过滤的可逆压缩为 `c/(1+maxRGB(c))`，过滤后用 `c/(1-maxRGB(c))` 还原，并对分母极点和非法值作防护。这是 TAA 内部的数值工作域，不是最终显示 ToneMap，也不改变线性 HDR 历史的角色。可选 YCoCg 不能省略 RGB gamut/有限值约束；固定基线还保留部分权重 gamut clamp 的 FIXME，不能把所有可选分支视为无条件正确。[F64](SourceIndex.md#f64) [F78](SourceIndex.md#f78)

`depthDisocclusion` 在该固定基线中默认关闭；启用时额外保留原始渲染分辨率 depth。比较 `q.z/q.w` 与 point-sampled depthHistory 的 raw Reverse-Z，两种深度差符号都可能表示表面变化；当前 3×3 深度范围和可选历史邻域用来容忍 jitter 在薄边界造成的采样跳变，不能用任意固定线性距离代替。当前/历史 depth 的 sample jitter 差只补一次，符号与当前重建使用同一接口定义。[F34](SourceIndex.md#f34) [F64](SourceIndex.md#f64) [F78](SourceIndex.md#f78)

此主路径以深度和相机变换为重投影基础，并不具备通用逐对象速度缓冲。骨骼动画、物体独立移动和多层透明会带来不同于纯相机运动的历史误差；depth rejection 也不能恢复其正确前态。代码中的 temporal upscaling 有独立的样本、重建和资源约束，不等于最后追加一个任意放大操作。[F08](SourceIndex.md#f08) [F64](SourceIndex.md#f64)

Physara 采用静态场景时，可复用相机-depth 重投影，无需建设对象 motion、Skin/Morph 前态。内容编辑与相机 pose 更新要分开识别：前者保守失效 TAA/SSR，后者保留静态重投影资格；确切失效 key、1:1 profile 和历史提交规则归架构。固定 Filament 的 FrameHistoryEntry 没有保存 preExposure 或在此函数显式重标定历史；Physara 若沿用预曝光变化，需要记录历史曝光并在压缩/混合前乘 `currentPreExposure/previousPreExposure`，这是项目适配，不应写成上游已经执行的行为。[F34](SourceIndex.md#f34) [F79](SourceIndex.md#f79)

### 13.3 景深：Camera 参数先转成 CoC，再调度空间滤波

Camera 的焦距 f、光圈 N、对焦距离 S 和传感器尺寸进入 CoC 计算。薄透镜近似中，孔径直径 A=f/N，传感器平面的弥散圈直径可写为：

$$
|\mathrm{CoC}|=\left|\frac{Af}{S-f}\left(1-\frac{S}{d}\right)\right|,
\qquad
\mathrm{CoC}_{px}=\mathrm{CoC}\frac{H_{px}}{H_{\mathrm{sensor}}}.
$$

源码据此准备 GPU 参数，而后在图上进行分辨率缩减、近远场分类、tile 范围统计/膨胀、颜色层级、散景 gather、过滤和合成。膨胀用于把前景大模糊核的影响传到相邻 tile，避免只按中心深度选择采样半径。[F12](SourceIndex.md#f12) [F34](SourceIndex.md#f34) [F65](SourceIndex.md#f65)

```cpp
coc = signedCircleOfConfusion(linearDepth, lensParameters);
tiles = reduceNearFarCocRange(coc);
tiles = dilateTilesForKernelReach(tiles);
lowResColor = prepareColorAndCocPyramid(hdr, coc);
bokeh = gatherNearAndFarLayers(lowResColor, tiles, apertureOptions);
return compositeSharpAndDefocused(hdr, filterBokeh(bokeh), coc);
```

这是图像空间的镜头模糊近似；它不代表相机每个像素真实发射多条穿过镜片的射线。曝光中的快门参数也不会自动产生物体运动模糊 Pass。

### 13.4 Bloom 与 Flare：提取高亮后跨尺度传播

Bloom 对 HDR 亮部进行阈值/亮度约束、降采样和多尺度过滤，再逐级重建并在颜色映射附近合成。可选 lens flare 使用相同图像输入构造镜头伪影，不参与 PBR 入射光积分。[F34](SourceIndex.md#f34) [F66](SourceIndex.md#f66)

```cpp
levels[0] = prefilterHighlights(hdr, threshold, exposureRelatedOptions);
for (i = 1; i < levelCount; ++i)
    levels[i] = filteredDownsample(levels[i-1]);
for (i = levelCount-2; i >= 0; --i)
    levels[i] = combine(levels[i], filteredUpsample(levels[i+1]));
bloom = levels[0] * bloomStrength;
```

Bloom 会放大 emissive 的视觉影响，却不会改变其他物体接收到的光。能量/亮度意义必须在合成权重中保持一致，不能在每个层级无约束累加后再假定亮度不变。

### 13.5 ColorGrading：CPU 烘焙变换，GPU 查表

ColorGrading 对象将白平衡、色彩变换、曝光相关创作调整、对比度、lift/gamma/gain 或 slope/offset/power、曲线、色调映射、色域/输出变换等组合进 LUT。不同 ToneMapper 提供 Linear、ACES、ACES Legacy、Filmic、PBR Neutral、GT7、AgX、Generic、Display Range 等实现。[F67](SourceIndex.md#f67) [F68](SourceIndex.md#f68)

```cpp
// 创建/修改 ColorGrading 时，在 CPU 端生成资源。
for (vec3 encodedSample : lutGrid) {
    vec3 sceneLinear = decodeLogDomain(encodedSample);
    vec3 graded = applyConfiguredColorOperations(sceneLinear);
    vec3 mapped = selectedToneMapper(graded);
    lut[encodedSample] = encodeConfiguredOutput(mapped);
}
upload(lut);

// GPU 的图像 Pass：具体合成顺序以 color grading material 分支为准。
hdrCombined = combineHdrWithBloomFlareAndVignetteInputs(...);
uv3 = encodeAndClampLutDomain(hdrCombined);  // 对应 LogC 等编码约定
out = sampleColorGradingLut(uv3);
out = applySelectedDitherAndAlphaRules(out);
```

这段伪代码说明 CPU 预计算和 GPU 查表的分工，不表示所有色彩操作都能交换顺序。色调映射处理 HDR 到目标范围的映射，OETF 处理输出编码；按所选目标与 LUT 配置保证总链路只做所需次数，不能在最终 Shader 再无条件加一次 pow(1/2.2)。[F34](SourceIndex.md#f34) [F67](SourceIndex.md#f67) [F68](SourceIndex.md#f68)

FXAA 和空间放大随后处理已选定的输出图像。最终 View 可能写入用户 RenderTarget、共享 SwapChain 的一个 viewport 或最终输出；有无最终 blit、混合、缩放和 padding 处理由资源与输出配置决定。[F08](SourceIndex.md#f08)
