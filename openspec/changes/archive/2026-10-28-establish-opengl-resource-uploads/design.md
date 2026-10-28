# Design

## Context

动机和工作包范围见 [proposal](proposal.md). 当前 Git 基线为 `4ed14e0b4acad66a68a4f278d819b93a4a3c9f91`, D02-T03 已提交并归档. 已核对 Architecture 7.1-7.7/11.1/11.5、Developments D01-D03、五个主规格的能力目录及三个相关主规格全文, 并阅读正式 Device/资源、Validation 列表/提交、Common tracker、Host 关闭与现有 native fixture.

当前 Device 的 68 个纯虚方法中没有可用列表/提交/WaitForIdle; 公共 CommandList 有 61 个纯虚方法, WriteBuffer/WriteTexture 不是 Device 方法. Validation 已约束 Graphics/immediate、整批预检、关闭后提交与一次消耗. 当前 native wrapper 持有 Device, Host 以 validated==1/raw==2 判断没有外部 owner. Device 自己保活上传资源后, 该 raw==2 条件不能继续直接使用.

新要求见 [上传规格](specs/opengl-resource-uploads/spec.md); resource-only 和 Host 的变化见另两份 delta. 下列名称均为本包拟实现的私有类型/互操作入口, 不描述为现有能力.

## Goals / Non-Goals

**Goals:** 在同一正式 RHI/Validation/Host 上建立有界初始上传和真实完成闭环; 源指针只借用到 Write 返回; 未提交即时工作、失败和退出与成功路径具有同等完整的资源保活. 保持 generic/native 依赖方向、来源谱系和既有资源分配语义.

**Non-Goals:** Shader/Binding/Pipeline、clear/Draw/Dispatch、公开 GPU copy/staging/readback API、Event/TimerQuery、volatile/push constants、永久状态、完整 GPU shader-write barrier lowering、RenderGraph、窗口复制/呈现、全 T05/D03 验收和性能结论. 深度/模板、MS、压缩及未映射格式仍可保留既有分配支持, 但不加入本包内容写入.

## Decisions

### 1. 阶段入口与工作包衔接

本次 apply 核验 D01 原 change 仍为 15/16、4.1 未勾选且仅查询到 1 个活动显示器, 原阶段出口未通过. 维护者随后明确选择“同意限定推进，修订规划后继续（推荐）”, 允许本 change 的 T04 初始上传、必要即时列表/fence/在途保活/Host 关闭与 T05 最小读回先实施. 这是本包新增的明确例外, 不是从 planning ready 或 T01/T02/T03 授权推导.

原因是上述设备内容/完成协议不消费跨屏比例, 宿主和输入影响仍按本 tasks 全量回归; 单屏或注入值仍不能代替 D01 真实跨屏. 例外不覆盖完整 Shader/Draw、通用读回/呈现、T05/D03 父包或 D01/D02 总出口. 任务 1.1 验证本次授权已同步五类文档后才允许进入后续实现; D01 验收补足并通过出口后恢复普通前置.

D02-T04 承担上传与必要列表/完成基础; T05 只提供本包数据验证, D03-T03/T04 后续沿同一列表/提交对象扩充绘制、通用访问和其它异步消费者. Developments 记录这次依赖细化, 父任务及 D01/D02 出口不提前完成. Architecture 的即时模型、完成票据和上传方案本身不变.

**备选:** 只提供 backend 逃生写入函数会绕过公共列表/Validation; 推迟全部上传到 D03 会使 D02 出口依赖后续阶段. 采用可独立验收的上传限定子集, 不另建临时提交系统.

### 2. 接口闭包与文件职责

PhysaraRHI 仍独立消费六公共头/Common/Validation, PhysaraRHIOpenGL PUBLIC RHI、PRIVATE glad, Platform PRIVATE native. 公共 RHI 字段/值/布局不改, 不引入窗口/Core 到 native 或 reference 到构建. 全部 61 个 CommandList 方法及 68 个 Device 方法建立逐项正向/拒绝矩阵, 包括重载及继承的可选查询.

| 文件/私有合同 | 本包职责 |
|---|---|
| RHI/OpenGL/CommandList.hpp/.cpp | OpenGLCommandList 的 CPU 生命周期、唯一 recording claim、只读描述与入口校验 |
| RHI/OpenGL/CommandListUpload.cpp | WriteBuffer/WriteTexture 的检查、准备和实际发射 |
| RHI/OpenGL/CommandListState.cpp | 本包支持的 Common tracker 入口与依赖消费 |
| RHI/OpenGL/CommandListUnsupported.cpp | 未交付命令的明确失败, 不读无关 payload |
| RHI/OpenGL/Upload.hpp/.cpp | 有界 persistent coherent chunk、行复制及包内分配 |
| RHI/OpenGL/Submission.hpp/.cpp | Device 所有的工作包、fence/ID、完成/退出及内部引用记账 |
| RHI/OpenGL/ContextState.hpp/.cpp | 实际需要的 unpack/PBO 原生状态缓存, 无空的绘制状态 |
| Backend.hpp、Device.cpp、Formats.cpp、Buffer.cpp、Texture.cpp | 原生入口检查、设备阶段、资源状态扩展、像素映射及删除衔接 |
| OpenGL.hpp、Platform/RhiDeviceHost.cpp/GlfwWindow.cpp/DeviceHost.cpp/.hpp | 无 GL 类型的关闭互操作、可重试 drain 和公开寿命说明 |

CommandList 的正向子集为 Open/Close/ClearState、WriteBuffer/WriteTexture、SetEnableAutomaticBarriers、BeginTracking/Set/Get 的 Buffer/Texture 状态、CommitBarriers、GetDevice/GetDesc. SetPermanent*、UAV 配置、公开 copy/clear/markers/queries、所有绘制及扩展明确 Unsupported. GetNativeObject 返回不可用, 不新增 GLuint 导出. Device 新增正向路径是 CreateCommandList、ExecuteCommandLists、WaitForIdle, RunGarbageCollection 扩展为先收集完成、再排空既有删除.

初始列表只接受 enableImmediateExecution=true、queueType=Graphics、lifetimeTracker=nullptr; uploadChunkSize 必须为正且对齐后的最低块容量不超过本包预算. scratch 字段按描述保留, 不分配无消费者的 scratch. 27 个 Feature 的原生结果继续全部 false/output 不变, 不用上传能力冒充 shader/range/扩展支持.

**备选:** 一个巨型 CommandList.cpp 隐藏不同寿命责任; 把全部功能都声明为可用会制造成功空操作. 这里按真实状态/消费关系分文件, 不添加匿名域或新的 Detail/Internal 收纳域.

### 3. 即时列表、物理包和跨线程最终释放

逻辑列表自持 DeviceHandle、参数和当前 recording 身份. Device 从 Open 起统一拥有物理 UploadWorkPacket; 列表独占写当前包, 只借用包身份. 包自持 tracker、目标资源强引用、chunk 和创建前取得的退休链接, 不持有逻辑列表. 这样列表消失后, 已发出的工作仍有明确 owner.

| 状态 | 可做的动作 | 退出条件 |
|---|---|---|
| Initial / SubmittedDetached | Open | CPU 包/容量准备成功, 获得唯一 claim |
| Recording | 支持的状态/写入、Close | 本包结束录制, 不意味着提交/完成 |
| Closed | 一次 Execute 或释放列表 | 整批成功后 detached; 放弃则交给 owner 退休 |
| Orphaned / FaultedRecording | 仅内部退休 | 未发 GL 的包 CPU 清理; 已发前缀以真实完成或失效退出收拢 |
| Submitted | Device 持有, 不允许重放 | tracking fence Complete 才释放 |

RecordingId 和 SubmissionId 分别使用非零、检查溢出的 uint64, 不使用 CPU 帧号. Device 的唯一 claim 记录 RecordingId; 旧列表释放通过 compare/exchange 只撤销自己的 claim, 不影响后来记录. 参数/数据方法单写者, 原生方法限 owner/current. 最终列表 Release 可以来自已停止的 worker: 先以默认 seq_cst compare/exchange 释放自己的 claim, 再以 release store 发布 orphaned; owner 用 acquire load 接收, worker 发布后不再访问包. 不新分配、不 GL、不修改包容器或销毁包内强引用, 容器/记账及回收仅由 owner 推进. 交接没有重试循环或 GPU 等待, 不对标准库原子实现作额外性能保证. GL/回调/等待均不在删除 FIFO 的锁内执行.

原生 Open 的 void 失败不能通过公共 API 改成 bool. CPU 准备或容量失败时 native list 保持不可录制并报告; Validation 即便已取得其 CPU claim, 后续 native 入口也须拒绝直到 Close/析构释放包装 claim. 验证 Open 故障后的安全 Close/释放/重新打开, 不仅检查前端状态.

**备选:** 让析构调用 Execute 会制造提交和 worker GL; 把工作随列表直接销毁会使已发 GPU 访问悬空. Device 拥有有限物理包提供唯一退出路径.

### 4. 有界预算与 chunk 策略

以下是本包明确的初始策略, 不是设备能力或测得的最佳值:

| 项目 | 初始值/处理 |
|---|---|
| Device 所有 mapped upload 存储 | 64 MiB 总上限, 按实际分配容量计费 |
| 未退休的物理工作包 | 最多 64, 包括 recording/closed/submitted/orphaned |
| 每包非空写入 | 最多 4096; 状态涉及的不同资源保活项也最多 4096 |
| 小 Buffer 客户端更新 | <=256 KiB 使用 glNamedBufferSubData |
| 更大 Buffer / 所有 Texture | persistent coherent chunk + GPU copy / PBO |
| chunk 最小容量 | 参数 uploadChunkSize, 默认源值 64 KiB; 按实际 mapping alignment 检查并上调对齐 |
| 显式 idle / 正常 shutdown 等待 | steady_clock 的 5 秒期限, 单次 client wait 最多 1 ms |

Device 工厂查询并验证 GL_MIN_MAP_BUFFER_ALIGNMENT 为正的可用二次幂. 大小/乘法/对齐/GLsizeiptr/offset+size 先以剩余容量比较证明可表示. 块容量取已检查的 max(最小块, 本次紧凑数据), 不超过 Device 剩余预算. 一个 chunk 只属于一个工作包, 包内 bump 分配; 不跨包共享同一 mapped 可写区间. 每包计费目标逻辑字节最多 64 MiB, 包内资源/块元数据也受上述计数上限约束.

包/退休记录在 Open 前准备; bounded vectors 的必要容量在使用前准备. 当前操作的 CPU 资源引用、tracker、chunk 描述及容量均在其目标写入前取得. 预算不足立即报告请求量/保留量/上限, 不隐式等 GPU、覆写 pending 区间或无限增长. 调用方可显式 RunGarbageCollection 后重试. 未发命令的准备可撤销, 已发前缀继续保活.

chunk 使用 glCreateBuffers/glNamedBufferStorage 和 glMapNamedBufferRange, storage/map 都声明 GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT. 映射空指针或 GL 错误均失败. 数据先 memcpy 到 owned mapping, 再发 server copy; coherent 只保证后继命令可见, 不允许覆盖 GPU 仍在读的区段. 完成收集后 unmap/delete 该包全部 chunk 并归还预算, 初版不保留 idle chunk cache. 依据 [ARB_buffer_storage 的 coherent 可见性合同](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_buffer_storage.txt), 生存期仍由本包 fence 保护.

**备选:** 全部调用 glFinish 会把上传变成隐式全设备等待; 无预算 chunk 池会无限保留. 采用每包 chunk 和有限记账, 暂不加入有测量前提的大小自适应策略或跨包池.

### 5. Buffer 写入与源数据边界

接受 T03 普通非 volatile、非 cpu-mapped/virtual/shared/RT/typed-view buffer; structured/raw 布局不改变字节写入语义. 先检查实际动态类型及 Owner, 再验证 offset<=capacity、size<=capacity-offset、非空 source 和 native 有符号表示范围. size==0 只接受合法位置, 不把 0 解释为 whole/remaining; 在分配/GL 首错查询前返回.

小更新在所有 CPU 准备后调用 glNamedBufferSubData; T03 已有 GL_DYNAMIC_STORAGE_BIT, 保持 immutable allocation. 大更新把源复制到本包 chunk, 再 glCopyNamedBufferSubData 到目标区间. 两条路径均在 Write 返回前结束对 caller source 的借用. 256 KiB 只是当前策略, 不从 Vulkan 的 64 KiB API 限额推导 GL 上限. 测试覆盖 64 KiB+1、策略两侧和非零偏移.

源数组实际可读范围仍是调用方的指针合同, backend 不能从 const void* 得知分配长度. 只有已初始化区间才可读回比较; 检查“不影响其它字节”前先用已知数据初始化那些字节.

**备选:** 保存 source 指针到 Close/Execute 会破坏公共即时复制合同; 从 mapped CPU 源直接延后使用没有 owner. 两条实际路径都在调用内消费来源.

### 6. Texture 格式、维度和 footprint

仅接受 T03 中已实际支持的下列 13 种颜色存储; resource caps 仍按实际 target 检查. allocation 支持不等于 upload 支持. 四种 depth/stencil 格式本包不定义 CPU packed 数据解释, MS 不提供 tex-subimage 写入, 其余压缩/排除格式不加近似转换.

| RHI 格式 | external format / type |
|---|---|
| R8_UNORM、RG8_UNORM、RGBA8_UNORM、SRGBA8_UNORM | RED/RG/RGBA, UNSIGNED_BYTE |
| R8_UINT、RGBA8_UINT | RED_INTEGER/RGBA_INTEGER, UNSIGNED_BYTE |
| R16_FLOAT、RG16_FLOAT、RGBA16_FLOAT | RED/RG/RGBA, HALF_FLOAT |
| R32_UINT | RED_INTEGER, UNSIGNED_INT |
| R32_FLOAT、RG32_FLOAT、RGBA32_FLOAT | RED/RG/RGBA, FLOAT |

在 Formats.cpp 既有 FormatMapping 记录增加 uploadFormat/uploadType, 排除写入的项保留 0, 不复制另一份存储支持矩阵. 使用每种表示可准确比较的有限测试值; 不以任意 NaN payload/浮点 padding 的位相等作为跨驱动承诺. SRGB8 上传的是调用方已编码的字节, 不额外编码/解码; swizzle 留在采样状态.

| 公开维度 | 拟用 DSA 子图命令 |
|---|---|
| Texture1D | TextureSubImage1D, width=mipWidth |
| Texture1DArray | TextureSubImage2D, yoffset=arraySlice, height=1 |
| Texture2D | TextureSubImage2D, width/height 为 mip 尺寸 |
| Texture2DArray、TextureCube、TextureCubeArray | TextureSubImage3D, zoffset=arraySlice, depth=1 |
| Texture3D | TextureSubImage3D, zoffset=0, depth=mipDepth, arraySlice=0 |

cube face 次序沿已有六个 face/layer 索引, 不缩小数组数. Cube 的 DSA 三维入口依据 [OpenGL 4.6 Core 8.6、表 8.15](https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf), 不把旧 bind-to-edit target 限制套到 DSA. 全部实际 target/入口均须 native 验证.

CPU 检查 mip<desc.mipLevels 且可安全 shift, 得到非零 w/h/d. 每个颜色 format 的 blockSize=1, rowBytes=w*bytesPerBlock. rowStride=rowPitch!=0?rowPitch:rowBytes, rowPitch==0 只在 h==1 接受. sliceBytes=(h-1)*rowStride+rowBytes 检查乘加; d>1 时 depthPitch>=sliceBytes 且 (d-1)*depthPitch+sliceBytes 可表示. d==1 时不消费 depthPitch, 不对无关参数做乘法. 紧凑 staging 大小 w*h*d*bytesPerBlock 另行检查并计预算. 逐行/逐 slice 复制 rowBytes, 不读取 padding, 不要求 padding 除以 texel 大小整除.

**备选:** 直接把任意 rowPitch 除为 GL_UNPACK_ROW_LENGTH 会截断非整 texel padding; 统一翻转会改写约定. CPU repack 到紧凑 owned chunk 保持精确 footprint 和原始行序.

### 7. 唯一 unpack/PBO 原生状态

ContextState 本包只缓存实际消费的 GL_PIXEL_UNPACK_BUFFER 及 GL_UNPACK_ALIGNMENT、GL_UNPACK_ROW_LENGTH、GL_UNPACK_IMAGE_HEIGHT、GL_UNPACK_SKIP_PIXELS、GL_UNPACK_SKIP_ROWS、GL_UNPACK_SKIP_IMAGES、GL_UNPACK_SWAP_BYTES、GL_UNPACK_LSB_FIRST. 首次写入设为 alignment=1、其余布局项 0/false, PBO 为当前 owned chunk. 命令中的 data 参数是已检查可表示的 PBO byte offset, 不是 caller 指针. DSA 不会绕过 pixel-store/PBO context state.

所有设置走同一 cache, 一次固定 unpack/PBO 布局建立作为原生操作组, 随后按既有首错合同检查; 失败终止当前目标写入并锁存设备状态. ClearState 失效这些条目, 下一写重新建立所需状态; 直接原生互操作后调用方必须 ClearState. 不 glGet 保存/恢复状态, 不同时在 Upload 和 CommandList 各维护一份缓存. 当前没有 Program/VAO 等消费者, 不预建其空字段.

删除 chunk 或 buffer 名之前使相关 binding cache 失效, 不能让 GLuint 重用命中旧条目. fixture 可通过私有边界布置 dirty unpack/PBO 状态, 随后走公开 ClearState/Write 检查恢复; PACK 读回状态由 fixture 自己设置, 不混入产品 cache.

**备选:** 每次上传 snapshot/restore 会隐含热路径查询并遮蔽 context 所有权; 假设默认 unpack 状态会在有 padding/互操作时写错. 使用有真实消费关系的窄 cache.

### 8. 复用 Common tracker, 限定 transfer 状态

Open 分配失败验收发现当前 MSVC Debug 的默认空 vector 构造为 noexcept, 调试 proxy 分配失败会直接 terminate. tracker 的四个 vector 和新工作包的两个 vector 使用 count=0 的可抛构造, 让既有 owner 的 bad_alloc 边界捕获失败. 这是构造失败协议修正, 不修改通用状态/永久状态算法; generic 与 native 的 Debug/Release OOM 回归同时验证.

Buffer/Texture 私有 wrapper 各增加已有 BufferStateExtension/TextureStateExtension, descRef 指向自持且不可变的 desc. 工作包拥有一个 CommandListResourceStateTracker, 资源 lease 覆盖其全部借用. BeginTracking/Set/Get 的类型、Owner、范围和值在 Release 同样检查.

初始正向状态限 Common、CopyDest 和 keepInitialState 所需的原始 ordinary initialState 恢复; 后者仍是元数据, 不启用 shader/RT 等执行. SetPermanent*、UAV/BindingSet 状态入口明确拒绝, 不因此改动通用永久状态算法. 自动模式为每个写入请求 CopyDest 并消费抽象依赖; 手动模式要求调用方已请求并 Commit 相应 transfer 状态. Unknown 或 tracker sticky error 拒绝受影响发射, 将已有前缀留作退休.

当前真实生产者只有普通 DSA/PBO/copy 上传, 无 shader/image/UAV 写入. 这些 ordered transfer 的依赖按 GL 的顺序语义消费, 不为了“有 barrier”调用全局 GL_ALL_BARRIER_BITS. 本包不支持需要 shader-write lowering 的新消费者状态. Close 形成 KeepInitialStates 所需恢复并消费已验证依赖; BeginTracking 只声明, 不冒充实际转换.

只有成功 fence+flush 的 Execute 才调用 tracker.CommandListSubmitted. 支持子集不产生 deferred permanent-state 更新, desc 稳定且已通过预检, 复用 T02 已验证的无清理分配协议, 不改 generic 算法. Orphan/failed 包只 Reset, 不发布 stateInitialized. stateInitialized 仍表示跟踪协议, 不是整个纹理内容有效标记.

**备选:** 另写一个 upload dirty/state 表会形成两份权威. 扩充全部 shader barrier 则没有本包消费者. 使用已交付 tracker 并限制实际执行 profile.

### 9. 整批提交与真实票据

Execute 的 CPU 预检覆盖 count<=64、Graphics、所有指针存储前提、实际 native 类型、Owner、Closed/未消耗、无重复、tracker 无错误、无 global open recording、Device Active 和 ID 可递增. Validation 已有同类包装预检仍保留; raw 入口也自行保证 native 安全. 后项非法不消费前项. 多个 Closed 包的 GL 顺序由录制时的实际调用决定, Execute 的数组次序不能重新排列已发生的更新.

这里的“不消费”按各自边界解释: wrapper 在其预检失败前保留全部包装状态, native 在其预检失败前保留 native 包状态. 现有 ValidationDevice.cpp 在委托前统一消耗包装状态, 委托后的 0/异常不恢复 Closed; 该 T02 已采纳合同保持. 若 wrapper 已委托而 native 预检拒绝, 不能承诺原 validated list 仍可按 Closed 重试. 调用方释放该 logical list, 由原生 orphan 协议保留/退休前缀, 需要重试时建立新的 recording; 不自动 Open 旧包或重放. 同时验证前端预检失败、raw 预检失败、委托后 native 失败三种结果.

完成记录和包集合在 glFenceSync 前全部准备, 不在工作已发出后依赖新 CPU 分配. 非空有效批次建立一枚 SYNC_GPU_COMMANDS_COMPLETE fence, 检查非空/实际错误, glFlush 成功后才发布一个递增 SubmissionId, 将所有包标成已消耗并发布各自 tracker metadata. Execute 不调用任何 Write/Copy 重放. 空 batch 返回 0, 不生成 fence. 已消费 recording 再执行为失败, 之后 Open 创建新 recording.

fence/flush 失败返回 0, 不消费为可重放状态: 包进入保留/失败退休状态, 公共元数据不发布, Device 停止新工作. 已返回的 native fence 只在可验证可用时清理; 无可靠 fence 的引用留到受控退出, 不作为 Pending 正常队列伪装成功.

**备选:** 先发 ID 再原生提交会产生假票据; 逐项消费 batch 会损坏可重试的前项. 采用整批 CPU 预检和原生成功后的无分配发布.

### 10. 完成收集与显式等待

Device 维护按 fence 发射次序的有限退休记录. 正常已提交组有 public ID; orphan 的内部退休 fence 没有 public ID. RunGarbageCollection 首先把无工作 orphan 作 CPU 清理, 为可用 context 上的已发 orphan 建立内部 fence/flush, 然后对最老 fence 使用 client-wait flags=0、timeout=0. Complete 释放对应包/chunk/lease 并更新适用的 completedId; Pending 立即返回; Failed 锁存失败并保留所有未证明引用. 之后才排空已安全到达 final Release 的 T03 删除 FIFO.

上述发 fence/正常收集只适用于可靠的 Active 路径. fence/flush/wait 失败使 Device Faulted 后, 普通 GC 不把该失败包重新解释为正常 Pending/Complete 或尝试补发公开票据; 交第 12/13 节的受控退出收拢. Draining 使用同一完成/退休实现, 但由显式 drain 入口推进等待, 不从普通 GC 隐式进入阻塞退出.

一次显式 collector/drain 调用的全部 fence 共享一个 steady_clock 截止点, 不能在前项完成后给下一项重置五秒预算; 每次 client wait 取剩余时间与 1 ms 的较小值, 到期不再等待. Collector 或退休过程中若确认 context 不可用, 退出阶段重新核对实际健康状态, 后续不再发 finish/delete/unmap/poll; 只有仍可用且缺少可靠完成证明时才允许退出专用 finish. GL 驱动调用本身不具可中断的硬实时保证.

ALREADY_SIGNALED/CONDITION_SATISFIED、TIMEOUT_EXPIRED、WAIT_FAILED 的区别依据 [Khronos ClientWaitSync](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glClientWaitSync.xhtml). 对 Failed 查询同时保留实际 GL error, 不把失败当无限 pending. 任何 DeleteSync/unmap/delete 错误继续遵守首错/失效上下文合同.

WaitForIdle 是调用方显式选择的冷路径. 在当前命令尾建立并 flush 一枚临时真实 fence, 以单调 5 秒期限等待. 只有 Complete 才返回 true 并收集该点之前已接受提交的包; 仍被逻辑 recording 拥有的 pending 区段即便物理完成也不复用/发布. Timeout 返回 false、保留工作且不推断 device lost; 清理临时 fence 不释放包. Failed 停止新工作, 交受控退出. 普通 Write/Execute/GC 不阻塞等待, 不调用 glFinish.

**备选:** 固定帧延迟、最后提交 ID 或非零 GLsync 都不是完成证明. 此处区分提交、物理完成和 pending recording 的拥有权.

### 11. 内部 lease 与外部 owner 的零分配预检

每个 packet 的资源保活由私有 move-only UploadResourceLease 表达: 一个现有 intrusive 强引用和它对该 native Buffer/Texture 的内部持有记账, 不新建公共 owning raw pointer. 同一包可按实际引用去重, 跨包的每个内部持有均精确计数. 资源有 m_InternalUploadHolds; 0->1/1->0 时 Device 更新内部唯一 resource-wrapper 数. 这些派生计数只在 owner 的 lease 取得/释放处改变, 来源就是实际强引用, 不是另一份可变资源身份表.

关闭调用方先停止并 join 用户. Host validated.GetRefCount()==1 必须成立. 对 rawDevice 的已知 root 数 K, 纯 CPU 预检要求:

```text
每个被内部包持有的 resource.GetRefCount() == resource.InternalUploadHolds
rawDevice.GetRefCount() == K + internallyRetainedUniqueResourceWrappers
K = 2 (Host raw + Validation 的 raw) 或 1 (原生工厂独立 root)
```

Packet 的内部资源项受数量上限约束, 可以直接遍历; 重复项只重复比较, 不必分配 map 或全资源 registry. 未被包保活的外部 wrapper、live logical list、额外 raw/Validation owner 都会使关系不成立. FBO/Sampler 不参与上传保活, 仍按外部 owner 拒绝. 外来资源的归属仍直接检查其 native Owner, 计数不代替归属.

Device->packet->resource->Device 是在途期间明确存在的寿命环, 由完成/退出释放 lease 打破; 根 owner 必须保留到显式 Shutdown 完成. 包和 chunk 不另持有 DeviceHandle, 不把正常退出交给“最后一次引用也许会发生”. 预检不分配、不 GL、不改 Ready 或包状态; Release worker 只排交已有包, lease 的销毁及计数改变留在 owner.

**备选:** 沿用 raw==2 会把正常内部保活误报 ResourceInUse; 为每个活对象建地址 registry 或临时分配统计 map 没有必要. 精确内部 lease 为真实所有权提供可验证来源, 还可覆盖 OOM 关闭.

### 12. 放弃、部分失败与无 fence 工作

在每个可能发出目标写入/拷贝的 native 调用前先标记 issuedWork, 因为失败结果不证明它未被驱动消费. CPU 参数/预算拒绝保持当前写未应用, 先前成功操作仍被保留; tracker 失败则使整个 recording 不可正常提交. 原生错误沿 T03 NativeFailure 停止新工作, 不能在 Report severity 中推断 context loss.

释放无 issuedWork 的包在 owner Reset tracker、释放 lease/预算即可. 释放带 issuedWork 的列表只将其标成 orphan, owner 在 context 可用时发内部退休 fence, 不发 public ticket、不 CommandListSubmitted、不回放内容. 若本次失败导致可靠 fence 不存在, 保留相同的 packet/chunk/lease 到退出. 已 Close 但未提交、worker 最后释放和 API 中途故障走同一协议.

正常 fence Pending 只待完成; Failed/无 fence 在可用 context 的受控退出才允许一次 glFinish 收拢. 成功确认后仅按退休释放, 无 public ID 的工作不会反向发布成功 metadata. glFinish 自身可能阻塞驱动, 本包不承诺该灾难退出调用具有可取消的硬实时上限. 若它再次失败或无法确认可用 context, 明确转 unavailable-context 最终退出; 不声称硬件 reset 或推进 completedId.

**备选:** 仅 Reset tracker 无法撤销已发 GL; 在异常时立即销毁 mapped chunk 同样不安全. 保留有界失败包, 正常路径不使用 exit-only 等待.

### 13. Host 关闭事务与 native 互操作

OpenGL.hpp 拟增加无 GL/GLFW/Core 类型的窄入口: CheckShutdownPreconditions(device, knownRootReferences) 返回 Ready/ResourceInUse/InvalidState; DrainDeviceWork(device, mode, nativeError) 返回 Ready/Pending/InvalidState. 前者纯 CPU、无副作用且只接受已说明的 root 关系; 后者仅在已收拢用户且预检通过后推进真实工作收拢. 现有 ShutdownDevice 保留独立工厂 root 的预检和最终 Stopped/CPU-only 析构责任, 失败时调用方保留 root 供重试或最终 context-unavailable 退出.

| Host 关闭步骤/结果 | 必须保留或释放的内容 |
|---|---|
| validated 与 native 内部 lease 预检失败 | ResourceInUse/InvalidState, 原 Ready、输入、context、callback、claim 不变 |
| 预检成功, 恢复所属 current | 恢复失败进入明确 ContextUnavailable 退出, 不假称设备已丢失 |
| 开始 Drain | Device 进入 Draining, Host ready=false, GetDevice/输入入口停止接新工作 |
| 有效 fence 到期仍 Pending | 返回新追加的 HostErrorReason::RhiDrainPending, stage=RhiShutdown; 保留 validated/raw、输入存储/回调、context/window/GLFW/process claim, 下次 Shutdown 继续 |
| 实际完成或合法 unavailable-context CPU 清理就绪 | Reset tracker/包/chunk/lease, 排空安全退休; 再按 Validation -> raw -> adapter -> 输入 -> context/window -> GLFW/claim 释放 |
| 已停止 | 幂等, 旧 owner 不影响后建 host |

RhiDrainPending 只追加枚举项, 不重排既有值. ResourceInUse 是尚未进入 drain 的可恢复拒绝; Pending 是已停止新工作但没有完成退出. 超时不是 GL error, 不伪造 nativeCode 或锁存为硬件 loss. 真正 native 首错和 cleanup 次错仍按既有 bounded error 合并. Drain Ready 带 native 错误时继续最终释放并返回原错; Pending 时绝不能先 Reset Validation/raw 再销毁 context.

ShutdownDevice 的旧“开始清理后一律 Stopped”说明须更新: GPU drain Pending 保留 Draining; 已确认可以退休并开始最终删除之后, 即使删除带错也按 T03 进入 Stopped、由 owner 最终释放 context. 对已知不可用 context 不 unmap/delete/wait, 仅撤销 CPU 记录, mapped 指针不再访问, native storage 由最终 context 销毁退出. Device 析构要求 Stopped、包/预算/删除记录全空. Host 析构若 Shutdown 后仍未 stopped, 沿既有 TerminateForContract 处理, 文案同时覆盖外部 owner 和未完成 drain.

**备选:** Host 在 raw 引用不等于 2 时直接拒绝无法关闭正常在途上传; false 一律继续 teardown 则会提前释放. 显式预检/收拢/最终释放分阶段保留现有根拥有关系.

### 14. 验证、来源与文档交付

职责评审: Backend.hpp 保留完整 68 项 Device 合同及其原生 wrapper, 不按行数复制另一份接口; 列表、上传布局/存储、窄 unpack cache、提交/退休各有实际状态与消费者. Execute、WriteTexture 和 DrainWork 超过普通函数的评审参考行数, 分别维持整批预检/发布、完整子资源准备/发射和关闭分阶段的单一不变量; footprint 与分配算法已独立, 不为减少行数新增转发 helper 或空 Detail 域. 没有新增长期 cache, 上传完成即释放.

主 fixture 仍使用同一正式 Host/Validation/backend, 不在产品增加测试 CLI 或第二个 backend. Native 最小读回限 fixture: 实际完成后 glGetNamedBufferSubData、glGetTextureSubImage, 后者有明确 bufSize 和 PACK 状态; 不新增业务 readback/staging 对象. 非对称 13 格式/7 维度数据覆盖 mip、array/cube face、3D depth、padding、编码和 source 提前销毁. 不读未初始化内容, float 使用已知可表示值, 对整数/已编码 byte 作精确比较.

检查构建保留真实 GL 调用观测与受控结果 hook, 不改生产 API 结果. GPU 太快不能保证自然观测 Pending, 故正常完整上传/完成/读回为真 native 证据, Pending/WAIT_FAILED/零 fence/flush error/预算边界的注入逐项标明. 分别覆盖零 destination update、已取得真实 chunk 后失败、部分已发前缀、worker 零分配/零 GL、外部/内部引用组合、shutdown timeout/retry、loss 与 exit-only finish. 辅助 foreign Device/context 复用 T03 已明确的入口地址比对和 current 恢复协议, 不重载全局 GLAD.

工厂在 T03 必需入口上新增检查 glNamedBufferSubData、glCopyNamedBufferSubData、glMapNamedBufferRange、glUnmapNamedBuffer、glBindBuffer、glPixelStorei、glTextureSubImage1D、glTextureSubImage2D、glTextureSubImage3D、glFenceSync、glClientWaitSync、glDeleteSync、glIsSync、glFlush、glFinish, 同时查询 mapping alignment. glIsSync 用于带错 native fence 的合法性确认, 不从一个非空句柄推断其有效. 读回专用入口只由 fixture 检查. fixed glad 声明必须逐项核对, 不从头中存在即声称实际支持. 来源依据还包括 [NVRHI 分析的上传与在途记录](../../../../Docs/Reference/NVRHI_RHI_Vulkan_Analysis.md#n6) 和 [固定来源的 Vulkan 上传实现](../../../../ThirdParty/nvrhi/src/vulkan/vulkan-upload.cpp), 其 64 KiB/Vulkan memory-coherency 细节不直接移植为 GL 约束.

Debug/Release 均执行正式 native 路径和相关 generic CPU/负例、Core/输入/Host/Runtime 回归、独立消费者、61/68 方法闭包、来源/许可/风格/依赖核对. 按任务记录实际命令、配置、环境、源标识与原始输出到 ignored build/artifacts/establish-opengl-resource-uploads/, 尚未运行的 C++/GPU 项保持未完成. 不增加 CI、测试框架、格式门槛或性能报告.

实施随对应任务更新 native header 的线程/源寿命、RHI/README 的支持矩阵/来源差异、CMake 实际源归属和 Build 实际入口; Developments 记工作包与前置. Architecture、历史归档和主规格在规划期保持不变. 归档时再按原生 sync/archive 合并 delta.

**备选:** CPU double 不能证明 PBO/真实 fence; 同步 fixture 读回也不能代替生产完成协议. 两种证据分开, 所有适用验收实际通过才勾选任务.

## Risks / Trade-offs

- [D01 前置仍不满足] -> 首任务按本次已明确批准的有限入口核验, 保留真实跨屏与原阶段出口缺口, 其它阶段不连带放行.
- [本包提前建立部分列表/提交基础] -> Developments 明确 T04 与 D03 的共享子集, 始终扩展同一 Device/列表, 不并行复制临时实现.
- [driver 内部内存不受 64 MiB 预算直接控制] -> 预算只声明本项目 owned upload bytes, 元数据/操作数另设上限, 不把它描述为全驱动显存上限.
- [coherent mapping 易被误认为可随时重写] -> chunk 独属于未退休物理包, Complete 前不复用或 unmap/delete.
- [复杂在途引用形成暂时寿命环] -> 根 owner 持续存在到 Shutdown, lease 来源单一、预检零分配, 完成/退出逐项打破环并验证无残留.
- [正常 Pending 与错误混淆] -> 零超时 GC、有限显式等待、独立 RhiDrainPending, 只有 Complete 发布完成; 错误不推断 loss.
- [glFinish 灾难退出可能长时间阻塞] -> 仅用于可用 context 的无可靠 fence/Failed 收拢, 正常上传/GC 不调用, 不给出硬实时承诺.
- [深度/模板及完整 rendering 未交付] -> 初始 profile 明确拒绝并保留分配能力, T05/D03 继续独立验收.

## Migration Plan

1. 验证实施入口, 保存当前五个主规格、源码、依赖与工具版本标识; 入口不满足时停止实施.
2. 先加入列表/物理包/lease/Unsupported 闭包与 CPU/首 include 检查, 再建立真实 fence 收集及 orphan/关闭基本退出, 使任何后续上传都有安全寿命.
3. 在同一基础上实现 bounded chunk、Buffer、Texture 和窄 ContextState/transfer tracker; 每组同时接入 native 正反证据.
4. 完整接入 Host drain/重试, 扩大故障/在途/worker/消费者回归, 更新实际支持/来源/构建说明.
5. 核对全部 requirements/scenarios、D01 入口及保留边界, 原生 verify/strict 通过后再同步/归档. 不提交 Git 或修改历史来掩盖失败; 如需回退未交付实现, 同一逻辑范围恢复 resource-only 入口/支持声明与工作包进度, 保留原始失败证据.
