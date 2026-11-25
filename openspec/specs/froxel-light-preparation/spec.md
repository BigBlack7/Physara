# froxel-light-preparation Specification

## Purpose

规定冻结帧中每个渲染视图怎样在稳定有限的视域内构造局部灯候选，向后续着色消费者交付可查询的网格、完整的全局局部灯输入，以及能核对来源和容量的 32 位紧凑数据。

## Requirements

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

网格参数、header 和索引 SHALL 由 CPU 与 Shader 共用的权威接口交付。header SHALL 以 32 位无符号字段表达条目偏移、数量及溢出标记。候选行号 SHALL 保持 32 位无符号语义。成功结果 SHALL 含有与有效网格对应的完整 header。正常 header SHALL 包含该格的全部有限灯候选，索引区间 SHALL 完全落在同一结果的已定义索引范围内。空候选 SHALL 以正常零计数表示。

有限索引池 SHALL 受请求及表达容量约束，并按稳定的 cell 顺序分配。每格候选全部可容纳时 SHALL 交付正常 header，恰好填满剩余预算为合法。其余非空格 SHALL 交付 overflow header，且该格占用的索引条目为零。后继格 SHALL 继续使用剩余预算判断。索引预算不足时，完整 header 和无显式范围的全局局部灯 SHALL 仍可消费。

网格尺寸、索引总数、字段及字节范围 SHALL 在发布前满足配置及表达容量。网格、完整 header、完整灯表行号或必须的 CPU 存储无法完整生成时 SHALL 返回可定位的不可用结果，本轮完整可见灯集仍可消费。

#### Scenario: Keep indices and offsets wider than compact upstream ranges

- **WHEN** 至少一个 cell 含有超过 255 个有效候选，后续 cell 的索引偏移超过 65,535，且总量在已声明容量内
- **THEN** header 的真实计数和条目偏移完整可读，首尾索引均解析到正确行，所有有效区间可由生成的 CPU 和 Shader 接口读取

#### Scenario: Admit a complete record or emit overflow within the remaining budget

- **WHEN** 同一有效网格含有预算内的格、超过当前剩余额度的非空格、此后恰好用尽剩余额度的格和空格，并再以零索引预算准备该 View
- **THEN** 可容纳的格各有完整索引区间，溢出格为显式 overflow 且占用零索引，空格为正常零计数。零预算下所有非空有限候选格使用 overflow，全局局部灯和完整灯表保持可用

#### Scenario: Reject an unrepresentable or over-capacity result atomically

- **WHEN** 请求的网格无法形成可表示边界，或所需 header 超过可交付容量，随后用合法范围再次准备
- **THEN** 前次结果给出相应原因且没有可消费的部分 header 或陈旧索引，完整可见灯集继续可用，重试取得当前输入的完整有效结果

### Requirement: Deliver froxel data through owned graph uploads

显式使用 Froxel 的受光消费者 SHALL 取得与当前冻结 Scene、View、相机、灯表及阴影引用配对的 GPU 数据。网格参数、完整 header 序列、有效有限索引和全局局部灯行号 SHALL 由封存前追加的帧上传包拥有，包含明确目标、实际字节范围和后继用途。初始字节来源的借用结束后，已准备的图上传 SHALL 仍可独立执行。

图中的结构化读取 SHALL 声明匹配写版本的 ShaderResource 用途，参数 SHALL 声明匹配写版本的常量读取用途。上传、绑定及有效索引范围 SHALL 满足实际设备和接口限制。有效数为零时 SHALL 提供合法已定义的活动绑定，Shader 的有效迭代数仍为零。完整灯表和可能访问的阴影数据 SHALL 属于同一读图。只有显式请求并被消费者使用的 Froxel 数据 SHALL 参与正常录制。

准备或设备准入失败 SHALL 保留原因，可用的完整灯通路继续具备独立消费资格。整份受光候选无法安全交付时 SHALL 沿现有准备或执行失败合同处理。只有匹配写入和消费均在正常图中完成并获得接受时 SHALL 确认该候选。已发命令所需的资源及上传区段 SHALL 留存至实际完成或既有退出收拢。

#### Scenario: Read the written payload for its own view

- **WHEN** 两个 View 共用一次冻结场景但各有不同的网格或候选，初始字节来源结束借用后，本图让各自的受光消费者读取其上传
- **THEN** 两个消费者分别取得自己该轮写入的网格、header 和灯行；图的生产者、精确访问范围及绑定可与实际读回核对

#### Scenario: Keep absent or unaccepted uploads out of a published result

- **WHEN** 一轮的 Froxel 消费者未请求或被裁剪，或实际上传后图只获得失败前缀的提交，随后重新准备并提交正常图
- **THEN** 无存活消费时对应上传不参与原生工作。失败前缀维持既有已接受结果及真实在途寿命，新的正常图只确认重试的匹配版本

### Requirement: Evaluate complete visible local lights on froxel fallback

受光查询命中正常 cell 时 SHALL 按共享灯表的相对行序评估该格的有限灯和无显式范围的全局局部灯，各灯行至多一次。命中 overflow 或查询无有效 cell 时 SHALL 走共享的完整局部灯循环，以完整灯表的当前顺序遍历全部可见局部灯，每行一次，且只读取该循环所需的数据。方向灯段 SHALL 独立求值一次。所有模式 SHALL 使用同一逐灯求值、对应灯行的阴影查询和预曝光合同。

#### Scenario: Match the full-list HDR result under forced overflow

- **WHEN** 在相同冻结场景、同一预曝光及画质下，一次以完整可见灯列表渲染，另一次以有正常格及 overflow 格的小索引预算渲染，样点含至少一个 overflow 和一个网格覆盖外的有效受光片元
- **THEN** overflow 与覆盖外样点的线性 HDR 与完整列表对照等价。正常格仍使用自己的候选，有限点光、聚光、无显式范围灯、方向灯和对应阴影引用保持正确

#### Scenario: Evaluate a normal empty cell with global lights

- **WHEN** 查询命中有限候选为空的正常格，且本 View 有无显式范围的局部灯
- **THEN** 该格的有限灯迭代数为零，全局局部灯各求值一次，方向灯段照常求值

#### Scenario: Preserve light accumulation order across an index budget boundary

- **WHEN** 同一格内有大量弱有限灯及完整灯表中靠后的无范围亮灯，两轮的索引额度分别刚好容纳和少一条
- **THEN** 正常格与 overflow 格按相同的相对行序累加这些灯，两轮的预曝光 HDR 与同包完整灯对照一致
