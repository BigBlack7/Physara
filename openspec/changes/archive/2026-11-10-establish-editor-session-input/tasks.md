# Tasks

## 1. A 接入会话和原生回调

- [x] 1.1 在模块 CMake 接入 `PhysaraEditor` 和固定 `imgui` target。以受影响的正式 target 验证离线配置、编译和依赖可见性，确认 ImGui 与 GLFW 头只进入各自的私有实现，Engine 无反向依赖，正式工程从仓库输入可重建。
- [x] 1.2 实现 Session 的 context、CPU 字体 atlas 和私有 GLFW 适配寿命。用真实宿主上的一段按键和字符输入核对 Platform 与 UI 各接收一次、顺序正确且原生用户数据关联稳定。拆除 Session 后先让同一宿主继续发布，再重新接入并正常退出，以覆盖残留回调和重复接入风险。

## 2. A 建立面板和输入路由

- [x] 2.1 建立主窗口停靠区、固定面板表、类型化可空选择和受限动作入口。用一个 CPU UI 会话检查停靠后隐藏及重新显示、面板身份保持，以及无场景或无选择时禁用领域操作。检查点以真实布局及动作结果为准。
- [x] 2.2 按当前文本、模态、Gizmo、相机捕获、视口和快捷键作用域统一仲裁。以一次面板显隐动作核对菜单和快捷键共用入口，首次 Press 只生效一次，Repeat 不生效。用一组竞争输入核对文本/模态阻断、Gizmo 优先、有效捕获和单次滚轮业务归属。
- [x] 2.3 处理有序失焦、同批次焦点往返、暂停输入清理及相对光标模式释放。以代表性序列验证旧交互取消、暂停期输入不重放、恢复后新触发才可捕获，并沿宿主观察核对有效光标模式与新位移基线。

## 3. A 装配唯一 Runtime 并验证入口

- [x] 3.1 在 `Runtime/Main.cpp` 装配真实 Session，先判断 close/poll limit，继续运行时区分正常 UI 帧和暂停清理，再应用宿主请求和有限等待。用连续发布、暂停和恢复核对终止轮次的 UI 帧计数、下一份 DrawData 的逻辑/像素单位和借用边界，以及无新帧时的结果。
- [x] 3.2 从 `build/d04-t01/` 下的新构建目录构建正式 Debug/Release `Physara`。用 `--help`、一项非法参数、`--hidden --poll-limit=1` 和 `--hidden --poll-limit=3` 核对参数早退、终止轮次及正常 CPU 帧路径。用一条真实可见窗口序列核对暂停恢复、正常关闭和再启动。以实际可触发的创建拒绝和已接入会话的失败退出核对有界首错、退出码与逆序释放。
- [x] 3.3 同步 Runtime 用法文本和运行说明，明确可见与隐藏路径均有 CPU 编辑器会话。以正式 `--help` 输出、实际命令和文档链接核对说明与实现。 [运行说明](../../../../Docs/Build.md#运行)

## 4. 独立 A/B 审查与注释闭环

- [x] 4.1 A 在本地检查达到停点后，按实际 commit、diff 和未跟踪输入建立交接，记录检查入口、结果与限制，将完整受影响文件交给维护者调度独立 B。 [当前问题单](../../../../Docs/CodeReview.md)
- [x] 4.2 B 独立审查依赖方向、回调/输入归属、CPU 帧寿命和失败退出。补齐本轮涉及的完整第一方文件的职责、接口合同和关键流程注释，在问题单中记录覆盖、问题及整改要求。
- [x] 4.3 A 按问题单整改，以受影响的最小回归证据交回实际修正版本。若无须整改，由 A 留存依据和自检状态。
- [x] 4.4 B 核对整改版本、证据和注释覆盖，逐项关闭或有依据地撤回问题，签署实际接受版本。 [审查闭环](../../../../AGENTS.md#review-loop)

## 5. A 最终验证与收尾

- [x] 5.1 B 接受后执行最终 OpenSpec verify，对照两份 delta 的场景、实际实现及匹配版本的构建与原生输入证据。仅在输入变化或存在具体缺口时补跑检查，并记录实际执行、复用及未覆盖范围。
- [x] 5.2 最终 verify 通过后同步两份 delta。同步时核对 `runtime-lifecycle` 的三条修订保留全部原场景，并更新该主规范的 Purpose 以描述当前编辑器生命周期。对同步后的规格执行严格校验。
- [x] 5.3 留存 B 验收、注释覆盖和最终验证证据，按原生流程归档，更新开发计划状态与引用。核对归档入口、change 状态和本地链接后重置当前问题单。 [开发计划](../../../../Docs/Developments/Developments.md#current-stage)

## 验证范围与停止条件

优先使用已存在的消费入口。第一组以真实回调和接合寿命为证，第二组用 CPU 消费者核对路由、状态和布局，第三组以正式程序验证启动、暂停、退出和数据借用。新检查材料只放在 `build/d04-t01/`，由交接提供实际源码、构建和可执行文件身份、原生命令、观察结果及限制。正式构建无需依赖这些材料。

满足两份 delta、D04-T01 完成条件以及独立审查后即停止。缺少真实宿主证据时保留对应验收项。原生 UI 像素、窗口呈现、GPU 在途寿命和阶段总出口各自按 D04 后续工作包验收。 [验证技能](../../../../.agents/skills/physara-verification/SKILL.md) · [工作包完成条件](../../../../Docs/Developments/D03-D05-FramePipeline.md#d04-t01)

<a id="a-local-evidence"></a>
## A 实施和本地验证

2026-11-09，`v2.0` 基准 `1db3c475f5b0eba3b958da152a2c1adbada3e775`。Runtime 装配 CPU Editor，主窗口工作区初始含 Viewport、Hierarchy 和 Inspector，Inspector 显隐动作使用 `Ctrl+Shift+I`。有序发布在失焦和暂停边界取消旧交互。实际实现输入包含未暂存 diff 和 8 份未跟踪 C++ 文件，待审范围以当前问题单为准。 [B 复核留存](#b-acceptance)

本机 Windows x64，MSVC 19.44.35228、Windows SDK 10.0.26100.0、CMake 3.30.0-rc4。原生会话使用 Intel RaptorLake-S Mobile / GL 4.6.0 Build 32.0.101.6129、GLFW 3.5.1。以下命令从仓库根执行，检查工程只链接本轮正式构建产物。

```powershell
$physaraRoot = (Get-Location).Path
cmake -S . -B build/d04-t01/product -G "Visual Studio 17 2022" -A x64 -T host=x64
cmake -S build/d04-t01/checks -B build/d04-t01/check-build -G "Visual Studio 17 2022" -A x64 -T host=x64 "-DPhysaraRoot=$physaraRoot" "-DPhysaraProduct=$physaraRoot/build/d04-t01/product"
foreach ($configuration in @('Debug', 'Release')) {
    cmake --build build/d04-t01/product --config $configuration --target Physara --parallel 8
    cmake --build build/d04-t01/check-build --config $configuration --target EditorChecks --clean-first
    & "build/d04-t01/check-build/$configuration/EditorChecks.exe"
    python build/d04-t01/checks/RunProduct.py "build/d04-t01/product/bin/$configuration/Physara.exe"
}
```

| 检查 | Debug / Release 实际结果 |
|---|---|
| 正式构建 | 新 binary tree 配置、两配置的 `Physara` 构建及当前输入的增量确认通过。生成项目的 Runtime include 路径只含项目的 Platform、Engine 和 Editor，ImGui include 留在 Editor 实现。 |
| 工作区和动作 | 在 CPU ImGui 帧点击真实 View 菜单隐藏 Inspector，用 Press 加 Repeat 恢复同一面板与停靠身份。检查无领域数据时控件禁用，并以同一生产仲裁函数核对文本、模态、Gizmo、有效相对模式和单次滚轮。 |
| 原生接合与恢复 | 向实际 Win32/GLFW 窗口送按键和字符，Platform 与 ImGui 各处理一次且顺序正确。第二次接入明确拒绝。拆除后原 WndProc 与 user pointer 保持有效，同一宿主独立消费输入并可重接。用受控 Win32 焦点消息和零像素发布验证取消、相对模式归还、零位移基线及无旧字符重放。 |
| 帧和 Runtime | 实际 Runtime 循环消费 close 或达到 poll 上限后直接退出，继续运行的轮次构建新帧。用独立的逻辑尺寸、像素尺寸和内容缩放验证 DrawData 比例及恢复时间基线。 |
| 正式入口与失败 | 帮助、非法参数、零宽度、隐藏 1/3 次 poll 分别返回 0/2/1/0/0。可见窗口最小化、恢复、正常关闭并重新启动。接合后通过 Win32 `WM_UNICHAR` 送非法标量，保留首错 `Invalid input value` 并返回 1，后续可再启动。 |

`build/d04-t01/source-inputs.json` 保存构建前的改动源码和检查输入 SHA-256，运行前已重新匹配。`build/d04-t01/evidence.json` 保存两配置的产品、检查程序和原始日志身份及退出值，SHA-256 为 `2967b9679d8e206d842e83abffdcaa6ed78f5db0b15913e2b24b41d432aa95f7`。保留首次配置的 SDK 访问失败日志和本地检查的菜单坐标、诊断文本预期调整记录。最后一次运行只使用匹配当前输入的程序。

本地证据限于上述机器和受控输入。DPI 使用逻辑尺寸、framebuffer 和内容缩放彼此独立的输入验证。实际物理跨屏、UI 像素、窗口交换和 GPU 在途生命周期分别由相关后续工作包验收。本节记录 1.1–3.3 和 4.1 的首轮结果，正式产物已在下述整改验证中重建。

<a id="a-correction-evidence"></a>
## A 整改和回归证据

A 从 `v2.0` 的 `1db3c475f5b0eba3b958da152a2c1adbada3e775` 及未暂存工作树继续。修改前对 12 个 C++/CMake 文件复算字节摘要，匹配 B 注释交接版 `ac531c31d9738727859f221d6e6fdb10e44e2b8890636faa14d07454506e1768`。A 修改输入清理、恢复基线、占用采样及 ImGui 的字符编译合同。B 注释随实际工作树交回，受行为影响的说明已随实现更新。 [B 复核留存](#b-acceptance)

下列命令从仓库根执行。定向用例沿用首轮本地检查工程并链接整改后的正式库，测试输入保存在 `build/d04-t01/a-correction/`。

```powershell
$physaraRoot = (Get-Location).Path
$correction = 'build/d04-t01/a-correction'
cmake -S . -B build/d04-t01/product -G "Visual Studio 17 2022" -A x64 -T host=x64
cmake -S "$correction/checks" -B "$correction/check-build" -G "Visual Studio 17 2022" -A x64 -T host=x64 "-DPhysaraRoot=$physaraRoot" "-DPhysaraProduct=$physaraRoot/build/d04-t01/product"
foreach ($configuration in @('Debug', 'Release')) {
    cmake --build build/d04-t01/product --config $configuration --target Physara imguizmo --parallel 8
    cmake --build "$correction/check-build" --config $configuration --target EditorChecks
    & "$correction/check-build/$configuration/EditorChecks.exe"
    python "$correction/checks/RunProduct.py" "build/d04-t01/product/bin/$configuration/Physara.exe"
}
```

| 复查项 | Debug / Release 实际结果 |
|---|---|
| CR-001 恢复指针 | 用 Win32 位置和按钮消息与受控逻辑位置驱动实际 EditorSession。暂停期点击丢弃，恢复后无新移动或进入消息，首个有效点击打开 View。 |
| CR-002 点击历史 | 暂停前单击、经过 400 ms 暂停后原地单击，计数均为 `1`。接着在有效输入期间点击，计数为 `2`。 |
| CR-003 键盘占用 | 用受控 Press 和菜单释放的先后顺序驱动同一帧。只执行菜单显隐，菜单关闭后的新 Press 恢复 Inspector，Repeat 不改变显隐。 |
| CR-004 字符 ABI | 通过实际 Win32 `WM_UNICHAR` 核对 Platform 发布和 ImGui 帧字符队列均各一次保留 `U+4E2D`、`U+1F642`，顺序一致。`Physara` 与 `imguizmo` 构建通过。生成项目中 `imgui`、`PhysaraEditor`、`imguizmo` 编译定义一致，Runtime 的编译参数未引入该定义或 ImGui include。 |
| 受影响入口 | 回调接合与重接、焦点往返、捕获归还、CPU 帧和 Runtime 终止次序通过。正式产品帮助、参数拒绝、隐藏 1/3 次 poll、可见最小化恢复、接合后非法字符拒绝和再启动均通过。 |

`build/d04-t01/a-correction/source-inputs.json` 记录 B 交接摘要及当前源码、CMake 和检查输入。`evidence.json` 记录两配置的实际程序、库、日志身份和退出值，SHA-256 为 `3b5850428b52180e9de045a71009ce6cf8a7340a5c3b3a4720b64415c1949f50`。执行前后核对输入和产物摘要，保留初版检查中枚举名与桌面指针环境的失败日志。Win32 消息和受控发布用于复现输入顺序，正式产品另以真实窗口验证最小化与恢复。字符检查覆盖输入标量，实际字形绘制随后续 UI 绘制验收。

A 已完成 4.3 自检。独立 B 复核接受实际整改版本后进入 5.1。 [审查闭环](../../../../AGENTS.md#review-loop)

<a id="b-recheck-evidence"></a>
## B 整改复核

2026-11-09，B 对照 `v2.0` 基准 `1db3c475f5b0eba3b958da152a2c1adbada3e775` 和实际未提交输入复核 A 整改。逐一核对 A 整改清单中的 13 个 C++/CMake 输入、3 个检查输入、10 个 Debug/Release 产物、4 份运行日志和 6 份构建及配置日志的 SHA-256。清单和证据文件摘要分别为 `bb1fc27bedc05b65c814b80513fd204af761caa7f8852f7513dc7490a5fc0b52` 和 `3b5850428b52180e9de045a71009ce6cf8a7340a5c3b3a4720b64415c1949f50`。对照 `imgui`、`PhysaraEditor`、`imguizmo` 和 Runtime 的生成项目核对字符宽度定义及编译依赖边界。

- **CR-001 待再次整改** A 的普通模式恢复回归通过。B 另执行 `& ./build/d04-t01/b-recheck-relative/RunProbe.cmd`，使用 MSVC 链接已核验的 Release 正式库。探针使用真实 Win32 回调和宿主光标模式请求，受控发布同批零 framebuffer 边沿及恢复末值。归还已在宿主生效，下一份发布为普通模式，首次静止按下到达 ImGui，但位置无效且菜单保持关闭。补一次位置回调后同样的点击打开 View。探针退出 1，`run-release.log` 的 SHA-256 为 `d50b9229a0b3bd94adb5c2c5344fa5f12c73ff08ad6b88d94e8f40cdeee92822`。该检查覆盖受控恢复时序。
- **CR-002 至 CR-004 复核通过** B 核对点击历史、多击时序、布局前后键盘占用、字符 ABI 和当前消费者，复用摘要匹配的 Debug/Release 原生及 CPU 回归。分别观察到 `1/1/2` 点击计数、同批菜单只生效一次及后续 Press/Repeat 正确、两个原 Unicode 标量均按序各到达两个消费者一次。 [A 整改和回归证据](#a-correction-evidence)

B 更新恢复适配和会话注释，复算确认 13 个 C++/CMake 输入的非注释行与已验证的 A 版本一致。按问题单记录的同一算法，复核注释版聚合摘要为 `0459218476e46d2e7d425b9f35e321a517a1ae254ef26019aeafeafd117c1bca`。构建和运行说明及两份 delta 的相关合同已核对。4.4 待 CR-001 再整改及复核，5.1 随 B 验收进入。 [B 复核留存](#b-acceptance)

<a id="a-recheck-relative-evidence"></a>
## A 再整改和回归证据

A 从 `v2.0` 基准 `1db3c475f5b0eba3b958da152a2c1adbada3e775` 的实际未暂存输入继续，修改前 13 个 C++/CMake 文件的聚合摘要匹配 B 复核注释版。CR-001 再整改版聚合摘要为 `a99aaca3ceabe64cb1d84970ad77b345a398694c56a953cc60a832d0f20a2933`。 [B 复核留存](#b-acceptance)

窗口已经恢复且宿主尚在相对模式时，输入保持关闭。Runtime 应用归还请求，确认成功后在等待事件前通知会话。可交互会话的私有 GLFW 适配读取当前逻辑位置并先于新按钮入队，暂停或失焦会话等待有效发布。已有恢复场景补充同批零像素边沿后的静止首击。 [设计决策 4](design.md#4-失焦和暂停先结束交互再进入下一帧) · [恢复场景](specs/editor-session-input/spec.md#scenario-restore-a-stationary-pointer-after-deferred-cursor-release)

下列命令从仓库根执行。检查复用首轮回归入口，增加 B 的同批边沿序列，链接本轮 Debug 和 Release 正式库。原生检查通过 Win32 输入桌面读取真实位置，测试窗口对齐菜单后用受控发布复现零像素往返。

```powershell
$physaraRoot = (Get-Location).Path
$correction = 'build/d04-t01/a-recheck-relative'
cmake -S . -B build/d04-t01/product -G "Visual Studio 17 2022" -A x64 -T host=x64
cmake -S "$correction/checks" -B "$correction/check-build" -G "Visual Studio 17 2022" -A x64 -T host=x64 "-DPhysaraRoot=$physaraRoot" "-DPhysaraProduct=$physaraRoot/build/d04-t01/product"
foreach ($configuration in @('Debug', 'Release')) {
    cmake --build build/d04-t01/product --config $configuration --target Physara --parallel 8
    cmake --build "$correction/check-build" --config $configuration --target EditorChecks
    & "$correction/check-build/$configuration/EditorChecks.exe"
    python "$correction/checks/RunProduct.py" "build/d04-t01/product/bin/$configuration/Physara.exe"
}
```

| 检查 | Debug / Release 实际结果 |
|---|---|
| CR-001 相对模式同批恢复 | 聚焦时实际开启相对模式，使虚拟位置偏离菜单。零像素往返批次丢弃旧按钮和字符。归还确认 Normal 后，未派发新移动或进入消息，实际逻辑位置只入队一次且先于首个 Win32 按下，View 菜单打开。 |
| CR-001 普通模式及持续暂停 | 普通模式恢复后的首个静止点击打开 View，旧点击丢弃。窗口仍暂停时，捕获归还后保持输入关闭。焦点往返、零像素恢复和逻辑/像素比检查通过。 |
| 既有回归及入口 | 多击计数 `1/1/2`、同批菜单只生效一次、Unicode 标量单次按序消费均通过。正式产品的帮助、非法参数、隐藏 poll、可见最小化恢复、接合后非法字符拒绝和再次启动通过。 |

`build/d04-t01/a-recheck-relative/` 保存构建前产品和检查输入摘要、实际构建及运行日志。`evidence.json` 的 SHA-256 为 `2812293086b974ce42bf6ff5716518c301d008ceb3152c5d205c3a4285b4373d`。运行前后核对输入与可执行文件身份。同批零像素边沿使用受控发布，按键、光标采样与模式归还使用本机 Win32/GLFW 窗口。

CR-001 的 A 再整改与自检已交回。4.4 由 B 复核实际修正版后签署。 [审查闭环](../../../../AGENTS.md#review-loop)

<a id="b-acceptance"></a>
## B 最终复核与验收

2026-11-09，B 签署 `v2.0` 基准 `1db3c475f5b0eba3b958da152a2c1adbada3e775` 上的实际未提交版本。A 再整改版聚合摘要为 `a99aaca3ceabe64cb1d84970ad77b345a398694c56a953cc60a832d0f20a2933`。B 签署版按同一算法计算的 13 个 C++/CMake 输入聚合摘要为 `3b3084160b94d25a400f6d4dae52bf99c636cf23a3eba2a6957b171a9682e56d`。B 的源码差异限于注释，逐文件核对非注释行及编译指令。 [签署问题单留存](#final-verification)

| 问题 | B 复核结论 |
|---|---|
| CR-001 | 通过。会话保持门控直到宿主确认相对模式已归还，恢复时用当前逻辑位置在新按钮前建立基线。复用两配置 Win32 回归，确认同批零像素往返后首个静止按下打开 View，旧按钮和字符丢弃。普通模式恢复与持续暂停门控通过。 |
| CR-002 | 通过。两配置确认暂停前、恢复后及连续下一次点击计数为 `1/1/2`。 |
| CR-003 | 通过。两配置确认布局关闭菜单时保留本批键盘归属，后续独立 Press 执行一次，Repeat 保持状态。 |
| CR-004 | 通过。两配置确认 Platform 和 ImGui 按序各消费两个原始 Unicode 标量一次。 |

B 复算 13 个产品源码和 CMake 输入、3 个检查输入、12 个 Debug/Release 产物、4 份运行日志及 6 份配置与构建日志。证据文件摘要为 `2812293086b974ce42bf6ff5716518c301d008ceb3152c5d205c3a4285b4373d`，所有已引用项匹配。B 核对测试断言和生产调用链，复用 A 的两配置结果。生成的 Editor 与 Runtime 项目验证 GLFW 头和编译定义留在 Editor 私有实现，ImGui 字符定义按目标依赖传播。 [A 再整改和回归证据](#a-recheck-relative-evidence)

B 通过注释验收。覆盖 9 个 C++ 文件和 4 个 CMake 文件，重点复核恢复适配的窗口借用、归还确认后的逻辑位置、会话门控、Runtime 等待前的完成通知及 GLFW 依赖。B 的本轮检查限于源码、注释、文档、实际生成的编译参数和匹配证据。同批零像素边沿来自受控发布，实际光标采样、回调和模式归还来自 Win32/GLFW。UI 像素、窗口呈现与 GPU 在途寿命由后续工作包验收。

B 完成 4.4，A 进入 5.1。最终 verify、规范同步和归档的验收证据由 A 接续记录。

<a id="final-verification"></a>
## A 最终验证与收尾

2026-11-10，按 `openspec-verify-change` 核对 B 签署的 `v2.0` 工作树，基准为 `1db3c475f5b0eba3b958da152a2c1adbada3e775`。13 个 C++/CMake 输入的实际聚合摘要为 `3b3084160b94d25a400f6d4dae52bf99c636cf23a3eba2a6957b171a9682e56d`。B 签署的 CR-001 至 CR-004 结论及 9 个 C++、4 个 CMake 文件的注释验收见上节。问题单的签署版字节保存在本机 `build/d04-t01/final-verification/CodeReview.accepted.md`，SHA-256 为 `7e40788746a540cb44f497affaeef4477e19e21473932ec247bb68bd7bb6166a`。 [B 最终复核](#b-acceptance)

| 维度 | 最终验证结果 |
|---|---|
| 完整性 | 实现与独立审查的 12 项均完成。两份 delta 包含 5 条新增、3 条修订及 18 个场景，均对应生产入口。 |
| 正确性 | 下表按合同核对实现和两配置证据。原生交互检查含实际 Win32 窗口、宿主请求及受控发布。 |
| 一致性 | 设计的六项决策与唯一 Runtime、Editor 私有输入适配、面板和动作所有权、暂停恢复、CPU 帧寿命及后续衔接一致。生成项目核实 GLFW/ImGui 编译选项的私有边界及字符 ABI。 |

| 合同 | 场景数 | 实现与证据 |
|---|---:|---|
| 原生宿主接合 | 2 | `GlfwPlatform` 链接官方 GLFW 回调。`CheckNativeCallbacksAndReset` 检查两个消费者的单次顺序投递、原回调和窗口数据关联恢复、拆除后的宿主消费与重接。补充平面标量由 `CheckNativeUnicode` 核对。 |
| 停靠工作区与领域动作门控 | 2 | `Workspace::Draw` 在主窗口建立固定面板并按显隐构建。`CheckWorkspace` 观测菜单隐藏、通过共享动作恢复同一停靠身份及空选择时的禁用行为。 |
| 动作唯一归属 | 2 | `ResolveInputOwner` 统一键鼠优先级，`Workspace::ApplyShortcuts` 消费合格首次按下。CPU 检查覆盖单次滚轮归属，Win32 菜单序列核对同批释放占用、后续 Press 和 Repeat。 |
| 失效与恢复 | 3 | `EditorSession::Advance`、`CompleteCursorRelease` 和私有适配处理同批焦点边沿、持续暂停及零像素往返。检查相对模式归还、旧输入丢弃、普通和相对模式恢复后的静止首击及多击序列。 |
| CPU 帧发布与寿命 | 2 | `GlfwPlatform::BeginFrame` 以本次发布设置逻辑尺寸与像素比例，`EditorSession::Advance` 封存 CPU 帧。检查独立内容缩放、恢复时间基线、无新帧及下一次帧的有效数据。 |
| 唯一产品入口 | 2 | `main` 在 owner 线程装配宿主及 Editor。正式程序无参数启动并经过可见窗口循环，隐藏启动用同一路径完成有界 poll。 |
| 终止判断与等待 | 3 | `RunEditor` 在 UI 帧与等待前判断终止。实际循环检查正常轮次、末次 poll 和 close 的帧数，正式程序检查最小化及恢复。 |
| 逆序关闭与首错 | 2 | `main` 的会话作用域先恢复回调再关闭宿主。接合后的非法标量返回 1 并保留有界错误，随后可以重启。报告与清理异常的首错保留由调用顺序和退出状态核对。 |

本轮复用 `build/d04-t01/a-recheck-relative/checks/` 的检查输入。B 的注释修改改变了六个已签署文件的字节，因此正式 `Physara` 和检查程序均在该版本上重新构建并运行。Windows x64、MSVC 19.44.35228、SDK 10.0.26100.0 和 Intel RaptorLake-S Mobile GL 4.6.0 的两配置结果均通过。

```powershell
$physaraRoot = (Get-Location).Path
$final = 'build/d04-t01/final-verification'
$checks = 'build/d04-t01/a-recheck-relative/checks'
cmake -S . -B build/d04-t01/product -G "Visual Studio 17 2022" -A x64 -T host=x64
cmake -S $checks -B "$final/check-build" -G "Visual Studio 17 2022" -A x64 -T host=x64 "-DPhysaraRoot=$physaraRoot" "-DPhysaraProduct=$physaraRoot/build/d04-t01/product"
foreach ($configuration in @('Debug', 'Release')) {
    cmake --build build/d04-t01/product --config $configuration --target Physara --parallel 8
    cmake --build "$final/check-build" --config $configuration --target EditorChecks
    & "$final/check-build/$configuration/EditorChecks.exe"
    python "$checks/RunProduct.py" "build/d04-t01/product/bin/$configuration/Physara.exe"
}
```

`build/d04-t01/final-verification/accepted-inputs.json` 和 `evidence.json` 保存当前源码、检查、产物及日志身份。证据文件 SHA-256 为 `35ec93e5bbbb29e256dce5d7f4b54a59523e2f888bcf1910ef72043ab651689f`。两配置的 CPU 与 Win32 检查和正式程序脚本均退出 0。产品帮助、未知参数、零宽度、隐藏 1/3 次 poll 分别返回 `0/2/1/0/0`，可见最小化恢复、接合后非法标量拒绝及再启动通过。严格 change 校验 1/1 valid，链接和空白检查通过。最终验证的适用检查均有证据，未发现阻断项、警告或建议。

同批零像素使用受控发布，模式归还、逻辑位置和按钮使用实际 Win32/GLFW。DPI 比例通过独立尺寸输入核对，实际跨屏切换未运行。后续工作包负责字形与像素绘制、窗口交换及 GPU 在途寿命。

### 主规范同步

按当前 `instructions specs --json` 合并两份 delta。`editor-session-input` 采用 delta 的 Purpose 以及 5 条需求和 11 个场景。`runtime-lifecycle` 合并 3 条修订并更新 Purpose，逐项核对原 4 条需求顺序、全部 10 个场景及启动参数合同。`openspec validate --specs --strict --json` 通过 13/13，无 ERROR 或 WARNING，INFO 为段落长度提示。 [Editor 主规范](../../../specs/editor-session-input/spec.md) · [Runtime 主规范](../../../specs/runtime-lifecycle/spec.md)

### 归档结果

OpenSpec 1.13.2 执行 `openspec archive establish-editor-session-input --yes --skip-specs --json`，退出 0，实际目录为 `openspec/changes/archive/2026-11-10-establish-editor-session-input/`。`--skip-specs` 用于保留已同步并通过严格校验的规范。执行归档时为 14/15，第 5.3 项涵盖移动后的核对及问题单重置。

移动后核对六份归档文件与十三份主规范的字节摘要，再按归档层级更新文档链接。开发计划已记录 D04-T01 完成并将当前工作包推进至 D04-T02 待立项。 [开发计划](../../../../Docs/Developments/Developments.md#current-stage)

归档后 `openspec list --json` 无活动 change，`openspec validate --all --strict --json` 为 13/13 valid，本地路径和锚点核对通过。问题单随后按模板重置，字节摘要与模板一致，SHA-256 为 `a26066c0106ba8d1991974974acf06ddc62ed5d24e2878a42eda8f6fbec847fc`。第 5.3 项在实际重置后勾选，最终 15/15。 [问题单模板](../../../../Docs/CodeReview.md.tpl)
