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

### Requirement: Evaluate clearcoat over the intact base surface response

清漆表面 SHALL 由有效 `clearcoatFactor × clearcoatTexture.R` 取得层强度，由 `clearcoatRoughnessFactor × clearcoatRoughnessTexture.G` 取得上层感知粗糙度。粗糙度 SHALL 沿共享过滤与下限一次派生至各自的微表面参数。清漆法线 SHALL 使用自己的线性向量、scale、有效 UV 与手性，缺席时使用经过面向处理的未贴图几何法线。基底法线、粗糙度和 IOR/specular 继续单独按其槽求值。

清漆 SHALL 使用 IOR 1.5 对应的中性 F0=0.04、F90=1 和取自清漆法线的出射 Fresnel。有效层权重为强度乘该 Fresnel。直接光 SHALL 以该层权重加上可见清漆 GGX 镜面，以权重余量缩放完整的基底直接响应。基底和清漆使用各自的入射余弦，清漆镜面受实际几何及自身半球可见性约束。光源、衰减和阴影按同一几何输入消费。清漆权重为零时 SHALL 交付对应未覆层基底的受光响应。

受光输出中的基底自发光 SHALL 使用相同的出射透过率，并在共享工作色域施加一次预曝光。清漆层 SHALL 以原有核心或 IOR/specular 基底为输入，两个 IOR 的 Fresnel 参数分别由所属层确定。生效的 sheen SHALL 先与该基底合成，再随完整下层受清漆透过率调制。

#### Scenario: Distinguish base and coat normals at the geometric horizon
- **WHEN** 同一可见表面以非对称方式扰动基底和清漆法线，包含缺席清漆法线的对照以及光线越过几何半球的样点
- **THEN** 基底贴图只改变基底框架，独立清漆的高光随清漆框架，缺席清漆贴图时其方向来自未扰动几何法线。超出清漆可见半球的方向交付有限结果且无清漆镜面贡献

#### Scenario: Composite direct lighting with independent base and coat inputs
- **WHEN** 核心或 IOR/specular 基底使用同一灯光和几何，分别以零清漆、有效清漆及改变基底 IOR 或 specular 的组合渲染
- **THEN** 零强度响应与对应基底相符，有效清漆按层权重衰减完整基底并交付单独的镜面。改变基底 IOR 或 specular 时，清漆的固定 Fresnel 保持一致

#### Scenario: Attenuate the emissive base at the coat view angle
- **WHEN** 无灯无环境的可见自发光表面在正视及斜视下分别使用零清漆和有效清漆，并改变一次曝光
- **THEN** 工作色中的发射按相应清漆出射透过率变化，所选角度的亮度比符合清漆 Fresnel，曝光只缩放一次已合成的发射

### Requirement: Evaluate the normalized shared sheen layer over the base response

标准 sheen 的有效颜色 SHALL 使用线性 RGB，感知粗糙度 SHALL 由对应因子与线性数据通道取得，并沿共享几何过滤与下限推导独立的 sheen 微表面参数。sheen SHALL 用面向一致且处理过法线贴图的基底法线，通过 Charlie NDF 和 Neubelt visibility 计算波瓣。逐灯贡献 SHALL 受几何和着色半球约束并使用同一光源衰减和阴影语义。

设 `S` 为 sheen 线性颜色，`Eraw` 为实际视向和过滤后粗糙度的匹配 Charlie/Neubelt 方向反照率。直接 sheen SHALL 按 `1/max(1,Eraw)` 归一，间接 sheen SHALL 使用 `min(Eraw,1)`。直接及间接基底受光 SHALL 使用同一 NoV-only 权重 `1−max(S)×min(Eraw,1)`。有效颜色为零时 SHALL 交付对应下层的受光。基底含 IOR/specular 时 SHALL 保有其介质与金属含义。

清漆 SHALL 在 sheen 与基底受光合成后按原有清漆出射权重处理完整下层。发射 SHALL 遵循其原有的清漆透过率与预曝光合同。各层光照权重 SHALL 在工作色限幅前生效。

#### Scenario: Shade the finite grazing lobe with the correct angular measure

- **WHEN** 同一布料标样采用不同的微表面粗糙度，包括触发共享下限的输入，并比较正视、掠射和越出几何半球的灯光方向
- **THEN** 正向样点符合配套 Charlie/Neubelt 方向反照率的受限响应，贡献有界且有限。越出几何半球的灯光方向交付零 sheen 贡献，源粗糙度保有原值

#### Scenario: Composite a sheen layer between the base lighting and clearcoat

- **WHEN** 同一 Directional 或有限局部灯使用零 sheen、彩色 sheen、含 IOR/specular 的基底和位于绒光上的清漆，另有仅发射的对照
- **THEN** 零 sheen 恢复对应基底。有效 sheen 先改变基底受光预算并交付镜面绒光，然后被清漆透过率衰减。仅发射的输出按原有清漆权重与一次预曝光交付

### Requirement: Resolve the anisotropic axes in the effective surface frame

方向槽 SHALL 用自己的有效 UV、采样器、仿射变换及梯度进行线性采样。RG SHALL 从 [0,1] 映射为 [-1,1] 的二维方向并归一，B SHALL 乘材质强度得到有效强度 `s`。缺席方向槽或采样方向退化时，旋转前的方向取 +T。缺席方向槽的强度乘数为 1。采样取得的方向 SHALL 在 Primitive 的切线空间应用正向 `anisotropyRotation`，贴图变换作用于采样坐标。方向槽与基底 normal 采用不同有效 UV 时，二者 SHALL 按各自坐标取样并使用同一基础切线空间解释向量。

有效方向 SHALL 由已校验的原始 T、`cross(N,T) × tangent.w` 映射，经实例仿射的线性部分进入世界空间，再相对带 normal map 和面向的着色法线正交化。垂直粗糙度轴 SHALL 在同一半球框架内重建。基底 normal 的纹理变换、实例 determinant 及背面处理 SHALL 与既有几何合同一致。插值或法线投影使各向异性轴不可定义的像素 SHALL 使用有限的等向基底响应。

#### Scenario: Distinguish image addressing from material direction rotation

- **WHEN** 基底 normal 使用 UV0，方向图以变换 UV1 采样不同的 RG/B 纹素，并改变一次材质旋转
- **THEN** 方向图读到预计的线性通道，+T 或已编码方向按正弧度朝 +B 转动。纹理变换与材质旋转分别反映在采样位置和高光轴

#### Scenario: Follow mirrored corners and non-uniform instance axes

- **WHEN** 带前后可见标记的拉丝标样使用斜向 RG 或材质角，在独立改变 UV 手性、负 determinant 和非均匀缩放的实例上取样，并用非平坦的基底 normal 对照
- **THEN** 正、背面高光轴与各自几何和着色法线的世界方向相符，镜像接缝和实例变换按各自的手性起作用，面向只作用一次

### Requirement: Evaluate the anisotropic GGX base response

各向异性镜面 SHALL 在共享感知粗糙度过滤和下限之后，从 `alpha=p²`、`alphaT=mix(alpha,1,s²)`、`alphaB=alpha` 得到两轴参数。方向、垂直轴和同一个着色法线的投影 SHALL 输入各向异性 GGX NDF 与高度相关 Smith visibility。visibility 含微表面分母，光照外层乘一次入射余弦。Fresnel、漫反射、金属混合、发射、光衰减及阴影继续按对应核心或 IOR/specular 基底合同求值。

`s=0` 或两轴粗糙度相等时，镜面 SHALL 收敛到对应的等向响应。粗糙度下限、掠射与退化半角 SHALL 交付有限结果，并沿用共享半球判断。

#### Scenario: Compare distinguishable anisotropic light samples and isotropic limits

- **WHEN** 固定同一几何、灯光和 F0/F90，比较方向强度中间值、旋转四分之一圈及零强度，并在最小与最大粗糙度取代表性正向和掠射样点
- **THEN** 两主轴光瓣与采用标准双轴粗糙度的独立数值参考一致，旋转交换主轴，零强度和等轴边界恢复等向结果，合法边界输出有限
