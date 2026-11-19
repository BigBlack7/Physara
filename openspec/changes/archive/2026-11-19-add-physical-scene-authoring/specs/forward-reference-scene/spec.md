# forward-reference-scene Delta

## Purpose

提供通过正式编辑器打开的物理 Forward 参考场景，使同一组固定资产、米制几何、灯光和主 View 输入可以在后续阶段复现线性工作色、显示颜色及窗口颜色。

## ADDED Requirements

### Requirement: Ship one editable Forward reference scene with identifiable inputs

项目 SHALL 交付 `Assets/Scenes/ForwardReference.scene.json` 及其项目相对依赖，使用现有 v1 Open Scene、Scene、Forward 和显示路径。场景 SHALL 具备代表性介质与金属粗糙度材质球、可核对的米制尺度和已知方向及局部照明、固定相机和曝光、自制的非对称线性 HDR 环境，以及可辨认的方向和边缘标样。项目 SHALL 为场景与参与计算的实际依赖记录来源、许可和内容标识，包含模型固定源版本，足以重放原始输入。适用于该场景的编辑与另存 SHALL 沿普通文档合同工作。

#### Scenario: Open the fixed scene through the product workspace
- **WHEN** 用户从现有资源根打开参考文档并在主 View 选择 Physical Lighting
- **THEN** 正常帧出现核心材质、直接光、IBL 与天空，参照物可识别实际方向和尺度，主 View 请求的相机和曝光与随附记录一致

#### Scenario: Save a reference derivative through the normal authoring path
- **WHEN** 用户对参考场景中的一个实例、灯或环境应用有效编辑并另存为新场景
- **THEN** 新场景可通过同一路径重开，创作值随文档恢复，共享依赖和固定原始输入仍可用于基线

### Requirement: Compare fixed linear HDR and presented samples by their producer

参考检查 SHALL 固定输入内容、相机／投影、显式曝光、采样坐标、实际渲染和输出尺寸及有效显示请求。线性 HDR SHALL 按同一成功帧的实际生产模式及预曝光解释，可辨认标样 SHALL 在显示映射前与独立的已知光照或辐亮度预期比较。记录 SHALL 给出量化、允许误差及适用样本。视口和可读回的原生窗口 SHALL 按实际显示变换、采样范围、合成和窗口编码独立比较。

#### Scenario: Distinguish physical units, exposure and directional IBL in working HDR
- **WHEN** 用固定曝光和标记位置取得正式 Physical Lighting 帧，并使用同一基线做直接光与环境的隔离、旋转和 +1 EV 对照
- **THEN** 读回区分已知尺度的光强关系和非对称 IBL 及天空方向，物理受光样点的曝光比值在声明误差内，超过显示白的可表示工作值仍可用于比较

#### Scenario: Reproduce view and window colors with None and FXAA
- **WHEN** 在相同场景、相机、曝光和渲染栅格下获得实际接受的 None 与 FXAA 场景帧
- **THEN** 显示前 HDR 和深度对照一致，FXAA 在方向明确的边缘产生可区分结果，平坦区域及可对齐的视口和原生窗口样点符合所记录的颜色与编码口径
