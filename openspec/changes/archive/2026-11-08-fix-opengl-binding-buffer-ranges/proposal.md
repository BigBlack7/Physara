# Proposal

## Why

[D03-T05](../../../../Docs/Developments/D03-D05-FramePipeline.md#d03-t05) 的独立 B 审查发现 [CR-001](tasks.md#b-final-review): D03-T02 已归档的 OpenGL 绑定 owner 错误拒绝公共 BufferRange 的零长度剩余范围. 本 change 仅承接这一存量合同修正, 不将它计作 T05 新能力或改写历史归档.

## What Changes

- 常量及只读结构化 Buffer 绑定按公共语义接受 `byteSize=0`, 与明确的剩余长度及全一长度哨兵等价.
- 保持原有 offset/显式长度边界、非空、CB 部分范围256-byte约束、记录粒度和真实设备限制, 不借用 clamp 将非法请求变成成功.
- 在已有 build 消费者增加最小范围对照/拒绝回归, 更新修复后输入与原生证据, 保留 B 的注释和已有 T05 图像预期.

## Capabilities

### New Capabilities

无.

### Modified Capabilities

- `opengl-shader-pipelines`: 在现有不可变绑定集合 requirement 中明确零长度与全一哨兵的剩余范围语义, 补充等价绑定和越界拒绝场景, 保留原有场景.

## Impact

- 正式修复仅位于 `RHI/OpenGL/Bindings.cpp` 的范围解析 owner 及直接说明, 公共 RHI、Common/Validation、Shader ABI、CMake 和依赖方向不变. 依据为 [架构7.1](../../../../Docs/Architecture.md#rhi)、[7.3](../../../../Docs/Architecture.md#opengl) 和公共 BufferRange/Resolve.
- 验证与脚本仍只在 build 下; 复用 T05 消费者与缓存, 不创建测试框架. 本修正沿用同一问题单和 A/B 轮次, A 整改后交 B 复核, 通过后才分别执行本 change 与 T05 的最终 verify/sync/archive.
