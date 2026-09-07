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

##### RDC 二次深分析（2026-08-31，RHI×Vulkan 视角）

方法：renderdoc MCP `execute_python` 直接统计结构化 chunk 流（不止 action 列表），逐 pass 归因真实 GL 调用构成；结论供模块 0/2/3/5/8 任务吸收。

**抓帧包组成（解读约束）**：两 capture 总 chunk 5812(F)/13655(F+)，首个 debug marker 前的 5249/13090 个 chunk 是 RenderDoc 抓帧序列化（含对 GPUScene buffer 的 3293 次 `glBindBuffer` 分块回读）与首帧初始化（124 纹理创建、107 纹理上传、18 shader 编译、20 FBO 创建），**非应用帧代码**；应用帧本体 ≈ 563-565 次 GL 调用。timing 离群值（首个 MDI 593ms、单 draw 111ms/27ms）为重放首用开销，剔除。

**帧内 GL 调用画像**（Forward；Forward+ 仅主 pass +2 `glBindBufferRange`、shader 变体不同，其余逐项一致）：

| Pass | GL 调用 | 构成 |
|---|---|---|
| Shadow | 245 / 8 MDI | 9×NamedBufferSubData；8×(UseProgram+BindVertexArray+VertexArrayVertexBuffer+ElementBuffer+BindBuffer)；6×BindBufferRange；4×(BindFramebuffer+Clear) |
| ForwardOpaque | 100 / 39 draw | 39 draw + 38×glBindTextures（批量）+ ~10 setup；无 program/VAO 切换 |
| PostProcess | 115 / 15 draw | 15×BindFramebuffer + 22×BindTextureUnit + 14×Viewport + 5×UseProgram |
| EditorUI | 75 / 30 draw | 直写 GL（17 tex + 13 scissor），绕 RHI |
| GPUSceneUpload | 0 有效调用 | 静态场景+静态相机下哈希门控跳过重传，门控生效实证 |

**新结论：**

1. GL 调用数很瘦（~563/帧）→ B 机 `graph exec 18.7ms` 的开销主体是引擎提交机器（命令构建/哈希/去重/span 拷贝/上传 staging），非 GL 调用数；0.3/0.6 需 CPU profiler 定位，RDC 只能给调用构成。
2. Shadow 逐 run 全量状态重设：单/双面 caster 分组各做一次完整 SetPipelineState+SetRenderPrimitive（program/VAO/VBO/EBO 重绑 ×2/cascade），状态缓存未去重；indirect 命令按 run 独立上传、独立 buffer（capture 中 8 个小 indirect buffer：2304B/3840B ×4 对），应合并为单 buffer + 偏移。
3. 级联 UBO 走 `BindBufferRange(同 buffer, 逐 cascade 偏移)`：GL 动态偏移模拟，语义可平移 Vulkan dynamic offset，但 RHI 无 dynamic offset 概念。
4. GPUScene 单 arena：ObjectBuffer/MaterialBuffer/ClusterEntry/ClusterLightIndex 均为同一 3MB buffer 的 range bind；创建参数 `GL_DYNAMIC_STORAGE_BIT|GL_MAP_WRITE_BIT`；全帧 0 次 Map、0 次 fence/sync——上传全靠 SubData + 哈希门控，无 frames-in-flight 概念。
5. 显存实测 5.6-6.4GB 纹理（84 张 4096² RGBA8 全 mip 链 ×89.5MB），默认场景即顶到 6GB 卡上限，靠驱动分页掩盖；无压缩/预算/流送。Bloom 7 级 ping-pong 14 张半浮点纹理 + 15 FBO（889×611 下过剩的实锤）。
6. FBO 仅 init 建 20 个，帧内 21 次 FBO 切换 ≈ 21 个潜在 `vkCmdBeginRenderPass`；Composite 常绑 shadow map/scene depth 待命 debug view。
7. RHI×Vulkan 差距（代码审计 + RDC 互证）：绑定模型为即时槽位式，`SetResourceSet` 展开 span 且丢弃 setIndex（OpenGLCommandList.cpp:910）；`SubmitCommandList` 空操作（OpenGLDevice.cpp:181-184）；`TextureBarrier`/`BufferBarrier` 丢弃资源参数（OpenGLCommandList.cpp:1527/1554）并经 `FirstStage()` 折叠多 stage（RHICommandList.hpp:167-172,207）；`PushConstants` 丢弃 stage 参数（OpenGLCommandList.cpp:1255）；`RHIPipelineStateDesc.renderPassDesc` 为 OpenGL 不消费的悬空指针。GL 零 barrier 是 GL 语义红利，Vulkan 下 RenderGraph 的状态推导须接上真 per-resource layout transition。

**Deferred 补充（同日，`8-25-B-Deffered.rdc`）**：307 actions / 239 draws / 11 clears；GBuffer 101 次 GL 调用 / 39 draw（38 次批量 glBindTextures + 4 次 Clearfv，与 Forward 同样瘦）→ Deferred 面板 CPU 30.5ms 的开销同样在引擎提交机器（CanMerge 逐 item 判定、纹理表上传），不在 GL 调用数。DeferredLighting（event 370）实测绑定：4 张 GBuffer RT + D24S8 深度 + shadow 阵列 + cluster 两表（arena range bind）+ LightBuffer，单次全屏三角形写 SceneHDR；SceneHDR 先 Clear 再全屏覆盖，冗余 clear 实证。存疑：GBuffer RT3 文档记 RGBA16F，本 capture 绑定清单只见 1 张 RGBA16F + 2 张 RGBA8，RT3 实际格式待核对（文档过期或绑定缺失）。本 capture 无 ClusterDebug marker（8-28 记录中有 1 个空 marker），差异源于抓帧时 debug 设置，非图剔除证据。

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
**现状核验（2026-08-31 RDC+代码）：** `GetCommandList()` 返回单例、`SubmitCommandList()` 空操作（OpenGLDevice.cpp:181-184）、`WaitIdle` 即 `glFinish`（OpenGLDevice.cpp:186-190）；两份 capture 全帧 0 次 fence/sync/map，无 frames-in-flight 概念。本任务是 Vulkan 后端前置：per-frame command buffer、队列提交、fence/semaphore 均依赖本层语义先成立，优先级应上调。

#### 0.2 OpenGL 状态缓存
完善状态缓存失效与外部状态失效模型。
**现状核验（2026-08-31 RDC）：** Shadow 单/双面 run 边界出现同一 program/VAO/VBO/EBO 全量重绑（每 cascade ×2），状态缓存未去重（见 P.1 二次分析第 2 条）。

#### 0.3 提交热循环去重
整理既有去重逻辑并保留必要统计。
**现状核验（2026-08-31 RDC）：** 帧内 GL 调用 ≈563 次，已很瘦；B 机 graph exec 18.7ms 主体是引擎提交机器而非 GL 调用数。先做 CPU profiler 定位（RenderProxy/CommandExecutor/上传 staging），再定去重目标。

#### 0.4 Descriptor 惰性绑定
评估仅提交发生变化资源槽位的绑定策略。
**现状核验（2026-08-31 代码）：** 当前为即时槽位绑定模型，`SetResourceSet` 仅展开 textures span 且丢弃 setIndex（OpenGLCommandList.cpp:910）；与 Vulkan descriptor set 语义差距大，需连同 0.11 一并设计。

#### 0.5 PSO 编译与 program binary 缓存
评估异步编译和磁盘缓存的架构、失效与回退策略。

#### 0.6 拆分 OpenGLCommandList
按职责拆分过大的命令列表实现。
**现状核验（2026-08-31 RDC）：** 同 0.3——GL 调用数非瓶颈，拆分的价值在可维护性与 Vulkan 对齐（状态缓存/管线应用/绑定/barrier/计时分离），不在 GL 调用减少。

#### 0.7 枚举到 GL 映射收敛
统一纯类型映射的来源，并补全 storage image 对 2D array 等维度的 layered 绑定语义。

#### 0.8 拆分 RHIDefinitions
按职责拆分 RHI 公共定义。

#### 0.9 ImGui 后端走 RHI
将 ImGui 资源和绘制提交迁入 RHI 边界。
**现状核验（2026-08-31 RDC）：** EditorUI 30 draw 直写 GL（17 次纹理绑定 + 13 次 scissor），绕开 RHI 状态缓存，是外部状态失效的主要来源。

#### 0.10 缓存碰撞与失效
加固管线和网格缓存的身份比较与生命周期失效。

#### 0.11 描述符模型统一
统一资源集与 GPU set index 的真实语义。
**现状核验（2026-08-31 代码）：** `RHIResourceSet` 仅含 textures span、无 buffer/offset 成员；`RHIPipelineStateDesc.renderPassDesc` 为 OpenGL 不消费的悬空指针。目标形态应接近 Vulkan：descriptor set layout + 按频分级（GPUResourceSetIndex 已是雏形）+ 批量更新 + dynamic offset。

#### 0.12 精确 barrier
建立精确资源访问同步和 RenderGraph 依赖规则。
**现状核验（2026-08-31 RDC）：** 全帧 0 `glMemoryBarrier` 是 GL FBO 语义红利，不代表问题不存在——Vulkan 下每处附件写→读都需显式 layout transition。当前 `TextureBarrier`/`BufferBarrier` 丢弃资源参数（OpenGLCommandList.cpp:1527/1554）、`FirstStage()` 折叠多 stage（RHICommandList.hpp:167-172,207）。任务重心从"GL 优化"转为"为 Vulkan 接上 per-resource stage/access/layout 语义"，RenderGraph 的 TrackedState 推导可直接复用。

#### 0.13 bindless 纹理驻留
管理 bindless handle 驻留、预算和延迟回收。
**现状核验（2026-08-31 RDC）：** 默认场景纹理显存实测 5.6-6.4GB（84 张 4096² RGBA8 无压缩全 mip 链），6GB 卡靠驱动分页硬扛。预算是硬需求不是优化项。

#### 0.14 上传同步优化
基于实际测量评估上传路径与 fence 优化。
**现状核验（2026-08-31 RDC）：** 帧内仅 14 次 `glNamedBufferSubData`，0 次 map/fence/sync；GPUScene 走单 3MB arena（`GL_DYNAMIC_STORAGE_BIT|GL_MAP_WRITE_BIT`）+ 哈希门控跳过重传（静态帧 GPUSceneUpload 零调用）。现状对单线程 GL 已合理；fence/frames-in-flight 是 Vulkan 语义下的新需求，与 0.1/0.17 联动。

#### 0.15 GL 调试回调治理
聚合、限频并保留高严重度调试消息。

#### 0.16 Reverse-Z RHI 前置
建立 Reverse-Z 所需 depth compare、clear 与 clip-depth 能力，并统一 Depth24Stencil8、Depth32F 等深度格式的附件识别与映射。
**现状核验（2026-08-31 代码）：** OpenGL 后端未调 `glClipControl`，默认左下原点 + [-1,1] 深度（`SetViewport` 注释明写）。建议引擎统一 Vulkan 约定（左上原点 + [0,1] 深度），GL 后端用 `glClipControl(GL_UPPER_LEFT, GL_ZERO_TO_ONE)` 适配，避免跨后端投影矩阵特判。

#### 0.17 Swapchain 与 frames-in-flight
分离窗口与呈现，建立 acquire/present 与帧同步语义。
**现状核验（2026-08-31 代码）：** `IWindow` 为窗口级抽象（PollEvents/SwapBuffers），无 swapchain 对象；RHI 无 fence/semaphore/frames-in-flight。Filament 的 FSwapChain（平台层只管 context/swapchain/present）为参照。与 0.1 配套构成 Vulkan 后端的地基。

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
**现状核验（2026-08-31 代码）：** `BuildRenderGraph()` 每帧 `Reset()` 后全量重建 pass/lambda（Renderer.cpp:516）；`RenderGraph::CreateTexture` 瞬态路径零调用（RenderGraph.cpp:179），所有纹理预分配后 Import，图实为线性链（拓扑序==声明序），瞬态池与剔除机制空转；GPUSceneUpload 等 side-effect 上传 pass 与渲染 pass 无资源边，排序安全纯靠声明顺序（图未来引入重排/并行即 use-before-upload 竞态）。另：Forward/Forward+ 实为同一 pass 同一 shader 仅 cluster define 之差（ForwardOpaquePass.cpp:310-319），"三条管线"实为两条半，拆分时应按渲染职责而非按枚举分支重组。

#### 2.2 PassContext 收敛
消除各 pass 的重复帧级上下文填充。
**现状核验（2026-08-31 代码）：** `ForwardPassContext` 同构字段在 Renderer.cpp:798-826 / 924-951 / 1034-1059 三处手拼；各 pass 经 context 拿十余个共享指针并自行 flush/EnsureResources，pass 边界无约束。

#### 2.3 辅助职责剥离
从 Renderer 剥离 benchmark、capture 等辅助职责。

#### 2.4 Resize 重建收敛
处理高频 resize、最小化和恢复的资源重建，并保证导入纹理的描述完整保留颜色空间等资源语义。
**现状核验（2026-08-31 代码）：** 资源所有权三套并存——Renderer 直接持有 SceneRT（Renderer.hpp:115-121）、DeferredResources 持 GBuffer 且存 SceneHDR/Depth 裸指针（DeferredResources.hpp）、Shadow/PostProcess 自管 RT（ShadowPass.cpp:339-387、PostProcessPass.cpp:232-309）；`RecreateRenderTarget` 手动逐一 reset（Renderer.cpp:370-382），漏一个即悬垂；Bloom 不在 resize 路径而靠尺寸比对惰性重建。统一资源池是根治方向。

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
**现状核验（2026-08-31 代码）：** `ToGLTextureFormat` 仅对 RGBA8 特判 sRGB，其余格式忽略 colorSpace（OpenGLTypeMapping.hpp:69-76）；纹理对象上还另设 727 次 `glTextureParameteri`（init 期），与采样器对象双源并存。Vulkan 下 sRGB 是格式属性（VK_FORMAT_*_SRGB），颜色空间模型需前移到格式枚举。

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
**现状核验（2026-08-31 代码）：** delta time 双源实证——`RenderSceneView` 用 `ImGui::GetIO().DeltaTime` 传给 `Renderer::RenderScene`（EditorApp.cpp:587、873），与引擎 `Time::Tick()`（Main.cpp:70）并存；且渲染帧循环被吞并进 `EditorApp::OnUIRender`（EditorApp.cpp:241-289），无独立 beginFrame/render/endFrame 边界。

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
**现状核验（2026-08-31 RDC）：** indirect 命令按 run 独立上传并各占一个 buffer（capture 中 8 个小 indirect buffer：2304B/3840B ×4 对，对应 4 cascade × 2 run）；应合并为单 dynamic indirect buffer + 偏移，一次上传多次绘制。Shadow 逐 run 全量状态重设见 5.1。

#### 3.3 Draw 排序与合并率
依据实测评估排序正确性、overdraw 与实例合并；审计排序键位宽、截断碰撞和完整材质身份比较，保证正确性优先于局部性优化。

#### 3.4 MDI 合并条件不一致修复
**现状核验（P.1 轮次 0 实测）：** `GBufferPass::CanMerge` 的 bindless 分支要求 `meshKey` 相等（`GBufferPass.cpp:195-197`），而 `ForwardOpaquePass::CanMergeIndirectRun` 的 bindless 分支直接放行（`ForwardOpaquePass.cpp:422-425`）。Deferred 面板实测：direct 37 / MDI 9 runs 155 cmd / breaks M/G/I/S 35/0/0/36；Forward 面板：direct 1 / MDI 10 runs 191 cmd / breaks 0/0/0/0。GBuffer pass 开头 5 次独立 `glClearNamedFramebuffer*`（4 色 + 1 深度）。
**补充（2026-08-31 RDC chunk 级）：** GBuffer 帧内 101 次 GL 调用 / 39 draw（38 次批量 glBindTextures + 4 次 Clearfv），与 Forward 同瘦——Deferred CPU 30.5ms 在引擎提交机器（逐 item CanMerge 判定、纹理表上传、break 后 per-item 路径），不在 GL 调用数；修 bindless 分支 meshKey 条件即可恢复 MDI，GL 层无需改动。5 次独立 clear 在 Vulkan 下应并为 render pass loadOp。存疑：GBuffer RT3 文档记 RGBA16F，Deferred capture 的 DeferredLighting 绑定清单只见 1 张 RGBA16F + 2 张 RGBA8，RT3 实际格式待核对。
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
**现状核验（2026-08-31 代码）：** 材质数据 5 跳（Collect→Registry→Repack→GPUScene→TextureCache），`frameData.materials` 拷贝整份 MaterialComponent；存在两套独立哈希（MaterialSignature::Build 与 UploadHasher 的 HashMaterialTable）；上传链每帧回查 AssetManager（GPUScene.cpp:52、MaterialTextureCache.cpp:487+）。

### 模块 5 — ShadowPass

#### 5.1 CSM 成本拆解
根据性能基线分析级联、裁剪、分辨率和提交成本。
**现状核验（2026-08-31 RDC 逐 chunk 序列）：** 每 cascade = BindFramebuffer+Clear → BindBufferRange（级联 UBO 偏移）→ UseProgram+BindVertexArray+Enable(CullFace) → indirect 上传 → VBO/EBO 重挂 → MDI#1（双面组）→ UseProgram/BindVertexArray 再次 → Disable(CullFace) → indirect 上传 → VBO/EBO 重挂 → MDI#2（单面组）。4 cascade 共 8 次 program/VAO/VBO/EBO 重绑、9 次 buffer 上传。成本分解除级联裁剪外，应计入 per-run 状态重设与 indirect 上传开销（单/双面分组导致的双 run 结构）。

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
**现状核验（2026-08-31 RDC）：** DeferredLighting 为单次全屏三角形，写 SceneHDR 前先 `glClearNamedFramebufferfv` 再全屏覆盖——冗余 clear 实证（Vulkan loadOp 语义下应直接 DontCare）。GBuffer RT3 格式存疑见 3.4。

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
**现状核验（2026-08-31 RDC）：** 889×611 下实测 7 级 ping-pong 共 14 张 R16G16B16A16F 纹理 + 15 个 FBO（每级独立 FBO）；PostProcess 全链 115 次 GL 调用中 15 次 BindFramebuffer、22 次 BindTextureUnit、14 次 Viewport——per-mip pass 的结构开销主导。

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
