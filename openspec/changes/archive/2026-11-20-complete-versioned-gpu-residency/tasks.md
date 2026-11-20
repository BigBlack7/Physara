# Tasks

## 1. 图导入与精确范围

- [x] 1.1 扩展现有 Buffer 导入的初始内容覆盖，接通声明、编译和执行期权限，保留整范围简写。用一个混合页证明等价导入复用、覆盖冲突与空洞读取拒绝、可读范围按写版本推进。 [图规范](specs/render-graph-contract/spec.md)
- [x] 1.2 让图的 Buffer 上传与消费传递目的 offset 和 size，使用精确区间解析同一页。以已有非零区间写入或可区分的跨页内区段结果核查声明、权限与命令所用 offset 一致。 [静态驻留规范](specs/static-gpu-residency/spec.md)

## 2. 版本化驻留与场景批

- [x] 2.1 在现有驻留 owner 内实现按布局分配的有界顶点/索引页，以及按 UBO 对齐的材质表和范围绑定缓存，让 Forward 直接消费。用两份共页几何和不同材质的原生输出核对非零 offset；通过编辑一个实例与发布新版模型核对旧代保留、局部更新及完成后的复用。
- [x] 2.2 补齐按固定版本、解释、有效视图范围和 sampler 的预算/淘汰，沿现有环境 CPU 产品键和需求入口完成 GPU 产品记账及换代。用同一图像的颜色/数据与有无 mip 契约验证实际采样，观察环境查询调整复用烘焙/驻留及换源后的完整产品。 [纹理与环境设计](design.md#纹理解释和环境产品沿用现有版本发布线)
- [x] 2.3 连接所有实际 View/Draw 生产者与场景批封存，合并兼容脏范围，保持 UI 自有上传、存活计划准入及逐代确认。用相邻写、空洞、共享/私有两路以及一个后置生产者拒绝，核对实际写、封存前诊断与重试身份。
- [x] 2.4 接通已准备、在途及淘汰代的预算和退休，记录最终默认页、缓存条目及帧工作集限制与依据。通过固定低容量或代表性上限区分可在退休后恢复的 Pending 和确定超限，核对旧 View 输出资格、基础 UI 与中断前缀的保活。 [驻留规范](specs/static-gpu-residency/spec.md)

## 3. OpenGL 映射区段与动态常量

- [x] 3.1 沿 `UploadWorkPacket`、Device 和真实提交包复用 persistent/coherent 区段，使准入使用实际存活写的顺序、容量与分配规则。用一次大 Buffer 写、PBO 写及完成前后的重用检查验证原始值、对齐、上限和完成后无需新增 chunk；复用仍覆盖本合同的普通写/行距证据。 [上传规范](specs/opengl-resource-uploads/spec.md)
- [x] 3.2 支持公开 volatile 描述、版本写入和范围解析，把记录内已写与提交内使用过的版本交给原退休协议。用小 `maxVersions` 复核耗尽无覆写、槽 0 及完成后环回，检验无有效写入的 Draw 和一个可到达的非法描述或写入。记录实际 backing 预算，核对公开 `maxVersions` 的后端说明。 [资源分配规范](specs/opengl-device-resources/spec.md) · [RHI 合同](../../../../RHI/RHI.hpp)
- [x] 3.3 在原有图形绑定计划、Shader lowering 与 Validation 接通 volatile UBO、push 布局及独立物理 UBO 位置。以实际 VS/FS link 和活动布局核对 push 大小、偏移、阶段及普通/volatile/push 不冲突；用不兼容布局验证公开拒绝。 [Pipeline 规范](specs/opengl-shader-pipelines/spec.md)
- [x] 3.4 通过 `SetPushConstants` 和直接 Draw 消费即时数据，检查同一图形状态下连续写入、状态重设和新 recording 的行为。用真实 OpenGL 已完成的两组区域读回及一次已发前缀的失败/放弃核对逐次值和保活。更新 [RHI 能力表](../../../../RHI/README.md#opengl-原生消费边界)，核对新增支持与仍属后续的能力。 [Draw 规范](specs/opengl-immediate-draw/spec.md)

## 4. 统计与正式路径集成

- [x] 4.1 将 Renderer 的页/材质/绑定/纹理/环境占用与 RHI 的真实分配、映射复用、写入接入既有统计通道。用一次冷帧、暖帧、局部变更及容量压力核对 owner、计数和快照身份；切换面板显隐时仍使用同一快照与查询行为。 [统计规范](specs/render-frame-statistics/spec.md)
- [x] 4.2 经正式 `Physara` 的构建和启动检查现有入口。用同一固定输入在实际 RHI 上回放冻结场景、局部编辑、重新导入、多 View 共享和失败恢复，核对图像、真实写次数和保留输出；结合模块检查覆盖 Debug/Release 的共享输入校验。 [构建入口](../../../../Docs/Build.md) · [验证范围](#verification-scope)

## 5. A 本地交付与独立审查

- [x] 5.1 A 保存本地结果、源码基准、修改范围、可复现输入和检查限制，在 [B 签收原件](../../../../build/d08-t02/final/review-signed.md) 交接给维护者调度的 B。核对收到的证据与实际构建和运行的产物一致。
- [x] 5.2 B 在独立会话审查实现并补全涉及的第一方注释，在问题单按 CR 项记下意见及对应源码版本。
- [x] 5.3 A 按每轮 CR 整改并重验受影响合同，保存对应工作树、结果和仍需 B 复验的条件。
- [x] 5.4 B 复核最终实现、实际证据及整份受影响文件的注释，签收最终源码版本并关闭对应 CR。A/B 各自勾选负责的任务。 [独立审查闭环](../../../../AGENTS.md#review-loop) · [B 最终签收](#b-recheck-3)

## 6. 最终验证、同步与归档

- [x] 6.1 B 签收后按本 change 对最终源码做 verify，经真实 OpenGL 满足同一验收范围。核对复用证据的源码、构建输入、实际运行产物和设备身份，补跑变化或未覆盖的检查并记录结果。运行 OpenSpec strict 验证。 [最终验证](#final-verification)
- [x] 6.2 将通过验收的增量要求同步到主规范，核对相关 capability 与既有场景完整性，运行主规范 strict 校验并保存结果。 [规范同步](#spec-sync)
- [x] 6.3 保留审查与验收证据后按原生流程归档，更新 D08-T02 状态及下一工作包入口，按模板恢复当前问题单并检查归档引用。 [归档收尾](#archive-closeout)

<a id="verification-scope"></a>
## 验证范围与停止条件

| 风险 | 最小充分证据 | 停止条件 |
|---|---|---|
| 页内内容与逻辑版本脱节，或批合并扩大写入 | 一组 CPU 图检查覆盖一页的有效区间、待写区间和未写空洞，再由实际共享几何/材质绘制和合并后的命令范围证明编译、授权与执行一致 | 已覆盖读空洞拒绝、非零起点、邻接合并、空洞与裁剪分离、两路共享写一次和封存前晚期拒绝 |
| 重导入、材质换代或失去需求后破坏旧代 | 用本地副本经 `AssetManager` 的失效/再发布和现有 Scene 消费换代，分别读回新旧 Draw；以可区分 mip 的颜色/线性数据图，以及非对称环境的查询调整/换源核对实际像素与产品身份 | 旧区段保有到其已证完成，材质单点修改保持其它结果，mip 与 sampler 配套，查询调整无额外烘焙或静态写，换源只确认匹配产品 |
| 映射区段、volatile/push 被过早覆盖或绑定到错误位置 | 复用同一个真实 GL fixture，普通大写与 PBO 使用已知数据，volatile/push 交替写入可区分的区域、用小版本容量触发压力；比较完成值推进前后和并存的普通 UBO | 源字节在调用后可释放，槽 0、同状态下一次 Draw、重新设状态与新 recording 都符合合同；未证明完成的范围仍被保有，完成后可确证复用 |
| 预算压力或部分执行被确认成成功 | 同一帧链核对暂时不足、确定超限及一个已发上传后失败的前缀，观察真实回执、当前诊断和 UI/旧输出；若修改了无可靠 fence 的保活路径，沿已有可达失败检查核对退出收拢 | 旧 View 输出及统计保留，未执行或失败代保持待重试，前缀保活、重试与完成匹配，当前预算原因可定位，常规压力路径无全设备等待 |

在 `build/d08-t02/` 归集需要的本地输入、可执行文件和证据。已有固定原生测试优先复用，只为上表的实质覆盖缺口补独立预期。利用暖机后的零静态重写、较少的实际写调用或原生分配证明复用和合并；如报告性能收益或改变阈值，另记录同输入、同设备和同画质的真实测量。各风险有匹配当前输入的证据后停止。 [验证技能](../../../../.agents/skills/physara-verification/SKILL.md) · [D08-T01 已归档基线](../2026-11-19-prepare-incremental-scene-views/tasks.md#final-verification)

<a id="a-local-checks"></a>
## A 本地证据

- 1.1/1.2 用 `Probe.ps1 GraphRanges` 在 Debug/Release 各跑一次。128 B 页先写入 [16,32) 和 [80,96)，再通过图写 [32,48)。等价覆盖复用，覆盖冲突和跨空洞导出拒绝，真实完成后的读回保留相邻字节。设备为 Intel RaptorLake-S Mobile，OpenGL 4.6.0 Build 32.0.101.6129。
- 2.1–2.4 用 `Replay.py` 从当前 Runtime 生成观察版回放，再以 `Probe.ps1 -Name Integration -ExtraSources Runtime/FrameStatistics.cpp` 链接当前 Release 库。两份顶点和索引范围实际共页，存活相邻写合并。冷单路 60 笔、18,056,896 B，冷双路 61 笔、19,105,472 B，相差一个 1,048,576 B 的私有灯表。暖帧静态写为零，材质局部编辑只编码和写入一行。模型显式失效后发布代 1 → 2，新槽出图时旧槽仍持有原 Draw 和票据，两个真实输出均可读回。环境参数变更保持原源且零静态上传，换入 `room.exr` 时先 Pending 并保留旧输出，随后完整新产品被接受。
- 2.3/2.4 回放还验证了完全裁剪的主路不写私有数据，独立离屏路获得正常提交。缺失显示包在几何准备后触发 `DisplayUnavailable`，只执行保留旧输出的安全 UI。已发写入后失败得到前缀票据 1505，保持旧场景统计与输出代，正常票据 1506 重传并恢复。代表性容量检查用四个真实 64 MiB 页占满设备驻留额度，确认临时 Pending、单 View 硬超限及区间归还后复用。
- 预算以现有单 View 128 MiB/2,048 条和设备 256 MiB/4,096 条为边界。常规顶点/索引/材质页起步为 4 MiB/1 MiB/64 KiB，允许按实际连续区间需求分配。240 B 材质 ABI 以实际零填充写入 256 B 范围，同时满足公共 RHI 和当前设备的偏移对齐。页字节含空闲及待退休区间，独立的资源条目用于限制工作量。
- 3.1–3.4 用 `Probe.ps1 DynamicUploads` 在 Debug/Release 各跑一次。大缓冲和带行 padding 的 PBO 在未收集的两包中分配 4 次，完成后同形状上传复用 2 次且新增为零。64 MiB staging/4,096 写每列表沿用现有预算，至多保留 64 个空闲 chunk，volatile backing 独立限制 16 MiB。两版本 volatile 槽真实读回验证耗尽、槽 0 环回和新 recording 缺数据的拒绝。VS/FS 的 push 共用保留 binding，成员实测 offset 16，普通 UBO 并存。放弃已发 Draw 与零 fence 的真实写入前缀保活至完成或受控退出。颜色/数据/基础层采样读回 73/128/64。
- 4.1 由回放核对已接受帧的页容量、材质命中、环境占用、上传需求与原生池计数。取消及只接受 UI 的帧保留原统计。面板显隐路径通过直接消费者核查，面板只在可见时读取快照，GPU 轮询在 Runtime 主循环。原生压力区分待完成容量和空闲设备仍超限。
- 4.2 以 `cmake --build build --config <Debug|Release> --target Physara --parallel 4` 构建正式目标，两配置 `--hidden --poll-limit=5` 均退出 0。重跑 `SceneSlots` 4,096 对象、8,192 Primitive 槽及 480 次合并编辑，以及 `ExtentBudget`、`ClippedLayout`。Release 间歇 C1001 原始日志已保留，同输入重试通过。OpenSpec strict 校验通过。
- 本轮是 A 本地验证，原生范围为上述 OpenGL 驱动和单 Graphics 即时路径。Linux、其它 GPU/驱动及后续执行 profile 未覆盖。构建命令、八个实际 probe 二进制哈希、固定输入及日志保存在 `build/d08-t02/`。 `Evidence.py verify` 核对当前实现文件及复现输入。

<a id="review-retention"></a>
## 独立审查证据

B 第 1 轮初审提出 CR-001 至 CR-006，初审注释版源码的 binary diff SHA-256 为 `e6932aa56ee8774f9115f39a52639927b32f90088f4dadb8ee3372f2e6588b2f`。第一次整改复核关闭 CR-001、003、004、005，第二次关闭 CR-006，第三次关闭 CR-002。六项均通过，B 已签收 5.4。[B 签收原件](../../../../build/d08-t02/final/review-signed.md) · [第一次复核](#b-recheck-1) · [第二次复核](#b-recheck-2) · [最终签收](#b-recheck-3)

<a id="a-correction-1"></a>
### 第 1 轮 A 整改与自检

- CR-001、CR-002、CR-006 通过 `AForwardCorrection`。冷深度图裁去 ORM 写入，随后颜色只补 1 笔 ORM、材质常量写入为 0，原生像素与冷绘一致，暖帧静态写入为 0。已发前缀的重试使用独立纹理存储。两个固定模型版本仍分别被场景引用时保留两代，最后一个旧消费者换代后将 156 B 标为待退休，旧槽真实完成并释放后归还并复用原页区间。四个预算用例核对字节/笔数及缓存命中的请求、占用和上限。
- CR-003、CR-004、CR-005 通过 `ARhiCorrection`。三种整段范围在普通和 volatile 的 raw/Validation 入口等价，真实局部 volatile 或显式越界仍拒绝。全占用的 64 个录制包槽给出 Pending，释放一槽后 Ready 并可立即 Open，硬超限仍优先返回 ExceedsLimit。volatile 诊断以 64 B 步长和 262145 个版本报告 16,777,280 B，拒绝保持原有占用。
- 两个定向回放均以 `build/d08-t02/a-correction-1/Probe.ps1` 在 Debug 和 Release 重新链接运行。两配置正式 `Physara` 构建和 `--hidden --poll-limit=5` 启动均通过，OpenSpec strict 校验通过。原生环境为 Windows x64，Intel RaptorLake-S Mobile，OpenGL 4.6.0 Build 32.0.101.6129。
- Release 的 `AIntegrated` 以当前 Runtime 复跑多 View、局部编辑、模型和环境换代及失败前缀恢复。冷帧按各自实际存活的写区间核对，单/双路为 62/61 笔、18,056,896/19,105,472 B，新增字节恰好为一路私有灯表。失败前缀票据 1597 保留旧输出，票据 1598 正常接受重试。
- A 整改版按 B 相同的 `git diff --binary HEAD -- '*.cpp' '*.hpp'` 口径核得 SHA-256 `637e9daf074cf4ee7bdf5695beebc70f8aa6fd69afe65005e91c5e3a9becb30b`。`Evidence.py verify` 核对当前 48 个源码文件及 109 个复现输入。B 复核时以该版现场逐项确认修正、回归和受影响文件的注释，完成后再执行最终 verify。 [整改证据清单](../../../../build/d08-t02/a-correction-1/a-correction-evidence.json)

<a id="b-recheck-1"></a>
### 第 1 轮 B 整改复核

- B 核对 A 交回的 48 个源码文件及 109 个复现输入，源码差异 SHA-256 为 `637e9daf074cf4ee7bdf5695beebc70f8aa6fd69afe65005e91c5e3a9becb30b`。在同一 Intel OpenGL 4.6 驱动上重跑清单内的 Forward、RHI Debug/Release 四个定向产物，均退出 0。CR-001 的裁剪补传、暖帧零静态写和失败前缀独立存储均通过。CR-003 的范围等价、CR-004 的包槽准入和 CR-005 的总 backing 诊断均通过。
- CR-002 的双版本共存、在途退休和同页复用通过。B 另经正式 Runtime 打开参考场景再新建空场景。冻结模型表已空，6 帧覆盖三个安全槽，Draw 已全部释放，设备几何目录仍有 10 项、占用 103,292 B。CR-006 的四个公共预算边界通过。默认 AssetManager 成功解码的 4096×4096 RGBA8 图启用 mip 后，完整请求为 89,478,484 B，超出 67,108,864 B 上限，材质准备返回的硬限制诊断缺少这两个数值。这两项继续使用原 CR 编号。[B 产品入口回放](../../../../build/d08-t02/b-recheck-1/BProductEdges-Release.log)
- B 在 7 个文件中更新纹理重试、整链生产、页区间寿命与物理包准入注释，交回源码差异 SHA-256 为 `732dcfcd5ca7d0af9d96d9ec687b75c6797294c1ab889c39c8669d43ad9285c4`。48 个文件的非注释行与 A 整改版一致。本次产品补测为 Windows x64 Release，复核结论及交 A 的验收条件见问题单。[本次证据清单](../../../../build/d08-t02/b-recheck-1/b-recheck-evidence.json) · [B 签收原件](../../../../build/d08-t02/final/review-signed.md)

<a id="a-correction-2"></a>
### 第 2 次 A 整改与自检

- CR-002 在正式 Runtime 转发成功提取的空模型表。无提取的帧仍保留 10 项和 103,292 B，首次空表使目录归零，旧槽仍保有的 103,292 B 计入待退休。三个安全槽轮转并释放 Draw 后已用与待退休归零，最近正常场景票据保持为 1。双版本共存和同页复用继续通过定向 Forward 回放。
- CR-006 先计算完整的 mip 链字节数，再按 UploadBatch 准入。同一张 4096×4096 PNG 成功解码，Forward 准备在录制前返回 `SceneUploadLimit` 及 `requested=89478484 retained=400 limit=67108864`，原生上传计数未增加。定向回放仍覆盖字节、笔数及缓存命中诊断。
- 使用 `build/d08-t02/a-correction-2/Probe.ps1` 重新链接并运行 Release 的 `AProductEdges` 和 `AForwardCorrection`。产品回放链接当前 `Runtime/Frame.cpp` 与 `Runtime/FrameStatistics.cpp`，实测为 Windows x64、Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129。正式 Debug/Release 构建及两次 `--hidden --poll-limit=5` 启动通过，OpenSpec strict 通过。 [产品回放](../../../../build/d08-t02/a-correction-2/AProductEdges-Release.log) · [Forward 回放](../../../../build/d08-t02/a-correction-2/AForwardCorrection-Release.log)
- A 第二次整改版沿 B 相同口径核得源码差异 SHA-256 `9d74b1a479f1a60d1dc837c34e74e678450bf7de84fb0d51254de897d577fcf7`。`Evidence.py verify` 核对 48 个源码文件和 71 个复现路径。CR-002、CR-006 及对应注释待 B 独立复核。 [证据清单](../../../../build/d08-t02/a-correction-2/a-correction-evidence.json)

<a id="b-recheck-2"></a>
### 第 2 次 B 整改复核

- 收件源码差异 SHA-256 为 `9d74b1a479f1a60d1dc837c34e74e678450bf7de84fb0d51254de897d577fcf7`。B 逐项核对 48 个源码文件、71 个复现输入和上次交回的源码差异，重跑 A 两个 Release 产物，并将 B 上轮的产品用例重新链接到当前 Release 库。三次均退出 0。实测为 Windows x64、Intel RaptorLake-S Mobile、OpenGL 4.6.0 / 32.0.101.6129。
- CR-006 已通过。4096×4096 RGBA8 图片的 13 级 mip 链在录制前返回 `requested=89478484 retained=400 limit=67108864`，原生上传计数保持原值。CR-002 的对照中，未提取时保留 10 项，首次成功提取空表后目录归零、待退休为 103,292 B，旧槽释放后已用和待退休均为 0。[原产品用例](../../../../build/d08-t02/b-recheck-2/BProductEdges-Release.log) · [分阶段对照](../../../../build/d08-t02/b-recheck-2/AProductEdges-Release.log)
- CR-002 仍待 A 整改。B 从参考场景打开仅含一个退化三角形的新模型场景。新模型合法导入，三个槽均已提取该非空模型表。6 个诊断帧后，旧 Draw 为 0，旧几何目录仍有 10 项、已用 103,292 B、待退休 0 B。正常场景统计仍保留票据 1。该补测退出 1，需要按已提取的完整固定版本表判断哪些旧版本失去消费者。[产品补测](../../../../build/d08-t02/b-recheck-2/BModelTable-Release.log)
- B 在 4 个文件中补充了模型表、目录撤销和纹理上传额度的注释。交回源码差异 SHA-256 为 `c77b650d6937cccd0b4114860780a7854527751fd2605cb33a107490f4de0a89`，48 个文件的非注释行与 A 第二次整改版逐一匹配。[复核证据清单](../../../../build/d08-t02/b-recheck-2/b-recheck-evidence.json)

<a id="a-correction-3"></a>
### 第 3 次 A 整改与自检

- CR-002 经 Runtime 将成功提取的完整固定模型表送给原驻留 owner，空表、非空诊断表和正常场景使用相同的身份与版本判定。本轮未提取时沿用上一份观察。更新失败沿用旧目录并进入帧诊断。
- 使用 B 原始共线模型和场景，通过 `AModelTable` 重放正式 `Runtime::RenderFrame`。新场景固定 1 个模型、无可绘表面。首个诊断帧将旧目录从 10 项降至 0，并把旧槽还持有的 103,292 B 计入待退休。三个槽安全轮转后，旧 Draw、已用和待退休均为 0。正常场景统计继续对应票据 1。 [非空诊断表回放](../../../../build/d08-t02/a-correction-3/AModelTable-Release.log)
- `AProductEdges` 复验无提取时保留旧目录、空表时退休和纹理整链预算。`AForwardCorrection` 复验双版本共存、同页区间复用和纹理补传。三个回放均由 `build/d08-t02/a-correction-3/Probe.ps1` 重新链接 Release 并在当前 Intel OpenGL 4.6 驱动执行。正式 Debug/Release 构建及 `--hidden --poll-limit=5` 启动、OpenSpec strict 均通过。 [空表回放](../../../../build/d08-t02/a-correction-3/AProductEdges-Release.log) · [Forward 回放](../../../../build/d08-t02/a-correction-3/AForwardCorrection-Release.log)
- A 第三次整改版源码差异 SHA-256 为 `5c5a6fea1c9b6cef01186078b146e66b7b7710e4a447421d65851a6499c9bc99`，`Evidence.py verify` 核对 48 个源码文件及 81 个复现路径。CR-002 与本轮注释待 B 独立复核。 [证据清单](../../../../build/d08-t02/a-correction-3/a-correction-evidence.json)

<a id="b-recheck-3"></a>
### 第 3 次 B 整改复核与签收

- **签收版本**　分支 `v2.0`，HEAD `d225f9baf5df2b68233c603b84a64d5cc4258092`。B 收件时核对 A 第三次整改版 `5c5a6fea1c9b6cef01186078b146e66b7b7710e4a447421d65851a6499c9bc99` 的 48 个源码文件及 81 个复现输入。B 补充一行 Runtime 注释后，最终 `git diff --binary HEAD -- '*.cpp' '*.hpp'` 的 SHA-256 为 `9d55d58850e2cba18c06fab10d79ba3f34368ad863f0884793d6885ff7bc6a0a`。48 个文件的非注释行逐一匹配。[签收源码差异](../../../../build/d08-t02/b-recheck-3/b-signed-source.diff) · [签收证据清单](../../../../build/d08-t02/b-recheck-3/b-recheck-evidence.json)
- **CR-002**　B 原样重新链接上次失败的共线三角形用例。新文档固定 1 个合法模型、无可绘表面。三个安全槽轮转后，旧 Draw、旧目录项、已用字节和待退休字节均为 0，最近正常场景票据仍为 1。分阶段回放另确认，首个诊断帧即撤销旧场景的 10 个目录项，旧槽仍持有的 103,292 B 全部标为待退休。空表、未提取、双版本共存和同页复用对照均通过。[B 原失败用例](../../../../build/d08-t02/b-recheck-3/BModelTable-Release.log) · [分阶段退休](../../../../build/d08-t02/b-recheck-3/AModelTable-Release.log) · [空表与未提取](../../../../build/d08-t02/b-recheck-3/AProductEdges-Release.log) · [双版本及页复用](../../../../build/d08-t02/b-recheck-3/AForwardCorrection-Release.log)
- **覆盖与结论**　第三次整改只改动 Forward 声明、实现及 Runtime 帧编排。B 核对正常、诊断和仅 UI 帧的共用快照、观察内存不足时的失败处理，以及页区间随槽释放的合同。代码正确性与注释可读性均通过。本次 B 的四次原生回放均退出 0，环境为 Windows x64 Release、Intel RaptorLake-S Mobile、OpenGL 4.6.0 / 32.0.101.6129。正式 Debug/Release 构建、启动及 strict 结果沿用 [A 第三次整改记录](#a-correction-3)。

<a id="final-verification"></a>
## 最终验证

- 以 B 签收的 48 个源码文件及同口径 C++ diff SHA-256 `9d55d58850e2cba18c06fab10d79ba3f34368ad863f0884793d6885ff7bc6a0a` 为最终实现输入。已核对 B 的四次签收回放身份，按登记种子重新链接本轮探针。Windows x64、MSVC 和 Windows SDK 10.0.26100.0 上，正式 Debug/Release 均构建并以 `--hidden --poll-limit=5` 启动成功。七项 Release 回放在 Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129 上通过。九个实际执行产物和 104 个回放路径均有 SHA-256 记录。 [最终验证清单](../../../../build/d08-t02/final/verification-evidence.json) · [B 签收原件](../../../../build/d08-t02/final/review-signed.md)

| 验证维度 | 结果 |
|---|---|
| 完整性 | 7 份增量规范中的 13 条新增或修改要求均映射到正式实现。独立审查的 6 项 CR 已关闭，OpenSpec strict 校验通过。 |
| 正确性 | 下表列出改动合同对应的原生证据。既有场景保留原规范和验收出处，本轮按变更影响重跑。 |
| 一致性 | 图身份和初始化覆盖沿现有 RenderGraph。驻留与准备预算由 Renderer owner 管理，原生包和区段由 OpenGL Device 及真实 fence 退休。Runtime 只对成功的正常场景提交发布统计，Editor 读取快照。B 已复核代码正确性和注释可读性。 |

| 增量规范 | 实现与原生证据 |
|---|---|
| `render-graph-contract` | `RenderGraph.cpp` 规范化部分覆盖导入，内容查询沿用同一范围语义。`GraphRanges` 验证等价导入、空洞拒绝、非零偏移写入和相邻字节保留。 |
| `static-gpu-residency` | `StaticGpu.cpp`、`StaticTextures.cpp`、`Forward.cpp` 及 `Runtime/Frame.cpp` 交付页、整链准备、场景上传封存和接受后确认。`AForwardCorrection` 证实 ORM 裁剪后补传一次，G/B 为 0.501953 / 0.752930，暖帧静态写入为 0。版本仍被固定时保留旧代，失去消费者后进入退休，释放后复用同页区间。 |
| `opengl-device-resources`、`opengl-resource-uploads` | `Buffer.cpp`、`Upload.cpp`、`Submission.cpp` 实施对齐 backing、物理录制包和完成回收。`DynamicUploads` 的等形重复上传分配 0、复用 2，已发前缀及零 fence 保活通过。`ARhiCorrection` 在 64 个槽用尽时得到 Pending，退休一槽后 Ready 并能立刻 Open，硬限制诊断报告对齐后的完整容量。 |
| `opengl-shader-pipelines`、`opengl-immediate-draw` | `BindingPlan.cpp`、`CommandListUpload.cpp` 和 Draw 状态准备绑定当前动态 range。真实片元读回区分同一状态下先后两组常量，跨阶段 push 成员偏移 16，验证失效、耗尽、环回及已发前缀。`ARhiCorrection` 核对普通/volatile 三种等价区间和合法非零偏移。 |
| `render-frame-statistics` | `Runtime/Frame.cpp` 和 `Editor/Panels/Workspace.cpp` 关联实际提交和原生分配。`AModelTable` 首个非空诊断模型表撤销旧目录并标记 103,292 B，槽释放后归零，正常统计仍对应票据 1。`AProductEdges` 复验无提取、空表、整链 89,478,484 B 对 67,108,864 B 限制及录制前拒绝。 |

- `AIntegrated` 以签收版 Runtime 回放双 View、局部编辑和模型/环境换代。冷单/双路 60/61 笔、18,056,896/19,105,472 B，新增字节匹配独立灯表。前缀票据 1835 保留旧输出，1836 接受重试。观察到邻接页写合并、局部材质写一次、查询复用和 EXR 新代完整接受。 [Runtime 集成回放](../../../../build/d08-t02/final/AIntegrated-Release.log) · [签收版 strict](../../../../build/d08-t02/final/validate-change.json)
- 最终原生验证范围为上述 Windows/OpenGL 驱动和本工作包的基础单队列图形路径。OpenSpec 6.2 和 6.3 分别以完成同步和实际归档回执记录。

<a id="spec-sync"></a>
## 规范同步

7 份主规范已合入 11 条修改、2 条新增需求和 48 个增量场景。逐条核对受影响要求及其场景，既有场景仅按增量更新相应合同，保留其余要求和场景。全部 36 份主规范通过 OpenSpec strict 校验。 [同步清单](../../../../build/d08-t02/final/spec-sync.json) · [主规范校验](../../../../build/d08-t02/final/validate-specs.json)

<a id="archive-closeout"></a>
## 归档收尾

OpenSpec 1.13.2 已在 7 份主规范完成同步后归档本 change。D08-T02 已验收并归档，下一工作包为 D08-T03。B 签收原件与最终验收证据已留存，当前问题单已按模板重置。 [归档回执](../../../../build/d08-t02/final/archive.json) · [开发计划](../../../../Docs/Developments/Developments.md#current-stage) · [B 签收原件](../../../../build/d08-t02/final/review-signed.md)
