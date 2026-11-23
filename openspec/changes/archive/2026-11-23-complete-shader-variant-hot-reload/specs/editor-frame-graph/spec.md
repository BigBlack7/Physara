# Spec Delta

## ADDED Requirements

### Requirement: Freeze a ready Shader generation before composing scene work

产品帧 SHALL 在声明正常图前固定可执行 Shader 代、各 View 已就绪计划及其绑定和资源代。帧内各消费路径 SHALL 使用相同的世代事实。因请求等待或准备失败而缺少已就绪计划时，帧 SHALL 使用仍合法的旧计划、上次接受的输出或带原因的占位，保持请求与有效结果可区分。安全 UI SHALL 能使用其已准备的基础程序独立合成。

Shader 代发布和 View 输出确认 SHALL 是独立事实。只有该 View 的匹配版本被正常图实际消费且相关 Graphics 提交获得接受，才确认新代对应的有效 View 结果。过期请求、失败前缀、安全 UI 和已经发布但尚未被匹配提交使用的 Shader 代 SHALL 保留上一有效 View 结果及其来源。

#### Scenario: Keep a compatible viewport while new preparation is unavailable

- **WHEN** 主视图请求需要新 Pipeline 的配置，该候选仍 Pending 或 Failed，已有一个可采样的 View 输出
- **THEN** 本轮图选择已就绪的正常计划或显式续用旧输出。当前请求及原因可见，旧输出的代和接受票据保持可追溯

#### Scenario: Confirm a new generation only through its actual submitted frames

- **WHEN** 新代已在帧边界激活，正常图已准备主视图及离屏结果，但一个出口未参与本次实际接受的工作
- **THEN** 仅与实际有效输出和接受提交匹配的 View 更新确认。另一 View 保留上次确认，安全 UI 同样可以继续消费合法旧输出
