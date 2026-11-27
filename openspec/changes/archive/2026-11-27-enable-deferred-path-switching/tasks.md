# Tasks

## 1. A · 路径请求、配置与文档

- [x] 1.1 扩展 View 的类型化路径和值域，接入按实际路径和资源用途的配置解析。走既有主 View 和显式离屏准备入口，核对单样本能力、Physical Lighting 的 None/FXAA、Direct Lighting 与诊断的实际配置。用同一布局或能力输入区分 Deferred 附件拒绝和可用 Forward，核对原因与原 AA 请求。Deferred+MSAA 为 false，TAA 保持未启用。 [配置规格](specs/view-preparation/spec.md#requirement-resolve-the-supported-configuration-for-each-opaque-strategy)
- [x] 1.2 在 View Settings、单字段事务及 v1 读写中接续同一 Deferred 值。用现有编辑与文件入口核对选择、Undo/Redo、同值幂等和冻结/过期目标。让尚未确认的 Deferred+FXAA 请求保存并重开，核对请求字符串和首帧前的待确认状态。用已有 v1 文件和一个显式非法路径对照缺省 Forward、Forward+ 兼容及失败后完整保留活动文档。 [编辑规格](specs/editor-domain-edits/spec.md#requirement-edit-and-inspect-the-main-view-opaque-strategy) · [文件规格](specs/scene-document-persistence/spec.md#requirement-persist-only-the-main-views-supported-requests)

## 2. A · 三策略组装和资源准入

- [x] 2.1 将三策略注册到固定的表面准备及不透明着色入口，接续共享天空与显示。用一份含受光核心、MASK 和 Unlit 的静态场景，在 Physical Lighting 的 None 与 FXAA 下各完成一轮三路径正常切换。核对实际生产身份、目标格式/样本、配对 HDR/D32、Deferred 最终 G3 与辅助 Equal 用途及真实显示 Pass。选择能区分核心、遮罩孔和辅助覆盖的小范围原生读回。复用同场景的 Direct Lighting 或诊断对照核对保留的 FXAA 请求及实际 None。 [子图规格](specs/view-preparation/spec.md#requirement-assemble-one-complete-opaque-subgraph-from-live-demand)
- [x] 2.2 按实际消费者接好主 View 和离屏 View 的有效、待选角色，沿用原生准备预算和上传所有权。让 Forward 的 Deferred 候选先受原生额度限制，再遇到一个必需 Deferred 角色拒绝，最终恢复合法输入。核对按本轮相机继续交付的有效图、明确角色原因、只有实际消费者保留的附件和私有写入。另一 View 同帧读取自己的四块 Froxel，记录计算/上传来源及其接受票据。本路恢复 Deferred 后切回 Forward，复验无本路专属列表写入及共享驻留仍可用。 [等待与失败](specs/view-preparation/spec.md#scenario-continue-the-effective-view-and-another-view-after-deferred-preparation-fails) · [资源归属](specs/view-preparation/spec.md#scenario-keep-only-uploads-and-attachments-with-a-live-consumer-when-a-candidate-is-dropped)
- [x] 2.3 按可执行身份选择完整计划。当已有效 Deferred 的附件限制阻止当前布局时，确认同输入下就绪的 Forward 能独立获准。复用 1.1 的附件条件核对两个策略的结果和预算归属。为 Forward+ 与 Deferred 互切检查每 View 的共享四块上传，沿已有独立表面回放检查仅 GBuffer、普通颜色附加表面和无核心受光消费者的合同。输入、Shader 或实际图声明改变时才重跑受影响的原生部分。 [反向切换](specs/view-preparation/spec.md#scenario-resolve-a-ready-forward-request-when-the-previous-deferred-layout-no-longer-fits) · [独立表面合同](../../../specs/deferred-opaque-rendering/spec.md#requirement-prepare-and-identify-the-real-deferred-color-producer)

## 3. A · 正常确认、视口与历史

- [x] 3.1 在现有正常图回执中核对本轮选用计划、View/Scene、请求身份和采样或导出。复用 2.2 的持续双 View 场景，快速提交 Deferred、另一策略及新的 Deferred 请求，使第一轮准备的完成或错误晚于新请求。用一次未获正常确认的就绪尝试及一次正常提交核对公开的请求/有效路径、实际 AA、生产身份、颜色与配对深度和票据。确认旧图安全显示、诊断或部分执行的保活票据没有路径确认资格。统计及延迟 GPU 结果与原 View/Scene/票据对应。 [事务规格](specs/view-preparation/spec.md#requirement-prepare-and-confirm-an-opaque-strategy-per-view) · [视口规格](specs/editor-viewport-output/spec.md#requirement-confirm-effective-viewport-settings-from-accepted-output)
- [x] 3.2 按已接受生产身份结算各 View 的兼容性。沿同一等待序列移动相机并改变渲染/显示尺寸，让原路径先在新栅格交付，再切换 Deferred。核对兼容位姿的前驱、resize 与路径各自的失效帧、失败候选保持的旧记录、另一 View 的兼容前驱及视口图像映射。保留一次旧输出或未完成提交，核对旧颜色仍对应自己的生产票据，兼容几何/材质继续供两路使用，存储按实际引用与完成条件回收。核对调用链沿帧槽完成协议推进。 [历史规格](specs/view-preparation/spec.md#requirement-invalidate-view-history-from-the-accepted-plan-compatibility) · [提交与保活](../../../../Docs/Architecture.md#history-commit)

## 4. A · 集成检查与独立审查交接

- [x] 4.1 在 Windows x64 上构建受影响的 Debug、Release 产品配置并检查真实 OpenGL 启动。用同一组正常入口回放汇总上述实现验证，核对输入、设备、配置库、可执行程序、包、日志和读回身份。以 D10-T04、D11-T03 签收证据为基线，登记复用的检查及对应输入，运行本次更改所需的检查，记录限制。OpenSpec 严格校验和本次变更的验证条件均通过后结束本地检查。开发夹具归入 `build/d11-t04/`。 [构建入口](../../../../Docs/Build.md) · [验证范围](../../../../.agents/skills/physara-verification/SKILL.md) · [双路径证据](../2026-11-26-switch-opaque-paths-atomically/tasks.md#final-verification) · [Deferred 证据](../2026-11-27-add-deferred-lighting-subgraph/tasks.md#final-verification)
- [x] 4.2 在当前问题单登记真实待审版本、直接消费者、验证入口及限制，给出 B 审查档位建议与理由。说明候选和有效计划的隔离、实际生产身份、跨 View 预算和在途存储的复核风险。记录交接后由维护者调度 B，审查和复核项保持待办。 [当前问题单](../../../../build/d11-t04/final/review-signed.md) · [审查档位](../../../../Docs/Developments/AgentModels.md#review-tier)

## 5. B 审查、A 整改与 B 复核

- [x] 5.1 B 独立核对实际工作树、约束与直接消费者，按规范补齐涉及的完整第一方文件注释。以稳定 CR 编号记录可触发问题、验收证据和注释覆盖，由 B 写入审查结论。2026-11-27 首审未通过，CR-001 至 CR-003 已原生复现，待 A 整改。 [首审问题与证据](../../../../build/d11-t04/final/review-signed.md#问题与整改) [独立审查](../../../../.agents/skills/physara-review/SKILL.md)
- [x] 5.2 A 按 CR 逐项修正并执行对应回归，在原问题单交接实际修订和证据。CR-001 至 CR-004 已完成 A 整改与自检。 [首轮整改](#a-correction-1) · [CR-004 整改](#a-correction-2) · [当前问题单](../../../../build/d11-t04/final/review-signed.md#问题与整改)
- [x] 5.3 B 完成两次整改复核，签收第二次整改的实现与注释。CR-001 至 CR-004 全部关闭。 [签收版本与原生证据](#b-final-review)

<a id="b-recheck-1"></a>
### 第一次 B 整改复核

CR-001 的双 View 上传预留、CR-002 的屏幕常量回滚与确认、CR-003 的重分批绑定复用均通过独立 Release 原生回放。新增 CR-004：同样剩余两个设备驻留条目时，两路保持 Forward+ 均可提交；两路同时请求 Deferred 后，离屏候选先占用冷屏幕缓冲及 Froxel 条目，主路连原有效计划也失去本帧票据。释放夹具施加的驻留压力后，两路 Deferred 均能提交。任务 5.3 保持待办，待 A 修正跨 View 的候选驻留准入。 [本轮问题单](../../../../build/d11-t04/final/review-signed.md) · [驻留竞争回放](../../../../build/d11-t04/b-recheck-1/simultaneous-residency.log)

<a id="b-final-review"></a>
### 第二次 B 复核与签收

B 签收 `v2.0@24ab273f0a8e20d8e7cb626ecb11ed573200aa33` 加第二次整改清单中的 12 个实现输入。清单 SHA-256 为 `5ca38b5abe147caf55695e0cb354e5b8700b8ac6f1954ddfcf7d8df13ae0bc3e`。源码与构建前快照一致，178 个登记文件的身份核对通过。A 建议 Y 档，实际 B 调度参数未经维护者确认。 [签收清单](../../../../build/d11-t04/a-correction-2/handoff.json) · [B 独立回放证据](../../../../build/d11-t04/b-recheck-2/acceptance.json)

| 问题 | B 结论与 Release 原生证据 |
|---|---|
| CR-001 · 上传额度 | 两种收尾顺序下，离屏有效策略均在单名额压力下提交新尺寸；暖缓存恢复后主路成功切换。 [上传对照](../../../../build/d11-t04/b-recheck-2/peer-admission.log) |
| CR-002 · 屏幕常量缓存 | 晚期拒绝及仅准备未录制后，原布局均为零次上传；已录制而未确认时，下一次重写并恢复正确读回。 [同槽对照](../../../../build/d11-t04/b-recheck-2/warm-rollback.log) |
| CR-003 · 路由重分批 | 两个新批次均沿用原 set 1 和间接缓冲，命令范围、各自表面管线、辅助 Equal 与读回均符合原对照。 [重分批对照](../../../../build/d11-t04/b-recheck-2/rebatch-reuse.log) |
| CR-004 · 设备驻留 | 条目及字节受限时，两路均取得新票据和完整 Froxel，只有可容纳的候选切换；解除压力后两路均交付 Deferred。新帧槽中强制主路先收尾，离屏有效 Deferred 也取得其冷屏幕缓冲。 [驻留竞争及恢复](../../../../build/d11-t04/b-recheck-2/simultaneous-residency.log) |

代码与注释通过。复核覆盖设备额度归属、候选失败撤回、帧槽缓存、绑定集合和正常提交。B 在单台 Intel OpenGL 4.6 上独立重链 Release 执行以上四项，额度压力由夹具施加。Debug、产品构建与隐藏启动沿用 A 本轮记录，阶段画质与成本留给 D11-T05。 [A 的验证范围](#a-correction-2)

## 6. A · 最终验证、同步与归档

- [x] 6.1 对 B 签收的实际输入执行最终 verify，核对规格场景、决策和原生证据身份，复用匹配的检查并补齐签收版的三路径、v1 和独立表面回放。 [最终验证](#final-verification)
- [x] 6.2 四份增量已同步并通过严格校验，Change 已归档。开发计划记录 D11-T04 验收与 D11-T05 前置状态，签收证据留存后已恢复当前问题单模板。 [规格同步与归档](#archive-closeout) · [计划工作包](../../../../Docs/Developments/D08-D11-RenderPaths.md#d11-t04)

<a id="a-local-verification"></a>
## A 实现与本地验证 · 2026-11-27

基准为 `v2.0@24ab273f0a8e20d8e7cb626ecb11ed573200aa33`。主 View 和离屏 View 的正常准备共用三路径选择。已选策略决定表面、实际照明角色和上传。编辑事务与 v1 边界接受 `deferred`，保留路径请求在正常受光提交前独立待确认。 [实现与验证清单](../../../../build/d11-t04/a/handoff.json)

Windows x64、MSVC 19.44.35228、SDK 10.0.26100.0。Debug 和 Release 产品构建及 `--hidden --poll-limit=3` 均通过。逐配置重链并运行同一夹具，各通过 7 项 OpenGL 原生回放。实际命令、源码、包、库、可执行文件、日志和读回摘要登记在清单内。 [Debug 构建](../../../../build/d11-t04/a/product-Debug.log) · [Release 构建](../../../../build/d11-t04/a/product-Release.log) · [回放回执](../../../../build/d11-t04/a/native-results.json)

| 检查 | 实际证据 |
|---|---|
| 1.1、2.1 · 三策略与配置 | Physical Lighting 按 None、FXAA 各完成三路径切换，图中实际 FXAA Pass 和显示边缘读回相符。Deferred 交付 623 个核心样点和 160 个辅助样点，颜色与单样本 D32 配对，采样末版 GBuffer，辅助只读 Equal。Direct Lighting 实际使用 None 并保留 FXAA 请求。Deferred+MSAA 与未支持的 AA 值经过准入拒绝。 [三策略回放](../../../../build/d11-t04/a/routes-Release.log) · [配置回放](../../../../build/d11-t04/a/limits-Release.log) |
| 1.2 · 编辑与 v1 | Deferred 选择、Undo/Redo、同值请求、Open 冻结和退休 Scene 通过真实编辑事务核对。保存和重开恢复未确认的 Deferred 与 FXAA。既有 v1 文件及 Forward+ 可恢复，非法值在 `/mainView/requested/opaquePath` 拒绝并保留活动文档。 [文档回放](../../../../build/d11-t04/a/document-Release.log) |
| 2.2、2.3 · 准备与退路 | 原生额度使候选等待 3 帧，随后报告实际缺失的 DeferredLightingPhysical 包。Forward 与离屏 Forward+ 各自继续提交。恢复合法候选后，以更窄的绑定额度触发晚期拒绝，Forward 图没有 Deferred 专用常量写入，离屏继续消费自己的 4 块 Froxel。恢复后实际切入 Deferred，退出时仅撤掉主路私有需求。2300 × 2100 布局拒绝旧 Deferred，Forward 在同一布局接受。 [恢复与隔离](../../../../build/d11-t04/a/recovery-Release.log) · [反向切换](../../../../build/d11-t04/a/limits-Release.log) |
| 3.1、3.2 · 接受与历史 | 用同一 View 快速复选相同路径，确认过时准备与错误均无法确认新请求。相机位姿的兼容前驱、resize 及真正切换的失效帧、独立 View 的兼容前驱分别核对。通过 Runtime 正常图采样和离屏导出实际接受 Deferred，验证未采样和诊断的确认规则、当前图像映射、保留旧颜色的票据与像素，以及延迟 GPU 计时的原始身份。 [历史与过期候选](../../../../build/d11-t04/a/recovery-Release.log) · [正常 Runtime 回放](../../../../build/d11-t04/a/runtime-Release.log) |
| 2.3、4.1 · 已有表面行为 | 重新运行混合图、联合深度读取与独立表面缺颜色包恢复。两配置共 12 份混合图原始读回与已登记的 D11-T03 基线逐字节一致，两配置 62 个产品 Shader 包也逐项匹配签收清单。 [混合图](../../../../build/d11-t04/a/mixed-Release.log) · [独立表面](../../../../build/d11-t04/a/surface-missing-color-Release.log) |

原生证据覆盖单台 Intel RaptorLake-S Mobile、OpenGL 4.6.0、驱动 32.0.101.6129。界面选项映射经源码核对，编辑及保存流程通过实际事务入口运行。兼容性验证覆盖当前单样本 profile，三路径整场等价、成本及后续时域效果按对应工作包验收。OpenSpec 1.13.2 严格校验通过。 [校验回执](../../../../build/d11-t04/a/openspec-validate.json)

<a id="a-correction-1"></a>
## A 整改与验证 · 第 1 轮 · 2026-11-27

以 B 首审的三项原生复现为基线，Debug 与 Release 均重新构建产品、隐藏启动并重链回放。两配置各通过三项定向回放和四项直接回归。 [首审问题](../../../../build/d11-t04/final/review-signed.md#问题与整改) · [本轮清单](../../../../build/d11-t04/a-correction-1/handoff.json) · [原生回执](../../../../build/d11-t04/a-correction-1/native-results.json)

| 问题 | 定向回放结果 |
|---|---|
| CR-001 · 两路写入准入 | 两路换尺寸，在已有 Froxel 预留之外只容纳一次屏幕常量写入。以产品优先顺序和夹具强制主路先收尾各回放一次，离屏在新尺寸交付有效票据、一次屏幕写入和四块 Froxel，主路继续以 Forward+ 交付并报告 Deferred 上传超限。离屏后续命中屏幕缓存，主路在同一上限下用唯一余量接受 Deferred。 [双 View 压力回放](../../../../build/d11-t04/a-correction-1/peer-admission-Release.log) |
| CR-002 · 屏幕常量回执 | 暖槽里新布局在屏幕载荷登记后因 Froxel 绑定受限退出，恢复原布局读取相同 HDR/D32，屏幕写入为零。仅就绪而未录制时同样复用。实际换尺寸写入一次，完整确认后同布局复用。夹具另执行一次原生写入并跳过确认，随后按原接受布局重写一次并恢复对应读回。 [同槽回放](../../../../build/d11-t04/a-correction-1/warm-rollback-Release.log) |
| CR-003 · 兼容绑定复用 | 原一个前向批次拆成核心与辅助两批。两批引用原 set 1 和间接缓冲，各有一个命令并覆盖原命令范围。两类使用不同表面 Pipeline，辅助颜色为只读 Equal。最终 GBuffer 标记、D32、HDR 与显示读回和切换前的 Deferred 对照一致。 [路线与批次回放](../../../../build/d11-t04/a-correction-1/rebatch-reuse-Release.log) |

直接回归覆盖缺包、等待与恢复、Runtime 正常视口采样和离屏导出、混合表面及独立表面缺颜色包。两处接口注释在编译后补齐，清单留有编译时头文件并核对差异仅为两行 Doxygen 文本。设备为 Intel RaptorLake-S Mobile、OpenGL 4.6.0、驱动 32.0.101.6129。写入压力由回放施加，阶段画质与成本由 D11-T05 验收。 [回归回执](../../../../build/d11-t04/a-correction-1/native-results.json) · [OpenSpec 严格校验](../../../../build/d11-t04/a-correction-1/openspec-validate.json)

<a id="a-correction-2"></a>
## A 整改与验证 · CR-004 · 2026-11-27

首次和待选不透明策略在创建私有 GPU 缓冲前，合计自身的分配需求及另一仍在准备的有效策略所需的驻留余量。按设备的字节和条目两项核对。有效策略已物化或被撤回后，候选根据实际已占用的容量准入。槽内屏幕缓冲可复用时保留原设备条目，需更新的字节仍按原上传预算结算。 [驻留设计](design.md#在现有预算内分别准备有效与待选计划) · [路径准入](../../../../Engine/Renderer/Paths/Forward.cpp)

使用 B 的 Release 复现条件，两路有效 Forward+ 只剩两个设备条目时仍可提交。两路同时请求 Deferred 后，暖缓存主路在原容量内切换，冷缓存离屏沿用 Forward+ 并报告驻留等待，两路各交付新票据与四块 Froxel。解除压力后离屏也接受 Deferred。只剩两份 Froxel 所需的字节时，得到同样的交付结果。在新帧槽的对照中，夹具让主路先收尾，离屏有效 Deferred 仍取得自己需要的屏幕缓冲和 Froxel。 [B 首次复核](#b-recheck-1) · [Release 驻留回放](../../../../build/d11-t04/a-correction-2/simultaneous-residency-Release.log) · [Debug 驻留回放](../../../../build/d11-t04/a-correction-2/simultaneous-residency-Debug.log)

Debug 与 Release 产品重新构建并隐藏启动，两配置各通过 6 项原生回放。直接回归覆盖上传单名额、屏幕常量回滚、候选等待与恢复、Runtime 的正常采样与离屏导出，以及旧 Deferred 附件超限时当前 Forward 请求的独立准入。压力由夹具施加，证据覆盖 Intel RaptorLake-S Mobile OpenGL 4.6.0、驱动 32.0.101.6129。 [本轮清单](../../../../build/d11-t04/a-correction-2/handoff.json) · [回放回执](../../../../build/d11-t04/a-correction-2/native-results.json) · [OpenSpec 严格校验](../../../../build/d11-t04/a-correction-2/openspec-validate.json)

<a id="final-verification"></a>
## A 最终验证 · 2026-11-27

核对了 B 签收的基准提交、12 个实现输入、构建前快照和 178 条文件记录。两配置的产品、原生夹具、链接库及 62 个 Shader 包均与签收清单匹配。任务文档中 B 的签收段落和 5.3 勾选按原字节复原核对，签收问题单原件已留存。复用 A 的两配置产品构建、隐藏启动及 12 项原生回放，以及 B 的 4 项独立 Release 回放。 [签收输入核对](../../../../build/d11-t04/final/signed-inputs.json) · [B 签收原件](../../../../build/d11-t04/final/review-signed.md) · [独立复核](#b-final-review)

| 核对范围 | 实现与验证 |
|---|---|
| 单样本配置及三路组装 | 三策略由固定入口组装，按实际材质选取 GBuffer、辅助等深或完整前向色。签收版 Release 的三路径 None/FXAA、Direct Lighting、混合表面和独立表面缺色包对照通过。六份混合图读回与首轮 Release 对照逐字节一致。 [路径组装](../../../../Engine/Renderer/Paths/Forward.cpp) · [三策略回放](../../../../build/d11-t04/final/routes-Release.log) · [独立表面回放](../../../../build/d11-t04/final/surface-missing-color-Release.log) |
| 路径事务、预算与历史 | 规划按当前 View 解析可执行策略，在其它有效策略的写入和驻留需求可满足时准入候选。正常采样或导出才确认生产身份与历史。签收版的过期请求、旧布局超限、压测下双 View 交付、屏幕常量缓存与绑定复用均具备原生回执。 [路径准备与准入](../../../../Engine/Renderer/Paths/Forward.cpp) · [正常提交](../../../../Runtime/Frame.cpp) · [接受与历史](../../../../Engine/Renderer/View.cpp) · [A 回归](#a-correction-2) · [B 复核](#b-final-review) |
| 编辑、视口与 v1 | 核对 Workspace 路径字段、编辑事务及当前采样回执的状态展示。签收版实际执行同值编辑、Undo/Redo、Open 冻结与退休 Scene 检查，恢复原 v1 文件、Forward+ 和仍待确认的 Deferred/FXAA，并在指定字段拒绝非法请求。Runtime 回放核对了匹配图像映射、有效 AA 与延迟票据身份。 [编辑入口](../../../../Editor/Panels/Workspace.cpp) · [v1 回放](../../../../build/d11-t04/final/document-Release.log) · [Runtime 回放](../../../../build/d11-t04/a-correction-2/runtime-Release.log) |

四份增量的 7 条要求、39 个场景已按代码、现行合同及上述回执核对，OpenSpec 1.13.2 严格校验通过。新增四项回放均沿用签收版 Release 可执行文件和同字节输入，在单台 Intel RaptorLake-S Mobile OpenGL 4.6.0（驱动 32.0.101.6129）验证。编辑入口通过真实事务与 Runtime 回放核对，界面点击和跨设备兼容性留在本轮证据范围之外。整场 HDR 等价、诊断可视化和成本由 D11-T05 验收。 [最终验证回执](../../../../build/d11-t04/final/verification.json) · [严格校验](../../../../build/d11-t04/final/verify-change.json)

<a id="archive-closeout"></a>
## 规格同步与归档 · 2026-11-27

将三路配置和子图要求并入 `view-preparation`，同步该规格的路径事务与历史要求，以及编辑、视口接受和 v1 保存恢复三条要求。合计新增 2 条、修改 5 条，各主规格已有场景均有对应的保留或更新。45 份主规格和本 change 均通过 OpenSpec 1.13.2 严格校验。 [同步清单](../../../../build/d11-t04/final/spec-sync.json) · [主规格校验](../../../../build/d11-t04/final/validate-main-specs.json) · [归档前 Change 校验](../../../../build/d11-t04/final/validate-synced-change.json)

B 签收问题单按原字节留存，原件内部相对链接以签收时的 `Docs` 目录为基准。 [签收原件](../../../../build/d11-t04/final/review-signed.md) · [D11-T05 工作包](../../../../Docs/Developments/D08-D11-RenderPaths.md#d11-t05)

D11-T04 已验收归档，D11-T05 的前置已满足。归档引用核对通过，当前问题单已按模板恢复。 [归档核对](../../../../build/d11-t04/final/archive-audit.json)
