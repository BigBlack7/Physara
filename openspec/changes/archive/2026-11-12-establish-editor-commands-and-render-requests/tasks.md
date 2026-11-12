# Tasks

## 1. A 接通字段编辑、事务及输入路由

- [x] 1.1 在 Editor/Commands 建立仅覆盖主 View 比例和主视口显隐的字段编辑，显式加入 Editor 模块。会话沿窄入口更新相应 owner，面板消费只读快照。用实际应用入口核对两个字段只影响各自状态，并在 Release 拒绝失效 View 或会话身份及非法比例。 [状态归属](design.md#edit-owners) · [字段编辑](specs/editor-domain-edits/spec.md#requirement-apply-field-level-editor-commands-at-a-frame-boundary)
- [x] 1.2 接通拖拽开始、逐帧预览、完成、取消与有界 Undo/Redo。用一条连续应用序列核对多次预览只需一次 Undo 和 Redo，交错显隐按原顺序重放，取消和净零保持原分支，新完成编辑清除 Redo。用同一检查确认容量满时只保留最近记录。 [事务设计](design.md#edit-transactions) · [重放合同](specs/editor-domain-edits/spec.md#requirement-replay-completed-edits-with-undo-and-redo)
- [x] 1.3 接通 Inspector 比例控件、View 菜单与视口关闭、Edit 菜单和当前帧快捷键。用一次实际会话输入序列确认文本占用及 Repeat 不触发历史，失焦后到达的更新和释放不会提交旧拖拽。以未完成编辑结束会话并在仍可用的宿主上重新接入，确认新会话历史为空、旧请求失效且输入只交给新会话。 [动作合同](specs/editor-session-input/spec.md) · [会话结束](specs/editor-domain-edits/spec.md#requirement-retire-editor-edits-on-session-shutdown)

## 2. A 让渲染比例驱动实际视口输出

- [x] 2.1 在 CPU UI 封存后的应用边界连接布局、当前请求和视口准备。通过真实测量消费者核对逻辑 `105 × 51`、framebuffer 比例 `1.5 × 2`、内容缩放 `3 × 4` 与比例 `0.75` 得到 `118 × 76`，可区分提前取整与重复 DPI 换算。核对隐藏或零面积跳过分配，重开继续用同一 View 和 token，窗口 PresentColor 仍对应窗口像素。 [帧边界](design.md#edit-frame-boundary) · [像素合同](specs/editor-viewport-output/spec.md#requirement-apply-viewport-extent-at-a-frame-boundary)
- [x] 2.2 接通 View requested/effective、Pending 和拒绝原因。用实际 OpenGL 帧追踪比例改变、等待匹配输出、按新尺寸接受及完成后的旧代退休，核对等待期间最后有效值和可见占位。组合验证一次取整同尺寸的接受及一次超出当前范围或预算后的恢复。确认只有本帧实际使用的诊断输出随完整提交发布有效比例，失败前缀与占位保持原值。 [发布设计](design.md#view-setting-readiness) · [有效比例合同](specs/editor-viewport-output/spec.md#requirement-confirm-effective-viewport-settings-from-accepted-output)

## 3. A 接通独立窗口请求和真实回执

- [x] 3.1 在 DeviceHost 实现按字段接收标题及 VSync 的原生设置入口。用同一活跃窗口核对一个合法 UTF-8 标题的实际 caption、支持设备的 0/1 交换区间读回，以及非法标题被拒绝。覆盖一个可达的原生不支持或确认失败，并在同轮观察另一有效字段仍可应用。沿现有宿主边界检查旧身份、非 owner 及已停止状态的拒绝。 [原生确认设计](design.md#native-window-settings) · [宿主规范](specs/native-window-host/spec.md)
- [x] 3.2 通过 Runtime 在帧边界连接 Window 菜单请求、宿主结果和 UI 显示。核对标题草稿确认后才应用，两个字段的请求与有效值可分别读取，设置失败后仍可继续 UI 呈现及应用 View 编辑，Undo/Redo 只重放所属领域记录。 [应用顺序](design.md#edit-frame-boundary) · [状态归属](design.md#edit-owners)

## 4. A 验证正式产品和 D04 阶段出口证据

- [x] 4.1 构建正式 Debug、Release 和同配置 EditorUi 包。在默认可见入口操作比例拖拽、Undo/Redo、视口关闭及恢复，核对比例状态、诊断图、独立窗口设置和关闭结果。用两配置的 `--hidden --poll-limit=N` 核对同一路径的有界退出。更新并核对构建说明中的真实操作和状态。 [构建说明](../../../../Docs/Build.md)
- [x] 4.2 对照 D04 阶段出口，列明本次实际覆盖的输入、窗口和视口尺寸、颜色、字体、末端复制和在途退出证据。先核对 D04-T01、T03、T04 的对应验收及实际输入，复用可匹配的证据，只补本次修改影响的路径。说明实际 DPI 及不同 framebuffer 比例的验证范围，区分同显示器读回与原生跨显示器验证。 [阶段出口](../../../../Docs/Developments/D03-D05-FramePipeline.md#dev-d04) · [窗口验收](../2026-11-11-establish-opengl-window-presentation/tasks.md#final-verification) · [帧槽验收](../2026-11-11-establish-frame-slots-and-viewport-output/tasks.md#final-verification)

检查材料仅放在 `build/d04-t05/`，优先复用实际可取得的现有消费者和夹具。记录独立的缺口及可区分回归的预期后再新增定向检查。已经删除的历史夹具按原交接范围处理，正式构建保持可独立重建。通过已列验收且输入身份一致后停止扩展检查。 [验证约定](../../../../.agents/skills/physara-verification/SKILL.md)

## 5. 独立 A/B 审查与注释

- [x] 5.1 A 在当前问题单登记基准、实际暂存和未暂存差异、新增文件、匹配的构建与运行产物、夹具入口及限制，交维护者调度独立 B 审查。A 本地交付时保留其余审查任务待处理。 [签收问题单留存](../../../../build/d04-t05/final-verification/b-signed-ledger.md) · [交接规则](../../../../.agents/skills/physara-review/SKILL.md)
- [x] 5.2 B 核对实际差异、事务状态、编辑和宿主的所有权、输出确认、直接消费者及相应证据，补齐本轮第一方文件的职责、接口与关键流程注释。按稳定编号写入发现及复验要求。 [B 第一轮审查](../../../../build/d04-t05/final-verification/b-signed-ledger.md#问题与整改)
- [x] 5.3 A 按编号整改，复跑能区分缺陷的对应检查，在问题单和本 change 留存修正输入、结果及限制。无正式整改项时记录适用的自检结论。 [A 整改与复验](#a-correction-verification)
- [x] 5.4 B 独立复核实际修正版、证据匹配和注释覆盖，逐项关闭或说明撤回发现，分别签收代码正确性和注释。 [B 复核与签收](#b-correction-review) · [独立审查闭环](../../../../AGENTS.md#review-loop)

## 6. A 最终验证、同步与归档

- [x] 6.1 B 签收后执行 OpenSpec verify，将全部新增或修改场景对应到签收版本的实现与证据，复验具体缺口。核对产品入口和 D04 阶段出口的实际覆盖，留存原生结果及适用限制。 [最终验证](#final-verification)
- [x] 6.2 最终验证通过后同步四个 delta，核对新增能力的 Purpose、现有需求保留的场景及未涉及主规范，执行 OpenSpec 严格校验。 [主规范同步](#spec-sync)
- [x] 6.3 留存 A/B 结论、最终版本和证据后按 OpenSpec 归档。核对归档文件、链接与任务完成状态，更新工作包和 D04 阶段出口。归档成功后按模板重置当前问题单。 [归档结果](#archive-result)

<a id="a-local-verification"></a>
## A 实施与本地验证

基准为 `v2.0` 的 `f049ff04d1f66e1b8c57ccfe3000083f4ea6990a`。暂存区为空，待审产品包含 18 个已跟踪变更和 3 个未跟踪文件。136 个正式输入按路径排序，用路径、Tab、文件 SHA256 和 LF 计算的聚合 SHA256 为 `ecec34a8c5f8fafe15f9165034ef6a132d7b2443124b8323ea8de85c3392af53`。交接清单 SHA256 为 `f81af00fbea4677b28a2dff9aec62c1e54d61795595f18c3c2252e43f7c7b036`。 [交接清单](../../../../build/d04-t05/a-local-04/handoff.json) · [实际跟踪差异](../../../../build/d04-t05/a-local-04/product-tracked.diff) · [签收问题单留存](../../../../build/d04-t05/final-verification/b-signed-ledger.md)

使用 MSVC 19.44.35228 与 Windows SDK 10.0.26100.0。Debug 首次并行构建在标准库头遇到 `C1001`。保持源码及编译选项，以 `cmake --build build --config Debug --target Physara --parallel 1` 重试，Debug 与对应 Release 构建均通过。Release 临时工程链接同配置正式库并读取同配置 UI 包。`python build/d04-t05/RunChecks.py a-local-04` 的四项夹具和两次隐藏产品运行均返回 0。前后核对 136 个正式输入、3 个夹具输入、40 个产物和 9 份配置及构建日志。清单与运行结果的 SHA256 分别为 `9ac9d6e2d141cd1e39bc44b419bc7ab88f514cfc6dae65b43dfaf2114f3efb67` 和 `f4a0d8e20b1628504fe8143b8e5736993537bf9834a51cce74ac9b631ba3e267`。 [源与产物](../../../../build/d04-t05/a-local-04/source-and-products.json) · [运行记录](../../../../build/d04-t05/a-local-04/evidence.json) · [首次编译日志](../../../../build/d04-t05/build-debug.log)

| 任务 | A 实际证据 |
|---|---|
| 1.1–1.3 | Release `edits` 经正式入口检查两个字段各自的 owner、失效身份、非有限及越界比例。多帧预览只需一次 Undo/Redo，交错显隐按顺序重放，取消及净零保留分支，新编辑截断 Redo，历史达到 64 条后只淘汰最早一条。实际工作区路由用文本占用、Press 和 Repeat 验证历史动作归属。Release `session` 激活实际 Inspector 控件，同批失焦后还原预览，在拖拽未完成时销毁会话并在同一宿主重接，宿主可继续发布且新会话无旧编辑。 |
| 2.1 | `edits` 调用实际 `MeasureViewport`，非等比输入得到 `118 × 76`，零面积无尺寸请求。`viewport` 中比例改变时窗口仍为 `640 × 480`，离屏输出从 `624 × 409` 变为 `468 × 306`，原生窗口 resize 后为 `700 × 520`。源码确认关闭后停止视口请求，纹理 token 在会话创建时注册一次。正式窗口关闭与撤销重开恢复同一停靠面板。 |
| 2.2 | Release `viewport` 在实际 GL 帧中受控收紧诊断额度，读回洋红占位并保留原有效值。匹配新比例的诊断图提交后发布，旧纹理在真实 fence 尚未被观察时保留，恢复完成观察后退休。取整同尺寸时复用纹理并确认比例。超设备尺寸时读回占位，恢复后再次发布。实际索引绘制后的受控错误获得前缀票据 9，最后有效比例保持在前一输出。 [视口日志](../../../../build/d04-t05/a-local-04/viewport.log) |
| 3.1–3.2 | Release `window` 读回实际 Unicode 标题和 WGL 区间 0、1，核对非法标题、旧窗口身份、错误线程及停止宿主的拒绝。Win32 `WM_SETTEXT` 受控拒绝后保留上次确认标题，同轮 VSync 仍成功。随后应用 0.75x 的 View 编辑，读回诊断颜色并观察到实际交换的票据 2。正式窗口中标题草稿在 Enter 后才改变标题栏，VSync 的有效区间为 0。 [窗口日志](../../../../build/d04-t05/a-local-04/window.log) |
| 4.1 | 正式 Debug 可见窗口显示 Docking、字体和四色诊断图。拖拽从 1.00x 改到 0.75x，Ctrl+Z 恢复 1.00x，Ctrl+Y 再次请求 0.75x。恢复可见布局后确认 0.75x 及 `948 × 486`。关闭视口后 Undo 恢复同一面板。第一次可见交互记录中的进程退出码为空。以相同 Debug exe 和 UI 包再次默认启动并通过 Alt+F4 关闭，由 Win32 观察到退出码 0。Debug 和 Release 的 `--hidden --poll-limit=9` 均返回 0。 [可见交互观察](../../../../build/d04-t05/a-local-04/handoff.json) · [可见退出码](../../../../build/d04-t05/product-debug-visible-exit.json) · [运行说明](../../../../Docs/Build.md#运行) |

### D04 阶段出口核对

- 本轮验证了编辑路由、同宿主会话重接、窗口与视口尺寸的独立变化、可见字体、诊断颜色及占位、同票据提交和交换、失败前缀保护及退出。受控完成探针使用真实 fence。测试程序通过正式宿主关闭路径收拢依赖。
- 前置验收保留了回调接入、原生末端复制及编码、字体 atlas 重建、最小化恢复、零票据和查询失败收拢的各自版本证据。核对了当前对应入口与既有合同，复验本轮修改触及的视口准备、窗口请求及编辑交接。 [输入验收](../2026-11-10-establish-editor-session-input/tasks.md#final-verification) · [呈现验收](../2026-11-11-establish-opengl-window-presentation/tasks.md#final-verification) · [帧槽验收](../2026-11-11-establish-frame-slots-and-viewport-output/tasks.md#final-verification)
- 本轮窗口环境为 Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129、96 DPI。非等比 framebuffer 换算由直接消费者核对。物理跨显示器 DPI 切换未实测。前置字体重建和特殊呈现故障沿各自历史覆盖。阶段出口最终验收在 B 签收后进行。

<a id="a-correction-verification"></a>
## A 整改与复验

基准为 `v2.0` 的 `f049ff04d1f66e1b8c57ccfe3000083f4ea6990a`，暂存区为空。整改前 136 个正式输入的聚合 SHA256 与 B 注释版一致，为 `0e06155b6313755d8390d030ff26da3a9ca44c3b0006095b5b59cc6c963044c5`。整改在相同路径集中修改 12 个文件，聚合 SHA256 为 `5d78838e75e73c8c31011f0793eb7b7b67a7efb87df67ed232a95b42af49b44d`。源与产物清单 SHA256 为 `4095d962cf704fbe36c40089813971f95fabd0502ac1b0e220bb35799cc88695`，运行记录 SHA256 为 `485303906721ecfd99fe3985dd88b2b6ad975eadc328fcf3c4faaf8081e2b0a0`。 [B 输入清单](../../../../build/d04-t05/a-correction/b-reviewed-inputs.json) · [A 源与产物](../../../../build/d04-t05/a-correction/verified-02/source-and-products.json) · [整改路径与运行记录](../../../../build/d04-t05/a-correction/verified-02/evidence.json)

以 MSVC 19.44.35228、Windows SDK 10.0.26100.0 串行构建正式 Debug 和 Release `Physara` 目标，均成功。Release 临时夹具链接同配置正式库并读取同配置 UI 包。首轮新增视口夹具在停靠布局形成前取样，沿原视口夹具预热到首个有效输出后复跑。执行 `python build/d04-t05/a-correction/RunChecks.py verified-02`，四项定向夹具、四项既有夹具和 Debug/Release `--hidden --poll-limit=9` 均返回 0。可见 Debug 与证据中的 exe、UI 包身份一致。 [Debug 构建](../../../../build/d04-t05/a-correction/build-product-debug-01.log) · [Release 构建](../../../../build/d04-t05/a-correction/build-product-release-01.log) · [首轮视口夹具](../../../../build/d04-t05/a-correction/verified-01/view-authority.log) · [通过的夹具构建](../../../../build/d04-t05/a-correction/build-checks-04.log)

| 问题 | A 实际证据 |
|---|---|
| CR-001 | 在真实 Inspector 中激活滑块，向 ImGui 队列投递末段移动与释放。同批和分批均提交 `0.89x`，一次 Undo 恢复 `1.00x`，一次 Redo 恢复 `0.89x`。包含失焦、晚到释放和恢复的宿主批次取消预览，随后可重放原历史。 [拖拽检查](../../../../build/d04-t05/a-correction/verified-02/drag-release.log) |
| CR-002 | 主 View 请求 `0.50x` 后，以真实合成与 GL 像素读回检查缺省及失配 View。两次显示占位并保留 `1.00x` 有效输出，过期代和异 View 请求无法准备颜色。匹配 View 使诊断图从 `624 × 409` 变为 `312 × 204`，实际接受后确认 `0.50x`。 [视口身份检查](../../../../build/d04-t05/a-correction/verified-02/view-authority.log) |
| CR-003 | 在隐藏宿主的 Win32 Wait 消息分发中调用实际 GLFW 回调链，宿主按顺序发布重接前的 Press 和重接后的 Release。新会话忽略旧 Press，同一首批的新 Press 和随后批次的新 Press 都能切换 Inspector，旧历史未进入新会话。 [重接检查](../../../../build/d04-t05/a-correction/verified-02/reattach.log) |
| CR-004 | 使用实际 Workspace 与 EditHistory，合并和拆分 Ctrl+Z、Ctrl+Y 都回到 `0.75x` 且历史游标一致。既有文本占用和 Repeat 检查以及新增活动事务门控均通过。 [有序重放](../../../../build/d04-t05/a-correction/verified-02/ordered-history.log) · [领域编辑回归](../../../../build/d04-t05/a-correction/verified-02/edits.log) |
| CR-005 | 默认 Debug 窗口输入 `Physara A 中文 🙂`，Enter 前检查码点草稿和原标题栏，Enter 后核对两项菜单回执及完整原生标题。Alt+F4 后 Win32 取得退出码 0。 [可见窗口检查](../../../../build/d04-t05/a-correction/visible-title.json) · [原生退出结果](../../../../build/d04-t05/a-correction/product-debug-visible-exit.json) |

本轮原生环境为 Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129、96 DPI。拖拽夹具向真实会话注入指定的 ImGui 队列和宿主失效边沿，重接夹具在隐藏宿主内受控投递 Win32 消息。正式可见窗口验证标题及退出，物理跨显示器 DPI 切换未覆盖。本轮交接待 B 独立复核代码与注释。 [签收问题单留存](../../../../build/d04-t05/final-verification/b-signed-ledger.md#问题与整改)

<a id="b-correction-review"></a>
## B 独立复核与签收

2026-11-12，Agent-B 复核通过 CR-001 至 CR-005。基准为 `v2.0` 的 `f049ff04d1f66e1b8c57ccfe3000083f4ea6990a`，暂存区为空。136 个正式输入、夹具、产物及 A 记录与交接清单吻合。实际运行对应 A 修正版聚合 SHA256 `5d78838e75e73c8c31011f0793eb7b7b67a7efb87df67ed232a95b42af49b44d`。B 签收的注释版聚合 SHA256 为 `1c11659a649230dacdf2396b343cf3170877d3fc113e26e2653fdf27ac0e6bcb`。逐文件比较确认 12 个差异文件只改注释，源码语句、CMake 指令和换行格式一致。 [复跑源与产物](../../../../build/d04-t05/a-correction/b-recheck-01/source-and-products.json) · [签收问题单留存](../../../../build/d04-t05/final-verification/b-signed-ledger.md#本轮验收与留存)

| 问题 | B 复核结论 |
|---|---|
| CR-001 | 滑块只在末段采样期间暂时保持按下状态，随即结束活动项。定向夹具的同批和分批释放均得到 `0.89x`，单次 Undo/Redo 与失焦回滚通过。正式 Debug 快速拖拽到 `0.75x`，一次撤销和重做恢复对应端点，切回视口后接受 `948 × 486`。 [拖拽复跑](../../../../build/d04-t05/a-correction/b-recheck-01/drag-release.log) |
| CR-002 | 封存、准备、采样及发布保留匹配的 View 身份、范围和请求代。GL 读回确认缺省或失配时使用占位，保留最后有效比例。恢复匹配后确认 `0.50x (312 × 204)`，同尺寸复用、旧代退休及失败前缀回归通过。 [请求匹配复跑](../../../../build/d04-t05/a-correction/b-recheck-01/view-authority.log) · [视口回归](../../../../build/d04-t05/a-correction/b-recheck-01/viewport.log) |
| CR-003 | 接入时记录已发布序号和待发布前缀长度。受控 Wait 复跑中宿主完整发布事件，新会话只响应首批后缀和之后的新 Press，历史从空开始。 [会话重接复跑](../../../../build/d04-t05/a-correction/b-recheck-01/reattach.log) |
| CR-004 | 两种入口交给同一历史 owner。合并和拆分 Ctrl+Z/Ctrl+Y 均回到 `0.75x`，Undo 可用且 Redo 为空。活动事务、文本占用和 Repeat 的回归通过。 [有序重放复跑](../../../../build/d04-t05/a-correction/b-recheck-01/ordered-history.log) · [编辑回归](../../../../build/d04-t05/a-correction/b-recheck-01/edits.log) |
| CR-005 | 在正式 Debug 的草稿、请求和确认中，中文、补充平面字符及字面反斜杠均可区分。Enter 前原生标题栏保留原标题，提交后原生 caption 与原始输入 `Physara B 中文 🙂 \u{4e2d}` 一致。 [原生退出](../../../../build/d04-t05/a-correction/b-recheck-01/visible-debug-exit.json) |

注释可读性复核通过。会话及宿主记录接入前缀与借用期，Workspace 说明末段采样、按序重放和标题转义，Viewport 和 View 说明请求代与颜色代及失配占位。直接消费者的提交回执、资源寿命与窗口边界已核对。 [注释覆盖](../../../../build/d04-t05/final-verification/b-signed-ledger.md#b-的注释覆盖)

B 使用同一构建的 Release 夹具复跑 8 个场景，正式 Debug、Release 有界隐藏运行及 Debug 可见退出均返回 0。原生证据覆盖 Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129 和单屏 96 DPI。物理跨显示器 DPI 切换沿用 A 的覆盖限制。下一步由 A 对签收版执行任务 6.1。 [B 运行证据](../../../../build/d04-t05/a-correction/b-recheck-01/evidence.json)

<a id="final-verification"></a>
## 最终验证

2026-11-12，按 OpenSpec 1.13.2 的 spec-driven 工作流核对 proposal、design、四份 delta 和 tasks。13 项实施和独立审查任务已完成。基准为 `v2.0` 的 `f049ff04d1f66e1b8c57ccfe3000083f4ea6990a`，暂存区为空。按路径重算 136 个正式输入的聚合 SHA256 为 `1c11659a649230dacdf2396b343cf3170877d3fc113e26e2653fdf27ac0e6bcb`，与 B 签收一致。B 签收问题单的留存摘要为 `cadf64abfdf9415588e64cfaa4ff69749b9e337b81f5aa9f33c1726af6c32de1`。 [B 复核与注释验收](#b-correction-review) · [签收问题单留存](../../../../build/d04-t05/final-verification/b-signed-ledger.md)

以 MSVC 19.44.35228 和 Windows SDK 10.0.26100.0 分别执行 `cmake --build build --config Debug --target Physara --parallel 1` 及对应 Release 构建。在独立目录重新编译已有 Release 夹具，链接同配置正式库及 UI 包。运行 `python build/d04-t05/final-verification/RunFinalChecks.py`，8 项夹具及 Debug、Release 的 `--hidden --poll-limit=9` 均退出 0。正式 Debug 无参数显示停靠标签、无场景说明和四色诊断图，关闭主视口后 Ctrl+Z 恢复原停靠标签及图像，Alt+F4 原生退出码为 0。前后核对 136 个正式输入、5 个夹具及运行脚本输入、40 个产物和 4 份成功构建日志。源与产物清单 SHA256 为 `8f776a8907e88166caa26b1982e535852da46c43d21e7d87aff21a9e215f9c5a`，运行记录为 `a83564277ddcd178fc4b4328c0ab7cd301da602b3b9bb860e62e09b68f933328`。 [最终源与产物](../../../../build/d04-t05/final-verification/source-and-products.json) · [最终运行](../../../../build/d04-t05/final-verification/evidence.json) · [可见观察](../../../../build/d04-t05/final-verification/visible-observations.json) · [原生退出](../../../../build/d04-t05/final-verification/product-debug-visible-exit.json)

| 需求 | 场景 | 签收实现与证据 |
|---|---:|---|
| [帧边界的字段命令](specs/editor-domain-edits/spec.md#requirement-apply-field-level-editor-commands-at-a-frame-boundary) | 2 | `Advance` 封存 UI 后应用意图，`RunEditor` 在槽等待时仍推进 CPU。Release `edits` 验证两个 owner 独立，失效身份、非有限及越界请求被拒绝且历史保持。 [会话](../../../../Editor/Session/EditorSession.cpp#L107) · [循环](../../../../Runtime/Main.cpp#L322) · [字段回归](../../../../build/d04-t05/final-verification/edits.log) |
| [连续编辑合并](specs/editor-domain-edits/spec.md#requirement-coalesce-one-continuous-viewport-edit) | 3 | `ApplyGesture` 按取消、预览、结束处理，净零保留 Redo。实际 Inspector 的分批与合批释放均提交 `0.89x`，单次 Undo/Redo 与同批失焦回滚通过。`edits` 复验多帧、净零及分支。 [手势](../../../../Editor/Commands/EditHistory.cpp#L53) · [拖拽回归](../../../../build/d04-t05/final-verification/drag-release.log) |
| [有界历史和重放](specs/editor-domain-edits/spec.md#requirement-replay-completed-edits-with-undo-and-redo) | 2 | `Replay` 在 owner 接受后移动游标，提交新记录截断 Redo。`edits` 检查交错字段、重放顺序、分支和 64 条容量。`ordered-history` 检查 Ctrl+Z、Ctrl+Y 两种分批方式。 [重放](../../../../Editor/Commands/EditHistory.cpp#L105) · [有序重放回归](../../../../build/d04-t05/final-verification/ordered-history.log) |
| [会话结束](specs/editor-domain-edits/spec.md#requirement-retire-editor-edits-on-session-shutdown) | 1 | `session` 在活动预览期间销毁会话并重接。`reattach` 经 Win32 Wait 验证旧事件仍由宿主发布，新会话响应新 Press 且历史为空。Runtime 先判断退出，随后释放会话及 GPU 工作。 [输入边界](../../../../Editor/ImGui/GlfwPlatform.cpp#L68) · [活动会话回归](../../../../build/d04-t05/final-verification/session.log) · [重接回归](../../../../build/d04-t05/final-verification/reattach.log) |
| [停靠工作区和动作门控](specs/editor-session-input/spec.md#requirement-own-a-dockable-workspace-and-gate-unavailable-actions) | 3 | `Workspace::Draw` 维持主窗口停靠和稳定面板身份。复用 T01 的菜单隐藏、恢复和空数据控件检查。签收版可见窗口验证关闭主视口、空层级和 Undo 恢复，原 View 及 token 由会话持有。 [工作区](../../../../Editor/Panels/Workspace.cpp#L259) · [输入验收](../2026-11-10-establish-editor-session-input/tasks.md#final-verification) · [本轮可见观察](../../../../build/d04-t05/final-verification/visible-observations.json) |
| [每次输入的动作归属](specs/editor-session-input/spec.md#requirement-dispatch-each-editor-trigger-to-one-eligible-action-owner) | 3 | 使用当轮输入归属，首次按下生成动作，历史按序判断可用性。复用 T01 的面板共用动作、文本/弹层/Gizmo/捕获与滚轮仲裁。本轮 `edits`、`ordered-history` 检查文本、Repeat 和活动事务门控。 [仲裁](../../../../Editor/Session/ActionRouter.cpp#L10) · [工作区派发](../../../../Editor/Panels/Workspace.cpp#L326) · [输入验收](../2026-11-10-establish-editor-session-input/tasks.md#final-verification) |
| [视口像素范围](specs/editor-viewport-output/spec.md#requirement-apply-viewport-extent-at-a-frame-boundary) | 3 | `MeasureViewport` 用每轴实际比例乘请求值后取整。`edits` 得非等比 `118 × 76`，并检查零面积。`viewport` 核对诊断图 `624 × 409 → 468 × 306`，窗口 resize `640 × 480 → 700 × 520`，复用 T04 的隐藏恢复、token 和最小化检查。 [换算](../../../../Editor/Viewport/Viewport.cpp#L69) · [原生视口回归](../../../../build/d04-t05/final-verification/viewport.log) · [帧槽与视口验收](../2026-11-11-establish-frame-slots-and-viewport-output/tasks.md#final-verification) |
| [接受匹配的有效输出](specs/editor-viewport-output/spec.md#requirement-confirm-effective-viewport-settings-from-accepted-output) | 3 | `Prepare`、合成和 `Publish` 关联 View、请求代及范围，槽不可用时由 Runtime 报告等待。`viewport` 在待上传和超限时读回占位，验证同尺寸复用、恢复接受、旧纹理退休与失败前缀票据 9。`view-authority` 验证缺省、失配及过期请求，匹配 `0.50x` 后确认 `312 × 204`。 [View 准备](../../../../Engine/Renderer/View.cpp#L79) · [提交回执](../../../../Runtime/Main.cpp#L252) · [视口匹配回归](../../../../build/d04-t05/final-verification/view-authority.log) |
| [原生窗口设置确认](specs/native-window-host/spec.md#requirement-confirm-live-window-title-and-swap-interval-requests) | 4 | `window` 读回实际 Unicode caption 与 WGL 区间 0、1，验证非法标题、过期/停止宿主和非 owner 线程拒绝。受控 `WM_SETTEXT` 拒绝保留旧标题，VSync 及后续 View 提交仍成功。复用 B 的可见草稿、Unicode 码点回执与原始 caption。 [宿主边界](../../../../Platform/DeviceHost.cpp#L617) · [原生窗口回归](../../../../build/d04-t05/final-verification/window.log) · [B 可见结果](#b-correction-review) |

9 条新增或修改需求的 24 个场景均与实际实现和证据对应。字段所有权、帧边界及接受顺序符合设计，检查了正式消费者的模块关系和代码约定。同步前 OpenSpec 严格校验 18/18 通过，最终 verify 通过。 [设计决定](design.md#decisions)

### D04 阶段出口

签收版实际显示 Docking、字体与诊断颜色，视口关闭、撤销恢复、真实 resize、GL 颜色读回及正常退出通过。字体更新与在途 atlas 退休复用 T02，窗口 resize、实际末端复制在 fence 之前及票据匹配复用 T03。T04 补足有界帧槽、暂停恢复及在途关闭。DPI 使用实际 framebuffer/client 的分轴比例和非等比输入证据。 [字体与合成](../2026-11-10-establish-imgui-rhi-renderer/tasks.md#final-verification) · [窗口呈现](../2026-11-11-establish-opengl-window-presentation/tasks.md#final-verification) · [帧槽与关闭](../2026-11-11-establish-frame-slots-and-viewport-output/tasks.md#final-verification)

本轮原生覆盖 Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129、单屏 96 DPI。物理跨显示器 DPI 切换未实测。Win32 拒绝和图形失败前缀来自受控输入，复用证据保留各自的签收版本及适用范围。

<a id="spec-sync"></a>
### 主规范同步

按 OpenSpec 1.13.2 的 specs 产物和指引同步四份 delta。新增 `editor-domain-edits` 的 Purpose 与四条需求，更新 `editor-session-input` 的两条需求，更新 `editor-viewport-output` 的尺寸需求并加入有效输出确认，向 `native-window-host` 加入实时标题和交换区间确认。共新增六条、修改三条需求。三个修改块保留原有六个场景，既有 Purpose、其他需求顺序和 14 份未涉及主规范保持原内容。同步后主规范严格校验 18/18，含活动 change 的严格校验 19/19。 [领域编辑](../../../specs/editor-domain-edits/spec.md) · [会话输入](../../../specs/editor-session-input/spec.md) · [视口输出](../../../specs/editor-viewport-output/spec.md) · [宿主设置](../../../specs/native-window-host/spec.md) · [同步清单](../../../../build/d04-t05/final-verification/spec-sync.json)

<a id="archive-result"></a>
### 归档结果

执行 `openspec archive establish-editor-commands-and-render-requests --yes --skip-specs --json`，退出 0，归档到 `openspec/changes/archive/2026-11-12-establish-editor-commands-and-render-requests/`。主规范已在归档前同步并复核。移动时为 15/16，归档和文档收尾对应剩余任务 6.3。8 份文件移动前后摘要一致，随后更新归档目录相对链接，历史审查链接指向签收留存。18 份主规范与同步结果一致，活动 change 清单为空，归档后严格校验通过 18/18。 [CLI 记录](../../../../build/d04-t05/final-verification/archive-cli.json) · [移动前清单](../../../../build/d04-t05/final-verification/archive-inputs.json) · [归档后校验](../../../../build/d04-t05/final-verification/all-after-archive-strict.json)

开发计划记录 D04-T05 及 D04 阶段出口通过，D05-T01 待立项。归档与链接核对通过后，当前问题单按模板重置，SHA256 与模板同为 `a26066c0106ba8d1991974974acf06ddc62ed5d24e2878a42eda8f6fbec847fc`。任务完成 16/16。 [开发计划](../../../../Docs/Developments/Developments.md#current-stage) · [问题单模板](../../../../Docs/CodeReview.md.tpl)
