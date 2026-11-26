# Spec Delta

## ADDED Requirements

### Requirement: Validate the numeric kind of active graphics texture fetches

图形片元的原生 Shader 与 Pipeline SHALL 接受普通非多样本二维无符号整数纹理的 texel fetch，保留活动位置的维度和数值类别。绑定消费时 SHALL 将活动位置与实际 SRV 视图解释匹配，浮点与无符号整数类别互换时 SHALL 在发出绘制前报告失败。直接提交的无符号采样图像若与逻辑 Sampler 配对，原生 Shader 创建 SHALL 报告不支持。合法的整数 fetch SHALL 返回未经归一化、混合或过滤的原始整数。

#### Scenario: Fetch a flag pattern and reject a mistyped view

- **WHEN** 对 R16_UINT 纹理中的 0x8105 执行无符号 fetch，并分别尝试将浮点视图接到该位置及将无符号视图接到浮点位置
- **THEN** 合法绘制精确得到 0x8105，两个错误绑定均在成功绘制前给出数值类别失配，Debug 和 Release 语义一致。
