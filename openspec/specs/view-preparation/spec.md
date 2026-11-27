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

View SHALL 持有独立的会话阴影请求，默认关闭。请求 SHALL 由类型化入口校验和提交，支持开启状态、有限正的阴影距离及 1024、512、256 texel 边长档，缺省距离和边长为 50 m 和 1024 texel。过滤 SHALL 支持 Hard 与 PCF，缺省 Hard。每 View 逻辑阴影预算 SHALL 支持 4、8、16、32 MiB，缺省 32 MiB，有效额度同时受现有设备、层与项目预算约束。请求 SHALL 接受 1–4 个方向光级联、有限的透视对数权重 `[0,1]` 及过渡半宽比例 `[0,0.25]`，缺省依次为 4、0.5 和 0.05。过渡比例以相邻两段中较短的名义长度为基准，零比例表示直接切换。请求无效时 SHALL 报告原因并保留原请求。主 View SHALL 可从现有 Renderer 面板修改阴影开关、过滤、分辨率及逻辑预算，离屏 View 使用同一准备合同。

正式 View 准备 SHALL 从其已解析的稳定相机、可见灯和同轮共享 `SceneFrame` 交付匹配的计划或当前原因，并按实际显示分支解析所需阴影组。相机或布局不可准备时，当前 View SHALL 具有空的阴影工作集。计划不可用时，仍可用的相机、主可见集和完整直接光输入 SHALL 继续按正常准备合同交付。只改变一路 View 的阴影设置或相关布局时，独立的另一 View SHALL 保有各自的计划输入和版本。

#### Scenario: Prepare two distinct shadow requests from one frozen scene

- **WHEN** 主 View 和离屏 View 共享同一 Scene，分别使用有效相机及不同的阴影质量、预算、级数或开启状态，随后只修改其中一路请求
- **THEN** 共享场景提取保持一次，各路计划和理由对应其自身输入。另一路的区域、所需版本和请求可独立核对

#### Scenario: Reject a request and recover a usable layout

- **WHEN** 一个 View 已接受有效规划请求，随后分别收到不合法距离、越界级数或非法过滤/预算选项，且尺寸从无有效图像恢复到合法像素范围
- **THEN** 非法请求被整体拒绝且原请求保留，无效布局没有该轮阴影区域，恢复后的计划对应当前相机、尺寸和有效请求

#### Scenario: Reject offscreen attachments before publishing a replacement allocation

- **WHEN** 一个离屏 View 已交付点光六面，场景改用聚光且离屏尺寸超出普通 FXAA 所需的附件预算，并以相同输入再次准备
- **THEN** 无有效级联诊断可用时，首次拒绝按该 View 上次交付的六个区域结算，第二次拒绝的释放数为零且分配代稳定。尺寸恢复后从当前聚光输入重新准入，其余 View 的分配与颜色独立结算

#### Scenario: Report a directional plan failure alongside an attachment limit

- **WHEN** 离屏尺寸只容纳级联诊断，而试算或匹配的已缓存方向光计划报告空接收区间，或者当前规划因 CPU 存储分配失败而不可用
- **THEN** 离屏输出仍按附件超限拒绝且无新区域，阴影摘要同时保留实际方向光原因及规划拒绝计数。一次存储失败后可用原参数重新准备

### Requirement: Resolve the selected local shadow group for the current view

View SHALL 持有独立的会话诊断选择，初始使用普通画面。选择 SHALL 能在普通画面、方向光级联和一个聚光或点光之间切换。类型化请求 SHALL 以当前 View、Scene 与带 generation 的灯身份验收，拒绝外来、失效或类型错误的选择并保留原请求。主 View 和离屏 View SHALL 沿相同的身份与当前输入合同处理。场景文档继续沿 v1 合同读写。

正式准备 SHALL 从当前冻结 Scene 和匹配的 CPU 计划解析所选灯及完整的阴影区域。当该灯被删除、转为其它类型、失去覆盖、预算不足或阴影禁用时，当前诊断结果 SHALL 带可定位原因及空诊断深度组。普通显示仍可用时 SHALL 继续交付有效的相机及完整灯输入，并按普通分支的实际消费解析获准阴影。附件预检及阴影分配结算 SHALL 对应同一次实际显示配置与该 View 上次已交付的分配。

#### Scenario: Change one view selection and reject a stale light identity

- **WHEN** 同一 Scene 的主 View 和离屏 View 分别选择聚光和点光，随后其中一个灯被删除、其槽位由另一个对象复用，并收到旧灯选择请求
- **THEN** 原灯的诊断变为当前不可用，旧身份无法选中新对象。另一 View 的选择、计划及可接受输出继续独立解析

#### Scenario: Settle an offscreen layout with an unavailable local shadow group

- **WHEN** 一个离屏 View 请求所选局部灯的线性诊断，尺寸可容纳该诊断但超出其普通 FXAA 配置的附件预算，随后该灯失去阴影准入
- **THEN** 有完整组的候选按实际诊断配置准备。失去准入后，超限普通候选报告附件及阴影原因，原有规划区域按本 View 的已交付状态结算。重复拒绝不会重复释放，尺寸或请求恢复后按当前输入重新准备

### Requirement: Resolve the supported configuration for each opaque strategy

View 准备 SHALL 依据当前路径、预览、单样本 profile 及实际图资源用途解析可运行配置。Forward、Forward+ 和 Deferred 的 Physical Lighting SHALL 在满足设备及布局条件时提供单样本 None 和 FXAA，经同一显示合同交付。Direct Lighting SHALL 在三路径沿用 LinearClamp/None，保留合法 AA 请求。材质预览、测试图及实际阴影诊断 SHALL 保留各自的输出语义和适用提示，受光策略的请求继续有效。显式组合 Deferred 受光与有效阴影诊断的单个输出请求 SHALL 按现有不兼容合同拒绝。

当前基线 SHALL 只承诺单样本。Deferred+MSAA 的能力 SHALL 为 false，TAA SHALL 保持未启用。AO 和屏幕反射输入 SHALL 沿用关闭时的中性值，材质 AO 和已有环境照明仍按受光合同消费。设备格式、深度读取、附件范围或预算等无法满足候选的实际需求时 SHALL 报告被拒的配置及原因，保留合法请求。路径切换 SHALL 保留兼容的 AA 设置，无法支持的组合 SHALL 有明确失败结果。

#### Scenario: Resolve the actual AA for the selected lighting preview

- **WHEN** 保有 FXAA 请求的 View 在三策略的 Physical Lighting 下切换，再进入 Direct Lighting 或独立诊断输出并返回物理预览
- **THEN** 各物理帧实际交付 FXAA，Direct Lighting 和诊断按各自显示合同报告实际 None，合法的 FXAA 请求保留。独立诊断的回执未确认新的受光路径

#### Scenario: Expose the single-sample profile and a concrete compatibility failure

- **WHEN** 检查当前策略的能力，同时一个带合法 None 或 FXAA 请求的 View 的 Deferred 候选超出附件能力而同输入的 Forward 仍可执行
- **THEN** 当前基线显示 Deferred+MSAA 不支持且 TAA 未启用。Deferred 候选报告对应的附件限制，合法路径与 AA 请求可读取，已接受的 Forward 状态和真实画质可从自己的正常帧确认

### Requirement: Assemble one complete opaque subgraph from live demand

同一冻结 View 的正常图 SHALL 通过统一的表面准备及不透明光照入口选用一个已就绪的策略。Forward SHALL 用完整可见灯集合，Forward+ SHALL 用本 View 的共享 Froxel 及全灯退路，两者沿用普通前向表面合同。Deferred SHALL 使用最终配对 GBuffer 和 D32 交付核心照明与等深辅助颜色，实际受光项使用本 View 同一 Froxel 及退路。三路 SHALL 交付同一有效矩形的 RGBA16F 工作色、配对单样本 D32 和实际阶段身份，接续已有天空及显示。独立表面请求和普通颜色的附加表面仍遵循其各自的准备与读回合同。

所需准备角色 SHALL 来自当前候选的真实材质、阴影、环境和显示消费者。待选策略的原生键 MAY 按共享准备额度推进。本轮图 SHALL 声明选用策略的工作，存活消费者 SHALL 驱动其私有上传及附件。已获准的共享资源和其它 View 的实际需求 SHALL 保持可交付。Forward 或仅表面且无独立受光消费者的计划 SHALL 无专属 Froxel 写入。Forward+ 和 Deferred 的受光作用域 SHALL 分别使用本 View 的同次生产者，每组包含参数及三个结构化块。只有 Unlit 或天空的 View SHALL 保持现有底色、辅助颜色和覆盖合同。

#### Scenario: Use the real surface and color producers on each accepted strategy

- **WHEN** 同一冻结静态场景含核心受光、MASK 与不受光辅助表面，View 先后正常接受三种策略
- **THEN** 各次图及生产身份对应实际选用的表面、光照和后段。前向图使用其完整灯表或 Froxel 消费，Deferred 图使用最终表面和辅助等深用途，颜色与配对 D32 交付到同一显示链

#### Scenario: Keep only uploads and attachments with a live consumer when a candidate is dropped

- **WHEN** 主 View 的 Deferred 候选等待、被拒绝或被替换，当前 Forward 继续交付，同一帧的另一 View 有存活的 Froxel 受光消费者
- **THEN** 主 View 的待选表面、私有常量和 Froxel 不进入已提交的 Forward 图。另一 View 可读取自己的完整四块上传，共享生产者按实际读取保留。此后主 View 接受 Deferred 又切回 Forward 时，附件及图写入随真实选用策略变化

### Requirement: Prepare and confirm an opaque strategy per view

主 View 和显式离屏 View SHALL 持有独立的 Forward、Forward+ 或 Deferred 请求，缺省 Forward。候选和原有效策略 SHALL 各自按当前输入解析路径、预览及显示配置。路径编辑 SHALL 保留同一 Scene、创作状态及兼容的驻留数据。每个 View SHALL 分别公开最新请求、当前 Scene 最近确认的有效路径和当前请求的准备状态。首次匹配的受光计划接受前，有效路径 SHALL 为空。

候选计划 SHALL 对应当前 View、Scene、路径请求代，以及本轮相机、布局、画质与冻结的 Shader 代。图声明前，所需表面与颜色角色、Shader/Pipeline、绑定配方、附件和上传需求 SHALL 作为同一个可执行计划就绪。缺项或瞬时额度未就绪 SHALL 报告 Pending。实际拒绝 SHALL 报告 Failed 和可定位原因。被取代或退役请求的完成、失败或取消 SHALL 无法修改当前请求的状态或确认其路径。相同路径的重复请求 SHALL 保持幂等。

当最新请求 Pending 或 Failed，原有效策略仍能满足当前输入时，该 View SHALL 继续正常绘制该策略。新帧 SHALL 使用当前相机、布局、Scene 内容及画质，确认实际执行的计划并继续呈现最新路径请求及原因。原有效策略也无法准备时 SHALL 使用仍有读取资格的先前输出或现有占位。当当前请求已就绪而原有效策略不再适合输入时，该请求仍 SHALL 有资格独立取得本轮工作。构图和 Draw SHALL 消费完整封存计划，使用帧前准备的对象。

就绪候选 SHALL 从帧边界进入正常图。只有当前候选实际用于匹配且有效的正常提交时，View 才 SHALL 一起确认有效路径、实际画质、颜色及配对深度、生产相机和布局、实际阶段与 Shader 代及提交身份。主视口 SHALL 满足正常图的实际采样条件，显式离屏 View SHALL 有自己本轮匹配的输出回执。候选失配、被裁剪、取消、执行失败或只得到安全 UI 旧图读取时，原有效记录 SHALL 保持，直至匹配的正常输出被接受。测试图、材质预览和实际诊断着色的提交 SHALL 遵循各自的输出合同，保持受光计划确认状态独立。

已接受场景的统计快照及延迟 GPU 计时 SHALL 保有对应票据实际接受的策略和 View/Scene 身份。新的请求、待选方案及后台完成结果 SHALL 与已接受票据的事实独立。

#### Scenario: Activate a ready strategy at a frame boundary for either supported AA mode

- **WHEN** 同一静态场景的 Physical Lighting View 分别在 None 和 FXAA 配置下从 Forward 请求 Forward+，所需计划在帧边界就绪
- **THEN** 下一次匹配的正常接受记录对应实际执行的 Forward+ 计划、原 AA 请求及实际 Shader 代和票据。Scene、同版本的几何与材质驻留和编辑状态继续可用

#### Scenario: Continue the effective strategy while the requested one is pending or failed

- **WHEN** 已接受 Forward 的 View 请求 Forward+，候选先等待实际需要的 Pipeline，之后发生可定位的编译失败，期间用户移动相机
- **THEN** 可执行的 Forward 继续提交使用新相机的正常帧，路径仍为 Forward。状态依次报告待选请求 Pending 和 Failed 及相关原因。修复后只有匹配的有效 Forward+ 输出才确认切换

#### Scenario: Preserve the last usable output while the new plan is pending or failed

- **WHEN** 最新路径还在准备或已被拒绝，原有效策略也无法满足当前布局或资源需求
- **THEN** 状态说明当前拒绝或等待原因，仍可读的旧结果保留其原有身份。首次尚无可读颜色时使用现有占位，有效路径仍待确认

#### Scenario: Ignore a superseded switch result

- **WHEN** 一个 View 从已接受的 Forward 请求 Forward+，随后在候选完成前回到 Forward，之后发起新的 Forward+ 请求
- **THEN** 首个 Forward+ 的完成或错误无法发布或覆盖当前请求状态。复用的结果在匹配最新输入后才有资格为新请求服务

#### Scenario: Confirm the selected plan only after actual normal use

- **WHEN** 候选达到帧前就绪，但可见主视口没有采样本轮颜色，或该图只交付了失败工作的保活票据或安全 UI
- **THEN** 有效路径、接受输出和对应票据仍由上次正常记录证明。之后只有满足当前输入和实际消费条件的渲染可以发布新路径

#### Scenario: Resolve independent plans from the same scene frame

- **WHEN** 主 View 与离屏 View 各自在当前相机、布局和画质下请求另一条路径，其中一路先达到正常接受条件
- **THEN** 该路独立确认实际路径和输出，仍在准备的一路继续使用自己的可执行策略。后到计时显示原提交记录的路径

#### Scenario: Continue the effective view and another view after deferred preparation fails

- **WHEN** 主 View 已接受一条前向策略并请求 Deferred，其实际所需表面或屏幕颜色角色先等待准备额度，随后被拒绝，另一路仍有可执行的独立计划
- **THEN** 主 View 分别报告 Deferred 的等待和具体失败原因，兼容的原策略按当前输入继续正常交付，另一 View 按自身准备和导出独立确认。恢复合法输入后，主 View 只在实际提交匹配的 Deferred 结果时确认切换

#### Scenario: Resolve a ready forward request when the previous deferred layout no longer fits

- **WHEN** 一个已确认 Deferred 的 View 请求 Forward，同时新布局超出该 View 的 Deferred 附件预算而仍满足 Forward
- **THEN** 旧策略的独占限制只影响旧策略，已就绪的 Forward 可以使用新布局正常提交。有效路径、生产身份和历史资格由该帧真实回执确认

### Requirement: Invalidate view history from the accepted plan compatibility

历史资格 SHALL 根据该 View 最近一次有效场景提交及本轮实际生产合同确定。作用域 SHALL 包含 View 和 Scene 身份、静态内容版本、投影及明确的相机 cut、颜色／深度的渲染和输出栅格、路径及相关 ABI、格式／样本与语义输入版本。成功接受不兼容计划时 SHALL 使受其影响的先前历史失去资格。新计划的首帧 SHALL 从没有这些兼容历史输入的状态开始。兼容计划间仅有连续相机位姿或曝光变化时 SHALL 保持相应历史资格。仅未参与实际生产的角色或 Shader 代号改变，且实际生产合同兼容时，也 SHALL 保留该历史资格。颜色、配对深度及生产相机、曝光等元数据在各已启用角色中 SHALL 随同次有效提交确认。当前单样本 None/FXAA 无时域消费者，SHALL 继续沿已接受场景记录表达生产身份。

历史失效和元数据推进 SHALL 发生在接受对应 View 的成功提交时。请求变更、准备成功或失败、无消费、部分执行失败或安全 UI 的旧色读取 SHALL 保持相应候选未确认。仍获接受的原有效计划 SHALL 按它本轮真实输入结算历史兼容性。另一 View SHALL 独立依据自己的接受记录。

切换 SHALL 复用兼容的共享驻留、管线和图资源，只有实际不兼容的布局、格式、尺寸、样本或相关绑定得到替换。切换禁止重新加载 Scene、重建 RenderEngine 或等待全设备空闲。先前输出可按既有规则供只读展示，旧存储 SHALL 由实际使用票据、完成及保留引用保护。

#### Scenario: Resize while a path switch waits and then accept the new path

- **WHEN** 新路径仍 Pending，原有效策略随 View resize 在新栅格成功出图，之后新路径才在该栅格成功提交
- **THEN** 尺寸和路径的历史不兼容分别对应各自产生变化的接受帧。陈旧尺寸候选无发布资格，另一兼容 View 的历史不受此切换影响

#### Scenario: Keep compatible history through a pose change and an unaccepted candidate

- **WHEN** 当前有效路径在连续相机移动中正常渲染，随后另一条路径的候选在正常提交之前失败
- **THEN** 纯位姿变化的同路径渲染继续使用其兼容性合同。失败候选无法推进相机、路径或历史。实际切换成功后，受路径变化影响的旧历史不可用于新计划

#### Scenario: Keep earlier submissions and retained output readable after a switch

- **WHEN** 新路径已确认，旧正常输出仍由合法读取方保留或有旧帧在途
- **THEN** 兼容的共享资源继续可用，旧输出快照可读且对应原票据；旧绑定及图存储按各自引用和完成证据退休

#### Scenario: Apply deferred history changes only to the view that accepted the plan

- **WHEN** 一个 View 在等待切入 Deferred 期间先以原策略接受相机移动及新尺寸的帧，再遇到 Deferred 未接受的图尝试，之后以匹配输入成功切换，另一个 View 的生产合同始终兼容
- **THEN** 该 View 的兼容位姿帧保持资格，栅格和路径的实际变化分别在对应接受帧结算。未接受的尝试保有原记录，另一个 View 继续从自己的兼容提交取得前驱

### Requirement: Preserve opaque appearance across matched supported strategies

Forward、Forward+ 与 Deferred SHALL 在匹配的单样本受光配置下，对同一冻结场景、有效材质与实例、可见灯、相机、曝光、渲染矩形、阴影和环境输入交付等价的不透明画质。最近表面的选择基于现有覆盖和深度合同。具有共同确定胜出者的像素 SHALL 保持相同几何覆盖、最终 Reverse-Z 深度和核心、Unlit 及当前受支持辅助表面的可见性。MASK 及有效实例覆盖 SHALL 影响同一批样点。天空及留边 SHALL 服从同一图像范围和遮挡语义。

等价 SHALL 在正常接受且确认了实际策略的显示前线性 RGBA16F 和配对主 D32 上判定。已定义像素 SHALL 有有限数值。允许的核心 HDR 差异 SHALL 有针对相应输入预先登记的范围，可追溯至 GBuffer 编码、屏幕位置重建、阴影采样或工作色精度。Unlit SHALL 保持共享的颜色与覆盖。使用 Froxel 的路径 SHALL 保留同一有效方向灯和无范围局部灯贡献。溢出或合法的不确定查询 SHALL 用完整可见灯表保持对应画质，受光样点保留可辨认的实际退路。

#### Scenario: Compare mixed static surfaces with the matching accepted outputs

- **WHEN** 三策略分别接受匹配的核心静态实例和 Unlit MASK 输入，包含不同实例覆盖、前后遮挡、有限远面和固定比例留下的边缘
- **THEN** 在相同像素得到配对的覆盖、D32 和预曝光线性颜色。孔洞保留后方实际颜色，胜出的 Unlit 保留标准基色，核心的差异落在登记的精度范围内。留边和远面可由范围、覆盖与深度配对独立核对。

#### Scenario: Compare shadowed and unshadowed lighting under one effective profile

- **WHEN** 同一冻结场景的三策略实际消费相同阴影质量、方向光级联和局部阴影组，以及匹配的 IBL、天空与曝光
- **THEN** 相同接收面、遮挡区、亮区及天空的工作色和深度满足已登记的画质判据。移除阴影或环境输入的独立对照能辨认其对应贡献。

#### Scenario: Preserve global local lights through compact and fallback traversal

- **WHEN** 同一受光工作集含有限和无范围局部灯，Forward 使用完整表，Forward+ 和 Deferred 在实际照明中同时遇到正常格与受限额度造成的溢出格
- **THEN** 正常格保留适用的紧凑候选及全局灯；溢出格沿完整可见灯表取得贡献。三策略均在代表性受光像素和整图落入登记的颜色、覆盖与深度范围，负对照能发现遗漏的灯贡献。
