# Proposal

## Why

D08-T06 要让渲染图在连续出帧和 resize 时复用已经安全退休的 Texture 与 Buffer。当前物理槽和瞬态字节预算仅覆盖单张图，后续帧会重新物化槽，而已接受视口、UI、读回和在途提交可能仍持有前一图的存储。 [工作包](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t06) · [图资源合同](../../../../Docs/Architecture.md#graph-resources)

## What Changes

- 在现有单队列 Graphics 路径中建立设备级 Texture/Buffer 池。继续按存活序列规划物理槽，在首次实际使用时获取兼容对象，跨图归还以真实完成和所有持有者释放为条件。
- 分开单图计划容量、跨帧对象与逻辑字节容量、空闲回收高水位。准入时区分固定超限和当前占用，先回收可安全淘汰的空闲项，按正常帧、安全 UI 和取消的既有顺序传播原因。
- 明确导入内容覆盖和图尾交接，保留纹理 mip/layer/aspect 与缓冲字节范围。以实际录制及 Close 的状态建立后续入口，用匹配提交及队列顺序发布外部结果，用真实完成回收存储。
- 接入主视口、现有离屏出口和 UI 纹理，保持稳定 View/token 与独立输出代。为 View 历史和外部读回复用相同的跨帧交接及保活机制。实现范围是资源交接，时域渲染、业务拾取与截图分别由后续工作包交付。 [图的物理计划](../../../../Docs/Architecture.md#graph-compile) · [历史提交](../../../../Docs/Architecture.md#history-commit) · [异步读回](../../../../Docs/Architecture.md#readbacks)
- 通过现有诊断通道发布有界图摘要和池快照，核对尺寸和格式切换、无消费者分支、预算耗尽、失败前缀和在途世代。 [诊断合同](../../../../Docs/Architecture.md#diagnostics)

## Capabilities

### New Capabilities

- `render-graph-resource-pool` — 定义共享池、兼容获取、容量准入、持有与退休、空闲淘汰。

### Modified Capabilities

- `render-graph-contract` — 规范局部有效纹理导入和跨帧边界身份。
- `render-graph-compilation` — 对实际存活槽增加对象容量，并核对精确子资源根与物理寿命。
- `render-graph-execution` — 在首用获取批准的槽，交付可确认的导出与末态，保留失败前缀。
- `editor-frame-graph` — 接入池准入、输出代持有和独立安全 UI，保持呈现与 View 发布时序。
- `render-frame-statistics` — 增加有界图摘要、池指标以及容量等待或拒绝的诊断。

## Impact

- 前置 D08-T02 驻留与上传、D08-T04 Compute 同步、D08-T05 传输与非阻塞查询均已验收归档。共用现有 RHI 状态跟踪、Graphics 提交与完成身份。 [开发计划](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t02)
- 直接消费者包括 RenderGraph 编译与执行、Runtime 帧编排、Renderer/View、Editor UI 纹理和统计面板。宿主呈现、外部资产驻留和 staging 仍由原 owner 管理。首发复用完整原生对象，目标为现有 Windows x64/OpenGL 基线。 [运行帧合同](../../../../Docs/Architecture.md#frame-flow)
