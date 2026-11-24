# Tasks

## 1. 局部灯计划与 View 请求

- [x] 1.1 A 从现有有限灯体交付聚光单投影和点光六面，校验投影交付精度、固定轴向、共同近远面及点光边带。V1 核对非对称世界样点、光照 range 独立和 `π/2` 聚光的阴影拒绝。 [计划规范](specs/shadow-planning/spec.md) · [投影设计](design.md#local-projection)
- [x] 1.2 A 将投影接入现有保守 caster、区域准入与内容版本。保留画外和跨面候选，关联逻辑区域和对应的物理层。V1 核对有间隙的逻辑层、六面全组准入及失败、保持直接光与相关版本变化。 [层与接受设计](design.md#storage-and-acceptance)
- [x] 1.3 A 在 View 与正式准备入口解析局部灯的会话选择、可用性和附件配置，将失配选择和分配结算与所属 View 对齐。V3 先核对类型化请求、灯身份及离屏附件边界。 [View 规范](specs/view-preparation/spec.md)

## 2. 共享深度绘制与实际层生产

- [x] 2.1 A 复用生成接口、静态 Draw、MASK、负缩放和双面 Pipeline，按局部透视的 texel 尺度应用常规 caster bias。接入正式构建及 Shader 代准备。V4 核对实际 ABI、活动绑定和角色，V2 以真实深度样点核对偏移方向与覆盖。 [深度设计](design.md#depth-query)
- [x] 2.2 A 在共享上传封存前准备有效 caster，以 RenderGraph 声明当前局部组的精确子资源、Clear/Store 和图尾只读交接。V2 核对画外 caster、MASK 孔、空面、精确实际层及主 Reverse-Z，保留已有 CSM 路径的覆盖。 [深度规范](specs/shadow-rendering/spec.md) · [层生产设计](design.md#storage-and-acceptance)

## 3. 可见性、正式诊断与跨帧接受

- [x] 3.1 A 在共享 Shadow 查询代码中接入局部投影、定面和单 texel 比较。声明实际被查询的视图，在任何读取前处理未分配、无效或越界的输入。V2 对照实际六面深度、点光边界和聚光锥边，确认半径外中性结果与直接光独立。 [查询规范](specs/shadow-rendering/spec.md)
- [x] 3.2 A 经现有 Renderer 面板和 ViewOutput 实现局部灯诊断、失配原因与普通显示恢复。V3 从正式请求入口核对切换、预算拒绝、实际接受画面和票据。 [Forward 规范](specs/static-forward-rendering/spec.md) · [诊断设计](design.md#view-diagnostic)
- [x] 3.3 A 连接 View 对完整只读层组的接受与复用。V3 核对静态命中无重绘和 caster 准备、灯和静物编辑、拒绝后重试、两路 View 隔离及资源保活。V4 核对影响深度写入的程序代切换。 [复用设计](design.md#storage-and-acceptance)

## 4. A 集成自检与交接

- [x] 4.1 A 运行 V1–V4 尚需的集成检查和当前 change 严格校验。记录当前工作树、正式包和二进制、实际命令、场景输入、结果及未覆盖范围，交维护者调度独立 B。 [A 本地验证](#a-local-verification) · [B 签收问题单](../../../../build/d09-t03/final/review-signed.md)

## 5. 独立审查、整改与复核

- [x] 5.1 B 独立核对设计及实际消费者，重点审查米制覆盖与透视深度、六面及跨面保证、计划到物理层的映射和接受边界。按约定核对并完善本轮第一方文件的职责、接口和流程注释，在当前问题单登记结论和定位证据。 [独立流程](../../../../AGENTS.md#review-loop) · [注释要求](../../../../AGENTS.md#annotation) · [首轮记录](#b-first-review)
- [x] 5.2 A 按问题单编号修正并自检受影响行为，在 change 留存本轮正式输入、整改结果及回放证据，由维护者交 B。 [首轮整改](#a-correction-1) · [第二轮整改](#a-correction-2)
- [x] 5.3 B 独立复核整改及注释，确认签收输入。首次复核关闭 CR-001 至 CR-004，第二次复核关闭 CR-005，五项全部通过。 [首次复核](#b-recheck-1) · [最终 B 签收](#b-recheck-2)

## 6. 最终验证、同步与归档

- [x] 6.1 A 在 B 签收后执行最终 verify。核对审查输入、正式源码与包、原生执行及场景身份，确认三类灯的查询、边界和预算退路满足 D09-T03。记录实际原生检查、匹配的复用证据及覆盖范围。 [最终验证](#final-verification) · [工作包完成条件](../../../../Docs/Developments/D08-D11-RenderPaths.md#d09-t03)
- [x] 6.2 A 在 change 留存签收问题单及验收证据，将增量合并到主规范，核对原有场景及本轮新增场景，并执行 OpenSpec 严格校验。 [规范同步](#spec-sync)
- [x] 6.3 A 执行 OpenSpec 归档，复核归档、文档链接和工作包状态。更新 D09-T03 验收链接，确认 D09-T04 前置，完成留存后按项目流程复位当前问题单。 [归档验收](#archive-closeout) [开发计划](../../../../Docs/Developments/Developments.md#current-stage)

## 验证口径与停止条件

| 检查 | 具体风险 | 最小可判定结果 |
| --- | --- | --- |
| V1 · CPU 几何与预算 | 用错面轴、半径和轴深混用、投影无法表示、点光部分准入或区域别名 | 用一个有限场景核对 spot 及六面的标称轴、非对称离轴点和边界。以 `f=8 m` 得 `n=1/128 m`，对灯相对点 `(3,2,1) m` 检查 `+X` 面用 `3 m` 轴深生成的投影值。检查无 range 点光的有限阴影和 `π/2` 聚光的单灯拒绝。用一次混合分配对照点光满六层与少一层，并核对有逻辑层间隙的成组映射、释放后重新分配及光度编辑不改变所需深度。 |
| V2 · 真实 OpenGL 深度与查询 | 写错层或原点、bias 符号错误、漏掉跨面或 MASK 覆盖、越界读取 | 用一份非对称静态场景，六面各有可辨认的深度内容或清除对照。结合实际图访问、逐层 D32 读回和线性诊断颜色，核对近远遮挡、画外 MASK 的实心和孔、镜像双面及空层。同时取聚光锥边、点光相邻面两侧、精确并列方向和三面角的样点，选一个轴深有效而半径越界的点，核对中性且无越界读取。核对主 Reverse-Z 和一组既有方向光选级、比较及过渡。 |
| V3 · 正式帧请求、发布与复用 | 用错灯 generation 或 View、部分工作被认成当前结果、预算退路丢灯、重复释放 | 在复用 V2 输入的正式帧回放中，主、离屏 View 分别请求不同诊断，确认静态命中。分别编辑灯的空间参数与 caster 覆盖，令一个已录制继任帧只获前缀票据，再用当前输入恢复并核对颜色、深度、图访问和接受回执。沿同一回放观察身份复用拒绝、六层预算退路与普通直接光、离屏线性诊断可用而 FXAA 超限时的单次结算。 |
| V4 · 构建、生成 ABI 与代切换 | 正式入口缺程序包、活动图像维度或常量错配、旧深度跨生产代复用 | 用现有 Windows x64 构建入口生成并消费受影响程序，核对 Debug、Release 受影响正式目标及主要诊断的本机 OpenGL link 与绘制。只检查实际变动的块、角色与绑定。在 V3 回放用一次影响深度写入的程序内容换代确认失效与重建。 |

先复用仍可取得且输入匹配的 D09-T01、D09-T02 场景与断言。仅为区分新增风险补代表性场景，临时输入、脚本和回放全部放在 `build/d09-t03/`。结构检查和 CPU 结果分别记录，真实 D32 与接受协议由原生结果证明。各合同有当前输入证据并记清未覆盖范围后停止，遇到失败、输入变化或具体缺口再扩大检查。 [验证约定](../../../../.agents/skills/physara-verification/SKILL.md) · [D09-T02 验收入口](../2026-11-24-add-stable-directional-csm/tasks.md#final-verification)

<a id="a-local-verification"></a>
## A 本地验证

- **V1 CPU。** Release 核对聚光和点光六面的非对称投影、`1/128 m` 近面和组内共享范围。等分量角点落在有效边带。五层余量拒绝整组，释放一个较早的聚光区域后，第二盏点光取得 `1、11–15` 六个逻辑层。光度编辑保持内容代，灯位编辑改变内容代并保留区域代。`π/2` 聚光仍进入完整灯集合，其阴影报告 `InvalidVolume`。错误 View、灯型及已删除的灯选择被拒绝。 [CPU 日志](../../../../build/d09-t03/a/cpu-Release.log)
- **V2 原生。** Debug、Release 均在 Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129 回放正式 Forward。图逐层授权，将 `1、11–15` 的逻辑区域映射到物理 `0–5`，并从 D32 读出五个具备投射物的离轴深度和一个清除为 1 的面。实际颜色确认点光边界两侧、精确并列及三面交角、聚光锥边、米制半径外、MASK 孔与画外遮挡。主深度保持 Reverse-Z。复用 D09-T02 场景以当前产品库重新链接，两个配置均核对 64,464 个 CSM 过渡样点，其中 1,010 个能区分相邻级联。 [局部 Release](../../../../build/d09-t03/a/native-local-Release.log) · [局部 Debug](../../../../build/d09-t03/a/native-local-Debug.log) · [CSM Release](../../../../build/d09-t03/a/native-csm-Release.log) · [CSM Debug](../../../../build/d09-t03/a/native-csm-Debug.log)
- **V3 正常帧。** 混合预算拒绝所选点光时，正式普通路径仍求值七盏灯，所选点光没有深度采样。两路 View 独立接受的静态组只读导入，图无深度写 Pass，准备路径跳过 caster。移动点光只重绘该组，一次已录制继任帧只取得前缀票据，旧诊断票据保留，重试重新生产并接受。关闭画外 MASK caster 后两个相交组更新。离屏大尺寸可接受线性诊断，灯删除后普通 FXAA 超限，两次结算释放 `16、0` 个逻辑区域，主 View 仍可接受，缩回尺寸后恢复 FXAA。 [回放步骤](../../../../build/d09-t03/a/LocalReplay.inc)
- **V4 构建与换代。** Windows x64 正式 Debug、Release 构建及隐藏启动均通过。两配置各八个 StaticForward 角色包通过生成 ABI 检查，实际局部程序通过 OpenGL 准备和绘制。隔离源改动把新深度程序代从 1 推进到 2，旧层组失效，每个配置均读到 7,049 个按预期偏移的 D32 样点。 [Release 构建](../../../../build/d09-t03/a/product-current-Release.log) · [Debug 构建](../../../../build/d09-t03/a/product-current-Debug.log) · [ABI](../../../../build/d09-t03/a/shader-abi.log)
- **证据范围。** 性质为一个真实 Windows OpenGL 设备上的产品回放。Renderer 面板经过正式编译和启动，选择与恢复由同一类型化请求入口在正式 Frame 中驱动。D32 的开发读回沿图的 `CopySource` 权限进入测试 PBO，探针等待实际完成。验证按本工作包的单点诊断范围执行。 [复现脚本](../../../../build/d09-t03/a/Probe.ps1) · [输入、产物及日志清单](../../../../build/d09-t03/a/handoff.json)

<a id="b-first-review"></a>
## B 首轮审查

- CR-001 至 CR-004 待 A 整改，分别涉及窄聚光锥外误采样、近半球投影获准却不能写深度、相机变化后的面板标记、离屏附件超限时丢失灯身份原因。问题单保留逐项输入、观测值和验证要求。 [B 签收问题单](../../../../build/d09-t03/final/review-signed.md)
- 核对 A 的 36 个正式输入并修改其中 12 个文件的注释。交回整改的源码摘要为 `29ba94f735ee0a16d927d11b33fa15d58be9cbf1446da65da3b4e06fd353008f`。Release CPU 与 CSM 对照通过，局部灯定向回放使用本机 Intel OpenGL 4.6。面板当前标记按实际版本与源码条件核对。 [注释记录](../../../../build/d09-t03/b-review/annotations.json) · [定向回放](../../../../build/d09-t03/b-review/runtime.log)

<a id="a-correction-1"></a>
## A 首轮整改与自检

- **CR-001。** `0.00030 rad` 聚光在同一无偏投影的单位圆内查询。沿用 B 的三个方向，加入深度可查的实心 caster。锥内读回遮挡色 `(0.23499, 0.06250, 0.05249)`，锥外且在方框内和方框外两点均读回 `(0.34985, 0.34985, 0.34985)`。 [定向回放](../../../../build/d09-t03/a-correction-1/native-cr-Release.log)
- **CR-002。** 按候选边长核对含偏移的 FP32 投影。`nextafter(π/2, 0)` 在 256 texel、4 m 远面的输入上返回 `InvalidVolume`。释放该名额后第二个点光获得完整六层，普通 Forward 提交七盏灯。80° 对照恢复 `Planned`、D32 有效写入和遮挡色。 [定向回放](../../../../build/d09-t03/a-correction-1/native-cr-Release.log)
- **CR-003。** 实际 ImGui 面板记录核对两种独立变化。Scene 相机从 1 变为 2、View 文档从 2 变为 3 时，旧诊断票据仍可查。各自匹配的计划与正常颜色接受后才显示当前计划和接受标记。 [定向回放](../../../../build/d09-t03/a-correction-1/native-cr-Release.log)
- **CR-004。** 离屏 `2560 × 2304` 在接受诊断后移离几何并删除所选灯，同时交付 `SceneAttachmentLimit` 和 `LightUnavailable`。连续两轮释放 `11、0` 区域，历史票据保留。选择仍存在的灯时在空可见集报告 `NoReceiverCoverage`，缩至 `320 × 240` 后恢复普通 FXAA。 [定向回放](../../../../build/d09-t03/a-correction-1/native-cr-Release.log)
- **回归与范围。** 当前产品 Debug、Release 正式构建和隐藏启动通过，各八个 StaticForward 角色均通过生成 ABI 检查。Release CPU V1、局部灯 V2–V4 和方向光 CSM 对照用当前库重跑。原生回放使用 Intel RaptorLake-S Mobile / OpenGL 4.6.0 Build 32.0.101.6129，面板用自动文本记录核对。 [Release 构建](../../../../build/d09-t03/a-correction-1/product-Release.log) · [Debug 构建](../../../../build/d09-t03/a-correction-1/product-Debug.log) · [ABI](../../../../build/d09-t03/a-correction-1/shader-abi.log) · [CPU](../../../../build/d09-t03/a-correction-1/cpu-Release.log) · [局部灯](../../../../build/d09-t03/a-correction-1/native-local-Release.log) · [CSM](../../../../build/d09-t03/a-correction-1/native-csm-Release.log)
- **交接输入。** A 在匹配 B 注释版的 36 个正式文件上修正 11 个文件。整改汇总 SHA-256 为 `134ebe72827e949cf355f51e55cc712891b017352ffdadb02c93becc5d5cebc2`。5.2 已完成，5.3 由 B 独立复核并签收。 [版本、产物与复现清单](../../../../build/d09-t03/a-correction-1/handoff.json) · [B 签收问题单](../../../../build/d09-t03/final/review-signed.md)

<a id="b-recheck-1"></a>
## B 首次整改复核

- CR-001 至 CR-004 已由 B 复核关闭。原窄锥三样点显示遮挡、灰、灰；近半球输入释放名额，80° 对照仍有遮挡。实际面板在两种相机换代下均区分历史票据与当前帧。离屏附件超限保留选灯原因，连续释放 11、0 层。 [定向回放](../../../../build/d09-t03/b-recheck-1/cr.log)
- CR-005 待 A 整改。旋转聚光半角为 `1e-8 rad` 时，FP32 矩阵把中心射线投到 NDC X ≈ 2.25773，当前组仍获准。本机读回中性灰，同灯的 `0.00030 rad` 对照读回有效面色。横向投影准入与闭环要求见问题单。 [失败对照](../../../../build/d09-t03/b-recheck-1/rotated-check.log) · [B 签收问题单](../../../../build/d09-t03/final/review-signed.md)
- 已核对 A 交接的 36 个产品输入及登记的产物与回放输入。本次 B 补充 `ShadowVolumes.hpp` 的筛选与准入时序注释，交回版本为 `f6a8ac22230e623941202164b9c1591a9517f39869b1ece2c266e0abfbc401c8`。Release CPU、局部灯 V2–V4、CSM 对照均通过，两个局部灯包独立重建后与交接包一致。原生回放限本机 Intel OpenGL 4.6。 [输入与证据](../../../../build/d09-t03/b-recheck-1/review.json) · [CPU](../../../../build/d09-t03/b-recheck-1/cpu.log) · [局部灯](../../../../build/d09-t03/b-recheck-1/local.log) · [CSM](../../../../build/d09-t03/b-recheck-1/csm.log)

<a id="a-correction-2"></a>
## A 第二轮整改与自检

- **CR-005 准入。** 按编码后的 FP32 系数、点积余量和有效边长限制局部投影的横向误差，按一个 texel 的 NDC 宽度校验。沿用 B 的 `(7/8, 0, −1)` 灯轴，`1e-8 rad` 旋转聚光返回 `InvalidVolume`，空出的名额使第二个点光取得整组六层。灯轴与世界坐标轴对齐时，同角度的 CPU 投影可准入。`0.00030 rad` 的旋转对照及恢复回放均读回中心面色 `(0.93994, 0.25000, 0.20996)`。 [定向回放](../../../../build/d09-t03/a-correction-2/native-rotated-Release.log)
- **普通显示。** 拒绝阴影准入后，冻结可见集保留七条灯输入及原聚光的外锥和 range。初次回放预期受光帧可正常接受，实际复现该极窄外锥在既有灯表边界的精度拒绝。定向回放确认 `SceneLightTableRejected`、Base Color 的正常接受与历史票据保留，并在 `0.00030 rad` 时重新提交七盏直接光。 [初次回放](../../../../build/d09-t03/a-correction-2/direct-contract-discovery.log) · [灯表合同](../../../specs/punctual-lighting/spec.md#requirement-deliver-the-current-light-set-to-the-shading-pass) · [修正后回放](../../../../build/d09-t03/a-correction-2/native-rotated-Release.log)
- **回归与范围。** Debug、Release 正式增量构建及隐藏启动通过。Release V1、CR-001 至 CR-004 和局部灯 V2–V4 用当前库重跑，原生设备为 Intel RaptorLake-S Mobile / OpenGL 4.6.0 Build 32.0.101.6129。共 30 个 Shader 包与生成接口文件的摘要与前轮匹配，ABI 结果沿用上一轮。方向光 CSM 对照见 B 首次整改复核。 [构建记录](../../../../build/d09-t03/a-correction-2/product-Release.log) · [V1](../../../../build/d09-t03/a-correction-2/cpu-Release.log) · [原四项](../../../../build/d09-t03/a-correction-2/native-cr-Release.log) · [局部灯](../../../../build/d09-t03/a-correction-2/native-local-Release.log) · [包与接口摘要](../../../../build/d09-t03/a-correction-2/unchanged-shader-inputs.json) · [B 的 CSM 回放](../../../../build/d09-t03/b-recheck-1/csm.log)
- **交接输入。** 在 B 的已登记版本上修改两个 Lighting 文件，36 个正式输入的汇总 SHA-256 为 `3df6387365c3fbbb04336e6dd35dc6828bdbb68e59b7c8d64c12f20b059000e2`。CR-005 待 B 独立复核。 [正式文件、产物与复现清单](../../../../build/d09-t03/a-correction-2/handoff.json) · [B 签收问题单](../../../../build/d09-t03/final/review-signed.md)

<a id="b-recheck-2"></a>
## B 第二次整改复核与签收

- **CR-005 已关闭。** B 从当前源码直接编译投影实现，复用上轮失败的原始断言。旋转灯轴的 `1e-8 rad` 输入返回 `InvalidVolume`，第二个点光取得六层；`0.00030 rad` 对照及恢复后的同一中心均读回有效聚光面色。轴对齐的同角度 CPU 对照仍可准入。极窄角度保留七条可见灯输入，按既有灯表合同拒绝直接受光，普通基色可出图；恢复可编码角度后重新提交七盏直接光。 [原始断言复跑](../../../../build/d09-t03/b-recheck-2/original-b.log) · [拒绝与恢复](../../../../build/d09-t03/b-recheck-2/rotated.log)
- **回归与范围。** 原 CR-001 至 CR-004 以及局部灯 V2–V4 在本机 Windows x64、Intel OpenGL 4.6 复跑通过。完整构建、隐藏启动及 CPU V1 复用 A 第二轮证据；前轮 ABI 对照所用的 30 个包与生成头逐项一致。方向光 CSM 沿用首次复核结果。 [原四项](../../../../build/d09-t03/b-recheck-2/cr.log) · [局部灯](../../../../build/d09-t03/b-recheck-2/local.log) · [输入核验](../../../../build/d09-t03/b-recheck-2/inputs.json)
- **签收版本。** 代码正确性与注释可读性均通过。B 在 `ShadowVolumes.cpp` 补充 W 误差影响 XY 位移的说明。基准 `34a2e5140b6aa6a5ba83b8ea5e0a09fc5c512ca3`，36 个正式文件的最终 SHA-256 为 `38f90c589543970085eff98c763b5f5cc45aed4994914bed980179f0f04b6ba1`。5.3 已完成，6.1–6.3 待 A 执行。 [逐文件签收与回放清单](../../../../build/d09-t03/b-recheck-2/review.json) · [B 签收问题单](../../../../build/d09-t03/final/review-signed.md)

<a id="final-verification"></a>
## A 最终验证

已核对 B 签收的 36 个正式文件的逐项身份，其中含 6 个未跟踪 Shader 文件，汇总 SHA-256 为 `38f90c589543970085eff98c763b5f5cc45aed4994914bed980179f0f04b6ba1`。CR-001 至 CR-005 全部关闭。原问题单按原字节留存，原件内的引用保留签收时以 `Docs/CodeReview.md` 为起点的路径。 [签收输入](../../../../build/d09-t03/final/signed-inputs.json) · [B 签收原件](../../../../build/d09-t03/final/review-signed.md) · [B 复核结论](#b-recheck-2)

| 验证范围 | 签收版依据和结果 |
| --- | --- |
| 阴影计划及预算 | Lighting 交付聚光与点光六面，CPU 核对非对称坐标、轴深、显式 range 与无 range 的独立阴影半径、整组拒绝和离散层。重链 CSM 探针，核对透视和正交切分、固定尺度吸附及画外投射物。近半球和旋转极窄聚光在有效边长下报告 `InvalidVolume`，可表示的对照恢复实际查询。 [投影实现](../../../../Engine/Renderer/Lighting/ShadowVolumes.cpp) · [局部灯 CPU](../../../../build/d09-t03/final/Cpu-Release.log) · [CSM CPU](../../../../build/d09-t03/final/CsmCpu-Release.log) · [CR-005](../../../../build/d09-t03/final/native-rotated-Release.log) |
| D32 生产及查询 | 原生回放从图内单层权限核对逻辑层 `1、11–15` 到六个物理视图的映射、正向 caster bias、空面清除、MASK 孔、画外投射物、镜像和双面覆盖。核对主 Reverse-Z、聚光外锥、点光半径外、跨面并列及三面角。方向光读回 64,464 个过渡样点，其中 1,010 个能区分相邻级联。 [Forward 图声明](../../../../Engine/Renderer/Paths/Forward.cpp) · [局部查询](../../../../Shaders/Shadow/Local.glsl) · [Release 回放](../../../../build/d09-t03/final/native-local-Release.log) · [Debug 回放](../../../../build/d09-t03/final/native-local-Debug.log) · [CSM](../../../../build/d09-t03/final/native-csm-Release.log) |
| 接受及复用 | 静态两路匹配组只读导入。灯位和 MASK 编辑、前缀后重试以及深度程序换代均核对各 View 的写入和接受。Debug、Release 的新版程序各读回 7,049 个发生规定偏移的 D32 点。 [接受入口](../../../../Runtime/Frame.cpp) · [图与查询回放](../../../../build/d09-t03/final/native-local-Release.log) · [Debug 代切换](../../../../build/d09-t03/final/native-local-Debug.log) |
| 请求及普通显示 | 单个主 View 在 1024 texel 接受聚光，切换点光时报告预算并保留七盏直接光。只改该 View 的边长至 256 后接受六面查询，关闭诊断恢复原 Direct Lighting。实际面板按相机和 View 请求区分历史票据，附加 View 识别已删灯的代，超限布局退回 `SceneAttachmentLimit` 加 `LightUnavailable` 并按 `11、0` 或 `16、0` 结算。 [面板入口](../../../../Editor/Panels/Workspace.cpp) · [同 View 切换](../../../../build/d09-t03/final/native-selection-Release.log) · [面板与离屏](../../../../build/d09-t03/final/native-cr-Release.log) |

核对 6 条新增或修改要求、13 个场景以及投影、子资源和接受边界的设计，签收源码与实施模式一致。Debug、Release 正式构建、隐藏启动、各八个角色的 Shader ABI 校验及同步前 OpenSpec 严格校验通过。原生证据来自 Windows x64 上的 Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129。复用已登记场景与断言，重链当前产品库。面板使用正式类型化入口和 ImGui 文本核对，读回使用图内权限和实际 EventQuery。 [运行入口](../../../../build/d09-t03/final/CheckFinal.ps1) · [源码、产物与日志](../../../../build/d09-t03/final/native-verify.json) · [同步前严格校验](../../../../build/d09-t03/final/pre-sync-validation.json)

<a id="spec-sync"></a>
## 规范同步

已将局部投影准入、D32 层组生产、同投影查询、接受后复用、诊断显示与离屏退路同步到四份主规范。`shadow-planning` 修改 1 条要求，保留其中已有的 4 个场景并增加投影不可表示场景。`shadow-rendering`、`static-forward-rendering` 和 `view-preparation` 分别增加 3、1、1 条要求。逐条比对确认四份规范的 Purpose 和未涉及的要求完整保留，共增加 9 个场景。OpenSpec 1.13.2 严格校验通过 42/42 份主规范，当前 change 也通过严格校验。 [主规范合并核对](../../../../build/d09-t03/final/spec-sync.json) · [主规范严格校验](../../../../build/d09-t03/final/main-validation.json) · [同步后 change 校验](../../../../build/d09-t03/final/post-sync-change-validation.json)

<a id="archive-closeout"></a>
## 归档验收

D09-T03 已按 OpenSpec 1.13.2 归档，全部 15 项任务完成。签收记录保留于本 change 的任务与证据，原问题单留存后按项目模板复位。D09-T04 前置已满足，待立项。 [归档回执](../../../../build/d09-t03/final/archive.json) · [归档核对](../../../../build/d09-t03/final/archive-audit.json) · [D09-T04](../../../../Docs/Developments/D08-D11-RenderPaths.md#d09-t04)
