# view-preparation Specification

## Purpose

规定每个 View 如何从当前相机请求、像素范围和共享静态场景构造相机快照、有效图像矩形及保守可见工作集，为各渲染路径提供同一份坐标和裁剪依据。

## Requirements

### Requirement: Resolve a supported current camera for each view

View 准备 SHALL 消费当前激活 Scene 和 View 已接受的相机请求。空实体引用 SHALL 使用该 View 的导航位姿，并按已接受的传感器高度与导航焦距计算透视垂直 FOV。缺省镜头 SHALL 产生原 60° 导航视场。已选实体 SHALL 在当前 Scene 解析为有效 Camera，采用该实体的世界位置、局部 `+Y` 上方向与局部 `-Z` 视线对应的刚体世界朝向。场景透视 Camera SHALL 使用其已解析的 `yfov`，有效显示焦距按该 `yfov` 与 View 的传感器高度反推。正交 Camera 的有效镜头 SHALL 表示正交投影，曝光仍来自该 View 的物理请求。准备值 SHALL 带足以识别本次 Scene、View 及相机输入版本的信息。

透视 SHALL 接受有限 `0 < yfov < π`、有限正 near 和缺省或有限且大于 near 的 far；显式 aspect SHALL 有限为正。正交 SHALL 接受有限正 `xmag`、`ymag` 和有限 `0 ≤ near < far`。超出支持 profile 或无法得到有效投影及逆变换的输入 SHALL 返回可解释的 View 拒绝。glTF 中负正交 magnification SHALL 沿用已有的超出项目 profile 诊断。

#### Scenario: Resolve a parented camera and the navigation camera independently

- **WHEN** 一个 View 使用带缩放的父级下的 glTF Camera，另一个 View 使用自己的导航位姿和缺省镜头
- **THEN** 前者使用实体原点、正交归一的世界相机轴和源投影，后者使用自己的位姿与 60° 导航透视，两者各自对应本帧的相机输入

#### Scenario: Reject unavailable or unusable camera inputs

- **WHEN** 选定相机无法在当前 Scene 解析，或投影参数或其派生结果超出支持范围
- **THEN** 本 View 返回带相机原因的不可准备状态，合法请求恢复后按新输入得到当前工作集

#### Scenario: Change a navigation lens and use authored scene projections

- **WHEN** 同一 View 修改传感器高度和导航焦距，随后依次选择带显式 `yfov` 的 glTF 透视 Camera、正交 Camera，再切回导航
- **THEN** 导航投影由传感器高度与导航焦距得到，场景透视保留源 `yfov`、aspect 和裁剪语义，显示焦距按该 `yfov` 更新，正交保留其原投影，切回导航时恢复已编辑的导航镜头

### Requirement: Separate stable and sample projections

相机快照 SHALL 提供右手系、列向量的世界／视空间变换、稳定有限投影、当前渲染投影和相应逆变换，光轴局部朝 `-Z`。主视图深度 SHALL 使用 `[0,1]` Reverse-Z。有限透视和正交 SHALL 保留其明确的近远面，近平面映到 1、远平面映到 0。无 far 的透视相机 SHALL 保持无限远渲染投影，另有有限、未抖动的裁剪范围，覆盖当前有界可绘静态几何的正向深度。采样抖动 SHALL 以有效渲染像素转换到当前渲染投影，稳定投影独立于本次偏移。正交视线 SHALL 具有固定方向及随图像位置变化的起点，透视视线 SHALL 具有共同眼点及随位置变化的方向。

#### Scenario: Project an off-axis witness into each depth profile

- **WHEN** 同一非对称世界点通过带平移和旋转的相机分别进行有限透视、无限远透视与正交准备
- **THEN** 得到对应的 NDC 和 Reverse-Z 深度，逆变换回到原位置，正交的不同采样点射线保持平行

#### Scenario: Keep a distant object eligible for an infinite render projection

- **WHEN** 一个无 far 的透视 View 观察到沿视线方向很远的有效有界 Primitive，同帧相机采样使用非零 jitter
- **THEN** 有限稳定裁剪区间仍覆盖该 Primitive 深度，渲染投影保持无限远且使用本次偏移，稳定投影不随 jitter 改变

### Requirement: Fit fixed projections to the effective image rectangle

View SHALL 从当前输出像素范围计算居中的有效图像矩形。固定透视 aspect 与正交 `xmag / ymag` SHALL 保持源比例，通过 letterbox 或 pillarbox 适配输出。缺省透视 aspect SHALL 取有效图像的像素宽高比。场景准备 SHALL 分别交付显示画布及其有效矩形、渲染栅格及其有效矩形。固定 aspect 使用同一源比例在两张栅格分别居中适配；整数适配沿用非限制轴向下取整，奇数余量分配在右或下。缺省 aspect 的透视在两张栅格都使用完整画布，投影比例取当前显示有效图像。相机投影及图像方向由同一快照交付，覆盖和深度使用实际渲染矩形；采样偏移及保守范围使用有效渲染像素。连续图像坐标 SHALL 从显示有效矩形映射，矩形外的留边返回无图像坐标。无布局、任一所需尺寸为零或任一有效矩形无法表示非空图像时 SHALL 返回明确的不可准备状态和空工作集。

#### Scenario: Fit a fixed camera to a differently shaped canvas

- **WHEN** 宽高比为 2 的固定相机准备 640×480 输出，面板内容与其图像区域分别用于指针坐标映射
- **THEN** 有效矩形为 `x ∈ [0,640)`、`y ∈ [80,400)`，图像坐标从矩形内映射，上下留边没有有效命中，投影仍按比例 2 求值

#### Scenario: Resume a view after an unusable layout

- **WHEN** View 缺少布局、换算后存在零轴或留边后无法得到有效像素，随后取得有效尺寸
- **THEN** 前者没有可用相机矩阵、图像映射或可见索引，后者从当前像素尺寸和请求重新准备

#### Scenario: Use one fixed projection at two sampling extents

- **WHEN** 宽高比为 2 的固定相机准备 321×241 显示画布和 241×180 渲染栅格
- **THEN** 显示矩形为 `[0,321) × [40,200)`，渲染矩形为 `[0,241) × [30,150)`，相同图像 UV 对应同一投影视线，命中和采样偏移各自使用已声明的矩形

### Requirement: Cull conservatively with a stable guard-banded frustum

每个有效 View SHALL 使用稳定有限视锥和世界 bounds 生成独立的 Primitive 与灯索引。侧面 guard band SHALL 由当前采样 profile 的最大 jitter 位移及滤波边界在有效渲染像素中的范围共同确定。候选与边界相交或相切时 SHALL 保留，完全位于裁剪面外时才剔除。启用的方向灯与无距离截断的局部灯 SHALL 保留；有限 range 的点光和聚光 SHALL 用覆盖其影响范围的保守 bounds 筛选，灯源位于视锥外本身不构成剔除条件。相同 Scene、相机、尺寸与 profile 下的逐帧 jitter SHALL 保持同一可见集合。

#### Scenario: Keep geometry within the largest jitter and filter footprint

- **WHEN** 非对称场景含有贴近未扩展侧面但仍在 guard band 中的几何、完全超出扩展侧面的几何和跨越近面的几何，本次 jitter 在允许范围的两端变化
- **THEN** guard band 内及跨越近面的候选始终可见，扩展范围外的候选被剔除，两次准备返回相同索引集合

#### Scenario: Cull finite local lights by influence

- **WHEN** 有限 range 灯的中心在视锥外但范围与之相交，另一有限灯完全在外，同时存在无范围的局部灯和方向灯
- **THEN** 仅完全在外的有限灯被剔除，其余有效灯仍可由 View 索引解析

### Requirement: Derive one calibrated pre-exposure for a prepared view

View SHALL 验证并接受有限正的光圈 f 数、快门秒数、ISO、传感器高度、导航焦距、对焦距离及有限的曝光补偿档数。计算光学关系时长度 SHALL 使用统一单位，导航镜头 SHALL 产生支持范围内的有限垂直 FOV。曝光 SHALL 使用 `EV100 = log2((N²/t) × (100/S))` 和 `preExposure = 2^(compensationEv − EV100) / 1.2`，正补偿提高预曝光倍率。倍率 SHALL 落在正有限的 FP32 正规数范围，值域或派生失败 SHALL 给出可定位原因并拒绝该完整请求。

成功准备的相机 SHALL 为本轮提供同一份 `preExposure`，并通过生成的 View Shader 接口交付该值。只改变曝光时，渲染及稳定投影、可见集合和视口布局 SHALL 继续遵循相同的几何输入。

#### Scenario: Read one stop of exposure from the current view data

- **WHEN** 固定镜头、投影和画布，设置 f/2、0.25 秒、ISO 200 和 0 档补偿，随后将补偿增加 1 档
- **THEN** 名义 EV100 为 3，当前倍率从约 0.10416667 变为 0.20833333，同一 View 的新 Shader 常量对应第二次准备，投影、布局与可见集合保持一致

#### Scenario: Reject an invalid camera request at its input boundary

- **WHEN** 用户或恢复请求提供零 ISO、非法长度、非有限参数或导致倍率超出支持范围的补偿
- **THEN** 请求整体被拒绝并指出对应字段或派生值，已接受的物理参数、投影及预曝光继续有效

### Requirement: Prepare independent work for requested offscreen views

正式帧准备 SHALL 支持同一 Scene 同时至少两个合法离屏 View 请求。请求集合 SHALL 有明确容量，每个有效请求 SHALL 带唯一 View 身份、当前请求版本、有效渲染与输出像素范围，并采用所属 View 的相机、曝光和设置。各路 SHALL 共用同一已准备 SceneFrame，以各自的稳定裁剪、投影、曝光及可见索引形成工作集。View 工作集和静态快照借用 SHALL 限于当前安全槽的该次使用。纯 View 请求变化 SHALL 独立更新该 View 所需的数据。

缺失、陈旧、重复或超出容量或尺寸限制的请求 SHALL 有可解释的状态，非法候选无法推进有效输出。局部 View 准备失败 SHALL 允许仍可用的 View 和基础 UI 继续准备。能在读取场景记录前确定全部请求均不具备可准备布局或相机输入的轮次 SHALL 跳过场景提取。

#### Scenario: Prepare two camera and image configurations from one scene

- **WHEN** 同一场景有一个主 View 和一个附加离屏请求，两者相机与画布不同，仅更新附加 View 的曝光或布局
- **THEN** 场景记录始终按同一帧快照共享，两个独立的相机、可见索引和图像矩形与各自请求匹配，主 View 的请求及有效设置保持一致

#### Scenario: Reject an invalid offscreen request locally

- **WHEN** 附加 View 交回陈旧身份或无效尺寸的请求，主 View 仍具备合法输入，随后全部请求均不可准备
- **THEN** 附加 View 的诊断可定位，主 View 仍可准备，全部请求不可准备的轮次保持零场景提取

### Requirement: Prepare shadow plans from the current per-view request

View SHALL 持有独立的会话阴影请求，默认关闭。请求 SHALL 由类型化入口校验和提交，支持开启状态、有限正的阴影距离及 1024、512、256 texel 边长档，缺省距离和边长为 50 m 和 1024 texel。请求无效时 SHALL 报告原因并保留原请求。主 View SHALL 可从现有 Renderer 面板开启或关闭 CPU 阴影规划，离屏 View 使用同一准备合同。

正式 View 准备 SHALL 从其已解析的稳定相机、可见灯和同轮共享 `SceneFrame` 交付匹配的计划或当前原因。相机或布局不可准备时，当前 View SHALL 具有空的阴影工作集。计划不可用时，仍可用的相机、主可见集和完整直接光输入 SHALL 继续按正常准备合同交付。只改变一路 View 的阴影设置或相关布局时，独立的另一 View SHALL 保有各自的计划输入和版本。

#### Scenario: Prepare two distinct shadow requests from one frozen scene

- **WHEN** 主 View 和离屏 View 共享同一 Scene，分别使用有效相机及不同的阴影质量或开启状态，随后只修改其中一路请求
- **THEN** 共享场景提取保持一次，各路计划和理由对应其自身输入；另一路的区域、所需版本和请求可独立核对

#### Scenario: Reject a request and recover a usable layout

- **WHEN** 一个 View 已接受有效规划请求，随后收到不合法距离，且尺寸从无有效图像恢复到合法像素范围
- **THEN** 非法距离被拒绝且原请求保留，无效布局没有该轮阴影区域，恢复后的计划对应当前相机、尺寸和有效请求
