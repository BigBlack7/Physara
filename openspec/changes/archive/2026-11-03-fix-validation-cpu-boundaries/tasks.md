# Tasks

## 1. 交接与边界

- [x] 1.1 A 固定收到的 HEAD、暂存/未暂存/未跟踪文件及 62 个源/构建文件摘要, 阅读规范、相关架构、主规格、B 问题与消费者; 记录与 B 历史摘要差异, 保留全部交入注释和历史归档.
- [x] 1.2 A 按官方 spec-driven 指令完成 proposal/spec/design/tasks, 自检需求和 CR-001/002/003 映射及文档职责, 通过 strict 结构检查后开始功能整改.

## 2. CR-001 与 CR-002

- [x] 2.1 A 修正 descriptor-set registerSpace 越界路径并同步入口注释/来源差异; Debug/Release 经公共 Pipeline 入口验证 0/7/8/16/UINT32_MAX, 非法值零底层委托, 合法值结果不变.
- [x] 2.2 A 调整 Pipeline Shader/绑定汇总工作集的异常边界并同步注释/来源差异; Debug/Release 逐分配点验证公共入口可捕获 bad_alloc、无 terminate/部分委托, 回归空/单/多 layout、重复绑定和正常委托.

## 3. CR-003

- [x] 3.1 A 修正两个纹理容量查询及 TextureState 初始化并同步注释/来源差异; Debug/Release 验证查询无分配、实际状态构造可捕获异常、公共 CreateTexture 合法/失败结果.
- [x] 3.2 A 以正式 tracker 的首次跟踪/子资源展开逐分配点注入失败, 验证 HasErrors、零 metadata 发布、Reset 后复用及正常子资源状态, 留存 Debug/Release 结果.
- [x] 3.3 A 用正式 Host/Validation/OpenGL 验证失败后已发出上传前缀仍保活且仅在真实完成/受控退出退休, 正常颜色纹理内容读回正确, 明确辅助/产品边界及实际环境.

## 4. A 集成自检与交回

- [x] 4.1 A 重建 Debug/Release 生产消费者与独立通用消费者, 运行 Runtime 冒烟及相关回归, 检查第一方警告、来源输入、注释 diff、文档链接和 git diff --check; 未覆盖项明确记录.
- [x] 4.2 A 在 CodeReview.md 逐项填写修正和自检, 只勾 A 框; 在本 tasks/Developments 留存当前范围、最终输入/检查入口/结果/限制并交维护者安排 B, B 框及收尾任务保持待验收.

## 5. 独立复核与收尾

- [x] 5.1 B 按 CodeReview.md 对最终实际 diff、受影响消费者、注释和检查输入执行 Check/Annotation, 逐项关闭 CR-001/002/003 或保留需整改项; 由 B 留存版本与验收结论.
- [x] 5.2 A 按 B 追加意见完成必要整改和自检, B 再次复核并留存结果; 无新增整改时由责任会话记录不适用依据, 不虚构修正. 本次 B 无新增正式问题, 追加 A 整改不适用; 两条 B 注释已核对并包含在最终构建版本内.
- [x] 5.3 B 通过后对匹配最终版本执行原生 verify 并留存 completeness/correctness/coherence 证据; 检查主规格同步预览、问题关闭和文档边界.
- [x] 5.4 按授权同步 delta 并使用官方 CLI 归档, 在 tasks/Developments 留存 B 结论/问题及复核摘要/证据后, 归档成功才重置当前 CodeReview 问题单.

## A 整改与本地检查记录 (2026-11-03)

以下记录 A 交回时的状态: A 实施/本地检查 9/9 完成, 当时全部任务 9/13, 三个 B 框及四项后续任务均未勾选. B 的本次独立复核与最终状态另见本文件末尾, 不以 A 自检或 CLI planning-ready/strict 替代验收.

### 范围与版本

基准 HEAD `1c897364167677b0df283afa187279d48a8f5c78`, 接收 58 个已跟踪未暂存文件及未跟踪的 CodeReview.md/tpl, 暂存为空. 接收 62 文件清单 `617880326e645c15cfdca260ab7d1f281c8fb5a3afb120e4aa36b0941afe4a7a` 与 B 记录不同且 TEMP 原始清单当前不存在, 已按实际输入重新核对和检查, 不继承不匹配的旧运行结果. 接收的 54 个 C++ 文件与 HEAD 的非注释 token 对照一致; 本轮只在四份 Common/Validation 源/私有头改变行为, 其余源/CMake 字节未动. B 注释保留, 删除私有 hash 域时仅移除两个失效 namespace 结束注释. 来源差异写入 RHI/README, 长期架构、八份主规格和已有归档共 57 份受保护文件保持不变.

最终 62 文件清单为 `980f8430cfd07458a9de4c4266f85a9106dacdf246676c84e2cad729e9a1f0d8`. 原始输入、A 相对接收版本的源码 diff、最终已暂存/未暂存/未跟踪清单、相关文件快照、检查输入及二进制身份位于本机忽略目录 `build/agent-a-review` 的 input-manifest.json、input/、input.diff、input-staged.diff、input-status.txt、a-source-corrections.diff、final-source-manifest.json 和 handoff.json. 不要求强制提交后交接; 如跨工作树, B 必须连同未跟踪 change、问题单/tpl 和本地检查输入核对实际内容.

### 修正与场景证据

| 对应项 | A 修正与实际 Debug / Release 结果 |
|---|---|
| CR-001 / descriptor-set index | 超限诊断后立即失败. 0/7 各委托一次并保留 raw 空结果; 8/16/UINT32_MAX 均错误可见且零委托. 同槽不同 set、非 descriptor-set 的 UINT32_MAX 语义保留. |
| CR-002 / Pipeline CPU failure | 普通数组消除 StaticVector noexcept 元素构造/赋值, Shader vector 使用 count=0. 实际失败栈又确认 xhash:261 内部 noexcept proxy 分配, 因而私有集合改 std::set, 保留完整语义键. Compute/Graphics/Meshlet CPU 的 0/1/3/8 layout 各逐分配点检查合计 543 / 324 点, bad_alloc 均可捕获且零部分委托, 正常结果与重复绑定拒绝通过. |
| 共享绑定汇总消费者 | BindingSet 的两个 arrayElement 与缺项检查通过, 集合交/并/差的相关分配失败 29 / 19 点可捕获; 不将相同 slot 的不同 set/arrayElement 合并. |
| CR-003 / query and TextureState | 两处长度查询不构造 vector, 无分配上界与实际受支持 vector.max_size 相同. 命名纹理公共 CreateTexture 与直接 TextureState 构造在 Debug 各 1 个失败点可捕获; Release 各无分配并保持普通合法结果. 由下层明确抛出的创建失败仍可捕获, 工厂自身 OOM 返回空. |
| CR-003 / tracker | 首次纹理跟踪 5 / 3 点、子资源展开 2 / 2 点及 Buffer 索引 4 / 3 点均锁存错误, 不发布 permanent/initialized 元数据, Reset 后新记录和合法提交可用. 私有 std::map 避免同类 hash proxy 的 noexcept 分配. |
| 已发上传前缀 | 正式 Host/Validation/OpenGL 中先发大 Buffer 持久映射上传, 对新纹理首次/展开注入 5+2 / 3+2 点 CPU 故障. 错误列表零成功票据, 已发前缀和资源/chunk 在外部引用与列表释放后继续保活, owner 真实 orphan fence 完成才退休/删除. 辅助读回比对独立已知字节, 不发布票据或退休存储. |
| 提交预检与颜色内容 | Validation 唯一列表集合改 std::set, 5 / 3 点 CPU 预检失败返回 0 且不消耗 Closed 列表; 同一列表正常重试成功, 已知颜色纹理字节在真实完成后读回一致. 没有新增公共 readback/Shader/Draw 支持. |

CPU double 仅记录/提供资源描述和底层返回, 直接链接正式 Common/Validation; Meshlet 的 API 身份仅用于 CPU 合同分支, 不声称任一原生后端支持. 注入只围绕待测调用, 预备工作在开关外, callback 使用固定存储. Debug iterator 检查未关闭, 未将受控失败说成自然 OOM. 通过日志是 cpu-Debug-v5.log、cpu-Release-v5.log、generic-Debug.log、generic-Release.log、native-Debug-final.log 和 native-Release-final.log.

### 构建与复现入口

工具: Windows x64, MSVC 19.44.35228, SDK 10.0.26100.0, CMake 3.30.0-rc4, OpenSpec 1.13.2. Native 设备为 Intel RaptorLake-S Mobile, GL 4.6.0 Build 32.0.101.6129. 当前本地输入已具备, 从仓库根执行:

```powershell
python build/agent-a-review/Generate.py
python build/agent-a-review/Evidence.py configure
python build/agent-a-review/Evidence.py build-Debug cmake --build build/agent-a-review/out --config Debug --target CpuChecks --parallel 4
python build/agent-a-review/Evidence.py cpu-Debug build/agent-a-review/out/Debug/CpuChecks.exe
python build/agent-a-review/Evidence.py native-build-Debug cmake --build build/agent-a-review/out --config Debug --target NativeChecks --parallel 4
python build/agent-a-review/Evidence.py native-Debug build/agent-a-review/out/Debug/NativeChecks.exe
python build/agent-a-review/Evidence.py configure-generic
python build/agent-a-review/Evidence.py generic-build-Debug cmake --build build/agent-a-review/generic-out --config Debug --target GenericChecks --parallel 4
python build/agent-a-review/Evidence.py generic-Debug build/agent-a-review/generic-out/Debug/GenericChecks.exe
python build/agent-a-review/Evidence.py product-build-Debug cmake --build build --config Debug --target Physara --parallel 4
python build/agent-a-review/RuntimeChecks.py Debug
python build/agent-a-review/Audit.py
```

Release 将配置名及程序目录中的 Debug 换为 Release. 实际执行使用本机已安装 Python/CMake 的绝对路径, 完整 argv、退出值和原始日志在 runs.jsonl; 上述工具名依赖 PATH, 不是生产构建的唯一机器路径. 新环境需取得这些忽略的本地检查输入; 仅拉取仓库不产生 fixture. Generate.py 仅从当前真实公开 Device 声明提取 68 个 double 签名, 不生成/复制校验算法, 不进入产品构建.

生产 root Physara、独立 generic 和 native fixture 三类构建均通过 Debug/Release, 第一方 /W4 /WX 保持. Runtime 每配置各 7 项通过: help=0、真实 hidden/poll-limit=3 启动/退出=0、unknown/zero-poll/duplicate/overflow=2、非法 Host 零宽=1. Root 与 generic 的实际依赖/编译项核对没有新增反向依赖、检查宏或 fixture/reference 接入生产; generic 只配置 RHI. 构建 stamp 检查源码/夹具和 exe 身份后再运行, 当前代码/构建输入以最终交接 manifest 为准.

### 异常、限制与待交接

中间实验和失败日志全部保留. 最初普通数组方案在 MSVC Debug 的 hash 内部 proxy 分配处仍 terminate, cpu-Debug-stack.log 记录调用栈并促成本设计第 3 项补充. 一次构建错误后曾运行旧程序 (cpu-Debug-ordered.log), 已明确排除该结果, 随后加上构建/运行身份门槛. MSVC 在最初 CPU Release、native Debug 和 root 产品 Release 构建的不同 STL 位置报过 C1001, 原因未定位; 相同源码和原选项重试成功, 未关闭优化、警告或 Debug iterator 检查. 不把编译器错误或旧程序输出计为通过.

A 的官方 strict 结构自检为 9/9, 仅有 native-window-host 主规格原有两条长需求 INFO, 无 ERROR/WARNING. Markdown 链接与锚点、受保护文件、HEAD/暂存区和 diff whitespace 通过; 当前链接总数见 final-source-manifest.json, 后续只新增证据链接时按该最终清单核对. 这不是 B 的独立验收或最终原生 verify.

本轮未执行 MSVC ASan、另一系统/编译器/GPU、自然 OOM/设备丢失、历史完整大矩阵、物理跨屏或 D03 绘制; 不重述成已覆盖. A 已完成本轮可执行整改与自检, 交维护者安排 B 对实际最终 diff、共享消费者、注释及检查输入复核. 正式问题全部由 B 关闭后才执行任务 5.3/5.4; 未同步主规格、未归档、未重置问题单、未暂存/提交/推送.

## B 独立复核与最终原生 verify (2026-11-03)

### 输入、修正复核与注释验收

**独立 B Check/Annotation 通过. CR-001/002/003 全部关闭, 无新增正式问题、撤回或非阻塞建议.** 任务 5.1 已完成; 5.2 的追加 A 整改不适用, 原因是 B 未发现新问题, 不虚构整改记录. B 两条注释已纳入最后构建并核对 token, 5.3 在 B 验收后完成以下原生 verify. 当前总进度 **12/13**, 任务 5.4 仍待后续同步/归档及问题单重置, 不称 all_done 或已归档.

B 实际接收 HEAD 为 `1c897364167677b0df283afa187279d48a8f5c78`, 暂存为空, 完整已跟踪 diff SHA256 为 `ceca832da32ef807ce232a55d03d503430cab7d14d8d435f4df9c16cb5c9674c`. 62 文件清单逐项匹配 A 交回的 `980f8430cfd07458a9de4c4266f85a9106dacdf246676c84e2cad729e9a1f0d8`, 12 份具名检查输入也与 A handoff 哈希一致. 未跟踪交接含问题单/tpl 和当前 change 的五份产物文件. 前序 B TEMP 已不可读取, 本次以 A 实际接收快照和最终内容独立复核, 不继承无法核对的旧二进制结果.

B 逐项阅读四个功能修改文件及其 BindingSet、Pipeline、提交和上传/退休消费者, 确认以下修正满足合同:

| 问题 | A 修正及 B 实际确认 |
|---|---|
| CR-001 | `ValidationDevice.cpp:1934-1942` 超限报告后立即返回, 不再写映射. 独立 Debug/Release 的 0/7 各委托一次, 8/16/UINT32_MAX 均零委托且诊断可见; 不误缩窄普通 register-space. |
| CR-002 | `ValidationDevice.cpp:1773` 的普通数组、2140/2222 的 count=0 vector 和共享 std::set 均允许本地分配异常进入既定边界. 完整 type/registerSpace/slot/arrayElement 比较与原 equality 一致, 集合交/并/差语义保持, 公开 StaticVector/公开 hash 不变. Pipeline、BindingSet 以及提交失败重试的独立结果见下方. |
| CR-003 | `ValidationDevice.cpp:99-111`、`StateTracking.cpp:104-107` 的查询不分配, 与支持配置实际 vector.max_size 一致; `StateTracking.hpp:68` 的 count=0 初始化可捕获, 两张 std::map 只索引稳定 extension 地址, 不取得 extension 所有权. 首次/展开故障锁存且不发布 metadata, Reset 后合法录制可用, 原生前缀仍保活到真正退休. |

B 在 `ValidationBackend.hpp` 补一条完整身份比较注释, 在 `StateTracking.hpp` 补一条指针 key 的全序和非拥有合同注释. 最终 62 文件清单为 `919551495a6a8739bbbae0bf446c3ab4bd33244ff280f6bf4578a4b3fd136c3c`; 相对 A 交回版只有这两条注释, 非注释 token 不变. 所有独立重建/运行均对应该最终源清单, 后续没有再修改源文件. 57 个受保护架构/主规格/历史归档文件不变; 生产 target、第三方、公开 ABI 和固定来源不变.

### B 实际构建与行为证据

完整记录见 [CodeReview 的 B01-B05](tasks.md#review-loop-closeout) 和本机 `%TEMP%\Physara-AgentB-20261103-check`. B 只按交接路径读取 A 的具名检查源码/日志/清单, 没有遍历 build 生成物; 独立构建执行和二进制输出均在 TEMP. 核对后复制的 CpuChecks/CpuDeviceBase/Injection/NativeChecks 与 A 的原哈希一致, 补充合同检查只调用实际实现, 不生成或替换校验算法. 为跨会话交接另将具名输入、命令、清单及原始日志保存到本机忽略的 `build/agent-b-review-20261103-check/evidence.zip`, 不包含 out 二进制/生成树, 不加入生产 target 或另建验证报告.

- MSVC 19.44.35228 / Windows SDK 10.0.26100.0 / CMake 3.30.0-rc4 / Ninja 1.13.2 的新目录 Debug/Release 构建全部成功, 包括正式 RHI/Core/Platform/OpenGL、产品 Physara、CpuChecks 和 NativeChecks. 保留 /W4 /WX、Debug iterator 及 Release 优化, 两配置零警告, 本次没有 C1001 或旧 exe 重用. 命令和结果在 `Configure.cmd`、`BuildDebug.cmd`、`BuildRelease.cmd`、`configure.log`、`build-Debug.log`、`build-Release.log`、`b-check-runs.json`.
- CpuChecks 两配置均 exit=0. Compute/Graphics/Meshlet 的 0/1/3/8 layout 共 543/324 个逐分配点异常全部可捕获且零部分委托; BindingSet 另通过 29/19 点和缺项拒绝. 命名纹理创建/TextureState 在 Debug 各 1 点可捕获, Release 各无分配; 两个容量检查零分配, 与实际 STL 上界一致. 纹理首次跟踪 5/3 点、展开 2/2 点、Buffer 索引 4/3 点均锁存错误、不发布元数据并可 Reset 复用. 日志为 `CpuChecks-Debug.log`、`CpuChecks-Release.log`.
- NativeChecks 两配置均 exit=0. Intel RaptorLake-S Mobile / GL 4.6.0 Build 32.0.101.6129 的实际上传中, 后续首次/展开故障共 7/5 点保留前缀、lease/chunk, 零成功票据; 外部释放后仍保活, 经真实 orphan fence 完成才退休及删除. 提交预检 5/3 点失败后同一 Closed 列表可成功重试; 正常颜色字节完成后读回一致, Host 正常关闭. 日志为 `NativeChecks-Debug.log`、`NativeChecks-Release.log`; 辅助同步读回不发布产品票据或退休内存.
- B 的 `fixture/SupplementaryChecks.cpp` 两配置均通过错类型/null/slot/arrayElement/view 拒绝、Buffer 溢出/合法边界、Texture mip/layer/sample、Unknown 前态/非法范围/永久状态冲突、丢弃/Reset/新录制合同. 独立新建产品 Runtime 每配置 7 项、共 14 项返回值全部符合 0/1/2 约定. 命令/日志/程序身份在 `BuildSupplementary*.cmd`、`SupplementaryChecks-*.log`、`runtime-*.log`、`b-supplementary-runs.json`.
- A 的独立 RHI-only generic 消费工程已检查源码、原始结果和匹配输入 stamp, 没有改动或失效证据; B 本次没有再构建该配置. 这与 B 自己新建的正式 CPU/native/产品构建分开记录.

### Verification Report: fix-validation-cpu-boundaries

本报告按项目锁定 OpenSpec 1.13.2 的原生 verify 技能执行, 在独立 B 验收后读取官方 list/status/instructions apply/show, 并结合实际 diff、设计、场景与以上构建/运行证据核对. 官方 status 的规划完成不代表实施验收, CLI strict 也不替代以下行为证据. 报告留在现有 tasks, 不新增 verification.md.

| Dimension | Status |
|---|---|
| Completeness | 12/13 任务完成; 3/3 delta 需求有实现, 2 MODIFIED + 1 ADDED, 无 REMOVED/RENAMED. 唯一待办为 5.4 同步/归档及重置, 无未完成实现任务. |
| Correctness | 3/3 需求映射一致, 12/12 场景有代码和检查证据; Debug/Release CPU、真实上传、补充合同及 Runtime 结果通过. 未以 CPU double 推导原生 Shader/Pipeline 支持. |
| Coherence | 五项设计决策与实际四文件功能修正一致, 比较键/所有权/异常边界及源码风格一致; 注释复核通过, 没有长期架构或第三方变化. |

场景映射:

| Delta 需求 | 实现与场景证据 |
|---|---|
| Reject unsafe inputs before device work, 5 场景 | Input 描述/range/view 检查和 `ValidateBindingSetItem` 拒绝错误初始化输入; B 补充合同覆盖错类型/null、溢出、mip/layer/sample、slot/arrayElement/view, 静态复核保留 heap 区间分支及 GL profile 的提前拒绝. 新增 descriptor-set 场景由公共 Compute 边界结果确认. |
| Expose invalid tracking and discard abandoned recordings, 5 场景 | CheckState/CheckTexture/Require、Reset 与 CommandListSubmitted 的错误和发布边界保持, B 补充合同确认未知/非法/冲突、丢弃及新录制. 逐分配点 tracker 与真实 native 前缀用例分别确认首次/展开失败和在途寿命两个新增场景. |
| Preserve ordinary CPU allocation failure boundaries, 2 场景 | 三种公共 Pipeline 入口逐失败点确认可捕获和零部分委托; 合法纹理容量查询零分配、实际状态构造和 factory OOM 确认各自既定边界. |

官方 `validate --all --strict --no-interactive --json` **9/9 通过**, 当前 change 无问题, 主规格仅保留 native-window-host 原有两条长需求 INFO, 没有 ERROR/WARNING. `show --json` 确认三个 delta. 日志为 `openspec-final-strict.log`、`openspec-final-show.log`.

主规格同步只做只读预览: rhi-common-validation 的 11 项要求应用两个修改和一个新增后为 12 项, 其余 9 项要求正文保留, 无删除、标题冲突或其他 capability 变化. `sync-preview.spec.md`、`sync-preview.diff` 保存在 B TEMP, 主规格未写入; 这是同步内容预览, 不冒称已经执行正式 sync. Markdown 本地链接/锚点核对为 1206 项、零失效, 三个 A/B 框均已勾选, `git diff --check` 通过, 结果在 `b-document-audit.json`.

验证问题分级: **CRITICAL 流程待办 1 项: 任务 5.4 尚未执行**, 后续责任会话按授权同步/归档并留存后重置问题单. **实现缺陷 0、WARNING 0、SUGGESTION 0**. 因该任务仍待办, 不将 change 记作全部任务完成或已归档; 它不撤销本次已完成的独立 B 验收和技术核对, 也不要求 A 虚构额外功能整改.

本轮适用的完整性/实现映射/场景/设计/风格检查均已执行. 覆盖限制为未跑 ASan、另一 OS/编译器/GPU、自然 OOM/设备丢失、历史完整资源矩阵、物理跨屏及 D03 绘制, 不属于本次修正新增能力或被宣称已覆盖的证据. **最终结论: 三项正式问题已关闭, B 验收和最终版本技术 verify 通过; 当前保留问题单, 等待任务 5.4, 不同步、不归档、不重置、不暂存或提交.**

<a id="review-loop-closeout"></a>
## 首次存量 Review/Annotation 闭环留存

本轮是开发中途采纳独立审查规范后的唯一一次全仓对齐. 范围为 D00-D02 已实现的 54 个 C++ 源/头、8 个 CMake 文件和相关规范/架构/来源/主规格, 无独立 Shader 源文件; 第三方只检查接入及固定输入, 不审计或重写其全部算法, 未实现的 Renderer/Scene/Editor/Pass/Vulkan 不计作完成. B 的 Review/Annotation 与续审补查涵盖 Core、Platform/输入、公开 RHI、Common/Validation、GL 资源/上传/完成/退休及 Runtime 消费链, 直接完善既有 36 文件的注释; Check/Annotation 又在两个私有头补充语义位置和非拥有指针 key 说明, 不修改功能或构建输入.

独立 B 会话为 `01a19cc6-b5af-74d0-88ac-c8f5f3423a86`. 基准 HEAD `1c897364167677b0df283afa187279d48a8f5c78`, 最终 62 文件清单 `919551495a6a8739bbbae0bf446c3ab4bd33244ff280f6bf4578a4b3fd136c3c`. B 已实际核对 A 交回版、全部四文件功能 diff 及共享消费者, 并对补充注释后的最终版本独立重建/运行. CR-001/002/003 全部由 B 复核关闭, 无新增正式问题、撤回或非阻塞建议, 追加 A 功能整改不适用. 当前源版本与 B 最终清单逐文件一致, 本次 A 收尾未改代码、注释或构建输入; 复用已匹配的 B 测试证据, 不将核对说成再次运行完整矩阵.

| 问题 | 触发与整改摘要 | A 自检 / B 复核 |
|---|---|---|
| CR-001 P1 | descriptor-set registerSpace 超限报错后仍访问固定映射数组; 修正为诊断后立即失败, 合法 set 与非 descriptor-set 语义保留. | Debug/Release 公共入口 0/7 正常委托, 8/16/UINT32_MAX 零委托且诊断可见; B02 独立重跑确认关闭. |
| CR-002 P2 | Pipeline 汇总在 StaticVector noexcept 元素操作中分配; 随逐失败点又发现 MSVC hash 内部 noexcept proxy. 使用普通数组、count=0 Shader vector 及私有有序集合, 保留完整位置身份、公开容器与 hash 合同. | 三种公共 Pipeline、0/1/3/8 layout 的 543/324 个分配点可捕获且零部分委托; BindingSet 29/19 点及集合差/数组身份通过, native 提交预检 5/3 点失败后同一列表可重试; B02/B03 关闭. |
| CR-003 P2 | 临时 vector.max_size 和 TextureState 默认 vector 构造可能在 Debug proxy OOM 时终止; 改为无分配长度查询、count=0 空表及私有 map, 保留 tracker 的 sticky-error/Reset 和提交元数据合同. | Debug/Release 首次/展开/Buffer 索引逐失败点、零元数据发布与 Reset 复用通过; 实际上传前缀的 lease/chunk 持有、orphan 真实完成及删除通过; B02-B04 关闭. |

下表从已验收问题单留存 B 的实际结论, 归档后问题单仅作下一轮可复用模板; 不以空模板证明本轮通过.

| 编号 | B 实际复核与结果 | 原始证据与限制 |
|---|---|---|
| B01 | 核对完整 A 源 diff, 62 文件/12 份交接检查文件的哈希, 以及消费者和 57 份受保护文档. A 仅四个 Common/Validation 文件有功能差异; B 两条注释的非注释 token 不变. | `b-input-manifest.json`、`b-final-source-manifest.json`、`b-comments.diff`; 未把 A 的摘要差异隐去或继承为旧证据. |
| B02 | 在新 TEMP binary 目录用 MSVC 19.44.35228、SDK 10.0.26100.0、CMake/Ninja Multi-Config 全新构建 Debug/Release 的正式 RHI、Platform、Core、产品及检查消费者, 两配置均成功且无警告. CpuChecks 全部通过, 分配点和边界结果与各 B 栏一致. | `Configure.cmd`、`BuildDebug.cmd`、`BuildRelease.cmd`、`configure.log`、`build-*.log`、`CpuChecks-*.log`、`b-check-runs.json`. 保留 /W4 /WX、Debug iterator 和 Release 优化; 当前构建没有 C1001, 未运行旧二进制. A 的独立 generic 消费者仅核对其源码/构建 stamp 和原始结果, 未冒称 B 又重跑. |
| B03 | 两配置 NativeChecks 在 Intel RaptorLake-S Mobile / GL 4.6.0 Build 32.0.101.6129 上通过: 首次/展开共 7/5 个故障点保持前缀和 lease/chunk; 提交预检 5/3 点零票据后同一 Closed 列表可重试; 真实完成、orphan 退休、native 删除和独立已知字节读回均通过, Host 正常关闭. | `NativeChecks-Debug.log`、`NativeChecks-Release.log`. 同步辅助读回不发布产品票据或退休存储; 未新增公共 readback/Shader/Draw 能力. |
| B04 | Debug/Release 补充旧合同检查通过: 错类型/null/slot/arrayElement/view 拒绝、Buffer 溢出/合法末区间、Texture mip/layer/sample 检查, 以及 Unknown 前态、非法跟踪范围、永久状态冲突、丢弃/Reset/新录制. 两配置 Runtime 各 7 项返回值全部符合 0/1/2 合同. | `fixture/SupplementaryChecks.cpp`、`BuildSupplementary*.cmd`、`SupplementaryChecks-*.log`、`runtime-*.log`、`b-supplementary-runs.json`. 补充检查调用实际 Common/Validation, 没有复制算法; Runtime 使用 B 新建产品 exe. |
| B05 | 核对四个功能修正文件的最终注释、公开/原生边界和来源差异, 文档职责保持一致; 本轮本地 Markdown 链接/锚点 1206 项零失效, A/B 三对框均已关闭, `git diff --check` 通过. | 最终 verify 的三需求/十二场景映射和官方 strict 9/9 结果留在 tasks, 本问题单在归档成功前保持当前内容. |

证据根目录为 B 的 `%TEMP%\Physara-AgentB-20261103-check` 及忽略的 `build/agent-b-review-20261103-check/evidence.zip` (SHA256 `13da265bc44466dd78ac1e5f395f45a985a4a415bcb29085ed28112ca05b3624`). 包含最终源清单、A 接收清单/diff、B 两条注释 diff、具名检查输入、命令和原始日志, 共 50 个条目, 不含生成树/二进制. A 收尾已核对 ZIP CRC、62 个当前文件、B 的构建/CPU/native/补充及 Runtime 返回值记录; Debug/Release 原始构建无警告, 不沿用 A 旧二进制来冒充 B 的新建产物. A 的独立 RHI-only generic 结果仍按原匹配输入证据保留, 未称 B 重建了该配置.

最终原生 verify 已在 B 验收后完成: 3/3 delta 要求、12/12 场景及五项设计决策有证据, 实现缺陷 0、WARNING 0、SUGGESTION 0. A 本次再次核对规划/源码/测试身份, strict 9/9 通过, 只剩自引用的管理任务 5.4. 当前范围仍未覆盖 ASan、另一 OS/编译器/GPU、自然 OOM/设备丢失、历史完整资源大矩阵、物理跨屏或 D03 绘制; native 结果限本机 Windows/Intel GL 4.6.0 Build 32.0.101.6129, 受控失败和辅助读回不扩张产品能力.

2026-11-03 维护者已选择同步主规格后归档并重置问题单. 已按官方 specs 指令快照同步 rhi-common-validation 的 2 MODIFIED + 1 ADDED, 合计 12 项要求/42 场景; delta 三个完整要求逐项匹配, 另外 9 项正文和其余 7 份主规格不变, strict specs 8/8 通过. B 的同步预览 diff 与实际语义合并一致, 没有删除/重命名要求, 不是复制 delta 标头到主规格.

归档入口仍为 12/13: 唯一未勾选的 5.4 包含归档本身及成功后的问题单重置, 不能在操作发生前伪称完成. 维护者已知悉并授权该收尾. 本节和开发计划先留存验收摘要, 问题单在官方归档成功前保留; 归档成功、模板重置和核验后才关闭此项并追加本次归档结果. 此收尾记录更新不改写 B 的原验收、技术设计、行为规格或既有历史归档.

<a id="archive-result"></a>
## 归档结果 (2026-11-03)

已按维护者选择同步 rhi-common-validation 主规格, 2 MODIFIED + 1 ADDED 的三个完整要求与 delta 一致, 主规格为 12 项要求/42 场景, 其余 9 项正文及另外 7 份主规格未变. 同步后官方 strict specs 8/8 通过. 官方 CLI 已归档至 `openspec/changes/archive/2026-11-03-fix-validation-cpu-boundaries/`, `.openspec.yaml`、proposal/design/delta 及本 tasks 均保留. 归档命令仅跳过已完成且核验一致的重复规格应用, 未跳过 archive 验证.

归档入口保留 12/13 的真实状态, 唯一待办 5.4 是归档本身和成功后的问题单重置; 维护者已明确确认继续. 在归档成功后, 本节之前的闭环留存与开发计划均已保存 B 验收、CR-001/002/003 修正/复核及证据摘要, 随后才将 CodeReview.md 恢复为其未启动 tpl, 最后关闭任务 5.4. 当前完成 13/13. 这是同一次归档收尾事务的实际结果记录, 不将 B 当时 12/13 的技术核对改称已提前完成, 不改写规格、技术取舍或先前归档.

本轮归档后源码/构建输入继续匹配 B 的 `919551495a6a8739bbbae0bf446c3ab4bd33244ff280f6bf4578a4b3fd136c3c` 清单; 没有新增功能/注释改动, 原生检查限已记录的当前环境. 最终 active list、strict、任务/元数据、主规格语义比较、文档链接、受保护历史、HEAD/暂存区和问题单模板一致性由本机 `build/validation-closeout/final-verification.json` 留存. 同目录保存授权、同步预览/差异、同步核对、官方归档输出、归档输入清单及本次管理收尾 diff. 没有执行 Git 暂存、提交或推送.
