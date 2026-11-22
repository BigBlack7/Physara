# Spec Delta

## MODIFIED Requirements

### Requirement: Retain frame bindings and resource generations until safe reuse

帧槽 SHALL 有界持有当前图的绑定、封存几何和上传数据。Pass 延迟使用的值及 CPU 载荷 SHALL 在消费期间有效，纹理请求元数据借用 SHALL 在下一次 UI 帧推进前通过确认或取消结束。GPU 使用的纹理、绑定及目标代 SHALL 至少保留至覆盖相应访问的真实完成。

逻辑 View 和 UI token SHALL 跨 resize 保持稳定。新帧可选择新颜色代，仍在途的工作继续使用原代。帧槽重新准备 SHALL 先取得真实安全复用证明，再销毁或清理旧图绑定并开始新图代，正常帧沿现有有界背压运行。

设备级图池 SHALL 为正常图和独立安全 UI 图提供相同的预算和首用合同。Runtime SHALL 在开放录制前依据各自的存活物理计划准入，区分计划硬超限、在途容量等待和有效外部持有占据的容量。主图无法准入时，只有重新声明并通过准入的独立安全 UI 图才可执行。安全图仍无法准入时 SHALL 归还取得并沿有界事件及退休流程继续，保持最初原因。执行中途失败继续按已有前缀协议处理。

View 的已接受输出和长期 UI 纹理 SHALL 持有对应的真实存储及跨帧入口事实。安全 UI SHALL 对旧输出重新声明采样，按实际使用更新最后一次访问的寿命。后继正常图 SHALL 为新请求使用自己的有效输出代。旧代仍可被合法读取时，新候选 SHALL 不覆写其受保留内容，即便描述和尺寸相同。Window PresentColor 继续由宿主管理其独立代。

#### Scenario: Resize while a previous binding remains in flight

- **WHEN** 先前提交仍持有视口颜色和 UI 绑定，当前布局或窗口尺寸要求新的目标代
- **THEN** 新帧的稳定 token 解析到正确的新输出，原提交继续引用旧对象，视口和窗口各按自身尺寸换代，真实完成后旧引用可退休

#### Scenario: Reuse a slot only after its submission completes

- **WHEN** 全部槽仍在途，随后某槽获得真实完成证明
- **THEN** 待完成期间输入和回收继续，槽存储及绑定保持有效，安全取得后旧帧数据被清理并建立新图代，旧准备值不能消费新帧内容

#### Scenario: Keep the accepted viewport while a new graph waits for pool capacity

- **WHEN** 新尺寸或格式的正常图受在途占用而未准入，上次接受的 View 输出在同一设备上仍可读，基础 UI 计划在现有容量内
- **THEN** 原计划没有开始录制。独立安全 UI 图实际采样旧代或按合同使用占位，保留请求原因及先前有效值。容量释放后可重试匹配候选，稳定 token 只在各图内解析自己的版本

#### Scenario: Cancel cleanly when even the safe graph cannot fit

- **WHEN** 可恢复容量压力仍阻止独立安全 UI 图准入
- **THEN** 归还本轮取得，没有新录制、无界重试或无效颜色提交。输入和退休继续推进，前一接受代与其资源依赖保持有效

### Requirement: Compile an independent safe UI fallback before recording

主图声明、封存、编译或图池准入失败且尚未开放即时列表, 已验证的基础 UI 程序仍可用、必需字体能够满足本次 UI 消费且没有 Fatal 时, 产品帧 SHALL 至多尝试一张独立的安全 UI 图。失败图及其帧内资源绑定 SHALL 结束后再建立新图身份与准备值。安全图 SHALL 使用同次合法呈现取得, 初始化并合成其窗口颜色, 沿相同的上传准入、图编译、图池准入、执行与宿主呈现合同运行。

安全图 SHALL 仅包含窗口初始化、必需 UI 上传和合成、合法驻留 UI 读取及同一 View 上次接受输出的显式读取; 不包含失败的诊断/场景生产分支或其读回与历史导出。旧输出 SHALL 具有有效内容、明确状态与独立寿命, 且未被失败候选覆写。无有效旧输出时 SHALL 显示带原因的占位。安全图的结果 SHALL 具有 `NoSceneOutputs` 语义。

安全图准备、编译或硬容量校验失败 SHALL 取消本次取得, 不递归 fallback。普通上传或图池容量等待 SHALL 归还取得并等待后续安全尝试。只要一次图执行已经开始, 即使在首次资源/recipe 创建处失败且尚无 Draw, 本轮也 SHALL NOT 拼接安全图, 而应按执行失败协议处理已开放录制。

#### Scenario: Replace a rejected main graph with a safe UI graph

- **WHEN** 主图被编译器拒绝, 同一 View 存在上次有效输出, 当前尚未开放列表
- **THEN** 失败主图没有执行上传或绘制命令, 新图只导入旧输出并合成 UI, 使用独立图代和新帧内绑定, 主图失败分支及其读回不执行

#### Scenario: Recover before any view output has been accepted

- **WHEN** 首次正常图失败, 没有可导入的已接受 View 输出, 基础 UI 可完成准备和上传
- **THEN** 安全图呈现明确缺失状态和编辑器界面, 不访问空或未初始化的旧纹理, 不伪造场景历史或有效 View 设置

#### Scenario: Cancel a failed fallback without another retry in the same acquisition

- **WHEN** 独立安全图准备、编译失败或其所需上传超过硬上限
- **THEN** 本次取得结束且无录制或交换, 不再生成另一张恢复图, 请求保持未确认, 后续事件和合法新帧仍可继续

#### Scenario: Refuse fallback after execution has started

- **WHEN** 正常图或安全图在首次物化、附件/绑定 recipe 或后续 Pass 执行时失败
- **THEN** 后续 Pass 和图尾成功发布停止, 本轮不执行另一张图, 窗口目标不获得复制或交换资格, 录制及可能已发前缀按真实提交或受控退出收拢
