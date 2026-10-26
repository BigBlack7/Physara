# Tasks

范围为 [D02-T02](../../../../Docs/Developments.md#d02-t02), 采用维护者已同意的限定推进方案, 方法见 [design](design.md), 验收合同见 [spec](specs/rhi-common-validation/spec.md). 本文件所有复选框均表示实际实现和指定验证共同完成. 先完成 1.1 的当前文档采纳, 再进行生产实现. 不勾选 D01 的跨屏验收或 D01/D02 总出口, 不提前实施 T03/T05 native 工作.

## 1. 阶段边界、来源与检查输入

- [x] 1.1 按 design 第 1 节将本次明确授权的 T02 限定前置、T02 通用层完成条件和 T03/T05 的真实 GL 包装/caps/资源归属/原生验收责任同步到 Developments, 在 Architecture 7.1/7.2/7.5/7.7 采纳有限适配并引用本 change. 验证计划中不再存在 T02 完成等待 T03、T03 前置 T02 的环, T03 仍受 D01 入口约束, 所有链接/锚点有效, 旧 change/归档和 D01 未完成事实不变; 此项先于相关代码.
- [x] 1.2 重核 design 的十个固定源输入及已有公共头来源, 在 RHI/README 扩充路径、LF SHA256、完整 MIT、命名/差异和预定定义归属; 在 build/artifacts/establish-rhi-common-validation 建立临时消费者/fixture 的可复核输入, 无生产假 Device. 验证十项 hash 匹配, 私有/native 排除清单符合实际 include 闭包, ThirdParty 未修改, 原有五头版本/数值/布局/绑定键差异仍受保护.

## 2. 通用定义与正式静态目标

- [x] 2.1 移植 Common/Misc.hpp、Misc.cpp、FormatInfo.cpp、Utils.cpp 的完整通用定义组, 同步 PhysaraRHI STATIC 的真实 sources、PUBLIC 模块 include/C++20 和 PRIVATE 第一方诊断, 保持 Runtime 链接边界; 随组更新 Build 和来源说明的当前可链接能力. 立即在临时正式消费者 Debug/Release 链接运行 VerifyHeaderVersion 真/假、全部格式 metadata/UNKNOWN、正常 range、Blend/Framebuffer/CoopVec 纯工具和所有 Utils 外部定义的适用检查; 不因缺少导出宏漏掉函数, 不先列尚未落地的 Validation cpp 或引入 native 依赖.
- [x] 2.2 按 design 第 4 节完成 safe subresource/IsEntireTexture 计算、合法 mip/私有算法前提、allocator 容量/索引算术与 RAII 条件锁, 更新相应头合同和来源差异. 在 Debug/Release 检查 whole sentinel、部分/越界交集、base/count 最大整数、0/剩余 buffer range、allocator 0/部分/满/释放复用、capacity>INT_MAX 的 length_error 且分配前拒绝、同步模式并发唯一性和重复循环; 不真的分配 INT_MAX 容量, 不执行违反 mip/helper 前提的 UB.

## 3. 抽象资源状态与错误退出

- [x] 3.1 移植 StateTracking.hpp/.cpp 的完整有效输入算法及依赖, 将其加入同一真实目标并记录 descRef/extension/callback 借用与单写者合同. 立即在 Debug/Release 检查 BeginTracking 只声明、整图/部分 mip/layer、batch buffer 用途、UAV 同态依赖与开关、keepInitialState 的恢复请求和 permanent/stateInitialized 的延迟发布; 消费抽象记录的 fixture 不发 native barrier, 不把调用 Submitted 当 GPU 完成.
- [x] 3.2 实现 design 第 9 节的 HasErrors/Reset、范围/索引/状态/分配失败检查和永久状态发布预检, 更新私有合同和有限差异. 在 Debug/Release 检查 Unknown 前态、越界/溢出/冲突请求置错, 错误或未消费依赖时禁止发布 metadata, Reset/取消/析构释放本次记录且不改 extension metadata, 同一 tracker 重复开始新 recording 无残留; 各个失败不通过 Submitted 伪清理, 不以全局 barrier/等待兜底.

## 4. 完整 Validation 与逐项边界检查

- [x] 4.1 移植 Validation.hpp、ValidationBackend.hpp 和两个完整 Validation cpp, 对照 RHI.hpp 核对所有 override/扩展/弃用重载, 实现工厂 null/API/callback 检查、CPU 拥有引用和诊断/分配失败边界, 更新来源与公共合同注释. 立即在 Debug/Release 验证第六个公共头独立首 include、包装工厂可链接、缺依赖返回空、Device/列表的 retain/最终释放、有效委托和底层空/false/0 失败原样传递、一次操作诊断含原因/源位置; double 的非被测路径 fail-fast, 不默认成功.
- [x] 4.2 按 design 第 6 节实现共同的 OpenGL API/profile 判定、OpenGL 字符串和全部入口 guard, 为引用 getter 声明并实现 Unsupported 后 logic_error 的唯一特殊失败形式, 不调用 assert helper 伪造拒绝. 随实现生成全 27 Feature 与全部 Device/CommandList 入口分类矩阵, 在 Debug/Release 逐族检查唯一允许 Feature 的真假委托、其余 false 且不改 output、format metadata 不等于支持、队列/deferred/heap/virtual/bindless/RT/Mesh/feedback/CoopVec 等创建/查询/void 路径零禁止转发, 包括 null 专有 payload; 核对 D3D11/Vulkan 关键来源规则和完整公开声明仍保留.
- [x] 4.3 按 design 第 7 节补齐允许路径的 Release null/type/enum/descriptor/usage/view/binding/range/copy/write/indirect/state 检查, 外部 Resource* 使用所有配置有效的动态类型检查, 算术先判界再计算; 同步文档的业务窄化前提、真实 limits 与未普遍包装资源的归属边界. 在 Debug/Release 用存活 probes 和底层调用计数覆盖合法输入一次转发与非法输入零转发, 含 offset/size 最大值、mip/layer 计数、错类型/空值、typed format、framebuffer 样本/层级、binding arrayElement/registerSpace/布局对应; 不读取 inactive union 或用 clamp 掩盖显式 copy 越界, 不宣称本层能查到未提供的 native limits/资源 Device.
- [x] 4.4 完成 design 第 5/8 节的 immediate claim 析构释放、底层列表先于包装设备释放和批次只读预检/统一消耗, 保持空 batch 的来源语义与底层返回. 在 Debug/Release 覆盖 Initial/Open/Closed 全转换、重复 open/close、Closed 未提交重开、第二 immediate 被拒后不破坏第一个、open 列表析构后可再开, 以及 null/foreign/raw/duplicate/wrong queue/未 Closed/过大 count 的整批拒绝; 特别验证“前项有效/后项无效”后修正重试仍成功, 预检分配失败不消耗 Closed, 底层提交 0 不伪造 ticket 或自动恢复重放.

## 5. 完整来源与构建集成回归

- [x] 5.1 对十个输入和现有公共头引用完成整份归一化 token/声明/默认值/overload/宏/override/enum/format 表核对, 允许差异逐项对应 design/README; 独立于映射表复核 PascalCase 和所有 override, 含前一 change 漏过的四个命令名. 保存完整 map、源/目标 hash 和原始输出; 在临时副本改输入、删声明/override、改回一个旧方法名均须拒绝, 实际参考不动, 不以同一映射自证或数量相等代替完整对应.
- [x] 5.2 从新 binary 目录离线配置只链接正式 PhysaraRHI 的独立消费者, 无手写 RHI include、PCH 或业务/native 依赖. 构建并运行全部适用 Debug/Release CPU 用例, 六个公共头和三个私有头独立首 include, 检查实际 STATIC/.lib、sources/include/links/flags 元数据和所有 Common 定义链接. 已实现 Common 的旧“缺定义”负例改为当前正向链接检查, 旧记录保留; 旧接口/旧方法名/错误 handle 转换的负例继续按预期拒绝, 没有新增产品入口或传递的全局警告配置.
- [x] 5.3 从另一个新 binary 目录离线构建全部现有产品 Debug/Release, 实际执行 Physara --help、--unknown、--hidden --poll-limit=3 并重跑正常宿主退出, 记录每配置精确 0/2/0 和真实工具链/SDK/命令/原始输出. 核对新库和关键消费者无新增警告、参考/native include/link 或反向依赖泄漏, Runtime 未增加无用 RHI 链接. 若本机不能运行真实 host, 如实保留该检查未完成; CPU double 不替代现有产品回归, 更不证明新的 GL Device.

## 6. 原生 verify 与工作包交付

- [x] 6.1 对照全部要求/场景执行原生 openspec-verify-change 和 Code Review, 核对设计第 1-10 节及每个受控差异、所有权/线程/异常、Release 失败路径、GL policy、状态取消/提交区分和来源谱系; strict、受影响 Markdown 链接/锚点、许可/格式及 git diff --check 均须通过. 将实际证据按下方约定补入本文件, 清理重复/失效配置与文档, 仅在适用 CPU/build/host 检查通过后标记 D02-T02; T03/T05 native 责任、D01 跨屏及两阶段总出口继续未完成, 不执行提交、主规格同步或归档.

## 归档前修订说明

归档前命名审查是维护者在初次 14/14 apply 后明确追加的修订. 初次证据保留为当时事实, 当前完成状态还须满足下列任务; 匿名域的旧说明由当前 AGENTS/design 第 3 节取代.

## 7. 归档前的统一风格审查

- [x] 7.1 按维护者本轮要求先修订 AGENTS、Architecture、README、proposal/design/spec/tasks: 第一方域采用 PascalCase, RT 等缩写统一, std/第三方协议保留; cpp 私有作用域不依赖匿名域或机械新建空泛域. 核对新增域拼写场景与来源映射, 本阶段 GPU/D01 边界及旧历史不变.
- [x] 7.2 保存当前源码/内容标识并统一 RHI 全部域声明、引用与 std hash 特化; 审查 RHI/Core/Platform/Runtime 的全部匿名域并用所属模块域和 static 保持私有函数/对象链接, main 保持语言入口. 核对完整 token/声明/默认值/布局/所有权与初始化/销毁顺序, 无旧域 alias、新空泛域或无关函数体改写.
- [x] 7.3 从新目录重建独立消费者及全部产品 Debug/Release, 重跑全部适用 RHI CPU/首 include/旧接口负例及新域正反例; 同时回归受影响 Core/输入/宿主故障退出和真实产品 help/unknown/hidden 0/2/0. 独立于映射表检查所有第一方域/匿名域和 private helper 链接, 保存负变异、工具链、命令、hash 和原始输出; 不用旧结果代替本轮.
- [x] 7.4 按最终 11 要求/37 场景和 design 1-10 再做原生 verify、Code Review、完整来源与风格/许可/链接/strict 检查, 保留旧 apply 证据并追加本轮结论. 完成本阶段归档候选后才按 archive 技能的同步选择核对主规格并移动; 不提交 Git, 不重写旧 change/归档或关闭 D01/D02 总出口.

## 要求与任务对应

| 要求 | 主要任务 | 核心观察 |
|---|---|---|
| Consume Physara domain namespace spellings | 7.1-7.4 | 统一域正向消费者、旧域拒绝、布局/hash/原有行为回归 |
| Link the generic behavior behind the public contract | 2.1, 2.2, 4.1, 5.2 | 实际链接、版本真/假、metadata/范围/工具结果 |
| Allocate indices within a representable fixed capacity | 2.2 | 容量/耗尽/复用、分配前拒绝、并发唯一性 |
| Compose validation with explicit CPU ownership | 4.1, 4.4 | 空依赖、强引用/借用、底层失败原样传递 |
| Reject unsafe inputs before device work | 4.3 | Release 正反用例、无溢出/错型/错误底层调用 |
| Distinguish API policy from underlying capabilities | 4.2 | OpenGL 身份、27 Feature、允许查询/format flags |
| Reject operations outside the initial OpenGL policy | 4.2 | 所有入口分类与族拒绝, 无成功空操作/假引用 |
| Enforce command-list ownership and lifecycle | 4.4 | 状态/归属、open claim、坏批次不部分消耗 |
| Produce abstract resource-state dependencies | 3.1 | begin、whole/subresource、同态 UAV、延迟 metadata |
| Expose invalid tracking and discard abandoned recordings | 3.2 | sticky error、Reset/取消、失败不发布 |
| Deliver a traceable generic implementation without native claims | 1.1, 1.2, 5.1-5.3, 6.1 | 固定源/完整映射、正式隔离构建、native 证据边界 |

## 实施证据约定

每个实现任务完成时在本文件追加实际命令、配置、工具链/SDK、fixture 源和原始输出、源/目标内容标识、返回码及未覆盖范围. 临时文件统一位于 build/artifacts/establish-rhi-common-validation/, 不新增验证报告文件、测试框架或 CI. 映射/编译/CPU/现有产品 native smoke 分别记录, 不用旧 change 的结果替代新实现检查, 不把 planning complete 当实现完成.

## 规划核对记录 (2026-10-26)

本轮仅创建原生 change 的 proposal、specs、design、tasks. 已读取三个当前主规格的全文、当前 Architecture/Developments/Build、公共头/来源说明以及十个固定源, 并计算 design 所列 LF SHA256. 维护者已明确同意限定前置和 T02/T03/T05 验收职责划分; 当前文档的正式采纳是实施任务 1.1, 尚未修改生产或现有权威文档.

Common/Validation 生产实现、编译/链接、CPU 测试、产品 host 回归和实现 verify 均未运行, 上述任务全部未勾选. 本轮原生状态/strict/链接与规划一致性检查的实际结果在执行后补于本段, 不计为实现证据.

实际规划检查已通过: 原生 status 为 spec-driven 四类产物齐备, instructions apply 为 ready、0/14 完成, 无不可读任务文件. `openspec validate --all --strict --json` 为 5/5, 本 change 无提示; 既有 native-window-host 保留两条长要求 INFO. 四份 Markdown 的 15 个本地链接/锚点、空白/冲突标记及 git diff --check 通过, 新文件另做内容检查. 十项固定输入 hash 与 design 表一致; spec 的 10 项 ADDED 要求、35 个 WHEN/THEN 场景和 14 个带验证方法的未完成任务对应完整.

本机复核命令为 `python build/artifacts/establish-rhi-common-validation/PlanningCheck.py`, 原始结构化结果为该目录的 planning-inputs.json / planning-check.json. Git 检查确认所有已有 tracked 源、权威文档、主规格、旧 change/归档未修改, 当前新增仅本 change. 这些结论只证明规划结构/来源和内容核对, 不表示未来实现或 native 验收已通过.

## 实施证据 (2026-10-26)

1.1: 在任何生产实现前更新 Developments 的限定前置、T02 通用完成条件及 T03/T05 原生责任, Architecture 7.1/7.2/7.5/7.7 采纳有限差异. 人工对照确认 T02 不再等待 T03, T03 仍要求 D01 出口. 使用 Python 3.12.9 运行 `build/artifacts/establish-rhi-common-validation/CheckDocs.py`, exit=0; docs-check.json 记录 8 份 Markdown、958 个本地链接/锚点和十项源 hash 通过, git diff --check 通过. ThirdParty、Runtime/Platform/Engine、当前主规格、旧活动 change/归档未变. 尚无本 change 的代码/CPU/native 验收证据.

1.2: 十个输入及 include 闭包记录于 inputs.json, 与 design LF SHA256 全部匹配; 既有五头使用原 Port.py --audit 全量 token/许可检查通过, exit=0. RHI/README 增补十项路径/hash、定义归属、命名与 native 排除项, 检查脚本/消费者输入留在 ignored build/artifacts, 无生产 double. CheckDocs.py 8 份文档、959 个链接通过; 第三方及旧规格/历史未变.

2.1: Misc.hpp 及 Misc/FormatInfo/Utils 三份完整 cpp 接入同名 PhysaraRHI STATIC, /W4 /WX /utf-8 /permissive- 私有, Runtime 未改. Python RunCheck.py CommonChecks 通过正式消费者 Debug/Release 编译、首 include 与运行, 各 exit=0 且无警告; 日志 CommonChecks-*-build/run.log, fixture CommonChecks.cpp + 固定源提取 FormatReference.inc. 覆盖版本真/假、每项格式所有语义字段及 UNKNOWN、正常范围、Blend/Framebuffer/CoopVec/纯 Utils 和其余外部定义地址链接. 工具链 CMake 3.30.0-rc4、VS2022/MSVC 19.44.35228、SDK 10.0.26100.0, 无原生后端执行. Build/Architecture/README 当前能力已同步.

2.2: Common 范围/IsEntireTexture 改为先判 base 再求剩余, 头合同说明合法 mip/Align 前提, uint32 mask, matrix stride 检查乘法; allocator 分配前 length_error、size_t 循环及 RAII 条件锁完成. RunCheck.py RangeChecks Debug/Release 全部 exit=0、无警告, fixture RangeChecks.cpp 和 RangeChecks-*-build/run.log; 包含 UINT32/64 最大值、零长度/剩余/越界/whole、两种锁模式各千轮耗尽复用、拦截大分配确认 INT_MAX+1 未申请存储、每配置 8 线程 32000 次活索引唯一性. 未执行非法 mip/私有前提 UB, 未实际分配巨量容量; 差异已登记 README.

3.1: StateTracking.hpp/.cpp 完整有效输入算法已加入正式目标, 头中说明单写者与 descRef/extension/callback 借用、抽象依赖和提交 metadata 的边界. RunCheck.py TrackerChecks Debug/Release 构建/运行全部 exit=0、无警告, TrackerChecks.cpp 与 TrackerChecks-*-build/run.log 覆盖 Begin 不发依赖、整图/单 mip-layer、不相关子资源、buffer 批次用途合并、两类资源同态 UAV 与关闭开关、keep initial 恢复及 permanent/stateInitialized 延迟发布. fixture 仅消费抽象记录, 未发射原生 barrier, 未证明 GPU 完成.

3.2: HasErrors/Reset、前态/描述/range/state/size_t 索引检查、待永久状态冲突和分配失败处理已实现; Submitted 先检查无错误/依赖消费并整体预检, 再无分配发布所有 metadata, Reset 释放工作存储. RunCheck.py TrackerChecks 两配置 exit=0、无警告; 新日志覆盖 Unknown 整图/部分与 buffer、最大 base/越界 index、非法 bits/null/mip、pending 冲突/子集 permanent、未消费依赖、提交前 metadata 冲突不部分发布、真实 new 故障注入、100 次无残留 Reset/重用及析构放弃. 同时重跑原有效算法用例. 来源差异/borrow/非 GPU 边界见 README, 未做原生 barrier/完成测试.

4.1: Validation.hpp、私有 backend 头及两份完整实现接入同一 STATIC, 68 个 Device/61 个 CommandList 纯虚方法与继承资源方法均由编译 override 核对; 保留扩展/弃用重载. 工厂 null/API/callback、bad_alloc 与 source_location 诊断、CPU intrusive owner/列表析构顺序已实现. MakeStubs.py 从真实公共接口生成完整 fail-fast stubs, 仅被测路径在 ValidationChecks.cpp 配置计数/结果. RunCheck.py ValidationChecks Debug/Release 构建/第六公共头和私有头首 include/运行 exit=0、无警告; 缺依赖、未知 API、工厂和列表包装分配故障、底层 null/false/0、父引用释放后列表保活及最终恰好销毁均通过. 详见 ValidationChecks-*-build/run.log. GL 限制策略与输入/批次补强仍待后续任务, 未声称 native 支持.

4.2: 共用 profile 已实现完整 Feature/queue/排除族和描述字段检查, GL 字符串、query output 保留及 crash-helper logic_error 合同完成; 所有声明/非 GL 分支保留. policy-matrix.json 覆盖 68 Device、61 CommandList、4 项继承/便捷入口和 27 Feature. RunCheck.py ValidationChecks 两配置 exit=0、无警告; 独立手写逐族用例覆盖所有禁止 create/query/mutation/void、null 专有 payload、已有 pipeline/bindings 旁路, fail-fast stubs 确保未触底层. 唯一允许 feature 的 true/false 与 output 委托、其余查询零委托/不改 output、metadata 与实际 format flags 区分、GL logical spaces 及 D3D11/Vulkan 关键来源规则通过. 日志 ValidationChecks-*-run.log, 无任何 native caps/GL work 证明.

4.3: 共用描述/范围/state 与绑定 validator 已完成, 外部 Resource* 所有配置动态类型检查, explicit copy/write/heap 先判界, texture shift/pitch/format、binding active payload/arrays/layout、framebuffer 样本/层级和 state/indirect/index 检查均在操作前执行; 诊断区分 InvalidArgument/Unsupported. RunCheck.py ValidationChecks Debug/Release 构建/运行 exit=0、无警告; input/state 两组覆盖合法一次委托和非法零操作计数, null/活错类型、UINT32/64 最大 offset/size/mip/pitch、typed views、arrayElement、独立 Texture/Sampler 槽、逻辑空间、sample/layer/mip、枚举和间接记录边界. CPU fixtures 与最新 ValidationChecks-*-build/run.log 留存. 已确认并登记 sampler-feedback 自递归局部修正, D3D12 身份 double 精确一次委托/同参数/空结果回归通过, GL 拒绝不变. README/头注释明确 builder 窄化和实际 native limits/owner/staging 状态边界; 未证明任何新 GPU 语义.

4.4: 列表 immediate claim CAS/关闭与析构释放、底层列表先于父包装销毁、Open/Close 全 flags 复位及整批只读预检后统一消耗已完成. RunCheck.py ValidationChecks 两配置 exit=0、无警告; LifecycleAndBatches 覆盖全部转换/重复操作、第二 immediate 不破坏首项、open 弃用后可再开、空 batch、null/foreign/raw/duplicate/wrong queue/未 Closed/UINT32_MAX+1、后项失败后修正重试、MSVC Debug 容器构造自身的分配故障、底层 0 消耗且不重放. ListProbe 借用原始 Device 而非替它保活, 检查包装真实父子释放顺序及最终恰好销毁; 详见 ValidationChecks-*-run.log. GPU rollback/完成仍未实现或宣称.

5.1: SourceAudit.py prepare/check 完成十输入和既有五头的整份 token/声明/默认值/overload/宏/override/enum/完整格式表对应, exit=0. source-audit 保存 complete-symbol-map.json、manifest.json、complete-inventory.json、完整 controlled-adaptations.diff、159 个 hunk 的 design/原因索引和逐文件复核副本; 原来源声明均保留, 受控签名变化逐项归一复核. 独立首字母命名规则核对 177 项公共 virtual 及全部私有声明, 不用动词/映射表自证. 十项输入漂移、实际删除声明、移除 override、Draw/Dispatch/ResolveTexture/CommitBarriers 旧名临时副本均被拒绝; audit-result.json 留存结果, 原参考未改. 最终审查补测发现并修正 MSVC Debug Reset 空容器 swap 的分配; optional map 销毁和空 vector 释放使 Reset/Submitted 清理无分配, TrackerChecks 两配置通过, 失败原始输出 reset-allocation-before-fix.log 保留. 对应 design 9/README 先同步, 无 native 结论.

5.2: ConsumerFinal.py 在此前不存在的 consumer-fresh-final 离线配置, 正式 PhysaraRHI STATIC/.lib 的 Debug/Release 构建无警告; 11 个可执行 CPU fixtures 每配置各 exit=0, 共 22 次运行, 含本次四组和前一公共合同六组, 旧 MissingCommon.cpp 当前作为 CommonNowLinked 正向链接运行. 六公共/三私有头各自首 include 通过; WrongHandle/OldInterface/OldMethods 六次编译均按预期 exit=1, 四旧命令名逐项出现拒绝诊断. consumer-final-*-build/run/negative.log 与 consumer-final-result.json 保存命令/结果/当前源 LF hash. File API 和 target 属性核对 6 cpp+9 header、唯一模块 include、C++20、私有 /W4 /WX /utf-8 /permissive-、无接口 options/links/native/PCH; 仅有 CMake 生成的 ZERO_CHECK 构建依赖. 工具链同 2.1, 未覆盖其他平台/标准库, 旧历史输出未改.

5.3: ProductFinal.py 从此前不存在的独立 product-fresh-final 离线配置并构建全部现有产品及新 RHI 静态库, Debug/Release 全部 exit=0、无新增警告. 实际 Physara --help/--unknown/--hidden --poll-limit=3 每配置精确 0/2/0, 隐藏宿主每配置重复三次均正常退出; product-final-*-build.log、help/invalid/hidden 原始输出及 product-final-result.json 留存. VS2022 MSVC 19.44.35228、SDK 10.0.26100.0、CMake 3.30.0-rc4; File API 确认唯一产品 executable=Physara, RHI 无业务/native/参考依赖, Runtime 无 RHI include/link, 第一方诊断未传播给第三方. 这是既有真实窗口宿主回归, 未证明新的 GL Device/caps/barrier/资源归属或 D01 跨屏.

6.1: 按已安装 openspec-verify-change 原生流程重读 status、instructions apply 的四类 contextFiles, 完成 10 项 ADDED 要求、35 场景及 design 1-10 的实现/fixture/实际日志对应与 Code Review; VerifyFinal.py 的 verify-final.json 记录三维检查和定位, 无遗留 critical/warning/suggestion. 审查核对 CPU owner/borrow、callback/单写者/标准异常合同、真实底层失败、GL 全族拒绝、批次预检和 tracker metadata/取消分工, 无多余 caps cache/地址 registry/第二 tracker. 修正并回归 sampler-feedback 一次委托、MSVC Debug 无分配取消/提交清理、两对来源 stencil SRV; 澄清 ShaderLibrary 普通阶段能力, 输入/诊断 helper 共享且 cpp 私有算法置匿名域. 最终 ConsumerFinal.py --resume 和 ProductFinal.py --resume 两配置均 exit=0、无编译警告, 22 CPU 运行/6 预期编译拒绝及现有真实 host 0/2/0 和重复退出再次通过; initial-build 日志保留首次全新构建. SourceAudit.py check 全量 token/签名/默认值/overload/enum/format/完整 MIT、独立命名与负变异通过. CheckDocs.py 为 8 份 Markdown、961 本地链接/锚点, git diff --check 通过; 十项输入及 ThirdParty/Runtime/Platform/Engine/主规格/历史均未变. OpenSpec 1.13.2 strict 5/5, 本 change 零提示, 仅 native-window-host 既有两条长要求 INFO; 原始 verify-status/apply/strict JSON 和 stderr 留存. 清理规划时态与过期当前状态, 仅标记 D02-T02 通用工作包. 平台仍仅 Windows x64/MSVC; 未验证新的 RHI native Device/caps/view/barrier/完成或全部资源归属, 对应 T03/T05 和 D01 跨屏、D01/D02 总出口保持待完成. 未执行提交、主规格同步或归档。

7.1: 维护者本轮明确要求已采纳到 AGENTS、Architecture、README 和本 change 的 proposal/design/spec/tasks. 对照原生 status 的四类现有产物修订, 新增统一域拼写要求及两场景, 共 11 要求/37 场景, 归档前任务共 18 项. std/第三方协议、旧归档、D01/T03/T05 前置和 GPU 证据边界不变. 初次匿名域/保留小写域的证据保留为当时事实, 当前规则明确取代; 33 份第一方头/源的修订前 LF 内容和 16 个匿名域清单已存 archive-review/before 与 before.json.

7.2: archive-review/StyleMigration.py 已在 33 份第一方头/源的不可覆盖 before 快照上完成域和链接属性修订, 23 份文件有改动, 全部 16 个匿名域去除. Utils/RT/Cluster/CoopVec/Validation/Input/Profile 同步公开/私有声明、调用和 std 特化, 无旧域 alias; Core/Platform/Runtime 复用真实模块域, 私有自由函数/对象保持 static, main 留在全局. StyleAudit.py 独立检查全体域拼写/匿名域/内部函数清单, 对 33 份文件比较仅允许域和 namespace 层链接属性变化的完整 token, 函数体/参数/字段/默认值不变. 原五公共输入全 token 和十项 Common/Validation 源 hash/完整 MIT/既有适配仍对应. style-diff.patch、style-audit.json、migration.json 留存; 旧域、匿名域、调用点/参数误加 static 的副本均被拒绝. 初次转换的两类误加 static 已由 /WX 编译拦截并修正, 原输出保留, 未勾选构建任务前继续重验.

7.3: 本轮 archive-review 使用全新 consumer-reviewed、product-reviewed、native-fresh-review 二进制目录, VS2022/MSVC 19.44.35228 + SDK 10.0.26100.0 + CMake 3.30.0-rc4 离线构建. Debug/Release 正式 STATIC 消费者 24 次执行均 exit=0, 六公共/三私有头各自首 include 通过; 旧接口/方法/handle 及七个旧域共 20 次编译按预期 exit=1. 原有字段/enum/布局/hash、Common/allocator、Validation profile/输入/生命周期、tracker 错误/Reset/提交和新域正反用例全部重跑. 全产品两配置构建无警告, 实际宿主 help/unknown/hidden 精确 0/2/0, hidden 每配置三次正常退出. 受影响 Core/Platform 检查的 CoreChecks/公开消费者/InputChecks/NativeChecks/WindowInputChecks 两配置通过, 每配置原生 32 次生命周期与 14 个部分设置失败/重建通过; 9 个分配检查和 8 类实际 Runtime 失败/关闭检查每配置通过, RuntimeProcessChecks 的真实可见窗口 resize/iconify/close 正常退出. 本机 Intel GL 4.6、单显示器 144 DPI, CROSS_DPI_VERIFIED=0 明确保留. dumpbin 实际 Debug lib 符号确认私有 helper 为 Static 且不在公开 archive 索引, Validation 新域工厂真实可链接. 详见 consumer/product/native-final-result.json、symbol-review.json 与各原始 build/run/negative/symbol 日志, 旧 apply 输出和初次转换失败输出均保留.

7.4: 本轮按 openspec-verify-change 原生流程读取 status/apply 的全部四类产物, 对最终 11 要求/37 场景和设计 1-10 做完整性、正确性、一致性及 Code Review. archive-review/verify-final.json 保存代码/fixture 定位, 本轮 consumer/product/native/symbol/style 原始证据可复核, 无未解决 critical/warning/suggestion. 重点核对统一域对全部接口/RT/Cluster/CoopVec/std hash 的传播、static 仅作用于声明、callback 地址/实际内部链接、原状态/借用/错误/析构顺序、无旧名 alias/无新增空泛域和原生边界. StyleAudit 对 33 文件完整 token 和固定 15 源输入/许可通过, 16 个匿名域均移除, 旧域/匿名域/错误 static 位置负变异均拒绝; dumpbin 实证为 Static. Debug/Release 全部受影响构建和正反/原生回归通过, 当前唯一跨屏缺口仍为 CROSS_DPI_VERIFIED=0, 不记为本次已验证. 9 份 Markdown 的 970 本地链接/锚点及 git diff --check 通过, strict 5/5、本 change 无提示; native-window-host 原有两条长要求 INFO 保留. 当前档案、旧活动 change 与三个已有主规格未改. 归档候选已就绪; 同步仅创建本 capability 主规格, 仍需 archive 流程规定的明确同步选择, 后续保留 .openspec.yaml 并移动当前 change, 不提交 Git 或关闭 D01/D02 总出口.

## 归档记录 (2026-10-26)

维护者明确选择同步主规格后归档. 已复用本轮唯一 specs 指令快照, 创建 rhi-common-validation 主规格并逐项比较全部 11 项要求/37 个场景及 Purpose, 既有三个主规格保持不变; specs strict 通过. 原生 status 四类产物均 done, list 为 18/18. 本目录已移动为 2026-10-26-establish-rhi-common-validation, .openspec.yaml 原样保留; 已调整本归档内部和当前文档的相对链接. 代码/设计审查和本轮 Debug/Release 消费者、产品及 Core/Platform/Runtime 回归见 7.2-7.4, 完整后验输出保存在 ignored build/artifacts/establish-rhi-common-validation/archive-review/. D01 跨屏及 D01/D02 总出口继续待完成, 未提交 Git.
