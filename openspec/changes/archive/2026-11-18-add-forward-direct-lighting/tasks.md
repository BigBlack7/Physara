# Tasks

## 1. 可见灯表与生成接口

- [x] 1.1 A 在 `Renderer/Lighting` 消费冻结的 `PreparedView.lights`，形成独立方向段与完整局部段。沿 glTF 实例和 SceneFrame 保留单位、世界轴、范围和稳定可见性。用 V1 检查相交有限灯、无截断、方向及隐藏灯，核对相机切换和灯变更沿现有版本传播。
- [x] 1.2 A 在权威 `StaticForward` schema 加入灯行、View 计数与观察输入、世界位置 IO。以生成类型编码实际字段和范围，校验 GPU 表达与容量。用 V1 核对源值、生成 stride 与容量边界、拒绝原因。

## 2. 图内交付与直接光求值

- [x] 2.1 A 将灯表接入 Forward 帧槽、完整缓冲上传和 `ShadeOpaque` 局部绑定，处理零表、失败重试及在途保活。沿 V2 核对写后版本、图许可、范围与接受状态，前置深度保持原活动资源。
- [x] 2.2 A 实现共享 `EvaluateDirect` 与 `EvaluateLighting`，在正式受光颜色入口用当前观察向量、世界位置、覆盖前导数及一次 `PixelParams` 输出直接光与预曝光自发光。更新 Color/Depth 配套包并核对 V2 的反射及绑定，用 V3 的独立数值核对单位、衰减、余弦、叠加、AO 隔离、曝光和零结果。随实现记录发射轴、数值边界及结果域。

## 3. 受光表面、视口与兼容

- [x] 3.1 A 用正式 Forward 和实际图完成 V2 同槽更新、在途数据及失败后恢复的原生核对。复用 V3 的场景完成 V4 的透视／正交、MASK 两种 Pass 组合和 Unlit 像素对照。
- [x] 3.2 A 在现有 View Settings 加入 Direct Lighting 选择及实际可用性说明，沿 Runtime 正常图确认模式和曝光。以 V4 的正式入口核对诊断切换、待确认或回退、匹配接受与显示编码，同步构建运行说明。

## 4. A 本地核对与交接

- [x] 4.1 A 核对 D07-T03 三项开发目标和 V1–V4 对应源码、包及运行证据，完成 OpenSpec 严格校验及链接检查。记录基准、实际 diff、未跟踪文件、输入、包与 exe 摘要、设备和未覆盖项，交由维护者调度独立 B。 [签收问题单快照](../../../../build/d07-t03/final/review-signed.md)

## 5. 独立审查、整改与复核

- [x] 5.1 B 独立核对实际受审版本、规范及直接消费者，检查物理公式、工作集完整性、图与在途寿命、阶段反射及视口语义。通读完整涉改第一方文件，补齐职责、接口、单位及流程注释，登记覆盖、稳定 CR 编号和结论。第 1 轮登记 CR-001 至 CR-005，待 A 整改。 [首轮问题单](../../../../build/d07-t03/final/review-signed.md) · [审查闭环](../../../../AGENTS.md#review-loop)
- [x] 5.2 A 按 CR 编号整改并自检，登记输入身份、改动及对应回归结果。无待整改项时据实登记。 [第 1 轮整改证据](#a-round-1-correction) · [再次整改证据](#a-round-1-second-correction) · [缩放顶点整改证据](#a-round-1-scaled-correction) · [原点重合整改证据](#a-round-1-origin-correction)
- [x] 5.3 B 已核对最终输入、整改、定向回归及完整注释，签收代码正确性与注释可读性。CR-001 至 CR-005 全部关闭。 [B 签收版本与证据](#b-final-signoff) · [首次整改复核](#b-round-1-recheck) · [再次整改复核](#b-round-1-second-recheck) · [缩放顶点整改复核](#b-round-1-scaled-recheck)

## 6. 最终验证、同步与归档

- [x] 6.1 A 在 B 签收后重建并运行 V1–V4，完成 OpenSpec 最终核验，核对源码、包、exe、实际原生提交和数值证据身份与范围。 [最终验证与证据](#final-verification)
- [x] 6.2 A 在本任务表留存 CR、整改、注释覆盖、签收及验收证据，同步 `punctual-lighting` 与 `static-forward-rendering`，保留既有诊断场景并通过主规范严格校验。 [规范同步](#spec-sync)
- [x] 6.3 A 用 OpenSpec 归档，核对归档链接和开发计划完成状态，证据留存后按模板恢复当前问题单。 [归档收尾](#archive-closeout)

<a id="verification"></a>
## 验证范围与停止条件

标样、探针和临时工程归入 `build/d07-t03/`。优先使用 D06 已有灯和遮罩见证、D07-T01 的 BRDF 参考及 D07-T02 的实际 Forward、GL 读回与会话验证方法。仅复用身份匹配的输入和证据，受影响的生成 ABI、包和消费者随当前源码构建。V2–V4 尽量共用一个可回放的正式 Forward 场景和读回入口。 [已有准备验收](../2026-11-16-prepare-static-scene-views/tasks.md#final-verification) · [BRDF 验收](../2026-11-17-add-core-surface-brdf/tasks.md#final-verification) · [预曝光验收](../2026-11-17-add-physical-camera-exposure/tasks.md#final-verification) · [验证范围](../../../../.agents/skills/physara-verification/SKILL.md)

| 检查 | 最小代表输入及独立预期 | 停止条件 |
|---|---|---|
| V1 · 源单位、集合与编码 | 将含方向、点光和聚光的 glTF 通过正式入口导入并实例化，用现有 Scene 事务隐藏一盏灯后取得 SceneFrame。用灯心在外但球相交、球完全在外及无 range 的标样核对候选，带旋转与缩放的父节点用于区分世界位置／发射轴与灯的独立源属性。用一份占满生成容量及再多一条的轻量 CPU 输入证明限额，在 Release 核对拒绝。再用源锥角合法但窄化到同一余弦的代表输入区分 GPU 编码失败和源数据合法性。 | 同一 SceneFrame 的 View 工作集各自匹配，生成计数、类型、字段及范围准确。超限或不可表示的灯得到明确诊断且整表拒绝。容量边界以 CPU 检查为止。 |
| V2 · 生成 ABI、图与帧槽 | 构建正式 `Physara` 及 `StaticForward`/`StaticDepth`，核对主机布局、活动资源反射、完整块范围及对应包。真实 GL 合成保持相机、模式和灯数不变，在同一已完成槽中修改一个灯，再改为零灯。额外保留一个未确认的匹配上传候选或复用可触发的图失败路径，对照上一接受输出和重新准备。检查同一数据在引用它的另一在途槽内仍有效。 | 前置深度不读灯表，颜色只从写后许可版本取值。同槽读回随变更更新，零计数无残留，未确认数据可重试，且完整后才确认新画面。记录实际采用的未确认路径。 |
| V3 · 光度与工作色 | 以平坦 `baseColor=0.5`、metallic=0、感知粗糙度=1、`N=V=L` 的受光样点和 `preExposure=5/48` 读回 `RGBA16F`。4 lux 白色方向光单通道期望为 `4 × (0.5 + 0.01) / π × 5/48 ≈ 0.06764085`，`0.01/π` 来自正入射核心镜面。同一 `N=V` 下将 `NoL` 设为 0.5，期望约 `0.03404239`，用于区分余弦乘一次或多次。16 cd 点光在 2 m 匹配 4 lux，在 4 m 为其四分之一。显式 `R=4 m` 在 2 m 另乘 `15/16`、边界归零。固定聚光位置、相机和表面，仅改变灯轴，余弦位于内、外端点中间时角因子为 `1/4`，内锥为 1、外锥外为零。在相同读回中比较彩色灯、方向与局部叠加及 AO 隔离。零灯时，未预曝光自发光 `12` 对应 `1.25`，+1 EV 对应 `2.5`，无发射为零。 | 读回位于由 FP32 运算和 FP16 量化预定的容差内。正距离满足数值锚点，重合点零贡献，线性加法、+1 档的直接光和自发光翻倍、可表示超白及零灯均可区分。数值比较取显示之前的工作色。 |
| V4 · 正式视口与覆盖 | 复用有 MASK 临界样本和非对称法线的场景，以正式 Color 检查有／无前置深度的 HDR 和 Reverse-Z。以正交固定视线及透视随位置改变的视线检查镜面方向，用一处几何法线梯度核对粗糙度过滤接入实际受光表面。在 View Settings 切换 Direct Lighting 与既有诊断，核对有灯／无灯时的 Unlit、旧输出资格、匹配的模式和已确认曝光，核对显示限幅沿原入口消费。 | 两种 Pass 组合覆盖相同，法线、观察向量和原诊断结果符合各自合同。只有正常图采样并接受时发布匹配结果，正式 v1 场景可通过既有入口打开及查看。 |

原生结果登记实际设备、驱动、构建配置、提交及读回格式，CPU 或布局校验分别登记。达到上述证据要求后结束本包验证，DFG、环境、显示参考、阴影及跨路径差分进入相应工作包。

<a id="a-local-evidence"></a>
## A 本地验证与交接

从仓库根目录执行 `& ./build/d07-t03/Verify.ps1`。使用 Windows x64、MSVC 19.44.35228.0、Windows SDK 10.0.26100.0 和 Ninja Multi-Config，在干净的 UTF-8 控制台配置中构建 Debug `Physara`、Release `PhysaraRenderer` 及探针。Debug 产品运行 `Physara.exe --hidden --poll-limit=6`，退出码为 0。验证脚本检查 Ninja 已为 Runtime 主程序对象登记 `DeviceHost.hpp` 依赖。 [回放入口](../../../../build/d07-t03/Verify.ps1) · [依赖日志](../../../../build/d07-t03/build-dependencies.log)

首次原生图编译暴露了实际结构化缓冲限制未传入编译器的问题。宿主现在于启动时查询最大 SSBO 块与偏移对齐，Runtime 的场景编译使用测得的值。原增量配置的 MSVC `showIncludes` 前缀解码错误导致旧对象混用，保留初次日志并从干净配置重建。 [首次图编译](../../../../build/d07-t03/native-before-graph-limits.log) · [初次增量构建](../../../../build/d07-t03/build-stale-dependency.log)

| 检查 | 本轮实测 |
|---|---|
| V1 | Debug 与 Release `NDEBUG` 均用正式 glTF 导入和 Scene 事务核对隐藏灯、有限灯球相交、无截断灯和方向灯。相机切换复用同一 SceneFrame，移动有限灯更新内容与可见性。父级旋转缩放后的聚光位置和世界发射轴正确，保留 7 cd、2 m。生成 stride 64 字节、容量 16384，核对填满、超一条和源锥角余弦窄化冲突。 |
| V2 | `ForwardView` 为 112 字节，正式 `StaticForward` 与 `StaticDepth` 共享 ABI `10de5757d45f25a669eff043d09ea30949bee3e2a5b50d426f9d3e3c6b312df7`。只有颜色片元反射出 set 3 的灯表，图读取完整写后范围。Intel GL 实测最大块为 1073741824 字节，对齐 4 字节。两个槽连续提交后分别读到 0.06762695 与 0.13525391，同槽改为 12 lux 的候选未提交，重试上传后读到 0.20288086。纯曝光继续复用灯表，零计数输出为零。 |
| V3 | `RGBA16_FLOAT` 读回方向光 0.06762695、斜入射 0.03402710。16 cd 在 2 m 与 4 m 分别为 0.06762695 和 0.01690674，4 m 有限灯在 2 m 为 0.06335449，边界外归零，重合点输出零。聚光中心与余弦中点读回 0.06762695 和 0.01690674。两个灯的叠加从 0.13525391 随 +1 EV 变为 0.27050781。带 AO 贴图的直接光仍符合独立预期，零灯自发光从 1.25 变为 2.5，现有显示输出限幅为 1。 |
| V4 | 合并与前置深度得到一致的遮罩颜色及 D32 深度 0.25、0.75。正交与透视的离轴镜面分别读到 0.89501953 和 0.08831787，法线梯度样点读到 0.55126953。验证 Unlit 基色、真实 v1 保存重开及 View Settings 选择，等待时保留 Base Color，正常图确认 Direct Lighting 和匹配曝光，随后可切回 Emissive。 |

复用 D07-T02 已核对的五个遮罩标样文件。回放前后核对 1593 个正式输入与实际 exe、库、生成包、探针及标样，另核对 24 项包依赖。交接清单 SHA-256 为 `536783722c9d2941946d2c4c74a82953a59f5011283ef17f73473b74ae3c98b1`。原生覆盖单台 Intel RaptorLake-S Mobile Graphics Controller 的 Debug OpenGL 4.6.0，驱动 32.0.101.6129。Release 覆盖 V1 CPU 边界。 [交接清单](../../../../build/d07-t03/handoff.json) · [原生回放](../../../../build/d07-t03/native.log) · [待审差异](../../../../build/d07-t03/a-source.diff)

`openspec validate add-forward-direct-lighting --type change --strict --json` 与 `git diff --check` 均通过。113 个本地链接与锚点可解析，实际待审源码、回放产物和交接清单摘要匹配。

<a id="a-round-1-correction"></a>
## 第 1 轮 A 整改与自检

按 B 登记的原始字节算法复算 19 个交回输入，摘要为 `57120ea420ddea39ef15c95fbbe44b2aff683af7fa2564b63d171d1b690cbbaf`。整改涉及其中的灯光编码、生成接口、共享顶点与受光 Shader，并新增设置及视口原因的直接消费者至审查覆盖。比对正式输入清单，12 个源码及 Shader 接口文件相对 B 初审交回版本发生变化。 [B 初审记录快照](../../../../build/d07-t03/a-correction/review-before-correction.md) · [B 源码清单](../../../../build/d07-t03/a-correction/b-source-manifest.json)

| 问题 | 整改与实际回归 |
|---|---|
| CR-001 | 强度允许零，正强度和显式 range 在生成行中须为有限 FP32 正规数。复用 B 的 `1e-40 m` 范围及 `1e-40 lux` 强度和 128 EV 补偿，正式 Forward 准备分别报告实体和 `range`、`intensity`，两者均拒绝整表。缺省范围的红通道为 `0.06762695`，零强度为零。Debug 和 Release 核对 FP32 最小正规数允许编码。 |
| CR-002 | 聚光余弦端点之间须有可表示的 FP32 值，才交给过渡求值。B 的相邻端点标样在轴心和中点朝向下均返回 `spot cone cosines`。宽锥轴心、余弦中点及外锥读回 `0.06762695 / 0.01690674 / 0`。 |
| CR-003 | 局部灯、对象和透视眼点保存世界位置高位及余量。光照先相减高位再合并偏移差。0.015625 cd 点光离表面 0.0625 m，整体平移前和沿 Z 平移 1048576 m 后，红通道均为 `0.06762695`。两次都有实际几何深度，重合对照为零。仅移动相机且可见索引匹配时，同一槽继续使用已上传灯表。 |
| CR-004 | 灯表编码拒绝使用独立的 View 状态及视口占位，Runtime 保留灯实体和字段诊断。用有效 OPAQUE 平面执行拒绝帧，捕获 View Settings 的灯表文案，逐顶点比对正式视口叠字生成的字形。用 BLEND 平面通过同一入口核对原表面错误及叠字。 |
| CR-005 | View Settings 从最后接受的颜色代读取模式。真实界面回放选择 Direct Lighting，灯表拒绝时仍显示上次接受的 Base Color。修正灯锥后先提交仅含 UI 的帧，颜色代保持原值。匹配的正常图采样及接受后才变为 Direct Lighting，核对两次面板文本。 |

从仓库根目录运行 `& ./build/d07-t03/a-correction/Verify.ps1`。在独立 UTF-8 Ninja Multi-Config 目录构建 Debug `Physara` 与 Release `PhysaraRenderer`，并以匹配产物复跑 V1–V4 和上述五项。原生检查覆盖 Debug 的 Intel RaptorLake-S Mobile Graphics Controller，OpenGL 4.6.0，驱动 32.0.101.6129。Release `NDEBUG` 覆盖 V1 的 CPU 编码及拒绝边界。Debug 产品短启动退出码为 0。 [回放脚本](../../../../build/d07-t03/a-correction/Verify.ps1) · [Debug 独立构建](../../../../build/d07-t03/a-correction/fresh-debug.log) · [Release 独立构建](../../../../build/d07-t03/a-correction/fresh-release.log) · [原生回归](../../../../build/d07-t03/a-correction/native.log)

`ForwardView` 为 128 字节，`ForwardObject` 为 160 字节，灯行 stride 为 64 字节，容量为 16384。正式颜色和深度包的 ABI 为 `7192091aae46e72fa56a2427168779c7b99eccc3142209d8b62d86309de587b6`，灯表只在颜色片元反射为活动资源。复用标样的 V1–V4 回放前后核对 1593 个正式输入、实际 exe、库、包及标样，并验证 24 项包依赖。A 整改清单 SHA-256 为 `fbd80a54203145847ca4ec6ba037120bb74574c58e108a4bd5e1681edd9b8eb1`。 [最终待审清单](../../../../build/d07-t03/a-correction/handoff.json) · [待审差异](../../../../build/d07-t03/a-correction/a-source.diff) · [拒绝面板](../../../../build/d07-t03/a-correction/rejected-settings.log) · [恢复面板](../../../../build/d07-t03/a-correction/recovered-settings.log) · [表面诊断](../../../../build/d07-t03/a-correction/unsupported-settings.log)

OpenSpec 严格校验、`git diff --check`、本地链接及交接清单核对均通过。当前版本已登记至问题单，待 B 独立复核。

<a id="b-round-1-recheck"></a>
## 第 1 轮 B 整改复核

B 已核对 A 回交的源码和运行输入，并在独立目录重放 Release CPU 与 Debug 原生回归。CR-001 的最小 FP32 正规数与非正规数拒绝边界、CR-004 的拒绝原因、CR-005 的已接受模式均通过。CR-002 在允许编码的窄锥中点读到 0.00751495，预期 0.01691021。CR-003 在顶点自带 Z 偏移的同一网格上共同平移光源、表面和相机，4 lux 样点由 0.06762695 变为零。两项继续使用原 CR 编号，5.3 待最终复核签收。 [签收问题单快照](../../../../build/d07-t03/final/review-signed.md) · [原回归重放](../../../../build/d07-t03/b-recheck-lighting/original-replay.log) · [B 定向读回](../../../../build/d07-t03/b-recheck-lighting/boundary-readback.log)

<a id="a-round-1-second-correction"></a>
## 第 1 轮 A 再次整改与自检

按问题单的原始字节算法核对 B 复核交回的 22 个源码及构建输入，汇总摘要为 `10d2b21a4e0be22985ed83e718d007319487522300b757ce131e88738e394163`。再次整改涉及灯行编码、生成接口、共享顶点入口、颜色入口和逐灯计算，共 5 个正式输入。 [B 交回记录快照](../../../../build/d07-t03/a-correction-2/review-before-correction.md) · [B 交回源码清单](../../../../build/d07-t03/a-correction-2/b-source-manifest.json)

| 问题 | 再次整改与实际回归 |
|---|---|
| CR-002 | 以单位方向差的平方表示余弦过渡，按编码精度预算拒绝无法分辨的极窄区间。用 B 使用的 3 步窄锥读回轴心、中点、外侧，结果为 `0.06762695 / 0.01690674 / 0`，中点与参考 `0.01691021` 一致至读回容差。此前的 1 步标样中点读回 `0.01692200`，正常宽锥中点为 `0.01690674`。不足精度的区间仍给出实体和锥角字段诊断。 |
| CR-003 | 节点平移的高位和余量与插值后的仿射顶点偏移分别传递。主差补回两次减法及位置编码余量。复用 B 的 `baked-z.gltf`，把世界平面 `Z = 1048580 m` 分别由节点与顶点表达，6.25 cm 样点均读回 `0.06762695`。12.5 cm 样点在无平移、0.0625 m 节点平移及 `2097151.9375 m` 公共平移下均读回同值。0.0625 m 节点平移样本可区分仅调整减法顺序与恢复减法余量。所有样点通过实际几何深度及基色检查，烘焙网格再核对真重合为零及同槽纯相机移动复用。 |

从仓库根运行 `& ./build/d07-t03/a-correction-2/Verify.ps1`。独立 UTF-8 Ninja Multi-Config 源码构建及后续回放使用 Debug `Physara`、Release `PhysaraRenderer` 和匹配探针。V1–V4 与五项定向回归通过，包含 CR-001 原输入拒绝和 CR-004、CR-005 的面板与接受状态。Debug 产品短启动退出码为 0。原生读回范围为 Intel RaptorLake-S Mobile Graphics Controller，Debug OpenGL 4.6.0，驱动 32.0.101.6129。Release `NDEBUG` 覆盖 V1 CPU 边界。 [回放脚本](../../../../build/d07-t03/a-correction-2/Verify.ps1) · [原生读回](../../../../build/d07-t03/a-correction-2/native.log) · [Release CPU](../../../../build/d07-t03/a-correction-2/v1-Release.log)

更新后的 `StaticForward` 和 `StaticDepth` 共用 ABI `3aee64fe96506a67e963bc8bff7dbff6f2d438a04e00229ea076c5a1cf362b37`，二者均通过阶段反射核对，灯表在颜色片元活动。灯行 64 字节，容量 16384，`ForwardView` 128 字节，`ForwardObject` 160 字节。核对 1593 项正式输入、24 项包依赖及回放产物身份。再次整改清单 SHA-256 为 `a65602ee0591335651aea88f5f94fc7c1817913becc37b5cb8ef873d8d9c5d89`。 [再次整改清单](../../../../build/d07-t03/a-correction-2/handoff.json) · [回交差异](../../../../build/d07-t03/a-correction-2/a-source.diff)

<a id="b-round-1-second-recheck"></a>
## 第 1 轮 B 再次整改复核

B 核对了 A 回交输入，并在独立目录重放 Debug 原生图、编辑器和 Release CPU 回归。CR-002 通过，三步窄锥中点读回 0.01690674，参考值为 0.01691021。CR-001、CR-004、CR-005 复验通过。CR-003 的原平移标样恢复，但将同一平面沿 Z 缩放 65/64 后，两种合法几何写法在 6.25 cm 灯距下分别读到 0.06762695 和 0.01690674，真重合时分别读到零和 0.06762695。仅 CR-003 交 A 继续整改，5.3 待最终签收。 [签收问题单快照](../../../../build/d07-t03/final/review-signed.md) · [回归重放](../../../../build/d07-t03/b-second-recheck-lighting/handoff-replay.log) · [缩放对照读回](../../../../build/d07-t03/b-second-recheck-lighting/scaled-plane-readback.log)

<a id="a-round-1-scaled-correction"></a>
## 第 1 轮 A 缩放顶点整改与自检

按问题单算法核对 B 本次交回的 22 个源码及构建输入，原始字节汇总为 `08db37c3ecb7002efde9172c09cf3aa016ae4c30999d28707632b8f296b7768d`。本次修正 5 个正式输入。 [B 交回记录快照](../../../../build/d07-t03/a-correction-3/review-before-correction.md) · [B 输入清单](../../../../build/d07-t03/a-correction-3/b-source-manifest.json)

CR-003 以图元的模型空间包围盒中点为局部基点，用原始 double 实例矩阵变换基点并拆出世界位置高位和余量。Shader 先减局部基点，再对偏移做线性变换。片元沿用补偿差值求局部灯距及透视观察方向。对象行按对象身份及基点区分，两种深度组合仍共用裁剪位置。 [对象行编码](../../../../Engine/Renderer/Draw/StaticDraw.cpp) · [顶点位置](../../../../Shaders/Geometry/StaticVertexFetch.glsl)

| 样点 | 节点平移写法 | 烘焙顶点写法 |
|---|---|---|
| Z 缩放 65/64，6.25 cm 灯距，0.015625 cd | `0.06762695` | `0.06762695` |
| Z 缩放 65/64，保持强度，灯与表面重合 | `0` | `0` |

两种写法均复用 B 的样本，实际基色为 `0.5`，D32 为 `0.5`。复跑上一轮公共平移、真重合和同槽灯表复用对照。额外使用同一实体的两个图元，分别在局部 `Z = 0` 和 `Z = 1048580`，验证对象行按基点区分。两个近灯样点均读回 `0.06762695`。保持可见集，仅沿 Z 移动相机，两者读回一致且图中没有对应对象行或灯表重传。 [原生读回](../../../../build/d07-t03/a-correction-3/native.log) · [双图元标样](../../../../build/d07-t03/a-correction-3/fixture/Assets/Models/LightingProbe/anchor-pair.gltf)

从仓库根运行 `& ./build/d07-t03/a-correction-3/Verify.ps1`。独立 UTF-8 Ninja Multi-Config 构建 Debug `Physara` 与 Release `PhysaraRenderer`，用同版产物完成 V1–V4 及五项定向回归。Debug 产品短启动退出码为 0。原生验证覆盖单台 Intel RaptorLake-S Mobile Graphics Controller 的 Debug OpenGL 4.6.0，驱动 32.0.101.6129。Release `NDEBUG` 覆盖 CPU 编码边界。 [回放脚本](../../../../build/d07-t03/a-correction-3/Verify.ps1) · [Debug 构建](../../../../build/d07-t03/a-correction-3/fresh-debug.log) · [Release CPU](../../../../build/d07-t03/a-correction-3/v1-Release.log)

两份正式 Forward 包共用 ABI `a8b4986e7255b81f871c35781ea8a6276e539b1b3a87e056dc9ec13f55c9a21f`，各自阶段反射一致于生成接口。`ForwardObject` 为 192 字节，新基点字段偏移为 144、160、176。`ForwardView` 为 128 字节，灯行 stride 为 64 字节，容量为 16384。回放核对 1593 个正式输入、24 项包依赖与产物身份。交接清单 SHA-256 为 `401752bd77a007ce140b7a84205e443ca961725cb1356125ed294bb70983e4e7`。 [本次回交清单](../../../../build/d07-t03/a-correction-3/handoff.json) · [待审差异](../../../../build/d07-t03/a-correction-3/a-source.diff)

<a id="b-round-1-scaled-recheck"></a>
## 第 1 轮 B 缩放顶点整改复核

B 核对了 A 交回的 5 个源码变更，在独立目录重放原生图、编辑器及 Release CPU 检查。CR-003 原四个缩放样点、同实体双图元基点和同槽缓存复用均通过。另一组对照将可见平面和点光固定在世界原点，仅在同一图元的局部 Z = −2.2 m 加入完全被遮挡的后表面。当前版本的真重合红通道从 0 变为 65504。整改前的版本在两份标样中均为 0，两版的 6.25 cm 对照均为 0.06762695。CR-003 继续整改，5.3 待最终签收。 [签收问题单快照](../../../../build/d07-t03/final/review-signed.md#cr-003-origin-overlap) · [本次读回](../../../../build/d07-t03/b-scaled-review/scaled-and-origin-readback.log) · [整改前对照](../../../../build/d07-t03/b-scaled-review/previous-origin-readback.log)

<a id="a-round-1-origin-correction"></a>
## 第 1 轮 A 原点重合整改与自检

核对 B 交回的 22 个源码及构建输入，原始字节汇总为 `95652a92592ed270898f53ce7ed2b48ad9d41c7d6939f3e0b34b268327735619`。本次修正涉及 7 个正式输入。双面标样沿用 B 复核文件，SHA-256 为 `71e8ddfb539255ab25ae089a613ef3e03adc727c8858ff812d29033f894b7e49`。 [B 交回记录快照](../../../../build/d07-t03/a-correction-4/review-before-correction.md) · [B 输入清单](../../../../build/d07-t03/a-correction-4/b-source-manifest.json) · [双面标样](../../../../build/d07-t03/a-correction-4/fixture/Assets/Models/LightingProbe/two-sheets.gltf)

CR-003 将表面仿射平移与灯位移到同一个 FP32 世界基点。顶点用显式拆分恢复乘积余量，用 TwoSum 恢复求和余量，再把偏移的高位和余量分别插值。已在两种缩放下验证遮挡后表面改变包围盒时，真重合仍为零，正距按反平方计算。 [数值设计](design.md) · [对象行](../../../../Engine/Renderer/Draw/StaticDraw.cpp) · [顶点偏移](../../../../Shaders/Geometry/StaticVertexFetch.glsl) · [片元灯距](../../../../Shaders/Lighting/Punctual.glsl)

| 原点中心样点，Z 缩放 65/64 和 1.1 | 仅前表面，HDR 红通道 | 同图元加入被遮挡的后表面，HDR 红通道 |
|---|---|---|
| 真重合，点光强度 0.015625 cd | `0` | `0` |
| 6.25 cm，0.015625 cd | `0.06762695` | `0.06762695` |
| 10 μm，4 × 10⁻¹⁰ cd | `0.06756592` | `0.06756592` |

各样点基色对照为 `0.5`，D32 为 `0.51612902`。两档正距的参考照度均为 4 lux，HDR 读回在 `0.0002` 容差内。复跑大坐标节点与烘焙顶点的 6.25 cm 和 12.5 cm 对照、原缩放与重合样点。两图元继续使用各自世界基点，纯相机移动时保留对象行与灯表的复用。 [原生读回](../../../../build/d07-t03/a-correction-4/native.log)

从仓库根运行 `& ./build/d07-t03/a-correction-4/Verify.ps1`。用独立 UTF-8 Ninja Multi-Config 构建 Debug `Physara` 与 Release `PhysaraRenderer`，运行匹配产物的 V1–V4 及 CR-001 至 CR-005 定向回归。Debug 产品以 `--hidden --poll-limit=6` 启动，退出码为 0。原生读回覆盖单台 Intel RaptorLake-S Mobile Graphics Controller 的 Debug OpenGL 4.6.0，驱动 32.0.101.6129。Release `NDEBUG` 覆盖 CPU 边界。 [回放脚本](../../../../build/d07-t03/a-correction-4/Verify.ps1) · [Debug 构建](../../../../build/d07-t03/a-correction-4/build-debug.log) · [Release CPU](../../../../build/d07-t03/a-correction-4/v1-Release.log)

正式 `StaticForward` 与 `StaticDepth` 包的生成接口和阶段反射通过工具校验，共用 ABI `758815c74785ab4ec72dc4898ca4989b3e4b26e014697e052bef4a4570341ffb`。对象行为 240 字节，世界基点、平移差的两部分及线性系数余量位于偏移 144、160、176、192。View 行为 128 字节，灯行 stride 为 64 字节，容量为 16384。回放核对 1594 个实际正式输入、26 项包依赖及包、库、可执行文件和标样的身份。回交清单 SHA-256 为 `8f8dc7b1d1ca4d26e2ce95faa6e998f5d727ab11e42997556e7205e9253a175e`。 [回交清单](../../../../build/d07-t03/a-correction-4/handoff.json) · [运行输入清单](../../../../build/d07-t03/a-correction-4/run-inputs.json) · [跟踪文件差异](../../../../build/d07-t03/a-correction-4/a-source.diff)

<a id="b-final-signoff"></a>
## 第 1 轮 B 最终复核与签收

B 签收代码与注释，五项正式问题全部关闭。基准为 `v2.0` 的 `7697cb45c0592d63633e46edff862ab763d3c417`。A 本次交接的 1594 个正式输入与工作区匹配。B 补注释后的 23 个涉审源码及构建输入汇总 SHA-256 为 `3e5c53be0af8b294cb349bfc64eaa9f98f7f37a31a06138755112888145a4a2d`。按仓库相对路径排序，将每行 `路径<TAB>原始文件 SHA-256 小写<LF>` 以 UTF-8 拼接后求摘要。 [B 签收源码清单](../../../../build/d07-t03/b-origin-review/signed-sources.json) · [审查与注释覆盖](../../../../build/d07-t03/final/review-signed.md#b-final-signoff)

| 问题 | B 在签收版本实际确认的结果 |
|---|---|
| CR-001、CR-002 | 显式极小范围及正强度按实体和字段拒绝。三步窄锥中点红通道为 `0.01690674`，参考 `0.01691021`。超出编码精度的锥角有明确诊断。 |
| CR-003 | 同一可见表面使用单面或同图元带遮挡后表面的 glTF，在正交和透视相机下，真重合读回均为 `0`，6.25 cm 均为 `0.06762695`。正式回放中的 10 μm、4 × 10⁻¹⁰ cd 样点为 `0.06756592`，符合 4 lux 参考和 `0.0002` 容差。大坐标、两种缩放、两个图元的基点及同槽复用对照通过。 |
| CR-004、CR-005 | 编辑器分别报告灯表和表面拒绝。请求 Direct Lighting 遭拒或仅提交 UI 时仍显示已接受 Base Color，正常图实际采样并发布后才显示已接受 Direct Lighting。 |

B 在 7 个文件补充了对象行、共同世界基点、顶点乘加余量和片元重组的注释。复用与 A 回交身份匹配的 Debug C++ 库，用补注释后的源码重编并校验 `StaticForward` 和 `StaticDepth`。两包四个阶段的 SPIR-V 及反射与 A 回交一致，ABI 为 `758815c74785ab4ec72dc4898ca4989b3e4b26e014697e052bef4a4570341ffb`。以上原生回放使用重编包和一台 Intel RaptorLake-S Mobile，Debug OpenGL 4.6.0，驱动 32.0.101.6129。Release 实跑 V1 CPU 编码边界。后续 6.1–6.3 由 A 完成。 [独立读回](../../../../build/d07-t03/b-origin-review/signed/b-readback.log) · [原生与编辑器回放](../../../../build/d07-t03/b-origin-review/signed/native.log) · [Release CPU](../../../../build/d07-t03/b-origin-review/release-cpu.log)

<a id="final-verification"></a>
## 最终验证与证据

2026-11-18，A 复算 B 签收的 23 项实际输入，汇总 SHA-256 为 `3e5c53be0af8b294cb349bfc64eaa9f98f7f37a31a06138755112888145a4a2d`。其余正式输入与 A 回交清单逐项匹配，1594 项实际正式输入中仅 B 补注释的 7 个文件变化。已按原始字节留存问题单和完整注释覆盖记录，快照 SHA-256 为 `29d737581e1445234a5b60f5366265cdff276048461f8104d3fdfde3567c49b1`，快照的相对链接以 `Docs/` 为基准。 [签收问题单快照](../../../../build/d07-t03/final/review-signed.md) · [源码清单](../../../../build/d07-t03/final/source-manifest.json)

从仓库根执行 `& ./build/d07-t03/final/Verify.ps1`，在独立 UTF-8 Ninja Multi-Config 目录以 Windows x64、MSVC 19.44.35228.0 和 Windows SDK 10.0.26100.0 重建 Debug `Physara`、Release `PhysaraRenderer` 及探针。复用已核对字节的标样和探针源码。正式程序以 `--hidden --poll-limit=6` 启动并以 0 退出。最终两份 Forward 包与 B 签收件逐字节一致，ABI 为 `758815c74785ab4ec72dc4898ca4989b3e4b26e014697e052bef4a4570341ffb`。回放前后核对正式输入、实际包、库、exe、标样和 26 条包依赖。最终证据清单的 SHA-256 为 `76a667830aac04ed53df5f0ba655f11577caab6700d16602f5fee5e31c0b4314`。 [回放入口](../../../../build/d07-t03/final/Verify.ps1) · [证据清单](../../../../build/d07-t03/final/handoff.json) · [运行输入](../../../../build/d07-t03/final/run-inputs.json)

| 规范要求 | 实现与场景核验 |
|---|---|
| 完整可见灯集 | V1 通过正式 glTF、SceneFrame、保守可见性及灯行编码核对相交有限灯、缺省范围、方向灯及隐藏灯。 [场景提取](../../../../Engine/Renderer/Prepare/SceneFrame.cpp) · [View 工作集](../../../../Engine/Renderer/Prepare/ViewPreparation.cpp) |
| 当前灯表交付 | V2 核对生成布局、写后块范围、在途两槽、同槽刷新与重试，零灯读回为零。CR-004 和 CR-005 在正式编辑器复放拒绝、旧输出保留及正常帧确认。 [灯表](../../../../Engine/Renderer/Lighting/PunctualLights.cpp) · [Forward 图](../../../../Engine/Renderer/Paths/Forward.cpp) · [帧回执](../../../../Runtime/Frame.cpp) |
| 标准单位及衰减 | V3 核对方向光位置独立、点光 2 m 与 4 m 的反平方和有限范围、聚光内外锥及余弦中点。CR-001 至 CR-003 的编码、窄锥、共点和缩放样点均通过。 [逐灯计算](../../../../Shaders/Lighting/Punctual.glsl) |
| Unlit 与诊断视口 | V4 复放 v1 场景、从 Base Color 请求 Direct Lighting、正常图接受和切回 Emissive，核对 Unlit 与显示限幅。两条既有诊断场景的取样和存储复用沿用已验路径，以正式诊断分支和对象键复核。 [诊断着色](../../../../Shaders/Material/StaticPreview.glsl) · [缓存键](../../../../Engine/Renderer/Draw/StaticDraw.cpp) · [设置面板](../../../../Editor/Panels/Workspace.cpp) |
| 当帧预曝光受光 | V3 及 V4 核对直接光和自发光相加、+1 EV 翻倍、零灯、AO 隔离、正交与透视的观察方向，以及两种不透明 Pass 的 MASK 与 Reverse-Z 一致性。 [颜色入口](../../../../Shaders/Passes/Forward/Color.frag) · [深度覆盖](../../../../Shaders/Passes/Forward/Depth.frag) |

两份增量的 5 项要求及 11 个场景均已核对实现和验证证据，生成接口、图内交付及编辑器入口与设计一致。OpenSpec 1.13.2 的严格变更校验通过。V1–V4 复验通过，原生回放覆盖单台 Intel RaptorLake-S Mobile Graphics Controller 的 Debug OpenGL 4.6.0，驱动 32.0.101.6129。Release `NDEBUG` 覆盖 V1 CPU 边界。 [严格校验](../../../../build/d07-t03/final/validate-change.json) · [原生回放](../../../../build/d07-t03/final/native.log) · [Release CPU](../../../../build/d07-t03/final/v1-Release.log)

<a id="spec-sync"></a>
## 规范同步

新增 `punctual-lighting` 的 3 项要求，为 `static-forward-rendering` 新增 1 项受光要求，并更新原诊断预览要求。保留 Forward 主规范原有用途、其他要求以及诊断切换和超白自发光两个原场景。逐条确认主规范与增量要求和场景一致，`openspec validate --specs --strict --json` 通过全部 32 份主规范。 [灯光主规范](../../../specs/punctual-lighting/spec.md) · [Forward 主规范](../../../specs/static-forward-rendering/spec.md) · [校验结果](../../../../build/d07-t03/final/validate-specs.json)

<a id="archive-closeout"></a>
## 归档收尾

`openspec archive add-forward-direct-lighting --skip-specs --yes --json` 已将 change 和 `.openspec.yaml` 移至 `2026-11-18-add-forward-direct-lighting`，两份主规范已在归档前同步。归档相对链接已核对，D07-T03 登记完成，开发计划进入 D07-T04。验收证据留存后，当前问题单按模板恢复。 [归档结果](../../../../build/d07-t03/final/archive.json) · [开发计划](../../../../Docs/Developments/Developments.md#current-stage) · [当前问题单](../../../../Docs/CodeReview.md)
