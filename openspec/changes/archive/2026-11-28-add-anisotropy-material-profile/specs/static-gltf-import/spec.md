# Spec Delta

## MODIFIED Requirements

### Requirement: Report extension support and valid core fallback

模型 SHALL 公布足以定位的支持报告。在全文件声明中，任何当前未支持的 `extensionsRequired` 成员 SHALL 拒绝。当前 CPU 支持 `KHR_lights_punctual`、`KHR_materials_unlit`、`KHR_materials_emissive_strength`、`KHR_texture_transform`、`KHR_materials_ior`、`KHR_materials_specular`、`KHR_materials_clearcoat`、`KHR_materials_sheen` 和 `KHR_materials_anisotropy` 在本规格中的标准描述。其它 optional 扩展有完整可用的核心回退时 SHALL 沿该回退导入并报告降级，缺少选定场景必需的可用核心数据时 SHALL 报告不支持。对于所选静态场景，anisotropy 的 CPU 支持 SHALL 按实际通过校验的材质与组合报告。

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

## ADDED Requirements

### Requirement: Import standard anisotropy with a defined tangent space

所选静态场景的兼容 glTF/GLB 材质 SHALL 保留 `KHR_materials_anisotropy` 声明、[0,1] 的 `anisotropyStrength`、有限可表示的 `anisotropyRotation` 和可选的 `anisotropyTexture`。强度和旋转缺省均为 0。旋转以弧度保存，正角从切线朝副切线转动。纹理 SHALL 以线性 RGB 解释方向和强度，保留其 sampler、有效 UV0/UV1 与 `KHR_texture_transform`。已读取的外部图像 SHALL 纳入模型依赖身份。

通过本 profile 导入的 anisotropy Primitive SHALL 具有有效源 NORMAL/TANGENT，或由可用的基底 normal 槽建立切线空间。缺 NORMAL 时沿 glTF 逐面生成法线并忽略源切线，再按基底 normal 的有效 UV 生成可用的角点框架。存在镜像或相异角点方向时 SHALL 保留拆分和 handedness。方向纹理本身只提供材质方向及强度。字段格式非法、有效 UV 缺失或框架不可用时 SHALL 给出对应材质字段、Primitive 属性或基底 normal 槽的原因。与 unlit 或 pbrSpecularGlossiness 同材质声明 SHALL 被拒绝。

本颜色 profile SHALL 接受核心及 IOR/specular 基底。正强度 anisotropy 需要与现有清漆颜色家族或有效非零 sheen 一同求值时，若 anisotropy 在 `extensionsRequired` 中 SHALL 报告该材质和组合并拒绝候选。若 anisotropy 为 optional 且其它已支持响应仍可使用，SHALL 交付它们并报告该材质省略了 anisotropy。BLEND SHALL 沿现有透明支持边界报告。

#### Scenario: Import defaults and a linear direction map with its own effective UV

- **WHEN** 有可用源框架的可达材质分别使用缺省声明及一张线性各通道可区分、具有 sampler 和 UV 变换的方向图，另一个候选只更新参与导入的外部图像
- **THEN** 缺省为零强度和零旋转，另一材质保留字段、槽的数据域和有效采样设置。新图像获得新源版本，已有固定模型仍对应原来的图像

#### Scenario: Obtain tangent space from the source or the base normal map

- **WHEN** 使用同一各向异性材质的标样分别有合法源切线、仅有可生成有效镜像角点框架的基底 normal 槽，或仅有方向纹理而缺可用框架
- **THEN** 前两种取得各自的源或派生框架及手性，第三种报告实际缺失的条件。无 NORMAL 的输入沿逐面法线规则处理

#### Scenario: Diagnose invalid fields and limited material combinations

- **WHEN** 选定材质有越界强度、缺席的方向槽有效 UV 或标准排斥项，或正强度各向异性和清漆组合分别声明为 required 与 optional
- **THEN** 非法项及 required 组合被定位并拒绝。可用的 optional 组合报告省略 anisotropy 并保有有效清漆响应，CPU 支持报告反映实际导入的配置
