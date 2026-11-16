# Design

## Context

Scene 事务已经按脏根提交 `WorldState`、每 Primitive 的世界 AABB 及不同用途的 revision。`Scene::Snapshot()` 面向文档保存，拷贝创作记录并舍弃派生世界数据。`EditorSession::Advance` 封存布局、应用当前领域事务，再生成带 View 身份和比例代的像素请求。Runtime 已在继续轮次先推进 AssetManager 和文档，并使用取得状态决定帧槽何时可写。 [Scene 合同](../../../../Docs/Architecture.md#scene) · [帧槽合同](../../../../Docs/Architecture.md#frame-state) · [编辑事务](../../../../Docs/Architecture.md#edits)

## Goals / Non-Goals

D06-T04 交付正式帧路径可消费的只读静态数据、相机快照及有效候选索引。直接消费者为现有诊断 View 和下一工作包的 Draw 准备。后续交付位置列在方案中。 [工作包](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t04) · [Proposal](proposal.md)

## Decisions

### 1. 事务提交之后借用 Scene，帧槽拥有提取结果

- Scene 提供同步只读遍历入口，借用已提交的实体 handle、类型化组件、`WorldState` 及固定版本定位。复用现有 Scene 材质解析合同与有效世界 bounds。该入口的借用在提取返回时结束，EnTT 和实体可写权限留在 Scene。
- Renderer/Prepare 将对象、Primitive、灯和共享版本引用存入当前槽的连续数组。一个可渲染实体贡献一个对象变换。Primitive 记录包含对象索引、固定模型的 mesh／源 Primitive 槽、材质定位、世界 AABB 及显示、投影标志。空表面沿用源槽身份。材质基础数据以固定模型和材质索引共享，实例覆盖只为实际修改的槽存一份帧内值，外置图像以其不可变版本保活。资产索引用 `AssetId` 与内容 revision 对应的拥有型引用，源路径只在导入和文档入口解释。
- Runtime 的 `FrameSlot` 持有 Renderer 定义的提取存储和每 View 工作集。存储独立于 UI／Graph arena 的重置和重试周期。以 Scene 身份和 `RenderContentRevision` 作为完整提取的复用键，命中时直接借用本槽已提取数组。缺失或变更时在安全槽完成整份提取并一次发布，尽量复用已有容量。数组索引和派生尺寸在转换前检查表示范围，提取失败时本轮交付有界原因。其余槽仍按各自完成状态保有原快照，消费方在本次槽使用结束时交还借用。
- CameraSnapshot 从当前 Scene／View 独立解析并按每 View 值交付。`PreparedView` 只增加自己的投影、映射和索引工作集，传入同一 SceneFrame 给多个 View，读取静态数据时以同一组索引为基准。本包在内容变更时重提取，不同槽首次使用时建立各自记录。跨槽合并脏索引、稀疏重打包与多离屏输出调度由 D08-T01 引入。 [提取合同](../../../../Docs/Architecture.md#prepare) · [Filament 参考](../../../../Docs/Reference/Filament/FrameSetup.md#f3) · [D08-T01](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t01)

### 2. 让 Scene 的世界状态同时表达刚体朝向

- 脏事务继续用完整 `parent * T * R * S` 求几何世界矩阵、绕序与 bounds。世界原点取该矩阵的平移，刚体朝向沿父先子后组合单位四元数。由世界旋转变换相机的 `-Z`、`+Y` 轴，方向灯与聚光灯复用 `-Z` 发射方向。镜像和奇异缩放仍参与几何及子节点位置，灯的范围及锥角按原参数解释。这个项目选择使非均匀父级下的相机基保持正交，并避免在求相机逆矩阵时依赖几何矩阵可逆。 [glTF 相机视图矩阵](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#view-matrix) · [KHR_lights_punctual](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_lights_punctual/README.md)
- 在 Scene 的事务计划与重放入口根据实际派生值更新 revision。纯相机投影或刚体位姿变化推进相机版本，灯方向/位置或几何与材质变化推进内容版本。具备两类角色的实体及其子树按各自实际变化推进两类版本。即使零缩放使几何矩阵相同，方向变化仍被记录。导航位姿和相机选择沿现有 View 请求版本更新，文档保存继续使用各 owner 的持久化 revision。 [静态内容版本](../../../../Docs/Architecture.md#static-content)
- `KeepWorld` 在受影响子树含相机、方向灯或聚光灯时，用新父级刚体朝向的逆和旧根世界朝向求目标 local 旋转，再用既有仿射约束核对该旋转下的平移和有符号缩放能否重组原几何世界变换。镜像父级下分解出的合法几何 TRS 可能朝向不同，同时保姿不可表示时沿 `KeepWorld` 既有拒绝路径退出。按受影响子树核对这一新增世界状态，保持已接受记录的一致性。 [层级事务](../../../specs/static-scene/spec.md)
- 通过稳定 ID 在当前 Scene 解析相机后，准备阶段只保留当前运行期身份、版本和按值捕获的参数。空相机引用采用已保存导航位姿，默认垂直 FOV 为 60°、near 为 `0.10 m`、渲染 far 缺省。选中的 Camera 缺失时返回 View 原因。编辑历史移除当前 Camera 时沿 D06-T03 的事务入口显式切回导航。投影参数沿用已采纳 profile，派生阶段检查像素比例、透视尺度和逆变换可表示性，加载与编辑入口各自继续报告实际非法输入。 [相机合同](../../../../Docs/Architecture.md#camera) · [已采纳的编辑历史](../../../specs/editor-domain-edits/spec.md)

### 3. 投影、视线和像素具有明确的坐标约定

- 世界为右手系米制坐标，列主序矩阵作用于列向量。由位置和旋转生成 `worldFromView` 及其刚体逆 `viewFromWorld`。令正向视深 `d = -viewZ`。有限透视的 Reverse-Z 深度为 `n(f-d) / (d(f-n))`，无限透视为 `n / d`，正交为 `(f-d) / (f-n)`。分别生成稳定有限的 `clipFromView` 和当前采样投影及逆变换，采样投影带本次 jitter。CPU 交付无需依赖后端坐标适配的相机常量。 [坐标和深度约定](../../../../Docs/Architecture.md#conventions)
- 有限透视与正交直接使用通过校验的远面。无显式 far 的透视按共享 SceneFrame 的可绘世界 bounds 求沿当前视线的最远正向深度，稳定远面采用能覆盖该深度且至少为 `2 * near` 的有限值。空场景或全部范围落在近面以内时使用这个近远区间。场景范围可从提取时的包围盒并集得出，投到视线上的远端是保守上界。派生结果无有效有限表示时拒绝本 View。渲染矩阵保留无限远，裁剪距离独立于每帧采样偏移。
- 用有效图像坐标构造视线。正交时从不同点的近面位置起射，方向固定为世界 `forward`。透视时从相机位置出发，方向由该点逆投影确定。需要表面朝向观察者时，正交用固定 `-forward`，透视用逐点指向相机的方向。NDC 到 UV 的纵轴反转一次，按 `u = 0.5x + 0.5`、`v = 0.5 - 0.5y` 转为左上原点。 [View 准备](../../../../Docs/Architecture.md#view-preparation)
- `ViewportRequest` 继续用同批 framebuffer 比例和已接受的比例请求计算输出像素画布 `W × H`。透视有固定 aspect 时用源值，正交用 `xmag / ymag`，缺省透视使用画布比例。固定比例时保留限制轴，向下取整另一轴，在画布中央放置半开像素矩形，奇数余量的额外像素在右或下。投影保留精确比例，布局误差来自像素取整。矩形尺寸在本包也是实际渲染采样尺寸。编辑器由逻辑面板到画布像素，再由有效矩形得到局部坐标，留边返回空。诊断画布尺寸、有效矩形、渲染尺寸和窗口像素各自表达自己的单位和用途。 [视口输入与拾取合同](../../../../Docs/Architecture.md#viewport-input)

### 4. 主可见集按稳定范围保守筛选

- 取稳定有限视锥的六个半空间，仅扩展左右上下四个侧面。每轴约定 `guardPx = maxAbsJitterPx + filterRadiusPx`，转换为 `2 * guardPx / renderSizePx` 的 NDC 扩展。采样投影的像素位移向右、向下为正，在有效渲染尺寸 `W × H` 内转换为 NDC 位移 `(2*jx/W, -2*jy/H)`。实际偏移在声明上限内。本包正常诊断入口使用零 jitter 和一像素的边缘滤波预留，以非零 profile 验证 CPU 边界。实际时域偏移和效果设置由对应下游工作包消费。
- 用 Primitive 世界 AABB 的远侧支持点判断是否被任一裁剪平面完全分离，交叠、相切与无法确证分离均保留。剔除隐藏项和无有效表面的源槽。局部灯以世界位置为中心、有限 range 为半径做球／平面保守测试，聚光灯可保留部分无贡献的候选。启用的方向灯和无范围的局部灯直接保留。世界坐标与平面计算优先用双精度，边界舍入向保守侧处理。分离的原因能从平面和候选 bounds 解释；结果仅为主相机候选，阴影 caster 范围由阴影工作包建立。 [View 准备](../../../../Docs/Architecture.md#view-preparation) · [D07 灯列表](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d07-t03)

### 5. 正常诊断帧直接消费准备结果

- Runtime 在 `Advance` 之后、安全取得的帧槽上调用一次正式 Renderer/Prepare。无可准备布局时跳过场景提取，有多个请求时各 View 共享同一一致 SceneFrame。当前唯一主 View 接入产品，多个 View 的 CPU 消费以同一入口验收。图声明和回调只持有必要准备值或图引用。安全 UI、上传待定和图重试沿当前回路使用已经完成的准备，避免重复遍历 Scene 或重放编辑。 [现有帧编排](../../../../Runtime/Frame.cpp) · [图合成合同](../../../specs/editor-frame-graph/spec.md)
- 诊断纹理继续使用现有 `NoSceneOutputs`、上传预算和图消费。有效矩形内以相对矩形的像素生成既有象限图，留边填背景，同一 UI token 对整幅画布采样。为正常诊断颜色按画布 extent 和有效矩形匹配输出代；纯位姿变化若保持矩形即可复用纹理。从有效矩形取得图像命中映射，工作区面板仍按同批逻辑布局收集输入。无有效 Camera 或矩形时通过现有可见占位／状态呈现有界原因。安全 UI 可按原协议使用标注的上次有效颜色，最终以实际被采样且提交接受的正常候选推进有效设置。 [现有输出合同](../../../specs/editor-viewport-output/spec.md)

## Risks / Trade-offs

| 风险 | 处理与证据 |
|---|---|
| 内容版本相同的新 Scene 或旧槽借用串用 | 帧槽以 Scene 身份和内容版本共同确认快照，使用实际两槽及文档替换的精确资产引用核对。 |
| 光轴符号、行列、透视除法或取整错误 | 用旋转且平移的非对称场景及已知参考点，对比有限／无限透视和正交深度、UV、逆投影及留边。 |
| 稳定范围误剔除采样边缘或灯 | 同一视图翻转 jitter，多测一个 guard band 外对照，并以相交的有限灯和无界灯核对候选。 |
| 可见 View 失败牵连整帧或错误确认新输出 | 正常帧通过当前占位入口处理局部结果，旧输出经安全 UI 仅保留历史含义。验证同尺寸换矩形、失败后的可见诊断及后继恢复。 |

## Migration Plan

启动和旧 v1 场景沿既有 owner 恢复导航位姿、实体相机及渲染比例。启用 Prepare 后，通过一个现有产品帧入口产出 CPU 准备值和诊断颜色。D06-T05 从该入口消费索引构建真实场景输出，D08-T01 在相同帧槽边界完善增量提取。 [持久化合同](../../../specs/scene-document-persistence/spec.md) · [阶段顺序](../../../../Docs/Developments/D06-D07-SceneAndForward.md#dev-d06)
