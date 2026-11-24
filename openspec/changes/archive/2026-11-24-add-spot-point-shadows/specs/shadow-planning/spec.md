# Spec Delta

## MODIFIED Requirements

### Requirement: Plan finite shadow views with conservative caster coverage

启用规划的有效 View SHALL 从当前冻结场景、稳定有限相机和保守可见灯集合构造阴影视图。主方向光 SHALL 在稳定相机近面至 `min(cullingFar, shadowDistance)` 的有限区间内形成所请求的 1–4 级。透视 SHALL 按请求权重混合线性和对数分割，正交 SHALL 线性分割并支持零近平面。有效端点 SHALL 有限且严格递增。空区间或端点无法按交付精度表示时 SHALL 返回可定位的无分配原因。

聚光 SHALL 根据有效灯位、发射轴和外锥交付一个有限透视视图。点光 SHALL 按 `+X, −X, +Y, −Y, +Z, −Z` 交付六个朝向明确的透视面，六面共用有限米制近远面。局部灯的有限阴影半径及投影远面 SHALL 取 `min(range, shadowDistance)`，无 `range` 时取 `shadowDistance`。投影近平面 SHALL 严格大于零且小于远面。深度生产和查询 SHALL 使用计划交付的同一组投影和独立的常规深度约定。局部阴影体与接收视锥完全分离、或单视图无法在 GPU 交付精度下表示时 SHALL 有可定位的无分配结果。光源 SHALL 按原有距离及锥角规则参与直接光。

各方向光级联 SHALL 以该级完整 receiver 切片的包围球定出固定光空间 XY 尺度，按有效内部分辨率的世界 texel 网格吸附中心，并保持可查询的 receiver 覆盖。内部切分两端声明的过渡带 SHALL 同时受到相邻两级的拟合和 caster 筛选覆盖。每个已分配视图 SHALL 提供相应的保守 caster 集，来自同一份冻结场景中可见、有 bounds 且允许投射阴影的 Primitive。可能沿光线到达接收区域的画外 Primitive 与相切候选 SHALL 保留，能确定与阴影视图覆盖分离的 Primitive SHALL 剔除。每级 Z 拟合 SHALL 覆盖接收段和相关 caster 的深度范围。点光的候选覆盖 SHALL 保留可能跨相邻面的投射物。

#### Scenario: Retain a caster outside the main camera

- **WHEN** 方向光照向稳定视锥内的接收表面，视锥外存在沿同一光线遮挡它的 caster，以及一个与接收范围完全分离的物体
- **THEN** 能覆盖接收表面的级联包含该画外 caster 与它所需的深度范围，主可见集仍由主相机决定，分离物和禁用投射的物体被剔除

#### Scenario: Keep local shadow coverage finite

- **WHEN** 无 range 的点光和有 range 的聚光影响当前 View，且请求的阴影距离为有限正值
- **THEN** 点光的六面具有同一有限近远面，聚光的单视图和保守候选范围由该灯外锥及有限远面确定，两灯继续按各自的源 range 参与完整的直接光集合

#### Scenario: Split finite perspective and orthographic coverage

- **WHEN** 同一 View 请求四级，先用近平面为 1 m、有效远界为 81 m 的透视相机和对数权重 0.5，再用近平面为 0、有效远界为 80 m 的正交相机
- **THEN** 两组名义端点分别为 `1, 12, 25, 44, 81` m 和 `0, 20, 40, 60, 80` m。请求单级时覆盖完整有效区间。非零过渡带的 receiver 及相关 caster 同时落入相邻两级的计划覆盖

#### Scenario: Keep a fixed XY scale during subtexel motion

- **WHEN** 有效切片形状、光方向和有效边长固定，稳定相机沿光空间 XY 微移，先停留在同一吸附格内，再越过相邻格线
- **THEN** 级联的 XY 尺度固定，同一世界样点先保持同一阴影 XY，随后沿跨格方向移动一个世界 texel 对应的阴影栅格量

#### Scenario: Reject an unrepresentable spot shadow independently of illumination

- **WHEN** 影响当前 View 的聚光使用合法的 `π/2` 外锥半角，随后将其改为可表示且仍覆盖接收区的有限透视外锥
- **THEN** 第一次规划报告该灯的单投影不可用且无区域，灯继续按该外锥参与直接光。空间、设备及预算条件满足后，第二次规划为新外锥交付单视图及匹配的区域版本
