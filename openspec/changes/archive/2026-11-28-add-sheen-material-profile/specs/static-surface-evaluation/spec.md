# Spec Delta

## ADDED Requirements

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

## MODIFIED Requirements

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
