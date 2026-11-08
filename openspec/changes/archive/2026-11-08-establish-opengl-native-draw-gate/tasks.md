# Tasks

## 1. A: 本地消费者与完成后观察

- [x] 1.1 在 `build/d03-t05/fixture/` 创建最小显式 CMake/JSON/GLSL/C++ 消费者, 使用正式接口/包生成 target、生成数据/绑定及 `LoadRhiProgram`; 构建可验证预期 ABI、实际包身份和依赖顺序, 正式源码及根默认构建无测试输入/入口.
- [x] 1.2 在同一消费者接通 Close/非零 Execute → Set Event → 有界等待/Poll 确认 → 冷路径 DSA 读回, 预置并恢复实际 pack 状态; 用本包公开 clear 的已知值核对初始观察, 诊断/完成/读取失败均不能返回通过, 原生 helper 不绘制或补同步. 将命令、结果与例外范围留在下方实施证据.

## 2. A: 固定图案与独立数值预期

- [x] 2.1 实现 design 决策2的四角、实例记录和 R16 索引前缀, 固定非零 binding offset/firstIndex/baseVertex/firstInstance; 真实 DrawIndexed 后按四角、未选记录标记及 builtin 预期自动判定, 证明忽略/重复偏移不能误过, 留存本轮输入和观测.
- [x] 2.2 在同一图案实现深度梯度、L形、CCW/负缩放和 scissor 区域, 以预定内部点、clear 对照及 D32 数值验收; 保持 UPPER_LEFT/ZERO_TO_ONE/Reverse-Z, 不重做已归档的驱动区域 clear 调查, 记录任何实际偏差及定位结论.
- [x] 2.3 接入16×16 sRGB写入/正式采样回浮点的同一消费者路径, 按固定字节与分域容差验收非饱和值, 结果能区分漏/重复转换; 不从实际输出反算预期, 在实施证据注明量化解释.

## 3. A: 跨目标、copy 与采样集成

- [x] 3.1 使用同一准备好的主图 Pipeline 绘制 A(64×48)/B(96×72), 按各自动态区域和独立像素中心预期验收两个附件, 确认无旧目标/尺寸残留; 复用第2组图案及观察 helper, 留存对应目标结果.
- [x] 3.2 按 design 决策3经公共区域 copy 生成带哨兵的 C, 再以公共采样 Draw 输出 D; 完成后检查复制点、L形及未覆盖区, 确认行序/版本/访问用途, 不原生 copy 或引入 feedback, 记录这条组合消费链的结果.

## 4. A: 代表性拒绝与可重复集成证据

- [x] 4.1 用共享输入的两个独立 recording/目标验证紧邻 IB 越界与重叠 feedback 拒绝, 在 Release/Debug 均观察相关 Error 及目标未被非法操作改变; 保留合法前缀的正常完成语义, 不为每种参数/失败点增加一例, 记录拒绝范围.
- [x] 4.2 对匹配当前输入的 Release 执行完整共享链并在独立进程再运行一次正向链, Debug 执行同一参考链和第4.1项; 复用最终输入下已完成的同一运行, 留存输入/包/配置/exe/设备身份、命令、每次自动判定、退出及实际失败, 不以旧 exe 或结构检查充当新图像证据.
- [x] 4.3 核对本轮正式 diff、默认 targets 与依赖闭包, 按实际输入等价性复用或运行必要的正式构建/直接消费者; 发现生产缺陷时仅修原 owner 并验证其独立风险, 无缺陷则注明无需生产修复. 同步必要的 Build/RHI 使用说明, 确认测试均在 build、普通工程可独立重建, 不运行无关完整矩阵.

## 5. 独立 A/B 审查与注释闭环

- [x] 5.1 A: 将实际 commit、相关 dirty/untracked 正式输入、可用本地复现材料及下方证据/限制交接至 [CodeReview](../../../../Docs/CodeReview.md), 开发计划标记待独立审查; 核对每个 delta 场景的证据并停止代码编辑, B 项保持未勾选, 由维护者调度另一会话.
- [x] 5.2 B: 独立核对本轮正式 diff、spec/架构和直接消费者, 审查实际涉及的完整第一方文件并完善职责/接口/流程注释; 在问题单写稳定 CR 或无发现结论、覆盖及代码/注释适用范围, 不把 build 测试材料列作正式代码审查范围, 只由 B 勾选.
- [x] 5.3 A: 按 CR 纠正并对实际改动做最小自检, 保留 B 的有效注释, 将修正/验证和最终输入写回问题单及本 tasks; 无正式问题则说明不适用及核对依据, 不自签 B 最终验收.
- [x] 5.4 B: 复核最终修正、证据身份和完整受影响注释, 分别确认代码正确性与注释可读性并签署最终版本; 未通过保留 CR 编号及待验收状态, 仅由 B 勾选, 无正式代码改动时如实注明适用范围.

## 6. A: 最终验证与归档收尾

- [x] 6.1 B 最终验收后按原生 verify 技能核对 proposal/design/全部 delta 场景及最终输入, 完成严格校验和文档链接检查; 在本 tasks 保留实际运行/复用/未覆盖范围、B 接受版本与结论, 未解决的必要项不计通过.
- [x] 6.2 通过最终 verify 后按原生流程同步 `opengl-immediate-draw` 的本次增量并核对既有九条合同完整, 严格校验主规范; 更新 T05 与 D03 出口实际验收状态及 change 引用, 不提前实施 D04 或改写历史归档.
- [x] 6.3 本轮关联范围均归档成功后核对归档内容、状态/链接和验收留存, 将计划引用切换为实际归档路径, 再按模板重置 CodeReview; 归档或留存失败保留现场, 不以重置问题单代替验收.

## 验证范围与停止条件

规划资料按 L0 核对内容、依赖、链接和 OpenSpec 结构; 实施验收涉及真实 GPU 执行/完成与读回, 按下表最小预算执行, 实际结果见下方实施证据. 场景共用一个消费者和输入, 不按函数、字段或每次任务重复运行相同检查; 本地结果不构成 B 或最终验收.

| 改变的合同 / 具体风险 | 最小充分证据 | 任务与停止条件 |
|---|---|---|
| 端到端参考输出: 编译/绑定正确但实际几何或 builtin 错位 | 一个四角/偏移参考, 生成 ABI/真实包与实例/顶点标记 | 1.1、2.1; 每种偏移错误均会改变已选观测即可, 不穷举偏移值 |
| 坐标/深度/颜色: oracle 被翻转或数值域错误污染 | 同图固定内部点、深度梯度及一个 sRGB 编码/采样对照 | 2.2、2.3; 预定坐标与容差全部满足后停止, 不重复 T03 格式/状态枚举 |
| 后继消费: 目标切换、copy 或采样读到旧对象/版本 | A/B 同 Pipeline, A→C 区域copy/哨兵→D采样 | 3.1、3.2; 各实际输出及未覆盖区可区分错误后停止 |
| 真实完成与重复性: 零快照或旧 exe 伪成功 | 非零提交后 Event 完成、冷路径读回, Release 两个独立进程、Debug一个 | 1.2、4.2; 三次匹配输入的运行满足同一预期即停止, 不做长期压力或性能测量 |
| 拒绝与工程边界: 非法请求发到 GPU、测试侵入正式构建 | 两个非法代表输入, 正式 diff/target 核对及匹配的构建证据 | 4.1、4.3; 不新增 OOM/自然loss/查询预算扫描, 未触及的既有证据按身份复用 |
| CR-001: 绑定 owner 错误拒绝公共剩余范围, 修正时可能误 clamp 越界 | 独立修正 change 的有限 CB/SSBO 对照与紧邻越界, 加当前输入下原图案 | 5.3及关联修正的1.1–1.3; 不重复历史故障矩阵, 当前结果匹配且边界不放宽后停止 |

新增检查只针对当前门槛的独立缺口. 若第4.3项实际修复生产行为, 在本表补该缺陷的风险和最小回归, 输入变化后重建受影响消费者并交 B 复核. 所需真实 context/设备缺失时记录原因并保持相关项未完成, 不用 CPU 替身或旧图代替.

## 既有证据的复用范围

- [T03 验收](../2026-11-07-establish-opengl-immediate-draw/tasks.md#final-verification): R16/R32、显式/Automatic 用途、逐项状态/颜色和历史 clear 定位. 本次只引用未改变的独立合同/归档结论, 不重跑整个单项矩阵, 不替代 T05 新图案.
- [T04 验收](../2026-11-08-establish-opengl-submission-retirement/tasks.md#final-verification): 查询、真实提交/引用及退出. 本次实际15文件摘要与最终 B 接受字节一致, 其余正式树也无 diff; 不复用旧 exe 运行, T05 及正式 Release 构建均有本次新证据.
- 规划时上述链接仅是复用候选; 当前已完成下方 T05 A 本地运行, 尚无 B 的本轮审查结论.

## 规划校验记录

2026-11-08, 基准为 `v2.0` / `5011b95bd8dbdb6a34596f4b3cca9054bf913a16`. 原生 spec-driven 依赖为 proposal → specs/design → tasks, 四类产物齐备. `openspec validate establish-opengl-native-draw-gate --strict --json` 通过, 无 issue; 本轮6份 Markdown 的152处本地文件/锚点引用通过, delta 为3条新增 requirement/7个 scenario, 17项任务均未勾选. 改动仅为本 change 的5个规划文件和两份开发计划的状态/引用, 正式源码、Architecture、主规范及空问题单未改. 本轮未构建、未运行 GPU 检查, 也未执行实现后的最终 verify 或独立 B 验收; 文档准备完成不代表 D03 出口通过.

<a id="implementation-evidence"></a>
## 实施证据

以下为 B 首轮前的 A 交接历史, 包括原联合输入 cdbd... 和当时无正式代码改动的事实; 不代表后续 B 注释/CR-001 整改后的当前原始字节. 当前版本见下方[整改证据](#correction-evidence).

2026-11-08: A 已开始实施, 所有本地材料仅在 `build/d03-t05/`. 使用 `python build/d03-t05/Evidence.py configure` 及 `build Release`, 当前 Release 消费者构建通过. 实际 MSVC 为19.44.35228, Windows SDK 为10.0.26100.0. 生成 Parameters=64 bytes、Instance stride=32、固定7条记录, ABI=`21659c40a9a6b899241369ee6fe1be07b4fc0a65b8287759fecab102f9b58b28`; 构建/输入及包/exe 身份留在本地 `build-Release.json` 与 `inputs-Release.json`. 新建的正式无 fixture binary tree 已配置通过, 不注册 Gate 目标.

首次构建的本地头路径错误(C1083)和随后不推荐的 Framebuffer 重载警告(C4996, /WX 拒绝)已纠正, 对应独立原始日志仍保留在 `build/d03-t05/logs/`, 未关闭警告或降低编译选项. 以上是构建接入证据, 原生结果另列如下, 不将配置/编译当作图像验收. 默认不声明跨 GPU/OS、呈现、自然 OOM/device loss、长期压力或性能结论.

Release 原生首轮在 CCW 点失败, 其它已读图案正确; 依据 design 决策2中模型/NDC 与目标空间的区别纠正本地 RasterState, 保持输入顶点、像素预期、clip 和正式源码不变. 原失败运行保留为 `20261108T094421316370Z-run-Release-full.log`. 重建后 `python build/d03-t05/Evidence.py run Release full` 于 `20261108T094559168161Z` 完整通过, 实际设备为 Intel(R) RaptorLake-S Mobile Graphics Controller / GL 4.6.0 Build 32.0.101.6129. A/B 图案、四角/offset/builtin、深度/方向/scissor、copy C/D 与哨兵均符合预定 oracle. sRGB 字节为137/187/225/128, 在预置±1量化单位内; 采样回线性为 `.2505684/.4966049/.7535973/.5019608`, 在预置 RGB≤.006、alpha≤1/255 范围内, 未放宽阈值. 两个拒绝分别报告 index byte interval 越界和 sampling/writable-attachment feedback, 目标保持哨兵, 合法前缀仍取得真实完成.

后续实际运行: `python build/d03-t05/Evidence.py run Release positive` 于 `20261108T094649761311Z` 独立进程复现通过; `build Debug` 成功后, `run Debug full` 于 `20261108T095259119568Z` 通过同一图案及两项拒绝. 三次成功运行均绑定联合输入 `cdbdffe86de2ae4436f2f1d6eca823a023bf5ff6f8d9cae07ff89d409abbbbb2`, 对应配置实际 exe、生成接口、包与 CMakeCache 在启动前逐份核对. Debug/Release 拒绝各为两个实际 Error, 非法请求未写目标, 正常关闭成功; 正向阶段无 Error. 未降低 Debug STL/运行时检查, 本包未观察到 C1001. 正式入口及交接核对结果见下节.

### A 最终本地证据, 待独立 B 审查

在不包含 fixture 的新 binary tree 执行 `cmake -S . -B build/d03-t05/formal -G "Visual Studio 17 2022" -A x64`, 再执行 `python build/d03-t05/Evidence.py formal Release`: 正式 `Physara` 从该树构建成功, `--hidden --poll-limit=3` 退出0, 构建至运行期间输入保持一致. 正式 solution 无 NativeDrawGate/接口样例 target; 不删除维护者的主 build, 不恢复旧 fixture. 本轮已达到既定最小验证预算, 停止新增 GPU 运行.

联合输入含1078份正式树文件和6份 fixture, 算法为相对路径到原始字节 SHA256 的映射按 JSON key 排序、紧凑编码后 SHA256; 完整映射在本地 `inputs-Debug.json`/`inputs-Release.json`. 它不等于 commit、15文件 native 摘要或包 ABI. 最终两配置逐项输入/产物复查通过. 配置相关包身份不同来自正式编译指纹, ABI 相同.

| 产物 | 当前 SHA256 / 内容身份 |
|---|---|
| Release NativeDrawGate.exe | `62e334a875e2296aad3260e410981af86ddd342dc0443e3d3591c43f040ec43c` |
| Debug NativeDrawGate.exe | `05885c9a5a6a32a6ba60241fccf9ba4114bc62fd3e1a0c93ea50f15bc1b0aa5a` |
| Release Reference.pshader 文件 | `655d66ca675d6174b394e505a735644ee2f5aa88eb01ee5ee7dcb440b71c37e3` |
| Debug Reference.pshader 文件 | `a0815505973a461c27c1eb4b5ba96eaa982e281dee6440750112c267dbe32b45` |
| Release 包 contentHash | `6096f656684b9862f9237675b24fa04b010b855cf25f20820a033e4fcc3cc8dd` |
| Debug 包 contentHash | `dca5025c88d7a4ab13b758778b810ef8fafe389c2992a100da9ecfe15aab4a5e` |
| 新正式 Release Physara.exe | `804d0a270786e92153d6045a5851215f9a96326bdf4f68cd093a78e310ce9e06` |

delta 场景与共享证据映射如下, 不表示七套独立程序或每项另跑一轮:

| Scenario | 本次实际证据 |
|---|---|
| Produce a known asymmetric image through the formal chain | 两配置均真实生成/读取 Gate ABI 与 canonical 包、GL Pipeline link、公共 Draw、非零提交/Event 完成后读取四角/L/clear 点 |
| Distinguish the requested instance and indexed vertex ranges | binding offset=4 bytes, firstIndex=3, baseVertex=8, firstInstance=2, count=4; 独立实例位置与 SSBO/builtin 身份共同决定四角, 干扰记录和 clear 对照不进入预期 |
| Judge depth orientation and color with independent numeric expectations | A 的深度为 .5/.6953125/0, B 为 .5/.69270831/0; CCW/负缩放、scissor 内外及 sRGB 原字节/采样值满足运行前的 oracle, 未调整容差 |
| Reuse a prepared pipeline on another compatible target | 单次创建的主图 Pipeline 用于64×48的A与96×72的B, 同一图形/剔除 Pipeline 在两目标重复使用, 动态区域各自生效 |
| Observe copied texels through a later sampled draw | A→C 区域copy, C/D 的四角片段、梯度底色、L 与哨兵均符合预期, 所有 copy/采样经公共入口 |
| Repeat the completed reference without reusing stale executables | Release full/positive 为两个独立进程, Debug full 为第三次成功运行; 每次有 build/input/package/exe/cache 身份检查, 无过期 exe 运行 |
| Reject a representative illegal integration request | Debug/Release 各有一次紧邻 IB 越界及一次同范围 sampling/write feedback 拒绝; 每次一个对应 Error、目标不变、合法 clear 前缀实际提交完成 |

正式 C++/Shader/CMake、接口 ABI、Architecture、主规范和历史归档均未修改, 没有发现需修复的正式实现缺陷. T04 已接受的15份 native 源/CMake 实际字节仍为 `ca3fc3d66dcc64f4dffd6495bd503528a69daa6d853e4dd263ea60fe45f91225`, 沿其原路径排序/TAB/逐文件SHA/LF算法核对; 此摘要不冒充全树或联合输入. Build 增加当前图像消费者的查阅入口, RHI 原有支持表已核对且无能力变化, 无需修改. 本轮正式代码/注释改动范围为空, B 仍须独立确认范围、合同、证据与文档结论.

复用的是未改变的 T03 R32/显式用途及 T04 查询预算/受控失败等独立合同和归档结论; 未把它们标成 T05 新运行. 旧 fixture 已清理, 本轮不运行旧程序. 上述所有 T05 图案与所列正式 Release 构建/启动均为本次实际执行, 不是复用旧图像. 正式产品 Debug 启动、本轮未改变的逐项/故障矩阵不重复运行. 当前环境之外的 GPU/OS、呈现、无显示服务器、自然 OOM/device loss、长期压力和性能均未覆盖, 不作为本包声明.

交接本地材料为 `build/d03-t05/handoff-fixture.zip`, 含6份输入和本次 Evidence.py, SHA256=`7d5aaf93c3f81828ea45aeb2d587ef963eef7c29a632d54ad1bbaee092ffb150`. 原始构建/运行日志及逐次退出结果保存在同目录 logs/operations.jsonl, 首次失败未覆盖; 包和 exe 仍须按当前身份使用. 这些路径可随 build 删除, 不纳入版本控制、正式代码审查或默认工程, 不承诺自动恢复.

<a id="review-evidence"></a>
## 独立审查与验收留存

2026-11-08: A 已按实际 dirty/untracked 版本完成本地检查并交接 CodeReview, 当前11/17. `openspec validate establish-opengl-native-draw-gate --strict --json` 为1/1 valid、无 issues; 8份 Markdown 的170处本地文件/锚点引用及 `git diff --check` 通过. 这些是 A 自检, 不代替 B 或最终 verify. CodeReview 和开发计划已标记待独立 B 审查/注释, 正式代码无改动的适用性仍由 B 独立确认.

上述是 A 首次交接时的记录. B 随后完成首轮审查并补充 Bindings.cpp 的6行说明性注释, 发现1项 P2存量缺陷 CR-001, 尚未最终验收. B 通读 Bindings.cpp/.hpp, 沿 Shader/BindingPlan/lowering/Pipeline、顶点与资源/目标、即时图形/传输/ContextState、提交/查询/退休及包消费主线核对; Validation/Common/宿主大文件仅覆盖所列直接入口. B 未审 build、未编译/GPU运行或核对联合输入, 原生证据只引用 A. 代码正确性有 CR-001 缺口, 本轮编辑文件注释已补充, A 修正后仍需 B 复核代码与完整受影响注释. B 接收后的 Bindings.cpp SHA256为 `f81dc175504fa35cb27143570cfb4edadac1833b63a8010d4b346cc5e0240d9e`.

<a id="correction-evidence"></a>
### A 按 CR-001 整改并自检

CR-001 由 [fix-opengl-binding-buffer-ranges](../2026-11-08-fix-opengl-binding-buffer-ranges/tasks.md) 承接已归档绑定行为修正, 保留 B 六行注释, 在原 owner 增加零长度剩余解析, 不改公共API/Common/Validation或放宽原边界. 原图像 oracle/Shader输入不变, 用满足公共约束的256-byte本地CB分配和零长度CB/SRV绑定重新运行. 当前功能回归与完整输入/产物身份见关联修正 tasks, 原 cdbd... 表格只作为历史, 不把旧 exe 冒充当前结果.

本轮 Release full/独立进程 positive、Debug full 及正式 Release 增量构建/隐藏3次poll均通过; 原图案、深度、copy/采样和sRGB结果不变. 新增范围回归 raw/Validation 各验证两种用途的zero/explicit/sentinel等价及剩余+1拒绝, 仅补CR-001独立缺口. 当前联合输入为 `3ba5e8dc20a4558199036dd73bd7cf5c586b83140737db7d98cee82d0f789227`, 正式代码差异仅 Bindings.cpp, 当前SHA256=`225259a35cdc6b82d2577fe9b327a1633a307c6e263c95757a26c3116e41bbc6`.

A 只勾本次5.3, 当前12/17. B 首轮按其维护者授权未改 tasks, 第5.2待 B按已有报告补签, 第5.4及第6组仍待; A 不代勾 B 的项或签最终验收. 两份 change 共用本轮问题单, 待 B 复核后分别最终 verify/sync/archive, 关联范围都完成前不重置问题单或验收 T05/D03 出口.

<a id="b-final-review"></a>
### B 整改复核与注释接受

2026-11-08: 上段12/17为A交回时的状态. B本次按已有首轮报告补签5.2, 并完成5.4, 当前14/17; 第6组仍待A执行. 共享问题单的CR-001已由B复核关闭, 未新增正式问题, 代码正确性与注释可读性均通过. 存量修正的完整结论同时留在 [fix-opengl-binding-buffer-ranges](../2026-11-08-fix-opengl-binding-buffer-ranges/tasks.md#b-final-review), 不重复建立审查轮次.

B接受版本为 `v2.0` / HEAD `5011b95bd8dbdb6a34596f4b3cca9054bf913a16` 加实际未暂存/未跟踪输入; 唯一正式代码差异为Bindings.cpp, SHA256=`225259a35cdc6b82d2577fe9b327a1633a307c6e263c95757a26c3116e41bbc6`. B重新通读完整Bindings.cpp/.hpp, 核对零长度分支、显式越界拒绝及原有CBV/record/设备约束, 复核其职责/接口/回滚注释, 原六行说明保留; 本次未再改源码或fixture. 首轮其余主线覆盖按问题单保留, 不重审无变化的全仓.

B只按交接点名的清单/构建/操作记录与必要日志核对证据, 未遍历build或审fixture代码. 两配置各1084项输入的当前字节与清单一致, 重算联合摘要均为 `3ba5e8dc20a4558199036dd73bd7cf5c586b83140737db7d98cee82d0f789227`; 两配置各13项构建产物及三次修复后图案运行、正式Release启动记录的产物摘要也与当前文件一致. CB `{0,512}` / SSBO `{256,256}` 的zero/explicit/sentinel等价和513/257拒绝、原IB/feedback拒绝与sRGB结果符合记录, 修复前/中间失败保持留存.

原生结果复用A的Release full `20261108T115032982419Z`、独立positive `20261108T115116951061Z`、Debug full `20261108T115130249157Z` 及正式启动 `20261108T115327070088Z`, 不记作B重跑. B实际执行源码/diff/注释和身份核对、文档链接/严格结构/diff检查; 未新增测试、编译或GPU运行. 现行公共CBV约束保持, 中间64-byte CB零长度请求的失败不计成功; 最终图案按已记录的256-byte分配消费64-byte payload. 不扩展为跨设备/OS、呈现、公共readback、OOM/loss、压力或性能验收.

B复核已完成, 后续由A在接受版本上执行本change与关联修正的最终verify、各自规范同步和归档. 当前未同步/归档或重置问题单, T05/D03出口仍待最终收尾; 代码/构建输入再改变时重新交B核对受影响范围.

B文档收尾实测: 两份change严格结构校验各1/1 valid、无issues, 12份Markdown的198处本地文件/锚点引用及diff检查通过; 非注释token核对仅有预期零长度条件变化. 更新本轮文档/勾选后, 两配置当前输入仍匹配接受摘要, 无源码或构建输入漂移.

<a id="annotation-coverage"></a>
### B 注释覆盖留存

以下留存 B 已签署问题单的覆盖与限制, 不另作 A 代签. B 通读本轮实际编辑的完整 Bindings.cpp/.hpp; 其余主线保留已经准确的说明, Validation/Common/宿主大文件只覆盖直接入口. 原问题单原始字节另存于本地 `build/d03-t05/final-verification/CodeReview.accepted.md`, SHA256=`f64e2f2ab0eb13fdbcc8c9f6bd4af452aec7c9aba5e127c6a7685e76f29301b4`; 本地副本可随 build 删除, 本节与上方签署结论才是归档留存.

| 文件或模块 | B 已核对的职责、合同与流程 | 剩余范围 |
|---|---|---|
| Bindings.cpp/.hpp | canonical 身份、不可变集合依赖、范围解析、整组预检→物化 view→发布/回滚; 零/全一剩余语义与显式越界拒绝、原 CBV/extent/stride/设备限制, 原6行有效说明保留 | CR-001关闭, 代码与注释均通过, 本次无需再改源码 |
| Shader.cpp/.hpp、BindingPlan.cpp、Lowering.cpp、Pipeline.cpp/.hpp | canonical 模块/反射/紧凑计划/Program 分工、跨阶段布局和 IO、实际 limits、同设备及目标兼容、base instance lowering、link 准备与回滚 | 原生编译/link只引用A, 不以BindingSet指针缓存替代资源映射 |
| InputLayout.cpp/.hpp、Framebuffer.cpp、Texture.cpp、Buffer.cpp、Sampler.cpp | 顶点/附件/视图与存储所有权、slot/stride/实例频率、形状/格式/样本/用途、预检→创建→FBO COMPLETE→发布/删除 | 不重验全部格式、MS分配或历史故障 |
| CommandList.cpp/.hpp、CommandListGraphics.cpp、CommandListDraw.cpp、Graphics.cpp | 唯一即时recording、图形候选、IB偏移/firstIndex/baseVertex/实例区间、动态区域/feedback、预检→保活→用途→ContextState→发布及逐Draw用途恢复 | CPU索引字节区间检查不代表读取并验证任意GPU索引内容 |
| CommandListTransfer.cpp、CommandListUpload.cpp、CommandListState.cpp、ContextState.cpp/.hpp、ContextGraphics.cpp | 上传/clear/copy、tracker/native cache边界、子资源/哨兵、viewport/scissor、绕序/sRGB、clear失效后重建及采样两端用途 | 未审build观察helper, 未独立确认读回或驱动行为 |
| CommandListOwnership.cpp、Upload.cpp/.hpp、Submission.cpp/.hpp、Queries.cpp/.hpp、CommandListQueries.cpp | packet lease/依赖、提交FIFO/查询、非零票据/零快照、Pending/Failed/有限等待、fence/flush后发布、真实完成退休及失败前缀收拢 | T04故障/退出只引用A历史记录, 未重跑 |
| Engine/Shader/RhiProgram.cpp/.hpp、Package.cpp/.hpp、Compiler.cpp及生成规则 | 正式编译/消费单向入口、expected ABI及IR/元数据、VS/PS失败不交付部分集合、唯一生成依赖与raw/Validation消费 | 未全仓审计生成器、文件系统或全部增量分支 |
| ValidationDevice.cpp、ValidationCommandList.cpp、Common/Misc.cpp、Common/StateTracking.cpp/.hpp的相关入口 | 公共范围、图形委托、tracker权威、集合/类型及SetGraphicsState/Draw/Execute/Event; wrapper不发布未经确认的候选快照 | 范围差异由CR-001修复, 不声称数千行全文件重审/注释 |
| Platform/RhiDeviceHost.cpp、OpenGLDeviceHost.cpp、DeviceHost.hpp、DeviceHost.cpp装配/退出入口及Runtime/Main.cpp | 宿主context/Device、借用与额外CPU owner、Pending保留、回调寿命及drain→根释放→context退出; 产品仍为事件循环 | 其它窗口输入、Editor、呈现和未来Renderer未重新验收 |

<a id="final-verification"></a>
## A 最终 verify 与收尾

2026-11-08, B 最终签署后使用原生 openspec-verify-change 技能, 读取两份 change 的 status/apply context、proposal/design/delta/tasks及直接消费链. 接受版本仍为 `v2.0` / HEAD `5011b95bd8dbdb6a34596f4b3cca9054bf913a16` 加实际工作区, Bindings.cpp SHA256仍为 `225259a35cdc6b82d2577fe9b327a1633a307c6e263c95757a26c3116e41bbc6`. A 本轮未改变源码、fixture、编译配置或默认构建.

| 维度 | 最终 verify 核对结果 |
|---|---|
| 完整性 | 最终17/17完成, 本delta的3条新增requirement、7个scenario均有证据. 进入verify时14/17, A第6.1–6.3在本轮顺序完成verify/同步/实际归档及后核对后才勾选 |
| 正确性 | 3/3 requirement映射到正式Shader包消费、即时Draw/跨目标与copy/采样、真实提交/Event完成链; 7/7场景由上方逐场景表和修复后同一固定图案覆盖, CR-001的独立差异由关联修正验收, 无缺失行为或场景 |
| 一致性 | design的7项决定与最终实现一致; 固定oracle/容差、冷路径DSA例外、唯一owner、build隔离、有限拒绝预算及关联存量修正均保留. 唯一正式功能diff为ResolveBuffer零长度条件, B的完整注释结论有效, 无代码风格或依赖偏差 |

A本轮实际执行: 两份change分别 `openspec validate <name> --strict --json` 均1/1 valid且无issues; 核对12份Markdown的198处本地文件/锚点引用及 `git diff --check`; 重新逐项核对两配置各1084份输入、各13份产物和正式Release的2份产物, 原始字节及联合输入仍为 `3ba5e8dc20a4558199036dd73bd7cf5c586b83140737db7d98cee82d0f789227`. 归档前的身份与主规范基准保存在本地 `build/d03-t05/final-verification/accepted-identity.json`, 不以其替代这里的结论.

原生结果复用已经匹配的Release full `20261108T115032982419Z`、独立positive `20261108T115116951061Z`、Debug full `20261108T115130249157Z`及正式Release隐藏3poll `20261108T115327070088Z`, 本轮没有重新编译或运行GPU. 这三次图案实际运行覆盖当前7个场景, sRGB实际字节137/187/225/128及采样值仍满足预置容差; 两项非法集成输入在Debug/Release均拒绝, 无非法目标写入, 合法前缀正常完成. T03 R32/显式用途和T04未改变的查询/故障合同只引用既有归档, 不计作本轮运行.

未覆盖范围保持不变: 其它GPU/OS、产品呈现/Editor/RenderGraph、公共staging/map/异步读回、自然OOM/loss、长期压力和性能. 这些不是本包采纳的验收范围, 没有以缺失环境冒充通过. 适用的verify检查均已执行; 没有新增实现缺陷、场景缺口、设计/注释警告或建议, 仅余上述A流程收尾, 完成前不声明归档成功.

规范同步: 读取两份change各自当前的 `instructions specs --json` 后, 本delta的3条requirement/7个scenario已语义合并到主规范, `opengl-immediate-draw` 从9条变为12条, 原9条内容/顺序和Purpose均保留. 关联绑定修正只更新其既有1条requirement并保留原3场景, 其它10份主规范原始字节未变. `openspec validate --specs --strict --json` 为12/12 valid, 无ERROR/WARNING; INFO仅提示部分段落长度. 开发计划已按实际验收记录T05/D03出口通过、两份change待归档, 未提前实施D04.

### 归档结果

官方 CLI 已依次执行 `openspec archive fix-opengl-binding-buffer-ranges --yes --skip-specs --json` 和 `openspec archive establish-opengl-native-draw-gate --yes --skip-specs --json`, 均退出0, 本包实际位置为 `openspec/changes/archive/2026-11-08-establish-opengl-native-draw-gate/`. skip-specs仅避免重复已核对完成的同步, 没有关闭CLI默认验证. 调用时仅第6.3项为归档后核对/重置任务, 因而保留16/17直至真实成功, 未提前勾选.

移动后先核对两份change共10份artifact原始字节及12份主规范摘要全部不变, 包含两份.openspec.yaml; 再只修正本轮新归档内部及Build/开发计划的实际相对引用, 不改变更早归档或已采纳设计. CR-001的发现/修正/B接受版本、完整注释覆盖及证据/限制均已留在两份tasks, 接受问题单本地副本已保存. 开发计划已记录D03五包及出口完成, 当前入口为D04-T01待立项, 没有实施下一阶段.

最终后核对: `openspec list --json` 无活动change; `openspec validate --all --strict --json` 为12/12 valid, 仅上述段落长度INFO, 无ERROR/WARNING. 12份Markdown的186处本地文件/锚点引用通过. 两份关联范围归档/留存完成后, CodeReview已按模板重置且原始字节相同(SHA256=`a26066c0106ba8d1991974974acf06ddc62ed5d24e2878a42eda8f6fbec847fc`), 再勾选第6.3项. 本包17/17、关联修正6/6完成; 最终verify的CRITICAL/WARNING/SUGGESTION均为0, 适用检查无跳过, 本轮流程闭环完成. 未创建commit、暂存、推送或进入D04实现.
