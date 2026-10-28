# Tasks

## 1. 实施入口与固定输入

- [x] 1.1 保存 D01 原 change 15/16、4.1/出口未通过和当前单屏的实际入口结论; 核验本次维护者单独批准的 T04 上传、必要即时列表/fence/在途保活/Host 关闭及 T05 最小读回例外已同步 proposal/design/spec/tasks/Developments. 检查原因、范围、回归和退出条件明确, 不继承 T03 授权或以单屏/CPU 注入代替跨屏, 完整 T05/D03/呈现及 D01/D02 出口不连带完成; 无有效本包授权时停止实施.
- [x] 1.2 入口满足后保存届时源码/主规格/依赖/工具版本及当前 Git 标识, 核对已经调研的 61 个 CommandList、68 个 Device 方法矩阵与真实目标闭包. 验证没有改变本设计前提的代码差异, 以新 binary 目录建立正式 Host/native 与 generic 独立消费者检查入口; 不依赖旧 build 残留或重新下载依赖.

## 2. 原生列表、物理包与 CPU 所有权

- [x] 2.1 按 design 2 建立完整列表声明/定义和 Unsupported 闭包, 接入既有 PhysaraRHIOpenGL target. 先实现工厂/只读 GetDesc/GetDevice 与明确拒绝, Write/状态/提交正向行为分别随后组验收. Debug/Release 检查 61/68 方法含重载及继承查询、首 include、缺失入口/非法 alignment/参数的失败和无关 payload 不解引用; 同步模块 CMake、Build 实际入口和初始支持矩阵, 不泄漏 GL/Core/Platform 到 generic.
- [x] 2.2 按 design 3/4 实现 Device 所有的最多 64 个物理包、逻辑列表生命周期和 RecordingId 独占 claim, 先预备包/退休节点再改变状态. Debug/Release 检查 raw/Validation 混用下的单 open、错误线程/current、重复状态、Closed 未提交重开拒绝、Open CPU/容量失败后的 Close/释放/重试和 ID 溢出; 核对旧 recording 不能释放新 claim, 补齐私有头线程/借用合同.
- [x] 2.3 实现 design 11 的内部 UploadResourceLease、唯一内部 wrapper 数和纯 CPU shutdown preflight, 不建全资源 registry. Debug/Release 以真实 Buffer/Texture 检查同资源多包引用、额外 caller/Device/Validation/list/FBO/Sampler owner 的区分、零分配预检和 worker 最终 Release 只排交已有包; 本组先验证无 GPU 写入的退出, 实际在途覆盖随 6.1 完成. 同步内部记账与根 owner 前提说明.

## 3. 单一 tracker、提交与完成基础

- [x] 3.1 在 native Buffer/Texture 接入已有 state extension, 工作包复用 Common tracker, 实现 design 8 的受限 BeginTracking/Set/Get、automatic 开关、Commit 和 Close 恢复. Debug/Release 检查 Common/CopyDest/允许的初始恢复、未知前态/非法 range/unsupported state、手动缺少 Commit、tracker 分配失败和 Reset 不发布; state-only 正例通过真实 fence 后再检查提交 metadata, 不修改通用算法或用 BeginTracking 冒充转换. 随实现更新 transfer profile 与头中借用前提.
- [x] 3.2 按 design 9 实现整批 CPU 预检、真实 fence/flush 成功后唯一递增票据和一次消耗, 保持空 batch=0. Debug/Release 检查最多 64 项、后项无效/foreign/duplicate/open/已消费/计数溢出在拒绝边界零提交且不消耗该边界前项, 非空 state-only 批次建立真实 fence, 原生成功后 metadata 无新 CPU 分配. 验证 wrapper 预检失败、raw 预检失败及委托后 0 的区别: 底层失败原样返回且不恢复已消耗包装状态, 原生前缀继续保活. 对真实写入无重放的观测在 4.2/5.1 同时验收; 同步票据与生命周期说明.
- [x] 3.3 按 design 10 建立有限退休队列、GC 零超时 Complete/Pending/Failed 分支和显式 WaitForIdle 的真实临时 fence/5 秒期限. Debug/Release 以真实 sync 检查 Complete/精确一次释放及实际 idle, 受控 Pending/WAIT_FAILED/timeout 单独标注并检查不提前释放/不推进假完成、零阻塞 GC、普通路径不 glFinish. 覆盖 temporary sync 清理失败与原错误保留, 更新完成/等待合同.
- [x] 3.4 按 design 12/13 实现 orphan/no-issued-work 的退休、Faulted/Draining/Stopped 和两个窄 native 关闭互操作入口, 接入独立 root 与 Host 已知 root 的预检. Debug/Release 先验证空包释放、无提交 metadata、错误 owner/mode/current、drain Pending 保留状态和幂等最终停止; 未完成的真实上传前缀、exit-only finish 与 Host 装配分别随 6.1-6.3 扩大. 更新 OpenGL.hpp 的失败/重试/最终 context 释放前提, 不先写成已全面支持.

## 4. 有界上传存储与 Buffer 内容

- [x] 4.1 按 design 4 实现 packet 独有的 persistent coherent chunk、checked alignment/容量、map 和完成后 unmap/delete, 默认无 idle cache. Debug/Release 在真实 context 检查 storage/map flags、可用 mapping 和预算计费, CPU wrapper/记录及实际 name/storage/map 后故障无未发布泄漏, NULL map 和实际 GL 首错可观察; 不在 Pending 时回收块. 随本组更新预算/块寿命及来源差异说明.
- [x] 4.2 实现 design 5 的小 Buffer DSA subdata 和大 Buffer chunk/GPU copy. 在同一正式 Host 的 Debug/Release 通过公开 Write/Close/Execute 和真实完成后最小 native 读回验证非零偏移、64 KiB+1、256 KiB 两侧、重复覆盖及已初始化区间保持; Write 返回即 overwrite/free 原 CPU 源, 证明 Execute 不重放和 GPU 不再读取原 source. 同步 Buffer 源寿命和支持说明.
- [x] 4.3 补齐 Buffer 的 size=0/null/end boundary、signed-size/offset 溢出、wrong implementation/Owner 和不支持 profile. Debug/Release 检查无效请求零 destination update、空写零分配/零 native 写入, 当前 CPU 预备失败不应用本次写且先前前缀保留; 大小拒绝不真申请极端容量. 将对应失败前提写入方法合同.
- [x] 4.4 实测或明确受控验证 64 MiB upload/包逻辑字节、64 物理包、4096 写入/资源项上限, 包括参数最小 chunk 的对齐后越界. Debug/Release 检查耗尽立即报告 requested/retained/limit、不隐式 wait/无限增长, Pending/Closed 不复用, Complete/GC 后可恢复预算且所有计数归零; CPU/driver 内存口径分别记录, 不把本包预算当全设备显存上限.

## 5. Texture 数据、footprint 与原生状态

- [x] 5.1 按 design 6 实现唯一 external format/type 映射和七种非 MS 维度的 PBO 上传. 在 Debug/Release 经正式公共 RHI 路径逐项读回 13 种颜色格式、非对称尺寸、各 mip、array/cube face/cube-array 和 3D volume, 核对 exact subresource、不重放、source 返回即销毁及未改已初始化的其它 subresource. 深度/模板/MS/排除格式写入拒绝与原分配行为分别验证, 随本组更新格式/维度矩阵.
- [x] 5.2 实现 checked row/depth footprint 和逐行紧凑复制. Debug/Release 检查多种 padding、非整 texel padding、3D slice stride、单行 rowPitch=0、单 depth 忽略 depthPitch, 以及短 stride/最大整数/无效 mip/layer 的读取前拒绝; 对 source padding 使用不可当 texel 的哨兵, 对未初始化目标不作内容比较. 在头/来源说明写清实际 CPU 指针前提.
- [x] 5.3 实现 design 7 的窄 ContextState、ClearState 失效与删除时缓存失效. Debug/Release 先经 fixture 私有边界弄脏 unpack/PBO, 再走公开 ClearState/Write 检查正确数据、全部相关 pixel-store 和 GLuint 真实重用; 证明没有 glGet snapshot/restore、第二份 cache、无消费者 Program/VAO 状态或产品原生逃生接口. 同步互操作的 ClearState 前提.
- [x] 5.4 验证上传编码与真实失败边界: Debug/Release 读取已编码 sRGB/linear bytes、整数/half/float 的已知可表示值、非默认 swizzle、行/slice/face 标记及 sampler 参数不变; 在真实 chunk/PBO 和目标写入后注入参数/拷贝失败, 检查初始首错/预算/引用保护和未发前准备回滚. 不用 gamma/翻转兜底, 不声称任意 NaN 位模式保真.

## 6. 实际在途退出、Host 与故障组合

- [x] 6.1 用正式上传扩充 2.3/3.4 的真实寿命证据: Debug/Release 覆盖 native/validated list 在 Open/Closed/Submitted 的释放、worker 禁分配零 GL、同资源跨多个 packet、caller 资源提前 Release、实际完成后精确一次删除和 FBO 依赖. 辅助真实 foreign Device/context 先比对本包全部 required 入口地址再生成负例, 恢复 current/入口后两设备仍可用; 同步结果与自然/受控证据边界.
- [x] 6.2 按 design 13 接入 Host 的纯 CPU 外部 owner 预检、current 恢复、drain、RhiDrainPending 和最终逆依赖释放. Debug/Release 覆盖外部资源/raw/Validation/list 拒绝时 Ready/input/claim 不变, 只有内部在途引用可完成关闭, Pending 时停止 GetDevice/输入并保留全部 owner/context 供重试, 完成后重建/幂等/旧 Host 隔离成立. 只追加枚举值, 同步公开 Host 借用/析构合同及 Developments 实际子集状态.
- [x] 6.3 覆盖 glFenceSync 空/错误、flush 错误、WAIT_FAILED、已发未提交前缀、RHI/GLFW cleanup 次错和日志失败. Debug/Release 检查无 positive ticket/假 metadata、失败包不复用、可用 context 只在退出 glFinish、再次失败/确认 loss/恢复 current 失败后 CPU-only 且不再 GL, 首错保留. 有效 fence Pending 不以 glFinish 假装正常完成, 超时后能重试; 清楚记录退出调用不具硬实时保证.
- [x] 6.4 在已取得真实 GPU 工作后验证低内存退出和寿命环打破: 禁止新的 CPU 分配下执行 worker 交接、外部 owner 预检、收集/最终退休, 检查 packet/chunk/内部 holds/删除记录全空. 在隔离进程检查仍有外部 owner 或未完成 drain 就析构的 fail-fast、wrong-thread 和 detach 合同, 核对 stopped Device 的最终 CPU Release 无 GL/回调. 不借删除检查、放宽断言或猜测 GPU 完成取得通过.

## 7. 集成回归与交付核对

- [x] 7.1 在上述各组已分别通过后执行完整 Debug/Release 集成: generic 独立头/消费者、既有 Common/Validation CPU 正反例、T03 分配/附件/所有权检查及 Core/Host/输入/Runtime 回归, 正式产品 help/unknown/hidden 的既定返回码. 检查生产库无检查宏/测试符号、native/generic target 依赖不泄漏、61/68 矩阵和全部 Unsupported payload/output 行为完整, 没有新增第一方警告或 reference target.
- [x] 7.2 交叉核对各组已经随实现更新的 CMake/Build、RHI 来源/支持说明、native/Host 头和 Developments, 只报告实际启用与运行的 profile/环境/证据. 验证链接/来源/许可/命名/文件作用域、主规格/历史归档/Architecture/D01 原 change 的保护和 git diff --check; 保留 T05、D03 和阶段出口边界, 不创建重复产品或流程外必交文档.
- [x] 7.3 按原生 verify 核对完整性/正确性/一致性及本文件要求映射, 进行 Code Review: source 何时可释放、内部/外部 owner 如何区分、所有失败前缀谁保活、预算何时释放、GPU 何时不再访问和 Pending 关闭如何重试. 验证 status/apply/strict 与实际任务证据一致, 尚未运行的检查不勾选, 修复适用验收的遗留问题后才声明本包完成; sync/archive/Git 提交不作为本规划的已授权实施动作.

## 要求与验收映射

| 要求 | 任务 | 主要可观察证据 |
|---|---|---|
| Record one owning immediate upload list | 2.1-2.2, 7.1 | 正式 Host/raw/Validation 生命周期、独占 claim 和 owner 拒绝 |
| Write checked ordinary buffer intervals | 4.2-4.3 | interval/native bytes、空写与溢出零更新 |
| Write the requested texture subresource | 5.1-5.2 | 七维度、mip/face/volume 和排除写入 |
| Interpret pitches without reading padding as texels | 5.2 | checked 最后读取字节、padding 哨兵与 stride 正反例 |
| Preserve uploaded encoding and row order | 5.1, 5.4 | native 数值/编码/非对称方向、swizzle/sampler 保持 |
| Consume borrowed CPU data before write returns | 4.2, 5.1, 6.1 | 立即 overwrite/free source 后真实完成读回 |
| Bound pending upload storage and work | 2.2, 4.1, 4.4 | 全部预算、Pending 不复用与完成后归还 |
| Keep transfer-state publication separate from recording | 3.1-3.2, 6.3 | 单 tracker、拒绝/放弃不发布、接受提交才发布 |
| Submit closed upload batches without replay | 3.2, 4.2, 5.1 | 整批拒绝、真实 fence/flush/递增票据、一次写入 |
| Reclaim only proven completed work | 3.3, 6.3 | Complete/Pending/Failed 与显式 idle deadline |
| Retire abandoned immediate work safely | 3.4, 6.1, 6.3-6.4 | orphan 前缀、worker 零 GL/分配、内部退休不发假票据 |
| Preserve upload failures transactionally | 3.2-3.4, 4.1-4.3, 5.4, 6.3 | CPU/native 分阶段故障、保留前缀/首错 |
| Distinguish internal retention from external ownership | 2.3, 6.1-6.2 | 内部 holds 与真正外部 owner、零分配预检 |
| Drain issued work before native-environment release | 3.4, 6.2-6.4 | actual drain、超时重试、失效退出与寿命环清空 |
| opengl-device-resources: Reject unavailable operations without fictitious GPU work | 2.1, 7.1 | 完整 61/68 闭包与旧 excluded-family 场景 |
| opengl-device-resources: Report allocation separately from completed rendering | 1.1, 7.2-7.3 | 当前入口、actual profile、后续工作和证据边界 |
| native-window-host: Release native ownership completely and repeatably | 6.2-6.4, 7.1 | 全部七个旧场景、内部在途/超时/无 fence 关闭 |

## 证据记录约定

各任务按真实命令、配置、环境和源标识记录到本文件相应任务状态及 ignored build/artifacts/establish-opengl-resource-uploads/. 原生读回/完成、CPU 分支、受控 native 返回分开标记, 不把计划和上轮测试当作本包执行结果. 规划创建时未运行 C++/GPU 任务, 当时实施项未勾选、D01 入口未通过且本包尚无独立前置例外; 后续独立授权与实施事实分别记录如下.

## 规划核验记录 (2026-10-28)

已按官方 OpenSpec 1.13.2 返回的产物依赖、模板和各自指令完成 proposal、三份 delta specs、design 和 tasks, 没有创建流程外必交文档. 原生 status 为规划 4/4, apply 为 ready、0/24; ready 只表示规划齐备, 不代表 D01 实施入口已通过. D01 原 change 仍为 15/16, 本包没有自动继承 T03 例外.

当前为 14 ADDED + 3 MODIFIED 要求、69 个场景及 14 项设计决策, 全部有任务/验证映射. 受影响旧要求的 12 个场景名称和覆盖保留, 其中 9 个正文不变; resource-only/list 与完成报告/前置的三个场景按明确的新能力边界更新, Host 的七个旧场景原文保留. 三份 delta 尚未同步主规格.

规划审查已纠正两个会影响实现的歧义: Validation 委托后返回 0 不恢复已消耗包装状态, 与 wrapper/native 各自预检拒绝分开验收; fence/flush/wait 失败后的 Faulted 包不由普通 GC 重新解释为正常完成. 关闭使用内部 lease 的真实来源区分外部 owner, 不用地址 registry 或 OOM 时临时分配的统计 map. 预算、profile、源数据消费、状态发布、GPU 完成和 Pending 关闭的动作/退出均已明确, 没有留作会改变实现路径的 Open Questions.

本轮原生 strict 7/7 通过, 当前 change 无提示; 11 份相关 Markdown 的 972 个本地链接/锚点、明确引用的 GL 函数/常量声明及 git diff --check 通过. 检查脚本和 CLI 原始输出在 ignored build/artifacts/establish-opengl-resource-uploads/. 801 个受保护输入的字节标识未变, 包括 C++/CMake、架构、主规格、既有归档、D01 原 change 和固定第三方. 当前非 ignored 编辑仅本 change 与 Developments 的事实纠正/对应工作包衔接; 未构建或运行本包 C++/GPU 检查、未勾选实施项、同步主规格、归档或提交 Git.

## 实施入口核验 (2026-10-28)

本次 apply 已回读官方 status 和 instructions apply, schema 为 spec-driven、CLI 状态 ready、0/24. 已读取 CLI 返回的全部六份 contextFiles 及当前项目 context; operationGuidance 为空, 没有与已采纳合同冲突的附加指令. ready 表示规划/跟踪文件齐备, 不是任务 1.1 的原生前置通过.

原生 list 显示 establish-window-input-runtime 仍为 15/16, 原任务 4.1 未勾选, Developments D01 出口未通过. 本次显式加载 Windows Forms 后查询 Screen.AllScreens, 实际为 1 个活动显示器; 不从该查询推断当前 DPI 或像素尺寸. 初始未加载该类型的查询无效, 其输出不作为显示器数量证据. CLI 原始输出和有效查询结论保存于 ignored build/artifacts/establish-opengl-resource-uploads/apply-entry-*.

按任务 1.1、design 1 及当前前置要求, 尚不能进入后续实施. 已提出仅覆盖本 change 既定 T04 上传、必要即时列表/fence/在途保活/关闭、T05 最小读回的限定入口选择, 待维护者明确决定后再同步五类规划/开发文档. D01 原验收、完整 T05/D03 与阶段出口不随该选择关闭. 本项及所有后续任务保持未完成, 未修改生产代码或 Architecture、未运行本包 C++/GPU 检查、未同步/归档/提交 Git.

## 限定前置授权 (2026-10-28)

维护者随后明确答复“同意限定推进，修订规划后继续（推荐）”. 已将本 change 的 T04 上传、必要即时列表/fence/在途保活/Host 关闭和 T05 最小读回独立例外同步 proposal/design/spec/tasks/Developments. 原因、受影响 Host/输入回归及恢复普通前置的退出条件见 design 1. D01 4.1、完整 T05/D03、呈现和 D01/D02 总出口保留未完成; 前一节保留授权之前的真实核验事实, 不改写原 change 或历史归档.

1.1 验证: 同步后的官方 strict 7/7 通过、当前 change 无提示, 973 个本地链接/锚点及 801 个受保护输入标识核对通过. 此项完成的是本次已授权的限定入口, D01 原出口仍未通过. 原始输出保留在 apply-entry-* 与授权后复核的 final-*.json/log, 不将本条作为后续 C++/GPU 完成证据.

1.2 验证: implementation-inputs.json 保存入口 Git/源码/固定依赖/五份主规格标识, CommandList-matrix.json 与 Device-matrix.json 分别含 61/68 项. 新 native/generic binary 配置通过, MSVC/SDK/CMake/CLI 见 configure/version 日志; generic Debug 构建及 12 组 CPU 检查通过. 未下载依赖或借用旧二进制.

## 实施验证记录 (持续更新, 2026-10-28)

2.1: 新列表完整 61 项声明/定义与 Device 68 项闭包已编译到正式 PhysaraRHIOpenGL. Debug/Release 的 UploadChecks list/EntryCases 检查完整拒绝矩阵、只读/继承查询、参数及新增 15 个入口逐个缺失、非法 GL_MIN_MAP_BUFFER_ALIGNMENT. 模块 CMake、Build、RHI 支持/来源说明及私有头随实现更新; generic 独立 Debug/Release 的 12 组 CPU 消费者通过. 具体每次源标识和命令见 ignored build/artifacts/establish-opengl-resource-uploads/runs.jsonl 及对应日志, 后续组的新增正向行为继续分别验收.

当前真实环境为 Intel(R) RaptorLake-S Mobile Graphics Controller, OpenGL 4.6.0 Build 32.0.101.6129, GL_MIN_MAP_BUFFER_ALIGNMENT=64. Debug 和首轮 Release 的 UploadChecks 已分别通过列表/Buffer/Texture/容量/故障/Host Pending 组合; Texture 为 13 格式 x 7 维度、702 次选定子资源实际读回, 不把受控 Pending/失败视为自然 GPU 状态. 扩大后的 worker/失效/边界检查仍按各任务完成后勾选.

Open OOM 检查发现并修正 tracker/packet 默认 vector 构造在当前 MSVC Debug proxy OOM 下 noexcept terminate 的问题, 改为 count=0 的可捕获构造, 未修改通用状态算法. 对应 design 与来源差异已同步. 初版 fixture 的 Validation 回调计数假设有误, 已改为在 native 可接受时仍返回 0 的行为证据, 验证包装状态不恢复. 这些失败记录与成功检查分开保存.

工具边界: MSVC 19.44.35228 在不同 STL 位置间歇报告 C1001, 串行构建仍可出现; 未放宽 /WX、语言或 Release 优化, 后续同配置 native 生产与检查目标成功编译. 不声称已定位工具链原因. 一次构建调用自动审批超时未执行, 按工具允许重试后成功; 一次回归 fixture 的生成 CMake 因 Windows 路径转义配置失败, 改用正斜杠输入后配置成功. 所有失败均不作为通过证据.

2.2-2.3: FinalChecks.py 顺序完成 Debug/Release build/final-native, Open 的全部工作包/Debug proxy 分配点、owner/current/重复状态、ID 溢出、64 包上限及 64 项整批通过. ListCases 和 UploadLifetimeCases 覆盖 raw/Validation 混用、委托后 0 不恢复、内部多 lease/唯一 wrapper 计数、额外 caller/Device/Validation/list/FBO/Sampler owner, 零分配预检与 stopped-worker 交接. ResourceChecks 的既有 owner/外来实现回归同时通过.

3.1-3.4: Common tracker 正常/未知/非法 range/分配失败、手动缺 Commit、Close/放弃不发布、真实 fence/flush 后 metadata、实际 glIsSync 检查、递增/溢出票据、整个批次拒绝和 reverse batch 不重放通过. Pending/WAIT_FAILED/临时 sync 清理错误单独受控; 实际成功 sync、零超时 GC、5 秒显式 idle 与 orphan 内部 fence 退休均经真实 context 执行. standalone root 和 Host root 的预检、空包 CPU 清理、Draining/Stopped 与幂等退出通过; 无普通路径 glFinish, 仅无可靠完成的退出尝试它.

4.1-4.4: BufferUploadCases 的公共 Write/Close/Execute + actual readback 覆盖非零 offset、64 KiB+1、256 KiB 两侧和重复覆盖, 源返回即覆写/销毁. NativeAllocationEdges 在实际 name/storage/map 取得后注入 GL 错误, NullMap 单独标记; 当前写准备 OOM 不触碰毒指针或目标, 已发前缀保活. CapacityCases 实际取得 64 MiB coherent/persistent storage, 检查 flags/alignment/mapping、64 物理包、每包 64 MiB logical bytes 及 4096 写入/不同资源上限. diagnostics 对应实际单位, Pending 不复用, Complete/GC 后计数归零并恢复预算. 该口径不包含 driver 内部分配或全设备显存.

5.1-5.4: Debug/Release 各 702 次实际 subresource 读回通过, 覆盖 13 种颜色格式、7 种非 MS 维度、非对称尺寸、mip/array/cube-face/cube-array/3D, 数据含 encoded sRGB、整数、已知可表示 half/float. 深度/模板与两种 MS 分配保留而上传拒绝, short/overflow row/depth footprint 在读源前拒绝. 非整 texel padding/单行零 pitch/单 depth 忽略 depthPitch 通过. 全部 unpack 字段由 ClearState 后重建, 实际 PBO 名称在第 2 次分配时重用且绑定/内容正确. swizzle 与独立 sampler 参数不变, 实际 PBO texture 写入后注入错误无 ticket/metadata 假成功.

6.1-6.3: raw/Validation 在 Open/Closed/Submitted 三种状态分别 worker 最终 Release, 禁分配且零 GL, 等待后目标精确一次删除. 同资源跨三包实际上传、提前释放 caller 及 FBO 依赖通过. foreign context 对本包全部 48 个 required 入口实际地址逐一匹配, 外来 Buffer/Texture/list 在发射前拒绝, 恢复后两设备可上传/完成/停止. Host 外部 owner 拒绝保持 Ready/输入/claim; 仅内部在途可关闭, 受控 Pending 保留全部 owner/context, 停止 GetDevice/输入, 实际 retry/rebuild 与旧 Host 隔离通过. fence-null/flush/wait 故障保留前缀与首错, 可用 context 仅退出一次 finish; 再次失败、受控 context loss/current 恢复失败后 CPU-only, 不假报完成. 实际在途清理的 GLFW 次错/日志失败也已覆盖.

上述任务的运行入口为 FinalChecks.py 与 Implement.py, 原始逐配置日志、预期非零退出和源码/fixture SHA 在同目录; 这些原生/受控结果分开记录. 生产独立 Debug/Release 与 regression-complete 构建已通过, 完整集成运行、最终文档核对和 verify/Code Review 继续作为 7 组验收, 此处不代勾.

6.4: 在真实 GPU 工作已取得后, 禁 CPU 分配执行 worker 交接、纯 CPU 预检、退出 finish/退休, 所有 packet/chunk/内部 holds/删除记录清空. 实际完成后目标精确一次删除; stopped Device 的 worker 最后释放不访问 GL/回调. 两配置隔离 Pending 析构返回 87、外部 owner 析构返回 86, 既有 wrong-thread 析构及 detach 致命合同返回 91, 均为预期结果. 没有通过放宽断言或忽略退出路径取得通过.

7.1: FinalChecks.py、IntegrationTail.py 和 Debug 的 Integration.py regression/runtime 顺序检查通过. Debug/Release 各 generic 12 组正例与 10 组有实际编译诊断的负例、T03 全量、6 个 native 私有/公开独立头消费者、Core/Host/输入 7 组、Runtime 37 案例及产品 help=0/unknown=2/hidden=0 通过. 独立 product 与 regression-complete binary 均重新配置构建, 第一方 /W4 /WX 无新增警告. File API 验证 generic 无 native 依赖、native 只依赖 RHI/glad、Platform 私有消费及唯一产品入口; dumpbin 验证两配置四个生产库无检查宏对应符号/测试逃生入口. 61/68 方法与全部拒绝/输出合同均核对; 不编译 NVRHI reference.

7.2: CMake/Build 只维护实际目标/入口, RHI README 维护支持/来源差异, native/Host 头维护方法与借用合同, Developments 记录 T04/前置及 T05/D03 子集衔接. 已消除“没有列表/提交”等过期当前描述, 规划与入口旧记录明确为当时事实. 975 个本地链接/锚点、MIT/固定来源、PascalCase/具名文件作用域、修改范围格式及 git diff --check 通过. 788 个不在本包编辑范围的既有文件原始字节标识保持, 含 Architecture、六个公共 RHI 头、主规格、归档、D01 原 change、其余 Common/Validation、固定第三方与运行入口.

7.3: 已按 openspec-verify-change 的完整性、正确性、一致性逐项审查本 change, 17 项 ADDED/MODIFIED 要求、69 场景、14 设计决策均有实现/实际验证映射. 审查 source 消费时刻、lease/外部 owner、已发失败前缀、chunk 预算/复用、真实完成证明、orphan 和 Pending 关闭重试, 无遗漏的适用行为或平台范围内的未运行项. Debug proxy OOM、错误清理/预算诊断和重复 context/native 诊断已修正, packet/queue 上限统一为一项设备私有常量; 这些最后修正后的 FinalChecks.py 两配置全量、T03/头/致命合同及 RebuildProducts.py 独立生产/消费者重构建与 0/2/0 复验通过. 无关 generic/Runtime 模块源码未随这些修正改变, 其此前完整回归保持有效.

官方 pre 核验确认仅当前 7.3 核验任务待勾, 其余 23 项已满足, strict 7/7、current change 无提示、D01 仍 15/16. 本项完成后最后官方 status/apply/strict 与只读核验报告保存在 verify-final.json 及 verify-final-*.stdout/stderr.log; 任务事实、源码和审计标识由该报告交叉核对. Code Review 的职责规模评审见 design 14. 本包未发现遗留 CRITICAL/WARNING/SUGGESTION, 不将 CLI ready/all_done 或结构数量本身当成行为证明.

### 完整性与证据边界

| 核验维度 | 本包结论 |
|---|---|
| 完整性 | 24 项实施/验证任务, 17 项要求与 69 个场景均已核对 |
| 正确性 | 两配置真实上传/读回/完成/退出与相应 CPU/集成检查通过, 受控 Pending/失败单列 |
| 一致性 | 14 项设计与 Physara 命名/所有权/依赖/文档职责一致, 生产无检查入口 |

支持环境只覆盖本次 Windows x64/MSVC/Intel OpenGL 4.6 设备, 不声称跨平台/跨厂商或性能结果. 当前仅一个活动显示器, D01 原任务保持 15/16; 本轮独立限定前置例外不替代跨屏证明. 完整 T05/D03、Shader/Draw、公共 readback/staging、呈现和 D01/D02 总出口仍未完成. 没有同步主规格、归档或提交 Git; 后续按对应原生流程处理.

## 归档复审 (2026-10-28)

已按官方 archive 输入读取 context、状态图和 delta 路径, 无附加 operationGuidance. 归档入口时为 spec-driven、4/4 artifacts、24/24 tasks, 代码和依赖仍基于 4ed14e0b4acad66a68a4f278d819b93a4a3c9f91; 尚未同步或移动 change. 本节追加当时实施验证没有覆盖到的复审发现, 不用旧通过记录代替新回归.

复审补充的 ArchiveReviewCases 在真实上传/提交后控制等待返回. 修正前 Debug 的 archive-loss 观察到 drain 等待确认 GL_CONTEXT_LOST 后仍有 2 次 GL 入口调用, archive-deadline 在第一条真实 fence 延后完成、第二条受控 Pending 时总等待 8.01454 秒. 两项回归分别以 9/10 失败, 原始失败日志和源/fixture 标识保存在 archive-loss-before-fix.log、archive-deadline-before-fix.log、archive-repros-before-fix.json 及 runs.jsonl; 运行标签的当前日志随复验更新. 因而重新打开 3.3、6.3、7.1、7.3, 不带已知缺陷归档.

修正统一 WaitSync 的显式截止点, 一次 collector/drain 的所有记录共享五秒预算且单次等待不超过剩余时间/1 ms; collector 返回后重新检查实际 context 健康状态, loss 后仅 CPU 退休, 不沿用旧健康快照发 finish. No-work orphan 的退休若发现故障也立即停止继续收集. design 同步原子交接的实际内存顺序/owner 回收条件及 unpack 设置操作组的检查边界, 没有新增行为范围或修改 Architecture. 修正后验证与最终审查结果在本节继续记录.

清理错误复核还发现 unmap 已报告实际 GL error 时又补报一次“数据损坏”, 原 Debug archive-unmap 回归观察到 2 次诊断并以 11 失败. 已保留 archive-unmap-before-fix.log 与对应源/fixture 标识, 修正为只有没有实际 GL error 的 GL_FALSE 才报告内容失效, 不替换或重复底层首错.

修正后的两配置 FinalChecks.py 原生全量/T03/6 个独立头/隔离进程退出合同通过, 各有 702 次纹理子资源读回; 三个新增归档回归分别观察到 loss 后 0 次 GL、共享等待 5.00003/5.00001 秒和单次 unmap 诊断. 这些时间仅为受控期限检查, 不作性能结论. ArchiveFinalChecks.py 已完成两配置 generic 12 正例/10 编译拒绝、Core/Host/输入 7 组、Runtime 37 案例及产品 0/2/0. 最后诊断修正后再次完成 FinalChecks.py 与 RebuildProducts.py; 未改变的 generic/Core/输入/Runtime 模块源保持其完整回归有效. 重新确认源码/依赖、命名/许可、生产无测试符号和 17 要求/69 场景的完整性/正确性/一致性后关闭重开的任务; D01、完整 T05/D03 及阶段出口仍按既定边界保留.

### 同步与归档结果

维护者明确选择“同步主规格后归档（推荐）”. 已复用本次唯一 specs 指令快照的空规则集, 新建 opengl-resource-uploads 主规格的 14 要求/54 场景, 同步 opengl-device-resources 的 2 要求/5 场景及 native-window-host 的 1 要求/10 场景. 其余三个主规格保持原始字节不变; 被修改要求之外的内容、既有 Purpose 及 Host 原七个场景均保留. 同步后从官方 artifactPaths 的全部三能力重新比较, 均已同步且无剩余差异; 主规格校验与移动前 strict 8/8、24/24 tasks 通过.

change 已移动至 2026-10-28-establish-opengl-resource-uploads, .openspec.yaml 随目录保留. 仅修正本次新归档自身的相对链接及当前 Developments/RHI README 的引用/归档状态, 既有历史归档和 Architecture 未改. 归档后最终校验和保护结果见 ignored build/artifacts/establish-opengl-resource-uploads/archive-final-verification.json 及对应 CLI 原始输出. 未提交 Git, D01 仍为 15/16, 完整 T05/D03 与阶段出口保留未完成.
