# Proposal

## Why

D05-T02 已能把封存图编译为确定的 CPU 计划。D05-T03 要让这份计划驱动真实 OpenGL 工作，在 D05-T04 接入产品帧之前证明首次物化、每个 Pass 的访问用途和原生结果遵守同一套依赖。 [工作包](../../../../Docs/Developments/D03-D05-FramePipeline.md#d05-t03) · [图编译主规范](../../../specs/render-graph-compilation/spec.md)

## What Changes

- 让执行器按编译计划消费同图代原图，在 first use 分配兼容物理槽，解析导入对象、附件和绑定准备，并通过 `PassResources` 交付本次声明授权的资源。图内槽复用沿实际结束状态接续。
- 在现有即时 Graphics 录制中执行计划的 typed clear、上传、复制和 Draw。GraphManaged 提供显式用途，Automatic 继续从现有 RHI 操作推导，两种入口共用 RHI tracker 与后端发射路径。
- 成功执行时应用图尾边界状态并交付指定版本、范围和用途。准备或执行失败返回首个可定位错误并停止后续 Pass，已发前缀由现有录制、提交和退休合同保护。
- 以真实 OpenGL 消费者核对 clear 后 Draw、只读深度附件联合采样、上传到采样、附件到 UI 式合成、Shader 读后覆写，及受影响的失败和 Automatic 恢复路径。 [图执行与同步](../../../../Docs/Architecture.md#barriers)

## Capabilities

### New Capabilities

- `render-graph-execution` 定义编译计划在即时 RHI 录制中的首次物化、Pass 执行、状态交接、单一同步权威和失败结果。

### Modified Capabilities

## Impact

- `Engine/Renderer/RenderGraph` 消费已归档的声明与编译合同，沿既有图、Core 和公共 RHI 边界新增执行入口。RHI Common/Validation/OpenGL 按需要接通显式用途、状态提交和失败退出。 [现有计划值](../../../../Engine/Renderer/RenderGraph/GraphCompiler.hpp) · [即时 RHI 规范](../../../specs/opengl-immediate-draw/spec.md)
- D05-T04 负责将诊断视口、ImGui、帧槽纹理绑定和窗口呈现接入产品 Graph。D05-T05 负责运行时 fallback 与帧取消。Compute、image/SSBO 写入和多消费域同步由 D08-T04 交付，跨帧池由 D08-T06 交付。 [产品接线](../../../../Docs/Developments/D03-D05-FramePipeline.md#d05-t04) · [错误帧](../../../../Docs/Developments/D03-D05-FramePipeline.md#d05-t05) · [计算同步](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t04) · [跨帧资源池](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t06)
