# Spec Delta

## MODIFIED Requirements

### Requirement: Consume the current pipeline resource mapping for every affected binding

实际图形执行 SHALL 依当前 Pipeline 的活动逻辑资源映射绑定正确的 UBO/只读 SSBO byte ranges 及 Texture/Sampler 组合. 同一纹理配不同 Sampler、跨集合配对与 texture-only fetch SHALL 保持准备时的语义, 不修改 Texture 的采样参数. Pipeline 或组合任一端的集合改变时 SHALL 更新相应映射, 不因 BindingSet 指针不变而沿用另一 Pipeline 的原生位置.

活动 volatile 常量 SHALL 在实际 Draw 前解析本 recording 中最近一次有效写入的原生范围，写入的有效长度 SHALL 覆盖活动 block。新的写入即使复用同一 Pipeline 与 BindingSet 且没有再次设置图形状态，随后的 Draw 仍 SHALL 使用新范围。活动 push 常量 SHALL 来自本次有效图形状态设置后的合法写入，并通过保留位置绑定自己的范围。重新设置有效图形状态或清除状态 SHALL 使旧 push 数据失效。新 recording SHALL 要求自己的 volatile 写入。缺少有效动态常量数据时 SHALL 拒绝受影响的绘制并保留已有提交保活责任。

#### Scenario: Observe independent bindings in shader results
- **WHEN** 程序从不同集合的同号 slot 读取常量/结构化数据, 并使用跨集合采样对、同纹理不同 sampler 及 texture-only fetch
- **THEN** 已知输出分别对应其正确资源、byte range、视图和 sampler, 不发生 set/slot 冲突或错误配对

#### Scenario: Change the pipeline while retaining the sets
- **WHEN** 相同集合依次用于具有不同活动映射的两个兼容 Pipeline, 或仅替换一个采样对的一端集合
- **THEN** 后续结果使用新 Pipeline/集合所需的完整映射, 不残留旧物理单元或采样组合

#### Scenario: Draw with two versions under one graphics state
- **WHEN** 同一图形状态中，两次用不同值写入活动 volatile 常量及 push 块并绘制可区分区域，两次之间沿用原 Pipeline 和集合
- **THEN** 真实读回各自对应其写入数据，同时普通 UBO 读值保持正确，原生绑定缓存命中不跳过动态范围更新

#### Scenario: Reject a draw with missing current dynamic values
- **WHEN** 新 recording 尝试借用上一轮 volatile 写入，或重新设置图形状态后仍未写入所需 push 值
- **THEN** 对应 Draw 报告缺少本轮实际数据，拒绝使用陈旧区段；写入本轮合法值后，合法候选可按当前范围执行
