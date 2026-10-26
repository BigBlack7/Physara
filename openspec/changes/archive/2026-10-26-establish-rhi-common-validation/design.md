# Design

## Context

动机、工作包和外部影响见 [proposal](proposal.md), 可观察合同见 [spec](specs/rhi-common-validation/spec.md). 设计起点为五个公共头和 PhysaraRHI INTERFACE target, 当时 Common 外部定义尚未交付. 当前 Common/Validation 交付归属见 [来源说明](../../../../RHI/README.md), 实际检查见 [tasks](tasks.md). 当前没有 RHI backend Device 或生产消费者, Runtime 仅消费既有 Core/Platform. 以下记录选定方案和实施中已核实的适配, 不以设计文本代替运行证据.

已核对 [Architecture 7.1](../../../../Docs/Architecture.md#rhi)、[7.2](../../../../Docs/Architecture.md#rhi-objects)、[7.5](../../../../Docs/Architecture.md#barriers)、[7.7](../../../../Docs/Architecture.md#capabilities)、[11.1](../../../../Docs/Architecture.md#retirement), D02-T02/T03/T05 和三个当前主规格. 参考分析的 [状态跟踪](../../../../Docs/Reference/NVRHI_RHI_Vulkan_Analysis.md#n7) 用于理解来源, 不覆盖项目架构或实际源码.

源码中已观察到的实施风险:

- Common 的 subresource resolve 使用 base+count, BitSetAllocator 将 size_t 容量直接转 int 且循环索引使用 int 加法.
- Validation 工厂直接解引用 device 和 callback; 外部绑定类型的 checked_cast 只在 Debug 动态检查, Release 退化为 static_cast.
- WriteBuffer/heap 绑定存在 offset+size 检查, CopyBuffer 等路径只检查 open 后直接转发.
- RequireExecuteState 在批次检查过程中修改列表状态; 后面的元素失败会消耗前面有效元素的 CLOSED 状态, 也未强制同一包装设备归属.
- tracker 的未知前态报告后仍继续形成记录; 永久状态在提交回调中逐项发布, 没有独立的错误/放弃录制出口.
- GL 不在 API 字符串 switch 中; Validation 的 API 分支和 QueryFeatureSupport 不能直接作为 GL 初始策略使用.

## Goals / Non-Goals

**Goals:** 提供真实可链接的通用实现和可组合 Validation, 在完整源谱系内解决上述确定风险; 将校验层调用协议、抽象依赖、底层返回和实际 GPU 完成分开; 用无 native 的消费者和 CPU fixture 验证本层可证明的事实.

**Non-Goals:** 不创建任何真实或生产假 Device, 不读取本机 GL limits 充当新 backend caps, 不发出原生命令/barrier 或创建 fence. 不实现 FrameGraph、上传池、提交引用/删除队列、Shader 编译器、DXGI/Aftermath SDK、Vulkan 或 RT 后端. 不借此改变既有公共字段、枚举值、intrusive 引用、绑定键或 HeaderVersion=27.

## Decisions

### 1. 先落实已获同意的阶段边界

维护者在本轮明确同意: 保留 D01 跨屏缺口, 允许先完成 D02-T02 通用实现和 CPU 验证; 真实 GL Device 包装、设备 caps 和原生行为验收归 T03/T05, D01/D02 总出口仍未完成. 这是本 change 的新限定决定, 不改写 [旧决定](../../establish-window-input-runtime/design.md#closeout) 或将 T01 例外当作全局许可.

实施前先更新 Developments: D02 例外明确列 T01/T02; T02 完成条件改为本通用层构建、链接、校验/profile 和 CPU 合同通过; T03 增加实际 GL Device 上的 Validation 集成和真实 limits/format/资源归属验证; T05 保持真实读回/生命周期/caps 一致性. T03 仍受 D01 阶段入口约束, 本次没有授权提前实施 native 工作. 从而 T02 能独立验收, T03 消费完成的 T02, 原生证据有明确归属而不是被删除.

同一首项任务在 Architecture 7.1/7.2/7.5/7.7 登记本设计的有限适配和阶段边界, 引用本 change; 不复制第二套总架构. 规划阶段只修改本 change, 当前权威文档的采纳更新在任何生产实现之前完成.

**备选:** 等待双屏再开始所有通用实现会阻塞与 DPI 无关的合同检查; 在 T02 创建最小假 GL Device 会掩盖真实能力并扩张范围. 二者均不采用.

### 2. 固定十个实际输入, 不把全部 Common 目录当构建清单

来源仍为 NVRHI commit `6b96fb03e07539f08327aea76c56d55f1de9d906`. 下列 SHA256 已在本轮读取计算: 严格 UTF-8 解码, 仅 CRLF 转 LF, 再编码计算; 不剥 BOM、空白或注释. 路径相对 ThirdParty/nvrhi.

| 来源 | 目标 | SHA256 (LF) |
|---|---|---|
| include/nvrhi/common/misc.h | RHI/Common/Misc.hpp | `15dc45aac6328db04ddcb8b1ed863a5441414192b6c81006dad6fcd91fdf4a49` |
| src/common/misc.cpp | RHI/Common/Misc.cpp | `b89291c78a31c13c61e02bb870b1dbcf19e333f717786c0ea959a2ea19924c99` |
| src/common/format-info.cpp | RHI/Common/FormatInfo.cpp | `e5a4240cf19c050c7ac3088ab4486fc4089e64634b6e9f6581cfe02ddb346908` |
| src/common/utils.cpp | RHI/Common/Utils.cpp | `92e0c334a6bc51b179f92e0c9cb2c701e8aae9c9095e54726ea8cc562eb1cb04` |
| src/common/state-tracking.h | RHI/Common/StateTracking.hpp | `01400f8bc5cbc5b0d52b546e45a35ed66c79d8a4c98cc8b17e6af05c9d325fff` |
| src/common/state-tracking.cpp | RHI/Common/StateTracking.cpp | `db3af0fd2e64f342908d04c7f1d10c8bbba003b209cff8ea3892a2f229d80759` |
| include/nvrhi/validation.h | RHI/Validation.hpp | `b224d5d4484554207261b864dcd0974845f104fffc1881952dd12a21ec16ebdc` |
| src/validation/validation-backend.h | RHI/Validation/ValidationBackend.hpp | `6c935cc88c44518734b06ceebdc9aea9941726308fc7592f9e3205d3a2b7efb9` |
| src/validation/validation-device.cpp | RHI/Validation/ValidationDevice.cpp | `b2fc3a065ffbae6f5b6401ba17c4aa602224fd2be6f8d22521cef68a10581264` |
| src/validation/validation-commandlist.cpp | RHI/Validation/ValidationCommandList.cpp | `3a07c1a0e9dc08725f2ad750ed99cddb6e235334a7af7f7d23dd4762278bd5e1` |

已有五头继续使用其固定源/当前目标和已登记差异. 每个移植文件保留完整 MIT 通知, RHI/README 扩充同一张当前来源/差异清单. 输入不匹配先查明差异, 不更新预期来接受漂移. `planning-inputs.json` 仅是 build/artifacts 下的本机规划输出, 上表是本 change 的来源记录.

排除 dxgi-format.*、aftermath.* (真实原生/可选 SDK 依赖), versioning.h、resourcebindingmap.h (本十项的 include 链不消费, 随实际 backend/提交消费者再移植). 完整 Validation 的扩展声明和通用验证代码仍保留, 不因此引入其后端实现或裁剪公共接口. Aftermath 的引用 getter 仅委托/拒绝, 不构造辅助对象.

**备选:** include/链接参考快照或照搬上游全目录会违反项目依赖边界; 仅复制几个 Common 函数又会留下公开已声明的未解释缺口. 采用完整实际通用依赖闭包.

### 3. 统一符号映射, 保持一个正式静态消费目标

复用当前 RHI/README 的命名/标准协议规则, 扩展实际声明清单, 不沿用仅靠动词前缀的旧脚本假设. 所有公开及私有普通函数按声明/调用位置映射为 PascalCase, 包括 Draw/Dispatch/ResolveTexture/CommitBarriers; 所有 override 逐项对照现有 RHI.hpp. std 操作/运算符、descriptor 字段和上游协议枚举保留. 按维护者归档前的明确修订, 所有项目自有命名空间段统一 Physara 风格: utils→Utils、rt→RT、rt::cluster→RT::Cluster、coopvec→CoopVec、validation→Validation、input/profile→Input/Profile. std 等外部协议域不改名, 不增加旧拼写 alias.

同次审核更新 AGENTS 与 Architecture/README, 取消第一方 cpp 强制匿名域的旧规则. RHI 的 cpp 专用 helper/表、Core 的文本/诊断辅助、Platform 的输入/窗口/GL 辅助和 Runtime 的启动辅助放在已存在的实际模块域内, 自由函数及可变/需明确内部链接的对象用 static 保持 TU 私有. Runtime 辅助使用实际模块域 Physara::Runtime, main 仍为全局语言入口且明确限定这些引用. cpp 专用类型保留清楚且不冲突的职责名称; 不以新建空泛 Detail/Internal 域取代匿名域. 既有 Platform/Core 的 Detail 私有头与跨 cpp 合同继续保留, 不改变拥有者、初始顺序或析构顺序. 第三方与已有归档不做风格重写.

归档审查前保存第一方源码快照、hash 和符号清单; 修改仅限 namespace 声明/引用、匿名域到已有模块域、static 及必要 main 限定. 不顺带重写函数体. 全量 token、声明/默认值/布局和独立域检查共同证明映射, 旧域/匿名域负变异必须拒绝. Debug/Release 重跑全部 RHI 消费者和受影响 Core/Platform/Runtime 的实际宿主/故障回归; 编译验证 static callback 地址和跨文件符号, 不以单个 happy path 代替.

具体补充: checked_cast→CheckedCast、hash_to_u32→HashToUInt32、align→Align、arraysAreDifferent→ArraysAreDifferent、arrayDifferenceMask→ArrayDifferenceMask、unwrapResource→UnwrapResource、calcSubresource→CalcSubresource. 私有 c_FormatInfo→FormatInfoTable, 避免去前缀后遮蔽 FormatInfo 类型; m_type→m_QueueType, 内部 CommandListState 的 INITIAL/OPEN/CLOSED→Initial/Open/Closed. 其余映射按实际符号表登记, 不在注释/普通字符串盲替换; 诊断中的操作名更新到实际 API. 不保留旧名字兼容 alias.

PhysaraRHI 改为 STATIC, 六份 cpp 为 PRIVATE sources, 私有三头不作为稳定公共入口; 新 Validation.hpp 和原五头为公共头. PUBLIC 只给模块 include 与 cxx_std_20, CXX_EXTENSIONS=OFF, PRIVATE /W4 /WX /utf-8 /permissive-, 不标 SYSTEM. 静态构建不设置共享库导出宏, 不添加 DLL 配置矩阵或任意全局选项. 每次 sources 只列已落地文件, 不用缺失 cpp 或空实现占位. Runtime 保持现有链接.

公共 Validation.hpp 自包含并声明 `Physara::RHI::Validation::CreateValidationLayer(Device*)`; 输入借用, 成功结果拥有其引用. Common/Misc.hpp、StateTracking.hpp、ValidationBackend.hpp 为本模块实现/fixture 的私有合同, 仅依赖模块和直接标准头. 两个大型 Validation cpp 保留按 Device/CommandList 职责的机械移植边界 (来源约 2444/2017 行), 此处完成规模评审; 不为行数拆成难以对照的多套状态.

**备选:** 新建平行 RHICommon/RHIValidation 消费入口会扩大链接/配置面; 把后端加进通用库或让 Runtime 先链接未用库没有当前消费者. 保持一个真实目标.

### 4. Common 的有效输入语义保留, 算术按范围含义修正

Misc/FormatInfo/Utils 三个 cpp 先一起形成可链接通用组, 避免 Misc 的内部 Utils 引用在只编译一份源时成为链接残缺. 实现 VerifyHeaderVersion、所有 Resolve/IsEntireTexture、BlendState、FramebufferInfo/Ex、CommandList 的 framebuffer state 辅助、CoopVec 纯大小/stride、格式表及 Utils/allocator 的全部既有声明; 不只筛导出宏. 格式表逐行对应, invalid-format 查询仍保留来源 UNKNOWN 信息 fallback, 该 fallback 从不代表设备支持.

范围算法:

- BufferRange::Resolve 保留原有 clamp 和 byteSize=0 表示剩余区间; 先 `offset=min(requestedOffset,capacity)`, 再以 `remaining=capacity-offset` 求大小, 不增加 offset+size.
- TextureSubresourceSet::Resolve/IsEntireTexture 改用 base 与边界比较、剩余计数减法和 min, 不计算可能溢出的 base+count. 超过边界的 base 得到零长度交集; UINT32_MAX/整个范围的已有 sentinel 和 oversized-count clamp 保留. 非数组纹理仍是来源的一层规则.
- TextureSlice::Resolve、FramebufferInfoEx 的 mip shift 只接收已满足 desc/mip 前提的输入; Validation 在进入前证明 mip<有效 mip 数且<整数位宽, 不把非法 shift 的结果改成猜测尺寸. 裸 helper 头内写清其合法调用前提.
- GetOptimalMatrixStride 等字节乘法在已知大小/有效类型前提下检查 size_t 表示能力; 支持平台为 x64, 不宣称 32 位可移植布局. 无有效结果的来源 layout 返回 0 保留; 输入维度和 matrix buffer 校验在 Validation.
- 私有 Align 明确非零二次幂和加法可表示前提; array mask 的移位使用 uint32_t{1}, 合法数组容量<=32. 这些是内部算法前提, 不能承担外部输入 Release 验证.

BitSetAllocator 容量不可变. 构造先拒绝 capacity>INT_MAX, 抛标准 length_error (和标准容器分配失败一样, 构造不返回半初始化对象). 零容量 Allocate 直接 -1. 循环用 size_t 做模运算并在证明范围后转 int, 避免 m_NextAvailable+i 的 int 溢出; 满时不扩容, Release 合法索引恢复槽位, 原有非法索引忽略语义保留. multithreaded=true 时用 RAII 条件锁覆盖同一可变数据, 不在锁中调用 callback 或 GPU.

**备选:** 用额外超大整数/新 Range 类型重写公开合同没有必要; 全保留加法与窄化会违反 Release 正确性. 只修正能直接证明的计算, 不改有效资源含义.

### 5. 包装所有权、错误和异常在边界表达

工厂先检查非空 Device、有效 GraphicsAPI 和非空 MessageCallback, 再分配 DeviceWrapper; 缺依赖返回空 handle, 无 callback 时不为输出一条消息再解引用它. DeviceWrapper 拥有 DeviceHandle, CommandListWrapper 拥有底层列表和所属 DeviceWrapper 的引用, 不引入全局 registry 或 shared_ptr. 回调为应用借用, 其寿命须覆盖所有包装对象, 不抛出也不重入同一正在校验的可变对象.

CommandListWrapper 增加完整析构协议: 若它仍拥有 open immediate 的计数 claim, 恰好释放一次; 在 DeviceWrapper 仍存活时先 Reset 底层列表. 不在析构补 Close/Execute/等待 GPU. 底层资源/回调须满足其原生 owner 的生命周期合同; 此处引用不等于 GPU 退休. 设备操作服从被包装设备的线程合同; 单个可变列表/tracker 仅一个写线程, immediate-open 全设备计数沿用原子机制, 不增加全局锁或迁移 context.

沿用空 handle/bool/0/MessageCallback. Error/Warning 保持一次报告, 在最有上下文的层添加操作及 InvalidArgument/InvalidState/Unsupported 原因; 有资源身份时包含 debugName, 并用私有 source_location 默认参数带出源位置. 不把每次 capability false 查询记 Error, 不逐帧刷日志. 无效请求不调用底层, 有效请求的底层失败原样返回.

异常合同: 普通 CPU/string/container 工具允许标准分配/容量异常; BitSetAllocator 的 length_error 写入公共合同注释. 校验包装工厂与提交预检的自身分配失败捕获 bad_alloc, 用不再分配的静态诊断并返回空/0; 不用 catch-all 声称未知后端异常已安全回滚. callback 的不抛出前提和底层失败合同明确. GL 的 IsAftermathEnabled=false; 没有失败返回值的 GetAftermathCrashDumpHelper 在报告 Unsupported 后抛 std::logic_error, 公开头注释记录此引用 getter 的前提/异常, 不返回伪对象或触发 Release UB. 其他有失败形式的接口沿原有形式, 不增加第二套错误框架.

Common 的 NotImplemented/NotSupported/InvalidEnum 仍是来源诊断辅助定义, 不是操作失败结果. 所有实际拒绝在入口显式报告并 return; 不依赖这些函数在 Release 的 assert 空效果.

**备选:** 空 callback fallback、默认成功资源、全局 logger 或假 crash helper 会掩盖错误; 吞所有异常又无法证明 native 已回滚. 使用现有边界及唯一必要的标准异常出口.

### 6. GL profile 是负约束, 正支持仍来自底层

API 分类使用显式 D3D11/D3D12/VULKAN/OpenGL switch, 未知值拒绝构造. Utils::GraphicsAPIToString 添加 OpenGL→"OpenGL". 在 ValidationBackend 的模块私有范围定义可被 Device/CommandList 同用的有限 profile 判定, Device/CommandList 两个实现调用同一规则; 不创建公开 DeviceCaps、GL SDK 类型、配置开关或第二份 mutable capability cache.

| 项目 | OpenGL 初始规则 |
|---|---|
| QueryFeatureSupport | 现有 27 个 Feature 逐项覆盖; 仅 ConstantBufferRanges 可向底层查询, 返回底层真/假; 其余 26 个为 false. 未知值为无效请求. 不支持项不写 pInfo |
| QueryFormatSupport / ChooseFormat | 已知格式向底层查询, 只依据实际返回 flags 判定 usage; UNKNOWN 无支持. 格式表只提供 metadata, 不设置支持位 |
| CommandQueue / lists | 仅 Graphics、enableImmediateExecution=true; Create/Execute/GetNativeQueue/EventQuery/LifetimeTracker 等带 queue 的入口检查队列. 不支持 cross-queue wait、deferred replay |
| heap/virtual/sparse | CreateHeap、Bind*Memory、tiling/tile mappings 和虚拟资源拒绝; 普通资源描述不能通过 isVirtual 旁路 |
| bindless | CreateBindlessLayout/DescriptorTable 及全部 table mutation 拒绝; 常规多个逻辑 registerSpace 保留, 不按 D3D11 忽略空间 |
| RT/Meshlet/feedback/CoopVec | Device 创建/大小查询、资源 flags/Shader stage、CommandList 状态/构建/dispatch/转换及相关 state 请求全部拒绝, 包括直接透传的 void 方法 |
| 其他扩展 | Source API 专属和其余 26 Feature 对应的 shader/pipeline/描述字段请求同样拒绝, 不因底层 double 谎报 true 放行 |

当前公共 ObjectTypes 只有 D3D/Vulkan 原生标识, GL native-handle import 随真实 backend 工厂接入. ShaderLibrary::GetShader 可提取普通与扩展阶段, 来源 Validation 没有该资源包装; 为维持初始 GL 的受检阶段闭包, 现阶段拒绝库入口, 普通 Shader 通过受检的 CreateShader, 后续随真实消费者接入库及对应验证. 这不是 RT 专属接口, 固定 Vulkan 源的 GetShader 也接受普通 ShaderType. 共同 profile 同时检查直接传入的现有 Pipeline/BindingSet, 防止绕过 Create 路径的负约束; 不增加资源 owner registry 或宣称能验证 native 归属.

实施矩阵从 RHI.hpp 全部 Device/CommandList 方法和 Feature 声明生成清单, 将每个入口分类为通用转发/queue 校验/profile 拒绝/查询; 不能只测一处 Create 再假设整族已拒绝. 不支持族先做 API/profile 判定, 再接触其专有 payload, 因而可对 null payload 安全报 Unsupported. 通用可用路径再验证具体输入后转发. 查询 false 是正常结果; 实际尝试不支持操作必须有 Unsupported 诊断.

D3D11/Vulkan 等来源已有规则分别保留: D3D11 的 register-space 处理、Vulkan 的 null-binding/descriptor-set 规则不复制到 GL, 不把未知 API 当 Vulkan. Common 的 CoopVec 纯字节工具可运行不等于 GPU CoopVec 支持. Device 后续是否真正实现基础 Shader/Copy/Compute 由底层失败/caps 决定, 本包装不硬编码基础操作成功.

**备选:** 把所有功能 query 都固定 false 无法消费真实 ConstantBufferRanges; 不过滤底层会误报初始 profile 没有实现的功能. 用一个有限负策略与底层查询求交, 以后扩大 profile 必须在相应 native change 中验收.

### 7. Release 输入验证先于转换、解析和转发

| 输入类别 | 具体顺序和条件 |
|---|---|
| 必要指针/数组 | 先 null、count 表示范围和非零数据大小对应的有效存储前提; 再读取. 不试图验证悬空地址或通过异常捕获猜测数组实际长度 |
| 纹理 | 先有效维度/format enum 与非零尺寸, 再原有 1D/2D/3D/cube/MS 组合约束、mip<=完整链且 shift<32、mip×layer 的 size_t/容器表示能力; 最后 usage/format 查询与底层限制 |
| Buffer/Heap | 非零创建尺寸、合法 flags/stride/CPU access/volatile 条件; interval 使用 offset<=capacity && size<=capacity-offset, alignment 先非零和取模, 乘法先 max/operand 检查 |
| mip/layer/range | 用源 sentinel 和 safe Resolve 获取有效交集; 不相交/零长度拒绝, 可解析 oversized count 仍按来源 clamp; copy/写入等显式尺寸必须完整落入对象, 不用 clamp 掩盖越界 |
| BindingLayout/Set | 类型/visibility/size/slot/registerSpace/arrayElement、布局对应和重复项先检查; Resource* 用 Release 有效的 dynamic_cast 检查真实活对象类型后再取 desc; 按 type 读取活动载荷 |
| typed views/framebuffer | 依据 source format kind/byte/block/channel 兼容和实际 QueryFormatSupport 检查 SRV/UAV/RT/depth 等用途; 多附件尺寸/样本/层级一致, 先验证再构造 FramebufferInfo/Ex |
| copy/write/indirect/state | 源/目标各自 range、非空数据、对齐、参数记录尺寸乘法、合法当前状态和 flags 都检查; 开始计算/调用底层前拒绝错误 |

公共 builder 已经窄化的值无法恢复, 外部数据进入 16 位 binding size 等字段前仍须由首个业务边界检查; 本层不宣称恢复截断前原值. 不改变已修复的 BindingSetItem equality/hash 或 raw resource 借用语义.

检查集中在模块私有 Input 及共享绑定 validator, 由 Create 和已有资源/集合的命令消费入口复用, 不保留两份可漂移的描述规则. Vertex attribute 只检查已知格式、槽/数组和 offset+extent 的表示能力及同 buffer 的 stride/frequency 一致性, 不把零步长或 offset>=stride 当成通用错误; 实际设备上限仍归后端. Viewport 的深度端点各自检查 [0,1], 不额外禁止反向范围. Utils 的自动 layout 工具保留来源每槽单元素的前提, 数组须提供匹配 layout, PushConstants 的原始宽值在调用前受检. 公开资源描述未暴露 staging 的 CPU access/mapped 状态, 本层不虚构查询; 真实 owner 的进入/退出约束归 T03/T05.

View metadata 检查保留固定源的 stencil SRV 对应: typeless D24S8→X24G8_UINT、D32S8→X32G8_UINT, 不因 view 只含 stencil 而要求 hasDepth 完全相同. 依据为固定源 common/dxgi-format.cpp 的同 resourceFormat 映射及 vulkan-texture.cpp 的 stencil-aspect 处理; 这些文件只提供事实, 不进入构建. 两对仅对 SRV 的 Texture 支持请求适用, 仍须底层允许格式/ShaderLoad 或 ShaderSample; 非 typeless、错误对及 UAV/attachment 不因此放宽. 本层的 metadata 必要检查不能替代真实 backend 的 view compatibility/caps 验收.

资源归属范围明确: CommandList 有所属 Device/包装身份, 本层严格验证; 通用 Buffer/Texture 等公开资源没有 GetDevice, 来源 Validation 也不普遍包装它们, 本轮不虚构 API 或维护无法感知析构的裸指针 registry. 它们实际 native Device/context 归属由 T03 对象实现与同一套真实 Validation 集成用例保证. 此边界在完成说明保留, 不把 CPU 类型检查称为全部资源归属证明.

同样, 当前公共 QueryFeatureSupport/QueryFormatSupport 不能表达所有真实尺寸 limits 或每格式 sample 集合. 本层只检查公共合同可证明的语义和查询 flags; 来源的 MS 2/4/8 规则是本初始验证范围, 不是设备保证. T03 的真实 Device 在 Release 检查本机 limits/per-format samples, T05 验证与实测一致, 不为本层发明 QueryLimits API 或硬编码一套机器上限.

**备选:** Debug-only cast、先 Resolve/clamp 所有错误或先调用底层再报错都不足以保证 Release 正确性. 全量资源 proxy/地址 registry 会改变本轮来源边界及生命周期, 不为一个无法验证的 owner getter 预建框架.

### 8. 命令状态与提交批次先预检, 再消耗

保留 Initial→Open→Closed→Initial 的调用协议. Open/Close 错误不调用底层, immediate-open claim 按设备计数; Closed immediate 未提交不能重新 Open. 这些状态描述校验层调用协议, 不证明底层 void 原生命令成功; 后端错误如何停止工作由其真实实现负责.

ExecuteCommandLists 分三步:

1. 检查 Graphics/既有队列条件、count/pointer/uint32_t 表示边界, 分配解包暂存. 逐项检查非空、确为本 DeviceWrapper 创建的 CommandListWrapper、queue 匹配、Closed、无重复; 所有检查只读. 拒绝 unwrapped/foreign 列表, 不允许绕过本层生命周期.
2. 全批通过且暂存准备成功后, 才统一消耗各 Closed 列表的本次执行机会, 解包并恰好调用一次底层 Execute. 不在检查第一个元素时调用会修改状态的来源 RequireExecuteState.
3. 原样返回底层 ticket, 包括失败的 0. 一旦已进入底层提交不自动恢复 Closed 或重放, 因为不能保证 GPU 工作可回滚; 底层失败走其错误/退出协议. 校验阶段拒绝则全部保留 Closed, 可修正后重新提交.

MSVC Debug 的 vector 构造本身可能分配, 暂存的构造也必须位于预检 bad_alloc 捕获范围, 而非只保护 resize. 同理 BitSetAllocator 在 vector 成员构造参数求值时检查容量. 空 batch 先返回 0, 不访问数组或改变任何状态; 每次成功 Open/Close 清理全部 pipeline/push-constant 协议 flags, immediate claim 原子获取并恰好释放一次.

取消/析构按第 5 节释放 CPU claim/引用, 不偷偷产生提交票据. 本轮不实现 CommandListLifetimeTracker 的 GPU 完成算法, 仅为合法 queue 委托已有接口并保留其失败结果.

**备选:** 边检查边修改会破坏正确重试, 盲目恢复底层已执行列表又可能重复 GPU 工作. 只保证本层预检原子性, 不承诺跨原生提交事务.

### 9. Tracker 保持唯一状态权威, 补齐可观察错误与放弃出口

StateTracking.hpp 保留原有 Buffer/TextureStateExtension、单态/子资源态和 barrier 记录. descRef、extension 指针、MessageCallback 均借用, owner 必须保持地址/内容稳定至 Submitted/Reset/析构; 本层不偷偷保活 GPU 资源. tracker 是单写者的 recording 工作集, 状态量随本次涉及资源/子资源/待发射依赖增长, 有 size_t/max_size 和分配失败检查, 没有全局缓存/池.

有效输入的来源算法保留: BeginTracking 只建立已知状态; Require*State 生成 TextureBarrier/BufferBarrier, 整图优先, 必要时展开 mip×layer; 同态 UAV 根据 enable/first-barrier 协议仍生成依赖; 同一 barrier batch 中 buffer 的用途合并保持来源规则. ClearBarriers 只消费当前抽象记录, 不清除资源状态, 不表示 GPU 完成. close 前 Keep*InitialStates 准备恢复依赖; 资源 permanentState 和 texture stateInitialized 只在真实 owner 确认提交后发布.

有限内部扩展: 增加 HasErrors() 和 Reset(), 保持既有 void 请求签名, 不改公共 RHI.hpp. 无效描述/索引、Unknown 前态、非法 state bits、permanent 冲突或本地分配失败设置 sticky error, 报告原因并停止把当前 recording 当可发射结果. 冲突在请求阶段就检查, 不能等到真实提交后才首次发现. 未来 backend 先检查 HasErrors, 再发射抽象记录, 提交成功后才调用 CommandListSubmitted.

Reset 丢弃 maps、barriers、待发布永久状态并复位错误, 不写 external extension metadata; 本轮按 recording 生命周期释放这些工作存储, 不引入长期高水位缓存. Submitted 只接受无错误且依赖已消费的 recording, 先预检所有待发布永久状态再一起写入/清理; 提前调用或冲突只报错、不部分发布. 错误/取消均走 Reset, 绝不通过 Submitted 假装完成来清理. 当前没有 hot-path producer, 不为容量复用作性能结论; 后续实际 backend 有测量后再决定带预算的复用.

取消故障注入确认 MSVC Debug 的空容器构造也可能分配. 因此两张 tracking map 使用 optional 按需构造, Reset 仅销毁已有 map; vector 清空后释放数据容量, 不用新建空容器 swap. 在当前锁定 MSVC profile 上检查 Reset 及 Submitted 清理均不发起分配, 向量容量归零且 metadata 发布不被清理分配中断. 不把这一实现证据推广为未覆盖标准库的保证.

Common 不知道 GL/Vulkan barrier 编码, Validation 不另建资源状态 tracker. GraphManaged/Automatic 的用途选择和唯一原生发射者仍由 Architecture 7.5 的后续消费者实现. 本轮 CPU 用例可以读取/消费抽象记录、调用提交 metadata 回调, 只能证明协议而非真实 barrier 或完成行为.

**备选:** 原错误只记录日志会让调用者继续使用无效依赖; 用全设备等待或默认 Common 来兜底既不正确也掩盖责任. 使用最小 sticky error 和显式 Reset, 不增加图/同步框架.

### 10. 白名单差异、证据与实施顺序共同闭合

允许的非机械差异仅为: 直接标准依赖/路径/格式与命名、归档前明确授权的全第一方具名模块作用域/内部链接统一、静态构建、safe range/allocator 算术、factory/回调/Release 输入检查、已列 GL profile/字符串、诊断/明确异常、列表 CPU 退出/批次预检和 tracker error/reset/提交 metadata 原子发布. 在 Architecture 采纳后, 来源 README 对每项记录原因、目标范围和用例; 其他发现先更新本 design/spec/tasks 再依赖该决策实施, 不把此表当任意重写许可.

实施核对固定输入时确认 validation-device.cpp:309 的 createSamplerFeedbackForNativeTexture 在 D3D12 分支无条件自递归. 维护者已明确同意增加唯一局部修正: 改为 m_Device->CreateSamplerFeedbackForNativeTexture 的一次委托, 保留底层 handle/失败结果, 不改变 GL 的提前 Unsupported 拒绝. 此项纳入上述差异白名单, 用 D3D12 身份的 recording double 核对调用次数/参数及空结果传递, 同时回归 GL 零委托; 不因此引入真实 D3D12 SDK/设备或修改参考快照. spec 的完整可组合包装及底层失败保留要求不变.

使用临时来源词法/token 对照覆盖十个输入及既有头引用, 所有工厂宏/overload/override/默认参数/完整扩展都在清单内. 命名另外用独立于映射表的规则核对, 避免再次因同一不完整表而自证正确. 保存完整 symbol map、适配位置、源/目标 LF hash; 删除声明和改变输入的临时副本必须被拒绝. 许可块逐文件精确保留, 格式沿维护者设置, 不加门禁 target.

CPU fixture 全部在 build/artifacts/establish-rhi-common-validation/: 使用实际通用实现、存活 Resource probes 和只用于校验边界的 recording Device/CommandList double. double 的被测路径用配置结果/计数器, 其他路径 fail-fast, 不默认为成功、不进生产. 它允许检查拒绝后零底层调用、真实的引用计数和状态重试, 不声称 native caps/GL work. 相关 mock 方法完整对照 RHI.hpp 生成并编译, 不另建测试框架.

| 检查组 | 要证明的观察 |
|---|---|
| Common | 版本真/假、全部格式表数据/UNKNOWN、range sentinel/边界/最大整数、Framebuffer/纯工具、所有外部定义链接; allocator 0/满/复用/越界容量/并发唯一性 |
| Validation ownership/input | factory null/callback、层和列表的 CPU 保活/退出; bad type/null/overflow/invalid enum/desc/view/binding, Release 仍零错误底层调用 |
| GL profile | 全 27 Feature、全 Device/CommandList 入口分类矩阵; 允许项只按 double 返回, 其余即使 double 广告 true 也拒绝; 非 GL 关键来源规则不变 |
| List lifecycle | initial/open/close/execute/弃用 overload 存在; 多 immediate、析构 open claim、foreign/raw/duplicate/wrong queue/坏批次及正确重试; 底层 0 不变成 ticket |
| Tracker | 整图/子资源、begin 不发依赖、同态 UAV、keep initial/permanent 延迟发布、未知/越界/冲突错误、Reset 后无 metadata 变更/指针残留、可重复新 recording |
| Consumer/product | 六个公共头首 include、私有头自包含、只链接正式 target 的新目录 Debug/Release; 元数据无 native/参考/反向依赖; 全部现有产品及真实 host 的 help/非法参数/hidden 0/2/0 |

每个实现任务同时运行其 Debug/Release 检查, 不把所有第一次运行推迟到最后. 最终使用原生 verify 检查全部要求/场景与设计, strict、链接、格式/许可、diff 检查共同验收. 编译/CPU/现有宿主 smoke 分开记录在 tasks, 不新增 verification.md 或性能报告. `NotSupported` 的 Release assert、CPU image/截图、固定帧差和 mock ticket 都不能作为 native 成功证据.

**备选:** 只看文件数量、能编译或一组 happy path 不能证明错误转发/状态损坏; 另建长期测试/生成平台超出此包. 使用原生 tasks 与局部可复核 fixture.

## Risks / Trade-offs

- 来源 Validation 文件很大, 映射可能漏掉无常见前缀的方法 -> 完整 override/符号清单、独立命名核对、逐文件 token 和编译共同覆盖, 不以数量代替对应.
- 错误防护偏离上游 -> 差异只限第 10 节, 先更新采纳文档, 为每项保留正反观察与来源 hash.
- GL policy 有意比未来 backend 小 -> 表达为有限负约束, 只有允许查询来自底层, 扩大 profile 另行 native 验收; 不缓存重复 caps.
- 公开资源无法通用查询 Device 归属 -> 只声明可证明的列表 owner/类型合同, 真实 Buffer/Texture 归属留 T03/T05, 不创建失效不可控的地址 registry.
- Callback/descRef 是借用, 标准容器也可能分配失败 -> 明确寿命/不重入/异常合同, factory/预检失效路径及 Reset 用例, 不以引用计数代替 GPU 退休.
- 现有 D01 与计划文本尚未反映新限定决定 -> 1.1 先采纳文档再实现; T02 独立完成不放宽 T03 入口或任何阶段出口.

## Migration Plan

1. 先核对来源与当前公共头/三个主规格, 将本次已确认的阶段边界、有限适配、T03/T05 native 责任更新到当前文档并建立来源条目.
2. 按通用实现/STATIC target、safe range/allocator、tracker、完整 Validation、profile、输入、列表生命周期的依赖顺序实施, 每组有即时检查. 对实际发现的白名单外差异先修订本 change.
3. 新 binary 目录离线构建消费者和产品两配置, 执行 CPU 与现有宿主 smoke, 核对源码映射/元数据/所有权/失败退出并完成原生 verify, 再按证据标记 D02-T02. 规划创建时不勾选实现任务, 实施时仅按已完成的检查更新.
4. 没有现有 RHI runtime 消费者需要迁移或数据格式转换. 若新静态库/实现检查失败, 保留失败输出和未完成状态, 在本 change 修复; 不回退为成功空操作或删除诊断. 初次 apply 不自动提交、同步或归档; 当前按维护者新请求完成风格修订和复核后进入原生 archive, 规格同步遵循该流程选择. 不提交 Git, 不重写旧历史.
