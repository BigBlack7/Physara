# Spec Delta

## MODIFIED Requirements

### Requirement: Display an explicit unlit and material preview profile

Forward 预览 SHALL 在现有 View Settings 提供 Base Color、World Normal、Material Channels、Emissive、Direct Lighting 和 Physical Lighting 模式，初始模式为 Base Color。Unlit 在各模式使用标准基色。其余已支持表面在四种诊断模式显示基色、世界法线映射、`R=AO/G=roughness/B=metallic` 或线性自发光，在 Direct Lighting 显示直接光结果。Physical Lighting SHALL 使用同一表面模型、完整直接光、DFG 与自发光，并按实际环境用途加入 IBL 和天空。场景省略环境时，Physical Lighting SHALL 交付带 DFG 补偿的直接光及自发光。工作颜色 SHALL 保持有限的线性值并保留附件可表示的超白值。Physical Lighting SHALL 由固定 ACESToneMapper/ColorGrading profile 得到 display-linear 颜色，包括该视图内 Unlit 表面所交付的基色。Base Color、World Normal、Material Channels、Emissive 与 Direct Lighting 仍 SHALL 用线性限幅 [0,1] 定义诊断显示颜色，诊断值的含义沿用其工作色合同。ViewOutput SHALL 提供 display-linear 采样语义，UI 沿现有合成与窗口编码合同消费。界面 SHALL 如实报告当前请求、正常图已接受模式、物理照明范围及显示 profile 的可用性。模式切换沿现有 View 运行请求处理，已有 v1 场景 SHALL 可直接恢复。

#### Scenario: Change material preview without rebuilding static storage

- **WHEN** 用户在同一已加载场景的现有视口切换到法线、材质通道或自发光预览
- **THEN** 当前匹配帧显示对应诊断值与模式标识，Unlit 保持基色，已驻留且解释相同的模型、纹理与实例行可复用

#### Scenario: Preserve an emissive value above display white in the working output

- **WHEN** 非 Unlit 表面在 Emissive 预览中产生附件可表示的线性自发光值大于 1
- **THEN** 工作颜色保留该值，视口采样得到固定映射后的显示线性颜色，颜色转换和窗口编码各在自己的边界生效

#### Scenario: Select Direct Lighting from the existing viewport

- **WHEN** 用户打开含灯和已支持表面的场景，从 Base Color 请求 Direct Lighting，随后切回诊断模式
- **THEN** 合法请求先在面板可见，正常图实际采样并接受后确认匹配的模式和色值，暂时不可用时保留旧输出资格，诊断颜色与 Unlit 的标准基色仍可得到

#### Scenario: Shade a scene without an environment in Physical Lighting
- **WHEN** 现有 v1 场景无环境请求，用户选择 Physical Lighting，并使用已支持的受光几何
- **THEN** 正常帧交付带核心 DFG 补偿的直接光和预曝光自发光，视口报告场景未配置环境，遮挡及有效图像矩形符合当轮 View
