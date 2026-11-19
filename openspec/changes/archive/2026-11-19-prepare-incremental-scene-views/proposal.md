# Proposal

## Why

D07 已交付可编辑的物理 Forward 场景和原生出图基线。当前安全帧槽能复用未变场景，但一次局部编辑就会触发整槽重提取，正式帧也只编排一个 View。D08-T01 为大场景和同帧多 View 建立可验证的准备成本与寿命合同。 [开发计划](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t01) · [D07 阶段出口](../2026-11-19-add-physical-scene-authoring/tasks.md#final-verification)

## What Changes

- 从已提交的 Scene 事务发布结构代与有界变更集合。FrameSlot 在获准复用后按身份和版本复用、更新或重建自己的对象、Primitive 和灯记录，保留场景及资产引用的寿命边界。
- 让同一 Scene 的多个离屏 View 共用一次准备的 SceneFrame。各 View 的相机、可见索引、输出尺寸和设置独立，主视口和显式离屏输出走同一张正式帧图并各自按回执确认。
- 让已准备的静态对象、材质和灯数据按实际依赖复用编码；相机常量独立更新。同图中的匹配资源共享生产版本，图裁剪和失败沿现有提交与退休合同处理。
- 通过已有帧统计交付提取与裁剪耗时、场景记录更新和静态编码计数、arena 高水位与扩容次数。用槽轮转、稀疏编辑、结构变化和双 View 的可重复输入证明增量性与输出正确性。

## Capabilities

### New Capabilities

### Modified Capabilities

- `static-scene` 提供随已接受事务发布的结构变化和局部渲染变更。
- `scene-frame-preparation` 在安全槽内按结构代更新连续记录，并保持各槽快照独立。
- `view-preparation` 为实际请求的离屏 View 准备独立工作集和设置。
- `static-gpu-residency` 复用未变编码，约束同图共享生产和上传确认。
- `editor-frame-graph` 在单一正常图中执行并按 View 确认离屏输出。
- `render-frame-statistics` 让提取、裁剪、编码和 arena 成本可随实际提交核对。

## Impact

- Scene、Renderer/Prepare、FrameSlot 与 Runtime 帧边界承担变更发布和槽内追赶。Renderer/Draw、Lighting、Forward 和 Display 复用现有 OpenGL 资源、Shader 接口和帧图生产链。现有主 View 的 v1 场景持久化仍是产品入口，附加离屏 View 使用运行期请求。 [静态内容版本](../../../../Docs/Architecture.md#static-content) · [场景与 View 准备](../../../../Docs/Architecture.md#prepare) · [帧槽复用](../../../../Docs/Architecture.md#history-commit)
- D08-T02 接续驻留预算、上传区段和范围合并。D08-T03 至 T07 按各自前置承接批次、compute、传输查询、图资源池和热重载，由 T08 验证整阶段退出条件。 [D08 后续工作](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t02)
