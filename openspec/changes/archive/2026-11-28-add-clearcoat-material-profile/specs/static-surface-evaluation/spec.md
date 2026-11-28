# Spec Delta

## ADDED Requirements

### Requirement: Evaluate clearcoat over the intact base surface response

清漆表面 SHALL 由有效 `clearcoatFactor × clearcoatTexture.R` 取得层强度，由 `clearcoatRoughnessFactor × clearcoatRoughnessTexture.G` 取得上层感知粗糙度。粗糙度 SHALL 沿共享过滤与下限一次派生至各自的微表面参数。清漆法线 SHALL 使用自己的线性向量、scale、有效 UV 与手性，缺席时使用经过面向处理的未贴图几何法线。基底法线、粗糙度和 IOR/specular 继续单独按其槽求值。

清漆 SHALL 使用 IOR 1.5 对应的中性 F0=0.04、F90=1 和取自清漆法线的出射 Fresnel。有效层权重为强度乘该 Fresnel。直接光 SHALL 以该层权重加上可见清漆 GGX 镜面，以权重余量缩放完整的基底直接响应。基底和清漆使用各自的入射余弦，清漆镜面受实际几何及自身半球可见性约束。光源、衰减和阴影按同一几何输入消费。清漆权重为零时 SHALL 交付对应未覆层基底的受光响应。

受光输出中的基底自发光 SHALL 使用相同的出射透过率，并在共享工作色域施加一次预曝光。清漆层 SHALL 以基底原有的核心响应或 IOR/specular 响应为输入，两个 IOR 的 Fresnel 参数分别由所属层确定。

#### Scenario: Distinguish base and coat normals at the geometric horizon
- **WHEN** 同一可见表面以非对称方式扰动基底和清漆法线，包含缺席清漆法线的对照以及光线越过几何半球的样点
- **THEN** 基底贴图只改变基底框架，独立清漆的高光随清漆框架，缺席清漆贴图时其方向来自未扰动几何法线。超出清漆可见半球的方向交付有限结果且无清漆镜面贡献

#### Scenario: Composite direct lighting with independent base and coat inputs
- **WHEN** 核心或 IOR/specular 基底使用同一灯光和几何，分别以零清漆、有效清漆及改变基底 IOR 或 specular 的组合渲染
- **THEN** 零强度响应与对应基底相符，有效清漆按层权重衰减完整基底并交付单独的镜面。改变基底 IOR 或 specular 时，清漆的固定 Fresnel 保持一致

#### Scenario: Attenuate the emissive base at the coat view angle
- **WHEN** 无灯无环境的可见自发光表面在正视及斜视下分别使用零清漆和有效清漆，并改变一次曝光
- **THEN** 工作色中的发射按相应清漆出射透过率变化，所选角度的亮度比符合清漆 Fresnel，曝光只缩放一次已合成的发射
