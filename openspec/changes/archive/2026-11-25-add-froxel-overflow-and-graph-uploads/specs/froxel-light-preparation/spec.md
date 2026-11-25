# Froxel Light Preparation Delta

## MODIFIED Requirements

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

## ADDED Requirements

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
