# Spec Delta

## Purpose

定义已准备的基线图形对象通过公共 RHI 单即时列表产生真实 OpenGL 绘制与数据结果的合同. 消费者能够依赖明确的状态、访问用途、绘制参数、坐标/深度/颜色及资源寿命边界, 并区分本阶段验证与后续完整提交、图像和呈现验收.

## ADDED Requirements

### Requirement: Execute baseline graphics through the existing immediate recording

受支持的 clear、图形状态、直接 Draw 与 copy SHALL 在现有 owning/current Graphics 即时列表的合法 recording 中执行, 与上传共享唯一打开列表和已有提交协议. 原生绘制 SHALL 在录制调用时发出, Execute SHALL 不重放已经发出的命令. 新 recording SHALL 无上一轮逻辑图形状态; ClearState SHALL 清除逻辑绘制状态并失效相关原生缓存, 同时保留实际资源依赖与已发工作的 owner.

#### Scenario: Mix uploads and graphics without replay
- **WHEN** 合法列表依次执行上传、clear、图形设置、直接 Draw 和 copy, 随后 Close/Execute
- **THEN** 命令在录制区间按序发出, 实际结果只包含一次绘制, Execute 仅为已发工作建立已有合同要求的真实提交与完成跟踪

#### Scenario: Require a fresh valid graphics recording
- **WHEN** 调用者在 Open 前、Close 后, 或在新 Open/ClearState 后尚未建立有效图形状态时 Draw
- **THEN** Debug 和 Release 均拒绝相应工作, 不使用旧 Graphics 状态绘制, 不影响另一列表的 claim 或丢弃已经发出的前缀

### Requirement: Compose executable graphics state from compatible owned objects

图形设置 SHALL 使用同一设备的真实 Pipeline、Framebuffer、绑定集合与 VB/IB. 目标兼容 SHALL 比较完整 FramebufferInfo, 不将附件身份或尺寸作为 Pipeline 身份. 活动绑定 SHALL 符合本次布局/资源类型、所需 block extent 和已知结构化记录粒度; 顶点绑定 SHALL 满足 InputLayout 的格式、slot、stride 和实例频率, 索引绑定 SHALL 满足用途、类型与对齐. 非法候选 SHALL 在该候选的原生应用前拒绝, 不发布部分可绘制状态.

#### Scenario: Draw into another compatible framebuffer
- **WHEN** 同一 Pipeline 分别与两个对象/尺寸不同但格式、sampleCount 和 sampleQuality 相同的 framebuffer 组合
- **THEN** 两次绘制分别使用各自附件和动态区域, 不要求因对象或尺寸不同重建 Pipeline

#### Scenario: Reject a foreign or incompatible executable state
- **WHEN** 候选状态包含另一设备/实现的对象、目标信息不匹配、活动绑定缺失或 range/recordStride 不满足接口
- **THEN** 设置明确失败且不应用该候选的原生状态, 诊断定位相关操作和输入, 不把单独成功创建的对象当作完整可绘制组合

#### Scenario: Preserve vertex input interpretation
- **WHEN** 合法状态使用非零 VB offset、多个合法 slot 或 per-instance 属性, 或仅用 builtin 而不需要顶点属性
- **THEN** 活动输入按公开描述的 location、格式、字节偏移、stride 和实例频率取数, 无属性路径同样可实际绘制

### Requirement: Consume the current pipeline resource mapping for every affected binding

实际图形执行 SHALL 依当前 Pipeline 的活动逻辑资源映射绑定正确的 UBO/只读 SSBO byte ranges 及 Texture/Sampler 组合. 同一纹理配不同 Sampler、跨集合配对与 texture-only fetch SHALL 保持准备时的语义, 不修改 Texture 的采样参数. Pipeline 或组合任一端的集合改变时 SHALL 更新相应映射, 不因 BindingSet 指针不变而沿用另一 Pipeline 的原生位置.

#### Scenario: Observe independent bindings in shader results
- **WHEN** 程序从不同集合的同号 slot 读取常量/结构化数据, 并使用跨集合采样对、同纹理不同 sampler 及 texture-only fetch
- **THEN** 已知输出分别对应其正确资源、byte range、视图和 sampler, 不发生 set/slot 冲突或错误配对

#### Scenario: Change the pipeline while retaining the sets
- **WHEN** 相同集合依次用于具有不同活动映射的两个兼容 Pipeline, 或仅替换一个采样对的一端集合
- **THEN** 后续结果使用新 Pipeline/集合所需的完整映射, 不残留旧物理单元或采样组合

### Requirement: Preserve direct draw counts offsets and builtin semantics

Draw SHALL 保留 vertexCount、instanceCount、startVertexLocation 与 startInstanceLocation. DrawIndexed SHALL 以 vertexCount 表示索引数, 以索引绑定 byte offset 加 startIndexLocation 乘元素大小定位索引, 并将 startVertexLocation 作为原生 baseVertex 一次性应用. R16_UINT/R32_UINT SHALL 按各自宽度取数. Shader InstanceIndex SHALL 包含 base instance, VertexIndex SHALL 已包含 indexed baseVertex; per-instance 属性 SHALL 遵守相应原生取数语义. 非法范围、对齐或 native 参数表示 SHALL 在受影响 Draw 前拒绝; 合法零 count SHALL 不发出原生 Draw 或改变目标内容.

#### Scenario: Draw nonindexed instances with a nonzero first instance
- **WHEN** 非索引绘制使用非零 startVertexLocation/startInstanceLocation, 多实例和具有不同标记的实例输入
- **THEN** 生成的几何、实例属性和 Shader 索引对应指定范围, 不遗漏或重复增加 base instance

#### Scenario: Draw indexed data with byte and element offsets
- **WHEN** 合法索引绘制使用非零索引绑定 offset、first index、base vertex 与 first instance, 并使用两种支持的索引宽度
- **THEN** 已知像素/数据标记对应正确索引区间, VertexIndex 包含一次 baseVertex, 不把 byte offset 当元素索引或重复偏移

#### Scenario: Refuse an invalid draw while keeping a zero draw harmless
- **WHEN** 参数无法表示为实际 native 类型、索引区间越界或输入非法, 或调用者提交合法零 vertex/instance count
- **THEN** 前者有明确拒绝且不发出该 Draw, 后者无绘制副作用且不虚构 GPU 工作

### Requirement: Preserve the adopted raster depth and color conventions

基础绘制 SHALL 保持 UPPER_LEFT/ZERO_TO_ONE, 逻辑纹理 row 0 和公开 viewport/scissor 坐标一致, 只在后端规定边界解释坐标与绕序. 单 viewport/scissor 的有限性、区间及实际 limits SHALL 被核对; 空数组 SHALL 保持公共“本次不设置”语义, 首次实际 Draw 前 SHALL 已具备所需动态区域. 公共普通 raster/depth/stencil/blend/write-mask 及动态 blend constant/stencil reference SHALL 有对应执行效果, 不能忽略已接受的非默认字段. 只读 depth attachment SHALL 不被写入.

Reverse-Z SHALL 由调用方选择的清除值、深度与比较函数共同实现, 不覆盖常规深度 Pass. 颜色处理 SHALL 区分线性数值、sRGB 存储和整型数据, 在规定写入/采样边界编码或解码一次, 并保持规定的线性混合语义. UPPER_LEFT 区域 depth clear 的既有异常 SHALL 得到当前环境定位及符合产品合同的结果证据, 不能以旧 LOWER_LEFT 辅助结果、任意翻转或修改预期代替.

#### Scenario: Render an asymmetric coordinate and depth pattern
- **WHEN** 固定图形包含可区分的上下/左右标记、CCW 与负缩放几何、部分 viewport/scissor、主视图 Reverse-Z 和普通深度对照
- **THEN** 内部采样点、裁剪区域、正反面及深度遮挡满足统一约定, 不因 API 适配重复翻转, 主深度清除/Greater 与常规清除/Less 分别生效

#### Scenario: Preserve color blending and write masks
- **WHEN** 使用已知非饱和线性值、sRGB 与线性目标, 并设置 blend、颜色掩码及 depth/stencil 操作/reference
- **THEN** 输出符合各自数值域与选定操作, 未允许写入的分量/aspect 保持原值, sRGB 不漏编码或重复编码

#### Scenario: Resolve the recorded upper-left clear observation
- **WHEN** 在当前设备复现历史区域 depth clear 组合, 并运行产品完整子资源 clear 后的非对称 scissor/深度绘制
- **THEN** 记录复现与定位结论, 产品在既定 clip/逻辑行序下给出独立预期的正确结果; 未满足时本包保留未完成, 不以临时 LOWER_LEFT guard 宣称通过

### Requirement: Clear and copy exactly the supported resources independently of prior draw state

clear SHALL 覆盖选定合法二维 mip/layer 子资源的完整范围, 区分 float/归一化与整数颜色, 并按 clearDepth/clearStencil 保留未选择 aspect. 清除 SHALL 不被残留 viewport/scissor/write mask 缩小, 不影响未选择的子资源.

普通 Buffer copy SHALL 按 byte intervals 复制, 同 format、单样本二维 texture slice copy SHALL 按 1:1 区域复制并保持编码与逻辑行序. 目标之外内容 SHALL 保留; 越界、同底层存储的非法重叠或未支持形状/格式/sample 组合 SHALL 在受影响操作前明确拒绝, 不通过截断、缩放、类型替换或 Y 翻转接受.

#### Scenario: Clear selected content despite restrictive graphics state
- **WHEN** 先设置限制性 scissor/write mask, 再清除指定颜色或 depth/stencil 子资源
- **THEN** 选中内容完整得到其 typed clear 值, 未选 layer/mip/aspect 保持已知值, 后续 Draw 恢复所需逻辑图形状态

#### Scenario: Copy known bytes and an asymmetric texture region
- **WHEN** 普通 Buffer 非零偏移区间和二维纹理非零 mip/layer 中的非对称区域被复制
- **THEN** 完成后的目标只在请求区间变化, 数据、行序、depth/stencil texel 与编码保持原语义, 不重采样或转换颜色

#### Scenario: Reject an invalid clear or copy request
- **WHEN** 请求具有显式越界子资源/byte interval、非法重叠或执行 profile 外的操作
- **THEN** 操作明确失败且不修改目标, 不借用分配/上传能力伪装对应 clear/copy 已受支持

### Requirement: Satisfy actual access uses independently of native cache hits

Automatic 与显式用途 SHALL 使用同一资源跟踪和原生同步发射权威, 资源状态与 native 状态缓存、数据更新 SHALL 表达不同事实. clear/copy SHALL 先满足自己的 CopyDest/CopySource 用途, Draw SHALL 满足当前实际附件/vertex/index/constant/SRV 用途. 缓存命中或数据更新未改变绑定身份 SHALL 不跳过依赖. 显式模式 SHALL 要求调用方已请求并提交本次用途, BeginTracking SHALL 不代替转换; 非法跟踪 SHALL 阻止有效执行/提交, Close 的初态恢复 SHALL 满足同一合同.

同步 SHALL 基于实际生产者、后继域与重叠访问, 不为当前无 Shader 写入的有序访问无条件发全域 barrier 或等待. 重叠采样与可写 attachment 的非法 feedback SHALL 被拒绝, 不能以宽泛 barrier 使其合法.

#### Scenario: Consume updated data without another graphics-state call
- **WHEN** 在一次 SetGraphicsState 后更新已绑定的普通 Buffer/Texture, 或 clear/copy 后再次 Draw 而不重新设置同一状态
- **THEN** 新数据和正确图形状态被实际使用, clear/copy 用途先于后续 Draw 用途成立, 状态缓存命中不掩盖该顺序

#### Scenario: Use the explicit access path without duplicate inference
- **WHEN** 同一合法序列分别使用 Automatic 和关闭自动推导后的显式用途/commit
- **THEN** 两者得到一致原生结果并各只由一个权威满足依赖, 缺失或错误的显式用途不被静默自动补成成功

#### Scenario: Reject overlapping sampling and writable attachments
- **WHEN** 图形状态使同一底层重叠子资源同时被活动采样并作为可写附件
- **THEN** 受影响绘制在发出前被拒绝, 显式 copy/ping-pong 的合法后继访问不因同一对象身份被误当作该反馈

### Requirement: Retain newly executed graphics dependencies through the existing retirement protocol

即时图形命令 SHALL 在可能发出工作前取得所需引用/账目, 将 Pipeline、Framebuffer、VB/IB、应跟踪的集合与实际资源依赖保留到既有真实完成或合法退出条件. BindingSet 显式关闭 trackLiveness 时, 调用者 SHALL 继续承担其公开的完成前保活责任. 状态清理、调用方 Release 或 recording 结束 SHALL 不等同于 GPU 完成; 内部复合依赖 SHALL 与真正外部 owner 区分, 保留既有关闭预检和有界存储.

CPU 准备失败 SHALL 不执行该次操作; 已发前缀在后续失败、abandon 或未取得有效票据时 SHALL 保留 owner, 不被假取消或重放. 最终 Release SHALL 沿用 owner-context 删除协议, 不在 foreign thread 执行 GL 或为退休申请新存储. 缓存/辅助对象 SHALL 不无界扣留旧资源.

#### Scenario: Release caller handles after issuing graphics
- **WHEN** 合法图形工作发出后调用者释放其 Pipeline、集合、framebuffer、Buffer/Texture 原始句柄并正常提交
- **THEN** 必须由列表保活的依赖持续有效, 实际完成后才能退休, 仅内部复合引用不阻止正常受控关闭

#### Scenario: Abandon or fail after an issued draw
- **WHEN** Draw 已发出而列表在提交前被释放, 或随后发生使录制无效的实际错误
- **THEN** 先前工作继续由原有 owner 保护到真实退休/合法退出, 不发布失败录制的正常成功票据, 不撤回或重放 Draw

#### Scenario: Keep a genuine external graphics owner
- **WHEN** 调用者仍强持有一个图形资源/复合依赖并请求破坏性关闭
- **THEN** 关闭保持可重试拒绝且对象继续有效, 释放该外部 owner 后才按实际内部工作状态推进退出

### Requirement: Deliver a rebuildable limited graphics capability with matching evidence

正式公共 RHI、Shader 包/接口 ABI 和依赖方向 SHALL 保持既有合同. 本包只启用已验证的单样本直接图形执行, 未交付 profile SHALL 明确拒绝. 开发检查代码、Shader/schema、脚本与临时工程 SHALL 只在按工作包归类的 build 目录, 不成为正式源码、默认目标或普通构建前提.

验收 SHALL 使用匹配当前输入的真实 backend/context/device 的像素/已知数据及必要拒绝观察, 明确区分实际运行、复用和未覆盖范围. 本包基础绘制成功 SHALL 不直接完成 T04 的完整在途验收、T05 稳定图像门槛、呈现或 D03 阶段出口.

#### Scenario: Build without development fixtures
- **WHEN** 在没有本地测试材料的新 binary tree 配置/构建正式目标和受影响消费者
- **THEN** 工程可构建且不下载或恢复 fixture, 通用 RHI 不引入原生/业务依赖, 正式默认目标无测试入口

#### Scenario: Report only the observed graphics boundary
- **WHEN** 当前有限图形检查及独立审查已完成并准备最终验收
- **THEN** 记录实际输入、构建/可执行文件、设备/驱动和结果/限制, 不将 Shader link、CPU 校验、过期运行或 T03 的有限结果当作后续阶段的完成证据
