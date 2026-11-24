# Design

## Context

`ShadowPlan` 已用 `LocalShadowVolume` 表达有限灯体，并给聚光分配一个区域、给点光成组分配六个区域。区域跨兼容计划保留，点光的逻辑层因此可能不连续。当前 `LocalShadowPlan` 交付整灯 caster 集，正式 Forward 的 D32 生产、逐层采样和已接受深度缓存仅服务方向光诊断。现有 `GraphicsBaselineV1` 接受二维和 cube 浮点采样图像，图已支持 D32 数组的只读层组交接以及单层二维采样。 [提案](proposal.md) · [逻辑区域规范](../../../specs/shadow-planning/spec.md) · [图层组交接](../../../specs/render-graph-execution/spec.md) · [OpenGL Shader 合同](../../../specs/opengl-shader-pipelines/spec.md)

## Goals / Non-Goals

- 用相同的局部灯投影和区域身份贯通 CPU 计划、静态深度绘制和 Forward 诊断，沿已接受的图输出复用完整组。
- 保证一个诊断帧只为当前 View 有效消费的灯生产深度。D09-T04 负责让受光路径消费各灯的可见性，并交付接收偏移、过滤核和相应边界处理。D09-T05 负责多灯压力与画质成本基准。 [工作包边界](../../../../Docs/Developments/D08-D11-RenderPaths.md#d09-t03)

## Decisions

<a id="local-projection"></a>
### 有限透视投影和覆盖范围

- `Renderer/Lighting` 沿用有限 receiver 与灯体分离判定、稳定灯身份和 D09-T01 的局部灯范围。投影远面与阴影半径同为 `f = min(range.value_or(shadowDistance), shadowDistance)`，近平面使用现有策略 `n = min(0.1 m, f / 1024)`。半径是相对灯位的米制距离上限，近远面裁剪使用该面轴向深度。主视图灯表继续保留源 range。 [准备合同](../../../../Docs/Architecture.md#light-preparation) · [直接光合同](../../../specs/punctual-lighting/spec.md)
- 聚光以有效世界灯位和发射轴建立右手视图，复用 `LightAxes` 保持 CPU caster 外锥、投影视向和绕序一致。平方画幅水平和垂直半角取外锥角。场景允许 `π/2` 外锥半角，单个有限透视投影要求小于 `π/2`。遇到该端点或投影在交付精度下不可用时，沿现有 `InvalidVolume` 路径跳过该灯的阴影准入，保留源灯。 [局部灯计划](specs/shadow-planning/spec.md) · [阴影合同](../../../../Docs/Architecture.md#shadows)
- Lighting 按候选边长 `E` 校核局部透视在有效画幅内的横向精度。先用轴深归一化方形视锥的世界坐标上界，累计 `X、Y、W` 系数窄化和 FP32 点积误差，再按 `W` 的下界约束除法后的 XY 位移。位移上界须落在一个 texel 的 NDC 宽度 `2 / E` 以内。随发射轴旋转放大的横向误差可在 caster 筛选和区域准入前报告 `InvalidVolume`。直接光仍从同一冻结可见灯集按既有合同校验锥角编码。 [局部灯计划](specs/shadow-planning/spec.md) · [灯表交付](../../../specs/punctual-lighting/spec.md#requirement-deliver-the-current-light-set-to-the-shading-pass)
- 点光沿已有子视图顺序，固定面前向和上向。由前向与上向构成右手基，视图局部看向 `−Z`。选择按灯到接收点向量的最大绝对分量定面，完全相等时优先 X、Y、Z，再按分量符号定正负面。实际每面的投影由 CPU 发布，由深度与查询共用。

| 子视图 | 前向 | 上向 |
| --- | --- | --- |
| `+X` | `+X` | `+Y` |
| `−X` | `−X` | `+Y` |
| `+Y` | `+Y` | `−Z` |
| `−Y` | `−Y` | `+Z` |
| `+Z` | `+Z` | `+Y` |
| `−Z` | `−Z` | `+Y` |

- 点光选面用标称 90° 分界。边长为 `E` 时，生产与查询使用扩大后的 `tan(halfAngle) = E / (E - 2)`，让标称边界和三面交角在 UV 上进入各面一 texel 的边带。各面仍使用完整 `E × E` D32 存储及同一 caster 集。当前边带服务单点比较，D09-T04 再按实际过滤核确定跨面 tap 与所需覆盖。 [坐标约定](../../../../Docs/Architecture.md#conventions) · [面边界规范](specs/shadow-rendering/spec.md)

<a id="depth-query"></a>
### 同投影深度、共享几何与偏移

- 以同一灯位和光空间基准备 `clipFromOrigin`，沿用静态顶点与局部灯的位置差值精度合同。设 `t` 为灯到样点向量在当前面前向的轴深，有限常规投影给出 `w = t` 和 `Z(t) = z / w = f / (f - n) * (1 - n / t)`。深度绘制输出齐次坐标，查询保留 `w`，透视除法后只按一次逻辑左上坐标转换得到 UV。距离平方或距离只服务灯体覆盖，比较量采用该面的投影深度。 [深度约定](../../../../Docs/Architecture.md#conventions) · [局部查询](specs/shadow-rendering/spec.md)
- 复用 `StaticShadowDepth` 的顶点、`Depth.frag` 的 MASK 覆盖和现有 `ShadowDepth` 绘制类别。复用静态实例、驻留、有效材质、负 determinant 与双面剔面处理，并在共享上传封存前准备实际会投射的绘制。点光首版沿用整灯保守 caster 集，六面由各自投影裁剪。这样跨面 caster 进入相邻面的边带，额外的逐面 CPU 筛选留待有成本证据时评估。 [几何合同](../../../../Docs/Architecture.md#geometry) · [上游生产与查询职责](../../../../Docs/Reference/Filament/FrameGraphAndPasses.md#f9)
- 维持 `D32`、`clear=1`、`Less` 和既有常规深度 slope bias。轴深 `t` 处的半个世界 texel 使深度求值点变为 `t * (1 + tan(halfAngle) / E)`。Lighting 在候选边长已知后直接编码偏移后的齐次 Z 常量，以交付的 FP32 系数核对近平面轴上是否仍有低于清除值的深度余量。余量不足的局部灯报告 `InvalidVolume`，该组在扣除准入名额之前退出。查询使用无偏投影，接收端等深可见。 [阴影 bias 合同](../../../../Docs/Architecture.md#shadows)
- GLSL 先检查有限方向与半径，点光按方向选面，各类局部灯均以投影的 `w` 核对轴深。聚光的圆锥投到 NDC 的单位圆，以 `dot(ndc.xy, ndc.xy) <= 1` 区分窄锥的内外。地址有效时查单个 texel，合法闭边界进入末端 texel。覆盖外直接返回全可见。

<a id="storage-and-acceptance"></a>
### 逻辑区域、物理层和接受回执

- 保留既有分配优先级及每 View 上限，包括 1024、512、256 texel 档、16 层、32 MiB D32 逻辑字节上限和设备更低限制。预算以实际规划层跨度核算。一个 spot 或完整 point 组获得准入后，Forward 将所选组映射到一个紧凑的 D32 二维数组，分别为一层或六层。每个绑定记录都关联灯、子视图、`ShadowRegion` 的逻辑层、分配代与物理层。已有逻辑地址即使离散也可按面正确映射。 [成组分配规范](../../../specs/shadow-planning/spec.md) · [图池及交接合同](../../../../Docs/Architecture.md#graph-resources)
- 单层深度附件按映射 `Clear`、绘制和 `Store`，空面也执行清除。诊断声明该版本所用的单层 `Texture2D` 深度视图，点光六个面各自绑定。按已支持且完整定义的 `ShaderRead` 层组交接。二维层方案沿用现有 Graph 和 OpenGL 能力，显式保留离散逻辑地址。原生 cube 采样要求相容的完整六面视图，当前预算与复用规则允许逻辑层离散。 [纹理子资源参考](../../../../Docs/Reference/NVRHI/ResourcesAndPipelines.md#n3) · [图执行规范](../../../specs/render-graph-execution/spec.md)
- 生产候选与 View 输出并行准备。Runtime 在相应 View 的查询分支存活、完整正常提交获接受且出口与该票据匹配后，把只读层组交给 View。缓存沿用匹配检查，纳入 Scene、设备、灯代、实际边长、逻辑到物理映射、内容代和影响写入的深度程序。预览颜色使用当前场景和请求，深度按实际生产依赖复用，因此光度、曝光等独立编辑可以产生新颜色而继续读取原深度。命中后显式只读导入，跳过该组 caster 准备、上传及深度绘制。 [跨帧规范](specs/shadow-rendering/spec.md)
- 所选灯或覆盖输入改变时，用匹配新版本的候选存储；关闭诊断、换场景及分配失配时结束该保留组的当前资格。仅有前缀票据或分支裁剪的候选继续待生产，上次接受颜色按现有视口合同展示，被替换资源按原保活和 GPU 完成协议退休。已知不支持的 caster 沿既有定位摘要报告。暂未就绪的支持数据经现有等待或拒绝路径重试，当前组随实际完整产出而确认。

<a id="view-diagnostic"></a>
### 按 View 选择与诊断呈现

- 在当前 Renderer 面板为现有诊断选择补充 spot／point 灯。View 保存普通显示请求和会话诊断选择。EditorSession 提交带 View、Scene、`EntityHandle` generation 和预期灯型的选择，附加 View 通过同一类型化入口请求。准备从当前冻结灯表与 `ShadowPlan` 解析选择，即使未准入也能显示该灯的原因。选择更改后，未来录制和接受都核对本次请求身份。 [View 规范](specs/view-preparation/spec.md)
- 普通、方向光级联和一盏局部灯是互斥显示选择。局部灯诊断沿现有线性显示和 ViewOutput 路径，以明暗表示实际比较值，点光以面颜色辅助定位。选择失效或关闭时按有效的普通显示配置回退。离屏附件预检以实际诊断是否有可用计划为依据，只有匹配的诊断候选才可用较小的线性显示附件。普通候选超限且没有可见几何时，用已提取的灯表解析选中灯身份或覆盖原因，并按该 View 上次交付的分配做一次结算。图开始后的失败沿安全 UI 和上次接受输出处理。 [Forward 规范](specs/static-forward-rendering/spec.md) · [视口输出](../../../specs/editor-viewport-output/spec.md)
- Shader 通过现有生成接口声明局部投影块及逐层绑定。spot 与 point 可共用诊断代码，以绑定一层和六层的真实结构差异选择编译角色或变体，其余投影、灯选择与边长均是帧数据。正式程序包随现有准备预算与帧边界完整代交付。面板分别核对 CPU 计划和已接受颜色的 Scene 内容及相机版本、View 请求和实际诊断身份。历史诊断保留它的灯和接受票据。 [代与准备](../../../../Docs/Architecture.md#plan-readiness) · [规划统计](../../../specs/render-frame-statistics/spec.md)

## Risks / Trade-offs

- 面角色、texel 朝向或投影 `w` 错误 → 用有非对称图案的真实六面深度、边界两侧与离轴接收点交叉核对。保守共用点光 caster 集可能增加面间重复提交，当前成本由每 View 的实际诊断组及既有准备预算约束。
- 删除或换灯可能让旧区域与新灯共用一个数字地址 → 深度复用与 UI 接受都校验所属 View、带 generation 的灯和区域版本。回放包含静态命中、编辑、部分提交及两路 View 各自接受。
- 极宽聚光、狭小近远区间或设备能力限制可能使投影或区域不可用 → 在计划及准备边界报告原因，交付可用的普通显示并保留全部直接光记录。

## Migration Plan

沿现有产品目录与 Shader 生成入口接入局部灯程序和配置。场景 v1 沿原入口打开，诊断初始关闭。使用普通显示退出诊断时保留用户的原 View 设置，旧深度出口沿正常图完成协议释放。 [构建入口](../../../../Docs/Build.md)
