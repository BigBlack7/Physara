# Design

## Context

`SceneFrameStorage::Prepare` 已把连续提取数组放在安全槽内，复用键是 Scene 身份和整场内容版本。Scene 的事务已枚举直接修改及传播过的世界变换，当前只发布总版本。Runtime 持有三槽及一组主 View 工作。Forward 每次准备开启一次驻留检查，构图时为这一组工作建立上传和导入映射。对象行在命中缓存前已经编码，实例覆盖会重复准备，灯表以整场内容版本判定失效。 [SceneFrame](../../../../Engine/Renderer/Prepare/SceneFrame.cpp) · [Scene 事务](../../../../Engine/Scene/SceneTransactions.cpp) · [帧编排](../../../../Runtime/Frame.cpp) · [Forward](../../../../Engine/Renderer/Paths/Forward.cpp)

D06 已建立共享快照、固定资产和独立 View 索引。D07 的参考场景、正式图和统计是本次兼容基线。Filament 参考中的连续场景数组和 View 裁剪提供分层依据，Physara 的槽寿命和增量规则以自身第 5 章为准。 [D06 准备设计](../2026-11-16-prepare-static-scene-views/design.md) · [Filament 场景准备](../../../../Docs/Reference/Filament/FrameSetup.md#f3) · [架构](../../../../Docs/Architecture.md#prepare)

## Goals / Non-Goals

**Goals**

在现有 owner 内分别推进已接受的 Scene 事务、槽内 CPU 提取、图内生产和有效提交。一个槽中的多个 View 借用同一份 SceneFrame；只有本轮工作集、显示附件和 View 常量按 View 分开。冻结场景经过三个槽的首次准备后，可以分别证明静态提取、编码和上传的复用。

**Non-Goals**

页式 Geometry、版本化大表、合并上传范围、volatile 常量和新上传区段由 D08-T02 承接。间接批处理、compute、图池和热重载沿后续各自工作包交付，活动实例的重导入及显式换代由 D16-T01 交付。首个 profile 为一个主视口和一个附加离屏 View，附加 View 由运行期消费者请求，场景文件仍沿 v1 主 View 入口。 [D08 后续依赖](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t02) · [D16-T01](../../../../Docs/Developments/D16-D17-EditorAndDelivery.md#d16-t01)

## Decisions

<a id="committed-scene-changes"></a>
### 1. Scene 在事务边界归并变更

由 `StagedChange` 现有分类和世界变换传播收集受影响实体。按对象变换与绕序、Primitive 显示与 bounds、覆盖源槽和灯有效字段记录变化，材质覆盖沿已排序的源槽比较。父级传播记录实际改变的渲染后代。结构代由 Scene owner 与事务同时发布，覆盖提取记录的增删、源布局变化及实际生效的固定模型代切换。纯名称和相机、环境查询字段以及同路径资产发布各走已有版本归属。只改变环境时发布新的内容版本和环境值，变更集合可为空。 [Scene 版本合同](../../../../Docs/Architecture.md#static-content) · [Scene 增量规范](specs/static-scene/spec.md)

发布值携带 Scene 身份、结构代和覆盖的内容版本区间。分配与容量检查留在事务准备阶段，使正式提交仍能一次发布完整状态。每个继续输入轮次在本轮文档、资产及 UI 事务结算后，由 Scene 的既有 owner 将合并结果交给 Runtime。Runtime 给对应结构的各槽合并待消费元数据，包括尚在途的槽。槽构建索引时按其规模为待消费标记与索引列表预留空间，按键去重，得到变更集合后无需在槽间转发期间再分配这些索引。冷槽、结构换代及 Scene 替换使用重建标记，先前结构的细目随之淘汰。事务入口已知道修改范围，逐帧比较记录需要再次扫描场景，逐事务保存历史则会随停帧期间的编辑次数累积。 [Runtime 输入边界](../../../specs/runtime-lifecycle/spec.md)

<a id="slot-scene-storage"></a>
### 2. 安全槽保留连续记录与索引

确认槽为 Ready 后比较自身 Scene 身份、结构代与已提取版本。冷槽或结构换代重建连续数组和实体到对象、Primitive 区间及灯的索引。结构匹配时按待消费索引追赶当前已接受版本。按完整实体身份解析，槽号和稳定字符串各有原有用途。一个实体变换保存一次，Primitive 保留源槽和对应世界 bounds。局部覆盖更新使用已有覆盖稀疏存储并回收槽索引，容量随结构内源槽总量有界。用 `WorldState.bounds` 维护可见有界对象的并集，槽内按对象的归并树更新脏叶及祖先，使删除极值、隐藏或内移都能缩小范围。原图判定有无源几何的事实可随结构记录维护，继续区分空文档和全部裁剪后的清色、清深度。 [现有世界数据](../../../../Engine/Scene/SceneTransform.cpp) · [快照规范](specs/scene-frame-preparation/spec.md)

数组、固定资产引用、索引和提取事实都归当前槽，独立于临时载荷与图 arena。提取完成才推进槽的 CPU 版本和消费标记。实际分配失败时交付有界原因，有部分写入的槽缓存失效，下一次合法准备从完整结构恢复。准备输出及其借用在本次槽使用内结束，图重试在原正常候选或安全 UI 间切换时沿现有寿命使用冻结数据。提取数组及准备存储的改写服从该槽的完成观察和背压。 [帧槽复用](../../../../Docs/Architecture.md#history-commit)

<a id="static-encodings"></a>
### 3. Forward 以实际静态依赖决定编码

在现有驻留 owner 上使用稳定对象身份、有效变换代和源图元局部基点语义，在进入对象矩阵及法线编码前查找缓存。保留每个图元的光照基点、绕序及精度行为。材质按固定模型、材质索引及实际实例覆盖代解析缓存；着色路由、使用的 UV 和绑定随匹配行复用。几何仍沿模型代与实际布局键复用。结构重建后按新索引和有效记录准备对应的对象、材质及灯内容，同时保留匹配的固定资产驻留。

灯的世界数据编码按 Scene、灯实体身份及该行版本在各 View 间复用。提取结构换代时，世界行缓存可淘汰已移除的灯，并保留仍在用的身份。各 View 以实际消耗的灯身份、版本和有序序列核对私有灯表，处理相同紧凑下标对应新身份的情况。保持每路的数值诊断以及方向灯／局部灯分段顺序。需更新时沿现有固定灯缓冲的完整范围上传，范围合并由 T02 承接。每路 View 常量按本轮相机、预曝光、模式和实际灯计数独立准备。 [Draw 准备](../../../../Engine/Renderer/Draw/StaticDraw.cpp) · [Forward 灯表](../../../../Engine/Renderer/Lighting/PunctualLights.cpp) · [驻留规范](specs/static-gpu-residency/spec.md)

保留 CPU 编码和 GPU 驻留确认各自的失效条件。一次已完成的 CPU 提取或编码允许重用相同载荷，实际图中已定义的 GPU 内容仍以匹配的完整提交为准。固定资源对象、配置和失败原因继续由现有预算与准备接口管理。

<a id="offscreen-view-outputs"></a>
### 4. 两个离屏请求进入同一帧图

Runtime 的正常帧入口接收有界请求集合，首轮容量为两路。主 View 沿 Editor 的封存布局和稳定 UI token，附加离屏 View 提供自己的像素范围及着色读出口请求。核对身份、重复请求、请求版本和尺寸，保留合法主 View 请求的优先级。只有存在初步合格的请求才提取 SceneFrame；必要的远面等派生参数在取得共享 bounds 后确定。每路使用自己的 `ViewWork`、Forward 工作、曝光、灯表和输出候选。附加 View 由实际消费者请求时才占用准备空间。容量以两路和现有单 View 资源上限、帧上传与图存储预算共同约束。 [View 准备](specs/view-preparation/spec.md) · [视口规范](../../../specs/editor-viewport-output/spec.md)

同一正常候选用一份持有载荷的场景上传集合和图资源版本映射。复用 `SceneGraphResources` 以物理对象及实际范围关联生产。共享几何、材质、纹理、环境和显示 LUT 都走相同的待写代规则。当前候选只有在整组写入被登记后才提供这个待写代。View 在局部准备失败时结束自己的候选，已经登记且另一路仍使用的共享生产者随本图保留，余项由图活性剪除。先声明界面和场景的纹理上传，再按现有准入顺序声明缓冲写。准入使用最终图存活上传的实际范围和参数，图存储和写入量按整帧检查。这个边界为 D08-T02 后续追加和封存上传数据保留位置。 [整帧合同](../../../../Docs/Architecture.md#frame-flow) · [上传](../../../../Docs/Architecture.md#uploads)

显式离屏请求在场景提取前用 Forward 的同一检查核对设备尺寸和固定附件字节预算，预算包含实际 FXAA 和重采样需要的线性中间目标。主视口可沿已有渲染栅格合同准备空文档诊断图。Forward 准备之前，Editor 用封存的视口纹理 token 和 UI 栅格化共用的像素裁剪确定主路采样需求。全裁剪主路省去驻留准备和私有写入，并报告当前请求正在等待采样，最近接受的颜色代和有效布局可继续读取。整帧上传上限继续由共同批次管理，最终图仍按实际版本读取和存活写入准入。环境缓存以当前 Scene 源开启帧需求收集，换源先撤销旧完成资格。各路按实际用途保留同源作业，所有路准备后再撤销失去消费者的作业。 [View 准备](specs/view-preparation/spec.md) · [环境合同](../../../specs/environment-lighting/spec.md) · [帧图规范](specs/editor-frame-graph/spec.md)

两路颜色在同一张图产生。主路按现有 UI 消费开放其出口，附加路使用显式 `ShaderRead` 导出，图执行返回当前图的实际对象。正常提交获接受后核对每路当前请求及导出，分别交给对应 `View`；共享资源确认核对该存储代全部所需生产者的存活和执行，一个被裁剪 View 的私有写入继续待处理。局部输入或准备阶段的失败可省掉该分支。正常图整体失败继续使用已有安全 UI 或取消规则；真实的失败前缀票据用于资源寿命。窗口交换仍由宿主处理。 [整帧图增量规范](specs/editor-frame-graph/spec.md) · [图资源交接](../../../specs/render-graph-execution/spec.md)

<a id="preparation-statistics"></a>
### 5. 将准备事实接入现有统计

提取记录槽、当前身份与版本、共享耗时、实际改写和重建；裁剪记录每路 View 和候选规模。编码点区分对象、材质和灯的实际打包，上传在正常图完整执行后按已执行的图写入归类，RHI 保留实际原生总量。Runtime 汇合一次成帧的数据和已接受 View 集合，显示方按明确作用域读取。保留现有主视口统计口径，附加路裁剪和共享工作分别标明；只接受附加路的帧也可按其独立身份记录本轮准备。CPU 共享项只累计一次，继续使用已有 GPU 计时区间与异步确认。 [统计架构](../../../../Docs/Architecture.md#diagnostics) · [统计增量规范](specs/render-frame-statistics/spec.md)

`FrameArena` 已保存容量和高水位，扩容次数只在成功更换储存时增长。Runtime 按槽分别报告临时载荷和图 arena，读数保留其生命周期口径。快照沿正常帧的真实提交发布，失败、取消及安全 UI 保留最后有效记录。

## Risks / Trade-offs

| 风险 | 应对 |
|---|---|
| 槽长时间在途，旧 Scene 身份或复用实体索引混入新数据 | 待消费元数据按 Scene／结构换代，仅在 Ready 后修改记录；归并按身份去重，用槽轮转与 Scene 同版号替换检查。 |
| 记录补丁正确但聚合范围或静态 GPU 行滞后 | 以实际对象范围和独立编码依赖更新，检查边界收缩、覆盖移除、父级灯姿态及锁定场景暖机后的分项计数。 |
| 第一个 View 生成共享数据后被裁剪，另一 View 仍用该数据 | 同图统一生产及导入，按存活依赖和提交确认分代推进，验证两路出图及撤去一个消费者。 |
| 第二路请求扩大实际存储、上传或裁剪成本 | 请求、附件和图预算均有边界；统计提供一次共享提取与各路成本。D08-T01 比较操作计数和版本，耗时报告以固定输入为准。 |

## Migration Plan

先接通事务变更和槽内增量消费，保留单主 View 的正式帧入口。随后让 Forward 在同图共享静态生产并接上附加离屏请求，再把事实汇入当前统计。将正常主视口和双 View 纳入同一原生回放路径。需要整体回退时恢复此前的 Runtime/Renderer 和 Scene 提取；现有 v1 文档、资产和 Shader ABI 兼容该回退。 [任务](tasks.md)
