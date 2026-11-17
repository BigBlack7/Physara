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

View SHALL 从当前输出像素范围计算居中的有效图像矩形。固定透视 aspect 与正交 `xmag / ymag` SHALL 保持源比例，通过 letterbox 或 pillarbox 适配输出。缺省透视 aspect SHALL 取有效图像的像素宽高比。输出像素范围、有效图像矩形和渲染采样尺寸 SHALL 分别提供。图像坐标 SHALL 从有效矩形映射，矩形外的留边返回无图像坐标。无布局、零尺寸或无法表示非空矩形时 SHALL 返回明确的不可准备状态和空工作集。

#### Scenario: Fit a fixed camera to a differently shaped canvas

- **WHEN** 宽高比为 2 的固定相机准备 640×480 输出，面板内容与其图像区域分别用于指针坐标映射
- **THEN** 有效矩形为 `x ∈ [0,640)`、`y ∈ [80,400)`，图像坐标从矩形内映射，上下留边没有有效命中，投影仍按比例 2 求值

#### Scenario: Resume a view after an unusable layout

- **WHEN** View 缺少布局、换算后存在零轴或留边后无法得到有效像素，随后取得有效尺寸
- **THEN** 前者没有可用相机矩阵、图像映射或可见索引，后者从当前像素尺寸和请求重新准备

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
