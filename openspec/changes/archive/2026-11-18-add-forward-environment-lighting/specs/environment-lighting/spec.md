# Spec Delta

## Purpose

规定静态场景如何把线性环境辐亮度生成可复用的天空、漫反射和镜面照明资源，并以匹配同一核心 BRDF 的 DFG 交付可验证的预曝光 HDR。定义环境资源身份、独立用途以及候选环境切换的可观察结果。

## ADDED Requirements

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
