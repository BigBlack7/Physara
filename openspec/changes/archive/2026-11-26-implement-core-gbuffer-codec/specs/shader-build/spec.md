# Spec Delta

## ADDED Requirements

### Requirement: Preserve unsigned 2D texel fetch in graphics fragment packages

图形片元接口 SHALL 能声明独立的单样本、非数组、无符号整数二维采样纹理。生成声明、ABI 身份、最终 IR 活动资源反射及正式包消费 SHALL 区分同 set/slot 的浮点和无符号纹理。按 texel 读取 SHALL 使用既有 Texture SRV 绑定，无符号图像只允许无逻辑 Sampler 的 fetch。构建与读取包时 SHALL 拒绝把该图像与逻辑 Sampler 配对的最终 IR。既有浮点纹理声明及既有包的合法解释 SHALL 保持兼容。

#### Scenario: Load the declared numeric kind and reject a conflicting consumer

- **WHEN** 一个图形片元程序从无符号二维纹理读取标志，并分别使用匹配接口和同 slot 的浮点接口作为加载预期
- **THEN** 匹配包保留无符号数值类别、二维形状及绑定，冲突预期得到明确的接口不匹配；既有浮点包仍按其原接口加载。
