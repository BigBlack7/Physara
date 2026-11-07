# Design

## Context

动机与工作包边界见 [proposal](proposal.md). 2026-11-06 规划基线为 `6bad1b83353b3b7e00533a3ebfdc040a4ddf5424`, 创建 change 前工作区干净.

下表保留规划基线的实现事实与直接消费者, 本轮实现/检查状态以 tasks 为准:

| 当前事实 | 实现入口 | 对本次设计的约束 |
|---|---|---|
| 一个 context 只允许一个 recording claim, 列表借用 Device 物理包 | [CommandList.cpp](../../../../RHI/OpenGL/CommandList.cpp)、[Submission.hpp](../../../../RHI/OpenGL/Submission.hpp) | 扩展同一列表/包, Execute 仍只封装已发工作 |
| 上传只支持 transfer 用途, CommitBarriers 当前消费有序 transfer 的抽象记录 | [CommandListState.cpp](../../../../RHI/OpenGL/CommandListState.cpp) | 增加实际图形消费与 clear/copy 用途, 不能沿用无条件清空记录的解释 |
| ContextState 目前只有 unpack PBO/pixel-store | [ContextState.hpp](../../../../RHI/OpenGL/ContextState.hpp) | 在此扩展唯一原生缓存, ClearState 同时清理逻辑执行状态 |
| Pipeline 已保存 Program、活动绑定计划、固定状态与完整 FramebufferInfo | [Pipeline.hpp](../../../../RHI/OpenGL/Pipeline.hpp)、[Bindings.hpp](../../../../RHI/OpenGL/Bindings.hpp)、[InputLayout.hpp](../../../../RHI/OpenGL/InputLayout.hpp) | 复用已核对的不可变事实, 执行组合时校验本次对象、活动 range/stride 与动态输入 |
| clear/copy/graphics/Draw 尚为 Unsupported | [CommandListUnsupported.cpp](../../../../RHI/OpenGL/CommandListUnsupported.cpp) | 只移出本次交付入口, 其它拒绝继续可见 |
| lease/提交/退出目前按 Buffer/Texture 上传 owner 记账 | [Upload.cpp](../../../../RHI/OpenGL/Upload.cpp)、[Submission.cpp](../../../../RHI/OpenGL/Submission.cpp) | 新绘制根和复合依赖必须接入, 不能直接增加强引用导致关闭永远报 ResourceInUse |

合同依据为 [Architecture 7.2](../../../../Docs/Architecture.md#rhi-objects)、[7.3](../../../../Docs/Architecture.md#opengl)、[7.5](../../../../Docs/Architecture.md#barriers)、[7.6](../../../../Docs/Architecture.md#command-execution)、[坐标/颜色](../../../../Docs/Architecture.md#conventions) 与 [11.1](../../../../Docs/Architecture.md#retirement). 固定上游行为按 [NVRHI 执行分析](../../../../Docs/Reference/NVRHI/ExecutionAndLifetime.md#n8) 及本地 `ThirdParty/nvrhi` 对照, 不将该参考目录编入工程.

现有 [资源上传规范](../../../specs/opengl-resource-uploads/spec.md) 的 CPU 源消费、唯一列表、提交、不重放、完成和退出要求继续适用; 本 change 不另建一套生命周期. [Shader/Pipeline 规范](../../../specs/opengl-shader-pipelines/spec.md) 的准备与 ABI 合同不变. 需要变更的旧能力边界仅写入本 change 的 device-resources delta, 不回改历史归档.

## Goals / Non-Goals

**Goals:** 沿已准备对象执行单样本直接图形命令; 将可复用的逻辑状态、原生事实缓存、实际资源用途与本次资源引用接到现有 owner; 用有限原生观察证明参数、坐标、深度和颜色语义.

**Non-Goals:** 不增加公共接口或第二套命令缓冲/Renderer. 不为未来 Shader 写、动态版本、查询、异步读回、Graph、窗口输出提前建设执行器或测试框架. T04 的完整在途验收和 T05 的阶段图像门槛仍独立; T03 为自身命令补齐必要保活不能作为这两个包已完成的依据.

## Decisions

### 1. 固定当前执行 profile, 不由分配或 link 成功推断支持

本包消费既有 GraphicsBaselineV1, VS/可选 FS、普通常量/只读定长结构化 Buffer、float 二维 SRV 与独立 Sampler; 目标为已支持的单样本、单 mip/单 layer attachment, 可以是二维数组/cube storage 的合法二维选层. Pipeline 兼容信息仍包含颜色/depth 格式、sampleCount 和 sampleQuality, 不加入附件身份/尺寸.

执行支持普通 topology、基础 raster/depth/stencil/blend/write mask、一个 viewport/scissor、Draw/DrawIndexed 中的实例数和偏移. clear 支持该 attachment profile 的浮点/归一化颜色、整型颜色以及 depth/stencil. 必要 copy 为普通 Buffer 的合法字节区间和同 format、单样本二维 texture slice 的 1:1 区域, 包含数组/cube 的选层; depth/stencil 同格式复制保留完整 texel/aspect 数据.

本次不支持的 1D/3D clear/copy、Buffer UInt clear、staging texture copy、格式重解释 copy、MSAA/resolve、多 viewport、Compute/UAV、indirect、volatile/push 等继续明确拒绝. 其中完整传输与 Buffer UInt clear 留在 [D08-T05](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t05), 不为首条图形链扩展其 UAV-storage 前置. 这不撤销已有相应 storage/上传能力. 不扩大 Shader profile、源扩展白名单或包 ABI. 公共 `ClearTextureFloat` 与整数 clear 的适用数值类型分别验证, 不以隐式类型替换接受未交付组合.

选择这个范围是为了闭合 T03 的直接图形执行与必要数据搬运. 一次实现所有公共命令会越过 D08; 只实现一个硬编码三角形则不能满足现有工作包.

### 2. 在现有 recording/packet 上扩展逻辑 Graphics 状态

Open 取得同一唯一 claim 并开始新逻辑状态. SetGraphicsState 在信任边界核对本次 Device/实现归属、Pipeline/Framebuffer 兼容、布局对应、活动资源和 VB/IB/dynamic state. 活动 Buffer range 须覆盖反射所需 extent, StructuredBuffer 的已知 recordStride 须与活动接口相符; 不在每个 Draw 重做 SPIR-V 反射或重复校验已创建对象的不可变描述.

候选状态先完成会失败的 CPU 准备及保活, 再通过 ContextState 应用; 不发布半份可绘制状态. Draw 只消费已经成立的逻辑状态, 检查本次 Draw 参数与当前可变前提. ClearState 清理逻辑 Graphics 状态并使原生缓存未知, 不抹去资源 tracker、已发工作和包引用. Open/Close、foreign owner、重复提交的旧规则直接复用.

CR-001 的消费者修正: 公共 SetGraphicsState 返回 void, Validation 无法据委托判定 GL 已接受候选. GL 路径因此只保留包装的公共录制/输入预检, 不写第二份图形快照; 直接 Draw/DrawIndexed 将生效状态与 IB 检查交给 owning recording. 其他 API 保留原上游快照规则. GL 未交付的 push/间接命令在既有 Profile 边界拒绝, 不再借用未确认的候选状态做后续检查. 不增加公共返回值、原生依赖或能力表.

Close 处理 keepInitialState 的实际依赖, 不能再以无条件 ClearBarriers 代替完成转换. accepted submission 才发布相应 tracker 元数据. 录制错误不撤回已经发出的 GL 前缀; 后续提交拒绝与前缀退休沿既有协议.

备选的第二套 graphics list 会分裂 claim/完成身份和上传数据的顺序, 不采用.

### 3. 唯一 ContextState 缓存 native 事实, 不拥有第二份权威资源状态

在现有 ContextState 中扩展 Program、VAO/顶点格式及动态 VB/IB、读/写 FBO、UBO/SSBO ranges、texture/sampler、viewport/scissor、raster/depth/stencil/blend/write mask、framebuffer-sRGB 和已有 pixel-store. 每项 compare-then-set; Draw 热路径不 glGet* 保存/恢复状态, 需要的 limits/entry points 在 Device 初始化查询并缓存.

本阶段优先用 Device 拥有的有界 VAO/clear 辅助 FBO, 按 InputLayout 的原生格式计划复用 VAO, 用 DSA 更新动态 Buffer/offset/index 绑定. 无属性绘制也使用合法 core VAO. 不建立按每次 Buffer 组合增长的 VAO/FBO 缓存; 原生辅助对象在发布前预留删除存储, 停机由同一 context owner 释放. VAO 与辅助 FBO 不能在任务完成后无界扣留旧资源, 释放/换绑时解除相应 native 引用.

实际命令选择: 本机驱动对 DSA element-buffer 的合法零解绑报错, 故非零 IB 仍使用 DSA, 零解绑统一在已绑定的唯一 VAO 上调用 `glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0)`. 这是同一 owner/缓存下的等价 core 入口, 不增加 vendor 判断、错误豁免或第二份绑定状态; 最终实际关闭结果记录在 tasks.

clear/copy 修改的状态直接写入同一缓存, 下一次 Draw 按仍有效的逻辑 Graphics 状态重建所需 native 值. 实际 native 删除先失效相关缓存/VAO 引用, 防止名称复用; 外部互操作遵守 ClearState 失效入口. 用 glGet 快照一套外部状态再恢复, 或仅以对象指针相同判定所有 native 事实不变, 均不采用.

### 4. 绑定按当前 Pipeline 计划消费, 动态输入保留公开单位

UBO/SSBO 绑定使用集合中预解析的 name/byte range, 不把逻辑 slot 当原生 unit. Texture/Sampler 按 plan 的双方身份组合到同一物理 unit; 任何一端集合变更都更新受影响 pair, texture-only fetch 使用既有 Device dummy. Pipeline 改变必须重新解释 plan, 即使 BindingSet 指针相同也不能跳过映射.

VB slot、属性 offset、stride、整数/归一化格式及实例 divisor 来自 InputLayout 计划; native Buffer offset 叠加本次 VertexBufferBinding.offset. 索引只接受 R16_UINT/R32_UINT, 指针为 `indexBinding.offset + startIndexLocation * indexElementBytes`. 所有乘加/GLsizei/GLint/GLintptr 转换在发出命令前检查, 合法空 Draw 不产生像素或 native Draw.

非索引路径保留 startVertexLocation/startInstanceLocation; indexed 路径使用现有 vertexCount 表示索引数, startVertexLocation 只作为原生 baseVertex 加一次, InstanceIndex 沿既有 lowering 包含 baseInstance. 不读取 GPU index 数据来全量计算顶点界限, 不用 CPU 调整顶点/索引数据或逐 Draw uniform 模拟这些 builtin. 实例属性取数与 Shader 实例索引分别由原生 base instance 语义负责.

### 5. 坐标只在后端合同边界解释, 已知异常列为前置定位门槛

原生 clip 保持 UPPER_LEFT/ZERO_TO_ONE. 按 [ARB_clip_control](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_clip_control.txt) 的坐标变换和 issues 4/12, clip origin 的变化不等于 glViewport/glScissor/读回参数的窗口原点变化, 正反面计算也有对应规则. 本项目离屏逻辑纹理的 row 0 与 texel y=0 对应; viewport/scissor 的映射从这一约定推导, 不能再套用默认窗口的高度翻转. 绕序适配不得同时在 Shader、数据和 glFrontFace 重复补偿.

首项实现工作用已有 [决策 12 的确切观测](../2026-10-29-establish-opengl-resource-conformance/design.md) 做小复现: UPPER_LEFT 下的具名 FBO 区域 depth clear、scissor 和独立读回. 固定带标记的 texture、区域和逻辑行预期, 分开定位 clip/raster、clear 与读回解释. 先记录当前驱动是否仍复现及根因, 再选择符合上述合同的命令路径; 不直接从失败输出反算预期, 不默默沿用旧 fixture 的 LOWER_LEFT guard.

公共 ClearDepthStencilTexture 没有矩形参数, 必须清除选定完整子资源, 不受之前 scissor 限制; 区域 clear 只用于 build 内重现/诊断, 不新增公共 API. 产品可使用合规的 typed clear/辅助 attachment 路径避开有缺陷的原生组合, 但最终 Draw/scissor/深度结果仍须在产品 clip 合同下验证. 若仅靠改变架构合同或按驱动任意翻转才能通过, 本包保留未完成并形成具体纠错提案, 不放宽像素预期或把问题移交 T05 以退出 T03.

1.1 当前观测: Intel 32.0.101.6129 的 UPPER_LEFT named/bound FBO 区域 depth clear 仍翻到另一行, TextureSubImage 与 ReadPixels 相互一致; 同一输入的 texel-region clear 不翻行, 禁用 scissor 的完整 FBO clear 在两种 origin 下均正确. 差异因此定位在该驱动的 FBO 区域 clear 路径, 不是 CPU 行序 oracle. 本包选择通过唯一 ContextState 禁用 scissor/建立完整 mask 后执行完整子资源 typed clear, 保持 clip control 不变; 不引入驱动特判或产品区域 clear. 产品 Draw/scissor 的后续 A 本地像素检查通过, 详见 [实际证据](tasks.md#native-evidence); 独立 B 复核和 A [最终 verify](tasks.md#final-verification) 均已通过.

公共 viewport/scissor 数组为空沿用其“本次不设置”含义, 不重置为默认窗口大小. 本 recording 首次 Draw 前须已有明确有效 viewport; 启用 scissor 时须有有效区域. 深度范围、有限性、宽高及适用 native limits 在设置边界检查. 多 viewport 明确 Unsupported.

### 6. 固定状态完整映射到普通单样本 profile

raster 的 fill/cull/front-face、depthClip 与 bias, depth 测试/写入/比较, 双面 stencil 与静态/动态 reference, 各有效颜色目标的 blend 方程/因子/常量与 write mask 均按公开状态映射. 未支持的扩展状态在发出相应工作前拒绝, 不忽略非默认值. 只读 depth attachment 与可能修改其内容的 Pipeline 组合不得执行写入. 验证每类可触发的独立风险, 不为每个枚举写一套用例.

CR-002/CR-003 的边界修正: 基线没有 index=1 的第二颜色输出, 所以准备阶段拒绝实际消费 Src1* 的通路, 不扩展 dual-source ABI. 关闭 blend、整数附件、Min/Max 或未写分量的因子仍可保留为合法 inactive 字段, 原生应用时统一使用 One/Zero, 避免未使用的 Src1* 触发 GL 双源限制. 准备与应用共用无状态的 BlendFactorUse, 固定状态缓存同时比较实际颜色格式, 保证 R32→RGBA 等切换后恢复需要的 alpha 因子. 只读深度冲突按 depthTestEnable && depthWriteEnable 判断, stencil 写入条件保持; depth test 关闭时保留默认 write flag 不产生深度写入. 依据为 [GL 4.6 Core 17.3.4/17.3.6](https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf#page=526), 不把无效配置发到 Draw 后才让设备 Faulted.

Reverse-Z 由具体清除值、Shader 深度和比较函数组成, 主视图样本用 clear=0 与 Greater/GreaterEqual; 常规深度样本仍可使用 clear=1/Less. 后端不把所有 depth 操作硬改为 Reverse-Z.

framebuffer-sRGB 按实际有效附件格式建立. float 颜色清除与 Shader 输出采用相应的线性域语义, sRGB 目标编码一次、采样解码一次, blend 在规定线性域进行; integer/data 目标不作 gamma 转换. 通过已知中间值检查编码, 不能只用 0/1 饱和值证明颜色正确.

实现细化: canonical Vulkan 的 flat 默认来自首 provoking vertex, ContextState 建立 `GL_FIRST_VERTEX_CONVENTION`, 依据 [Vulkan flat shading](https://docs.vulkan.org/spec/latest/chapters/vertexpostproc.html#primsrast-flatshading). 活动 PointSize 在现有 Shader builtin 解析中记录, 执行时决定是否启用 `GL_PROGRAM_POINT_SIZE`; 未写入时采用单位点大小, 不改变 IR/包 ABI 或 profile. native 普通状态失效后同样恢复单位线宽等本 profile 的固定前提.

### 7. clear/copy 有自己的用途和状态, 后续 Draw 再建立实际消费

浮点/整数颜色 clear 按目标数值类型使用正确 typed 路径; depth-only/stencil-only 保留另一 aspect. 选定 mip/layer 集合先完整验证, 再逐子资源操作, 不把显式越界截成合法区域. 所有 clear 忽略残留 viewport/scissor/write mask 对清除范围的影响, 同时把实际改动反映到 ContextState.

Buffer copy 使用 checked byte intervals, Texture copy 使用已解析的同格式二维 slice/区域; 保留其它字节、子资源及编码. 同一底层 storage 上的重叠源/目标拒绝, 不能只比较可能不同的 view 名称. Copy 不执行缩放、颜色变换或 Y 翻转, 不绕到 Renderer 原生 GL.

clear 建立 CopyDest, copy 建立 CopySource/CopyDest, Draw 才建立附件/vertex/index/constant/SRV 等实际用途. Automatic 和显式模式走同一 Common tracker/backend 入口. 在缓存命中、SetGraphicsState 后又 Write/Clear/Copy、以及没有再次 SetGraphicsState 的 Draw 上, 仍按实际消费检查用途; 普通更新不会凭空改变绑定 range, 数据更新事实不与 native cache dirty 混用.

当前无 image/SSBO Shader 写生产者, 有序 transfer/attachment 与只读使用不需要假设性 GL_ALL_BARRIER_BITS/glFinish. Commit 只发当前实际访问域所需的 native 依赖, 无需新增 UAV epoch/多队列系统; 未交付的 Shader 写入口继续拒绝. 可写 attachment 与重叠采样的 feedback 先拒绝, 用显式 copy/ping-pong; 不用 barrier 掩盖非法算法. 显式模式必须已经请求并提交本次用途, BeginTracking 只是前态声明.

### 8. 新执行引用接入现有 packet, 不把 T04 验收提前勾选

在首个可能使用对象的 native 调用前, 同一包准备必要 Pipeline、Framebuffer、VB/IB、BindingSet 与实际访问资源的引用/账目, 随已建立的 fence 完成退休. BindingSet.trackLiveness=false 仍由调用者承担集合及其依赖到完成前的寿命, 不偷偷改成另一种强持有语义; 其它必须由包保活的对象不因此免除责任. 按状态变化/实际引用去重, 不每 Draw 递归重保活完整依赖树.

现有内部 lease 不能只加一个 RefCountPtr 就算完成: Pipeline→Shader/InputLayout/布局、集合→布局/资源、Framebuffer→附件等复合所有权要计入实际内部依赖与 Device 引用. 同一对象跨包计数准确, 真正外部 owner 仍阻止破坏性关闭; 仅内部工作可经正常 drain 退休. 将这些事实接到现有 Device/packet owner 记账, 不用全局地址注册表或固定帧延迟.

实现选择为在状态准备时将实际强引用闭包去重为包 lease, 每个 wrapper 区分 packetHolds 与 dependencyHolds. 复合父对象首个包引用才计入其实际子边, 最后一个包引用退出才撤销; 跨包不会重复计入同一父对象的强引用. 每个 wrapper 的 Device 根仍只计一次, 关闭预检同时比较真实 RefCount, 不把外部 Shader 子 owner 混入内部边. 新状态准备可分配 CPU 记录, Draw 热路径不再递归保活; 每包沿用 4096 不同资源预算.

发出前的 CPU 准备失败不执行该操作, 保留此前前缀. 已发 native 错误沿原有 callback/stop 语义报告, 缓存不得继续声称完整已知, 不能立即释放可能在使用的对象. orphan/关闭未提交包按既有完成或退出路径保留引用. 普通可传播分配失败遵守既有边界; 不新增穷举分配点注入、通用恢复框架或对 STL noexcept 致命 OOM 的同进程恢复保证.

T03 仅用一个代表性 release/abandon/关闭序列证明新增引用没有破坏既有 owner 协议. T04 继续覆盖完整 Draw 在途、查询、后续上传版本、fence/poll 失败和退出矩阵; 这些未改变部分不为本包重跑.

### 9. 实现按现有职责拆分, 文档与构建不引入测试消费者

Graphics 状态/Draw、clear/copy 和用途 lowering 按现有 OpenGL 目录拆成必要正式实现文件, CommandList.cpp 保留生命周期, ContextState 保留唯一缓存, Submission 保留完成/退休. 不新增空接口层或通用命令框架. CMake 只列实际正式文件并保留依赖方向; 无新增第三方与 configure 下载.

公共头只按本轮能力更新必要说明, 不改签名/布局/枚举. RHI 使用说明和 Build 仅更新确实改变的消费边界, 不宣称 Runtime 已有 Renderer/画面. 中文注释/英文标点、紧凑 Doxygen 及必要上游命名例外说明遵守项目技能; 独立 B 通读本轮涉及的完整第一方文件并完善注释.

开发 fixture、Shader/schema、脚本、临时工程只在 `build/d03-t03/`. 当前 build 中未发现前包的本地 fixture, 不假定它们永久存在或自动恢复; 按实际风险创建最少的新材料, 复用正式编译/RHI 链. 正式工程对这些材料零依赖.

### 10. 验证按不同缺陷合并, 达到证据门槛即停止

验证分组、风险和停止条件在 [tasks 的验证预算](tasks.md#verification-budget) 统一管理. 主要原生结果运行 Release, Debug 编译并复用同一 fixture 做必要正向 smoke; 不因同一算法有两种配置就重复整套 GPU 矩阵. Release 的公开非法操作必须有实际拒绝证据. 新的配置差异、失败或缺口才扩测.

像素/Buffer oracle 来自固定输入和独立计算, 在轮廓内部取有判别力的点并预设格式容差, 不用被测转换函数生成预期. 最终读回可在 build 内显式等待已接受的真实工作, 只证明冷路径结果; 不为此引入产品全设备等待或假异步读回. 即时性用录制/Execute 区间的 native 观察配合非幂等像素结果区分, 不能仅凭一个 Draw 计数就宣称图形正确.

## Risks / Trade-offs

- [UPPER_LEFT 区域 depth clear 的旧观测可能仍在当前驱动复现] → 首先运行精确小复现, 分离行序 oracle 与 native 操作, 保留原始失败; 按决策 5 达到产品合同后才结束, 不预先保证驱动已修复.
- [缓存命中掩盖 plan 变化或操作后的实际用途] → Pipeline remap、clear/write 后不重设 GraphicsState 以及 Automatic/显式模式在同一有限序列里验证.
- [复合依赖保活造成提前删除或关闭死锁] → 扩充现有内部账目并在新增 Draw 的真实完成/abandon 序列检查; 不另建完成队列.
- [实现基本状态时忽略已接受的非默认字段] → 对照当前 Pipeline/GraphicsState 的全部当前 profile 字段, 未支持值明确拒绝并记录, 禁止静默回退.
- [将准备证据、native 调用计数或过期可执行文件当作当前像素结果] → 记录源/fixture/构建/可执行文件/设备身份及实际原生输出, 注明复用和未覆盖项.
- [测试扩张拖慢实现] → 每个独立风险只选最小可判别输入, 共享 fixture/结果; 不重跑未变的历史矩阵或构造失败点扫描.

## Migration Plan

1. 在现有源码与工作包上实现本 delta, 先通过坐标定位门槛, 再接通状态/用途/Draw/clear/copy 与必要引用. 保留不支持入口, 不改历史归档或已采纳主规范.
2. 完成 A 本地检查后更新当前问题单并交维护者调度 B; A 整改与 B 复核各自勾选, 同时验收代码与注释.
3. B 接受最终实际版本后执行原生 verify, 将 CR、最终输入与实际运行/复用/未覆盖证据保存在本 change tasks, 再同步两份 delta、归档并更新开发计划; 归档成功才按模板重置问题单.
4. 运行失败沿既有 device stop/drain 处理, 不假装回滚已发 GL. 若需撤销实现, 按本 change 的实际文件 diff 恢复到此前明确拒绝相关命令的状态并更新能力说明; 不留下可调用的半实现入口. 不涉及部署、数据格式迁移或历史改写.
