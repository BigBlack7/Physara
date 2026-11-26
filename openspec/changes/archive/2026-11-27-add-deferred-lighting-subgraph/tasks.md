# Tasks

## 1. A · 共享光照与正式 Shader 角色

- [x] 1.1 提取由 Forward、Forward+ 和 Deferred 共同调用的核心光照聚合，接收已预曝光自发光，保留现有累加和限幅顺序。编译受影响的产品变体，从同输入的普通 Forward、Forward+ 签收场景核对同一 HDR、深度和覆盖，确认入口自发光仍应用一次预曝光。固定各模式、相机和资源身份后用一组覆盖共享分支的回放即可。Release 重链原生回放后，四组 HDR 和 D32 与签收件逐字节相同。 [共享光照规格](specs/deferred-opaque-rendering/spec.md#requirement-evaluate-shared-lighting-from-the-frozen-view-inputs) · [冻结基线](../../../../Docs/Performance/ForwardPlusReference.md#阴影与环境画质) · [回放比对](../../../../build/d11-t03/a/shared-replay.json)
- [x] 1.2 在已有生成 ABI 和产品目录加入 DeferredLighting 的有限角色，接好默认构建、运行期包定位和帧前原生准备。核对各变体的活动读项、阶段、类型、目标格式、四块 Froxel 和对应阴影/环境配方，尤其是 G3 无符号取数及 D32 的用途。用一份同 ABI 但遗漏必需 G3 活动读项的测试包核对冷准备及未被 View 选择时的热候选拒绝，检查原因指向正确角色，兼容代及独立 Forward View 按原预算继续，合法包可恢复。同步产品包的构建说明。 [角色规格](specs/deferred-opaque-rendering/spec.md#requirement-prepare-and-identify-the-real-deferred-color-producer) · [Shader 构建](../../../../Docs/Build.md#shader-构建) · [冷包与热候选回放](../../../../build/d11-t03/a/shader-Release-verified.log)

## 2. A · 冻结 View 的生产准备

- [x] 2.1 将回调入口接成 Renderer 内部类型化生产需求，冻结对应的相机、渲染矩形与光照角色。让核心及辅助受光共同驱动现有灯表、阴影和环境准备，以实际消费者保留、结算或撤回 Froxel 写入。用同一共享批次的 Deferred 核心与独立 Forward 核对本 View 的身份和四块独占写入，撤去核心颜色后核对只剩独立表面及其所需资源。检查阴影诊断组合在准备写入前返回原拒绝原因。 [准备与交付](design.md#用-renderer-内部的显式请求对接现有准备链)
- [x] 2.2 记录准备就绪且实际使用的 DeferredLighting 身份及本次相关生产阶段。跟随 2.1 的主路和附加路，核对 Pending/Failed 的上一有效输出、正常接受后的真实包身份，以及单独表面与普通颜色的需求。复用 1.2 的拒绝与恢复输入作为跨阶段检查。 [生产身份规格](specs/deferred-opaque-rendering/spec.md#requirement-prepare-and-identify-the-real-deferred-color-producer)

## 3. A · 生产图、像素与接续合成

- [x] 3.1 用最终表面、当前逆渲染投影和高低位相机数据在正式屏幕颜色 Pass 恢复像素位置、视深、观察方向及核心材质。以带矩形留边、渲染与显示尺寸不同的偏移透视平面和有限正交远面作为原生标样。用已知几何的独立双精度射线交点与灯响应核对取数像素、朝向、深度和真实 HDR。记录字段量化、D32 与重建的样点误差范围。固定场景做一次大世界共同平移对照，区分相机相对重建与低精度绝对坐标。 [重建规格](specs/deferred-opaque-rendering/spec.md#requirement-reconstruct-the-eligible-final-surface-in-the-current-view)
- [x] 3.2 接入共享 Froxel、阴影和环境输入以及明确的屏幕效果默认值。用一个含方向灯、有限灯、无范围灯和可调索引预算的受光场景检查正常格、溢出及边界不确定退路。核对实际查询归属和生产 HDR 与同输入全灯参考，关注累加次序及漏灯。以一个包含环境、两种已知材质 AO 和非单位曝光的发光标样核对 DFG、IBL、材质 AO 及一次预曝光，按相同材质点做无环境或无直接光对照。兼用 G2 与 HDR 读回检查只改变间接光的遮蔽及 G2 的工作域语义。 [受光规格](specs/deferred-opaque-rendering/spec.md#requirement-evaluate-shared-lighting-from-the-frozen-view-inputs)
- [x] 3.3 以几何深度重建阴影接收法线。用一个有解析法线的斜面、更改法线贴图的对照和同帧邻点不足的薄边核对接收位置。按当前阴影组的矩阵、尺度、D32 及指定样点独立检验比较结果，选择能区分有效法线偏移和零偏移的接收条件。核对 Hard 与 PCF 沿共享查询交付可见性，复用现有方向与局部灯约定。 [阴影增量](specs/shadow-rendering/spec.md#requirement-resolve-hard-and-pcf-from-conventional-depth-comparisons)
- [x] 3.4 组装 `SurfaceOutput → LightingInputs → OpaqueOutput`，继续共用辅助重画、天空和显示。在 3.1 的正交标样加入实际 Unlit 遮罩和非对称天空，读回最终 G3/D32、适用的合成前 HDR 和合成后 HDR/显示色。证明辅助遮罩孔、远平面有效几何、背景与留边的覆盖正确，读项追溯到 Core 与 SurfaceAux 的相应版本。复用附件运行纯 Unlit 和仅天空控制，核对底色初始化以及无核心照明需求时的图与资源活性。 [合成规格](specs/deferred-opaque-rendering/spec.md#requirement-deliver-composable-opaque-color-and-unchanged-final-surface-depth)
- [x] 3.5 核对产品图中附件写入到采样、采样后接只读 Equal 深度及未被改写的最终 D32。在同一深度生产者和当前 GL 能力下，复用或补一条联合只读原生检查，让实际深度测试与采样在同一 Pass 消费该 D32 的同一范围。核对计划状态、写后同步、刻意设置的通过和未通过像素及读回值。受支持的联合读取和产品实际用途都有原生证据后停止本项。 [图编译规格](../../../specs/render-graph-compilation/spec.md#requirement-preserve-overlapping-storage-order-and-reject-illegal-feedback)

## 4. A · 本地验收与独立评审交接

- [x] 4.1 运行受影响的 Windows x64 产品构建和所需 OpenGL 回放。开发期脚本、样例、临时工程和原始读回放在 `build/d11-t03/`。记录实际运行和复用的命令、源码、包、库、场景、驱动及结果身份，标明未覆盖范围。受影响的实际结果、资产对应关系和严格规格校验齐备即结束本地检查。 [验证约定](../../../../.agents/skills/physara-verification/SKILL.md) · [构建与运行](../../../../Docs/Build.md) · [交接清单](../../../../build/d11-t03/a/handoff.json)
- [x] 4.2 在当前问题单交接实际待审版本、直接消费者、可定位的原生证据和限制。按实际实现给出 B 的审查档位建议与理由，着重说明位置重建、几何阴影偏移及活性和包身份的复核风险。交由维护者调度 B，审查与复核任务保持待办。 [当前问题单](../../../../build/d11-t03/final/review-signed.md) · [审查档位](../../../../Docs/Developments/AgentModels.md#review-tier)

## 5. B 审查、A 整改与 B 复核

- [x] 5.1 B 完成首审及源码注释。原生复现了深度轴共同平移丢失局部光（CR-001）和独立表面受未用颜色包阻断（CR-002）；已接受 Deferred 输出在等待、拒绝及恢复之间的票据与像素对照通过。两项待 A 整改。 [本轮问题单与证据](../../../../build/d11-t03/final/review-signed.md)
- [x] 5.2 A 完成 CR-001、CR-002 的整改与自检，并按 B 复核意见补齐 CR-003 的诊断片元活动资源准入，将当前版本与原生证据交回 B。 [首轮整改验证](#a-correction-1) · [第二次整改验证](#a-correction-2) · [第三次整改验证](#a-correction-3) · [第四次整改验证](#a-correction-4) · [当前问题单](../../../../build/d11-t03/final/review-signed.md)
- [x] 5.3 B 完成四轮独立复核并签收第四次整改工作树。CR-001、CR-002、CR-003 均已关闭，代码与最终注释通过。 [签收版本与证据](#b-final-review)

第一次 B 复核已通过 CR-002。CR-001 的原平移样例通过，新增非零眼点低位对照仍将同一核心像素的 D32 从 0.5234375 改为 0.5，有限点光消失。B 已在原问题单登记余差来源、定向回放及下次复核判据，待 A 继续整改。 [本轮问题单](../../../../build/d11-t03/final/review-signed.md) · [失败读回](../../../../build/d11-t03/b-recheck-1/eye-low.log)

第二次 B 复核确认前两组平移和纯表面缺包/恢复对照均通过。CR-001 仍有眼点编码误差：对第二组再共同平移 0.375 m，同一核心像素保持正确点光响应，但最终 D32 为 0.546875，解析值为 0.5234375。任务 5.3 保持待办，待 A 修正光栅与重建使用的眼点精度。 [B 本轮结论](../../../../build/d11-t03/final/review-signed.md) · [原生读回](../../../../build/d11-t03/b-recheck-2/eye-encoding.log)

第三次 B 复核已关闭 CR-001。三个平移对照及暖帧尾数更新均得到正确的解析 D32 和 HDR，CR-002 保持通过。新增 CR-003：级联诊断的片元眼点改由 `perView` 提供，同 ABI 包缺少该活动项时仍通过冷加载及未选角色热发布。任务 5.3 待 A 整改后复核。 [问题单](../../../../build/d11-t03/final/review-signed.md) · [精度对照](../../../../build/d11-t03/b-recheck-3/depth-comparison.json) · [准入失败见证](../../../../build/d11-t03/b-recheck-3/preview-eye.log)

<a id="b-final-review"></a>
### B 第四次复核与签收

B 签收 `v2.0@9627d618bda990cc8a05fcaa80f433e0f5529c70` 加第四次交接清单所列 44 个源码输入。已核对 211 个登记文件及工作树范围，清单 SHA-256 为 `bdbdcb2774b7f7bf027c2488fc77bd46b62d8408dd1d410ba19eca603b9bdd4d`。A 建议 Y 档，实际 B 调度参数未经维护者确认。 [签收清单](../../../../build/d11-t03/a-correction-4/handoff.json) · [独立回放与身份](../../../../build/d11-t03/b-recheck-4/acceptance.json)

| 正式问题 | B 最终结论与证据 |
|---|---|
| CR-001 · 共同平移的深度与受光 | 三组平移及暖帧眼点尾数变化均通过，整图 G3、D32、HDR 匹配。 [读回](../../../../build/d11-t03/b-recheck-3/depth-comparison.json) |
| CR-002 · 独立表面准备 | 未用颜色包缺失时纯表面仍交付，普通颜色请求独立报告失败，合法包恢复后两路可读。 [读回](../../../../build/d11-t03/b-recheck-3/surface-missing-color.log) |
| CR-003 · 诊断片元眼点准入 | 第四轮独立 Release 回放确认原缺项包在冷、热入口均被拒绝，旧代继续提交，恢复后 HDR、D32 和显示色逐字节匹配两级联对照。 [回放](../../../../build/d11-t03/b-recheck-4/preview-eye.log) |

已复核路径准备、三段眼点上传与使用、图版本、角色准入及失败恢复的源码注释。第四轮核对了两配置六个诊断角色/配置组合的阶段反射，并在 Intel OpenGL 4.6 上重跑 Release 的 Deferred G3 缺项对照。Debug 运行、产品构建和启动沿用 A 的本轮记录。B 审查完成，最终 verify、同步和归档由 A 执行。 [B 共享入口回归](../../../../build/d11-t03/b-recheck-4/shader.log) · [A 第四次验证](#a-correction-4)

## 6. A · 最终验证、同步与归档

- [x] 6.1 对 B 签收的实际工作树执行最终 verify，核对实现、场景、包及原生输出身份。确认规格覆盖、复用对应同一输入的原生证据，并按实际变化补受影响的检查。记录结果和限制。 [最终验证](#final-verification)
- [x] 6.2 最终验证通过后同步新增 `deferred-opaque-rendering` 和修改的 `shadow-rendering` 规格，严格校验并归档。更新 D11-T03 的验收状态、归档引用及 D11-T04 前置，保留 B 建议档位、实际配置、CR 及验收证据。核对归档链接后按约定重置当前问题单。 [当前工作包](../../../../Docs/Developments/D08-D11-RenderPaths.md#d11-t03)

<a id="a-local-verification"></a>
## A 本地验证

本轮原生运行使用 Windows x64、MSVC 和 Intel RaptorLake-S Mobile OpenGL 4.6，驱动 32.0.101.6129。产品从常规 CMake 目标构建，回放使用匹配配置的 Shader 包及 Renderer、RenderGraph 和 OpenGL 库。内部 Renderer 请求走真实准备、图编译、录制、提交及 View 接受。开发期观察包仅为分支和可见性读回提供额外 alpha，正常 HDR 取自产品角色。 [产品构建](../../../../Docs/Build.md) · [夹具](../../../../build/d11-t03/a/Native.cpp)

Debug 和 Release 的产品构建及 `Physara.exe --hidden --poll-limit=3` 均通过。Release 运行 `VerifyDeferred.exe Release` 的 `mixed`、`shader`、`geometry`、`physical`、`shadows` 五项，Debug 运行前三项。两配置核对了六个 Deferred 角色的绑定、ABI 及阶段 IO。OpenSpec 严格验证通过。 [构建、回放及原始输入身份](../../../../build/d11-t03/a/handoff.json) · [角色 ABI](../../../../build/d11-t03/a/role-abi.json) · [OpenSpec 回执](../../../../build/d11-t03/a/openspec-validate.log)

| 合同 | 实际结果 |
|---|---|
| 共享光照基线 | 用当前库、产品包和显示链重链 D10 冻结回放。Forward、Forward+、关闭阴影和移除环境的四组 HDR 和四组 D32 与签收件逐字节一致，原始回放含发光标样。 [比对](../../../../build/d11-t03/a/shared-replay.json) |
| 准入、需求和身份 | 同 ABI 缺少 G3 活动读取的包在冷准备及未选角色的热准入中均定位到 `gBufferG3`。独立 Forward 输出与单独准备一致，合法代恢复后 Deferred 提交记录实际核心 Shader，重复兼容帧接续已接受的票据。同图双 View 中，仅 Deferred 一路读取四笔本路 Froxel 写入。保留独立表面时该组退出活性。 [准入回放](../../../../build/d11-t03/a/shader-Release-verified.log) · [双 View 回放](../../../../build/d11-t03/a/mixed-Release-verified.log) |
| 像素重建及候选退路 | 偏移透视 View 的独立几何交点在 8 m 平面，D32 为 0.025。有限灯的解析范围对照有 282 个内部与 968 个外部样点，HDR 最大绝对差为 0.0009765625。该固定样例按 `2^-12 + 0.003 × |参考 HDR|` 核对，底限覆盖近截断的栅格插值差异。正交样例原生观测有 896 个正常格像素、1152 个溢出像素和 384 个不确定位置退路像素。对应的全灯、溢出和 16777216 m 共同平移对照在当前读回上无 HDR 差异。 [重建及查询回放](../../../../build/d11-t03/a/geometry-Release-verified.log) |
| 中性屏幕输入和工作域 | 非单位预曝光约 0.416667。G2 红通道为 0.15625，只有自发光时与输出工作色一致。去掉环境或直接光的控制保持各自贡献，材质 AO 只改变间接项。物理模式实际读取 DFG 和环境，关闭屏幕效果时保留环境照明。 [物理回放](../../../../build/d11-t03/a/physical-Release-verified.log) |
| 真实阴影查询 | 以签配的方向光投影、解析平面法线和实际阴影层 D32 独立重算。每种贴图输入核对 1088 个 Hard 或 PCF 样点。Hard 有 10 处由几何接收偏移改变的见证及 10 个薄边阴影样点，PCF 对应 11 和 12。替换法线贴图保持可见性。组合 IBL、DFG、Froxel 和阴影的产品角色也完成了原生提交。 [阴影回放](../../../../build/d11-t03/a/shadows-Release-verified.log) |
| D32 访问和合成 | 实际屏幕 Pass 读取最终 G0–G3 版本和 D32，辅助颜色读取同一 D32 的只读附件并加载核心 HDR。混合图含 63 个 D32 为零的核心像素、160 个辅助像素和 2289 个天空像素。联合探针在同一 D32 既采样又做只读 Equal 测试，560 个通过像素读回 0.5，未通过处保持清色，原生绘制记录一条联合读取屏障。相同 G3 存储复用为纯 Unlit 和仅天空后，旧核心资格清零。 [合成与联合读取](../../../../build/d11-t03/a/mixed-Release-verified.log) |

测试范围为内部单样本子图和现有 Direct、Physical Lighting 的生产链。D11-T04 继续完成 View/Editor 的三路径配置与切换，D11-T05 完成阶段等价与成本对照。 [后续工作包](../../../../Docs/Developments/D08-D11-RenderPaths.md#d11-t04)

A 建议 B 采用 Y 档，重点独立复核位置重建误差、几何阴影偏移、实际图版本以及不同 View 的资源与包身份。B 的实际调度待维护者确认。 [本轮交接](../../../../build/d11-t03/final/review-signed.md)

<a id="a-correction-1"></a>
## A 首轮整改验证

**CR-001**　将主表面、等深重画和前向几何的 clip 变换建立在同一眼点相对位置上。复用对象行的高低位偏移和相对投影的光栅 ABI，阴影子视图继续使用自己的偏移原点和常规深度矩阵。重跑 B 的视深平移标样，两配置的 1,536 个核心像素覆盖相同，解析 D32=0.5234375。共平移 16,777,216 m 后，点光见证样点保持 0.3737793，整图 HDR 最大误差为零。 [Release 证据](../../../../build/d11-t03/a-correction-1/depth-axis-Release.log) · [Debug 证据](../../../../build/d11-t03/a-correction-1/depth-axis-Debug.log)

**CR-002**　显式独立表面需求从 PlanView 到 Declare 只准备匹配的表面输入及角色。保留普通颜色同时附带表面的入口。两配置的缺包对照验证独立表面只登记一个着色键，冷图只有一笔本路相机行写入，暖规划的上传需求为零，实际读回 G3=1、D32=0.5234375、G2.r=0.15625。同图普通颜色候选明确报告缺少 `StaticForward`，合法包恢复后颜色与附加表面均可提交。 [Release 证据](../../../../build/d11-t03/a-correction-1/surface-missing-color-Release.log) · [Debug 证据](../../../../build/d11-t03/a-correction-1/surface-missing-color-Debug.log)

**共享入口回归**　用当前 Release 库和产品包重链 D10 签收回放。Forward、Forward+、关闭阴影与移除环境的四组 HDR 与签收件逐字节一致。四组现有 D32 彼此一致，各比签收件有 3,928 个样点改变，最大 `5.9604645 × 10⁻⁸`，即 1 ULP，覆盖一致。274,168 个地板样点与原解析深度的对照、十一层实际阴影及原 ROI 判据均通过。D11 夹具中四份 Hard/PCF 投射深度与原生首轮数据逐字节一致。 [基线差分及输入](../../../../build/d11-t03/a-correction-1/shared-replay.json) · [D10 判据回执](../../../../build/d11-t03/a-correction-1/shared-witness.log) · [投射深度对照](../../../../build/d11-t03/a-correction-1/caster-depth-compare.json)

**实际执行与范围**　Windows x64、MSVC 19.44.35228、SDK 10.0.26100.0，Intel RaptorLake-S Mobile OpenGL 4.6，驱动 32.0.101.6129。Debug 与 Release 产品构建和隐藏启动均通过。两配置均运行 `depth-axis`、`surface-missing-color`、`mixed`、`geometry`、`shader`，Release 另运行 `physical` 与 `shadows`。共核对 24 个角色与配置组合的 ABI 或反射，OpenSpec strict 通过。回放覆盖单样本内部请求和现有 Forward/Forward+ 消费链。 [源码、包、库及回放清单](../../../../build/d11-t03/a-correction-1/handoff.json) · [角色 ABI](../../../../build/d11-t03/a-correction-1/role-abi.json) · [OpenSpec 回执](../../../../build/d11-t03/a-correction-1/openspec-validate.log)

<a id="a-correction-2"></a>
## A 第二次整改验证

**CR-001**　三参数 `LightingDelta` 复用 `CompensatedLightingDelta`，将高低位作差的有符号余差与主差值合并。共享静态顶点变换与阴影读写沿用相同的补偿作差。Froxel 的带误差入口以主差值和绝对余差界判定当前格。两配置在 `2^24` m 及 `2^48 + 2^23` m 的共同平移后读回 1,536 个核心像素，解析 D32=0.5234375，点光样点红通道为 0.373779296875。两组 G3、D32 和 HDR 的平移前后读回逐字节匹配。 [B 失败对照](../../../../build/d11-t03/b-recheck-1/eye-low.log) · [Release](../../../../build/d11-t03/a-correction-2/eye-low-Release.log) · [Debug](../../../../build/d11-t03/a-correction-2/eye-low-Debug.log)

**共享入口回归**　辅助 Equal、联合只读和偏移透视光照通过。四份 Hard/PCF 投射深度与第一次整改逐字节一致，实际接收偏移与零偏移对照通过。用第一次整改的 D10 回放程序和同一资产，换用本次 Release 产品包。四组 HDR 与 D10 签收件逐字节一致。本次主 D32 与第一次整改逐字节一致，相对 D10 签收件最大差为 1 ULP，覆盖一致。 [混合图](../../../../build/d11-t03/a-correction-2/mixed-Release.log) · [阴影回放](../../../../build/d11-t03/a-correction-2/shadows-Release.log) · [投射深度对照](../../../../build/d11-t03/a-correction-2/caster-depth-compare.json) · [D10 冻结对照](../../../../build/d11-t03/a-correction-2/shared-replay.json)

**实际执行与范围**　Windows x64、MSVC 19.44.35228、Intel OpenGL 4.6，驱动 32.0.101.6129。Debug 与 Release 产品构建和隐藏启动均通过。以各自当前配置的库与产品 Shader 重链原生夹具，两配置运行 `depth-axis`、`eye-low`、`mixed`，Release 另运行 `geometry`、`shadows`、`shader` 与 `surface-missing-color`。十次回放、24 个角色与配置组合的 ABI/反射对照及 OpenSpec 严格校验均通过。CR-002 已由 B 复核通过，CR-001 待 B 独立复核。 [版本与证据](../../../../build/d11-t03/a-correction-2/handoff.json) · [角色核对](../../../../build/d11-t03/a-correction-2/role-abi.json) · [OpenSpec 回执](../../../../build/d11-t03/a-correction-2/openspec-validate.log)

<a id="a-correction-3"></a>
## A 第三次整改验证

**CR-001**　统一编码 FP64 原点的高位、低位与低位窄化余量。主光栅和 Deferred 从同一 `ForwardView` 取得眼点，Froxel 的上传误差检查扣除已编码的第三段。阴影读写共用三段投影原点，级联查询从主 View 行取得眼点。全屏恢复将眼点低位、尾数与相对位置分别交给共享补偿作差。两配置运行 B 的 `2^24` m、`2^48 + 2^23` m 及在后者基础上追加 `0.375` m 的对照。B 新样例的 D32 为解析值 0.5234375，点光见证像素的红通道为 0.373779296875，1,536 个核心像素的 G3、D32 和 HDR 读回逐字节匹配未平移输入。同一帧槽连续准备后两个样例，验证高位和低位相同且尾数更新时会上传正确视图常量。 [B 失败读回](../../../../build/d11-t03/b-recheck-2/eye-encoding.log) · [Release 修正读回](../../../../build/d11-t03/a-correction-3/eye-encoding-Release.log) · [Debug 修正读回](../../../../build/d11-t03/a-correction-3/eye-encoding-Debug.log)

**共享入口回归**　辅助 Equal、D32 联合只读、偏移透视及 Froxel 退路通过。Hard/PCF 接收验证通过，四份实际投射深度和第二次整改逐字节一致。用当前库和生成接口重链 D10 冻结回放，四组旧路径 HDR 与 D10 签收件逐字节一致，主 D32 与第二次整改逐字节一致。地板解析深度、11 层阴影与原 ROI 判据通过。 [混合图](../../../../build/d11-t03/a-correction-3/mixed-Release.log) · [投射深度对照](../../../../build/d11-t03/a-correction-3/caster-depth-compare.json) · [D10 对照及输入](../../../../build/d11-t03/a-correction-3/shared-replay.json)

**实际执行与范围**　Windows x64、MSVC 19.44.35228、Intel OpenGL 4.6，驱动 32.0.101.6129。两配置产品构建和隐藏启动通过。以当前库和产品 Shader 重链夹具，两配置均执行 `depth-axis`、`eye-low`、`eye-encoding` 和 `mixed`，Release 另外执行 `geometry`、`shadows`、`shader` 及 `surface-missing-color`。十二次原生回放、30 个角色与配置组合的生成 ABI/反射检查及 OpenSpec 严格校验通过。CR-001 待 B 复核。 [源码与证据清单](../../../../build/d11-t03/a-correction-3/handoff.json) · [角色 ABI](../../../../build/d11-t03/a-correction-3/role-abi.json) · [OpenSpec 回执](../../../../build/d11-t03/a-correction-3/openspec-validate.log)

<a id="a-correction-4"></a>
## A 第四次整改验证

**CR-003**　三个诊断角色共用按阴影组类型分派的查询，三个正式片元模块均激活 set 0/b0 `perView`。在冷、热共用的诊断配方中登记该必需项。顶点与片元按各自阶段核对。Debug 和 Release 的六组正式诊断反射均符合配方。两配置使用 B 记录的同一缺项包，冷准备拒绝及未选角色热候选拒绝均定位 `StaticCascadePreview` 的片元 `perView`。热拒绝后活动代为 1，普通 Forward 继续提交且前驱票据接续。合法候选发布为活动代 2 后，级联诊断的 HDR、D32、显示色与两级联合法对照逐字节一致。 [B 原始复现](../../../../build/d11-t03/b-recheck-3/preview-eye.log) · [Release 回放](../../../../build/d11-t03/a-correction-4/preview-eye-Release.log) · [Debug 回放](../../../../build/d11-t03/a-correction-4/preview-eye-Debug.log) · [阶段反射](../../../../build/d11-t03/a-correction-4/preview-reflection.json)

**实际执行与范围**　Windows x64、MSVC 19.44.35228、Intel OpenGL 4.6，驱动 32.0.101.6129。Debug、Release 产品构建与隐藏启动均通过。用各自当前库重链夹具，两配置均通过 `preview-eye` 和 `shader`。后者复验 Deferred 的 G3 缺项拒绝及独立 View 交付。产品 Shader 包逐项匹配第三次整改清单，CR-001 与 CR-002 的 B 复核记录见问题单。OpenSpec 严格校验通过，CR-003 待 B 独立复核。 [本轮清单](../../../../build/d11-t03/a-correction-4/handoff.json) · [Release G3 对照](../../../../build/d11-t03/a-correction-4/shader-Release.log) · [OpenSpec 回执](../../../../build/d11-t03/a-correction-4/openspec-validate.log)

<a id="final-verification"></a>
## A 最终验证 · 2026-11-27

`v2.0@9627d618bda990cc8a05fcaa80f433e0f5529c70` 的 44 个实现输入及四份提案、设计和增量规格与 B 签收一致。复核第四次交接的 211 个登记文件，确认两配置的库、产品、包、夹具和读回。B 的独立 Release 诊断恢复前后 HDR、D32 和显示色逐字节匹配，并与 A 的同配置对照相同。OpenSpec 1.13.2 严格校验通过。 [最终验证清单](../../../../build/d11-t03/final/verification.json) · [B 签收原件](../../../../build/d11-t03/final/review-signed.md) · [严格校验](../../../../build/d11-t03/final/verify-spec.log)

| 规格要求 | 实现与场景证据 |
|---|---|
| 当前 View 的最终表面重建 | `Lighting.glsl` 用最终 G3 门控取数，以有效矩形、逆渲染投影和三段眼点恢复位置。偏移透视、正交零深度远面和 3 组沿视深共同平移均已验证。B 的 18 份平移对照原始读回与登记相符，暖帧眼点余量变化也通过。 [重建与全灯参考](../../../../build/d11-t03/a-correction-3/geometry-Release.log) · [B 平移对照](../../../../build/d11-t03/b-recheck-3/depth-comparison.json) |
| 冻结输入与共享受光 | `EvaluateOpaqueLighting` 共用逐灯、DFG、IBL 与工作色聚合。正常格、溢出和不确定退路的代表样点与全灯参考匹配。本轮用签收 Release 夹具和 31 个对应产品包复跑 `physical`，预曝光 0.416667，G2 自发光红通道为 0.15625，低 AO 和单位 AO 的合成 HDR 红通道分别为 3 和 3.7832。无环境及无直接光对照验证 AO 只调制间接分量。 [光照回放](../../../../build/d11-t03/final/physical-Release.log) |
| 工作色合成与最终 D32 | `DeclareCoreSurface`、`ShadeDeferredOpaque` 和辅助 Equal 重画消费表面末版，随后共享 Sky 和 Display。混合、纯 Unlit、仅天空及池复用检查覆盖遮罩孔、远面、背景和留边。联合采样和只读 Equal 由实际 GL 图回放检验通过及未通过像素。 [混合与图用途](../../../../build/d11-t03/a-correction-3/mixed-Release.log) |
| 准备与实际生产身份 | 冻结 View 的准备需求驱动四块 Froxel 上传及 `OpaqueProduction`。跨 View、撤去颜色、纯表面缺包和恢复场景均通过。无符号 G3 的阶段与类型由生成接口及反射核对。B 的冷、热缺项回放验证产品拒绝、兼容代与独立 View，等待与拒绝期间的上一输出由票据和显示读回对照。 [纯表面](../../../../build/d11-t03/b-recheck-3/surface-missing-color.log) · [缺少 G3](../../../../build/d11-t03/b-recheck-4/shader.log) · [上一有效输出](../../../../build/d11-t03/b-first-review/retained.log) |
| 几何接收法线与共享 Hard/PCF | 屏幕邻点供接收几何估计，边缘回退零法线偏移。实测覆盖法线贴图切换、几何偏移对照、薄边及两种滤波。四份 caster 深度与前一回放逐字节相同。原常规深度比较及诊断偏移沿用已验收的共享对照。 [几何接收](../../../../build/d11-t03/a-correction-3/shadows-Release.log) · [Caster 深度](../../../../build/d11-t03/a-correction-3/caster-depth-compare.json) · [共享对照](../2026-11-25-add-hard-pcf-shadow-visibility/tasks.md#final-verification) |

两份增量涉及 5 条要求和 13 个场景，其中两项 Hard/PCF 场景沿用既有合同与证据。设计中的生产准备、图版本、角色准入和光照共享均有对应实现。A4 的行为变化局限于诊断片元角色准入，两配置共 62 个产品包与 A3 逐项一致。复用与当前签收匹配的双配置构建、隐藏启动、诊断回归，以及经身份核对的 A3 图、阴影和 Forward/Forward+ 原生证据。四组冻结 HDR 与 D10 签收件逐字节一致，主 D32 相差至多 1 ULP、覆盖一致。原生证据覆盖单台 Intel RaptorLake-S Mobile、OpenGL 4.6.0、驱动 32.0.101.6129。三路径事务切换及整场等价和成本门槛分别在 D11-T04、D11-T05 验收。 [冻结回放](../../../../build/d11-t03/a-correction-3/shared-replay.json) · [B 签收](#b-final-review)

<a id="archive-closeout"></a>
## 归档与工作包交接

新增 `deferred-opaque-rendering` 主规格的 4 条要求和 10 个场景。将屏幕表面的几何接收与零偏移退路并入 `shadow-rendering` 的 Hard/PCF 要求，保留已有场景并增加一项屏幕接收场景。两份主规格同步后，45 份主规格全部通过 OpenSpec 1.13.2 严格校验。 [Deferred 主规格](../../../specs/deferred-opaque-rendering/spec.md) · [阴影主规格](../../../specs/shadow-rendering/spec.md#requirement-resolve-hard-and-pcf-from-conventional-depth-comparisons) · [同步清单](../../../../build/d11-t03/final/spec-sync.json) · [严格校验](../../../../build/d11-t03/final/validate-main-specs.json)

D11-T03 已验收归档，D11-T04 的前置已满足。B 签收问题单按原字节留存，原件内部相对链接以签收时的 `Docs` 目录为基准。 [D11-T04 工作包](../../../../Docs/Developments/D08-D11-RenderPaths.md#d11-t04) · [签收原件](../../../../build/d11-t03/final/review-signed.md)

已核对归档链接与签收版本，当前问题单已恢复项目模板。 [重置前核对](../../../../build/d11-t03/final/archive-pre-reset.json)
