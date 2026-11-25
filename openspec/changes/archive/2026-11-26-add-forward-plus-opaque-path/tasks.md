# Tasks

## 1. 每 View 的路径输入

- [x] 1.1 为 View 加入带类型的运行时路径请求，默认 Forward，并通过正常主视口和离屏帧入口传入准备。核对现有文档加载、默认调用和两个独立 View 的请求值。以 V1、V3 验证打包启动及实际切换结果。
- [x] 1.2 解析预览、有效阴影诊断及其普通模式退回，让可能受光的 Forward+ View 请求同轮 Froxel 准备。核对 Forward、非受光路线和诊断的实际消费条件。通过 V2 核对相机及布局输入，在 V3 核对两路请求与统计身份。

## 2. 共享求值与产品 Shader 准备

- [x] 2.1 接入共享颜色入口的两种候选选择，用当前渲染投影和坐标合同取得像素及正向视深。登记可达受光组合的产品包、目录与生成接口。用 V1 检查每个新增角色的编译及反射，用 V2 验证真实片元查询。
- [x] 2.2 按角色准备实际布局、Pipeline 和绑定配方，统一冷准备与热重载的活动资源准入，并保留帧前原生准备额度。用 V1 核对全灯和 Froxel 角色、冷包可用性及不兼容角色的拒绝。

## 3. 正常图消费和帧边界确认

- [x] 3.1 在现有计划准备及上传封存之间按实际颜色需求准入 `FroxelUpload`，保留带原因的 Pending/Failed，沿用现有 View 的读回退和状态确认。核对失败时独占写入及配额结算与另一路共享写入。在 V3 运行失效及恢复场景。
- [x] 3.2 用唯一场景上传的写版本组装 Forward+ 颜色读依赖和预检绑定。沿用 `PrepareSurface`、普通或前置深度组合以及天空与显示阶段。向正常接受输出交付实际路径与 Shader 代。按 V2 核对图权限、原生 HDR、反向深度及失去消费者的上传，按 V3 核对已接受身份。

## 4. A 集成自检与交接

- [x] 4.1 完成 V1–V3 的本地集成检查，记录实际运行目标、工作树与包身份、输入配置、设备、原生读回及范围限制。按验证停止条件复用匹配的既有结果。
- [x] 4.2 A 写入本 change 的本地验证和 [签收问题单](../../../../build/d10-t03/final/review-signed.md)，交给维护者调度 B。交接记录能定位到本轮源码、输入和真实消费者。

## 5. 独立审查、整改及复核

- [x] 5.1 B 独立检查范围、实现、验证证据，并按注释标准覆盖受影响第一方文件。记录实际结论及稳定问题编号。[第 1 轮 B 审查](#b-review-1)
- [x] 5.2 A 按问题单整改，验证受影响风险并逐项填写证据。若首轮无整改项，由 A 核实并登记适用情况。 [第 1 轮 A 整改](#a-correction-1) · [第 2 次 A 整改](#a-correction-2) · [第 3 次 A 整改](#a-correction-3) · [第 4 次 A 整改](#a-correction-4) · [第 5 次 A 整改](#a-correction-5) · [第 6 次 A 整改](#a-correction-6) · [第 7 次 A 整改](#a-correction-7)
- [x] 5.3 B 已复核最终版本的问题、回归与注释。CR-001 至 CR-009 全部关闭，D10-T03 已签收。[第 7 次 B 签收记录](#b-recheck-7)

## 6. 最终验证、同步与归档

- [x] 6.1 B 签收后，以确认的源码、包、运行输入和环境完成最终 verify。已登记两配置构建、最终 Release 回放、复用的 B 核对和验证限制。[最终验证](#final-verification)
- [x] 6.2 将三个增量能力同步至各自主规格。已核对完整修改块、旧场景覆盖及 `openspec validate --specs --strict`。[规格同步](#spec-sync)
- [x] 6.3 验收通过后归档 change，更新 D10-T03 和下一工作包状态及链接。保存 B 签收及最终证据后重置当前问题单，核对归档结果。

<a id="verification-scope"></a>
## 验证口径与停止条件

| 编号 | 改变的风险 | 最小有效检查 |
| --- | --- | --- |
| V1 产品角色与代 | 冷包与重载使用不同配方、变体组合缺项、未请求的角色影响旧路径 | 用正常产品目标生成新增角色，核对生成 ABI 和每个角色实际活动的 PerView、灯、阴影及 IBL 资源。原生运行打包输入的 Forward 启动和一次按需准备的 Forward+。以一组不兼容候选验证缺失或误激活 Froxel 资源的拒绝，确认旧代可用及合法修复可发布，包含当前未选择的角色。 |
| V2 不透明子图和真实片元 | 坐标或相机取错、恒走全灯退路、重复图生产者或绑定旧 View、深度或显示语义回归 | 用产品程序和正常 `ForwardFrame` 声明在 OpenGL 读回预曝光 HDR 及反向深度。固定同一场景和画质，组合受光 OPAQUE/MASK、Unlit、有效阴影与 IBL，使用非对称留边和不同 XY/Z 的代表样点。覆盖透视无限远及零近平面正交、正常格与强制 overflow。若已有证据覆盖网格外退路则复用，仍须验证新增片元入口的坐标。用一个能区分所查正常格与全灯退路的原生见证，例如在仅开发使用的受控重放中替换一个已知可贡献的有限记录，另用有效载荷比较两路径输出。检查真实早期深度消费者及常规合并组合。核对各组图读写版本、范围、原生依赖及唯一生产者，移除最后的颜色消费后对应 Froxel 上传应被裁剪。 |
| V3 正常帧与就绪切换 | Pending/Failed 被当作成功、附件/上传压力污染另一 View、路径切换动到 AA 或驻留 | 经正常 Runtime 同一场景在 None 和 FXAA 各验证一次就绪选择，记录实际颜色角色、显示阶段、View 身份及接受票据。以共用 SceneFrame 的混合路径 View 覆盖一路缺项 Pending 和一项真实准入拒绝后恢复。核对未接受候选保留可读旧输出，另一 View 正常工作，Forward 无 Froxel 活动上传，稳定资产及编辑状态不发生与路径有关的重置。检查诊断和关闭的候选不驱动新绑定。 |

先使用 [现有构建入口](../../../../Docs/Build.md)、D10-T02 已验收的列表、退路及上传证据和受影响原生消费者。新增开发探针、替换载荷及样例仅放 `build/d10-t03/`。匹配的输入、设备和目标已有足够证据时复用，记录复用依据。所有改动风险已覆盖、图与读回配对且已登记未运行范围时停止。HDR 按工作附件的表示精度核对有明确预期的稳定样点。阶段差分和成本测量由 D10-T05 验收。 [已有 Froxel 证据](../2026-11-25-add-froxel-overflow-and-graph-uploads/tasks.md#final-verification) · [验证约定](../../../../.agents/skills/physara-verification/SKILL.md)

<a id="a-local-verification"></a>
## A 本地验证与交接

基准为 `v2.0` 的 `895311d9aadf6449da9dda99e110a5fa07a7a952`。本轮 31 个产品输入的指纹为 `e456e7989430f051f3a52308d6f95d2e77d7f05c4c1ed16c963a2262eb6e400c`。交接清单记录逐文件摘要、实际 Debug/Release 包、Release 回放链接输入和命令。 [交接清单](../../../../build/d10-t03/a/handoff.json) · [签收问题单](../../../../build/d10-t03/final/review-signed.md)

| 检查 | 实际结果 | 证据 |
| --- | --- | --- |
| 构建与启动 | 官方 Debug/Release `Physara` 目标成功，两配置各生成 23 个产品包。`--hidden --poll-limit=15` 均退出 0。 | [Debug 构建](../../../../build/d10-t03/a/debug-verified.log) · [Release 构建](../../../../build/d10-t03/a/release-verified.log) · [退出码](../../../../build/d10-t03/a/native-exits.json) |
| V1 产品角色与代 | 六个 Froxel 颜色角色用生成 ABI `43d8f5203317ce730e66d2d19b5d4b12fcd678acbff5dea1d2948ee1d9711016` 编译并核对四个块的活动槽和范围。冷包走全灯和 Froxel 的原生颜色 Pass。两个真实编译的候选分别因缺少 Froxel 块及全灯误激活 Froxel 槽被拒，代 1 保留完整。合法候选发布后按准备额度取得新增需求，并成功出图。 | [原生角色回放](../../../../build/d10-t03/a/roles-accepted.log) |
| V2 不透明子图和真实片元 | 97×81 画布上，零近平面正交采用合并深度，无限远透视采用前置深度。含 OPAQUE、MASK、Unlit、已准入阴影数组与 IBL 的两路径得到逐像素一致的 RGBA16F 和 D32。强制混合 overflow 保持结果。移去正常格的有限候选后，左侧样点的 R 从 `0x283d`、`0x283e` 分别降到 `0x1f91`、`0x1f92`，右侧和 Unlit 见证及深度保持。四个图读版本各自有唯一的前置写入，移除最后颜色消费后无存活 Pass 或上传。HDR 用 RHI staging，D32 在真实提交完成后从导出的 OpenGL 深度纹理读取。 | [原生 HDR 回放](../../../../build/d10-t03/a/hdr-accepted.log) |
| V3 正常帧与就绪切换 | 正常 Runtime 的混合路径在每帧一个原生键的准备额度下出现 Pending。离屏保留旧票据，主 View 继续正常接受。2048×2048 离屏请求因 header 超额被拒并保留原图，缩回有效布局后恢复。None、FXAA 均确认 Forward+，图持有原 Scene 和同版本驻留，诊断依旧沿线性显示及原阴影绑定运行。另检查参数块容量差一字节时创建零缓冲。取消已就绪但未选中的 View 时准确归还四笔和 2,163,308 字节，保留另一路共享图。 | [Runtime 回放](../../../../build/d10-t03/a/runtime-accepted.log) · [配额检查](../../../../build/d10-t03/a/uploads-accepted.log) |

原生回放使用 Windows x64、MSVC 19.44.35228、Intel RaptorLake-S Mobile 和 OpenGL 4.6 驱动 32.0.101.6129。回放链接同版 Release 库及实际 Runtime 对象。开发夹具和不兼容候选存于 `build/d10-t03/a/`。网格外完整灯退路沿用 D10-T02 已验收的共享实现。该轮覆盖上述 Windows OpenGL 环境，D10-T05 的画质差分和多灯成本待对应工作包。 [D10-T02 验收](../2026-11-25-add-froxel-overflow-and-graph-uploads/tasks.md#final-verification)

<a id="b-review-1"></a>
## 第 1 轮 B 审查

第 1 轮未通过。B 在注释前核对了 A 登记的 31 个产品输入、构建与原生验证记录。已用同版 Release 库及产品包在 Intel OpenGL 4.6 重现三个边界，另从接受统计的生产和延迟消费确认一项身份缺失。整改及逐项复核使用[签收问题单](../../../../build/d10-t03/final/review-signed.md)。

| 编号 | 本轮证据及结果 |
|---|---|
| CR-001 · P1 | 共同平移相机、几何和有限点光后，同一像素的两路径 D32 匹配，Forward 红通道为 `0x3544`，Forward+ 降到 `0x18f5`。[HDR 读回](../../../../build/d10-t03/b-review1/precision.log) |
| CR-002 · P1 | 未被请求的深度角色额外读取 set 3 灯表时，候选从代 1 发布至代 2。随后请求前置深度，真实 Pipeline 因缺少该绑定创建失败。[角色回放](../../../../build/d10-t03/b-review1/roles.log) |
| CR-003 · P2 | 离屏准备被拒和就绪后取消两例中，最终主路批次各多占 4 笔、1,052,996 B。四笔在编译图中均被裁剪。[上传配额](../../../../build/d10-t03/b-review1/uploads.log) |
| CR-004 · P2 | 已接受颜色保存了路径，Runtime 复制给不可变场景统计及延迟 GPU 结果的身份缺少该字段。[发布位置](../../../../Runtime/Frame.cpp) · [身份结构](../../../../Editor/Session/FrameStatistics.hpp) |

B 已补充准备接口、片元坐标、绑定与交付流程等 15 个产品文件的注释。源码逐项核对及 12 个受影响颜色包的独立编译表明，B 注释前后的行为输入一致。完整 Debug/Release 产品构建采用 A 的记录。5.2、5.3 与最终验证待相应责任人完成。[注释后源码清单](../../../../build/d10-t03/b-review1/annotated-source.json) · [ABI、反射、SPIR-V 比对](../../../../build/d10-t03/b-review1/annotation-shaders/comparison.txt)

<a id="a-correction-1"></a>
## 第 1 轮 A 整改

从 B 注释后的 31 个产品输入开始，保留相关注释。整改版包含 35 个产品输入，源码指纹 `c5d52017ca93d5c0570af2a1606b11f3abc4516774cea0c27be979bcc771cda8`。构建、复现命令及文件身份见[整改清单](../../../../build/d10-t03/a-correction-1/handoff.json)。该轮结论记录在[签收问题单](../../../../build/d10-t03/final/review-signed.md)，由 B 复核修订后的实现与说明。

| 编号 | A 自检结果 |
| --- | --- |
| CR-001 | 原 B 精度输入原样重链。原点和 −33,554,432 m 共同平移的两路 HDR 与 D32 均逐像素相同，远移样点红通道均为 `0x3544`。V2 保留正交及无限远透视、前置深度、强制 overflow、正常格候选摘除和无有效投影的完整灯退路。[精度反例](../../../../build/d10-t03/a-correction-1/precision.log) · [原生 HDR](../../../../build/d10-t03/a-correction-1/Native-hdr.log) |
| CR-002 | 原 B 深度入口用整改版生成 ABI 重打包。同一错误包在冷准备和未选角色的热重载准入时均被拒，诊断包含 `StaticDepth/lightRows`。旧代及修复后发布的合法代均完成前置深度绘制。重新核对六个 Froxel 角色的活动资源并复跑 PerView 两个拒绝用例。[深度角色回放](../../../../build/d10-t03/a-correction-1/Contracts-roles.log) · [产品角色回归](../../../../build/d10-t03/a-correction-1/Native-roles.log) |
| CR-003 | 原 B 两路拒绝及取消分支均恢复仅主路的 27 笔、1,301,820 B，零死写入。临界探针控制真实批次计数，撤回前拒绝，撤回后保留共享生产者并在 67,108,864 B 和 4,096 笔的上限取得四个实际 Froxel 缓冲。[配额回放](../../../../build/d10-t03/a-correction-1/Contracts-uploads.log) |
| CR-004 | 正常 Runtime 以相同 Shader 代和 AA 接受两路径。当前场景快照记 Forward+ 票据 38，稍后的原生计时仍记 Forward 票据 37。一次视口不可用的请求保持两份记录。混合 View、None/FXAA 和阴影诊断的 V3 正例通过。[Runtime 与计时回放](../../../../build/d10-t03/a-correction-1/Native-runtime.log) |

Windows x64、MSVC 19.44.35228、Intel RaptorLake-S Mobile OpenGL 4.6 驱动 32.0.101.6129。官方 Debug/Release `Physara` 目标、两配置各 23 个产品包、两个打包启动及六次 Release 原生回放通过。新增 `FroxelView` 相机参数沿生成 ABI，块长为 704 B。性能阶段验收仍按 D10-T05。 [Release 构建](../../../../build/d10-t03/a-correction-1/release.log) · [Debug 构建](../../../../build/d10-t03/a-correction-1/debug.log) · [启动结果](../../../../build/d10-t03/a-correction-1/smoke-exits.json)

<a id="b-recheck-1"></a>
## 第 1 次整改的 B 复核

B 核对了基准 `895311d9aadf6449da9dda99e110a5fa07a7a952` 上 A 登记的 35 个产品输入、11 个合同输入、两配置各 23 个包和 62 项构建及验证文件。B 在 Intel OpenGL 4.6 上重跑原精度、深度角色、上传、Runtime 与 HDR 回放，五项退出 0。下表记录补充对照，整改要求留在[签收问题单](../../../../build/d10-t03/final/review-signed.md)。

| 编号 | B 结论与证据 |
|---|---|
| CR-001 · 开放 | 原透视平移例已通过。正交相机高低位编码仍有 0.09375 m 余差时，中心像素覆盖和两路深度相同，有限点光红通道从 Forward 的 `0x2594` 降至 Forward+ 的 `0x18f5`。[正交原点及平移对照](../../../../build/d10-t03/b-recheck1/camera-origin.log) |
| CR-002 · 开放 | 额外灯表已被正确拒绝。同 ABI 的深度顶点缺少必需的 `perView` 时，冷包仍被接受，热重载仍发布代 2，随后前置深度样点从预期 0.8 变为 0。[缺失绑定、换代及读回](../../../../build/d10-t03/b-recheck1/missing-view.log) |
| CR-003 · 开放 | 原私有写入及临界计数对照已通过。仅离屏可见的几何在该路因 header 超额退出后仍占账，剩余额度使本可就绪的主路在申请 2,163,388 B 时失败。[主路单独就绪及双路失败对照](../../../../build/d10-t03/b-recheck1/isolation.log) |
| CR-004 · 通过 | 最新场景确认为 Forward+ 票据 36，延迟 GPU 回执仍为 Forward 票据 35。后续未接受的请求保留了两份快照。[Runtime 及 GPU 回执](../../../../build/d10-t03/b-recheck1/Native-runtime.log) |

B 在五个第一方文件补充眼点精度、角色配方、上传撤回和统计身份的注释，核对 35 个产品输入的非注释行与 A 的整改版一致。完整 Debug/Release 构建及启动复用 A 的记录。CR-001 至 CR-003 交回 A，5.3 与最终验证保持待办。[复核文件及证据身份](../../../../build/d10-t03/b-recheck1/review-inputs.json)

<a id="a-correction-2"></a>
## 第 2 次 A 整改

以 B 第 1 次复核后登记的 35 个产品输入为起点。修订版的 36 个产品输入指纹为 `5088e57f47024584a65b2d25877409aff8c4dc4ef639ec57d9507b65c57186b5`。B 已通过的 CR-004 实现按文件摘要确认无改动，Runtime 回归仍保留较早 GPU 回执的路径身份。 [修订版交接清单](../../../../build/d10-t03/a-correction-2/handoff.json) · [问题单](../../../../build/d10-t03/final/review-signed.md) · [Runtime 回归](../../../../build/d10-t03/a-correction-2/Native-runtime.log)

| 编号 | 本次修正及 A 自检 |
|---|---|
| CR-001 | 把眼点高低位编码余差折算为有限灯相交的距离上界。原正交反例的 0.09375 m 余差仍存在，平移后两路 RGBA16F 红通道为 `0x2594` 且 D32 一致。原透视大平移为 `0x3544`。正常格摘除、混合 overflow 和无效投影回放通过。 [正交用例](../../../../build/d10-t03/a-correction-2/CameraOrigin-hdr.log) · [透视回归](../../../../build/d10-t03/a-correction-2/Precision-hdr.log) · [正常子图回归](../../../../build/d10-t03/a-correction-2/Native-hdr.log) |
| CR-002 | 冷包和热重载均校验深度顶点必需的 View 变换。B 的原错误包报告 `StaticDepth`、`vertex`、`perView`，活动代 1 保持，旧代前置深度仍交付预期 D32≈0.8。额外 `lightRows` 与普通 PerView 角色拒绝回归通过。 [深度角色回放](../../../../build/d10-t03/a-correction-2/Boundary-roles.log) · [六个 Forward+ 产品角色](../../../../build/d10-t03/a-correction-2/Native-roles.log) |
| CR-003 | 冷帧不同可见集中的离屏拒绝撤回孤立几何，主路账面为 1,301,820 B 并正常准入。同一共享生产者由随后被拒的 View 先登记时，主路仍成功提交。合法输入在同帧重试时，未接受的独占写入重新建立，两路正常出图。 [原边界与原生恢复](../../../../build/d10-t03/a-correction-2/Boundary-uploads.log) |

官方 Debug/Release 产品构建和启动均通过。七次 Release 原生回放均退出 0，环境为 Windows x64、MSVC 19.44.35228、Intel OpenGL 4.6 驱动 32.0.101.6129。当前进入 B 独立复核。 [Release 构建](../../../../build/d10-t03/a-correction-2/release.log) · [Debug 构建](../../../../build/d10-t03/a-correction-2/debug.log) · [原生退出码](../../../../build/d10-t03/a-correction-2/native-exits.json) · [启动退出码](../../../../build/d10-t03/a-correction-2/smoke-exits.json)

<a id="b-recheck-2"></a>
## 第 2 次整改的 B 复核

B 核对 A 交接的 36 个产品输入、11 个合同输入、两配置各 23 个包及 69 项构建与验证文件。相对上次 B 记录有 6 个产品文件发生变化。B 在 Intel OpenGL 4.6 上重跑 7 项已登记的 Release 回放，全部退出 0。补充对照中，表面坐标、收尾预算和颜色角色各有一项正确性断言失败。触发条件及整改要求见[签收问题单](../../../../build/d10-t03/final/review-signed.md)。

| 编号 | 第 2 次 B 结论 |
|---|---|
| CR-001 · 开放 | 原 0.09375 m 眼点编码余差反例通过。眼点编码完全精确、片元作差发生舍入时，平移后 Forward / Forward+ 的 R 仍为 `0x225c` / `0x18f5`。同一输入强制全灯退路后 HDR 和 D32 均逐像素恢复。[表面作差对照](../../../../build/d10-t03/b-recheck2/surface-delta.log) |
| CR-002 · 通过 | 深度角色缺失顶点 `perView`、额外读取片元灯表两类错误均在发布前被拒绝，旧代的前置深度读回正确。[深度角色回放](../../../../build/d10-t03/b-recheck2/Boundary-roles.log) |
| CR-003 · 开放 | 上轮的孤立几何回收、共享生产者保留和同帧重试均通过。两路网格都有效、仅离屏路独立超限时，按 Runtime 顺序调用正常规划与收尾入口仍使两路均因上传配额失败。单独主路可就绪。[三个准入对照](../../../../build/d10-t03/b-recheck2/finish-budget.log) |
| CR-004 · 保持通过 | 接受统计及计时回执相关实现与上次 B 记录一致。本轮当前场景为 Forward+ 票据 34，迟到的 GPU 回执为 Forward 票据 33。[Runtime 回放](../../../../build/d10-t03/b-recheck2/Native-runtime.log) |
| CR-005 · 新增 P1 | 缺少基色图像及采样器的 Forward+ 颜色包在冷启动的程序准备和未选角色的热发布中均被接受。正常两路径从 MASK 孔读得后方 D32=0.625，错误代将其覆盖为 0.658。[颜色角色、换代及覆盖读回](../../../../build/d10-t03/b-recheck2/mask-role.log) |

B 补充 5 个 C++ 文件的注释，36 个产品输入的非注释行与本次 A 交接一致。完整 Debug/Release 产品构建及启动复用对应记录。CR-001、CR-003 和 CR-005 交 A 整改，5.3 与最终验证保持待办。[复核版本和证据身份](../../../../build/d10-t03/b-recheck2/review-inputs.json)

<a id="a-correction-3"></a>
## 第 3 次 A 整改

从 B 第 2 次复核后的 36 个产品输入开始。修订版包含 38 个产品输入，源码指纹 `12dd796cfc4f84ad1ede9d668e7935c4f3fd1402fc7fb829ba00ae80ff08564a`。B 已通过的 CR-002、CR-004 均复跑。对应问题的状态与证据留在[签收问题单](../../../../build/d10-t03/final/review-signed.md)，新版本的源码、包和回放输入见[整改清单](../../../../build/d10-t03/a-correction-3/handoff.json)。

| 编号 | 本次修正及 A 自检 |
|---|---|
| CR-001 | 用相对作差余量界定片元查询区间，跨格以完整列表求值。原 B 夹具原样重链，零编码余差的平移样点恢复为 Forward 与 Forward+ 的 R `0x225c`，和强制退路的整幅 HDR、D32 相同。原两组眼点及正常格摘除、overflow 回归通过。 [新反例](../../../../build/d10-t03/a-correction-3/SurfaceDelta-hdr.log) · [正常格回归](../../../../build/d10-t03/a-correction-3/Native-hdr.log) |
| CR-003 | 固定 Froxel 额度前移到 View 规划，收尾时转成等额的实际写入。单主和双路主 View 的预算均为 3,465,208 B、31 笔，原离屏输入在单独和并行规划中均超限。实际提交的两张主路图 HDR、D32 一致，共享生产者保留及同帧恢复通过。 [双 View 与读回](../../../../build/d10-t03/a-correction-3/Delivery-uploads.log) |
| CR-005 | 冷包和所有热候选共享受光颜色的片元材质配方。B 的错误包在两处拒绝并报告 `StaticForwardPlus / fragment / baseColorImage`，旧代保留。恢复合法包后，MASK 孔洞的 HDR、覆盖和 D32 与合法 Forward 匹配，样点 D32 为 0.625。 [颜色角色回放](../../../../build/d10-t03/a-correction-3/Delivery-roles.log) |

Windows x64、MSVC 19.44.35228、Intel RaptorLake-S Mobile OpenGL 4.6 驱动 32.0.101.6129。官方 Debug/Release 构建与打包启动通过，十次 Release 原生回放退出 0。CR-002 的深度输入及 CR-004 的延迟统计回执表现与上一轮接受行为一致。 [构建](../../../../build/d10-t03/a-correction-3/release.log) · [Debug 构建](../../../../build/d10-t03/a-correction-3/debug.log) · [回放与启动身份](../../../../build/d10-t03/a-correction-3/handoff.json)

<a id="b-recheck-3"></a>
## 第 3 次整改的 B 复核

B 在注释前核对 38 个产品输入、11 个合同输入、两配置各 23 个包及 83 项构建与验证文件。Intel OpenGL 4.6 上的 10 项登记回放全部通过。补充对照确认 3 个仍影响交付的行为，复现条件与整改要求已写入[签收问题单](../../../../build/d10-t03/final/review-signed.md)。

| 编号 | 第 3 次 B 结论 |
|---|---|
| CR-001 · 通过 | 透视、正交眼点编码、片元作差三个原例均恢复；零眼点余差的平移例两路 R 均为 `0x225c`，逐像素匹配强制退路，正常格摘除仍有辨别力。[表面作差](../../../../build/d10-t03/b-recheck3/SurfaceDelta-hdr.log) · [正常格回归](../../../../build/d10-t03/b-recheck3/Native-hdr.log) |
| CR-002、004 · 保持通过 | 深度必需项与额外活动项均被正确拒绝。本次正常场景票据 34 保留 Forward+，延迟 GPU 票据 33 保留 Forward。[深度角色](../../../../build/d10-t03/b-recheck3/Boundary-roles.log) · [计时回执](../../../../build/d10-t03/b-recheck3/Native-runtime.log) |
| CR-003 · 开放 | 原主路在单路、双路下的 31 笔、3,465,208 B 及提交图像已匹配。反向对照中，主路合法 Shader Pending 或主路包确定失败时，临时预留先使离屏路超额。归还后该路仍被拒，单独离屏则可就绪。对照经公开准备入口按 Runtime 顺序执行。[原提交对照](../../../../build/d10-t03/b-recheck3/Delivery-uploads.log) · [反向准备对照](../../../../build/d10-t03/b-recheck3/late-main.log) |
| CR-005 · 通过 | 对缺失基色资源的颜色片元，冷包和未选角色的热候选都被拒绝，合法旧代与恢复代保有 MASK 孔洞及相同的 HDR、D32。[角色读回](../../../../build/d10-t03/b-recheck3/Delivery-roles.log) |
| CR-006 · 新增 P1 | 顶点 set 2 缺少 `materialRows` 的颜色角色能冷准备并发布。旋转 90° 的法线 UV 对照中，深度一致，HDR 红通道从 `0x2002` 变为 `0x23ee`。[顶点角色与 HDR](../../../../build/d10-t03/b-recheck3/vertex-material.log) |
| CR-007 · 新增 P2 | 距离至少 0.625 m、范围 0.60 m 的点光在大坐标下给完整灯循环带来非零贡献。关闭该灯后红通道从 `0x20d2` 恢复到正常格的 `0x18f5`，覆盖和深度相同。[关灯与强制退路对照](../../../../build/d10-t03/b-recheck3/light-error.log) |

B 在 5 个第一方文件完善数值和配额注释，核对 38 个文件的非注释行及 17 个受影响角色的 ABI、反射和 SPIR-V。完整 Debug/Release 产品构建与启动沿用 A 的匹配记录。5.3 与最终验证保持待办。[B 复核清单](../../../../build/d10-t03/b-recheck3/review-inputs.json) · [17 个角色比对](../../../../build/d10-t03/b-recheck3/annotation-shaders/comparison.txt)

<a id="a-correction-4"></a>
## 第 4 次 A 整改

从 B 第 3 次复核后登记的 38 个产品输入开始。修订版包含 39 个产品输入，源码指纹 `b1ad8e1cc3313fcb11d6bd4fc23d2a5bf69533d0640174e74698506e02125c41`。测试覆盖本轮三个开放项及受影响的既有行为。 [签收问题单](../../../../build/d10-t03/final/review-signed.md) · [版本、重放命令及摘要](../../../../build/d10-t03/a-correction-4/handoff.json)

| 编号 | 本次修正及 A 自检 |
|---|---|
| CR-003 | 主路收尾归还配额后，SceneUploadLimit 失败的离屏候选用原工作集同帧重试。真实 Runtime 的合法 Pending 与错误包两种对照均提交离屏，像素分别与独立离屏一致。双路轮次各启动一个原生键，停用重试的开发对照仍失败。原反向准入变化、孤立写入回收和共享生产者保留继续通过。 [离屏存活回放](../../../../build/d10-t03/a-correction-4/Isolation-uploads.log) · [停用重试对照](../../../../build/d10-t03/a-correction-4/IsolationWithoutRetry-uploads.log) · [主路存活回放](../../../../build/d10-t03/a-correction-4/Delivery-uploads.log) |
| CR-006 | 受光颜色的顶点 `materialRows` 纳入冷包和未选角色热重载准入，17 个合法几何角色逐一通过。同 ABI 错误包在两处被拒，活动代保持为 1。旋转法线 UV 的合法 Forward、旧代 Forward+ 与恢复代在同一像素均读得 `0x2002`，HDR 和 D32 匹配。 [顶点角色回放](../../../../build/d10-t03/a-correction-4/Isolation-roles.log) |
| CR-007 | 逐灯测距使用具有有符号余差补偿的米制位移。原 0.625 m 场景在原点与大平移下使用 0.60 m 范围时，两条路径及强制完整列表一致，原样点与关灯均为 `0x18f5`。平移后的 0.625 m 边界同为零贡献，0.70 m 范围有有效贡献 `0x1db4`，两路 HDR 和 D32 匹配。 [距离窗与关灯回放](../../../../build/d10-t03/a-correction-4/LightDelta-hdr.log) |

Windows x64、MSVC 19.44.35228、Intel RaptorLake-S Mobile OpenGL 4.6 驱动 32.0.101.6129。Debug/Release 产品构建及启动通过，九项 Release 原生回放退出 0。CR-001 正常格与强制退路、CR-002 深度、CR-004 统计、CR-005 MASK 的相关回归均通过。 [Release 构建](../../../../build/d10-t03/a-correction-4/release.log) · [Debug 构建](../../../../build/d10-t03/a-correction-4/debug.log) · [运行结果](../../../../build/d10-t03/a-correction-4/handoff.json)

<a id="b-recheck-4"></a>
## 第 4 次整改的 B 复核

B 核对了本次 A 交接的 39 个产品输入、12 个合同输入、两配置各 23 个包及 85 项构建与验证文件。9 项登记的 Release 原生回放在 Intel OpenGL 4.6 上全部退出 0；停用离屏重试的负对照在预期断言处退出 1。两项补充对照的输入、原因与验收条件留在[签收问题单](../../../../build/d10-t03/final/review-signed.md)。

| 编号 | B 结论与证据 |
|---|---|
| CR-003 · 通过 | 主路 Pending、主路包确定失败时，双路的离屏票据分别为 15、24，与单路票据 10、21 的显示像素匹配。两种重试均未突破原生额度。[真实 Runtime](../../../../build/d10-t03/b-recheck4/Isolation-uploads.log) · [停用重试对照](../../../../build/d10-t03/b-recheck4/IsolationWithoutRetry-uploads.log) |
| CR-006 · 通过 | 受光顶点缺少 `materialRows` 在冷、热两处被拒，旧代保留；合法角色和恢复代的旋转法线样点均为 `0x2002`，深度与 HDR 保持匹配。[17 个角色与读回](../../../../build/d10-t03/b-recheck4/Isolation-roles.log) |
| CR-007 · 开放 | 原距离窗外侧、边界和内侧对照通过。给原大坐标夹具增加 0.0625 m 共同平移，源灯的 0.625 m 轴向距离在灯位上传后只剩 0.5625 m。范围为 0.60 m 时，两路开灯 R 均为 `0x1cf5`，仅关该灯降为 `0x18f5`。[原范围控制](../../../../build/d10-t03/b-recheck4/LightDelta-hdr.log) · [编码与关灯读回](../../../../build/d10-t03/b-recheck4/packed-light.log) |
| CR-008 · 新增 P2 | 双路离屏提交成功后，主路实际仍 ShaderPending，准备快照却为 Active，角色及原因均为空。调用相同的无新计划预热入口，单主路新增 1 个原生键，双路新增 0 个。[快照和预热对照](../../../../build/d10-t03/b-recheck4/retry-status.log) |

CR-001、002、004、005 保持通过。B 在 4 个文件完善注释，核对全部产品输入的非注释行及 17 个受影响 Release 角色的 ABI、反射和模块字节。完整产品 Debug/Release 构建与启动复用 A 的交接记录。5.3 与最终验证待办。[B 复核清单](../../../../build/d10-t03/b-recheck4/review-inputs.json) · [注释等价检查](../../../../build/d10-t03/b-recheck4/annotation-shaders/comparison.txt)

<a id="a-correction-5"></a>
## 第 5 次 A 整改与本地自检

以 B 第 4 次复核的 39 个产品输入为起点，逐文件摘要全部匹配。修改其中 7 个文件后，当前源码指纹为 `64d549647ac987c436bb35234a6c5117668a6e71a6c988311c3a9ef30ae0951e`。构建、产品包、开发对照、运行命令及对应摘要随同交接。 [签收问题单](../../../../build/d10-t03/final/review-signed.md) · [版本和证据清单](../../../../build/d10-t03/a-correction-5/handoff.json)

| 问题 | 修正与自检 |
|---|---|
| CR-007 | 灯位高位及尾数保存源有效位，Shader 在补偿作差时合入解出的余量。原点与 B 的两种平移均保存 0.625 m 轴向间距。范围外和边界样点为关灯值 `0x18f5`，0.70 m 范围内为 `0x1db4`。正常格与完整列表 HDR、D32 匹配。编码拒绝经正常 Runtime 进入 SafeUi，旧票据 29 的颜色可读，恢复后接受票据 31。 [B 原样回放](../../../../build/d10-t03/a-correction-5/PackedLight-hdr.log) · [三位置原生读回](../../../../build/d10-t03/a-correction-5/PositionDelivery-hdr.log) · [拒绝与恢复](../../../../build/d10-t03/a-correction-5/PositionDelivery-runtime.log) |
| CR-008 | 同帧重试合并两路键，继续使用活动代和本帧额度。离屏票据 15、24 分别与单路的显示像素相同。合法冷包的双路轮次只启动 1 个原生键，快照保留 `StaticForwardPlus` 及预算等待，无新计划时成功补齐 1 个键。错误包分支继续报告原程序失败。 [两路快照、预热与读回](../../../../build/d10-t03/a-correction-5/RetryStatus-uploads.log) |

新生成接口保有 64 字节灯行、16,384 行及 1 MiB 灯表。Debug、Release 的 StaticForward ABI 一致，均生成 23 个产品包。旧 ABI 对照按预期退出 1。Debug/Release 产品构建和启动通过。Windows x64、MSVC 19.44.35228、Intel RaptorLake-S Mobile OpenGL 4.6 驱动 32.0.101.6129 的 11 项 Release 原生回放全部退出 0。回归覆盖正常格摘除、完整列表退路、双向预算压力、统计所属票据、深度和材质角色的冷包及热候选。 [Release 构建](../../../../build/d10-t03/a-correction-5/release.log) · [Debug 构建](../../../../build/d10-t03/a-correction-5/debug.log) · [旧 ABI 对照](../../../../build/d10-t03/a-correction-5/old-abi-rejection.log) · [全部回放退出状态](../../../../build/d10-t03/a-correction-5/handoff.json)

<a id="b-recheck-5"></a>
## 第 5 次整改的 B 复核

B 核对了本次交接的 39 个产品输入、12 个合同输入、Debug/Release 各 23 个包及 117 项构建与验证文件。11 项登记的 Release 回放均在 Intel OpenGL 4.6 上退出 0，旧 ABI 包检查按预期退出 1。复现条件与验收要求保存在[签收问题单](../../../../build/d10-t03/final/review-signed.md)。

| 编号 | B 结论与证据 |
|---|---|
| CR-007 · 通过 | 原点、两个正向大平移及 B 补测的负向大平移均保留 0.625 m 轴向灯距。范围 0.60 m、0.625 m 的中心 R 为关灯值 `0x18f5`，0.70 m 时 R 为 `0x1db4`，两路 HDR 与 D32 匹配。不可编码时保留票据 43，合法输入恢复后接受票据 45。[实际灯行](../../../../build/d10-t03/b-recheck5/PositionDelivery-hdr.log) · [负坐标读回](../../../../build/d10-t03/b-recheck5/signed-position.log) · [拒绝与恢复](../../../../build/d10-t03/b-recheck5/PositionDelivery-runtime.log) |
| CR-008 · 通过 | 单键额度下，离屏票据 15 像素匹配且本帧只启动 1 个原生键。联合需求数为 5，快照保留主路 Pending 角色与预算原因。无新计划时新增 1 个 Pipeline，坏包分支保留主路 Failed 并提交离屏票据 24。[真实 Runtime 回放](../../../../build/d10-t03/b-recheck5/RetryStatus-uploads.log) |
| CR-009 · 新增 P2 | 主路坏包、离屏合法但颜色 Pipeline 尚未准备时，正常产品首帧只启动 3/64 项，离屏票据为 0，次帧才提交匹配像素。单离屏首帧只需 2 项。仅改变开发夹具的补备选择范围，双路首帧启动 4 项即提交相同输出。整改须同时保留 CR-008 的全帧需求与诊断。[产品](../../../../build/d10-t03/b-recheck5/cold-retry.log) · [调度对照](../../../../build/d10-t03/b-recheck5/cold-subset.log) |

CR-001 至 CR-006 保持通过。B 完善 4 个文件的注释，核对全部 39 个产品输入及 12 个受影响颜色角色的 ABI、反射和模块字节。完整产品构建与启动沿用 A 的匹配记录。当前仅 CR-009 开放，5.3 与最终验证保持待办。[B 复核与文件摘要](../../../../build/d10-t03/b-recheck5/review-inputs.json) · [注释后包比对](../../../../build/d10-t03/b-recheck5/annotation-shaders/comparison.txt)

<a id="a-correction-6"></a>
## 第 6 次 A 整改与本地自检

整改从 B 登记的 39 个产品输入及 26 项复核材料开始，逐项 SHA-256 匹配。当前 40 个产品输入中有 5 个相对 B 版本新增或变化，源码指纹为 `e3151ec7ba76cba675df1f8eba5b8361dce9a889e139002496f032cd07ee9622`。 [签收问题单](../../../../build/d10-t03/final/review-signed.md) · [版本和证据清单](../../../../build/d10-t03/a-correction-6/handoff.json)

CR-009 通过活动代的角色级包拒绝保留首因和全帧需求，在剩余额度内继续其它角色。原生创建及资源失败的中止边界沿用既有合同。同一 B 夹具、15,300,000 个索引和 64 项额度下，单离屏首帧提交票据 19，双路首帧提交票据 37，显示像素逐字节相同。双路需求为 6，4 次尝试中创建 3 个 Pipeline，`StaticForwardPlus` 主路保留缺少 `baseColorImage` 的失败诊断。 [B 原失败回放](../../../../build/d10-t03/b-recheck5/cold-retry.log) · [整改后同输入回放](../../../../build/d10-t03/a-correction-6/ColdRetry-uploads.log)

CR-008 的合法冷包双路仍在单键额度内提交离屏票据 15，保留 5 个需求和主路预算等待。无新计划时再创建 1 个原生键并转为 Active，坏包分支在原诊断有效时正常提交离屏。受影响的深度及颜色角色准入、共享生产者撤回和恢复、已接受场景与延迟 GPU 回执身份均已回放。 [需求与预热](../../../../build/d10-t03/a-correction-6/RetryStatus-uploads.log) · [深度角色](../../../../build/d10-t03/a-correction-6/Boundary-roles.log) · [颜色角色](../../../../build/d10-t03/a-correction-6/Delivery-roles.log) · [已接受票据](../../../../build/d10-t03/a-correction-6/Native-runtime.log)

Debug/Release 产品构建和打包启动通过，各有 23 个产品包。B 的 Shader 注释纳入常规重打包，StaticForward ABI 与上一轮一致。Windows x64、MSVC 19.44.35228、Intel RaptorLake-S Mobile OpenGL 4.6 驱动 32.0.101.6129 的 7 项 Release 原生回放均退出 0。5.3 与最终验证待 B 确认后推进。 [Release 构建](../../../../build/d10-t03/a-correction-6/release.log) · [Debug 构建](../../../../build/d10-t03/a-correction-6/debug.log) · [回放退出状态](../../../../build/d10-t03/a-correction-6/native-exits.json)

<a id="b-recheck-6"></a>
## 第 6 次整改的 B 复核

B 核对本次 A 交接的 40 个产品输入、12 个合同输入、Debug/Release 各 23 个包及 73 项构建与验证文件。当前基准为 `895311d9aadf6449da9dda99e110a5fa07a7a952`，注释前源码指纹为 `e3151ec7ba76cba675df1f8eba5b8361dce9a889e139002496f032cd07ee9622`。7 项登记的 Release 原生回放在 Intel OpenGL 4.6 上全部退出 0。[B 复核清单](../../../../build/d10-t03/b-recheck6/review-inputs.json)

| CR-009 的对照 | B 结果 |
|---|---|
| 原主路活动绑定缺失 | 双路在首帧取得离屏票据 33，像素匹配单离屏票据 17。保留 6 个不同需求和主路失败原因，4 次尝试中创建 3 个 Pipeline。CR-008 的单键额度、Pending 原因及后续预热回归通过。[原夹具回放](../../../../build/d10-t03/b-recheck6/ColdRetry-uploads.log) · [需求与预算](../../../../build/d10-t03/b-recheck6/RetryStatus-uploads.log) |
| 同一坏包的文件长度边界 | 64 MiB 时解码拒绝包内长度，双路首帧成功。仅追加一个零字节后，读取器提前以 `InvalidInput` 拒绝。双路首帧只启动 3/64 项、离屏票据为 0，下一帧才提交与单路匹配的像素。[界内坏包](../../../../build/d10-t03/b-recheck6/bad-length.log) · [超限一字节](../../../../build/d10-t03/b-recheck6/over-limit.log) |
| 主路包不存在 | 打开文件返回 `IoFailure`。单离屏首帧可交付，双路离屏首帧 Pending、次帧成功。读包首因沿单键中止路径传播。[缺包的 Runtime 回放](../../../../build/d10-t03/b-recheck6/missing.log) |

B 在三个 C++ 文件完善注释，核对全部产品输入的非注释行与本次交接一致。完整产品构建及启动复用 A 记录。CR-009 保留原编号交 A 处理读取阶段的角色隔离，5.3 和最终验证保持待办。[具体位置及整改要求](../../../../build/d10-t03/final/review-signed.md)

<a id="a-correction-7"></a>
## 第 7 次 A 整改与本地自检

核对 B 的 40 个产品输入、16 项复核材料及原 46 个产品包的摘要后开始整改。更新 4 个产品文件，当前源码指纹为 `194e74f2757c15a9354c8a73975a54c1d54449c74609fee76bc97baa2de241ad`。 [签收问题单](../../../../build/d10-t03/final/review-signed.md) · [待复核版本和证据清单](../../../../build/d10-t03/a-correction-7/handoff.json)

| 对照 | 本轮结果 |
|---|---|
| 64 MiB 坏包、超限一个字节、文件缺失 | 读取器仍分别报告 `CorruptPackage`、`InvalidInput`、`IoFailure`。读包拒绝以角色作用范围交给帧前准备器。三个双路首帧取得离屏票据 34、34、33，分别与单离屏票据 16 的像素匹配。各轮仍显示主路失败首因及 6 个需求，4 次尝试创建 3 个 Pipeline，限额 64。 [界内坏包](../../../../build/d10-t03/a-correction-7/bad-length.log) · [超限](../../../../build/d10-t03/a-correction-7/over-limit.log) · [缺失](../../../../build/d10-t03/a-correction-7/missing.log) |
| 缺失包恢复 | 主路退出失败角色并取得 Forward 票据 36，补齐合法包后重新请求 Forward+。同一个 Renderer 在活动代 1 上准备 2 个键，取得 Forward+ 票据 37，主视口显示像素逐字节匹配。 [恢复回放](../../../../build/d10-t03/a-correction-7/restored.log) |
| CR-009 原接口拒绝及 CR-008 | 同 ABI 的坏包仍在角色准入定位 `baseColorImage`，独立离屏继续首帧提交。单键额度下，联合快照保留 5 个键和主路预算等待，下一次无新计划预热创建 1 个待办键。确定失败分支的离屏提交及诊断也通过。 [接口拒绝](../../../../build/d10-t03/a-correction-7/interface.log) · [预算与预热](../../../../build/d10-t03/a-correction-7/RetryStatus-uploads.log) |

Debug/Release 构建和打包启动通过，两套各 23 个产品包的摘要与上一轮一致。Windows x64、MSVC 19.44.35228、Intel RaptorLake-S Mobile OpenGL 4.6 驱动 32.0.101.6129 的 11 项 Release 原生回放均退出 0，含深度和材质角色冷热拒绝、双路上传隔离及已接受场景统计回归。资源不足、系统及原生设备错误沿用现有中止路径，本轮的原生对照覆盖实际文件与接口拒绝。5.3 待 B 复核。 [Release 构建](../../../../build/d10-t03/a-correction-7/release.log) · [Debug 构建](../../../../build/d10-t03/a-correction-7/debug.log) · [完整回放记录](../../../../build/d10-t03/a-correction-7/handoff.json)

<a id="b-recheck-7"></a>
## 第 7 次 B 复核与签收

B 签收 D10-T03 的渲染器实现及注释。CR-001 至 CR-008 保持通过，CR-009 本轮关闭，开放问题为零。[签收问题单](../../../../build/d10-t03/final/review-signed.md)

| CR-009 输入 | B 在正常 Runtime 中的观测 |
|---|---|
| 64 MiB 坏包、仅超限一字节、缺失包 | 三组单离屏均在首帧提交票据 17。对应双路首帧票据为 35、33、34，显示像素分别与单路逐字节相同。联合需求 6，尝试 4/64 项，创建 3 个 Pipeline，主路保留对应失败首因。[坏包](../../../../build/d10-t03/b-recheck7/bad-length.log) · [超限](../../../../build/d10-t03/b-recheck7/over-limit.log) · [缺包](../../../../build/d10-t03/b-recheck7/missing.log) |
| 已解码包的活动接口不符 | 同样隔离到主角色。双路首帧票据 33 与单路票据 16 的显示像素匹配，主路保留缺失 `baseColorImage` 的诊断。[接口拒绝](../../../../build/d10-t03/b-recheck7/interface.log) |
| 补齐合法包后重新请求 | 退出失败角色后接受 Forward 票据 35，原路径补包并重选后接受 Forward+ 票据 36。活动代保持 1，创建 2 个键，主视口像素相同。[角色恢复](../../../../build/d10-t03/b-recheck7/restored.log) |

签收基准为 `v2.0` 的 `895311d9aadf6449da9dda99e110a5fa07a7a952`。A 的受测源码指纹为 `194e74f2757c15a9354c8a73975a54c1d54449c74609fee76bc97baa2de241ad`。B 在 `ForwardShaderKeys.hpp` 澄清两处注释，核对可精确还原 A 的输入。**签收源码指纹为 `a99b69c906de0b44370ed39a313c6ea053c96fa85e3a3d26a98559229bad538f`**，覆盖 32 个已跟踪及 8 个未跟踪产品文件。[B 签收清单](../../../../build/d10-t03/b-recheck7/review-inputs.json) · [对应构建与包摘要](../../../../build/d10-t03/a-correction-7/handoff.json)

B 在 Windows x64、Intel OpenGL 4.6 驱动 32.0.101.6129 上重跑本轮 11 项 Release 回放，并加跑同版 HDR/D32 回归，12 项全部通过。Debug/Release 产品构建与启动沿用经摘要核对的 A 记录。系统、内存及原生设备故障分支通过源码复核，本轮未注入这些故障。后续 6.1–6.3 由 A 接续。[单键预算与预热](../../../../build/d10-t03/b-recheck7/RetryStatus-uploads.log) · [HDR/D32 回归](../../../../build/d10-t03/b-recheck7/Native-hdr.log) · [已接受票据](../../../../build/d10-t03/b-recheck7/Native-runtime.log)

<a id="final-verification"></a>
## A 最终验证 · 2026-11-26

签收源码为 `a99b69c906de0b44370ed39a313c6ea053c96fa85e3a3d26a98559229bad538f`。基准、暂存区、40 个产品文件、12 项合同文件、两配置各 23 个产品包、81 项 A 文件和 13 项 B 回放产物均已核对。仅还原 B 在 `ForwardShaderKeys.hpp` 补充的两处注释即可逐字节重现 A 的头文件。签收问题单按原字节留存，原件链接以原 `Docs/CodeReview.md` 为起点。 [签收身份](../../../../build/d10-t03/final/signed-inputs.json) · [签收问题单](../../../../build/d10-t03/final/review-signed.md) · [B 签收结论](#b-recheck-7)

从签收源码重建 Debug 和 Release，`--hidden --poll-limit=15` 均退出 0。两套 46 个包的摘要与 B 一致。用该次 Release 产品库与 Runtime 对象重链，最终 8 项原生回放通过。另复用 6 项身份相符的 B 回放。Windows x64、MSVC 19.44.35228、Intel RaptorLake-S Mobile OpenGL 4.6 驱动 32.0.101.6129。 [Release 构建](../../../../build/d10-t03/final/build-Release.log) · [Debug 构建](../../../../build/d10-t03/final/build-Debug.log) · [回放命令及摘要](../../../../build/d10-t03/final/native-verify.json) · [同步前严格校验](../../../../build/d10-t03/final/pre-sync-validation.json)

| 增量要求 | 签收版本和验证依据 |
|---|---|
| 选择当前不透明灯方案 · 4 个场景 | 两条路径共用表面、深度及逐灯 BRDF。正交与无限远透视的 HDR、D32 对照通过，正常格候选摘除有可辨变化，强制溢出及无有效投影保有完整灯结果。大坐标分箱及作差的原反例和同格之外使用完整列表的记录随签收保留。 [片元入口](../../../../Shaders/Passes/Forward/Color.glsl) · [稳定选格](../../../../Shaders/Lighting/FroxelLighting.glsl) · [最终 HDR/D32](../../../../build/d10-t03/final/Native-hdr.log) · [大坐标作差](../../../../build/d10-t03/b-recheck4/SurfaceDelta-hdr.log) |
| 消费同一 View 的图上传 · 4 个场景 | 四个块由拥有型批次和独立版本提供，颜色消费者消失后回放得到 0 个 Pass 和 0 笔对应上传。两向配额压力、失败路撤回、共享生产者保活及可交付离屏同帧恢复均有原生读回。 [图读声明](../../../../Engine/Renderer/Lighting/FroxelGpu.cpp) · [View 配额](../../../../Engine/Renderer/Paths/Forward.cpp) · [相反方向压力](../../../../build/d10-t03/b-recheck7/Delivery-uploads.log) · [离屏同帧读回](../../../../build/d10-t03/final/ReadRejection-over-limit.log) |
| 当轮预曝光的受光输出 · 3 个场景 | 共享预曝光和 MASK 覆盖由现有场景与 HDR/D32 对照确认。最终负坐标平移读回中，轴向间距 0.625 m，范围 0.60 m 和 0.625 m 的红通道与关灯相同，0.70 m 的红通道为 `0x1db4`。 [逐灯求值](../../../../Shaders/Lighting/Punctual.glsl) · [位移补偿](../../../../Shaders/Geometry/LightingPosition.glsl) · [范围与深度](../../../../build/d10-t03/final/SignedPosition-hdr.log) · [MASK 覆盖](../../../../build/d10-t03/b-recheck7/Delivery-roles.log) |
| 局部阴影诊断 · 2 个场景 | 所选点光与聚光的诊断请求、预算拒绝及退出诊断的接受规则沿用 D09-T03 的已验收对照。核对该 View 的诊断入口及有效选择，再以本轮共享 HDR 与 Runtime 路径确认普通受光和诊断组装的衔接。 [View 准备](../../../../Engine/Renderer/Prepare/ViewPreparation.cpp) · [已有局部可见性验收](../2026-11-24-add-spot-point-shadows/tasks.md#final-verification) · [本轮 Runtime](../../../../build/d10-t03/final/Native-runtime.log) |
| 每 View 准备并确认方案 · 3 个场景 | 真实 Runtime 完成 None/FXAA、待备和拒绝后恢复及同场景两路独立接受。最终场景票据 30 表示 Forward+，延迟 GPU 回执票据 29 表示 Forward。 [路径请求](../../../../Engine/Renderer/View.cpp) · [提交确认](../../../../Runtime/Frame.cpp) · [最终 Runtime](../../../../build/d10-t03/final/Native-runtime.log) |
| Shader 活动角色准入 · 9 个场景 | 当前 ABI 编译的两份对照分别缺少 Froxel 必需项及在全灯角色激活额外 Froxel 绑定，冷包和热候选均被拒，合法代可发布。深度、MASK 和顶点材质配方保有独立诊断。单键额度、无新计划预热、同帧读包隔离和同代恢复已核对。 [准入与调度](../../../../Engine/Renderer/Paths/ForwardShaderPreparation.cpp) · [当前 ABI 角色对照](../../../../build/d10-t03/final/Native-roles.log) · [预算和预热](../../../../build/d10-t03/final/RetryStatus-uploads.log) · [恢复](../../../../build/d10-t03/final/ReadRejection-restored.log) |

核对 6 条增量要求和 25 个场景。实现保持现有 Renderer 和 Scene 所有权、角色级准入、冻结 Shader 代与图依赖合同。`RetryStatus roles` 首次在读取合法 `StaticForwardPlusPhysicalShadows` 时退出 1，夹具只记录角色名。相同二进制和包摘要复跑通过，该包独立校验通过，初次失败原因未确定。系统、内存或原生设备故障沿用源码复核。 [首次日志](../../../../build/d10-t03/final/RetryStatus-roles-first.log) · [同输入复跑](../../../../build/d10-t03/final/RetryStatus-roles-retry.log) · [单包校验](../../../../build/d10-t03/final/verify-physical-shadows-package.log)

<a id="spec-sync"></a>
## 规格同步

将当前增量完整合入三个既有主规格。`static-forward-rendering` 新增两条要求并更新两条，`view-preparation` 和 `shader-generation-reload` 各新增一条。全量核对原 Purpose、未涉及的 19 条要求、原有场景及合并后的 25 个增量场景。OpenSpec 1.13.2 严格校验通过 43/43 份主规格与当前 change。 [不透明渲染](../../../specs/static-forward-rendering/spec.md) · [View 准备](../../../specs/view-preparation/spec.md) · [Shader 代](../../../specs/shader-generation-reload/spec.md) · [合并核对](../../../../build/d10-t03/final/spec-sync.json) · [主规格校验](../../../../build/d10-t03/final/main-validation.json) · [change 校验](../../../../build/d10-t03/final/post-sync-change-validation.json)

<a id="archive-closeout"></a>
## 归档与阶段推进

D10-T03 已验收并归档，开发计划推进至 D10-T04。B 的九项结论、注释验收、最终验证、规格同步与归档记录均已留存。归档确认后按模板重置当前问题单。 [签收问题单](../../../../build/d10-t03/final/review-signed.md) · [当前阶段](../../../../Docs/Developments/Developments.md#current-stage) · [下一工作包](../../../../Docs/Developments/D08-D11-RenderPaths.md#d10-t04)
