# Spec Delta

## ADDED Requirements

### Requirement: Deliver the matched physical clearcoat response

Physical Lighting SHALL 按清漆法线的视角和其过滤后感知粗糙度取得匹配的 GGX 方向反照率和镜面补偿。清漆环境反射 SHALL 使用对应清漆法线的反射方向和对应粗糙度的预滤波环境响应。两层 SHALL 消费同一已准备的环境代、朝向和标定。清漆的出射 Fresnel 权重作用一次，完整基底间接光以同一出射透过率衰减后与清漆贡献合成。

Physical Lighting 的直接清漆镜面和环境清漆镜面 SHALL 各使用一次对应的镜面补偿。场景省略环境时，Physical Lighting SHALL 交付含匹配 DFG 补偿的直接光和经过清漆的发射。间接项 SHALL 使用对应波瓣的 AO 语义，并与直接光及发射沿现有预曝光合同合成。

#### Scenario: Compare coated directional reflectance in a unit environment
- **WHEN** 白炉中同一受控表面分别使用零清漆和非零清漆，含两种可区分的清漆粗糙度或角度，以及一个有独立基底响应的样点
- **THEN** 零强度恢复对应基底，清漆与基底的合计响应在登记的积分及量化误差内符合选定的能量近似，清漆反射随其视角与粗糙度改变

#### Scenario: Resolve coat direction and DFG with and without an environment
- **WHEN** 同一可见清漆用不同的基底和清漆法线观察有方向性的 HDR 环境，随后在相同 Physical Lighting 配置下省略环境
- **THEN** 清漆层的反射和模糊度跟随其法线和粗糙度，且与基底共享实际环境标定。无环境时仍交付带对应 DFG 补偿的直接光和分层后的自发光
