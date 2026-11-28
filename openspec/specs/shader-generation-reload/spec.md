# shader-generation-reload Specification

## Purpose

定义 Windows 开发会话中现有静态 Forward 与显示 Shader 的源码变更、候选准备、帧边界发布和退休合约，使视图在新代编译、准备或失败期间继续消费明确的有效结果。

## Requirements

### Requirement: Observe declared Shader sources and coalesce edits

开发会话 SHALL 能显式选择有效的源码根并启用文件变更通知。通知 SHALL 覆盖已纳入目录的程序请求、权威接口及实际传递依赖，支持写入、重命名替换、删除后重建等保存方式。受影响程序 SHALL 依据经核实的输入确定。通知重复、过密或不完整时 SHALL 以有界方式合并并重新核实相关输入。内容等价可沿用既有结果。缺失根、丢失依赖和观察失败 SHALL 有界报告原因。常规打包启动 SHALL 可由现有包准备程序。

#### Scenario: Save a shared include through an atomic replacement

- **WHEN** 开发编辑器以临时文件替换实际使用的 include，一次保存产生多条通知
- **THEN** 该批最终内容对应一个有效请求，相关程序重新核实依赖，未受影响的程序保留可复用结果

#### Scenario: Use packaged shaders when development inputs are unavailable

- **WHEN** 产品从打包资源启动，或开发会话指定的源码根不可用
- **THEN** 有效打包输入仍能准备和运行。仅在请求了开发源但无法读取时报告明确的热重载原因

### Requirement: Prepare the current Shader candidate within work budgets

每次候选 SHALL 关联请求身份、程序和变体、实际输入版本及所需准备范围。后台工作 SHALL 只执行 CPU 预处理、编译、验证和反射，并交付自持结果。来源修订与所需准备输入 SHALL 在接收及发布时核对。更早请求的完成、失败或取消回执 SHALL 不替代当前候选。队列、结果及待重试工作 SHALL 保持有界。

OpenGL 原生准备 SHALL 在设备 owner context 中分批推进，每次帧前推进 SHALL 遵守明确的工作额度并记录实际成本。缺项 SHALL 处于可辨认的 Pending 状态。构图、绘制回调和 Draw 消费已准备对象，SHALL 不发起缺项编译或 link，也不等待后台任务、设备空闲或超额准备。

#### Scenario: Discard an older result after rapid requests

- **WHEN** 首个编辑的 CPU 构建正在执行，后续修改或请求组合更新先到达
- **THEN** 等待内容合并到最新请求，先前结果无法推进当前候选。下一份完成结果的请求、内容及变体身份可追溯

#### Scenario: Carry unfinished native work to another frame

- **WHEN** 完整候选需要的原生 Pipeline 创建次数超过本次准备额度
- **THEN** 已开始的操作保持其真实结果，空余工作跨帧继续。当前帧记录额度和 Pending 原因，绘制仍沿用可执行计划

### Requirement: Publish only complete compatible Shader generations

候选 Shader 代 SHALL 在所需包已验证、各角色接口及资源使用匹配、原生 Pipeline、绑定布局、绑定配方与所需资源代全部就绪后才具备发布资格。相同身份的依赖可复用。兼容性 SHALL 同时核对运行中的 CPU 接口、活动反射、跨阶段 IO、实际绑定及目标信息。就绪且仍是当前请求的候选 SHALL 在构图前的帧边界整体发布，使该帧所有相关 Pass 和 View 取得同一代。已冻结帧 SHALL 继续持有它取得的代。

编译、反射、原生准备或资源兼容失败时，候选 SHALL 保持 Failed 并保留程序、变体、阶段、可用的源路径和原始行号及首个可定位原因。原生日志无法定位到源码时 SHALL 保留实际报告的位置及原生原因。失败保留旧完整代，后续源或请求变化可产生新候选。尚无就绪代时 SHALL 沿现有诊断画面或安全 UI 入口呈现原因。

#### Scenario: Switch every current consumer at one frame boundary

- **WHEN** 主视图与离屏视图使用同一 Shader 家族，依赖同一修改的候选仍缺少最后一项准备
- **THEN** 准备期间两视图保持旧有效代或各自的上次接受输出。缺项完成后下一次有效发布只交付一份完整代，所有选用的新计划引用该代

#### Scenario: Reject an interface the running CPU cannot consume

- **WHEN** Shader 编辑改变运行中数据块布局、跨阶段接口或实际绑定形态，使现有业务数据或配方不兼容
- **THEN** 发布被拒绝并定位冲突，旧布局、Pipeline、资源与可读输出仍完整，修正后可以发起新的准备

#### Scenario: Report a source failure and recover upon a later edit

- **WHEN** 实际使用的 include 含有能被编译器定位的语法错误，随后用户修复该位置
- **THEN** 失败记录标明程序、变体、原文件行号和原因，旧完整代保持 Active。修复后的当前请求可经过完整准备并在帧边界发布

### Requirement: Retire generations according to recorded ownership and completion

替换后的 Shader 代 SHALL 依照冻结帧、绑定依赖及实际提交保活。旧 GPU 工作尚未取得完成证明时，更新 SHALL 保留其 Pipeline、布局及所需资源。缓存腾退、候选取消和退出 SHALL 分别释放各自持有，原生回收沿设备线程和 context 的既有合同推进。停止会话 SHALL 先停止通知和新任务，消化或丢弃已取消 CPU 结果，再解除候选及设备持有。设备不可用时 SHALL 维持已有受控退出语义。

#### Scenario: Keep an older generation usable while its frame remains in flight

- **WHEN** 新代已经发布，上一代的提交还没有实际完成
- **THEN** 新录制使用新代，旧包仍拥有其实际依赖。最后一次相关使用完成且持有解除后，旧对象按既有协议退休

#### Scenario: Close while a candidate is still being built

- **WHEN** 文件通知或候选 CPU 编译尚未结束时关闭会话
- **THEN** 尚未发布的结果不能再进入原生准备或修改 Active。退出收拢 CPU 工作和源观察，再依设备退出顺序清理已经准备的原生资源

### Requirement: Admit active resources according to the opaque Shader role

产品目录 SHALL 为当前可达的全灯与 Froxel 颜色角色定义有限且可辨认的结构变体。打包冷准备与热重载准入 SHALL 对同一角色使用一致的 CPU 绑定合同，核对生成 ABI、各阶段活动资源、类型、槽位和可交付范围。Froxel 颜色角色 SHALL 具备原有 View 常量和完整灯输入，并在片元阶段具有网格参数、header、有限索引和全局局部灯四个活动资源。全灯颜色、深度、阴影诊断及其余共享角色 SHALL 使用其原有活动绑定配方。当前没有绘制需求的角色也 SHALL 在候选代准入时核对其角色合同。

原生 Pipeline 及布局 SHALL 由实际 View 需求在帧前额度内推进。产品角色及其绑定发生冲突时 SHALL 交付可定位的失败，旧兼容代依照已有的帧与提交所有权继续可用。只有实际需要且具备完整准备内容的角色 SHALL 成为本轮就绪计划的消费者。

同帧离屏重试新增的原生键 SHALL 并入当轮需求去重，保留主路的待办键及其准备原因。同帧补备 SHALL 使用原有剩余额度和当前代，后续预热可接续未完成工作。

当前活动代某角色的程序包因文件缺失、读取超限或包级准入失败而被拒绝时，该角色的所有固定状态键 SHALL 保有同一原因，仍属于本帧实际需求。其它角色 SHALL 在剩余额度内继续准备。资源不足、系统失败及原生设备错误 SHALL 沿用既有中止规则，候选代 SHALL 遵守完整发布条件。

#### Scenario: Prepare the packaged Froxel role on demand

- **WHEN** 无开发源码的会话先运行全灯 Forward，之后请求需要 Froxel 颜色 Pipeline 的正常视图
- **THEN** 全灯图可使用原布局完成。Froxel 角色依照准备额度由其产品包取得正确的活动资源与原生 Pipeline，缺项期间报告 Pending。就绪后该图可绑定四个当前 View 的活动块

#### Scenario: Reject a mismatch in either opaque role before publishing a hot reload

- **WHEN** 候选代中某个 Froxel 角色的活动资源缺失或类型、阶段、槽位错误，或者某个全灯角色意外激活 Froxel 绑定
- **THEN** 准入指出对应角色及资源冲突，即使该角色当前没有被 View 选中。旧兼容代仍可用，修复后的代可按真实需求完成准备并在帧边界发布

#### Scenario: Reject a depth role that requests a set with no layout

- **WHEN** 同 ABI 的深度片元程序在覆盖配方之外激活只有颜色角色能绑定的灯表
- **THEN** 冷包与热重载准入均拒绝该角色并定位资源。当前未请求前置深度时也拒绝候选发布，已有兼容代仍能准备前置深度

#### Scenario: Reject a depth vertex program missing the View transform

- **WHEN** 同 ABI 的深度程序保持覆盖配方，但其顶点阶段未使用必需的 `perView` 变换输入
- **THEN** 冷准备和热重载均定位到该角色的顶点必需项并拒绝新包，已有有效代仍能交付前置深度。深度片元可按原覆盖配方省略该常量

#### Scenario: Reject an opaque color program missing the MASK sampling inputs

- **WHEN** 同 ABI 的普通受光颜色包缺少基色图像或采样器，包含该角色的热重载候选尚未被 View 选中
- **THEN** 冷准备和热重载均定位到颜色角色缺失的材质输入并拒绝新包。保留的合法代及随后启用的修复代均可正确交付 MASK 孔洞后的颜色与深度

#### Scenario: Reject a color vertex program missing its normal-map material input

- **WHEN** 同 ABI 的受光颜色包保留片元材质配方，但失去顶点阶段用来为法线 UV 变换建立正确切线基底的 `materialRows`
- **THEN** 冷准备与未选角色的热重载均报告顶点缺项并拒绝新包。正常颜色仍使用兼容旧代，恢复的合法代继续按材质的法线 UV 变换着色

#### Scenario: Preserve pending primary work during an offscreen retry

- **WHEN** 本轮原生额度已经用完，主路仍等待 Shader 键而归还上传额度，离屏候选重试后可以正常提交
- **THEN** 原生准备快照继续报告主路所欠的角色与预算等待原因。离屏提交后，无新计划的预热可推进主路缺项，同帧新增的创建次数仍受原额度约束

#### Scenario: Prepare the independent offscreen role after a primary package rejection

- **WHEN** 主路的多个固定状态键引用同一个缺失、超限或不兼容的包，并在拒绝后归还批次配额。离屏重试需要尚未准备的兼容角色，且本帧仍有足够原生额度
- **THEN** 主路保留该角色的失败原因及整帧需求。补备跳过已拒角色的其它键，准备离屏缺少的原生对象，使离屏在本轮提交并交付与单独运行一致的输出

#### Scenario: Retry a restored package after the rejected role leaves demand

- **WHEN** 已拒角色退出 View 需求，随后合法包就位，View 重新请求该角色
- **THEN** 准备器读取恢复的包并按原生额度推进。View 只在匹配的正常图提交后更新已接受的输出与路径身份

### Requirement: Prepare and admit IOR and specular color roles

产品 Shader 目录 SHALL 为受支持的 IOR/specular 颜色角色声明有限的结构变体。角色 SHALL 按使用的全灯或 Froxel、直接光或物理环境、阴影资源匹配现有消费者，并具有扩展颜色实际使用的 specular 强度和颜色纹理及采样器。仅需几何覆盖或表面数据的角色 SHALL 以其实际资源配方准备。核心颜色角色的活动资源合同仅包含自身所需输入。

打包冷准备与热候选 SHALL 校验相同的 ABI、角色和阶段活动资源。即使当前 View 未选择扩展角色，缺项或类型、阶段、槽位不兼容的候选 SHALL 给出具体原因。待执行的 View 只消费就绪的匹配色角色和材质代。发布失败 SHALL 沿现有机制继续使用旧完整代及其在途持有，后续匹配候选在帧边界确认。

#### Scenario: Reject an incompatible IOR and specular program in both preparation paths
- **WHEN** 冷包缺少扩展颜色角色必需的 specular 纹理或采样器，热候选包含同类缺项且角色当前未被 View 选用，随后提供兼容版本
- **THEN** 两种准入均定位相应角色与资源，热拒绝期间旧完整代仍可用于已有计划，合法候选发布后的正常提交使用匹配的新 Shader 代与实际纹理

### Requirement: Prepare and admit the clearcoat color roles

产品 Shader 目录 SHALL 为清漆定义有限的受光颜色角色，与全灯或 Froxel、Direct 或 Physical、实际环境和阴影组合对应。核心与 IOR/specular 基底 SHALL 由清漆角色处理其相应清漆材质数据。角色 SHALL 具有清漆强度、粗糙度、法线的活动纹理和配套 sampler，并为所需的两个表面框架使用匹配的生成接口。核心及仅 IOR/specular 颜色、独立表面、深度、阴影和诊断 SHALL 按对应消费者的阶段活动资源合同准备。

冷包与开发热候选 SHALL 对所有相关角色核对相同 ABI、阶段、资源类型、槽位及绑定能力。未选的清漆角色也 SHALL 通过候选准入。准备与发布 SHALL 按实际预算推进，失败时给出可定位的角色和资源原因，旧完整代及其已经持有的输出继续沿提交合同服务。

#### Scenario: Prepare a packaged clearcoat program on real device limits
- **WHEN** 打包产品在当前设备首次为可见清漆材质准备受光颜色，另一个 View 只需要表面准备
- **THEN** 受光色角色使用配对的材质表、清漆纹理及实际设备允许的绑定，独立表面只消费其所需资源。准备就绪后接受的颜色使用该角色和有效材质

#### Scenario: Reject a missing clearcoat binding before cold or hot publication
- **WHEN** 冷包缺少所需清漆活动绑定，随后未被 View 选中的热候选包含同类错误，再提供完整候选
- **THEN** 冷包和热候选均定位到相应角色、阶段和资源，原有效代保持可用资格，合法恢复只在同一代的真实正常提交后确认新颜色
### Requirement: Prepare and admit the finite sheen lighting roles

产品 Shader SHALL 为 sheen 及其与清漆共用的颜色声明有限角色，按全灯或 Froxel、Direct 或 Physical 的实际照明需求、阴影匹配相应消费者。色角色 SHALL 具有配对材质表、两槽及其采样器、活动 UV 输入和匹配 DFG 读。环境 sheen 角色 SHALL 具有匹配的预滤波环境活动读。组合角色 SHALL 同时满足清漆两套法线框架的阶段合同。独立表面、深度、阴影和诊断仅按其实际输入准入。

产品冷包与开发热候选 SHALL 按一致的 ABI、活动资源、阶段、槽位、绑定类型和真实设备限制准入所声明的角色。当前未选角色的缺项也 SHALL 拒绝不完整候选并报告原因。活动与候选的准备 SHALL 按有界预算执行，整代匹配后才允许新正常提交确认。

#### Scenario: Prepare a reflected sheen role and its same-generation resources

- **WHEN** 在真实设备上准备单独 sheen 及含清漆、阴影和 IBL 的 sheen 颜色，并同时存在独立表面用途
- **THEN** 各角色通过对应生成 ABI、两阶段活动资源与设备准入。所用颜色从实际绑定的相符材质、DFG、遮挡和环境交付，独立表面仅消费自身资源

#### Scenario: Refuse a missing sheen binding in both cold and unselected hot roles

- **WHEN** 一个冷包和一个热候选的 sheen 角色缺少必需的活动绑定，热错误角色当前未被选择，随后从合法输入得到完整候选
- **THEN** 冷、热准入定位角色与缺项，已有可用代保留有效输出资格，匹配候选在整代就绪后由新正常帧确认
