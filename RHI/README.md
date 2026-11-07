# RHI 公共合同与来源

本模块按 [Architecture 7.1](../Docs/Architecture.md#rhi) 移植固定 NVRHI 的公共合同. PhysaraRHI 是无 native 依赖的 STATIC 消费入口, 提供六个自包含公共头及 Common/Validation 定义. 原生组件通过独立 PhysaraRHIOpenGL 消费, 支持范围见下方; API 或枚举存在不代表对应设备操作已经实现.

## OpenGL 原生消费边界

OpenGL.hpp 是不包含 GL/GLFW 类型的后端互操作入口. PhysaraRHIOpenGL PUBLIC 链接通用 PhysaraRHI, PRIVATE 使用固定 glad 与 spirv-cross-glsl; 根工程显式接入, 只 add_subdirectory(RHI) 的通用消费者不配置或链接 loader/Shader 工具链. 后端根据公共接口实现, 不存在可直接移植的 NVRHI GL backend; 新原生代码的签名与固定公开合同对应, 第三方参考仍不编译.

后端已实现实际工厂/caps/clip convention、四类基础分配、无分配退休及正式 Host 的 raw/Validation 装配, 验证结果和实际环境见 [归档任务](../openspec/changes/archive/2026-10-27-establish-opengl-device-resources/tasks.md). 固定公共来源和完整声明不随本后端裁剪.

| 当前入口 | 支持和边界 |
|---|---|
| Device | 68 项 override 保留; 仅 ConstantBufferRanges 随实际 BindingSet 范围消费返回 true, 其它 Feature 为 false/output 不变, 格式 flags 只说明已实现用途. clip 为 upper-left/zero-to-one, 有限离屏直接绘制不代表稳定图像/呈现阶段出口 |
| Buffer | 普通 immutable storage, null data/GL_DYNAMIC_STORAGE_BIT; 保留 raw/structured 和 vertex/index/constant 等描述, 拒绝 CPU/volatile/typed/virtual/shared/RT profile. 列表支持 checked interval 写入和普通 byte copy, 公共 Map/readback 与 UInt clear 仍拒绝 |
| Texture | 17 项格式映射与 9 种 target 按合法组合、每 target 实际用途/limits/样本集合的交集创建; 3D 深度/模板存储拒绝. 公共格式 flags 表示 2D 基线. 非 MS 应用合法 component mapping, initial state/clear 仅存 metadata |
| Sampler | 独立 filter/address/border/bias/anisotropy; finite 和实际范围预检, Comparison 使用固定 Vulkan 来源的 LESS, Minimum/Maximum 拒绝 |
| Framebuffer | 单 mip/单层或 cube face, color/depth/组合及实际 MS; 完整附件集先预检, typeless 解释还需实际 view class 兼容, COMPLETE 后发布. 必要 view 用 glGenTextures -> glTextureView, 不缓存. 当前执行仅单样本; 3D/layered/空/shading-rate/只读颜色拒绝, 只读深度在图形组合时拒绝潜在写入 |
| Shader | 自持 canonical Vulkan SPIR-V 1.3、main 入口和活动反射, GraphicsBaselineV1 的 VS/FS 子集; 创建不编译 GLSL. 原始输入存储可在返回后释放 |
| BindingLayout/Set | 常规逻辑布局保留 set/b/t/s, 支持顺序和 descriptor-set 策略. Set 强持有布局/资源并预解析普通 UBO、只读定长 record SSBO、float 2D 视图与独立 Sampler, 不绑定全局单元. descriptor 数组、typed/raw、image/UAV、volatile/push 消费拒绝 |
| InputLayout/GraphicsPipeline | 有限 core 顶点格式和属性展开, 单样本普通图形准备. Pipeline 按跨 stage 活动 IR 紧凑分配 UBO/SSBO/pair 单元, 核对 per-stage/combined limits, lowering 为 GLSL 460 并真实 link. 同纹理不同 sampler 占不同 pair, texture-only fetch 使用设备自有 nearest/clamp sampler; 兼容性沿 FramebufferInfo 全部字段, 不含附件身份/尺寸 |
| CommandList | 61 项 override 完整; 仅 Graphics/immediate、无外部 lifetimeTracker. 支持上传、完整二维选层 typed clear、普通 Buffer/同格式单样本二维 copy、单 viewport/scissor 图形状态和直接 Draw/DrawIndexed/实例; 其余命令在读取专有 payload 前 Unsupported |
| 提交/完成 | Execute 整批预检, 非空成功才发布真实 fence/flush 票据, 不重放上传、clear/copy 或 Draw. GC 零超时轮询, WaitForIdle 显式 5 秒期限. CPU Release 与 GPU 完成分开, 新绘制依赖沿同一包保活, orphan 由 owner 建内部 fence 退休 |
| 其余 Device 操作 | compute/Shader library/specialization、bindless、公共 readback/query/extensions 明确 Unsupported, 不读取无关 payload、不制造对象/票据. 空 Execute 为 0, 引用型 crash helper 报告后 logic_error |

GetNativeObject/GetNativeView 不新增 GL 公共标识且对未支持导出返回空. Backend.hpp/CommandList.hpp 中的声明形状来自同一公共合同并保留 MIT 通知, GL 算法为本项目实现. Sampler LESS 的参考为固定 src/vulkan/vulkan-texture.cpp 的 compareOp=eLess, 未将 Vulkan 原生实现加入链接. 上传与完成的具体方法/证据见 [上传 change](../openspec/changes/archive/2026-10-28-establish-opengl-resource-uploads/design.md), 不从分配 flags 推导 Shader/Draw 或完整 readback 能力.

GL 4.6 Core 8.5 不允许 TEXTURE_3D 的深度/模板存储. 自有 `Formats.cpp` 在构建 target caps 时显式过滤 D16/D24S8/D32/D32S8 的此组合, 即使驱动 internal-format query 返回正向值也不发布支持, 创建在原生分配前拒绝. 这是保守能力交集的实现修正, 不修改固定 NVRHI 来源. 复现、来源依据与回归见 [资源一致性 design](../openspec/changes/archive/2026-10-29-establish-opengl-resource-conformance/design.md) 的局部修正; 其他合法 target 仍按实际用途和样本验证.

历史资源一致性 fixture 复用同一正式 Host/Validation/backend; 当时的辅助 native clear/resolve/readback 不等于公共执行能力. 本机 UPPER_LEFT 原生区域 clear 的历史观测见上述 design, 当前执行边界以本节为准, 历史本地检查与复现条件见 [Build](../Docs/Build.md#历史本地检查).

上传策略为设备总 mapped capacity 64 MiB、最多 64 物理包、每包最多 4096 非空写入/不同资源及 64 MiB 逻辑目标字节. 小于等于 256 KiB 的 Buffer 写入使用 DSA subdata, 更大写入使用包独有的 persistent/coherent chunk 和 GPU copy. 所有 Texture 上传使用同类 PBO chunk; 完成前不覆写或跨包复用, 完成即释放, 没有 idle cache. 耗尽报告 requested/retained/limit, 不隐式等待. 这些是初始有界策略, 不是设备显存或性能结论.

颜色上传沿唯一 FormatMapping 扩充 external format/type, 支持 R8_UNORM/R8_UINT/RG8_UNORM/RGBA8_UNORM/RGBA8_UINT/SRGBA8_UNORM、R16/RG16/RGBA16_FLOAT、R32_UINT/R32/RG32/RGBA32_FLOAT, 覆盖七种非 MS 维度. 每次写完整选定 mip/layer/face 或 3D volume; row/depth footprint 检查后逐行紧凑复制, 不读取 padding 为 texel, 不翻转或做 gamma 转换. 单行允许 rowPitch=0, 单 depth 忽略 depthPitch; caller 须保证源指针覆盖计算出的可读 footprint. Write 返回后源可释放. 深度/模板/MS 仍可按原分配 profile 创建, 上传拒绝; 已知有限 half/float 值的存储行为不扩展为任意 NaN 位模式保证.

ContextState 统一拥有 unpack、Program/FBO、ranges、采样对、VAO 输入及普通固定/动态状态缓存, 只有一个辅助 VAO 和完整子资源 clear FBO. 逻辑 Graphics 状态与 Common 用途 tracker 另由同一物理包拥有; 外部原生互操作后在 recording 中 ClearState 并重新设置图形状态, 已发工作和引用仍保留. Automatic 在每次实际消费建立用途; 显式模式由调用方请求并 Commit. 缓存命中不能跳过依赖, Close 同样恢复 keepInitialState; 当前只读 Shader profile 不发无依据的全域 barrier/等待. 只有 fence/flush 接受后才发布 tracker metadata, 不表示 texel 初始化或 GPU 完成.

GL 的有效图形快照与本次 IB 范围检查统一由 owning recording 保存/消费; Validation 不把 void 设置委托当作候选生效, 其他 API 保留上游快照规则. 基线没有第二颜色输出, 准备阶段拒绝活动双源 blend 因子; inactive 因子按关闭 blend、附件类型/分量、Min/Max 和写 mask 判定, 原生使用无害值. 缓存包含颜色格式, 切换附件格式时重新应用实际使用的通路. 只读深度拒绝测试和写入均启用的组合, 不因关闭测试时未使用的 write flag 拒绝合法颜色 Pass; stencil 写入仍检查.

完整二维 clear 逐 mip/layer 使用 typed FBO 路径, 关闭残留 scissor 并恢复完整 mask, 不改变 clip origin; depth-only/stencil-only 保留另一 aspect. copy 保留 byte/texel、编码及逻辑 row 0, 拒绝越界/重叠和格式转换. 图形消费依当前 Pipeline 的活动计划绑定 UBO/SSBO ranges 与跨集合采样对, 支持整数/归一化属性、R16/R32 索引及实例偏移. 公共目标空间绕序只在后端适配, viewport/scissor 不额外翻转; Reverse-Z 由调用方 clear/深度/比较共同选择. sRGB 目标写入编码一次、采样解码一次. staging/resolve、1D/3D clear/copy、Buffer UInt clear、MSAA 执行、多 viewport、indirect、Compute/UAV 仍明确拒绝.

默认保活的 Pipeline、集合、Framebuffer 及其实际子引用接入同一包 lease; 内部复合边只记一次, 真正外部 owner 继续阻止破坏性关闭. trackLiveness=false 时调用方负责集合及其依赖到完成前的寿命. 包/辅助缓存不形成增长的资源组合表, native 删除前解除旧绑定. worker 最终释放列表只交接 orphan, owner 完成收集或受控退出后才销毁. 当前有限执行和验收记录随 [D03-T03](../Docs/Developments/D03-D05-FramePipeline.md#d03-t03) 引用, 不代替 T04 完整在途矩阵、T05 稳定图像或 Runtime Renderer.

ContextAccess 的 user/isCurrent 和 MessageCallback 借用到所有 Device CPU 引用结束; 直接工厂调用方保留根 owner, 收拢资源和额外引用后在 owner 线程 ShutdownDevice. CPU final Release 只转移创建时预留的删除记录, 真正 glDelete 由 owning/current 线程收集. Normal 与 ContextUnavailable 明确区分当前 context 删除和外部 owner 随后销毁不可用 context 的退出; 后者不发布 GPU 完成. 普通缺少 current 不自动切换模式.

Host 的 GetDevice 只在 owner/Ready 返回 Validation 借用. 调用方先停止/join 使用者并释放外部资源及额外 Device owner; 纯 CPU 预检核对 validated==1、raw==已知根数+内部唯一 wrapper 数及每个资源的精确内部 holds. 不满足时 ResourceInUse 在 teardown 前拒绝且可重试. 通过后 drain -> Validation -> raw -> adapter -> input/context/window. RhiDrainPending 停止新工作并保留全部 owner/context 供重试; 析构仍有外部 owner 或未完成 drain 属于致命合同违例. 回调先保存有界主错再写 Diagnostics, 不从 severity 推断 context loss. 无可靠 fence 只在受控退出尝试一次 glFinish, 驱动调用无硬实时保证; 再次失败/确认不可用后只撤销 CPU 状态并由 owner 最终销毁 context, 不发布假完成.

## 固定输入

首次存量审查的 CR-001/002/003 修正属于项目局部正确性差异: Validation 在 descriptor-set registerSpace 超限时于映射访问前拒绝; Pipeline 汇总使用普通数组, Shader 工作 vector 使用可抛出的 count 构造, 保留普通 CPU 异常传播; 两处纹理子资源长度检查直接计算受支持默认分配器的可表示上界, TextureState 的空表以 count=0 构造使 Debug proxy 分配失败进入 tracker 的 sticky-error 边界. 这些修正不改变公开 StaticVector 的元素操作前提、固定来源或原生支持范围. 方法与独立复核状态见 [整改 design](../openspec/changes/archive/2026-11-03-fix-validation-cpu-boundaries/design.md) 和 [审查闭环留存](../openspec/changes/archive/2026-11-03-fix-validation-cpu-boundaries/tasks.md#review-loop-closeout).

同轮逐分配点检查还确认 MSVC Debug 的私有 hash 表在 noexcept 内分配 proxy: Validation 的逻辑位置/输入/提交去重集合改为 std::set, tracker 的两个状态索引改为 std::map. 比较仍基于完整语义字段或资源地址身份, 生命周期和错误边界不变; 删除仅供私有 BindingLocationSet 使用的 hash 特化, 公开 RHI hash 保留. 这是已实测的 CPU 故障路径修正, 不宣称更快或更低开销.

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

D03-T02 对同一 Validation 增加三个 GL 准备适配: 顺序布局按 Pipeline 位置解释有效 set, 显式布局沿 registerSpace; Texture_SRV 允许受支持 array/cube storage 的显式二维单层 view, 仍检查范围/格式并由 backend 落实原生兼容性; 两种图形工厂共用描述校验, framebuffer 重载保留实际对象到 backend 归属检查. 其它 API 的寄存器空间与维度规则不变, 不增加对象 registry. 修正与复核状态见 [Shader/Pipeline change](../openspec/changes/archive/2026-11-06-establish-opengl-shader-pipelines/tasks.md#review-retention).

D02-T02 的当前有限差异及前提:

- Common 的 TextureSubresourceSet 用剩余计数计算交集, 不作 base+count; 越界 base 产生空交集, oversized count/whole sentinel 保留 clamp, 非数组仍为一层. BufferRange 保留 byteSize=0 的剩余区间含义.
- TextureSlice/FramebufferInfoEx 只消费已验证的描述和 mip<32, 头中写明前提; matrix stride 检查 size_t 乘法表示能力. 私有 Align 要求非负/二次幂/可表示加法, array mask 使用 uint32_t{1} 移位. 这些 helper 不代替外部输入校验.
- BitSetAllocator 在分配前以 length_error 拒绝 capacity>INT_MAX, 零容量有效且耗尽, 固定容量不增长. 循环索引用 size_t 后证明可窄化, 同步模式用 RAII 条件锁; 非法 Release 索引仍忽略. RangeChecks 的 Debug/Release 最大整数、拒绝前无大分配和并发活索引唯一性验证这些差异.
- StateTracking 保留来源的有效状态/UAV/用途合并算法, descRef/extension/callback 借用且单写者. 索引用 size_t 并检查所需描述/range/state 和存储表示能力; Unknown 前态、永久状态冲突和本地存储失败置 HasErrors. 错误 recording 不可发射, Reset 丢弃全部记录/存储且不发布 metadata. 待永久状态也约束后续请求并阻止 keepInitialState 回退, 包括来源遗漏的 buffer permanentTransition 标记. Submitted 要求依赖已消费且整体预检通过后一起发布 metadata, 随后释放工作集; 不表示 GPU 完成. TrackerChecks 同时覆盖有效输入、失败/取消和 clean reuse.
- Validation 工厂先拒绝 null Device/callback 和未知 API, 成功包装保留底层 Device 引用; 列表保留底层列表及父包装. 借用 callback 不抛出/不重入且寿命覆盖包装, 设备按底层线程合同、列表单写者. 工厂和列表包装自身 bad_alloc 返回空并以无分配诊断报告, 普通 CPU/string/container 异常可传播. 诊断包含原因、操作和 source_location; 底层空/false/0 原样传递. 析构释放 open immediate claim 并在父包装仍存活时先释放底层列表, 不隐式 Close/Execute/等待 GPU.
- 初始 OpenGL 策略在 Device/CommandList 共同的私有 profile 中判定, 无第二份 caps/cache. 27 个 Feature 仅 ConstantBufferRanges 可委托底层, 其余 false 且不改 output; 已知格式仅转发实际 flags, UNKNOWN 无支持. Graphics/immediate 以外、heap/virtual/sparse、bindless、RT/Mesh/feedback/CoopVec 及专属 Shader/raster/VRS/共享资源扩展拒绝. 当前仅有 D3D/Vulkan 原生对象标识, 对应 native-handle imports 拒绝. ShaderLibrary 可提取普通及扩展阶段, 来源 Validation 不包装其 GetShader; 初始 GL 关闭这一未受检入口, 普通 Shader 通过受检的 CreateShader, 库能力随真实消费者接入并验收. 创建/查询/mutation/void 及复用现有 pipeline/bindings 的状态入口共同检查, 不先读取专有 payload. IsAftermathEnabled=false, 引用 getter 报告后抛 logic_error. 现有 D3D11 忽略 registerSpace、Vulkan 要求 descriptor-set 标记等分支保留, GL 的 logical spaces 不被合并.
- 经维护者确认, 修正固定源 validation-device.cpp:309 的 sampler-feedback native-handle 工厂自递归, 仅在来源允许的 D3D12 分支改为底层一次委托并保留 handle/失败结果; GL 的 Unsupported 拒绝不变. CPU recording double 的调用/参数/空结果回归验证此局部差异, 不声称原生 D3D12 支持.
- Release 输入检查先于解析/转换/委托. 共用私有 Input 规则集中 Texture/Buffer 的维度、mip、格式、flags、范围与状态条件, 创建和命令消费同一事实; 绑定 validator 在创建和复用已有集合时共享, 不新增可变 registry/caps cache. Resource* 的 Texture/Buffer/Sampler/反馈/AS 类型检查在所有配置使用 dynamic_cast. 只读取 type 对应的 union 载荷, 校验 view、arrayElement、slot/type/layout 与 push constant size. 合法请求保留一次委托, 参数错误、缺少格式能力分别为 InvalidArgument/Unsupported, 一个失败操作只报告一次.
- buffer/heap 显式区间以 size<=capacity-offset 检查, 不用加法或 clamp 掩盖 copy/write 越界. Texture slice 先证明 mip<32 与范围, 写入按 block/row/depth pitch 检查最小可表示 footprint. Framebuffer 多附件检查 mip 尺寸、层级、sampleCount/quality, 状态检查 vertex/index/indirect range、固定枚举、viewport/scissor 和绑定对应. 公共数据不能表达全部 native limits、每格式样本集合、staging CPU access/mapped 状态或资源 Device. T03 已原生验证基础分配的 limits、样本集合与 Device 归属, T04 验证普通 Buffer/初始颜色上传及实际完成; 公共 staging/mapped 与完整读回仍归后续验收. 不臆造 QueryLimits、owner getter 或全局地址表.
- 外部值进入 builder 的窄字段前仍须检查, 本层无法恢复已截断的原值. Utils 自动 layout 生成保留来源的每槽单元素前提, 数组由调用者提供现有匹配 layout. Vertex attribute 不臆造设备 stride/offset 上限, 允许可表示的零步长和较大相对偏移; viewport 深度端点分别受 [0,1] 限制, 不额外禁止反向范围. 资源描述与参数均须先初始化, 例如值初始化 GraphicsState, 再填充实际使用字段.
- 列表批次先检查数量/包装身份/queue/Closed/重复项并准备全部暂存, 再统一消耗状态并委托一次. 拒绝或自身预检分配失败保留全部 Closed, 后项失败不消耗前项; 委托后的 0/异常不恢复重放. MSVC Debug 的容器构造也纳入分配失败边界, 空 batch 按来源直接返回 0. immediate claim 以原子 CAS 获取, 关闭/析构只释放一次; 底层列表先于父包装释放, 不补执行. BitSetAllocator 的容量检查放在 vector 成员的构造参数求值中, 先于其 Debug bookkeeping/数据存储; 私有数组比较使用 size_t 遍历, 不截断逻辑序列长度.

完整理由与适配边界见 [通用层 design](../openspec/changes/archive/2026-10-26-establish-rhi-common-validation/design.md), 实际结果见其 tasks. 不将本表的 CPU/metadata 合同解释为原生支持或 GPU 完成.

Tracker 的两张 map 按需在 optional 中构造, Reset 销毁它们并清空/释放 vector 容量. 这避免 MSVC Debug 空容器 swap 的清理分配; 故障注入覆盖 Reset 和 Submitted 的无分配清理, 未推广到其他标准库.

Tracker 的四个空 vector 以 count=0 构造, 避免当前 MSVC Debug 默认 noexcept 构造在 proxy OOM 时终止, 使 native Open 的 owner 可捕获失败. 不改动通用状态算法或公共数据布局; 相关差异与验收见上传 change 的 design/tasks.

View 检查保留来源已有的 typeless stencil SRV 对: D24S8→X24G8_UINT 和 D32S8→X32G8_UINT, 依据为同快照的 common/dxgi-format.cpp 与 vulkan-texture.cpp. 不把 stencil-only metadata 误判成深度布局不兼容; 格式/采样能力仍查询底层, 错误对、非 typeless 和其他 view 用途不放宽. T03 已原生验证本包单层附件 view 的兼容性; stencil SRV 导出与内容访问未启用, 不由通用 metadata 检查推导为已支持.

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
| Device 纯虚方法 | OpenGL/Backend.hpp 及其 cpp 的完整 68 项闭包, 已实现资源与明确拒绝见上表 |
| 即时 CommandList | OpenGL/CommandList*.cpp 的 61 项闭包; 正向子集与明确拒绝见上表 |
| 其它未交付对象、AftermathCrashDumpHelper | 后续实际执行/可选能力工作包; 当前工厂拒绝, 不提供占位实例 |

ScopedMarker 和 descriptor 的头内转发仍需满足其调用对象/外部定义前提, 不因函数体在头里就保证所有使用方式可链接. 公共头不依赖 Core、Platform、Editor、GLFW、GLAD、ImGui 或 Vulkan SDK. 构建/运行说明集中在 [Build](../Docs/Build.md), 当前工作包状态见 [Developments](../Docs/Developments/D00-D02-Foundation.md#d02-t02).
