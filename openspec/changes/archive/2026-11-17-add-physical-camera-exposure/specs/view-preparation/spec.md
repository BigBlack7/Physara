# View Preparation Spec Delta

## MODIFIED Requirements

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

## ADDED Requirements

### Requirement: Derive one calibrated pre-exposure for a prepared view

View SHALL 验证并接受有限正的光圈 f 数、快门秒数、ISO、传感器高度、导航焦距、对焦距离及有限的曝光补偿档数。计算光学关系时长度 SHALL 使用统一单位，导航镜头 SHALL 产生支持范围内的有限垂直 FOV。曝光 SHALL 使用 `EV100 = log2((N²/t) × (100/S))` 和 `preExposure = 2^(compensationEv − EV100) / 1.2`，正补偿提高预曝光倍率。倍率 SHALL 落在正有限的 FP32 正规数范围，值域或派生失败 SHALL 给出可定位原因并拒绝该完整请求。

成功准备的相机 SHALL 为本轮提供同一份 `preExposure`，并通过生成的 View Shader 接口交付该值。只改变曝光时，渲染及稳定投影、可见集合和视口布局 SHALL 继续遵循相同的几何输入。

#### Scenario: Read one stop of exposure from the current view data

- **WHEN** 固定镜头、投影和画布，设置 f/2、0.25 秒、ISO 200 和 0 档补偿，随后将补偿增加 1 档
- **THEN** 名义 EV100 为 3，当前倍率从约 0.10416667 变为 0.20833333，同一 View 的新 Shader 常量对应第二次准备，投影、布局与可见集合保持一致

#### Scenario: Reject an invalid camera request at its input boundary

- **WHEN** 用户或恢复请求提供零 ISO、非法长度、非有限参数或导致倍率超出支持范围的补偿
- **THEN** 请求整体被拒绝并指出对应字段或派生值，已接受的物理参数、投影及预曝光继续有效
