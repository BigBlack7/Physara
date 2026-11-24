# shadow-planning Spec Delta

## MODIFIED Requirements

### Requirement: Plan finite shadow views with conservative caster coverage

启用规划的有效 View SHALL 从当前冻结场景、稳定有限相机和保守可见灯集合构造阴影视图。主方向光 SHALL 在稳定相机近面至 `min(cullingFar, shadowDistance)` 的有限区间内形成所请求的 1–4 级。透视 SHALL 按请求权重混合线性和对数分割，正交 SHALL 线性分割并支持零近平面。有效端点 SHALL 有限且严格递增。空区间或端点无法按交付精度表示时 SHALL 返回可定位的无分配原因。

聚光 SHALL 以外锥与有限灯空间范围描述一个视图。点光 SHALL 描述六个有序面及共同的有限近远面。局部灯未指定 range 时，有限阴影距离 SHALL 只限制阴影覆盖，直接光仍保持无范围截断。局部灯的有限阴影覆盖与接收视锥完全分离时 SHALL 有明确的无分配结果。阴影视图 SHALL 交付独立的常规深度约定。

各方向光级联 SHALL 以该级完整 receiver 切片的包围球定出固定光空间 XY 尺度，按有效内部分辨率的世界 texel 网格吸附中心，并保持可查询的 receiver 覆盖。内部切分两端声明的过渡带 SHALL 同时受到相邻两级的拟合和 caster 筛选覆盖。每个已分配视图 SHALL 提供相应的保守 caster 集，来自同一份冻结场景中可见、有 bounds 且允许投射阴影的 Primitive。可能沿光线到达接收区域的画外 Primitive 与相切候选 SHALL 保留，能确定与阴影视图覆盖分离的 Primitive SHALL 剔除。每级 Z 拟合 SHALL 覆盖接收段和相关 caster 的深度范围。

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

### Requirement: Allocate bounded shadow regions with stable light correspondence

每个 View SHALL 交付光源与阴影视图、区域或明确无分配结果的对应关系。灯 SHALL 以带 generation 的稳定场景内身份关联同轮灯记录。区域 SHALL 具有可定位的层、像素范围和分配代。方向光的各级 SHALL 有确定的近远顺序和可单独识别的区域。在有效布局和分配相容时，同一灯及子视图 SHALL 保有其区域索引。被释放的地址再分配时 SHALL 带新的区域代。

计划 SHALL 按已解析的质量档、设备能力、层和逻辑字节上限进行准入。一个主方向光优先，随后为聚光和点光，同类按稳定灯身份排序。主方向光的已请求级联、聚光的单视图和点光的六面 SHALL 各自成组发布。有效尺寸或容量不足时 SHALL 按此顺序降低分辨率或拒绝阴影分配，并报告生效值及原因。准入的方向光 SHALL 保持请求级数。逻辑字节 SHALL 涵盖实际规划的层跨度，所有区域 SHALL 处于该布局内。无阴影分配的灯 SHALL 按完整灯集合的正常规则参与照明。

#### Scenario: Admit or reject a whole point-light allocation

- **WHEN** 同一预算下主方向光和聚光先取得区域，剩余容量先不足六层，后因容量扩大而足够
- **THEN** 点光先有明确无分配结果，后一次取得六个合法区域。相容的区域索引保持，计划始终满足字节及层上限，完整灯集合仍可使用

#### Scenario: Change the storage extent and reuse a released layer

- **WHEN** 删除一个已分配灯且当前 View 继续使用相容的区域布局，另一灯随后取得被释放的层，再改变有效边长
- **THEN** 消失的灯从当前对应关系移除，复用地址拥有新区域代，尺寸变化产生匹配新布局的区域身份与用量

#### Scenario: Fit a complete directional group within the budget

- **WHEN** 请求四级、1024 texel 的主方向光，在允许四层的设备上取得 4 MiB 的 D32 逻辑预算，随后设备层上限收紧到三层
- **THEN** 首次在 512 texel 档准入四级，层地址相异且范围合法。层容量收紧后该方向光无区域且有可定位原因，它仍在直接光集合中

### Requirement: Version required shadow contents and reuse unchanged plans

CPU 计划 SHALL 提供与每个已分配区域相匹配的所需内容版本。静态输入和解析后的阴影设置相同的后继准备 SHALL 复用计划和区域版本。投射物纳入、移出或改变实际几何覆盖，灯的有效空间参数、接收范围或区域质量使所需深度改变时 SHALL 更新受影响的版本。方向光各级 SHALL 独立标明内容代。只影响曝光、每帧实际抖动、灯光光度或诊断显示的变化 SHALL 复用与其独立的阴影内容。缓存 SHALL 按 View 和 Scene 隔离，并保持已交付的当前帧数据在约定借用期内可读。

禁用阴影、灯删除或有效输入不可获得时，当前计划 SHALL 对相应对象给出无分配结果或失败原因，恢复时 SHALL 从当前输入重新确认。CPU 计划的区域版本 SHALL 表示需要生产的内容，后续深度生产以对应的实际接受结果确认完成。

#### Scenario: Reuse frozen inputs and invalidate the affected shadow volume

- **WHEN** 已规划的静态输入经过帧槽轮转，期间只变更曝光和实际 jitter，随后一个曾在画外的 caster 移出某灯某级的覆盖范围
- **THEN** 冻结轮次复用计划且不增加所需内容版本。该 caster 的消失推进受影响区域的版本，依赖未变的区域继续复用

#### Scenario: Clear and restore planning state

- **WHEN** 有效规划的 View 禁用阴影，再开启规划并观察替换后的 Scene 或改变了有限接收范围的相机
- **THEN** 禁用时当前区域为空且有原因，重新启用时计划只引用当前 Scene、相机及匹配的区域版本
