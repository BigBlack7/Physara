# Tasks

## 1. 资产输入与可用框架

- [x] 1.1 接入声明、标准字段和线性方向槽，保留 sampler、有效 UV、仿射及依赖身份。沿正式 Asset 入口用一个自制 glTF 标样及其最小 GLB 对照核对默认值、线性 RGB、UV 覆盖及换图后新旧固定版本。用能区分字段越界和纹理有效 UV 缺失的负例核对具体诊断。记录所用输入身份和可复查结果。 [导入规格](specs/static-gltf-import/spec.md)
- [x] 1.2 按目标 Primitive 选择合法源框架或由基底 normal 的有效 UV 生成角点框架。使用带镜像接缝的小网格检查拆点和 handedness，沿既有逐面法线规则覆盖缺 NORMAL 的输入。合法源切线无需依赖 normal 图，有方向图而两种来源均不可用时在导入边界给出原因。复用已覆盖相同输入的 MikkTSpace 检查。 [几何设计](design.md#geometry-frame)
- [x] 1.3 落地有限材质支持及可定位报告。用同一场景的两份声明对照 required 拒绝和 optional 保留其它已支持响应，另核对一个标准排斥项。以原固定模型仍有效、未发生部分发布和报告与实际颜色家族相符为完成条件。 [输入与范围](design.md#input-and-scope)

## 2. 实例编辑与场景文档

- [x] 2.1 接入 Scene 有效材质、Inspector 和类型化编辑。用共享源的两个实例提交强度、弧度和方向槽编辑，检查显式零、单项清除及 Undo/Redo。用一次无法建立框架的基底 normal 编辑核对具体拒绝原因和原有实例状态。颜色就绪归因随 4.2 的原生交付验收。 [编辑规格](specs/editor-domain-edits/spec.md)
- [x] 2.2 扩充 v1 的读写及恢复校验。将 2.1 的同一有效覆盖保存并重开，核对旧 v1、显式零、弧度和该槽的采样参数。用相同入口测试一次不兼容槽用途或几何，确认活动文档、路径和版本保有资格。 [文档规格](specs/scene-document-persistence/spec.md)

## 3. 生成接口、各向异性表面与照明

- [x] 3.1 更新权威 Shader ABI、材质行编码、槽配方及有限产品目录。以普通生成目标检查布局、反射和按家族分组的索引，核对 t5、t13/s13、576 B 行大小、12 个新增色角色及已有角色的互斥范围。核对实际普通、覆盖及各向异性角色的顶点/片元活动输入。 [ABI 与角色设计](design.md#roles-and-delivery)
- [x] 3.2 复用几何键和切线流实现方向纹理采样及世界轴求值。用 1.2 的标样增加 UV0/UV1 的非对称线性纹素、独立镜像、负和非均匀变换及双面/normal 对照。以已知 T=X、B=Y、角度 π/4 和实例比例 (2,1,1) 时的投影轴 `normalize((2,1,0))` 起算，对照法线投影和实际高光轴。检查衰减的纹理 B 与零方向、缺图、局部退化的代表样点。 [表面规格](specs/static-surface-evaluation/spec.md)
- [x] 3.3 将双轴 GGX D/V 接入共享直接基底镜面。用双精度独立 NDF/Smith 参考核对两轴和中间强度的正向与掠射样点、粗糙度上下界及等向极限。在同一场景用一个 IOR/specular 介质与金属对照确认 Fresnel 和金属响应沿原基底合同，重复使用基底等价证据。 [光照设计](design.md#lighting-model)
- [x] 3.4 配对现有环境资源和 bent-reflection。运行固定方向环境与单位白炉两个用途，用独立几何计算预测旋转后的查找方向，以匹配 DFG 的独立参考核对白炉和等轴边界。比较 Direct、无环境 Physical 及含环境 Physical 的配方应用，记录实测范围内的近似误差和局限。 [环境规格](specs/environment-lighting/spec.md)

## 4. 原生交付与范围回归

- [x] 4.1 用正式 Windows x64 目标构建 Debug 和 Release，生成全套配对 ABI 和产品包，并分别运行现有隐藏启动入口。在实际 GL 冷准备以及现有活动代上，检验所声明各向异性角色的阶段输入和原生绑定。用同类缺项验证一个冷包及一个未选热角色，按真实设备配方确认拒绝、原因、旧代可用和合法恢复。运行真实 `SourceReload` 证明完整候选在定额内发布，登记实际条目、包字节、共驻峰值与帧前进度。如确实超限，以测得需求调整固定额度并复验。 [构建入口](../../../../Docs/Build.md) · [Shader 规格](specs/shader-generation-reload/spec.md)
- [x] 4.2 在同一冻结单样本场景对比 Forward、Forward+ 和 Deferred 的 Direct 及带方向环境 Physical。合用各向异性 MASK、孔内核心、等深对照与代表性的已有扩展，登记正常接受票据、读回 HDR/D32、影图和 Deferred G1/G3，比较可见胜出和已声明工作色容差。保存方向参考图和可对应原始样点。沿 2.1 的目标观察零强度路由变更、历史失效，以及 Inspector 的准备、诊断和最后实际受光身份。 [渲染规格](specs/static-forward-rendering/spec.md) · [三路径参考](../../../../Docs/Performance/ThreePathReference.md#quality)
- [x] 4.3 复用 4.1 的缺色资源与恢复回放，配对独立 SurfaceAux/深度的读回、图读取及实际上传需求。验证只用等向基础材质时选择原角色和原几何、纹理配方，另外量化共同材质行的尺寸差额。比较匹配输入的现有角色及定向样点，复用覆盖相同风险的 T01 到 T03 检查。 [就绪设计](design.md#editing-and-routing) · [既有签收证据](../2026-11-28-add-sheen-material-profile/tasks.md#final-verification)

## 5. A/B 交接与整改

- [x] 5.1 Agent A 完成上节可观测检查，登记源码、包、实际命令、环境、输入身份、正常回执、原始结果、复用证据和未覆盖范围。依据待审改动在当前问题单推荐 B 档位并写明切线/镜像、方向性 IBL 和原生准入风险。核对工作区和交接输入摘要，留待维护者调度独立审查。 [当前问题单](../../../../Docs/CodeReview.md) · [审查档位](../../../../Docs/Developments/AgentModels.md#review-tier)
- [x] 5.2 Agent B 独立核对本轮源、正式消费者与登记证据，按风险重放必要的方向、路由或原生准入用例。登记实际档位、模型、推理级别、范围和问题，补齐本轮所涉完整第一方文件的注释。审查及注释结果记录到当前问题单和本 Change。 [独立审查闭环](../../../../AGENTS.md#review-loop)
- [x] 5.3 Agent A 按问题项整改并自检，针对改变的合同和实际输入更新同一问题单与证据。在有新增发现时继续同一闭环，登记逐项修复或无待修项的交接结论。 [整改流程](../../../../.agents/skills/physara-review/SKILL.md) · [首轮整改](#a-correction-verification)
- [x] 5.4 Agent B 独立复核修正及注释，确认待验收问题、实际签收版本和对应证据。以审查结论和签收输入身份可复验为完成条件。 [独立审查闭环](../../../../AGENTS.md#review-loop)

## 6. 签收后验证与归档

- [x] 6.1 Agent A 在 B 签收后执行最终 verify，按正式运行的真实设备核对实现、规格及任务。按签收输入重用可证明同一合同的结果，重放必要的原生交付。记录实际运行和复用的检查、范围、结果、设备、源、产品包及可复查日志，再严格验证 Change。 [项目验证合同](../../../../Docs/Architecture.md#tests) · [最终验证](#final-verification)
- [x] 6.2 同步七份增量到对应主规格，严格校验主规格和已完成任务，更新工作包状态、当前阶段引用及归档链接。验证归档内容完整并保留实际调度、审查签收及验收证据后，按模板重置当前问题单。 [开发计划](../../../../Docs/Developments/Developments.md#current-stage) · [当前问题单](../../../../Docs/CodeReview.md)

本地标样、脚本、原生回放、参考图和临时工程统一置于 `build/d12-t04/`。参考图使用同一相机、曝光、环境、分辨率与正常接受帧，配对保存数值读回和来源；深度及覆盖对照按配对 D32/G3 校验。连续 HDR 样点采用既有三路径工作色容差 `2⁻¹¹ + 0.025 × |参考值|`，对独立计算的方向和能量另外登记样点、输入精度及判据。先用最小受影响目标和已有回放，有独立缺口再补代表用例。达到工作包验收并能匹配当前输入后停止。 [验证范围](../../../../.agents/skills/physara-verification/SKILL.md) · [参考判据](../../../../Docs/Performance/ThreePathReference.md#quality)

<a id="a-local-verification"></a>
## A 本地验证

2026-11-28，基准 `c58ecb96326596600e2d21ac351b56cd8e4f222b`。正式 Debug 和 Release 构建各生成 93 份产品包，两个配置的 `Physara.exe --hidden --poll-limit=3` 均返回 0。早期并行 Release 的 CL 进程异常保留原日志，保持原编译选项的串行重试通过，最终构建采用已定额的缓存。原生回放使用 Intel RaptorLake-S Mobile Graphics Controller、OpenGL 4.6.0、驱动 32.0.101.6129。 [Debug 构建](../../../../build/d12-t04/product-Debug.log) · [Release 构建](../../../../build/d12-t04/product-Release.log) · [并行编译日志](../../../../build/d12-t04/product-Release-msvc.log) · [Debug 启动](../../../../build/d12-t04/startup-Debug-A.txt) · [Release 启动](../../../../build/d12-t04/startup-Release-A.txt)

- 正式 Asset 路径验证 glTF/GLB、有效采样设置、外部方向图版本、镜像角点和逐面法线。required 组合重导入被拒绝后，原 optional 版本保持可用。实际 Inspector、单项清除、Undo/Redo、v1 保存重开、错误纹理用途和非法基底 normal UV 的拒绝均通过。 [资产及文档回放](../../../../build/d12-t04/ani-assets-Release-A.log)
- 生成材质行 576 B，`anisotropy` 偏移 560 B，保留 t5，方向图使用 t13/s13。12 个新角色核对了实际阶段输入和原生准入，最重角色使用 11 个片元纹理单元，设备上限 32。 [接口与角色回放](../../../../build/d12-t04/ani-admission-Release-A.log)
- 真实 HDR 逐灯样点与独立双精度参考的最大相对误差为 0.0744%，包含斜轴非均匀缩放、镜像接缝、双面、独立 UV、法线贴图、B 衰减、IOR/specular 基底、零 RG 与退化投影。物理白炉白金属归一值为 0.999609，旋转环境和材质方向的样点与弯曲反射方向参考匹配。 [逐灯复算](../../../../build/d12-t04/direct-proof.json) · [逐灯参考图](../../../../build/d12-t04/ani-direct-regular-reference.png) · [环境复算](../../../../build/d12-t04/ibl-proof.json) · [环境参考图](../../../../build/d12-t04/ani-gradient-reference.png)
- 640×480 单样本固定输入的 Direct 与 Physical 各有 32,906 个扩展像素，在 Forward、Forward+ 和 Deferred 的 HDR 逐字节匹配。D32 与 10 层影图一致，孔内 G3=1，前景、等深标样及已有扩展的对应见证为 G3=4。显式零切换路由、Undo、历史失效以及 Inspector 诊断期间保留最后正常受光票据均通过。 [三路径输入与结果](../../../../build/d12-t04/route-proof.json) · [原生回放](../../../../build/d12-t04/ani-routes-Release-A.log)
- 冷包与未选热角色对照均定位到缺席的 `anisotropyImage`。独立 SurfaceAux 和恢复色前后 G1/G3/D32 逐字节匹配，前者只上传 4 B 占位纹理，颜色恢复再上传 16 B 方向图。普通核心与带休眠方向图的显式零强度对照同用 160 B 顶点流、一次图像写入和原有色角色，公共材质行增加 48 B。合法热候选发布后用新的 Deferred 票据确认实际颜色。 [冷热拒绝与已恢复提交](../../../../build/d12-t04/ani-fault-Release-A.log)
- 真实默认 20 MiB 对照在 157 项及 20,883,032 B 已用时拒绝新候选并保留活动代。单代实际 12,056,144 B，24 MiB 额度覆盖单代字节的两倍及必要余量。最终真实 SourceReload 发布两次完整候选，第二次记录共驻 177 项、24,056,772/25,165,824 B，其间设备检查和 Pipeline 创建沿原帧前配额逐帧推进。 [20 MiB 对照](../../../../build/d12-t04/ani-reload-default20.log) · [24 MiB 正常发布](../../../../build/d12-t04/ani-reload-Release-A.log)
- 当前 Release 库运行的八项原生模式全部返回 0。另核对 17 项已签收 T03 标样输入，同一逐灯绒光回放的 16 份 HDR/D32 文件与签收版本逐字节一致。现场范围限于记录的 OpenGL 设备与驱动。 [退出记录](../../../../build/d12-t04/native-exits-A.json) · [T03 对照](../../../../build/d12-t04/t03-regression.json) · [A 交接清单](../../../../build/d12-t04/handoff.json)

A 建议 B 采用 Y 档（GPT-6.2 Astra，`high`）。方向图与源切线、镜像/非均匀实例和 normal map 的空间语义需要跨 Asset、Scene 和 Shader 核对，绑定与整代发布可按已登记的独立检查逐项复验。 [当前交接](../../../../Docs/CodeReview.md) · [档位规则](../../../../Docs/Developments/AgentModels.md#review-tier)

<a id="a-correction-verification"></a>
## A 首轮整改

2026-11-28，基准 `c58ecb96326596600e2d21ac351b56cd8e4f222b`，保留 B 首审注释。Windows x64/MSVC 按正式 `cmake --build build --config <配置> --target Physara` 构建，Debug、Release 各交付 93 份产品包，生成的 StaticForward ABI 与首轮一致。两个配置的 `Physara.exe --hidden --poll-limit=3` 均返回 0。 [Debug 构建](../../../../build/d12-t04/review-a1/product-Debug.log) · [Release 构建](../../../../build/d12-t04/review-a1/product-Release.log) · [Debug 启动](../../../../build/d12-t04/review-a1/startup-Debug.txt) · [Release 启动](../../../../build/d12-t04/review-a1/startup-Release.txt)

- **CR-001** 使用同一几何、相机和光源对照旧包和整改包。着色法线的原始 NoV 约 −0.7985，几何半球的视向和入射余弦均约 0.3162。NoV 钳位后按原方位重建切向幅度。强度 0 和 0.001 的 FP16 HDR 读回一致，旧包对应红通道增幅约 66%。中间强度与正常朝向也符合独立双精度 Smith 参考，四点最大相对误差 0.092%。 [直接光回放](../../../../build/d12-t04/review-a1/review-direct-Release.log) · [复算与原始样点索引](../../../../build/d12-t04/review-a1/review-readback.json)
- **CR-002** 斜副切线与视向平行时，以双叉积和平方长度阈值 `1e-12` 识别投影或混合退化，恢复原着色法线。满强度时方向环境对单位环境的红通道比由 1.43142 恢复至 0.56829，与等向对照的整点 HDR 字节匹配。邻近 0.18 rad 投影样点实测 0.74795，几何参考为 0.74804。 [方向环境回放](../../../../build/d12-t04/review-a1/review-bend-Release.log) · [方向与采样值](../../../../build/d12-t04/review-a1/review-readback.json)
- **CR-003** 源材质在固定模型中保留导致可选 anisotropy 省略的冲突层，Inspector 按所选 Primitive 展示。摘要按原容量截断至 2048 字节，源材质 19 的清漆冲突和 21 的 sheen 冲突仍可辨认，同源文件的原生清漆材质 20 显示其清漆 profile。实际导入与实例回放再次检查 required 拒绝、旧版本保留及 v1 恢复。 [截断对照](../../../../build/d12-t04/review-a1/review-fallback-Release.log) · [导入及 v1 回放](../../../../build/d12-t04/review-a1/ani-assets-Release.log)
- **CR-004** 复用首轮 `ior-dielectric` 在 `NoV=0.58, p=0.48` 的 HDR。补跑 `NoV=0.075, p=0.9, IOR=4` 的各向异性介质，以及掠射时过滤后的 `p=0.045` 下限、`p=1` 等轴上限和白金属。基于 GGX 半角分布的独立双精度中点积分以 256×256 与 512×512 网格复算，按现有 128×128 表的 texel 中心、FP16 量化和双线性插值预测 DFG。主样点参考 R/G 为 0.0363281/0.8344385，预曝光为 0.0032552083，参考与实测的 HDR 最大绝对误差为 `4.46e-6`，小于逐通道容差 `8.60e-6` 至 `1.02e-5`。省略镜面补偿及使用未补偿反照率计算漫反射预算的对照各偏离实测 5.07 倍和 3.27 倍容差。白金属归一值 0.999609，等轴两组 HDR 匹配。 [积分与容差](../../../../build/d12-t04/review-a1/review-readback.json) · [白炉回放](../../../../build/d12-t04/review-a1/review-furnace-Release.log)

整改版用 `RunReview.ps1` 运行十项 Release 原生模式，`ReviewReadback.py` 和 `ReplayRoutes.py` 校验读回。Direct 与 Physical 在 Forward、Forward+、Deferred 各有 32,906 个扩展像素逐字节配对。当前六个路由帧对首轮冻结输入的 HDR、D32、十层影图及 Deferred 的 G1/G3 均逐字节匹配。实际 12 个各向异性色角色的原生准入通过，两次 SourceReload 发布的共驻峰值为 177 项、24,076,836/25,165,824 B，准备定额逐帧记录。 [回放入口](../../../../build/d12-t04/review-a1/RunReview.ps1) · [路由对照](../../../../build/d12-t04/review-a1/route-readback.log) · [角色准入](../../../../build/d12-t04/review-a1/ani-admission-Release.log) · [整代重载](../../../../build/d12-t04/review-a1/ani-reload-Release.log)

原生读回覆盖 Intel RaptorLake-S Mobile Graphics Controller、OpenGL 4.6.0、驱动 32.0.101.6129 的 Release。Debug 覆盖正式构建和隐藏启动。缺方向绑定的冷热同角色故障注入及 T03 回归沿用首轮对应输入的记录。具体输入、旧包对照、工具、正式产物与原始读回见整改版清单。待 B 独立复核代码、注释和证据。 [首轮 A 证据](#a-local-verification) · [整改版交接清单](../../../../build/d12-t04/review-a1/handoff.json) · [当前问题单](../../../../Docs/CodeReview.md) · [OpenSpec 严格校验](../../../../build/d12-t04/review-a1/openspec-validate.json)

<a id="b-correction-acceptance"></a>
## B 整改复核与签收

B 已签收基准 `c58ecb96326596600e2d21ac351b56cd8e4f222b` 上的整改版。工作区输入和正式产物以 A 整改交接清单固定，清单 SHA-256 为 `0a9f0245ff9041a82fc42879cfa67cf4f3d5470250f4d5a502fae1ed112479dc`。签收时 64 个输入均匹配，新增记录仅涉及本 tasks 和当前问题单。A 建议 Y 档，维护者指定独立 B 会话，实际档位、模型及推理级别待确认。 [A 整改清单](../../../../build/d12-t04/review-a1/handoff.json) · [逐项复核](../../../../build/d12-t04/final/review-signed.md)

| 问题 | B 独立确认 |
|---|---|
| CR-001 | 重放不同视向与入射方向的直接光，零强度与 0.001 的 HDR 一致，四个样点对参考的最大相对误差为 0.092%。 |
| CR-002 | 满强度斜轴平行时恢复原法线，方向环境红通道比为 0.56829；邻近方向实测 0.74795，预测 0.74804。 |
| CR-003 | 实际 Inspector 在模型摘要截断后，仍正确区分源材质 19 的清漆冲突、20 的原生清漆和 21 的 sheen 冲突。 |
| CR-004 | 独立重放掠射介质，按双精度积分核对原始 HDR，主样点最大绝对误差 4.46×10⁻⁶。两种错误能量公式均超出容差。 |

B 在 A 登记的 Intel OpenGL 4.6 设备与驱动上运行四个 Release 模式并复算。另核对 A 的三路径原始读回，两个模式各 32,906 个扩展像素精确匹配，核心像素均在容差内。四项问题及受影响的注释均已通过复核。构建、启动与其余原生模式使用已登记的 A 证据。最终验证、规格同步与归档由 A 执行。 [B 回放与复算](../../../../build/d12-t04/review-b1-y89jeyfy/verification.json)

<a id="final-verification"></a>
## A 最终验证

2026-11-28，在 `v2.0@c58ecb96326596600e2d21ac351b56cd8e4f222b` 核对 B 签收版本。64 个工作区条目与清单路径一致。交接清单共 571 条记录，569 条的字节与 SHA-256 匹配。两条变化对应 B 更新的 tasks 和问题单，CR-001 至 CR-004 的代码及注释均由 B 签收。B 实跑的四幅 HDR、D32 与样点坐标也与登记的 A 读回逐字节核对。 [签收输入核对](../../../../build/d12-t04/final/signature-verification.json) · [B 签收问题单](../../../../build/d12-t04/final/review-signed.md)

最终轮使用哈希已核对的 Debug、Release 程序，以及每个配置的 93 份正式 Shader 包。两种程序重新通过 `--hidden --poll-limit=3`。在隔离工作目录复制签收的 Release 探针、产品包及夹具，重放 `ani-routes`。Intel RaptorLake-S Mobile Graphics Controller、OpenGL 4.6.0、驱动 32.0.101.6129，640×480，单采样，关闭 VSync。六次正常提交的冻结输入及 Shader 身份匹配签收帧，HDR、D32、十层影图、样点坐标及 Deferred 的 G1/G3 均与签收读回逐字节一致。Direct 和 Physical 各有 32,906 个扩展像素在三条路径上精确匹配，核心像素差异均在登记的工作色容差内。 [运行入口](../../../../build/d12-t04/final/RunFinal.ps1) · [退出记录](../../../../build/d12-t04/final/native-exits.json) · [原生回放](../../../../build/d12-t04/final/ani-routes-Release.log) · [读回复算](../../../../build/d12-t04/final/native-verify.json)

| 能力与场景 | 实现及验收证据 |
|---|---|
| [静态 glTF 导入](specs/static-gltf-import/spec.md)（2 条需求、8 个场景） | 由 glTF 解析、基底 normal 与原始切线验证交付线性方向槽，required 拒绝及 optional 回退保留材质定位。glTF/GLB、源版本替换与丢失有效 UV 由 A 回放，截断摘要后的材质定位由 B 复验。既有扩展报告与核心回退沿用已验收规格。 [导入入口](../../../../Engine/Asset/GltfData.cpp) · [资产回放](../../../../build/d12-t04/review-a1/ani-assets-Release.log) · [B 复核](#b-correction-acceptance) |
| [实例编辑](specs/editor-domain-edits/spec.md)（1 条需求、2 个场景） | Scene 合成固定源和稀疏覆盖，Inspector 区分降级原因、请求强度和实际接受的颜色。两实例编辑、显式零、撤销重做、框架失败与诊断期间保留颜色身份由资产及路由回放覆盖。 [有效材质](../../../../Engine/Scene/SceneMaterial.cpp) · [Inspector](../../../../Editor/Panels/PhysicalInspector.cpp) · [路由回放](../../../../build/d12-t04/final/ani-routes-Release.log) |
| [场景 v1](specs/scene-document-persistence/spec.md)（1 条需求、2 个场景） | v1 入口保留显式零、弧度和稀疏方向槽，通过既有文档事务校验用途和有效 UV。已验证旧文档、保存重开和非法用途拒绝后保留活动文档。 [读取](../../../../Editor/Session/SceneDocumentRead.cpp) · [写出](../../../../Editor/Session/SceneDocumentWrite.cpp) · [恢复回放](../../../../build/d12-t04/review-a1/ani-assets-Release.log) |
| [共享表面](specs/static-surface-evaluation/spec.md)（2 条需求、3 个场景） | 方向取样独立使用有效 UV，世界轴沿原始 T/B 及实例线性变换建立，双轴 GGX 使用共享过滤粗糙度。旋转、镜像、双面、非均匀变换及零或退化方向有独立参考。B 重放 NoV 钳位处的连续性。 [表面框架](../../../../Shaders/Geometry/SurfaceFrame.glsl) · [Smith 求值](../../../../Shaders/BRDF/CoreBrdf.glsl) · [逐灯参考](../../../../build/d12-t04/direct-proof.json) |
| [环境照明](specs/environment-lighting/spec.md)（1 条需求、2 个场景） | 从垂直轴、原着色法线及共享粗糙度求 bent-reflection，沿同一 GGX 预滤波、DFG、SH 与预曝光合同。B 复验平行轴与邻近方向，及能区分补偿和漫反射预算的掠射介质白炉。 [环境求值](../../../../Shaders/Lighting/Environment.glsl) · [B 方向与白炉复算](../../../../build/d12-t04/review-b1-y89jeyfy/verification.json) |
| [三路径交付](specs/static-forward-rendering/spec.md)（2 条需求、4 个场景） | 全灯及 Froxel 共用波瓣，扩展 SurfaceAux 保持 D32、G1 和 G3=4，ForwardExtension 按等深合成。A 最终复验 MASK 孔、等深与三路径输出，独立表面、缺方向绑定与颜色恢复复用首轮证据。 [Draw 准备](../../../../Engine/Renderer/Draw/StaticDraw.cpp) · [最终三路读回](../../../../build/d12-t04/final/native-verify.json) · [缺绑定及恢复](../../../../build/d12-t04/ani-fault-Release-A.log) |
| [Shader 准入与重载](specs/shader-generation-reload/spec.md)（1 条需求、2 个场景） | 生成的材质行 576 B，12 个各向异性色角色按原生阶段资源准入。复用整改版两次真实 SourceReload 发布，第二次共驻为 177 项、24,076,836/25,165,824 B，复用首轮同 ABI 的冷包与未选热角色故障注入。 [准入实现](../../../../Engine/Renderer/Paths/ForwardShaderPreparation.cpp) · [角色回放](../../../../build/d12-t04/review-a1/ani-admission-Release.log) · [整代重载](../../../../build/d12-t04/review-a1/ani-reload-Release.log) |

七项能力的 10 条需求及 23 个场景都对应实现和实测或可复用证据。设计的固定源所有权、共享双轴波瓣、有限色角色与独立表面需求和签收实现一致。OpenSpec 1.13.2 严格校验通过。原生像素验收覆盖登记的 Release 设备配置，Debug 复验覆盖启动。 [设计合同](design.md) · [严格校验](../../../../build/d12-t04/final/openspec-change-validation.json)

<a id="archive-closeout"></a>
## 规格同步与归档

- 七份主规格已合并各向异性增量，新增八条需求、更新两条需求，保留已有场景。同步后的 45 份主规格与本变更均通过严格校验。 [同步摘要](../../../../build/d12-t04/final/spec-sync.json) · [主规格校验](../../../../build/d12-t04/final/openspec-sync-validation.json) · [变更校验](../../../../build/d12-t04/final/openspec-post-sync-change-validation.json)
- 2026-11-28 归档 `add-anisotropy-material-profile`，18 项任务全部勾选。归档后 45 份主规格严格校验通过，活动列表为空。开发计划登记 D12-T04 完成，下一工作包为 D12-T05。 [归档回执](../../../../build/d12-t04/final/openspec-archive.json) · [归档后主规格校验](../../../../build/d12-t04/final/openspec-main-validation.json) · [活动列表](../../../../build/d12-t04/final/openspec-post-archive-list.json) · [开发计划](../../../../Docs/Developments/Developments.md#current-stage)
- 签收问题单保留原始字节，SHA-256 为 `e5397c1e6352959ec4c8d838b0f4e59bc90a46b0b6b1229aa898a6d0c4837802`。归档路径、任务状态、链接、签收实现及留存记录经核对后，当前问题单按模板重置。 [签收问题单](../../../../build/d12-t04/final/review-signed.md) · [归档核对](../../../../build/d12-t04/final/archive-before-reset.json)
