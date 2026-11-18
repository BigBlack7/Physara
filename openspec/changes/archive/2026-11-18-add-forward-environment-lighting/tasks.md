# Tasks

## 1. 贯通可验证的 cubemap 消费

- [x] 1.1 A 为生成接口及 canonical IR 增加独立浮点 cube 类型，维度进入包活动反射和 ABI。用 V1 核对合法 cube 及既有二维消费者，在接口与 IR 维度不一致时得到明确诊断。同步生成接口说明。 [采样维度](design.md#sampled-cube)
- [x] 1.2 A 接入 GL 活动采样对、完整六面视图、跨面过滤和设备实际能力。用 V1 在真实 Draw 中区分方向及 mip，并检查代表性的 cube 维度错绑和缺面视图，核对 raw 与 Validation 的公开边界。同步 RHI 能力说明。
- [x] 1.3 A 经 Platform 提供已观察的 cube 能力，在 Runtime 图 profile 和 Pass 准备消费它，扩展上传以精确声明 face/mip。用 V1 检查图中写后采样，合并核对提交、子资源及目标范围。

## 2. 交付环境源与 v1 场景入口

- [x] 2.1 A 用现有固定 TinyEXR 接入环境解码，复用有界读取、作业准入、源身份和发布。用 V2 检查 ZIP/PIZ、线性超白与行序、Relative/文件声明标定、受控路径和一个实际解码拒绝。 [环境源与场景](design.md#source-and-scene)
- [x] 2.2 A 让 Scene、场景文档和 Open 候选持有类型化环境请求，其中像素版本来自实际源。用 V2 核对旧 v1、环境 round-trip、当前源解析及非法环境拒绝，保有旧文档。随任务记录受支持的场景和环境输入。

## 3. 生成可缓存的环境与 DFG

- [x] 3.1 A 以固定角色和版本化烘焙设置准备基础 cube、GGX mips 和 SH9。接入有界 CPU 作业、组内共用数据、请求合并、取消及关闭。用 V3 核对解析方向样点、粗糙层及归一化，通过相同源/设置复用和新源重取验证缓存身份。 [环境 profile](design.md#bake-profile)
- [x] 3.2 A 生成独立于源内容的固定 DFG，匹配 T01 的 Smith/GGX/Schlick 和固定 Filament 通道。用 V3 核对半精度编码、正有限 G 与两个独立方向参考，由 V4 检查实际采样方向和使用。 [DFG profile](design.md#dfg-profile)

## 4. 从正式 Forward 交付物理 HDR

- [x] 4.1 A 在帧准备及图中交付活动角色、匹配 DFG、SH9 和完整 cube 范围，以可靠接受确认本次资源代。用 V4 核对活动布局和实际写后读，用 V5 证明未确认候选与旧代保活。 [接受协议](design.md#acceptance)
- [x] 4.2 A 通过同一颜色源和活动布局接入 Physical Lighting，将直接光和 IBL 接到共享表面与 DFG，复用当前预曝光和材质 AO。用 V4 检查独立 HDR 参考、旧直接光/Unlit 对照及无环境物理结果。 [物理 Forward](design.md#physical-forward)
- [x] 4.3 A 在不透明图之后合成基础天空，只覆盖有效图像矩形内未被有效几何占据的位置。用 V4 检查有限远平面、MASK、两种深度组合及天空独立用途。
- [x] 4.4 A 在 View Settings 交付模式、请求环境、等待或拒绝原因及最后实际使用的环境。用 V5 通过正常编辑器入口核对匹配帧确认、旧图标识及新代恢复。更新构建运行说明中的实际 EXR、场景和视口入口。

## 5. A 本地核对与交接

- [x] 5.1 A 按下表留存 V1–V5 中实际运行或复用的证据、限制及输入身份。核对 D07-T04 三项目标与七份增量，执行 OpenSpec 严格校验及相关链接检查。登记基准、实际 diff、未跟踪输入、产生的程序/包及原生运行版本，在现有问题单准备交接，由维护者调度 B。 [开发约定](../../../../AGENTS.md#review-loop) · [本轮签收快照](../../../../build/d07-t04/final/review-signed.md)

## 6. 独立审查、整改与复核

- [x] 6.1 B 独立核对实际受审输入及直接消费者，检查 EXR 与文档边界、烘焙/DFG 配对、图范围、采样与接受。通读本轮完整第一方文件，补齐职责、单位、接口和流程注释，在现有问题单按 CR 编号登记覆盖与证据。初审已登记 CR-001 至 CR-009，交 A 整改。 [本轮签收快照](../../../../build/d07-t04/final/review-signed.md) · [审查技能](../../../../.agents/skills/physara-review/SKILL.md)
- [x] 6.2 A 已按 CR-001 至 CR-009 整改并通过定向自检，记录回交版本、原生重放和逐项结果。 [第 1 轮整改证据](#a-round-1-correction)
- [x] 6.3 B 已复核 CR-001 至 CR-009 的实际修正、定向回放及受影响注释，全部通过。 [B 最终签收与输入身份](#b-round-1-acceptance)

## 7. 最终验证、同步与归档

- [x] 7.1 A 已核对 B 签收源码、实际实现、七份增量及验收证据。用签收版本重建 Debug、Release，复验 V1–V5 和 CR-001 至 CR-009 对应边界，通过 OpenSpec 严格校验。 [最终验证](#final-verification)
- [x] 7.2 A 已留存 CR、整改、注释覆盖与签收证据，同步七份增量并通过主规范严格校验。 [规范同步](#spec-sync)
- [x] 7.3 A 已完成原生归档，核对归档引用和开发计划状态，并按模板恢复当前问题单。 [归档收尾](#archive-closeout)

<a id="verification"></a>
## 验证范围与停止条件

开发标样、探针和回放材料放在 `build/d07-t04/`。产品验证用独立构建树，按现有 exe 相对路径放置标样的 `Assets/Scenes` 和 `Assets/Environments` 及匹配 Shader 包。共用受光面、MASK、天空和版本交换标样。T01 的解析值及 T03 的回放方式可作参考，已有结果只有输入和环境匹配时才计入证据。GPU 比较从可靠提交完成后的显示前附件取值，并记录实际纹理格式。 [构建入口](../../../../Docs/Build.md) · [验证范围](../../../../.agents/skills/physara-verification/SKILL.md) · [T01 数值参考](../2026-11-17-add-core-surface-brdf/tasks.md#verification) · [T03 最终证据](../2026-11-18-add-forward-direct-lighting/tasks.md#final-verification)

| 检查 | 输入与判据 |
| --- | --- |
| V1 · ABI 与实际 cube 采样 | 用现有 ShaderBuild 和正式 GL 图形路径编译并加载匹配包。采用一张六面、两个可区分 mip、带非对称方向标记的 cube，通过图逐子资源上传后读回面中心、指定 LOD 和一对跨面样点。保留一个二维采样对照。用同一探针检查二维视图在 cube slot 的拒绝、一个缺面视图及接口/IR 维度不匹配。生成 ABI、发出 Draw 或拒绝诊断与原生像素分别记录。 |
| V2 · 源与文档边界 | 从仓内各取一个 ZIP/PIZ 源验证支持路径，用一份小型已知顶部行与超白像素的 EXR 核对线性值和 alpha，使用其中的合法标定元数据对照仓内源的 Relative。核对当前输入哈希、旧 v1、省略与环境请求的保存重开。代表性非法输入覆盖资源根越界与一个可触发的格式或预算拒绝，确认失败时旧活动文档有效。在 Release 复核改变的输入拒绝。 |
| V3 · 冷路径解析参考 | 用一份线性环境同时含常量色通道和 `L(d)=1+0.5d_x` 的方向通道。检查基础 cube 与 SH9，SH 对 +X 的方向通道预期为 4/3。粗糙度最大时固定积分的预滤波方向样点应符合按余弦归一的同一独立参考，用端点和基础样点差异区分普通缩小链。复用环境值做同源查询复用与改源更新。用 `p=1、NoV=1` 的 `G=1−ln(2)` 校准独立积分，再比较正式 DFG 两个预选样点的实际网格值或过滤预期，包含一个内部非对称混合输入。对 DFG R/G 与方向通道使用绝对误差 0.01，另按实际半精度交付检查正有限 G。 |
| V4 · 正式工作色及天空 | 用正式 Forward 包和固定场景在 GL 读取 HDR 及 D32。用均匀单位环境检查白色金属约为 1，固定直接光对照同一 DFG 补偿。用一个非对称环境核对朝向、环境强度、自发光/直接光和 AO 隔离，验证 +1 EV 在可表示区间翻倍。梯度样点、固定画幅边缘、MASK 孔与有限远平面在两种深度组合中核对覆盖。用已复用的场景核对无环境、独立天空、Unlit 和一次旧 Direct Lighting。工作色参考按实际量化与上游积分误差设定，白炉和曝光对照采用 2% 容差，D32 与覆盖按明确采样点核对。 |
| V5 · 正式入口与确认 | 在编辑器打开 A 并接受 Physical Lighting，再打开 B。用本次实际的预计算 Pending 或可重试的图上传准入等待观察旧图、状态和当前图输入映射。保留一次实际新增上传尚未确认的尝试，核对不提前发布任何子资源，下一次完整正常帧才同步确认 B、相机、曝光和用途。使用匹配的提交与完成记录保存原生证据。 |

阶段内先完成各组的对应检查。所有新增可观察合同都有当前输入证据、代表性回退和未覆盖范围已记录时收束，只因失败、输入变化或明确缺口扩大范围。A 的本地证据和 B 的独立签收分别记录。

<a id="a-local-evidence"></a>
## A 本地验证

- V1 使用 Windows x64、MSVC 19.44.35228.0、SDK 10.0.26100.0 的独立 Ninja Multi-Config Debug 构建。正式 `Physara`、现有 Shader 包和短启动已通过。Ninja 记录了 Runtime 对 DeviceHost 头的实际依赖。 [构建日志](../../../../build/d07-t04/checked-build.log) · [回放入口](../../../../build/d07-t04/v1/Verify.ps1)
- V1 在 Intel RaptorLake-S Mobile Graphics Controller、GL 4.6.0 Build 32.0.101.6129 读取实际写后采样。raw 和 Validation 都各提交 13 笔存活上传，通过六面、两层 mip、跨面线性值和二维对照。两条路径均拒绝缺面视图及活动 cube 位置的二维绑定。包构建拒绝同槽 `expected kind=texturecube, actual kind=texture2d`。 [原生记录](../../../../build/d07-t04/v1/run.log) · [IR 拒绝](../../../../build/d07-t04/v1/mismatch.log)
- V2 在 Debug 与 Release 通过仓内 ZIP/PIZ 源、合成 EXR 的线性超白及标定、旧 v1、保存重开及当前源刷新。将已缓存的标样的单位白由 3.25 换到 7.5 后，重开读取实际新源，旧读者仍固定原值。非 2:1 与越界路径均拒绝且保留活动文档。 [回放入口](../../../../build/d07-t04/v2/Verify.ps1) · [Debug](../../../../build/d07-t04/v2/run-Debug.log) · [Release](../../../../build/d07-t04/v2/run-Release.log)
- V3 以 Release CPU worker 对解析环境完成相同源复用、不同源重新烘焙及天空基础层复用。方向通道 +X 基础层、最高粗糙层与 SH9 分别为 1.499699、1.331284、1.333200，常量超白通道保持 2。两个 DFG 样点的 G 为 0.312012 和 0.812500，独立半球参考为 0.311532 和 0.813978，R/G 在约定容差内。 [回放入口](../../../../build/d07-t04/v3/Verify.ps1) · [数值记录](../../../../build/d07-t04/v3/run.log)
- V4 在当前正式包和 GL 图中读回预曝光 HDR 与 D32。在 0.104167 的预曝光下，远平面白金属、带 AO 的直接光加自发光、MASK 孔天空分别读到 0.236450、0.132202、0.104126，远平面深度为 0。Unlit 为 0.25，留边为 0.017990。两种不透明 Pass 组合的样点一致。旋转后绿色天空与镜面样点为 0.078064、0.140991，此次同时将环境强度设为 0.5。还核对了 +1 EV、无环境的 DFG 物理光和旧直接光。 [原生读回](../../../../build/d07-t04/v4/run.log)
- V5 由正式 EditorSession 文件入口及 Runtime 图推进独立天空场景。B 等待时安全 UI 继续读取 A 的颜色代 2，图像输入映射不生效。开发见证在 B 的 HDR 生产后取消，保有实际 6 笔纹理写入和提交票据 10，接受记录仍是 A。下一完整正常帧确认 B 的颜色代 3 及其环境、相机和曝光。读回显示 R 为 126，透视天空的上下蓝通道为 131、121。 [正式路径回放](../../../../build/d07-t04/v4/Verify.ps1) · [接受与读回](../../../../build/d07-t04/v4/run.log)

2026-11-18 从仓库根执行 `& ./build/d07-t04/Verify.ps1`。本机 Debug/Release 正式目标和短启动、V1–V5 全部通过，核对前后相同的 1235 项正式输入及 14 项探针输入，正式输入清单 SHA-256 为 `0d63fd38c4e8c2f620f2bc3864e7276be90ec0dadad0e88cfcc286b93e484bdc`。包、exe 与读回日志的摘要在交接记录。原生证据覆盖单一 Intel OpenGL 驱动。开发标样留在本地 build 目录。 [统一回放](../../../../build/d07-t04/Verify.ps1) · [运行日志](../../../../build/d07-t04/a-local.log) · [交接清单](../../../../build/d07-t04/handoff.json)

<a id="a-round-1-correction"></a>
## 第 1 轮 A 整改与自检

2026-11-18 核对 B 交回版本的全部 1,235 项正式输入，清单 SHA-256 为 `51b54cf1821f8e5d518cb3c48c3e9286badca0cce7d05488fc974b2fc4ba092d`。基准 commit 为 `8914f0aa2f5466a7298e019a20b10a6b2ad57b7a`。本轮修正 20 个正式输入文件。用独立 UTF-8 Ninja Multi-Config 树构建 Debug、Release，并以相同输入清单完成正式程序短启动和定向回放。回放前后正式输入与探针字节一致，回交清单 SHA-256 为 `77e177cbf84cdd756bc1e83481c80f57ebee83763b0cc2ee13b5b0f619aa9bf1`。清单索引实际变更、包、链接库、可执行文件及探针日志。 [B 交回快照](../../../../build/d07-t04/a-correction-1/b-source-manifest.json) · [本轮回交清单](../../../../build/d07-t04/a-correction-1/handoff.json)

| 对应问题 | 实际核对 |
| --- | --- |
| CR-001、CR-002 | B 的升降序 EXR 逐样本一致，R 顶行 2、底行 7。208 字节的合法标样成功，207 字节和计数不符分别得到预算及格式拒绝，刷新失败仍保有固定旧版。V2 Release 的 ZIP/PIZ、标定和 v1 重开通过。 [EXR 定向](../../../../build/d07-t04/a-correction-1/checks/ExrRows.log) · [V2](../../../../build/d07-t04/a-correction-1/v2/run-Release.log) |
| CR-003、CR-004 | 同 ABI 的正确 IBL 包可建立实际 GL 管线，错误 Physical 包在加载时按活动角色拒绝并保持旧接受色代。SH 的极端亮度差样点恢复绿色 1。V4 使用重建包，原生 HDR、D32、旋转及 +1 EV 对照通过。 [管线角色](../../../../build/d07-t04/a-correction-1/checks/PackageRole.log) · [SH 数值](../../../../build/d07-t04/a-correction-1/checks/ShScale.log) · [V4](../../../../build/d07-t04/a-correction-1/v4/run-normal.log) |
| CR-005、CR-009 | RGBA32_FLOAT 的 cube 能力与创建结果同为支持。D32 的 cube 采样能力及六面 SRV 都被拒绝，仍保有存储和读取能力。V1 Debug 的 raw/Validation 正常读回和错误诊断通过，日志包含逻辑 set/slot、五面范围和预期与实际维度。 [目标能力](../../../../build/d07-t04/a-correction-1/checks/PackageRole.log) · [V1](../../../../build/d07-t04/a-correction-1/v1/run.log) |
| CR-006、CR-007、CR-008 | V5 真实编辑器回放同路径换代。等待期间旧图和旧代摘要保留，叠字说明等待和旧图，帧准备 Error 数不增加，输入映射关闭。新代成功后摘要一致。开发探针为实际队列构造边界注入一次启动失败并记录 UI 文本，旧图继续显示，随后可启动 worker 并接受正常帧。 [原生 V5](../../../../build/d07-t04/a-correction-1/v4/run-normal.log) · [启动失败和 UI 观测](../../../../build/d07-t04/a-correction-1/v4/run-trace.log) |

原生证据覆盖 Intel RaptorLake-S Mobile Graphics Controller、OpenGL 4.6.0 Build 32.0.101.6129。回放由 `build/d07-t04/a-correction-1/Verify.ps1` 进入，正式 Debug/Release 构建、短启动、V1/V2/V4/V5 和新增定向通过。本段为 A 整改自检。回交后由 B 复核修正及受影响的注释。 [回放入口](../../../../build/d07-t04/a-correction-1/Verify.ps1) · [整轮日志](../../../../build/d07-t04/a-correction-1/a-local.log)

<a id="b-round-1-acceptance"></a>
## 第 1 轮 B 复核与签收

2026-11-18，B 复核通过 CR-001 至 CR-009，代码与注释均已签收。基准提交为 `8914f0aa2f5466a7298e019a20b10a6b2ad57b7a`，分支 `v2.0`，暂存区为空。B 独立复算 A 回交清单 `77e177cbf84cdd756bc1e83481c80f57ebee83763b0cc2ee13b5b0f619aa9bf1`，其中 1,235 个正式输入、32 个验证输入和 61 个产物及日志逐项匹配。相对初审交回版的 20 个正式文件已按实际差异复核。[A 回交清单](../../../../build/d07-t04/a-correction-1/handoff.json)

| 问题 | B 的定向证据 |
|---|---|
| CR-001、002 | 两份 EXR 的全部 RGB 样本一致，顶、底行 R 分别为 2、7。精确预算 208 字节通过，207 字节被拒，块数不符的刷新保留旧 CPU 版。[解码与刷新](../../../../build/d07-t04/b-recheck-1/exr.log) |
| CR-003、005、009 | 正确 IBL 包成功建管线，同 ABI 的 Physical 包作为 IBL 时被拒。RGBA32_FLOAT 与 D32 cube 的采样能力分别与可创建、被拒绝的 SRV 对应。缺面及维度错绑都报告 set/slot 和实际形状。B 在 Debug/Release 的 raw/Validation 新录制内尝试错绑后的 Draw，四组原生图元计数均为 0。[包与能力](../../../../build/d07-t04/b-recheck-1/package-role.log) · [Debug](../../../../build/d07-t04/b-recheck-1/cube-draw-debug.log) · [Release](../../../../build/d07-t04/b-recheck-1/cube-draw-release.log) |
| CR-004 | 常量环境 RGB 为 `(2^90, 2^-90, 0)`，强度为 `2^90`。CPU 打包后绿色还原为 1。两块正对相机的介质表面分别使用绿色、黑色基色，无直接光和自发光。RGBA16F 的 G 实测 0.104248047、0.001418114，相减消除镜面项，再除以预曝光 0.104166664，漫反射为 0.987167，独立半球积分为 0.987694。[常量](../../../../build/d07-t04/b-recheck-1/sh-constants.log) · [工作色读回](../../../../build/d07-t04/b-recheck-1/weak-diffuse.log) |
| CR-006、007、008 | 同路径等待时，界面区分请求源代 2 与旧图源代 1 的指纹和旋转，Error 计数未增加，旧图输入映射关闭。部分上传的 6 次纹理写入随票据 9 提交，色代仍为 3，完整帧才接受色代 4。首次启动失败在开发替身中被转为一次环境拒绝，编辑器保留旧图并成功重试。[视口及启动失败回放](../../../../build/d07-t04/b-recheck-1/viewport.log) |

最终正式输入清单 SHA-256 为 **`bad09668b38815bc6d8200a34f0beb4a3a2b16e67ead70bf2c7d2ea2e47e8cb2`**。与 A 的验证版本相比，仅 6 个 C++ 文件的注释变化。B 逐文件核对了代码、预处理指令与宏续行，Shader 源、schema 及重放所用包字节一致。最终注释说明了 EXR 偏移表额度、作业启动失败、按片元活动资源核对同 ABI 角色，以及挂接旧图时的当前原因。[B 签收清单与证据身份](../../../../build/d07-t04/b-recheck-1/handoff.json)

原生实测覆盖 Intel RaptorLake-S Mobile Graphics Controller，GL 4.6.0 Build 32.0.101.6129。正式 Debug/Release 构建、短启动、仓内 ZIP/PIZ 和常规 HDR/D32 回放沿用 A 整改记录。本轮 B 未重跑完整 V3，积分实现沿用初审证据。冷加载时重复准备静态资源为非阻塞观察，未测量帧耗时。任务 7.1–7.3 由 A 按签收版本继续执行。

<a id="final-verification"></a>
## 最终验证与证据

2026-11-18，A 对 B 签收的 1,235 项正式输入逐项复算，清单 SHA-256 为 `bad09668b38815bc6d8200a34f0beb4a3a2b16e67ead70bf2c7d2ea2e47e8cb2`。按原始字节留存完整问题单、九项独立结论和注释覆盖，快照 SHA-256 为 `17bb021fdd2c40f8000dc95a11a8c853346dc81586f3e5e8931baf9ad75a44a3`，快照中的历史相对链接以 `Docs/` 为基准。 [签收问题单快照](../../../../build/d07-t04/final/review-signed.md) · [源码清单](../../../../build/d07-t04/final/source-manifest.json)

在独立 UTF-8 Ninja Multi-Config 树以 Windows x64、MSVC 19.44.35228.0 和 Windows SDK 10.0.26100.0 从零构建 Debug、Release 的 `Physara`。两个正式 exe 均以 `--hidden --poll-limit=4` 启动并以 0 退出。对回放前后的正式输入、43 项验证输入和实际使用的 exe、库、Shader 包进行核对，证据清单 SHA-256 为 `15a8c349fa848ef62f6365d1878c9d4d1e6411266818f8a3665cf429c764259c`。 [Debug 构建](../../../../build/d07-t04/final/fresh-product-Debug.log) · [Release 构建](../../../../build/d07-t04/final/fresh-product-Release.log) · [原生回放入口](../../../../build/d07-t04/final/Verify.ps1) · [证据清单](../../../../build/d07-t04/final/handoff.json)

两个配置的 14 个新包均通过包内摘要检查。逐字段核对，ABI、接口声明、活动反射、依赖和 SPIR-V 载荷与 B 签收对应包一致。复算 CMake 的 299 项指纹输入，只有 B 签收时补注释的 `RhiProgram.hpp` 字节变化，按现有规则派生新的编译器指纹、内容身份和包摘要。正式启动及探针使用本次重建的匹配包。 [包与指纹复算](../../../../build/d07-t04/final/package-identity.json) · [指纹定义](../../../../Engine/Shader/CMakeLists.txt)

| 主规范能力 | 实现及验收场景核对 |
| --- | --- |
| Shader 包及 OpenGL 管线 | V1 重新生成接口，核对六面、两级 mip、跨面采样、二维对照和 IR 维度拒绝。实测 raw/Validation 每组完成 13 笔子资源写入。B 的定向探针用最终库重放，Debug/Release 的 raw/Validation 四组在二维视图错绑后均生成 0 个原生图元。角色错包和 D32 cube SRV 拒绝通过。 [V1](../../../../build/d07-t04/final/v1/run.log) · [包与能力](../../../../build/d07-t04/final/checks/PackageRole.log) · [Debug 错绑](../../../../build/d07-t04/final/b-native/cube-draw-Debug.log) · [Release 错绑](../../../../build/d07-t04/final/b-native/cube-draw-Release.log) |
| CPU 资产发布及 v1 场景 | V2 以 Release 重放仓内 ZIP/PIZ、线性超白、标定、旧 v1、保存和重开，以及当前源更新和越界拒绝。定向对比两种行序，顶部/底部 R 为 2/7，208 字节预算通过，207 字节及错误块数被拒且旧 CPU 版本仍有效。沿用已验的 PNG 用途场景，审阅描述与 EXR 拒绝入口。 [V2](../../../../build/d07-t04/final/v2/run-Release.log) · [EXR 行序与预算](../../../../build/d07-t04/final/checks/ExrRows.log) · [解码入口](../../../../Engine/Asset/EnvironmentDecode.cpp) |
| 环境准备、DFG 与 IBL | V3 复验同源复用、换源重烘焙、天空层、GGX 端点与 SH9。SH 的 +X 样点为 1.333200，两个 DFG 网格的 R/G 均在独立积分容差内。V4 核对白炉、预曝光翻倍、旋转和强度、AO 及直接光对照。CR-004 在正式 RGBA16F 路径读回的线性漫反射 G 为 0.987167358，独立参考为 0.987693616。AO 缺省和查询空间变换同时核对实际 Shader。 [V3 参考](../../../../build/d07-t04/final/v3/run.log) · [HDR 漫反射](../../../../build/d07-t04/final/b-native/weak-diffuse.log) · [环境求值](../../../../Shaders/Lighting/Environment.glsl) |
| Forward 工作色及天空 | V4 在事件查询完成后读取 RGBA16F 和 D32。两种不透明 Pass 组合对远平面深度 0、MASK 孔天空、留边及 Unlit 的实际样点一致。复验无环境 Physical Lighting 与旧 Direct Lighting，其他既有诊断场景沿用项目验收证据并核对同一诊断分支与存储键。 [V4 读回](../../../../build/d07-t04/final/v4/run-normal.log) · [着色入口](../../../../Shaders/Passes/Forward/Color.frag) · [既有诊断覆盖](../2026-11-18-add-forward-direct-lighting/tasks.md#final-verification) |
| 视口确认与完整资源代 | V5 从正式编辑器入口加载独立天空及同路径新源。常规回放的部分候选记录了 6 次实际纹理写入和提交票据 11，保留旧色代，完整帧确认新代、用途和曝光。定向回放以开发替身在 worker 启动边界注入失败，核对等待及失败提示、旧图输入映射、保留旧代和重试接受。 [正常回放](../../../../build/d07-t04/final/v4/run-normal.log) · [定向回放](../../../../build/d07-t04/final/v4/run-trace.log) · [帧提交入口](../../../../Runtime/Frame.cpp) |

七份增量的 13 项要求、31 个场景均已对照实现、设计及验收材料核对。V1–V5 原生复验及 OpenSpec 1.13.2 严格变更校验通过。图形运行证据覆盖单台 Intel RaptorLake-S Mobile Graphics Controller，OpenGL 4.6.0，驱动 32.0.101.6129。集成回放使用 Release，cube 错绑在 Debug/Release 复验。冷加载重复准备的观察未做性能测量。 [完整原生日志](../../../../build/d07-t04/final/native.log) · [严格变更校验](../../../../build/d07-t04/final/validate-change.json)

<a id="spec-sync"></a>
## 规范同步

新建 `environment-lighting` 主规范，并将其他六份增量合入原有能力。逐项核对 9 项新增、4 项修改及其 31 个场景。六份既有主规范保留原用途、未涉及要求和全部既有场景名称，更新后的要求与增量逐块一致。OpenSpec 严格校验通过全部 33 份主规范。 [同步清单](../../../../build/d07-t04/final/spec-sync.json) · [严格校验](../../../../build/d07-t04/final/validate-specs.json)

[环境照明](../../../specs/environment-lighting/spec.md) · [CPU 资产发布](../../../specs/cpu-asset-publication/spec.md) · [场景文档](../../../specs/scene-document-persistence/spec.md) · [Shader 构建](../../../specs/shader-build/spec.md) · [OpenGL 管线](../../../specs/opengl-shader-pipelines/spec.md) · [Forward](../../../specs/static-forward-rendering/spec.md) · [编辑器视口](../../../specs/editor-viewport-output/spec.md)

<a id="archive-closeout"></a>
## 归档收尾

2026-11-18，主规范同步后由 OpenSpec 将 `add-forward-environment-lighting` 归档至 `2026-11-18-add-forward-environment-lighting`。归档保留 `.openspec.yaml` 和七份增量，已核对归档及开发计划中的本地引用。D07-T04 三项工作包任务已标为验收完成，下一工作包为 D07-T05。签收证据留存后，当前问题单逐字节恢复为模板。 [归档结果](../../../../build/d07-t04/final/archive.json) · [开发计划](../../../../Docs/Developments/Developments.md#current-stage) · [当前问题单](../../../../Docs/CodeReview.md)
