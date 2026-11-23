# Spec Delta

## Purpose

定义 Windows 开发会话中现有静态 Forward 与显示 Shader 的源码变更、候选准备、帧边界发布和退休合约，使视图在新代编译、准备或失败期间继续消费明确的有效结果。

## ADDED Requirements

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
