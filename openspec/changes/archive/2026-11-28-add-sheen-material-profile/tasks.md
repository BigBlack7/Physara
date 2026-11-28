# Tasks

## 1. A · 导入、实例与场景 v1

- [x] 1.1 实现 `KHR_materials_sheen` 字段、两个槽和标准排斥。用一组共享的 glTF/GLB 夹具经真实资产入口核对 required、缺省、同图的非对称 RGB/A、两个有效 UV 及各自变换，替换一次实际依赖图像以核对版本和旧固定引用。选一个越界值、一个缺失 UV 和两项同材质排斥定位拒绝，CPU 支持报告与受光状态各自有据后结束。 [导入规格](specs/static-gltf-import/spec.md) · [输入设计](design.md#inputs-and-profile) · [Release 回放](../../../../build/d12-t03/sheen-assets-Release.log)
- [x] 1.2 接入 typed edit、Inspector 和 v1 稀疏读写。复用 1.1 的两实例和共享源，经正式编辑入口写入、清除及 Undo/Redo。保存重开现有 v1 和有显式零及独立槽设置的 v1，核对继承、有效值和槽用途。用一份非法用途或有效 UV 候选同时验证编辑与打开的边界，确认活动文档、覆盖和版本保持有效。 [编辑规格](specs/editor-domain-edits/spec.md) · [文档规格](specs/scene-document-persistence/spec.md) · [Release 回放](../../../../build/d12-t03/sheen-assets-Release.log)

## 2. A · 生成接口与逐灯绒光

- [x] 2.1 生成材质行、UV 与槽合同，接通有效色角色和实际材质需求。构建生成接口及 C++ 直接消费者，读取 stride、t5 和两对新槽的生成及反射结果。用同一份非对称 UV/通道夹具核对显式零到非零的角色切换、纯 sheen 复用基底几何、组合沿用清漆框架，以及实际样点读取的材质槽。 [ABI 设计](design.md#inputs-and-profile) · [准入规格](specs/shader-generation-reload/spec.md) [ABI 准入](../../../../build/d12-t03/sheen-admission-Release.log)
- [x] 2.2 实现共享 Charlie/Neubelt、一次粗糙度过滤、视向归一和清漆下层合成。用实际 Direct 帧对比独立角度求值，包含一个掠射锐利样点触发归一、一个普通角度样点、一次几何半球越界，以及在平面上对照原始粗糙度 0/下限。对照零色、核心与 IOR/specular 基底、有效清漆及发射，确认各自权重与预曝光。复用同一灯表和几何夹具，按预先登记的解析及工作格式误差界通过后结束。 [表面规格](specs/static-surface-evaluation/spec.md) · [层合成设计](design.md#layer-response) [逐灯数值记录](../../../../build/d12-t03/proof-direct.json)

## 3. A · 环境产品、间接光与完整交付

- [x] 3.1 接入按需 Charlie 预滤波、固定蓝通道 DFG 和实际图消费。用程序生成的常量环境及方向梯度确认立方图各必要 mip 的有限归一结果，独立按方向积分核对两个代表性 DFG 查表点，包含一处原始能量大于一的对照。复用一个场景的实际视图请求，核对 Direct、Physical DFG-only、有环境的 Physical、Sky-only 与独立表面的作业、写入与图读。按同一 View 的正常票据确认仅传递实际所需产品。 [环境规格](specs/environment-lighting/spec.md) · [卷积和驻留设计](design.md#environment-products) [实际环境读写](../../../../build/d12-t03/sheen-demand-Release.log)
- [x] 3.2 交付 Physara 的配套 sheen IBL。沿用 3.1 的单位环境和梯度，核对白炉出射预算、两角度或粗糙度的工作色，以及 +1 EV、环境逆旋转与 AO 的间接作用。以独立角积分和方向一阶矩 `2/(0.5/αs+3)` 对照选定卷积，并取一处亮区证明透过权重先于工作色限幅。先登记积分与半精度误差界，继续验证到能够区分 Charlie 与 GGX 采样且高亮读回通过。布料 UV、半球和光照样例均在 `build/d12-t03/` 生成。 [环境响应规格](specs/environment-lighting/spec.md) · [工作色设计](design.md#layer-response) · [数值回放](../../../../build/d12-t03/proof-indirect.json)
- [x] 3.3 用实际环境替换验证完整交付。先接受 A，再让 B 的 Charlie 角色进入可触发的等待或未完成上传，随后提供完整 B。记录实际源代、各读写用途、返回原因和正常票据，核对旧已接受帧保活及 B 整代确认。仅改朝向或强度的对照复用产品键，证明查询变化与源内容变化确实可区分。 [环境代规格](specs/environment-lighting/spec.md) · [环境代回放](../../../../build/d12-t03/sheen-ibl-Release.log)

## 4. A · 三路径、角色准入与编辑结果

- [x] 4.1 完成三路径的 sheen 与组合受光。用同一真实导入的小场景包含 sheen MASK、核心下表面、清漆+sheen、IOR/specular 和一个等深及镜像双面标样。固定相机、分辨率、曝光、灯及阴影，对 Direct 和 Physical 分别比较正常接受票据、D32、影图和显示映射前 HDR，并读取 Deferred G1/G3、扩展 Draw 数和四附件。确认孔内核心资格、基底 G1、相同深度与登记精度内工作色。 [三路径规格](specs/static-forward-rendering/spec.md) · [现有比较判据](../../../../Docs/Performance/ThreePathReference.md#quality) [三路径读回](../../../../build/d12-t03/proof-paths.json)
- [x] 4.2 逐角色核对两族 24 个正式配方的阶段活动输入、实际绑定和设备上限。用一个真实编译的缺 sheen 活动绑定包覆盖冷准入、独立表面与受光切换，以及当前未选角色的热拒绝。随后合法候选在实际受光提交恢复，记录独立表面的图读与配对 G1/G3/D32。用共享接口的合法源码重载实测活动与候选的编码字节和条目，核对有界同驻和整代发布，按实际需要复验额度调整。 [准入规格](specs/shader-generation-reload/spec.md) · [角色与覆盖设计](design.md#routes-and-admission) · [冷、热准入和恢复](../../../../build/d12-t03/replay-fault-Release.log)
- [x] 4.3 用 1.2 的同源双实例和 4.1 的标样，确认实际 Inspector、覆盖编辑、清除、Undo/Redo、诊断后恢复受光。记录有效材质身份、最后接受的受光票据、匹配历史和两个实例的颜色。非法提交保留之前可读结果。面板及历史与连续正常提交一致后停止。 [实例规格](specs/editor-domain-edits/spec.md) [实际面板](../../../../build/d12-t03/native/sheen-inspector-accepted.txt) · [历史回放](../../../../build/d12-t03/sheen-routes-Release.log)

## 5. A 本地检查、B 审查、A 整改与 B 复核

- [x] 5.1 按实际 ABI 变更重建 Debug、Release 正式产品与生成包，隐藏启动，执行 `openspec validate add-sheen-material-profile --strict`。复用匹配的 T01/T02 数值及 D11 三路径证据，在当前版本定向核对实际受影响的基底、清漆和环境消费者。记录设备、驱动、配置、输入和产物身份及已跑、复用、未覆盖范围。在当前问题单按分级登记 B 建议档位，说明方向能量归一、环境代和阶段资源风险，交由维护者调度。 [构建说明](../../../../Docs/Build.md) · [B 档位](../../../../Docs/Developments/AgentModels.md#review-tier) · [签收问题单](../../../../build/d12-t03/final/review-signed.md)
- [x] 5.2 独立 B 核对实际待审输入、标准字段、逐灯与 IBL 配对、Shader 冷热准入、覆盖路由和环境候选。按受影响的第一方文件完善注释，记录问题、复现证据、代码与注释结论和覆盖限制。 [审查闭环](../../../../AGENTS.md#review-loop) · [B 首审与首次整改复核](../../../../build/d12-t03/final/review-signed.md)
- [x] 5.3 A 按问题编号整改和自检，重放被改动或缺证的风险，登记匹配输入及结果；独立 B 接收同一待验版本。 [首轮整改](#a-correction-verification) · [第二轮整改](#a-second-correction) · [签收问题单](../../../../build/d12-t03/final/review-signed.md)
- [x] 5.4 独立 B 验证整改并补齐注释，所有问题已通过或有依据撤回后签收最终版，记录实际审查配置、版本、复核范围与限制。 [B 最终签收](#b-second-review)

## 6. A · 最终验证、同步与归档

- [x] 6.1 对 B 签收的源码、包、产物及原生证据执行 OpenSpec verify。对影响范围或证据缺口重建或重放，核对七项能力与代表性读回，登记实际支持组合、验证与复用范围、档位建议及独立签收。 [验证范围](../../../../.agents/skills/physara-verification/SKILL.md) · [A 最终验证](#final-verification)
- [x] 6.2 最终验证通过后同步对应主规格并归档。核对严格校验、归档链接及计划状态。留存落签问题单并按模板重置当前问题单，开发计划登记 D12-T03 完成及依赖。 [工作包](../../../../Docs/Developments/D12-D13-Materials.md#d12-t03) · [归档记录](#archive-closeout)

<a id="a-local-verification"></a>
## A 本地验证与交接

2026-11-28，Windows x64、MSVC 19.44、Windows SDK 10.0.26100.0。实际读回使用 Release 的 Intel RaptorLake-S Mobile Graphics Controller，OpenGL 4.6.0，驱动 32.0.101.6129。三路径回放为 640×480 单采样、曝光 `f/4 · 1/16 s · ISO 100`、512 边长 PCF 阴影。Debug 首次触发 MSVC C1001，保留日志后同配置串行重试通过。最终 Debug、Release 正式产品和各 81 份配套包已重建并通过隐藏启动。 [Release 构建](../../../../build/d12-t03/product-Release.log) · [Debug 构建](../../../../build/d12-t03/product-Debug.log) · [C1001 记录](../../../../build/d12-t03/product-Debug-C1001.log)

- **导入、编辑与 v1**　真实 glTF/GLB 对照覆盖缺省与 required、同源 RGB/A 的独立用途、两套 UV 与变换及图像替换后的版本保活。越界、缺席 UV 及同材质的 unlit、specular-glossiness 各有定位拒绝。两实例对照验证显式零、继承、清除与 Undo/Redo。旧 v1 和含稀疏 sheen 的 v1 可重开，非法槽用途同时被编辑及 v1 候选拒绝。 [资产与文档](../../../../build/d12-t03/sheen-assets-Release.log)
- **ABI 与直接光**　材质行 528 B，t5 和 t11/s11、t12/s12 各自匹配。24 个 sheen 颜色配方均在实际设备完成角色准入，最重角色使用 16 个片元纹理单元，设备可用上限为 32。错开两套 UV 的 RGB 和线性 A 对照命中不同纹素。掠射窄波瓣的独立方向能量积分约为 3.10，归一后的红通道读值约 9.534，预期约 9.553。零粗糙度和共享下限的工作色一致，越出几何半球的标样交付零 sheen。 [角色准入](../../../../build/d12-t03/sheen-admission-Release.log) · [逐灯与有效 UV 对照](../../../../build/d12-t03/proof-direct.json)
- **环境产物与数值**　DFG 蓝通道两个实测点为 3.078125 和 0.193726，独立积分为 3.078356 和 0.193712。五级 Charlie 卷积与方向一阶矩对照。单位环境中，独立 sheen 的四个白炉样点最大通道绝对误差约 0.000293；+1 EV 缩放、180° 旋转、间接 AO 与清漆下的发射对照均通过。高亮案例绿、蓝通道读回 65,216 和 61,824，符合限幅前加权后的目标值。 [环境积分与读回](../../../../build/d12-t03/proof-indirect.json)
- **环境需求与有效代**　Direct 和 Physical DFG-only 的图中只读取共享方向表。天空查询增加一个天空读，启用物理环境后保留天空并增加 GGX 与 Charlie 各 30 次层面写入，实际 sheen 颜色读取 Charlie 的 5 mip、6 面。仅改变查询的朝向及强度时四项产品继续有效，纹理写入为零。换源等待期间保留旧 A 与 Charlie 准备原因，匹配产品就绪后正常提交确认 B。 [图读写](../../../../build/d12-t03/sheen-demand-Release.log) · [替换票据](../../../../build/d12-t03/sheen-ibl-Release.log)
- **三路径及历史**　Direct、Physical 各 17,328 个扩展像素的 HDR 在三路逐字节一致，各组完整 D32 和十层阴影逐字节一致。G1、G3、MASK 孔、共面胜出和有限远面对照通过。编辑、清除和 Undo 的扩展数为 `5 → 4 → 5`，Undo 恢复对应工作色和表面，非法编辑保留相容历史。实际 Inspector 在 Base Color 期间保留最近 sheen 受光票据，回到正常受光后更新。 [三路径读回](../../../../build/d12-t03/proof-paths.json) · [诊断面板](../../../../build/d12-t03/native/sheen-inspector-diagnostic.txt) · [恢复后的面板](../../../../build/d12-t03/native/sheen-inspector-accepted.txt)
- **热重载预算**　原 16 MiB 额度在 16,727,376 B 时无法纳入下一包，代 2 保持有效。采用 20 MiB 后真实兼容源码修改发布代 3，当时活动与候选共驻 153 项、20,189,508 B。 [原配额拒绝](../../../../build/d12-t03/sheen-reload-16MiB.log) · [合法发布](../../../../build/d12-t03/sheen-reload-Release.log)
- **冷包、未选热角色及恢复**　真实编译的缺项包在对应片元阶段定位 `sheenRoughnessImage`。同一 OPAQUE 标样的独立表面只准备 1 个材质图像，受光恢复后使用 3 个，包含 RGB 和线性 alpha 的两个解释。配对 G1、G3、D32 逐字节相同。未选热角色拒绝期间代 1 及相容历史保持有效，合法候选后 Deferred 受光正常接受。 [冷、热回放](../../../../build/d12-t03/replay-fault-Release.log) · [独立表面对照](../../../../build/d12-t03/proof-fault.json)
- **既有行为**　重放 D11 混合表面及 IOR/specular、clearcoat 独立材质的两类光照三路径。覆盖、D32、现有 G1/G3 判据、阴影和正常受光结果满足对应精度要求。 [D11 比较](../../../../build/d12-t03/native/mixed-proof.json) · [既有扩展比较](../../../../build/d12-t03/proof-legacy-routes.json)
- **证据与限制**　OpenSpec 1.13.2 严格校验通过。11 项 Release 原生模式在同一正式产物及同一最终探针上各有通过记录。归集时逐灯模式出现一次 `0xC0000005`，发生在普通视角的 Direct 读回之后。随后同一探针在调试器下及原运行方式单独复跑均正常退出，逐灯数值重新核对通过。异常原因待定位，原输出已保留。 [严格校验](../../../../build/d12-t03/openspec-validate.json) · [归集回放](../../../../build/d12-t03/replay-delivery-Release.log) · [异常记录](../../../../build/d12-t03/sheen-light-AV.log) · [调试复跑](../../../../build/d12-t03/sheen-light-debugger.log) · [原方式复跑](../../../../build/d12-t03/replay-light-retry-Release.log)

A 建议 B 审查档位 Y（GPT-6.2 Astra `high`）。方向反照率归一与上层透过、环境候选同代交付、活动 Shader 绑定及暖缓存资源读均可独立复核。待审工作树、正式产物、探针及读回按 SHA-256 登记。 [待审输入](../../../../build/d12-t03/handoff.json) · [签收问题单](../../../../build/d12-t03/final/review-signed.md#本轮交接)

<a id="a-correction-verification"></a>
## A 首审整改验证

2026-11-28，基准 `45c40932aa4208e2335a57a52467780d436132af`，整改版包含 B 首审注释。Windows x64/MSVC、Debug 和 Release 正式构建及各 81 份正式包、两次隐藏启动、OpenSpec 1.13.2 严格校验均通过。Release 使用 Intel RaptorLake-S Mobile Graphics Controller、OpenGL 4.6.0、驱动 32.0.101.6129。 [整改版 SHA-256 清单](../../../../build/d12-t03/review-a/handoff.json) · [Debug 构建](../../../../build/d12-t03/review-a/product-Debug.log) · [Release 构建](../../../../build/d12-t03/review-a/product-Release.log) · [OpenSpec 校验](../../../../build/d12-t03/review-a/openspec-validate.json)

- **CR-001**　独立半球积分、正式 DFG 读值和两份 HDR 互相校正。掠射端归一后直接能量 `1.00116`，单位环境 `0.99961`；内部对照分别为 `0.99903`、`0.99961`。复跑两个内部 DFG 数值点、五级 Charlie 方向矩、AO、亮区及粗糙度下限。 [边缘数值](../../../../build/d12-t03/review-a/cr001-proof.json) · [逐灯](../../../../build/d12-t03/review-a/proof-direct.json) · [间接](../../../../build/d12-t03/review-a/proof-indirect.json)
- **CR-002、CR-003**　两个同布局正式包交叉放置都在产生颜色前定位拒绝，恢复后接受各自角色。未选组合角色保持活动名时实际配对达到 `36/32`，在整代发布前拒绝，原代保有可读票据并续接相容历史，合法候选发布后正常出图。原有缺 `sheenRoughnessImage` 的冷、热拒绝及恢复复跑通过，独立表面恢复前后的 G1、G3、D32 逐字节一致。 [冷包](../../../../build/d12-t03/review-a/cr002-role-Release.log) · [未选组合包](../../../../build/d12-t03/review-a/cr003-pairs-Release.log) · [独立表面](../../../../build/d12-t03/review-a/proof-fault.json)
- **CR-004**　本地时序钩子只延迟实际 Charlie worker，观察有效色归零和同源仅天空时作业取消及各自的首张正常票据。两 View 时普通环境先接受，另一 View 仍保留同序号 Charlie 作业并在完成后接受。重新验证需求图读写、同源查询复用与换源后完整接受。 [受控时序](../../../../build/d12-t03/review-a/cr004-consumers-Release.log) · [图读写](../../../../build/d12-t03/review-a/sheen-demand-Release.log) · [环境替换](../../../../build/d12-t03/review-a/sheen-ibl-Release.log)
- **CR-005、CR-006**　实际 Inspector 在同帧独立 sheen、组合、独立清漆与视锥外实例之间切换，票据 65 的角色随目标匹配，Base Color 票据 67 保留对应受光资料。既有 clearcoat 的诊断和跨场景复用通过。坏 UV 夹具定位到 Primitive 的 `attributes`，注明 sheen 颜色槽所需的有效 `TEXCOORD_1`。 [Inspector 选择](../../../../build/d12-t03/review-a/cr005-selection-Release.log) · [清漆回归](../../../../build/d12-t03/review-a/cr004-inspector-Release-verified.log) · [导入](../../../../build/d12-t03/review-a/sheen-assets-Release.log)
- **复用回放及范围**　Direct、Physical 各 17,328 个扩展像素的 HDR 在三路径逐字节一致，D32 与十层阴影也一致。有效色编辑、清除与 Undo 恢复配套 HDR 和 G3。实际 `SourceReload` 完整候选发布代 3，活动与候选同驻 `153` 项、`20,217,372/20,971,520 B`。本轮 14 个 Release 模式各有通过记录。清漆历史回放补齐原 T02 夹具后通过，初次缺失夹具的输出留存。首轮逐灯访问冲突本次未复现，原异常原因待定位；原生范围为这一台 Intel OpenGL 设备。 [三路径](../../../../build/d12-t03/review-a/proof-paths.json) · [源码热重载](../../../../build/d12-t03/review-a/sheen-reload-Release.log) · [定向回放](../../../../build/d12-t03/review-a/review-first-Release.log) · [既有回放](../../../../build/d12-t03/review-a/regression-Release.log) · [清漆补齐后复跑](../../../../build/d12-t03/review-a/regression-inspector-retry.log) · [首轮访问冲突](../../../../build/d12-t03/sheen-light-AV.log)

CR-001 至 CR-006 已登记 A 自检，交独立 B 复核代码、注释及当前输入。B 的任务勾选和验收结论由 B 会话填写。 [签收问题单](../../../../build/d12-t03/final/review-signed.md)

<a id="a-second-correction"></a>
## A 第二轮整改验证

2026-11-28，基准 `45c40932aa4208e2335a57a52467780d436132af`，含 B 首审和首次复核注释。Debug、Release 正式构建与隐藏启动均通过，各 81 份 Shader 包摘要与 B 首次复核输入匹配。OpenSpec 1.13.2 严格校验通过。原生回放使用 Release、Intel RaptorLake-S Mobile Graphics Controller、OpenGL 4.6.0 和驱动 32.0.101.6129。 [当前交接清单](../../../../build/d12-t03/review-a2/handoff.json) · [启动退出码](../../../../build/d12-t03/review-a2/startup-exits.log) · [严格校验](../../../../build/d12-t03/review-a2/openspec-validate.json)

- **CR-005**　实际不透明受光计划接受后才推进 View 的叠层快照。四级阴影诊断票据 92 保留受光票据 91。诊断请求退回正常受光时，实际无阴影程序随票据 93 确认。离开诊断后继续推进至票据 94。复跑同帧独立 sheen、组合与清漆实例的 Inspector 匹配。 [阴影诊断与正常退路](../../../../build/d12-t03/review-a2/cr005-shadow-Release.log) · [实例选择](../../../../build/d12-t03/review-a2/cr005-selection-Release.log)
- **CR-007**　设备绑定预检和实际 Pipeline 创建共用帧前额度与累计耗时，活动代优先；新修订撤销旧进度。低额度探针记录每次一项、同次补备遵守已计额度、迟到回执零工作及晚阶段未选角色的 `36/32` 拒绝。合法恢复在最后一项完成后才发布并接受正常颜色。实际 `SourceReload` 的两次发布分别在 24 个帧前轮次完成 24 项设备检查，代 3 的活动和候选共驻 `153` 项、`20,217,372/20,971,520 B`。 [预检及发布](../../../../build/d12-t03/review-a2/cr007-budget-Release.log) · [逐帧热重载](../../../../build/d12-t03/review-a2/sheen-reload-Release.log)
- **CR-008**　主路及离屏路分别在快照最终分配点注入一次 `std::bad_alloc`，故障路报告 `ScenePreparationFailed`、保留旧可读输出与票据，同帧另一条路仍接受。解除注入后两路均恢复提交，故障路接续各自的相容历史。 [双 View 故障与恢复](../../../../build/d12-t03/review-a2/cr008-view-allocation-Release.log)
- **回归与限制**　三路径 Direct 和 Physical 各 17,328 个扩展像素 HDR 逐字节一致，D32 与十层阴影一致。缺绑定冷、热包仍拒绝，合法恢复前后的独立表面 G1、G3、D32 逐字节相符。旧回放曾在活动代仍报告冷包问题时提前结束；调整本地探针以推进跨帧候选后，七项模式在同一最终探针上均通过。本轮资源不足验证限于登记的分配点和本机设备，首轮逐灯访问冲突原因仍待定位。 [三路径读回](../../../../build/d12-t03/review-a2/proof-paths.json) · [缺绑定读回](../../../../build/d12-t03/review-a2/proof-fault.json) · [定向回放](../../../../build/d12-t03/review-a2/review-Release.log) · [既有回放](../../../../build/d12-t03/review-a2/regression-Release.log) · [原探针提前退出记录](../../../../build/d12-t03/review-a2/sheen-fault-initial-Release.log)

三项复核发现已完成 A 整改与自检。第二轮源码、正式产物及回放证据交独立 B 核对，5.4 由 B 确认。 [签收问题单](../../../../build/d12-t03/final/review-signed.md)

<a id="b-second-review"></a>
## B 第二次复核与签收

2026-11-28，维护者指定的独立 B 会话签收 D12-T03。首轮复核已关闭 CR-001 的掠射方向能量、CR-002 的冷包角色身份、CR-003 的未选热角色设备上限、CR-004 的环境子产品取消及 CR-006 的 UV 诊断。本轮关闭 CR-005、CR-007、CR-008，八项全部通过。A 建议 Y 档；会话未提供可核实的实际档位、模型细分和推理级别。 [签收问题单](../../../../build/d12-t03/final/review-signed.md)

| 编号 | B 独立复核 |
|---|---|
| CR-005 | 受光票据 72 经四级阴影诊断 73 后仍可查询。保留诊断请求时，实际交付的正常受光退路以 74 更新快照，退出诊断后继续接受 75。 [回放](../../../../build/d12-t03/review-b2/cr005-shadow.log) |
| CR-007 | 预算为 1 时，部分请求、末项超限和合法恢复合计 51 次预检均逐次计费。迟到回执执行零项，末项超限保留可提交的代 1，合法候选全部完成后代 2 才发布。 [逐步记录](../../../../build/d12-t03/review-b2/cr007-budget.log) |
| CR-008 | 分别在主路与离屏路的叠层快照分配点注入失败。故障路保有旧输出，另一 View 正常提交；解除注入后分别以 72 接续 70、以 74 接续 72。 [双 View 回放](../../../../build/d12-t03/review-b2/cr008-view-allocation.log) |

B 编辑前核对 A 登记的 90 项工作树输入及正式产品、包、库和回放证据。三项定向实跑使用同一 Intel OpenGL 4.6 设备和已核对的 Release 探针，均返回 0。三路径矩阵及真实 SourceReload 复用 A 的本轮证据。系统级内存压力与其它设备未覆盖，历史逐灯访问冲突原因仍未确认。B 校正两处收尾注释后登记基准 `45c40932aa4208e2335a57a52467780d436132af` 上的 77 项实现输入。签收清单 SHA-256 为 `4b82c64584bdff44ea9823fa70ed10f8333af87a83b5bedf5d6b74152265192f`。A 的最终验证、规格同步及归档按第 6 节继续。 [签收清单](../../../../build/d12-t03/review-b2/signed-inputs.json)

<a id="final-verification"></a>
## A 最终验证

2026-11-28，以 `v2.0@45c40932aa4208e2335a57a52467780d436132af` 和 B 签收工作树为准。签收清单的 77 项实现输入、3 份 B 原生记录及 A 交接清单摘要全部匹配。A 交接的 515 条记录中，512 条字节及 SHA-256 匹配。余下三条对应 B 更新的问题单、tasks 以及两处经核对的 `Forward.cpp` 注释。B 验收 CR-001 至 CR-008 的代码和注释，A 建议审查 Y 档（GPT-6.2 Astra `high`），实际档位、模型和推理级别待维护者登记。 [B 签收](#b-second-review) · [输入核对](../../../../build/d12-t03/final/signature-verification.json) · [注释差异](../../../../build/d12-t03/final/signed-forward-comment-delta.diff)

接受范围为 lit metallic-roughness 的 OPAQUE/MASK，支持独立 sheen、IOR/specular 基底及与清漆叠层。528 B 材质行保留 t5 并增加 t11/s11 和 t12/s12。24 个正式颜色角色覆盖全灯与 Froxel、Direct、Physical DFG-only、Physical IBL 及阴影组合。实际设备最重角色使用 16 个片元纹理单元，设备上限为 32。Deferred 继续使用四个 GBuffer 附件。 [设计](design.md) · [正式包反射和设备准入](../../../../build/d12-t03/final/sheen-admission-Release.log)

签收清单所列 Debug、Release 正式程序、库和各 81 份包继续作为产品输入，两种配置均重新通过 `--hidden --poll-limit=3`。在隔离目录从签收的 `Forward.cpp` 重编观测探针，链接核对过的 Release 库，八项原生回放均通过。设备为 Intel RaptorLake-S Mobile Graphics Controller、OpenGL 4.6.0、驱动 32.0.101.6129，三路径采用 640×480 单采样和同一预曝光。 [启动记录](../../../../build/d12-t03/final/startup-exits.log) · [最终回放](../../../../build/d12-t03/final/replay-Release.log) · [验证清单](../../../../build/d12-t03/final/verification-manifest.json)

| 能力与场景 | 实现及验收证据 |
|---|---|
| [静态 glTF 导入](specs/static-gltf-import/spec.md)（2 条需求、7 个场景） | 资产入口解析 required、缺省、线性因子、sRGB RGB 与线性 A、两套有效 UV 及变换。实跑覆盖同图两种用途、依赖替换保活、缺失 TEXCOORD_1、越界及不兼容扩展。未知 required 和有限核心回退沿用已有验收。 [导入实现](../../../../Engine/Asset/GltfData.cpp) · [本轮资产回放](../../../../build/d12-t03/final/sheen-assets-Release.log) · [既有扩展验收](../2026-11-16-import-static-gltf-models/tasks.md#final-verification) |
| [实例编辑](specs/editor-domain-edits/spec.md)（1 条需求、8 个场景） | shared source 的独立编辑、覆盖清除、Undo/Redo 和非法编辑沿 Scene 合同。独立、组合与清漆身份使用签收回放；四级阴影诊断票据 101 保留同目标受光票据 100，实际正常退路以 102 更新。两路快照分配失败时，对端仍能接受，故障路解除注入后接续既有受光历史。 [有效材质实现](../../../../Engine/Scene/SceneMaterial.cpp) · [同帧实例](../../../../build/d12-t03/review-a2/cr005-selection-Release.log) · [诊断与退路](../../../../build/d12-t03/final/cr005-shadow-Release.log) · [双 View](../../../../build/d12-t03/final/cr008-view-allocation-Release.log) |
| [场景 v1](specs/scene-document-persistence/spec.md)（1 条需求、2 个场景） | 稀疏读写核对源继承、显式零及两套独立采样配置。旧 v1 和新文档可重开；非法用途候选保留活动文档，拒绝位置含实例与槽名。 [v1 实现](../../../../Editor/Session/SceneDocumentRead.cpp) · [本轮资产回放](../../../../build/d12-t03/final/sheen-assets-Release.log) |
| [共享表面](specs/static-surface-evaluation/spec.md)（2 条需求、5 个场景） | 逐灯读回重算几何半球、粗糙度下限、独立纹理槽与清漆透过。`NoV=0.02, p=0.08` 的红通道归一读值约 9.548，预期约 9.553。包摘要匹配的边缘对照在 `NoV=0.0001, p=0.045` 记录方向能量 1.00116，B 独立积分确认容差。既有双法线与无 sheen 清漆响应沿用 T02 验收。 [波瓣实现](../../../../Shaders/Lighting/OpaqueLighting.glsl) · [数值复算](../../../../build/d12-t03/final/proof-direct.json) · [边缘对照](../../../../build/d12-t03/review-a/cr001-proof.json) · [清漆基线](../2026-11-28-add-clearcoat-material-profile/tasks.md#final-verification) |
| [环境照明](specs/environment-lighting/spec.md)（2 条需求、4 个场景） | 重算两处 DFG 与五级 Charlie 方向矩，核对单位环境、光照反转、AO、预曝光和工作色限幅。实际换源等待保留上一正常票据，完整环境就绪后接受。沿用同包的 DFG/GGX/Charlie/sky 图读写和双 View 取消时序证据。 [缓存实现](../../../../Engine/Renderer/Lighting/EnvironmentCache.cpp) · [间接复算](../../../../build/d12-t03/final/proof-indirect.json) · [替换回放](../../../../build/d12-t03/final/sheen-ibl-Release.log) · [图消费](../../../../build/d12-t03/review-a/sheen-demand-Release.log) · [分步取消](../../../../build/d12-t03/review-a/cr004-consumers-Release.log) |
| [三路径交付](specs/static-forward-rendering/spec.md)（3 条需求、6 个场景） | Direct、Physical 各 17,328 个扩展获胜像素的 HDR 三路逐字节一致，完整 D32 和十层阴影逐字节一致。MASK 孔、G1、G3、等深及有限远面和清除/Undo 对照通过。独立表面在缺色包时可交付，合法恢复前后 G1、G3、D32 一致。独立 IOR/specular 与清漆行为沿用其签收记录。 [当前路径读回](../../../../build/d12-t03/final/proof-paths.json) · [缺绑定读回](../../../../build/d12-t03/final/proof-fault.json) · [IOR/specular 基线](../2026-11-28-add-ior-specular-material-profile/tasks.md#final-verification) · [清漆基线](../2026-11-28-add-clearcoat-material-profile/tasks.md#final-verification) |
| [Shader 准入与重载](specs/shader-generation-reload/spec.md)（1 条需求、2 个场景） | 核对正式包角色、实际两阶段 IO/绑定及有无 IBL 和阴影的配方，重跑冷包与未选热包缺项及恢复。签收证据覆盖 Direct/Physical 错包、36 对采样拒绝、低额度跨帧进度及旧代保活。真实 SourceReload 两次候选各用 24 帧完成 24 项设备检查，代 3 的共驻为 153 项、20,217,372/20,971,520 B。 [准入实现](../../../../Engine/Renderer/Paths/ForwardShaderPreparation.cpp) · [缺项回放](../../../../build/d12-t03/final/sheen-fault-Release.log) · [错包](../../../../build/d12-t03/review-a/cr002-role-Release.log) · [B 预算及限额](../../../../build/d12-t03/review-b2/cr007-budget.log) · [源码热重载](../../../../build/d12-t03/review-a2/sheen-reload-Release.log) |

OpenSpec 1.13.2 严格校验通过。七项能力的 12 条需求及 34 个场景已对应实际消费者与本轮或可复用证据。原生读回和分配点注入范围为上述 Intel OpenGL 设备及单采样配置。早期逐灯访问冲突在最终回放中未复现，原因仍待定位。 [严格校验](../../../../build/d12-t03/final/openspec-change-validation.json) · [原异常记录](../../../../build/d12-t03/sheen-light-AV.log) · [最终逐灯回放](../../../../build/d12-t03/final/sheen-light-Release.log)

<a id="archive-closeout"></a>
## 规格同步与归档

- 已同步七份增量，新增七条需求并更新五条需求。同步后 45 份主规格和本变更均通过严格校验，归档后 45 份主规格复验通过。 [同步摘要](../../../../build/d12-t03/final/spec-sync.json) · [同步后主规格校验](../../../../build/d12-t03/final/openspec-sync-validation.json) · [同步后变更校验](../../../../build/d12-t03/final/openspec-post-sync-change-validation.json) · [归档后主规格校验](../../../../build/d12-t03/final/openspec-main-validation.json)
- 2026-11-28 归档 `add-sheen-material-profile`，OpenSpec 活动列表为空。开发计划登记 D12-T03 完成，D12-T04 的既定前置已满足。 [归档回执](../../../../build/d12-t03/final/openspec-archive.json) · [活动列表](../../../../build/d12-t03/final/openspec-post-archive-list.json) · [开发计划](../../../../Docs/Developments/Developments.md#current-stage) · [D12-T04](../../../../Docs/Developments/D12-D13-Materials.md#d12-t04)
- 归档前后的实现输入与签收版本一致。核对归档路径、同步内容、任务状态及本地链接后，按模板重置当前问题单。B 签收的问题单保留原始字节，SHA-256 为 `ca04e6978c5c11a1b7d5d6cd5e1f4d56a35d924d1bbc37402c`。 [签收问题单](../../../../build/d12-t03/final/review-signed.md) · [重置前核对](../../../../build/d12-t03/final/archive-before-reset.json)
