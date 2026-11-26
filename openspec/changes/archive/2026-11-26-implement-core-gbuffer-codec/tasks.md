# Tasks

## 1. A · 原生格式与 Shader 合同

- [x] 1.1 在开发计划当前阶段与 D11-T01 入口登记本 change 和实施状态，核对两处链接。给 OpenGL 接入 R11G11B10_FLOAT 与 R16_UINT 的二维目标映射和实际用途查询。用当前设备创建四颜色附件加 D32 的单样本 FBO，记录用途与组合结果，并按既有不兼容附件检查核对拒绝路径。
- [x] 1.2 在图形片元接口生成、ABI、IR 反射和包消费中接入无符号二维 fetch，更新接口声明说明。用匹配消费者加载正式构建链生成的包，以同 slot 的浮点接口验证拒绝；用旧浮点输入验证兼容性。
- [x] 1.3 将活动纹理的数值类别从原生反射带到绑定检查。使用现有 samplerless lowering 对 R16_UINT 读取 0x8105；在 Debug/Release 确认合法读值及两种 uint/float 错配的绘制前拒绝，保留与当前输入相符的结果。

## 2. A · 共享表面模型与正式几何包

- [x] 2.1 定义 G0–G3 输出与 flags 的权威接口，交付共享编解码和正式核心几何包。共用静态顶点、UV、覆盖、切线框架与有效实例行，将已有过滤步骤与可复用的波瓣派生衔接。构建受影响的核心、Forward/Forward+ 包，核对输出 location 0–3、标量类别和材质绑定。
- [x] 2.2 以独立输入核对颜色转换、AO、oct 正负半球与负 Z 极点、非零核过滤后的粗糙度、金属度、核心波瓣及预曝光自发光。用同组见证值测零、超白和各通道有限上界，记录格式预期、观察精度与误差。直接复用该组值作为图内回放的判据。 [字段与误差](design.md)

## 3. A · 图生产者和原生集成

- [x] 3.1 增加可由当前 PreparedView 和 Draw 工作集调用的 Renderer Pass，声明实际读依赖、四个附件及独立深度，交付匹配的图版本与有效矩形，显式初始化深度和资格。用有后继消费的图确认生产/读取顺序，核对现有普通 Forward 帧的资源需求。
- [x] 3.2 在 `build/d11-t01/` 组织最小原生回放，用正式生产入口和共享解码库取样，经现有可读回目标检查字段与 D32。用同网格不同实例覆盖、MASK 阈值两侧、双面镜像法线和核心范围外的 Unlit 验证表面归属；在有已知旧内容的存储上重绘以检查背景。记录非空见证像素、有效输入身份和第 2 组判据的实际结果。
- [x] 3.3 核对受影响的 Forward/Forward+ 代表样点和正式产品包的构建依赖。记录实际配置、设备/驱动、包与回放输入、命令、结果、复用及未覆盖项。四个规格中的风险各有可区分证据即可结束本地验证；只有输入变化、失败或明确缺口才扩大范围。
- [x] 3.4 A 在 [签收问题单](../../../../build/d11-t01/final/review-signed.md) 登记待审版本、改动文件、验收入口、本地结果和 B 审查档位建议及具体风险，核对交接能定位同一套源码与包。交接后由维护者调度 B。

## 4. B 审查与 A 整改

- [x] 4.1 B 独立审查规格、生成接口、共享 Shader、OpenGL 和图消费者，按项目约定核对并补齐所涉完整第一方文件的中文注释；在问题单登记实际档位、证据与结论。2026-11-26 首审交回 A，CR-001 至 CR-005 待整改。实际调度参数待维护者确认。[首审记录](../../../../build/d11-t01/final/review-signed.md)
- [x] 4.2 A 按问题单整改并自检，对实际受影响的风险重验；逐项登记修订输入、结果及交给 B 的版本。
- [x] 4.3 B 独立复核修订、注释及遗留项；以问题单中的 B 勾选、所验版本及签收结论为完成依据。CR-001 至 CR-005 全部复核通过。[B 签收记录](#b-recheck-1)

## 5. A · 最终验证与归档

- [x] 5.1 B 签收后，A 对最终输入执行原生 verify，核对规格、产物、签收版本与证据。将实际 A/B 档位、验收结论、运行/复用边界及最终验证记录留在本 change。
- [x] 5.2 A 在最终验证通过后同步增量规格并归档，核对主规格、计划状态、归档证据链接和 D11-T02 的前置，再按交接约定重置问题单。

<a id="a-local"></a>
## A 本地自检与交接 · 2026-11-26

基准为 `v2.0@5f7d9aba43fa7cd5883e26a7395fc0c93670d7ae` 加本次源码、着色包及工作树新增输入。A 建议 B 使用 Y 档（GPT-6.2 Astra，high）。B 的实际配置和签收由独立会话登记。 [签收问题单](../../../../build/d11-t01/final/review-signed.md) · [输入、产物与 SHA-256](../../../../build/d11-t01/handoff.json)

| 已运行 | 观察结果 |
|---|---|
| MSVC 19.44、Windows x64，`cmake --build build --config <Debug或Release> --target Physara` | 两个配置的产品、正式核心包及 Forward 系列包构建成功。核心片元 location 0–3 对应 float4、float4、float3、uint32，资格位常量为 1。两个配置的核心接口 ABI 均为 `8eac1e295e77c3bbf8bd330750e93120623dfda4480356d735b7e61c5701916b`。 |
| Intel RaptorLake-S Mobile，OpenGL 4.6.0，驱动 32.0.101.6129，Debug/Release 类型检查 | 四颜色附件加 D32 成功创建，异尺寸深度被拒绝。片元从 R16_UINT 取回 0x8105；同槽错误 ABI 以及浮点/无符号视图双向错接均被拒绝。D10 已保存的浮点包仍按原 ABI 成功读取。 [Debug 原生记录](../../../../build/d11-t01/Debug/native.log) · [Release 原生记录](../../../../build/d11-t01/Release/native.log) |
| Release，64×48，正式核心生产者与图内独立解码 Pass | 812 个有效像素。阈值下方/恰等于 cutoff 的 D32 分别为 0.68273091/0.72289157，Unlit 前景保留后方核心结果。镜像双面法线角差 0.01095°，负 Z 极点正确。过滤后 p 实测 0.22631836，独立预期 0.22638220。sRGB、AO、金属度、F0、漫反射和正常自发光通过字段判据。超白组的 R/G/B 为 65024、0.32421875、64512。复用同一原生 G3 纹理重绘空场景后，3072 个像素的深度和资格全部清零。 [字段结果](../../../../build/d11-t01/Release/field-results.json) · [独立判据](../../../../build/d11-t01/check.py) |
| Release，D10 冻结 32 灯输入，640×480，Physical Lighting | 普通两路均未请求核心附件。分别与 D10 对应路径的显示前 RGBA16F 比较，307200 个像素全部一致；四份图像的 SHA-256 均为 `060d20d8148e6047a374aa0167a699bb8103e949866e3a9f608c19c0dd8c68d7`。 [冻结输入说明](../../../../Docs/Performance/ForwardPlusReference.md#many-lights-fixture) |

复现时先构建当前产品，再从仓库根执行 `python build/d11-t01/inputs.py <配置>`，用 CMake 将 `build/d11-t01` 配置到 `build/d11-t01/obj` 并构建同配置的 Verify。分别执行 `build/d11-t01/obj/Debug/Verify.exe Debug`、`build/d11-t01/obj/Release/Verify.exe Release surface` 和 `python build/d11-t01/check.py Release`。回放与临时 Shader 仅存放于该工作包的 build 目录。比较复用 D10 的场景生成参数、旧包以及冻结的两张 HDR 参考图。

早期 Release 编译在不同编译单元出现 MSVC C1001 和编译器异常退出。保留了[失败日志](../../../../build/d11-t01/product-release.log)。限制本机构建子进程的处理器亲和性后，以工程原 Debug/Release 编译选项完成构建，尚未确定编译器异常根因。开发回放曾因合成资产与 D10 复用了同一版本身份导致读到旧材质，已为测试输入分配独立身份，并用冻结图重新验证。

本轮设备验证覆盖一台 OpenGL 实机。完整字段和 HDR 回放在 Release 运行。SurfaceAux、完整 DeferredLighting、三路径切换、成本测量和 B 独立验收各自保留在所属工作包或交接阶段。

<a id="a-correction-1"></a>
## A 首轮整改与自检 · 2026-11-26

本轮对应 4.2，基准为 `5f7d9aba43fa7cd5883e26a7395fc0c93670d7ae` 加工作树。整改版包含 B 首审注释，五项逐条结论及 A 勾选见[签收问题单](../../../../build/d11-t01/final/review-signed.md)。B 复核与最终验收待独立会话完成。 [整改源码、产品与回放输入摘要](../../../../build/d11-t01/a-correction-1/handoff.json)

| 问题 | 本轮实际对照 |
|---|---|
| CR-001 · 远平面覆盖 | Release 在 B 的 64×48 场景比较原 D32/G3 和资格门控的 F0.r。256 个零深度核心像素恢复，有效 F0.r 与背景可分。第二帧只清 G3，深度保持为 0.5625 的样点跳过核心光照。 [首审帧逐字段比较](../../../../build/d11-t01/a-correction-1/Release/far-plane.json) |
| CR-002 · 角色附件 | 冷包按角色检查零、一、四颜色输出。Release 热候选带多余 location 3 时，普通 Forward 继续用第 1 代完成绘制，统计给出角色及冲突；恢复正确包后发布第 2 代。 [同次图回放](../../../../build/d11-t01/a-correction-1/Release/native.log) |
| CR-003 · 无符号读取 | Debug/Release 从同一错误 GLSL 源构建、读取其旧包、将该包的同一片元 IR 直接交给 OpenGL，三处均拒绝逻辑 sampler 配对。有效整数读取为 0x8105，同时检查双向错配和合法浮点路径。 [Debug 构建拒绝](../../../../build/d11-t01/a-correction-1/Debug/reject-build.log) · [Release 构建拒绝](../../../../build/d11-t01/a-correction-1/Release/reject-build.log) |
| CR-004 · 图能力 | Debug/Release 用实际 Runtime profile 编译包含 Sampled 读和 ShaderRead 导出的图。R16_UINT 与 R32_UINT 各有一层可读，缺 ShaderLoad 的两个反例被拒。 [图检查](../../../../build/d11-t01/a-correction-1/Probe.cpp) |
| CR-005 · 程序复用 | Debug/Release 的核心程序摘要复制失败后，目标程序、摘要和槽保持为空。重试后 VS/FS、Pipeline、ABI 和内容摘要逐项与来源一致。 [Debug 原生结果](../../../../build/d11-t01/a-correction-1/Debug/native.log) |

当前工作树已用 MSVC 19.44 构建 Debug/Release 的 Physara、正式 Shader 包和临时 Verify。原生检查使用 Intel RaptorLake-S Mobile / OpenGL 4.6.0 / 32.0.101.6129。构建延用本机单处理器亲和性。完整热发布及远平面图回放在 Release 执行。初次自检中的 D10 双路径冻结 HDR 比较和其他材质样点沿用原记录，本轮按五项整改的风险取证。

从仓库根以 `cmake --build build --config <配置> --target Physara` 构建当前产品，执行 `python build/d11-t01/a-correction-1/inputs.py <配置>` 生成验证包。将 `build/d11-t01/a-correction-1` 配置到其 `obj` 子目录并构建同配置的 Verify。运行 `build/d11-t01/a-correction-1/obj/Debug/Verify.exe Debug` 与 `build/d11-t01/a-correction-1/obj/Release/Verify.exe Release surface`。旧错误包取自 B 首审输入，逐配置的错误源构建命令与原始片元载荷身份见交接清单。 [原生检查](../../../../build/d11-t01/a-correction-1/Probe.cpp) · [两帧场景](../../../../build/d11-t01/a-correction-1/Surface.inc)

<a id="b-recheck-1"></a>
## B 第一次整改复核与签收 · 2026-11-26

B 签收基准 `5f7d9aba43fa7cd5883e26a7395fc0c93670d7ae` 加本轮工作树。A 整改交接的 46 项源码与文档、206 项证据在复核时逐项匹配。B 最终调整 `StaticGpu.hpp` 的几何程序复用失败说明及 `ForwardShaderPreparation.cpp` 的冷热角色准入说明，逐行核对仅改变三行注释。A 建议 Y 档，实际调度参数由维护者确认。[签收输入与逐文件摘要](../../../../build/d11-t01/b-recheck-1/acceptance.json)

| 已关闭问题 | B 独立核实的结果 |
|---|---|
| CR-001 远平面资格 | 3072 像素中，256 个合格远平面像素恢复了门控 F0.r。仅清 G3 后，全图门控 F0.r 为零，近处深度保持 0.5625。[像素比较](../../../../build/d11-t01/b-recheck-1/far-plane.json) |
| CR-002 角色片元输出 | 原错包多写 location 3。B 增设仅写 location 1 的同数量对照，ABI 和活动资源保持相同；两种错误分别在冷包和热候选被拒，旧代完成绘制，正确包恢复后第 2 代发布。[数量](../../../../build/d11-t01/b-recheck-1/release-native.log) · [位置](../../../../build/d11-t01/b-recheck-1/location-native.log) |
| CR-003 无符号图像与采样器 | Debug/Release 重新构建错源、加载旧错包、直接提交错误 IR 均被拒。合法无逻辑 sampler 读取恢复 0x8105，合法浮点绘制与双向错接均符合预期。[构建拒绝](../../../../build/d11-t01/b-recheck-1/Release/reject-build.log) · [原生对照](../../../../build/d11-t01/b-recheck-1/debug-native.log) |
| CR-004 图内二维 uint 读取 | R16_UINT、R32_UINT 在实际 profile 均有一层可读，同图 Sampled 读与 ShaderRead 导出成功，移除 ShaderLoad 后均被拒。[图准入](../../../../build/d11-t01/b-recheck-1/release-native.log) |
| CR-005 核心程序复用 | 首次摘要分配失败时 VS、FS、Pipeline 及两份摘要保持为空；同一目标重试后各项与来源相等。[故障与重试](../../../../build/d11-t01/b-recheck-1/debug-native.log) |

代码与注释均通过 B 复核，开放问题为零。B 在单台 Intel RaptorLake-S Mobile / OpenGL 4.6.0 / 32.0.101.6129 上重放 Debug 原生检查及 Release 两帧回放，复用 A 交接的整产品构建和前轮的 D10 冻结 HDR 结果。5.1 最终验证及 5.2 同步、归档由 A 接续。[签收问题单](../../../../build/d11-t01/final/review-signed.md)

<a id="final-verification"></a>
## A 最终验证 · 2026-11-26

基准提交为 `5f7d9aba43fa7cd5883e26a7395fc0c93670d7ae`。重建前核对 B 签收的 46 项源码与文档、A 整改清单的 206 项证据及 B 的 8 项证据，SHA-256 全部匹配。B 的最终三行注释已进入本次 Release 构建。A 和 B 的实际调度参数均未登记；D11 的 A 建议档位为 M，本包的 B 建议档位为 Y。B 已独立签收五项问题及注释，完整问题单按原字节留存，单内的相对链接以仓库 Docs 目录为基准。[签收问题单](../../../../build/d11-t01/final/review-signed.md) · [最终输入与产物摘要](../../../../build/d11-t01/final/verification.json)

| 主规格 | 实现与验收证据 |
|---|---|
| core-gbuffer（3 条要求、6 个场景） | `DeclareCoreSurface` 交付同图的四个颜色附件及独立 D32。正式片元和共享解码器使用有效材质、世界法线、过滤后粗糙度及 G3 资格位。64×48 回放中有效像素为 812，MASK 两侧深度分别为 0.68273091、0.72289157，镜像背面法线角差 0.01095°，G2 的 R、B 分别饱和到 65024、64512。复用存储后 3072 个像素的深度与资格均为零。远平面与正深度清资格另由整改图实测。 [字段结果](../../../../build/d11-t01/final/fields/Release/field-results.json) · [整改图读回](../../../../build/d11-t01/final/correction/native.log) |
| opengl-device-resources（1 条要求、2 个场景） | 原生格式表与帧缓冲创建按实际用途、附件数和形状核对。取得合法混合 FBO，异尺寸深度拒绝。图内读取 R16_UINT、R32_UINT 成功，缺少 ShaderLoad 时被拒。 [原生回放](../../../../build/d11-t01/final/fields/native.log) |
| shader-build（1 条要求、1 个场景） | 包反射区分二维 uint 和 float，同槽错误 ABI 和含逻辑 sampler 的旧错包均被拒，旧浮点包按原 ABI 读取。Debug/Release 从错误源构建的拒绝复用 B 的双配置证据。 [B 复核](#b-recheck-1) |
| opengl-shader-pipelines（1 条要求、1 个场景） | 同一错误片元 IR 直接创建 Shader 被拒，合法 fetch 精确返回 0x8105，实际 SRV 数值类型反接在绘制前被拒。错误角色附件的候选保持第 1 代可绘，正确包恢复后发布第 2 代。 [Release 原生记录](../../../../build/d11-t01/final/correction/native.log) |

在 MSVC 19.44、Intel RaptorLake-S Mobile / OpenGL 4.6.0 / 32.0.101.6129 上，以签收工作树重建 Release Physara，并重新链接两组已有回放。运行 `fields.exe Release surface`、`correction.exe Release surface` 和 `python build/d11-t01/final/fields/check.py Release`。整改场景两帧的 12 份浮点读回逐字节匹配 A 整改结果。32 灯、640×480 的普通 Forward 和 Forward+ 分别与 D10 冻结 HDR 全图逐字节相同，四份 SHA-256 均为 `060d20d8148e6047a374aa0167a699bb8103e949866e3a9f608c19c0dd8c68d7`。[构建日志](../../../../build/d11-t01/final/product-release.log) · [回放与产物身份](../../../../build/d11-t01/final/verification.json)

完整性与正确性核对覆盖四份规格的 6 条要求、10 个场景。图资源版本、显式申请附件、有限角色准入和共享材质派生与 design 一致。Debug 原生对照和单输出错位的独立候选复用 B 签收记录。本轮实机覆盖一台 Intel 设备；其他驱动、编辑器路径切换和性能测量未覆盖。

<a id="archive-closeout"></a>
## 归档与工作包交接

新增主规格 [core-gbuffer](../../../specs/core-gbuffer/spec.md)，并将三条增量要求分别并入 [opengl-device-resources](../../../specs/opengl-device-resources/spec.md)、[shader-build](../../../specs/shader-build/spec.md) 和 [opengl-shader-pipelines](../../../specs/opengl-shader-pipelines/spec.md)。同步后逐段比对要求与场景正文，四份主规格及活动 change 均通过严格校验。使用 OpenSpec 1.13.2 的 archive 命令归档，已同步的主规格跳过二次写入。执行归档时 5.2 正在进行，该项在检查归档路径、主规格、开发计划引用及问题单重置后勾选。

[D11-T01](../../../../Docs/Developments/D08-D11-RenderPaths.md#d11-t01) 已验收，[D11-T02](../../../../Docs/Developments/D08-D11-RenderPaths.md#d11-t02) 的前置已满足。B 签收的原问题单及档位记录保存在[签收副本](../../../../build/d11-t01/final/review-signed.md)。五项逐条验收结论见[本 change 的 B 复核记录](#b-recheck-1)。
