# Tasks

## 1. 整合正式表面消费者

- [x] 1.1 A 从现有有效材质行提取共享取样与 MaterialInputs，接到现有 Base Color、World Normal、Material Channels、Emissive 和 Unlit 路径。先核对 V1 的包、活动反射和 ABI，再用 V4 的非白顶点色、不同通道与超白自发光核对颜色域、默认值和原始感知粗糙度。
- [x] 1.2 A 将几何法线、着色法线和必要的切线框架组织为共享 SurfaceGeometry，保留有效 UV 朝向、双面翻转及插值退化的语义。以 V4 读回镜像实例和两种深度组合的遮罩边缘，核对现有共享位置和 alpha。补齐共享入口的职责和方向约定。

## 2. 实现并验证核心 BRDF

- [x] 2.1 A 实现核心 PixelParams 和几何粗糙度求值，使用当前光栅像素的几何法线导数和固定粗糙度下限。建立 V2 的端点及混合材质标样，用 V3 的平坦和已知梯度像素验证派生参数、过滤顺序和连续因子共用程序。
- [x] 2.2 A 实现共享 GGX、完整 height-correlated Smith、显式 F90 的 Schlick 和 Lambert，按设计处理半球和退化半角。用 V2 推导参考值和边界预期，用 V3 原生读回检出 fast Smith、重复余弦或掠射非有限值。随实现记录参考来源、单位和精度选择。
- [x] 2.3 A 实现 DFG R/G 到方向反照率、间接漫反射预算及镜面补偿的共享转换。用 V2 的双精度白炉和方向积分确定预期，用 V3 的不同 R/G 标样核对通道、逐色分量和补偿乘数。记录单位白金属的闭合和介质、混合材质的近似偏差。

## 3. A 本地核对与交接

- [x] 3.1 A 核对 D07-T01 三项工作目标，汇总 V1–V4 的匹配证据、严格 OpenSpec 校验与链接检查。记录源码、实际 Shader 包和探针或产品、fixture、命令、设备和未覆盖范围。将受审版本、相关 diff、未跟踪材料和检查入口登记到 [当前问题单](../../../../Docs/CodeReview.md)，由维护者调度 B。

## 4. 独立审查、整改与复核

- [x] 4.1 B 独立核对实际受审版本、规范及直接消费者，检查颜色和覆盖、几何过滤、BRDF 与 DFG 模型及数值证据。通读涉改完整第一方文件，补齐职责、接口合同和流程注释，在当前问题单登记覆盖、稳定 CR 编号及结论。 [审查闭环](../../../../AGENTS.md#review-loop)
- [x] 4.2 A 按 CR 编号修正并自检，记录实际变更及与风险对应的回归。无待整改项时由 A 据实登记并确认。
- [x] 4.3 B 对最终实际输入复核整改、证据和涉改注释，分别确认功能和注释质量，登记最终版本及验收结论。

## 5. 最终验证、同步与归档

- [x] 5.1 A 在 B 签收后对当前输入运行或复用匹配的 V1–V4，完成 OpenSpec 最终 verify。记录实际包与原生绘制的身份、结果和证据范围。
- [x] 5.2 A 在本任务表留存审查、整改、复核与最终验收证据，将 delta 同步到 `static-surface-evaluation`，核对既有场景与新增条款，运行严格校验。
- [x] 5.3 A 用 OpenSpec 归档，核对相对链接、验收引用和开发计划登记。通过归档并留存完整证据后，按模板恢复 [当前问题单](../../../../Docs/CodeReview.md)。

<a id="verification"></a>
## 验证范围与停止条件

开发探针、输入和临时工程放在 `build/d07-t01/`，使用现有 ShaderBuild 和正式 RHI。优先取得 D06 已有标样；当前工作区的 `build/` 未包含其归档引用的探针目录。材料缺失时，只针对本包的变化组装下面所需的最小场景。凡复用结果，核对对应的源码、包、消费者、标样与环境。 [D06-T05 定向用例](../2026-11-16-render-static-scenes-forward/tasks.md#final-verification) · [构建说明](../../../../Docs/Build.md#shader-构建) · [验证规则](../../../../.agents/skills/physara-verification/SKILL.md)

| 检查 | 实际观察与预期 | 停止条件 |
|---|---|---|
| V1 · 正式包与构建 | 用 CMake 的 `Physara` 目标生成验证配置的受影响包，核对 `StaticForward` 及 `StaticDepth` 的实际传递依赖、活动反射、生成 ABI 和包身份。V4 同次验证正式包被加载并完成原生链接。 | 受影响入口正常构建并使用匹配包，正式构建只依赖项目输入，检查范围限于直接消费者。 |
| V2 · 独立 CPU 参考 | 用双精度解析值及半球积分核对介质、金属和一组混合值。理想白金属 `p=1` 的单次积分为 `1−NoV×ln((1+NoV)/NoV)`，在 `NoV=1` 与 `0.5` 分别约为 `0.3068528194` 和 `0.4506938557`。以解析锚点和独立积分的 DFG 查看方向反照率，补偿白炉积分在 `1±0.001` 内，再以一个中间粗糙度核对积分收敛及介质、混合近似。 | 数值积分与两个解析锚点的绝对差至多 `1e-5`，金属端点和混合预期可复核，补偿后的白炉与记录的近似满足各自判据。 |
| V3 · 共享 GLSL 原生读回 | 在真实 GL 上用当前共享 include 构建开发包。用浮点附件读回派生参数、波瓣与 DFG 项。几何过滤取已知像素梯度且使 kernel 低于上限的样点，对照平坦和有梯度的输出。用 `p=0` 正入射、一个非轴对齐的中间粗糙度、掠射、半球外和退化半角区分精度及边界，混合颜色输入搭配不同 R/G 的参考样点检出错码与补偿重复。 | 从真实完成的提交读到有限值，常规 FP32 标样满足 `absError ≤ 2e-5 + 5e-4×abs(reference)`，半球外为零，退化半角镜面为零。记录所用光栅位置、样点值与附件格式。 |
| V4 · 正式 Forward 回归 | 用一份非对称 glTF 场景和当前正式包绘制，组合非单位因子、UV0/UV1、常规和负非均匀实例、正反面以及前后层 MASK/OPAQUE。核对标准 Unlit、材质通道和自发光工作色，阈值等号保留的像素在直接和前置深度组合中读到同一遮挡。 | 按现有格式及读回容差得到可区分的法线、线性通道和深度结果。产物对应实际图及有效提交，命中上述风险后停止。 |

V3 的 DFG 样点来自 V2 的独立参考，V2 的白炉积分和 V3 的实际 Shader 求值分别记录。T03 的正式直接光、T04 的 LUT 上载和实际 IBL，以及跨设备和性能结论沿相应工作包验收。

<a id="a-local-evidence"></a>
## A 本地核对与证据

以 `213a705e01ff54bd9a6471bf6a91a9fce26ce28a` 为基准，在 Windows x64、MSVC 19.44.35228、Windows SDK 10.0.26100.0 下从独立 Ninja Multi-Config 目录构建正式 Debug `Physara`，随后从仓库根执行 `& ./build/d07-t01/Verify.ps1`。本机 MSBuild 的 SDK 探测被用户目录访问权限阻断，回放用同一 MSVC/SDK 和正式 CMake 目标，保留初次失败日志。回放前后比对 1591 项正式输入和实际包、库、exe、探针及标样的 SHA-256，并逐一校验读回包报告的 31 个依赖。 [回放入口](../../../../build/d07-t01/Verify.ps1) · [首次构建](../../../../build/d07-t01/v1-ninja-debug.log) · [MSBuild 诊断](../../../../build/d07-t01/v1-build-debug-sdk.log)

| 检查 | 实际结果 |
|---|---|
| V1 | 正式 Debug 构建及独立工作目录中的 `Physara.exe --hidden --poll-limit=6` 通过。`StaticForward` 和 `StaticDepth` 的期望 ABI 相同且核对成功，活动资源为顶点 3、片元分别 12 和 3。两个组合在正式 Forward 的原生执行中加载对应包并完成 GL 链接。 |
| V2 | `p=1` 在 `NoV=1/0.5` 的双精度单次方向反照率分别为 `0.3068528194/0.4506938557`，与解析锚点吻合。`p=0.55、NoV=0.42` 得到 `G=0.8145535850`。三个白金属补偿结果均在 `1±0.001` 内。白色介质的直接响应积分约为 `1.0134–1.0616`，间接近似约为 `1.0006–1.0011`，混合材质各色分量留在参考记录。 [参考日志](../../../../build/d07-t01/v2-cpu-reference.log) |
| V3 | 一次真实 Graphics 提交完成后读取 72 组 `RGBA32_FLOAT` 结果，均满足计划容差。零粗糙度读到 `p=0.045、alpha=0.002025`，平坦和有梯度的几何分别读到 `p=0.25/0.3039894`。半球外及退化半角的约定零值、共享单次散射和 DFG R/G 能量项均对照独立预期。 |
| V4 | 用本包重建的非对称 glTF 标样，经正式 Asset/Scene、View 和 Forward 图执行。两种深度组合各读四种模式，共 40 个工作色与深度标样。MASK 阈值处保留的近像素读到 `0.75`，阈值下方露出的 Unlit 远面读到 `0.25`。镜像与背面法线方向、UV0/UV1、AO/roughness/metallic、缺席槽默认值及超白 Emissive 都匹配预期，最高观测工作色分量为 `6.3984375`。 |

原生证据覆盖本机 Intel RaptorLake-S Mobile Graphics Controller、OpenGL 4.6.0 Build 32.0.101.6129 的 Debug 路径。V3 的开发包和 V4 的正式包均按事件完成后读回。Release、其他驱动、正式直接光及 IBL 的证据等待各自对应的检查。OpenSpec 严格校验以及变更文档的链接和 diff 核对属于本轮结构检查。 [原生读回](../../../../build/d07-t01/native-debug.log) · [比对摘要](../../../../build/d07-t01/native-compare.log)

本轮涉及 3 个已跟踪和 5 个新增 Shader 文件。交接清单 SHA-256 为 `6a0bd229d826c99ae994f4c3c923073a491add1b90030d397975dc5e18360ea2`，其中记录基准、实际差异、正式输入、测试输入、产物和日志身份。A 本地检查交接给维护者调度的 B。 [交接清单](../../../../build/d07-t01/handoff.json) · [已跟踪 Shader diff](../../../../build/d07-t01/a-shaders.diff) · [当前问题单](../../../../Docs/CodeReview.md)

<a id="a-round-1"></a>
## 第 1 轮 A 注释接纳与自检

2026-11-17，A 按问题单接纳 B 对 8 个 Shader 的注释。本轮正式发现为 0 项，功能整改不适用。按首轮交接摘要核对原始输入和产物，从锁定的副本、Git 差异及原始取样源码复核注释前版本。8 个文件逐行去除独立行注释和空行后，代码与预处理指令相同。实际工作区 1591 个正式输入中，变动集中在这 8 个 Shader，集合摘要 `764eee18c648285f547034fb6f5eeff0946fd531ba20acdcda6b1afbfba0f365` 与 B 登记一致。 [B 审查及注释](../../../../build/d07-t01/final/review-signed.md) · [当前正式输入](../../../../build/d07-t01/a-correction/source-manifest.json)

从仓库根运行 `python -B -X utf8 build/d07-t01/a-correction/CheckAnnotations.py`，复用首轮锁定的 Debug ShaderBuild 和生成接口，将新包写入本轮证据目录。包检查核对当前依赖、活动反射和期望 ABI。 [验证入口](../../../../build/d07-t01/a-correction/CheckAnnotations.py) · [构建及校验日志](../../../../build/d07-t01/a-correction/package-check.log)

| 程序 | 本轮结果 |
|---|---|
| `StaticForward` | 从当前源码重建，内容身份为 `3bb9ba3db0dda7cd776eaade9dcb8fc83825fd37f17146aee60d434787afca34`。两阶段 SPIR-V 原始字节与首轮包相同。 |
| `D07Numerics` | 用当前共享 include 和原来匹配的标样重建，内容身份为 `46e11e139901bdfde24c83e88f97d5795b1c9f7e073825182d2108f44371776b`。两阶段 SPIR-V 原始字节与首轮探针包相同。 |
| `StaticDepth`、`ForwardDisplay` | 首轮包身份、当前依赖及期望 ABI 核对通过，使用原包。 |

本轮证据范围为源码和包级验证。V2–V4 的数值证据引用首轮记录，复用依据为相同的测试输入、消费程序和 SPIR-V。当前验证包、原始证据及身份已关联到交接清单，清单 SHA-256 为 `738df0c791b1ccd4f4b1fe8f1cc2dc6fba9f28c1cab2f70358ef762b0b01413e`。最终 verify 依据 B 签收版本核对对应产物。 [首轮原生证据](#a-local-evidence) · [本轮交接清单](../../../../build/d07-t01/a-correction/annotation-handoff.json)

<a id="b-round-1"></a>
## 第 1 轮 B 复核与签收

2026-11-17，B 完成 A 接纳后的独立复核，代码正确性与注释可读性均通过，正式发现 0 项，功能整改不适用。首轮已通读并补齐本 change 的全部 8 个 Shader：`UnitDirection.glsl`、`SurfaceFrame.glsl`、`MaterialInputs.glsl`、`StaticInputs.glsl`、`CoreSurface.glsl`、`CoreBrdf.glsl`、`StaticPreview.glsl` 和 `Color.frag`；已核对材质有效值/CPU 编码、静态顶点与覆盖、Forward 及 Shader 包的直接消费合同。 [审查结论和完整注释覆盖](../../../../build/d07-t01/final/review-signed.md)

B 本次重新读取这 8 个文件，与首轮 B 留存内容逐文件比对，原始字节完全相同；按问题单所列算法现场重算集合摘要为 `764eee18c648285f547034fb6f5eeff0946fd531ba20acdcda6b1afbfba0f365`。签收版本为分支 `v2.0`、HEAD `213a705e01ff54bd9a6471bf6a91a9fce26ce28a` 上的实际工作区，含 3 个已跟踪 Shader 差异及 5 个新增 Shader，无暂存变更；相关 C++、生成接口和构建输入无新增差异。注释再次通读后仍准确，本次未修改 Shader 或实现。文档链接与 diff 检查通过。

B 已核对 [A 本轮记录](#a-round-1) 的接纳范围、重建包角色及证据复用说明：`StaticForward`/`D07Numerics` 重建，`StaticDepth`/`ForwardDisplay` 核对后复用，与当前源码影响面相符。按维护者限定的代码审查范围，B 未扫描或读取 build 产物；包字节等价、反射/ABI 及 V1–V4 实机结果引用 A 的记录，未独立重跑。首轮 B 的公式、解析锚点和注释 L0 核对仍适用；GPU、产品界面、Release、跨驱动及后续正式直接光/IBL 不新增验收声明。

B 已自行勾选 4.1 和 4.3，A 的 4.2 保持原状态。本次签收允许 A 进入 5.1 的最终 verify，并对签收输入登记实际产物及复用依据；5.1–5.3 仍未完成。本次未同步主 spec、归档或重置问题单；后续改变源码或构建输入时，重新复核相应范围。

<a id="final-verification"></a>
## 最终验证与证据

2026-11-17，A 核对 B 签收的 8 个 Shader，集合摘要为 `764eee18c648285f547034fb6f5eeff0946fd531ba20acdcda6b1afbfba0f365`。1591 项正式输入与 A 回交 B 的输入一致。B 原始签收问题单的 SHA-256 为 `ee8d4defce650aafde59518825a6e4632fc1b51bc28cbfad3cdca49b642f359c`。 [B 签收快照](../../../../build/d07-t01/final/review-signed.md) · [最终源码清单](../../../../build/d07-t01/final/source-manifest.json)

从仓库根运行 `python -B -X utf8 build/d07-t01/final/VerifySigned.py`。在独立目录装配匹配的 Debug 消费者和标样，开始及退出时核对源码、包、exe 和标样的身份。最终证据清单 SHA-256 为 `20073ba7ea4e24e5ea698ca21a0d80af96397d8558b11c3b6315d0f28a2717f1`。 [回放入口](../../../../build/d07-t01/final/VerifySigned.py) · [最终运行清单](../../../../build/d07-t01/final/signed-verification.json)

| 检查 | 签收版本结果 |
|---|---|
| V1 · 正式包与构建 | 复用首轮 CMake Debug 构建的同一 C++ 消费者、库和 ShaderBuild，C++、生成接口及构建输入逐项匹配。用签收 Shader 重建 `StaticForward` 和 `D07Numerics`，文件摘要与 A 回交的包一致。按实际依赖复用 `StaticDepth`、`ForwardDisplay`、`EditorUi`，五个包按期望 ABI 和活动反射校验通过。独立目录的正式 `Physara.exe --hidden --poll-limit=6` 成功退出。 |
| V2 · CPU 参考 | 复用摘要匹配的双精度积分、白炉输出和标样期望。5 份场景输入与初次运行一致。 |
| V3 · 原生数值 | 以签收源码重建的共享 Shader 包通过实际提交和读回，72 组 `RGBA32_FLOAT` 值均匹配计划容差。 |
| V4 · 正式 Forward | 真实 glTF 和图输出在两种深度组合、四种预览模式中共读回 40 个匹配像素。原生图提交记录为 2–9，宿主完成关闭。 |

| 验证维度 | 结论 |
|---|---|
| 完整性 | D07-T01 的实现和 A/B 审查任务均已完成，4 项需求及 5 个场景有对应的实施路径和证据。 |
| 正确性 | 有效取样与核心参数由 `MaterialInputs`、`BuildPixelParams` 实现，V3/V4 区分介质、金属和材质通道。几何过滤和共用 MASK 分别由 `SurfaceFrame` 与静态覆盖链验证。BRDF 角度边界、DFG 通道和白炉结果由 `EvaluateCoreBrdf`、`EvaluateCoreDfg` 及 V2/V3 核对。 [核心参数](../../../../Shaders/Material/CoreSurface.glsl) · [几何过滤](../../../../Shaders/Geometry/SurfaceFrame.glsl) · [核心波瓣和能量项](../../../../Shaders/BRDF/CoreBrdf.glsl) |
| 一致性 | 共享库遵循材质、几何与 BRDF 边界，正式预览、顶点和深度入口沿用生成 ABI 与现有图。8 个 Shader 的实现和注释匹配 B 签收，OpenSpec change 严格校验通过。 |

实机范围为单台 Intel RaptorLake-S Mobile Graphics Controller、OpenGL 4.6.0 Build 32.0.101.6129 的 Debug 路径。其它驱动、Release 和后续正式照明按相应范围验收。 [原生读回](../../../../build/d07-t01/final/native-debug.log) · [比对摘要](../../../../build/d07-t01/final/native-compare.log)

### 主规范同步

2026-11-17，A 将已通过验收的 delta 合入 `static-surface-evaluation`。新增 4 项需求和 5 个场景，合并后共 8 项需求和 11 个场景。核对原 Purpose、既有需求和场景的原始字节以及新增块的内容，重复同步无需写入。主规范严格校验 31/31。 [主规范](../../../specs/static-surface-evaluation/spec.md) · [同步清单](../../../../build/d07-t01/final/spec-sync.json)

### 归档

2026-11-17，OpenSpec 按 `spec-driven` 将 `add-core-surface-brdf` 移入 `2026-11-17-add-core-surface-brdf`。保留 `.openspec.yaml` 和规范增量的原始字节。核对归档链接、同步结果、B 签收和最终运行证据后，开发计划登记 D07-T01 已验收，并推进到 D07-T02。归档后 31 份主规范严格校验通过，活动 change 为 0。 [当前阶段](../../../../Docs/Developments/Developments.md#current-stage)

留存归档前问题单后按模板逐字节恢复工作区问题单。本 change 完成 12/12。 [归档前问题单快照](../../../../build/d07-t01/final/review-closeout.md) · [问题单模板](../../../../Docs/CodeReview.md.tpl)
