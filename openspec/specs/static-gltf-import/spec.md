# static-gltf-import Specification

## Purpose

规定 Physara 静态模型导入支持的 glTF 2.0 / GLB 输入、校验和数据语义，使后续 Scene、材质和几何准备可消费完整、可固定版本、可定位失败的只读 CPU 模型记录。

## Requirements

### Requirement: Resolve glTF and GLB sources within the model resource root

模型导入 SHALL 读取项目资源根下 `Assets/Models` 中的 glTF 2.0 文档或 GLB 2 容器。容器和版本 SHALL 在消费其内容前校验。所选 scene 使用的 buffer 和图像 SHALL 支持合法 GLB BIN、图像 bufferView、标准 base64 data URI 和相对文件引用。外部 URI 从声明它的模型目录解析，经规范化和实际打开对象验证后 SHALL 位于允许的 `Assets/Models` 范围内。输入大小、声明范围、累计依赖和解码结果 SHALL 受预算约束。越界、缺失或不支持的输入 SHALL 返回可定位的有界原因。

#### Scenario: Read the same static content from external and embedded sources
- **WHEN** 等价的合法模型分别以相对文件、data URI 或 GLB BIN 和图像 bufferView 提供所需字节
- **THEN** 输入从模型自身的位置及容器解析，得到等价的已验证几何和图像内容

#### Scenario: Reject a disallowed external reference
- **WHEN** 所选模型的有效依赖指定绝对／远程 URI，或实际文件因路径规范化或重解析落在允许范围外
- **THEN** 导入失败，诊断指向相应 buffer 或图像引用，外部正文不进入解码

#### Scenario: Reject an invalid container or excessive dependency
- **WHEN** GLB 的有效块序列、长度或声明范围无法验证，或导入所需字节超过配置
- **THEN** 返回对应容器或预算错误，模型不获得有效记录

### Requirement: Select and validate the static scene being imported

导入 SHALL 支持显式选择合法源 scene，未提供时使用有效默认 scene，再用无默认时唯一已声明的 scene。无可选 scene 或多 scene 无默认时 SHALL 给出可操作原因。所选层级 SHALL 保留源根、节点身份、父子次序、局部 TRS 或可表达为 TRS 的矩阵、负缩放及源 Primitive 槽。非法引用、环、重复父级或无效局部变换 SHALL 拒绝。所选 scene 实际绑定 Skin 或具有 Morph target 的节点 SHALL 拒绝，即使当前权重为零。其它 scene 有合法但未使用的变形数据 SHALL 不阻止静态场景。存在动画时 SHALL 交付源节点初始变换并报告不播放动画。

#### Scenario: Retain the selected source scene and transforms
- **WHEN** 调用方指定有效 scene 或采用确定的默认／唯一 scene，且其层级含负缩放
- **THEN** 结果保留该 scene 的源编号、层级、局部变换及 mesh/Primitive 关联，源几何绕序不随实例变换改写

#### Scenario: Keep unused deformation outside the selected static scene
- **WHEN** 所选 scene 合法且静态，另一 scene 含合法的 Skin 或 Morph，文档还含动画曲线
- **THEN** 所选 scene 成功导入并保留初始变换，支持报告说明动画不播放

#### Scenario: Reject unsupported deformation or invalid hierarchy on the selected path
- **WHEN** 选择的节点实际绑定 Skin、使用带 Morph target 的 mesh，或其层级含非法引用、环或多父
- **THEN** 导入报告对应源节点与原因，未交付部分层级

### Requirement: Decode only validated geometry accessor ranges and values

导入 SHALL 按有效 bufferView 和 accessor offset、stride、元素类型及 count 取得数据。同一 bufferView 被所选几何的不同顶点 accessor 共用时，SHALL 声明 `byteStride`。同一源 accessor 可在多个 Primitive 中复用。计算范围、元数和字节大小 SHALL 在读取、分配或窄化前校验，读取不得越过声明及实际可用字节。对稀疏 accessor，基底缺席时使用标准零基底，校验索引类型、范围及严格递增顺序，叠加后保持元素和 normalized 语义。被消费的核心顶点属性须满足各自的类型、数量和数值约束，索引 SHALL 为合法无符号标量且小于顶点数。供下游使用的局部 bounds SHALL 对应实际解码几何。

#### Scenario: Combine interleaving, normalization, and sparse replacement
- **WHEN** 核心材质用到带 offset 和 stride 的 UV，其中整数 normalized 数据含合法 sparse 替换，另有有效的无基底 sparse 数据
- **THEN** 读取结果反映标准归一化、零初值和稀疏覆盖，未覆盖元素保持原值

#### Scenario: Reject invalid accessor spans or vertex indices
- **WHEN** accessor 的类型、对齐、stride 或范围不合法，包括不同顶点 accessor 共用未声明 `byteStride` 的 bufferView，或显式索引超过该 Primitive 的顶点范围
- **THEN** 模型失败并指出对应的 bufferView、accessor 或 Primitive，坏数据不进入拓扑转换

#### Scenario: Reject incorrectly ordered sparse indices
- **WHEN** sparse 索引块的字节范围合法，但包含重复、逆序或越界的索引
- **THEN** 在应用替换和发布前拒绝该 accessor

### Requirement: Deliver canonical triangles and a valid tangent frame when needed

静态 Primitive SHALL 支持 `TRIANGLES`、`TRIANGLE_STRIP` 和 `TRIANGLE_FAN`，交付保持源绕序的三角形索引。无索引时按源顶点顺序建立索引。退化面 SHALL 从有效索引移除，完全退化的 Primitive 保留源槽作空表面。点和线拓扑 SHALL 明确报告不支持。缺少 `NORMAL` 时 SHALL 从有面积的三角形生成逐面法线并忽略源 `TANGENT`。需法线纹理且缺少可用切线时 SHALL 由有效采样 UV 集生成正确的面角点切线。当共享顶点的角点切线或 handedness 不同，SHALL 拆分顶点并重建索引。源合法切线的 `w` SHALL 保留。实际使用的 UV 集缺失、超出 UV0/UV1 或不能形成所需切线空间时，SHALL 报告具体 Primitive 和纹理槽。

#### Scenario: Preserve triangle winding through topology conversion
- **WHEN** 合法的有索引三角带和无索引三角扇带有可区分正反面的顶点布局
- **THEN** 导入结果的三角形索引及绕序符合各自拓扑，对空面积面不生成伪法线

#### Scenario: Generate flat normals despite a source tangent
- **WHEN** 两个不共面的相邻三角形没有 NORMAL，但源提供 TANGENT
- **THEN** 每面输出独立法线，源切线不进入结果；需要法线贴图时由生成的法线重新取得切线框架

#### Scenario: Split shared corners with different tangent frames
- **WHEN** 同一源索引连接镜像 UV 侧的两个面，法线贴图使用 UV1 且缺少源切线
- **THEN** 相异角点有分离的顶点索引和正确 handedness，连续属性随拆分保留

#### Scenario: Report incompatible topology or a missing active UV set
- **WHEN** 所选 Primitive 使用点／线，或其有效纹理槽需要不存在的 UV 集
- **THEN** 返回对应的拓扑或纹理引用诊断，未用零 UV 或任意切线代替

### Requirement: Preserve standard material, image and sampler semantics

核心 metallic-roughness 材质 SHALL 保留标准因子、缺省、纹理引用、`alphaMode`／`alphaCutoff` 与 `doubleSided`。首版图像 SHALL 由每样本至多 8 位的标准 PNG 或 8 位 JPEG 交付 RGBA8，保留有效格式和级数。图像、纹理、sampler 和材质槽 SHALL 分别表达作用，允许同一只读图像供 sRGB 颜色和线性数据两种用途。baseColor/emissive 为 sRGB 颜色，normal/occlusion/metallic-roughness 为线性数据，粗糙度取 G、金属度取 B、遮蔽取 R。纹理 UV 集、变换和 sampler SHALL 保留于各自引用。实际使用的图像、MIME、纹理或 sampler 非法或不在支持范围时 SHALL 报告对应位置。

#### Scenario: Interpret the same model image with two texture uses
- **WHEN** 同一图像被 baseColor 和线性数据槽引用，并有不同 sampler 或 UV 设置
- **THEN** 两槽引用同版像素并保留各自用途、通道和 sampler，变换与 UV 集不被合并

#### Scenario: Retain material defaults and defined texture transforms
- **WHEN** 一个 Primitive 未指定材质，另一个使用核心材质和有效的 KHR_texture_transform
- **THEN** 前者具有标准缺省，后者保留其显式因子和 alpha/双面语义，按每槽的有效 UV 集和变换交付

### Requirement: Retain usable camera and punctual light descriptors

所选 scene 的 perspective 和 orthographic 相机 SHALL 保留 glTF 投影参数，区分缺省与显式值。透视 FOV、近远面与 aspectRatio、正交 magnification 和有限裁剪区间 SHALL 符合项目投影 profile。所选 `KHR_lights_punctual` 灯 SHALL 保留节点引用、方向／点／聚光类型、线性颜色、原标准强度单位、可选 range 和锥角。缺省 range 表示无距离截断，显式 range 和角度须合法。非法或超 profile 的参数 SHALL 返回相应诊断。

#### Scenario: Preserve projection optionals and light units
- **WHEN** 合法场景含未给远面的透视相机、有限正交相机及方向和局部灯
- **THEN** 记录保留可选值与实参，方向灯为 lux，点／聚光灯为 candela，未指定 range 保持无限范围语义

#### Scenario: Distinguish explicit invalid values from omission
- **WHEN** 透视 zfar 或局部灯 range 显式为零，或者正交 magnification 超出本项目 profile
- **THEN** 诊断对准相机或灯字段，显式值未被重新解释成缺省

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

### Requirement: Publish complete model records with dependency identity

模型 SHALL 沿通用 CPU 资产发布合同异步加载，和图片共用作业准入，并在 owner 边界交付完整不可变记录。记录 SHALL 包含所选源 scene、可用的标准数据与引用、支持报告和由实际参与导入的文件字节确定的稳定 `sourceVersion`。相同模型内容及依赖输入 SHALL 保持源版本；显式刷新后仅变更参与导入的外部 buffer 或图像也 SHALL 改变源版本并发布新内容代。取消、失败、源失效或过期资料 SHALL 保留最后有效版本，结果只表达 CPU 就绪。

对固定版本的源核对 SHALL 使用实际参与导入的主文件与外部依赖读证据，支持区分完全匹配、内容改变和无法读取。在文档边界对固定版本的核对 SHALL 保持该版本及其已发布资产状态有效，采用新候选须经独立的 Asset 发布和 Scene 采用合同。核对 SHALL 保持已有 `sourceVersion` 字节身份的解释。

#### Scenario: Commit a validated model through the shared CPU asset service
- **WHEN** 图片和模型使用同一作业额度，模型已在 worker 完成但 owner 尚未处理结果
- **THEN** 模型仍为 `Loading`；在通用发布检查后，才交付完整的 CPU 数据、身份和 sourceVersion

#### Scenario: Change only a resolved model dependency
- **WHEN** 同一 source 和 scene 先以完全相同输入显式刷新，随后只更改参与导入的外部 bin 或图像字节并再次刷新
- **THEN** 第一次保留内容版本，第二次产生新的源版本和完整记录，已固定的旧版本仍可读取

#### Scenario: Leave current content intact after a failed or stale replacement
- **WHEN** 一次替换失败、被取消或在更新的请求之后才交回
- **THEN** 无效结果不发布，诊断和最后有效模型按当前请求状态保留，释放未采纳数据

#### Scenario: Verify a pinned source against its actual dependency bytes
- **WHEN** 场景文档操作核对已固定且仍可读取的模型，期间依次观察相同输入和只改变一个参与导入的外部依赖
- **THEN** 前者报告匹配，后者报告依赖冲突，原模型数据、`sourceVersion` 和 AssetManager 的已发布内容仍可由已有读者取得

### Requirement: Import the standard IOR and specular material inputs

所选静态 scene 使用的 metallic-roughness 材质 SHALL 支持独立及同时使用 `KHR_materials_ior` 和 `KHR_materials_specular`。CPU 记录 SHALL 保留扩展声明、标准参数、有效纹理引用与采样语义。IOR 缺省为 1.5，接受有限可表示的 `ior >= 1` 及单独的兼容值 `0`。specularFactor 缺省为 1，合法范围为 [0,1]。specularColorFactor 缺省为线性 RGB (1,1,1)，接受有限可表示的非负分量，包括大于 1 的合法值。

specularTexture SHALL 使用线性 A 通道数据，specularColorTexture SHALL 使用 sRGB RGB。两个槽分别保留各自的 sampler、UV 集及 `KHR_texture_transform`。有效数据槽缺少图像 A 通道时 SHALL 得到 1。读取的外置扩展纹理属于模型依赖。已支持扩展的非法字段、不可用 UV 或不合法引用 SHALL 在发布前定位到材质及字段或 Primitive 槽。与 `KHR_materials_unlit` 或 `KHR_materials_pbrSpecularGlossiness` 同时出现在同一活动材质的 IOR/specular 扩展 SHALL 被拒绝。

#### Scenario: Publish the standard extension values and image dependencies
- **WHEN** 可达材质含 IOR、specular 的独立声明与联合声明，含缺省项、兼容值 0 和大于 1 的颜色因子，两个槽共用一份有非对称 RGBA 的图像但使用不同采样设置，随后只更换参与导入的外部图像字节
- **THEN** CPU 记录保留声明及合法初值，强度与颜色槽独立解释同一图像，替换图像产生新 sourceVersion，固定的旧记录仍可读

#### Scenario: Reject malformed or incompatible supported material extensions
- **WHEN** 选定材质使用介于 0 和 1 的非零 IOR、有效扩展槽缺少所需 UV，或组合了同材质的 Unlit 和 specular
- **THEN** 导入交付对应字段、槽或扩展冲突的错误，失败候选无法取得 CPU 就绪资格

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
### Requirement: Import the standard sheen material inputs

静态 glTF/GLB 中声明 `KHR_materials_sheen` 的受支持 lit metallic-roughness 材质 SHALL 保留有限的 `sheenColorFactor` 三个线性分量和 `sheenRoughnessFactor`，各分量范围均为 [0,1]。缺省颜色为零向量，缺省粗糙度为零。颜色槽 SHALL 以 sRGB 解码后的 RGB 乘颜色因子，粗糙度槽 SHALL 以线性 A 乘粗糙度因子。缺图槽各用单位乘数。两个槽 SHALL 各自保留采样器、有效 UV0/UV1 和适用的纹理变换。参与导入的图像和采样引用 SHALL 归属所发布的固定源记录。

同一材质的 sheen 声明与 unlit 或旧 specular-glossiness 同时存在时 SHALL 给出字段定位并拒绝该不兼容材质。通过校验的核心、IOR/specular 和 clearcoat 参数 SHALL 保持各自的标准含义。超范围、类型错误或活动几何无法使用的有效 UV SHALL 在模型发布前得到位置和原因。

#### Scenario: Resolve sheen defaults and both independent texture interpretations

- **WHEN** glTF 与 GLB 的对应材质含缺省值对照，以及同时引用有明显 RGB/A 差异的同源图像、两种采样用途和不同有效 UV 变换的 sheen 槽
- **THEN** 缺省 sheen 颜色和粗糙度为零，两个槽保留各自的有效 UV、采样设置和色彩域。替换实际依赖图像使新内容使用新源版本，已有固定引用仍对应它的源像素

#### Scenario: Reject invalid sheen inputs before model publication

- **WHEN** 所选材质的 sheen 颜色或粗糙度越界、槽声明引用缺席的有效 UV，或同一材质把 sheen 与 unlit 或旧 specular-glossiness 一起声明
- **THEN** 对应字段、纹理或排斥扩展获得明确诊断，未通过校验的材质候选失去模型发布资格

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
