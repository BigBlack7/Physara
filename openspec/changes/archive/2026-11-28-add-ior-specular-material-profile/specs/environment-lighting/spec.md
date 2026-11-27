# Spec Delta

## ADDED Requirements

### Requirement: Evaluate the matched directional reflectance for IOR and specular

IOR/specular 的 Physical Lighting SHALL 按同一表面的 NoV 及过滤后感知粗糙度读取现有 Filament DFG。以 R=B、G=A+B 解出可变 F90 的单次散射反照率 `(G−R)×F0 + R×F90`。介质与金属 SHALL 分别使用各自的 Fresnel 端点及镜面补偿，介质强度参与其反射与漫反射预算。间接介质漫反射的预算 SHALL 由经过补偿的介质镜面方向反照率取最大分量后求补，完整间接响应按金属度混合。

镜面多散射近似在每项直接镜面和环境镜面响应中各应用一次。镜面环境查询及 SH9 漫反射使用同一环境代、朝向与标定。材质 AO 作用于间接照明，直接光、间接光与发射分别在共享工作域施加一次预曝光。没有环境照明请求的 Physical Lighting SHALL 交付有匹配 DFG 补偿的直接光。

#### Scenario: Apply the dielectric energy budget in the unit environment
- **WHEN** 受控表面在无直接灯的单位环境中使用彩色且非单位的 specular 强度，配合零强度和 IOR=0 全反射介质的对照，并取至少一个不同粗糙度或视角的可分辨样点
- **THEN** 镜面反照率与实际 F0、F90 和对应 DFG 一致，漫反射随介质预算变化。零强度只有漫反射，全反射介质保持单位辐亮度且漫反射为零

#### Scenario: Preserve independent environment and exposure inputs
- **WHEN** 同一 IOR/specular 表面依次接受仅直接光、加入非对称环境光和 +1 EV 的正常帧，含一个材质 AO 非单位的对照
- **THEN** 直接镜面使用对应物理补偿，环境方向及 AO 效果只影响匹配的间接分量。工作域可表示的光照分量随 +1 EV 翻倍
