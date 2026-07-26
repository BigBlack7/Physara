## Context

本 change 为大重构前的两道护栏(见 `proposal.md`):`gpu-contract-verification`(契约防漂移)与 `visual-regression-testing`(视觉回归)。二者均为**新增验证能力,不改渲染运行时行为**。

现状关键事实(第 1-4 轮源码核实,作为设计前提):
- CPU 侧 GPU 结构定义在 `Engine/Renderer/GPUContracts.hpp`(`alignas(16)`,仅用 `mat4`/`vec4`/`uvec4`);GLSL 镜像在 `Assets/Shaders/Includes/Common.glsl`、`FrameUniforms.glsl`、`Material.glsl`。二者**手工对齐、当前布局全部匹配**(已验证),但**无任何自动防线**防止未来漂移。
- `Tools/verify_gpu_contracts.py` 现仅校验枚举/常量数值。
- binding 号:GLSL `PHYSARA_BINDING_*`(`Common.glsl`)与 C++ `GPUBufferBinding`/`GPUTextureBinding` 手工同步,存在死枚举(`GPUBufferBinding::Shadow=6`/`IBL=7`)。
- 截图能力已有:`Engine/Renderer/RendererCapture`(GPU 回读→PNG/JPG)。默认场景稳定(`Assets/Scenes/default.scene.json`)。
- 约束:项目**无测试框架、无 headless 运行路径**(rules.mdc / 第二轮 H2.5),IBL 预计算为后台线程(完成时机不定)。

## Goals / Non-Goals

**Goals:**
- 在 GPU 布局/binding 发生漂移时**尽早、显式失败**(编译期或一条命令),覆盖 `CameraData`/`ObjectData`/`MaterialGPUData`/`LightData`/`ShadowData`/`IBLData`/`ClusterGridData`/`FrameUniforms`。
- 提供可复用的 golden-image "重构前后比对"流程,判定标准明确(容差而非逐像素)。
- 作为后续重构的 gate:契约校验**阻塞模块 4.2**;视觉回归用于任何触及 pass 的模块。

**Non-Goals:**
- 不引入测试框架、不搭 CI 自动化(与项目现状一致,dev 手动运行)。
- 不做 CPU↔GLSL 的单源代码生成(codegen)——本 change 只立"校验",codegen 若需要另立 change。
- 不改任何渲染行为、不动 `GPUContracts` 现有布局(仅在其上加断言)。
- 不实现 headless 捕获(依赖 H2.5,超范围)。

## Decisions

**D1 — 契约校验用"编译期 static_assert(CPU 权威)+ 扩展脚本(CPU↔GLSL 比对)"混合方案。**
- CPU 侧:为每个 GPU 结构加 `static_assert(sizeof(...))` 与逐字段 `offsetof(...)` 断言(就近置于 `GPUContracts.hpp` 或配套头),编译器计算真实偏移,字段被误改/重排/改类型即**编译失败**,零运行时成本。
- CPU↔GLSL:扩展 `verify_gpu_contracts.py`,解析 GLSL 结构体的字段序列/类型与 `layout(binding=)`,与 C++ 结构和 `GPUBufferBinding` 枚举比对。
- 备选与否决:①纯脚本解析 C++(offset 需自行推 std140/std430,易错)——否决,交给编译器更权威;②仅 static_assert(覆盖不到 GLSL 侧漂移)——否决;③全 codegen(过重)——本轮 Non-Goal。

**D2 — 以"GPU 结构仅用 `vec4`/`mat4`/`uvec4`"为受检不变量。**
当前布局刻意规避了 std140 的 vec3/标量数组陷阱,使 std140(UBO)与 std430(SSBO)偏移**恰好一致**,校验大幅简化。脚本额外**断言该纪律**:发现任何 `float`/`vec2`/`vec3`/标量数组成员即告警(既是 bug 也是可读性风险)。每个结构标注其 UBO/SSBO 归属供校验选择规则。

**D3 — binding 号交叉校验 + 死枚举清理。**
校验 `PHYSARA_BINDING_*` 定义值 == 对应 C++ 枚举值;报告未被任何 GLSL 引用的死枚举(`Shadow=6`/`IBL=7`)与语义重载(`binding=4`)。

**D4 — 视觉回归 = 确定性捕获剖面 + 容差比对。**
- 确定性捕获:固定场景(default)、固定相机位姿、固定分辨率、关闭抖动;**门控 IBL 预计算完成** + N 帧 warmup 后再捕获(否则后台 IBL 线程导致画面不定)。
- 比对:容差法(逐像素 abs 差阈值 + 允许失败像素占比),失败时输出 diff 图与量化指标;**非逐像素精确**(规避驱动/浮点差异)。
- 备选否决:精确 diff(过脆)、SSIM(当前过度)。首版 MSE/阈值足够,后续可升级。

**D5 — dev 手动运行,golden 入库。**
经编辑器动作(或最小捕获路径)驱动 `RendererCapture`;golden 基准图存于仓库固定目录。与"无测试框架/无 headless"现状一致,不追 CI。

## Risks / Trade-offs

- **GLSL 正则解析脆弱** → 缓解:static_assert 为 CPU 侧权威兜底 + 保持 GLSL 结构书写纪律(D2)+ 解析失败即报错(fail-loud),不静默放过。
- **golden 非确定性**(驱动浮点、IBL 线程时序) → 缓解:容差比对 + IBL-ready 门控 + warmup;并明确**故意的画面变更**(如模块 15 reverse-Z、模块 4.2 布局重设计后的正确新画面)需**主动刷新 golden**,附变更说明。
- **static_assert 偏移为手写、可能腐化** → 缓解:断言就近置于结构定义旁 + 脚本亦校验 C++ 侧,双保险。
- **golden PNG 入库致仓库膨胀** → 见开放问题(可选降分辨率/存哈希)。
- **无 headless 致捕获需走编辑器** → 现状接受;H2.5 若引入 headless 可自动化。

## Open Questions

- golden 基准图的存放目录与是否直接提交二进制 PNG(仓库体积);备选:降分辨率基准 / 存感知哈希 / Git LFS。
- 容差阈值(epsilon、失败像素占比)的初始标定值——需在本机驱动上跑几次取经验值。
- static_assert 与脚本的职责边界:CPU 布局是否**只**靠 static_assert(脚本仅管 GLSL↔CPU 与 binding),避免重复维护。
