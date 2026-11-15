# Spec Delta

## Purpose

规定编辑器产品帧如何将上传、诊断视口、UI 采样及窗口颜色纳入同一张渲染图，使消费者能够确定本帧读取的资源版本、实际执行的工作和提交后的结果，并在裁剪、尺寸变化及帧槽复用时保持身份和资源寿命正确。

## ADDED Requirements

### Requirement: Compose the product frame through one compiled graph

每个可渲染的产品帧 SHALL 从封存的 UI、当前 View 请求和宿主取得的窗口目标形成一张图。图 SHALL 声明实际纹理及缓冲上传、诊断颜色生产、UI 采样、窗口颜色初始化与合成，并导出 UI 写入后的原窗口目标。各读取 SHALL 指向对应生产版本及范围。构图和编译 SHALL 只形成 CPU 声明与计划，存活 Pass 的资源物化和命令由图执行阶段完成。

产品帧 SHALL 使用编译计划驱动 Graphics 录制。图尾交付的呈现输出 SHALL 对应本次取得的纹理和 `ShaderResource` 出口。Runtime 持有关闭列表、窗口末端复制、提交和交换责任，并沿现有宿主协议保护末端访问。

#### Scenario: Present an editor frame with graph-produced diagnostic color

- **WHEN** 主视口可见、诊断颜色和字体需要上传，并取得合法窗口目标
- **THEN** 计划中上传先于对应采样，窗口底色先于 UI 合成，合成后导出原目标。执行并获有效提交后，窗口显示标识明确的诊断图和正确 UI 像素

#### Scenario: Export an initialized target from an empty UI draw set

- **WHEN** 已封存的有效帧没有普通 UI draw，且没有其它图内消费者
- **THEN** 图仍初始化并导出本次窗口颜色，无消费者的几何和纹理上传缺席，窗口可呈现约定底色

### Requirement: Resolve UI tokens to exact frame-local texture versions

UI SHALL 保持会话和代可校验的稳定纹理 token。长期 View 条目只关联 View 身份和输出角色，驻留条目持有外部纹理及导入事实。当前帧绑定 SHALL 在本图内把实际 draw 使用的 token 映射到确切纹理版本及采样范围。GraphId 的寿命 SHALL 限于所属图和帧槽使用期。

同一 token 的重复引用 SHALL 共享本帧解析结果。同一外部对象的多个 token SHALL 共享规范导入身份，并消费本帧更新产生的正确版本。UI 的实际纹理、视图和绑定 SHALL 由当前 Pass 的已声明权限解析。非基础图像的无效 token 或缺失 View 输出 SHALL 沿已有诊断占位合同处理，占位本身的上传及采样也进入图。实际 draw 必需的基础字体无法准备时 SHALL 按既有合同返回失败。

#### Scenario: Sample an updated texture through repeated or aliased tokens

- **WHEN** 多条普通 draw 使用同一 token，或两个有效 token 指向同一驻留纹理，且该纹理本帧有一次更新
- **THEN** 图中只有一份规范资源身份，所有相关 draw 采样同一更新后版本，各自的裁剪和几何顺序保持，更新只执行一次

#### Scenario: Resolve a newly produced view color during execution

- **WHEN** UI 在构图时引用本帧新声明且尚未物化的诊断颜色
- **THEN** 封存的绑定保存本图版本，构图不需要原生纹理或 BindingSet，UI 执行阶段从已授权资源取得实际颜色并采样

#### Scenario: Diagnose a stale token and reject an old frame binding

- **WHEN** 当前非基础图像 draw 含过期会话或代的 token，或调用者尝试在后继图中消费先前帧的绑定
- **THEN** 过期 token 解析为带原因的占位，先前帧绑定被拒绝且不会访问后继图中碰巧同索引的资源

### Requirement: Cull unused editor producers from actual UI reads

UI 的资源读取集合 SHALL 由会实际绘制的普通命令确定，空裁剪和 reset-state 命令不产生纹理采样。隐藏且无其它消费者的诊断输出，以及没有存活消费者的普通上传 SHALL 被裁剪或省去声明，其物理资源和命令不得因准备过候选而进入执行。保留为 View 或驻留纹理的输出 SHALL 只在本帧实际消费需要该版本时成为外部导出。

同一图版本只要仍有合法 Image 消费者，相关生产者 SHALL 保持存活。隐藏和恢复视口 SHALL 沿用逻辑身份，恢复帧以当前布局与 View 请求解析输出。

#### Scenario: Hide or fully clip the only diagnostic consumer

- **WHEN** 诊断视口隐藏，或其全部 Image 命令的像素裁剪为空，且其它 UI 正常可见
- **THEN** 其它面板继续呈现，诊断分支没有原生分配和上传，View 有效设置保持最后接受值

#### Scenario: Keep a producer for a remaining Image reference

- **WHEN** 同一候选诊断颜色被多个 Image 引用，其中一条被裁剪而另一条实际绘制
- **THEN** 生产者保留且只执行一次，存活 Image 采样本帧版本，重复引用不形成多份资源或上传

### Requirement: Admit the uploads that will actually execute

帧准备 SHALL 根据存活计划中实际上传的大小、次序及列表参数核对现有上传容量，并独立持有执行需要的字节。上传准入与实际命令 SHALL 使用一致的工作集合。被裁剪的上传不得占用本次 GPU 上传额度或变为已确认内容。

诊断颜色暂时无法纳入共享上传容量时，准备 SHALL 沿既有 UI 优先行为选择带原因的占位并重新确定完整正常帧的依赖与预算。基础 UI 仍等待容量时 SHALL 在开放列表前归还本次呈现取得，保持待处理请求，继续输入与退休推进。

#### Scenario: Exclude a culled texture request from upload admission

- **WHEN** 合法纹理上传请求已规划，但它的结果在最终图中没有存活消费者
- **THEN** 本次准入和命令均不包含该上传，请求保持待处理，其后真正被 UI 消费时才进入新的准入和执行

#### Scenario: Preserve UI priority under upload backpressure

- **WHEN** 加入诊断上传将超出当前共享上传额度，而使用占位的基础 UI 可以纳入
- **THEN** 当前帧呈现带等待原因的 UI，预算与最终执行计划一致，View 保留最后有效设置。若基础 UI 也等待容量，本轮没有新 Graphics 录制

### Requirement: Publish only accepted graph outputs

图执行成功 SHALL 交付所请求的版本和实际对象。UI 上传确认及 View 有效比例、尺寸和颜色代发布 SHALL 同时满足对应工作实际执行、当前图完整成功，以及本次宿主回执接受完整有效颜色。提交票据、帧槽、录制和准备值 SHALL 属于同一次工作。诊断输出继续使用 `NoSceneOutputs` 语义，场景历史保持原状态。

构图、编译、执行、关闭或提交失败 SHALL 保留原始诊断，结束本次 CPU 请求借用，并阻止成功输出发布。尚未发出命令时归还取得。已经发出的工作 SHALL 沿现有前缀提交或受控退出责任保活，失败目标不得进入有效颜色复制和交换。交换结果继续独立于已经接受的输出发布。

#### Scenario: Accept a sampled view version with its matching submission

- **WHEN** 当前请求对应的诊断版本实际被 UI 采样，图及关闭成功，宿主接受完整有效颜色
- **THEN** View 发布对应比例、像素范围及颜色代，相关已执行上传可确认，诊断图不推进场景历史，后续交换状态单独报告

#### Scenario: Preserve pending state when graph work fails

- **WHEN** 图在命令发出前失败，或已发上传后发生执行错误并只获得前缀票据
- **THEN** 当前帧不发布有效颜色、上传完成或 View 有效设置，首错可定位，未执行取得可取消，已发前缀及引用按真实完成或受控退出收拢

### Requirement: Retain frame bindings and resource generations until safe reuse

帧槽 SHALL 有界持有当前图的绑定、封存几何和上传数据。Pass 延迟使用的值及 CPU 载荷 SHALL 在消费期间有效，纹理请求元数据借用 SHALL 在下一次 UI 帧推进前通过确认或取消结束。GPU 使用的纹理、绑定及目标代 SHALL 至少保留至覆盖相应访问的真实完成。

逻辑 View 和 UI token SHALL 跨 resize 保持稳定。新帧可选择新颜色代，仍在途的工作继续使用原代。帧槽重新准备 SHALL 先取得真实安全复用证明，再销毁或清理旧图绑定并开始新图代，正常帧沿现有有界背压运行。

#### Scenario: Resize while a previous binding remains in flight

- **WHEN** 先前提交仍持有视口颜色和 UI 绑定，当前布局或窗口尺寸要求新的目标代
- **THEN** 新帧的稳定 token 解析到正确的新输出，原提交继续引用旧对象，视口和窗口各按自身尺寸换代，真实完成后旧引用可退休

#### Scenario: Reuse a slot only after its submission completes

- **WHEN** 全部槽仍在途，随后某槽获得真实完成证明
- **THEN** 待完成期间输入和回收继续，槽存储及绑定保持有效，安全取得后旧帧数据被清理并建立新图代，旧准备值不能消费新帧内容
