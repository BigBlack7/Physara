# Proposal

## Why

Physara 需要一个便于个人维护的轻量工程起点。落实 D00-T01–T04，使当前第三方源码能够从干净目录构建，明确模型资产与编辑器场景的格式边界，并让文档、构建和实际目录保持一致。

## What Changes

- 第三方保留实际需要的源码、头文件和许可；版本、来源及本地差异集中记录在 ThirdParty/README.md。
- 仅用根目录和模块内的 CMakeLists.txt 组织当前源码；NVRHI 仅供参考，不参与构建。
- 模型资产使用 glTF/GLB，编辑器场景使用 JSON；在架构中明确导入、实例化、场景加载与保存的职责和失败行为。
- 工程初始化不引入 CI、独立 Tools 模块、开发工具安装、格式检查门禁或预置测试体系；格式化沿用维护者的 VS Code 设置。
- 沿用官方 OpenSpec 1.13.2 与 spec-driven schema，工作包状态仅依据当前可核对的证据更新。

## Capabilities

### New Capabilities

- `project-baseline`: 轻量构建、可追溯依赖和明确资产边界的工程起点。

### Modified Capabilities

无。

## Impact

影响工程入口、第三方构建和说明、AGENTS.md、架构、开发计划、资源说明及本 change。保持现有资源和维护者删除内容的状态，不实现 Runtime、Editor、场景序列化或渲染功能。测试组织由维护者后续确定。
