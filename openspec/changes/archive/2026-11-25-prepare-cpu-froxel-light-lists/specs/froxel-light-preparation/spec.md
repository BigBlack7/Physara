# Froxel Light Preparation

## Purpose

规定冻结帧中每个渲染视图怎样在稳定有限的视域内构造局部灯候选，向后续着色消费者交付可查询的网格、完整的全局局部灯输入，以及能核对来源和容量的 32 位紧凑数据。

## ADDED Requirements

### Requirement: Prepare a stable finite froxel grid per view

显式请求 Froxel 的有效 View SHALL 使用同次冻结场景、已解析相机、有效渲染矩形和稳定采样范围构造自己的网格。透视 SHALL 在正近平面与有限稳定裁剪远面之间作对数分层，缺省渲染远面时仍使用已解析的有限裁剪远面。正交 SHALL 在线性视深上分层并接受零近平面。当前抖动只影响从渲染位置到稳定网格的映射。同一相机、布局、采样范围及灯输入下只改变当帧抖动时，网格边界和候选集合 SHALL 一致。

结果 SHALL 对应产生它的 View 和冻结帧。各 View SHALL 共享场景输入，并在各自工作区内保有网格和候选。借用 SHALL 限于该次安全帧槽使用。相机、有效矩形或相关场景输入变化后，新结果 SHALL 反映当前输入。

#### Scenario: Build finite layers for an infinite render projection

- **WHEN** 透视相机省略渲染远面，冻结场景有近处和远处可绘几何，且同一稳定采样范围内采用两个不同的抖动
- **THEN** 两次结果的深度边界覆盖当前有限裁剪区间，视深按对数分层，网格和各 cell 候选保持一致

#### Scenario: Prepare an independent zero-near orthographic view

- **WHEN** 一路使用近平面为零的正交相机，另一路使用不同投影或有效矩形，两者共享场景，随后仅移动或调整其中一路
- **THEN** 正交结果自零起线性分层，两路结果各自匹配输入，改变的一路更新位置或范围，另一路及仍在使用的旧帧槽保有自己的结果

### Requirement: Resolve matching CPU and Shader cell coordinates

CPU 分箱与 Shader 查询 SHALL 使用同一份可表示的边界和 cell 顺序。XY 坐标 SHALL 以有效渲染矩形左上为原点，转换到未抖动的像素坐标，并使用稳定采样范围定义外缘。Z SHALL 使用以米计的正向视深。内部边界的查询 SHALL 归入起点位于该边界的 cell，有限远面 SHALL 属于最后一层。分箱 SHALL 保守包含与 cell 边界相交或相切的灯。超出覆盖或无法表示的查询 SHALL 返回无有效 cell。

#### Scenario: Locate a cell across render padding and shared boundaries

- **WHEN** 固定比例相机的渲染矩形带有留边，渲染和显示尺寸不同，样点落在内部 tile 或层边界、远面、扩展外缘及其外侧
- **THEN** CPU 和 Shader 合同从渲染矩形及当前抖动得到相同的稳定查询归属，远面选中最后一层，覆盖外返回无有效 cell，相切候选仍可被相邻的有效查询覆盖

### Requirement: Preserve conservative visible light candidates

Froxel SHALL 从所属 View 的完整可见灯集合构造候选。方向灯 SHALL 沿用独立的有效方向灯段。无显式范围的点光和聚光 SHALL 各在全局局部灯列表出现一次，并保持光照无距离截断。有限点光 SHALL 使用覆盖其影响球的保守判定，有限聚光 SHALL 使用同时覆盖其径向范围及外锥的保守判定。按分箱使用的保守界可证明与 cell 分离的候选 SHALL 被剔除，影响体相交、相切或判定不确定时 SHALL 保留。

各 cell 的有限局部灯索引和全局局部灯索引 SHALL 使用同一冻结 View 的有效着色灯表行号，并按行号递增。每个候选 SHALL 在一个查询结果中只出现一次。着色灯行与现有逐灯阴影引用的配对 SHALL 保持一致。

#### Scenario: Resolve finite and global lights with the current row order

- **WHEN** 可见列表含有交错的方向灯、无范围局部灯、影响体跨入 cell 的点光、斜向聚光和可证明分离的灯，且场景下标和着色行号不同
- **THEN** 方向灯继续来自独立前缀，全局列表只含无范围局部灯，cell 列表保留可能影响其覆盖的有限灯并排除可证明分离的灯，各条索引可解到正确的灯行及该行的阴影引用

#### Scenario: Represent a valid empty light set

- **WHEN** 有效 View 的完整可见灯集为空
- **THEN** 查询仍可定位网格内的 cell，各 header 的有效数为零，有限索引、全局局部灯及方向灯的有效数均为零

### Requirement: Publish a complete bounded 32-bit froxel payload

网格参数、header 和索引 SHALL 由 CPU 与 Shader 共用的权威接口交付。header SHALL 以 32 位无符号字段表达条目偏移、数量及溢出标记。候选行号 SHALL 保持 32 位无符号语义。成功结果 SHALL 含有与有效网格对应的完整 header，正常 header 的索引区间 SHALL 完全落在同一结果的已定义索引范围内。网格尺寸、索引总数、字段及字节范围 SHALL 在发布前满足配置及表达容量。无法表示或无法完整生成时 SHALL 返回可定位的不可用结果，本轮完整可见灯集仍可消费。

#### Scenario: Keep indices and offsets wider than compact upstream ranges

- **WHEN** 至少一个 cell 含有超过 255 个有效候选，后续 cell 的索引偏移超过 65,535，且总量在已声明容量内
- **THEN** header 的真实计数和条目偏移完整可读，首尾索引均解析到正确行，所有有效区间可由生成的 CPU 和 Shader 接口读取

#### Scenario: Reject an unrepresentable or over-capacity result atomically

- **WHEN** 请求的网格不能形成可表示的边界，或网格或索引总量超过已声明容量，随后用合法范围再次准备
- **THEN** 前次结果给出相应原因且没有可消费的部分 header 或陈旧索引，完整可见灯集继续可用，重试取得当前输入的完整有效结果
