# Spec Delta

## ADDED Requirements

### Requirement: Import the standard sheen material inputs

静态 glTF/GLB 中声明 `KHR_materials_sheen` 的受支持 lit metallic-roughness 材质 SHALL 保留有限的 `sheenColorFactor` 三个线性分量和 `sheenRoughnessFactor`，各分量范围均为 [0,1]。缺省颜色为零向量，缺省粗糙度为零。颜色槽 SHALL 以 sRGB 解码后的 RGB 乘颜色因子，粗糙度槽 SHALL 以线性 A 乘粗糙度因子。缺图槽各用单位乘数。两个槽 SHALL 各自保留采样器、有效 UV0/UV1 和适用的纹理变换。参与导入的图像和采样引用 SHALL 归属所发布的固定源记录。

同一材质的 sheen 声明与 unlit 或旧 specular-glossiness 同时存在时 SHALL 给出字段定位并拒绝该不兼容材质。通过校验的核心、IOR/specular 和 clearcoat 参数 SHALL 保持各自的标准含义。超范围、类型错误或活动几何无法使用的有效 UV SHALL 在模型发布前得到位置和原因。

#### Scenario: Resolve sheen defaults and both independent texture interpretations

- **WHEN** glTF 与 GLB 的对应材质含缺省值对照，以及同时引用有明显 RGB/A 差异的同源图像、两种采样用途和不同有效 UV 变换的 sheen 槽
- **THEN** 缺省 sheen 颜色和粗糙度为零，两个槽保留各自的有效 UV、采样设置和色彩域。替换实际依赖图像使新内容使用新源版本，已有固定引用仍对应它的源像素

#### Scenario: Reject invalid sheen inputs before model publication

- **WHEN** 所选材质的 sheen 颜色或粗糙度越界、槽声明引用缺席的有效 UV，或同一材质把 sheen 与 unlit 或旧 specular-glossiness 一起声明
- **THEN** 对应字段、纹理或排斥扩展获得明确诊断，未通过校验的材质候选失去模型发布资格

## MODIFIED Requirements

### Requirement: Report extension support and valid core fallback

模型 SHALL 公布足以定位的支持报告。在全文件声明中，任何当前未支持的 `extensionsRequired` 成员 SHALL 拒绝。当前 CPU 支持 `KHR_lights_punctual`、`KHR_materials_unlit`、`KHR_materials_emissive_strength`、`KHR_texture_transform`、`KHR_materials_ior`、`KHR_materials_specular`、`KHR_materials_clearcoat` 和 `KHR_materials_sheen` 在本规格中的标准描述。其它 optional 扩展有完整可用的核心回退时 SHALL 沿该回退导入并报告降级，缺少选定场景必需的可用核心数据时 SHALL 报告不支持。

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

#### Scenario: Report validated sheen as a CPU capability

- **WHEN** 合法文件在 extensionsUsed 和 extensionsRequired 中声明 sheen，活动内容通过对应的材质和静态几何校验
- **THEN** CPU 发布对应 sheen 数据与支持报告。受光准备和已接受颜色状态由 Renderer 和 Editor 按实际交付另行确认
