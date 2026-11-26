# Tasks

## 1. A · Shader 接口与角色准备

- [x] 1.1 用生成接口定义表面分类，统一两种 Pass 的颜色附件位置配方，并接入 SurfaceAux 正式 Shader 包和 Pipeline 角色。核对核心逻辑 G0–G3 的顺序及格式、Aux 的两个位置和数值类型，以及冷包和热候选实际使用的顶点、材质资源。重建受生成接口影响的正式包，当前固定角色通过既有反射检查。 [位置配方](design.md#用同一生成接口描述紧凑的两个颜色附件) · [构建说明](../../../../Docs/Build.md#shader-构建)
- [x] 1.2 将 SurfaceAux 的原生准备纳入现有需求及预算。一份同接口但缺少整数输出的候选包应在冷准备和热候选准入均定位到相应角色，已有效的代及普通路径仍可用。合法包能够准备，接入后的受影响产品角色准入通过后结束本项。

## 2. A · Draw 路由与共享表面求值

- [x] 2.1 按已解析材质和准入 profile 确定不透明路由，在完整排序后按需切分批次。用一个包含核心、Unlit、实例 MASK 覆盖和 BLEND 的混合工作集核对批次选择、有效材质及原有透明拒绝。核对纯核心时的空辅助批次以及未请求表面时的常规 Draw 准备。相同工作集的原始相对顺序与各 Pass 实际选择一致即完成本项。 [Draw 合同](../../../../Docs/Architecture.md#draws)
- [x] 2.2 共用覆盖前梯度、有效纹理取样、静态切线框架及一次粗糙度过滤供辅助表面生产。核对与核心角色的材质单位和法线约定，检查两路均使用覆盖前取得的梯度。准备具有不同法线和感知粗糙度的核心及 Unlit 标样，以便 3.1 使用独立方向和目标量化精度区分取错表面或重复过滤。 [表面求值](../../../specs/static-surface-evaluation/spec.md)

## 3. A · 表面图与颜色合成

- [x] 3.1 用正式 Core GBuffer 和 SurfaceAux 接续最终的 D32、G1、G3，核对图计划的 Load 依赖、读写版本及有效矩形。以同一 OpenGL fixture 读回近侧 MASK 的孔与等 cutoff 样点、被核心遮住的辅助样点、有限正交远平面和留边。再用观察到的池复用核对已离开表面处的零资格和深度。对照固定几何、覆盖和材质参数证明最近表面，同时从一个纯核心请求确认辅助 Pass 缺席。 [表面规格](specs/core-gbuffer/spec.md#requirement-resolve-the-nearest-opaque-surface-across-material-routes)
- [x] 3.2 交付最终核心资格判定及辅助前向合成。使用同一标样的开发期核心标记 Pass，先读回标记以证明在辅助像素没有核心贡献，再读回正式颜色合成结果。核对辅助前向只读最终 D32、加载 HDR、孔和留边保持旧值、遮挡正确、存活 alpha=1。加入一处核心与两项异批辅助几何的同深度重叠，用已知批次顺序和可区分颜色验证最终颜色与辅助法线属于同一胜者。 [颜色规格](specs/core-gbuffer/spec.md#requirement-compose-auxiliary-color-at-the-depth-of-the-final-surface)
- [x] 3.3 用当前产品 Shader、库和执行器回放 T01 的代表性核心字段用例，验证位置调整后仍能按逻辑 G0–G3 解码。用普通 Forward 与 Forward+ 回放一组可核对身份的 HDR/深度及覆盖对照。确认未请求表面的图无需预备 SurfaceAux，也无对应附件或 Pass。已有回放在输入匹配时复用，目标合同通过后结束回归。 [归档证据](../2026-11-26-implement-core-gbuffer-codec/tasks.md#final-verification)

## 4. A · 本地验收与交接

- [x] 4.1 运行受影响的 Windows x64 产品构建和所需 OpenGL 回放，严格校验 change。开发期回放、核心标记包和读回材料归入 `build/d11-t02/`。记录实际命令、源码和产物身份、设备、运行结果、可复用及未覆盖范围。以 1–3 的受影响合同各有匹配证据为完成条件。 [验证约定](../../../../.agents/skills/physara-verification/SKILL.md)
- [x] 4.2 在 [当前问题单](../../../../build/d11-t02/final/review-signed.md) 交接实际待审工作树、代码和证据入口。按本次修改风险评估 B 的档位，说明混合附件 ABI、同深度顺序、图版本及覆盖的复核难点。确认标样可定位并交由维护者调度 B，交接时保留审查和复核项待办。 [审查档位](../../../../Docs/Developments/AgentModels.md#review-tier)

## 5. B 审查、A 整改与 B 复核

- [x] 5.1 B 独立审查实现、直接消费者和受影响的完整第一方文件。重点核对材质覆盖和批次边界、整数角色准入、实际图依赖、共面颜色和原生证据身份。补齐所涉文件的中文注释并在问题单记录结论及证据。
- [x] 5.2 A 按问题单稳定编号完成必要整改与定向自检，交接实际修改版本和匹配证据。核对各发现都有可复核的整改结果或双方已记录的处理结论。 [首轮整改](#a-correction-1) · [第二轮整改](#a-correction-2)
- [x] 5.3 B 独立复核整改、必要回放及最终注释，按问题编号记录实际结果。B 确认改动和注释满足验收后签收最终版本及其证据。 [独立审查闭环](../../../../AGENTS.md#review-loop) · [B 签收](#b-signoff)

## 6. A · 最终验证与归档

- [x] 6.1 对 B 签收的实际工作树执行最终 verify，核对源码、生成包、库、回放输入和结果的身份。对签收后的输入变化重跑受影响的真实检查，记录复用证据及限制，完成严格规格校验。 [最终验证](#final-verification)
- [x] 6.2 最终验证通过后同步 `core-gbuffer` 增量、归档 change，并更新工作包状态、归档引用及 D11-T03 前置。检查主规格与归档链接，留存 B 档位建议、实配和验收证据，再按约定重置当前问题单。 [归档与交接](#archive-closeout)

<a id="a-local"></a>
## A 本地检查与交接 · 2026-11-26

待审基准为 `v2.0@baf1bd0c8bafa9fb95ad7de74e411dc31359edbf` 加本轮工作树。A 建议 B 使用 Y 档（GPT-6.2 Astra，high），按角色核对包 ABI、图版本、同深度胜出次序和实际像素证据。B 实际配置由维护者确认。 [当前问题单](../../../../build/d11-t02/final/review-signed.md) · [源码、回放输入与产物摘要](../../../../build/d11-t02/handoff.json)

| 检查 | 实际结果 |
|---|---|
| 产品构建与启动 | Windows x64、MSVC 19.44.35228、SDK 10.0.26100.0 的 Debug/Release 构建成功。两版运行 `--hidden --poll-limit=3` 均返回 0。核心四输出与 Aux 两输出的活动反射分别匹配原生角色。 [Debug 构建](../../../../build/d11-t02/product-Debug.log) · [Release 构建](../../../../build/d11-t02/product-Release.log) |
| Shader 准备 | 两种配置用同 ABI、缺少位置 1 输出的 Aux 包触发冷拒绝。热候选在该角色未被请求时也被拒，旧代仍能正常绘制。修复后第 2 代获发布，辅助表面和颜色与原合法包逐像素一致。冷回放按每轮一键推进。 [Debug 原生回放](../../../../build/d11-t02/native-Debug.log) · [Release 原生回放](../../../../build/d11-t02/native-Release.log) |
| 路由、图版本与颜色 | 80×48 画布内读得 352 个核心像素、416 个 Unlit 像素和 3072 个背景像素。MASK 孔处保持核心深度 0.5。恰等于 cutoff 处辅助深度为 0.625，核心资格清除，颜色覆盖 alpha 为 1。D32=0 的远平面、共面异批的最后覆盖者、较远辅助几何和留边均符合独立标样。辅助粗糙度过滤样点为 0.22631836。实际复用 G3 存储后，无辅助 Pass 且旧辅助区域重新为零。编译计划证实 G1/G3/D32 继承 Load，辅助颜色读取最终 D32。 [Debug 像素摘要](../../../../build/d11-t02/Debug/summary.json) · [Release 像素摘要](../../../../build/d11-t02/Release/summary.json) |
| 原路径回归 | 重新链接回放并重建当前包。12 份核心字段读回与 D11-T01 签收结果逐字节相同。两条普通路径在 640×480 的 32 灯输入下保持 D10 冻结 HDR，匹配的 D32 也逐字节相同。普通图无 SurfaceAux 与 GBuffer。 [D11-T01 签收](../2026-11-26-implement-core-gbuffer-codec/tasks.md#final-verification) · [逐字段比较](../../../../build/d11-t02/check.py) |

产品入口为 `cmake --build build --config <Debug或Release> --target Physara --parallel 2`。本次显式传入 MSBuild 的 `_LatestWindowsTargetPlatformVersion=10.0.26100.0`、`TargetPlatformSdkPath` 和 `TargetPlatformDisplayName`，子进程沿用本机单处理器亲和性。默认 SDK 用户目录探测被沙箱拒绝的日志已留存。构建后可在 x64 Visual Studio Developer PowerShell 执行 `build/d11-t02/Verify.ps1 -Configuration <Debug或Release>`，或逐步运行 `prepare.py`、链接回放、运行 `VerifySurface.exe` 和 `check.py`。本次使用 Intel RaptorLake-S Mobile／OpenGL 4.6.0／32.0.101.6129。 [早期 SDK 诊断](../../../../build/d11-t02/first-package.log) · [回放脚本](../../../../build/d11-t02/Verify.ps1)

本轮以当前受支持的核心与 Unlit 材质做端到端验收。实际非核心 lit 扩展由 D12 提供。D11-T03 的屏幕光照、D11-T04 的用户切换和 D11-T05 的画质、成本整体验收仍各自登记，其他 GPU 和驱动未覆盖。B 审查、整改复核及最终验证待对应责任会话推进。

<a id="a-correction-1"></a>
## A 首轮整改与自检 · 2026-11-26

复核版本为 `v2.0@baf1bd0c8bafa9fb95ad7de74e411dc31359edbf` 加当前工作树，包含 B 首审注释及 A 整改。按 [当前问题单](../../../../build/d11-t02/final/review-signed.md) 保留 CR 编号。原生回放重新链接两种配置的库，使用对应的当前产品包。 [复核版本、输入和产物 SHA-256](../../../../build/d11-t02/a-correction-1/handoff.json)

| 检查 | Debug 和 Release 实测 |
|---|---|
| CR-001 · HDR 与深度生产身份 | 纯核心 Direct Lighting 在冷准备和预热 Forward 后记录相同的核心程序身份，均成功提交相同颜色、深度与标记图。普通 Forward 的重复候选引用其已接受票据。核心回调没有稳定的程序身份时，输出仍可接受，历史前驱为零。混合图只替换 SurfaceAux 包，Core 与 Forward 的摘要保持相同，辅助像素的 G1 粗糙度由 0.625 变为 0.3125，G3、D32 与 HDR 一致。下一候选未接续旧混合提交。 |
| CR-002 · Froxel 与共享批次 | 有效有限灯网格已准备。纯核心 View 分别以 `holdFroxelWrites=false/true` 与普通离屏 View 共同规划、收尾并声明。两轮共享额度均为 66,840,380 字节、19 笔，余 268,484 字节。图中实际登记 66,839,420 字节、15 笔，两轮均有离屏大纹理上传且无 Froxel 写入。普通 Forward+ 对照执行 4 笔存活 Froxel 上传，共 2,163,388 字节，读回有效点光。 |
| CR-003 · 阴影诊断 | 混合 View 实际交付一层方向光级联。核心颜色回调与有效阴影诊断的组合在上传前以 `InvalidViewRequest` 拒绝。普通诊断额外请求 GBuffer 后，其 HDR 和主 D32 与普通诊断逐字节一致。关闭诊断后，辅助片元恢复线性基色与配对的 D32。 |
| 产品与既有回归 | Windows x64、MSVC 19.44.35228、SDK 10.0.26100.0 的两种产品构建及 `--hidden --poll-limit=3` 通过。重跑核心与辅助覆盖、错误包拒绝和池复用。D11-T01 的 12 份签收字段逐字节匹配，普通 Forward 与 Forward+ 的 32 灯 HDR 匹配冻结基线，两者 D32 相同。 |

使用与首次 A 检查相同的显式 SDK 参数和构建入口。在 x64 Visual Studio Developer PowerShell 按 `-Configuration` 参数运行对应的回放。 [本轮回放脚本](../../../../build/d11-t02/a-correction-1/Verify.ps1) · [Debug 构建](../../../../build/d11-t02/a-correction-1/product-Debug.log) · [Release 构建](../../../../build/d11-t02/a-correction-1/product-Release.log) · [Debug 启动](../../../../build/d11-t02/a-correction-1/hidden-Debug.log) · [Release 启动](../../../../build/d11-t02/a-correction-1/hidden-Release.log) · [Debug 原生回放](../../../../build/d11-t02/a-correction-1/native-Debug.log) · [Release 原生回放](../../../../build/d11-t02/a-correction-1/native-Release.log) · [严格规格校验](../../../../build/d11-t02/a-correction-1/openspec.log)

原生设备为 Intel RaptorLake-S Mobile，OpenGL 4.6.0，驱动 32.0.101.6129。CR-002 的双路临界用例覆盖准备与图声明，独立 Forward+ 对照覆盖 GPU 执行与读回。当前状态为 A 自检完成，待 B 复核及验收。

<a id="a-correction-2"></a>
## A 第二轮整改与自检 · 2026-11-26

B 首轮复核通过 CR-001 至 CR-003，并在 Release 实机复现了 CR-004。A 在当前 `v2.0@baf1bd0c8bafa9fb95ad7de74e411dc31359edbf` 工作树收紧灯表准备条件。仅普通前向颜色或辅助重画触发灯表准备，核心栅格继续使用独立准备的 View 常量。 [当前问题单](../../../../build/d11-t02/final/review-signed.md) · [B 原始复现](../../../../build/d11-t02/b-recheck-1/budget-Release.log) · [复核输入及产物 SHA-256](../../../../build/d11-t02/a-correction-2/handoff.json)

| 检查 | Debug 和 Release 实测 |
|---|---|
| CR-004 · 冷帧与暖帧 | 冷态直接准备并提交与 B 复现相同的 4096 × 4040 单 mip RGBA8 纹理。该纹理上传在编译图中存活，总准备额度为 66,442,160 字节、11 笔，余 666,704 字节。纯核心图无灯表资源或写入，随后两帧上传需求均为 0。三个输出均由 View 接受，中心 G3 为核心、D32 为 0.5，冷暖 HDR 逐字节一致。 |
| CR-004 · 前向有消费者 | 同一暖态场景切回普通 Forward，颜色 Pass 读取实际存活的 1,048,576 字节零灯表写入版本。提交获接受，覆盖像素的 RGBA 为 (0, 0, 0, 1)，D32 为 0.5。冷态混合辅助回放同样读取其零灯表生产版本。 |
| 既有回归 | 重建两种产品配置及回放，`--hidden --poll-limit=3` 均返回 0。重跑 CR-001 至 CR-003。CR-002 的纯核心规划需求减为 250,768 字节，共享需求仍为 66,840,380 字节，普通 Forward+ 的四笔 Froxel 写入仍存活。D11-T01 的 12 份签收字段及 D10 冻结的两条普通路径 HDR 逐字节匹配。 |

本轮沿用首次 A 的构建入口和显式 SDK 参数。原生回放覆盖同一台 Intel RaptorLake-S Mobile（OpenGL 4.6.0，32.0.101.6129），供 B 复核 CR-004。 [回放入口](../../../../build/d11-t02/a-correction-2/Verify.ps1) · [Debug 原生回放](../../../../build/d11-t02/a-correction-2/native-Debug.log) · [Release 原生回放](../../../../build/d11-t02/a-correction-2/native-Release.log) · [OpenSpec 严格校验](../../../../build/d11-t02/a-correction-2/openspec.log)

<a id="b-signoff"></a>
## B 第二轮复核与签收 · 2026-11-26

**D11-T02 已通过 B 验收。** CR-001、CR-002、CR-003 在首轮整改复核中关闭；本轮确认 CR-004 的修正及注释并关闭该项。签收版本为 `v2.0@baf1bd0c8bafa9fb95ad7de74e411dc31359edbf` 加第二轮交接清单中的正式输入。清单 SHA-256 为 `f4a4c389485873f4610b8070e8ef7c7d745cd213bd0770d05933e155b5f7a732`。签收后问题单与本 tasks 新增 B 的结论。A 建议 Y 档，维护者已指定本会话担任 B，具体模型和推理级别未提供。[问题单](../../../../build/d11-t02/final/review-signed.md) · [签收清单](../../../../build/d11-t02/a-correction-2/handoff.json)

B 独立复跑已核对身份的 Release 原生回放。4096 × 4040 单 mip 核心场景的冷帧以 66,442,160 字节、11 笔上传成功提交，随后两帧均为零上传，三帧无灯表资源。普通前向以及混合辅助重画均保留 1 MiB 零灯表的真实消费者。该次回放生成的 31 份原始读回与交接清单匹配。Debug 复用 A 的对应回放，本轮原生结果限于同一台 Intel OpenGL 4.6 设备。A 接手 6.1、6.2。[B 实机日志](../../../../build/d11-t02/b-cr004-signoff/native-Release.log) · [A 第二轮记录](#a-correction-2)

<a id="final-verification"></a>
## A 最终验证 · 2026-11-26

核对基准 `v2.0@baf1bd0c8bafa9fb95ad7de74e411dc31359edbf`、工作树的 27 份源文件与文档和 B 签收清单的 SHA-256。25 份输入与交接一致，另外两份为 B 写入验收结论的问题单及 tasks。13 份回放输入、17 份签收基线及 Debug/Release 各 81 份产物与记录逐项匹配。B 独立 Release 回放中的 25 份产品包、4 份标样包和 31 份原始读回分别与交接版本一致。签收后的构建和回放输入保持相同，本次复用 A 的双配置构建、隐藏启动、原生回放和 B 的独立 Release 回放。OpenSpec 1.13.2 严格校验通过。 [验证清单](../../../../build/d11-t02/final/verification.json) · [B 签收问题单原件](../../../../build/d11-t02/final/review-signed.md) · [严格校验](../../../../build/d11-t02/final/verify-spec.log)

| 增量要求 | 实现与场景证据 |
|---|---|
| Produce a typed single-sample core surface | `DeclareCoreSurface` 与共享附件配方交付 G0–G3、D32 和明确的 G3 分类。当前包的两帧 12 份核心字段与 T01 签收结果逐字节一致。核心远平面资格沿用 T01 已验收场景，当前回放另核对辅助远平面及背景分类。 |
| Resolve the nearest opaque surface across material routes | `PrepareDeferredSurface` 按覆盖和 Reverse-Z 接续 G1、G3、D32。读回涵盖 MASK 孔与等阈值、被核心遮挡的辅助表面、辅助远平面、留边和池复用后的初始化。 |
| Compose auxiliary color at the depth of the final surface | `AddOpaqueForwardExtensions` 按同一批次顺序加载核心工作色并读取最终 D32。标记回放核对 G3 资格、MASK、共面最后覆盖者及覆盖 alpha。有效阴影诊断与核心合成的互斥准入和独立表面请求均有实机证据。 |

增量的 3 条要求与 7 个场景均有实现和对应证据。设计中的冻结批次、共享附件配方、原生角色准入、读写版本和按实际消费者计费均与交付一致。B 已签收代码与注释。A 建议的 B 审查档位为 Y，实际调度参数未登记。原生证据覆盖单台 Intel RaptorLake-S Mobile、OpenGL 4.6.0、驱动 32.0.101.6129。生产 `DeferredLighting`、路径事务及三路径等价门槛分别由 D11-T03 至 D11-T05 交付。 [B 签收](#b-signoff) · [原生回放](../../../../build/d11-t02/a-correction-2/native-Release.log)

<a id="archive-closeout"></a>
## 归档与工作包交接

将最近表面路由和等深合成两条要求并入 `core-gbuffer`，更新四附件与 G3 分类要求。主规格共 5 条要求、11 个场景，44 份主规格通过 OpenSpec 1.13.2 严格校验。按已同步的状态归档此 change，核对活动项、归档链接和签收版本一致性。 [主规格](../../../specs/core-gbuffer/spec.md) · [同步核对](../../../../build/d11-t02/final/spec-sync.json) · [归档校验](../../../../build/d11-t02/final/archive-audit.json)

D11-T02 已验收归档，D11-T03 的前置已满足。B 签收问题单按原字节留存，内部链接以签收时的工作区为基准。当前问题单已恢复项目模板。 [开发计划](../../../../Docs/Developments/D08-D11-RenderPaths.md#d11-t03) · [签收原件](../../../../build/d11-t02/final/review-signed.md)
