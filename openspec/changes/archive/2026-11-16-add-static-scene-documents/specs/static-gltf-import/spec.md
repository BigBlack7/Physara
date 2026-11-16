# Static glTF Import Spec Delta

## MODIFIED Requirements

### Requirement: Publish complete model records with dependency identity

模型 SHALL 沿通用 CPU 资产发布合同异步加载，和图片共用作业准入，并在 owner 边界交付完整不可变记录。记录 SHALL 包含所选源 scene、可用的标准数据与引用、支持报告和由实际参与导入的文件字节确定的稳定 `sourceVersion`。相同模型内容及依赖输入 SHALL 保持源版本；显式刷新后仅变更参与导入的外部 buffer 或图像也 SHALL 改变源版本并发布新内容代。取消、失败、源失效或过期资料 SHALL 保留最后有效版本，结果只表达 CPU 就绪。

对固定版本的源核对 SHALL 使用实际参与导入的主文件与外部依赖读证据，支持区分完全匹配、内容改变和无法读取。在文档边界对固定版本的核对 SHALL 保持该版本及其已发布资产状态有效，采用新候选须经独立的 Asset 发布和 Scene 采用合同。核对 SHALL 保持已有 `sourceVersion` 字节身份的解释。

#### Scenario: Commit a validated model through the shared CPU asset service
- **WHEN** 图片和模型使用同一作业额度，模型已在 worker 完成但 owner 尚未处理结果
- **THEN** 模型仍为 `Loading`；在通用发布检查后，才交付完整的 CPU 数据、身份和 sourceVersion

#### Scenario: Change only a resolved model dependency
- **WHEN** 同一 source 和 scene 先以完全相同输入显式刷新，随后只更改参与导入的外部 bin 或图像字节并再次刷新
- **THEN** 第一次保留内容版本，第二次产生新的源版本和完整记录，已固定的旧版本仍可读取

#### Scenario: Leave current content intact after a failed or stale replacement
- **WHEN** 一次替换失败、被取消或在更新的请求之后才交回
- **THEN** 无效结果不发布，诊断和最后有效模型按当前请求状态保留，释放未采纳数据

#### Scenario: Verify a pinned source against its actual dependency bytes
- **WHEN** 场景文档操作核对已固定且仍可读取的模型，期间依次观察相同输入和只改变一个参与导入的外部依赖
- **THEN** 前者报告匹配，后者报告依赖冲突，原模型数据、`sourceVersion` 和 AssetManager 的已发布内容仍可由已有读者取得
