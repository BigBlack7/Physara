# Design

## Context

`ViewDocumentRequests` 当前拥有导航位姿、可选相机实体和 render scale。`ViewWork::Prepare` 选择固定 60° 的导航投影或 Scene Camera，`CameraSnapshot` 交付稳定及渲染空间数据。`ForwardView` 当前打包 `clipFromWorld` 和模式。`View::Publish` 在正常图接受后将场景快照存入 `RenderedScene`，测试图只确认自己的显示输出。 [View owner](../../../../Engine/Renderer/View.hpp) · [相机准备](../../../../Engine/Renderer/Prepare/ViewPreparation.cpp) · [生成接口](../../../../Engine/Shader/Interface/StaticForward.json) · [接受边界](../../../../Runtime/Frame.cpp)

## Goals / Non-Goals

在同一 View 请求上确定可验证的光学和曝光值，沿已有会话、帧准备、上传及正常图接受边界交付它们。T02 交付预曝光输入及其来源记录。正式逐灯、自发光和 IBL 消费按 T03/T04 接入，显示与镜头效果各自按后续工作包验收。 [本 change 范围](proposal.md) · [工作包依赖](../../../../Docs/Developments/D06-D07-SceneAndForward.md#dev-d07)

## Decisions

### 1. View 持有参数，准备阶段确定有效镜头

物理参数是带显式单位的 `ViewDocumentRequests` 组成。Scene Camera 继续提供世界位姿和投影，模型内的 `yfov` 通过既有 Scene 解析进入 View。采用以下同一组缺省建立新 View 及兼容旧 v1 文档。

| 已接受请求 | 领域单位 | 缺省 |
|---|---|---|
| `fNumber`、`shutterSeconds`、`iso` | f 数、秒、ISO 感光度 | f/16、1/125 秒、ISO 100 |
| `exposureCompensationEv` | 曝光档 | 0 |
| `sensorHeightMeters` | 米 | 0.024 m，界面显示 24 mm |
| `navigationFocalLengthMeters` | 米 | 由 24 mm 和现有 60° 垂直 FOV 反推，约 20.78460969 mm |
| `focusDistanceMeters` | 米 | 10 m，本项目初始对焦距离 |

曝光三元组取固定 Filament 的初始值。导航默认镜头以现有的 `NavigationProjection` 为视场依据，沿用其 0.10 m 近平面、无限远渲染投影及现有 aspect 适配。领域计算统一用米和弧度，界面在输入输出边界换算 mm、m 和度。对焦距离在 T02 校验为有限正值，透视薄透镜的对焦距离与有效焦距关系由后续 DoF 在计算 CoC 时检查。 [相机合同](../../../../Docs/Architecture.md#camera) · [现有导航默认](../../../../Engine/Renderer/View.hpp) · [Filament 默认曝光字段](https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/details/Camera.h#L211) · [Filament 镜头参考](../../../../Docs/Reference/Filament/FrameSetup.md#f3)

- 导航用 `2 × atan(sensorHeight / (2 × navigationFocalLength))` 得到垂直 FOV，使用数值稳定的等价式。由现有投影路径继续生成稳定和渲染投影、留边和可见集。
- 场景透视直接消费有效 `yfov`，再用 `sensorHeight / (2 × tan(yfov/2))` 形成用于界面读取的有效焦距。有效投影的显示焦距超出数值表示时，界面采用不可用状态，投影仍使用 Scene 的权威值。导航焦距是该 View 的导航参数，切回导航时重新生效。
- 正交保持其 magnification、近平面、视线及 aspect 语义。有效镜头信息表示正交，View 光圈、快门、ISO 和曝光补偿仍可计算。同一准备合同交付相机和曝光。 [相机准备](../../../../Docs/Architecture.md#view-preparation) · [行为约束](specs/view-preparation/spec.md)

### 2. 计算一次标定倍率并随请求发布

集中求值当前物理请求。光圈、快门、ISO 和三项长度为有限正值，曝光补偿为有限档数，导航镜头需给出有限且位于 `(0, π)` 内的垂直 FOV。曝光在 double 的对数域计算，等价于下式。

```text
EV100 = 2 × log2(N) − log2(shutterSeconds) + log2(100) − log2(ISO)
effectiveEV100 = EV100 − exposureCompensationEv
preExposure = 2^(−effectiveEV100) / 1.2
```

正一档令倍率加倍。快门秒数作为曝光输入。用对数域避免先形成易溢出的光圈平方和比值，转换前检查派生值以及实际 FP32 正规数范围。拒绝时沿已有有界诊断携带字段名称或倍率原因，View 和文档事务保持此前已接受的值。文件类型及对象形状由 JSON 边界检查，物理范围及派生合法性归 View 的同一求值入口。

已校验的倍率及名义 EV100 随新请求原子发布。View 可以向界面和帧准备公开这一份派生值。相机快照携带本轮有效光学参数和同一 FP32 倍率。倍率来源于准备快照，Shader 消费直接读取该值。 [项目标定](../../../../Docs/Architecture.md#camera) · [Filament Exposure](https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/Exposure.cpp#L26)

### 3. 已接受帧、生成 ABI 与请求版本各有来源

- 用现有 View 文档 revision 标识物理请求，`PreparedView` 把相应请求代交给场景候选，用于确认时核对物理值来源。保留既有相机位姿和引用的版号规则，导航有效视场变化同步推进相机代。纯曝光和仅显示焦距的变化由文档代标识。render scale 的布局代、Scene 内容代和时域 key 各有自己的生命周期。
- 在权威 `StaticForward.json` 的 `ForwardView` 增加 `float32 preExposure`，沿 `PrepareViewRow` 与图内上传交付同轮 `clipFromWorld`、模式及倍率。把新增值纳入已上传行内容的比较，覆盖相同矩阵和模式下只有曝光改变的帧。按生成合同更新 `StaticForward` 与 `StaticDepth` 的配套包，并通过反射及原生读回验证。当前预览色的解释由既有静态 Forward 规范定义。生产光照、环境、发射和后处理按 D07 后续工作包使用该曝光域。 [当前行编码](../../../../Engine/Renderer/Draw/StaticDraw.cpp) · [Shader 生成合同](../../../../Docs/Architecture.md#shader-build) · [静态预览](../../../specs/static-forward-rendering/spec.md)
- `View::PreparePreview` 继续以当前帧相机快照形成场景候选，`View::Publish` 以匹配的请求身份和正常合成的接受回执确认 `RenderedScene`。从这一记录读取最后成功渲染值，首次确认前显示待确认，有旧场景结果时明确标识其归属。当前合法请求来自 View owner，安全图的上次有效颜色和无场景测试图按其各自用途显示。保留 `currentPreExposure` 和已接受值为后续重标定输入。 [View 发布入口](../../../../Engine/Renderer/View.cpp) · [时域合同](../../../../Docs/Architecture.md#temporal-contract) · [视口确认](specs/editor-viewport-output/spec.md)

### 4. 复用既有字段编辑和 v1 场景会话

- View Settings 使用相应单位提交字段值。界面的提交意图由会话核对当前 Scene/View，在已有编辑边界合并到实际 View 请求，并通过 `EditMainView` 及 `EditHistory::ApplyView` 接纳。输入单位转换只有一处。当前曝光和 EV100 来自 View 已接受请求，历史曝光读自匹配的已渲染记录，实体 `yfov` 及其有效显示焦距沿同一镜头求值。离散提交沿用现有 Undo/Redo 记录及文件打开期间的冻结规则。 [编辑历史](../../../../Editor/Commands/EditHistory.cpp) · [会话入口](../../../../Editor/Session/EditorSession.cpp) · [View Settings](../../../../Editor/Panels/Workspace.cpp) · [编辑行为](specs/editor-domain-edits/spec.md)
- 在 v1 `mainView.requested` 增加可省略的 `physicalCamera`。Writer 写入本设计表格中的七个请求字段，全部是 canonical 单位的字段值。Reader 对整个对象的省略使用同一个 View 缺省值，对显式对象要求字段完备、已知且值域有效。EditorSession 将 Scene/mainView 候选整体发布。保存与 dirty 继续以所捕获的 View 文档 revision 为准，派生状态由准备合同恢复。新 reader 覆盖已有 v1 文件，新字段按本 change 更新后的 v1 合同读写。 [持久化合同](../../../../Docs/Architecture.md#asset-persistence) · [读取](../../../../Editor/Session/SceneDocumentRead.cpp) · [写入](../../../../Editor/Session/SceneDocumentWrite.cpp) · [文档 delta](specs/scene-document-persistence/spec.md)

## Risks / Trade-offs

- 曝光符号、单位及极值处理可改变倍率。用可独立求解的 EV 档位和镜头锚点核对派生值，检查值域失败后原请求及历史仍有效。
- 同一视图矩阵下变化的曝光可能被行缓存或相机回执漏掉。在同一复用槽进行两次不同曝光的原生常量读回，并用一次失败或回退与下一成功场景候选核对最后渲染值。
- v1 兼容和用户事务可能只恢复一部分请求。以同一最小场景覆盖旧文件、改镜头后保存重开、Undo/Redo 和显式坏字段，按 Scene/View 已有整体提交规则判定。 [验证范围](tasks.md#verification)

## Migration Plan

正式构建用匹配的生成 ABI 重新发布 `StaticForward` 和 `StaticDepth`，消费者按当前包配置运行。旧版 v1 文件由可选物理对象的缺省路径直接读取，更新后的保存入口写出当前请求。 [构建与运行](../../../../Docs/Build.md)
