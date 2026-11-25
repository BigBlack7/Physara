# shader-build Spec Delta

## MODIFIED Requirements

### Requirement: Preserve sampled texture dimensionality in graphics packages

权威图形接口 SHALL 表达独立浮点 `texture2D`、`textureCube` 和 `texture2DArray` 的区别。生成声明、ABI、最终 IR 的活动资源反射及包消费 SHALL 保留实质维度并相互核对。cube 基线 SHALL 使用非数组、非多样本、非深度浮点采样图像及独立 Sampler，沿用逻辑 Texture SRV 身份和 Vulkan set/binding 约定。二维数组 SHALL 作为单个普通浮点非多样本 Texture SRV 表达层维度，支持纹理独立 fetch，沿用当前图形 profile 的资源及反射合同。

#### Scenario: Compile and load a canonical cube sampling consumer

- **WHEN** 图形程序以匹配的接口声明采样独立浮点 cube，调用者按生成 ABI 加载包
- **THEN** canonical IR 和包的活动反射保留 cube 维度与原逻辑身份，包可交给受支持的原生消费者准备

#### Scenario: Reject a dimension mismatch at the package boundary

- **WHEN** 接口声明 cube，最终 IR 在同一逻辑位置提供二维图像，或使用基线范围外的 cube 数组
- **THEN** 核对报告维度或 profile 原因，拒绝发布该包

#### Scenario: Preserve the graphics array image shape through package loading

- **WHEN** 片元程序使用一个已声明的普通浮点二维数组按整数层读取 texel，或以非数组图像替代同一声明
- **THEN** 匹配程序经过 canonical 验证和加载后仍保有单 Texture SRV 的二维数组维度及原逻辑身份。维度失配无法成为有效编译包。

#### Scenario: Reject sampled array visibility in a Compute interface

- **WHEN** 权威接口为 `texture2d-array` 声明 Compute 可见性
- **THEN** 接口生成阶段 SHALL 报告资源名与 Compute 阶段并拒绝发布接口。合法 Compute storage 数组继续按 `image2d-array` 的格式及访问合同生成和打包。
