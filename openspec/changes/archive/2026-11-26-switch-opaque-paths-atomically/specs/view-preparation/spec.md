# View Preparation Delta

## MODIFIED Requirements

### Requirement: Prepare and confirm an opaque strategy per view

主 View 和显式离屏 View SHALL 持有独立的 Forward 或 Forward+ 请求，缺省 Forward。Physical Lighting SHALL 在两条路径下支持当前单样本 None 和 FXAA。Direct Lighting SHALL 沿用 LinearClamp/None。各模式 SHALL 保留合法的 AA 请求。路径编辑 SHALL 保留同一 Scene、创作状态及兼容的驻留数据。每个 View SHALL 分别公开最新请求、当前 Scene 最近确认的有效路径和当前请求的准备状态。首次匹配的受光计划接受前，有效路径 SHALL 为空。

候选计划 SHALL 对应当前 View、Scene、路径请求代，以及本轮相机、布局、画质与冻结的 Shader 代。图声明前，所需 Shader/Pipeline、绑定配方和资源需求 SHALL 满足当前计划的实际合同。缺项或瞬时额度未就绪 SHALL 报告 Pending。实际拒绝 SHALL 报告 Failed 和可定位原因。被取代或退役请求的完成、失败或取消 SHALL 无法修改当前请求的状态或确认其路径。相同路径的重复请求 SHALL 保持幂等。

当最新请求 Pending 或 Failed，原有效策略仍能满足当前输入时，该 View SHALL 继续正常绘制该策略。新帧 SHALL 使用当前相机、布局、Scene 内容及画质，确认实际执行的计划并继续呈现最新路径请求及原因。原有效策略也无法准备时 SHALL 使用仍有读取资格的先前输出或现有占位。构图和 Draw SHALL 消费完整封存计划，使用帧前准备的对象。

就绪候选 SHALL 从帧边界进入正常图。只有当前候选实际用于匹配且有效的正常提交时，View 才 SHALL 一起确认有效路径、实际画质、颜色、生产相机和布局、Shader 代及提交身份。主视口 SHALL 满足正常图的实际采样条件，显式离屏 View SHALL 有自己本轮匹配的输出回执。候选失配、被裁剪、取消、执行失败或只得到安全 UI 旧图读取时，原有效记录 SHALL 保持，直至匹配的正常输出被接受。测试图、材质预览和实际诊断着色的提交 SHALL 遵循各自的输出合同，保持受光计划确认状态独立。

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

## ADDED Requirements

### Requirement: Invalidate view history from the accepted plan compatibility

历史资格 SHALL 根据该 View 最近一次有效场景提交及本轮实际生产合同确定。作用域 SHALL 包含 View 和 Scene 身份、静态内容版本、投影及明确的相机 cut、颜色／深度的渲染和输出栅格、路径及相关 ABI、格式／样本与语义输入版本。成功接受不兼容计划时 SHALL 使受其影响的先前历史失去资格。新计划的首帧 SHALL 从没有这些兼容历史输入的状态开始。兼容计划间仅有连续相机位姿或曝光变化时 SHALL 保持相应历史资格。颜色、配对深度及生产相机、曝光等元数据在各已启用角色中 SHALL 随同次有效提交确认。当前单样本 None/FXAA 无时域消费者，SHALL 继续沿已接受场景记录表达生产身份。

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
