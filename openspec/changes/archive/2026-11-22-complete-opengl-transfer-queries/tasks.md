# Tasks

## 1. 传输能力与公共参数

- [x] 1.1 将已交付操作与实际 format、target、usage、native 入口和样本集合合并为 Device 传输能力。核对普通分配仍可独立使用，采集一个确实支持的 resolve 组合和一个分配合法但不能 resolve 的组合。记录可调用的各项用途和样本能力。 [能力设计](design.md#transfer-capabilities)
- [x] 1.2 扩充直接传输入口的公共预检及 raw 对等拒绝。复用既有 range 和 format 布局 helper。以等字节数的错误格式、显式越界子资源或不匹配配对分辨是否在委托前拒绝，核对 backend Device/owner 检查和失败输出。 [公共预检规范](specs/rhi-common-validation/spec.md)

## 2. Buffer 与 Texture staging

- [x] 2.1 实现专用 Read/Write Buffer、Texture staging backing 和逐 mip/layer 的可检查 footprint。校验奇宽 `R8_UNORM` 真实行间距与 `BC5_UNORM` 尾块，核对字节超预算在分配前拒绝、对象限额在释放并完成原生删除后恢复。 [布局与预算设计](design.md#staging-layout)
- [x] 2.2 接通 `CopyBuffer`、`MapBuffer`/Unmap 及 staging lease 进入原录制、接受和退休。以非零偏移双向复制判定字节和哨兵。先尝试映射未提交的新使用，再 Poll 捕获的有效提交并读取新数据。观察错误方向、已映射冲突或未完成时无相应原生操作。
- [x] 2.3 接通 `CopyTexture` 两个 staging 重载及完整选层 Map/Unmap，统一 ContextState 的 pack/unpack。原生回放用奇宽且非对称的 mip/layer 区域验证行距、顺序与相邻哨兵，核对压缩块往返，以及恢复普通上传时的紧密布局。用 pitch sentinel 判定失败未改输出。 [传输与映射规范](specs/opengl-immediate-transfer/spec.md)
- [x] 2.4 验证真正的生命周期缺口。使用同一回放观察请求取消或包放弃、最后句柄释放、后续退休和新写入，确认失效的复制不可映射。复用既有 Drain 检查，加入在途 staging 以区分正常退出、真实外部 owner 和确认 context 不可用后的 CPU 收拢。

## 3. Typed clear、mip 与基础 resolve

- [x] 3.1 交付整 Buffer UInt clear 和候选 MS 颜色的完整选层 clear，核对既有 Float/UInt 颜色、深度/模板和普通 copy 的实际规则。以非零 UInt 重复字节、受限 scissor 下的全选层和未选层哨兵作为独立预期。普通二维/cube copy 和 depth/stencil 回归优先复用匹配用例。 [clear/copy 规范](specs/opengl-immediate-draw/spec.md)
- [x] 3.2 交付逐层颜色 mip 生成与 Automatic/Explicit 入口。选择非零 base mip 的 cube 面，用可计算的线性/编码颜色数值检验过滤、未选层和采样 LOD。对照一个超范围或未交付用途，确认目标未修改。 [FBO 传输设计](design.md#framebuffer-transfer)
- [x] 3.3 交付等尺寸且同格式的颜色 resolve。用公开 MS clear、非方形原生 resolve 和正式 staging 验证实际层/mip 配对、已知颜色、行序及未选择内容。记录真实源样本集合。用不支持的样本或配对对照确认拒绝。每类独立目标路径有代表性结果后停止。 [resolve 规范](specs/opengl-immediate-transfer/spec.md#requirement-resolve-matching-multisample-color-subresources)

## 4. 完成观察与计时轮转

- [x] 4.1 保留 Event 最后接受提交快照，建立 CPU 对象预算并验证饱和、Reset、最后 Release 与已捕获工作的独立关系。通过提交 A、设置 Event、再提交 B 的回放核对范围。沿用既有零快照合同。
- [x] 4.2 使 Timer 在正常接受其 recording 后才解析双 available，保持原生名预算、秒单位、有界冷路径及实际使用身份。用同一 Timer 经未接受、合法接受/消费、轮转和放弃回放，核对结果身份与只在 available 后读取结果。覆盖未就绪时的非阻塞返回及已解析旧包晚退休，复用已登记且匹配的预算/错误证据。 [查询规范](specs/opengl-completion-queries/spec.md)

## 5. 实际消费链与本地验收

- [x] 5.1 用 D08-T04 可复用的直接 Graphics/Compute 序列验证同版 image 在采样后再被复制，SSBO 非零范围经 Buffer 传输，再发生重叠后继 Shader 写。观察读回数值、未覆写区间、实际 barrier 域及顺序。复核 Automatic/Explicit 的等价结果、缺少必要 Explicit 状态的拒绝和交错 transfer 后的原生状态。
- [x] 5.2 验证 GraphManaged 的存活 CopySource Texture 复制到外部 staging，及图尾 Readback Buffer 由调用方接续复制。覆盖裁剪、超授权失败和图返回后恢复 Automatic。以接受真实复制提交之后捕获 Event、Poll 再 Map 的一条链验证像素/字节、范围和次序。 [图读回规范](specs/render-graph-execution/spec.md)
- [x] 5.3 构建正式 Debug/Release 并运行隐藏启动。验证既有 Runtime 计时消费沿用原提交身份和正常非阻塞轮转，回放受影响的原生绘制/上传消费者。按构建输入复用证据，用两配置的代表性拒绝证明 Release 行为。更新 `RHI/README.md` 和受影响的接口说明，核对用法、实际能力与原生证据一致。 [构建说明](../../../../Docs/Build.md) · [能力记录](../../../../RHI/README.md)

## 6. 独立审查与整改

- [x] 6.1 Agent A 登记本轮源码、相关 diff、新增文件、构建/回放输入、原生结果及限制，更新当前工作包状态，并使用唯一问题单交接。以已登记证据覆盖第 1–5 组作为交接条件。 [归档审查记录](#review-closeout)
- [x] 6.2 Agent B 独立核对交接实际内容、资源与队列合同、可重放验证及直接消费者，完成本轮第一方文件的职责、接口与流程注释，登记稳定 CR 和覆盖结论。 [首轮结果](#b-first-review)
- [x] 6.3 Agent A 按 CR 修正并验证受影响的特定行为，保留 B 的有效注释，登记证据及自检。无需要修正事项时按 B 的结论记录适用性。 [首轮整改](#a-first-correction) · [CR-005 整改](#a-second-correction)
- [x] 6.4 Agent B 对接受版本复核逐项修正、真实证据和完整注释，以问题单记录 CR 结论与签收。任何需要补验的验收合同继续走原编号。 [独立审查闭环](../../../../AGENTS.md#review-loop) · [首次整改复核](#b-first-recheck) · [CR-005 复核与最终签收](#b-second-recheck)

## 7. 最终验证、同步与归档

- [x] 7.1 B 签收后逐项核对最终实现与 spec/design，确认真实 native 证据对应当前源码、构建输入、包、库、可执行文件和设备。复用匹配证据，补齐变化和验收缺口，执行 strict 校验并保留记录。 [最终验证](#final-verification)
- [x] 7.2 比对完整 delta 及现有主规范，按锁定 OpenSpec 流程同步。核验新增/更新要求、保留的场景与主规范严格校验结果。 [同步记录](#spec-sync)
- [x] 7.3 用锁定 CLI 归档，记录开发计划的包内完成项、归档与下一工作包引用。核对归档链接、实际状态和保留的 review/验证证据，归档成功后依据空模板重置当前问题单。 [归档收尾](#archive-closeout)

## 验证范围与停止条件

先复用 D08-T02 的提交/上传与 D08-T04 的 Compute/Draw 有效输入。只对本次新增的布局、实际传输能力、结果 gate、取消和查询时序补代表性原生用例。相同 fixture 可组合上述断言，让字节、非对称图案、已知颜色、实际原生命令和诊断互相校验。Pending 先用未接受 recording 和真实零等待观察证明。只有缺少必要的 Pending 或在途失败证据时才对该缺口安排定向检查，并区分模拟和实机结果。 [D08-T02 基线](../2026-11-20-complete-versioned-gpu-residency/tasks.md#final-verification) · [D08-T04 基线](../2026-11-21-enable-opengl-compute-synchronization/tasks.md#final-verification)

本地开发材料集中在 `build/d08-t05/`，使用正式 DeviceHost、Validation、RHI 库与实际驱动。证据记录命令、源码/构建/fixture 身份、实际样本集合、GPU/驱动、观察结果和未覆盖范围。现有检查覆盖同一风险且输入匹配时直接引用，所需环境或证据无法取得时保留任务待验收。各改变的合同都有可区分错误结果的证据时停止。 [验证范围](../../../../.agents/skills/physara-verification/SKILL.md)

<a id="a-local-verification"></a>
## Agent A 本地验证与交接

| 已运行检查 | 观察结果与证据 |
|---|---|
| 1.1–3.3 传输与完成 | Debug/Release 通过 Buffer 非零范围、UInt clear、packed depth/stencil 独立 aspect、R8 奇宽 row pitch、数组非零 mip、BC5 7×5 尾块和普通 unpack 恢复。sRGB cube 选面保留未选面，实发 4 次 blit，非方形数组颜色与 HDR resolve 读回一致。设备返回 RGBA8 array 与 RGBA16F 2D resolve 源掩码 `0x114`，正用例使用 2 样本，另核对 sRGB MS 可分配而 resolve 不支持。两配置均观察了非法范围、方向、缺少 Explicit 用途以及未接受、未完成、放弃的 staging 不可提前 Map。 [Release](../../../../build/d08-t05/TransferChecks-Release.log) · [Debug](../../../../build/d08-t05/TransferChecks-Debug.log) |
| 2.4 取消与退出 | 测到丢弃录制后旧使用无法发布，后续新复制能恢复映射，真正外部 owner 阻止关闭。正常带在途 staging 的 Host 退出通过。单独调用受控 ContextUnavailable 进入退出，观察到该分支无 GL 等待、删除或 Finish。 [退出回放](../../../../build/d08-t05/ContextUnavailable-Release.log) |
| 4.1–4.2 Event/Timer | 两配置验证 Event A 只退休捕获前缀、256 个 CPU 观察者、512 个原生名字的 Timer 额度及安全回收，Timer 区间提交前、放弃后不会发表结果。受控 unavailable 对照无原生结果读取，后续真实 available 返回秒数，两次使用分别读取一对 timestamp。正常回放没有非零超时 GL 等待或全设备 Finish。 [查询回放](../../../../build/d08-t05/TransferChecks-Release.log) |
| 5.1 Draw/Compute 消费 | 复用 D08-T04 输入和 Shader 包，与本轮 Release 库重连。回放实际 15 次 Dispatch、22 次按域 barrier，验证采样后再复制同版 image、非零范围 SSBO 与覆盖前见证、后继重叠写、Explicit 与缺少授权对照。通过正式 staging 读回两个 Draw 结果及 image/Buffer，并用 Draw 采样实际生成 mip。 [混合命令回放](../../../../build/d08-t05/ComputeStage-Release.log) |
| 5.2 GraphManaged | 实测有根的 CopySource Texture 和图尾 Buffer 读回，裁剪分支没有运行，图退出后 Automatic 复制成功。越权层被拒绝，已发但图失败的复制即使前缀接受并完成也不可映射。 [图回放](../../../../build/d08-t05/GraphReadback-Release.log) |
| 5.3 构建及实际消费者 | 产品 Debug/Release 构建与 `--hidden --poll-limit=5` 均通过。用正式 Release Runtime 对象回放计时 Pending、真实 available 后的轮转及原提交身份。 [Debug 构建](../../../../build/d08-t05/build-Debug-final-bc5.log) · [Release 构建](../../../../build/d08-t05/build-Release-final-bc5.log) · [Debug 启动](../../../../build/d08-t05/smoke-Debug.log) · [Release 启动](../../../../build/d08-t05/smoke-Release.log) · [Runtime 计时](../../../../build/d08-t05/RuntimeTiming-Release.log) |

原生环境为 Windows x64/MSVC、Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129。BC5 对照定位了非零原生 mip 的尾块复制问题，正式路径按逻辑级别持有独立的实际尺寸 level 0 存储，两个配置都通过对应的逐块字节检查。ContextUnavailable 和无法取得 timestamp 的分支使用受控 fixture，真实设备丢失及其它驱动留待其对应的平台验收。增量计划中的颜色和样本仅声明所实现的能力边界。 [BC5 形状对照](../../../../build/d08-t05/BC5Shape-Release.log) · [BC5 PBO 对照](../../../../build/d08-t05/BC5Diagnostic-Release.log)

构建中曾发生 MSVC `C1001` 和 Debug 中间 COFF 对象 `LNK1236`。保留失败日志，收拢已失败的编译进程并重建损坏的中间对象，正式 Debug/Release 使用原构建检查和配置完成。源码、差异、Shader 包、本地输入、库、可执行文件及实际运行日志均登记在交接清单。 [失败记录与产物身份](../../../../build/d08-t05/handoff.json) · [独立审查问题单](#review-closeout)

<a id="b-first-review"></a>
## Agent B 首轮审查

2026-11-22 首轮结论为退回整改。B 核对了基线 `7244376e509b66290904d7ab976c37ade13ccfcd` 和 33 项 RHI 工作树输入的 SHA-256。CR-001 为图退出失败后仍发布读回，CR-002 为 PBO 报损后其他层仍能使用，均为 P1。CR-003 为 raw 接受非法 BC5 staging 描述，CR-004 为 Validation 遗漏传输预检，均为 P2。6.3 由 A 整改，6.4 待 B 对整改版本复核。 [问题、位置与复验要求](#review-closeout)

B 补齐了 30 个 C++ 文件和通用层 CMake 的注释。注释后 33 项输入的路径及哈希汇总为 `83ebcbc7958934eb702e7e64c085bd1607bd2d47d228fc4d25a74ad162877612`。在本机 Intel OpenGL 4.6 上，Debug 和 Release 定向回放均复现全部四项，Release 既有传输/查询与图读回回放通过。图退出回放临时解绑并恢复 context，PBO 损坏分支通过受控 Unmap 返回值触发。 [输入、程序与日志身份](../../../../build/d08-t05/b-initial-review/evidence.json) · [Debug](../../../../build/d08-t05/b-initial-review/TransferEdges-Debug.log) · [Release](../../../../build/d08-t05/b-initial-review/TransferEdges-Release.log)

<a id="a-first-correction"></a>
## Agent A 首轮整改

整改前核对 33 项 RHI 输入与 Agent B 注释后的版本逐项一致，B 登记的 11 项复现产物也与清单一致。修订保留同一基线和原 CR 编号，修正后的输入指纹为 `bca0f4fea297963ae0ccee07f54377a9950774f5c58817e0004764feb1b566fe`。 [收到的版本](../../../../build/d08-t05/a-correction-1/reviewed-input.json) · [整改交接清单](../../../../build/d08-t05/a-correction-1/handoff.json)

| 问题 | 已验证的结果 |
|---|---|
| [CR-001](#cr-001) | 图退出失败后，恢复 context 并完成前缀提交，仍不能 Map 这次图读回，pitch 保持 99。成功对照仍读到 415、pitch 8，回调主动失败的复制也不发布。 |
| [CR-002](#cr-002) | 受控 Unmap 报损使同一 PBO 的其他 Read 层返回空，pitch 保持 99，其他 Write 层原生上传数为 0。补测成功重写及完成复制只恢复指定层。 |
| [CR-003](#cr-003) | raw 与 Validation 都在分配前拒绝 BC5 渲染目标 staging 描述。普通颜色附件 staging 和完整 BC5 mip 往返通过。 |
| [CR-004](#cr-004) | 整数 mip 生成、仅 UAV 的颜色 clear、部分 BC5 staging 复制均在 Validation 拒绝。合法颜色 mip、clear 和块传输通过实际回放。 |

使用 B 的原始 fixture 对整改库重新链接，Debug 和 Release 四项均通过。Release 补跑读回恢复、成功图、回调失败、越权和既有传输回放。正式产品 Debug/Release 构建和隐藏启动通过。原生环境为本机 Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129，PBO 报损采用受控返回值。 [Debug 复现](../../../../build/d08-t05/a-correction-1/TransferEdges-Debug.log) · [Release 复现](../../../../build/d08-t05/a-correction-1/TransferEdges-Release.log) · [恢复对照](../../../../build/d08-t05/a-correction-1/RecoveryControls-Release.log) · [图回归](../../../../build/d08-t05/a-correction-1/GraphReadback-Release.log) · [传输回归](../../../../build/d08-t05/a-correction-1/TransferChecks-Release.log)

<a id="b-first-recheck"></a>
## Agent B 首次整改复核

2026-11-22，B 核对基线、实际 diff、33 项 RHI 输入和 A 交接清单登记的文件哈希。10 项整改文件与首轮 B 注释版的差异集合吻合。B 在本机 Intel OpenGL 4.6 独立运行原定向检查的 Debug、Release，以及 Release 的恢复、图读回和传输/查询对照。五次均以 0 退出，输出与 A 记录一致。图退出失败后的 Map、PBO 整体报损及单层恢复、BC5 描述和原三项包装拒绝均符合预期，CR-001 至 CR-004 已由 B 逐项关闭。 [独立回放](../../../../build/d08-t05/b-recheck-1/runs.json) · [归档审查记录](#review-closeout)

新增 CR-005（P2）待 A 整改。普通纹理间复制的两个非法请求仍到 raw 层才拒绝，包括同一 mip/layer 上的重叠矩形和两个普通 BC5 Texture 之间的整级复制。Debug 和 Release 均观察到非法请求的原生复制数为 0，合法相邻区间复制一次并读回 `17,29,17,29`。 [CR-005 位置与整改要求](#cr-005) · [Debug 记录](../../../../build/d08-t05/b-recheck-1/OrdinaryCopy-Debug.log) · [Release 记录](../../../../build/d08-t05/b-recheck-1/OrdinaryCopy-Release.log)

B 在四个头文件补充了图退出、整份 PBO 报损和块范围预检的注释。本次回放对应 A 指纹 `bca0f4fea297963ae0ccee07f54377a9950774f5c58817e0004764feb1b566fe`，交回 A 的注释版指纹为 `7d2f150b2a6ae1dfa4710c646fdbfb49eba5d392b8c5cfd1809080d764beadcd`。6.4 待 CR-005 完成整改并复核后签收。 [逐文件身份、检查程序与日志](../../../../build/d08-t05/b-recheck-1/evidence.json)

<a id="a-second-correction"></a>
## Agent A 的 CR-005 整改

整改前匹配 B 注释后的 33 项 RHI 输入和登记的 19 项复现产物。修改集中在普通纹理复制的公共格式与范围准入，整改版指纹为 `c9086242eee1605809e6ef2903e82bd6b63cf5f03bf36d3ba52c5c1676f4129e`。 [收到的版本](../../../../build/d08-t05/a-correction-2/reviewed-input.json) · [整改交接清单](../../../../build/d08-t05/a-correction-2/handoff.json)

用 B 的原始 fixture 对整改库重链，Debug 和 Release 均确认 [CR-005](#cr-005) 的两种请求由 Validation 拒绝，各发出 0 次原生复制。相邻半开区间实发一次复制，按 16 字节行距读回 `17,29,17,29`。Release 对照确认 raw 自身拒绝重叠及普通 BC5 复制，另一个 mip 或数组层上的相同矩形正常复制，BC5 整级 staging 和前四项整改回归通过。 [Debug](../../../../build/d08-t05/a-correction-2/OrdinaryCopy-Debug.log) · [Release](../../../../build/d08-t05/a-correction-2/OrdinaryCopy-Release.log) · [原生对照](../../../../build/d08-t05/a-correction-2/OrdinaryCopyControls-Release.log) · [传输回放](../../../../build/d08-t05/a-correction-2/TransferChecks-Release.log) · [前四项回归](../../../../build/d08-t05/a-correction-2/TransferEdges-Release.log)

Debug/Release 正式产品构建及 `--hidden --poll-limit=5` 均通过，原生复验限于本机 Intel RaptorLake-S Mobile / OpenGL 4.6.0 Build 32.0.101.6129。CR-005 和 6.4 待 B 对本版本独立复核。 [构建和实际产物](../../../../build/d08-t05/a-correction-2/handoff.json)

<a id="b-second-recheck"></a>
## Agent B 的 CR-005 复核与签收

2026-11-22，B 核对基线 `7244376e509b66290904d7ab976c37ade13ccfcd`、33 项 RHI 输入、工作树 diff 和本轮登记的库、程序、复现输入及日志。相对上一 B 注释版，A 修改了 6 个文件。普通重载在委托前区分端点身份，Validation 与 raw 使用相同的未压缩格式和半开矩形规则。staging 继续按自身的块格式规则校验。 [CR-005 与逐项复核](#cr-005)

用 B 的原始复现源码分别重链当前 Debug、Release 库，两种非法复制均由 Validation 诊断，原生复制数为 0。合法相邻区间复制一次，pitch 为 16，读回 `17,29,17,29`。Release 对照确认 raw 独立拒绝、跨 mip 与跨层复制、BC5 完整 mip 往返和前四项回归均通过。另用当前 Release 库重链固定输入的 Compute/Draw 混合回放，观察到 15 次 Dispatch 和 22 次原生屏障，数据读回通过。共 6 次回放均以 0 退出。原生环境为本机 Intel OpenGL 4.6，前四项中的 PBO 报损为受控返回值。 [定向回放](../../../../build/d08-t05/b-recheck-2/runs.json) · [混合回放](../../../../build/d08-t05/b-recheck-2/ComputeStage-Release.log)

B 确认 CR-001 至 CR-005 均已关闭，代码和注释通过独立验收。实测的 A 整改版指纹为 `c9086242eee1605809e6ef2903e82bd6b63cf5f03bf36d3ba52c5c1676f4129e`。B 在 Common 传输和 Validation 接口两个头文件补充注释后签收，33 项输入指纹为 `6e14c43afb71b626da51a6c3754534fc25119efa017f1bdc72bbedad7e50f3ba`。7.1 至 7.3 待 A 执行。 [签收版本与证据](../../../../build/d08-t05/b-recheck-2/evidence.json)

<a id="final-verification"></a>
## 最终验证

核对 33 项签收源码与基线 `7244376e509b66290904d7ab976c37ade13ccfcd`，签收指纹为 `6e14c43afb71b626da51a6c3754534fc25119efa017f1bdc72bbedad7e50f3ba`。用已登记哈希逐字节还原实测版本的两个头文件，确认与签收版的差异仅为 Doxygen 注释，核对有效行及预处理续行后复用匹配的 Debug/Release 构建、库和独立回放。复查 146 项已登记输入与产物，保留五项 CR 的签收结论及注释覆盖。 [签收记录](#b-second-recheck) · [源码和签收证据](../../../../build/d08-t05/final/signed-source.json)

| 验证项 | 合同与证据 |
|---|---|
| 完整性 | 实现与独立审查 1.1–6.4 已完成。已核对六份增量规范中的 15 项要求与 51 个场景，按下列真实消费者与拒绝路径核对变化。 [OpenSpec 严格校验](../../../../build/d08-t05/final/validate-change.json) |
| 资源与传输 | 按实际 target/用途/样本交集发布能力，Validation 与 raw 按已解析范围拒绝非法复制，typed clear/copy 与 sRGB mip/同格式 resolve 保持邻区和编码。已复用 Debug/Release 的 CR-005 复现及正式 staging 字节验证。 [能力](../../../../RHI/OpenGL/Formats.cpp#L297) · [区间校验](../../../../RHI/Validation/ValidationDevice.cpp#L262) · [复制和附件传输](../../../../RHI/OpenGL/CommandListTransfer.cpp#L343) · [传输/查询回放](../../../../build/d08-t05/b-recheck-2/TransferChecks-Release.log) |
| 布局与生命周期 | 按实际 mip/layer footprint 和 pack/unpack 搬运，BC5 完整块行按实际 GL 存储读取。Map 关联原已接受并完成的提交，作废图结果和 PBO 报损清理受影响内容，预算只随实际退休归还。复用 CR-001 至 CR-004 独立检查，另运行带在途 staging 的受控 context 不可用路径。 [staging](../../../../RHI/OpenGL/Staging.cpp#L79) · [退休](../../../../RHI/OpenGL/Submission.cpp#L129) · [前四项回放](../../../../build/d08-t05/b-recheck-2/TransferEdges-Release.log) · [受控退出](../../../../build/d08-t05/final/ContextUnavailable-Release.log) |
| 同步与消费者 | 复用当前 Release 库的 Compute/Draw 混合回放，15 次 Dispatch、22 次实际屏障。重链补验图 CopySource/读回根、图尾 Buffer 复制、失败前缀及越权拒绝。Runtime 回放保留原提交身份并只在 available 后读取 Timer。 [混合消费](../../../../build/d08-t05/b-recheck-2/ComputeStage-Release.log) · [图读回](../../../../build/d08-t05/final/GraphReadback-Release.log) · [Runtime Timer](../../../../build/d08-t05/final/RuntimeTiming-Release.log) |
| 产物与启动 | 按生成的 Runtime 资源路径记录全部 20 个 Debug/Release 产品 Shader 包，核对构建日志中的内容标识、生成接口 ABI 和正式 ShaderBuild 读取结果。补跑两配置的 `--hidden --poll-limit=5`，运行前后源码、库及包身份一致。 [产品包身份](../../../../build/d08-t05/final/product-packages.json) · [Debug](../../../../build/d08-t05/final/smoke-Debug.log) · [Release](../../../../build/d08-t05/final/smoke-Release.log) |

设计核对覆盖统一即时列表、单一访问发射、分域同步、staging 与查询预算、owner 线程和图执行边界。原生回放环境为 Windows x64 / Intel RaptorLake-S Mobile / OpenGL 4.6.0 Build 32.0.101.6129。PBO 报损、计时未就绪及 context 不可用分支含定向控制，真实驱动丢失和跨驱动表现未纳入本次原生范围。 [本轮最终验证凭据](../../../../build/d08-t05/final/verification-evidence.json)

<a id="spec-sync"></a>
## 主规范同步

比对六份增量规范与同步前基线。新增 `opengl-immediate-transfer` 主规范，保留原有规范的目的、条款顺序和未受影响要求。合并 8 项新增和 7 项修订要求，保留全部原有场景并同步指定内容。逐份复核六份主规范与合并结果一致，OpenSpec 1.13.2 对全量 38 份主规范执行严格校验，全部通过。 [逐项同步凭据](../../../../build/d08-t05/final/spec-sync.json) · [全量校验](../../../../build/d08-t05/final/validate-specs.json)

<a id="review-closeout"></a>
## 审查闭环留存

Agent B 对最终签收版本确认五项问题通过，核对第一方职责、接口合同、实现流程和受影响消费者的注释。下表保留稳定编号及独立复核证据。 [首次整改复核](#b-first-recheck) · [最终签收](#b-second-recheck) · [签收问题单原件](../../../../build/d08-t05/final/review-signed.md)

| 编号 | 修正及复核结论 |
|---|---|
| <a id="cr-001"></a>CR-001 | 按图执行与录制归还的实际成功状态共同决定读回发布，失败前缀仍安全退休。 [图回归](../../../../build/d08-t05/b-recheck-1/GraphReadback-Release.log) · [退出失败对照](../../../../build/d08-t05/b-recheck-2/TransferEdges-Release.log) |
| <a id="cr-002"></a>CR-002 | PBO 报损时使 backing 的旧层内容全部失效，重新定义的层独立恢复。 [注入与边界](../../../../build/d08-t05/b-recheck-2/TransferEdges-Release.log) · [恢复对照](../../../../build/d08-t05/b-recheck-1/RecoveryControls-Release.log) |
| <a id="cr-003"></a>CR-003 | raw 和 Validation 使用相同规则拒绝与块格式不兼容的 staging 描述。 [复验](../../../../build/d08-t05/b-recheck-2/TransferEdges-Release.log) |
| <a id="cr-004"></a>CR-004 | Validation 提前拒绝非法 mip 格式、typed clear 用途和不完整块传输。 [复验](../../../../build/d08-t05/b-recheck-2/TransferEdges-Release.log) |
| <a id="cr-005"></a>CR-005 | Validation 与 raw 共用普通复制的格式和同子资源重叠判定，相邻、跨 mip、跨层和合法 BC5 staging 通过。 [复验](../../../../build/d08-t05/b-recheck-2/OrdinaryCopy-Release.log) · [跨子资源对照](../../../../build/d08-t05/b-recheck-2/OrdinaryCopyControls-Release.log) |

<a id="archive-closeout"></a>
## 归档收尾

OpenSpec 1.13.2 已在六份主规范同步后归档本 change，21 项任务全部完成。D08-T05 已验收并归档，开发计划指向 D08-T06。B 签收原件已留存，当前问题单已按模板复原，归档工件和开发计划的本地引用已复核。 [归档回执](../../../../build/d08-t05/final/archive.json) · [开发计划](../../../../Docs/Developments/Developments.md#current-stage) · [B 签收原件](../../../../build/d08-t05/final/review-signed.md)
