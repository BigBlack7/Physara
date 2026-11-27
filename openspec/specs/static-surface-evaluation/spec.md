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

### Requirement: Derive one core metallic-roughness surface response

核心静态表面 SHALL 从有效的线性基色、金属度及感知粗糙度派生共享着色参数。漫反射色为 `baseColor × (1−metallic)`，`F0 = mix(0.04, baseColor, metallic)`，核心 `F90 = 1`。连续因子变化 SHALL 通过材质数据生效，同一绘制布局 SHALL 使用同一份求值和着色程序。材质通道预览中的 G 保留贴图与因子的感知粗糙度乘积。

#### Scenario: Use dielectric, metallic and mixed values from one material layout

- **WHEN** 同一布局对同一非白基色分别使用金属度 0、1 和一个中间值
- **THEN** 介质的 F0 为 0.04 且漫反射包含完整基色，金属的 F0 等于基色且漫反射为零，中间值按线性金属度混合，各次求值使用同一程序

### Requirement: Filter geometric roughness once for core shading

核心粗糙度 SHALL 基于单位几何法线的屏幕导数进行过滤，在选定精度下将过滤后的感知粗糙度限制在正下限至 1，再平方得到微表面粗糙度。光栅片元的导数 SHALL 在逐片元覆盖分歧前取得。着色法线 SHALL 使用已声明的静态切线与双面合同。过滤所得参数 SHALL 供后续求值直接消费一次。

#### Scenario: Broaden a highlight from geometric normal variation

- **WHEN** 两个样点有相同的输入粗糙度，其中一个位于平坦几何，另一个的几何法线有已知屏幕变化量，且样点的输入粗糙度足以观察过滤差异
- **THEN** 有法线变化的样点得到可由所选过滤模型计算的更大微表面粗糙度，材质通道预览仍显示原感知粗糙度，MASK 的可见轮廓保持覆盖合同

### Requirement: Evaluate the shared core BRDF in a defined measure

核心镜面响应 SHALL 使用 GGX NDF、完整的 height-correlated Smith visibility 和 Schlick Fresnel，漫反射 SHALL 使用 Lambert。响应以单位立体角的 BRDF 值交付，Smith visibility 含微表面分母，入射余弦由外层光照或半球积分乘一次。入射方向在着色表面半球外时贡献为零。支持的粗糙度下限、掠射方向和未定义半角 SHALL 产生有限结果。

#### Scenario: Resolve roughness and angular limits

- **WHEN** 核心表面输入粗糙度为 0 或 1，且使用有效法线，分别在正入射、掠射、表面下方和半角退化的方向上求值
- **THEN** 结果分别符合有下限的 GGX 响应、共享掠射约定和零半球外贡献，退化半角的镜面贡献为零，各分量有限

### Requirement: Apply the core DFG encoding and energy response

对于与样点的视角和过滤后感知粗糙度匹配的有效 Filament DFG 值，核心模型 SHALL 以 `R=B`、`G=A+B` 解释 split-sum 系数，取 `E=mix(R,G,F0)`。镜面多散射近似 SHALL 使用 `C=1+F0×(1/G−1)`，间接漫反射预算 SHALL 使用 `1−E`。共享求值 SHALL 交付明确的单次散射和对应能量项，补偿的镜面响应使用一次 C。

#### Scenario: Interpret distinct directional reflectance channels

- **WHEN** 一个核心表面提供来自同一模型的有效 DFG 样点，R 和 G 不同且 F0 的至少一个颜色分量在 0 与 1 之间
- **THEN** 方向反照率、镜面补偿和间接漫反射预算分别符合该 R/G 编码与 F0，各颜色分量可独立对照 CPU 参考

#### Scenario: Integrate a perfectly reflective rough metal in a white furnace

- **WHEN** 理想白色金属在单位均匀半球入射辐亮度下，使用匹配视角和粗糙度的有效 DFG 样点求值
- **THEN** 漫反射为零，单次散射方向反照率符合独立参考积分，补偿后积分在约定容差内接近 1

### Requirement: Derive the shared opaque IOR and specular response

IOR/specular 表面 SHALL 从同一有效材质和几何求得基础波瓣。介质强度为 `specularFactor × specularTexture.A`，线性介质颜色为 `specularColorFactor × decodeSrgb(specularColorTexture.RGB)`，缺席纹理的乘数为 1。普通 IOR 的基准反射率为 `((ior−1)/(ior+1))²`。兼容 IOR=0 的基准 Fresnel 恒为 1，再应用 specular 强度和颜色。介质 F0 为基准反射率乘颜色、逐通道限制上界为 1 后乘强度，F90 为强度。金属波瓣的 F0 来自有效 baseColor，F90 为 1。

直接光 SHALL 用介质 Fresnel 最大颜色分量的补数缩放介质 Lambert 项，并按有效 metallic 混合完整的介质及金属响应。金属镜面贡献独立于介质 IOR、specular 强度与颜色。法线、粗糙度过滤、微表面量纲、几何覆盖和预曝光沿各自的共享合同。数值调节 SHALL 作用于同一已启用的材质 profile。

#### Scenario: Resolve dielectric endpoints and the metal boundary
- **WHEN** 相同几何受固定直接光照射，分别使用缺省扩展值、不同 IOR、有色 specular、零强度及 IOR=0，且有纯介质、纯金属和一个中间金属度的对照
- **THEN** 可辨别的正入射与掠射样点符合标准端点和介质能量分配，零强度给出介质漫反射，兼容值产生指定的角度响应。纯金属输出与介质参数无关，中间金属度按两完整波瓣的比例响应
