# Spec Delta

## ADDED Requirements

### Requirement: Produce a complete authorized local shadow depth group

当实际 Forward 诊断消费当前 View 的一个已准入局部灯时，Renderer SHALL 由该 View 同轮的冻结场景和阴影计划生产或只读导入完整 D32 组。聚光 SHALL 有一层，点光 SHALL 有六个可独立识别和访问的层。每层 SHALL 可从当前灯、子视图、逻辑区域与分配代追溯到实际层和对应投影。图 SHALL 声明精确的层、深度写入及后继采样权限，各点光面 SHALL 一同交付。

局部阴影深度 SHALL 使用常规 `[0,1]` 投影深度、`clear=1` 和 `Less` 写入。Caster bias SHALL 以同一投影和有效 texel 尺度换算到常规深度。实绘 caster SHALL 使用共享的静态实例、有效 MASK alpha、镜像绕序和双面覆盖。可能遮挡屏内接收者的画外 caster SHALL 参与相应投影。已分配且实际无 caster 的层 SHALL 交付已清除内容。准备或生产未完成的组 SHALL 给出可定位原因，查询 SHALL 仅使用完整、已定义且获图授权的当前组。

#### Scenario: Resolve local depths from the actual region mapping

- **WHEN** 一张正式图先诊断聚光，再诊断一个逻辑区域不连续的点光，非对称 caster 图案含画外 MASK、镜像双面实例和一个空面
- **THEN** 聚光的一层及点光的六层分别按其实际映射及投影产出。采样和读回可追溯到正确的灯及面，实心与 MASK 孔可区分，空层为 1，主 View 仍使用 Reverse-Z

### Requirement: Query local shadow visibility in the matching perspective projection

查询 SHALL 从有效灯位至接收点的方向出发。聚光 SHALL 使用当轮外锥与对应视图，点光 SHALL 按方向选择一个面并以固定顺序消解同优先级的面边界。标称点光面边缘及角点 SHALL 落入该面的定义采样范围，边界两侧 SHALL 分别使用各自面的同次投影及相同灯的深度层。

接收点 SHALL 先通过有效阴影半径、面前方、投影近平面、远平面、UV 及可采样层检查。有效点 SHALL 从写入所用的同一无偏投影作透视除法得到常规比较深度，并从实际层读取一个有效 texel。接收比较深度小于或等于保存深度时 SHALL 返回 1，遮挡时 SHALL 返回 0。无有效组、覆盖外或地址无效的查询 SHALL 返回 `visibility=1`，且跳过纹理读取。

#### Scenario: Query a spot edge and a finite shadow cutoff

- **WHEN** 聚光的有效接收点分布在外锥边界两侧，且无 range 的点光在阴影半径内外均有实际直接光影响，其中一个点光样点的面轴深度仍在投影区间但到灯位的距离超过阴影半径
- **THEN** 锥内及半径内的有效样点用各自投影深度区分遮挡与无遮挡。覆盖外样点返回 1 并跳过深度读取，点光的直接光求值仍按无 range 的距离规则运行

#### Scenario: Cross a point face seam with distinguishable face depths

- **WHEN** 同一已分配点光的相邻面有可区分的深度图案，固定接收点覆盖面边界两侧、精确等优先级方向及三面交角，并有跨面 caster 覆盖边界
- **THEN** 两侧、并列方向和交角各按指定面的投影与层取得有效 texel。跨面 caster 的遮挡在边界仍可查询，面外地址无法读到其它灯、其它代或未初始化层

### Requirement: Reuse local depths only after full matching acceptance

局部灯的深度组 SHALL 仅在匹配诊断实际消费、完整正常提交被接受且所需层均定义后成为该 View 的只读复用输入。复用 SHALL 匹配设备、View、Scene、带 generation 的灯、有效布局、每个区域及其所需内容版本，以及影响生产的深度程序和 bias。命中时 SHALL 跳过该组的深度写入与 caster 数据准备。相关空间输入或区域版本变化后，下一次有效查询 SHALL 使用本次匹配的完整内容。

阴影关闭、所选灯失效或更换、图裁剪、录制失败及前缀提交 SHALL 按各自结果保留正确的可用状态。失配及未接受的深度 SHALL 无法成为当前查询输入。已有在途工作、过往已接受显示和被替换存储 SHALL 沿各自的持有和真实完成协议处理。

#### Scenario: Rebuild and accept the current group after a local-light edit

- **WHEN** 一张 View 首次接受所选点光后保有静态复用，随后灯位或 caster 覆盖改变，使继任候选只获前缀票据，再次准备相同的当前输入
- **THEN** 静态帧没有该组重绘和 caster 准备，旧组在编辑后无法当作当前查询输入。前缀候选无法发布或复用，重试完整接受后才以新组查询；另一路 View 的匹配组可独立保留
