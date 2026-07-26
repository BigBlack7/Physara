## Why

大重构启动前必须先立"护栏",否则重构会**静默改坏渲染**且无从察觉——这是整份开发计划中优先级最高、且不依赖任何测量结果的前置工作:

- **契约漂移无防护**:CPU↔GPU 数据契约(`GPUContracts.hpp` 的 struct 与 GLSL 镜像结构)当前仅由 `Tools/verify_gpu_contracts.py` 校验**枚举/常量数值**,**不**校验结构体字段布局与 binding 号。计划中的模块 4.2 将重设计 `MaterialGPUData` 等 GPU 布局——一旦字段顺序/类型/std140-std430 对齐或 binding 号漂移,画面会全错却**没有任何编译或运行期报错**(`ObjectData` 现有 4×uint32→uvec4 隐式映射即为无人校验的高危点)。因此本能力是模块 4.2 的**硬前置**。
- **无视觉回归手段**:当前重构任一渲染 pass 都可能悄悄改变画面而无从发现。项目已有 `RendererCapture`(截图)与稳定的默认场景,具备低成本建立 golden-image 回归的条件。

两项护栏都**与 P.1 性能测量结论无关、无论瓶颈在哪都要先做**,是后续所有重构模块的公共安全网。

## What Changes

- 扩展 CPU↔GPU 契约校验,从"仅枚举/常量数值"覆盖到:
  - **结构体字段布局**:字段顺序、类型、std140(UBO)/std430(SSBO)对齐,针对 `CameraData`/`ObjectData`/`MaterialGPUData`/`LightData`/`ShadowData`/`IBLData`/`ClusterGridData`/`FrameUniforms`。
  - **binding 号一致性**:C++ `GPUBufferBinding`/`GPUTextureBinding` ↔ GLSL `PHYSARA_BINDING_*`。
  - 顺带清理已知死枚举(`GPUBufferBinding::Shadow=6`/`IBL=7`)。
- 建立 **golden-image 视觉回归**基建:基于 `RendererCapture` 对默认场景产出基准图,提供"重构前后逐像素/容差比对"的可复用流程与判定标准。
- 确立工作流约定:涉及 GPU 布局/契约变更的任务强制跑契约校验;触及渲染 pass 的任务在重构前后跑视觉回归。
- **非破坏性**:仅新增验证能力,不改变任何渲染运行时行为、不改 API。

## Capabilities

### New Capabilities

- `gpu-contract-verification`: CPU↔GPU 数据契约的自动校验能力(结构体字段布局 + binding 号 + 枚举/常量),在重构期阻断契约静默漂移。
- `visual-regression-testing`: 基于渲染截图的 golden-image 回归比对能力,保护重构不改坏画面。

### Modified Capabilities

(无——本项目此前无已建 spec,以上两项均为新建能力。)

## Impact

- **工具/验证**:`Tools/verify_gpu_contracts.py`(扩展布局 + binding 校验);`Engine/Renderer/RendererCapture.*`(golden-image 复用)。
- **校验目标(只读,不改其行为)**:`Engine/Renderer/GPUContracts.hpp` ↔ `Assets/Shaders/Includes/Common.glsl`、`FrameUniforms.glsl`、`Material.glsl`。
- **工作流依赖**:本 change 是后续重构模块的前置 gate——`gpu-contract-verification` 阻塞模块 4.2 的布局重设计。
- **运行时**:无行为变更,无 API 破坏,无性能影响(校验为离线/构建期与按需截图)。
