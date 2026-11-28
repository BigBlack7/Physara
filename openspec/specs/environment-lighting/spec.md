# environment-lighting Specification

## Purpose

规定静态场景如何把线性环境辐亮度生成可复用的天空、漫反射和镜面照明资源，并以匹配同一核心 BRDF 的 DFG 交付可验证的预曝光 HDR。定义环境资源身份、独立用途以及候选环境切换的可观察结果。

## Requirements

### Requirement: Prepare versioned environment products for the declared roles

系统 SHALL 按实际声明并被视图需要的用途交付环境产品。天空使用未作粗糙度卷积的 HDR 基础 cube，照明使用带 GGX 卷积的完整可采样 mip 区间和 SH9。SH9 SHALL 交付余弦卷积并含 Lambert 的 `1/π`，常量入射辐亮度在漫反射输入处保持原量级。可采样层级 SHALL 与过滤模型及粗糙度到 mip 的映射配套。环境烘焙身份 SHALL 包含实际源字节哈希、烘焙设置和算法及工具版本。相同身份可复用，源内容或烘焙身份改变时 SHALL 重新取得匹配产品。朝向、环境强度和相机曝光属于查询参数。

#### Scenario: Prepare one declared environment role

- **WHEN** 文档仅声明天空用途并请求物理视口
- **THEN** 视口可显示有效天空，间接照明保持关闭，当前需求只确认天空产品

#### Scenario: Reuse a bake when only the query changes

- **WHEN** 同一线性 EXR 的源哈希和烘焙 profile 相同，多个请求使用不同旋转、强度或曝光，并随后更换实际源字节
- **THEN** 前一组请求共享烘焙产品，查询所得方向和亮度随参数变化。更换源字节后，当前候选须取得相应新产品

### Requirement: Deliver the matched Filament DFG for core shading

核心受光表面 SHALL 共享固定 profile 的线性 DFG，按同一个 `NoV` 和过滤后感知粗糙度查表。DFG SHALL 按 Filament 编码交付 R 等于 split-sum 系数 B，G 等于 A+B，蓝通道保存 cloth 项。核心表面使用 R/G，实现单次散射方向反照率 `E = mix(R,G,F0)`、镜面补偿 `C = 1 + F0×(1/G−1)` 及间接漫反射预算 `1−E`。采样可用范围 SHALL 有正有限的 G 和有限的 R/G。直接镜面与环境镜面 SHALL 各使用一次 C。

#### Scenario: Use the production DFG in direct lighting and a white furnace

- **WHEN** 白色金属以已知粗糙度和视角分别接收固定直接光和常量单位环境，另取一个 R/G 有区别的非对称混合材质样点
- **THEN** 实际查表的方向反照率与独立参考吻合，直接镜面响应使用对应补偿，白色金属的间接镜面在约定误差内闭合，混合样点符合共同通道解释

### Requirement: Evaluate the configured environment in the current working color domain

环境照明 SHALL 用同一环境逆旋转变换 SH9 的着色法线与镜面查询方向，并按匹配烘焙 profile 的过滤后感知粗糙度选择有效 mip。间接漫反射 SHALL 使用共享漫反射色和 `1−E`，镜面 SHALL 使用预滤波环境及 `E×C`。缺席的材质或屏幕遮蔽 SHALL 提供单位可见性，材质 AO SHALL 只调制间接照明。颜色合成 SHALL 保留直接光和自发光的各自贡献。间接光和天空在求值得到的线性 HDR 中各应用一次源标定倍率、环境强度和当帧预曝光。

#### Scenario: Rotate and scale an asymmetric environment

- **WHEN** 固定物理表面和相机位置，依次改变非对称环境的朝向、强度及 View 曝光补偿
- **THEN** 漫反射、镜面和天空方向符合该朝向，只有环境分量随环境强度成比例变化，物理光照各分量在工作格式可表示范围内随 +1 EV 翻倍

#### Scenario: Restore unit visibility when occlusion inputs are absent

- **WHEN** 固定灯、材质和环境，仅切换带已知非单位遮蔽值的材质 AO 和缺席 AO 槽
- **THEN** 缺席槽对应单位可见性，间接光符合各自的遮蔽输入，直接光和自发光在两次读取中一致

### Requirement: Use a complete environment generation for each accepted frame

当前帧 SHALL 只消费所需用途与请求身份匹配且内容完整的一代环境。首次使用或版本改变时，实际采样的面及 mip SHALL 由图追溯到完整上传和正确的范围。拒绝、取消、过期或部分上传的候选 SHALL 失去当前帧确认资格。已接受资源由该帧及提交生命周期保活，替换使用独立候选。只有所有本次实际资源消费均成功且正常帧获得可靠提交，系统才 SHALL 确认该帧环境。暂不可用及拒绝 SHALL 保留旧已接受输出资格及可定位原因。

#### Scenario: Accept a complete replacement after an interrupted upload

- **WHEN** 环境 A 已接受，环境 B 的准备或首次上传未能确认，随后又准备并提交了完整 B
- **THEN** 等待期间旧输出仍对应 A 且理由可见，提前完成的 B 子资源没有单独生效，匹配且被接受的正常帧一次确认 B

### Requirement: Evaluate the matched directional reflectance for IOR and specular

IOR/specular 的 Physical Lighting SHALL 按同一表面的 NoV 及过滤后感知粗糙度读取现有 Filament DFG。以 R=B、G=A+B 解出可变 F90 的单次散射反照率 `(G−R)×F0 + R×F90`。介质与金属 SHALL 分别使用各自的 Fresnel 端点及镜面补偿，介质强度参与其反射与漫反射预算。间接介质漫反射的预算 SHALL 由经过补偿的介质镜面方向反照率取最大分量后求补，完整间接响应按金属度混合。

镜面多散射近似在每项直接镜面和环境镜面响应中各应用一次。镜面环境查询及 SH9 漫反射使用同一环境代、朝向与标定。材质 AO 作用于间接照明，直接光、间接光与发射分别在共享工作域施加一次预曝光。没有环境照明请求的 Physical Lighting SHALL 交付有匹配 DFG 补偿的直接光。

#### Scenario: Apply the dielectric energy budget in the unit environment
- **WHEN** 受控表面在无直接灯的单位环境中使用彩色且非单位的 specular 强度，配合零强度和 IOR=0 全反射介质的对照，并取至少一个不同粗糙度或视角的可分辨样点
- **THEN** 镜面反照率与实际 F0、F90 和对应 DFG 一致，漫反射随介质预算变化。零强度只有漫反射，全反射介质保持单位辐亮度且漫反射为零

#### Scenario: Preserve independent environment and exposure inputs
- **WHEN** 同一 IOR/specular 表面依次接受仅直接光、加入非对称环境光和 +1 EV 的正常帧，含一个材质 AO 非单位的对照
- **THEN** 直接镜面使用对应物理补偿，环境方向及 AO 效果只影响匹配的间接分量。工作域可表示的光照分量随 +1 EV 翻倍

### Requirement: Deliver the matched physical clearcoat response

Physical Lighting SHALL 按清漆法线的视角和其过滤后感知粗糙度取得匹配的 GGX 方向反照率和镜面补偿。清漆环境反射 SHALL 使用对应清漆法线的反射方向和对应粗糙度的预滤波环境响应。两层 SHALL 消费同一已准备的环境代、朝向和标定。清漆的出射 Fresnel 权重作用一次，完整基底间接光以同一出射透过率衰减后与清漆贡献合成。

Physical Lighting 的直接清漆镜面和环境清漆镜面 SHALL 各使用一次对应的镜面补偿。场景省略环境时，Physical Lighting SHALL 交付含匹配 DFG 补偿的直接光和经过清漆的发射。间接项 SHALL 使用对应波瓣的 AO 语义，并与直接光及发射沿现有预曝光合同合成。

#### Scenario: Compare coated directional reflectance in a unit environment
- **WHEN** 白炉中同一受控表面分别使用零清漆和非零清漆，含两种可区分的清漆粗糙度或角度，以及一个有独立基底响应的样点
- **THEN** 零强度恢复对应基底，清漆与基底的合计响应在登记的积分及量化误差内符合选定的能量近似，清漆反射随其视角与粗糙度改变

#### Scenario: Resolve coat direction and DFG with and without an environment
- **WHEN** 同一可见清漆用不同的基底和清漆法线观察有方向性的 HDR 环境，随后在相同 Physical Lighting 配置下省略环境
- **THEN** 清漆层的反射和模糊度跟随其法线和粗糙度，且与基底共享实际环境标定。无环境时仍交付带对应 DFG 补偿的直接光和分层后的自发光
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
