# Proposal

## Why

D08-T07 要让现有 Forward 视图在 Shader 编辑和准备期间保持可用。当前程序请求能确定编译包身份，但 Forward 材质和显示路径仍在首次准备时同步读包并创建 Pipeline，源码变化也没有完整代发布入口。 [工作包](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t07) · [准备及发布合同](../../../../Docs/Architecture.md#plan-readiness)

## What Changes

- 为现有静态 Forward 与显示程序建立可验证的有限 `VariantKey` 和合法组合。按完整源码、生成接口、配置及工具身份复用 CPU 包，按当前设备、驱动、绑定映射与 Pipeline 兼容事实复用原生结果。
- 为显式指定源码根的 Windows 开发会话接入文件变更通知。合并保存事件，按实际传递依赖选取受影响程序，在后台复用构建时的 canonical 编译、验证和反射规则。
- 在 OpenGL context owner 线程按预算准备实际需要的 Pipeline、布局及相容的资源代，完整候选在帧边界发布。主视图和离屏视图消费同一代，继续使用有效计划或上次接受的画面，直到所需准备就绪。
- 区分 Active、Pending 和 Failed。报告程序、变体、源位置和首个失败原因，保留旧完整代及在途依赖。准备计量沿用现有统计快照。

## Capabilities

### New Capabilities

- `shader-generation-reload` — 开发源码通知、异步候选、预算化准备、完整代发布和失效收拢。

### Modified Capabilities

- `shader-build` — 受控程序目录、合法变体解析和按完整编译身份复用的 CPU 包。
- `opengl-shader-pipelines` — 原生准备的设备与驱动缓存身份、兼容键和容量。
- `editor-frame-graph` — 构图前取得已就绪的 Shader 代以及独立的 View 提交确认。
- `render-frame-statistics` — 只读展示 Shader 代、准备预算、缓存和源码诊断。

## Impact

- 直接边界为 Engine/Shader、现有静态 Forward/Display 准备、OpenGL Pipeline、View 和 Runtime 帧边界。Editor 通过现有统计及诊断快照消费状态，安全 UI 由已打包的 EditorUi 程序保障。现有场景、材质数值、静态资产、帧槽和图池按各自合同工作。 [Shader 构建](../../../../Docs/Architecture.md#shader-build) · [GLSL 职责](../../../../Docs/Architecture.md#shader-runtime) · [预算和诊断](../../../../Docs/Architecture.md#diagnostics)
- D08-T02 与 D08-T04 是工作包前置，既有 D08-T06 归档提供图池、View 输出与受保留资源接续。D08-T08 负责扩展执行链及同口径成本验收，后续路径与 AA 事务沿各自工作包推进。 [D08-T02](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t02) · [D08-T04](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t04) · [D08-T06](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t06) · [D08-T08](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t08)
