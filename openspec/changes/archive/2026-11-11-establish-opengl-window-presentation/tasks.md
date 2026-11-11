# Tasks

## 1. A 建立目标取得与失效边界

- [x] 1.1 在 DeviceHost 建立单个呈现槽和单次取得身份。沿验证 Device 成组创建实际像素尺寸的 `SRGBA8_UNORM` 纹理与 framebuffer，核对格式、样本和 `ShaderResource` 导出。用实际宿主验证可渲染取得，最小化或零范围返回 Suspended 且无替代分配。 [目标设计](design.md#presentation-target)
- [x] 1.2 在复制前用当前窗口事实拒绝已失效尺寸，按目标代和取得身份拒绝旧 token。用一次取消后重取核对旧 token 在 Release 被拒绝且当前取得仍可用，再用一次窗口尺寸变化核对旧取得无末端访问，完成后按新像素范围取得。

## 2. A 实现 OpenGL 末端复制与缓存协作

- [x] 2.1 接通 Validation 私有列表借用和 OpenGL 专用入口，核对同一 context、闭合包、源 FBO 和导出状态。在原生访问前将源及附件依赖登记到现有包。用一组合法闭合包与仍开放的代表性拒绝输入核对前置及保活。更新实际启用的互操作边界。 [复制设计](design.md#window-copy) · [RHI 说明](../../../../RHI/README.md#opengl-原生消费边界)
- [x] 2.2 按实际后缓冲编码执行同尺寸颜色复制和一次行序适配，维护唯一 `ContextState`。在交换前读回真实、未遮挡窗口的后缓冲，比较非对称四角、裁剪边缘、已知底色和半透明 UI 叠加区。按实际 RGB 位数定义容差，期望值独立于复制实现。下一个列表复用带 scissor 的绘制设置，核对目标、裁剪和编码。另一种实际后缓冲编码可得时复用同一图案，只有一种可得时用另一编码的等尺寸原生附件核对转换，并单列缺少的窗口证据。同步检查原生观察 helper 恢复其触及的状态。 [颜色及行序合同](specs/opengl-window-presentation/spec.md#requirement-deliver-one-sdr-display-encoding-and-correct-row-order)

## 3. A 闭合提交、取消、完成和交换

- [x] 3.1 宿主把复制、验证 Device 提交结果和单次交换资格关联到同一取得。以实际调用顺序观察 Close、blit、fence、flush 和 swap，顺序检查期间不插入同步读回或额外等待。用单调 CPU 秒记录 `PresentWait`，核对交换前后该票据的 Event 完成独立推进。一次受控交换错误仍保留该接受票据和真实等待区间。 [提交设计](design.md#submission-and-cancel) · [计时设计](design.md#present-timing)
- [x] 3.2 用一个合法前缀后的可诊断拒绝核对失败目标跳过复制和交换，正票据只保护前缀。再用定向的真实复制错误和复制发出后无票据各核对一次。检查本轮没有交换及错误的 UI 上传确认，前缀和源 lease 保留到有效完成或受控收拢。构造失败的本地材料限 `build/d04-t03/`，同一错误合同复用结果。
- [x] 3.3 接上槽的 Event 观察和 Shutdown。以清屏后复制的帧区分源 FBO 的末端保活，观察 Pending 不覆盖或重建槽、真实完成后才复用。将最后一帧在途关闭和前项无票据的受控收拢纳入同一组检查。若观察失败则槽停止复用，外部持有仍触发原关闭拒绝。 [完成和关闭设计](design.md#completion-and-shutdown)

## 4. A 接通唯一产品入口

- [x] 4.1 按实际可执行文件目录加载对应配置的正式 `EditorUi` 包。核对普通 Debug/Release 构建及从其他工作目录启动时的包身份，确认新构建不依赖本地夹具。用一份缺失或 ABI 不符的实际包验证状态 1 和有界错误，确认帮助不初始化原生环境或读取包。同步构建说明的包位置及实际运行入口。 [装配设计](design.md#runtime-assembly) · [构建说明](../../../../Docs/Build.md)
- [x] 4.2 将真实 `EditorSession` 封存帧、公共 RHI 底色与 UI 录制、提交回执及宿主交换接入 Runtime。用默认可见程序观察 Docking UI，再用 `--hidden --poll-limit=3` 验证同一帧链和成功退出。核对 close 或达到 poll limit 的轮次不再发出下一帧，暂停仍推进事件和输入清理，最后一帧使用已验收的收拢路径。 [产品规范](specs/runtime-lifecycle/spec.md)

## 5. 独立 A/B 审查与注释闭环

- [x] 5.1 A 完成对应实现与最小检查后，记录基准 commit、实际跟踪修改及未跟踪输入、匹配的构建/二进制/包、窗口编码、设备/驱动、图案和原生调用顺序。区分本次运行、复用与未覆盖范围，将实际本地材料和受影响的完整文件交给维护者调度 B。 [问题单](../../../../Docs/CodeReview.md) · [验证约定](../../../../.agents/skills/physara-verification/SKILL.md)
- [x] 5.2 B 独立核对目标身份、copy/fence/flush/swap 顺序、颜色和行序、闭合包 lease、诊断及关闭路径，核验实现与证据身份，并补齐本轮完整第一方文件的职责、接口和关键流程注释。 [B 复核记录](#b-review-acceptance)
- [x] 5.3 A 按问题单整改，重跑能区分问题且受输入变化影响的检查，在同一问题单记录修正及证据。无整改项时记录本地自检依据。 [A 整改证据](#a-correction-evidence)
- [x] 5.4 B 核对实际修正版、匹配证据与完整注释，关闭或说明撤回问题并签收最终输入。 [B 复核记录](#b-review-acceptance) · [独立审查流程](../../../../AGENTS.md#review-loop)

## 6. A 最终验证与归档

- [x] 6.1 B 签收后执行最终 OpenSpec verify，将两份 delta 的场景逐项对应到实现与签收版本的原生证据。仅按输入变化、失败或具体缺口补查，留存实际运行、复用和未覆盖范围。 [最终验证](#final-verification)
- [x] 6.2 验证通过后同步 `opengl-window-presentation` 和 `runtime-lifecycle`，复核新增能力的 Purpose 和全部需求，合并产品入口的完整替换块，核对其余启动、事件及关闭合同。严格校验主规范和 change。 [主规范同步](#主规范同步)
- [x] 6.3 在 change 留存 B 签收与最终证据，执行原生归档。更新开发计划状态和归档链接，核对归档入口与状态后按模板重置当前问题单。 [开发计划](../../../../Docs/Developments/Developments.md#current-stage)

<a id="a-local-evidence"></a>
## A 实现与本地验证

2026-11-11，基准 commit 为 `8bc8a26a910448df3ad64c3fbff1fb9e5805d184`，暂存区为空。实现包含单槽 PresentColor、宿主取得/准备/提交/交换、闭合包的原生源保活，以及唯一 Runtime 的 RHI UI 装配。源码和构建输入按实际未暂存与未跟踪内容取证。现有 UI Shader 及 ImGui 实现沿 D04-T02 合同消费。 [目标与身份](../../../../Platform/Presentation.hpp) · [宿主状态推进](../../../../Platform/Presentation.cpp) · [末端访问](../../../../RHI/OpenGL/WindowPresentation.cpp)

### 实际运行

从仓库根执行 `& 'build/d04-t03/checks/RunEvidence.ps1'`。材料限 `build/d04-t03/`，其中 `a-local/` 保存当前输入的证据，`attempts/` 保留初次构建、调试和重试记录。本机环境为 Windows x64、VS 2022、MSVC 19.44.35228、SDK 10.0.26100.0、CMake 3.30.0-rc4、Intel RaptorLake-S Mobile OpenGL 4.6.0 Build 32.0.101.6129。真实默认后缓冲为 sRGB，RGB 各 8 位。

Debug 和 Release 采用普通工程选项及原有警告门槛。两配置分别完成正式产品构建、8 个原生模式，以及 4 个产品入口检查。全新 `clean-product/` binary 目录另外配置并构建 Release，生成对应 UI 包，从其他工作目录执行隐藏产品并正常退出。两次本机 `C1001` 分别出现在 Debug 首建及新目录 Release 首建，同配置重试成功，未改变警告、优化、调试信息或迭代器检查。新目录首建失败和重试完整日志分别为 `attempts/clean-release.log`、`attempts/clean-release-retry.log`。

| 已验收项 | 观察与判据 |
|---|---|
| 取得和取消 | 取消后重取拒绝旧 token，当前取得仍可使用。实际最小化返回 Suspended，恢复可取得。仍开放的验证列表在原生复制前被拒绝。 |
| 窗口颜色与状态 | 两配置在交换前读回真实后缓冲，比较四角、已知底色、半透明着色区 `RGB=(56,71,130)` 和半开裁剪边界，8 位 RGB 容差为 2。连续两帧复用原 UI 状态与资源，第二帧的裁剪和编码保持正确。 |
| 末端顺序和退休 | 单独顺序模式在 Close 后无同步读回，实际记录 blit、fence、flush、SwapBuffers 为 `CBFLS`。受控 Pending 返回 Retry，后续真实 Event 完成才允许复用。清屏帧单独核对末端 FBO 增加 lease，完成回收后恢复原引用数。 |
| 尺寸过期 | Close 后改变真实窗口尺寸，原取得无 blit 或交换，已发前缀获有效票据。完成后重建匹配尺寸及新代。 |
| 失败前缀 | 在合法 UI 前缀之后触发实际验证层写入拒绝，得到用于退休的票据 1，保持 `WantCreate` 和源像素，撤回临时 token，未复制或交换。 |
| 原生失败 | 定向使实际 blit 返回 GL 错误、fence 返回零、完成查询报告实际 GL 错误。分别核对无交换、无错误上传确认、源与前缀保留及受控退出。copy/query 的原生首错在 Shutdown 返回中保留，再次关闭成功。 |
| 交换失败 | 本地 Win32 导入观察器用无效 DC 触发真实 `SwapBuffers` 失败。宿主报告 Win32 错误 6、实际等待区间并保留接受票据 1。Platform 使用 Win32 返回值的原因见 design 决策 6。 |
| 产品与关闭 | 从其他工作目录执行两配置 `--hidden --poll-limit=3`、`--poll-limit=1`，正常退出。复制产品到无包的本地目录，`--help` 返回 0，正常启动缺包返回 1 并报告有界原因。默认可见 Debug 入口经 computer-use 观察到真实 Docking 面板，按维护者授权以 Alt+F4 关闭，退出 0。 |

### 输入、产物与覆盖范围

`a-local/source-inputs.json` 记录 127 项仓库产品输入和 4 份本地检查输入，SHA-256 为 `03afebce468ba4140f07bbb105c627b3335ea4d405ef78252bf3ce5fdec6120d`。`build-inputs.json` 记录匹配的 39 份构建产物，SHA-256 为 `052dcf1013020ebc49e7bd065095c770764512d2d46be02f97197f933d481a2a`。`evidence.json` SHA-256 为 `b4993d818a1770bbaca0b51d7f9aa8a205184888606b0086285fb31cfecf2413`，25 次脚本运行均取得预期结果，其中两次缺包启动按合同返回 1。默认桌面入口另外记录在 `product-visible-sky.json`，SHA-256 为 `c3f568e3301bed958259219c700be677adbaef0a318cdb654698dceb066957cd`。已独立复算输入、运行文件、正式包、日志及桌面退出结果的摘要。

实际窗口覆盖本机 sRGB 默认附件。另一颜色支路在真实线性附件上执行同尺寸复制并读回，属于转换代理证据。Pending 使用一次受控观察返回，随后由真实 Event 确认完成。原生失败来自定点入口替换，材料和改写均限本地检查进程。受限执行环境的首次可见 runner 未能在桌面交互并超时，最终可见检查由 computer-use 在交互桌面启动、观察并正常关闭，单独留存其实际退出结果。本节保留首次交 B 审查时的 A 本地验证。 [当前问题单](../../../../Docs/CodeReview.md)

<a id="a-correction-evidence"></a>
## A 整改与回归

2026-11-11，基准仍为 `8bc8a26a910448df3ad64c3fbff1fb9e5805d184`，暂存区为空。A 在整改前复算 B 补注释后的 25 个 C++ 和 CMake 文件，摘要为 `30f85f0757dd288d4cc31375091dda13a757ec4b1b35b4a2e69eed2735c6057a`。相对该快照仅修改 `Runtime/Main.cpp`，保留其余文件和 B 的注释。按同一口径计算的修正快照为 `88bad60111441d8ec40248a2363f297826c340ccf0d8ec5ebdc2501136c017b8`。两份快照均含未跟踪实现。 [问题单](../../../../Docs/CodeReview.md) · [Runtime 入口](../../../../Runtime/Main.cpp)

在仓库根依次执行 `python build/d04-t03/a-correction/checks/Evidence.py snapshot`、同脚本的 `build` 和 `run`。脚本以正式工程配置分别构建 Debug、Release 的 `Physara`，并为每个配置构建本地探针。探针编译实际 `Runtime/Main.cpp` 与 `Resources.cpp`，链接同配置正式库并使用同一 UI Shader 包。帧探针通过实际 `RunEditor` 读取诊断环，关闭探针通过完整产品 `main`。各配置另从其他工作目录运行未插桩的正式 `Physara --hidden --poll-limit=3`，均退出 0。

| 问题 | 两配置的实际结果 |
|---|---|
| CR-001 | 真实窗口 resize 320×240 → 416×304。观察次序为旧 fence 真实完成、删除该 fence、新目标 blit、`glDeleteFramebuffers` 删除旧 FBO、`glDeleteTextures` 删除旧纹理。两次删除均发生于产品循环结束前。循环内非零超时等待与 `glFinish` 均为 0 次。 |
| CR-002 | 在同一有效 fence 上控制两个五秒关闭期限超时，再允许实际查询完成。两次超时均保留原窗口、DC 与当前 context。仅有最后一帧的 1 次交换，`glFinish` 为 0 次。真实完成后删除 fence，窗口销毁、context 解绑。首条 Pending 诊断只向 stderr 输出一次，进程按合同退出 1。 |
| CR-003 | 正常首帧的诊断环依次记录 CPU 就绪、票据 1 接受和同票据交换成功。定向用无效 DC 触发真实 `SwapBuffers` 错误 6，环中保留票据 1 的接受记录，成功交换记录为 0 条，退出 1。 |

材料位于 `build/d04-t03/a-correction/`。`received/` 保留 B 快照，`runtime-correction.patch` 记录相对 B 版本的修正。`source-inputs.json` 含 127 项产品输入与 3 项本地输入，SHA-256 为 `0ba6e48160e1b0508f963724cb043dad1881c5f7b436d5d27f5042778f3c1df5`。`build-inputs.json` 含 41 项匹配产物及构建日志摘要，SHA-256 为 `bc0b0fb731604ed2e14b6a6121a9b218ec8b13413516eca4cc9d703c67a4aebc`。`evidence.json` 记录 8 次运行的实际命令、退出值与日志摘要，SHA-256 为 `ec5180f21420b0ac17f5607df6c67664d271b5bed596b22700f07037eb4b2745`。首轮探针误把 GLFW 初始化交换计入产品帧，导致 resize 超时及失败场景计数错误，日志保存在 `attempt-01/`。修正观察起点并重新构建后，以上 8 次运行均取得预期结果。

本轮原生结果来自本机 Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129 的隐藏 sRGB 窗口。关闭超时由探针控制，完成由原生 fence 证明。默认可见交互、颜色与行序、真实线性附件代理等沿用首次 A 验证记录；真实线性窗口与实际 GPU 挂起未覆盖。该回归记录用于 B 复核 CR-001 至 CR-003、修正版身份和受影响注释。 [首次 A 本地验证](#a-local-evidence) · [当前问题单](../../../../Docs/CodeReview.md)

<a id="b-review-acceptance"></a>
## B 复核与签收

2026-11-11，B 签收基准 `8bc8a26a910448df3ad64c3fbff1fb9e5805d184`、空暂存区和工作区快照 `88bad60111441d8ec40248a2363f297826c340ccf0d8ec5ebdc2501136c017b8`。25 文件摘要包含 6 个未跟踪实现文件。与 B 注释交接版本逐文件比对，唯一变化的源码为 `Runtime/Main.cpp`。 [产品入口](../../../../Runtime/Main.cpp) · [问题单](../../../../Docs/CodeReview.md)

| 编号 | B 复核结论 |
|---|---|
| CR-001 | 通过。正常、暂停和槽忙的继续轮次在等待前推进零超时回收。Debug/Release 的 resize 记录均在旧 fence 真实完成后、退出前删除旧 FBO 和纹理。 |
| CR-002 | 通过。产品在 owner 线程保留宿主和诊断并重试 `RhiDrainPending`。两次受控超时后由真实 fence 证明完成，随后销毁窗口、解绑 context，首错保留且退出 1。 |
| CR-003 | 通过。CPU UI 就绪、Graphics 接受与交换成功分别可观察。正常帧使用同一票据关联接受和交换，交换失败日志保留接受票据。 |

B 复算首次交接的 127 项产品输入与记录，并核对 25 份历史运行日志的摘要。对整改版复算 127 项产品输入、3 项探针输入和 41 项产物，核对 5 条构建记录及 8 条回归的命令、可执行文件、退出值与日志，匹配上述 A 整改证据。B 复用匹配记录，原生覆盖为本机 sRGB 窗口、线性附件代理与定点失败观察。 [首次 A 本地验证](#a-local-evidence) · [A 整改证据](#a-correction-evidence)

Platform、RHI/OpenGL、Validation、Runtime、Editor 直接消费接口和相关 CMake 的职责、接口及流程注释通过复核。Runtime 的接受记录、帧后回收和关闭重试说明与修正版一致。 [A 最终验证](#final-verification)

<a id="final-verification"></a>
## 最终验证与归档

2026-11-11，按 OpenSpec 1.13.2 的 spec-driven 工作流核对 proposal、design、两份 delta 和 tasks。13 项实施与独立审查任务完成，CR-001 至 CR-003 均由 B 关闭。复算基准、空暂存区和 25 个受审文件的摘要，包含 6 个新增未跟踪实现，与 B 的签收一致。签收问题单保存为 `build/d04-t03/final-verification/CodeReview.accepted.md`，SHA-256 为 `4faf98a2c8e3b8a29bbe2d42201aa4d2c939d078b86231eb5dce11fbaa6e9439`。 [B 签收](#b-review-acceptance)

复用签收版 Debug/Release 的 8 次整改回归，逐项复算 127 项产品输入、3 项探针输入、41 项产物以及构建和运行日志。首次 A 的四个模块库在两配置的文件摘要已有变化，因此从仓库根运行 `python build/d04-t03/final-verification/FinalEvidence.py`，使用原夹具的同一输入，在独立构建目录编译并链接签收版 Release 库。8 个原生模式均退出 0。同一正式 Release 产品置于缺包目录后启动，报告有界缺包错误并退出 1。 [A 整改证据](#a-correction-evidence)

签收版 Debug 无参数启动后显示实际 Docking UI，经 `Alt+F4` 关闭，进程退出 0。第一次观察已确认窗口关闭，进程监听未取到退出码，保留为 `product-Debug-visible-attempt-01.json`。第二次在关闭前持有进程查询句柄，以 `GetExitCodeProcess` 记录退出状态和匹配的 exe SHA-256。正式桌面结果 `product-Debug-visible.json` 的 SHA-256 为 `69b5b294bb783a3849e0571219ac738c8ac0b0a6948ced8d8bb80febc6555069`。其余 9 条新增运行、构建命令、实际输入和日志摘要存于 `evidence.json`，SHA-256 为 `5ce39eb8ca66774f4a648e6886edca0d096a34e5fc1ffc38da56a70cee76a139`。

### 需求与场景核对

| 场景 | 实现与签收版证据 |
|---|---|
| Acquire a renderable window target | `BeginPresentation` 成组发布实际像素尺寸的 `SRGBA8_UNORM` 目标和身份，Runtime 首写清屏。Release `normal` 在实际取得上绘制并读回图案。 [取得](../../../../Platform/Presentation.cpp#L66) · [首写](../../../../Runtime/Main.cpp#L157) |
| Suspend acquisition without a substitute target | 窗口不可渲染时在目标创建前返回 Suspended。Release `normal` 在最小化时拒绝取得，恢复后按现有像素范围呈现。 [暂停](../../../../Platform/Presentation.cpp#L76) |
| Reject an expired presentation identity | 取得操作共用身份及阶段检查。Release `normal` 拒绝已取消的 token，保留当前有效取得。 [身份检查](../../../../Platform/Presentation.cpp#L54) |
| Present one accepted editor frame | 闭合包保活源，提交在末端复制后建立 fence 并 flush，同一取得携带票据交换。Release `sequence` 无同步读回时观察到 `CBFLS`，实际交换与提交票据一致。 [复制与保活](../../../../RHI/OpenGL/WindowPresentation.cpp#L53) · [提交](../../../../RHI/OpenGL/Submission.cpp#L90) |
| Reestablish RHI state after native presentation | `ContextState` 更新末端互操作触及的状态。Release `normal` 的第二帧沿相同图形设置通过实际裁剪和颜色读回。 [状态切换](../../../../RHI/OpenGL/ContextGraphics.cpp#L252) |
| Copy asymmetric UI colors to an sRGB back buffer | 按观测编码启用 sRGB 写入并反向 Y。Release `normal` 比较真实 sRGB 后缓冲四角、底色、裁剪和半透明区，8 位 RGB 容差 2。 [窗口复制](../../../../RHI/OpenGL/WindowPresentation.cpp#L57) |
| Copy encoded SDR colors to a linear back buffer | 线性支路以等尺寸复制保留存储的 SDR 编码。Release `normal` 在真实线性附件上走该支路并读回同一图案。当前机器的实际默认后缓冲为 sRGB，真实线性窗口缺覆盖。 [窗口编码选择](../../../../Platform/Presentation.cpp#L168) · [原生编码状态](../../../../RHI/OpenGL/ContextGraphics.cpp#L252) |
| Invalidate a frame before the terminal copy | `PrepareSubmission` 核对闭合列表、录制结果和窗口范围。Release `boundaries`、`prefix-error` 观察到过期和失败前缀有真实退休票据，跳过复制与交换，失败上传请求保留。 [复制门控](../../../../Platform/Presentation.cpp#L139) |
| Fail a native terminal copy | 原生复制错误使槽停止并沿关闭收拢。Release `copy-error` 观察到实际 GL 错误，上传未确认，关闭保留首错。 [复制失败](../../../../RHI/OpenGL/WindowPresentation.cpp#L72) |
| Receive no accepted submission after the copy | 票据为零时停用槽及交换资格。Release `no-ticket` 在复制后控制 fence 返回零，未交换，未确认上传，按退出合同收拢。 [提交拒绝](../../../../Platform/Presentation.cpp#L187) |
| Observe a busy slot before reusing or replacing it | Event 成功后才复用或成组替换。Release `sequence` 和 `boundaries` 检查 Pending、源 FBO lease 和真实完成。复用两配置的 CR-001 记录，确认 resize 后旧 FBO 和纹理在运行中删除。 [完成门控](../../../../Platform/Presentation.cpp#L83) |
| Close with an outstanding terminal source access | Release `poll-error` 和 `no-ticket` 核对完成证据失败后的收拢。复用 CR-002 两配置的连续 drain 超时回归，保留同一 owner 直至真实完成并退出 1。 [完成查询](../../../../RHI/OpenGL/Queries.cpp#L114) · [关闭重试](../../../../Runtime/Main.cpp#L347) |
| Observe a swap before the submission is observed complete | `Present` 以单调时钟包围实际 GDI 调用，Event 仍独立判定完成。Release `normal` 检查成功交换的等待值，`sequence` 在交换后经受控 Pending 和真实 Event 完成。 [交换计时](../../../../Platform/Presentation.cpp#L254) |
| Report a swap failure after submission acceptance | Release `swap-error` 观察 Win32 错误 6、已接受票据与本次交换等待值。复用 CR-003 两配置结果，失败轮次仍记录票据且成功交换记录为 0。 [交换结果](../../../../Platform/Presentation.cpp#L256) · [提交诊断](../../../../Runtime/Main.cpp#L177) |
| Start the default product | 签收版正式 Debug 默认可见运行观察到 Docking UI，窗口关闭后进程退出 0。 [产品入口](../../../../Runtime/Main.cpp#L281) |
| Start a hidden bounded run | 复用两配置正式产品 `--hidden --poll-limit=3` 的退出 0，以及探针的 CPU 就绪、提交接受和同票据交换记录。 [帧循环](../../../../Runtime/Main.cpp#L206) |
| Fail when the required UI program cannot be initialized | 路径按实际 exe 的配置生成。签收版正式 Release 缺包启动报告 `EditorUi.pshader` 并退出 1。源码核对 Editor 和 Host 按顺序收拢。 [资源定位](../../../../Runtime/Resources.cpp#L17) · [启动与关闭](../../../../Runtime/Main.cpp#L314) |

7 条需求和 17 个场景均完成实现对应。实际后缓冲读回来自 Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129 的 8 位 sRGB 窗口，线性分支为附件代理。完成超时、零 fence、查询和交换失败来自定点控制，实际 GPU 挂起未覆盖。设计的单槽取得、同配置 UI 装配、末端复制、同包保活、Event 退休、显式收拢和独立交换计时均可在对应入口核对。模块边界与注释以 B 签收版本为准。OpenSpec 严格校验通过 15/15，最终 verify 通过。 [设计决定](design.md#decisions) · [呈现 delta](specs/opengl-window-presentation/spec.md) · [产品 delta](specs/runtime-lifecycle/spec.md)

### 主规范同步

按 `openspec instructions specs --json` 取得本次同步输入。`opengl-window-presentation` 的 Purpose、6 条需求及 14 个场景与 delta 一致。`runtime-lifecycle` 完整替换产品入口需求，保留同一需求的原有两个场景并加入缺包场景。逐字节核对了其余启动、事件和关闭合同以及其余主规范。同步后 `openspec validate --specs --strict --json` 通过 15/15，含活动 change 的 `openspec validate --all --strict --json` 通过 16/16。 [窗口呈现主规范](../../../specs/opengl-window-presentation/spec.md) · [产品入口主规范](../../../specs/runtime-lifecycle/spec.md#requirement-provide-one-real-product-entry)

### 归档结果

执行 `openspec archive establish-opengl-window-presentation --yes --skip-specs --json`，退出 0，归档至 `openspec/changes/archive/2026-11-11-establish-opengl-window-presentation/`。主规范已在上一步同步并校验。归档时为 15/16，第 6.3 项包含移动后的核对、开发计划更新和问题单重置。

移动后复算 6 份归档文件和 15 份主规范的摘要，再调整归档层级的相对链接。归档及开发计划的本地链接和锚点均通过核对。OpenSpec 无活动 change，归档后 `openspec validate --all --strict --json` 为 15/15 通过。开发计划记录 D04-T03 完成，当前工作包推进到 D04-T04 待立项。 [开发计划](../../../../Docs/Developments/Developments.md#current-stage)

问题单在归档核对后恢复为空模板，SHA-256 与模板一致，为 `a26066c0106ba8d1991974974acf06ddc62ed5d24e2878a42eda8f6fbec847fc`。实际重置后勾选 6.3，任务最终为 16/16。 [问题单模板](../../../../Docs/CodeReview.md.tpl)
