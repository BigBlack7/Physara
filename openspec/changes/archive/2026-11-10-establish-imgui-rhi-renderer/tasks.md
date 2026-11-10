# Tasks

## 1. A 接通正式 Shader 与会话入口

- [x] 1.1 在 `Engine/Shader/Interface` 和 `Shaders/UI` 建立 UI 声明与 `GraphicsBaselineV1` 程序。让 `PhysaraEditor` 依赖生成头与唯一包 target，沿 `LoadRhiProgram` 校验生成的 `AbiHash`。在 `build/d04-t02/` 下配置正式工程，构建消费目标并核对包来源、生成顺序和布局。用一份实际不匹配的包验证拒绝和逆序清理，按真实默认构建行为更新构建说明。 [构建入口](../../../../Docs/Build.md#shader-构建)
- [x] 1.2 从实际 `EditorSession` 接入可选 RHI renderer。在首个 `NewFrame` 前设置固定版本的 renderer 能力、reset 回调和 atlas 限制，保留 CPU-only 会话的字体准备。核对两条路径均能封存第一帧，renderer 初始化失败能结束会话并释放已取得的资源，公开接口及 Engine 依赖中没有 ImGui 或 GLFW 细节。

## 2. A 准备和录制离屏 UI 几何

- [x] 2.1 实现从封存帧到单次消费准备值的接口，明确调用者拥有初始化过的目标、开放列表及提交。核对无新帧、零像素范围、非零不兼容目标和超预算几何的结果，前两项无 GPU 工作，后两项有明确拒绝原因。用代表性边界输入核对尺寸和偏移检查早于复制或录制。
- [x] 2.2 实现有界复用 VB/IB/CB、逻辑投影、目标像素裁剪以及跨列表索引和 base vertex。用公共 RHI 画同一非对称图，取 `DisplayPos=(11,7)`、`DisplaySize=(32,24)`、`FramebufferScale=(2,1.5)` 与 `64×36` 目标，覆盖带小数边界的越界裁剪、空裁剪、多个列表和非零局部偏移。比较独立计算的内外像素及干扰区域。将 ImGui、Editor 和本地图案一起按 32 位索引重编一次，复跑同一图案核对 R16/R32，避免跨索引 ABI 链接。
- [x] 2.3 在普通纹理解析之前处理 reset-state，使用公共 `ClearState` 及下一条 draw 的完整重绑。向同一图案插入无可采样 token 的 reset，核对前后 draw 的像素及 reset 本身没有纹理诊断或绘制。用一项任意回调核对准备期拒绝、回调未执行且没有 UI 命令录制。

## 3. A 实现纹理发布与真实退休

- [x] 3.1 建立会话内有界的 `UiTextureId` 注册及采样绑定，复用驻留资源和诊断占位。核对驻留图标能在实际帧绑定，槽复用后旧 token 只能绘制占位，新代显示自己的颜色。同一宿主重接时用旧 token 复查代隔离。
- [x] 3.2 消费固定 ImGui 的 `WantCreate` 和 `WantUpdates`。首次创建复制整 mip 并安装本次临时 token，更新保持有效代，以暂存纹理走公共 `WriteTexture` 与同格式矩形 `CopyTexture`。以一个有源行距的非满幅区域核对准备值拥有字节，准备后改变测试源像素仍读到已复制的内容，区域外和有效身份保持，上传先于同帧采样。
- [x] 3.3 以同一录制/提交的结构化状态确认上传，失败保留请求和源数据。先取消一次未录制的创建，核对临时 token 失效且请求可重试。再在一次真实更新批的诊断作用域内触发一项公共 RHI 参数拒绝，核对已发前缀取得正票据时请求仍未确认。让下一有效帧按最新像素成功重试，比较状态、有效代、更新区和区外像素。诊断核对严重度及记录完整性。
- [x] 3.4 按 `WantDestroy` 结束新解析并解除 CPU 持有，保留 `WantDestroyNextFrame` 对当前帧的正常解析。沿先前 draw 的真实提交与 Event 完成核对在途引用，完成后核对可退休。序列包含 atlas 重建或删除、后续新代绘制和带最后在途提交的受控退出，以实际完成及释放结果判定。

## 4. A 验收线性颜色与正式消费链

- [x] 4.1 在线性域完成顶点色、颜色纹理和字体覆盖率合成。在第 2 组的原生图案补入着色图标、已为线性的纹理与实际字形边缘。以公共 RHI 写入 sRGB 底色字节 `(32,80,144,96)`，固定顶点色 `(128,64,192,128)` 和图标 texel `(192,128,96,128)`，按独立线性混合得到存储 RGBA `(56,71,130,136)`，允许每通道 2 个 UNORM 单位。字形边缘以 CPU atlas 的部分覆盖率计算预期，并核对透明处及未覆盖底色。
- [x] 4.2 完成正式 Debug/Release 构建和受影响的原生整合检查。用 `--help` 和 `--hidden --poll-limit=3` 核对产品入口的 CPU 帧回归，离屏检查消费同版本 `PhysaraEditor`、正式 UI 包及真实 OpenGL Device。读回顺序为公共录制、非零 Execute、设置 Event、有限等待并 Poll 确认实际完成，再使用 `build/d04-t02/` 的冷路径观察 helper。记录命令、配置、输入与可执行文件身份、原生设备、预期和结果。32 位配置只复用几何验收所需图案。 [原生读回合同](../2026-11-08-establish-opengl-native-draw-gate/design.md)

## 5. 独立 A/B 审查与注释闭环

- [x] 5.1 A 达到本地检查停点后，记录真实 commit、diff、未跟踪输入以及本地检查材料和结果，把完整受影响文件交给维护者调度独立 B。 [当前问题单](../../../../Docs/CodeReview.md)
- [x] 5.2 B 独立核对几何单位、Shader 和颜色、纹理所有权、上传确认及退休边界，补齐本轮完整第一方文件的职责、接口合同和关键流程注释，在问题单记录覆盖及问题。
- [x] 5.3 A 按问题单整改，并以实际修正输入重新核对受影响的最小检查。由 A 在问题单记录对应修正及本地证据，无整改项时记录自检依据。
- [x] 5.4 B 复核修正版、原生证据和完整文件的注释，关闭或有依据地撤回问题，签署实际接受版本。 [B 最终复核](#b-final-review)

## 6. A 最终验证与收尾

- [x] 6.1 B 接受后执行最终 OpenSpec verify，将每个场景对应到实际实现和匹配版本的证据。输入变化、检查失败或发现具体缺口时补跑相应检查，记录实际运行、复用和未覆盖范围。 [最终验证](#final-verification)
- [x] 6.2 最终 verify 通过后同步 `editor-ui-rendering`，核对新增要求和全部场景完整，严格校验主规范与 change。 [主规范同步](#主规范同步)
- [x] 6.3 在本 change 留存 B 接受及最终验证证据，按原生流程归档。更新开发计划的完成状态和归档链接，核对归档入口、链接及 change 状态后重置当前问题单。 [当前阶段](../../../../Docs/Developments/Developments.md#current-stage)

<a id="a-local-evidence"></a>
## A 实现与本地验证

2026-11-10，基准 `5a5a4c031774ff0bbc3aa1810fb60df6afca53c1`。`EditorSession` 可接入 `EditorUi` 正式 Shader 包与宿主 Device，封存帧由准备值独立持有绘制和上传数据。准备值以同一列表的结构化执行结果及接受票据确认上传，下一次 `Advance` 前取消未确认的批次。驻留绑定随纹理身份解析，GPU 已发工作由 RHI 提交包保活。

实际环境为 Windows x64、MSVC 19.44.35228、SDK 10.0.26100.0、CMake 3.30.0-rc4 与 Intel RaptorLake-S Mobile OpenGL 4.6.0 Build 32.0.101.6129。首次配置受本机 SDK 目录访问限制，常规 Debug 构建出现多处 `C1001` 和 CL 进程异常退出。`product-z7` 的本地 Debug 生成使用 CMake Embedded 调试符号，保留原警告、优化和迭代器检查。Release 使用普通发布选项。初始与重试日志保存在 `build/d04-t02/` 及其 `attempts/` 下。

从仓库根执行 `& 'build/d04-t02/checks/RunEvidence.ps1'`。它记录输入摘要，以 VS 2022 x64 生成器构建两个正式 `Physara` 配置并链接同配置的 `UiChecks`，再以一致的 `ImDrawIdx` 定义局部重编 ImGui、Editor 和离屏检查程序。错误 ABI 包由同一 `GraphicsBaselineV1` 程序把纹理的逻辑 slot 从 0 改为 1 后生成。所有验收材料位于 `build/d04-t02/`。

| 任务 | 实际观察 |
|---|---|
| 1.1–1.2 | 正式生成顺序及配置路径正确，生成的 `AbiHash` 为 `9caa2ad94f920545bde098661e3a732b5ac2a66f6a5aadaf10bb1b898a78eb69`。同一宿主拒绝另一份合法 ABI 包后可接入 CPU 会话。原生会话在首帧准备 Alpha8 atlas，实际编辑器帧完成离屏合成。生成项目核实 ImGui、GLFW 与生成头只用于 Editor 私有实现。 |
| 2.1–2.3 | 暂停和零尺寸返回空结果，线性目标、超预算几何、越界索引及任意回调被拒绝。64×36 非对称图中验证非零逻辑原点、非等比缩放、小数裁剪、空裁剪、多列表、非零局部偏移及大于 65535 的顶点基址。R16 和独立 R32 重编的固定像素结果一致，reset 前后绘制正确且无纹理诊断。 |
| 3.1–3.4 | 取消未录制字体创建撤销临时 token。RGBA32 区域上传独立持有两行源字节，读回确认准备后改写源像素未影响已准备区域，区外保持。Debug/Release 都在真实 `WriteBuffer` 参数拒绝下取得票据 5，更新保持待处理，下一帧按最新内容确认。`WantDestroyNextFrame` 的当前帧仍可采样，逻辑销毁后先完成上传专用批，再读回前批结果。槽复用和同宿主重接分配不同代，旧 token 产生一次诊断占位。撤销驻留注册后，真实 draw 包仍持引用，Event 完成并回收后原生纹理删除。最后一批提交由宿主关闭收拢。 |
| 4.1–4.2 | 色标像素通过 `(56,71,130,136)` 的容差检查，线性纹理及未覆盖区符合独立预期。R16 的实际字形检查对照 22 个部分覆盖点和 90 个透明点。两配置的产品 `--help` 与 `--hidden --poll-limit=3` 均通过。两配置的原生全序列和 R32 图案模式均由非零提交的 Event 完成后读回，七次执行退出码均为 0。 |

`source-inputs.json` 记录构建前的 129 个输入，SHA-256 为 `f8af2e2be0a10e2af6b6ba6c7879ee3ff180de0fd3043ea367a79a952aef339a`。`evidence.json` 记录命令、可执行文件和包的摘要及日志，SHA-256 为 `c65f2919bf206e07244a126d49715be588348d9c84d48c4c0a6e69ae1548db8d`。运行前后核对实际输入及产物。`handoff.json` 和 `handoff-a.diff` 记录受影响源码与构建说明的未暂存改动和 9 份新增文件。 [当前问题单](../../../../Docs/CodeReview.md)

本地证据覆盖上述设备与离屏目标，R32 仅跑图案模式。窗口复制和交换由 D04-T03 验收，resize 与 FrameSlot 由 D04-T04 验收。本轮进入独立 B 审查与注释，B 复核后执行最终 verify。

<a id="a-correction-evidence"></a>
## A 整改与回归证据

基准 `5a5a4c031774ff0bbc3aa1810fb60df6afca53c1`。A 对照首轮交接的 129 个实际输入，收到 B 的 11 份注释变更，保存在 `build/d04-t02/a-correction/b-source/`。收到版本的清单 `b-inputs.json` SHA-256 为 `d214d35c14e41deed31b71b301fef736a011f547bd0e816713ba397d73a2ae98`。相对收到版本的修正限定在 6 份 Editor 源文件和 4 份 Platform 源文件，实际差异保存在 `a-corrections.diff`。

从仓库根执行 `& 'build/d04-t02/a-correction/checks/RunEvidence.ps1'`。沿用本机 MSVC 19.44.35228、SDK 10.0.26100.0、Debug Embedded 调试符号和真实 Intel RaptorLake-S Mobile OpenGL 4.6.0。两配置正式构建、`--hidden --poll-limit=3`、R16 原生全序列及一致重编的 R32 图案模式均通过。帮助参数沿用首轮证据。首轮容量 fixture 在进入 UI 录制前拒绝辅助 Buffer 写入，明确其初态后重跑，原失败保留于 `rejected-native-Debug-prefix-initial.log`。

| 审查项 | 整改和实测结果 |
|---|---|
| CR-001 | 宿主按同步 Error/Fatal 提供区间计数，录制在失败写入后停止。真实闭合上传包占住 64 MiB 映射容量，UI 暂存纹理与大 VB 写入分别报告容量拒绝。两配置均以票据 6 接受已发前缀，实际 Event 后读回确认两个绘制区域保留底色，另用原 token 核对驻留 texel 保持原值。`WantUpdates` 和矩形保持，后续安全重试更新指定区域且区外像素保持。 |
| CR-002 | 先发出有效 UI draw，再把同一可写目标作为第二条 draw 的采样源，实际 OpenGL 拒绝 feedback。两配置的 `Record` 均在状态设置失败处返回，票据 5 保活第一条 draw。Event 后读回第一条像素仍在，第二条区域保持底色。 |
| CR-003 | 空几何与上传项表使用可抛 count 构造，像素改由独占数组持有，准备中的资源名称直接可抛构造。MSVC Debug 在批次建立、已有临时创建身份后各触发一次实际迭代器代理分配失败。两次都返回有界错误并撤销本批临时身份，保持源像素和 `WantCreate`，随后正常准备、提交与读回通过。 |

整改目录的 `source-inputs.json` SHA-256 为 `156846a4e7d052e3e4d99c9b13b499da02fa5e464922706c7514abfdd7a534b2`。`evidence.json` SHA-256 为 `0c45949525062ce189348f1a254a1465e9726801d1d7870fb63008db50944d6d`，五次执行均匹配当前源码、构建和包并返回 0。`handoff.json` SHA-256 为 `50136dbc8538b9279e35667d3e6b5fbe14ed3dd4ffd203cfa3581de3ca79d385`。代理分配注入只在 Debug，R32 沿图案模式核对受影响录制入口。已提交给维护者调度 B 复核。 [当前问题单](../../../../Docs/CodeReview.md)

<a id="a-binding-correction-evidence"></a>
## A 再整改与采样绑定回归

B 复核通过 CR-001 和 CR-002，将 CR-003 的采样绑定构造缺口交回 A。基准仍为 `5a5a4c031774ff0bbc3aa1810fb60df6afca53c1`。重新核对 129 个实际输入，收到 B 的 10 份注释变更。`build/d04-t02/a-binding-correction/b-recheck-inputs.json` 的 SHA-256 为 `b319907c41900149acb8f3ccb46d4da39cd9c169b66f98f1c9e1f78364803db8`。相对收到版本的两处修正保存在 `a-binding-corrections.diff`。

- Editor 在 `UiTextures::Bind` 直接以可抛 count 构造初始化 `BindingSetDesc::bindings`。OpenGL 对 `OpenGLBindingSet` 的资源、绑定记录和视图节点三个向量作同样处理，分别沿既有准备和原生工厂边界返回。 [Editor 描述](../../../../Editor/ImGui/UiTextures.cpp#L81) · [OpenGL 对象](../../../../RHI/OpenGL/Bindings.cpp#L70)
- 从仓库根执行 `& 'build/d04-t02/a-binding-correction/checks/RunEvidence.ps1'`。沿用本机 MSVC 19.44.35228、SDK 10.0.26100.0、Debug Embedded 调试符号和 Intel OpenGL 4.6.0。Debug 使用同次构建的本地 PDB 按调用栈选择代理分配点。两次试验均在先前纹理已有临时 token 时命中，分别观察到 `UiTextures::Bind` 的 `vector<BindingSetItem>` 构造和 `OpenGLBindingSet` 的 `vector<ResourceHandle>` 构造。前者由 UI 准备返回有界存储错误，后者由 `CreateBindingSet` 记录一次分配错误并返回空句柄，由 UI 返回有界绑定错误。两次都撤销了本批临时身份，全部 `WantCreate` 请求和像素字节保持原值。
- Debug 重试和 Release 正常路径均接受真实票据 1，Event 完成后读回两张托管纹理的预期像素，宿主关闭通过。两配置正式构建与定向原生检查均通过。CR-001、CR-002 与 R32 图案沿用 B 已核对的先前证据。新故障注入限 MSVC Debug 的这两个调用栈。

再整改目录的 `source-inputs.json` SHA-256 为 `f110310b0e3830df49e0a7f63685257031be0fc6b860a777f5ac4fccb4b6b98e`。`evidence.json` SHA-256 为 `79679c05da19b4cf22bdcfc2852e5d7767b062af1d9c55d0d7cc36998ef915d8`，记录两次退出码 0 的运行、匹配的二进制、Shader 包、Debug PDB 和日志。`handoff.json` SHA-256 为 `9f97ecbdec6ea5c7b412ad8b910d97ccb8253cec9cb44b6a08cdd1590e7bbc37`。本版待 B 复核及注释。 [当前问题单](../../../../Docs/CodeReview.md)

<a id="b-final-review"></a>
## B 最终复核与签收

2026-11-10，Agent-B 签收 D04-T02。基准 commit 为 `5a5a4c031774ff0bbc3aa1810fb60df6afca53c1`，暂存区为空。签收版本采用 A 再整改 `source-inputs.json`（SHA-256 `f110310b0e3830df49e0a7f63685257031be0fc6b860a777f5ac4fccb4b6b98e`）中 123 项仓库产品输入。以下两项采用补注释后的 SHA-256。 [A 再整改证据](#a-binding-correction-evidence)

| 源文件 | B 签收 SHA-256 |
|---|---|
| `Editor/ImGui/UiTextures.cpp` | `7198746d5b493485bb340590a586e890c788d5b1785f114196ac3ed7a092311c` |
| `RHI/OpenGL/Bindings.cpp` | `b983790ff238c978e49273f999e4e73ddc670b3ae247f4b99928a11c1acbf0ad` |

| 问题 | B 复核结论 |
|---|---|
| CR-001 | 关闭。写入失败先于依赖的区域复制或绘制停止，匹配的两配置原生证据验证原驻留内容、裁剪区域和请求重试。 |
| CR-002 | 关闭。图形候选失败在索引绘制前返回，真实 feedback 检查及读回验证仅保留合法绘制前缀。 |
| CR-003 | 关闭。Editor 绑定描述及 OpenGL 绑定对象的三个容器均可沿既有错误边界返回。Debug 定点注入验证两层构造和临时身份回滚，Debug 重试与 Release 正常路径经票据 1、Event 完成及双纹理读回。 |

B 核对两次再整改运行的日志、源清单、可执行文件、Shader 包及 Debug PDB 的身份。代码复核与注释验收覆盖 Editor 几何和纹理、宿主错误区间、OpenGL 绑定构造、Shader 颜色及 CMake 生成关系。B 的两份源码改动通过注释前后代码和预处理指令对照。CR-001、CR-002 和 R32 图案复用先前已核验的原生证据，绑定构造成功时的资源内容及保活合同一致。定点注入限 MSVC Debug，B 本轮为源码与证据复核。任务 6.1 由 A 对签收输入执行最终 verify。 [当前问题单](../../../../Docs/CodeReview.md) · [A 初次整改证据](#a-correction-evidence)

<a id="final-verification"></a>
## 最终验证与归档

2026-11-10，按 OpenSpec 1.13.2 的 spec-driven 工作流核对 proposal、design、spec 和 tasks。15 项实施和独立审查任务均完成，CR-001 至 CR-003 均由 B 关闭。最终构建前后逐项核对 B 签收的 123 项仓库输入，包括已跟踪修改和未跟踪源码。签收版问题单保存在 `build/d04-t02/final-verification/CodeReview.accepted.md`，SHA-256 为 `6dc868d1b2982f40eaae50a859c0c3bb511768fd76acca3a1f25da26da04b6d6`。 [B 签收](#b-final-review)

从仓库根执行 `& 'build/d04-t02/final-verification/checks/RunEvidence.ps1'`。用 VS 2022 x64、MSVC 19.44.35228 和 SDK 10.0.26100.0 重建正式 Debug/Release 与对应的原生检查。Debug 使用 Embedded 调试符号，R32 图案将 ImGui、Editor 与检查程序按一致索引 ABI 编译。本机 GPU 为 Intel RaptorLake-S Mobile，OpenGL 4.6.0 Build 32.0.101.6129。两配置各执行一次产品入口 `--hidden --poll-limit=3` 和一次完整 R16 离屏序列，另执行 Debug 绑定构造注入及 R32 原生图案，六次退出码均为 0。`--help` 沿用 A 本地验证。代理分配注入只在 MSVC Debug 执行，R32 只覆盖图案，窗口呈现属于 D04-T03。 [入口与验证范围](#a-local-evidence)

验证目录的 `accepted-inputs.json` SHA-256 为 `a3ad9ae8b070d45fc1242a260f87affc2dcfcd23e7a71ee65729ba5f51411ffd`，`source-inputs.json` SHA-256 为 `11b69b9601a55dd5023d97dbebf912fce307ee55b72bd9790cd14d056df424c2`。`evidence.json` SHA-256 为 `8b8ce1c421b1bb224f00d7ce111f149d2fc8384e8adcd11b84ffa7cb1242b37c`，已独立复算实际输入、六次日志、运行的可执行文件、Shader 包及定点注入使用的 Debug PDB 摘要。

### 需求与场景核对

| 需求 | 场景、实现及本次证据 |
|---|---|
| 封存帧合成（5） | `EditorSession::PrepareUiFrame` 将封存帧交给 `UiRenderer::Prepare/Record`。`CheckSessionStart` 在两配置读回正式会话的离屏 UI，拒绝错误 ABI、目标格式与超预算几何，并验证零尺寸、暂停及临时身份撤回。Debug 在批次建立、已有临时 token、Editor 绑定描述与 OpenGL 绑定构造处验证准备失败及回滚。两配置 `CheckFeedbackRejection` 用实际 attachment feedback 拒绝第二条图形状态，读回第一条 draw 保留及第二处底色。 [会话入口](../../../../Editor/Session/EditorSession.cpp#L144) · [准备与录制](../../../../Editor/ImGui/UiRenderer.cpp#L392) · [绑定构造](../../../../RHI/OpenGL/Bindings.cpp#L69) |
| 坐标与索引（2） | `PrepareProjection`、像素裁剪与 `PrepareGeometry` 分别处理投影、裁剪和索引范围。`CheckGeometry` 用非零原点、非等比缩放、分数边界与空裁剪核对像素，并跨列表使用非零本地偏移与大于 65535 的顶点基址，R16 与 R32 均通过。 [几何准备](../../../../Editor/ImGui/UiRenderer.cpp#L112) |
| 回调分流（2） | 准备阶段只接受已注册 reset 回调。录制以 `ClearState` 结束该命令，下一条 draw 完整重绑。图案在两次有效 draw 之间使用无纹理的 reset，核对两侧实际像素和无纹理诊断。R16 两配置还核对任意回调在准备期被拒绝。 [回调准备](../../../../Editor/ImGui/UiRenderer.cpp#L189) · [图形录制](../../../../Editor/ImGui/UiRenderer.cpp#L445) |
| 线性域合成（2） | 正式 Shader 解码顶点 sRGB 并分开计算 RGB 和 alpha，`UiRenderer::Initialize` 建立独立混合因子。两种索引配置读回图标预期 `(56,71,130,136)`（各通道误差不超过 2）、线性纹理、基于 CPU atlas 计算的字形边缘及透明处底色。 [顶点 Shader](../../../../Shaders/UI/EditorUi.vert) · [片元 Shader](../../../../Shaders/UI/EditorUi.frag) · [Pipeline](../../../../Editor/ImGui/UiRenderer.cpp#L303) |
| 纹理上传确认（4） | `UiTextures::PrepareUpload` 独立拥有整 mip 或更新矩形，`UiRenderer::Record` 以宿主同步诊断约束后续命令，`PreparedUiFrame::Finish` 核对同一列表、成功区间和非零票据。`CheckTextures` 核对首次上传先于采样、接受后确认及更新字节脱离源。`CheckUploadRejection` 触发 64 MiB 容量下的暂存和 VB 拒绝，确认原驻留内容、底色和请求保留。暂停取消与真实 RHI 写入错误覆盖失败回执，正票据保护已发前缀，下一次更新成功。零票据和列表不匹配的拒绝分支经源码核对。 [上传准备](../../../../Editor/ImGui/UiTextures.cpp#L225) · [同步诊断](../../../../Platform/DeviceHost.cpp#L482) · [回执](../../../../Editor/ImGui/UiRenderer.cpp#L572) |
| 纹理解析与退休（2） | `UiTextures::Resolve/Destroy/Detach` 处理代和逻辑注销。`CheckTextures` 核对槽复用、重接后的旧 token 诊断占位、新代像素，以及上次提交后的逻辑销毁。`CheckGeometry` 在实际 Event 前检查驻留引用，在完成回收后检查原生纹理释放。原生序列通过受控关闭。 [纹理解析](../../../../Editor/ImGui/UiTextures.cpp#L304) · [退休合同](../../../../Docs/Architecture.md#retirement) |

6 条需求和 17 个场景均有实现和对应证据。实现符合 design 的五项决定。Session 按消费方式接入 renderer，正式生成链提供 Shader ABI，同一 UI 帧内以拥有型准备值录制并结束，稳定代与上传请求分开管理，回执确认与实际退休各有明确责任。最终 verify 通过。 [设计决定](design.md#decisions) · [变更规范](specs/editor-ui-rendering/spec.md)

### 主规范同步

按 `instructions specs --json` 同步唯一新增 capability `editor-ui-rendering`。主规范沿用 delta 的 Purpose，6 条需求和 17 个场景逐字核对一致。`openspec validate --specs --strict --json` 通过 14/14，含活动 change 的 `openspec validate --all --strict --json` 通过 15/15。 [主规范](../../../specs/editor-ui-rendering/spec.md)

### 归档结果

OpenSpec 1.13.2 执行 `openspec archive establish-imgui-rhi-renderer --yes --skip-specs --json`，退出 0，归档到 `openspec/changes/archive/2026-11-10-establish-imgui-rhi-renderer/`。`--skip-specs` 保留已同步且严格校验通过的主规范。执行归档时为 17/18，第 6.3 项包含归档后的核对和问题单重置。

移动后先核对五份归档文件及十四份主规范的字节摘要，再按归档层级调整链接。开发计划记录 D04-T02 完成，当前工作包推进至 D04-T03 待立项。`openspec list --json` 无活动 change，`openspec validate --all --strict --json` 为 14/14 valid，归档和开发计划的本地链接及锚点通过核对。 [开发计划](../../../../Docs/Developments/Developments.md#current-stage)

问题单随后按模板重置，SHA-256 与模板一致，为 `a26066c0106ba8d1991974974acf06ddc62ed5d24e2878a42eda8f6fbec847fc`。第 6.3 项在实际重置后勾选，最终 18/18。 [问题单模板](../../../../Docs/CodeReview.md.tpl)
