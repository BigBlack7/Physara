# Proposal

## Why

当前设计将动画、骨骼和动态变形纳入 Physara，偏离维护者明确的静态场景物理渲染器定位；场景保存、材质映射、时域效果和同步的若干跨模块合同也尚未闭合。应在业务实现开始前修正设计依据、架构与开发顺序，避免后续返工。

## What Changes

- **BREAKING**：从项目架构及开发范围移除动画、骨骼、Morph 和动态变形；保留编辑静态场景、相机导航和时域抗锯齿。
- 明确单个场景 JSON 保存 Assets 引用、实体参数、实例材质/纹理覆盖及主 View 配置；glTF/GLB 源资产只读。
- 采用静态场景相机重投影，补齐 Filament TAA/SSR 的数据流与标准 glTF 到物理材质的映射。
- 修正 GL 同步、提交失败与退出合同，明确 NVRHI 完整公共契约和按能力实现后端的边界。
- 依据真实消费调整 D01 及受影响工作包，清理当前文档中的错误、重复承诺与失效链接。

## Capabilities

### New Capabilities

无。

### Modified Capabilities

- `project-baseline`：收紧模型与场景设计边界，明确静态定位、只读模型和 JSON 实例覆盖；本变更仍只交付设计，不声称已实现运行期功能。

## Impact

影响 AGENTS.md、Docs/Architecture.md、Docs/Developments.md、Docs/Reference 两份技术分析、README.md、Assets/README.md 及 OpenSpec 当前上下文。重点承接修订后的 D01-T01–T04、D02-T01/T02、D06–D08、D12–D16 与 D19；移除无独立增量的动画阶段，后续编号和交叉引用同步更新。

本变更仅修改文档与规格，不修改 C++、Shader、CMake、第三方快照或 D00 归档，不增加工具、测试框架或验证报告文件。
