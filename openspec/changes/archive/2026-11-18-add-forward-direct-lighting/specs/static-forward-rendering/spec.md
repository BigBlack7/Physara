# Static Forward Rendering Spec Delta

## MODIFIED Requirements

### Requirement: Display an explicit unlit and material preview profile

Forward 预览 SHALL 在现有 View Settings 提供 Base Color、World Normal、Material Channels、Emissive 和 Direct Lighting 模式，初始模式为 Base Color。Unlit 在各模式使用标准基色。其余已支持表面在四种诊断模式显示基色、世界法线映射、`R=AO/G=roughness/B=metallic` 或线性自发光，在 Direct Lighting 显示正式直接光结果。工作颜色 SHALL 保持有限的线性值并保留附件可表示的超白值。当前 View 输出 SHALL 将工作色线性限幅至 [0,1] 并提供 display-linear 采样语义，UI 沿现有合成与窗口编码合同消费。界面 SHALL 如实报告当前请求、正常图已接受模式、物理照明范围及显示 profile 的可用性。模式切换沿现有 View 运行请求处理，v1 场景文件结构保持兼容。

#### Scenario: Change material preview without rebuilding static storage

- **WHEN** 用户在同一已加载场景的现有视口切换到法线、材质通道或自发光预览
- **THEN** 当前匹配帧显示对应诊断值与模式标识，Unlit 保持基色，已驻留且解释相同的模型、纹理与实例行可复用

#### Scenario: Preserve an emissive value above display white in the working output

- **WHEN** 非 Unlit 表面在 Emissive 预览中产生附件可表示的线性自发光值大于 1
- **THEN** 工作颜色保留该值，视口采样得到固定映射后的显示线性颜色，颜色转换和窗口编码各在自己的边界生效

#### Scenario: Select Direct Lighting from the existing viewport

- **WHEN** 用户打开含灯和已支持表面的场景，从 Base Color 请求 Direct Lighting，随后切回诊断模式
- **THEN** 合法请求先在面板可见，正常图实际采样并接受后确认匹配的模式和色值，暂时不可用时保留旧输出资格，诊断颜色与 Unlit 的标准基色仍可得到

## ADDED Requirements

### Requirement: Shade lit surfaces in the current pre-exposed working domain

Direct Lighting 的受光 OPAQUE 和 MASK SHALL 通过共享表面求值及核心 BRDF 消费完整的当帧直接光。覆盖、变换和背面法线沿用静态表面合同。透视观察向量 SHALL 取片元朝眼点的方向，正交 SHALL 使用固定反视线方向。几何粗糙度 SHALL 在覆盖分歧前取得有效导数，其结果供本片元所有灯使用。直接光各项 SHALL 乘一次入射余弦并线性相加，AO 不衰减直接光或自发光。直接光与线性自发光 SHALL 各乘一次同轮 `preExposure`。无灯时，非发光受光表面直接输出零光照，自发光仍按当帧曝光显示。

#### Scenario: Accumulate direct light and emission at the current exposure

- **WHEN** 固定可见表面和相机几何，先分别测量方向及局部灯，再同时启用并增加一档曝光补偿，最后关闭所有灯
- **THEN** 直接光符合各灯线性叠加，直接光和自发光在工作色可表示的范围内同时随 +1 EV 翻倍，关闭所有灯后罩内像素只保留预曝光自发光或零

#### Scenario: Preserve masked coverage in the two opaque pass arrangements

- **WHEN** 带有遮罩孔和临界 alpha 的受光表面分别使用合并颜色深度和前置深度组合，并从明确投影类型的相机观察
- **THEN** 两种组合得到相同覆盖和反向深度，留下的受光片元使用符合该投影的观察向量、同一表面过滤和直接光响应
