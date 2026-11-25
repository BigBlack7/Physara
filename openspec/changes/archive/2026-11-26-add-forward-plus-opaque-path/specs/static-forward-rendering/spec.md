# static-forward-rendering Spec Delta

## ADDED Requirements

### Requirement: Render opaque geometry with the prepared light candidate strategy

同一静态场景子图 SHALL 用同一表面准备、有序绘制、覆盖合同和反向深度交付两条不透明策略。Forward 的受光颜色 SHALL 消费完整可见灯集合。Forward+ 的 Direct Lighting 和 Physical Lighting SHALL 以参与逐灯求值的同一表面位置取得稳定投影下的像素坐标和米制正向视深，查询本 View 的 Froxel。正常格 SHALL 消费方向灯、有限候选及无范围局部灯。溢出、查询无有效格或无法保守定位表面时 SHALL 使用完整灯列表。两策略 SHALL 共用材质采样、核心 BRDF、逐灯阴影、环境光、预曝光和自发光。天空、诊断及后处理 SHALL 继续消费相同语义的颜色与深度输出。

#### Scenario: Render both strategies through the supported working and display outputs

- **WHEN** 以同一冻结场景、相机、曝光和已支持画质分别渲染 Forward 和 Forward+，可见项包含受光、MASK 和 Unlit 表面，并有有效的阴影和环境光
- **THEN** 实际颜色 Pass 使用各自的候选策略。可见覆盖、反向深度、HDR 贡献及天空和显示语义匹配，同一 AA 设置在两次渲染中被真实消费

#### Scenario: Use the correct cell in the render rectangle and keep the full-list fallback

- **WHEN** 有留边的有效渲染矩形内存在处在不同 X/Y 格及视深层的受光片元，同一帧同时有正常格和被预算压成 overflow 的格
- **THEN** 正常格按自己的栅格位置和正向视深取得候选，overflow 和网格外的有效片元保留完整局部灯贡献。使用透视无限渲染远面或零近平面正交时，查询仍遵守相应的稳定有限分箱合同

#### Scenario: Preserve a finite light contribution under a large rigid translation

- **WHEN** 相机、受光表面和有范围的局部灯一起平移，光栅深度的舍入足以越过 Froxel 内部层边界
- **THEN** 对实际覆盖的表面，Forward+ 保留在其照明位置有贡献的灯。相同输入的两策略交付一致的覆盖、反向深度及 HDR

#### Scenario: Evaluate the whole list when the projected arithmetic error crosses a cell boundary

- **WHEN** 同一受光表面的眼点可准确编码，片元高低位作差的舍入范围仍跨越 Froxel 边界
- **THEN** Forward+ 使用完整可见灯列表，交付与同位置的 Forward 一致的覆盖、反向深度和 HDR。误差范围可归于单一有效正常格时继续使用该格候选

### Requirement: Consume the live froxel upload of the same view

Forward+ 的受光颜色 Pass SHALL 读取本 View 同轮图上传写出的完整参数及三个结构化块，并使用同轮完整灯表和对应的阴影引用。图 SHALL 有精确的写版本、读用途和读范围。只有存活的 Froxel 消费者 SHALL 驱动该组原生上传。全灯颜色、表面准备、阴影生产、单灯诊断及后处理 SHALL 有各自实际使用的绑定。仅有不受光表面或背景时 SHALL 能由共享阶段交付颜色及所需深度。失败的图 SHALL 按真实提交保存已发工作所引用的资源和上传区段。

准备被拒或未选入图的 View SHALL 在仍可追加的上传批次中撤回其独占写入及额度。对仍有需求的候选 SHALL 保留可共享的生产者，失去所有候选消费者时 SHALL 归还其上传额度。后继需要未接受的资源时 SHALL 能重新建立对应生产者。

#### Scenario: Use separate view inputs and remove uploads with their consumer

- **WHEN** 主视图和离屏视图读取同次冻结场景，只有一路存在存活的 Forward+ 受光颜色 Pass，随后取消该 Pass 的最后一个实际输出消费者
- **THEN** 第一轮只有对应 View 的四个块参与原生上传与颜色绑定，每块由唯一生产者写出。取消消费后这组上传失去活性，另一路和仍被实际消费者读取的共享资源维持自己的访问合同

#### Scenario: Release unconsumed geometry within a shared upload budget

- **WHEN** 同一冷帧中的第二路 View 在登记仅自己需要的几何后被拒，另一 View 仍需共享几何和自己的 Froxel 数据
- **THEN** 只剩该 View 可用的生产者及其额度，未被它消费的上传不阻塞后续准入。被拒 View 用合法输入恢复时，所需但尚未接受的几何重新取得生产版本

#### Scenario: Protect the primary view's fixed upload demand during peer admission

- **WHEN** 同一帧的主 View 可独立交付，随后规划的离屏 View 可生成网格及绘制输入，但连同它的固定 Froxel 写入无法满足批次上限
- **THEN** 主 View 的固定需求已经纳入准入，离屏 View 的超限不影响主 View 的输出。主 View 收尾使用同额的实际写入，共享上传仍只计一次

#### Scenario: Retry a viable offscreen view after the primary plan releases its upload demand

- **WHEN** 附加 View 在规划时因上传额度被拒，主 View 随后在收尾阶段因 Pending 或 Failed 归还先前的额度
- **THEN** Runtime 允许附加 View 使用同一工作集重试一次。可独立交付的附加 View SHALL 能提交自己的正常输出，共享生产者保持单次计费，原生补备遵守同帧额度

## MODIFIED Requirements

### Requirement: Shade lit surfaces in the current pre-exposed working domain

Direct Lighting 的受光 OPAQUE 和 MASK SHALL 通过共享表面求值及核心 BRDF 交付完整的当帧直接光贡献。局部灯由已准备的不透明策略选择。覆盖、变换和背面法线沿用静态表面合同。透视观察向量 SHALL 取片元朝眼点的方向，正交 SHALL 使用固定反视线方向。几何粗糙度 SHALL 在覆盖分歧前取得有效导数，其结果供本片元所有灯使用。直接光各项 SHALL 乘一次入射余弦并线性相加，AO 不衰减直接光或自发光。直接光与线性自发光 SHALL 各乘一次同轮 `preExposure`。无灯时，非发光受光表面直接输出零光照，自发光仍按当帧曝光显示。

灯表 SHALL 保留可交付源灯位的精度，使分箱及着色使用相同的有限灯位置。灯位无法编码时 SHALL 交付灯表拒绝原因，并保有该 View 仍可读取的已接受输出。

#### Scenario: Accumulate direct light and emission at the current exposure

- **WHEN** 固定可见表面和相机几何，先分别测量方向及局部灯，再同时启用并增加一档曝光补偿，最后关闭所有灯
- **THEN** 直接光符合各灯线性叠加，直接光和自发光在工作色可表示的范围内同时随 +1 EV 翻倍，关闭所有灯后罩内像素只保留预曝光自发光或零

#### Scenario: Preserve masked coverage in the two opaque pass arrangements

- **WHEN** 带有遮罩孔和临界 alpha 的受光表面分别使用合并颜色深度和前置深度组合，并从明确投影类型的相机观察
- **THEN** 两种组合得到相同覆盖和反向深度，留下的受光片元使用符合该投影的观察向量、同一表面过滤和直接光响应

#### Scenario: Preserve the finite range cutoff under a large rigid translation

- **WHEN** 表面、相机和有限点光共同平移，灯位编码或片元相对作差有足以跨过显式范围的余差
- **THEN** 正常 Froxel 格和完整列表对范围边界及外侧的该灯贡献均为零，范围内的有效灯仍可着色，覆盖和反向深度沿用共享光栅合同

### Requirement: Display local shadow visibility for the selected light

Forward SHALL 通过正式 ViewOutput 交付当前 View 所选聚光或点光的线性阴影诊断。诊断 SHALL 使用当前接受场景对应的有效深度组和查询数据，以明暗显示实际可见性，点光另以颜色标识所查面，覆盖外用中性显示。主 View SHALL 可从现有 Renderer 面板提交选择及结束诊断，离屏 View SHALL 接受所属 View 的独立请求。结束诊断后 SHALL 按该 View 原请求的普通模式显示，方向光级联诊断 SHALL 保持可选。

面板 SHALL 区分当前所选灯、计划质量与预算原因，及最近一次正常接受的诊断和所属票据。当前无有效分配或诊断不可用时 SHALL 可按可用的普通模式出图。录制或提交失败时 SHALL 沿既有视口退路显示。仅当本轮正常图实际采样并完整接受诊断时，其结果 SHALL 被标为当前有效。普通受光模式 SHALL 通过该 View 已准备的不透明策略交付完整灯光贡献。

#### Scenario: Show accepted spot and point visibility in the viewport

- **WHEN** 用户从 Renderer 面板先选择一个有实际遮挡的聚光，再选择一个有跨面接收区的点光，最后结束诊断
- **THEN** 各次正常接受的视口颜色反映对应灯的遮挡及点光面，面板显示匹配的灯和接受状态，退出后恢复之前请求的普通画面

#### Scenario: Explain a budget rejection without losing the light or the viewport

- **WHEN** 一个 View 请求点光诊断，但在主方向光和聚光取得区域后剩余预算不足六层，随后调整阴影设置使其全组准入
- **THEN** 拒绝时面板显示当前灯的预算原因，普通受光输出仍包含该点光。若新诊断候选失败，上次接受画面及其票据仍可辨认。当前完整组实际获接受后才显示有效的六面查询
