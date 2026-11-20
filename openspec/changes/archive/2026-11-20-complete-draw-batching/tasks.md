# Tasks

## 1. 公共 RHI 和 OpenGL indexed indirect

- [x] 1.1 接通普通 `isDrawIndirectArgs` Buffer、实际 GL 入口检查及现有 `IndirectArgument` 跟踪。核对公开参数记录的大小、字段偏移与符号，验证 raw 与 Validation 创建得到相同描述，排除未交付的命令和用途。随原生支持更新并核对 [RHI 能力表](../../../../RHI/README.md#opengl-原生消费边界)。 [资源规范](specs/opengl-device-resources/spec.md)
- [x] 1.2 接通有效图形状态、indirect 绑定缓存与单次/多次间接命令。用一份可读回的固定几何核对非零参数 byte offset、firstIndex、baseVertex 与 firstInstance，并覆盖 R16/R32 索引宽度及单记录和 MDI 的原生调用数。沿 raw 和 Validation 检查改变的信任边界及合法空调用，保留直接 indexed 的非零索引绑定偏移。 [执行规范](specs/opengl-immediate-draw/spec.md)
- [x] 1.3 复用现有录制同步和退休路径。使用同一参数 Buffer 的更新对照 Automatic 与 Explicit，观察匹配的数据、现有动态常量范围和缺失依赖时的拒绝。用已发批次后释放参数句柄的真实完成或合法前缀退休验证新增保活边。沿用证据所覆盖的其它提交边界。 [执行规范](specs/opengl-immediate-draw/spec.md)

## 2. 图对间接参数的生产与访问

- [x] 2.1 增加 Buffer 的独立 `IndirectArgument` 读用途，接通描述校验、版本/区间、状态映射及 Pass 查询。用一条双消费者和独立裁剪范围的上传图证明生产顺序和活性，用紧邻非法范围及错误角色/用途对照验证拒绝。 [图增量](specs/render-graph-contract/spec.md)
- [x] 2.2 在已有 GraphManaged 入口核对每次间接命令的实际参数范围和用途。用上传后绘制的原生读回证明正确版本到达消费者，用错误用途或范围的同图对照证明相应 Draw 未发出。 [图增量](specs/render-graph-contract/spec.md)

## 3. Forward 排序、实例数据与批次

- [x] 3.1 在现有 Shader 接口生成对象、材质及实例表，从同一有效行消费对象变换和 depth/color 的材质数据。保留覆盖、镜像双面及光照数值，核对生成端与阶段反射的偏移、stride、extent、flat 材质索引与设备限额。用固定带区分标记的实例、变换和材质行证明对应关系。 [设计](design.md#实例表驻留行与索引单位) · [驻留规范](specs/static-gpu-residency/spec.md)
- [x] 3.2 在现有可见集合中按完整有序状态准备批次和实例区间，从共享几何页生成经校验的索引元素起点和有符号基顶点。用超过 256 个有效材质身份、同简化键的不同资源及实际兼容的同纹理数值覆盖核对顺序、分段、行索引和完整批次键。 [Forward 规范](specs/static-forward-rendering/spec.md)
- [x] 3.3 把实例表、参数和页范围接入场景上传封存、准入、图消费和安全槽退休，保留逐行静态复用与每 View 独立实例数据。实际 Depth/Color 只消费准备好的状态及固定数量间接命令。用一份固定场景核对无前置深度与有前置深度的颜色/深度结果，并验证一个迟发准备拒绝不进入正常录制。 [Forward 规范](specs/static-forward-rendering/spec.md) · [驻留规范](specs/static-gpu-residency/spec.md)

## 4. 统计与产品入口验收

- [x] 4.1 从已执行的几何 Pass、设备原生计数和实际上传发布只读快照。对照一批多记录及一记录多实例的情况核对表面、批次、间接记录、真正的 Draw/绑定和上传次数及字节。验证失败或仅安全 UI 候选保持原快照及面板显隐时的同一口径。 [统计规范](specs/render-frame-statistics/spec.md)
- [x] 4.2 构建受影响的正式 Debug/Release 目标，并从正式入口运行固定输入的基本绘制与启动检查。用实际 OpenGL 回放同序直接与批次结果，比较有效像素、遮挡/覆盖、计数与上传。复用同一输入的对照检查两个 View、暖帧槽轮转和一次局部材质变化的影响。 [验证范围](#verification-scope) · [构建入口](../../../../Docs/Build.md)

## 5. A 本地交付与独立审查

- [x] 5.1 A 保存工作树、关联输入与构建产物身份、实际命令、复用证据及可见限制，在 [B 签收原件](../../../../build/d08-t03/final/review-signed.md) 交给维护者调度独立 B。以已登记且能匹配当前源码的检查作为交接证据。
- [x] 5.2 B 独立审查实现及直接消费者，按约定补全本轮涉及的第一方文件注释，在问题单登记 CR、结论及对应源码版本。首审登记 CR-001 至 CR-005。 [B 签收原件](../../../../build/d08-t03/final/review-signed.md#问题与整改)
- [x] 5.3 A 对 B 的意见完成整改和受影响合同的本地复验，在原 CR 项记载证据与复核条件。 [第 1 轮整改与复验](#a-correction-1) · [第 2 轮整改与复验](#a-correction-2) · [第 3 轮整改与复验](#a-correction-3)
- [x] 5.4 B 复核最终实现、实际证据与注释，关闭 CR-001 至 CR-007 并签收第 3 轮交回的源码版本。 [B 签收与复核证据](#b-recheck-3) · [审查流程](../../../../AGENTS.md#review-loop)

## 6. 最终验证、同步与归档

- [x] 6.1 B 签收后以当前变更工件 verify 同一签收源码和构建产物，核对匹配证据并补跑已变更或缺失的原生检查，记录设备范围、结论和 OpenSpec strict 结果。 [最终验收](#final-verification)
- [x] 6.2 将已验收 delta 同步到现有主规范，检查受影响要求与保留场景的完整性，并保存主规范 strict 结果。 [规范同步记录](#spec-sync)
- [x] 6.3 按原生流程归档，核对开发计划与归档引用，保留审查签收和验收证据后按模板重置当前问题单。以真实归档状态和可解析的引用确认收尾。 [归档收尾](#archive-closeout)

<a id="verification-scope"></a>
## 验证范围与停止条件

| 风险 | 最小充分证据 | 停止条件 |
|---|---|---|
| 角色、参数单位或窄化在 GL 边界错用 | 同一原生数据用例包含前置哨兵、两段不同几何、非零参数偏移及基值，单记录和多记录各有可区分的像素及调用数。用其中的非法状态和范围对照复验 raw/Validation。 | 非法命令无绘制，已发路径、记录顺序、R16/R32 和零数量各有相应观察。复用既有直接偏移检查。 |
| 完整键比较、材质索引宽度或阶段 ABI 失配 | 用高位不同的材质身份、两种同纹理数值和不同 sampler/几何页证明兼容决策。核对生成 ABI 与实际 Shader 读取，使用同一有序表面记录的直接绘制作读回对照。 | 相邻兼容项才形成批次，真实覆盖、排序及材质结果匹配已知值，块分段仍保持序列。 |
| 同对象的权限、数据更新、确认或完成被错误复用 | 复用一个有范围和裁剪的图用例，另看真实参数更新与已发批次的完成。用正式 Runtime 的两路候选、暖槽和一次材质编辑核对私有实例数据、共享页及保留确认。 | 未写间隙和错误用途不能发出 Draw，实际消费者读到目标版本，退休及失败候选保活有证据。 |
| 统计单位导致把一个批次当多个原生调用 | 以同输入和画质的回放同时核对 View 表面数、执行批次和记录，配对整图实际 Draw、绑定、上传及其字节。 | 计数可追到实际阶段和已接受身份。只报告已测量项，性能比较另用同口径测量。 |

检查优先复用匹配的现有用例。为已确认的缺口新增开发用例时，放在 `build/d08-t03/`，包含可独立判定的预期值。于本文件记录当前实际命令、输入与产物身份、结果、可复用范围及未覆盖的风险。 [验证规则](../../../../.agents/skills/physara-verification/SKILL.md)

<a id="a-local-checks"></a>
## A 本地检查与交接

- 分支 `v2.0`，基准提交 `41cba91c8c50d9cd2c0597ad736203e81eb97b26`。30 个变更的实现输入及其 binary diff 已登记，SHA-256 为 `9592b97c896fac21cafc6403bc24f0d749bdaffb862af07f98fbf8ee7d80ac88`。无暂存输入，本 change 的规划文件为新增。清单记录 5 次实际执行、73 个产物、参考场景、探针及编译命令身份。 [源码差异](../../../../build/d08-t03/source.diff) · [A 本地证据](../../../../build/d08-t03/a-local-evidence.json)
- 在 Windows x64 的正式 MSVC Debug/Release 目标上重新构建并各执行 `Physara.exe --hidden --poll-limit=5`，均退出 0。部分编译出现 MSVC `C1001`，以相同编译选项和 MSBuild 单文件任务调度重试后通过。原始错误、最终编译及启动日志均保留。OpenSpec change 严格校验通过。 [构建及回放入口](../../../../build/d08-t03/Verify.ps1) · [Debug 构建](../../../../build/d08-t03/final-build-Debug.log) · [Release 构建](../../../../build/d08-t03/final-build-Release.log) · [C1001 记录](../../../../build/d08-t03/final-build-Release-C1001.log) · [strict](../../../../build/d08-t03/validate-change.json)
- `Indirect` 通过 Validation R16 Automatic 和 raw R32 Explicit 的实际像素读回，区分非零参数偏移、firstIndex、正负 baseVertex、firstInstance 和连续更新的动态常量。单条间接与 MDI 各记一次原生命令。越界、缺失 Explicit 用途和不支持的间接命令均拒绝，释放参数句柄后实际完成才退休。GraphManaged 复验两个消费 Pass 共用一个存活参数生产者，以及物理容量内但越出授权的请求在前缀绘制后被拒绝。 [间接与图回放](../../../../build/d08-t03/Indirect-Release.log)
- `BatchForward` 核对 545 个表面跨越 512 行的实例、对象和材质块。两个颜色批次包含四条记录，原生发出一次 MDI 和一次单条间接命令。按同序展开的直接实例绘制与原生批次的工作色、D32 读回逐值相等，前置深度组合保留 masked 覆盖与等深次序。同一采样视图的 repeat/clamp 分成两批并分别读回红、蓝。相同安全槽的暖帧静态和批次写入均为 0。 [批次回放](../../../../build/d08-t03/BatchForward-Release.log)
- `BatchRuntime` 由当前 Runtime 帧编排和已登记参考场景重放。主 View 的 12 个表面使用 1 个颜色批次、10 条间接记录。整图记录 14 次原生 Draw、106 次绑定，两个 View 的批次数据实际上传 4 次共 8,592 B。三个安全槽完成暖机后静态和批次上传均为 0，局部材质修改仅编码和写入一行。裁剪 View 独立，失败前缀保留原快照及输出并在后续正常提交恢复，迟发显示准备失败走安全 UI。 [Runtime 回放](../../../../build/d08-t03/BatchRuntime-Release.log)
- 原生定向回放使用 Release 和 Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129。统计面板显隐核对的是只读消费路径，Runtime 取样和接受条件独立于该面板。直接绘制对照使用本地探针展开记录并读回，证据用于像素、索引与调用数核对。 [B 签收原件](../../../../build/d08-t03/final/review-signed.md)

<a id="a-correction-1"></a>
## A 第 1 轮整改与复验

- 核对 B 注释版的 30 个实现文件及 binary diff `f9197bef2ca9521b17ecf4e362bd37eb46331c99875e8230f6506a76de366881`，登记 B 的原始探针及结果。整改版含 35 个实现输入，基准 HEAD 与前次一致，binary diff SHA-256 为 `0e0ec6435f0cf22683800b781d04f70a3a01853c85a93353bad2d10110d74fb4`。清单对应 9 次实际执行及其库、Shader、输入和可执行产物。 [B 交回版本](../../../../build/d08-t03/a-correction-1/b-baseline.json) · [A 整改清单](../../../../build/d08-t03/a-correction-1/a-correction-evidence.json)

| 项目 | 本轮实测结果 |
|---|---|
| CR-001 | B 原始 `GraphState` 重连整改版，两入口都返回图失败与诊断，各只发出一次有效前缀 Draw。 [日志](../../../../build/d08-t03/a-correction-1/GraphState-Release.log) |
| CR-002 | `ForwardRows` 及 `--culled-view-only` 的暖主路和裁剪帧均无静态写。附加路可见时补写两行共 480 B 并读回正确像素。545 表面、前置深度及 sampler 对照继续通过，Runtime 双 View 上的局部材质修改仍只写入一次。 [表页复验](../../../../build/d08-t03/a-correction-1/ForwardRows-Release.log) · [原裁剪入口](../../../../build/d08-t03/a-correction-1/ForwardRows-culled-Release.log) |
| CR-003 | 两入口允许参数缓冲的间接用途，普通 Buffer 和 Texture2D 均拒绝。与着色读取的组合请求得到相同的角色判定，拒绝后的实际状态查询不含间接用途。 [StateRoles](../../../../build/d08-t03/a-correction-1/StateRoles-Release.log) · [CombinedRoles](../../../../build/d08-t03/a-correction-1/CombinedRoles-Release.log) |
| CR-004 | 核对正式 Shader 源码的梯度取得、逐实例分歧与丢弃顺序。原生混合材质用例在通道预览及直接光中各用一条双实例命令，对照两次独立直接 Draw 和前置深度，工作色与深度逐值一致。MASK 内样点深度为 0 与 0.5，幸存通道读回 G=0.501953、B=0.399902，符合 UV1 仿射梯度选择的平均 mip。 [梯度复验](../../../../build/d08-t03/a-correction-1/ForwardGradients-Release.log) |
| CR-005 | 实际双 View 快照经正式 ImGui 面板渲染，捕获所选 View 的 `View surfaces 12` 和整图 `Graph Forward instance and command uploads: 4 writes / 8592 B`。同一回放验证共享上传、暖槽复用、裁剪及失败前缀保留快照。 [Runtime 复验](../../../../build/d08-t03/a-correction-1/BatchRuntime-Release.log) |

- 正式 Debug/Release 用原编译选项构建并经 `Physara.exe --hidden --poll-limit=5` 启动，均退出 0。两种构建及统计探针出现过 MSVC `C1001`，原日志留存，原配置重试通过。定向 Release 原生回放限于 Windows x64、Intel RaptorLake-S Mobile 和 OpenGL 4.6.0 Build 32.0.101.6129。OpenSpec strict 通过。 [复验入口](../../../../build/d08-t03/a-correction-1/Recheck.ps1) · [strict](../../../../build/d08-t03/a-correction-1/validate-change.json) · [B 签收原件](../../../../build/d08-t03/final/review-signed.md)

<a id="b-recheck-1"></a>
## B 第 1 次整改复核

- **复核版本**　基准 HEAD 为 `41cba91c8c50d9cd2c0597ad736203e81eb97b26`。B 独立重算整改版 35 个实现输入的差异摘要 `0e0ec6435f0cf22683800b781d04f70a3a01853c85a93353bad2d10110d74fb4`，核实登记的输入、库、Shader 包和可执行文件。原生复核使用 Windows x64、Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129。正式 Debug/Release 构建与隐藏启动沿用 A 的对应记录。 [A 整改清单](../../../../build/d08-t03/a-correction-1/a-correction-evidence.json) · [B 运行清单](../../../../build/d08-t03/b-recheck-1/results.json)
- **关闭 CR-001 至 CR-005**　B 复跑 6 个 Release 探针。两条 RHI 入口都只发出 1 次合法前缀 Draw。裁剪帧原生上传为 0，附加路可见后补写两行并读回正确颜色。单一及组合的间接用途均按角色拒绝非法资源。混合材质的两种深度路径与逐表面直接绘制逐值相同。面板实际输出可区分所选 View 的 12 个表面与整图的 4 笔、8,592 B 上传。额外的表范围检查确认未定义的整表绑定和越出声明行的查询均拒绝。 [B 签收原件](../../../../build/d08-t03/final/review-signed.md#问题与整改) · [表范围对照](../../../../build/d08-t03/b-recheck-1/TableRanges.log)
- **新增 CR-006，待 A 整改**　普通 Automatic 录制下，公共 Validation 把缺少必需对象和间接参数角色错误的图形候选各委托底层 1 次，合同要求各为 0。合法候选委托 1 次。底层 OpenGL 报告了两个非法请求。5.4 保持待签收。 [复现源码](../../../../build/d08-t03/b-recheck-1/ValidationBoundary.cpp) · [实测日志](../../../../build/d08-t03/b-recheck-1/ValidationBoundary.log)

- **注释交回版本**　B 在 7 个文件补充表行与绑定范围、GL 录制与 Validation 声明、材质 UV 梯度的说明。36 个实现文件的交回差异摘要为 `c2af001055d737a4d7e2708631f9178cc08ae6b90eb24c005f524a89f00c953c`。两版间只改注释，证据单列运行输入和交回输入。 [B 签收原件](../../../../build/d08-t03/final/review-signed.md#b-的注释覆盖) · [交回差异](../../../../build/d08-t03/b-recheck-1/source.diff)

<a id="a-correction-2"></a>
## A 第 2 轮整改与复验

- 核实 B 交回的 36 个实现输入和已登记探针。仅两个 Validation 文件有本轮实现变化，交回版 binary diff SHA-256 为 `11ae09c420d130568371f17f5df90487cee75bea606ccf4a66f339cd24fd2449`，基准 HEAD 为 `41cba91c8c50d9cd2c0597ad736203e81eb97b26`。证据清单对应五次实际执行及其编译输入、Shader、库和可执行文件。 [B 注释版身份](../../../../build/d08-t03/a-correction-2/b-baseline.json) · [本轮证据清单](../../../../build/d08-t03/a-correction-2/a-correction-evidence.json)
- CR-006 恢复委托前的公开候选预检。重连整改版的 B 原用例在普通 Automatic 录制中得到 `1、0、0` 次底层 `SetGraphicsState` 调用，两个非法候选各有拒绝诊断。图内预检失败复用 `ClearState` 撤销旧绘制状态，原生录制继续保有用途和已发前缀。CR-001 原用例的 raw 与 Validation 均返回图失败，两路的原生 Draw 增量均为 1。 [委托计数](../../../../build/d08-t03/a-correction-2/ValidationBoundary-Release.log) · [图内前缀](../../../../build/d08-t03/a-correction-2/GraphState-Release.log)
- `ForwardRows` 验证正常图形候选仍能经公共预检执行。545 个表面的批次、逐表面直接绘制和前置深度对照通过。暖页裁剪仍上传 0 笔，附加 View 参与后补写两行，像素正确。 [Forward 对照](../../../../build/d08-t03/a-correction-2/ForwardRows-Release.log)
- 正式 Debug/Release 构建并以 `--hidden --poll-limit=5` 启动，均退出 0。OpenSpec strict 通过。Release 原生回放使用 Windows x64、Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129。 [构建与复验入口](../../../../build/d08-t03/a-correction-2/Recheck.ps1) · [严格校验](../../../../build/d08-t03/a-correction-2/validate-change.json) · [B 签收原件](../../../../build/d08-t03/final/review-signed.md#a-correction-2)

<a id="b-recheck-2"></a>
## B 第 2 次整改复核

- **复核版本和范围**　B 核实基准 HEAD `41cba91c8c50d9cd2c0597ad736203e81eb97b26`、36 个实现文件及其差异摘要 `11ae09c420d130568371f17f5df90487cee75bea606ccf4a66f339cd24fd2449`，并核对 21 个回放输入、71 个产物和 10 份原日志。相对上次 B 交回版只有两个 Validation 文件改变。Debug/Release 构建和隐藏启动沿用 A 的记录。本次独立原生复核限 Windows x64、Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129。 [A 证据清单](../../../../build/d08-t03/a-correction-2/a-correction-evidence.json) · [B 运行清单](../../../../build/d08-t03/b-recheck-2/results.json)
- **关闭 CR-006**　B 独立重跑的三个 Release 探针均返回 0。普通录制的合法候选、错误参数角色及缺失必需对象分别委托 1、0、0 次，图内拒绝后的 raw 和 Validation 各只发出 1 次合法前缀 Draw。545 个表面的直接/批次工作色与深度一致，暖页裁剪时原生上传为 0，附加路可见时补写两行且像素正确。CR-001 至 CR-005 保持关闭。 [普通委托](../../../../build/d08-t03/b-recheck-2/ValidationBoundary.log) · [图内前缀](../../../../build/d08-t03/b-recheck-2/GraphState.log) · [Forward 对照](../../../../build/d08-t03/b-recheck-2/ForwardRows.log)
- **新增 CR-007 [P1]**　正常的双批次 Forward 图发出 2 次 Draw 并返回成功。在第二批的公共预检内让一次 40 B CPU 分配失败后，`std::bad_alloc` 离开 Validation 并触发正式 `noexcept` 绘制回调的 terminate。第一批已绘制，第二候选的底层委托数为 0，图未返回。当前唯一开放项为 CR-007，5.4 待签收。 [B 签收原件](../../../../build/d08-t03/final/review-signed.md#cr-007-p1-forward-批次预检的分配异常会终止渲染进程) · [正常对照](../../../../build/d08-t03/b-recheck-2/ForwardPreflight-control.log) · [故障对照](../../../../build/d08-t03/b-recheck-2/ForwardPreflight-allocation.log)

- **本次注释交回**　B 在 Validation 声明和实现中补明预检的返回值、可能的分配异常及图内状态撤销责任。交回的 36 个实现文件差异摘要为 `2f111978623c9dfacdf24aac01d802202744d95621896f95ed5186251f222c47`，去除注释后的代码与 A 第 2 轮逐文件一致。 [B 签收原件](../../../../build/d08-t03/final/review-signed.md#b-的注释覆盖) · [交回差异](../../../../build/d08-t03/b-recheck-2/source.diff)

<a id="a-correction-3"></a>
## A 第 3 轮整改与复验

- 核实 B 注释版的 36 个实现文件与 14 个登记的回放文件。CR-007 的改动只涉及两个 Validation 文件，交回版 binary diff SHA-256 为 `c5bf32ad7ac5c415b2d3ff1efe1bcf67f26c94f92567bef5b8b631e7f8352176`。基准 HEAD 为 `41cba91c8c50d9cd2c0597ad736203e81eb97b26`。清单关联六次实际执行及其源码、编译输入、Shader、库和可执行文件。 [B 交回基准](../../../../build/d08-t03/a-correction-3/b-baseline.json) · [本轮证据清单](../../../../build/d08-t03/a-correction-3/a-correction-evidence.json)
- 公共图形候选的预检将临时存储的 `std::bad_alloc` 转为错误诊断和候选拒绝。图内拒绝沿用 `ClearState`，已发前缀保留在原录制包。B 原始 `ForwardPreflight` 正常对照成功返回并发出 2 次 Draw，第二候选委托 1 次。以同一可执行文件运行 `--fail-one-allocation`，确认第二批内实际注入一次 40 B 分配失败，图返回错误、Draw 增量为 1、第二候选委托为 0。前缀退休和宿主关闭通过，进程退出 0。 [正常对照](../../../../build/d08-t03/a-correction-3/ForwardPreflight-Release.log) · [故障对照](../../../../build/d08-t03/a-correction-3/ForwardPreflight-allocation-Release.log)
- CR-006 普通录制用例继续得到合法候选 1 次、非法候选各 0 次的委托计数，两个拒绝均有诊断。CR-001 图内拒绝用例在 raw 和 Validation 下均返回错误，各保留 1 次前缀绘制。 [委托对照](../../../../build/d08-t03/a-correction-3/ValidationBoundary-Release.log) · [图内前缀](../../../../build/d08-t03/a-correction-3/GraphState-Release.log)
- 正式 Debug/Release 构建并以 `--hidden --poll-limit=5` 启动，均退出 0。OpenSpec strict 通过。Release 原生回放使用 Windows x64、Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129。 [复验入口](../../../../build/d08-t03/a-correction-3/Recheck.ps1) · [严格校验](../../../../build/d08-t03/a-correction-3/validate-change.json) · [B 签收原件](../../../../build/d08-t03/final/review-signed.md#a-correction-3)

<a id="b-recheck-3"></a>
## B 第 3 次整改复核与签收

- **签收版本**　D08-T03 的 CR-001 至 CR-007 全部关闭，代码正确性和注释可读性均通过 B 复核。分支 `v2.0`，基准 HEAD `41cba91c8c50d9cd2c0597ad736203e81eb97b26`，36 个实现文件的 binary diff SHA-256 为 `c5bf32ad7ac5c415b2d3ff1efe1bcf67f26c94f92567bef5b8b631e7f8352176`。B 从工作树独立重算差异，核对 24 个回放输入、71 个产物和 11 份原日志。B 复核清单登记了本轮实跑的可执行文件、运行参数、Shader 输入和结果。 [签收差异](../../../../build/d08-t03/a-correction-3/source.diff) · [A 源码与产物清单](../../../../build/d08-t03/a-correction-3/a-correction-evidence.json) · [B 复核清单](../../../../build/d08-t03/b-recheck-3/results.json)
- **CR-007**　B 在独立目录对同一个 Release 可执行文件运行两次。正常输入成功返回，发出 2 次 Draw，第二候选委托 1 次。故障输入在第一批已绘制后实际注入一次 40 B 分配失败，图返回错误，原生 Draw 增量为 1，第二候选委托为 0。探针释放列表、等待完成、回收资源并关闭宿主，进程退出 0。源码确认预检在委托前将 `std::bad_alloc` 转为无分配的错误报告，图内随即撤销旧绘制资格。 [正常对照](../../../../build/d08-t03/b-recheck-3/ForwardPreflight.log) · [故障对照](../../../../build/d08-t03/b-recheck-3/ForwardPreflight-allocation.log)
- **相关回归**　CR-006 的普通录制中，合法候选、错误参数角色、缺失必需对象的底层委托次数分别为 1、0、0。CR-001 的 raw 与 Validation 均报告图失败，各只发出 1 次合法前缀 Draw。四次回放均成功退出，输出逐行匹配 A 的记录。CR-002 至 CR-005 的暖页裁剪、状态角色、混合材质梯度及双 View 面板口径复用前两轮已通过的对照。 [普通委托](../../../../build/d08-t03/b-recheck-3/ValidationBoundary.log) · [图内前缀](../../../../build/d08-t03/b-recheck-3/GraphState.log) · [第 1 次复核](#b-recheck-1) · [第 2 次复核](#b-recheck-2)
- **注释与覆盖范围**　已复核 Draw 参数和表行单位、Forward 与 RenderGraph 的范围及寿命、OpenGL 与 Validation 的状态所有权、Shader 取样顺序和 Runtime/Editor 的统计口径。本轮 A 更新的返回合同与 catch 分支一致，B 保留交回源码的原字节。原生观察限 Windows x64、Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129。正式 Debug/Release 构建及隐藏启动沿用 A 的对应记录。6.1 最终 verify、6.2 同步和 6.3 归档待 A 接续。 [A 构建记录](#a-correction-3)

<a id="final-verification"></a>
## A 最终验收

| 维度 | 结论 |
|---|---|
| 完整性 | OpenSpec `spec-driven` 的 proposal、design、6 份 delta 和 tasks 均齐备。最终 verify 完成时已勾选 15/17 项，剩余项为规范同步与归档。11 项增量要求和 38 个场景均已核对。 |
| 正确性 | 已核对 36 个签收文件及 binary diff。复用同版 2 次正式启动和 B 的 4 次独立原生回放，补跑 5 个 Release 用例，全部退出 0。OpenSpec 1.13.2 的 change strict 通过。 |
| 一致性 | Forward 从冻结输入组织并使用同一有序数据，跨 View 共享的版本与图范围保持独立。OpenGL 在现有录制中消费紧排参数并保活实际依赖，Validation 在底层委托前处理非法候选。Runtime 从已接受图发布统计。正式 CMake 配置和默认目标沿用既有构建入口，复用基础 profile 的独立构建与能力边界记录。 [基础验收](../2026-11-07-establish-opengl-immediate-draw/tasks.md#final-verification) |

签收差异 SHA-256 为 `c5bf32ad7ac5c415b2d3ff1efe1bcf67f26c94f92567bef5b8b631e7f8352176`，基准 HEAD 为 `41cba91c8c50d9cd2c0597ad736203e81eb97b26`。已核实两个配置的库、Shader 包、正式可执行文件及启动记录，原命令为 `Physara.exe --hidden --poll-limit=5`。B 的四次实际运行经可执行文件、参数、复制输入与输出指纹核对。第二批预检分配失败返回图错误，保留 1 次原生 Draw 并完成退休，第二候选的底层委托为 0。 [签收问题单](../../../../build/d08-t03/final/review-signed.md) · [源码与输入](../../../../build/d08-t03/final/signed-source.json) · [B 回放清单](../../../../build/d08-t03/final/b-signed-evidence.json) · [最终验收清单](../../../../build/d08-t03/final/verification-evidence.json) · [change strict](../../../../build/d08-t03/final/validate-change.json)

| 能力 | 签收版本的验证依据 |
|---|---|
| OpenGL 资源与即时绘制 | `Indirect` 核对参数角色、真实不可变分配容量、非零参数偏移、R16/R32 和各基值。单命令与 MDI 分别增加一次原生 Draw。同绑定更新后读到新参数和常量，非法范围、数量和索引偏移被拒绝，直接索引偏移仍可用，参数 Buffer 等待真实完成后退休。`CombinedRoles` 在 raw 和 Validation 确认组合用途只接受具有参数角色的 Buffer。 [间接绘制](../../../../build/d08-t03/final/Indirect-Release.log) · [组合用途](../../../../build/d08-t03/final/CombinedRoles-Release.log) |
| RenderGraph | `Indirect` 验证同一生产范围先写后供两个存活消费者读取，独立无消费者区间被裁剪。错误角色在声明时拒绝，跨许可区间的命令在录制时拒绝。`GraphState` 复用 B 已匹配结果，raw 和 Validation 各只发出 1 次合法前缀 Draw。 [范围回放](../../../../build/d08-t03/final/Indirect-Release.log) · [前缀回放](../../../../build/d08-t03/b-recheck-3/GraphState.log) |
| Forward 与 GPU 驻留 | `ForwardRows` 的 545 个表面使用 2 个颜色批次、4 条记录，两种深度配置与同序逐表面直接路径的颜色及深度逐值一致。同纹理的 repeat/clamp 分批并读回红/蓝。暖页在裁剪帧无写入，附加 View 可见后补写两行并读回正确像素。`ForwardGradients` 的混合材质在预览与直接光路径均通过，MASK 样点深度为 0 与 0.5。 [分页和绑定](../../../../build/d08-t03/final/ForwardRows-Release.log) · [混合材质取样](../../../../build/d08-t03/final/ForwardGradients-Release.log) |
| Runtime 与 Editor 统计 | `BatchRuntime` 的正式面板显示所选 View 有 12 个表面、1 个颜色批次、10 条间接记录，整图有 14 次原生 Draw、106 次绑定及 4 笔共 8,592 B 实例/命令上传。暖槽静态写入为 0，材质编辑只编码并写入 1 行，裁剪、失败前缀及安全 UI 保留对应的有效输出与快照。 [双 View 与面板回放](../../../../build/d08-t03/final/BatchRuntime-Release.log) |

补跑使用签收版 Release 库和 Shader，沿用登记的输入与观察点，编译及执行命令见回放清单。验收设备为 Windows x64 上的 Intel RaptorLake-S Mobile，OpenGL 4.6.0 Build 32.0.101.6129。其它 GPU 或驱动未覆盖，性能测量属于 D08 阶段出口。 [执行记录](../../../../build/d08-t03/final/verified-runs.json) · [回放入口](../../../../build/d08-t03/final/RunFinal.ps1)

<a id="spec-sync"></a>
## 规范同步

- 按 OpenSpec 1.13.2 返回的本仓库 `specs` 工件路径，将 6 份签收 delta 同步到已有主规范。合并 7 项修改和 4 项新增要求，共 38 个增量场景。原有场景顺序及未涉及的要求块均保留。 [合并清单](../../../../build/d08-t03/final/spec-sync.json) · [CLI 工件规则](../../../../build/d08-t03/final/instructions-specs.json)
- `openspec validate --specs --strict --json` 通过 36/36 份主规范。同步后复核了各目标文件摘要。 [主规范 strict](../../../../build/d08-t03/final/validate-specs.json)

<a id="archive-closeout"></a>
## 归档收尾

OpenSpec 1.13.2 已在 6 份主规范同步后归档本 change。D08-T03 已验收并归档，下一工作包为 D08-T04。B 签收原件与最终验收证据已留存，当前问题单已按模板复原，归档工件与开发计划的本地引用已复核。 [归档回执](../../../../build/d08-t03/final/archive.json) · [开发计划](../../../../Docs/Developments/Developments.md#current-stage) · [B 签收原件](../../../../build/d08-t03/final/review-signed.md)
