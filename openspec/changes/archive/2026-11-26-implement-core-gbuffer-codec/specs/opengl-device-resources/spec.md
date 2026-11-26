# Spec Delta

## ADDED Requirements

### Requirement: Expose packed emission and unsigned flag attachment usages

OpenGL SHALL 在普通单样本 Texture2D 上按实际设备能力分别公开 R11G11B10_FLOAT 与 R16_UINT 的纹理、颜色附件及按 texel 读取用途，接受匹配的浮点与无符号清除。四附件组合 SHALL 受实际 draw buffer、颜色附件数、格式与样本能力校验；缺少必需用途的设备 SHALL 明确拒绝对应资源或组合。

#### Scenario: Create and consume a mixed-format framebuffer

- **WHEN** 设备支持 RGBA8_SRGB、RGBA16_FLOAT、R11G11B10_FLOAT、R16_UINT 和 D32 的相应用途，调用者准备同尺寸四颜色附件与深度附件
- **THEN** 组合可用于单样本光栅与后继纹理读取，整数清除可精确恢复，浮点与整数附件遵守各自的颜色解释。

#### Scenario: Request an unavailable attachment configuration

- **WHEN** 所需颜色附件数超出实际限制，或请求的格式缺少声明的必需用途
- **THEN** 能力查询及创建结果与实际限制一致，失败指出不支持的用途或组合。
