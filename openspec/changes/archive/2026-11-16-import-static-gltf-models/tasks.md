# Tasks

## 1. 容器、静态选择与依赖读取

- [x] 1.1 A 私有接入固定 TinyGLTF C11，添加 opt-in 元数据读取与原字段校验，集中验证 glTF 版本和 GLB 块结构。绑定借用与释放寿命，保留默认解析入口和许可。按 R1 核对合法 GLB 与错误块序列、声明长度，以及元数据模式在确定依赖前不触发外部读取。记录实际适配和构建来源。
- [x] 1.2 A 选择源 scene，校验所选层级、局部 TRS／矩阵、引用、扩展声明和静态约束，收集实际会消费的资源。按 R2 验证确定选择、合法负缩放、活跃及未使用的变形、初始姿态与支持报告，确保非法层级或缺失回退得到定位。
- [x] 1.3 A 按实际依赖解析模型目录相对 URI、data URI 和 GLB BIN，和 Core 受控快照建立单一字节读链。检查 MIME、长度、累计预算和规范路径，保留实际参与输入的指纹。按 R1 验证外部、内嵌及拒绝事实，按 R6 核对整版指纹由实际字节决定。

## 2. 标准数据与可消费几何

- [x] 2.1 A 构建活动模型的核心材质、图像、纹理槽、sampler 和扩展描述，验证其引用、数值、缺省和有效 UV 集。复用 T01 图像解码和保有预算，输出透视／正交及三类 punctual 灯的标准描述。按 R5 核对共享像素的不同用途、每槽 UV/transform、相机可选值、灯的单位和显式非法参数。
- [x] 2.2 A 建立经过验证的 accessor 视图，按小端读取和叠加 sparse，检查 offset、stride、语义、尺寸、count、对齐、归一化、整数窄化及顶点索引。按 R3 用独立预期核对真实数据和范围拒绝，验证 POSITION extrema 和局部 bounds 的对应关系。
- [x] 2.3 A 实现三种三角拓扑及逐面法线，随实际切线生成接入固定 MikkTSpace。按角点结果拆顶点并重建索引，保留源身份、属性和有效 handedness。按 R4 验证绕序、缺 NORMAL 时忽略源切线、镜像 UV 两侧的角点输出及缺 UV 的定位诊断。

## 3. 原子发布与消费集成

- [x] 3.1 A 在现有 CPU 资产 owner 交付有类型的模型请求和只读模型版本，共用作业队列及发布、取消和关闭入口。完整版本含所选 scene、结构、数据和支持报告。按 R6 验证外部依赖换代、相同输入复用、失败／取消／过期结果及旧版本保活。
- [x] 3.2 A 用 Windows x64/MSVC 的 Debug 和 Release 构建并运行实际 CPU 模型消费者，核对受影响的正式 Physara 产品、私有 C11 库链接和原有图片导入。按 R7 验证正式构建和应用启动／退出，更新实际 CPU 入口、支持 profile、必要构建说明与依赖来源。

## 4. A 本地验收与交接

- [x] 4.1 A 核对 D06-T02 包内条件、增量规范场景与最小验证结果，执行 OpenSpec 严格校验、链接和 diff 检查。在本 tasks 记录执行、复用、未覆盖及相应输入／产物身份。将实际范围登记到 [当前问题单](../../../../Docs/CodeReview.md)，开发计划标明待独立审查。

## 5. 独立审查、整改与复核

- [x] 5.1 B 由维护者调度，独立核对实际 diff、直接消费者、C11 适配和构建来源。通读本轮完整第一方文件，补齐职责、接口合同和关键流程注释。由 B 在问题单记录发现、覆盖和结论并勾选。
- [x] 5.2 A 按稳定问题编号整改清单，运行实际受影响路径的最小回归，保存改动和证据。没有整改项时按实际记录不适用，由 A 勾选。
- [x] 5.3 B 复核当前代码、验证证据和完整文件注释，分别确认代码正确性及注释可读性。只有 B 签收最终版本后勾选。

## 6. 最终验证、同步和归档

- [x] 6.1 A 在 B 签收最终版本后核对 D06-T02 的全部目标与场景。按 R1–R7 对签收输入执行或复用适用的 CPU、构建与实际产品检查，记录范围及最终 verify 结果。
- [x] 6.2 A 在本 tasks 留存签收范围、最终源码和产物身份、场景证据与审查闭环，按官方流程同步 `static-gltf-import` 主规范并通过严格校验。
- [x] 6.3 A 在验收满足后完成归档及引用校验，登记 D06-T02 完成并更新到前置齐备的 D06-T03。证据留存、归档成功后按模板重置当前问题单，再核对最终状态。

## 验证范围与停止条件

以 [静态 glTF 规范](specs/static-gltf-import/spec.md) 和 [既有 CPU 资产验收](../2026-11-15-publish-versioned-cpu-assets/tasks.md#final-verification) 为基线。用共享的代表性输入组合覆盖以下风险，按 [验证技能](../../../../.agents/skills/physara-verification/SKILL.md) 记录实际执行、复用和范围。

| 风险组 | 最小检查及独立预期 | 停止条件 |
|---|---|---|
| R1 容器、URI 和依赖范围 | 用同一小型几何／像素预期核对相对外部源、base64 data URI 和 GLB BIN＋图像 bufferView。至少一次包含合法的模型目录相对引用及转义。以错误块范围／顺序、一个被拒 URI 和小累计预算分别证明容器、路径、预算的拒绝，合法不用的其它 scene 资源没有被读取。 | 正式消费者确实使用 Core 所校验的打开对象时复用 T01 的打开句柄证据。覆盖模型解析差异后停止，不重复通用 I/O 矩阵。 |
| R2 静态选择、变形与扩展 | 用一个多 scene 输入核对默认、显式选择、无法自动确定、负缩放和初始姿态。将合法 Skin/Morph 位于未选场景与选中节点两种状态作对照。用一个有核心回退的 optional 扩展证明降级报告及 required 的拒绝，将无完整核心回退和层级错误各取代表性拒绝。 | 已证明选择和结构范围、真实变形拒绝、fallback 与 required 差异，未选合法数据不会误触发静态拒绝。 |
| R3 accessor 字节和数值 | 组合非零 offset、交错 stride、归一化 UV 和 sparse 叠加，其中有基底与无基底各一。用能手算的元素/索引/bounds 作预期。选有独立缺陷的字节范围、非法索引和非递增 sparse 索引，检查定位与无发布。 | 已证明实际读视图、数值和覆盖顺序正确，保留能区别安全失败的代表性输入。 |
| R4 绕序和几何基 | 用非对称条带／扇核对显式三角形及绕序。相邻非共面三角形的逐面法线有已知方向，并故意提供应忽略的源切线。镜像 UV1 正例核对拆顶点后的索引、切线符号与保留属性。点／线与缺有效 UV 不生成假结果。 | strip/fan 绕序、法线生成、真实角点拆分及 UV 拒绝都有可观察证据，无需为每种网格做图像测试。 |
| R5 材质、相机与灯 | 同图像跨 sRGB／线性角色且 sampler 或 UV 不同，核对共享像素、独立语义、有效变换 texCoord 及核心缺省。组合透视／正交和方向／局部灯测试投影及标准单位，用显式零远面或零 range 等代表性输入防止缺省掩盖非法值。 | CPU 记录足够支持下游使用，纹理或相机的实际图像效果随后续工作包验证。 |
| R6 内容身份、失败和寿命 | 经真实 owner 入队、完成与发布。以复制到 build 的输入按顺序验证同内容刷新、只改参与导入的外部文件、有失败／取消的替换与受控迟到，确认上一有效模型和已固定引用不变。并安排一次模型与图片共用小额度，核对待取结果的背压和退出收拢。 | 同一数据链已经证明组装原子性、版本身份、取消与回收。只在出现不同缺口时追加交错。 |
| R7 正式消费者与构建 | 用现有 [gallinera_table](../../../../Assets/Models/gallinera_table/gallinera_table.gltf) 证明多 Primitive 与外部文件导入，使用 [Camera.gltf](../../../../Assets/Models/camera/Camera.gltf) 核对 optional transmission 的实际回退报告。检查 Debug/Release 受影响的 CPU 模型入口、普通 PNG/JPEG 和正式产品短启动。从新的 binary 目录验证普通配置只需要仓库输入。 | 使用签收输入对应的产物。再运行的范围限本次变化的消费者，历史图/RHI 矩阵随实际影响复用。 |

开发测试、脚本、输入副本和临时消费者置于 `build/d06-t02/`。仓库模型作为只读输入，记录复现所需的文件身份。运行前核对实际源码、构建输入、外部资源、fixtures 及二进制，失败保留日志。现阶段验收按 CPU 静态模型定义，原生检查限受影响的正式产品装配与启动。

<a id="a-local-verification"></a>
## A 本地验证与交接

在 Windows x64、MSVC 19.44.35228、Windows SDK 10.0.26100.0 上构建 Debug 和 Release 的正式 `Physara`。从独立目录配置 Ninja Multi-Config 并编译 `PhysaraAsset`，使用仓库内固定源码构建 `tinygltf` 和 `mikktspace`。两种配置均从另一工作目录运行正式 `Physara.exe --hidden --poll-limit=6`，退出码为 0。CPU 消费者链接对应配置的正式静态库，模型数据检查通过 `RequestModel`、`PublishCompleted` 和 `StatusModel` 执行。 [构建与运行](../../../../Docs/Build.md#cpu-资产导入)

| 风险组 | 实际结果 |
|---|---|
| R1 | 外部转义相对文件、base64 data URI、GLB BIN 和图像 bufferView 均交付一致的三角形、bounds 与 RGBA8 texel。拒绝错误块顺序及长度、非法 URI、MIME 冲突、非对象数组项、累计输入超限及 JSON／arena 额度不足。C11 对照确认默认 data URI 解码与元数据模式的延迟读取。 |
| R2 | 默认、唯一及显式 scene 稳定选择，初始 TRS、负缩放和父子关系可读。成功导入的 scene 跳过其它 scene 的变形与缺失依赖。选中 Skin、Morph、环、shear、缺少回退及不支持的 required 成员分别拒绝。 |
| R3 | 交错非零 offset 和 stride、normalized UV／颜色、稀疏基底覆盖、零基底及实际表面 bounds 均符合已知元素值。错误 stride、越界、重复 sparse 索引、错误 extrema 和坏索引均定位并拒绝。 |
| R4 | 有索引带、无索引扇保留期望绕序，剔除退化面并保留空槽。非共面逐面法线忽略源切线。镜像 UV1 生成正负 handedness 并拆分共享角点，原 UV 与颜色保留。点线 profile 以线拓扑拒绝作代表，并验证缺失 UV、退化 UV、焊接角点工作上限。 |
| R5 | 同一图像按 sRGB 和线性槽共享像素，sampler、UV 和变换独立。校验核心材质缺省、emissive strength、alpha、unlit 的有效依赖，以及透视／正交和三类 punctual 灯描述。显式零远面与零 range 拒绝。 |
| R6 | 相同源与依赖刷新在恰好占满像素及保有几何额度时复用旧版。只改外部图像或 bin 分别推进版本，失败、取消和 owner 析构后仍可读取固定版本。在临时源码副本的两处时序探针中，完成待取占满图像与模型共用额度，关闭收拢待取结果，旧模型候选在后继版本发布后到达仍被拒绝。 |
| R7 | `gallinera_table` 交付 3 个 Primitive／3 张图像，`Camera.gltf` 交付 5 个 Primitive／9 张图像并报告 transmission 的核心回退。普通 PNG/JPEG 图片请求及正式产品短启动／退出通过。 |

`build/d06-t02/a-local/result.json` 与同目录的 `runs.json` 保留命令、退出码及日志摘要。运行前后核对实际源码、测试输入与产物字节，包含工作树中的本轮新增文件。时序探针由 `PrepareOrderProbe.py` 从当前 `AssetManager.cpp` 和 `Jobs.hpp` 生成，常规模型消费者使用正式库。实际模型和验证输入见 `fixture-manifest.json`。

| 身份 | SHA-256 或 commit |
|---|---|
| 基准 commit | `00d5fec46bd7c929ffc1ca8f265ab7cd6ae91c4b` |
| 源码清单 `source-manifest.json` | `f9f0bf94807c95a4cf3e63509e14698260087606a94f536aa17ef4a826092a27` |
| 样例、fixture 与检查入口 `fixture-manifest.json` | `4734bf1eb0d7d1190edaeeb5b1c7047d6795e7078a06e94adc6ecdfabc384890` |
| 检查产物 `artifacts.json` | `cd12db91da2169efe904a00630c91e7f3ab1b5a543637d3058b08dbbc5e4fb12` |

受控读链复用 T01 已验收的同一打开句柄检查，当前场景覆盖模型 URI、容器及实际依赖的新增边界。验收范围为 CPU 模型数据和受影响的正式产品装配。Scene 实例化、GPU 模型上传和模型画面由后续工作包验证。 [T01 验收证据](../2026-11-15-publish-versioned-cpu-assets/tasks.md#final-verification)

OpenSpec 1.13.2 的 `validate --all --strict` 通过 24/24 项。核对 8 份相关文档中的 127 个本地链接，`git diff --check` 和本轮新增文件的空白检查通过。A 登记了该次交接的工作树和检查范围，供 B 独立审查与注释。

<a id="a-correction-cr-001"></a>
## CR-001 整改与自检

A 在所选几何的基底视图建立阶段传入源 accessor 下标，由 `Resources` 检查共享布局，在源指纹比较和几何构建之前拒绝缺少 `byteStride` 的非法共享。 [B 复核留存](#b-recheck-cr-001) · [读视图校验](../../../../Engine/Asset/GltfResources.cpp#L290) · [基底 accessor](../../../../Engine/Asset/GltfAccessor.cpp#L115)

使用 Windows x64、MSVC 19.44.35228 和 Windows SDK 10.0.26100.0 构建正式 `PhysaraAsset` 的 Debug 与 Release。两套回归消费者分别链接对应配置的正式库，通过 `RequestModel`、`PublishCompleted`、`StatusModel` 与 `InvalidateSource` 检查以下结果，构建、编译和运行均返回 0。

| 输入 | 两种配置的实际结果 |
|---|---|
| B 的 `byteStride: 12` 对照 | 发布一次，输出已知三角形及顶点值。 |
| B 的无 `byteStride` 对照 | `Failed`，诊断为 `/bufferViews/0/byteStride` 的 `InvalidModel`，发布计数为 0。 |
| 同一 POSITION accessor 被两个 Primitive 引用 | 无 `byteStride` 时成功发布，两个 Primitive 各输出一个三角形。 |
| 已发布模型的刷新输入改为非法共享 | 新请求返回相同字段错误，旧记录引用、revision 1 和三角形可继续读取，新增发布计数为 0。 |

本轮使用 B 的原始复现输入，并在 `build/d06-t02/a-correction/` 中建立重复引用和刷新对照。`VerifyCR001.py` 保存编译和运行结果，`result.json` 登记命令、退出码和日志摘要。36 项源码清单以 B 注释版为基准，实际源码差异为 `GltfResources.hpp/.cpp`、`GltfAccessor.hpp/.cpp` 和 `GltfGeometry.cpp`。本轮检查覆盖 R3 的布局边界和 R6 的失败刷新，最终验收按 6.1 执行。

| 清单 | 文件 SHA-256 |
|---|---|
| `source-manifest.json` | `4db65be0ebf896b08debdd0ba5e5feb73f9b01a73841d326b0106f1aba136631` |
| `fixture-manifest.json` | `9b6d5c316a567ccb00cfece503fd0c30692957f3990d367cf996e96c7763e4c4` |
| `artifact-manifest.json` | `8025729f2496495feb38b1c03dc0d57b183baa92acb4d04de7dcd2c93b2126ce` |

OpenSpec 1.13.2 严格校验通过 24/24 项。139 个本地链接、`git diff --check` 和本轮相关新增文件的空白检查通过。

<a id="b-recheck-cr-001"></a>
## B 复核与签收

2026-11-16，B 关闭 CR-001 [P2]，代码正确性与注释可读性分别通过。初审完整覆盖 `Engine/Asset` 22 份 C++ 文件、两处 CMake、C11 适配和直接合同；本次重新通读整改涉及的 `GltfResources.hpp/.cpp`、`GltfAccessor.hpp/.cpp`、`GltfGeometry.cpp`，其余 31 项清单输入与初审注释版一致。修正按文档内源 accessor 身份拒绝无 stride 的非法共享，允许同 accessor 跨 Primitive 复用。B 补充 `GltfResources.hpp` 的参数前提及 ViewUse 寿命说明，仅改注释。签收问题单副本保存在 `build/d06-t02/final/review-signed.md`。

B 核对 A 整改的 36 项源码、11 项 fixture、12 项产物清单和四份编译/运行日志，均匹配记录。随后复用现有四项检查，从当前正式源码重建 Release 消费者并独立运行：合法共享通过、非法共享定位拒绝、相同 accessor 复用通过、失败刷新保留同一旧记录和 revision 1。构建与运行退出码均为 0，运行前后源码/输入/产物身份一致；Debug 复用已核对的 A 证据。未新增行为用例，未复跑完整 R1–R7、产品 UI/GPU 或执行最终 verify。

签收基准 HEAD 为 `00d5fec46bd7c929ffc1ca8f265ab7cd6ae91c4b`，包含当前相关未暂存及未跟踪源码。`build/d06-t02/b-review/recheck-source-manifest.json` 的 SHA-256 为 `342f0fb4b274ec22896649cfdacbfed848e0f84efd4c341499df5656092b198d`；fixture 清单为 `eb0cb833ea2cd754ee50a94ac583337776972aa8322a357852488fd8179a2405`；`ninja/ReviewCorrections.exe` 为 `9c6e2266447162dd932c9ea8611b489bb7b1f3631f559a325dab0cec843fbf5f`。命令与日志摘要保存于同目录 `recheck-result.json`。B 本次补登已完成的 5.1 并勾选 5.3；A 仍须按 6.1–6.3 完成最终验证、同步和归档，之后才重置问题单。签收后若代码或构建输入变化，重新复核受影响范围。

<a id="final-verification"></a>
## A 最终验证

在 B 签收的 `v2.0` 工作树上运行 `python -B build/d06-t02/final/VerifySigned.py`。核对 36 项签收源码、B 的复核输入、日志与 Release 回归产物。工作树未暂存及新增源码与签收记录逐字节匹配，验证前后比对实际源码、fixture 和产物。逐文件摘要、命令、日志身份及复用记录保存在 `build/d06-t02/final/`。

| 清单 | 项数 | 集合 SHA-256 |
|---|---:|---|
| `source-manifest.json` | 1084 | `884fabe098998672156522c0134e4d2f04f03faa3fcb05bdf47480fd9252aeae` |
| `fixture-manifest.json` | 77 | `cd75ff28ebe31ad4449874eb4df017a1d8a0a218211f44614cc53db8a3342096` |
| `artifact-manifest.json` | 26 | `36eacbe4e72699e35db250992b34347ede4640572bed23e92ed91e1645d3aef4` |

Windows x64／MSVC 19.44.35228／SDK 10.0.26100.0 上重建 Debug、Release 的正式 `Physara`、CPU 消费者和时序探针。以新的 Ninja Multi-Config binary 目录配置及构建 `PhysaraAsset`，核对本地 TinyGLTF C11、MikkTSpace 链接。普通 PNG/JPEG 和两份仓库模型在两种配置均通过；从另一工作目录运行两份 `Physara.exe --hidden --poll-limit=6`，退出码均为 0。重跑 Debug 的 CR-001 四项回归，Release 复用 B 对同一签收输入的四项检查。15 条执行记录均成功，包括同步前 OpenSpec 严格校验 24/24。

| 规范要求 | 实现入口及最终场景证据 |
|---|---|
| 受控模型来源（3 场景） | R1 核对外部相对路径、data URI 和 GLB 的等价输出、容器与预算拒绝、失败定位。物理句柄约束复用 T01。 [容器](../../../../Engine/Asset/GltfDocument.cpp#L172) · [外部读取](../../../../Engine/Asset/GltfResources.cpp#L186) |
| 静态 scene（3 场景） | R2 核对确定选择、初始姿态和负缩放、未使用变形及依赖、活跃 Skin/Morph 与非法层级的拒绝。 [静态选择](../../../../Engine/Asset/GltfScene.cpp#L139) |
| Accessor（3 场景） | R3 和 CR-001 核对 offset、stride、normalized 与 sparse 数据、范围和索引拒绝，源 POSITION extrema 与局部 bounds。 [基底及稀疏视图](../../../../Engine/Asset/GltfAccessor.cpp#L115) · [共享视图](../../../../Engine/Asset/GltfResources.cpp#L290) |
| 三角形与切线框架（4 场景） | R4 核对带／扇绕序、退化面、逐面法线、源切线处理、镜像 UV1 拆点及 handedness，拒绝不支持的拓扑和采样集。 [拓扑转换](../../../../Engine/Asset/GltfGeometry.cpp#L211) · [逐角点结果合并](../../../../Engine/Asset/GltfGeometry.cpp#L320) |
| 核心材质与图像（2 场景） | R5 核对相同像素的 sRGB／线性用途、独立槽及 sampler、核心缺省、变换和有效 UV。 [纹理槽](../../../../Engine/Asset/GltfData.cpp#L125) · [图像解码](../../../../Engine/Asset/GltfData.cpp#L382) |
| 相机与 punctual 灯（2 场景） | R5 核对原投影可选值、灯类型、单位和取值，拒绝显式零远面及零 range。 [相机转换](../../../../Engine/Asset/GltfData.cpp#L226) · [灯转换](../../../../Engine/Asset/GltfData.cpp#L292) |
| 扩展与回退（2 场景） | R2 和 R7 核对 required 拒绝、无可用回退拒绝及可用核心回退诊断。`Camera.gltf` 实际发布 5 个 Primitive／9 张图像并报告 transmission 回退。 [扩展报告](../../../../Engine/Asset/GltfScene.cpp#L14) |
| 模型发布（3 场景） | R6 和 CR-001 核对共用额度、完成待取、依赖换代、相同输入复用、失败与取消、迟到结果丢弃及旧读者寿命。 [依赖指纹](../../../../Engine/Asset/GltfResources.cpp#L371) · [发布裁决](../../../../Engine/Asset/AssetManager.cpp#L350) |

| 验证维度 | 结论 |
|---|---|
| 完整性 | D06-T02 三项包内目标和 8 项规范要求均有实现，四份规划工件完整，独立审查任务及 6.1 完成。 |
| 正确性 | 22 个场景均核对实现路径及对应的代表性证据，CR-001 在签收版本闭环。 |
| 一致性 | 受控读链、选定 scene、容量与寿命、核心几何和材质转换、单一 Asset owner 及正式构建依赖与设计一致。临时消费者和时序探针保存在 build 下。 |

最终验证通过。覆盖范围为静态模型 CPU 接口、现有图片导入和正式产品装配及短启动。模型 Scene 实例化、GPU 驻留及画面验证由 D06-T03–T05 覆盖，通用文件句柄限制沿用 T01 验收。 [T01 验收证据](../2026-11-15-publish-versioned-cpu-assets/tasks.md#final-verification)

### 主规范同步

新增 `static-gltf-import` 主规范。Purpose、8 项要求和 22 个场景与 delta 逐项一致。OpenSpec 1.13.2 主规范严格校验通过 24/24，包含当前 change 的全量严格校验通过 25/25。 [静态模型导入主规范](../../../specs/static-gltf-import/spec.md)

归档前剩余 6.3，负责移动 change、校验归档引用、更新开发计划和重置当前问题单。完成这些动作后勾选该项。

### 归档及计划收尾

OpenSpec CLI 将 change 归档为 `2026-11-16-import-static-gltf-models`，保留 `.openspec.yaml`。主规范在 6.2 已完成同步和校验。归档后校正 proposal、design 和 tasks 的相对引用，保留 CR-001 整改、B 签收及 A 最终验证记录。开发计划登记 D06-T02 三项完成，当前工作包推进至 D06-T03。 [开发计划](../../../../Docs/Developments/Developments.md#current-stage)

归档链接核对通过后，将 `Docs/CodeReview.md` 恢复为模板并逐字节核对。最终检查通过 10 份文档的 213 个本地链接，180 项既有架构、主规范及归档文件和 1084 项源码输入与基准相符。OpenSpec 无活动 change，24 项主规范严格校验通过。归档任务最终为 15/15。
