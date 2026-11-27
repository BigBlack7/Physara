# Spec Delta

## ADDED Requirements

### Requirement: Import the standard IOR and specular material inputs

所选静态 scene 使用的 metallic-roughness 材质 SHALL 支持独立及同时使用 `KHR_materials_ior` 和 `KHR_materials_specular`。CPU 记录 SHALL 保留扩展声明、标准参数、有效纹理引用与采样语义。IOR 缺省为 1.5，接受有限可表示的 `ior >= 1` 及单独的兼容值 `0`。specularFactor 缺省为 1，合法范围为 [0,1]。specularColorFactor 缺省为线性 RGB (1,1,1)，接受有限可表示的非负分量，包括大于 1 的合法值。

specularTexture SHALL 使用线性 A 通道数据，specularColorTexture SHALL 使用 sRGB RGB。两个槽分别保留各自的 sampler、UV 集及 `KHR_texture_transform`。有效数据槽缺少图像 A 通道时 SHALL 得到 1。读取的外置扩展纹理属于模型依赖。已支持扩展的非法字段、不可用 UV 或不合法引用 SHALL 在发布前定位到材质及字段或 Primitive 槽。与 `KHR_materials_unlit` 或 `KHR_materials_pbrSpecularGlossiness` 同时出现在同一活动材质的 IOR/specular 扩展 SHALL 被拒绝。

#### Scenario: Publish the standard extension values and image dependencies
- **WHEN** 可达材质含 IOR、specular 的独立声明与联合声明，含缺省项、兼容值 0 和大于 1 的颜色因子，两个槽共用一份有非对称 RGBA 的图像但使用不同采样设置，随后只更换参与导入的外部图像字节
- **THEN** CPU 记录保留声明及合法初值，强度与颜色槽独立解释同一图像，替换图像产生新 sourceVersion，固定的旧记录仍可读

#### Scenario: Reject malformed or incompatible supported material extensions
- **WHEN** 选定材质使用介于 0 和 1 的非零 IOR、有效扩展槽缺少所需 UV，或组合了同材质的 Unlit 和 specular
- **THEN** 导入交付对应字段、槽或扩展冲突的错误，失败候选无法取得 CPU 就绪资格

## MODIFIED Requirements

### Requirement: Report extension support and valid core fallback

模型 SHALL 公布足以定位的支持报告。在全文件声明中，任何当前未支持的 `extensionsRequired` 成员 SHALL 拒绝。当前 CPU 支持 `KHR_lights_punctual`、`KHR_materials_unlit`、`KHR_materials_emissive_strength`、`KHR_texture_transform`、`KHR_materials_ior` 和 `KHR_materials_specular` 的上述标准描述。其它 optional 扩展有完整可用的核心回退时 SHALL 沿该回退导入并报告降级，缺少选定场景必需的可用核心数据时 SHALL 报告不支持。

#### Scenario: Reject a required extension outside the current profile
- **WHEN** 文件声明当前未支持的 required 扩展，即使存在核心字段或它只关联另一个 scene
- **THEN** 拒绝加载并列出该扩展，模型未发布

#### Scenario: Allow only an available optional fallback
- **WHEN** 未支持的 optional 扩展出现于带有效核心回退的输入，或出现在没有可用核心数据的活动内容
- **THEN** 前者发布核心结果并报告扩展降级，后者给出定位明确的未支持结果

#### Scenario: Report required IOR and specular as CPU capabilities
- **WHEN** 合法文件在 extensionsUsed 和 extensionsRequired 中声明 IOR 与 specular，所选静态内容通过字段与引用校验
- **THEN** 模型可发布对应 CPU 材质数据，报告标明两个扩展的 CPU 支持，Renderer 和编辑器仍按实际帧的就绪结果报告受光状态
