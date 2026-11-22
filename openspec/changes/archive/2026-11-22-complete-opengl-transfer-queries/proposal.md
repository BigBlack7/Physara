# Proposal

## Why

D08-T02 已交付有界上传和在途保活，D08-T04 已交付 Shader 写入对多类消费者的同步。D08-T05 补齐从 GPU 生产、传输到 CPU 确认完成并读取的一条正式链路，供后续拾取、截图和帧计时直接复用。 [D08-T05 工作包](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t05) · [D08-T02 验收](../2026-11-20-complete-versioned-gpu-residency/tasks.md#final-verification) · [D08-T04 验收](../2026-11-21-enable-opengl-compute-synchronization/tasks.md#final-verification)

## What Changes

- 完善 Graphics 即时列表上的 typed clear 和精确 copy，补齐整 Buffer 的 UInt clear、有限颜色 mip 生成及多样本颜色 resolve。逐格式、目标、用途及实际样本集合认定支持范围，保留未选字节、mip、数组层和 cube 面。
- 交付固定 Read 或 Write 方向的 Buffer 与 Texture staging。按实际 mip/layer、像素或块宽、行间距和区域搬运数据，Map 只借出当前可用的 CPU 地址和真实 row pitch。读回沿用复制、提交、捕获 Event、非阻塞 Poll 和 Map 的顺序。
- 延续现有提交完成权威、Event 快照和 Timer timestamp 对。补齐 Event 对象容量及 Timer 结果发布的提交前提，复用运行时有界轮转。未接受、未完成、已取消、确认失败和有效零秒有明确的观察边界。
- 让既有 RenderGraph 通过有范围授权的 CopySource Pass 或图尾导出接入 staging，复用 Automatic/Explicit/GraphManaged 的同一访问跟踪。验收真实图形与计算交错、读回数据和次序、取消与在途退出。

## Capabilities

### New Capabilities

- `opengl-immediate-transfer` 定义有限颜色 mip/resolve、双向 staging、读回布局、CPU 映射及实际传输依赖和退休。

### Modified Capabilities

- `opengl-device-resources` 更新实际传输格式/样本支持、CPU staging Buffer 分配及公开操作边界。
- `opengl-immediate-draw` 扩充 typed clear/copy 约束，保持后继 Draw 的状态语义，接入正式读回完成证明。
- `opengl-completion-queries` 明确 Timer 的接受提交前提，以及 Event 与 Timer 各自的有限容量和复用条件。
- `rhi-common-validation` 定义新增入口的公共形状、方向、区域及映射参数预检。
- `render-graph-execution` 定义实际 staging 复制与图授权、图执行结果、外部调用方完成观察的衔接。

## Impact

- 主要涉及 RHI/OpenGL 的 Transfer、Staging、FormatCaps、ContextState、Queries 和物理包退休，以及 Common/Validation、RenderGraph 的直接传输消费者和 RHI 能力说明。沿用已有 Graphics 队列、公开 NVRHI 形状及 Runtime 计时入口。
- 本工作包交付拾取、截图与计时所需的底层传输和完成合同。资源池、跨帧导入和图预算由 D08-T06 交付。Editor 拾取、截图编码与应用级请求管理按各自工作包交付。Forward/Forward+ 的完整 MSAA 及深度/法线 resolve 由 D18-T01 单独定义。 [命令执行](../../../../Docs/Architecture.md#command-execution) · [读回](../../../../Docs/Architecture.md#readbacks) · [能力](../../../../Docs/Architecture.md#capabilities) · [D08-T06](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t06) · [D18-T01](../../../../Docs/Developments/D18-D19-Extensions.md#d18-t01)
