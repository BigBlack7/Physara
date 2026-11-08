# Tasks

## 1. A: 承接 CR-001 整改与最小自检

- [x] 1.1 在原 ResolveBuffer owner 接受零长度剩余范围并补充其公共来源说明, 保留 B 的全部有效注释; 对照 diff 确认显式长度/offset、非空、对齐、record stride 和实际限制检查不变, 不改 Common/Validation/公共 ABI.
- [x] 1.2 在既有 build 消费者以512-byte CB全范围和stride=32 SSBO非零起点范围对照零长度、显式长度、全一哨兵, 各增加一个剩余+1越界, raw/Validation核对创建及精确记录; 保留 Release 修复前失败, Debug/Release 修复后通过, 记录输入/产物及实际诊断, 不扩展矩阵.
- [x] 1.3 重建并运行受影响 T05 的规定共享链/独立进程复现及必要正式消费者, 保持原图案和容差; 在共享 CodeReview、本 tasks、T05 tasks 和开发计划关联本修正, 更新当前身份与旧证据适用范围, 严格校验/链接/diff通过后交 B 复核.

## 2. B: 同轮复核与注释验收

- [x] 2.1 独立复核 CR-001 修复、实际回归证据、最终输入及完整 Bindings.cpp 注释, 在共享问题单与两份 tasks 按各自范围确认代码/注释结论; B 的项仅由 B 勾选, 未通过保留问题与状态.

## 3. A: 最终验证、同步与归档

- [x] 3.1 B 最终验收后按原生 verify 核对本 delta/设计/实际实现及当前证据, 保留 CR处理、B接受版本和限制; 未完成的必要检查不计通过.
- [x] 3.2 最终 verify通过后同步本次 opengl-shader-pipelines增量并严格校验, 归档后更新引用和完成状态; 与T05的关联范围全部归档/留存前保留共享问题单, 不提前重置或改写历史归档.

## 验证预算与停点

本 change 承接已完成的 A 开发交接及 B 首轮审查/注释, 当前是 CR-001 的 A 整改, 不是跳过或另起 A/B 轮次. B 首轮结论来自共享问题单, A 不代勾其原 T05 第5.2项. B 复核前不执行最终 verify/sync/archive.

唯一行为风险是接受剩余范围时误将显式越界 clamp 成成功. 第1.2项的两种用途、三种等价表示和各一个紧邻越界共享现有消费者, 原始显式起点检查保留并以源码核对, 不穷举所有对齐/limit/分配失败. 范围对照和受影响的实际图像链匹配当前输入后停止; B纯注释按L0对照, 不为其新增行为用例. 未改变的历史故障/退出矩阵只引用原合同和记录, 不声称重跑.

## 实施与审查证据

2026-11-08: A 在 B 接收的 Bindings.cpp 版本上仅调整 size 解析表达式及直接说明, 保留原始边界与其6行有效注释. 修复前使用 `python build/d03-t05/Evidence.py build Release` 构建当前回归, `run Release ranges` 于 `20261108T114818651998Z` 真实失败(退出1): CB/SSBO 在 raw/Validation 的 zero 均为0, explicit/sentinel 均为1, 紧邻越界均拒绝. 这与 B 的源码推导一致, 不是假故障注入. 原 A metadata、fixture zip、B 接收源字节及修复前构建身份分别保存于 `build/fix-opengl-binding-buffer-ranges/received/` 和 `before-fix/`; 旧 cdbd... 联合输入不称作当前原始字节输入. 修复后原生回归见下文, B 复核尚待.

修复后实际运行: Release full (`20261108T115032982419Z`)、独立进程 Release positive (`20261108T115116951061Z`)、Debug full (`20261108T115130249157Z`) 均退出0. 两配置的 raw/Validation CB `{0,0}`、SSBO `{256,0}` 都接受, 与显式和全一哨兵的精确记录分别为 `{0,512}`、`{256,256}`; 明确的513/257长度均不发布集合, 原有效集合/记录不变. 当前图像消费者使用零长度CB/SRV绑定, 原四角、深度/方向、copy/采样与sRGB预期均通过, 各配置范围回归预期4个 Error, 图像正向无新增 Error, 原T05非法Draw/feedback各自仍拒绝且关闭成功.

一次中间运行 `20261108T114920749865Z` 的范围回归已通过, 但原图案64-byte CB的零长度写法不满足 Validation 现行256-byte约束, 因而正确拒绝. 只把本地分配容量改为256, 生成payload仍为64, 不改Validation、Shader/schema、图案或容差; 该原始失败保留, 不算成功运行或扩大本修正范围. 当前行为检查达到预算, 停止增加用例; 正式消费者/身份与文档交接结果另列.

### 当前交回版本

branch `v2.0`, HEAD `5011b95bd8dbdb6a34596f4b3cca9054bf913a16`, 加实际未暂存/未跟踪输入. 唯一正式代码文件为 Bindings.cpp, SHA256=`225259a35cdc6b82d2577fe9b327a1633a307c6e263c95757a26c3116e41bbc6`. 相对 B 接收版仅多了零长度 size 分支及其直接说明, B 六行注释保留. 公共头、Common/Validation、Shader/JSON、正式CMake/依赖、Architecture、主规范与历史归档无改动.

当前原始字节联合输入为 `3ba5e8dc20a4558199036dd73bd7cf5c586b83140737db7d98cee82d0f789227`, 仍是1078份正式树文件加6份实际fixture的映射口径, 逐份清单和build/package/exe/cache检查见本地 `build/d03-t05/inputs-*.json`、`build-*.json` 与 operations.jsonl. 启动前核对当前输入与产物, 三次成功原生运行及正式消费者都对应这个联合输入, 不运行旧 exe 后冒称新版本.

| 当前产物 | SHA256 |
|---|---|
| Release NativeDrawGate.exe | `a2138a0c251a26739cecfc6b0cd275b536508cebcd3c4b163324d8a7ab1212da` |
| Debug NativeDrawGate.exe | `d31526a1be21d81bd933988f4c3c85fd2043e60a28fafd7dcf0782cee61cc887` |
| 正式 Release Physara.exe | `c931b376ccb492fefdbd570e36882704315fe95c6b2e787f04a82d7f1214a45a` |

Shader 包与生成接口未改变: ABI仍为 `21659c40a9a6b899241369ee6fe1be07b4fc0a65b8287759fecab102f9b58b28`, Release/Debug包文件摘要仍分别为 `655d66ca675d6174b394e505a735644ee2f5aa88eb01ee5ee7dcb440b71c37e3` / `a0815505973a461c27c1eb4b5ba96eaa982e281dee6440750112c267dbe32b45`. 正式 Release 用既有无 fixture binary tree增量构建当前 Bindings.cpp, `--hidden --poll-limit=3` 于 `20261108T115327070088Z` 退出0; 该现有正式工程原配置隔离性可复用, 无需重配或重跑其它矩阵.

当前复现输入归类为 `build/fix-opengl-binding-buffer-ranges/handoff-fixture.zip`, SHA256=`29f828236845e1bed703768aea4216d16ca7e6eaf2a68ac8a2075f731faf96fa`, 含现有消费者6份输入和Evidence.py, 不进入正式工程/代码审查或版本控制, 可随build删除. 修复前与中间失败原日志保持在 `build/d03-t05/logs/`, 接收基准另存, 不覆盖旧身份. 本轮只有上述真实Intel/Windows环境, 无跨GPU/OS、自然OOM/loss、性能、呈现或长期压力结论.

共享CodeReview中CR-001的A项已勾, B复核仍未勾. T05只勾A第5.3项至12/17, B第5.2依其原编辑范围未勾, 待其按已有首轮报告自行补签; 本修正的B第2.1及最终收尾同样保留. 这些是A自检及交回事实, 不是B最终接受.

本地收尾: 两份change的 `openspec validate <change> --strict --json` 各为1/1 valid、无issues, 12份Markdown的194处本地文件/锚点引用及 `git diff --check` 通过. 本修正A项3/6完成, T05为12/17, 同一CR交回维护者调度的B复核; 这不是B最终签署后的verify, 未同步主规范、归档或重置问题单.

<a id="b-final-review"></a>
### B 整改复核与注释接受

2026-11-08: 上述3/6和12/17为A交回时的历史状态. B已在共享问题单勾选CR-001复核通过, 本项2.1完成, 当前4/6; T05由B补签5.2并勾选5.4至14/17. 两份change的A最终收尾项仍未勾选.

B接受 `v2.0` / HEAD `5011b95bd8dbdb6a34596f4b3cca9054bf913a16` 加实际工作区, Bindings.cpp SHA256=`225259a35cdc6b82d2577fe9b327a1633a307c6e263c95757a26c3116e41bbc6`. 逐项diff确认功能修改仅扩展既有size分支: offset<capacity先成立, 零/全一长度取剩余, 明确长度原样校验, 非空/对齐/record stride/实际block限制不变. 对照公共Resolve、Validation和实际NativeBinding消费者, 未新增解析/状态owner或clamp成功路径. 完整Bindings.cpp/.hpp的职责、调用约束、发布/回滚注释与实现一致, B六行说明保留, 本次无需再改源码; 代码正确性与注释可读性均接受, 未发现新正式问题.

B在不遍历build树、不审fixture代码的前提下, 只核对点名的两配置输入清单、构建记录、操作记录及必要日志. 两配置各1084项当前文件散列与清单一致, 重算映射摘要均为 `3ba5e8dc20a4558199036dd73bd7cf5c586b83140737db7d98cee82d0f789227`; 两配置各13项构建产物以及三次成功图案运行和正式Release启动记录的产物摘要与当前文件一致. 修复前zero失败、修复后CB `{0,512}` / 非零起点SSBO `{256,256}` 三表示等价和513/257拒绝符合源码, 原固定图案及IB/feedback拒绝结果保留. 中间64-byte CB零长度请求因既有Validation条件失败的记录不算成功, 当前图案按design使用256-byte分配, 不放宽公共输入规则.

复用A已记录的Release full `20261108T115032982419Z`、独立positive `20261108T115116951061Z`、Debug full `20261108T115130249157Z` 及正式启动 `20261108T115327070088Z`; B未新增用例、编译或重跑GPU. B实际运行源码/注释/diff/输入产物身份和文档链接/两份change严格结构检查, 不将这些检查代替A最终verify. 其余环境、呈现、公共readback、OOM/loss、压力及性能仍未覆盖. 更完整的主线覆盖留存在 [T05 注释覆盖](../2026-11-08-establish-opengl-native-draw-gate/tasks.md#annotation-coverage) 和 [T05 B留存](../2026-11-08-establish-opengl-native-draw-gate/tasks.md#b-final-review); CodeReview仅为可重置的当前问题单入口.

下一步由A在B接受版本上完成本change的最终verify、对应规范同步/归档, 并完成T05的独立收尾. 两份关联范围都归档并留存前保留共享问题单; 源码或构建输入变化须重新交B复核实际影响.

B文档收尾实测: 两份change严格结构校验各1/1 valid、无issues, 12份Markdown的198处本地文件/锚点引用及diff检查通过; 非注释token核对仅有预期零长度条件变化. 更新本轮文档/勾选后, 两配置当前输入仍匹配接受摘要, 无源码或构建输入漂移.

<a id="final-verification"></a>
## A 最终 verify 与收尾

2026-11-08, B 已完成本第2.1项及共享CR-001最终复核后, A按原生 openspec-verify-change 核对本change的status/apply context及全部artifact、Bindings.cpp的实际diff和公共/Validation/NativeBinding消费者. 接受版本与上方B签署一致, 正式代码SHA256仍为 `225259a35cdc6b82d2577fe9b327a1633a307c6e263c95757a26c3116e41bbc6`; 本轮未改源码或构建输入.

| 维度 | 最终 verify 核对结果 |
|---|---|
| 完整性 | 最终6/6完成, 1条MODIFIED requirement完整保留原3场景并补入2场景. 进入verify时4/6, A第3.1–3.2在本轮实际完成verify/同步/归档及留存后才勾选 |
| 正确性 | ResolveBuffer第98–124行先证明offset合法, 零/全一长度计算剩余, 显式长度不裁剪, 原非空/CBV/对齐/记录粒度/block limit不变; CreateBindingSet在第234行存下精确range, 后继图形候选继续核对活动extent/stride. 1/1 requirement及5/5场景有实现/证据 |
| 一致性 | design的3项决定均满足: 原owner的一处分支、既有build消费者的有限范围对照及受影响图像链、同一A/B问题单下分别收尾. 未引入第二解析owner、公共API/ABI或默认测试目标, B完整Bindings.cpp/.hpp的注释验收有效 |

场景证据分别为: 原依赖保活、错误/外来资源拒绝、非法active range的既有实现/未改变合同由B源码核对并按D03-T02归档范围复用; 新剩余范围场景由修复后Debug/Release的raw/Validation CB `{0,512}`、SSBO `{256,256}`三表示精确等价及实际图像消费覆盖; 新越界场景由513/257拒绝覆盖, offset本身的原预检未改并经源码确认. 不为旧3场景重新扩展原生矩阵.

A实际执行本轮两份change严格结构校验(各1/1 valid、无issues)、文档链接与diff检查, 逐项核对当前两配置各1084份输入/13份产物和正式Release产物, 联合SHA256仍为 `3ba5e8dc20a4558199036dd73bd7cf5c586b83140737db7d98cee82d0f789227`. 原生结果复用上方已匹配的三次修复后图案运行及正式启动, 本轮未编译/运行GPU. 修复前zero失败与中间64-byte CB拒绝均保留, 不计通过; 最终256-byte分配/64-byte payload不改变公共输入约束.

CR-001发现、修正、B复核及完整受影响注释结论已在本tasks和[T05覆盖留存](../2026-11-08-establish-opengl-native-draw-gate/tasks.md#annotation-coverage)保留. 适用verify检查均已执行, 无实现缺失、场景/设计/代码模式警告或建议; 仅余本轮A收尾. 其它设备/OS、呈现/公共readback、自然OOM/loss、压力和性能未覆盖且不属本修正范围. 两份change均归档及留存完成前不重置共享问题单.

规范同步: 按本change当前specs指令快照将完整MODIFIED块合入 `opengl-shader-pipelines`, requirement仍为9条, 受影响块的原3场景逐字保留并增加2场景. 其它8条、Purpose及顺序未变, 本delta与主规范对应块完全一致. 全部主规范严格校验12/12 valid, 无ERROR/WARNING, 段落长度INFO不涉及本修正; 两份关联范围的归档/留存仍待实际完成后勾选3.2.

### 归档结果

官方 `openspec archive fix-opengl-binding-buffer-ranges --yes --skip-specs --json` 已退出0, 实际归档为 `openspec/changes/archive/2026-11-08-fix-opengl-binding-buffer-ranges/`. 主规范已独立同步且逐块匹配, skip-specs避免重复写入, 未跳过CLI默认验证. 当时仅第3.2项的实际归档/后续确认尚待, 保留5/6至归档成功. 关联T05随后也已归档成功, 两份change的10份artifact及12份主规范在移动前后摘要一致, .openspec.yaml保持原字节; 本轮归档后的修改限于迁移引用和收尾记录/勾选, 无更早归档或正式代码变动.

CR-001的处理、B最终正确性与注释结论、输入/运行/限制及注释覆盖已在本tasks和T05留存. 本修正作为D03-T02的存量补充关联到开发计划, 不计作T05新增能力; 本轮全部关联范围已完成归档, 共享问题单才可按模板重置.

最终后核对: 两份change均已不在活动清单, 全部主规范严格校验12/12 valid、无ERROR/WARNING, 12份Markdown的186处本地文件/锚点引用通过. CodeReview在两份归档/留存完成后已恢复模板且原始字节相同, 此后勾选3.2; 本修正6/6、T05为17/17完成. 最终verify的CRITICAL/WARNING/SUGGESTION均为0, 适用检查无跳过, CR-001及本轮关联流程闭环完成. 本轮没有再次编译/GPU运行、提交或推送.
