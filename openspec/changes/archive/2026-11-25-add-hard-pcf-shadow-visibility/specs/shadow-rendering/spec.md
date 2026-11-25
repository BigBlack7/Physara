# shadow-rendering Spec Delta

## ADDED Requirements

### Requirement: Resolve Hard and PCF from conventional depth comparisons

同一可见性查询 SHALL 在 `[0,1]` 内交付结果。Hard SHALL 比较唯一的近邻深度。PCF SHALL 对当前物理区域内以近邻 texel 为中心的固定 `3 × 3` 核逐样本比较，以等权重归一化比较结果。等深 SHALL 视为可见。核及层权限有效时 SHALL 仅比较同一投影的深度。任一不合法地址、无有效输入或超出名义接收范围的查询 SHALL 使用 `visibility=1` 且不读取相应纹理。Poisson 仅定义可能采用的采样分布，本基线的 PCF 使用固定网格。

接收偏移 SHALL 基于接收处的几何法线、受光方向和当前视图的世界 texel 尺度，并受所交付余量约束。Hard 与 PCF SHALL 使用相同覆盖判定、接收偏移及常规深度比较。主视图 Reverse-Z 与材质法线贴图 SHALL 不改变偏移方向或比较语义。

#### Scenario: Filter comparisons of a mixed depth footprint

- **WHEN** 接收比较深度为 0.5 的有效样点对应包含 4 个深度 0.25 和 5 个深度 0.75 的九点核，中心深度为 0.25
- **THEN** Hard 查询为 0，PCF 查询为 5/9。改变中心深度为 0.5 后，Hard 查询为 1。比较采用同一接收深度和各自样本，边界仍满足相同的层权限。

#### Scenario: Apply the same receiver offset for ordinary lighting and diagnostics

- **WHEN** 同一个受光表面以不同法线贴图取值观察同一阴影边界，且在两个实际世界 texel 尺度下检查偏移
- **THEN** 同一尺度下，诊断和逐灯查询的可见性一致，偏移与几何法线及光空间尺度对应。渲染范围外仍是中性值。已知有遮挡的非共面样点仍被遮挡。

### Requirement: Provide a bounded and complete shadow input to the live view

普通受光分支 SHALL 消费完整灯表中实际获准的阴影组。方向光级联及单局部灯诊断 SHALL 只消费所选组。View 无有效消费时 SHALL 不要求阴影深度生产或采样。每一个可查询的层地址 SHALL 追溯到当前 View、Scene、带 generation 的灯、有效子视图、区域及内容版本、已定义物理层，且在本 Pass 的采样权限内。同一组的层地址 SHALL 形成完整且连续的采样范围。阴影关闭、设备不支持或 CPU 预算拒绝的组 SHALL 以中性可见性交付。

阴影深度输入的替换 SHALL 以已定义的完整消费范围为单位。只有完整正常提交接受的当前消费范围 SHALL 成为可复用的只读结果。实际准备或出图失败的候选 SHALL 沿现有等待或拒绝路径处理，尚未接受的新层及失配旧层 SHALL 不成为当前有效查询输入。

#### Scenario: Render several admitted shadows through one live view

- **WHEN** 同一受光 View 有已准入的主方向光、聚光和点光，另有一个仍参与照明但未分配阴影的灯，且实际阴影图层可互相区分
- **THEN** 存活分支只读取各灯当前配对的深度。图能追溯每层足以定义其采样范围的写入或已接受导入。无分配的灯完全可见。切换到一个局部灯诊断只要求该组的采样范围。

#### Scenario: Cull a cold shadow array with no downstream use

- **WHEN** 调用方声明一个含新阴影数组的 Forward 子图，分别省略下游消费、请求显示色或声明独立深度读回
- **THEN** 无消费的分支没有存活阴影 Pass 或数组分配。显示色消费使所需生产存活，完整正常接受后可保留该路的新数组。独立读回仅通过自己的图依赖使请求的深度存活。

## MODIFIED Requirements

### Requirement: Produce authorized directional shadow depths from static casters

在普通光照或级联诊断中实际消费主方向光阴影的 View SHALL 从同一冻结场景和对应阴影计划生成或只读导入全部已准入级联的 D32 深度。每级 SHALL 对应一个可追溯到计划区域的层，图 SHALL 声明精确的深度写入及后继采样范围。每级 SHALL 使用独立的常规 `[0,1]` 深度，近端为 0，以 1 清除，并以 `Less` 写入。Caster bias SHALL 使用与该投影及 texel 尺度相容的方向和单位。已分配且 caster 集为空的层 SHALL 交付已清除内容。

支持的静态 OPAQUE 和 MASK caster SHALL 使用与正常静态几何一致的实例变换、有效基色 alpha、采样和覆盖阈值。负缩放 SHALL 保持有效绕序，双面材质 SHALL 从两侧投射。当前 View 的准备 SHALL 使画外 caster 需要的几何、实例和 MASK 数据在写入通道消费前可用。投射物准备发生失败时 SHALL 以既有可定位的拒绝或等待状态处理受影响的候选。

#### Scenario: Render a masked offscreen caster into the matching layer

- **WHEN** 冻结场景有遮挡屏内接收点的画外 caster，并有共享静态网格的 MASK、镜像和双面实例，当前图有实际阴影消费
- **THEN** 图先生产被实际读取的数据，再向对应的 D32 层写深度。实心部位留下常规深度，MASK 孔保留后方几何或清除值，实例按各自的变换、绕序和双面属性覆盖。主视图仍可读到匹配的 Reverse-Z 场景深度

#### Scenario: Clear the layer after its last caster leaves

- **WHEN** 一个已分配级联的最后一个 caster 被移除或关闭投射，接收区间仍有效
- **THEN** 匹配该级新版本的深度层只含清除值，原先被该物遮挡的接收点查询为全可见

### Requirement: Resolve directional cascade visibility by stable view depth

方向光查询 SHALL 以稳定相机的线性视深和同次计划的名义分割点确定级联。内部非过渡区 SHALL 使用左闭右开区间，最终有效区间包含有限远端。非空过渡带 SHALL 使用相邻两级各自的有效层和投影，按视深在带内线性混合两级各自过滤后的可见性。

查询 SHALL 按同级的世界 texel 尺度应用共享接收偏移，由该级光空间投影生成 UV 和常规比较深度，使用统一纹理原点。Hard 的单点比较 SHALL 在接收深度小于或等于保存深度时返回全可见。视深超出有效区间时 SHALL 返回中性的 `visibility=1`。每个级联查询 SHALL 只访问投影坐标有效、内容已定义且有采样权限的层，否则该级返回全可见。

#### Scenario: Query a cascade boundary and the ends of shadow coverage

- **WHEN** 一个固定场景的接收样点分布在近端、远端和某个非零过渡带两侧，带内相邻两级在选定点给出可区分的 Hard 或 PCF 可见性
- **THEN** 过渡带内的可见性按两级的可见性及线性视深权重变化，带的两端分别接续相邻级联的结果，覆盖区间外为全可见。仅改变主相机实际 jitter 时，固定世界样点的选级和权重保持稳定

### Requirement: Reuse directional depth only from a matching accepted graph output

当前普通光照或诊断读取范围内生产依赖完全匹配的完整深度输入 SHALL 从同设备的已接受结果复用，静态后继帧保持零深度重绘。复用 SHALL 核对 View、Scene、有效存储规格、实际层映射、逻辑区域及内容版本，以及影响写入的深度程序和偏移规则。深度内容只有在对应生产者实际执行、该 View 的消费分支存活且完整正常提交被接受后 SHALL 成为可供后继帧复用的版本。

取消、裁剪、录制失败或仅有前缀票据的候选 SHALL 保持待产出资格。阴影禁用、布局或有效输入改变时 SHALL 结束不匹配内容的当前可用状态。替换的 GPU 存储 SHALL 沿既有图池和提交完成协议保活及退休。

#### Scenario: Retry an unaccepted replacement without reading a stale layer

- **WHEN** 静态 View 复用了一组已接受深度，随后一个影响阴影覆盖的输入改变，继任候选的场景分支被裁剪或提交未被完整接受
- **THEN** 未接受版本无法用于后继帧采样，恢复后为当前输入生产并接受匹配组才可复用。原来的在途访问仍按其真实完成条件退休

### Requirement: Produce a complete authorized local shadow depth group

当 Forward 的普通光照或诊断实际消费当前 View 的任一已准入局部灯时，Renderer SHALL 由该 View 同轮的冻结场景和阴影计划生产或只读导入完整 D32 组。聚光 SHALL 有一层，点光 SHALL 有六个可独立识别和访问的层。每层 SHALL 可从当前灯、子视图、逻辑区域与分配代追溯到实际层和对应投影。图 SHALL 声明精确的层、深度写入及后继采样权限。聚光或点光的查询 SHALL 使用同一灯的完整组，各点光面 SHALL 一同交付。

局部阴影深度 SHALL 使用常规 `[0,1]` 投影深度、`clear=1` 和 `Less` 写入。Caster bias SHALL 以同一投影和有效 texel 尺度换算到常规深度。实绘 caster SHALL 使用共享的静态实例、有效 MASK alpha、镜像绕序和双面覆盖。可能遮挡屏内接收者的画外 caster SHALL 参与相应投影。已分配且实际无 caster 的层 SHALL 交付已清除内容。准备或生产未完成的组 SHALL 给出可定位原因，查询 SHALL 仅使用完整、已定义且获图授权的当前组。

#### Scenario: Resolve local depths from the actual region mapping

- **WHEN** 一张正式图先诊断聚光，再诊断一个逻辑区域不连续的点光，非对称 caster 图案含画外 MASK、镜像双面实例和一个空面
- **THEN** 聚光的一层及点光的六层分别按其实际映射及投影产出。采样和读回可追溯到正确的灯及面，实心与 MASK 孔可区分，空层为 1，主 View 仍使用 Reverse-Z

### Requirement: Query local shadow visibility in the matching perspective projection

覆盖与选面 SHALL 使用有效灯位至原接收点的方向。聚光 SHALL 使用当轮外锥与对应视图，点光 SHALL 按方向选择一个面并以固定顺序消解同优先级的面边界。标称点光面边缘及角点 SHALL 支持该面上整个过滤核。边界两侧 SHALL 分别使用各自的扩展面投影和同一灯的定义深度。

接收点 SHALL 先通过有效阴影半径、面前方、投影近平面、远平面、UV 及可采样层检查。有效点 SHALL 从写入共用的投影和与该处实际世界 texel 尺度匹配的接收偏移生成 UV 及透视比较深度。Hard SHALL 比较一个有效 texel，PCF SHALL 比较并过滤同一面的有效 texel。每个比较在接收比较深度小于或等于保存深度时 SHALL 得到 1，遮挡时 SHALL 得到 0。无有效组、覆盖外或地址无效的查询 SHALL 返回 `visibility=1`，且跳过纹理读取。

#### Scenario: Query a spot edge and a finite shadow cutoff

- **WHEN** 聚光的有效接收点分布在外锥边界两侧，且无 range 的点光在阴影半径内外均有实际直接光影响，其中一个点光样点的面轴深度仍在投影区间但到灯位的距离超过阴影半径
- **THEN** 锥内及半径内的有效样点用各自投影深度区分遮挡与无遮挡。覆盖外样点返回 1 并跳过深度读取，点光的直接光求值仍按无 range 的距离规则运行

#### Scenario: Cross a point face seam with distinguishable face depths

- **WHEN** 同一已分配点光的相邻面有可区分的深度图案，固定接收点覆盖面边界两侧、精确等优先级方向及三面交角，并有跨面 caster 覆盖边界
- **THEN** 两侧、并列方向和交角各按指定面的投影与层取得整个有效采样核。跨面 caster 的遮挡在边界仍可查询，面外地址无法读到其它灯、其它代或未初始化层

### Requirement: Reuse local depths only after full matching acceptance

局部灯的完整深度组 SHALL 仅在匹配的正常光照或诊断实际消费、完整正常提交被接受且所需层均定义后成为该 View 的只读复用输入。复用 SHALL 匹配设备、View、Scene、带 generation 的灯、有效布局、实际层映射、每个区域及其所需内容版本，以及影响生产的深度程序和 bias。当前读取范围匹配完整已接受输入时 SHALL 跳过相应深度写入与 caster 数据准备。相关空间输入或区域版本变化后，下一次有效查询 SHALL 使用本次匹配的完整内容。

阴影关闭、所需灯失效或读取范围改变、图裁剪、录制失败及前缀提交 SHALL 按各自结果保留正确的可用状态。失配及未接受的深度 SHALL 无法成为当前查询输入。已有在途工作、过往已接受显示和被替换存储 SHALL 沿各自的持有和真实完成协议处理。

#### Scenario: Rebuild and accept the current group after a local-light edit

- **WHEN** 一张 View 首次接受所选点光后保有静态复用，随后灯位或 caster 覆盖改变，使继任候选只获前缀票据，再次准备相同的当前输入
- **THEN** 静态帧没有该组重绘和 caster 准备，旧组在编辑后无法当作当前查询输入。前缀候选无法发布或复用，重试完整接受后才以新组查询；另一路 View 的匹配组可独立保留
