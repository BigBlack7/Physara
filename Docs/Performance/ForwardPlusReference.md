# Forward+ 画质与多灯成本参考

本基准由 D10-T05 验收，供 D11-T05 对齐 Forward、Forward+ 和 Deferred 的输入。构建基准为 `v2.0@2e94c06064d32f1cfcf2000e6e1b1c7020ac3155` 及两处签收的头文件注释。实测使用 Windows x64、MSVC 19.44.35228 Release、Intel RaptorLake-S Mobile、OpenGL 4.6.0 驱动 `32.0.101.6129`，有效交换区间为 0。正式着色包、回放程序、资产和原始样本均有内容摘要。 [D11-T05](../Developments/D08-D11-RenderPaths.md#d11-t05) · [签收输入](../../build/d10-t05/final/signed-inputs.json) · [最终验证](../../openspec/changes/archive/2026-11-26-verify-forward-plus-hdr-and-lighting-cost/tasks.md#final-verification)

<a id="shadow-reference"></a>
## 阴影与环境画质

按 D09 阴影参考重建地板、遮挡和 MASK 见证板，沿用获准的第 4 号源节点铜球及原 EXR。`corner` 的缩放为 `(0.16,0.14,1)`，三面角 ROI 中心为 `(-0.20,0.65)` m。用同一冻结内容版本 1、640 × 480、Physical Lighting、单样本 None AA 及相同预曝光渲染两路。正交相机在 `(0,0,8)` 朝 −Z，画幅 8 × 6 m，近平面 0.1 m、远平面 25 m。使用 f/4、1/16 s、ISO 100、0 EV，预曝光 `0.0032552082557231188`。EXR 强度 16，开启 IBL 与天空。有效 PCF 512 消费 3 组 11 层阴影，请求预算 16 MiB、最大阴影距离 `10.633333333333333 m`、四级 CSM、一个 spot 和一个六面 point。 [场景参数与许可](ForwardShadowReference.md#fixture) · [资产许可](../../Assets/README.md#forward-参考场景)

在显示映射前比较天空合成后的预曝光线性 RGBA16F 与主 Reverse-Z D32。有效画布全图的每通道最大差、RMSE 与差异像素数均为零，零的两种符号按数值一致处理。274,168 个地板深度样点满足解析式 `(25−8)/(25−0.1)`，绝对误差上限 `2e-6`。11 个 D09 遮挡和亮区标样采用参考坐标。三面角的 36 点蓝通道平均遮挡差为 `0.007064`。同直接灯下移除环境，接收面蓝通道降低 `0.0368824`，天空回到无环境清色。 [D09 判据](ForwardShadowReference.md#quality) · [最终差分](../../build/d10-t05/final/replay/build/d10-t05/a-correction-1/reference-difference.json) · [阴影及环境原始标样](../../build/d10-t05/final/replay/build/d10-t05/a-correction-1/reference-witness.json)

<a id="many-lights-fixture"></a>
## 多灯成本场景

- 单 View、640 × 480、Physical Lighting、单样本 None AA、相同物理曝光。阴影、IBL 和天空关闭。相机位于原点朝 −Z，正交半宽 4 m、半高 3 m，近平面 0、远平面 20 m。光源规模为 0、32 或 256 个实际可见局部灯，方向灯为零。
- 受光面是位于 `(0,0,-5.1)` m、缩放 `(3.9,2.9,1)` 的单块不透明板。局部四角为 `(−1,−1,0)`、`(1,−1,0)`、`(1,1,0)`、`(−1,1,0)`，法线 +Z，三角形索引为 `0,1,2,0,2,3`。材质为线性底色 `(0.7,0.7,0.7,1)`、metallic 0、roughness 0.75 的核心介质，扩展材质覆盖为零。5.1 m 接收面位于 Froxel 层内。原 5 m 校准面位于层边界，受光片元进入保守退路。
- 第 `i` 盏灯取 `s=(73*i)%256`、`c=s%16`、`r=s//16`，位置为 `(-3.6+0.48*c, -2.625+0.35*r, -4.78)` m。点光强度 8 cd、range 0.83 m，RGB 的第 `i%3` 个分量为 1，其余为 0.35。每档使用此序列的前 0、32 或 256 盏。
- Forward+ 使用 32 × 32 像素格、24 个深度层、7,200 个格及常规 262,144 条有限索引额度。256 灯强制溢出组把有限索引额度设为 0，沿用同一灯表与画质。

| 可见局部灯 | 冻结输入 SHA-256 |
|---:|---|
| 0 | `d8dacaabe1d956e782dd41bab8b975f784e491cd95caad116e35ff85e1cfba96` |
| 32 | `d53a5883b980b54ae41754e8a4aa76bfa2bdb88fa095047b94281a46d06913d7` |
| 256 | `872fbb43e6c8b01fb724e07b91d32aeb5b2d1ac088a61568dcd49de33c5d7caf` |

原生画质回放证明每个规模及强制溢出控制与对应完整灯表的 HDR、D32 全图一致。匹配回放从实际着色分支和局部灯访问记录候选数。常规 0、32、256 灯每像素实际访问分别为 0、0–5、6–30 行，退路比例均为 0。强制溢出组为 900/7,200 个格（12.5%），实际退路为 289,536/289,536 个受光像素（100%），每个像素遍历 256 行。 [场景生成与回放](../../build/d10-t05/a-correction-1/Scenes.inc) · [规模差分](../../build/d10-t05/final/replay/build/d10-t05/a-correction-1/scale-difference.json) · [退路观测](../../build/d10-t05/final/replay/build/d10-t05/a-correction-1/fallback-ratios.json)

<a id="cost"></a>
## 实测分布和工作量

每组取三轮交错窗口，每轮确认正式路径后暖机六个接受帧，再采 24 个接受帧。七组共 504 帧。整图和颜色 GPU 区间均匹配到原提交的 Ready 结果。CPU 工作包括 UI、准备、构图、编译及执行，分箱、载荷准备和写入录制分别取实际区间。GPU 颜色区间包围 `ShadeOpaque` 的真实绘制，零灯对照用来估计候选查询、访存和局部照明工作量带来的增量。独立 GPU 传输区间未采样，FrameSlot 和 PresentWait 的分布留在原始复算中。 [测量合同](../Architecture.md#benchmark) · [逐票据复算](../../build/d10-t05/final/replay/build/d10-t05/a-correction-1/timing-summary.json)

表中时间单位为 ms，数值为 median／p95。

| 局部灯和路径 | CPU 工作 | CPU 分箱 | CPU 载荷准备 | CPU 上传录制 | GPU 颜色 | GPU 整图 |
|---|---:|---:|---:|---:|---:|---:|
| 0 · Forward | 0.777 / 1.529 | — | — | — | 0.287 / 0.312 | 0.596 / 0.683 |
| 0 · Forward+ | 1.982 / 2.580 | 0.010 / 0.022 | 0.666 / 1.002 | 0.286 / 0.545 | 0.969 / 1.003 | 4.490 / 5.976 |
| 32 · Forward | 0.937 / 1.570 | — | — | — | 7.653 / 7.911 | 8.801 / 9.290 |
| 32 · Forward+ | 2.336 / 3.225 | 0.211 / 0.381 | 0.643 / 1.017 | 0.302 / 0.756 | 1.951 / 2.022 | 5.638 / 7.307 |
| 256 · Forward | 1.180 / 2.523 | — | — | — | 55.200 / 55.215 | 56.472 / 58.069 |
| 256 · Forward+ | 4.953 / 6.131 | 2.460 / 3.133 | 0.719 / 0.943 | 0.371 / 0.703 | 8.249 / 8.511 | 12.956 / 15.808 |
| 256 · Forward+ 强制溢出 | 3.685 / 5.903 | 0.991 / 2.126 | 0.691 / 1.688 | 0.347 / 1.119 | 78.278 / 78.308 | 83.701 / 85.412 |

各组整图均发出 8 次 Draw，Forward 与 Forward+ 分别有 41 和 45 次绑定。每个 Forward+ 暖帧发出 4 笔 Froxel 写入，共 2,163,388 B，同时出现 4 个新原生缓冲存储，零灯和强制溢出组仍交付这些固定块。Forward 票据 243 的 3 个图纹理与 1 个图缓冲分配随原帧统计留存。 [实际上传分项](../../build/d10-t05/a/timing-256.csv) · [零灯原帧](../../build/d10-t05/a/timing-0.csv)

各自扣除零灯颜色区间中位数后，32 灯的 Forward／Forward+ 增量为 7.366／0.982 ms，256 灯为 54.913／7.280 ms。256 灯强制溢出相对 Forward+ 零灯底线增加 77.309 ms。分轮方向一致。收益取决于候选收缩，固定分箱与交付成本在零灯时仍可见，退路付出完整遍历成本并保留 HDR。 [分轮结果](../../build/d10-t05/final/replay/build/d10-t05/a-correction-1/timing-summary.json)

<a id="replay"></a>
## 重放与适用范围

按上文生成两类冻结输入，使用正式 Runtime 的 View、图提交、着色包与读回链。先确认本轮真正接受的策略、工作色与深度版本、预曝光、有效矩形和阴影／环境身份，再比对 HDR/D32。保持同一场景的光源和材质，按各自实际有效设置登记 D11 的附件量和扩展表面占比。阶段验收还覆盖无 range、聚光边界、混合溢出、无限远透视、零近平面正交、留边、远移和空灯。大坐标标样以实际 D32 和覆盖证明接收面存在，原远移的小近板丢失覆盖作为负例保留。 [验收范围](../../openspec/changes/archive/2026-11-26-verify-forward-plus-hdr-and-lighting-cost/tasks.md#final-verification) · [原生复验](../../build/d10-t05/final/native-verify.json)

临时回放、观测包、脚本和原始数据按签收输入及回放计划定位。上述画质和成本结果适用于登记的设备、驱动与配置。 [命令与输入清单](../../build/d10-t05/a-correction-1/input-manifest.json) · [最终重放计划](../../build/d10-t05/final/replay-plan.json)
