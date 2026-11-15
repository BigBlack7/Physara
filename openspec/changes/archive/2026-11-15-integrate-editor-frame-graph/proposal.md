# Proposal

## Why

D05-T03 已交付从声明、编译计划到原生执行的完整 RenderGraph 合同。D05-T04 将现有产品帧接入这条链，使上传、诊断视口、UI 采样和窗口输出的版本、顺序及寿命能由同一张图解释。 [工作包](../../../../Docs/Developments/D03-D05-FramePipeline.md#d05-t04) · [主循环](../../../../Docs/Architecture.md#frame-flow)

## What Changes

- 从封存的编辑器帧构建上传、诊断视口和 UI Pass，显式初始化窗口颜色，并将合成后的原 `PresentColor` 以 `Presentation` 用途导出。Runtime 消费编译计划，再沿现有宿主协议关闭、复制、提交和呈现。
- 保持稳定的 `UiTextureId`，在 `FrameSlotUiFrameBindings` 中记录本图的纹理版本和范围。按实际普通 draw 收集、去重采样声明，在执行器的 Pass 准备与 `PassResources` 中建立和取得实际绑定。
- 使隐藏面板、空裁剪和重复 Image 引用正确影响图活性。诊断颜色换代、驻留纹理更新和帧槽复用遵守真实完成票据，纹理请求仅确认实际执行且获有效提交的上传。
- **BREAKING** 将 Editor 的独立 UI 录制入口迁移为构图和提交确认入口，同步迁移 Runtime 及直接消费者。保留 UI 坐标、索引、reset-state、颜色合成、预算与 Pending 的既有行为。
- 由呈现 owner 提供 `PresentColor` 的明确入口与出口状态，覆盖首次使用、未执行取消和完成后复用，使图导入事实与实际 RHI 生命周期一致。 [本帧纹理解析](../../../../Docs/Architecture.md#editor-output) · [UI 纹理协议](../../../../Docs/Architecture.md#imgui-renderer) · [呈现边界](../../../../Docs/Architecture.md#present)

## Capabilities

### New Capabilities

- `editor-frame-graph` 定义产品帧的图组装、帧内纹理版本解析、按消费者裁剪、上传准入、输出发布和帧槽保活。

### Modified Capabilities

- `editor-ui-rendering` 将封存 UI 的消费合同接入 Graph，并使纹理确认区分已声明、被裁剪与实际执行的上传。
- `opengl-window-presentation` 让取得的目标携带可供图使用的显式进出状态，明确首次使用和复用的交接事实。

## Impact

- `Runtime` 负责帧槽、图的编译执行及宿主回执。`Engine/Renderer` 负责诊断输出的图版本与接受发布。`Editor/Session`、`Editor/ImGui` 和 `Editor/Viewport` 负责封存 UI、稳定 token、本帧绑定及上传确认。`Platform` 提供目标状态并继续拥有呈现 token。相关 CMake 目标接入既有 `PhysaraRenderGraph`。 [现有产品入口](../../../../Runtime/Main.cpp) · [图执行接口](../../../../Engine/Renderer/RenderGraph/GraphExecutor.hpp)
- 本 change 覆盖 D05-T04 的三项任务。D05-T05 继续负责安全 UI-only fallback、错误帧恢复和完整帧取消验证，D08-T06 负责跨帧资源池。D05-T04 遇到新接线失败时沿现有取消、提交前缀或受控退出合同收拢。 [D05-T05](../../../../Docs/Developments/D03-D05-FramePipeline.md#d05-t05) · [D08-T06](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t06)
- 验收使用实际产品入口与真实 OpenGL 消费者，覆盖窗口像素、图裁剪、纹理更新、resize 及在途绑定。开发材料位于 `build/d05-t04/`，实现后按 A/B 独立审查、整改复核、最终验证、同步及归档推进。 [开发约定](../../../../AGENTS.md#review-loop)
