# Tasks

## 1. 执行入口和资源准备

- [x] 1.1 接通同图代编译计划和调用方的 owning/current 即时 Graphics 录制。使用既有图声明和 Pass 授权，交付本次录制及准备后的资源。验证依赖重排、裁剪回调缺席，以及异图代或未打开的录制在分配、模式切换和命令前被拒绝。 [入口合同](specs/render-graph-execution/spec.md#requirement-execute-only-a-matched-compiled-graph-plan)
- [x] 1.2 按计划建立导入映射、瞬态槽和当前 Pass 的 Framebuffer/绑定准备，覆盖计划中的视图范围及只读附件模式。在受控消费者观察裁剪不分配、兼容槽首用只分配一次、交叠寿命分槽、同槽状态延续。用合法查询和越权对照核对原有 `PassResources` 边界。若分配和 recipe 创建走不同入口，各用一个确定失败验证该 Pass 未执行。 [物化合同](specs/render-graph-execution/spec.md#requirement-materialize-authorized-resources-at-first-use)

## 2. 分阶段用途和单一同步

- [x] 2.1 接通公共 typed clear、普通阶段的状态合并和 `CommitBarriers`，保持声明的 mip、layer、aspect 和 byte range。用同 Pass 的清除后局部 Draw、下一 Pass 的附件 Load 和未选范围哨兵，核对 CopyDest 先于 clear、普通状态先于 Draw、完成后原生内容符合预期。 [清除合同](specs/render-graph-execution/spec.md#requirement-apply-typed-clears-before-ordinary-pass-uses)
- [x] 2.2 用同一 tracker/emitter 接通 GraphManaged 请求与独立 Automatic 推导，清除状态缓存后继续保留实际依赖。在真实 GL 上用一张小图串联上传、采样、附件产出、UI 式合成和 Shader 读后覆写，令相关重叠写入各有存活消费者。核对旧值、合成值和最终值，观察计划用途、原生同步责任和无重复推导。同一录制恢复 Automatic 后，用一个直接操作确认状态恢复及新内容可见。 [同步合同](specs/render-graph-execution/spec.md#requirement-use-one-authority-for-graph-and-automatic-synchronization)
- [x] 2.3 在真实能力支持的 profile 接通只读深度附件联合采样。用先写深度、下一 Draw 深度测试且采样同一 mip/layer 的 OpenGL 对照，核对合并状态、深度测试结果、读出值与附件未改写。记录所需依赖由同一后端发射，并核对允许写入时同范围拒绝。 [只读深度场景](specs/render-graph-execution/spec.md#scenario-sample-a-depth-image-while-its-attachment-is-read-only)

## 3. 图尾交接和失败退出

- [x] 3.1 按允许范围记录已使用导入的 entry/final，将瞬态导出保活至指定图尾用途，录制前检查同追踪单元的出口冲突。用同一消费者核对使用过但未导出的导入、延寿的瞬态输出及原 `PresentColor` 的 `ShaderResource` 交接，再用无 Pass 的直接导入和不兼容的瞬态出口区分漏交接和错误接受。核对对象、版本、未选范围的状态、Close 恢复及提交和完成边界。 [交接合同](specs/render-graph-execution/spec.md#requirement-complete-declared-resource-handoffs-at-the-end-of-execution)
- [x] 3.2 用当前诊断区间收口首错，停止后继 Pass 和图尾发布，并按录制可用性恢复 Automatic 与图形状态。复用 1.2 的准备失败，检查仍可用的列表恢复。用一个已发实际 Draw 或上传后再触发 RHI 拒绝的序列确认首错、停调度、无成功出口、发出前缀在真实完成或合法退出前仍保活。以可控失效 context 核对退出不发额外原生命令，故障控制仅在本地消费者。 [失败合同](specs/render-graph-execution/spec.md#requirement-stop-on-execution-failure-and-preserve-issued-work)

## 4. 正式构建与 A 本地验证

- [x] 4.1 构建受影响的正式模块与 `Physara` 产品，并按 [Build](../../../../Docs/Build.md#运行) 验证当前直接消费者仍能启动。Debug 与 Release 均构建并运行各自受影响的最小合同检查，Release 跑完整的原生执行链，Debug 至少复验代表性的成功及拒绝路径。记录实际命令、退出码、构建输入和产物身份，以及 GL vendor、renderer、version、预期像素/字节、实际状态与完成观察。未变化的 T01/T02 声明、编译和独立 RHI 行为复用匹配的证据，受影响项按缺口补验。

开发用消费者、Shader、数据、故障控制和日志放在 `build/d05-t03/`，正式构建使用仓库输入。新检查只针对各项列出的独立风险，可复用同一图、资源和读回结果。CPU 证据用于拒绝、顺序、数量和作用域，OpenGL 数值及完成证据使用真实设备。确认六条需求的代表性证据后停止扩大范围。 [验证入口与预算](../../../../.agents/skills/physara-verification/SKILL.md)

## 5. 独立 A/B 审查与注释

- [x] 5.1 A 本地验证通过后，在[签收问题单原件](../../../../build/d05-t03/final-verification/b-signed-ledger.md) 登记基准、实际源码差异及新增输入、复现命令、结果和限制。维护者调度 B，交接以对应输入和产物可核对为完成依据。 [A 审查交接](#a-review-handoff)
- [x] 5.2 B 独立核对计划、实现差异、原生证据和直接消费者，补齐本轮完整第一方文件的职责、接口合同和流程注释。在问题单按稳定编号记录发现、覆盖及验证要求。 [第 1 轮 B 审查](../../../../build/d05-t03/final-verification/b-signed-ledger.md#问题与整改)
- [x] 5.3 A 按编号整改并用与缺陷对应的最小用例复验，记录实际修正、输入与结果。无发现时记录适用的自检结论。 [A 整改](#a-review-corrections) · [B 复核](#b-correction-recheck) · [A 再次整改](#a-second-correction)
- [x] 5.4 B 复核整改版的实际输入、行为及注释。在问题单关闭或有依据地撤回正式发现，分别记录代码正确性和注释可读性，签收最终版本。 [B 签收](#b-review-acceptance) · [独立审查闭环](../../../../.agents/skills/physara-review/SKILL.md)

## 6. 最终验证与归档

- [x] 6.1 B 签收后按 OpenSpec verify 将每条需求和场景对应到签收源码、直接消费者与结果。复用身份一致的原生证据，仅补实际缺口，记录真实设备验证、命令、运行结果和覆盖限制。 [最终验证](#final-verification)
- [x] 6.2 最终验证通过后同步 `render-graph-execution` 的 Purpose 与需求，使用仓库锁定的 OpenSpec 版本严格校验场景和主规范。 [主规范同步](#spec-sync)
- [x] 6.3 留存 A/B 与最终验证证据后归档，核对产物、链接和任务状态。更新 D05-T03 和下一工作包的开发计划引用，归档成功后按模板重置当前问题单。 [归档结果](#archive-result)

<a id="a-local-verification"></a>
## A 本地验证

2026-11-14，基准为 `v2.0` / `98f2ccc0943385ef2085fdebdc84f3e1519edcbb`，暂存区为空。实际验证输入包含 1124 项正式树文件，按仓库相对路径和每项原始字节 SHA-256 生成有序清单，聚合摘要为 `7ac5d12984727c580675f0f51cd88119c0300bf5790656520c2bf6402a278518`。8 项本地输入的同法摘要为 `de882b7f213790928fc1cf4a6a0dafd9308413a68631be9f972c02ec234480d1`，复现包摘要为 `44f44bd11bd03020b4a3b19a6c8ea338a50020b3e5b21cec4c5ecafdc846c56e`。分别记录两配置各 19 项正式产物及 4 项消费者产物，比较构建至运行期间的实际输入和产物身份。 [输入清单](../../../../build/d05-t03/formal-inputs.json) · [本地输入包](../../../../build/d05-t03/handoff-fixture.zip) · [Release 命令与产物](../../../../build/d05-t03/evidence-release.json) · [Debug 命令与产物](../../../../build/d05-t03/evidence-debug.json)

在 Windows x64 使用 VS 2022/MSVC 19.44.35228 与 SDK 10.0.26100.0。从仓库根执行如下入口。正式构建使用现有 Visual Studio 17 2022 二进制树，消费者链接正式库并用 glslang 14.3.0 编译三个小 Shader。Release 跑全链，Debug 跑入口、槽复用、色彩链和代表性拒绝。两次消费者与两次产品 `--hidden --poll-limit=3` 均退出 0。 [Release 原生结果](../../../../build/d05-t03/fixture-run-release-handoff.log) · [Debug 原生结果](../../../../build/d05-t03/fixture-run-debug-handoff.log) · [产品 Release](../../../../build/d05-t03/product-run-release-handoff.log) · [产品 Debug](../../../../build/d05-t03/product-run-debug-handoff.log)

```powershell
python build/d05-t03/Evidence.py capture
if ($LASTEXITCODE -ne 0) { throw "Could not capture build inputs" }
foreach ($configuration in 'Release', 'Debug') {
    $runLog = "build/d05-t03/formal-$configuration-handoff.log"
    cmake --build build --config $configuration --target PhysaraRenderGraph Physara --parallel 1 > $runLog 2>&1
    if ($LASTEXITCODE -ne 0) { throw "$configuration formal build failed. See $runLog" }
}
python build/d05-t03/Evidence.py Release
if ($LASTEXITCODE -ne 0) { throw "Release evidence failed" }
python build/d05-t03/Evidence.py Debug
if ($LASTEXITCODE -ne 0) { throw "Debug evidence failed" }
```

原生设备为 Intel / Intel(R) RaptorLake-S Mobile Graphics Controller / OpenGL 4.6.0 Build 32.0.101.6129。图的命令调用使用正式 Validation/OpenGL，非零设备提交与 Event 完成分别检查。消费方只在完成后用本地 GL 冷路径读回。

| 任务 | 自检结果 |
|---|---|
| 1.1 | 异图代、未打开列表、手动模式、带待交用途的录制均在设备创建或 GL 命令前拒绝。覆盖写在声明中位于旧版读取之前，实跑按编译顺序移到该读取之后。 |
| 1.2 | 三个存活逻辑 Buffer 使用两个物理槽，首、末资源同对象，重叠对象不同，已裁剪回调缺席。复用前为 CopySource，后任写入为 CopyDest，原生读取的首 8 字节等于 `0x1234aa55ee778800`。附件与样本视图按原 mip/layer 准备，越权查询、首次分配失败和无效 BindingSet 各通过拒绝对照。 |
| 2.1 | 局部 Draw 时已发生 clear，RenderTarget 用途在回调前生效。Load 保留未写像素蓝色 `0,0,255,255`，邻层保留洋红。R32_UINT 读回 170，D24S8 只改变所选 mip/layer 的 stencil 至 41，邻 mip/layer 保留 77，深度保持 `0xbfffff`。 |
| 2.2 | 上传、采样、Load、覆写、UI 式合成共 5 个回调，2 个上传和 3 个 Draw。旧值和合成均为 `211,13,37,255`，覆写值 `0,255,255,255`，复用直接 Automatic copy 后读到相同合成值。该有序色彩链无 TextureBarrier。保留 tracker 的前态以探测未声明用途，写入仍由当前提交范围检查拒绝。 |
| 2.3 | 实机允许 D32 联合采样，合并状态含 DepthRead 和 ShaderResource。结果深度 0.25，通过像素 `64,0,0,255`，拒绝像素 `0,0,0,255`。多次 Draw 以及同包恢复 Automatic 的同范围读取共发射一次 TextureBarrier。启用写的 Pipeline 无 Draw，并独立检查未公布联合能力时的编译拒绝。 |
| 3.1 | 使用但未导出的纹理只更新选中层到 CopySource，邻层状态保持 CopyDest。瞬态导出保留实际对象和版本，双用途冲突拒绝。零 Pass 直接导入通过。宿主原 PresentColor 从首次 Common 回到 ShaderResource，经 Close 和已接受提交后第二份零 Pass 图从 ShaderResource 进入，交还同一对象。 |
| 3.2 | BindingSet 准备失败未发生该 Pass 的 clear，后续直接 RHI 清除在 Automatic 下成功。实写上传前缀后拒绝未准备的用途，同回调的后续查询错误未覆盖首错。提交拒绝后引用沿 orphan 保留至实际 WaitForIdle，返回失败且无成功输出。以受控 context 不可用核对退出及失效清理未调用 GL，保留原始执行失败。 |

Debug 的一次增量链接在 `ValidationCommandList.obj` 报 `LNK1163`，已保留当次日志、对象及库。以原编译选项串行重新编译该对象并链接通过，后续以清单核对产物再运行，原因未确定。较早的编译和视图准备定位记录仍在本地目录。未变化的 T01 声明与 T02 依赖算法沿用归档结论，本次原生消费者复查受影响的用途和录制边界。实测覆盖该设备的首发 Graphics/传输 profile，实际设备丢失、其他驱动、产品图接线、窗口交换、Compute、正式 staging 和性能未测。 [首次 Debug 链接日志](../../../../build/d05-t03/formal-Debug-final.log) · [D05-T01](../../archive/2026-11-13-establish-render-graph-contract/tasks.md#final-verification) · [D05-T02](../../archive/2026-11-14-compile-render-graph-plan/tasks.md#final-verification)

<a id="a-review-handoff"></a>
## A 审查交接

已在当前问题单登记首轮待审版本、实际改动和新增文件、复现入口及限制。恢复工作后重新计算正式源码、夹具、两种配置的产物和日志摘要，均与验证记录一致。B 随后独立审查，补齐注释并登记 CR-001 至 CR-006。 [B 首审记录](../../../../build/d05-t03/final-verification/b-signed-ledger.md#问题与整改) · [B 注释检查](../../../../build/d05-t03/review-b/annotation-check.json)

<a id="a-review-corrections"></a>
## A 整改与本地复验

2026-11-14，整改开始前逐项核对 B 补注释的 32 个文件，原始字节均与 B 清单相符。基准为 `v2.0` / `98f2ccc0943385ef2085fdebdc84f3e1519edcbb`，暂存区为空。整改版 1124 项正式输入的 SHA-256 为 `e6ec787d32db3557defcc3ea5e8e339985c92ab1f2d4710614456efa61d4e55d`，13 项复现输入为 `54199b4e136fe1c0a02e29ea41c16e5019806884865e155717bb8ee10028853a`，输入包为 `2bffdb12db01cf132a0babee4c5778abdacc8f0a497f48e29542c7286c3b6fdb`。 [输入清单](../../../../build/d05-t03/review-a/inputs.json) · [本地输入包](../../../../build/d05-t03/review-a/correction-fixture.zip)

| 问题 | 修正与实际结果 |
|---|---|
| CR-001 | 执行器独占 GraphManaged 状态准备。包装和原始命令列表拒绝回调重新接管及修改 tracker 或用途批次，实测哨兵保持 77。Release 全链成功发出合法命令，恢复 Automatic 后继续直接复制。 [状态回归](../../../../build/d05-t03/review-a/release-state-override.log) · [原生全链](../../../../build/d05-t03/review-a/release-native.log) |
| CR-002 | 按用途及确切字节范围核对 WriteBuffer、CopyBuffer 两端、活动绑定和 Draw 的可判定区间。合法写入和复制为 `41,77`，相邻越界操作为 `77,77`。合法 indexed Draw 读回绿色，越界的顶点、实例、索引和常量绑定发出零次 Draw。Indexed Draw 的顶点索引值由数据提供者保证落在声明内。 [传输回归](../../../../build/d05-t03/review-a/release-buffer.log) · [Draw 与绑定回归](../../../../build/d05-t03/review-a/release-draw-range.log) |
| CR-003 | FBO 实际单挂 Depth 或 Stencil，对照原生挂载点和读回。Depth-only Draw 保留 stencil 77，Stencil-only Draw 写出 41 并保留深度 `0xffffff`，需要缺席 stencil 的 Pipeline 发出零次 Draw。既有分量清除、附件清除入口及 D32 联合采样通过，联合采样仍发出一次 TextureBarrier。 [附件回归](../../../../build/d05-t03/review-a/release-attachment.log) · [原生全链](../../../../build/d05-t03/review-a/release-native.log) |
| CR-004 | 空图在 B 原发布时点尚保留待触发的分配故障并成功交付输出，全链另验证非空输出。普通纹理在原定点得到 CPU 分配诊断，原生创建数未增加，重试成功。Debug 保持迭代器检查等级 2。 [图尾故障注入](../../../../build/d05-t03/review-a/debug-publish-oom.log) · [纹理故障注入](../../../../build/d05-t03/review-a/debug-texture-oom.log) |
| CR-005 | 实际同格式视图支持 Depth。Stencil-only 与双分量的普通采样及着色导出均在编译时拒绝。Depth 从导出到下一张图绑定和 Draw，读回 `64,0,0,255`。 [采样与导出回归](../../../../build/d05-t03/review-a/release-stencil-export.log) |
| CR-006 | 保留先于异常记录的 `AccessDenied` 及资源、Pass 和版本 1。单独的分配失败得到 `OutOfStorage`。两条路径的图清除与 execute 均未进入，恢复后的直接 Automatic clear 随原录制提交并完成。 [Release](../../../../build/d05-t03/review-a/release-prepare-error.log) · [Debug](../../../../build/d05-t03/review-a/debug-prepare-error.log) |

Windows x64，MSVC 19.44.35228 / SDK 10.0.26100.0。正式构建、消费者运行与产品启动均退出 0。Release 执行原生全链和六组整改回归，Debug 执行代表性原生链、两个定点分配故障及 prepare 首错。验证脚本在构建、链接、运行之间核对输入与正式产物，对两配置分别登记 19 个正式产物和 6 个消费者产物。初次在沙箱内运行 MSBuild 因 SDK 路径读取被拒绝而报 MSB4184，允许读取 SDK 后以原构建配置通过。 [Release 证据](../../../../build/d05-t03/review-a/evidence-release.json) · [Debug 证据](../../../../build/d05-t03/review-a/evidence-debug.json) · [MSB4184 日志](../../../../build/d05-t03/review-a/formal-Release-first.log)

```powershell
python build/d05-t03/review-a/EvidenceReview.py capture
if ($LASTEXITCODE -ne 0) { throw "Could not capture inputs" }
foreach ($configuration in 'Release', 'Debug') {
    $log = "build/d05-t03/review-a/formal-$configuration-verified.log"
    cmake --build build --config $configuration --target PhysaraRenderGraph Physara --parallel 1 > $log 2>&1
    $code = $LASTEXITCODE
    Add-Content -Encoding utf8 -LiteralPath $log -Value "D05_FORMAL_BUILD_EXIT=$code"
    if ($code -ne 0) { throw "$configuration build failed" }
    python build/d05-t03/review-a/EvidenceReview.py $configuration
    if ($LASTEXITCODE -ne 0) { throw "$configuration checks failed" }
}
```

原生验证使用 Intel RaptorLake-S Mobile / OpenGL 4.6.0 Build 32.0.101.6129。分配和 context 失效使用可控注入，实际观察已接受提交、Event 完成或失败包的安全退休。产品接线、窗口交换、其他驱动和性能验证依各自工作包。

恢复交接时重新计算源码、复现输入与 ZIP、两种配置的产物及日志摘要，均与整改复验记录相符。OpenSpec 1.13.2 严格校验通过 21/21 项。B 复核由维护者调度。 [签收问题单原件](../../../../build/d05-t03/final-verification/b-signed-ledger.md#本轮交接) · [OpenSpec 严格校验](../../../../build/d05-t03/review-a/openspec-strict.json)

<a id="b-correction-recheck"></a>
## B 第 1 轮整改复核

2026-11-15，B 按 A 整改版源码 `e6ec787d32db3557defcc3ea5e8e339985c92ab1f2d4710614456efa61d4e55d` 核对实际工作树及已登记的复现输入、构建产物和日志。Intel OpenGL 4.6 实机独立重跑两配置的 11 次原生和回归程序，全部通过。CR-001、CR-002、CR-003、CR-006 已复核关闭。现有证据的产品启动结果沿用 A。 [独立重跑](../../../../build/d05-t03/review-b/recheck-1/a-regression-rerun.json) · [A 整改证据](#a-review-corrections)

CR-004 的实际图内 BindingSet 准备在 MSVC Debug 代理分配失败时触发进程终止，CR-005 的 R32_UINT 在 Release 可发布 `ShaderRead`，随后同设备拒绝同格式采样视图。两项保留原编号继续整改。补齐受影响的第一方注释，逐文件确认除注释外的代码一致。未签收整改版，最终验证仍待复核通过。 [问题与整改](../../../../build/d05-t03/final-verification/b-signed-ledger.md#问题与整改) · [补查记录](../../../../build/d05-t03/review-b/recheck-1/edge-runs.json) · [注释检查](../../../../build/d05-t03/review-b/recheck-1/annotation-check.json)

<a id="a-second-correction"></a>
## A 再次整改与本地复验

2026-11-15，整改前核对 1124 项正式输入与 B 补注释后的摘要 `42f0f1bd443114971b376ea95fea6595750686e153755085af66282295c16f45` 一致。基准仍为 `v2.0` / `98f2ccc0943385ef2085fdebdc84f3e1519edcbb`，暂存区为空。整改影响 `PassPreparation.cpp`、OpenGL `Formats.cpp` 和 `Bindings.cpp` 及 RHI 模块说明。再次整改版 1124 项正式输入 SHA-256 聚合摘要为 `57d8a0c645662c5afb744970fc1f7a52f08a08f71ee1bca52c82768f767e841f`，15 项复现输入为 `aa632adb3b5eaba565f625696f59fc0b421ebad0d2b67ecb931f7c8d81c98486`，输入包摘要为 `67a259bba373bdca68401acb1b6aeb18e6dd3905bc3bd65ddd35a65eadc93fee`。 [再次整改输入](../../../../build/d05-t03/review-a-second/inputs.json) · [本地输入包](../../../../build/d05-t03/review-a-second/correction-fixture.zip)

| 问题 | 修正与结果 |
|---|---|
| CR-004 | Pass 直接从授权范围构造拥有型 BindingSet 描述。Debug 保留迭代器检查等级 2，在 B 的原注入点返回 `GraphExec.Prepare / OutOfStorage`，跳过该 Pass 的 clear 和 execute，恢复同一列表的 Automatic。此前已发 clear 经提交和 Event 完成后读回红色 `255,0,0,255`。原输出发布和普通纹理分配注入、正常绑定对照通过。 [绑定故障](../../../../build/d05-t03/review-a-second/debug-binding-oom.log) · [Debug 证据](../../../../build/d05-t03/review-a-second/evidence-debug.json) |
| CR-005 | OpenGL 的 `ShaderSample` 表示驱动与首发浮点采样路径共同支持的格式。图和原生绑定消费同一能力，整数存储继续使用已有的资源能力。`R32_UINT` 的 `Sampled` 和 `ShaderRead` 在编译时拒绝，直接绑定在创建原生视图前拒绝，typed clear 与 `Readback` 完成后读到 `170`。颜色合成、Depth 联合采样和同格式 Depth 导出后的实际采样仍通过。 [整数回归](../../../../build/d05-t03/review-a-second/release-integer-output.log) · [Depth 导出回归](../../../../build/d05-t03/review-a-second/release-stencil-export.log) · [Release 原生链](../../../../build/d05-t03/review-a-second/release-native.log) |

正式构建使用 VS 2022/MSVC 19.44.35228 和 Windows SDK 10.0.26100.0。两种配置的正式构建、原生程序、定点回归和 `Physara --hidden --poll-limit=3` 启动均退出 0。各记录 19 项正式产物和 7 项复现产物。原生证据来自 Intel RaptorLake-S Mobile / OpenGL 4.6.0 Build 32.0.101.6129，分配故障使用本地定点注入。已由 B 通过的独立定点用例沿用其复核结论，本次原生链复验共同受影响的绑定和图尾行为。OpenSpec 1.13.2 严格校验通过 21/21。 [Release 证据](../../../../build/d05-t03/review-a-second/evidence-release.json) · [Debug 证据](../../../../build/d05-t03/review-a-second/evidence-debug.json) · [OpenSpec 校验](../../../../build/d05-t03/review-a-second/openspec-strict.json)

```powershell
python build/d05-t03/review-a-second/EvidenceReview.py capture
if ($LASTEXITCODE -ne 0) { throw "Could not capture inputs" }
foreach ($configuration in 'Release', 'Debug') {
    $log = "build/d05-t03/review-a-second/formal-$configuration-verified.log"
    cmake --build build --config $configuration --target PhysaraRenderGraph Physara --parallel 1 > $log 2>&1
    $code = $LASTEXITCODE
    Add-Content -Encoding utf8 -LiteralPath $log -Value "D05_FORMAL_BUILD_EXIT=$code"
    if ($code -ne 0) { throw "$configuration build failed" }
    python build/d05-t03/review-a-second/EvidenceReview.py $configuration
    if ($LASTEXITCODE -ne 0) { throw "$configuration checks failed" }
}
```

维护者调度 B 复核 CR-004、CR-005 及受影响的注释，并签收对应版本。 [签收问题单原件](../../../../build/d05-t03/final-verification/b-signed-ledger.md#本轮交接)

<a id="b-review-acceptance"></a>
## B 再次整改复核与签收

2026-11-15，B 核对 `v2.0` / `98f2ccc0943385ef2085fdebdc84f3e1519edcbb` 未暂存工作树、A 的 1124 项正式输入 `57d8a0c645662c5afb744970fc1f7a52f08a08f71ee1bca52c82768f767e841f`、15 项复现输入 `aa632adb3b5eaba565f625696f59fc0b421ebad0d2b67ecb931f7c8d81c98486`，以及复现包、两配置各 26 项产物和相应日志。所有输入与产物均与交接一致。 [交接身份核对](../../../../build/d05-t03/review-b/recheck-2/handoff-check.json)

| 问题 | B 验收摘要 |
|---|---|
| CR-001 | 图执行器独占状态接管和提交，回调不能扩大计划权限。原始和 Validation 列表的写入拒绝及哨兵 77 沿用首次复核，图后 Automatic copy 在再次复验通过。 [首次回归](../../../../build/d05-t03/review-b/recheck-1/release-state-override.log) · [再次原生链](../../../../build/d05-t03/review-b/recheck-2/release-native.log) |
| CR-002 | 实际 Buffer 传输、绑定和 Draw 可确定的字节区间均受各自用途约束。沿用首次复核的传输哨兵与 Draw 对照。Indexed 的逐顶点索引值范围由数据提供方保证。 [传输](../../../../build/d05-t03/review-b/recheck-1/release-buffer.log) · [Draw](../../../../build/d05-t03/review-b/recheck-1/release-draw-range.log) |
| CR-003 | Packed 附件按声明挂载分量，清除与 Draw 消费实际挂载点。沿用单分量哨兵与缺席分量的拒绝对照，再次复验 D32 联合采样只发一次 TextureBarrier。 [附件](../../../../build/d05-t03/review-b/recheck-1/release-attachment.log) · [再次原生链](../../../../build/d05-t03/review-b/recheck-2/release-native.log) |
| CR-004 | 图尾移交只读独占输出，纹理历史在可报告失败的路径分配。BindingSet 从授权条目直接构造。三处 Debug 定点复验通过，绑定准备返回 `GraphExec.Prepare / OutOfStorage`，跳过该 Pass 的 clear 和 execute，恢复 Automatic，先前清除完成后读回 `255,0,0,255`。 [绑定故障](../../../../build/d05-t03/review-b/recheck-2/debug-binding-oom.log) · [输出故障](../../../../build/d05-t03/review-b/recheck-2/debug-publish-oom.log) · [纹理故障](../../../../build/d05-t03/review-b/recheck-2/debug-texture-oom.log) |
| CR-005 | 采样格式能力与实际绑定共用 `ShaderSample`。R32_UINT 的 `Sampled` 和 `ShaderRead` 在编译拒绝，直接绑定不创建原生视图，typed clear 读回 `170`。独立复验 stencil 分量拒绝及 packed Depth 成功交接后采样。 [整数](../../../../build/d05-t03/review-b/recheck-2/release-integer-output.log) · [分量](../../../../build/d05-t03/review-b/recheck-2/release-stencil-export.log) |
| CR-006 | 保留 prepare 的首个查询错误，再发生的异常经既有边界收口。沿用首次复核的首错、无清除与 Automatic 恢复证据。再次复验共同的绑定准备和出错退出路径。 [首错](../../../../build/d05-t03/review-b/recheck-1/debug-prepare-error.log) · [再次原生链](../../../../build/d05-t03/review-b/recheck-2/debug-native.log) |

代码复核全部通过。B 核对格式与分量能力、BindingSet 构造原因与异常传播，并补齐 4 个文件的注释。签收版正式输入 SHA-256 为 `ae5918ff4a28ef49dc93d3bdbcb337be4b483f71c71359c15efb92a21a1c94ad`，与构建和重跑所用的 A 输入仅有注释差异。 [签收输入](../../../../build/d05-t03/review-b/recheck-2/accepted-inputs.json) · [注释差异](../../../../build/d05-t03/review-b/recheck-2/annotation-check.json)

B 在 Intel RaptorLake-S Mobile / OpenGL 4.6.0 Build 32.0.101.6129 独立重跑 9 次，含 Release 原生全链、Debug 代表性链路、五组定点回归及两配置产品启动，全部退出 0。补注释后的字节身份已另存清单，最终 verify 按 6.1 记录签收输入与对应原生证据。产品 Graph 接线、实际窗口交换、其他驱动和性能验证依各自工作包。 [运行命令、日志和退出码](../../../../build/d05-t03/review-b/recheck-2/rerun.json) · [问题单验收](../../../../build/d05-t03/final-verification/b-signed-ledger.md#本轮验收与留存)

<a id="final-verification"></a>
## 最终验证

2026-11-15，按 OpenSpec 1.13.2 的 spec-driven 工作流核对规划产物、实施任务及独立审查签收。当前工作树的 1124 项正式输入与 B 签收摘要 `ae5918ff4a28ef49dc93d3bdbcb337be4b483f71c71359c15efb92a21a1c94ad` 一致，基准为 `v2.0` / `98f2ccc0943385ef2085fdebdc84f3e1519edcbb`。独立核对 B 的签收版本、四处整行注释差异、此前 15 项复现输入与 ZIP、两配置各 26 项产物，以及 9 次最近复跑和 6 次相关的前轮复跑。签收问题单原件 SHA-256 为 `17727ef9d72c503e65bfb581369307cb0d021aa5dfddfd48ec623316ddabf0cc`，其中相对链接以原 `Docs/` 位置为基准。 [签收输入](../../../../build/d05-t03/review-b/recheck-2/accepted-inputs.json) · [证据核对](../../../../build/d05-t03/final-verification/b-evidence-match.json) · [签收问题单原件](../../../../build/d05-t03/final-verification/b-signed-ledger.md)

在 VS 2022 x64、MSVC 19.44.35228 和 Windows SDK 10.0.26100.0 下重建签收版的 `PhysaraRenderGraph` 和 `Physara`，两种配置均退出 0。使用原复现输入的副本链接签收版正式库。Release 的原生全链和 `integer-output`、Debug 的代表性原生链和 `binding-oom`、两配置的 `Physara --hidden --poll-limit=3` 均退出 0。每种配置记录 19 项正式产物、5 项复现产物和实际运行命令。构建及运行使用 Intel RaptorLake-S Mobile / OpenGL 4.6.0 Build 32.0.101.6129。 [验证入口](../../../../build/d05-t03/final-verification/VerifySigned.py) · [签收构建输入](../../../../build/d05-t03/final-verification/signed-build-inputs.json) · [Release 证据](../../../../build/d05-t03/final-verification/evidence-release.json) · [Debug 证据](../../../../build/d05-t03/final-verification/evidence-debug.json)

```powershell
python build/d05-t03/final-verification/VerifySigned.py capture
if ($LASTEXITCODE -ne 0) { throw "Signed inputs differ" }
foreach ($configuration in 'Release', 'Debug') {
    $log = "build/d05-t03/final-verification/formal-$configuration.log"
    cmake --build build --config $configuration --target PhysaraRenderGraph Physara --parallel 1 > $log 2>&1
    $code = $LASTEXITCODE
    Add-Content -Encoding utf8 -LiteralPath $log -Value "D05_FORMAL_BUILD_EXIT=$code"
    if ($code -ne 0) { throw "$configuration build failed" }
    python build/d05-t03/final-verification/VerifySigned.py $configuration
    if ($LASTEXITCODE -ne 0) { throw "$configuration verification failed" }
}
```

| 验证维度 | 结果 |
|---|---|
| 完整性 | 实施、独立审查和最终验证已完成 13/15，剩余为主规范同步和归档。六条新增需求都有实现。 |
| 正确性 | 下表逐项核对六条需求和十三个场景。签收版实跑与 B 的定点复核覆盖本工作包。 |
| 一致性 | 执行器保持对 Core 与公共 RHI 的依赖，设备和原生同步归属 RHI。图声明、编译和执行分别沿已有入口，宿主保留窗口生命周期。正式构建只使用仓库输入。 [设计决定](design.md#decisions) · [图构建边界](../../../../Engine/Renderer/RenderGraph/CMakeLists.txt) · [架构合同](../../../../Docs/Architecture.md#barriers) |

| 需求与场景 | 签收实现、直接消费者和结果 |
|---|---|
| [仅执行匹配的编译计划](specs/render-graph-execution/spec.md#requirement-execute-only-a-matched-compiled-graph-plan) | （1）上传覆盖在编译顺序移至旧版采样之后，五个存活回调各调用一次。槽复用用例运行四个回调，裁剪的分支无分配。（2）异图代、未打开列表、显式模式及有待提交请求的录制在接管和发出命令前拒绝。 [入口与阶段检查](../../../../Engine/Renderer/RenderGraph/GraphExecutor.cpp#L571) · [RHI 接管](../../../../RHI/OpenGL/CommandList.cpp#L175) · [入口消费者](../../../../build/d05-t03/final-verification/fixture/Graphs.cpp#L528) · [原生结果](../../../../build/d05-t03/final-verification/release-native.log) |
| [按首用物化并保持权限](specs/render-graph-execution/spec.md#requirement-materialize-authorized-resources-at-first-use) | （3）三个存活逻辑 Buffer 共用两个物理分配，相交寿命分槽，前任实际 `CopySource` 状态接续到后任的 `CopyDest`，原生读到 `0x1234aa55ee778800`。附件和采样 recipe 由当前 Pass 准备，越权查询及不可用 recipe 均拒绝。 [首次物化](../../../../Engine/Renderer/RenderGraph/GraphExecutor.cpp#L196) · [绑定准备](../../../../Engine/Renderer/RenderGraph/PassPreparation.cpp#L65) · [原生结果](../../../../build/d05-t03/final-verification/release-native.log) |
| [按阶段执行精确清除](specs/render-graph-execution/spec.md#requirement-apply-typed-clears-before-ordinary-pass-uses) | （4）先提交 CopyDest 并清除，再提交 Draw 的附件用途。后继 Load 保留蓝色底色，相邻层为洋红，整数 clear 读到 170，D24S8 选中 stencil 为 41，邻层/邻 mip 仍为 77，深度为 `0xbfffff`。 [清除与用途阶段](../../../../Engine/Renderer/RenderGraph/GraphExecutor.cpp#L311) · [原生分量清除](../../../../RHI/OpenGL/CommandListTransfer.cpp#L149) · [原生结果](../../../../build/d05-t03/final-verification/release-native.log) |
| [共用同步权威](specs/render-graph-execution/spec.md#requirement-use-one-authority-for-graph-and-automatic-synchronization) | （5）实际上传、采样、Draw 与覆写区分旧值和合成值 `211,13,37,255` 及最终青色 `0,255,255,255`。（6）D32 联合采样读到 0.25，通过和拒绝像素分别为 `64,0,0,255` 和 `0,0,0,255`，深度保持，真实发出一次 TextureBarrier。可写反馈对照被拒。（7）同包恢复 Automatic 后直接 CopyTexture 得到相同合成像素。B 的定点复核还覆盖状态覆盖、Buffer 越界与附件分量。 [用途提交](../../../../RHI/OpenGL/CommandListState.cpp#L241) · [实际 Draw](../../../../RHI/OpenGL/CommandListDraw.cpp#L24) · [原生联合采样同步](../../../../RHI/OpenGL/CommandListGraphics.cpp#L213) · [原生结果](../../../../build/d05-t03/final-verification/release-native.log) · [B 复核](#b-review-acceptance) |
| [完成版本和状态交接](specs/render-graph-execution/spec.md#requirement-complete-declared-resource-handoffs-at-the-end-of-execution) | （8）用过但未导出的分层纹理只更新选中层，瞬态输出交付正确版本和句柄。实际宿主 `PresentColor` 归还原对象和 ShaderResource，Close 后以同状态跨录制接入。两次提交均接受并完成。（9）零 Pass 的直接导入保留版本、范围和对象，并转换到 final。（10）不相容的瞬态出口在分配前返回资源和范围。格式对照中的整数输出以 Readback 读到 170，ShaderRead 与 Sampled 均在编译拒绝。 [图尾实现](../../../../Engine/Renderer/RenderGraph/GraphExecutor.cpp#L481) · [冲突诊断](../../../../Engine/Renderer/RenderGraph/GraphExecutionStates.cpp#L53) · [宿主消费者](../../../../build/d05-t03/final-verification/fixture/Graphs.cpp#L834) · [原生结果](../../../../build/d05-t03/final-verification/release-native.log) · [整数回归](../../../../build/d05-t03/final-verification/release-integer-output.log) |
| [保留首错与已发工作](specs/render-graph-execution/spec.md#requirement-stop-on-execution-failure-and-preserve-issued-work) | （11）首用 Buffer 创建及绑定准备失败定位到相应阶段，阻止清除、执行和出口。Debug 的绑定分配失败返回 OutOfStorage，恢复 Automatic，此前已发的红色 clear 完成。（12）已发上传后的错误阻止后续 Pass，拒绝提交包通过 orphan 保活至真实完成。（13）受控 context 不可用时保留原诊断，退出和失效清理的原生命令计数为零。 [首错和退出](../../../../Engine/Renderer/RenderGraph/GraphExecutor.cpp#L388) · [归还录制](../../../../RHI/OpenGL/CommandList.cpp#L194) · [原生退休](../../../../RHI/OpenGL/Submission.cpp#L118) · [原生结果](../../../../build/d05-t03/final-verification/release-native.log) · [Debug 分配](../../../../build/d05-t03/final-verification/debug-binding-oom.log) |

原生结论覆盖本机首发 Graphics/传输 profile。故障使用本地定点注入或可控 context 状态。D05-T04 产品接线和真实窗口交换、D05-T05 失败帧调度、其他驱动、Compute 和性能仍属各自验证范围。OpenSpec 1.13.2 同步前严格校验为 21/21。 [严格校验](../../../../build/d05-t03/final-verification/openspec-strict-before-sync.json)

<a id="spec-sync"></a>
## 主规范同步

按 OpenSpec 1.13.2 将 `render-graph-execution` delta 的 Purpose、六条需求和十三个场景完整同步到新主规范。已有 20 份主规范的 SHA-256 均与同步前一致。严格校验通过 21/21 份主规范，含活动 change 为 22/22。 [主规范](../../../specs/render-graph-execution/spec.md) · [同步清单](../../../../build/d05-t03/final-verification/spec-sync.json) · [主规范严格校验](../../../../build/d05-t03/final-verification/openspec-specs-strict-after-sync.json) · [合并校验](../../../../build/d05-t03/final-verification/openspec-all-strict-after-sync.json)

<a id="archive-result"></a>
## 归档结果

`openspec archive execute-render-graph-plan --yes --skip-specs --json` 退出 0，归档目录为 `2026-11-15-execute-render-graph-plan`。移动时 14/15，剩余 6.3 为归档收尾。移动前后五份产物的 SHA-256 均匹配。随后按归档位置修正相对链接，保留 B 签收问题单原件的引用。归档后活动 change 清单为空，全部 21 份主规范严格校验通过。 [CLI 记录](../../../../build/d05-t03/final-verification/archive-cli.json) · [移动前清单](../../../../build/d05-t03/final-verification/archive-inputs.json) · [归档后严格校验](../../../../build/d05-t03/final-verification/openspec-strict-after-archive.json)

开发计划记录 D05-T03 已验收归档，D05-T04 的前置满足并待立项。完成归档与链接检查后，审查问题单按模板恢复，SHA-256 与模板一致。任务完成 15/15。 [重置前的身份和链接核对](../../../../build/d05-t03/final-verification/archive-check-before-reset.json) · [开发计划](../../../../Docs/Developments/Developments.md#current-stage) · [审查模板](../../../../Docs/CodeReview.md.tpl) · [重置记录](../../../../build/d05-t03/final-verification/ledger-reset.json)
