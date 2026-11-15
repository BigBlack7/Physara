# Tasks

## 1. 外部交接与帧内所有权

- [x] 1.1 由 Platform 为取得的 PresentColor 提供真实 entry/final 状态，驻留纹理注册提供明确的导入事实，同步公开合同及直接消费者。用一个新目标执行前取消、重新取得、正常图交接并提交、真实完成后再次取得的序列核对 `Common → ShaderResource`，确认取得和取消不会提前推进状态。复用现有 token/槽范围拒绝检查，迁移后的 Release 入口仍有明确拒绝。 [目标状态](specs/opengl-window-presentation/spec.md#requirement-acquire-a-bounded-window-presentation-target)
- [x] 1.2 建立当前帧图输出值、`FrameSlotUiFrameBindings`、请求确认数据及专用 graph arena，按实际公开依赖接通 `PhysaraRenderGraph`。接口注释说明图代、载荷借用及结束顺序，构建受影响目标核对依赖单向。用一个 setup 返回后输入变化、销毁候选图后载荷仍有效、后继图拒绝旧绑定的组合检查证明独立存储与图代边界。 [所有权设计](design.md#2-帧槽分别提供载荷与图对象存储)

## 2. View 和 UI 的图生产消费

- [x] 2.1 将诊断准备改为逻辑候选、拥有型像素和上传声明，已接受的匹配颜色显式导入。保留请求代、尺寸、预算、NoSceneOutputs 与有效设置合同，按实际消费者决定持久导出。在 CPU 消费者中对照有采样与无消费者的计划，核对版本、上传活性和声明/编译阶段没有原生分配。 [裁剪合同](specs/editor-frame-graph/spec.md#requirement-cull-unused-editor-producers-from-actual-ui-reads)
- [x] 2.2 将字体/托管纹理 create、区域 update 及占位上传接入图，暂存写和复制分为相依 Pass，保留区域外内容。按终结写 Pass 形成逐请求执行集合，临时 token 和实际句柄分别发布。用一张小原生图验证创建后采样、一次局部更新及区域外哨兵，并让另一个请求无消费者，核对只确认实际执行项及后续重试。复用同一图中的拒绝对照检查上传停止。 [纹理确认](specs/editor-ui-rendering/spec.md#requirement-confirm-texture-requests-only-after-their-upload-is-accepted)
- [x] 2.3 收集封存帧中实际普通 draw 的纹理引用，去重 token 与规范资源身份，保留当前写版本。实现 UI 几何上传、精确 Buffer 读取和 Raster 的 Pass 准备/执行，迁移旧独立 Record 入口及消费者。核对投影范围、局部偏移、空裁剪和 reset 语义，用已有 UI 图像输入或覆盖同一缺口的最小原生消费者验证像素。重复 Image、alias token 和过期身份在同组输入中核对一次更新、同版本采样及明确占位。 [UI 合同](specs/editor-ui-rendering/spec.md#requirement-compose-a-sealed-ui-frame-on-a-supplied-rhi-target) · [帧内解析](specs/editor-frame-graph/spec.md#requirement-resolve-ui-tokens-to-exact-frame-local-texture-versions)

## 3. Runtime 编译执行与接受发布

- [x] 3.1 将产品帧接入单一构图、编译和执行入口，按真实能力及现有预算建立编译输入，显式 clear 后合成并导出原 PresentColor。从存活计划生成上传准入清单，核对纹理写后 Buffer 写的实际次序。用可见诊断、全部诊断 Image 被裁剪、仅保留一个重复 Image、空 UI 四个配置检查存活工作和呈现版本，在同一容量检查中验证诊断压力占位及基础 UI Pending 时的取得归还。 [产品图](specs/editor-frame-graph/spec.md#requirement-compose-the-product-frame-through-one-compiled-graph) · [上传准入](specs/editor-frame-graph/spec.md#requirement-admit-the-uploads-that-will-actually-execute)
- [x] 3.2 将图导出、逐请求结果、View 请求代和同帧宿主回执关联，接通 UI/诊断发布及帧槽退休，清理被替换的固定顺序调用并同步流程说明。以一条正常接受序列确认正确有效设置和 NoSceneOutputs，再用准备分配失败、编译拒绝及已发上传后的 RHI 拒绝覆盖不同收拢边界，核对首错、临时身份回滚、无成功发布及前缀保活。后继失败仅在既有调用责任需要时补验。 [接受发布](specs/editor-frame-graph/spec.md#requirement-publish-only-accepted-graph-outputs)

## 4. A 产品集成验证

- [x] 4.1 构建受影响正式模块与 `Physara`，Debug 和 Release 均经同一产品入口运行。Release 以实际 OpenGL 完成票据和原生像素验证诊断四象限、字体/透明 UI、窗口底色、sRGB 单次编码与方向，并观测末端复制、提交和交换顺序。Debug 运行代表性成功及拒绝路径。记录源码和产物身份、命令、退出码、GL 设备、像素预期与结果，关联前面已经通过的图和 UI 检查。 [产品运行入口](../../../../Docs/Build.md#运行)
- [x] 4.2 在同一产品生命周期序列中验证隐藏/恢复面板、窗口与视口独立 resize、请求比例变化、在途旧绑定、槽背压、暂停恢复和最后一帧退出。核对稳定 token、新颜色代及匹配请求的有效设置，原代在真实完成前继续保活，安全复用时旧绑定失效。引用匹配的已有输入语义与宿主证据，仅对接线实际改变的边界补验。 [帧槽与换代](specs/editor-frame-graph/spec.md#requirement-retain-frame-bindings-and-resource-generations-until-safe-reuse)
- [x] 4.3 对照三份 delta 的每条场景填写本地证据映射，核对 UI 坐标/索引/reset/颜色合同及 View、帧槽、呈现的既有行为。记录实际运行、复用与未覆盖项，检查正式目标和注释、文档链接，运行本 change 的 OpenSpec 严格校验。已识别风险均有匹配证据后结束 A 本地验证。 [验证对应](#verification-map)

## 5. 独立 A/B 审查与注释

- [x] 5.1 A 在本地检查通过后填写当前问题单，交接基准 commit、实际暂存/未暂存 diff、新文件、检查入口、结果和限制。在本 tasks 留存 A 交接摘要，由维护者调度 B。检查材料按需引用实际存在的本地输入。 [当前问题单](../../../../Docs/CodeReview.md)
- [x] 5.2 B 独立核对 spec、实际实现和直接消费者，审查图依赖、导入事实、上传确认及在途寿命，通读并补齐本轮完整第一方文件的职责、接口合同与流程注释。按稳定 CR 编号记录问题、覆盖和验证要求，分别给出正确性及注释结论。 [审查技能](../../../../.agents/skills/physara-review/SKILL.md)
- [x] 5.3 A 按 CR 编号整改并自检，把 B 的注释纳入整改版，记录缺陷对应的最小验证及真实输入。无正式发现时记录适用的自检与整改不适用结论，由 A 勾选本项。
- [x] 5.4 B 复核整改版、匹配证据和最终注释，关闭或有依据地撤回正式问题，记录签收版本及限制，由 B 勾选本项。后续代码或构建输入变化时再次复核受影响范围。

## 6. 最终验证与归档

- [x] 6.1 B 签收后按 OpenSpec verify 将全部需求和场景对应到签收输入及原生结果，复用身份一致的证据并补实际缺口。在本 tasks 留存最终版本、A/B 结论、CR 摘要、注释覆盖、命令和限制。
- [x] 6.2 最终验证通过后同步新 `editor-frame-graph` 主规范，以及 `editor-ui-rendering`、`opengl-window-presentation` 的修改需求，核对已有场景完整保留并运行锁定版本的严格校验。
- [x] 6.3 留存验收结论后归档，核对任务、产物与链接，更新 D05-T04 完成状态及 D05-T05 前置。归档成功后按模板重置当前问题单。 [开发计划](../../../../Docs/Developments/Developments.md#current-stage)

<a id="verification-map"></a>
## 验证对应与停止条件

| 合同或风险 | 主要任务与最小证据 |
|---|---|
| 产品图的上传、初始化、合成、原对象导出及真实窗口 | 3.1 的计划对照与 4.1 的同一原生产品帧 |
| 重复/alias token、瞬态颜色、过期 token 和旧帧绑定 | 1.2 的图代检查与 2.3 的同组图像及声明 |
| 隐藏/全裁剪/保留一个 Image、裁剪请求后续重试 | 2.1、2.2 的生产者检查与 3.1 的最终读取集合 |
| 实际上传账单、UI 优先和基础 UI Pending | 3.1 的一次容量对照，核对图顺序与执行字节 |
| UI prepare 失败、不兼容输入、区域更新、上传或 draw 状态拒绝 | 2.2、2.3、3.2 中对应入口的代表性拒绝与首错 |
| 图接受发布、取消及失败前缀不确认 | 2.2 的逐请求确认与 3.2 的正常/失败回执 |
| 新目标状态、取消重取、已接受复用、token 拒绝 | 1.1 的状态序列及对应宿主入口 |
| 尺寸换代、稳定 token、在途引用、帧槽复用和暂停退出 | 4.2 的连续生命周期序列 |

开发消费者、故障控制、样例、脚本和日志仅置于 `build/d05-t04/`。检查使用正式模块和真实设备边界，独立像素预期取既有四象限颜色、已知底色、更新区域及线性合成计算。每个独立风险使用能区分正确与错误结果的代表性输入，同组证据可覆盖多个场景。L0 规划检查使用文档 diff、链接和 CLI 解析，L2 接口检查覆盖当前消费者，L3 图执行及在途行为使用真实 OpenGL/完成证据。 [验证技能](../../../../.agents/skills/physara-verification/SKILL.md)

2026-11-15 规划时，本地 `build/d05-t03/` 及早期工作包 fixture 已清理，正式 build 树仍存在。归档中的结果作为历史依据，运行前按实际输入确认可复用证据。需要的新消费者只补本次改变合同的缺口。每个验收场景具备当前输入匹配的实跑或复用依据，且覆盖限制已说明后停止扩大检查。D05-T05 的安全 UI-only 恢复、真实场景输出、跨帧资源池及性能测量按各自工作包推进。

<a id="planning-validation"></a>
## 规划验证

2026-11-15，规划基准为 `d780edab5683a9ae241741ec7c99cbb29619445b`。使用官方 OpenSpec 1.13.2 的 repo-local `spec-driven` schema 创建本 change，proposal、specs、design、tasks 四类产物齐备。共三项能力、九条需求和二十八个场景，十七项实施、审查及收尾任务均待执行。

`openspec validate integrate-editor-frame-graph --strict --json` 通过，问题数为零。八份本次涉及的 Markdown 共 201 个本地文件/锚点引用通过检查，三条 MODIFIED 需求保留了原有十三个场景，任务编号唯一且全部未勾选，`git diff --check` 通过。本轮完成 L0 规划文档检查，C++ 构建及原生验收按上面的实施任务执行。

<a id="a-local-verification"></a>
## A 实施与本地验证

本节保留 A 首次交接的实现与验证记录；B 首审及其后的整改输入、结论见下方 [CR-001 整改](#cr-001-correction)。

2026-11-15，A 已完成本轮实现及本地检查，交接基准仍为 `d780edab5683a9ae241741ec7c99cbb29619445b`，分支 `v2.0`。未提交或暂存本轮改动。实现与当前工作树交付给维护者调度的独立 B，任务 5.2–6.3 继续按各自责任推进。

### 实现结果

- Runtime 以一张图完成显式窗口 clear、诊断/托管/占位纹理上传、UI Buffer 上传及 Raster，导出原 PresentColor。上传准入从存活计划按实际次序生成，诊断压力触发一次正常占位候选重建，基础 UI Pending 在 Open 前归还取得。
- View 只准备逻辑候选和帧槽像素，实际纹理在存活 Pass 首用时物化。UI 长期 registry 保存稳定身份及已接受句柄，图版本和去重采样留在 `FrameSlotUiFrameBindings`。区域更新通过独立暂存写及复制保留原 mip 其余 texel，alias 使用当前写版本。
- Raster prepare 通过已声明的采样和 Buffer 权限建立集合，execute 逐条检查状态设置和绘制诊断。投影存储缩到生成 ABI 的 16 字节，使用完整视图，符合既有 RHI 对部分常量视图的 256 字节对齐合同。
- UI 按同图代、录制列表、存活终结 Pass、导出结果及有效颜色提交回执逐项确认。取消、被裁剪及失败前缀均不确认请求。Platform 由正常目标交接及接受提交推进 entryState，未知失败前缀停止再次取得并进入既有关闭路径。
- 帧槽有独立 graph arena，图回退不会覆盖载荷。GPU 纹理、绑定和上传 lease 仍由原 RHI 包及真实完成协议保活。Architecture、主 specs 和第三方内容未修改。

### 本轮检查输入

所有本地消费者、故障控制、构建产物和日志均在 `build/d05-t04/`。正式 CMake 没有开发检查目标，不依赖这些本地文件。B 代码审查只覆盖正式实现及直接消费者，按需引用本地结果。

- `verify/Verify.cpp` 与 `verify/CMakeLists.txt` 是本次新增的局部消费者，链接当前 Debug/Release 正式库。产品生命周期段直接编译使用当前 `Runtime/Main.cpp` 的 `RenderFrame`，独立 `Physara.exe --hidden --poll-limit=6` 另行确认正式入口。
- `Evidence.py` 在运行前后核对源码、检查输入、库、Shader 包和 exe 身份，写入 `source-artifact-manifest.json`、`accepted-runs.json`。源清单覆盖根 CMake 及 Engine、Editor、Platform、Runtime、RHI、Shaders 的当前文件，包含新文件，未修改第三方以基准 commit 定位。
- 当前源码集合 SHA-256 为 `953754251a03294a555399b654ed19cdbb19ac4c734c57dc2752ee3d15c7ed57`。检查输入集合 SHA-256 为 `6fb27ff17785cb4134ac74e689eba0f7a109bd7ae82fec661242b3c0fc4ae622`。各配置的产品、消费者、正式库及包哈希见上述 manifest。
- native 接受日志为 `native-debug-accepted.log` 和 `native-release-accepted.log`，产品日志为 `product-debug-accepted.log` 和 `product-release-accepted.log`。四次进程退出码均为 0，产品没有 stderr 输出，原生消费者以 `ALL PASS` 结束。

### 命令与结果

在仓库根执行，检查配置与所用正式库、Shader 包相同。

```powershell
cmake --build build --config Debug --target Physara --parallel 2
cmake --build build --config Release --target Physara --parallel 2
cmake -S build/d05-t04/verify -B build/d05-t04/verify-build -G "Visual Studio 17 2022" -A x64
cmake --build build/d05-t04/verify-build --config Debug --parallel 2
cmake --build build/d05-t04/verify-build --config Release --parallel 2
python build/d05-t04/Evidence.py
```

正式构建保留 `/W4 /WX /utf-8 /permissive-` 和配置原有优化、Debug 迭代器检查。最终构建日志为 `build-product-debug-final.log`、`build-product-release-final.log`、`verify-build-debug-handoff.log`、`verify-build-release-handoff.log`，均退出 0。首次沙箱构建因 SDK 元数据目录不可读返回 MSB4184，允许访问已安装 SDK 后继续。早期同输入重试分别遇到 STL `unordered_map`、`vector` 中的 MSVC C1001，原日志 `build-editor-debug-c1001.log`、`build-editor-debug-retry.log` 保留，未降低编译选项。后续正式构建通过，未把失败构建的旧 exe 作为接受证据。

检查使用 Windows x64、MSVC 19.44 工具链、SDK 10.0.26100.0 和 Intel RaptorLake-S Mobile Graphics Controller，OpenGL `4.6.0 - Build 32.0.101.6129`。Debug 与 Release 同组验证结果一致。

### 原生证据分组

| 代号 | 本轮实际结果 |
|---|---|
| N1：图生产、绑定及像素 | 两个诊断 Image 共用版本，首次成功图为 8 Pass、3 个去重采样、3 次纹理写及 3 次 Buffer 写。prepare/声明/编译期间原生纹理创建计数不变。修改源顶点和像素后结果保持原值，非零 DisplayPos、2x FramebufferScale、VtxOffset、IdxOffset 和 reset 均经实际像素检查。图销毁后载荷 arena 游标保持，graph arena 归零。 |
| N2：逐请求、区域更新及裁剪 | 未使用创建保持 WantCreate 且撤销临时 TexID，后续 Image 才确认。一次 3×3 区域更新后，托管 token 和 resident alias 同时读到 `(220,30,90,255)`，外侧哨兵保持 `(20,80,140,255)`。过期 token 显示 `(255,0,255,255)`。两个 Image 全裁剪及空 UI 都只保留 1 个 clear Pass，上传清单为空；保留一个 Image 时只有一次诊断上传。 |
| N3：取消及失败 | 在临时 token 安装后定点触发 CPU bad_alloc，确认有界错误、临时身份撤销及载荷回退。不兼容目标和编译能力拒绝均在命令前发生。已发前缀后的错误保留首个 Pass 诊断，取得前缀 ticket，颜色资格及交换为 false，更新仍 Pending。 |
| N4：依赖命令停止 | 在真实 GL 上传入口定点拒绝暂存写，驻留更新区域保留原像素且后继 draw 为零。另一轮先成功发出一个合并 draw，再在后续纹理绑定状态设置时报告 RHI 错误，后继索引 draw 不发出。两轮均保留前缀并阻止请求确认。 |
| P1：正式产品图及窗口 | 当前 Runtime::RenderFrame 的 PresentColor 和末端默认后缓冲四象限均为 `(180,71,53)`、`(41,109,149)`、`(55,127,82)`、`(118,75,161)`，alpha 255。底色为 `(36,43,53,255)`。原生字体样本可见，半透明红色线性合成为 `(189,28,36,255)`，独立预期 `(189,29,36,255)`，容差 2。末端复制在真实 fence 前，接受票据后成功交换，方向与单次 sRGB 编码正确。 |
| P2：生命周期与资源代 | 经真实槽和提交执行比例 1.0→0.75、窗口 1000×720→1200×800、独立视口换代，旧原生纹理在 owner 换代后仍存在，完成观察和回收后删除。三个槽不可复用期间绑定图代与 arena 不变，真实完成后安全重用拒绝旧图绑定。Inspector/Viewport 标签切换隐藏及恢复实际消费者，原生最小化/恢复暂停及恢复 UI，末帧交正式 Shutdown 收拢。 |
| P3：真实上传池压力 | 用有效已提交上传包分别占用 63 MiB 和 64 MiB 共享池。前者使诊断候选改为占位图并保留原有效设置，后者配合合法的大 UI 几何使基础 UI Pending，Open/提交前归还取得且无新 fence。退休占用后，同一正常产品入口恢复诊断并确认设置。 |

N3 的 CPU 失败为一次受控分配注入，N4 为原生调用边界的定点拒绝注入，不声称自然内存耗尽或真实驱动丢失。P2/P3 暂时延迟 `glClientWaitSync` 完成观察以固定背压时序，释放控制后必须由真实 GL fence 完成才能复用，未声称测得自然 GPU 忙时长。像素探针只在本地消费者中读取原生结果。未新增性能结论。

### 三份 delta 的场景映射

| 能力 / 场景 | 证据 |
|---|---|
| editor-frame-graph / Present an editor frame with graph-produced diagnostic color | N1、P1、两配置正式产品入口 |
| editor-frame-graph / Export an initialized target from an empty UI draw set | N2 的 clear-only 计划和底色 |
| editor-frame-graph / Sample an updated texture through repeated or aliased tokens | N1 去重及 N2 的 alias 新版本像素 |
| editor-frame-graph / Resolve a newly produced view color during execution | N1 的无原生准备/编译及执行后像素 |
| editor-frame-graph / Diagnose a stale token and reject an old frame binding | N2 占位、N1/P2 图代拒绝 |
| editor-frame-graph / Hide or fully clip the only diagnostic consumer | N2 全裁剪、P2 后台标签和恢复 |
| editor-frame-graph / Keep a producer for a remaining Image reference | N2 单个存活 Image、单次上传 |
| editor-frame-graph / Exclude a culled texture request from upload admission | N2 裁剪请求清单、状态及后续重试 |
| editor-frame-graph / Preserve UI priority under upload backpressure | P3 的两级压力及恢复 |
| editor-frame-graph / Accept a sampled view version with its matching submission | P1/P2 的有效设置及原对象交接，仍为 NoSceneOutputs |
| editor-frame-graph / Preserve pending state when graph work fails | N3/N4 前缀、首错、无有效颜色与 Pending |
| editor-frame-graph / Resize while a previous binding remains in flight | P2 的旧对象保活及真实完成后删除 |
| editor-frame-graph / Reuse a slot only after its submission completes | P2 三槽 Retry、存储保持及完成后图代变化 |
| editor-ui-rendering / Record a sealed editor frame into an offscreen target | N1、P1 的真实像素 |
| editor-ui-rendering / Reject incompatible rendering inputs | N3 的不兼容颜色目标和能力拒绝 |
| editor-ui-rendering / Cancel a partial preparation on CPU allocation failure | N3 临时身份后的分配失败 |
| editor-ui-rendering / Stop after rejection of a draw's graphics state | N4 后续状态拒绝，已发 draw 保活 |
| editor-ui-rendering / Leave a paused iteration without UI work | P2 原生暂停时无 DrawData、取得返回 Suspended |
| editor-ui-rendering / Publish a managed texture after its first accepted upload | N1 首次上传采样与 N2 逐请求 OK |
| editor-ui-rendering / Apply an independently owned texture update | N2 区域像素、哨兵和相同 token，N1 拥有型载荷 |
| editor-ui-rendering / Stop recording at a rejected upload | N4 暂存拒绝、原区域不变及零 draw |
| editor-ui-rendering / Keep a request pending after cancelled or failed work | N3/N4 回滚、Pending 和前缀 ticket |
| editor-ui-rendering / Confirm only the executed requests in a successful frame | N2 一项接受、一项裁剪及下轮真正消费 |
| opengl-window-presentation / Acquire a renderable window target | N1 合法目标、P1 原窗口对象 |
| opengl-window-presentation / Suspend acquisition without a substitute target | P2 原生最小化及 Suspended |
| opengl-window-presentation / Reject an expired presentation identity | N1 取得号失配和槽 3 的 Release 拒绝 |
| opengl-window-presentation / Rebuild one slot using the newest framebuffer extent | P2 窗口换代及独立视口范围 |
| opengl-window-presentation / Describe first use cancellation and accepted reuse | N1 Common→取消→Common→正常提交→真实完成→ShaderResource |

### 覆盖与停止条件

本轮实际运行了上述构建、原生消费者与正式入口，没有将早期工作包的缺失 fixture 或历史 exe 作为当前通过依据。未重新验证未改变的完整输入路由/多显示器矩阵、其它 GPU/OS、32 位 ImDrawIdx 配置、自然 OOM/device loss、Scene 历史或性能。D05-T05 的安全 UI-only 错误恢复和完整失败矩阵仍在后续范围。本轮可接受目标的独立风险已有匹配证据，结束扩大原生检查。

L0 检查通过。10 份本轮 Markdown 的 229 个本地文件/锚点引用无缺失，`git diff --check` 通过，旧 Record 路径和 UiPrepareStatus 消费已清理。官方 OpenSpec 1.13.2 的 `validate integrate-editor-frame-graph --strict --json` 返回 valid=true、issues=[]。当前进度为 11/17，仅勾选本轮 A 已完成的任务。B 还须独立审查实际实现并完善完整文件注释，A 本地通过不代表最终验收。

<a id="a-handoff"></a>
## A 首次交接

维护者可依据 [当前问题单](../../../../Docs/CodeReview.md) 调度 B。本轮正式变更包括 Renderer 的 FrameGraph helper 与 View、Editor 的准备/绑定/逐请求确认、Platform 的目标状态事实、Runtime 的产品图和 Engine 的实际目标依赖。新正式源码仅 `Engine/Renderer/FrameGraph.hpp`、`FrameGraph.cpp`，另有本 change 的规划文件；实际清单、diff 与哈希保存在本地交接材料。B 字段和任务 5.2、5.4 保持未填写/未勾选，整改任务 5.3 待 B 反馈，最终 verify、同步和归档尚未执行。

<a id="cr-001-correction"></a>
## CR-001 · 准备后注销驻留纹理的所有权整改

2026-11-15，B 首次 Review/Annotation 已在 [当前问题单](../../../../Docs/CodeReview.md) 记录一项 P2。A 按 B 指定算法核对 17 个受审源码/CMake 文件，当前实际输入集合为 `74e6bfe6f7c2c781809069b6cfd198110a46d94594a5eb12d76b57bcb224f3cc`，与 B 注释交接版完全一致。六份 OpenSpec 上下文文件仍与 A 首次交接时的内容一致。保留 B 的完整文件注释，本轮只修正 UiRenderer.cpp、UiTextures.cpp/.hpp 的驻留快照及相关说明。

修正后的可观察行为为：可见驻留 Image 成功准备后，注销 token 并释放外部句柄，原批次仍可构图、执行并提交原像素；下一帧的旧 token 显示占位。准备值、图和 GPU 提交逐段拥有原对象，真实完成后退休；取消准备释放快照并允许后继准备。快照按 token 去重，不保存 GraphId 或提前创建 BindingSet，alias 仍沿规范导入取得当前写版本。

验证范围为一个通过公开 EditorSession 准备/注销入口的直接消费者序列，包含原批次、下一帧占位、真实退休及同类准备的取消出口，在 Debug/Release 核对；复用既有 NativeGraphCases 的 alias/更新/裁剪/准备失败检查入口进行受影响回归，并用两个配置的正式产品入口做短运行。原生窗口多尺寸/完整输入等未改变范围不重复扩展。开发材料单独放在 `build/d05-t04/cr-001/`，保留首次 A 交接证据，不覆盖原日志或 manifest。

整改前以 B 输入重新构建 Debug 模块及直接消费者，观察到注销和外部释放后原生纹理已不存在，构图返回 `Expired UI graph token`，进程按预期退出 1。对应输入、产物及日志记录在 `b-input.json`、`before-run.json`、`before-native-debug.log`。整改结果如下。

### A 整改与自检结果

- 已完成 `UiResidentSnapshot`：普通可见外部驻留引用在 Prepare 中按 token 去重捕获强引用及 `UiTextureImport`，成功后移交 UiBatch。BuildGraph 优先使用本批快照，导入事实和规范版本选择沿原合同处理；注销只移除长期 registry 条目。Finish/Cancel 结束准备 owner，图及 RHI 继续保活已交接引用。B 的其余注释保持原字节，相关三份文件仅补修正所需说明。
- Debug/Release 正式 `Physara` 和 CR-001 消费者构建均退出 0，保留原 `/W4 /WX /utf-8 /permissive-` 及配置选项。本次未遇到 C1001，也未降低检查。命令与构建日志均在 `build/d05-t04/cr-001/`。
- 两配置的公开 `EditorSession::PrepareUiFrame → UnregisterUiTexture → 释放外部句柄 → PreparedUiFrame::BuildGraph` 序列均通过。原批次的两个 Image 实测 `(44,130,211,255)`，下一帧旧 token 实测 `(255,0,255,255)`。原准备值及图均结束后，RHI 包仍持有纹理，真实完成观察和回收后纹理删除。另一次同类准备取消后释放纹理，并能继续准备，未遗留 pending owner。
- 既有 NativeGraphCases 在 Debug/Release 均通过，覆盖受影响的 alias 当前写版本、区域更新与哨兵、裁剪请求、过期 token、拥有型载荷和部分准备分配失败。两个配置的正式 `Physara.exe --hidden --poll-limit=3` 均退出 0，无 stderr。
- Debug 的 CR-001 直接序列复用本轮修正构建后已经完成、输入未变的运行；其余五次检查在身份记录脚本中执行。`corrected-runs.json` 区分本轮实际执行与同输入复用，六项结果均为退出 0。未重复全部窗口尺寸/输入矩阵或 N4 注入组，未声称 B 复核通过或最终 verify。

源/产物身份：完整当前源码集合 SHA-256 `99c4640e79546a7e7dd372a0936b72ddca17df6ecb3ca2bf3dd39be93689d2ca`；按 B 的相同 17 文件算法计算的整改版集合 `70f5ec5f1d2365d6a577be0ac4487e28e645e071929a6e919f963afa1ded1d78`；本地消费者集合 `78911d17f67c4e6d2b9edf2419844a92fc98328d81894a2011605806b1ec3cb5`。逐文件、库、Shader 包、产品与消费者 exe 的哈希位于 `corrected-manifest.json`。`correction.diff` 只比较本轮三个文件与保存的 B 注释输入，不混用整个 change 的初次实现 diff。

```powershell
cmake --build build --config Debug --target Physara --parallel 2
cmake --build build --config Release --target Physara --parallel 2
cmake -S build/d05-t04/cr-001 -B build/d05-t04/cr-001-build -G "Visual Studio 17 2022" -A x64
cmake --build build/d05-t04/cr-001-build --config Debug --parallel 2
cmake --build build/d05-t04/cr-001-build --config Release --parallel 2
python build/d05-t04/cr-001/Evidence.py
```

构建日志为 `build-corrected-debug.log`、`build-corrected-release.log`、`build-consumer-debug.log`、`build-consumer-release.log`。直接序列日志为 `corrected-native-debug-initial.log`、`cr-001-release.log`，既有回归为 `existing-native-debug.log`、`existing-native-release.log`，产品短运行为 `product-debug.log`、`product-release.log`。设备仍为 Intel RaptorLake-S Mobile / OpenGL 4.6.0 Build 32.0.101.6129。延迟完成观察只用于固定在途保活时序，释放该控制后使用真实 GL fence 判断完成；不声称测得自然 GPU 忙时长。未修改正式构建目标、RHI、Architecture、主 specs 或历史归档。

A 只勾选任务 5.3 及问题单的 A 整改项。B 首次审查已按问题单完成，但 B 按维护者限定未编辑 tasks，因此 5.2 仍留给 B 自行勾选；5.4 的复核和 6.1–6.3 收尾保持待办。待维护者调度 B 核对上述整改版及三份文件的相关注释后，再按流程进入最终验证。

L0 收尾通过：10 份本轮 Markdown 的 253 个本地文件/锚点引用无缺失，`git diff --check` 通过，官方 OpenSpec 1.13.2 strict 返回 valid=true、issues=[]。任务勾选进度为 12/17，仅新增 A 的 5.3；B 的任务及最终收尾项保持待办。

<a id="final-verification"></a>
## 最终验收与 B 签收留存

2026-11-15，维护者反馈 B 复核完成并要求按流程继续。当前基准仍为 `d780edab5683a9ae241741ec7c99cbb29619445b`、分支 `v2.0`。下述最终记录取代前面交接时的待办状态；原 A/B 过程及输入指纹作为历史保留。

### 独立审查闭环

B 已在当前问题单亲自勾选 CR-001 的复核框，结论为“代码与注释均通过，本项关闭；未发现新增正式问题”。A 本次核对其签收的 17 文件集合为 `a3c560b871d08762b81f60e05f38a791fee80ad54cc754ac48171d9d62c7cf79`，与实际源码完全一致。按保存的 A 整改输入复原并比较，B 后续只修改 UiRenderer.cpp、UiTextures.cpp/.hpp、UiFrame.hpp 的注释，17 文件代码 token 均未变化。任务 5.2、5.4 的勾选由已签署问题单同步，责任人及结论仍为 B，不是 A 重新作出审查或代替签收。

唯一正式问题 CR-001（P2）为成功准备后、构图前注销驻留纹理导致原批次丢失依赖。A 通过按完整 token 去重的拥有型驻留快照保留原对象和导入事实，图再接管引用，实际绑定仍由 Pass prepare 创建。B 复核确认槽位复用不会替换旧快照，alias 保留当前写版本，准备失败、Finish/Cancel 与 RHI 退休责任明确，公开注销合同及注释一致。本项已关闭，无撤回问题和新增问题。

B 的完整注释覆盖包括 FrameGraph.hpp/.cpp、View.hpp/.cpp、UiFrame.hpp、UiRenderer.hpp/.cpp、UiTextures.hpp/.cpp、EditorSession.hpp/.cpp、DeviceHost.hpp、DeviceHostInternal.hpp、Presentation.hpp/.cpp、Runtime/Main.cpp 和 Engine/CMakeLists.txt，共 16 个 C++ 文件及 1 个 CMake 文件。职责、公开合同、快照和载荷借用、图版本、逐请求确认、状态交接、失败前缀及完成后退休均通过 B 的源码与注释复核。B 没有复跑原生消费者，最终原生证明由 A 对签收输入完成，二者不混记。

签收问题单原文在重置前保存到本地 `build/d05-t04/final/b-signed-review.md`，其摘要、结论与版本在本节持久留存。B 签收输入及注释差异见 `b-acceptance.json`、`b-comment-diff.diff`。Architecture、第三方和其它历史 change 不因本次收尾改写。

### 最终原生验证

使用 B 签收源码重新构建 Debug/Release 的正式 Physara、已有 D05-T04 原生消费者和 CR-001 直接消费者，六次构建均退出 0，未降低编译选项。随后两配置各运行完整既有原生场景、CR-001 序列和正式产品入口，六次进程均退出 0。未新增测试矩阵；复用现有消费者源文件，在本次签收产物上重新执行。

```powershell
cmake --build build --config Debug --target Physara --parallel 2
cmake --build build --config Release --target Physara --parallel 2
cmake --build build/d05-t04/verify-build --config Debug --parallel 2
cmake --build build/d05-t04/verify-build --config Release --parallel 2
cmake --build build/d05-t04/cr-001-build --config Debug --parallel 2
cmake --build build/d05-t04/cr-001-build --config Release --parallel 2
python build/d05-t04/final/Evidence.py
```

当前全源码集合 SHA-256 为 `8873ba10c65d4551e1e9f5c324c66c6e32336341a29067c4207c08a0abc4a823`，两套消费者输入集合为 `20cf34f6fa9b91f107ddbc783d033dbb4b4dac23a7b30c4ba8346bd86b877566`。构建库、Shader 包、产品及消费者 exe 的逐项身份见 `final/source-artifact-manifest.json`，运行命令、退出码和日志摘要哈希见 `final/runs.json`。运行前后源码、消费者和产物保持一致，签收输入未被后续代码编辑改变。

| 当前输入的运行 | Debug / Release 结果 |
|---|---|
| 既有原生消费者 | `native-debug.log`、`native-release.log` 均 ALL PASS，覆盖前述 N1–N4、P1–P3 的 28 场景映射 |
| CR-001 公开入口序列 | `cr-001-debug.log`、`cr-001-release.log` 均 PASS，原批次两个 Image 为 `(44,130,211,255)`，下一帧占位为 `(255,0,255,255)`，真实退休及取消后重准备通过 |
| 正式 Physara | 两配置 `--hidden --poll-limit=6` 均退出 0，无 stderr，日志为 `product-debug.log`、`product-release.log` |

测试设备仍为 Intel RaptorLake-S Mobile Graphics Controller / OpenGL 4.6.0 Build 32.0.101.6129，Windows x64、MSVC 19.44、SDK 10.0.26100.0。原生证据覆盖实际像素、上传与采样依赖、重复/alias、裁剪、逐请求确认、窗口复制/fence/交换次序、尺寸换代、旧引用退休、暂停恢复和上传压力。CPU 分配/RHI 拒绝为定点注入，背压通过延迟完成观察固定时序，解除后仍由真实 fence 判定完成。没有扩大其它 GPU/OS、多显示器、32 位 ImDrawIdx、自然 OOM/device loss、Scene 历史或性能覆盖；D05-T05 的安全 UI-only 错误恢复和完整失败矩阵仍属后续范围。

### Verify 的需求与实现对应

以下九条需求均有当前代码与本轮原生证据。前文逐场景表的 28 行映射全部沿用本轮重新运行的 N/P 分组，并补入 CR-001 序列，不把任务勾选或 B 静态审查当成原生执行证明。

| 需求 | 签收实现入口 | 当前证据 |
|---|---|---|
| Compose the product frame through one compiled graph | [Runtime/Main.cpp](../../../../Runtime/Main.cpp) 的 RenderFrame（206 行起），导出原目标并串接宿主回执 | N1、P1、空 UI 的 N2 |
| Resolve UI tokens to exact frame-local texture versions | [UiTextures.cpp](../../../../Editor/ImGui/UiTextures.cpp) 的 Resolve（357）、ResolveGraph（494）；[UiRenderer.cpp](../../../../Editor/ImGui/UiRenderer.cpp) 的 BuildGraph（707）、PrepareRaster（616） | N1、N2、CR-001 |
| Cull unused editor producers from actual UI reads | [View.cpp](../../../../Engine/Renderer/View.cpp) 的 Declare（171），UiRenderer 的普通 draw 过滤、条件导出及已编译计划 | N2、P2 |
| Admit the uploads that will actually execute | [FrameGraph.cpp](../../../../Engine/Renderer/FrameGraph.cpp) 的 CollectGraphUploads（104），Runtime 的准入及一次候选重建 | N2、P3 |
| Publish only accepted graph outputs | UiTextures::Finish（538）、PreparedUiFrame::Finish（914）、View::Publish（198）及 Runtime 的 colorValid 判定 | N2–N4、P1、P3 |
| Retain frame bindings and resource generations until safe reuse | Runtime 的固定槽/双 arena、驻留快照及图/RHI 引用交接 | N1、P2、CR-001 |
| Compose a sealed UI frame on a supplied RHI target | [EditorSession.cpp](../../../../Editor/Session/EditorSession.cpp) 的 PrepareUiFrame，UiRenderer::Prepare（472）、ExecuteRaster（657） | N1、N3、N4、P1、P2 |
| Confirm texture requests only after their upload is accepted | UiTextures 的请求准备、终结写 Pass 及逐项 Finish | N2–N4 |
| Acquire a bounded window presentation target | [Presentation.cpp](../../../../Platform/Presentation.cpp) 的 BeginPresentation（66）、SubmitPresentation（194），真实 Common/ShaderResource 交接 | N1、P2 |

完整性：实施和独立审查项 1.1–5.4 共 14 项均有完成记录；本次最终 verify 对九条需求和 28 场景已全部完成。正确性：上述映射及当前输入的六次运行均通过，CR-001 已关闭。设计一致性：模块依赖保持单向，CPU 声明/执行物化、驻留快照、帧槽存储、逐项确认及唯一同步责任符合 design；原有 C++/Shader/CMake 约定和开发材料隔离保持。行为与设计检查未发现 CRITICAL、WARNING 或 SUGGESTION，未跳过适用检查。此时任务 6.2（同步）和 6.3（归档）尚在收尾流程中，必须完成后才报告整个 change 完成。

### 规范同步与归档执行

已按锁定 CLI 的一次 specs-rule 快照完成三项能力同步，返回未配置额外 rules。新增 editor-frame-graph 的 6 条需求，修改 editor-ui-rendering 的 2 条和 opengl-window-presentation 的 1 条。三条修改需求的 13 个原场景均保留，范围外需求和已有 Purpose 未改变；逐条回读确认 delta 已完全反映在主规范。同步后 `openspec validate --all --strict --json` 通过 23/23（22 个主规范及本 change）。

归档前实施、独立审查、最终 verify 和同步均已完成，任务为 16/17，仅 6.3 的归档/计划更新/问题单重置正在执行。按维护者本次继续流程的授权执行归档，6.3 在移动、链接核对、计划更新及问题单重置实际完成后才勾选，不提前标记完成。最终 verify 的任务完整性将在收尾后更新为全部完成。

### 归档完成核对

2026-11-15，官方 OpenSpec 1.13.2 的 `archive integrate-editor-frame-graph --skip-specs --yes --json` 成功归档至 `openspec/changes/archive/2026-11-15-integrate-editor-frame-graph/`。`--skip-specs` 用于避免重复写入本次已经合并并逐条核对的主规范。归档前仅 6.3 的实际归档动作尚在执行，现已完成移动、45 个相对链接调整、开发计划更新及问题单按模板重置，任务 17/17 完成。

| Verify 维度 | 最终结论 |
|---|---|
| 完整性 | 17/17 任务完成，九条 delta 需求具备实际实现，三项主规范同步完成 |
| 正确性 | 九条需求实现映射、28 个场景及 CR-001 补充序列通过，签收输入的六次运行全部退出 0 |
| 一致性 | design 决定与项目模式一致，B 代码及完整文件注释签收，源/产物身份保持一致 |

最终未留 CRITICAL、WARNING 或 SUGGESTION，未跳过本 change 的适用验证项。归档后 22/22 主规范严格校验通过，活动 change 列表为空；13 份涉及文档的 236 个本地文件/锚点引用通过，`git diff --check` 通过。当前源码仍匹配 B 签收版及最终原生记录，Architecture 与此前归档保持原字节。D05-T04 验收完成，D05-T05 前置已齐备、待建立下一 change，D05 整阶段出口仍由 D05-T05 完成。
