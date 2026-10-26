# RHI 公共合同与来源

本模块按 [Architecture 7.1](../Docs/Architecture.md#rhi) 移植固定 NVRHI 的公共合同. 当前 PhysaraRHI 是 STATIC 消费入口, 提供六个自包含公共头及 Common/Validation 真实定义, 产生 RHI .lib. Device/backend 仍未实现; API 或枚举存在不代表设备能力已实现, 也不保证后端方法可以链接.

## 固定输入

来源为 NVIDIA-RTX/NVRHI commit `6b96fb03e07539f08327aea76c56d55f1de9d906`, 原始 HeaderVersion 为 27. MIT 版权及许可保留在每份移植头开头; 完整来源许可见 [参考快照许可](../ThirdParty/nvrhi/LICENSE.txt). 下表路径相对 ThirdParty/nvrhi; SHA256 对 UTF-8 内容仅规范化 CRLF 为 LF, 保留 BOM、空白和注释, 不等于 Git blob 或整个源码 zip 的哈希.

| 来源 | 目标 | SHA256 (LF) |
|---|---|---|
| `include/nvrhi/nvrhi.h` | `RHI.hpp` | `6fe801bd3ea0d039798065c80fdefabbcfcc4d30ebf3941dab9696529ffadcf8` |
| `include/nvrhi/common/resource.h` | `Resource.hpp` | `479a9a61586b4ba460fc936e24a469901ab8731311b421109bb4747c2ed1be30` |
| `include/nvrhi/common/containers.h` | `Containers.hpp` | `6df5867f3755b3e6456fbdf4dba8c40bc35a0f643622d5cd720263a28f894db4` |
| `include/nvrhi/nvrhiHLSL.h` | `ShaderTypes.hpp` | `4ac8f5407131320645d2301bfa025b4a66fa5f72a517f1e89453bf107320ef8f` |
| `include/nvrhi/utils.h` | `Utils.hpp` | `929ddbafcd6d18601745980656b5acced38f3d7a3faf409c95997fdd65a08a93` |

来源输入改变时重新审查映射与全部差异, 不自动接受漂移. ThirdParty/nvrhi 始终仅作参考, 正式 target 不 include 或链接该目录. 复核方法及实施证据见 [本次 change](../openspec/changes/archive/2026-10-25-establish-rhi-public-contract/tasks.md); 不在 CMake 运行移植脚本或哈希门禁.


D02-T02 的十个实际输入如下. 本表登记固定来源, 当前定义归属见下方. MIT 通知逐文件完整保留, 来源许可同上.

| 来源 | 目标 | SHA256 (LF) |
|---|---|---|
| include/nvrhi/common/misc.h | Common/Misc.hpp | `15dc45aac6328db04ddcb8b1ed863a5441414192b6c81006dad6fcd91fdf4a49` |
| src/common/misc.cpp | Common/Misc.cpp | `b89291c78a31c13c61e02bb870b1dbcf19e333f717786c0ea959a2ea19924c99` |
| src/common/format-info.cpp | Common/FormatInfo.cpp | `e5a4240cf19c050c7ac3088ab4486fc4089e64634b6e9f6581cfe02ddb346908` |
| src/common/utils.cpp | Common/Utils.cpp | `92e0c334a6bc51b179f92e0c9cb2c701e8aae9c9095e54726ea8cc562eb1cb04` |
| src/common/state-tracking.h | Common/StateTracking.hpp | `01400f8bc5cbc5b0d52b546e45a35ed66c79d8a4c98cc8b17e6af05c9d325fff` |
| src/common/state-tracking.cpp | Common/StateTracking.cpp | `db3af0fd2e64f342908d04c7f1d10c8bbba003b209cff8ea3892a2f229d80759` |
| include/nvrhi/validation.h | Validation.hpp | `b224d5d4484554207261b864dcd0974845f104fffc1881952dd12a21ec16ebdc` |
| src/validation/validation-backend.h | Validation/ValidationBackend.hpp | `6c935cc88c44518734b06ceebdc9aea9941726308fc7592f9e3205d3a2b7efb9` |
| src/validation/validation-device.cpp | Validation/ValidationDevice.cpp | `b2fc3a065ffbae6f5b6401ba17c4aa602224fd2be6f8d22521cef68a10581264` |
| src/validation/validation-commandlist.cpp | Validation/ValidationCommandList.cpp | `3a07c1a0e9dc08725f2ad750ed99cddb6e235334a7af7f7d23dd4762278bd5e1` |

排除 dxgi-format.*、aftermath.* 的原生/可选 SDK 实现; versioning.h、resourcebindingmap.h 不在本闭包 include 链中, 随真实 backend/提交消费者接入. 完整 Validation/扩展声明继续保留. 实施方法与证据见 [Common/Validation change](../openspec/changes/archive/2026-10-26-establish-rhi-common-validation/tasks.md).

## 完整改名规则

- 项目根命名空间 `nvrhi` 映射为 `Physara::RHI`. 子域完整映射为 `utils→Utils`、`rt→RT`、`rt::cluster→RT::Cluster`、`coopvec→CoopVec`、`validation→Validation`; 私有共享子域 `input/profile→Input/Profile`. 项目域各段统一 PascalCase, 缩写 RHI/RT 保持项目拼写. 不提供旧域 alias; `std` 及其 hash 特化等外部协议保留规定名称.
- 只对源声明的接口类去掉 I: Resource、Heap、Texture、StagingTexture、SamplerFeedbackTexture、InputLayout、Buffer、Shader、ShaderLibrary、Sampler、Framebuffer、OpacityMicromap、AccelStruct、BindingLayout、BindingSet、DescriptorTable、GraphicsPipeline、ComputePipeline、MeshletPipeline、EventQuery、TimerQuery、Pipeline、ShaderTable、MessageCallback、Device、CommandListLifetimeTracker、CommandList. 构造/析构、继承、前置声明、Handle 和全部类型引用同步映射; 不对其他 I 开头标识符做猜测.
- 非标准协议的普通函数将首字母改为大写, snake_case 自由函数 `hash_combine` 改为 `HashCombine`; 包含 CommandList 的 `draw/dispatch/resolveTexture/commitBarriers` 到 `Draw/Dispatch/ResolveTexture/CommitBarriers`, 不限于动词前缀匹配. 来源已为 PascalCase 的名称保留. 根据函数声明/调用位置改名, 同名 `width/height` 数据字段和参数保持原样.
- `static_vector` 改为 `StaticVector`; 其模板参数 `_max_elements` 改为 `Capacity`, 容量常量 `max_elements` 改为 `MaxElements`, 内部别名 `base` 改为 `Base`.
- 标准容器操作 `at/front/back/data/begin/cbegin/end/cend/empty/size/max_size/fill/swap/push_back/pop_back/resize/emplace_back`、标准 aliases、`std::hash` 与运算符保留; 不提供另一套同义 API. 静态工厂如 `Texture_SRV` 保留与上游协议类型对应的拼写.
- `c_` 常量去掉前缀, cluster 的 `k` 前缀常量去掉 k; 枚举项和值保持上游拼写及数值. `NVRHI_*` 宏统一改为 `PHYSARA_RHI_*`, 导入导出条件不扩展为本轮 DLL 构建能力.
- 句柄/容器内部成员 `ptr_/m_refCount/current_size/m_commandList` 分别改为 `m_Pointer/m_RefCount/m_CurrentSize/m_CommandList`; RefCountPtr::Create 的局部 Ptr 改为 ptr. descriptor 字段和原有纯数据记录字段不改名.
- 通用/Validation 新增映射为 checked_cast→CheckedCast、hash_to_u32→HashToUInt32、align→Align、arraysAreDifferent→ArraysAreDifferent、arrayDifferenceMask→ArrayDifferenceMask、unwrapResource→UnwrapResource、calcSubresource→CalcSubresource. c_FormatInfo→FormatInfoTable 避免遮蔽类型, g_GraphicsShaderStages/g_MeshletShaderStages→GraphicsShaderStages/MeshletShaderStages, kOperationTypeStrings→OperationTypeStrings, m_type→m_QueueType, 私有列表枚举 INITIAL/OPEN/CLOSED→Initial/Open/Closed. Range/BindingSummary 的 Empty/Any/OverlapsWith 是领域方法, 不沿用标准容器命名例外. cpp 私有 helper/表放在所属具名模块域内, 以 static 表达内部链接; 跨文件声明在相应头和领域域中, 不用匿名域或仅用于收纳 helper 的新域.
- 只替换代码标识符及引用方法名的弃用诊断, 不在字符串/注释里盲替换普通英文词. 许可和来源注释保留, 新适配说明用中文. 格式采用项目的 4 空格、Allman、120 列与控制体大括号; 大主头保持单一来源合同, 规模评审见 design.

## 限定适配与行为前提

1. 五个头补齐直接标准库依赖、改写模块内 include; ShaderTypes 保留跨语言条件并补 C++ 整数头, 本轮只验证 C++20/MSVC profile.
2. `HeaderVersion` 保留 27, `OriginalHeaderVersion=27` 识别来源; GraphicsAPI 末尾追加 `OpenGL=3`, 既有 D3D11/D3D12/VULKAN 仍为 0/1/2. 不声明 NVRHI 二进制 ABI 兼容, 不新增 GL 对象类型或工厂占位.
3. 绑定项相等和哈希纳入 `arrayElement`, 公共字段及按 type 选择的有效 subresources/range 均按语义比较/组合. 不读取不同活动 union 成员、reserved 或 padding. None/Sampler/AccelStruct 无范围载荷参与 key.
4. 绑定集合相等和哈希纳入 `trackLiveness`, bindings 顺序有意义. 相同 key 的 hash 相同; 不同 key 可以碰撞, equality 必须正确区分.
5. 资源裸指针为借用. RefCountPtr 的 copy/retain 增加引用, move/Attach/Create/Detach 按源合同转移, Get/箭头不增引用. 引用仅保护 CPU wrapper, 不证明 GPU 完成, 原子计数不表示资源内容可以并发修改.
6. BindingSetItem 默认构造及 HeapDesc::type 等来源无默认值字段保持暂存前提, 须先通过工厂/赋值完成初始化. key 的 type/活动载荷匹配不等于已通过未来 Device/Validation 的资源、范围或能力验证. 进入窄字段的外部值须在 builder 之前受检.
7. StaticVector 保持合法容量/索引及元素操作前提, 包括原数组槽的保留/重置语义; 不静默扩容, Release 不允许越界调用. 标准协议和暂存构造的保留只限于本次移植边界. T01 归档时保留的小写源子域已由本次统一域映射取代, 历史记录保持原状.
8. 经维护者确认, initializer-list 构造补 Base() 初始化, 与默认/尺寸构造一致; 防止部分填充的标量/指针容器在整数组 swap 中读未初始化槽. 布局、逻辑内容和容量不变, 不作性能结论.

上述非机械差异的原因、范围、替代方案和验证见 [design](../openspec/changes/archive/2026-10-25-establish-rhi-public-contract/design.md). 不据此普遍允许重写来源算法.

D02-T02 的当前有限差异及前提:

- Common 的 TextureSubresourceSet 用剩余计数计算交集, 不作 base+count; 越界 base 产生空交集, oversized count/whole sentinel 保留 clamp, 非数组仍为一层. BufferRange 保留 byteSize=0 的剩余区间含义.
- TextureSlice/FramebufferInfoEx 只消费已验证的描述和 mip<32, 头中写明前提; matrix stride 检查 size_t 乘法表示能力. 私有 Align 要求非负/二次幂/可表示加法, array mask 使用 uint32_t{1} 移位. 这些 helper 不代替外部输入校验.
- BitSetAllocator 在分配前以 length_error 拒绝 capacity>INT_MAX, 零容量有效且耗尽, 固定容量不增长. 循环索引用 size_t 后证明可窄化, 同步模式用 RAII 条件锁; 非法 Release 索引仍忽略. RangeChecks 的 Debug/Release 最大整数、拒绝前无大分配和并发活索引唯一性验证这些差异.
- StateTracking 保留来源的有效状态/UAV/用途合并算法, descRef/extension/callback 借用且单写者. 索引用 size_t 并检查所需描述/range/state 和存储表示能力; Unknown 前态、永久状态冲突和本地存储失败置 HasErrors. 错误 recording 不可发射, Reset 丢弃全部记录/存储且不发布 metadata. 待永久状态也约束后续请求并阻止 keepInitialState 回退, 包括来源遗漏的 buffer permanentTransition 标记. Submitted 要求依赖已消费且整体预检通过后一起发布 metadata, 随后释放工作集; 不表示 GPU 完成. TrackerChecks 同时覆盖有效输入、失败/取消和 clean reuse.
- Validation 工厂先拒绝 null Device/callback 和未知 API, 成功包装保留底层 Device 引用; 列表保留底层列表及父包装. 借用 callback 不抛出/不重入且寿命覆盖包装, 设备按底层线程合同、列表单写者. 工厂和列表包装自身 bad_alloc 返回空并以无分配诊断报告, 普通 CPU/string/container 异常可传播. 诊断包含原因、操作和 source_location; 底层空/false/0 原样传递. 析构释放 open immediate claim 并在父包装仍存活时先释放底层列表, 不隐式 Close/Execute/等待 GPU.
- 初始 OpenGL 策略在 Device/CommandList 共同的私有 profile 中判定, 无第二份 caps/cache. 27 个 Feature 仅 ConstantBufferRanges 可委托底层, 其余 false 且不改 output; 已知格式仅转发实际 flags, UNKNOWN 无支持. Graphics/immediate 以外、heap/virtual/sparse、bindless、RT/Mesh/feedback/CoopVec 及专属 Shader/raster/VRS/共享资源扩展拒绝. 当前仅有 D3D/Vulkan 原生对象标识, 对应 native-handle imports 拒绝. ShaderLibrary 可提取普通及扩展阶段, 来源 Validation 不包装其 GetShader; 初始 GL 关闭这一未受检入口, 普通 Shader 通过受检的 CreateShader, 库能力随真实消费者接入并验收. 创建/查询/mutation/void 及复用现有 pipeline/bindings 的状态入口共同检查, 不先读取专有 payload. IsAftermathEnabled=false, 引用 getter 报告后抛 logic_error. 现有 D3D11 忽略 registerSpace、Vulkan 要求 descriptor-set 标记等分支保留, GL 的 logical spaces 不被合并.
- 经维护者确认, 修正固定源 validation-device.cpp:309 的 sampler-feedback native-handle 工厂自递归, 仅在来源允许的 D3D12 分支改为底层一次委托并保留 handle/失败结果; GL 的 Unsupported 拒绝不变. CPU recording double 的调用/参数/空结果回归验证此局部差异, 不声称原生 D3D12 支持.
- Release 输入检查先于解析/转换/委托. 共用私有 Input 规则集中 Texture/Buffer 的维度、mip、格式、flags、范围与状态条件, 创建和命令消费同一事实; 绑定 validator 在创建和复用已有集合时共享, 不新增可变 registry/caps cache. Resource* 的 Texture/Buffer/Sampler/反馈/AS 类型检查在所有配置使用 dynamic_cast. 只读取 type 对应的 union 载荷, 校验 view、arrayElement、slot/type/layout 与 push constant size. 合法请求保留一次委托, 参数错误、缺少格式能力分别为 InvalidArgument/Unsupported, 一个失败操作只报告一次.
- buffer/heap 显式区间以 size<=capacity-offset 检查, 不用加法或 clamp 掩盖 copy/write 越界. Texture slice 先证明 mip<32 与范围, 写入按 block/row/depth pitch 检查最小可表示 footprint. Framebuffer 多附件检查 mip 尺寸、层级、sampleCount/quality, 状态检查 vertex/index/indirect range、固定枚举、viewport/scissor 和绑定对应. 公共数据不能表达全部 native limits、每格式样本集合、staging CPU access/mapped 状态或资源 Device; 这些仍由真实 T03 owner 实现并于 T05 验证. 不臆造 QueryLimits、owner getter 或全局地址表.
- 外部值进入 builder 的窄字段前仍须检查, 本层无法恢复已截断的原值. Utils 自动 layout 生成保留来源的每槽单元素前提, 数组由调用者提供现有匹配 layout. Vertex attribute 不臆造设备 stride/offset 上限, 允许可表示的零步长和较大相对偏移; viewport 深度端点分别受 [0,1] 限制, 不额外禁止反向范围. 资源描述与参数均须先初始化, 例如值初始化 GraphicsState, 再填充实际使用字段.
- 列表批次先检查数量/包装身份/queue/Closed/重复项并准备全部暂存, 再统一消耗状态并委托一次. 拒绝或自身预检分配失败保留全部 Closed, 后项失败不消耗前项; 委托后的 0/异常不恢复重放. MSVC Debug 的容器构造也纳入分配失败边界, 空 batch 按来源直接返回 0. immediate claim 以原子 CAS 获取, 关闭/析构只释放一次; 底层列表先于父包装释放, 不补执行. BitSetAllocator 的容量检查放在 vector 成员的构造参数求值中, 先于其 Debug bookkeeping/数据存储; 私有数组比较使用 size_t 遍历, 不截断逻辑序列长度.

完整理由与适配边界见 [通用层 design](../openspec/changes/archive/2026-10-26-establish-rhi-common-validation/design.md), 实际结果见其 tasks. 不将本表的 CPU/metadata 合同解释为原生支持或 GPU 完成.

Tracker 的两张 map 按需在 optional 中构造, Reset 销毁它们并清空/释放 vector 容量. 这避免 MSVC Debug 空容器 swap 的清理分配; 故障注入覆盖 Reset 和 Submitted 的无分配清理, 未推广到其他标准库.

View 检查保留来源已有的 typeless stencil SRV 对: D24S8→X24G8_UINT 和 D32S8→X32G8_UINT, 依据为同快照的 common/dxgi-format.cpp 与 vulkan-texture.cpp. 不把 stencil-only metadata 误判成深度布局不兼容; 格式/采样能力仍查询底层, 错误对、非 typeless 和其他 view 用途不放宽. 实际 native view 兼容性仍随 T03/T05 验收.

## 外部定义归属

通用定义已链接交付如下; 后端方法不应用成功空操作补齐链接:

| 声明 | 当前定义/后续归属 |
|---|---|
| VerifyHeaderVersion、TextureSlice/TextureSubresourceSet/BufferRange 的 Resolve、IsEntireTexture | `Common/Misc.cpp`, 已实现 |
| BlendState/RenderTarget 的 UsesConstantColor、FramebufferInfo/Ex 的外部构造 | `Common/Misc.cpp`, 已实现 |
| CommandList::SetResourceStatesForFramebuffer、CoopVec::GetDataTypeSize/GetOptimalMatrixStride | `Common/Misc.cpp`, 已实现 |
| GetFormatInfo | `Common/FormatInfo.cpp`, 已实现, 仅 metadata |
| Utils 的自由函数、BitSetAllocator 构造/Allocate/Release | `Common/Utils.cpp`, 已实现 |
| Validation::CreateValidationLayer 及完整 Device/CommandList 包装 | `Validation/ValidationDevice.cpp` / `ValidationCommandList.cpp`, 仅通用层 |
| Device、CommandList 等纯虚方法、AftermathCrashDumpHelper 的实现 | 实际后端/可选能力工作包, 当前无实例 |

ScopedMarker 和 descriptor 的头内转发仍需满足其调用对象/外部定义前提, 不因函数体在头里就保证所有使用方式可链接. 公共头不依赖 Core、Platform、Editor、GLFW、GLAD、ImGui 或 Vulkan SDK. 构建/运行说明集中在 [Build](../Docs/Build.md), 当前工作包状态见 [Developments](../Docs/Developments.md#d02-t02).
