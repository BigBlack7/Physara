# Tasks

## 1. 封存 View 请求与两套像素范围

- [x] 1.1 A 在 View 增加类型化 None/FXAA 请求、变更代和 v1 读写，接入现有文档快照与编辑控制。先按 V5 检查缺省 None、保存待确认 FXAA 后重开、一个非法字段拒绝且活动文档保持有效。
- [x] 1.2 A 在现有布局批次提供渲染及显示范围，使相机、HDR/D32 和输入映射消费各自的有效矩形。用 V3 核对取整、固定画幅、半开边界、比例 1 的原布局和一个实际不支持的尺寸；记录诊断纹理继续使用其缩放尺寸。 [两套布局](design.md#image-layout)

## 2. 交付固定显示 profile

- [x] 2.1 A 按固定 Filament 色调算子、成对原色转换、色域处理和 shaper 生成有界半精度 LUT。保留必要的上游声明和来源，记录实际移植范围。用 V1 的独立上游参考检查节点、黑点、HDR 边界和非网格插值。
- [x] 2.2 A 让正式 ShaderBuild 生成显示包，沿图上传和接受确认交付设备级 LUT。用 V1 核对活动绑定、图写后读、首次匹配接受和后续复用；在原生颜色路径读取固定色块并与 CPU 查表及目标存储编码对照。 [固定 profile](design.md#fixed-profile)

## 3. 组装 FXAA、缩放及 HDR 比较出口

- [x] 3.1 A 实现固定感知亮度的单样本 FXAA，根据实际请求在图中消费显示变换后的 HDR 映射结果。用 V3 的倾斜边缘、平坦色块和留边核对 None 与 FXAA，并检查有效矩形的取样范围。同步移植与 Shader 接口说明。
- [x] 3.2 A 接通双线性输出缩放和实际 ViewOutput，保有配对的显示前工作色、深度及快照。用 V3 核对采样顺序、大小和显示编码，按 V2 检查同一曝光下 HDR 在 None/FXAA 间一致以及 +1 EV 的 HDR 线性关系。 [显示流水线](design.md#fxaa-and-output)

## 4. 接通编辑器与最终颜色出口

- [x] 4.1 A 在 View Settings 提供请求和有效 AA、profile、实际大小及失败原因，通过既有正常帧确认和安全 UI 保留旧代。用 V5 检查同尺寸切换、实际准备拒绝、恢复和诊断预览。更新 v1 选项与 View 设置入口的使用说明。 [帧确认](design.md#accepted-settings)
- [x] 4.2 A 用正式 Forward、UI 和 Platform 链完成 V2/V4 的颜色纹理、线性数据、自发光、高亮、UI 色块和窗口编码读回，记录显示与 HDR 比较入口及实际生成包。 [颜色合同](../../../../Docs/Architecture.md#conventions)

## 5. A 本地集成检查与交接

- [x] 5.1 A 检查 D07-T05 三项开发目标与五份增量的覆盖，构建实际消费者，运行受影响的 V1–V5 或引用匹配输入证据。执行 OpenSpec 严格校验和链接检查。记录基准、实际脏/未跟踪输入、包、exe、驱动、运行及未覆盖范围；在现有问题单准备独立 B 交接。 [当前问题单](../../../../Docs/CodeReview.md)

## 6. 独立审查、整改和复核

- [x] 6.1 B 独立核对实际输入及消费者，复查 ACES/色域适配、FXAA、两套布局、图数据/寿命、v1 与有效状态；对本轮涉及的完整第一方文件按规范补注释。在现有问题单记录覆盖、证据与 CR 项。
- [x] 6.2 A 已整改并复验 CR-001 至 CR-008，登记当前输入及定向证据，交回 B 独立复核。 [第 1 轮整改与自检](#a-round-1-correction) · [CR-007 补充整改](#a-round-1-followup) · [CR-008 补充整改](#a-cr008-correction)
- [x] 6.3 B 已独立核对签收源码、产物与回放身份，复核修正和注释；CR-001 至 CR-008 全部关闭，交 A 最终验证。 [B 独立签收](#b-cr008-acceptance) · [审查闭环](../../../../AGENTS.md#review-loop)

## 7. 最终验证、同步与归档

- [x] 7.1 A 已对 B 签收版完成最终验证，核对源码、重新生成的包与 exe、标样、真实提交和回放结果。 [最终验证](#final-verification)
- [x] 7.2 A 已留存 CR、注释覆盖及最终证据，同步五份规范增量并通过主规范严格校验。 [规范同步](#spec-sync)
- [x] 7.3 A 已完成原生归档，核对开发计划及引用，并按模板恢复当前问题单。 [归档收尾](#archive-closeout)

<a id="verification"></a>
## 验证范围与停止条件

复用 D07-T02 的曝光参考、D07-T04 的正式 Forward 读回与独立环境场景、既有 UI 像素和窗口呈现检查。匹配的旧证据按实际输入和运行版本引用。新标样、探针和可选的上游独立参考程序放在 `build/d07-t05/`，由实际产品构建产生匹配接口和 Shader 包。读取 GPU 颜色前取得真实完成，记录尺寸、格式、View 接受状态和设备/驱动。 [T02 曝光](../2026-11-17-add-physical-camera-exposure/tasks.md#final-verification) · [T04 HDR](../2026-11-18-add-forward-environment-lighting/tasks.md#final-verification) · [验证约定](../../../../.agents/skills/physara-verification/SKILL.md)

| 检查 | 风险、最小检查和判据 |
| --- | --- |
| V1 · 固定 profile 和实际采样 | 防止算子、矩阵方向、输出域或 LUT 轴偏差。使用固定上游的直接 CPU 求值作为独立参考，选择黑、中灰、超白阶梯、暗部非网格色和两个不同方向的饱和色。核对黑为零、有效区间的高亮区分、有限范围和分量顺序，并在查表上端内外取相邻样点核对边界。选定样点的半精度三线性结果与直接参考逐通道绝对差 ≤0.02（display-linear）；GPU 读回的 SRGBA8 码与同一 LUT 参考的正确编码差 ≤2 个码值。复用同一批检查核对首笔 LUT 上传进入采样、接受后下帧无同一 LUT 的重复上传。 |
| V2 · 生产 HDR 与材质色域 | 防止输入域混用或重复施加曝光。复用小型受光/自发光场景，在诊断画面分别检查 sRGB 颜色纹理与线性材质通道的 8 位中间值。例如码值 128 在 base color 解码到约 0.21586，在线性数据为约 0.50196。用实际材质和固定相机取得 Physara 的超白 RGBA16F，检查 +1 EV 在可表示区间翻倍（2% 容差），同一曝光的 None/FXAA 对应 HDR 样点一致。选取片内平坦高亮用 V1 的参考映射所读 HDR；旧诊断限幅用既有线性对照。 |
| V3 · FXAA 与空间几何 | 防止在错误色彩域、缩放后或留边外过滤。共用一幅非对称斜边标样和固定比例相机；选择线性亮度差低于 FXAA 绝对阈值、转换为感知亮度后能触发的色块。None 保留基准边缘，FXAA 在预定边缘样点产生有界线性混合，平坦及留边输出在 1 个 SDR 码值内一致。用独立双线性权重检查一个非网格缩放点和四角方向，缩放输入以实际 FXAA 结果为准。核对逻辑矩形折算 321.8×241.25、比例 0.75、aspect 2 时的 321×241 输出、241×180 渲染及两矩形；命中用已接受显示矩形，无额外 DPI、scale 或翻转。用同一布局检查一次能力或预算拒绝和恢复。 |
| V4 · UI 到窗口 | 防止 UI 被显示变换处理、线性混色被提前编码或重复编码。复用 V3 的方向标记，在已知 ViewOutput 背景覆盖一个中间 alpha 的 sRGB UI 色块。分别核对 UI 采样、线性合成后的 PresentColor 和本机真实 backbuffer 路径，码值与独立 sRGB EOTF/OETF 加直通 alpha 计算差 ≤2。记录实际 backbuffer 编码；其他呈现分支仅引用匹配的既有验证。 |
| V5 · v1 和正常帧确认 | 防止请求未保存、同尺寸错代或安全 UI 提前发布。在正式编辑器创建或打开旧 v1，保存和重开合法 FXAA 请求，核对一个非法 AA 字段保留原文档。用同一 Physical Lighting 场景保留 None 输出，在开发隔离目录省略新请求所需的 FXAA 包，确认拒绝能显示旧图且有效 profile、AA、布局和场景记录保持原代。包可用后按正式入口恢复匹配正常帧，确认实际请求、已接受输出和输入映射一起推进；切到诊断时核对实际 None 与 FXAA 请求并存。 |

按风险从 CPU 参考和生成接口推进到原生邻域、缩放、文档和呈现。新增探针以能区分尚未覆盖的失败为条件；每类合同有对应证据并说明覆盖范围后停止扩张。只有失败、实际输入变化或证据缺口触发追加检查。性能结论另以同口径测量为依据。

<a id="a-local-evidence"></a>
## A 实施与本地检查

2026-11-18 使用 `v2.0` 基准提交 `7024c2876ea2ca0a9395adff0f23aa1b8a43b1b7`。工作区暂存区为空，实测清单覆盖 1,250 项正式输入，其中本轮正式受审范围包含 22 项修改和 15 项未跟踪输入。回放前后的源码清单 SHA-256 均为 `737cc55c094c49e86a8b0a8b2700a96272566682d6633297deedba57ff5ab88d`。32 项探针和标样输入、Debug/Release 各十个正式包及实际 exe、库和日志均按原始字节登记。交接清单自身 SHA-256 为 `6ce3e55ec861a0324c53add56867fca1721dd4d03658042f2ae623ab42f989cc`。 [本地交接清单](../../../../build/d07-t05/handoff.json)

在独立 Ninja Multi-Config 树构建 Debug、Release 的正式 `Physara`，两种配置均以 `--hidden --poll-limit=5` 原生启动，退出码为 0。V1–V5 对应本轮 Release 库及包。读回覆盖 Windows x64、MSVC 19.44.35228.0、SDK 10.0.26100.0、Intel RaptorLake-S Mobile Graphics Controller 和 OpenGL 4.6.0 Build 32.0.101.6129。测试色块与窗口观测仅由本地探针加入。窗口实测的是 sRGB backbuffer 路径。 [统一回放](../../../../build/d07-t05/Replay.ps1) · [Debug 构建](../../../../build/d07-t05/product-Debug.log) · [Release 构建](../../../../build/d07-t05/product-Release.log) · [原生日志](../../../../build/d07-t05/native.log)

| 检查 | 实测结果 |
| --- | --- |
| V1 | 固定 Filament 标量参考用独立的上游 math 头和直接求值。半精度 atlas 的最大逐通道绝对误差为 0.00100391，实际 SRGBA8 最大码值差为 1。黑保持 0，超白阶梯可区分，查表上端在显示边界截断。提交票据 1/2 覆盖首次 LUT 写后采样及后续复用，LUT 上传笔数为 1/0。 |
| V2 | 同一 glTF 纹理的 sRGB 颜色、线性数据分别读为 0.216309、0.501953。None/FXAA 的 RGBA16F 工作色相同，D32 在 AA 及曝光对照中相同。自发光工作色由 2.5 随 +1 EV 增至 5.0，最终红通道 sRGB 码值由 247 增至 252。 |
| V3 | 0.75 比例的固定画幅由 241×180 渲染到 321×241 输出。核对半开 UV、能力拒绝和恢复、诊断测试图使用渲染布局。感知亮度斜边产生 348 个改变样点，平色、留边和四角方向相符，596 个缩放边缘样点在独立线性插值码值容差内。 |
| V4 | 同帧 ViewOutput 背景的 RGB 码值为 (61,62,63)。按直通 alpha 混合后的独立参考、PresentColor 和实际窗口 backbuffer 均为 (146,66,53)，在可靠提交完成后读取。 |
| V5 | 旧 v1 缺省 None，待确认的 FXAA 可保存并恢复。非法值按 `/mainView/requested/antiAliasing` 拒绝 Open，活动文档保留。缺少 FXAA 包时明确标记旧图，保留色代 1 和 None 并关闭当前输入映射。恢复原包后同尺寸确认色代 2 和 FXAA。实际编辑器以 708×439 渲染、944×586 输出确认比例，诊断预览保留 FXAA 请求并确认实际 None，回到 Physical Lighting 后重新确认 FXAA。 |

五份增量通过 OpenSpec 严格校验。A 本地检查完成，待维护者调度独立 B 审查代码和注释。 [严格校验](../../../../build/d07-t05/validate-change.json) · [当前问题单](../../../../Docs/CodeReview.md)

<a id="a-round-1-correction"></a>
## 第 1 轮 A 整改与自检

2026-11-19，A 从基准 `7024c2876ea2ca0a9395adff0f23aa1b8a43b1b7` 的工作区核对 B 交回的 38 个路径，按 B 的算法复算摘要为 `d92d3239c94e2920a506f036107117e782f15165a98583085df9a0404c2c6d79`。同时核对全部 1,250 项正式输入，与 A 初交版本相比，B 仅修改本轮的 25 个注释文件。整改相对 B 交回版修改 12 个正式输入，回放前后源码清单 SHA-256 均为 `fc52a78d9e44978f4f4ca46252a8fbebd88486ebe022d14ec357c44cb54380e5`。交回清单逐项登记 36 项回放输入和 79 个正式/检查产物及日志，其文件 SHA-256 为 `28784e33d86a478e148610f8f5cd328d8302517fbfff5db5b992b3ecc7ce0955`。 [B 交回源码清单](../../../../build/d07-t05/a-correction-1/b-source-manifest.json) · [A 整改交回清单](../../../../build/d07-t05/a-correction-1/handoff.json)

| 问题 | 定向实测 |
| --- | --- |
| CR-001 | 隔离的 FXAA 入口先放入同 ABI 的正式 Scale 包。正常 None 已接受，错包报告期望和实际片元源并保留旧图和有效 None。恢复正确包后，实际合成接受新的 FXAA 色代，斜边 976 个红通道样点变化至少 3 码值。 |
| CR-002 | 两个槽在首次确认前持有同一固定 LUT 的上传快照。首槽提交接受后归还 CPU 载荷，次槽仍有 2,097,200 字节预算，包含 2,097,152 字节 atlas 和 48 字节矩形常量。两槽读回相同，首次确认后准备的第三槽直接复用 atlas。实际提交 1/2/3 的 LUT 写入次数为 1/1/0。 |
| CR-003 | Base Color 选择 0.75 缩放时，隔离的 Scale 包缺失。冷启动使用带原因的占位；已有比例 1 的色代 1 时安全 UI 保留原纹理和两个尺寸，并暂停命中。恢复包后正常帧确认色代 2 及命中映射。 |
| CR-004 | 固定 0.75 比例，面板物理宽分别为 321.2、321.8。显示输出同为 321，渲染宽分别为 240、241。正常 UI 对旧/新候选的实际消费为 0/1，接受匹配正常帧前旧候选不取得命中，之后发布 241 的渲染布局。 |
| CR-005 | 尚无场景实绩时面板显示等待。接受物理场景后，删去唯一几何使测试图接受，面板同时标记当前测试图及上次场景的 ACES、FXAA、708×439 渲染与 944×586 输出。新建文档后面板从等待状态开始。 |
| CR-006 | 使用同一场景、相机、布局和曝光，读回快照区分 Material Channels 与 Physical Lighting。线性诊断样点为 0.501953，物理自发光随 +1 EV 从 2.5 增至 5.0，Unlit 基色为 0.25。 |

独立 Ninja Multi-Config 目录从零构建 Debug、Release 的 `Physara`，以新 exe 完成两种配置的原生短启动。逐项核对两种配置的 `ForwardDisplay`、`DisplayColorGrading`、`DisplayFxaa`、`DisplayScale` 包内阶段身份和摘要。Release V1–V5 及六项定向回放通过，覆盖单台 Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129 及 sRGB 窗口路径。编辑器观测代码仅在开发夹具中生成。OpenSpec 严格校验通过。 [统一回放](../../../../build/d07-t05/a-correction-1/Replay.ps1) · [原生日志](../../../../build/d07-t05/a-correction-1/native.log) · [严格校验](../../../../build/d07-t05/a-correction-1/validate-change.json)

<a id="b-round-1-recheck"></a>
## 第 1 轮 B 整改复核（未整体签收）

2026-11-19，B 独立核对 1,250 个正式输入、36 个回放输入和 79 个产物/日志，复算的整改版源码摘要为 `fc52a78d9e44978f4f4ca46252a8fbebd88486ebe022d14ec357c44cb54380e5`。在隔离目录实跑同版 Release 探针，V1–V5 与 CR-001 至 CR-006 均通过，六项已关闭。覆盖单台 Intel RaptorLake-S Mobile / OpenGL 4.6.0 Build 32.0.101.6129 / sRGB backbuffer；Debug/Release 全量构建和产品短启动复用 A 的匹配证据。[B 原生重放](../../../../build/d07-t05/b-recheck-1/replay.log)

另用生产 Editor/Forward 在同一 944×586 物理预览中只改 AA（None → FXAA），正常合成对旧/新候选的选择实测 `1/1`，预期 `0/1`。测的是 UI 选用资格，不是已把旧图交换到窗口。新增 [CR-007](../../../../build/d07-t05/final/review-signed.md#cr-007)，待 A 重新链接并验证匹配版本。B 随复核补齐上传释放与回退注释，未改产品行为；注释后源码摘要 `13cfdf6c1b396fe6621b0922fa1da2b01bb9858c5789708c906cb15aa27cb44b`。最终 verify、同步和归档均未开始。[B 对照日志](../../../../build/d07-t05/b-recheck-1/aa-candidate.log)

<a id="a-round-1-followup"></a>
## CR-007 补充整改与自检

2026-11-19，A 加载 B 复核交回的 1,250 项正式输入，逐字节复算摘要 `13cfdf6c1b396fe6621b0922fa1da2b01bb9858c5789708c906cb15aa27cb44b`，并核对上一轮登记的 36 项输入和 79 项产物。补充整改涉及 `EditorSession.cpp` 和 `EditorSession.hpp`，回放前后的当前源码摘要均为 `1b34818a7a771c1d3c002e711063677cb9370507fac39c418ad897929114696f`。交回清单登记 48 项回放输入和 46 项产物与日志，自身 SHA-256 为 `5768c016d39604581972f146788e1cfb87085477afaae6555ed3e6b5ae74f355`。 [B 交回源码清单](../../../../build/d07-t05/a-correction-2/b-source-manifest.json) · [A 补充整改交回清单](../../../../build/d07-t05/a-correction-2/handoff.json)

编辑器从本次准备值保存双布局和 View 文档代。正常 UI 按候选的实际生产文档代核对场景色，失配时在录制前选择占位并撤销当前命中。用新 Release 库重新链接原样的 B 探针，在同一 944×586 场景中从 None 切到 FXAA，旧/新文档代为 0/1，正常 UI 选用为 `0/1`，退出 0。观测点为合成包的选用资格。 [实现](../../../../Editor/Session/EditorSession.cpp) · [重新链接 B 探针的运行日志](../../../../build/d07-t05/a-correction-2/aa-candidate.log)

同版视口回放覆盖 CR-004 的双栅格对照。显示宽均为 321 时，渲染宽 240/241 对应选用 0/1。实际帧另核对带原因的旧图回退、候选等待期间的输入命中、正确包恢复后的有效 FXAA，以及测试图的接受与场景实绩。Release 的 `Physara` 从独立目录重建并原生短启动，回放和结构校验通过。原生覆盖本机 Intel RaptorLake-S Mobile / OpenGL 4.6.0 Build 32.0.101.6129 的 sRGB 窗口路径。 [回放入口](../../../../build/d07-t05/a-correction-2/Replay.ps1) · [合成与回退日志](../../../../build/d07-t05/a-correction-2/viewport.log) · [变更严格校验](../../../../build/d07-t05/a-correction-2/validate-change.json)

<a id="b-round-1-followup"></a>
## CR-007 补充复核与 CR-008 交回

2026-11-19，B 独立复算 1,250 项正式输入，确认相对前次仅 `EditorSession.cpp/.hpp` 变化；48 项回放输入、46 项产物/日志与 A 补充交回清单一致。B 在隔离目录运行重链接的原探针：944×586 下仅切 None → FXAA，旧/新候选选用 `0/1`、退出 0；双栅格 `240/241 → 321`、有原因的旧图回退、命中恢复和场景历史的视口回归亦退出 0。CR-007 已关闭，不重开原六项。[B AA 对照](../../../../build/d07-t05/b-recheck-2/aa-candidate.log) · [B 视口回归](../../../../build/d07-t05/b-recheck-2/viewport.log)

另用同版生产 Editor/Forward 验证文档代不变的正常选色：只切物理/诊断模式、只移动几何、只移动所选场景相机，三个旧/新候选均为 `1/1`，而非 `0/1`，退出 3。这是构图前的选用资格，不是旧像素上屏的结论；三例记作一项 [CR-008](../../../../build/d07-t05/final/review-signed.md#cr-008)。当前整体未签收。原生覆盖本机 Intel RaptorLake-S Mobile / OpenGL 4.6.0 Build 32.0.101.6129，窗口回归为 sRGB；本次未重跑 Debug 或完整 V1–V5。[来源对照](../../../../build/d07-t05/b-recheck-2/scene-selection.log)

实测正式源码 SHA-256 为 `1b34818a7a771c1d3c002e711063677cb9370507fac39c418ad897929114696f`；B 补注释后交接摘要为 `20f34c5ae3873a76afdae31cb1c7a1198cba37be834bb8df3824169ebd4b7a61`，未修改产品行为。A 重新链接对照并交回后由 B 复核；最终 verify、同步与归档尚未执行。

<a id="a-cr008-correction"></a>
## CR-008 补充整改与自检

2026-11-19，A 核对 B 注释后的 1,250 项正式输入及上次交回的 48 项回放输入、46 项产物。B 交回源码摘要为 `20f34c5ae3873a76afdae31cb1c7a1198cba37be834bb8df3824169ebd4b7a61`。整改涉及 7 个正式文件，当前输入在回放前后的摘要均为 `20b95d79e079124f1ec36acdc40c5d09f7d2522fe27651d778fa4d5212d01bf3`。交回清单登记 49 项回放输入和 50 项 Release 产物及日志，清单自身 SHA-256 为 `bad33958b6bb4e6fec12c0be5a0f30b5018972fe19d0d935cbc88b5ddeb5a245`。 [B 交回清单](../../../../build/d07-t05/a-correction-3/b-source-manifest.json) · [A 交回清单](../../../../build/d07-t05/a-correction-3/handoff.json)

View 准备值封存会话预览模式，Runtime 按该模式准备 Forward，场景颜色快照使用同一次准备的模式、相机及版本。正常合成在声明场景读取前匹配该来源，场景身份、渲染内容代、场景相机代、预览模式、View 文档代和两套布局任一失配时，改用占位并撤销当前命中。 [选色边界](../../../../Editor/Session/EditorSession.cpp) · [View 准备](../../../../Engine/Renderer/Prepare/ViewPreparation.hpp)

以原样的 B 探针及本轮 Release 库重链接。在同一 944×586 视口、相同 View 文档代和持久 FXAA 请求下取得下列录制前选用结果，进程退出 0。 [来源对照日志](../../../../build/d07-t05/a-correction-3/scene-selection.log)

| 唯一改变 | 旧候选 / 新候选 |
| --- | --- |
| Physical Lighting → Base Color | 0/1 |
| 移动模型，Scene 渲染内容代 1 → 2 | 0/1 |
| 移动所选场景相机，场景相机代 1 → 2 | 0/1 |

CR-007 的同尺寸 None → FXAA 和 CR-004 的同显示宽双栅格对照仍为 0/1。视口回放覆盖旧图标识和回退、正常帧接受、命中恢复以及无场景测试图。独立 Release 构建和原生短启动通过。本机原生证据覆盖 Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129 及 sRGB 窗口。另以 Debug 参数和启用的断言通过 `EditorSession.cpp` 语法检查。 [回放入口](../../../../build/d07-t05/a-correction-3/Replay.ps1) · [AA 对照](../../../../build/d07-t05/a-correction-3/aa-candidate.log) · [视口回归](../../../../build/d07-t05/a-correction-3/viewport.log) · [Debug 语法检查](../../../../build/d07-t05/a-correction-3/debug-editor-syntax.log) · [严格校验](../../../../build/d07-t05/a-correction-3/validate-change.json)

<a id="b-cr008-acceptance"></a>
## B 独立签收

2026-11-19，B 签收 `v2.0` 基准 `7024c2876ea2ca0a9395adff0f23aa1b8a43b1b7` 上的实际工作区，暂存区为空。复算 1,250 项正式输入、49 项回放输入、50 项 Release 产物/日志均匹配；签收源码 SHA-256 为 `20b95d79e079124f1ec36acdc40c5d09f7d2522fe27651d778fa4d5212d01bf3`，[交回清单](../../../../build/d07-t05/a-correction-3/handoff.json) SHA-256 为 `bad33958b6bb4e6fec12c0be5a0f30b5018972fe19d0d935cbc88b5ddeb5a245`。7 个整改文件中的来源匹配、快照寿命及拒绝/发布说明与实现一致，本次 B 未改源码。

- **CR-008 通过。** B 将清单中的 11 个标样文件和 10 个 Release Shader 包置于独立目录，实际运行以本轮 Release 库重链接的原探针。944×586、View 文档代固定为 1 时，仅切换 Physical Lighting → Base Color、仅移动模型、仅移动已选相机，旧/新候选选用分别为 `0/1`、`0/1`、`0/1`，退出 0。观测点在正常 UI 的图内采样声明之前，无需让过期画面上屏。[B 场景来源日志](../../../../build/d07-t05/b-recheck-3/scene-selection.log)
- **相邻回归通过。** CR-007 同尺寸 None → FXAA 与 CR-004 同显示宽、不同渲染宽均保持 `0/1`；真实帧验证安全 UI 旧图回退、当前命中暂停/恢复及无场景测试图历史。两个程序均退出 0。三份 B 日志分别与 A 日志逐字节相同。[B AA 日志](../../../../build/d07-t05/b-recheck-3/aa-candidate.log) · [B 视口回归](../../../../build/d07-t05/b-recheck-3/viewport.log)
- **范围与后续。** CR-001 至 CR-008 均已关闭，代码与注释的独立审查完成。覆盖本机 Intel RaptorLake-S Mobile / OpenGL 4.6.0 Build 32.0.101.6129 和 sRGB 窗口；本次未重跑完整 Debug 构建或 V1–V3。不代 A 执行第 7 节的最终 verify、同步与归档。[本轮签收问题单快照](../../../../build/d07-t05/final/review-signed.md#b-accepted)

<a id="final-verification"></a>
## 最终验证与证据

2026-11-19，A 按基准 `7024c2876ea2ca0a9395adff0f23aa1b8a43b1b7` 复算 1,250 项实际正式输入，逐项匹配 B 的签收清单 `20b95d79e079124f1ec36acdc40c5d09f7d2522fe27651d778fa4d5212d01bf3`。已按原始字节保存包含八项 CR 结论及注释覆盖的问题单，其 SHA-256 为 `855d606bff2bdb0d90f0f7b03f4c92e22da0ba2d127260040f70b579541ea517`，快照中的历史相对链接以 `Docs/` 为基准。 [B 签收问题单快照](../../../../build/d07-t05/final/review-signed.md#b-accepted) · [签收源码清单](../../../../build/d07-t05/final/signed-source.json)

在独立 Ninja Multi-Config 目录，用 Windows x64、MSVC 19.44.35228.0 和 Windows SDK 10.0.26100.0 从零构建 Debug、Release。两份正式 `Physara` 以 `--hidden --poll-limit=5` 启动并退出 0。Debug/Release 的八份显示阶段包通过包内摘要及逻辑片元源校验，Release 的十份包与 B 实测副本逐字节相同。回放前后核对 48 项输入及 72 项构建产物和日志，并登记 14 项回放产物和日志，证据清单 SHA-256 为 `948d77de66a09f58511a9c1b67841882264d6cb67045803d7ae47b8e09e4fd4b`。 [Debug 构建](../../../../build/d07-t05/final/product-Debug.log) · [Release 构建](../../../../build/d07-t05/final/product-Release.log) · [回放入口](../../../../build/d07-t05/final/Replay.ps1) · [最终证据清单](../../../../build/d07-t05/final/handoff.json)

| 规范范围 | 实现与场景核对 |
| --- | --- |
| `display-post-processing`，4 项 | V1 与独立上游计算的最大线性误差为 0.00100391，SDR 最大码值差为 1。双槽 LUT 的实际写入为 1/1/0。V2 验证 HDR 自发光随 +1 EV 从 2.5 增至 5.0，None/FXAA 工作附件与 D32 数值一致。V3 在低亮斜边测得 348 个变化样点，596 个缩放样点通过线性插值对照。V4 按事件完成后读回，线性合成预期、PresentColor 和实际 sRGB 后缓冲均为 (146,66,53)。 [LUT](../../../../Engine/Renderer/PostProcess/ColorLut.cpp) · [显示图](../../../../Engine/Renderer/PostProcess/Display.cpp) · [HDR 来源](../../../../Engine/Renderer/Paths/Forward.hpp) |
| `editor-viewport-output`，2 项 | V3/V5 核对 `241×180 → 321×241`、半开式命中、能力拒绝、比例接受、旧图回退和测试图历史。最终回放拒绝同 ABI 的错包，恢复后正常帧发布 FXAA。CR-004、CR-007 和 CR-008 用三份重链接探针覆盖，双栅格、AA、模式、内容和相机的旧/新选用均为 0/1。窗口缩放解耦、隐藏恢复和稳定 token 使用既有验收与实际消费者核对。 [尺寸换算](../../../../Editor/Viewport/Viewport.cpp) · [选色和命中](../../../../Editor/Session/EditorSession.cpp) · [帧回执](../../../../Runtime/Frame.cpp) · [既有视口验收](../2026-11-11-establish-frame-slots-and-viewport-output/tasks.md#final-verification) |
| `scene-document-persistence`，1 项 | V5 用正常入口打开缺省为 None 的 v1，保存并重开待确认的 FXAA 请求，非法 AA 指向具体字段并保留活动文档。物理相机字段、请求比例和引用校验对照同一读写入口及既有验收。 [读取](../../../../Editor/Session/SceneDocumentRead.cpp) · [写入](../../../../Editor/Session/SceneDocumentWrite.cpp) · [v1 文档验收](../2026-11-16-add-static-scene-documents/tasks.md#final-verification) · [物理请求验收](../2026-11-17-add-physical-camera-exposure/tasks.md#final-verification) |
| `static-forward-rendering`，1 项 | V2 核对预曝光超白、sRGB 解码与线性材质数据、Unlit 基色保留。V5 通过正式模式切换核对物理显示和材质诊断，CR-006 由冻结的生产模式解释工作色。已有法线、自发光及直接光路径核对其消费者和历史验收。 [工作色入口](../../../../Shaders/Passes/Forward/Color.frag) · [Forward 验收](../2026-11-16-render-static-scenes-forward/tasks.md#final-verification) · [直接光验收](../2026-11-18-add-forward-direct-lighting/tasks.md#final-verification) |
| `view-preparation`，1 项 | V3 以固定相机使用两次像素适配，检查渲染和显示矩形及留边命中。无有效布局的失败和恢复沿已验收的 View 准备入口核对。 [View 准备](../../../../Engine/Renderer/Prepare/ViewPreparation.cpp) · [既有投影验收](../2026-11-16-prepare-static-scene-views/tasks.md#final-verification) |

完整性核对覆盖已完成的 12 项实施及独立审查任务、五份增量的 9 项要求和 32 个场景。正确性证据由签收版回放与保留场景的历史记录组成。设计核对覆盖固定 LUT、图内线性后处理、双栅格与接受边界，构建沿既有生成接口和包依赖。OpenSpec 严格变更校验通过。原生图形回放在 Release 上覆盖单台 Intel RaptorLake-S Mobile Graphics Controller，OpenGL 4.6.0，驱动 32.0.101.6129，窗口为 sRGB 后缓冲。过期候选对照观察录制前的 UI 选用资格，回退和恢复另外使用实际提交验证。 [严格校验](../../../../build/d07-t05/final/validate-change.json) · [完整原生回放](../../../../build/d07-t05/final/native.log) · [AA 对照](../../../../build/d07-t05/final/aa-candidate.log) · [场景来源对照](../../../../build/d07-t05/final/scene-selection.log)

<a id="spec-sync"></a>
## 规范同步

新增 `display-post-processing` 主规范及 4 项要求，更新视口输出、场景文档、Forward 和 View 准备的 5 项要求。主规范与五份增量逐块一致，涉及的 32 个场景均已核对，保留既有主规范的用途、未涉及要求及既有场景名称。OpenSpec 严格校验通过全部 34 份主规范。 [同步清单](../../../../build/d07-t05/final/spec-sync.json) · [主规范严格校验](../../../../build/d07-t05/final/validate-specs.json)

[显示后处理](../../../specs/display-post-processing/spec.md) · [视口输出](../../../specs/editor-viewport-output/spec.md) · [场景文档](../../../specs/scene-document-persistence/spec.md) · [静态 Forward](../../../specs/static-forward-rendering/spec.md) · [View 准备](../../../specs/view-preparation/spec.md)

<a id="archive-closeout"></a>
## 归档收尾

2026-11-19，五份增量完成同步后由 OpenSpec 原生归档 `add-forward-display-profile`。归档保留 schema 元数据及五份增量，已核对归档文档与开发计划的引用。D07-T05 三项工作包任务已标为验收完成，下一工作包为 D07-T06。保留签收原件和最终验证证据后，当前问题单逐字节恢复为模板。 [归档结果](../../../../build/d07-t05/final/archive.json) · [开发计划](../../../../Docs/Developments/Developments.md#current-stage) · [当前问题单](../../../../Docs/CodeReview.md)
