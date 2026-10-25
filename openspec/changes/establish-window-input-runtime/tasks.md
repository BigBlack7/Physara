# Tasks

范围为 [D01-T03/T04](../../../Docs/Developments.md#d01-t03), 依赖已完成的 native-window-host. 实施方法以 [design](design.md) 为准. 清单按实际实施和检查结果更新; 只有对应行为、检查和随附说明实际通过后勾选, 原生环境不足的验收保持未完成. 临时源码、命令和原始输出留在 `build/artifacts/establish-window-input-runtime/`, 不新增仓库测试框架或第二产品入口.

## 1. 输入值合同与独立累积算法

- [x] 1.1 建立 `Platform/Input.hpp` 的逻辑/像素/scale 值、Key/MouseButton/Action/Modifier、固定 InputSnapshot、事件和 HostRequest 及 HostFrameInput; 直接在公开头写明单位、线程和 span 寿命. 验证仅消费此头的 C++20 程序无需 GLFW/GLAD/RHI 头, Unknown/Count 不能作为合法索引, 事件大小 <=64 字节且复制/赋值不抛出.
- [x] 1.2 实现私有 InputState 的两个 4096 事件 bank、实际窗口事实初始化、只在值变化时递增的 metricsRevision、受检 sequence、发布交换及 interval reset; 同步说明其单写/存储预算. 用 CPU 检查验证初始 revision=1、空 poll 递增、held 保留、edge/delta 只发布一次、旧 published bank 在 pending 写入时不变、capacity/capacity+1、两类 uint64 临界均不回绕且失败批次不成功发布.
- [x] 1.3 实现 key/button/Unicode 输入转换, 覆盖 8 个鼠标按钮、Unknown/scan code、Press/Repeat/Release、modifier 和 Unicode scalar 规约; 对应实现边界注释随代码提交. 检查同区间 press+repeat+release 的顺序与双边沿、未知键不越界、中文与 non-BMP 字符保序、surrogate/越界 scalar 被有界失败拒绝.
- [x] 1.4 实现逻辑光标/滚轮的有限数值累积、focus/模式切换基线、held 释放及待解除捕获标志; 同步记录焦点与原生 release 的分工. 检查 fractional 输入、累加溢出/NaN/Inf、失焦同区间 edge 保留、运动归零、首样本零 delta、无自动重捕获, 且不伪造重复物理 release 事件.
- [x] 1.5 把实际输入算法源加入 Platform target, 在临时 fixture 构建并运行以上 CPU 检查的 Debug/Release; 核对构造后的正常记录/发布在禁止动态分配时仍可工作, 两 bank 预算成立, 公共消费者和第一方警告检查通过. 不把这些 CPU 结果写成原生输入验收.

## 2. DeviceHost 原生输入与安全操作

- [x] 2.1 将 InputState 原位纳入稳定 DeviceHostState, 实现 GlfwInput 的私有码表、12 类回调、原生窗口事实初始化/协调和受检 LOCK_KEY_MODS, 在 InputSetup 完成后才 Ready; 同时接入部分安装解绑和既有回滚, 更新 Architecture 对实际源文件职责的说明. 用真实窗口对照初始窗口事实, 在输入准备各取得点受控失败并正常重建; 回归原宿主创建失败、32 次关闭和进程错误回调恢复, 确认 user pointer 从未转移到第二 owner.
- [x] 2.2 实现 `PollEvents`、操作阶段/elapsed、owner/Ready/Stopped/重入检查、首错与终止输入态、双 bank 发布和安全捕获释放; 同时把显式 Shutdown 的重入拒绝与析构致命保护纳入原释放链, 从 ReportError 抽取供诊断与 Runtime 共用的无分配 DescribeHostError, 更新公开头中的宿主输入合同, 由 Docs/Build.md 提供入口. 验证正常空/非空 poll、pre-poll 事件保留、accepted/rejected poll 的借用边界、wrong-thread/Stopped/reentry 无原生副作用、故障后 Ready=false 且仍可关闭, 描述函数的拥有性/字节上限/native code 与 requestIndex, 回调内析构的隔离进程按合同终止.
- [x] 2.3 实现 `WaitForEvents` 的有限正秒预检、真实原生等待与 pending 协调, 同步记录它不发布/不缩短 span 的合同. 在真实宿主检查 wait 到达事件与超时后下一 poll 只发布一次, 旧 batch 不变, 零/负/NaN/Inf 在 Debug/Release 均先于原生断言拒绝; 记录实际时长但不宣称实时上界或性能提升.
- [x] 2.4 实现 `ApplyDeferredRequests` 的整批预检、Normal/Relative 与 close flag 的顺序调用/观测、0-based requestIndex、prefix 失败及捕获中失焦拒绝; 同步公开请求/错误说明. 检查实际 cursor/close 状态、无效 suffix 不产生有效 prefix 副作用、失焦捕获预检、模式转换首 delta、close 取消后再次请求、旧 batch 不变; native 调用失败与 cleanup 同时失败时保留主错误和已经产生的 prefix 效果.
- [x] 2.5 使用现有实际 ImGui GLFW backend 在临时原生 fixture 安装/转发/Shutdown, 不给生产 Platform/Runtime 增加 ImGui 依赖; 同步记录集成边界. 检查 key/char/focus/mouse 等在两个消费者各到达一次、稳定 user pointer 未覆盖、adapter 先恢复平台回调再关闭 host, 随后重建无旧目标; 明确这不验证 UI Draw 或完整光标控制.
- [x] 2.6 组合检查新的输入故障与生命周期: 在真实 host 上受控触发容量超限、非法回调值、计数耗尽、poll/wait/query/request native error 和附加释放错误; 只通过私有检查入口观察仍由 owner 拥有的 native 状态, 不解引用失效借用. 验证无成功半批、held/运动清理、安全解除捕获或明确关闭失败、首错/饱和计数有界、无重复日志, 故障 host 在 Shutdown 前仍拒绝第二宿主占用, Shutdown 幂等且新 host 正常恢复; 检查过程区分实际原生返回和受控注入.

## 3. 单产品入口与可观察进程结果

- [x] 3.1 在 Runtime CMake 建立唯一 `Physara` target, 实现同一 Main.cpp 的无分配参数解析、现有 Diagnostics/Host 装配、poll 后 close/limit 判定、暂停状态转换与 0.05 秒空闲等待、共同显式退出; 不添加 RHI/Editor 占位或空请求生产者. 同步 Docs/Build.md 的构建运行命令、五个选项和当前无渲染范围. Debug/Release 运行 default/native close、`--hidden --poll-limit=1`、`--hidden --poll-limit=3`、help、重复/未知/空值/符号/尾随/溢出选项及零/超 int 维度, 检查 0/2/1 返回码、limit 前无额外 wait, Main 无原生 include/第二入口.
- [x] 3.2 对已存在的实际入口补齐跨 owner 的进程失败检查, 只在 build 树临时构建加入受控分配/操作故障或观测, 不在生产参数增加注入开关; 同步失败输出与退出证据. 验证 Diagnostics/Host 创建、运行中输入失败、显式关闭、诊断写入/输出失败和原操作+cleanup 失败均保留主错误并以 1 退出, 错误描述不分配且不依赖最后日志, native/window 回调目标先于 Diagnostics 释放, 重复启动关闭成功; 校验 help/参数错误在 owner 创建前返回, 不假称未具备的无显示环境已经覆盖.

## 4. 跨模块验收与原生 verify

- [ ] 4.1 在完整 Runtime/Host 集成上完成 D01 的 Windows 验收: 真实 resize/快速 resize、最小化/恢复、focus/cursor、按键释放与 Unicode 提交、OS close 及取消; 有不同 DPI 显示器时实际跨屏并记录 logical/framebuffer/scale/revision 的独立变化. 核对零 extent/非 renderable 路径不创建伪目标且持续处理关闭, 区分真实 OS 输入、原生受控消息、回调/纯值注入; 缺少跨屏/焦点等条件的对应原生检查保留未完成, Unicode 可用实际 OS 提交验证而不扩展到 IME 组合界面, 不据缺失证据勾选 D01 总出口.
- [x] 4.2 从新 binary 目录离线 configure/build 全部 Debug/Release targets, 再以独立生产消费者运行窗口/产品入口; 用 File API/编译参数/链接与生产符号检查证明 C++20、现有 /W4 /WX /utf-8 /permissive-、Core/Platform/Runtime 单向依赖、原生头私有、检查宏/符号未进入产品, 无新增警告、NVRHI 构建或测试框架. 复查 Docs/Build.md 命令能复现, 原有 Core/宿主关键消费者无回归.
- [x] 4.3 对照 15 条新增要求和 40 个场景执行原生 verify 与 Code Review, 重点审查状态唯一来源、固定预算、借用/回调寿命、请求 prefix、线程/重入、错误/退出和零尺寸, 修正发现后只重跑受影响检查; 执行 `openspec validate --all --strict`、本地链接/锚点与 `git diff --check`. 将真实命令/环境/覆盖及未运行限制记入本 tasks, 核对此前各组随功能交付的头合同/Build/Architecture 一致, README 只保留项目介绍及文档导航, 按实际验收更新 Developments 的 T03/T04 和 D01 出口; 不提前勾选、同步主规格、归档或改写上一 change 历史.

## 规格与检查对应

| 能力要求 | 主要任务 | 证据类型 |
|---|---|---|
| Live window facts | 1.2, 2.1, 4.1 | 实际 getter 对照、revision/零 extent 算法、真实 DPI/窗口变化 |
| Ordered publication | 1.2, 2.2, 2.3 | 双 bank/空区间、poll 外到达、只发布一次及借用 |
| Key/text meaning | 1.3, 2.1, 4.1 | 有序边沿/repeat/Unknown、Unicode 值与实际提交 |
| Pointer/scroll units | 1.4, 2.4, 4.1 | fractional/有限数值、原生观察与单位边界 |
| Focus and motion reset | 1.4, 2.2, 2.4, 4.1 | held/edge、原生自动 release、capture/基线 |
| Close intent | 2.4, 3.1, 4.1 | OS close 不自毁、取消后重发、Runtime 决策 |
| Explicit request boundary | 2.4, 2.6 | 整批预检、顺序/prefix/index/失焦拒绝 |
| Callback integration | 2.1, 2.5, 2.6 | 实际官方 adapter、稳定 user pointer、逆序解绑 |
| Bounded input failure | 1.2, 1.3, 1.4, 2.6 | capacity+1、非法值/计数临界、首错/关闭/重建 |
| Idle event waiting | 2.3, 3.1 | 正秒预检、真实 wait/timeout/pending 与产品等待 |
| Operation preconditions/failures | 2.1, 2.2, 2.6 | owner/Stopped/重入/安装失败/native error 与释放 |
| One product entry | 3.1, 4.2 | 唯一实际入口、可见/隐藏同宿主与链接图 |
| Startup options | 3.1, 3.2 | 参数/尺寸/帮助及 native 初始化前的进程结果 |
| Event decisions and idle | 3.1, 4.1 | close/limit、暂停恢复、无 GPU/伪目标 |
| Ownership-ordered exit | 3.1, 3.2, 4.2 | 进程返回码、失败保留、逆依赖释放及重复启动 |

## 证据记录约定

每项实施时记录对应命令、构建配置、观察结果和原始输出位置. 原生矩阵记录 Windows/SDK/MSVC、固定 GLFW/GLAD、GPU vendor/renderer/version、窗口 visibility/debugContext 及显示器/DPI 条件. 临时注入只证明指定分支, 不扩大为真实硬件故障、物理输入设备或其它平台支持. 规划结构校验不作为代码或 D01 行为验收证据.

以下产品命令已实际执行, Debug/Release 均通过; 本机 SDK 选择为 10.0.26100.0, 普通使用者仍按 Docs/Build.md 的已安装 SDK 配置:

```powershell
cmake -S . -B build/artifacts/establish-window-input-runtime/product-build -G "Visual Studio 17 2022" -A x64 -DCMAKE_SYSTEM_VERSION=10.0.26100.0
cmake --build build/artifacts/establish-window-input-runtime/product-build --config Debug --clean-first --parallel
cmake --build build/artifacts/establish-window-input-runtime/product-build --config Release --clean-first --parallel
& ./build/artifacts/establish-window-input-runtime/product-build/bin/Debug/Physara.exe --help
& ./build/artifacts/establish-window-input-runtime/product-build/bin/Debug/Physara.exe --hidden --poll-limit=3
& ./build/artifacts/establish-window-input-runtime/product-build/bin/Debug/Physara.exe --width=0
```

前两个产品调用实际 exit=0, 最后一项实际 exit=1; Release 相同. 全部目标构建、外部生产消费者与符号/依赖结果见后面的实施记录.

## 规划检查记录 (2026-10-25)

已核对当前 Architecture/Developments、完整的两个既有主规格、宿主/错误/构建实现及固定 GLFW/ImGui 适配源码, 具体 API 依据与取舍记录在 design. 原生 status 为 4/4 规划产物完成, apply 指令解析为 ready、0/16 实施任务完成; 两份 delta 共 15 条新增要求、40 个 WHEN/THEN 场景. `openspec validate --all --strict` 的当前 change 与两个主规格 3/3 通过, 六份相关 Markdown 的 805 个本地链接/锚点通过, 规划文件无占位、尾随空白或冲突标记, `git diff --check` 通过.

上述为规划完成时点的记录: 当时只创建规划并更新 Developments 的待实施关联, Runtime/Main.cpp 与 Runtime CMakeLists 尚为空, 实施清单全未勾选. 当前实现和执行结果记录在下文及任务清单中. D01-T01/T02 既有完成状态保留, D01 出口不因规划完成而通过; 不修改其归档历史.

CPU 实施证据 (2026-10-25): InputChecks/InputPublicConsumer 在临时 fixture 的 Debug/Release 编译运行均通过, 无新增编译警告. 覆盖 KeyCount=120/8 按钮、双 bank 借用/空发布/边沿、Unknown、中文/non-BMP/非法 scalar、focus 与模式首 delta、fractional 数值、NaN/Inf/累加溢出、4096/4097 事件及 uint64 临界; 构造后禁止动态分配时正常记录和发布仍通过. 原始输出与 CPU 检查源在 build/artifacts/establish-window-input-runtime, 此为 CPU 合同证据, 非原生输入验收.

原生输入证据 (2026-10-25): WindowInputChecks 在 Debug/Release 均通过实际 GLFW callbacks、当前尺寸/像素/scale 对照、wait/borrow、整批预检与 prefix、关闭取消、wrong-thread/Stopped/reentry、14 个 InputSetup 受控失败及重建、三种原生处理错误及附加 cleanup、capacity/scalar/counters 故障、官方 ImGui key/char/mouse/focus 转发与恢复、实际捕获/最小化/焦点转移/恢复及捕获中失焦拒绝. 两个 --fatal-reentry 隔离进程按预期 exit=91. 原有 CoreChecks/NativeChecks 两配置通过, 后者每配置 32 次真实生命周期与原有失败矩阵. 日志为本轮 build/artifacts 下 *-Debug|Release-run.log. 私有注入不等于自然硬件故障; 本机当前仅枚举 1 屏, 150% scale, CROSS_DPI_VERIFIED=0, task 4.1 的跨屏验收仍未完成.

Runtime 实施证据 (2026-10-25): RunRuntimeMatrix.ps1 对 Debug/Release 各完成 20 项 CLI 检查、3 次可见窗口实际 resize/iconify/WM_CLOSE 退出和 14 项实际 Main.cpp 隔离进程检查. 后者包含分配失败、poll/wait/shutdown、主错误+cleanup、诊断写失败、持续禁止 C++ new、底层 stderr 句柄写失败、异常及 owner 创建前 help/参数拒绝. poll-limit=3 的观测为 poll=3/wait=2/Shutdown=1; diagnostic 分支实际记录 writeFailed=1. 返回码均符合 0/2/1 合同. 原始记录为 runtime-cli-*.json、runtime-process-*.log、runtime-faults-*.json; 检查注入仅在临时链接同一 Main.cpp 的 fixture 中, 生产参数不含注入开关.

## 实施与复核记录 (2026-10-25)

环境: Windows x64, OS API 版本 Microsoft Windows NT 10.0.26200.0; Visual Studio 2022, MSVC 19.44.35228, Windows SDK 10.0.26100.0; 固定 GLFW 实测版本 3.5.1, GLAD 来源保持 ThirdParty/README 的 2.0.8 快照. 实际 GPU 为 Intel(R) RaptorLake-S Mobile Graphics Controller, GL 4.6.0 / GLSL 4.60, driver Build 32.0.101.6129. 可见/隐藏与独立 debugContext 检查均记录于 NativeChecks 原始日志, 不把 swap interval 请求当驱动等待行为测量.

临时 fixture 是本目录已有正式源的消费者, 用下列入口配置/构建, 每个 exe 都分别运行 Debug/Release. 它启用私有检查宏; 无检查宏的 product-build 与 production-consumer-build 单独配置和验收, 不能混称生产检查:

```powershell
cmake -S build/artifacts/establish-window-input-runtime -B build/artifacts/establish-window-input-runtime/fixture-build -G "Visual Studio 17 2022" -A x64 -DCMAKE_SYSTEM_VERSION=10.0.26100.0 -DPHYSARA_SOURCE=D:/MyWorkSpace/Physara
cmake --build build/artifacts/establish-window-input-runtime/fixture-build --config Debug --target CoreChecks InputChecks InputPublicConsumer NativeChecks WindowInputChecks RuntimeFaultChecks RuntimeProcessChecks Physara --parallel
cmake --build build/artifacts/establish-window-input-runtime/fixture-build --config Release --target CoreChecks InputChecks InputPublicConsumer NativeChecks WindowInputChecks RuntimeFaultChecks RuntimeProcessChecks Physara --parallel
& ./build/artifacts/establish-window-input-runtime/fixture-build/bin/Debug/InputChecks.exe
& ./build/artifacts/establish-window-input-runtime/fixture-build/bin/Debug/WindowInputChecks.exe
& ./build/artifacts/establish-window-input-runtime/fixture-build/bin/Debug/NativeChecks.exe
& ./build/artifacts/establish-window-input-runtime/RunRuntimeMatrix.ps1
& ./build/artifacts/establish-window-input-runtime/InspectProductionBuild.ps1
```

- CPU: 120 键/8 按钮合同、双 bank、边沿/顺序、未知键、Unicode scalar、fractional 光标与滚轮、失焦/模式首样本、零 framebuffer 及恢复、独立 scale、NaN/Inf/光标减法与滚轮加法溢出、4096/4097、sequence/revision 与拒绝计数饱和、构造后禁止分配均通过. 仅消费 Input.hpp 的独立 C++20 consumer 通过. 最后输出为 coverage-InputChecks-Debug|Release-run.log.
- 原生: WindowInputChecks 增补 WM_KEYDOWN/repeat/up 和 WM_CHAR 的中文/UTF-16 surrogate pair, 验证实际 Win32 消息分发后的 key/text 顺序. 通过真实 wait 分发字符并在下一 poll 只发布一次; 通过 20 次快速尺寸修改、关闭取消、可见窗口最小化/恢复和拥有窗口间焦点转移. 观察 GetClipCursor 的 client 限制与失焦后的 desktop 释放, held A 释放和无自动捕获. 查询准备/poll/wait 的实际 GLFW invalid-enum 错误、负尺寸/Inf 滚轮回调、全部 modifier 位、清理计数饱和、部分安装/故障/重建及官方 ImGui chaining 均通过. 最后输出为 coverage-WindowInputChecks-Debug|Release-run.log.
- 生命周期: 原 CoreChecks 及 NativeChecks 通过; 后者每配置 32 次真实创建/释放及原有失败矩阵, 输出含实际驱动和 visible/debugContext. --fatal-detach、--fatal-destructor 和 --fatal-reentry 的每配置隔离进程均按合同 exit=91, 原始输出为 review-fatal-*.log. 查询错误是受控非法 enum 引发的真实 GLFW 返回, 输入溢出/计数/分配为私有注入, 不称自然硬件故障.
- 生产: 新 product-build 从空 binary 离线配置, 全部 Debug/Release 目标及最终 clean-first 重建通过. 独立 production-consumer 仅链接 PhysaraPlatform, 实际运行 poll/wait/借用/close 取消/两次关闭与重建. 两配置生产 Physara 的 help、hidden limit、非法维度及每配置 3 次可见 resize/iconify/close 进程均通过. File API/编译参数证明唯一 exe+8 个静态库、C++20 与既有警告、Core 无 Platform 反向依赖, Runtime/消费者只继承 Platform/Engine include, 不继承 GLFW/GLAD/spdlog. dumpbin 检查生产 Core/Platform 无 Checks/ForChecks/Observer/注入符号, 无 NVRHI target 或新测试框架. 原始记录为 product-configure.log、production-final-*-build.log、production-reviewed-*-build.log、production-consumer-*.log、production-build-inspection.json 和 production-symbols-*.log.
- 工具异常: 一次增量编译出现 MSVC C1001 于 optional 头; 同一代码单目标 --parallel 1 重编通过, 随后的 Debug/Release 完整 clean-first 并行构建和受影响检查均通过. 未改标准库、语言/警告选项或关闭诊断. 记录该次现象, 不推断未确认的编译器根因.
- 环境缺口: 仅 1 个 2560x1600 活动显示器, dpi=144, scale=1.5/1.5; 本机窗口观测的 logical 与 framebuffer 可相同, 仍保持独立量, 不由 scale 相乘推造尺寸. CROSS_DPI_VERIFIED=0. 使用者确认目前单屏、以后才有第二屏, 因此 4.1 的真实跨屏部分未运行; 标准化程序内部坐标不构成这项原生证据. 零 framebuffer 是实际 CPU 算法输入检查, 不称自然窗口已产生零像素; Win32 消息是受控原生分发, 不称物理键盘或 IME 组合界面验收. 未覆盖其他 OS/GPU、自然设备故障或 UI Draw, 本次无性能结论.

## 要求实现与场景复核

下表与上方 15 行任务映射一一对应. 40 个场景均已核对实现和检查; Observe independent metric changes 的不同 DPI 原生切换部分未验证, 其余场景按 CPU/原生/进程的适用合同完成检查.

| 新增要求 | 主要实现定位 | 对应检查 |
|---|---|---|
| Observe independent live window facts | [InputState.cpp](../../../Platform/InputState.cpp):34,242; [GlfwInput.cpp](../../../Platform/GlfwInput.cpp):303,339 | 初始 getter、快速 resize、独立量/重复 revision、iconify、零 extent/恢复; 跨屏部分未验证 |
| Publish each input interval once in order | [InputState.cpp](../../../Platform/InputState.cpp):61,85; [DeviceHost.cpp](../../../Platform/DeviceHost.cpp):510 | poll 外/Wait 到达、双 bank、空区间、只发布一次及借用 |
| Preserve key transitions and committed text meaning | [InputState.cpp](../../../Platform/InputState.cpp):167,180,187; [GlfwInput.cpp](../../../Platform/GlfwInput.cpp):184 | press/repeat/release、Unknown、中文/non-BMP、8 按钮 |
| Preserve pointer and scroll units | [InputState.cpp](../../../Platform/InputState.cpp):196,222 | fractional、scale 独立、有限数值与差/和溢出 |
| Reset held and relative motion state at focus boundaries | [InputState.cpp](../../../Platform/InputState.cpp):140,268; [GlfwInput.cpp](../../../Platform/GlfwInput.cpp):588 | 持键失焦、实际 ClipCursor 释放、无重捕获及首样本 |
| Publish close intent without destroying the window | [GlfwInput.cpp](../../../Platform/GlfwInput.cpp):675 | WM_CLOSE 后窗口仍存活、请求取消和再发布、旧值不变 |
| Apply requests through an explicit safe boundary | [GlfwInput.cpp](../../../Platform/GlfwInput.cpp):675 | 整批预检无 prefix 副作用、顺序、index=1 的失败、捕获中失焦及附加 cleanup |
| Preserve callback ownership and integration order | [GlfwInput.cpp](../../../Platform/GlfwInput.cpp):454,612; [GlfwWindow.cpp](../../../Platform/GlfwWindow.cpp) | 官方 ImGui backend 各到达一次、user pointer 不变、恢复/解绑和重建 |
| Bound input retention and fail incomplete batches visibly | [InputState.cpp](../../../Platform/InputState.cpp):47,61,85; [DeviceHost.cpp](../../../Platform/DeviceHost.cpp):145 | capacity+1、非法值/计数、无半批、held 清理、无重复日志及关闭 |
| Process idle events without premature publication | [GlfwInput.cpp](../../../Platform/GlfwInput.cpp):656 | 正秒 wait 的原生字符/超时、span 和 sequence、非法值先拒绝 |
| Enforce operation preconditions and preserve failures | [DeviceHost.cpp](../../../Platform/DeviceHost.cpp):145,466; [GlfwInput.cpp](../../../Platform/GlfwInput.cpp):454 | owner/Stopped/重入、部分安装、原生泵/查询错误、cleanup 与可关闭 |
| Provide one real product entry | [Main.cpp](../../../Runtime/Main.cpp):101; [Runtime CMakeLists](../../../Runtime/CMakeLists.txt) | 唯一产品 File API、实际可见默认/隐藏入口 |
| Validate explicit startup options | [Main.cpp](../../../Runtime/Main.cpp):42,104 | 每配置 20 项参数/维度检查、无 owner 的 help/参数拒绝 |
| Separate event decisions from idle waiting | [Main.cpp](../../../Runtime/Main.cpp):136,148,157 | close/limit 先于 wait、limit=3 时 wait=2、最小化与零 extent 合同、无 GPU 调用 |
| Exit in ownership order with meaningful results | [Main.cpp](../../../Runtime/Main.cpp):119,175; [DeviceHost.cpp](../../../Platform/DeviceHost.cpp):466 | 正常重复进程、14 项失败/分配/输出检查、显式关闭及 owner 顺序 |

Code Review 已检查状态唯一来源、固定预算、值/借用寿命、回调 target、请求 prefix、owner/reentry、原始错误和清理、零尺寸及退出. 修正解绑阶段多个 native 错误的计数遗漏, 把原有饱和合并算法统一供窗口/输入释放; 补齐捕获拒绝的 cleanup 计数和两类输入的显式 modifier 转换. 受影响 CPU、原生、旧宿主与 Runtime 检查已重跑通过. Input.hpp/GlfwInput.cpp 的职责规模评审见 design 第 1 节, 未为行数机械拆分或增加第二 owner. 收尾复审另复现并修正请求期间回调超限被归为可恢复失焦拒绝的问题: 在原生返回与光标释放边界核对 InputFault, 统一 requestIndex 和 native 首错优先级, 立即停止输入, 保留附加 cleanup. 已在 Debug/Release 回归缓冲满时捕获失焦及同时 cleanup 失败的两种组合, 以及旧宿主/生产消费者和 Runtime 检查. 本轮未发现尚未处理的代码问题; 真实跨屏环境缺口单独保留.

## 原生 verify 结论

使用锁定版本的原生 status/instructions apply 取得 spec-driven 的四类 contextFiles, 逐份读取并核对以上定位, 未把 planning 的 isComplete 当成实施完成. 没有修改既有两个主规格、同步 delta 或归档.

| 维度 | 结果 |
|---|---|
| Completeness | 15/16 任务完成; 15/15 ADDED 要求均找到实现; 无 MODIFIED/REMOVED/RENAMED 要求. 4.1 尚缺真实跨屏验收 |
| Correctness | 15 项实现与 40 个场景逐项核对; 39 个场景的适用 CPU/原生/进程检查通过. Observe independent metric changes 部分验证, 不同 DPI 跨屏为 Not verified (当前只有一屏) |
| Coherence | 11 项设计决策、AGENTS 的依赖/风格/所有权/错误/退出规范与当前实现一致; 源码规模触发的职责复审已记录, 未发现遗留代码问题 |

CRITICAL (验收缺口) 1 项: 4.1 的真实跨屏 DPI 行为未运行. 在具备两个活动且不同缩放的显示器后, 使用同一正式宿主/Runtime 执行切换, 记录 logical/framebuffer/contentScale/metricsRevision、窗口响应和关闭, 再核对验收及 D01 出口. 统一内部坐标和注入比例不替代该证据, 本项保持未勾选. 没有额外 WARNING 或 SUGGESTION; 该场景的原生跨屏部分是本报告唯一未验证项, 当前不具备完整归档结论.

工具检查通过: openspec validate --all --strict 为 3/3 (当前 change+两个既有主规格), 仅有两条 >500 字符的 INFO 提示, 无结构错误; 受影响 Markdown 以及全部第一方当前/归档 Markdown 的本地链接/锚点检查通过; git diff --check、23 份修改/未跟踪文件的空白/冲突标记检查、变更 C++ 格式复核和最终构建日志无警告/错误检查通过. 未运行跨屏或其它硬件覆盖, 不把结构/链接检查当行为验收. 临时检查源码和原始输出在上述忽略目录中保留供本机复现, 不新增必交验证文档.

## 本次收尾交付

维护者确认暂时无法提供双屏环境, 接受保留 4.1 并自行提交本次实现; 当前进度仍为 15/16. 本次不提交代码、不同步主规格、不归档, D01 出口不勾选. 后续独立 change 可先推进 D02-T01 的限定前置见 design 的 closeout 节, 已同步到 Developments, 不扩展为取消其他前置.

根 README 已精简为项目介绍、展示和文档导航; Docs/Build.md 集中构建/运行命令与五项选项, 公开头继续承担 API 合同. Architecture 的文档归属表记录这一定位, 本 change 的 proposal/design/tasks 已同步引用, 不再把阶段实现/API/验收记录堆入 README.

收尾 Code Review 的组合边界复现记录为 closeout-repro-before.log: 缓冲满后捕获请求触发失焦回调, 原先误报 RequestRejected 且 Ready=true. 修正后 WindowInputChecks 在两配置分别验证无附加清理错误/同时清理失败两种情况, 均返回 InputOverflow、requestIndex=0、Ready=false, 捕获回到 Normal 且可 Shutdown. 最终记录为 closeout-WindowInputChecks-Debug|Release-run.log; 旧 NativeChecks 的 32 次生命周期及原失败矩阵也通过. 生产目标与独立消费者重建运行、生产符号/依赖检查、实际产品重复关闭和 Runtime 每配置 20 项参数及 14 项失败检查均通过, 输出为 closeout-*.log、runtime-cli-*.json 与 runtime-faults-*.json. 本轮未发现未修复的代码问题; 唯一保留的验收缺口是 4.1 的真实跨屏部分.
