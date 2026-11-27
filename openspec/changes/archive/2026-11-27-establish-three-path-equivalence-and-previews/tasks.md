# Tasks

## 1. 冻结验收输入与读回入口

- [x] 1.1 A 核对 D11-T01 至 T04 已采纳合同、当前构建输入和 D09/D10 场景许可。在 `build/d11-t05/` 登记可重建输入、原生运行条件、资源哈希、拟复用的签收检查和组别判据。先登记同口径的覆盖、D32、HDR 和误差来源，用已知字段及解析深度确认样点可以区分正确结果与取错表面。 [画质协议](design.md#quality-protocol) · [固定阴影输入](../../../../Docs/Performance/ForwardShadowReference.md#fixture) · [固定多灯输入](../../../../Docs/Performance/ForwardPlusReference.md#many-lights-fixture)
- [x] 1.2 A 沿正常 Runtime 接受链接入开发期图读回。对一组明确有覆盖的样点核对真实策略、Shader/Pass、天空后工作色和配对 D32、有效矩形、提交后完成、格式和行距。保留一次请求与接受的路径身份不同的对照，证明采集端能拒绝把旧路径计入新路径。停在可用的正式逐帧读回及身份清单。 [策略回执](specs/view-preparation/spec.md) · [读回合同](../../../../Docs/Architecture.md#readbacks)

**基线记录**　以 `78cb8e8` 的产品库和正式包派生 D10-T05 的 Runtime 观察回放。`reference` 在 Intel RaptorLake-S Mobile OpenGL 4.6.0 驱动 `32.0.101.6129`、VSync 0 上接受三条路径的 640×480 Physical Lighting / None 帧。RGBA16F staging 行距为 5,120 B，三路 D32 一致，地板见证为 `0.6827309132`，匹配解析深度且 alpha 为 1。隐藏视口时 Forward+ 请求等待，原 Forward 票据被采集端拒绝。 [冻结输入](../../../../build/d11-t05/inputs.json) · [原生回执](../../../../build/d11-t05/reference-baseline.log) · [样点与身份](../../../../build/d11-t05/reference-baseline.json)

## 2. 类型化表面预览与编辑器交付

- [x] 2.1 A 交付 D32 和混合 GBuffer 的预览配方、产品 Shader 准备与图颜色输出。用同一混合表面原生读回核对 G0 线性值、G1 解码、G2 工作域转换、G3 整数类别和 D32，覆盖辅助遮挡、D32 为零的远面、留边、主次栅格及核心无效区域。lit 扩展类别用独立的整数标样通过同一转换配方验证。达到既定通道和格式精度后复用该夹具。 [转换与格式](design.md#preview-flow) · [预览字段规格](specs/editor-viewport-output/spec.md#requirement-inspect-typed-opaque-surface-previews-for-the-current-view)
- [x] 2.2 A 将会话选择和稳定的预览角色接到真实 UI Image 与当前帧的图颜色绑定。用一条包含有效 Deferred、待选路径、新尺寸、Forward 生效及 Deferred 恢复的序列核对来源、占位、显示、普通颜色回执与在途保护。对照同一输入的预览开关，核对普通 HDR/D32 及实际配置，确认无预览消费者时新增 Pass、存储和上传消失。用主视口隐藏对照确认诊断自身无法推进路径或历史。 [会话与寿命](specs/editor-viewport-output/spec.md#requirement-bind-surface-previews-only-to-a-live-matching-frame)
- [x] 2.3 A 在正式包准入中核对活动资源和颜色输出配方。以整数资源类型不符的热重载候选证明拒绝并保留旧代。以冷态缺预览角色与预览专属额度受限各取一个可达对照，确认带原因占位、普通颜色及另一可用 View 继续，合法输入恢复后才交付新预览。检查范围止于本次新增角色和可触发的准入失败。 [Shader 准备](../../../../Docs/Architecture.md#shader-runtime) · [预览准入](design.md#preview-flow)

**预览记录**　正式 GPU 输出核对八种通道，展示字节与独立计算的误差最多 1。混合标样包含非均匀缩放的着色法线、非单位 AO、超过 1 的预曝光自发光及有覆盖的零深度。lit 扩展色来自同一片元转换配方和本图独立的 `R16_UINT = 4` 标样。活动 G3 误标为浮点的候选被准入拒绝，原代继续交付。冷包缺失和 1900×1700 的单 View 附件上限均保留普通主 View 与离屏接受，恢复 592×448 后重新交付预览。 [Release 预览](../../../../build/d11-t05/preview-Release.log) · [颜色和深度复算](../../../../build/d11-t05/native/preview-proof.json) · [冷包缺失](../../../../build/d11-t05/preview-missing-Release.log)

**交付与寿命记录**　正常 UI 栅格 Pass 读取当前预览色版本。关闭和完全裁剪省掉预览色和参数上传，普通 RGBA16F 与 D32 逐字节相等。移除主视口请求时诊断不能确认新正常色。原生 Event 明确仍在途时，从 1280×1080 的票据 149 改尺寸并申请 Forward。票据 150 用新尺寸继续接受当时有效的 Deferred，旧色仍由 149 持有。Forward 生效后显示占位，Deferred 恢复时交付新版本，原 Event 正常完成。 [在途控制](../../../../build/d11-t05/lifetime-Release.log) · [开关和裁剪](../../../../build/d11-t05/native/preview-proof.json)

## 3. 三路径同输入 HDR 等价

- [x] 3.1 A 用第 2 组的冻结混合场景验证当前正式三策略。核对核心/Unlit、MASK 阈值、穿孔处后方实例覆盖、前后遮挡、正交远面、留边的 RGBA16F、几何覆盖和 D32。以全画布差分和独立样点完成验收，区分编码及重建可解释误差和实际回归。 [混合表面场景](specs/view-preparation/spec.md#scenario-compare-mixed-static-surfaces-with-the-matching-accepted-outputs)
- [x] 3.2 A 重建阴影及 IBL 参考场景，三策略以有效 PCF 512、3 组 11 层、相同灯和环境实际交付。对账正式阴影消费、主深度和影图，比较受光、遮挡、MASK 和天空的原生输出。用一个关闭阴影和一个移除环境的匹配对照核对贡献，按登记范围记录边界差异与未覆盖配置。 [阴影参考](../../../../Docs/Performance/ForwardShadowReference.md#quality) · [阴影场景](specs/view-preparation/spec.md#scenario-compare-shadowed-and-unshadowed-lighting-under-one-effective-profile)
- [x] 3.3 A 在一个有效光照工作集内核对方向灯、无范围局部灯、紧凑格及实际溢出退路。用匹配的独立原生观察证实 Forward+ 和 Deferred 真实查询分支、该 View 读到的范围及灯行，和 Forward 完整表的正式 HDR/D32 比较。选一个可辨认贡献做负对照。现有无限远透视、正交近端和边界证据按输入与共享消费者核对复用。 [候选与退路场景](specs/view-preparation/spec.md#scenario-preserve-global-local-lights-through-compact-and-fallback-traversal)

**画质记录**　混合场景有 79,824 个覆盖像素，三路 D32 和覆盖一致，Forward 与 Forward+ 的工作色数值一致，Deferred 通过冻结的连续材质误差界。三路参考场景的 11 层原生 PCF 影图逐字节相等，亮区、遮挡、MASK 和关闭环境的对照通过。连续区阈值外的两个蓝通道像素均在同深度 PCF 边缘，报告保留坐标、Off 对照与一像素邻域的实际衰减范围。局部灯组的紧凑、溢出、仅全局行及无有效格见证在两条 Froxel 策略分别读到 3、4、2、4 行。关闭红灯和将无范围聚光强度归零都在有覆盖接收面产生可检测差分。本次局部灯覆盖正交零近平面，沿用 D10 与 D11-T03 对无限远、投影及大坐标的签收证据。 [混合原始差分](../../../../build/d11-t05/native/mixed-proof.json) · [阴影及环境](../../../../build/d11-t05/native/reference-three-path-proof.json) · [局部灯见证](../../../../build/d11-t05/native/local-three-path-proof.json) · [D11-T03 复用证据](../2026-11-27-add-deferred-lighting-subgraph/tasks.md#final-verification)

## 4. 附件、路由与成本比较

- [x] 4.1 A 从已接受的实际 View 和存活计划发布紧凑统计。按描述符对账 D32、G0–G3、工作色、逻辑读写量与预览目标的范围，检查 Deferred 最终 D32 的去重，以及无辅助重画时核心屏幕生产仍计入。按实际表面项复算 lit ForwardExtension 与 Unlit 占比；用不同工作量的主/离屏 View 及安全 UI 回执确认取数归属。 [来源与单位](design.md#workload-scope) · [快照规格](specs/render-frame-statistics/spec.md#requirement-attribute-opaque-attachment-and-route-workload-to-the-accepted-view)
- [x] 4.2 A 在计时前对每个 0、32、256 灯组及同输入的 256 灯受限组核对实际有效路径、画质与可见灯。用同一运行的一次多条计时返回验证聚合前采集、一条错标或遗漏记录验证对账拒绝。记录该 View 的实际 Froxel 预算、溢出、受光见证和完整灯表行数，确保“溢出”标签由本轮实际工作证明。 [同口径采样](design.md#benchmark-protocol)
- [x] 4.3 A 按固定的 Release 场景和交错暖帧协议采集有效样本。以真实票据对账后复算 CPU 工作、等待、GPU 的 median/p95、样本数及缺测，并记录附件、绑定、分配、上传和路由占比。在本清单引用本轮实际命令、输入和比较结果。完整样本已能解释三路径与退路差异时停止，对不足的组定向补测。 [性能规格](specs/render-frame-statistics/spec.md#requirement-compare-opaque-path-cost-from-matched-accepted-samples)

**统计记录**　640×480 的单核心无天空场景中，Forward 和 Forward+ 的主 D32 与工作色共 3,686,400 B。Deferred 多 5,529,600 B 的 G0–G3，逻辑读取 6,758,400 B、逻辑写入 9,216,000 B。该 Deferred 组没有辅助重画，核心屏幕光照仍计入。混合场景四个可绘项包含两个 Unlit、零个 lit 扩展。仅离屏接受时，统计来自 192×128 Forward 的新票据。后续仅 UI 保留该票据。 [冷包与离屏对照](../../../../build/d11-t05/preview-missing-Release.log) · [已接受计数](../../../../build/d11-t05/native/timing-256.csv)

**计时记录**　执行 `Replay-Release.exe scale`、`scale-map` 后先复算画质与实测遍历，再执行 `timing` 和 `Proof.py timing`。11 组各有 3×24 个已接受样本，792 个 CPU 及对应整图 GPU 区间全部可复算。第 607 次 Poll 同时返回原票据 522、523，删除一个终态或将普通 Deferred 样本标为溢出都被拒绝。256 灯常规三策略的整图 GPU 中位数为 57.95、14.78、14.34 ms，限制索引额度的 Forward+ 与 Deferred 分别为 86.82、88.17 ms。常规格的候选减少和退路整表遍历均有独立受光像素见证。整图区间包含 UI，局部颜色区间按实际生产 Pass 标名。附件字节为逻辑规模。本次只对同一 Windows/OpenGL 设备、正式 Release 包、None AA、单 View 与已登记灯序取样。 [灯组及退路复算](../../../../build/d11-t05/native/scale-three-path-proof.json) · [阶段耗时与逐票据分布](../../../../build/d11-t05/native/timing-three-path-proof.json) · [原始查询终态](../../../../build/d11-t05/native/events-256.csv)

## 5. A 本地集成检查与交接

- [x] 5.1 A 构建受影响的 Debug、Release 产品和正式 Shader 包，检查真实 OpenGL 隐藏启动及正常退出。Release 形成上述阶段原生证据，Debug 跑影响图权限、预览角色及生命周期的代表场景。对实际更改的共享入口核对 T04 的 None/FXAA、Direct 和独立表面消费者，复用身份仍匹配的检查。登记本轮实跑、复用及未覆盖范围，严格校验 change。 [构建入口](../../../../Docs/Build.md) · [验证范围](../../../../.agents/skills/physara-verification/SKILL.md) · [T04 复用入口](../2026-11-27-enable-deferred-path-switching/tasks.md#final-verification)
- [x] 5.2 A 完成本地检查后更新当前问题单。登记基准及待审文件、可重放命令与最新输入/程序/包/证据摘要，按现行档位给出 B 的建议，写明确需独立核对的图版本及寿命、候选/验收身份和 HDR/计时复算风险。维护者决定实际调度。提交 A 交接时保留后续审查任务待办。 [当前问题单](../../../../build/d11-t05/final/review-signed.md) · [审查档位](../../../../Docs/Developments/AgentModels.md#review-tier)

**A 集成记录**　Debug 和 Release 正式构建、隐藏窗口启动与正常退出均通过。Debug 原生回放重复八通道、真实 UI 绑定、裁剪、跨路径尺寸、额度降级和活动 G3 类型拒绝。基于当前产品库独立重建 T04 夹具，`routes` 和 `surface-missing-color` 覆盖 None/FXAA、Direct Lighting 与单独表面读取。验证范围为 Windows x64、Intel RaptorLake-S Mobile Graphics Controller、OpenGL 4.6.0 驱动 `32.0.101.6129`。性能证据只采用 Release 的单样本 Physical Lighting / None、VSync 0。lit 前向扩展的实测计数为零，类别转换采用整数标样。 [Debug 原生回放](../../../../build/d11-t05/preview-Debug.log) · [T04 路径回归](../../../../build/d11-t05/compat/routes.log) · [独立表面回归](../../../../build/d11-t05/compat/surface-missing-color.log)

**A 交接记录**　建议 B 使用 X 档，映射为 GPT-6.2 Astra `xhigh`。图版本、FrameSlot 退休、普通 View 接受资格、候选 Shader 准入和逐票据对账需要联合复核。命令与源、程序、包及证据摘要记录在交接清单。独立审查、签收后 verify、同步和归档由后续流程完成。 [待审输入与命令](../../../../build/d11-t05/handoff.json) · [当前问题单](../../../../build/d11-t05/final/review-signed.md)

## 6. B 首审、A 整改与 B 复核

- [x] 6.1 B 在独立会话核对交接版本、实际图消费者、原生读回和比较依据，复核代表性的预览/等价输出和逐票据数据。补齐本次第一方文件注释，登记实际审查配置、覆盖、发现及结论。首审发现 CR-001 至 CR-007，已交 A 整改；实际调度参数待维护者确认。 [首审记录](../../../../build/d11-t05/final/review-signed.md) · [独立审查闭环](../../../../AGENTS.md#review-loop) · [审查与注释](../../../../.agents/skills/physara-review/SKILL.md)
- [x] 6.2 A 按稳定 CR 编号修正并自检，记录新输入及受影响的原生证据，逐项更新 A 的整改结果。 [首轮整改](#a-correction-1) · [第二次整改](#a-correction-2) · [第三次整改](#a-correction-3) · [第四次整改](#a-correction-4)
- [x] 6.3 B 对签收版本独立复核整改、注释、局部重放和样本复算。确认最终版本及原始证据的匹配，记录遗留限制并签收该轮。将 CR、实际档位/模型配置及复核结论保留在本清单的验收记录中。 [第四次复核及签收](#b-recheck-4)

<a id="a-correction-1"></a>
### A 首轮整改自检

- CR-001 至 CR-003 的原生回放覆盖额外 16 B 在 4,096 笔写入临界点触发的降级、主和离屏 View 的提交连续性，以及 Forward 和 Deferred 切换及改尺寸时首张实际 UI 的配对。相同产品的两配置隐藏启动及 Debug UI 路径通过。 [上传临界](../../../../build/d11-t05/a-correction-1/upload-boundary-Release.log) · [Release UI](../../../../build/d11-t05/a-correction-1/resize-and-status-Release.log) · [Debug UI](../../../../build/d11-t05/a-correction-1/resize-and-status-Debug.log) · [产品启动](../../../../build/d11-t05/a-correction-1/product-smoke.json)
- CR-004 与 CR-005 分别核对两种纹理解析顺序、有效预览的零告警对照，以及合法七级 mip、非法八级和 33 级预检。混合表面和八通道在当前正式包下读回复算通过，代表性普通 HDR/D32 与首轮记录逐字节一致。 [纹理告警](../../../../build/d11-t05/a-correction-1/stale-token-Release.log) · [图池边界](../../../../build/d11-t05/a-correction-1/mip-budget-Release.log) · [当前画质复算](../../../../build/d11-t05/a-correction-1/quality-proof.log)
- CR-006 复用原始 792 帧及 GPU 终态，原分布保持一致，复算拒绝重复票据。CR-007 定向补采同场景 11 组的图槽和同时发生的原生创建。当前包摘要包含新构建指纹，按新接受票据及本轮正式包配对，全部 11 组灯输入和 HDR/D32 与原组一致。计时继续使用原运行的样本。测试设备和配置沿用已登记的 Windows x64、Intel OpenGL 4.6。 [唯一票据对照](../../../../build/d11-t05/a-correction-1/timing-proof.json) · [图槽与原生分配](../../../../build/d11-t05/a-correction-1/native/graph-slots.csv) · [证据与待审版本](../../../../build/d11-t05/a-correction-1/handoff.json)

<a id="b-recheck-1"></a>
### B 第一次整改复核

- CR-002 至 CR-007 已复核通过。Release 原生回放确认改尺寸的首个采样帧宽高比为 2.0、当前图像与当前原因互斥、两种失效 token 顺序各告警一次，以及非法 mip 链在容量计算前被拒绝。原始 792 帧和另外构造的重复票据已独立复算，11 组图槽与原生创建数也已配对。 [逐项结果](../../../../build/d11-t05/final/review-signed.md)
- CR-001 仍待整改。4,096 笔写入的对照通过，固定相同场景载荷的字节临界对照中，未选标签仍占 7,604 B，导致本可接受的主、离屏两路退回安全 UI。新增 CR-008 记录拟合后全裁剪仍创建冷 Pipeline，CR-009 记录首次带非 ASCII 路径的原因标签在字体上传规划后才生成字形。签收项 6.3 待下轮复核。 [B 原生证据](../../../../build/d11-t05/b-recheck-1/native.log) · [复现入口](../../../../build/d11-t05/b-recheck-1/Probe.cpp)

<a id="a-correction-2"></a>
### A 第二次整改自检

- CR-001 的上传账单和 RHI 范围共用所选几何。B 的固定 64,902,016 B 场景载荷在两配置均交付主、离屏两路，打开 Route 时只追加 92 B 预览几何和对应参数。额外的 8 B 余量对照复现 16 B 参数越界，正常提交保留两路回执并在同票据记录原因。余量恢复后实际采样图像。Release 的 4,096 笔对照继续显示当帧容量标签。 [Release 字节边界](../../../../build/d11-t05/a-correction-2/upload-bytes-Release.log) · [Debug 字节边界](../../../../build/d11-t05/a-correction-2/upload-bytes-Debug.log) · [写入笔数边界](../../../../build/d11-t05/a-correction-2/upload-boundary-Release.log)
- CR-008 与 CR-009 的两配置回放分别核对同一冷 Route 的下部裁剪、首张带非 ASCII 路径的失败标签。裁剪阶段原生 Pipeline 数量维持 9，恢复后为 10 并有实际预览。错误帧使用已就绪的 atlas 显示转义路径，主、离屏同票据接受。 [裁剪回放](../../../../build/d11-t05/a-correction-2/clipped-cold-Release.log) · [首次标签回放](../../../../build/d11-t05/a-correction-2/unicode-reason-Release.log)
- Debug 和 Release 产品构建及隐藏启动通过。Release 九项、Debug 四项原生回放通过，覆盖改尺寸首帧、路径切换与标签，Release 还检查两种 token 告警顺序、八个预览通道和混合表面。代表性当前 HDR/D32 读回与 B 已复核的首次整改读回逐字节一致。设备为 Intel RaptorLake-S Mobile OpenGL 4.6.0。11 组图槽和 792 帧计时沿用 B 已复核的证据。 [启动结果](../../../../build/d11-t05/a-correction-2/product-smoke.json) · [本轮画质复算](../../../../build/d11-t05/a-correction-2/quality-proof.log) · [待审输入与结果](../../../../build/d11-t05/a-correction-2/handoff.json)

<a id="b-recheck-2"></a>
### B 第二次整改复核

- CR-001、CR-008、CR-009 已通过。Release 回放确认 64 MiB 字节边界和 4,096 笔上限均保住两路正常回执，完全裁剪时冷 Pipeline 数保持 9，首次带重音字符的包路径使用已驻留字形。混合表面和八通道原始读回复算通过，既有六项继续关闭。 [逐项结论](../../../../build/d11-t05/final/review-signed.md) · [B 原生回放](../../../../build/d11-t05/b-recheck-2/correction-replay.log)
- 新增两项 P2。CR-010 在普通 UI 几何尚余 740 B 时复现，标签未绘制，票据 218 也无原因记录。CR-011 在标签因上传额度省略时复现，长包路径的诊断达到 2,048 B，丢失接受票据 495。两组主、离屏 View 均正常提交。当前版本退回 A，任务 6.3 保持待验收。 [原始记录](../../../../build/d11-t05/b-recheck-2/diagnostics.log) · [复现夹具](../../../../build/d11-t05/b-recheck-2/Geometry.cpp)

<a id="a-correction-3"></a>
### A 第三次整改自检

- CR-010 在 Debug 和 Release 复用 B 的几何压力对照。可见标签在 8 MiB 预算只余 740 B 时省略，保留主、离屏同票正常回执及当前无 GBuffer 原因。撤去压力即恢复标签。关闭和完全裁剪均无省略诊断。另验证同图的可见预览图像在几何预算仅余 4 B 时省略并按同票记录容量原因，恢复后采样图像。 [Release 几何回放](../../../../build/d11-t05/a-correction-3/diagnostic-geometry-Release.log) · [Debug 几何回放](../../../../build/d11-t05/a-correction-3/diagnostic-geometry-Debug.log)
- CR-011 在两配置用长缺包路径和 64 MiB 上传边界省略标签，从诊断环读回完整的接受票据和可辨认的截断正文。Release 另重放同一日志入口的短原因并实测 16 B 预览参数的准入边界。主、离屏两路均接受相应正常提交。 [Release 长原因](../../../../build/d11-t05/a-correction-3/diagnostic-long-Release.log) · [Debug 长原因](../../../../build/d11-t05/a-correction-3/diagnostic-long-Debug.log) · [短原因及字节边界](../../../../build/d11-t05/a-correction-3/upload-bytes-Release.log)
- Debug 与 Release 构建及隐藏启动通过。Release 四项、Debug 两项原生回放通过，包含 Release 冷 Route 的裁剪对照。设备沿用 Intel RaptorLake-S Mobile OpenGL 4.6.0。本轮验证几何准入和诊断，画质、11 组图槽及 792 帧计时沿用 B 已复核的证据。 [冷预览对照](../../../../build/d11-t05/a-correction-3/clipped-cold-Release.log) · [待审输入与结果](../../../../build/d11-t05/a-correction-3/handoff.json)

<a id="b-recheck-3"></a>
### B 第三次整改复核

- CR-011 通过。B 独立运行四项 Release 回放，短原因和截断的缺包长原因分别保留完整票据 247、270，均与主、离屏回执一致。正常 Forward 标签和 Deferred 图像超出几何额度时按票据记录原因，关闭与完全裁剪均无误报。两配置构建、隐藏启动及 Debug 回放复用本轮 A 的匹配证据，画质、图槽与计时沿用前轮结论。 [B 回放](../../../../build/d11-t05/b-recheck-3/correction-replay.log) · [A 交接清单](../../../../build/d11-t05/a-correction-3/handoff.json)
- CR-010 保留 P2。主视口保留票据 212 的旧色时，离屏继续接受 214、215、216。214 和 216 均显示安全 UI 原因，215 因只余 740 B 省略标签，诊断环却记录需要匹配的可见视口。A 需统一该帧所选标签与提交日志的原因，按同一余量序列验证。B 已补齐 UI 几何拒绝和提交诊断的注释，10/11 项通过，任务 6.3 待签收。 [问题单](../../../../build/d11-t05/final/review-signed.md) · [安全 UI 回放](../../../../build/d11-t05/b-recheck-3/safe-reason.log) · [复现夹具](../../../../build/d11-t05/b-recheck-3/SafeReason.cpp)

<a id="a-correction-4"></a>
### A 第四次整改自检

- CR-010 的省略日志使用本次 UI 所选标签的原因，构图时复制正文，供 UI 回执消费后随成功票据写入诊断环。Debug 和 Release 复跑 B 的三帧序列。主视口保留旧色、离屏接受新提交时，740 B 余量内省略标签的日志原因与前后两帧显示一致。还原尺寸后，主、离屏恢复同票提交并采样当前预览。 [Release 安全 UI 回放](../../../../build/d11-t05/a-correction-4/safe-reason-Release.log) · [Debug 安全 UI 回放](../../../../build/d11-t05/a-correction-4/safe-reason-Debug.log)
- 两配置产品构建及隐藏启动通过。Release 的普通几何压力与长缺包原因对照通过，省略原因及完整票据在诊断环保留。设备沿用 Intel RaptorLake-S Mobile OpenGL 4.6.0。画质、11 组图槽及 792 帧计时沿用 B 已复核证据。 [普通几何回放](../../../../build/d11-t05/a-correction-4/diagnostic-geometry-Release.log) · [长原因回放](../../../../build/d11-t05/a-correction-4/diagnostic-long-Release.log) · [当前待审输入](../../../../build/d11-t05/a-correction-4/handoff.json)

<a id="b-recheck-4"></a>
### B 第四次整改复核及签收

- CR-010 通过。B 的 Release 原生回放中，主视口保留旧色，离屏接受票据 230、231、232。前后两帧绘制同一安全 UI 标签，中间帧因只余 740 B 省略标签，诊断环仍记录完整票据 231 和相同原因。恢复尺寸后，票据 236 的主、离屏回执及当前预览均恢复。普通几何边界和长缺包原因回放也通过。Debug 安全 UI、两配置产品构建及隐藏启动复用 A 的匹配证据。画质、11 组图槽及 792 帧计时沿用前轮复核结果。 [B 三项回放](../../../../build/d11-t05/b-recheck-4/correction-replay.log) · [A 自检与环境](../../../../build/d11-t05/a-correction-4/handoff.json)
- CR-001 至 CR-011 全部关闭，任务 6.3 签收。B 核对了本轮改动的三个产品文件，补齐原因正文的来源、借用期限和提交前复制的注释。签收基准为 `78cb8e8975af59f0d1a7bca52b9701026a5ee2cd`，47 项工作区输入的最终摘要已留存。B 按维护者指派独立审查，实际模型档位未登记。后续按 7.1、7.2 由 A 最终验证、同步及归档。 [问题单](../../../../build/d11-t05/final/review-signed.md) · [签收文件 SHA-256](../../../../build/d11-t05/b-recheck-4/signed-inputs.json)

<a id="final-verification"></a>
## 7. 最终验证、同步与阶段出口

- [x] 7.1 A 在 B 通过后核对签收源码、构建/包、读回夹具和设备身份，完成最终原生 verify。用一条签收的混合预览序列及受影响的读回或计时复算验证当前输入，登记其余可复用证据、各规格场景和测量边界。确认 D11-T05 及 D11 阶段出口的画质、遮挡、诊断、原子切换和同口径成本均有对应证据。 [D11 阶段出口](../../../../Docs/Developments/D08-D11-RenderPaths.md#d11-t05)
- [x] 7.2 A 保存签收问题单和复现依据，整理阶段比较参考。同步三份 delta、严格校验主规格和 change，归档并核对文档链接与归档记录，再重置当前问题单。开发计划登记 D11 出口和下一阶段前置，按当前档位规则在下一阶段入口给出 A 开发档位建议。 [比较参考归属](design.md#delivery) · [下一阶段开发档位](../../../../Docs/Developments/AgentModels.md#development-tier)

**A 最终原生验证**　核对 B 的基准及全部 47 项签收输入、第四次交接产物、夹具与复用证据。签收源码的 Debug/Release 产品重建和隐藏启动通过，66 份着色包与 24 份生成接口摘要保持一致。Release 重放 `mixed`、`preview` 和 `safe-reason`。12 份配对 HDR/D32 与 13 份预览输出逐字节匹配已复核读回。预览标签在余 740 B 时省略，主视口保留旧色，票据 261 的离屏回执和安全 UI 原因均有效。恢复尺寸后，两路在 266 同票交付当前预览。 [B 签收问题单](../../../../build/d11-t05/final/review-signed.md) · [签收 SHA-256](../../../../build/d11-t05/final/signed-inputs.json) · [最终程序、包、夹具与结果](../../../../build/d11-t05/final/verification-manifest.json)

| 规格合同 | 验证依据 |
|---|---|
| 视口的类型化表面预览 | 当前八通道、前向 D32、Deferred 最终 G0–G3、MASK 胜者、零值远平面和范围外背景读回通过。扩展路由颜色用受控整数标样核对。 [预览读回和样点](../../../../build/d11-t05/final/native/preview-proof.json) |
| 预览只绑定匹配且实际可见的帧 | 当前覆盖关闭、裁剪、改尺寸和安全 UI 独立离屏回执。沿用 B 验收的冷包、几何拒绝、长原因和调度证据，复用在途改尺寸和路径切换的实际回放。 [本轮预览](../../../../build/d11-t05/final/preview-Release.log) · [在途证据](../../../../build/d11-t05/lifetime-Release.log) · [B 签收复验](../../../../build/d11-t05/b-recheck-4/correction-replay.log) |
| 同输入的三路径不透明画质 | 当前混合表面通过，复用已核对的阴影及 IBL、紧凑和整表局部灯证据。D11-T02 的辅助表面遮挡和 D11-T04 的事务切换覆盖阶段出口的对应前置。 [混合差分](../../../../build/d11-t05/final/native/mixed-proof.json) · [阴影及环境](../../../../build/d11-t05/native/reference-three-path-proof.json) · [局部灯](../../../../build/d11-t05/native/local-three-path-proof.json) · [T04 原子切换](../2026-11-27-enable-deferred-path-switching/tasks.md#final-verification) |
| 接受 View 的附件与路由统计 | 当前预览统计与实际图读取配对，离屏与仅 UI 的来源沿用已签收对照。11 组槽数和字节按各自补采提交和等价画质复算。 [统计发布](../../../../Runtime/Frame.cpp) · [离屏/仅 UI 对照](../../../../build/d11-t05/preview-missing-Release.log) · [图槽原始补采](../../../../build/d11-t05/a-correction-1/native/graph-slots.csv) |
| 等质且同票据的成本样本 | 复算 11 组 792 个原始暖帧和 2,002 条查询终态，分布与 B 已复核记录一致。缺失终态、重复票据及溢出误标均被拒绝。成本范围为同一 Windows x64 Intel OpenGL 设备、Release、单 View、单样本 Physical Lighting / None、640×480、VSync 0。 [原始运行复算](../../../../build/d11-t05/final/timing-proof.json) · [三路径参考](../../../../Docs/Performance/ThreePathReference.md) |

规格场景与图内版本、接受票据及既有模块职责一致。Debug 的诊断边界、独立 View 验证及早期画质组沿用签收证据，本轮原生复验以 Release 为准。


<a id="archive-closeout"></a>
### 归档与阶段出口

- D11-T05 的三份增量已同步至主规格，新增五条需求。45 份主规格及当前 change 均通过严格校验。 [主规格校验](../../../../build/d11-t05/final/openspec-main-validation.json) · [change 校验](../../../../build/d11-t05/final/openspec-change-validation.json) · [同步文件与摘要](../../../../build/d11-t05/final/spec-sync.json)
- 签收问题单及最终验证资料随工作包留存，三路径输入、判据和同口径成本汇入[三路径比较参考](../../../../Docs/Performance/ThreePathReference.md)。D11 阶段出口已通过，D12 入口登记建议 M 档。 [签收问题单](../../../../build/d11-t05/final/review-signed.md) · [OpenSpec 归档回执](../../../../build/d11-t05/final/openspec-archive.json)

- 验证归档和文档链接后已按模板重置当前问题单。 [归档校验与交付记录](../../../../build/d11-t05/final/archive-audit.json)
