# Physara 任务实现方案

> 本文档与 `Docs/Development.md` 的任务编号一一对应。
> 每个子任务当前只保留简要描述。开始实施前，在对应位置补充现状核验、方案选型、涉及文件与验证方式；完成后记录结果与偏差。
> 性能任务由人工启动渲染器或抓帧完成测量，并以阶段 0 基线为对照。GPU 数据契约变更使用 `Tools/verify_gpu_contracts.py` 检查。

---

## 阶段 0 — 前置测量与契约检查

### 模块 P — 性能基线与 GPU 契约

#### P.1 手工性能基线

**现状核验：** Editor 已有 Scene View 叠加层、`FrameStatistics` 与 Pipeline Benchmark（warmup 120 / sample 300）。无需新计时器。Scene View 尺寸随停靠布局变化，必须以首次有效 B 机采样的 `Size: W x H` 锁定。

**方案选型：** 数字落本文件，不另开 Baseline 文件。电脑 B 必填，电脑 A 预留空位。轮次覆盖同一张表，只改 `轮次` 标记。三条管线只改 Render Path。

##### 锁定配方

| 项 | 锁定值 |
|---|---|
| 构建 | Release（当前 MSVC / CMake） |
| 场景 | `Assets/Scenes/default.scene.json` |
| 窗口 | `1900 × 1000`，首次停靠布局出现后不要再改窗口或重排 Dock |
| 显示模式 | Docked |
| 相机 | 加载场景后让编辑器对齐 `MainCamera`，之后禁止轨道 / 飞行 / 移动相机 |
| 唯一变量 | `Render Path`：Forward / Forward+ / Deferred |
| VSync | 关 |
| World Grid | **采样时关**（编辑器叠加，不是场景内容） |
| Debug View | None |
| Skybox | On；默认路径为空则保持为空，不要临时选一张 HDR |
| Skybox Intensity | 1.0 |
| Tone Mapping | ACES |
| Bloom | On；Threshold 1.0 / Knee 0.5 / Intensity 0.12 / Scatter 0.7 |
| Anti-Aliasing | **三条管线都用 FXAA Quality**（不要给 Forward 单独开 MSAA） |
| Shadows | On；Filter PCF 3x3；Resolution 2048；Cascades 4；其余滑条保持默认 |
| Benchmark | Enabled；Warmup 120；Sample 300；等 `complete` 再抄 median / p95 |

**帧缓冲锁定：** 电脑 B 第一次有效采样时，把 Scene View 叠加层的 `Size: W x H` 写入下方环境块。之后同机采样必须相同；Dock 被打乱则恢复布局或作废该次采样。

##### 操作步骤

1. Release 构建并启动。可用环境变量 `PHYSARA_STARTUP_SCENE=Scenes/default.scene.json`，或在 Content Browser 加载 `Assets/Scenes/default.scene.json`。
2. 等默认 Dock 布局稳定。不要再拖面板、改窗口大小。
3. 确认相机已对齐场景 `MainCamera`，之后不要再操作视口导航。
4. 按锁定配方核对 Renderer Settings（尤其 World Grid 关、AA = FXAA Quality、VSync 关）。
5. 打开 Pipeline Benchmark，Enabled，120 / 300。
6. 抄 Scene View `Size` 到电脑 B 环境块（仅第一次）。
7. 对 Forward、Forward+、Deferred 各做一次：只改 Render Path → Restart Benchmark → 等 complete → 抄面板数字 → 看画面写视觉备注。
8. 缺字段写 `unavailable`，不要编造。RenderDoc 后补不增加轮次。

##### 环境 — 电脑 B（低性能台式，本轮必填）

- 轮次：`0`
- 日期：`2026-08-25 / 2026-08-28`
- CPU：Intel i7-7700；GPU：GTX 1060 6GB；驱动：580.xx；RAM：32GB；OS / 编译器：Windows / MSVC Release
- git commit：`unavailable`
- 窗口：`1900 x 1000`
- Scene View Size：`889 x 611`（首次有效采样，后续同机必须相同）
- VSync：Off；AA：FXAA Quality；Shadow：PCF 3x3 / 2048 / 4 cascades
- 备注：UI 中 World Grid 勾选为开，但 benchmark 采样期间网格被强制隐藏（`worldGridEnabled && !benchmarkEnabled`），grid GPU 实测 0.00。rdc 抓帧在 benchmark 关闭时进行，包含 WorldGrid pass（~0.05ms）与 EditorUI。

##### 环境 — 电脑 A（高性能笔记本，预留）

- 轮次：`—`
- 全部字段：`unavailable`（禁止把 B 的数字抄进本槽）

##### 电脑 B 轮次 0 测量表

数据来源：叠加层截图（benchmark complete 状态）+ 对应 rdc。场景 889x611，相机 EV100 14.97，Skybox 空路径（pass 被跳过）。

| 字段 | Forward | Forward+ | Deferred |
|---|---|---|---|
| Frame CPU ms（整帧 / UI build / Scene / UI draw） | 22.76 / 3.61 / 18.87 / 0.11 | 22.26 / 3.64 / 18.34 / 0.11 | 33.59 / 2.75 / 30.53 / 0.12（单帧含尖峰） |
| CPU scene（collect / cluster / graph build / graph exec） | 0.18 / 0.00 / 0.01 / 18.66 | 0.25 / 0.17 / 0.07 / 18.06 | 0.18 / 0.12 / 0.06 / 30.33 |
| CPU pass（主几何 / Light / Trans / Post） | Fwd 1.55 / — / 0.85 / 3.90 | Fwd 1.34 / — / 0.63 / 3.31 | GBuf 1.92 / Light 10.73（单帧尖峰）/ 0.75 / 3.37 |
| GPU frame ms | 21.17 | 21.48 | 22.32 |
| GPU pass（Sh / 主几何 / Light / Tr / Grid / Post） | 14.92 / 5.84 / — / 0.01 / 0.00 / 0.39 | 14.86 / 6.20 / — / 0.01 / 0.00 / 0.40 | 15.41 / GBuf 6.01 / 0.47 / 0.03 / 0.00 / 0.40 |
| Post GPU（prefilter / down / up / composite） | 0.07 / 0.05 / 0.05 / 0.22 | 0.07 / 0.06 / 0.05 / 0.22 | 0.07 / 0.06 / 0.05 / 0.22 |
| Benchmark CPU med / p95 | 17.94 / 20.93 | 19.10 / 21.34 | 18.99 / 25.66 |
| Benchmark GPU med / p95 | 20.45 / 21.89 | 20.63 / 22.47 | 21.60 / 25.66 |
| Draws/Cmds、Instances、Tris | 207/192、211、2,493,601 | 207/192、211、2,493,601 | 208/192、212、2,493,602 |
| Visible O/U/T、Lights、Clusters/Refs/Max/Ov、Mat/Sets | 40、39/0/1、3/0、0/0/0/0、36/36 | 40、39/0/1、3/2、5888/7728/2/0、36/36 | 40、39/0/1、3/2、5888/7728/2/0、36/36 |
| Upload MB | 0.01 | 0.09 | 0.09（GBuffer 12.43 MB 显存） |
| Submit（direct / MDI runs/cmd / breaks M/G/I/S） | 1 / 10 runs 191 cmd / 0/0/0/0 | 1 / 10/191 / 0/0/0/0 | 37 / 9 runs 155 cmd / 35/0/0/36 |
| GL（RP / Draw/MDI/Cmd / Tex/Samp / Bar C/E/S） | 21 / 16/10/191 / 22/2 / 25/0/25 | 21 / 16/10/191 / 22/2 / 25/0/25 | 22 / 53/9/155 / 26/9 / 30/0/30 |
| 视觉（操作者观察） | 正常，无异常 | 正常，无异常 | 正常，无异常 |
| RenderDoc | `8-25-B-Forward.rdc`（已分析） | `8-25-B-Forward+.rdc`（已分析） | `8-25-B-Deffered.rdc`（已分析） |

##### Forward rdc 分析（2026-08-28）

帧结构：GPUSceneUpload(1) → Shadow(165) → FrameUniformsUpload(1) → ForwardOpaque(42) → WorldGrid(2) → ForwardTransparent(2) → PostProcess(47) → EditorUI(31)，共 300 actions / 238 draws / 7 clears / 21 个 render pass（FBO 切换）。全帧 **0 个 glMemoryBarrier**：25 个候选全被 `framebufferOrderedTransition` 短路抑制——FBO 附件写→后续采样的转换在 GL 语义下本就不需要显式 barrier，抑制正确。

1. **Shadow 是最大热点：GPU 14.92ms / 帧 21.17ms ≈ 70%**。4 个 cascade 各画一遍全场景 38 个提交（2 MDI 调用），不做按 cascade 裁剪；667,092 索引（22.2 万三角形）的大网格在每个 cascade 画一遍，单 cascade ~2.0-2.4ms，四次 ≈ 8.5ms；加上 313K 索引网格 ×4 ≈ 2.4ms。两个大网格贡献 shadow 成本的大头。每 cascade 还有完整状态重设（program/VAO/buffer）+ 独立的间接命令上传（IBuf 10 次/帧）。
2. **ForwardOpaque GPU 5.84ms**，39 个 draw。大网格单 draw 2.86ms。rdc 抓帧帧里是 39 个**直接 instanced draw**（逐个 `glBindTextures` + draw，传统逐纹理绑定路径），与面板 steady-state 的 `direct 1, MDI 10/191`（bindless 合并生效）不一致——见下方存疑项。
3. **PostProcess GPU 仅 0.39ms 但 CPU 3.90ms**：15 个 draw（prefilter + 6 downsample + copy + 6 upsample + composite），每个 pass 独立 FBO/program/viewport/buffer/texture 全设置，CPU 提交开销是 GPU 的 10 倍。889x611 下 6+6 级 bloom mip 偏多。
4. Shadow 深度图 D32 2048² ×4 layer 数组；CSM 4 级 + PCF3x3 采样端仅 ~0.5ms（在 Forward 内），成本全在生成端几何。
5. EditorUI 31 events / ~0.1ms，正常。

##### Forward+ rdc 分析（2026-08-28）

帧结构与 Forward 完全一致（300 actions / 238 draws / 7 clears），pass 序列仅 `ForwardOpaque` 换成 `ForwardPlusOpaque`。

1. **Cluster 链路已生效**：Forward+ PS 声明 `ClusterEntryBuffer` / `ClusterLightIndexBuffer` / `LightBuffer` SSBO；面板显示 5888 clusters / 7728 refs / max 2/overflow 0。CPU cluster build 0.07ms，GPU 端 cluster 开销很小：面板 Fwd 5.84ms → F+ 6.20ms（+0.36ms），rdc 中大网格 draw 2.86ms → 5.17ms（rdc 计时含重放开销，量级供参考，以面板为准）。
2. **阴影结构与成本同 Forward**：667K 索引大网格在 4 个 cascade 各画一遍（1.93 / 2.35 / 1.92 / 1.98ms，rdc 计时），无 cascade 裁剪。
3. **抓帧帧仍为传统逐纹理绑定 + 直接 draw**（39 个 `glDrawElementsInstancedBaseVertexBaseInstance`），与 Forward 抓帧相同——旁证 RenderDoc 下 bindless 未生效（RenderDoc 对 GL `ARB_bindless_texture` 支持受限，扩展可能被隐藏，导致回退传统路径）。因此 rdc 的提交形态不代表 bindless 生效时的 steady-state；MDI 合并收益以面板统计为准（`MDI 10 runs / 191 cmd`）。

##### Deferred rdc 分析（2026-08-28）

帧结构：GPUSceneUpload → Shadow(165) → FrameUniformsUpload → **GBuffer(45)** → **DeferredLighting(3)** → WorldGrid → ForwardTransparent → PostProcess → EditorUI，共 307 actions / 239 draws / 11 clears / 22 render pass。

1. **GBuffer MRT 布局**：4 张颜色附件 + D24S8 深度——RT0 BaseColor `R8G8B8A8_UNORM`、RT1 Normal `R16G16_FLOAT`（八面体打包）、RT2 Material `R8G8B8A8_UNORM`、RT3 Emissive `R16G16B16A16_FLOAT`。合计 18B/px 颜色 + 4B 深度，889x611 下 12.43MB，与面板 `GBuffer 12.43 MB` 一致。RT3 用全 16 位浮点存 emissive 偏宽（R11G11B10 可省一半），记入 P.3 证据。
2. **GBuffer pass 开头 5 次独立 clear**（4 色 + 1 深度分开调 glClear），属可合并的小开销。
3. **DeferredLighting = 1 次全屏 draw**，绑 3 张 GBuffer 采样纹理 + Cluster/Light SSBO，GPU 面板 0.47ms，无浪费。
4. **GBuffer 提交全是直接 draw（rdc 39 个；面板 direct 37 / breaks 35/0/0/36）**：`GBufferPass::CanMerge` 在 bindless 分支仍要求 `meshKey` 相等（`GBufferPass.cpp:195-197`），跨网格合并永远失败；而 `ForwardOpaquePass` 的 bindless 分支直接放行（`ForwardOpaquePass.cpp:422-425`）。同一帧数据两条路合并策略不一致，Deferred 因此多发 ~2.3 倍直接 draw（53 vs 16）。注意 meshKey 相等≈同网格，而跨网格同材质合并正是 MDI 的意义所在——该条件疑似写反/写错位置，属模块 3 审计范围的具体缺陷。
5. Shadow 与前两条管线完全相同（大网格 ×4 cascade ≈ 7.9ms rdc 计时）。

##### 三管线 rdc 交叉结论

- **GPU 瓶颈一致是阴影生成**：三份抓帧均为 4 cascade × 全场景几何，无按 cascade 视锥裁剪，两个大网格（667K + 313K 索引）贡献 shadow 成本大头。模块 5.1 证据确凿。
- **三条管线在本场景 GPU 差距 ≤1.2ms**（21.17 / 21.48 / 22.32），Deferred 无收益（GBuffer 带宽 + 合并失效的 CPU 开销）——在 3 灯光的小场景属预期。
- **CPU 提交 ≈ GPU 帧时**：graph exec 18.7-18.9ms（Forward/F+），Deferred 30.5ms。CPU 侧主要耗在 pass 提交与状态设置；Post 链 15 个全屏 pass 花 3.3-3.9ms CPU 换 0.4ms GPU。
- **GL barrier 模型健康**：全帧 0 个 glMemoryBarrier，25-30 个候选全部被"FBO 附件写→后续采样"短路正确抑制（GL 语义下本就不需要）。0.12 的优先级应下调。
- **RenderDoc 抓帧不含 bindless**：三份抓帧的主几何 pass 均为传统逐纹理绑定 + 直接 draw，与面板 steady-state（bindless + MDI 合并）不同。结论：rdc 用于 pass 结构与 GPU 耗时分析；提交形态/bindless 行为以面板为准。

##### 存疑项（已解决）

- ~~rdc 抓帧帧为传统逐纹理绑定 + 直接 draw，与面板 bindless MDI 不一致~~ → 已确认：RenderDoc 对 GL `ARB_bindless_texture` 支持受限，抓帧环境回退传统路径。rdc 用于 pass 结构/GPU 耗时分析，提交形态以面板为准。

##### 完成记录

轮次 0 已于 2026-08-25/28 完成采集：三管线面板 + benchmark 数据（Deferred 的 benchmark 于 8-28 补测）、三份 rdc 全部分析完毕。硬件：i7-7700 / GTX 1060 6G / 驱动 580 / 32G RAM。视觉：三条管线均正常。Scene View 锁定 889x611。偏差：World Grid 采样期间 UI 勾选为开，但 benchmark 采样时强制隐藏，grid GPU 实测 0.00，不影响数据。

#### P.2 GPU CPU↔GPU 契约检查

**现状核验：** `Tools/verify_gpu_contracts.py` 原先只比对常量、绑定/枚举值和结构字节尺寸；注释仍写已失效的 `P.4`。`ObjectData` 在 C++ 为 4×`uint32`，GLSL 为 `uvec4`，属有意等价。

**方案选型：** 保留单脚本。增加硬检查：规范化后的成员类型序列（4 个连续标量折叠为 vec4/uvec4/ivec4）、已知 UBO=`std140` / SSBO=`std430` 前缀。软告警（绑定重载、死枚举、对齐风险）不失败。CPU `static_assert` offsetof 仍只留在头文件。

**涉及文件：** `Tools/verify_gpu_contracts.py`、`Engine/Renderer/GPUContracts.hpp`、`Docs/Physara.md`。

**验证：** 仓库根目录 `python Tools/verify_gpu_contracts.py`。

**运行结果（本 change）：**

```
warning: binding=0 被多义重载: Camera, FrameUniforms
warning: binding=4 被多义重载: InstanceIndices, PostProcessSettings, SkyboxSettings, WorldGridSettings
warning: binding 枚举 RenderSettings (宏 PHYSARA_BINDING_RENDER_SETTINGS) 无 shader 引用,疑似死枚举
warning: binding 枚举 Shadow (宏 PHYSARA_BINDING_SHADOW) 无 shader 引用,疑似死枚举
warning: binding 枚举 IBL (宏 PHYSARA_BINDING_IBL) 无 shader 引用,疑似死枚举
GPU contract verification passed.
```

硬检查通过。残留告警为既有绑定卫生问题，不在本模块修复。已删除 `P.4` / `pre-refactor-validation-guards` 注释。

#### P.3 阶段一性能优先级复核

依据：电脑 B 轮次 0（i7-7700 / GTX 1060 6G，889x611，default.scene.json，2.49M 三角形，3 灯）。GPU 帧 ~21-22ms，其中阴影 ~15ms（70%）；CPU 提交 18.7-18.9ms（Forward/F+）≈ GPU 帧时，Deferred 30.5ms。三管线 rdc 结构分析见 P.1。

**测量后攻击顺序（优先级以 B 机为准，不改任务编号）：**

1. **5.1 CSM 成本拆解 —— 最高优先**。证据：shadow GPU 14.9-15.4ms 占帧 70%；4 个 cascade 无差别重画全场景（rdc：38 提交 ×4，667K+313K 索引大网格各画 4 遍 ≈ 8-11ms）。按 cascade 视锥裁剪/距离剔除是最大单项收益。
2. **8.1 / 8.2 Bloom 收敛与合并 —— 高优先（CPU 侧）**。证据：Post 链 15 个全屏 pass，CPU 3.3-3.9ms vs GPU 0.39ms；每个 pass 全套 FBO/program/纹理重绑。889x611 下 6+6 级 mip 明显过剩。
3. **3.2 / 3.3 提交合并审计 —— 高优先（CPU 侧）**。证据：Deferred 下 `GBufferPass::CanMerge` 的 bindless 分支要求 meshKey 相等导致合并失效（35 break / 37 direct），与 ForwardOpaque 策略不一致；CPU graph exec Deferred 30.5ms vs Forward 18.7ms。属具体缺陷，不止"评估"。
4. **0.3 / 0.6 提交热循环与 CommandList 拆分 —— 维持高优先**。证据：CPU 提交 ≈ GPU 帧时，弱 CPU 上提交路径是帧率地板之一。
5. **2.1 / 2.2 RenderGraph pass 组织 —— 维持中优先**。证据：21-22 个 render pass/帧，per-pass 固定开销明显（Post 链最典型）。
6. **6.2 Deferred 无效区域 —— 下调为观察项**。证据：3 灯小场景 Deferred 已无收益（GPU +1.2ms、GBuffer 12.43MB），灯光规模上来前不投优化。附带发现：RT3 Emissive 用 R16G16B16A16F 偏宽，可评 R11G11B10。
7. **0.12 精确 barrier —— 下调**。证据：全帧 0 个 glMemoryBarrier，25-30 个候选被 FBO 有序转换正确短路；不存在全局 barrier 泛滥问题。保留任务但降级为正确性审计而非性能。
8. **7.2 / 7.3 Cluster —— 下调**。证据：5888 clusters / 7728 refs / max 2 / overflow 0，CPU 0.07ms，GPU 增量 +0.36ms。当前规模无问题。
9. **0.13 bindless 驻留管理 —— 维持，但注意测量环境**。证据：RenderDoc 下 bindless 不可用（抓帧回退传统路径），rdc 无法验证 bindless 行为；面板统计是唯一证据源。
10. 卫生类任务（模块 1、H1、H2）维持原文顺序，不占性能预算。

**新增问题（当前计划未覆盖，建议入模块 3）：** GBuffer 与 Forward 的 MDI 合并条件不一致（见上第 3 条），且 GBuffer 开头 5 次独立 clear 可合并。

**方法学记录：** 本机 rdc 内提交形态（传统逐纹理路径）≠ 实机 steady-state（bindless+MDI）。后续凡是"提交形态/合并率"类问题以面板统计为准；rdc 仅用于 pass 结构与 GPU 耗时。电脑 A 数据到位前，所有优先级结论仅对 B 机负责。

---

## 阶段一 — 核心架构地基

### 模块 0 — RHI / OpenGL Backend 提交层

#### 0.1 命令录制/回放层
明确可选命令录制和回放的生命周期、数据所有权与提交语义。

#### 0.2 OpenGL 状态缓存
完善状态缓存失效与外部状态互操作。

#### 0.3 提交热循环去重
整理既有去重逻辑并保留必要统计。

#### 0.4 Descriptor 惰性绑定
评估仅提交发生变化资源槽位的绑定策略。

#### 0.5 PSO 编译与 program binary 缓存
评估异步编译和磁盘缓存的架构、失效与回退策略。

#### 0.6 拆分 OpenGLCommandList
按职责拆分过大的命令列表实现。

#### 0.7 枚举到 GL 映射收敛
统一纯类型映射的来源，并补全 storage image 对 2D array 等维度的 layered 绑定语义。

#### 0.8 拆分 RHIDefinitions
按职责拆分 RHI 公共定义。

#### 0.9 ImGui 后端走 RHI
将 ImGui 资源和绘制提交迁入 RHI 边界。

#### 0.10 缓存碰撞与失效
加固管线和网格缓存的身份比较与生命周期失效。

#### 0.11 描述符模型统一
统一资源集与 GPU set index 的真实语义。

#### 0.12 精确 barrier
建立精确资源访问同步和 RenderGraph 依赖规则。

#### 0.13 bindless 纹理驻留
管理 bindless handle 驻留、预算和延迟回收。

#### 0.14 上传同步优化
基于实际测量评估上传路径与 fence 优化。

#### 0.15 GL 调试回调治理
聚合、限频并保留高严重度调试消息。

#### 0.16 Reverse-Z RHI 前置
建立 Reverse-Z 所需 depth compare、clear 与 clip-depth 能力，并统一 Depth24Stencil8、Depth32F 等深度格式的附件识别与映射。

### 模块 1 — 公共基础设施收敛

#### 1.1 MaxValue 去重
清理重复最大值工具并改用标准库。

#### 1.2 RGBuilder 冗余 API 清理
移除无价值短转发，保留明确资源访问 API。

#### 1.3 工具函数归位
收敛通用工具，保留领域工具的独立语义。

### 模块 2 — RenderGraph / Renderer 框架

#### 2.1 拆分 BuildRenderGraph
按渲染职责拆分 RenderGraph pass 注册。

#### 2.2 PassContext 收敛
消除各 pass 的重复帧级上下文填充。

#### 2.3 辅助职责剥离
从 Renderer 剥离 benchmark、capture 等辅助职责。

#### 2.4 Resize 重建收敛
处理高频 resize、最小化和恢复的资源重建，并保证导入纹理的描述完整保留颜色空间等资源语义。

#### 2.5 Renderer 生命周期
补齐 shutdown、重初始化和 GPU 资源释放。

#### 2.6 Debug view 收敛
将 debug view 枚举、参数来源和 shader 分发收敛为单一契约，移除各渲染路径间的魔法下标。

### 模块 H2 — Scene / Resource / Serialization / Platform 卫生

#### H2.1 SceneSerializer 单点维护
统一序列化字段维护、版本迁移和原子加载；未知版本须显式拒绝或迁移，任何失败不得清空原 Scene。

#### H2.2 GLTFLoader 职责分层
分离文档解析、资源注册与场景构建；收敛重复 Material→MaterialComponent 转换，保证字段完整传递并避免重复解析同一文档。

#### H2.3 Scene 职责拆分
拆分实体、关系、相机和变换职责，保证查询无副作用；区分纯主相机查询与显式单相机规范化，并防御畸形层级销毁。

#### H2.4 AssetManager 线程与缓存契约
明确资源管理器线程访问、缓存淘汰和 generation 回绕语义。

#### H2.5 Platform 生命周期
完善窗口库生命周期、输入、运行时工厂归属、VSync 配置和文件系统关闭顺序。

#### H2.6 纹理颜色空间模型
分离源图数据与采样用途/视图的颜色空间解释。

#### H2.7 文件系统与构建边界
收敛路径安全、字体资产路径、平台支持和构建可移植性。

#### H2.8 IBL 异步作业生命周期
管理异步 IBL 作业的取消、关闭和代际结果。

### 模块 H1 — Editor / ImGui 架构

#### H1.1 IPanel 与面板注册表
建立面板接口和注册驱动的 Editor 组织方式。

#### H1.2 EditorContext / EditorBridge
拆分编辑器状态与渲染器交互边界。

#### H1.3 绘制阶段分离
分离场景更新、渲染、UI 构建和呈现阶段，并统一 Editor、Renderer 与引擎 `Time` 的 delta time 来源。

#### H1.4 编辑器源码收敛
迁移 inline 实现并拆分过长 UI 函数。

#### H1.5 输入路由集中化
建立统一的输入捕获和事件消费优先级，并让 EditorCameraInputFrame 完整快照所消费的输入状态。

#### H1.6 TRS 写回与相机同步
处理变换写回失败、特殊矩阵和相机旋转基；明确编辑器相机与场景多相机/主相机策略，禁止隐式破坏场景相机数据。

---

## 阶段二 — 渲染主干纠错与收敛

### 模块 3 — RenderProxy / CommandExecutor

#### 3.1 SidedBuckets 收敛
抽取 bucket 公共容器行为，保留领域职责。

#### 3.2 实例与提交去重整理
整理既有实例合并和 direct/indirect 提交能力。

#### 3.3 Draw 排序与合并率
依据实测评估排序正确性、overdraw 与实例合并；审计排序键位宽、截断碰撞和完整材质身份比较，保证正确性优先于局部性优化。

#### 3.4 MDI 合并条件不一致修复
**现状核验（P.1 轮次 0 实测）：** `GBufferPass::CanMerge` 的 bindless 分支要求 `meshKey` 相等（`GBufferPass.cpp:195-197`），而 `ForwardOpaquePass::CanMergeIndirectRun` 的 bindless 分支直接放行（`ForwardOpaquePass.cpp:422-425`）。Deferred 面板实测：direct 37 / MDI 9 runs 155 cmd / breaks M/G/I/S 35/0/0/36；Forward 面板：direct 1 / MDI 10 runs 191 cmd / breaks 0/0/0/0。GBuffer pass 开头 5 次独立 `glClearNamedFramebuffer*`（4 色 + 1 深度）。
**方案选型：** 实施时先判断 GBuffer 的 meshKey 条件是否有正确性理由（如 per-draw 网格相关绑定）；若无，对齐到 Forward 的 bindless 合并语义。clear 合并到 pass 入口的 FBO 绑定处一次完成。
**涉及文件：** `Engine/Renderer/Passes/GBufferPass.cpp`、`Engine/Renderer/Passes/ForwardOpaquePass.cpp`（对齐参考）。
**验证：** Deferred 面板 Submit 行 breaks M 显著下降、direct 提交数接近 Forward 量级；`python Tools/verify_gpu_contracts.py` 不受影响；视觉回归人工观察。

### 模块 4 — Material 数据层与扩展策略

#### 4.1 Material 职责校正
校正材质实例、签名、注册表和纹理缓存的职责。

#### 4.2 材质扩展路线决策
选择胖结构/uber shader 或 feature variant/分桶路线。

#### 4.3 MaterialSignature 同步机制
防止材质字段变化遗漏签名更新。

#### 4.4 材质数据流收敛
移除 GPU 打包链中的不必要组件副本和跳数。

### 模块 5 — ShadowPass

#### 5.1 CSM 成本拆解
根据性能基线分析级联、裁剪、分辨率和提交成本。

#### 5.2 Bias 常量治理
统一阴影 bias、padding、clamp 与参数单位。

#### 5.3 Shadow filter 评估
比较不同阴影滤波方案的画质与性能。

#### 5.4 Alpha-mask shadow caster
实现 alpha-mask 材质的阴影采样与 discard。

### 模块 6 — GBuffer / Deferred / Forward

#### 6.1 曝光职责统一
统一各 radiance 来源与后处理的曝光链路；明确 pre-exposure 只在 CPU 或 GPU 的单一归属层应用，定义 `ev100` 与 exposure compensation 的组合公式，消除路径间隐式常量偏移。

#### 6.2 Deferred 无效区域优化
依据测量决定是否优化 Deferred 背景区域。

#### 6.3 BRDF 一致性审计
审计 f90、漫反射和镜面 BRDF 的模型选择。

### 模块 7 — IBL / ClusteredLighting

#### 7.1 SH 系数一致性
检查 SH 基函数、顺序、卷积和归一化一致性。

#### 7.2 Cluster slice 边界
检查 CPU/GPU slice 公式及边界行为。

#### 7.3 Cluster 重建策略
依据光源规模评估全量和增量重建。

### 模块 8 — PostProcess / Bloom

#### 8.1 Bloom mip 收敛
评估 Bloom mip 数、pass 数和资源布局。

#### 8.2 Bloom pass 合并
评估 prefilter、downsample 和资源通道合并。

### 模块 9 — 收尾清理

#### 9.1 死代码复查
清理确认无调用的代码、接口与注释。

#### 9.2 Physara.md 同步
按实际代码结构更新工程说明文档。

---

## 阶段三 — 材质、光照与相机功能拓展

### 模块 10 — 材质扩展机制

#### 10.1 可插拔材质架构
建立材质布局、feature mask、pipeline 与 shader 组织策略。

#### 10.2 KHR_materials 解析框架
建立 glTF 材质扩展的通用解析和注册机制。

#### 10.3 PrepareMaterial 扩展点
建立着色模型输入准备的扩展边界。

#### 10.4 可插拔 BRDF
将硬编码 BRDF 重构为按模型分发的架构。

### 模块 11 — Sheen / Cloth

#### 11.1 Sheen CPU 数据
增加 sheen 数据、纹理槽、序列化和编辑器支持。

#### 11.2 KHR_materials_sheen 解析
解析 sheen glTF 扩展。

#### 11.3 Charlie 与 Neubelt
实现 sheen 所需 NDF 与可见性项。

#### 11.4 Cloth 着色模型
接入 Cloth/Sheen 的材质和光照路径。

### 模块 12 — Clearcoat

#### 12.1 Clearcoat CPU 数据
增加 clearcoat 数据、纹理槽与 GPU 契约。

#### 12.2 KHR_materials_clearcoat 解析
解析 clearcoat glTF 扩展。

#### 12.3 Clearcoat BRDF
实现双层 clearcoat 的可见性和组合公式。

#### 12.4 Clearcoat 法线
实现 clearcoat 独立法线纹理通道。

### 模块 13 — Subsurface / Transmission / Refraction

#### 13.1 CPU 数据扩展
按近似方案分批扩展次表面、传输、体积和折射数据。

#### 13.2 glTF 扩展解析
解析 transmission、volume、ior 与 dispersion 扩展。

#### 13.3 次表面散射模型
实现适用于实时渲染的次表面散射近似。

#### 13.4 折射方案
建立折射材质与屏幕空间折射的接入策略。

### 模块 14 — 物理光照

#### 14.1 IES Profile
接入 IES 资产、GPU 数据和光照采样。

#### 14.2 Area Light
实现面积光的实时数据、求值和编辑器支持。

#### 14.3 点光/聚光阴影
实现点光与聚光阴影的分配、更新和光照接入。

### 模块 15 — 物理相机

#### 15.1 Reverse-Z 与无限远投影
实现 Reverse-Z 渲染投影并保留有限裁剪投影。

#### 15.2 单帧相机快照
完善 RenderView / FrameData 的值语义快照。

#### 15.3 曝光链路对接
对接统一曝光职责与相机参数。

#### 15.4 Reverse-Z 消费者审计
迁移深度重建、裁剪、调试和渲染状态消费者。

---

## 阶段四 — 高级渲染特性

### 模块 16 — 屏幕空间反射与折射

#### 16.1 共享 scene-color 基建
建立透明前 scene-color copy 与 mip 链。

#### 16.2 SSR
实现屏幕空间反射及 IBL 回退。

#### 16.3 屏幕空间折射复用
使折射复用模块 16.1 的共享资源。

### 模块 17 — 后处理与物理相机进阶

#### 17.1 DoF
增加对焦距离并实现物理景深。

#### 17.2 TAA
实现 jitter、velocity、history 与重投影。

#### 17.3 Color Grading
实现 LUT 色彩分级与固定后处理顺序。

### 模块 18 — 光照与 AO 进阶

#### 18.1 太阳角径
用物理太阳角径统一高光和阴影半影。

#### 18.2 GTAO 与进阶 AO
分阶段实现基础、时域和可选高级 AO。

#### 18.3 混合模式补全
补全 ADD、SCREEN、MULTIPLY、FADE 等混合模式。
