# static-surface-evaluation Specification

## Purpose

规定静态 glTF Primitive 在可见帧中共享的几何变换、切线框架、纹理字段与像素覆盖规则，供 Forward 深度和颜色等消费者一致求值。

## Requirements

### Requirement: Transform static geometry and its surface frame consistently

静态表面 SHALL 使用本轮渲染相机及实例的完整仿射变换。相同几何在各 Pass 产生一致的裁剪空间位置。法线 SHALL 经逆转置并规范化，切线 SHALL 在变换后相对法线重新正交化，bitangent SHALL 保留源 `tangent.w` 与实例及所用 UV 朝向。负 determinant SHALL 与有效正面判定配套。单面绘制剔除背面，双面绘制的背面 SHALL 翻转最终法线一次。不可构造可用变换或切线框架时 SHALL 给出对应 Primitive 和原因。

#### Scenario: Evaluate an asymmetric normal map on a mirrored double-sided instance

- **WHEN** 一份带倾斜法线、已知 tangent handedness 和非对称 normal map 的几何同时以常规和负非均匀缩放出现，并从正反侧观察
- **THEN** 有效正面与源绕序吻合，法线符合仿射和切线约定，单面背侧被剔除，双面背侧使用翻转后的法线

### Requirement: Evaluate the selected UV set and texture transform

活动纹理槽 SHALL 使用有效 UV0/UV1，覆盖后的 `texCoord` 优先。变换 SHALL 按 scale、rotation、offset 应用，并维持统一纹理逻辑行序。normal 贴图所用框架 SHALL 与有效 UV 集及纹理变换的旋转、镜像方向一致。有匹配切线时保留其源 handedness；需要新框架时使用有效 UV 的角点结果并保留接缝。没有所需 UV 或无法取得可用切线框架时 SHALL 报告对应槽。

#### Scenario: Select a different UV set using a transformed texture slot

- **WHEN** UV0 与 UV1 指向不同颜色块，槽指定带 `texCoord` 的 `KHR_texture_transform`，并提供能区分 scale、rotation 和 offset 顺序的参数
- **THEN** 基色及覆盖读取有效集合中的变换后位置，normal 槽的旋转与镜像反映在表面框架中，图像原点保持一致

#### Scenario: Use a normal override with a different tangent UV

- **WHEN** 有效实例覆盖选用与源切线不同的合法 UV 集，且新集合含需要保留的手性接缝
- **THEN** 此实例使用与新 UV 匹配的框架及接缝，源模型和另一实例仍使用各自原有框架；若所需有效框架不可构造，预览说明对应的 normal 槽原因

### Requirement: Evaluate glTF color and data channels in their declared domains

基色 SHALL 为解码的 baseColor texture、线性因子与线性顶点颜色的乘积。roughness、metallic SHALL 分别使用纹理 G/B 与各自因子，AO SHALL 以纹理 R 和 occlusionStrength 从缺省 1 插值。normal SHALL 在线性域解码，normalScale 乘 XY 后规范化并应用表面框架。自发光 SHALL 采用解码后的 emissive texture、线性因子与 emissiveStrength，不生成光源。缺席纹理按标准默认值处理。`KHR_materials_unlit` 的颜色 SHALL 仅来自标准基色，覆盖与双面仍适用。

#### Scenario: Observe channel products with deliberately different values

- **WHEN** 使用具有不同各通道值的 RGBA 纹理、非白顶点颜色和非单位因子，自发光强度大于 1
- **THEN** 线性预览的基色、AO、roughness、metallic、normal 和自发光遵循各自字段与默认值，自发光超 1 的值在适用预览附件范围内仍可观察

#### Scenario: Display unlit color independent of scene lighting and other maps

- **WHEN** Unlit Primitive 带有基色、顶点色和有效 alpha，同时文档的灯或其它非覆盖材质值变化
- **THEN** 已覆盖像素保持标准基色，灯、normal、ORM 和 emissive 均不参与其颜色

### Requirement: Use matching opaque and masked coverage in depth and color

OPAQUE SHALL 按几何及面剔除规则参与颜色与深度，MASK SHALL 额外以基色因子 alpha、顶点 alpha 及基色纹理 alpha 的乘积比较同一 alphaCutoff，严格小于阈值时舍弃。当前直接绘制或早期深度组合 SHALL 使用相同采样、抛弃和光栅覆盖，保持遮挡结果一致。透明 `BLEND` SHALL 保持明确的未支持路由及可见原因。

#### Scenario: See a rear primitive through the same mask holes in both pass arrangements

- **WHEN** 一个包含阈值下方和恰在阈值上的 alpha 样本的前方 MASK Primitive 遮挡后方 OPAQUE Primitive
- **THEN** 只有阈值下方样本露出后方颜色及深度，等于阈值的样本保留前方结果，合并绘制与独立深度加颜色得出相同可见表面
