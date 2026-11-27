# View Preparation Spec Delta

## ADDED Requirements

### Requirement: Preserve opaque appearance across matched supported strategies

Forward、Forward+ 与 Deferred SHALL 在匹配的单样本受光配置下，对同一冻结场景、有效材质与实例、可见灯、相机、曝光、渲染矩形、阴影和环境输入交付等价的不透明画质。最近表面的选择基于现有覆盖和深度合同。具有共同确定胜出者的像素 SHALL 保持相同几何覆盖、最终 Reverse-Z 深度和核心、Unlit 及当前受支持辅助表面的可见性。MASK 及有效实例覆盖 SHALL 影响同一批样点。天空及留边 SHALL 服从同一图像范围和遮挡语义。

等价 SHALL 在正常接受且确认了实际策略的显示前线性 RGBA16F 和配对主 D32 上判定。已定义像素 SHALL 有有限数值。允许的核心 HDR 差异 SHALL 有针对相应输入预先登记的范围，可追溯至 GBuffer 编码、屏幕位置重建、阴影采样或工作色精度。Unlit SHALL 保持共享的颜色与覆盖。使用 Froxel 的路径 SHALL 保留同一有效方向灯和无范围局部灯贡献。溢出或合法的不确定查询 SHALL 用完整可见灯表保持对应画质，受光样点保留可辨认的实际退路。

#### Scenario: Compare mixed static surfaces with the matching accepted outputs

- **WHEN** 三策略分别接受匹配的核心静态实例和 Unlit MASK 输入，包含不同实例覆盖、前后遮挡、有限远面和固定比例留下的边缘
- **THEN** 在相同像素得到配对的覆盖、D32 和预曝光线性颜色。孔洞保留后方实际颜色，胜出的 Unlit 保留标准基色，核心的差异落在登记的精度范围内。留边和远面可由范围、覆盖与深度配对独立核对。

#### Scenario: Compare shadowed and unshadowed lighting under one effective profile

- **WHEN** 同一冻结场景的三策略实际消费相同阴影质量、方向光级联和局部阴影组，以及匹配的 IBL、天空与曝光
- **THEN** 相同接收面、遮挡区、亮区及天空的工作色和深度满足已登记的画质判据。移除阴影或环境输入的独立对照能辨认其对应贡献。

#### Scenario: Preserve global local lights through compact and fallback traversal

- **WHEN** 同一受光工作集含有限和无范围局部灯，Forward 使用完整表，Forward+ 和 Deferred 在实际照明中同时遇到正常格与受限额度造成的溢出格
- **THEN** 正常格保留适用的紧凑候选及全局灯；溢出格沿完整可见灯表取得贡献。三策略均在代表性受光像素和整图落入登记的颜色、覆盖与深度范围，负对照能发现遗漏的灯贡献。
