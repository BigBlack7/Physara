# Proposal

## Why

[D03-T03](../../../../Docs/Developments/D03-D05-FramePipeline.md#d03-t03) 已交付真实即时 Draw 及必要保活, 但公共 Event/Timer 查询仍明确不支持, 完整绘制在途验收尚未完成. 下一工作包 [D03-T04](../../../../Docs/Developments/D03-D05-FramePipeline.md#d03-t04) 需要把查询消费者接到已有提交/完成/删除协议, 证明 CPU 释放、后续上传和失败退出均不会提前回收 GPU 仍使用的内容.

## What Changes

- 保留唯一即时 Graphics 列表、物理包与 fence FIFO, 完成真实 Draw 的 closed/submitted/completed、复合依赖与上传区段验收; 修正本包实际暴露的缺口, 不重建已交付的提交系统.
- 实现公共 EventQuery, 捕获设置时最后已接受的 Graphics 提交身份, 通过同一完成权威进行非阻塞 poll、显式等待及 reset, 不把录制前缀或后续提交纳入快照.
- 实现公共 TimerQuery 的即时 GPU 计时点、同 recording 配对、available 检查和秒单位结果; 支持有界查询对象轮转, 约束未完成复用、释放与失败后的状态.
- 将新查询对象/原生名字纳入同一引用、预算、owner-context 删除及正常/失效退出协议, 保留外部 owner 对关闭的阻止作用.
- 用共享的少量真实绘制/数据输入及必要受控失败观察完成 T04 验收, 更新已交付能力和证据边界; Runtime 的普通事件循环不宣称已有渲染画面.

范围仅为 **D03-T04**. 普通 Buffer/Texture 后续上传使用已有路径, 不实现 D08-T02 的 volatile/push 多版本, 不增加 Shader/绑定 profile、Compute/UAV/indirect、多队列或可重放列表. 公共 staging/map、异步截图/拾取、Renderer/Editor/呈现、稳定图像基准与 D03 阶段出口仍由后续工作包交付.

## Capabilities

### New Capabilities

- `opengl-completion-queries`: 公共 EventQuery 的已接受提交快照, TimerQuery 的真实 GPU 区间、可用性、单位和合法复用, 以及查询输入、引用/预算和失败边界.

### Modified Capabilities

- `opengl-device-resources`: 将已交付 Event/Timer 查询与仍不支持的读回/扩展操作区分, 明确 T04 在途验收与 T05/呈现/阶段出口的关系.

已有 [opengl-resource-uploads](../../../specs/opengl-resource-uploads/spec.md) 的提交、Complete/Pending/Failed、上传存储和退出合同, 及 [opengl-immediate-draw](../../../specs/opengl-immediate-draw/spec.md) 的图形依赖保活合同直接复用. T04 将这些合同用于完整 Draw 消费链验收, 不另建一份同义规范或改写历史归档.

## Impact

- 主要位置为 RHI/OpenGL 的 Queries、CommandList 查询入口、Submission/Upload、Device/删除协议, 直接消费者为现有 Graphics/BindingSet/Framebuffer 和 Platform/RhiDeviceHost. Validation 继续校验公共前提并委托, 生效状态仍由原生 owner 保存.
- 保留公共 RHI 的签名/布局/枚举和 Shader ABI. CMake 只显式接入必要正式实现, 无新增第三方、下载、默认测试目标或产品入口; 通用 RHI 不引入 native 依赖.
- 遵循 [11.1 在途退休](../../../../Docs/Architecture.md#retirement)、[11.3 查询/读回](../../../../Docs/Architecture.md#readbacks)、[11.5 退出](../../../../Docs/Architecture.md#shutdown) 与 [7.4 上传](../../../../Docs/Architecture.md#uploads). 本次不提出架构变更.
- 开发检查仅在 `build/d03-t04/`, 可引用输入匹配的既有证据, 不重复未受影响的图形/格式矩阵. 独立 A/B 代码与完整文件注释闭环完成后才最终 verify、同步和归档.
