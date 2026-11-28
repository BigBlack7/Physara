# Spec Delta

## ADDED Requirements

### Requirement: Prepare a matched sheen environment generation for live color demand

生效的 sheen 受光 SHALL 使用匹配的方向反照率 LUT。Direct 和关闭环境照明的 Physical Lighting SHALL 只为各自的实际颜色角色消费方向表。场景声明环境照明并交付 Physical sheen 受光时 SHALL 使用同一匹配源的基底环境与 Charlie/Neubelt 预滤波反射产品。天空、独立表面与无 sheen 的颜色只准备其实际用途。产品身份 SHALL 包含源内容及匹配的烘焙 profile，朝向、强度和曝光 SHALL 按当前 View 查询。

只有所需的源版本、预滤波产物和本轮实际采样范围均完整时，正常受光帧才 SHALL 确认对应环境和材质身份。替换、取消、拒绝或部分上传期间 SHALL 沿 View 完整交付合同保留可读的旧帧与可定位状态。

#### Scenario: Request only the environment products used by each output

- **WHEN** 同一固定环境在天空、仅独立表面、Direct sheen、无环境照明的 Physical sheen 及完全照明的 Physical sheen 中作为可选输入
- **THEN** 受光 sheen 颜色使用匹配的方向表。环境开启时，Physical sheen 进一步实际读取配套预滤波环境。无 sheen 颜色需求的输出由原有产品交付。改变查询朝向、强度或曝光继续复用同一烘焙产品

#### Scenario: Keep the accepted frame through an interrupted sheen environment replacement

- **WHEN** 有效 sheen 帧 A 已接受，随后请求与新源匹配的环境 B，其中 Charlie 产物或上传尚未完成或被拒绝，之后提供完整 B
- **THEN** 等待或拒绝阶段保有已接受 A 与实际欠缺原因，B 的基底与 sheen 产品在全部匹配后的正常受光提交中一并确认

### Requirement: Evaluate the bounded sheen response with matched IBL

Physical sheen 的间接项 SHALL 用与其过滤后粗糙度匹配的 Charlie/Neubelt 环境预滤波，方向来自经环境逆旋转的基底反射，幅度来自同一受限方向反照率预算。基底间接受光 SHALL 先按 sheen 透过权重衰减，额外 sheen 环境项 SHALL 使用自己的间接可见性。清漆进一步衰减包含 sheen 的下层响应。环境标定、强度与预曝光 SHALL 各施加一次，并按现有工作色合同交付。

#### Scenario: Match directional energy in a uniform environment

- **WHEN** 无灯单位环境照射受控布料样点，含零 sheen、两组不同视角或粗糙度以及原始方向反照率超过一的点
- **THEN** 零值恢复对应基底，其余样点按匹配方向预算归一，在登记的积分与量化误差内交付有限的基底和绒光合成

#### Scenario: Distinguish the sheen environment response from the base reflection

- **WHEN** 用可解析的非对称环境照明同一材质，改变环境朝向与曝光，并对照一份仅 DFG 和灯光的 Physical 结果
- **THEN** 环境 sheen 与基底的方向和频率响应符合各自过滤 profile，工作格式可表示的光照随一次曝光变化同比缩放。仅 DFG 结果保有匹配的直接光
