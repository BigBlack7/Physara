# Proposal

## Why

[D01-T01/T02](../../../Docs/Developments.md#d01-t01) 已交付并提交真实 GL 宿主, 但窗口状态尚未持续发布, 没有有序输入或可运行的产品入口. [D01-T03/T04](../../../Docs/Developments.md#d01-t03) 需要在同一宿主上闭合事件处理、调用方关闭决策与退出, 作为后续 RHI 和编辑器的前置.

## What Changes

- 完成 D01-T03: 发布实际逻辑尺寸、framebuffer 像素尺寸、content scale、焦点、最小化和关闭意图; 将鼠标、键盘、Unicode 文本累积为有序事件及 poll 后只读快照.
- 固定输入发布、借用寿命、按键边沿/重复、失焦清理和相对光标模式的合同; 对缓冲超限、计数耗尽及原生失败给出有界错误和安全退出, 不把不完整输入作为成功帧.
- 通过明确的宿主调用边界应用关闭标志和光标模式请求; 回调只采集事实, 保持稳定 user pointer 与后续官方 UI callback chaining 的兼容性.
- 完成 D01-T04: 让 `Runtime/Main.cpp` 成为唯一产品入口, 装配已存在的 Diagnostics/DeviceHost, 顺序执行 poll、关闭/最小化判断及空闲等待, 采用统一的逆依赖关闭路径.
- 提供小型启动参数和受限运行选项, 让可见/隐藏启动、原生宿主失败、关闭与进程退出可重现; 产品与临时检查继续消费同一正式宿主.
- 按正常、边界、故障与真实 Win32 事件检查实施, 同步记录文档与证据; 没有运行的验证不勾选, D01 总出口仅在其验收满足后更新.

本 change 不实现 RHI Device、绘制/交换、GPU 目标、Renderer、Editor 或 UI 面板. 文件拖放/导入、剪贴板、IME 组合界面、raw mouse、游戏手柄及全屏显示管理随实际消费者接入; 本轮文本范围是 Unicode 提交字符, 光标范围是普通与相对模式.

## Capabilities

### New Capabilities

- `window-input`: 窗口实时事实、有序输入/快照、单位与寿命、失焦/光标/关闭请求、等待、回调与有界失败的合同.
- `runtime-lifecycle`: 单产品入口、启动参数、事件与空闲循环、可观察进程结果及有序退出的合同.

### Modified Capabilities

无. 已完整核对 [native-window-host](../../specs/native-window-host/spec.md) 的 8 条要求和 [project-baseline](../../specs/project-baseline/spec.md) 的 4 条要求: 新行为由以上两个能力承载, 既有创建/线程/回滚与轻量构建要求继续适用, 不改名或重复复制原有能力.

## Impact

实现影响 `Platform/Input.hpp`、私有输入状态/GLFW 适配、现有 DeviceHost 的启动/关闭和公开事件入口, 以及 `Runtime/Main.cpp`、Platform/Runtime 的 CMakeLists. Core、GLAD 和 GLFW 版本保持不变; Platform 的原生依赖仍私有, Runtime 只消费 Core/Platform 的公开合同, Engine 不反向依赖 Platform.

Docs/Build.md 和 Developments 随实际入口及已验证范围更新, README 只保留项目介绍与文档导航; 具体数据流、预算、错误协议和检查矩阵见 design/tasks. 临时 CPU/原生/UI chaining 检查留在 build 树, 复用已有固定依赖; 不增加 CI、独立 Tools、仓库测试框架或第二套产品入口. 当前阶段没有渲染结果、GPU 完成或多硬件兼容性的新增承诺.
