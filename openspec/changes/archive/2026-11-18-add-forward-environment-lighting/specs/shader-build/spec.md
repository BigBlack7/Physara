# Spec Delta

## ADDED Requirements

### Requirement: Preserve sampled texture dimensionality in graphics packages

权威图形接口 SHALL 表达独立浮点 `texture2D` 和 `textureCube` 的区别。生成声明、ABI、最终 IR 的活动资源反射及包消费 SHALL 保留实质维度并相互核对。cube 基线 SHALL 使用非数组、非多样本、非深度浮点采样图像及独立 Sampler，沿用逻辑 Texture SRV 身份和 Vulkan set/binding 约定。

#### Scenario: Compile and load a canonical cube sampling consumer

- **WHEN** 图形程序以匹配的接口声明采样独立浮点 cube，调用者按生成 ABI 加载包
- **THEN** canonical IR 和包的活动反射保留 cube 维度与原逻辑身份，包可交给受支持的原生消费者准备

#### Scenario: Reject a dimension mismatch at the package boundary

- **WHEN** 接口声明 cube，最终 IR 在同一逻辑位置提供二维图像，或使用基线范围外的 cube 数组
- **THEN** 核对报告维度或 profile 原因，拒绝发布该包
