# Proposal

## Why

D00 已建立可重建的第三方依赖与文档基线, 但尚无第一方窗口宿主. D01 必须先证明真实 OpenGL context 的创建、失败回滚和关闭可靠, 才能继续接入输入、Runtime 和 RHI, 避免各模块分别接管原生环境.

## What Changes

- 覆盖 [D01-T01/T02](../../../../Docs/Developments.md#d01-t01): 让窗口链实际消费类型化身份、单调秒计时、带上下文错误及有界诊断.
- 建立唯一的 `DeviceHost` 创建/关闭边界, 顺序完成 GLFW 初始化、窗口/context、current、GLAD 加载、实际 GL 4.6 Core 核查和必要环境查询; 支持可见与隐藏窗口使用同一实现.
- 返回拥有自身数据的环境信息, 包括宿主身份、原生版本/厂商/渲染器、基础 limits、默认 framebuffer 编码及启动耗时; 观测值与后续后端能力声明分开.
- 明确主线程和单活跃宿主合同, 为部分初始化、原生错误回调、重复创建销毁和幂等关闭建立同一释放协议.
- 通过实际宿主和最小 Core 检查验证成功、边界与失败; 构建接入现有模块, 证据随对应 tasks 保留.

本 change 不覆盖 D01-T03/T04 的有序输入、窗口状态发布和产品主循环. RHI Device 工厂、资源/绘制、GPU 提交、编辑器、正式呈现分别由后续工作包推进; 本次规划完成也不代表 D01 或任何实现任务已验收.

## Capabilities

### New Capabilities

- `native-window-host`: 定义真实 GL 宿主的创建条件、环境信息、身份/时间/诊断、线程和唯一性、失败及释放的可观察合同.

### Modified Capabilities

无. `project-baseline` 的构建、依赖与初始化约束继续适用, 不修改其已采纳要求.

## Impact

实施将影响 `Engine/Core`、`Platform` 及其模块 CMakeLists, 必要时调整根 CMake 的子目录顺序. 只使用已固定的 GLFW、GLAD、spdlog 与 C++20 标准库; GLFW/GLAD 保持 Platform 私有依赖, Engine targets 不依赖 Platform, NVRHI 仍仅作参考.

实施按实际证据更新 README 的可用范围与 Developments 的 D01-T01/T02 状态. 验证复用正式 DeviceHost, 临时检查和输出放在 build 树, 不预设测试框架、独立 Tools 或第二个产品入口. 实施不覆盖 Runtime、RHI 或 Editor, 不提前勾选 D01-T03/T04, 主规格同步与归档沿后续原生流程进行.
