# Spec Delta

## ADDED Requirements

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

## MODIFIED Requirements

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
