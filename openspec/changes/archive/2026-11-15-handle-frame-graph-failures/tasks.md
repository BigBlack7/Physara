# Tasks

## 1. 失败分类、诊断与准备期门控

- [x] 1.1 A 在 Platform 既有 RHI 消息适配器锁存 Fatal, 提供停止事实并保留独立错误 scope; 完成相应接口/寿命注释。按 R1 检查普通 Error 不锁存 Fatal、后建 scope 不掩盖 Fatal, 并验证图/arena 销毁后有界首错仍可读。
- [x] 1.2 A 扩展 Runtime 的正常、等待、可恢复取消和必须退出结果, 将拥有型主错/附加原因及恢复状态交给 EditorSession/现有面板; 明确基础字体启动失败分类。按 R1/R3 检查普通准备失败归还取得且事件继续、初始化失败仍退出 1, 并确认最近错误显示不依赖失效借用或日志字符串。

## 2. 已接受输出与独立安全 UI 图

- [x] 2.1 A 为 View 提供只读已接受输出快照, 在 Editor 准备中显式区分正常候选与安全旧输出, 完善 Viewport 状态文字及相关合同注释。按 R2 检查旧尺寸输出可只读显示、身份不匹配仍被拒绝、无旧输出时明确占位, requested/effective 和颜色代不被快照或 fallback 推进。
- [x] 2.2 A 在尚未 Open 的主图声明/Seal/Compile 失败后重建至多一张安全 UI 图, 正确结束旧图/准备值后清空绑定和 arena; 复用存活上传准入及既有 UI/呈现链。按 R2/R3 检查独立图代、原窗口目标、失败生产者/读回缺席、旧像素和缺失占位, fallback 再失败或等待时归还取得, 不出现无界重试。
- [x] 2.3 A 将上传确认及 View 发布绑定到实际成功候选, 保持 Cancel/Finish 与下一次 UI 帧的借用边界; 更新受影响的 UI 请求说明。按 R4 检查主图撤销后更新可由 fallback 或下一帧重试、逐请求确认且裁剪请求仍 pending, 失败前缀不确认, 驻留 Prepare 后注销的既有快照合同保持。

## 3. 执行取消、目标换代与受控退出

- [x] 3.1 A 在 Platform 按槽记录失败目标需重建, 保留合法失败前缀 ticket/Event 并结束取得, 真实完成后才更换目标代; 同步呈现接口和实现注释。按 R5 检查旧目标在途保活、其它安全槽可继续、同槽 Pending 不复用、新代 Common 入口及旧 token 拒绝, 不增加槽数或正常帧全设备等待。
- [x] 3.2 A 接通 Open 后执行失败的唯一取消路径, 不追加 UI 图; 在无 Fatal 且原列表合法时完成 Close/前缀提交/取得消费, 否则保留退出 owner。按 R3/R5 检查首次资源创建、recipe 和已发命令后的代表性失败: 后续回调停止、无末端复制/交换、无错误发布, 可恢复前缀后下一帧仍正确。
- [x] 3.3 A 补齐 UI/图消费者准备、fallback 准备、执行后及提交/发布边界的 Fatal 与无法可靠完成门控; 结束 CPU 借用后沿实际后端健康收拢。按 R6 检查准备期 Fatal 不再 Open、fallback Fatal 不再执行、提交零票据和 Event 失败不复用, 原生不可用时无后续原生命令; 保留首错和进程结果, 不放宽 invalid RHI 包的拒绝条件。

## 4. 产品集成与 A 本地交接

- [x] 4.1 A 用当前正式目标完成 Debug/Release 构建及 R7 产品集成, 核对正常帧→安全 UI/取消→正常帧、窗口/视口变化、背压和带在途工作的关闭。更新 Docs/Build.md 的实际可恢复/退出行为; 复用输入匹配的 D05-T01 至 T04 证据核对阶段出口, 记录本轮执行、复用、未覆盖项及最小必要追加检查。
- [x] 4.2 A 执行 OpenSpec 严格校验、diff/本地链接检查, 在本 tasks 填写当前输入/构建/可执行文件及 fixture 身份、命令、结果和限制; 将实际第一方改动范围与交接引用写入当前 Docs/CodeReview.md, 开发计划标记待独立审查。确认检查对应当前版本, B 和最终收尾任务保持未勾选。

## 5. 独立审查、整改与复核

- [x] 5.1 B 在维护者调度的独立会话中检查实际 diff 与直接消费者, 通读本轮涉及的完整第一方文件并完善职责、接口合同及流程注释; 在当前问题单记录版本、范围、发现、注释覆盖与代码/可读性结论, 由 B 勾选本项。
- [x] 5.2 A 按稳定 CR 编号修正本轮问题并运行受影响的最小回归, 同步整改导致失效的说明, 将证据写入问题单和本 tasks; 没有发现时明确记录整改不适用, 只由 A 勾选本项。
- [x] 5.3 B 复核实际整改输入、对应证据与最终完整文件注释, 分别签收代码正确性和注释可读性; 未通过保持问题编号与未完成状态, 仅 B 在签收最终版本后勾选本项。

## 6. 最终验收、规范同步与阶段收尾

- [x] 6.1 A 在 B 签收最终版本后执行最终 verify, 核对四份 delta 的全部场景、D05-T05 三项包内目标与 D05 阶段出口; 对最终输入运行所需原生检查, 同输入已有结果可明确复用, 不把旧 exe 或 A 自检当作 B 验收。
- [x] 6.2 A 将最终源码/产物身份、实际命令和结果、场景映射、CR 闭环、B 注释覆盖与签收、未覆盖限制留存在本 tasks, 同步四份主 specs 并通过官方 CLI 严格校验; 不改历史归档或用进度内容修改 Architecture。
- [x] 6.3 A 在满足验收后归档 change 并核对归档引用, 更新 D05-T05 和 D05 出口完成状态、将当前阶段推进到前置满足的 D06-T01, 归档成功且结论留存后才按模板重置 Docs/CodeReview.md; 核对链接与实际状态后勾选本项。

<a id="verification-plan"></a>
## 验证范围与停止条件

下表为规划时确定的 L2/L3 验收范围, 实际执行结果见本页实施留存。按 [验证技能](../../../../.agents/skills/physara-verification/SKILL.md) 使用代表性输入, 同一场景可证明多项合同, 不逐 spec scenario 新建测试。

| 风险组 | 独立风险与对应合同 | 最小充分检查和独立预期 | 复用与停止条件 |
|---|---|---|---|
| R1 诊断及停止事实 | 普通 Error/Fatal 混淆、scope 重建丢首错、arena 借用悬空; editor-frame-graph 分类/Fatal | 一个有界直接消费者依次观察 Error、独立 scope、Fatal; 在失败候选销毁后核对首错/附加原因和停止事实, 不以日志文本决定结果 | 分支结果和拥有寿命明确后停止; 不遍历诊断环容量或每个分配点 |
| R2 独立 fallback 与旧输出 | 原图命令混入、旧 GraphId 泄漏、旧输出被覆写或误确认; editor-frame-graph fallback、editor-viewport-output | 正常接受有已知四象限像素的旧输出后改变尺寸/请求, 令主图编译失败; 原生安全帧读取旧像素、当前请求保持未确认, 后继正常帧发布新代。另取无旧输出分支核对占位; 声明错误在相同恢复入口做一个 CPU 检查 | 复用 T04 已知颜色/布局预期, 新增仅验证恢复序列和绑定代隔离; 两个输出分支及后继正常帧通过即止 |
| R3 无命令取消与单次尝试 | UI 准备、fallback 准备/编译失败丢 token、同轮递归重试、基础字体失败被掩盖; 分类/fallback、presentation、runtime | 在既有消费者中分别触发普通 UI 准备失败和 fallback 准备/编译拒绝; 观察 Open/复制/交换为零、token 只结束一次、下一轮取得成功。保留初始字体失败退出 1 的消费者断言 | 复用 T04 准备回滚及上传 Pending 检查, 只增加对应新编排分支; 不扫描所有内存失败点 |
| R4 UI 重试与候选发布 | 临时 token 残留、更新错误确认、主图借用 fallback ticket; publish 合同 | 一个托管更新和一个未消费请求贯穿主图取消→安全图接受, 再穿过失败前缀→后续成功; 读回更新区域及区域外 sentinel, 核对逐请求状态、NoSceneOutputs 和 View 代不误变 | 复用 T04 managed/culling/CR-001 fixture 的预期和消费者, 输入改变后重新构建所需部分; 所有权与逐请求证据齐备即止 |
| R5 执行失败与前缀退休 | first-use/recipe 失败误传空资源、半图后拼 fallback、未知目标代复用; execution 与 presentation | 分别在首次物化、一次附件/绑定 recipe、已发真实命令后的普通回调制造代表性失败; 合法前缀获 ticket/Event, 不复制/交换, 等待真实完成后同槽目标换代并正常绘制; Pending 时资源不提前销毁 | 复用 T04 N3/N4 和原生完成链, 按新恢复行为更新断言; 同一组合观察回调次数、资源删除、像素和目标代, 不扩成全失败点矩阵 |
| R6 致命与不可证明完成 | 晚期准备 Fatal 被忽略、零票据/观察失败被当作 Pending、失效 context 再调用; Fatal/presentation/runtime | 覆盖 UI 准备、临时读回消费者准备、fallback 准备的 Fatal 门控; 在实际原生调用边界注入一次 fence 失败及一次已获 ticket 后 Event 失败, 观察停止新工作与保活。复用/补齐原环境可用与失效退出的必要证据 | Fatal 不等于自然 device loss; 记录注入方式和环境。Close/invalid 拒绝、末端复制错误及失效退出可复用输入匹配的既有证据, 不因每种错误都重跑全部产品矩阵 |
| R7 正常消费者与 D05 出口 | 修改恢复后损害普通图、编辑输入、背压、退出; 四份 delta 与既有合同 | 正式 Debug/Release 产品和受影响临时原生消费者; 一个组合序列包含恢复后的 resize/比例、暂停恢复、槽复用和关闭, 保留正常像素、接受/交换次序及退出结果。D05 编译非法图门槛引用 T01/T02 验收并核对受影响点 | 不重新跑未改 Shader ABI/多显示器/全部历史矩阵。只有输入变化、失败或具体覆盖缺口才扩大; 本 change 不作性能结论 |

R6 的临时 Readback 消费者必须经过本次真实构图/准备诊断边界, 参见 [design 决策 7](design.md#7-在当前消费者上验证阶段出口)。当前没有产品截图/拾取或真实 Scene 时域历史, 最终记录这一范围, 不把临时消费者称作产品读回实现。不能提供必要公共边界证据的项保持未验收。

既有材料已确认本机存在于 `build/d05-t04/verify/`、`build/d05-t04/cr-001/`; 历史验收见 [T04 最终记录](../2026-11-15-integrate-editor-frame-graph/tasks.md#final-verification)。fixture/脚本/临时 CMake/故障注入只写入 `build/d05-t05/`, 不纳入正式构建; build 被清理后不自动恢复。测试环境不可用时说明缺口并保持对应任务未勾选。

运行前将实际源码(含 dirty/untracked)、构建输入、Shader 包、fixture 和库/可执行文件身份匹配。Debug/Release 保留项目原编译选项; 编译失败日志应保留, 不用旧 exe 运行替代构建成功。每个风险组已有匹配证据且各项验收满足即停止, B 注释等最终输入变化按实际影响决定需要的最终重建/检查。

<a id="planning-checks"></a>
## 规划检查记录

2026-11-15 规划完成时: 根据已归档 D05-T04、当前源码及架构合同建立本 change, 使用官方 OpenSpec 1.13.2 的 `spec-driven` schema。四份 delta 修改既有能力, 不新增实现能力名。当时实施、原生验证和 A/B 审查均未开始, 16 项任务全部待实施。

实际完成的 L0 检查:

- `node build/d05-t04/openspec-cli/node_modules/@fission-ai/openspec/bin/openspec.js status --change handle-frame-graph-failures --json`: proposal、specs、design、tasks 四类产物均完成。CLI 的 planning/isComplete 只表示规划产物存在。
- 同一 CLI 的 `instructions apply --change handle-frame-graph-failures --json`: `ready`, 实施进度 `0/16`。
- 同一 CLI 的 `validate handle-frame-graph-failures --strict --json` 及 `validate --all --strict --json`: 本 change 通过, 全库 23/23 通过(22 份主 specs 加本 change), 无 issue。
- `python build/d05-t05/planning/CheckPlanning.py`: 9 份 Markdown 的 180 个本地链接/锚点通过; proposal 与四份 delta 一致, 共新增 3 项及修改 5 项要求、31 个场景, 被修改要求的全部 12 个原有场景均保留。16 个任务编号唯一且未提前勾选。
- `git diff --check` 及变更路径核对通过。正式改动仅为本 change 和两份开发计划状态; C++/Shader/CMake、Architecture、主 specs、历史归档及当前问题单均未改动。未构建或执行原生程序, 本次结果不表示实现验收。

本机规划校验数据保存在可删除的 `build/d05-t05/planning/`, 包括 `planning-checks.json`、`validate-all.json` 和 `change-preview.json`; 上述结论留在本节, 不依赖这些临时文件永久存在。

<a id="implementation-evidence"></a>
## 实施、审查与最终验收留存

2026-11-15 首次 A 交接: 实现与 A 本地检查完成, 基准为分支 `v2.0` 的 `3a3d5a6afdbaea193664f393243ea6e3da3818b0`。以下至首次交接状态保留当时的 A 本地证据; 当时 B 审查及最终 verify 尚未开始, 任务 5.1–6.3 待完成。后续首轮审查及整改见本页 CR-001 留存, 没有同步主 specs 或归档。

- R1: `python build/d05-t05/verify/Run.py Debug diagnostics` 返回 0, 在 Intel RaptorLake-S Mobile / GL 4.6.0 Build 32.0.101.6129 验证普通 Error 的独立 scope、拥有型首错、Fatal 在新 scope/捕获后仍锁存、禁止新取得及可用 context 的关闭。对应输入和日志见本机 `build/d05-t05/diagnostics-debug.json` / `.log`, 运行前后身份一致。
- 正式 Debug 产品已构建通过, 原始日志为 `build/d05-t05/build-debug-1.log`、`build-debug-2.log`。临时消费者前两次因第三方头宏规则与正式目标不一致编译失败; 按正式目标补齐 SYSTEM includes 和 `IMGUI_USE_WCHAR32`, 保留第一方 `/W4 /WX`, `build-verify-debug-3.log` 构建通过。
- 实施期间此前可读的 `build/d05-t04/` 被清理, 旧测试没有恢复。按本 change 风险在 `build/d05-t05/verify/` 编写有限新消费者; 当前源码副本只增加四个帧阶段探针和一个真实 Validation Event 拒绝, 注入位置与原源码 hash 记录在 `injection-input.json`。正式产品没有测试宏、故障参数或测试目标。官方 OpenSpec 1.13.2 已从本机 npm 缓存离线安装至 `build/d05-t05/openspec-cli/`, 后续校验使用该 CLI。

### A 交接版本与实现边界

- 完整正式输入集合 SHA-256: `a353b77949e21bf8d45cf5c871bba438201442e5381af4b18bfd2cc5f959bc6f`。
- 本轮 21 份第一方 C++/CMake 审查输入集合 SHA-256: `60842931e57b14ea02bf7fdd14a80efa60a88d5256000440f4f8a573dc6b4513`。算法为按仓库相对路径排序, 拼接 UTF-8 的 `path + NUL + SHA256(raw bytes) + LF`, 再取 SHA-256。包含新增的 `Runtime/Frame.hpp` / `Frame.cpp`, 不能只以 HEAD 或 tracked diff 判断同一输入。
- 逐文件 hash、两配置库/可执行文件/Shader 包身份、10 次消费者执行及 2 次正式产品执行记录汇总于本机 `build/d05-t05/handoff.json`; 每次原生运行均核对前后输入一致。该目录可清理, 本页保留结论和边界。
- [Runtime/Frame](../../../../Runtime/Frame.cpp) 从 Main 提取一次取得的尝试与交接, 正常背压重建和单次错误 fallback 分别有界。候选图/准备值销毁后才 reset, 执行后只处理原前缀。存储分配异常按是否已 Open 选择独立恢复或退出。
- [View](../../../../Engine/Renderer/View.hpp) 通过 `LastAcceptedOutput` 返回拥有型只读快照; `IsRetained` 阻止错误发布。Editor 的 `UiFrameMode` 保留正常请求匹配, 安全模式显式采样旧输出, `Frame status` 菜单持有最近错误及恢复状态。UI 逐请求 Finish/Cancel 和驻留快照沿既有协议工作。
- Platform 在既有回调锁存 Fatal, 并有独立帧首错捕获。失败前缀只有在有效列表、可靠 ticket/Event 及其它持久导入可恢复状态成立时继续; 失败 PresentColor 完成后换代。当前自动恢复只依赖已声明 `keepInitialState` 的可靠入口, 不推测任意外部资源的失败末态。invalid 列表、Close/复制/提交/观察失败保留受控退出。

### 实际构建与运行

以下命令均在仓库根执行, 构建配置保持正式 `/W4 /WX /utf-8 /permissive-` 及各配置原有选项。

```powershell
cmake --build build --config Debug --target Physara --parallel 2
cmake --build build --config Release --target Physara --parallel 2
python build/d05-t05/verify/Prepare.py
cmake -S build/d05-t05/verify -B build/d05-t05/verify-build -G "Visual Studio 17 2022" -A x64
cmake --build build/d05-t05/verify-build --config Debug --parallel 2
cmake --build build/d05-t05/verify-build --config Release --parallel 2
```

当前正式构建日志为 `build-debug-4.log` / `build-release-2.log`, 当前消费者构建日志为 `build-verify-debug-7.log` / `build-verify-release-2.log`, 均返回 0, 本轮未出现 C1001。后续执行以下实际命令组合, 各组 Debug 和 Release 均返回 0:

```powershell
python build/d05-t05/verify/Run.py Debug diagnostics
python build/d05-t05/verify/Run.py Debug recovery
python build/d05-t05/verify/Run.py Debug stops
python build/d05-t05/verify/Run.py Debug textures
python build/d05-t05/verify/Run.py Debug integration
python build/d05-t05/verify/Run.py Release diagnostics
python build/d05-t05/verify/Run.py Release recovery
python build/d05-t05/verify/Run.py Release stops
python build/d05-t05/verify/Run.py Release textures
python build/d05-t05/verify/Run.py Release integration
python build/d05-t05/ProductRuns.py
```

`ProductRuns.py` 实际运行当前 `build/bin/Debug/Physara.exe --hidden --poll-limit=6` 及相同 Release 入口, 均返回 0。正式产品未注入故障。消费者注入只改变 build 下的边界输入或报告, 不放宽正式 Validation/编译拒绝条件。

| 风险/场景映射 | 本轮实际结果 | 当前有效记录, 路径均相对 build/d05-t05 |
|---|---|---|
| R1; 分类与 Fatal scope, 独立首错 | Error 后新 scope 可用而原 scope 失败; Fatal 后所有新 scope 和新取得失败; 拷贝首错在捕获重置后保留, 可用 context 关闭成功 | `diagnostics-debug-3.json`, `diagnostics-release-2.json` |
| R2/R3; 正常/安全候选隔离、旧尺寸/无输出、准备/声明/编译取消、主循环继续 | 改变请求后主图失败, 两次尝试只有一次执行和窗口复制; 安全图保留旧四象限像素且不发布新请求。后继正常帧发布新比例。初次无旧输出时安全 UI 接受真实字体而不伪造 View 有效值。真实非法 Readback 声明触发同一恢复入口; 普通 UI 准备、fallback 准备和双编译失败无新执行/复制/提交, token 正确归还; 正常退出返回 0 | `recovery-debug-3.json`, `recovery-release-2.json` |
| R4; UI 逐请求重试/裁剪、旧 View 不发布、驻留拥有快照 | 8x8 托管纹理 3x3 更新经过主图取消→安全图成功及已发前缀失败→下一帧成功; 失败前缀保持 WantUpdates, 安全图只确认其存活请求, 未消费请求仍 WantCreate。窗口读回更新区 `(220,30,90,255)`、区域外 `(20,80,140,255)` 正确。公开 Prepare→Unregister→外部 Release→Build 仍显示原驻留 `(44,130,211,255)`, 图与已完成包释放后原生对象删除 | `textures-debug-3.json`, `textures-release-2.json` |
| R5; 执行后不再 fallback、前缀票据、Pending 保活、目标换代 | 一次真实 Draw 后报告 Error, 后续 draw/窗口复制停止, 没有第二次图尝试。非零前缀票据保护原目标; 同槽 Pending 不复用, 其它安全槽继续。真实完成后原目标删除、新代 Common 入口、同槽下一帧四象限正确 | `recovery-debug-3.json`, `recovery-release-2.json` |
| R6; 晚期准备 Fatal、首次创建/recipe、Close、复制、fence、Event、失效退出 | UI/临时 Readback/fallback 准备 Fatal 均在 Open 前停止。原生纹理/FBO 零名字令实际物化/recipe 失败, 关闭/提交不可用时保留主错退出。末端复制报告错误后不提交或交换; fence 零值没有有效票据; 已接受票据后的真实 Validation Event 拒绝保留 ticket 并停止复用。注入 GL_CONTEXT_LOST 后受观察原生入口不再调用, 退出报告实际失效原因。首个必需 UI 准备失败仍为终止结果 | `stops-debug-3.json`, `stops-release-2.json` |
| R7; 槽/上传背压、暂停恢复、窗口尺寸、在途关闭 | 三个在途槽均 Pending, 不增加槽数; 实际 iconify/restore、1200x800 resize 后恢复正常。64 MiB 真实上传 lease 加需要暂存的合法 UI 几何触发 Open 前 Pending, 退休后正常。最后在途帧由关闭路径真实收拢 | `integration-debug-4.json`, `integration-release-2.json` |
| 正式装配入口与退出 | 两配置原始产品入口各一轮, 配置匹配包与当前源码身份一致, 返回 0 | `product-runs.json` |

正常/旧输出四象限独立预期仍为 `(180,71,53,255)`、`(41,109,149,255)`、`(55,127,82,255)`、`(118,75,161,255)`, 比较容差 2。诊断、失败尝试与目标代的具体状态由消费者断言, 不只观察 exit 0。

### 复用、已知限制与失败记录

- RHI 及 RenderGraph 核心、Architecture、主 specs 和历史归档与基准 HEAD 完全一致。D05-T01/T02 的未初始化读、feedback/环和稳定计划, T03 的同步/执行合同沿对应归档复用, 本轮没有重跑已清理的旧 fixture 或声称旧 exe 对当前 UI 接口有效。T04 的产品/UI 相关行为通过上述新消费者重新覆盖受影响的部分。
- 读回准备检查使用真实图声明的临时 Readback side effect, 在产品构图后、编译前的同一门控处报告失败/Fatal; 当前仍无产品截图/拾取服务。`NoSceneOutputs` 和 View 设置/颜色代保持得到验证, 没有真实 Scene/TAA/SSR 历史数值测试。
- 环境仅 Windows x64 / MSVC 19.44.35228 / SDK 10.0.26100.0 / Intel 上述 GL 驱动。故障为定点注入, 不是自然 OOM 或自然设备丢失。Pending 通过延后 `glClientWaitSync` 的完成观察形成, 释放后使用真实 GL 完成; 不据此宣称自然 GPU 延迟或性能收益。
- 未新增多 GPU/OS、真实多显示器、32 位 ImDrawIdx、完整历史测试矩阵或性能测量。UI 错误文字/恢复菜单通过接口、构图消费及源码核对, 没有新增逐字 OCR/截图测试。
- 初次 integration 消费者负载没有需要暂存的几何, 未触发预期 Pending; 增加预算内的实际缓冲负载后修正。随后一次消费者额外 ImDrawList 活到 context 关闭触发 ImGui 断言, 终止该测试进程并将该对象限制到当前帧作用域。两次失败保留在 `integration-debug.log` / `integration-debug-2.log` 和记录中, 修正后第三次通过, 最后交接输入的结果见上表; 未为测试改变产品预算、关闭断言或放宽检查。

### 首次 A/B 交接状态

A 的代码/构建范围为上述 21 文件, 逐模块清单和当前轮次见 [CodeReview](../../../../Docs/CodeReview.md)。A 已完成本地检查; B 需要检查实际未暂存 diff、新文件和直接消费者, 并通读完整第一方文件补齐注释。B 的代码正确性与注释可读性结论均待填写。当前不标记 D05-T05 工作包或 D05 出口最终完成。

A 交接前完成最后一轮接口注释和新代码行宽核对, 随后重新构建两配置并执行上表全部检查, 因而当前原生结果与最后源码字节身份一致。`python build/d05-t05/Handoff.py` 核对当前 21 文件、所有有效运行记录及保护范围通过, 11 份 Markdown 的 210 个本地链接/锚点通过。`git diff --check` 通过, 官方 CLI `validate --all --strict --json` 为 23/23, `instructions apply` 的 A 交接进度为 10/16。

<a id="cr-001-correction"></a>
## B 首轮审查与 CR-001 整改

B 已在当前问题单签署首轮代码审查和完整文件注释结论: 21 文件完整覆盖, 其中 20 份 C++ 补充注释, 代码正确性因 CR-001 保持待整改。B 初始输入与首次 A 交接的 `60842931e57b14ea02bf7fdd14a80efa60a88d5256000440f4f8a573dc6b4513` 匹配; B 注释后为 `2d7e6b4e60ebfafc2324651137ff3947354971eaf425ff27630a6efec82a44af`。A 本轮进入整改前再次实算并匹配后者, 保存该输入及问题单快照。

B 按维护者限定未编辑 tasks, 因此 5.1 的首轮完成以问题单签署为据, 该 checkbox 留给 B 自行同步。A 本轮仅勾选自己的 5.2, 不代勾 B 的 5.1/5.3。

### 修正与版本

- CR-001 [P2]: 帧诊断菜单长正文在窄窗口中横向裁剪, 实际 Pass、资源及附加原因不能完整读取。
- 本轮功能修改仅在 [Workspace.cpp](../../../../Editor/Panels/Workspace.cpp) 增加视口约束的文本换行, 为菜单边距和纵向滚动预留空间。恢复状态及两段正文均完整布局; 仍用 TextUnformatted, 不重解释错误正文或再次截短。超过可见高度时使用既有纵向滚动。B 的既有注释保留, 并补充布局意图说明。
- A 修正后 21 文件集合 SHA-256: `c4b3014546e3befd78657902f335f2a6387cc886adf9fb8413dddeb7c58aab07`, 算法沿首次 A 交接。`Workspace.cpp` 原始字节 SHA-256 为 `8232db74172d709dd68010ec5103cfe93c93c39aa53d2b1c95631d5bb5ae8d08`; 其它 20 份文件与 B 注释后快照逐字节相同。
- 本机输入快照、最小 diff、提取记录和产物身份保存在可删除的 `build/d05-t05/cr-001/b-input.json`、`before/`、`correction.diff`、`correction.json` 和 `menu-probe/input.json`。没有改架构指南、主 specs、历史归档或 B 复核勾选。

### 实际检查

本问题为 CPU 菜单布局改变, 按 B 给定方法复用固定 ImGui 与正式 FrameDiagnostic/BoundedText 类型。从 B 交回和 A 修正的 Workspace 原样提取菜单块, 只插入 item/popup 观察点, 比较实际布局矩形与 clip, 不另实现换行算法。1900+ 字节连续文本用于验证换行后的纵向可达性, 不扩展为 GPU 故障矩阵。

```powershell
python build/d05-t05/cr-001/Snapshot.py
python build/d05-t05/cr-001/menu-probe/Extract.py
cmake -S build/d05-t05/cr-001/menu-probe -B build/d05-t05/cr-001/menu-probe-out -G "Visual Studio 17 2022" -A x64 "-DCMAKE_SYSTEM_VERSION=10.0.26100.0"
cmake --build build/d05-t05/cr-001/menu-probe-out --config Debug --parallel 2
./build/d05-t05/cr-001/menu-probe-out/Debug/MenuProbe.exe
cmake --build build/d05-t05/cr-001/menu-probe-out --config Release --parallel 2
./build/d05-t05/cr-001/menu-probe-out/Release/MenuProbe.exe
cmake --build build --config Debug --target PhysaraEditor --parallel 2
cmake --build build --config Release --target PhysaraEditor --parallel 2
python build/d05-t05/cr-001/RecordCorrection.py
```

Snapshot 在整改前只运行一次, 拒绝覆盖已有 B 输入。两配置菜单消费者及受影响的 PhysaraEditor/依赖构建均返回 0, 保持第一方 `/W4 /WX /utf-8 /permissive-` 和正式 ImGui 字符 ABI, 没有 C1001 或放宽诊断。

| 布局输入 | Debug/Release 的相同实测结果 |
|---|---|
| B 原菜单, 640×480, 问题单两条原消息 | popup 宽 634; 三段文本右界 347/893/1215, clip 右界 636, 无横向滚动条, `scroll_max_x=586`; 成功复现原缺陷 |
| A 修正, 640×480, Cancelled | popup 534×107; 右界 347/501/529 均在 clip 右界 536 内, 横向剩余滚动为 0 |
| A 修正, 1280×720, Normal 恢复快照 | popup 534×107; 原首错和附加正文均保留且完整换行, 恢复状态可读 |
| A 修正, 640×480, SafeUi, 两段 1900+ 字节连续文本 | popup 548×455; 无横向溢出, 可见纵向滚动条, `scroll_max_y=263`; 滚动到底时末行底界 466 小于 clip 底界 473 |

菜单消费者未创建 Inspector, 入口仍可打开; 源码 diff 确认会话恢复状态维护、Inspector 显隐及错误存储均未改。每个输入在布局后都核对两段有界正文与原输入逐字节相同。日志为 `menu-debug.log` / `menu-release.log`, 构建日志为 `build-probe-*.log` / `build-editor-*.log`, 均在本机整改目录。

整改交接检查通过: 官方 OpenSpec 1.13.2 `validate --all --strict --json` 为 23/23; `git diff --check` 无错误; `CheckCorrection.py` 核对 21 文件当前指纹、其余 20 文件的 B 输入保留、11 份 Markdown 的 237 个本地链接/锚点均通过。OpenSpec 当前计数为 11/16, B 自有 checkbox 未代勾。结构与链接结果留在本机 `validate-all.json`、`document-checks.json`。

### 边界与下一环节

本轮没有运行窗口/GL, 没有重跑前次 GPU 矩阵, 也不以旧原生 exe 声称当前修正已获最终验收。初次 A 原生记录继续对应当时的输入, B 注释及本次菜单修正纳入后续最终 verify 的版本。当前 CR-001 的 A 自检完成, B 尚未复核代码或本次补充注释; 保留当前问题单, 等待维护者交 B 复核, 不同步主 specs、不归档。

<a id="b-review-acceptance"></a>
## B 复核签收

2026-11-15, 维护者将 A 的 CR-001 修正交回本独立 B 会话。B 实算当前 21 文件集合 SHA-256 为 `c4b3014546e3befd78657902f335f2a6387cc886adf9fb8413dddeb7c58aab07`, 与 A 交接一致。唯一改变的 `Editor/Panels/Workspace.cpp` 为 `8232db74172d709dd68010ec5103cfe93c93c39aa53d2b1c95631d5bb5ae8d08`; 其余 20 文件与 B 首轮注释后记录逐字节相同, 首轮完整审查与注释结论继续适用。

B 完整复读 Workspace 并核对其最小整改 diff: 新增宽度受视口/字体约束的换行范围, 覆盖恢复状态、首错和附加原因, 成对恢复文本样式; 保留原有诊断存储、Inspector 显隐、会话恢复状态及所有既有注释。新增注释准确说明边距/滚动空间和超高内容的读取方式, 本次无需再改源码。

B 实际构建正式 Debug imgui, 审阅并执行已有 Extract.py 重新抽取当前菜单, 重新构建 Debug MenuProbe 并运行。命令如下, 构建及运行均返回 0:

```powershell
cmake --build build --config Debug --target imgui --parallel 2
python build/d05-t05/cr-001/menu-probe/Extract.py
cmake --build build/d05-t05/cr-001/menu-probe-out --config Debug --parallel 2
./build/d05-t05/cr-001/menu-probe-out/Debug/MenuProbe.exe
```

菜单构建首次被沙箱 SDK 探测访问限制阻止, 工具授权重试后成功; 没有源码编译失败或旧 exe 替代。此次 exe SHA-256 为 `8e85ede823945897a387740283fa743327e1e7d8675d1c6fee4a22e107ce7ae0`。运行后 21 文件源码集合仍与交回一致。

- 原菜单在 640×480 的裁剪仍可复现, 用于确认检查能区分修正前后行为。
- 修正后的 640×480 文本右界为 `347/501/529`, 均在可见右界 `536` 内, 无横向剩余滚动; 1280×720 的 Normal 恢复快照及原有错误正文通过。
- 两段 1900+ 字节连续长文本有纵向滚动条, `scroll_max_y=263`; 滚动末尾底界 `466` 小于可见底界 `473`, 两段正文原值保留。
- B 本次复用 A 对匹配修正输入的 Release 布局及两配置 Editor 目标构建记录, 未扩大 GPU 测试矩阵。`git diff --check` 和修改 Markdown 的本地链接/锚点检查通过。

**B 结论: CR-001 已关闭, 当前代码正确性与完整文件注释可读性均复核通过, 没有新增正式问题。** 由 B 同步勾选首轮已完成的 5.1 及本次 5.3, 不代勾 A 的最终项。详细问题历史、覆盖及证据保存在 [CodeReview.md](../../../../Docs/CodeReview.md)。

本次签收允许 A 进入 6.1–6.3, 不代表最终原生 verify 或 D05 阶段出口已经完成。B 没有运行产品窗口/GL、没有同步主 specs 或归档。原始 A GPU 结果对应其当时输入, 当前版本仍需按既定影响范围完成最终 verify; 真实 Scene 时域历史和产品读回的阶段限制保留。签收后若代码或构建输入改变, 再交 B 复核受影响范围。


<a id="final-verification"></a>
## 最终 verify 与 D05 阶段出口

2026-11-15, B 签收后 A 核对当前 21 文件集合仍为 `c4b3014546e3befd78657902f335f2a6387cc886adf9fb8413dddeb7c58aab07`, 随后从这份源码重新构建 Debug/Release 正式产品和既有原生消费者。完整正式输入集合 SHA-256 为 `ee500c393b0863cf5e9ad28b44b8413a781028df2116b23c7091fa561929712f`。没有修改 B 签收后的代码或构建规则。

### B 签收与 CR 留存

唯一发现 CR-001 [P2] 已闭环: 原菜单单行长正文裁剪了 Pass/资源等定位; A 增加按当前视口/字体约束的完整换行及既有纵向滚动, B 重新提取当前源码并构建 Debug CPU 菜单消费者复核通过, 原正文未缩短。A 的 Release 布局与两配置 Editor 构建按相同签收输入复用。B 分别签收代码正确性和注释可读性, 5.1/5.3 已由 B 自行勾选, 没有新增问题。CR 详情、原始测量及修正/复核证据见本页前两节, 不依赖重置后的当前问题单。

B 完整文件覆盖留存如下, 共 21 文件. 首轮补充 20 份 C++ 注释, 既有 CMake 说明保留; CR 修正后 B 完整重读 Workspace, 其它 20 文件以逐字节身份确认未变。

| 范围 | 签收文件 | 主要合同与注释结论 |
|---|---|---|
| Runtime, 4 文件 | Main.cpp, Frame.hpp, Frame.cpp, CMakeLists.txt | 唯一产品入口、有限候选转移、帧槽、错误拥有语义、Close/提交区间和 UI/View 发布分离通过 |
| Engine/Renderer, 2 文件 | View.hpp, View.cpp | 正常候选/只读已接受快照、请求代、强引用、不原地覆写及禁止 retained 发布通过 |
| Editor, 10 文件 | Session/EditorSession.hpp/.cpp, Session/UiFrame.hpp, ImGui/UiRenderer.hpp/.cpp, ImGui/UiTextures.hpp/.cpp, Panels/Workspace.hpp/.cpp, Viewport/Viewport.cpp | 会话/批次/绑定 owner、三种 UI 模式、逐请求确认、取消后重试、驻留快照、菜单完整可读及 Inspector 独立性通过 |
| Platform, 5 文件 | DeviceHost.hpp/.cpp, DeviceHostInternal.hpp, RhiDeviceHost.cpp, Presentation.cpp | Fatal 与原生健康分离、首错捕获、取得及失败目标完成后换代、前缀保活和受控退出通过 |

B 的交叉检查还覆盖直接消费的 Renderer/FrameGraph、RenderGraph、FrameArena、Presentation 值、Validation、OpenGL 列表/状态/提交/Event/关闭及固定 ImGui 协议。该范围不等于未改模块的全仓重审。B 未运行 GPU 矩阵, 本节以下原生结果由 A 在签收后实际执行。

### 最终构建与运行

沿用现有消费者, 在 `build/d05-t05/final/consumer/` 取得当前签收的 Runtime/Presentation 源码副本并加入原有四个帧边界探针和一个 Event 拒绝, 没有新增测试场景或产品测试入口。原先 A/B/整改材料保留。实际命令如下:

```powershell
python build/d05-t05/final/PrepareFinal.py
cmake --build build --config Debug --target Physara --parallel 2
cmake --build build --config Release --target Physara --parallel 2
cmake -S build/d05-t05/final/consumer -B build/d05-t05/final/consumer-build -G "Visual Studio 17 2022" -A x64
cmake --build build/d05-t05/final/consumer-build --config Debug --parallel 2
cmake --build build/d05-t05/final/consumer-build --config Release --parallel 2
python build/d05-t05/final/Evidence.py
```

全部构建返回 0, 正式编译选项和检查保持, 本次无 C1001。Evidence.py 实际对每个配置运行 `D05T05Verify.exe <对应 EditorUi.pshader> diagnostics/recovery/stops/textures/integration` 五组消费者, 再运行正式 `Physara.exe --hidden --poll-limit=6`。10 组原生消费者执行和 2 次正式产品运行全部返回 0; 逐次 argv、返回码、exe/log hash 在 `final/runs.json`, 正式输入、fixture、库、Shader 包和可执行文件身份在 `final/source-artifact-manifest.json`, 执行前后均未改变。旧 exe 没有用于当前签收版本的结论。

| 最终检查 | 实际结果 |
|---|---|
| diagnostics, Debug/Release | 普通 Error 的独立 scope、首错拥有寿命、Fatal 不被新 scope 清除、禁止新取得及可用 context 收拢通过 |
| recovery, Debug/Release | 主图拒绝后只执行一张独立安全图, 旧尺寸像素和有效设置保持, 无旧输出时不伪造 View; UI/声明/fallback 编译取消能继续; 合法失败前缀有票据, Pending 保活, 完成后目标换代及正常帧通过 |
| stops, Debug/Release | UI/临时读回/fallback 准备 Fatal、实际纹理/recipe 创建失败、关闭/末端复制/提交失败、已获 ticket 后观察拒绝、失效 context 退出及初始必需 UI 准备失败门控通过 |
| textures, Debug/Release | 安全图逐请求确认、取消前缀不确认、下帧重试、区域外 sentinel、裁剪请求保持、Prepare 后注销驻留仍保有原像素及正确退休通过 |
| integration, Debug/Release | 三槽背压、原生最小化/恢复、窗口 resize、实际上传 lease 压力下 Open 前 Pending、退休后恢复及带在途工作的关闭通过 |
| 正式产品, Debug/Release | 同一产品装配和配置匹配 Shader 包, hidden 有界运行及关闭返回 0 |

### 要求与场景核对

四份 delta 共 8 项 ADDED/MODIFIED 要求, 31 个场景. 本次逐项核对其声明、实现入口及相应证据, 不把一个场景机械拆成一个新测试。原有未改变的范围/身份拒绝分支结合源代码、B 审查和对应归档验收复用; 当前改变的失败/恢复/退休路径使用上述新构建的原生结果。

| 要求 | 当前实现入口 | 场景与证据 |
|---|---|---|
| Classify frame failures with owned diagnostics (3 场景) | Runtime/Frame.cpp:101,443; Editor/Session/EditorSession.cpp:247; Editor/Panels/Workspace.cpp:291 | 普通 UI 准备拒绝、fallback 再失败保留首错、独立 scope 不改写原候选: diagnostics/recovery; 完整可读性复用 CR-001 同版本 CPU 布局和 B 复核 |
| Compile an independent safe UI fallback before recording (4) | Runtime/Frame.cpp:335,443; Engine/Renderer/View.cpp:72 | 主图拒绝、无旧输出、单次 fallback 再失败、执行开始后不再切图: recovery/stops, 目标/绑定重建与生产者缺席同时核对 |
| Latch fatal graphics diagnostics before further frame work (3) | Platform/RhiDeviceHost.cpp 的 Message; Platform/DeviceHost.cpp:535; Runtime/Frame.cpp:335,443 | UI/临时 Readback 晚期准备、fallback 准备、可用/失效原生环境: diagnostics/stops; Fatal 与原生健康保持独立 |
| Publish only accepted graph outputs (3) | Runtime/Frame.cpp:210; Editor 的 Finish; Engine/Renderer/View.cpp:208 | 匹配提交接受、失败保持 pending、安全图只确认自身请求: recovery/textures, 不借 fallback ticket 发布原主图 |
| Resolve a stable viewport token to the current output version (3) | EditorSession::PrepareUiFrame; UiTextures::FindSample/ResolveGraph; View::LastAcceptedOutput | resize 身份及在途原对象、安全图旧尺寸显示: recovery/textures; 过期/缺失匹配拒绝沿原有 token 合同和未改拒绝分支核对 |
| Confirm effective viewport settings from accepted output (4) | View::Prepare/Publish; Runtime/Frame.cpp:210 | Pending 保留有效值、正常匹配发布、安全图与当前请求分离: recovery/integration; 原有不支持尺寸拒绝/后继恢复逻辑保持, 结合 T04 既有验收复用 |
| Cancel failed frames without discarding issued work (6) | Platform/Presentation.cpp:112,203; Runtime/Frame.cpp:185,210 | 复制前失效、复制错误、零票据、无命令 token 取消、合法前缀退休换代、观察失败保留票据: recovery/stops/integration |
| Exit in ownership order with meaningful results (5) | Runtime/Main.cpp 的 RunEditor/main; Runtime/Frame.cpp:443; 宿主 Shutdown | 正常关闭/再次启动、启动后不可恢复失败、已处理错误继续、初始字体失败、准备 Fatal: recovery/stops/正式入口; 外层退出码分派及 owner 顺序同时静态核对 |

| verify 维度 | 结论 |
|---|---|
| 完整性 | 8/8 要求与 31 个场景均有实现/证据映射, 独立 A/B 闭环及规范同步完成; 收尾进度 16/16, 规范同步、归档、计划更新及问题单重置均完成 |
| 正确性 | 最终签收输入的 12 次运行全部通过; CR-001 同版本布局证据复用, 没有新的功能缺口 |
| 一致性 | 单一 Runtime/Graph/RHI 链, 明确 owner/发布资格, 无新正式测试入口/依赖, 现有风格及完整文件注释已获 B 签收 |

完整性、正确性及一致性核对通过, 16/16 任务完成, 无未处理的实现、设计或收尾问题。测试环境与范围限于 Windows x64 / MSVC 19.44.35228 / SDK 10.0.26100.0 / Intel RaptorLake-S Mobile / GL 4.6.0 Build 32.0.101.6129。定点注入不代表自然 OOM/设备丢失; 延后完成观察不代表性能测量。未声称其它 GPU/OS、额外多显示器/32 位索引矩阵、真实 Scene 时域历史或产品截图/拾取验证。

### D05 出口核对

D05-T01 至 T04 已分别完成独立审查及验收归档。本次未改变 RHI 或 RenderGraph 核心, 相应未初始化读、feedback/环拒绝、稳定依赖/裁剪与单一同步权威沿既有已验收代码和证据复用。当前正常产品帧只经统一 Graph 到窗口; 编图前失败可读未覆写旧输出或明确占位, 执行失败取消且保活前缀, Fatal/不可靠完成沿受控退出, UI 请求可重试而 NoSceneOutputs 不冒充场景历史。D05-T05 的三项包内目标及 D05/M0 正式帧骨架出口在当前诊断 profile 下满足; D06 的真实资产/场景工作尚未开始。


### 主规范同步及归档前状态

已按当前 status 返回的四份 delta 和官方 specs instructions 完成合并, 未改变主规范 Purpose 或未提及要求。editor-frame-graph 新增 3 项要求并修改 1 项, editor-viewport-output 修改 2 项, opengl-window-presentation 与 runtime-lifecycle 各修改 1 项。被修改要求的原有 12 个场景全部保留, 每项 delta 的正文和场景与主规范逐项一致, 重复合并结果不变。同步后四份主规范分别为 9/4/6/4 项要求、24/12/19/15 个场景, 其余 18 份主规范、Architecture 和已有归档未变。

官方 `validate --all --strict --json` 同步后为 23/23。收尾仅剩包含归档动作本身的 6.3, 按维护者本次“按文档规范流程继续推进”的指示执行已签收 change 的同步/归档, 不提前把尚未完成的归档、链接更新或问题单重置勾为完成。CLI 归档使用 `--skip-specs --yes --json`, 因为主规范已单独合并且复核一致; 不是跳过规范同步或验证。归档成功后再完成余下收尾并勾选 6.3。


### 归档及计划收尾

官方 CLI 已将 change 归档为 `2026-11-15-handle-frame-graph-failures`。主规范在归档前已同步并复核一致, CLI 的 `specsUpdated: false` 表示本次移动没有重复改写主规范。归档内相对链接按新深度更新, 既有归档内容保持不变。

已更新 D05-T05 三项完成状态和 D05/M0 出口, 当前阶段推进为 D06, 下一工作包 D06-T01 前置齐备、待建立 change。最终验证、CR-001 闭环、B 的 21 文件代码/注释签收、源码身份和范围已完整留存在本 tasks, 因而归档成功后按模板重置唯一的 Docs/CodeReview.md。没有开始 D06 实现或创建额外 change。

归档后的内部及外部入口共 11 份 Markdown、203 个本地链接/锚点检查通过。6.3 在这些动作实际完成后勾选, 最终 16/16。

归档后官方 CLI `validate --all --strict --json` 为 22/22 主规范通过, `list --json` 无活动 change。最终闭环检查再次确认 B 签收的 21 文件、全部原生输入/产物及历史保护文件未变, 当前问题单与模板逐字节一致; 结果记录在本机 `final/completion.json`。
