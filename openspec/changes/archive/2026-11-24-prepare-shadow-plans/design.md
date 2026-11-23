# Design

## Context

`ViewWork` 已从共享 `SceneFrame` 和稳定有限的 `CameraSnapshot` 准备主可见集。`PrimitiveRef` 含世界范围、显隐、`castsShadow` 和材质覆盖代，`SceneObject` 含变换代，`LightRecord` 含实时实体句柄、世界姿态和源灯语义。这些记录足以建立第一段 CPU 阴影工作集。 [场景快照](../../../../Engine/Renderer/Prepare/SceneFrame.hpp) · [View 准备](../../../../Engine/Renderer/Prepare/ViewPreparation.hpp) · [开发计划](../../../../Docs/Developments/D08-D11-RenderPaths.md#d09-t01)

固定 Filament 参考将阴影视图与分配元数据交给后续深度生产。Physara 采用 Architecture 选择的有限稳定接收范围、保守 caster 和独立阴影深度约定。 [固定快照分析](../../../../Docs/Reference/Filament/VisibilityAndCommands.md#f4) · [阴影组织 F31](../../../../Docs/Reference/Filament/SourceIndex.md#f31) · [拟合 F75](../../../../Docs/Reference/Filament/SourceIndex.md#f75) · [光照准备](../../../../Docs/Architecture.md#light-preparation) · [阴影合同](../../../../Docs/Architecture.md#shadows)

## Goals

正式帧准备中的主 View 和离屏 View 各自交付一份可定位的 CPU 计划。后续深度生产可由灯、子视图、逻辑层与版本找到同一次准备的投射物。D09-T02 承接 CSM 多级划分、texel 吸附及图内深度生产，D09-T03 承接聚光与点光最终深度变换及接缝，D09-T04 承接过滤、质量界面和统一光照查询，D09-T05 核验压力与图像。 [D09 工作包](../../../../Docs/Developments/D08-D11-RenderPaths.md#dev-d09)

## Decisions

<a id="ownership-and-requests"></a>
### View 请求与帧内计划

`View` 持有会话请求和最近一次完整的 CPU 计划缓存。Lighting 从已验证请求、设备能力值和冻结记录构造候选。`ViewWork::Prepare` 绑定候选到当前 `PreparedView`，主 View 与离屏 View 共用这条入口。缓存以 Scene 与 View 身份限定，持有自己的描述和版本值。当前计划的帧内 Light/Primitive 索引只在相应的 `SceneFrame` 中解析。安全槽保有本次使用的不可变计划数据，后续准备可以产生新计划而不改写旧槽。切换槽时可复用同一逻辑 View 的计划。 [View 准备合同](../../../../Docs/Architecture.md#view-preparation) · [帧槽合同](../../../../Docs/Architecture.md#history-commit)

主 View 的 Renderer 面板提供会话级 CPU 阴影规划开关。请求经现有帧边界按 View 身份提交。离屏调用方使用相同类型化入口。默认关闭规划，缺省边长为 1024 texel，缺省距离为 50 m，可选择 1024、512、256 三档。有限正距离和受支持档位在请求边界校验，拒绝时保留原请求和原因。已配置的会话请求随所属 View 保有，Open/New 更换 Scene 时重新解析计划。主场景文件继续按当前 v1 请求合同读写。 [编辑器设置](../../../../Docs/Architecture.md#editor-input) · [现有文档请求](../../../specs/scene-document-persistence/spec.md)

Runtime 从实际设备已有的能力快照取得 D32 2D array 的深度附件与采样支持、纹理边长和层上限，按值传给准备端。现有 OpenGL 已记录目标格式能力和层上限，可通过 DeviceHost 的只读入口转交。规划时使用该快照，GPU 生产进入后续工作包时按正式图资源链验证。 [设备能力合同](../../../../Docs/Architecture.md#capabilities) · [能力来源](../../../../RHI/OpenGL/Formats.cpp)

<a id="conservative-shadow-views"></a>
### 接收区域与投射物

使用稳定投影和有效采样边界构造有限接收视锥。边界取最大 jitter 与滤波 footprint 对应的固定扩展。每帧实际 jitter、曝光和无限远渲染投影都独立于这个输入。首段方向光使用 `[nearPlane, min(cullingFar, shadowDistance)]`；`nearPlane = 0` 的正交相机同样可用。空区间产生可解释的无分配状态。

当前主方向光交付一个有限 receiver 段及光空间拟合范围。把段的八个角和冻结 Primitive 世界 bounds 保守地投到同一光空间。候选与 receiver 的 XY 投影接触或重叠，且其最靠近光源的一端处于 receiver 最下游点上游或相切时，保留候选。这样可从同一 SceneFrame 纳入主相机画外的投影物，并按选中记录扩展光空间深度范围。T02 的分段拟合可在各段复用这个范围合同。 [阴影拟合](../../../../Docs/Architecture.md#shadows)

局部灯使用以灯为原点的有限阴影范围，`far = min(range, shadowDistance)`，省略 range 时取 `shadowDistance`。逐面影响球粗筛后，将稳定接收视锥截到灯的影响球可达深度，并在相机轴系中以灯心和影响半径表示。点光比较到有限接收体的最近距离，同时计入两侧和端面的距离。聚光再将接收切片裁到影响球的横向包围范围，用裁剪后凸体和聚光外锥约束下的有限灯体查找分离平面。保留相切及数值不确定项，延长灯体可达范围以外的远面不改变这项判定。

聚光描述发射轴和 outer cone，点光按 `+X, -X, +Y, -Y, +Z, -Z` 交付六个有序子视图，同组共享有限近远面。局部近面可取 `min(0.1 m, far / 1024)`，以可表示正值且小于 far 为准。派生范围不可表示时给出无分配原因。caster 入选考虑触及灯位置或近面的 bounds。Spot 可用包含外锥的透视体筛选，Point 各面共用一个有限影响球投射物集合。最终逐面朝向、投影和跨面采样由 T03 对应合同验收。

从整个冻结 Primitive 表选取存在 bounds、可见且 `castsShadow = true` 的记录。紧贴边界的记录按相交处理。允许已分配视图拥有空 caster 集，使后续消费者能识别删除最后一个 caster 引起的清理需求。描述携带 `ShadowDepthConvention`，采用常规深度 `[0,1]`、clear 1、Less。几何／MASK 覆盖和最终 bias 由后续深度生产使用共享合同。 [深度约定](../../../../Docs/Architecture.md#conventions) · [统一几何](../../../../Docs/Architecture.md#geometry)

<a id="allocation-and-light-indices"></a>
### 区域分配与稳定索引

每个 View 使用同一边长的 D32 单样本单 mip 2D array 逻辑布局，每个区域占一个完整层，坐标为 `[0, edge) × [0, edge)`。这种布局让预算可直接从层地址反算，并给点光六面独立地址。单层按 `edge × edge × 4 B` 计，最多 16 层及 32 MiB，再由设备能力收紧。字节转换和层跨度使用经检查的整数运算。实际规划字节按最高已用层加一乘单层字节，内部空层仍计入跨度。无区域时规划字节为零。

准入顺序固定。先取稳定实体身份最小且具备可用接收段的可见方向光，再按身份处理聚光和点光。其余方向光可以继续照明，显示方向光额度原因。先在设备边长允许的档位中选出能容纳最高优先请求组且最接近请求质量的档位，再在该档按优先顺序准入。一个方向光或聚光占一层，一个点光占六层。后续组容量不足时报告预算拒绝。有效边长降至设备允许的最高档时报告尺寸限制，继续降档时报告逻辑字节预算。

先确定本轮准入集合，为仍匹配的子视图保留兼容层，再为新增及需要迁址的子视图分配最低可用层。点光显式记录有序六面各自的层，全部具备时一起发布，允许使用离散空层。有效边长相同且地址仍在容量内的面保有分配代，内容依赖相同的面也保有所需内容版本。

逻辑灯键使用 `SceneId` 与包含槽号及 generation 的 live 句柄，按该键将当前 SceneFrame 的灯索引与各子视图关联。当前紧凑灯表的行从同帧可见灯顺序解析。区域键带 View、子视图、层与分配代，复用被释放的层得到新代。整份计划另携带分配修订，便于对照当前映射。预算未准入和删除的灯具有明确的无分配结果。 [完整灯集合](../../../specs/punctual-lighting/spec.md)

<a id="plan-cache-and-invalidation"></a>
### 缓存与失效

版本核对覆盖 Scene/View 身份、冻结场景结构与内容、稳定相机几何、有效采样范围、阴影请求及有效布局。相同工作输入可跳过阴影扫描、拟合和分配，帧内版本标签仍对应本次准备。相关版本变化时重新核对实际依赖。结构变化时用稳定键重新解析帧内索引，兼容区域仍可保留。

每个已分配区域保有所需内容版本。新增、迁址、边长变化，或实际 receiver／灯投影／投射物输入变化时，更新受影响区域的所需版本。投射物依赖包含稳定实体与源槽、固定几何版本、变换代、资格与几何覆盖，包括 MASK 所用 alpha、纹理和双面信息。检查集合增加和减少，防止删光、移出或关闭 `castsShadow` 后沿用旧覆盖。灯变化用影响投影的方向、位置、范围与外锥求值。仅曝光、实际 jitter、灯光色或强度变化可复用同样的阴影内容。

完成一份相容的 CPU 计划后才发布其映射和版本，失败时报告本轮无分配。禁用、灯删除、替换 Scene 或无法解析当前布局的结果清除相应活动映射，既有只读借用沿安全槽生命周期结束。统计中的 dirty 是本次所需内容变化，后续 GPU 生产须与其已接受的区域版本比较。持续保存所需版本可让取消帧后的重试正确识别待产出的区域。

<a id="plan-diagnostics"></a>
### 诊断与发布

Renderer 从实际准入、拟合和缓存路径记录有界摘要。Runtime 通过 FrameStatistics 发布每路最近 CPU 规划结果，包括 View、Scene、请求及版本身份，请求／有效边长、层和逻辑字节的上限与用量、计划／拒绝灯数、dirty 和缓存复用，以及可定位的禁用、不可用或预算原因。CPU 计划结束即可显示，正常场景的接受统计仍按自己的提交产生。状态注明 CPU 计划，面板显隐只影响界面构建。 [只读统计合同](../../../../Docs/Architecture.md#diagnostics)

## Risks / Trade-offs

- 用主可见集做投射物输入会遗漏画外遮挡。用已知光线穿过画外 caster 和屏内 receiver 的小场景验证几何关系，再用完全分离的 bounds 验证确有剔除。
- 点光部分分配或层复用时的代错配会把不同灯连到同一区域。一次受限预算回放检查六面原子准入、删除、复用和不可用地址，按实际层跨度独立核算字节。
- 同帧两路 View 与跨槽复用可能掩盖缓存身份错配。用同一场景的两个不同请求走正常准备，对照冻结输入和局部编辑，确认区域版本及统计归属。

## Migration Plan

现有场景在默认关闭规划的会话请求下使用原有产品入口。开启与关闭规划可在同一会话观测，按三份增量规格验收。 [实现与验证](tasks.md)
