# Spec Delta

## MODIFIED Requirements

### Requirement: Resolve Hard and PCF from conventional depth comparisons

同一可见性查询 SHALL 在 `[0,1]` 内交付结果。Hard SHALL 比较唯一的近邻深度。PCF SHALL 对当前物理区域内以近邻 texel 为中心的固定 `3 × 3` 核逐样本比较，以等权重归一化比较结果。等深 SHALL 视为可见。核及层权限有效时 SHALL 仅比较同一投影的深度。任一不合法地址、无有效输入或超出名义接收范围的查询 SHALL 使用 `visibility=1` 且不读取相应纹理。Poisson 仅定义可能采用的采样分布，本基线的 PCF 使用固定网格。

接收偏移 SHALL 基于接收处的几何法线、受光方向和当前视图的世界 texel 尺度，并受所交付余量约束。Hard 与 PCF SHALL 使用相同覆盖判定、接收偏移及常规深度比较。主视图 Reverse-Z 与材质法线贴图 SHALL 不改变偏移方向或比较语义。

当 Deferred 核心着色通过屏幕表面取得接收位置时，接收法线 SHALL 由有效矩形内可用于局部几何重建的深度估计。缺少有效估计时，法线偏移 SHALL 为零，仍在恢复的接收位置执行共享阴影范围、滤波和深度比较。法线贴图继续只参与着色方向。

#### Scenario: Filter comparisons of a mixed depth footprint

- **WHEN** 接收比较深度为 0.5 的有效样点对应包含 4 个深度 0.25 和 5 个深度 0.75 的九点核，中心深度为 0.25
- **THEN** Hard 查询为 0，PCF 查询为 5/9。改变中心深度为 0.5 后，Hard 查询为 1。比较采用同一接收深度和各自样本，边界仍满足相同的层权限。

#### Scenario: Apply the same receiver offset for ordinary lighting and diagnostics

- **WHEN** 同一个受光表面以不同法线贴图取值观察同一阴影边界，且在两个实际世界 texel 尺度下检查偏移
- **THEN** 同一尺度下，诊断和逐灯查询的可见性一致，偏移与几何法线及光空间尺度对应。渲染范围外仍是中性值。已知有遮挡的非共面样点仍被遮挡。

#### Scenario: Keep shadow queries geometric on a screened surface

- **WHEN** Deferred 的斜面接收者在连续深度区域更换法线贴图，另有邻点不足以建立局部平面的可见边缘
- **THEN** 连续区域的查询偏移与几何估计及原 texel 尺度一致，法线贴图只改变材质响应。边缘处以零法线偏移继续得到实际接收位置的 Hard 或 PCF 可见性。
