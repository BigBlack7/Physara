# Spec Delta

## ADDED Requirements

### Requirement: Import standard clearcoat material inputs and their tangent prerequisites

选定静态场景的 metallic-roughness 材质 SHALL 接受 `KHR_materials_clearcoat` 声明，并保留强度、感知粗糙度、可选法线和各槽的采样语义。 `clearcoatFactor` 与 `clearcoatRoughnessFactor` 的默认值均为 0，合法范围为 [0,1]。强度纹理使用线性 R，粗糙度纹理使用线性 G，缺席纹理的乘数为 1。清漆法线采用独立线性 RGB 向量槽，normal scale 默认 1，接受有限可表示的值并按 normalTexture 语义作用于 XY。

每个源槽 SHALL 保留自己的 sampler、有效 UV 集和 `KHR_texture_transform`，参与读取的外部图像 SHALL 进入模型依赖身份。带清漆法线槽的 Primitive SHALL 具有可用的源 `NORMAL` 和 `TANGENT`，或由有效基底 normal 槽提供可用切线空间。槽的有效 UV 和切线框架 SHALL 按静态几何合同校验。非法字段、不合法引用和无可用框架的候选 SHALL 定位至材质字段或 Primitive 槽。清漆与 Unlit 或 pbrSpecularGlossiness 出现在同一活动材质时 SHALL 拒绝该候选。清漆可与合法的 IOR/specular、emissive_strength 和 texture_transform 一同导入。

#### Scenario: Publish standard clearcoat values and independently interpreted slots
- **WHEN** glTF 或 GLB 包含缺省声明和有图的清漆材质，强度与粗糙度槽共享 R/G 分量可区分的图像且使用不同采样设置，随后只替换参与导入的外部清漆图像字节
- **THEN** 固定 CPU 记录保留声明、缺省值、线性通道和各自槽语义，图像改动产生新 sourceVersion，旧固定结果仍可读

#### Scenario: Resolve or reject a clearcoat normal within the selected primitive
- **WHEN** 清漆法线分别用于已有源切线框架的几何、由有效基底法线槽建立框架的几何，以及缺少所需有效 UV 或切线空间的输入
- **THEN** 合法输入取得对应有效采样框架，非法输入定位到清漆法线槽，失败候选无法发布 CPU 材质

#### Scenario: Reject malformed fields or a prohibited clearcoat combination
- **WHEN** 选定材质的清漆因子超出合法范围，或同一材质声明清漆及 Unlit 或 pbrSpecularGlossiness
- **THEN** 导入指出对应字段或相斥扩展，现有固定版本继续可用

## MODIFIED Requirements

### Requirement: Report extension support and valid core fallback

模型 SHALL 公布足以定位的支持报告。在全文件声明中，任何当前未支持的 `extensionsRequired` 成员 SHALL 拒绝。当前 CPU 支持 `KHR_lights_punctual`、`KHR_materials_unlit`、`KHR_materials_emissive_strength`、`KHR_texture_transform`、`KHR_materials_ior`、`KHR_materials_specular` 和 `KHR_materials_clearcoat` 在本规格中的标准描述。其它 optional 扩展有完整可用的核心回退时 SHALL 沿该回退导入并报告降级，缺少选定场景必需的可用核心数据时 SHALL 报告不支持。

#### Scenario: Reject a required extension outside the current profile
- **WHEN** 文件声明当前未支持的 required 扩展，即使存在核心字段或它只关联另一个 scene
- **THEN** 拒绝加载并列出该扩展，模型未发布

#### Scenario: Allow only an available optional fallback
- **WHEN** 未支持的 optional 扩展出现于带有效核心回退的输入，或出现在没有可用核心数据的活动内容
- **THEN** 前者发布核心结果并报告扩展降级，后者给出定位明确的未支持结果

#### Scenario: Report required IOR and specular as CPU capabilities
- **WHEN** 合法文件在 extensionsUsed 和 extensionsRequired 中声明 IOR 与 specular，所选静态内容通过字段与引用校验
- **THEN** 模型可发布对应 CPU 材质数据，报告标明两个扩展的 CPU 支持，Renderer 和编辑器仍按实际帧的就绪结果报告受光状态

#### Scenario: Report required clearcoat after CPU validation
- **WHEN** 文件的 extensionsUsed 和 extensionsRequired 声明 clearcoat，且活动的标准材质通过字段、纹理及切线校验
- **THEN** 模型公布清漆的 CPU 支持。面板按实际覆盖模式、颜色准备和已接受帧报告受光状态
