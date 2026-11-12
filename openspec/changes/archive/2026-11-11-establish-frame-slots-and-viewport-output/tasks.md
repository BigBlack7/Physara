# Tasks

## 1. A 接入帧内存储、呈现槽与完成票据

- [x] 1.1 在 Core 实现有界 FrameArena，由 Runtime 槽提供给 Editor 的实际 UI 顶点、索引和拥有型纹理上传。一次 CPU 检查覆盖带填充的最后合法范围、非法对齐、运算溢出和容量不足，核对失败保留已有范围及游标。用一次准备值结束后再尝试录制或确认，核对旧借用失效和 ImGui 输入改写后已复制数据的独立性。 [存储设计](design.md#frame-arena) · [帧存储合同](specs/frame-slot-lifecycle/spec.md#requirement-prepare-bounded-frame-owned-cpu-data)
- [x] 1.2 按固定索引组织宿主的 PresentColor、framebuffer、目标代、活动取得及 Event。用实际 OpenGL 宿主核对合法索引取得、Release 拒绝越界及旧槽 token、仍在途槽的原目标未被更换、完成后仅该槽按最新有效尺寸重建。窗口颜色、复制与状态缓存沿现有路径。 [窗口目标合同](specs/opengl-window-presentation/spec.md)
- [x] 1.3 在 Runtime 接通 3 槽调度、有限背压和同槽提交回执。用一次有真实 fence 的受控 Pending 序列占满槽，核对推进 CPU 发布及交换结果均不触发 reset，忙时继续处理事件且没有替代槽，解除等待条件后只回收获得真实完成的槽。检查背压记录可与 `PresentWait` 分别观察。 [调度设计](design.md#frame-slots) · [复用合同](specs/frame-slot-lifecycle/spec.md#requirement-reuse-a-fixed-number-of-slots-after-real-completion)
- [x] 1.4 接通准备取消、已发前缀的非零票据、缺票据或完成观察失败的停止和收拢。优先重用 D04-T03 的 prefix-error、no-ticket、poll-error 检查材料并适配索引，以实际结果核对已发前缀只保护依赖，观察失败后未发布新工作、没有槽 reset 或假完成。另核对已接受票据后观察者未建立时的真实回执仍保留；仅在现有检查无法区分这条路径时加定向用例。 [收拢设计](design.md#frame-retirement) · [前置原生证据](../2026-11-11-establish-opengl-window-presentation/tasks.md#final-verification)

## 2. A 接入视口身份、诊断颜色与基础字体

- [x] 2.1 接通主 View 的稳定类型身份和现有 UI 纹理注册，Editor 由内容区域发布可检查的像素请求，在当前帧绑定有效输出代。用一次视口尺寸更新及隐藏再打开核对 token 稳定和零面积不分配，再用一个失效 token 核对仅得到带原因的占位。 [绑定设计](design.md#viewport-output) · [视口身份合同](specs/editor-viewport-output/spec.md#requirement-resolve-a-stable-viewport-token-to-the-current-output-version)
- [x] 2.2 用公共 RHI 完成测试纹理的新代写入、采样和接受后的发布，并接通 `NoSceneOutputs`。用同一可辨认的不对称图案核对窗口显示、独立于 PresentColor 的尺寸及实际采样代，观察先前提交使用的旧代在完成后退休。用一次未就绪输出核对仍有清屏、UI、诊断占位，核对有票据的测试图和占位均不推进 View 历史。 [输出与历史合同](specs/editor-viewport-output/spec.md#requirement-present-a-diagnostic-editor-without-advancing-scene-history)
- [x] 2.3 将正式 `EditorUi` 程序、基础字体准备与有效上传回执纳入产品启动结果。核对实际首帧上传先于采样，基础字体初始化失败有明确阶段、无成功呈现并返回 1，程序包失败复用匹配的已有检查。触发一次真实 atlas 更新或重建，在旧使用仍在途时确认新代按协议上传及确认、旧代等到最后使用完成。 [装配与录制](design.md#frame-order) · [产品入口合同](specs/runtime-lifecycle/spec.md)

## 3. A 验证实际产品帧路径

- [x] 3.1 构建受影响的 Debug/Release 产品和同配置 UI 包，用默认可见入口验证 Docking、测试视口、连续窗口 resize、独立拖动视口及最小化恢复。核对当前逻辑与物理尺寸及有效比例、两类目标各自换代和恢复的输入处理。实际 DPI 变化无法取得时，明确记录原生覆盖范围，用一个不同 framebuffer 比例的直接消费者检查补足换算证据。检查请求在录制前封存，录制只消费当前帧；更新并核对构建说明的运行入口和实际帧路径。 [尺寸合同](specs/editor-viewport-output/spec.md#requirement-apply-viewport-extent-at-a-frame-boundary) · [构建说明](../../../../Docs/Build.md)
- [x] 3.2 用正式产品的隐藏有界入口核对同一渲染路径，验证 close/poll limit 决策后无下一帧。用一组最后一帧仍有真实在途访问的原生退出记录，确认先停止新工作和释放会话输入接入，必要的提交引用保持到设备收拢，后续才销毁 context。结合前项失效观察验证，检查暂停、背压和普通 resize 中没有全设备等待。 [暂停及关闭合同](specs/frame-slot-lifecycle/spec.md#requirement-keep-input-responsive-during-suspension-and-retire-on-exit)

开发检查使用受影响的现有构建与实际消费者。需要补充的代码、脚本、故障控制和临时工程集中于 `build/d04-t04/`。同一真实风险已有输入、环境、检查与产物匹配的结果即可引用。按任务记录检查入口、版本和限制。证据覆盖槽复用、输出换代、基础字体及退出合同后，只在输入变化、失败或可说明的缺口处补查。 [验证约定](../../../../.agents/skills/physara-verification/SKILL.md)

## 4. 独立 A/B 审查与注释闭环

- [x] 4.1 A 实现并完成本地检查后，在当前问题单记录基准 commit、实际暂存/未暂存修改及新增文件、正式构建及二进制和包身份、必要的本地检查输入、实际运行与复用结果、未覆盖范围。由维护者将对应工作树及证据交 B。 [问题单留存](#review-closure) · [交接规则](../../../../.agents/skills/physara-review/SKILL.md)
- [x] 4.2 B 独立核对工作树内容和实际消费者，覆盖有界槽及真实完成、失效借用、窗口和视口换代、字体协议、`NoSceneOutputs` 与退出。记录发现和证据身份，补齐本轮完整第一方文件的职责、接口和关键流程注释。
- [x] 4.3 A 按问题单的稳定编号整改，核对受影响的注释和回归，记录对应版本、结果及范围。无正式整改项时记录适用的自检结果。 [整改与证据](#a-correction-verification)
- [x] 4.4 B 对实际修正版、匹配回归及注释独立复核，按编号关闭或说明撤回正式发现，分别签收代码正确性和注释覆盖。 [审查闭环](../../../../AGENTS.md#review-loop)

## 5. A 最终验证与归档

- [x] 5.1 B 签收后执行 OpenSpec verify，将新增及修改需求对应到最终实现、签收版本的原生结果和可见行为。只补实际缺口，区分本轮运行、复用与未覆盖范围，保留最终验收记录。 [最终验证](#final-verification)
- [x] 5.2 最终验证通过后同步 `frame-slot-lifecycle`、`editor-viewport-output` 的 Purpose 与全部新增需求，以及 `opengl-window-presentation`、`runtime-lifecycle` 的完整替换需求。核对保留场景及其它主规范，执行严格校验。 [主规范同步](#spec-sync)
- [x] 5.3 在 change 留存 A/B 签收、修订及最终验证证据，按原生流程归档。更新工作包状态及归档入口，核对归档后重置当前问题单。 [归档结果](#archive-result) · [开发计划](../../../../Docs/Developments/Developments.md#current-stage)

<a id="a-local-verification"></a>
## A 实现与本地验证

使用 `v2.0` 基准 `c55b68efa838ce191a54cd35def45f19fa6df2d6`。交接输入包含 19 个有改动的已跟踪产品文件、6 个未跟踪产品文件和当前 OpenSpec/开发文档。正式构建采用 MSVC 19.44.35228 与 Windows SDK 10.0.26100.0。原生检查在 Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129、96 DPI、8 位 sRGB 后缓冲执行。 [审查交接留存](#review-closure)

### 实际运行

| 任务 | 验证入口与结果 |
| --- | --- |
| 1.1 | `PresentationChecks arena` 覆盖对齐填充后的最后合法范围、无效对齐、溢出和容量耗尽，保留旧数据、地址及游标。`normal` 在准备后改写源顶点与像素，原生图案符合原输入。槽复用后旧准备值的 Record 和 Finish 均拒绝。 |
| 1.2 | `PresentationChecks indexed` 在 Release 核对独立窗口目标、越界索引、旧槽及错槽 token。两次原生 resize 后 Pending 仍持有旧目标，仅完成的指定槽按最新 `120 × 81` 像素事实换代。 |
| 1.3、3.2 | `RuntimeChecks slots` 扣留真实 fence 的零超时完成观察。7 个 CPU UI 帧中有 5 次真实 fence 和交换，连续槽忙时没有第 4 个在途槽。解除扣留后原生完成先于同槽复用。关闭时保留最后一笔工作，恢复原窗口回调后设备收拢，随后窗口和 context 释放。正常循环的非零 GL 等待与 `glFinish` 调用均为 0。 |
| 1.4 | 重编 D04-T03 材料的 `prefix-error`、`no-ticket`、`poll-error` 均符合预期。取消帧的合法前缀仍有票据，零 fence 与真实查询错误阻止复用。`observer` 在真实 fence 和 flush 之后暂时解除当前 context，使 Event 建立失败，原接受票据仍返回，未获得交换资格或新槽。 |
| 2.1、2.2 | `RuntimeChecks layout` 覆盖独立视口 resize、隐藏再显示、稳定逻辑 token、失效 token 占位、原生窗口连续 resize 和最小化恢复。最终记录的 36 个实际 PresentColor 像素采样符合四象限或占位预期。旧视口纹理在真实完成前仍有效，随后退休。`NoSceneOutputs` 仅交付诊断颜色，真实提交沿 `View::Publish` 仅发布该颜色代，场景历史没有提交输入。 |
| 2.3 | 同一 `layout` 运行触发真实 ImGui atlas repack，新 atlas 在上传并接受后为 `OK`，旧 atlas 等实际完成再删除。`font-error` 令第一次字体上传产生实际 GL 错误，产品循环保留有界字体原因并返回 1，交换数为 0。复制正式 Release 可执行文件到无配套包的目录后，实际产品报告包路径并返回 1。 |
| 3.1 | Debug/Release 正式产品与同配置 UI 包构建通过。默认 Debug 可见入口直接显示 Docking、测试图和无场景状态，以 Alt+F4 退出 0。`RuntimeChecks window-resize` 验证 2560 × 1417 最大化及恢复，离屏颜色和实际后缓冲的 56 个采样均通过。 |
| 3.2 | 两配置正式入口以 `--hidden --poll-limit=7` 运行并返回 0。退出判断位于取得及新 UI 帧之前。最后一帧在途收拢由 `slots` 原生记录确认。 |

最大化裁切先在可见产品中观察到，并在 [修正前原生读回](../../../../build/d04-t04/runtime-window-resize-before.log) 复现。显式绑定默认 framebuffer 后，同口径的 [修正后读回](../../../../build/d04-t04/final-runtime-window-resize.log) 通过。新绑定由现有 ContextState 跟踪。编译与运行记录保留首次 MSBuild SDK 路径拒绝以及早期探针失败，最终检查使用成功构建后匹配的二进制。

### 复现与证据身份

- 正式构建为 `cmake --build build --config Debug --target Physara --parallel 4` 和对应 Release。开发检查以 `cmake -S build/d04-t04/checks -B build/d04-t04/check-build -G "Visual Studio 17 2022" -A x64 -T host=x64` 配置，Release 构建后运行 `python build/d04-t04/RunEvidence.py`。14 项结果与预期一致，另有一次默认可见 Debug 退出。 [本地检查结果](../../../../build/d04-t04/evidence.json) · [可见产品退出](../../../../build/d04-t04/product-Debug-visible.json)
- 源与产物清单覆盖 133 个正式输入、4 个本地检查输入、41 个产物和 3 个构建日志。检查前后身份一致。清单 SHA256 为 `ef5ae3c3aa452212e05de56c77b92b1331bde0e6cf59502bc20362dab8833c49`，检查结果 SHA256 为 `6efd953b8bf5bde4d432e26e9b8d579f59030884694f801b2d155bdace351fe1`，默认可见退出记录 SHA256 为 `9c1e06714b6bd8c1984a9f2dfc0f492ccc44bb1a96e8e0bf9d1c048b878e9179`。 [源与产物清单](../../../../build/d04-t04/source-and-products.json)
- 原生窗口仅覆盖 96 DPI，跨显示器 DPI 切换未实测。不同 framebuffer 比例以正式 `MeasureViewport` 消费者核对，输入像素比例 `1.5 × 2`、独立内容缩放 `3 × 4` 得到 `152 × 101`，零面积被拒绝。受控 Pending 和失败验证保留真实 fence 或原生错误，覆盖的是对应协议路径。构建目录中的证据由实际交接提供。 [验证约定](../../../../.agents/skills/physara-verification/SKILL.md)

<a id="a-correction-verification"></a>
## A 整改与自检

修正版使用同一 `v2.0` 基准 `c55b68efa838ce191a54cd35def45f19fa6df2d6`，暂存区为空。相对 B 审查版本调整上传准入、当帧视口占位和准备回滚，对应更新职责及调用合同注释。CR-001 至 CR-003 的 A 整改项均已完成，等待 B 复核代码与注释。 [本轮问题单留存](#review-closure)

| 问题 | 本轮实际运行 |
| --- | --- |
| CR-001 · 基础 UI 背压 | `RuntimeChecks upload-pressure` 先使 atlas 就绪，再提交 4096 × 2048 诊断色和 2048 × 2048 托管纹理。换代视口并销毁旧逻辑纹理、请求第二笔 16 MiB 纹理。扣留旧包的真实 fence 完成观察，三轮有 CPU 帧且没有新交换，旧原生纹理仍存活。恢复原生观察后上传占位和第二笔纹理，随后恢复视口图并复用已完成槽。合计 14 个 CPU 帧、11 次提交及交换，循环内非零 GL 等待和 `glFinish` 均为 0。 [压力记录](../../../../build/d04-t04/a-correction/verified-runtime-upload-pressure.log) |
| CR-001、CR-002 · 诊断色延后 | `RuntimeChecks diagnostic-pressure` 持有 48 MiB 原生上传容量，后续 12 MiB 托管写入及首次占位可准入，合计 8 MiB 的诊断候选暂缓。真实完成观察仍被扣留时已交换带 `Waiting for upload` 的占位帧，读回保留菜单字形，实际完成后恢复诊断图。 [诊断色压力记录](../../../../build/d04-t04/a-correction/verified-runtime-diagnostic-pressure.log) |
| CR-002 · 失效身份 | `RuntimeChecks layout` 改写同帧视口图像命令的 token 代，读回洋红占位与 `Texture expired`，并覆盖视口隐藏再打开、稳定 token、atlas 重建及真实窗口缩放和恢复。`font-error` 注入实际字体上传错误，产品循环返回 1，交换和 fence 均为 0。 [布局读回](../../../../build/d04-t04/a-correction/verified-runtime-layout.log) · [字体失败](../../../../build/d04-t04/a-correction/verified-runtime-font-error.log) |
| CR-003 · 准备回滚 | `RuntimeChecks arena-rollback` 在已有 23 字节的 arena 上完成临时 atlas 和几何分配，拒绝未声明的回调后游标退回 23，保留地址、数据、4 MiB 容量及 263204 字节高水位，atlas 回到无临时 token 的 `WantCreate`。同帧换用有效回调后以同一 arena 录制、提交并交换一次，atlas 为 `OK`。 [回滚记录](../../../../build/d04-t04/a-correction/verified-runtime-arena-rollback.log) |

- Debug/Release 正式产品和同配置 UI 包构建通过。与修正版匹配的 Release 原生探针及两配置产品有界退出共 11 项结果符合预期，另覆盖三槽退休、原生最大化/恢复的 56 个像素采样、基础 arena 边界和准备值借用。原生环境为 Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129、96 DPI。跨显示器 DPI 和前一轮未受影响的提交故障探针未在本轮重跑。 [运行清单](../../../../build/d04-t04/a-correction/evidence.json)
- 复现入口为 `python build/d04-t04/a-correction/RunCorrection.py`。清单覆盖 133 个正式构建输入、4 个本地检查输入、41 个产物和 5 份构建日志，运行前后 SHA256 一致。修正版清单 SHA256 为 `7028c040b5a63193f5aa1b9b5bd7ebacb0ea9f2f40625c712df2eb50cfd7723b`，运行清单 SHA256 为 `d6150ce4be9a6a4aaab6e432836b362a3bd81df883f92f50a97c00f2e755e8ef`。 [源与产物清单](../../../../build/d04-t04/a-correction/source-and-products.json)

<a id="b-correction-review"></a>
## B 第 1 轮整改复核

本次复核使用 `v2.0` 基准 `c55b68efa838ce191a54cd35def45f19fa6df2d6` 的实际工作树。暂存区为空。复核前按 A 的修正版清单核对 133 个正式构建输入、4 个夹具输入、41 个产物及 5 份构建日志，全部匹配。 [修正版清单](../../../../build/d04-t04/a-correction/source-and-products.json)

| 问题 | B 结论与证据 |
| --- | --- |
| CR-001 | 原压力场景通过。`upload-pressure` 在 14 次 CPU 帧中有 11 次有效提交及交换，旧包保留至真实完成。`diagnostic-pressure` 在完成观察仍受控时呈现 12 MiB UI 写入及占位，随后恢复诊断图。 |
| CR-002 | `layout` 与 `diagnostic-pressure` 原生读回得到 `Texture expired`、`Waiting for upload` 和正常菜单，恢复后得到诊断图。`font-error` 的 Runtime 返回 1，交换和 fence 均为 0。 |
| CR-003 | `arena-rollback` 的失败准备将游标退回到 23 字节并保留原数据、容量和高水位。同一 arena 重试获得票据 1。 |
| CR-004 | 待 A 整改。空闲设备上的合法 64 MiB chunk 被 UI 准入误拒，同列表实际录制两笔写入获得票据 1。 [最小复现](../../../../build/d04-t04/b-recheck/UploadCapacityReview.cpp) · [原生结果](../../../../build/d04-t04/b-recheck/capacity-review.log) |

B 还独立复跑 `PresentationChecks normal` 和正式 Release `Physara.exe --hidden --poll-limit=7`，均得到预期结果。使用 A 清单中的可执行文件及同配置包。共 7 项，日志 SHA256 为 `1f26fcd92b02ea9184e339fefe8f645c95e34bd6ffb61f6e82f71296414ee3ab`。CR-004 探针链接匹配 A 清单的 Release 库，其可执行文件 SHA256 为 `f2d487873e09f39f5da36d1602b9cf903bf58bb42706a0cb3a48962e2e3c28b8`，日志 SHA256 为 `db295a9bafcee53d9d87d93f727a71de6b1a6be3bf66c8040ca5065cafc248f0`。原生检查覆盖 96 DPI，跨显示器 DPI 未验证。 [B 复跑记录](../../../../build/d04-t04/b-recheck/native-recheck.log)

复核覆盖本轮 29 个第一方产品文件。B 补充 14 个文件的注释并逐文件核对非注释内容。复核后 133 个产品输入按路径排序，每行记录路径、Tab 与文件 SHA256 并以 LF 结束，对该 UTF-8 文本计算的 SHA256 为 `c5ac785692c4d7fb9272d37cbd6966080e41ddb00c4cbb7fa5223a894415dfc3`。4.4 待 CR-004 整改及独立复核。 [本轮问题单留存](#review-closure)

<a id="a-cr004-verification"></a>
## A 补充整改与自检

在同一 `v2.0` 基准上核对 B 复核后 133 个产品输入的聚合 SHA256，结果一致。OpenGL 预检依录制顺序应用实际分配的对齐和首个可用 chunk 尾部规则。UI 按封存命令的裁剪和采样需求填入诊断色、首次占位、托管纹理和缓冲写入，准备和录制使用同一最小 chunk 参数。 [容量设计](design.md#frame-arena) · [CR-004](#b-correction-review)

| 检查 | 本轮实际结果 |
| --- | --- |
| B 的最小复现 | 原探针退出 1，64 MiB 列表的首帧准备被误拒，但同列表实际两笔写入获票据 1。以修正版 Release 库重编相同夹具后，262144 字节及 4 字节的计划返回 `Ready`，首帧准备成功，实际写入获票据 1，退出 0。 [基线](../../../../build/d04-t04/a-cr004/b-baseline-capacity.log) · [整改后](../../../../build/d04-t04/a-cr004/verified-64m-list.log) |
| 实际占位需求与容量分类 | 一个真实提交持有 63.75 MiB chunk。只剩 256 KiB 时，首次 UI 的 atlas 写入返回 `Ready` 并实际交换，票据 2，atlas 为 `OK`。加入未使用的 4 字节占位会使这组写入返回 `Pending`，逻辑载荷超过 64 MiB 的计划返回 `ExceedsLimit`。此例可区分同包尾部计算正确时仍会误计占位的实现。 [原生结果](../../../../build/d04-t04/a-cr004/verified-actual-plan.log) |
| 两项在途压力 | Release `upload-pressure` 为 14 个 CPU 帧、11 次提交及交换，真实完成后恢复上传和同槽复用。`diagnostic-pressure` 为 14 个 CPU 帧、13 次提交及交换，真实完成观察被扣留时提交 12 MiB UI 写入及可见占位，实际完成后恢复诊断图。两项的循环内非零 GL 等待和 `glFinish` 均为 0。 [基础 UI 压力](../../../../build/d04-t04/a-cr004/verified-upload-pressure.log) · [诊断图压力](../../../../build/d04-t04/a-cr004/verified-diagnostic-pressure.log) |
| 受影响的准备和采样 | Debug `layout` 的 36 个读回样本、过期 token 文本、atlas 重建和恢复均符合预期。`arena-rollback` 拒绝未声明回调后保留前 23 字节，同一 arena 重试获票据 1。两配置正式入口有界退出均为 0。 [布局](../../../../build/d04-t04/a-cr004/verified-layout-debug.log) · [回滚](../../../../build/d04-t04/a-cr004/verified-arena-rollback-debug.log) |

Debug/Release 正式构建通过。运行 `python build/d04-t04/a-cr004/RunChecks.py` 得到 8 项通过，前后核对 133 个正式输入、5 个夹具输入、43 个产物和 5 份构建日志均一致。修正版清单 SHA256 为 `b80d25ecb18069666d3cd39b929dbc33b09694c6c46aaf1fe7133d8415ae41c0`，运行清单 SHA256 为 `f87ed913938ae18db1af9264fe21e0d52059766e7b65b11a4a13b82611f91d8e`。本轮原生运行覆盖同一 96 DPI 环境。跨显示器 DPI 和此前独立核对过的提交故障未在本轮复跑。4.4 等待 B 对 CR-004 及相关注释的独立复核。 [修正版清单](../../../../build/d04-t04/a-cr004/source-and-products.json) · [运行清单](../../../../build/d04-t04/a-cr004/evidence.json)

<a id="b-cr004-review"></a>
## B 第二次整改复核与签收

使用 `v2.0` 基准 `c55b68efa838ce191a54cd35def45f19fa6df2d6` 的实际工作树。暂存区为空。相对上一份 B 复核快照，8 个正式构建输入有变化。复跑前核对 A 清单中的 133 个正式构建输入、5 个夹具输入、43 个产物及 5 份构建日志，哈希全部匹配。A 清单 SHA256 为 `b80d25ecb18069666d3cd39b929dbc33b09694c6c46aaf1fe7133d8415ae41c0`。 [源与产物清单](../../../../build/d04-t04/a-cr004/source-and-products.json)

| 检查 | B 独立复跑 |
| --- | --- |
| CR-004 原复现 | Release `UploadCapacityReview` 通过。合法 64 MiB 列表通过首帧 UI 准备，262144 字节和 4 字节的真实纹理写入共用一个 chunk，提交获票据 `1`。 |
| 实际写入准入 | Release `ActualUploadPlan` 通过。持有真实在途包后剩余 256 KiB，atlas 单独写入为 `Ready`，增加占位为 `Pending`，实际单包逻辑超限为 `ExceedsLimit`。首帧以票据 `2` 接受，atlas 为 `OK`，交换成功。 |
| 上传压力 | Release `upload-pressure` 与 `diagnostic-pressure` 通过。均有 14 个 CPU 帧，提交及交换分别为 11 和 13 次。旧依赖在真实完成前保留，之后恢复视口及同槽复用。两项循环内非零 GL 等待和 `glFinish` 均为 0。 |
| 相关准备及正式入口 | Debug `layout` 通过 36 个读回样本，覆盖失效 token 的可见原因、atlas 重建和视口恢复。Debug `arena-rollback` 保留准备前的 23 字节，重试获得票据 `1`。Debug 和 Release 正式入口的 `--hidden --poll-limit=7` 均返回 0。 |

复跑使用匹配 A 清单的可执行文件及同配置包，8 项退出码均为 0。B 日志 SHA256 为 `e605d64bb0446765e8b5241f4742dc888421cfb6e5ade44e368a54173d42a8ad`。原生检查覆盖 Intel RaptorLake-S Mobile、OpenGL 4.6.0 和 96 DPI。跨显示器 DPI 及此前已独立核对的提交故障路径本轮未复跑。 [B 原生复跑](../../../../build/d04-t04/b-cr004/native-recheck.log)

代码验收通过。预检和实际分配以相同的对齐及首个可用尾部规则计算同包用量。UI 按本帧可见采样、纹理请求和实际缓冲写入构成计划，拟录制的最小 chunk 与实际列表一致。CR-001 至 CR-004 均关闭。注释验收通过。本轮核对 8 个修正文件及其直接消费者，更新其中 5 个文件的注释，逐文件确认非注释内容与复跑输入一致。 [问题单留存](#review-closure)

B 签收的 133 个正式构建输入按路径排序，以 UTF-8 的路径、Tab、文件 SHA256 和 LF 计算聚合 SHA256，结果为 `71b3cd54a31a22a1cd17544dffda41263090a9c76ef1c1ce9f3c8e7760136e78`。4.4 已完成，交 A 执行 5.1 最终验证。

<a id="review-closure"></a>
## 审查闭环留存

CR-001 的在途容量背压、CR-002 的同帧占位原因和 CR-003 的失败准备回滚由 A 整改并由 B 独立复核。CR-004 的实际写入计划与同包 chunk 计量由 A 补充整改，经 B 第二次独立复核关闭。B 分别签收代码和注释：前轮覆盖 29 个第一方产品文件，最后一轮核对 8 个修正文件及直接消费者，补充 5 个文件的注释并核对非注释内容。两个轮次的结论、版本及限制见 [第一次整改复核](#b-correction-review) 和 [第二次整改复核与签收](#b-cr004-review)。签收问题单副本 SHA256 为 `12e5035511c76a34dcf81c68a41ee7b6ec3848a67f314203a42177923e82fcf6`。 [本地问题单副本](../../../../build/d04-t04/final-verification/accepted-code-review.md)

<a id="final-verification"></a>
## 最终验证

在 `v2.0` 基准 `c55b68efa838ce191a54cd35def45f19fa6df2d6` 上重新枚举已跟踪及未跟踪的正式输入。暂存区为空，133 个输入的聚合 SHA256 与 B 签收值一致。正式源码和构建输入自签收后没有改动。`cmake --build build --config Debug --target Physara --parallel 4` 及对应 Release 目标构建成功，检查使用同配置 `EditorUi.pshader`。首次受限环境的 Windows SDK 读取失败保留在日志中，最终证据使用 SDK 读取获准后的成功构建。

复用 `build/d04-t04/a-cr004/checks` 和 `build/d04-t04/checks` 的临时工程，以 MSVC 19.44.35228 和 Windows SDK 10.0.26100.0 重新链接现有夹具。运行 `python build/d04-t04/final-verification/RunFinalChecks.py`，12 项全部符合预期；另有签收版 Debug 默认可见退出 0，以及签收版 Release 在隔离目录缺少 UI 包时退出 1。前后核对 133 个正式输入、7 个夹具输入、46 个产物和 7 份成功配置/构建日志。清单 SHA256 为 `39153a81d15bc0814da1b815412eaca0a3e078581422b88959167cf3b6c89e77`，12 项运行清单为 `a00b0526cf048eb766d28b300168229330dd2ae7d0888c7641ead20e1fefb5b9`。可见和缺包两次运行的 exe 哈希均与同一清单相符，记录 SHA256 分别为 `fa31e6801a7e75873822b5eaa7de9385bdfb4a55ba405b58e729e78f3508dcf2` 和 `ca5fd6b22bafa07e97f761b9f0159965821ca3b8b69711cc265d7b65de6f8a74`。 [源与产物清单](../../../../build/d04-t04/final-verification/source-and-products.json) · [运行清单](../../../../build/d04-t04/final-verification/evidence.json) · [可见入口](../../../../build/d04-t04/final-verification/product-Debug-visible.json) · [缺包入口](../../../../build/d04-t04/final-verification/product-release-missing-package.json)

| 需求 | 实现与验证证据 |
|---|---|
| [帧内有界 CPU 数据](specs/frame-slot-lifecycle/spec.md#requirement-prepare-bounded-frame-owned-cpu-data) | `FrameArena::Allocate` 检查对齐、溢出和末端；UI 准备复制几何及上传数据，`Record` 和 `Finish` 受本次借用限制。复用 `arena`、`normal` 的合法末端、非法分配、源改写及失效借用结果。签收版 `arena-rollback` 拒绝未声明回调后保留 23 字节和高水位，同一 arena 重试获票据 1。 [范围](../../../../Engine/Core/FrameArena.cpp#L37) · [准备值](../../../../Editor/ImGui/UiRenderer.cpp#L795) |
| [真实完成后的有界槽复用](specs/frame-slot-lifecycle/spec.md#requirement-reuse-a-fixed-number-of-slots-after-real-completion) | `AcquireFrame` 只检查三个索引，宿主证明同槽完成后才开放存储。`slots` 得到 7 个 CPU 帧、5 次真实 fence 和交换，等待期间无第四个 GPU 槽，循环内非零 GL 等待和 `glFinish` 均为 0。复用 `prefix-error`、`no-ticket`、`poll-error`、`observer` 的前缀、缺票据及完成观察失败证据；核对先保存真实票据再处理错误，不为失败帧确认纹理或发布 View 颜色。 [取得](../../../../Runtime/Main.cpp#L141) · [提交](../../../../Runtime/Main.cpp#L242) · [完成观察](../../../../Platform/Presentation.cpp#L198) |
| [暂停与最后一帧收拢](specs/frame-slot-lifecycle/spec.md#requirement-keep-input-responsive-during-suspension-and-retire-on-exit) | `layout` 运行最小化、恢复及当前窗口尺寸；`slots` 在最后一帧仍有在途访问时确认先释放会话输入接入、再收拢 GPU 工作。默认可见关闭和两配置有界入口均退出 0；源码确认退出在下一帧前决定，暂停期间只清理输入、推进退休及有限等待。 [循环](../../../../Runtime/Main.cpp#L305) · [关闭](../../../../Runtime/Main.cpp#L443) |
| [稳定视口身份与输出代](specs/editor-viewport-output/spec.md#requirement-resolve-a-stable-viewport-token-to-the-current-output-version) | `UiTextures` 用 View 身份与输出角色登记逻辑 token，准备时绑定本帧输出。`layout` 核对 resize 前后 token 相同，旧 View 和 atlas 在真实完成前保留、随后退休；篡改 token 后实际呈现洋红占位及 `Texture expired`，其它 UI 正常。 [注册](../../../../Editor/ImGui/UiTextures.cpp#L146) · [解析](../../../../Editor/ImGui/UiTextures.cpp#L401) |
| [帧边界的视口尺寸](specs/editor-viewport-output/spec.md#requirement-apply-viewport-extent-at-a-frame-boundary) | `MeasureViewport` 检查内容矩形及 framebuffer 比例；RenderFrame 在打开列表前准备 View。`layout` 的 36 个采样覆盖独立缩放、隐藏后重开和窗口恢复；`window-resize` 的 56 个采样核对最大化及恢复时离屏颜色和真实后缓冲。非等比换算复用正式消费者的 `1.5 × 2` 比例证据。 [尺寸](../../../../Editor/Viewport/Viewport.cpp#L65) · [帧准备](../../../../Runtime/Main.cpp#L172) |
| [无场景时的诊断呈现](specs/editor-viewport-output/spec.md#requirement-present-a-diagnostic-editor-without-advancing-scene-history) | 默认可见入口显示 Docking、四色诊断图和 `Test viewport \| No scene loaded.`。`diagnostic-pressure` 在旧容量仍在途时交换带 `Waiting for upload` 的占位，菜单仍可读，真实完成后恢复图像。`NoSceneOutputs` 的 `Publish` 仅更新诊断颜色代；图像和占位均无场景历史输入。 [占位](../../../../Editor/Viewport/Viewport.cpp#L46) · [发布](../../../../Engine/Renderer/View.cpp#L121) |
| [有界窗口目标取得](specs/opengl-window-presentation/spec.md#requirement-acquire-a-bounded-window-presentation-target) | 宿主仅为指定可用索引取得匹配像素范围的目标和身份。Release `indexed` 拒绝越界、过期和错槽 token，在两次 resize 中保持在途目标，完成后仅该槽按 `120 × 81` 换代；`layout` 证明暂停不分配替代目标，恢复后用当前事实。 [身份及取得](../../../../Platform/Presentation.cpp#L54) · [首写清屏](../../../../Runtime/Main.cpp#L226) |
| [唯一正式产品入口](specs/runtime-lifecycle/spec.md#requirement-provide-one-real-product-entry) | 正式 Debug 默认显示诊断视口，Debug/Release 的 `--hidden --poll-limit=7` 同路渲染并退出 0。Release 缺包启动报告路径并退出 1；`font-error` 的真实 GL 上传错误使产品循环返回 1，交换及有效 fence 为 0。`layout` 证明首次上传及重建都在提交接受后确认 atlas。 [装配及退出](../../../../Runtime/Main.cpp#L429) · [纹理确认](../../../../Editor/ImGui/UiRenderer.cpp#L809) |

CR-001 和 CR-002 的两项在途压力检查得到 14 个 CPU 帧，分别提交及交换 11、13 次。CR-004 的 64 MiB chunk 复现和实际写入计划通过，确认真正在途占用产生 `Pending`，逻辑载荷永久超限才产生 `ExceedsLimit`。最终 verify 对应 8 条新增或修改需求、22 个场景。三槽和窗口取得、帧内所有权、View 诊断颜色及录制/确认/关闭顺序符合 [设计决定](design.md#decisions)；无阻塞问题。

复用的故障和范围检查保留其各自的旧版源/产物身份，最终实现按上表重新核对相关门控和所有权。本轮真实窗口为 Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129、96 DPI 和 8 位 sRGB 后缓冲。未原生验证跨显示器 DPI、真实线性窗口、GPU 挂起或其它 GPU/驱动。范围不包含 D04-T05 的领域命令或 D05 的 RenderGraph。

<a id="spec-sync"></a>
### 主规范同步

按 OpenSpec 1.13.2 返回的 specs 产物路径与指引同步。`frame-slot-lifecycle` 和 `editor-viewport-output` 分别加入 Purpose 及三条需求；`opengl-window-presentation` 的有界目标取得、`runtime-lifecycle` 的唯一产品入口分别完整替换一条需求。两个替换块各保留原有三个场景并新增一个场景，未涉及的主规范及需求保持原内容和顺序。`openspec validate --specs --strict --json` 通过 17/17；含活动 change 的 `openspec validate --all --strict --json` 通过 18/18。 [帧槽主规范](../../../specs/frame-slot-lifecycle/spec.md) · [视口主规范](../../../specs/editor-viewport-output/spec.md) · [窗口主规范](../../../specs/opengl-window-presentation/spec.md) · [产品主规范](../../../specs/runtime-lifecycle/spec.md) · [同步校验清单](../../../../build/d04-t04/final-verification/spec-sync.json)

<a id="archive-result"></a>
### 归档结果

执行 `openspec archive establish-frame-slots-and-viewport-output --yes --skip-specs --json`，退出 0，归档到 `openspec/changes/archive/2026-11-11-establish-frame-slots-and-viewport-output/`。主规范已单独同步。归档移动时为 15/16，剩余 5.3 即归档及移动后的收尾核对。8 份归档文件移动前后摘要一致；随后调整归档层级的相对链接，将历史问题单引用指向本 change 的留存结论。17 份主规范与同步结果一致，无活动 change，归档后严格校验 17/17 通过。 [CLI 记录](../../../../build/d04-t04/final-verification/archive-cli.json)

开发计划记录 D04-T04 完成并指向归档，下一工作包 D04-T05 待立项。归档与文档链接核对通过后，当前问题单按空模板重置，SHA256 与模板同为 `a26066c0106ba8d1991974974acf06ddc62ed5d24e2878a42eda8f6fbec847fc`。5.3 完成，最终 16/16。 [开发计划](../../../../Docs/Developments/Developments.md#current-stage) · [问题单模板](../../../../Docs/CodeReview.md.tpl)
