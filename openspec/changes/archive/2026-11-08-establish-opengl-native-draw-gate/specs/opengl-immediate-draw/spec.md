# Spec Delta

## ADDED Requirements

### Requirement: Render a fixed asymmetric reference through the canonical graphics chain

基线图形能力 SHALL 提供可重现且可自动判定的完整原生图像门槛. 固定参考输入 SHALL 经权威接口生成、canonical 编译包、后端 lowering、公共绑定和 Draw 产生实际附件内容, 不以替代 Shader、原生绘制或从实际输出反推的预期代替该链. 输入 SHALL 在运行前固定逻辑尺寸、四角不同颜色/形状标记、深度梯度、实例范围、索引偏移及数值域; 独立预期 SHALL 能区分上下/左右反转、错误深度比较、遗漏或重复的 base instance/base vertex 和颜色转换错误.

门槛 SHALL 组合验证既有 UPPER_LEFT/ZERO_TO_ONE、CCW/负缩放、viewport/scissor 和主视图 Reverse-Z 合同. 内部采样位置、清除区和误差限 SHALL 预先声明, 不依赖边缘光栅化差异或截图目测. 浮点数值、sRGB 存储和采样解码 SHALL 分域判定, 不仅用 0/1 饱和值宣称颜色正确.

#### Scenario: Produce a known asymmetric image through the formal chain
- **WHEN** 固定参考输入在真实后端经生成接口、编译包、绑定与公共即时 Draw 完整执行
- **THEN** 四角身份、形状方向、有效区域及清除区分别满足独立预期, 自动判定无需人工观察, 任一不符均使本次门槛失败

#### Scenario: Distinguish the requested instance and indexed vertex ranges
- **WHEN** 参考绘制具有非零索引绑定 byte offset、first index、base vertex、first instance 及多个实例, 未选中的前后记录带有不同标记
- **THEN** 只有指定记录产生预期图案, Shader 实例/顶点身份各包含一次相应偏移, 忽略、重复或混淆偏移不能得到通过结果

#### Scenario: Judge depth orientation and color with independent numeric expectations
- **WHEN** 同一有限参考输入包含深度梯度与重叠遮挡、正反面/负缩放标记、受限 scissor、非饱和线性值及 sRGB 写入和采样
- **THEN** 深度选择、未写区域和逻辑行序满足主视图约定, 颜色在各自数值域内满足事先固定的容差, 不通过额外翻转、修改 clip convention 或放宽预期掩盖错误

### Requirement: Preserve the reference across framebuffer copy and sampling consumers

整体图像门槛 SHALL 覆盖相同兼容信息但对象及尺寸不同的 Framebuffer, 同一已准备图形 Pipeline SHALL 能按各次动态区域绘制正确目标. clear → Draw → 同格式单样本二维 copy → 后继纹理采样 SHALL 通过同一正式资源/访问/绑定合同产生可判定结果. copy SHALL 保持请求 texel、编码和逻辑行序, 后继采样 SHALL 使用该版本及正确 Texture/Sampler, 不采样同时可写的重叠附件.

#### Scenario: Reuse a prepared pipeline on another compatible target
- **WHEN** 同一准备好的图形 Pipeline 依次绘制到对象和尺寸不同但兼容信息相同的两个目标
- **THEN** 各目标按自身 viewport/scissor 得到对应参考图案, 不残留前一目标、尺寸或状态, 不以重建同一 Pipeline 绕过目标切换

#### Scenario: Observe copied texels through a later sampled draw
- **WHEN** 已绘制非对称内容的合法区域经公共 copy 进入另一纹理, 后继公共 Draw 再将其采样到不同输出附件
- **THEN** 复制区及未覆盖区、采样方向与数值符合各自预期, 没有隐式翻转、重采样式 copy、陈旧版本或非法反馈; 最终像素能定位这条实际消费链是否正确

### Requirement: Accept the native draw gate only with matching completion and rejection evidence

图像门槛 SHALL 在接受真实非零提交后观察该提交完成, 然后才执行显式冷路径读回及成功判定. 未捕获提交的 Event、未完成/失败的票据、调用返回、CPU 释放或计时结果就绪 SHALL 不代替完成证明. 读回解释 SHALL 保持统一逻辑行序; 本地原生观察 SHALL 不接管绘制、复制、同步或正常帧编排, 也不使公共 staging/map/异步读回成为已支持能力.

同一固定输入 SHALL 在独立进程运行中重复得到满足同一数值预期的结果. 记录 SHALL 绑定实际源码/fixture、Shader 包、构建配置、可执行文件和设备/驱动, 明确区分本次运行、可复用证据和未覆盖范围. 所需环境或检查缺失 SHALL 保持待验收, 不算通过. 代表性非法集成输入 SHALL 在 Release 和 Debug 均由正式 Validation/后端合同拒绝, 不发出该非法操作或改变其目标; 不要求所有 void 拒绝都使整个合法 recording 失效.

开发测试代码、脚本、Shader/JSON 和临时工程 SHALL 仅在按工作包归类的 build 目录, 不进入正式模块或默认构建; 删除本地测试材料 SHALL 不影响正式工程重新配置和构建. 门槛通过 SHALL 只证明该基线原生链及已记录环境, 不扩展为呈现、截图/拾取、Editor、RenderGraph 或其它后端验收.

#### Scenario: Repeat the completed reference without reusing stale executables
- **WHEN** 相同有效输入在独立进程中再次执行 clear/Draw/copy/采样、提交、完成观察及读回
- **THEN** 每次结果均满足同一参考预期且证据对应各自真实构建和可执行文件, 任何未完成、读回失败或输入身份不符都不产生通过结论

#### Scenario: Reject a representative illegal integration request
- **WHEN** 集成消费者请求越过已知索引 Buffer 边界的 Draw, 或同一重叠子资源同时采样和可写附件的组合
- **THEN** 对应非法操作在发出前明确拒绝且目标保持预期, 已发合法前缀仍按既有协议保活; Debug/Release 的拒绝不以断言、GL 错误或成功空操作代替
