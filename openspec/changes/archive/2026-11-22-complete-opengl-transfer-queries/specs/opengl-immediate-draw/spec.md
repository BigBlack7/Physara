# Spec Delta

## MODIFIED Requirements

### Requirement: Clear and copy exactly the supported resources independently of prior draw state

clear SHALL 覆盖选定合法二维 mip/layer 子资源的完整范围, 区分 float/归一化与整数颜色, 并按 clearDepth/clearStencil 保留未选择 aspect. 清除 SHALL 不被残留 viewport/scissor/write mask 缩小, 不影响未选择的子资源.

普通 Buffer copy SHALL 按 byte intervals 复制, 同 format、单样本二维 texture slice copy SHALL 按 1:1 区域复制并保持编码与逻辑行序. 目标之外内容 SHALL 保留; 越界、同底层存储的非法重叠或未支持形状/格式/sample 组合 SHALL 在受影响操作前明确拒绝, 不通过截断、缩放、类型替换或 Y 翻转接受.

合法的四字节对齐普通 UAV Buffer SHALL 可按公开 UInt clear 值清除全范围。已交付颜色 resolve 的多样本源 SHALL 可使用受支持的 typed 颜色 clear 定义选定的完整子资源。选中范围的普通 copy 和专用 staging copy SHALL 各按其格式与实际数据合同执行。

#### Scenario: Clear selected content despite restrictive graphics state
- **WHEN** 先设置限制性 scissor/write mask, 再清除指定颜色或 depth/stencil 子资源
- **THEN** 选中内容完整得到其 typed clear 值, 未选 layer/mip/aspect 保持已知值, 后续 Draw 恢复所需逻辑图形状态

#### Scenario: Copy known bytes and an asymmetric texture region
- **WHEN** 普通 Buffer 非零偏移区间和二维纹理非零 mip/layer 中的非对称区域被复制
- **THEN** 完成后的目标只在请求区间变化, 数据、行序、depth/stencil texel 与编码保持原语义, 不重采样或转换颜色

#### Scenario: Reject an invalid clear or copy request
- **WHEN** 请求具有显式越界子资源/byte interval、非法重叠或执行 profile 外的操作
- **THEN** 操作明确失败且不修改目标, 不借用分配/上传能力伪装对应 clear/copy 已受支持

#### Scenario: Read a typed buffer clear through the ordinary copy path
- **WHEN** 已有其它写入或绑定的对齐普通 UAV Buffer 按非零 UInt 值完整清除，再按非零偏移复制到合法 Read staging
- **THEN** 完成的所选字节按重复四字节值解释，前次绑定及数据不改变本次清除范围，非法尺寸或资源角色的对照未发出 clear

### Requirement: Accept the native draw gate only with matching completion and rejection evidence

图像门槛 SHALL 在接受真实非零提交并确认相关读回工作完成后才读取 CPU 数据及判定成功. 公开 staging 可用时, 正式链路 SHALL 按 opengl-immediate-transfer 在实际复制所属的接受提交后捕获 Event 并读取对应已完成的 staging. 未捕获提交的 Event、未完成/失败的票据、调用返回、CPU 释放或计时结果就绪 SHALL 不代替完成证明. 读回解释 SHALL 保持统一逻辑行序; 保留的辅助原生观测 SHALL 不接管绘制、复制、同步或正常帧编排, 也不为其未走过的 staging 路径提供通过证明.

同一固定输入 SHALL 在独立进程运行中重复得到满足同一数值预期的结果. 记录 SHALL 绑定实际源码/fixture、Shader 包、构建配置、可执行文件和设备/驱动, 明确区分本次运行、可复用证据和未覆盖范围. 所需环境或检查缺失 SHALL 保持待验收, 不算通过. 代表性非法集成输入 SHALL 在 Release 和 Debug 均由正式 Validation/后端合同拒绝, 不发出该非法操作或改变其目标; 不要求所有 void 拒绝都使整个合法 recording 失效.

开发测试代码、脚本、Shader/JSON 和临时工程 SHALL 仅在按工作包归类的 build 目录, 不进入正式模块或默认构建; 删除本地测试材料 SHALL 不影响正式工程重新配置和构建. 门槛通过 SHALL 只证明该基线原生链及已记录环境, 不扩展为呈现、截图/拾取、Editor、RenderGraph 或其它后端验收.

#### Scenario: Repeat the completed reference without reusing stale executables
- **WHEN** 相同有效输入在独立进程中再次执行 clear/Draw/copy/采样、提交、完成观察及读回
- **THEN** 每次结果均满足同一参考预期且证据对应各自真实构建和可执行文件, 任何未完成、读回失败或输入身份不符都不产生通过结论

#### Scenario: Reject a representative illegal integration request
- **WHEN** 集成消费者请求越过已知索引 Buffer 边界的 Draw, 或同一重叠子资源同时采样和可写附件的组合
- **THEN** 对应非法操作在发出前明确拒绝且目标保持预期, 已发合法前缀仍按既有协议保活; Debug/Release 的拒绝不以断言、GL 错误或成功空操作代替
