# Tasks

范围为 D01-T01/T02. 勾选只表示对应实现和检查已有证据, 创建规划产物不构成完成证据. 临时检查放在 build 树, 使用正式实现, 不新增仓库测试框架或产品入口. 实际命令、环境和结果记录在相邻段落, 未运行的场景保持未勾选.

## 1. 最小 Core 与即时验证

- [x] 1.1 实现 `Identity<Tag>` 的 index/generation、无效值与受检代际递增, 在公开头说明同域和代际合同; 用临时编译检查证明跨域不可隐式转换, 并检查同槽换代、无效代和最大代溢出拒绝.
- [x] 1.2 实现有界自持错误上下文, 明确 native code 来源/缺失与源位置, 在实际 Diagnostics/宿主使用标准单调时钟和秒 duration; 检查临时描述销毁后的错误值、超长 UTF-8 截断、分配失败可构造错误及预 GLFW 的有限非负秒计时.
- [x] 1.3 实现显式 owner 的同步有界 Diagnostics 和私有 spdlog 适配, 固定 1024 条/2048 字节总 payload, 暴露 eviction/truncation/writeFailed 状态; 检查容量覆盖顺序、中文码点边界、自持快照、计数饱和与后端写失败, 并在接口说明线程/寿命和读取成本.
- [x] 1.4 接入 `PhysaraCore` 的 C++20、目标级警告及 PRIVATE spdlog, 保持 Core 不依赖 GLFW/RHI/Platform; 构建 Debug/Release 并编译仅消费 Core 公共头的临时消费者, 核对生成依赖与无新增第一方警告. 此时不单独宣称 T01 窗口消费链已完成.

## 2. 唯一宿主、原生启动与失败释放

Core 证据 (2026-10-25): 临时 fixture 的 `CoreChecks` 在 Debug/Release 均通过身份/UTF-8/计时/容量/自持快照/创建及快照首处分配 OOM/受控后端抛错/计数饱和/并发读写检查. `IdentityMisuse` 按预期报 C2664, 无其它警告; `CorePublicConsumer` 两配置均可编译且不需 spdlog/GLFW/GLAD 头. 独立 `product-build` 的 `PhysaraCore` 两配置无警告构建通过. 命令入口为 `cmake -S build/artifacts/establish-native-gl-host -B build/artifacts/establish-native-gl-host/fixture-build -G "Visual Studio 17 2022" -A x64 "-DPHYSARA_SOURCE=D:/MyWorkSpace/Physara"`, 随后 `cmake --build <binary> --config Debug|Release --target CoreChecks|CorePublicConsumer`, 运行 `<binary>/bin/<config>/CoreChecks.exe`; 原始 configure/build/run/负例输出保留在该临时目录. MSBuild 的 SDK 探测需正常进程权限; 此处 OOM 证据仅覆盖入口分配, 更深分配及返回路径由下方归档复核补齐. 这些检查尚不构成窗口消费链完成.

- [x] 2.1 定义 HostDesc、HostInfo、HostError 和创建/显式关闭结果, 实现独占稳定 DeviceHost/Impl 边界及私有 GlfwAccess, 在公开入口写明 main thread/owner thread/借用合同; 编译无 GLFW/GLAD include 的公共消费者, 检查非法尺寸、窄化范围、内嵌 NUL 和无半初始化成功值.
- [x] 2.2 实现唯一占用、attempt generation、受限错误捕获目标和 owner 线程检查; 用临时边界检查验证第二次占用拒绝、代际溢出拒绝、只有当前 owner 可以释放占用及错误线程检查, 核对这些拒绝均先于原生调用, 所有进程状态持锁区间不调用原生/日志/未知回调; 真实活跃宿主的组合场景在 3.1 核对.
- [x] 2.3 实现 GLFW 错误 callback 安装/有界复制、初始化、GL 4.6 Core 窗口、稳定 user pointer 和 current 核对, 与已取得状态的回滚同时交付; 用真实未编译平台触发 init 失败、私有检查配置触发 window 创建失败, 核对原始错误、callback 恢复及随后正常初始化, 并恢复 fixture init hints.
- [x] 2.4 实现 current 后的 GLAD resolver、必要入口检查及实际版本/profile/default framebuffer/基础 limits 查询, 形成自持 HostInfo; 在真实隐藏 context 上核对原生值, 并检查缺失入口、不足版本/错误 profile/查询错误、linear 编码及快照在关闭后可读, 分别标注原生观测和受控输入证据.
- [x] 2.5 实现 swap interval 请求和秒耗时封存, 仅在全部前置成功后发布 Ready; 检查请求值与实际呈现能力表述分离, 捕获 swap interval 阶段失败并验证回滚, 启动/失败只在完整上下文位置记录一次.
- [x] 2.6 完成正常/部分初始化/析构共用的幂等关闭, 保持错误捕获活到 terminate 后恢复 callback, 清除私有借用并只释放本 owner 占用; 用各取得阶段的受控停止检查逆序释放、主错误不被 cleanup 错误覆盖、重复关闭、无悬空 callback 以及释放成功后的真实重建. 对无法安全解除 current 的终止分支采用隔离进程检查并明确其原生覆盖限制.
- [x] 2.7 接入 `PhysaraPlatform` 与 root/module CMake, Core 为公开类型依赖而 GLFW/GLAD 为 PRIVATE, 第一方使用目标级 C++20/警告; 构建受影响 Debug/Release, 核对 Engine 无反向依赖、NVRHI 不进入构建、生产配置不编译私有故障入口, 在 README 同步当前库能力及尚无 Runtime 程序的使用边界.

## 3. 跨模块集成验收

原生证据 (2026-10-25): Windows 10.0.26200.0、MSVC 19.44.35228、Windows SDK 10.0.26100.0、CMake 3.30.0-rc4. 真实 WGL 返回 Intel(R) RaptorLake-S Mobile Graphics Controller, GL `4.6.0 - Build 32.0.101.6129`, GLSL `4.60 - Build 32.0.101.6129`, GLFW 3.5.1, GLAD 固定 2.0.8. 实际默认附件为 SRGB, max texture=16384 像素、SSBO bindings=16、UBO offset alignment=16 字节; 这些是创建期观测, 不是 RHI 能力验收.

`cmake --build build/artifacts/establish-native-gl-host/fixture-build --config Debug|Release --target CoreChecks NativeChecks PlatformPublicConsumer CorePublicConsumer --parallel` 及两个配置的全部检查程序通过. `NativeChecks` 每配置完成可见/隐藏窗口、独立 debugContext 与 swap interval=0/1 请求、32 次生命周期、HWND 释放、线程/占用/代际、旧 Stopped owner、sentinel 恢复与错误文本自持检查. 各已取得阶段停止、resolver 拒绝和观测值替换是受控注入; X11 未编译平台 init 失败、不可能的 99.0 context、受控非法查询的 GL_INVALID_ENUM、无 current context 的 swap interval 失败是实际原生调用结果, 每次均验证恢复后正常创建. Linear 编码在本机通过观测值注入验证, 不声称实际获得过 linear 窗口. `NativeChecks --fatal-detach` 与 `--fatal-destructor` 在 Debug/Release 的隔离进程均按预期终止 (检查处理器 exit=91); 前者注入不解除 current, 并非本机自然发生的驱动解绑失败. 原始输出为临时目录中的 `corechecks-*-final.log`、`nativechecks-*-final.log`、`*--fatal-*.log` 与各 build 日志. 此轮不覆盖输入/DPI、RHI、图像编码或 GPU 提交/寿命.

- [x] 3.1 在同一正式宿主上完成可见/隐藏创建、32 次创建关闭、保留旧 Stopped owner 时重建、第二宿主拒绝、worker 显式关闭拒绝和 sentinel callback 恢复的组合检查; 核对本轮窗口实际释放及每个失败后的正常重建, 记录 OS/工具链/驱动返回字符串、依赖版本、visibility/debugContext 和实际命令.
- [x] 3.2 从新 binary 目录完成离线 configure 和 Core/Platform 的 Debug/Release 构建, 编译公共消费者并核对完整 target/include/link 关系; 确认无新增第一方警告、无测试框架/第二产品入口/源码树生成物, 临时故障检查不改变默认生产构建.

构建证据: `cmake -S . -B build/artifacts/establish-native-gl-host/final-clean-build -G "Visual Studio 17 2022" -A x64 "-DCMAKE_SYSTEM_VERSION=10.0.26100.0"` 成功, 对该新目录分别运行 `cmake --build <binary> --config Debug --parallel` 和 Release, 八个静态库全部通过. CMake File API 的可编译 target 图证实 Core 没有其它第一方依赖 (spdlog 是其私有 INTERFACE 头库), Platform 的实际依赖为 Core/glfw/glad; 两个配置的第一方编译参数均含 C++20、/W4 /WX /utf-8 /permissive-, 无私有检查宏. 生产库 dumpbin 符号无 DiagnosticsChecks/DeviceHostChecks/FailingSink. 独立 `production-consumer` 以同样 configure/build 方式构建不启用故障入口的 HostSmoke, Debug/Release 各完成 4 次真实创建关闭; 其 include 路径仅 Platform/Engine, 不含私有第三方头. `clean-*.log`、`production-consumer-*-build.log` 与最终 fixture build 日志均无编译/CMake 警告. 结果和符号输出留在临时目录; Runtime/Main.cpp、RHI/Editor/Runtime CMakeLists 仍为空, 没有第二产品入口或测试框架.
- [x] 3.3 按本 change 全部规格场景核对实际实现与设计, 执行原生 verify 对照、`openspec validate --all --strict` 与 `git diff --check`, 将缺失证据明确记录; 只有窗口消费链和全部对应检查通过后更新 Developments 的 D01-T01/T02 及其 change 链接, T03/T04 和 D01 总出口保持未完成, 不自动归档或改写历史.

实施核对 (2026-10-25): 按原生 openspec-verify-change 的完整性、正确性、一致性三维核对, 14/14 任务、8 条新增要求和 22 个场景均有实现与上述检查证据, 未发现 Critical/Warning/Suggestion. `openspec validate --all --strict` 的当前 change 与主规格 2/2 通过; 修改文档的 816 个实际本地链接/锚点通过 (忽略注释与示例代码中的占位路径), 23 个交付文件无尾随空白/冲突标记, `git diff --check` 通过. Developments 仍为 113 个工作包, 仅 D00 原有 12 个清单项和本轮 T01/T02 的 5 项勾选, T03/T04 为 0 项; 延后模块/入口仍为空. 上述为实施阶段的核对; 归档前 Code Review 的发现、修正及更完整故障检查见下方, 未改写既有归档历史. 原生环境与注入覆盖边界按上方说明, 不扩展到其它硬件或后续功能.

规格到证据的核对:

| 要求 | 实现落点 | 场景证据 |
|---|---|---|
| Verified native context | [GlfwWindow](../../../../Platform/GlfwWindow.cpp), [OpenGLDeviceHost](../../../../Platform/OpenGLDeviceHost.cpp) | 可见/隐藏真实 context、非法初始配置、入口拒绝及版本/profile/双缓冲验证, 3 场景 |
| Owned observations | [HostInfo](../../../../Platform/DeviceHost.hpp), [环境查询](../../../../Platform/OpenGLDeviceHost.cpp) | 实际值对照、linear 观测注入、自持快照在关闭后可读, 3 场景 |
| Exclusive owner/thread | [占用与释放](../../../../Platform/GlfwWindow.cpp), [Shutdown](../../../../Platform/DeviceHost.cpp) | 第二宿主不变更 current/callback/GLAD, worker 显式拒绝, 2 场景 |
| Contextual failures | [ErrorContext](../../../../Engine/Core/Error.hpp), [工厂及错误捕获](../../../../Platform/DeviceHost.cpp) | 临时文本、各取得阶段失败、cleanup 与日志失败不覆盖主错误, 3 场景 |
| Repeatable release | [ReleaseNativeState](../../../../Platform/GlfwWindow.cpp) | 32 次循环、失败重建、sentinel/原生 HWND、旧 Stopped owner, 4 场景 |
| Distinct identities | [Identity](../../../../Engine/Core/Identity.hpp) | C2664 负例、同槽换代、最大代拒绝且无原生副作用, 3 场景 |
| Monotonic seconds | [Diagnostics](../../../../Engine/Core/Diagnostics.cpp), [创建计时](../../../../Platform/DeviceHost.cpp) | 成功与预 GLFW 失败的有限非负秒数, 1 场景 |
| Bounded diagnostics | [Diagnostics](../../../../Engine/Core/Diagnostics.cpp), [BoundedText](../../../../Engine/Core/Error.cpp) | 1024 条覆盖、2048 字节 UTF-8 截断、后端抛错可见, 3 场景 |
## 归档前复核 (2026-10-25)

Code Review 发现并修复 1 项 P1 问题: 原日志后端及快照返回的 MSVC Debug STL noexcept 构造/移动在部分内存失败点直接终止进程, 原有首处分配检查未覆盖这些位置. 隔离进程复现在 Create 的 7 个位置、ReadRecent 的 2 个位置以检查处理器 exit=9 终止. 先更新 design 第 7 节, 再将私有后端收敛为固定 spdlog circular_q 的有界记录保留, 快照结果使用可捕获分配失败的原位复制. 公开接口、8 条行为要求及 22 个场景不变, 未修改第三方源码或关闭 Debug 迭代器诊断.

修正后 `AllocationProbe` 对 Create、普通/空/满容量 ReadRecent 的分配序号 0..63 逐个注入 bad_alloc, Debug/Release 共 512 个隔离进程全部成功退出. Debug 实际出现的创建/普通快照/空快照/满快照分配数为 4/4/2/4, Release 为 3/2/0/2; 每个实际分配点均返回 OutOfMemory, 超过实际分配数时正常成功. CoreChecks 另在持续禁止分配时验证普通有界写入成功, 并检查非法级别与受控保留失败不改变覆盖计数. 该结果证明本次受测入口与配置, 不扩大为任意后续代码的内存失败保证.

重跑 `cmake --build build/artifacts/establish-native-gl-host/fixture-build --config Debug|Release --target CoreChecks NativeChecks CorePublicConsumer PlatformPublicConsumer AllocationProbe --parallel`, 全部通过. 两配置的 CoreChecks、NativeChecks、两个公开头消费者和独立生产 HostSmoke 均返回 0; NativeChecks 各完成 32 次真实创建关闭, `--fatal-detach` / `--fatal-destructor` 的四个隔离进程均按预期 exit=91. 新 `archive-review-build` 从根目录离线 configure, Debug/Release 的八个静态库全部干净构建通过, 新 configure/build 日志无编译、链接或 CMake 警告. 生产 Core/Platform 四个库的符号不含故障检查入口. File API 复查 Core/Platform 依赖方向、C++20 和 /W4 /WX /utf-8 /permissive-; 生产 HostSmoke 只需 Platform/Engine 公开 include 路径.

原始命令输入与结果在忽略的 `build/artifacts/establish-native-gl-host/` 中: `allocation-probe-before.json`、`allocation-probe-after-Debug.json`、`allocation-probe-after-Release.json`、`archive-*-build.log`、`archive-*-run.log`、`archive-NativeChecks-*--fatal-*.log`、`archive-symbols-*.log`. 干净构建命令沿 README, binary 参数为 `build/artifacts/establish-native-gl-host/archive-review-build`, 固定本机 SDK 参数为 `-DCMAKE_SYSTEM_VERSION=10.0.26100.0`; 该路径/SDK 不是产品配置的唯一值.

| verify 维度 | 归档前结果 |
|---|---|
| 完整性 | 14/14 任务完成, 8/8 新增要求具备实现 |
| 正确性 | 22/22 场景逐项核对, 原生结果与受控注入明确区分, 内存失败复核通过 |
| 一致性 | 所有权、依赖方向、线程、回滚和有界状态符合 design; 发现的 P1 已关闭, 无未解决的 Critical/Warning/Suggestion |

用户已明确选择同步主规格后归档. `native-window-host` 的 Purpose、8 条要求和 22 个场景已同步到主规格, 与 delta 逐条一致; `openspec validate --specs --strict` 两项主规格通过. 当前复核仍只覆盖所记录的 Windows x64/MSVC 与 Intel GL 4.6 环境, linear 编码及故障门槛仍按受控注入解释; 输入、DPI、RHI、渲染图像与 GPU 提交/寿命不在本 change 范围. 归档前复核完成后沿原生 archive 流程移动本 change, 不修改已归档历史.
