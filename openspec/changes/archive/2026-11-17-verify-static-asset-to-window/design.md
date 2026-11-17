# Design

## Context

[提案](proposal.md) 对应 [D06-T07 与阶段出口](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t07)。规划基线为 `81b7beed292707d848245a04f10fe5fb05b7f3f4`（D06-T06），开始规划时工作区干净。证据应覆盖各所有者交界，沿 [按合同验证行为](../../../../Docs/Architecture.md#tests)、[能力闭环](../../../../Docs/Architecture.md#feature-closure) 和 [取消与关闭](../../../../Docs/Architecture.md#shutdown) 追到接受结果与退出。

| 已有边界 | 本次验证的交接结果 |
|---|---|
| AssetManager 固定完整的 CPU 模型版本。EditorSession 按导入请求和活动文档提交实例事务。 | 匹配的不可变记录、完整实例、独立的场景与资产身份。 [CPU 发布](../../../../Engine/Asset/AssetManager.cpp) · [导入会话](../../../../Editor/Session/SceneImport.cpp) |
| Scene 提供类型化实体与内容版本。RenderFrame 在安全槽提取 SceneFrame，由 ViewWork 准备相机、范围和可见项。 | 编辑与镜像几何通过同轮准备进入真实绘制，Scene 的快照边界只提供领域数据。 [帧编排](../../../../Runtime/Frame.cpp) · [视图准备](../../../../Engine/Renderer/Prepare/ViewPreparation.cpp) |
| Forward 准备静态 GPU 数据与 Draw。完整图接受且 UI 实际采样才确认新的场景预览和本图上传；宿主独立报告图形接受、交换及完成。 | 视口颜色、窗口像素和版本属于同一次正常帧。旧帧的资源按真实完成退休。 [Forward](../../../../Engine/Renderer/Paths/Forward.cpp) · [窗口呈现](../../../../Platform/Presentation.cpp) |
| Runtime 在继续轮次先发布 CPU 结果，终止轮次直接进入会话和作业关闭。 | 关闭时截止发布和收拢 CPU 作业，已发图形工作沿宿主的关闭协议完成。 [运行循环及关闭](../../../../Runtime/Main.cpp) |

## Goals / Non-Goals

四组检查各自说明可判定的失败和当前阶段的最小证据。实机场景使用正式资源包、相同会话、同一帧图和宿主；观测、合成样例和回放都在 `build/d06-t07/` 中构建及执行。D07 后续通过现有产品入口消费验收通过的静态输入与帧链。

## Decisions

### 1. 用一份带明显差异的小场景贯穿解码、图像和编辑

- 生成固定的 `.gltf`、外置 BIN 和小型 PNG。场景包含前层核心材质 MASK 与后层 Unlit OPAQUE Primitive，具有可辨认的材质颜色、遮罩孔和退后的重叠区域。用一次 sparse POSITION 替换形成非对称轮廓，CPU 核对已知替换值及未替换哨兵，窗口选取由替换新增的内部区域。输入使用有明确偏移的合法 accessor 和有效索引。
- 用两套指向不同色块的 UV。法线贴图使用 UV1 且输入缺少源切线，制作共享角点的镜像 UV 接缝和偏向一侧的线性法线。已知顶点、绕序、手性和法线预期从合成输入独立预先写定。两套 UV 的内部标样与一处颜色/数据中间值能暴露错用 UV、纹理行反向、错解码及重复显示编码。图像探针取样远离三角边界、遮罩边界和 UI 覆盖，记录坐标、颜色域与明确容差。
- 从同一模型导入两次得到双实例。一个保留原变换，另一个在不同位置做有符号的非均匀缩放，便于比较镜像实例的面向、世界法线及独立编辑。模型带正交与有限透视相机。固定测试布局使用与相机比例不同的画布，核对有效矩形和留边。例用 2:1 投影、正交 `xmag=2, ymag=1` 与透视垂直视场 90°，在相机空间的 `(1,0,-2)` 和 `(1,0,-4)` 中，正交横向 NDC 均为 0.5，透视横向 NDC 分别为 0.25 与 0.125。近远遮挡标样用有效矩形中的颜色与反向深度关系判定。
- 复制最少的描述数据到独立的坏资产相对路径，留下一个缺失的外置依赖，确定触发真实导入失败。生成输入使用本地合成内容，记录生成参数、相对目录和文件摘要。GLB、更多拓扑、图像与边界编码沿用前置规格及 D06-T02 的相应验收范围。 [D06-T02 证据](../2026-11-16-import-static-gltf-models/tasks.md#final-verification)

### 2. 实际编辑输入、接受的图和窗口结果使用同一条链

按 [构建与运行说明](../../../../Docs/Build.md#运行) 准备与可执行文件同级的资源根和实际 Shader 包。主回放经过真实 EditorSession 的 File、Hierarchy、Inspector、Edit 与 View 面板动作，使用正式 Runtime 帧编排和 GL 宿主。开发探针可以链接本配置的正式对象和库并驱动会话输入，产品短运行另外核对唯一入口的装配和退出。选择实体相机及非默认渲染比例后保存，切到不同请求再用正式 Open 读取实际 JSON。核对稳定实体、TRS、资源引用、主 View 相机及请求比例各自恢复和生效，再执行一笔可见编辑。

场景像素从已接受的 View 图像取得，窗口像素从相同提交的原生默认后缓冲取得。可沿 D04-T03 的本地窗口观测边界，在复制完成至真实 SwapBuffers 之间读取有限标样，保留实际交换结果。将 View 代、同一帧实际使用的 UI binding 与颜色接受、提交票据、交换结果和样本对应起来。单独的 CPU Ready、`FrameStatus::Rendered` 或非零票据分别只证明各自的阶段。像素读回与 Pending 寿命场景使用独立时序，避免同步读回使寿命检查提前完成。 [窗口观测先例](../2026-11-11-establish-opengl-window-presentation/tasks.md#final-verification) · [帧结果合同](../../../../Runtime/Frame.hpp)

### 3. 以可观测次序控制取消、退出和退休

- 先确认导入为 Loading，再推进一轮会话取消，随后放行该请求的完成/采用顺序。只取消编辑器导入时，共享 CPU 作业仍可提供有效缓存，取消的会话操作应产生零实体及零文档事务。重复导入按同源同选项核对固定版本复用与独立实体身份。实际失败应有可见原因、零新场景提交和可继续使用的旧状态。用正式发布边界控制顺序，记录完成是运行中还是已待取。
- 对真正的关闭输入，在资产作业仍运行或有完成待取时进入 Runtime 的终止分支，观测停止、取消、join、零终止轮次发布以及窗口正常退出。需要稳定的时序时沿用本地输入回放及 CPU 作业顺序控制，以实际队列和宿主结果取证。 [原发布边界与关闭证据](../2026-11-15-publish-versioned-cpu-assets/tasks.md#final-verification)
- 从实际绘制的一份已接受 View 输出开始，在原提交尚待退休时删除实例并连续改变 View 尺寸，另取一次真实窗口 framebuffer 尺寸变化。保留 View token 不变，观察当前请求、旧有效输出代、已接受的新图和槽退休次序。自然 Pending 难以稳定复现时，沿 D04 的完成观察边界在本地探针中暂缓一次完成观察，随后依赖原生 Event/fence 的完成。报告受控 Pending 和实际完成，关键保活归正式帧槽、图形提交与 View 所有者。用于比较的观测引用在退休断言前释放。 [真实完成与槽复用](../../../specs/frame-slot-lifecycle/spec.md)

<a id="verification-matrix"></a>
## 验证矩阵

| 检查 | 通过条件 | 判定依据 |
|---|---|---|
| V1 · 标准输入到完整场景 | 验证 Primitive/材质对应、UV0/UV1、稀疏替换及镜像接缝切线。重复请求的依赖和 CPU 版本一致，两个实例的稳定身份、局部变换和选择独立。Scene/SceneFrame 边界只传递领域数据及固定 CPU 引用。 | [glTF 导入](../../../specs/static-gltf-import/spec.md) · [静态 Scene](../../../specs/static-scene/spec.md) · [SceneFrame](../../../specs/scene-frame-preparation/spec.md) |
| V2 · 图像到窗口与场景读写 | 由真实界面导入，核对 UV、轮廓、镜像法线、MASK 孔、近远遮挡、两投影及留边的代表性像素。对同一正常帧证明图内资源生产、View 采样和原生交换。变换、Undo/Redo、保存重开及重开后编辑均在身份、文档内容和新接受画面上留下可核对结果。 | [View 准备](../../../specs/view-preparation/spec.md) · [静态表面](../../../specs/static-surface-evaluation/spec.md) · [Forward](../../../specs/static-forward-rendering/spec.md) · [编辑器帧图](../../../specs/editor-frame-graph/spec.md) · [原生呈现](../../../specs/opengl-window-presentation/spec.md) · [编辑事务](../../../specs/editor-domain-edits/spec.md) · [JSON 持久化](../../../specs/scene-document-persistence/spec.md) |
| V3 · 取消、失败及关闭 | 确认取消操作未采用迟到结果；同源合法重试后仍可渲染。坏依赖给出当前操作的失败，活动场景与已有有效资产保持完整。待处理导入遇到关闭时不发生终止轮次发布，作业收拢、退出码和本轮窗口行为相符。 | [CPU 发布](../../../specs/cpu-asset-publication/spec.md) · [编辑器导入](../../../specs/editor-domain-edits/spec.md) · [Runtime 生命周期](../../../specs/runtime-lifecycle/spec.md) |
| V4 · 快速删除和 resize | 实际或受控的 Pending 观察下，旧槽的上传和对象引用保持有效，View token 保持身份。普通新图只确认匹配的最新布局和场景。如保留上一有效图，它保持旧代语义且无当前输入资格。完成或关闭收拢后才允许退休，删除后的像素和另一实例状态符合预期，撤销可恢复。 | [静态驻留](../../../specs/static-gpu-residency/spec.md) · [视口输出](../../../specs/editor-viewport-output/spec.md) · [帧槽寿命](../../../specs/frame-slot-lifecycle/spec.md) · [原生呈现](../../../specs/opengl-window-presentation/spec.md) |

<a id="support-profile"></a>
## 当前支持 profile

| 层次 | D06 已采纳范围及本轮代表 |
|---|---|
| 静态输入 | `Assets/Models` 下只读 glTF/GLB、受控外部或容器内数据、合法三角拓扑、标准 PNG/JPEG。CPU 可解码本阶段四个已采纳扩展的标准描述，未知 required 或选定实例实际使用的 Skin/Morph 由 Asset 拒绝，动画数据仅报告。V1 使用外置依赖的 glTF 与明确的 UV/切线/稀疏输入。 [导入与报告合同](../../../specs/static-gltf-import/spec.md) |
| 预览与呈现 | 正交、透视和导航相机；单样本 Reverse-Z Forward；OPAQUE/MASK、Unlit 及 Base Color、World Normal、Material Channels、Emissive 预览；SDR 窗口。V2 验收 Base Color 与 World Normal 的代表性图像，材质通道、自发光及透明不支持诊断沿用 T05 已有覆盖。灯源描述保留在 CPU，物理求值按 D07。 [T05 原生验收](../2026-11-16-render-static-scenes-forward/tasks.md#final-verification) |
| 编辑和文件 | 活动文档中的场景层级、TRS、删除与历史；`Assets/Scenes/*.scene.json` v1 保存及原子打开。模型正文仍由模型文件提供；场景文档保存引用、领域状态与主 View 支持的请求。 [场景持久化](../../../specs/scene-document-persistence/spec.md) |
| 设备与运行 | Windows x64、MSVC Debug/Release，实际窗口系统与 OpenGL 4.6 Core。证据范围记录实际设备、驱动、窗口属性、观测精度及未覆盖环境。 [产品入口](../../../../Docs/Build.md#运行) |

## Risks / Trade-offs

- 合成模型几何过于对称时，镜像、UV 错配或默认相机仍能生成貌似正确的图。为每个独立区别预定可采样位置和数值，优先复用同一模型、同一画面中的标样。
- 隐藏窗口的交换和读取仍依赖真实窗口系统。把正常图、交换与完成分别记账，使用启动身份、实际 GPU 观测与退出结果验证，保留必要环境缺口。
- 异步作业和 GL fence 的先后受调度影响。以已核对的 Loading/待取/完成观察和实际票据安排操作，保留实际触发和受控时序的差异。
- 现存正式产物或历史日志可能对应不同源。每个配置收集当前正式构建输入、Shader 包、实际执行文件、fixture 和命令身份。能匹配且能回答同一检查时复用；证据缺失时构建当前输入并补足该组。

## Migration Plan

1. 在工作包临时目录组织当前输入和最小回放，按 V1–V4 验证阶段出口。已采纳合同出现偏差时保留失败复现，在本设计记录最小修复及定向回归。
2. A 本地检查后交接 B。B 签收实际输入后完成最终 verify，记录支持范围、验收与关闭证据。按 [工作包状态规则](../../../../Docs/Developments/Developments.md#current-stage) 更新进度并归档；保持 `skip_specs: true` 时直接沿原生无 delta 流程关闭。
3. 本地开发材料可随 build 清理。复现用留存的输入清单和回放步骤，并按交接提供所需本地文件。已归档的前置证据继续用于其原覆盖范围。
