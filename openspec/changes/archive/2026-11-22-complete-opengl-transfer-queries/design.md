# Design

## Context

Graphics 即时列表已经在录制时发出二维 clear/copy。D08-T02 的物理录制包持有实际资源及内部上传 lease，接受提交后由原有 fence 推进完成和回收。D08-T04 在同一后端跟踪 Shader 写入对各消费域的可见性和后继 Shader 写的执行依赖。公开的 staging/map 与 resolve 入口当前拒绝执行。 [提案](proposal.md) · [传输入口](../../../../RHI/OpenGL/CommandListTransfer.cpp) · [提交与退休](../../../../RHI/OpenGL/Submission.cpp) · [同步合同](../../../../Docs/Architecture.md#barriers)

Event 已捕获 Graphics 的最后已接受提交。Timer 已采用一对 native timestamp，运行时已有按原提交身份发布的定长轮转。当前后端以 Ended/available 为计时解析条件，物理包也可能仅为放弃前缀而进入内部退休。新链路需要独立辨认录制已发出、实际被接受、可公开观察这几个时刻。 [查询实现](../../../../RHI/OpenGL/Queries.cpp) · [运行时计时](../../../../Runtime/FrameStatistics.cpp) · [上游查询与退休](../../../../Docs/Reference/NVRHI/ExecutionAndLifetime.md#n10)

## Goals / Non-Goals

在现有 RHI 和单 Graphics 队列上建立可复用的传输和完成合同。底层 owner 能判定真实访问、CPU 借用和资源退休，图 owner 仍掌握版本和活性，后续读回业务 owner 掌握应用请求身份。用可验证的范围、格式、样本集合及诊断交付有限原生 profile。 [命令执行](../../../../Docs/Architecture.md#command-execution) · [读回合同](../../../../Docs/Architecture.md#readbacks) · [能力合同](../../../../Docs/Architecture.md#capabilities)

D08-T06 负责图资源池和跨帧导入生命周期，D16-T04/T05 负责拾取身份、截图管理和编码，D18-T01 负责完整 MSAA 绘制及算法指定的深度、法线 resolve。Mip 生成采用常规颜色过滤，环境预滤、法线重建和 Hi-Z 的 reduction 由各自算法定义。 [D08-T06](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t06) · [D16-T04](../../../../Docs/Developments/D16-D17-EditorAndDelivery.md#d16-t04) · [D16-T05](../../../../Docs/Developments/D16-D17-EditorAndDelivery.md#d16-t05) · [D18-T01](../../../../Docs/Developments/D18-D19-Extensions.md#d18-t01)

## Decisions

<a id="transfer-capabilities"></a>
### 1. 用实际操作所需的格式、目标和样本配对

以既有格式表和按目标建立的 Device caps 为权威，查明实际 GL 入口、目标格式用法、FBO 可用性和样本集合。需跨模块查询时沿用 `RHI/OpenGL.hpp` 的轻量设备查询方式，返回同一份经检查的事实，避免占用 NVRHI 公共 Feature 或从通用 `FormatSupport::Texture` 推断执行能力。调用点仍检查完整描述和配对限制。文档按实际实现和原生结果更新操作矩阵。 [格式能力源](../../../../RHI/OpenGL/Formats.cpp) · [互操作边界](../../../../RHI/OpenGL.hpp)

| 操作 | 首批 profile |
|---|---|
| 现有 Texture clear/copy | 单样本二维、2DArray、Cube、CubeArray 的已交付格式和精确范围。typed clear 保留原附件、分量及格式限制，普通 copy 保持同格式数据和编码 |
| Buffer UInt clear | 普通、非 volatile、带 UAV 用途的 Buffer，容量可按四字节整除，完整写入相同 32 位值 |
| 颜色 mip 生成 | 已证实可过滤且可作为颜色附件的 `RGBA8_UNORM`、`SRGBA8_UNORM`、`RGBA16_FLOAT`，单样本二维、2DArray、Cube、CubeArray，使用声明为 RenderTarget 的 texture |
| 颜色 resolve 源及配套 clear | 候选 `RGBA8_UNORM`、`RGBA16_FLOAT` 的 Texture2DMS / Texture2DMSArray 颜色附件。样本数取已有受支持集合中的实际值，目标为对应的同格式单样本二维或二维数组颜色附件 |
| Texture staging | 单样本二维、2DArray、Cube 与 CubeArray 上的 `R8_UNORM`、`R32_UINT`、`RGBA8_UNORM`、`SRGBA8_UNORM`、`RGBA16_FLOAT`，用 `arraySlice` 选择数组层或展平的 cube 面。块格式先采用 `BC5_UNORM` 的二维完整 mip，以相同压缩存储往返 |
| Buffer staging | 固定 CPU Read 或 Write 方向的专用不可变容量 Buffer。通用 `CopyBuffer` 搬运其范围，GPU 端点为普通非 volatile Buffer |

块压缩入口、格式和布局单独核对。BC5 使用已有 RHI 格式元数据和实际支持的 RGTC 存储，限制到二维完整 mip 以对齐读写规则，包含末尾不足一个块的小 mip。各操作分别拒绝其 profile 外的格式、样本和形状。SDR/HDR 消费按表选择颜色格式，整型拾取走精确传输。 [OpenGL 4.6 Core §8.7、§8.11、§18.3](https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf) · [压缩子区域的边界](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_get_texture_sub_image.txt)

Intel 32.0.101.6129 上，直接将多级 BC5 的 7×5 或 14×10 非零原生 mip 下载到 PBO 会重复块行。同样的数据在原生 level 0 可逐字节往返，改变 PBO 偏移或使用同格式 mip view 不能修复。项目现有 BC5 仅用于传输，因此每个逻辑 mip 使用自身实际尺寸的独立原生 level 0，由对应 Texture 一次创建并统一保活和退休。每级只有一份原生存储，保留公开尺寸、mip 索引和块字节，沿相同 PBO、提交及 Map 路径验收。 [原生对照](../../../../build/d08-t05/BC5Shape-Release.log) · [原生存储](../../../../RHI/OpenGL/Texture.cpp)

<a id="framebuffer-transfer"></a>
### 2. FBO 传输按选中附件工作，原生访问进入已有跟踪

设备复用少量 scratch FBO，共享现有 clear 的附件配置规则。颜色 mip 生成逐层从已定义基准开始，按相邻 mip 进行线性 framebuffer blit，sRGB 目标按其颜色域设置解码及编码。范围可由非零的基础 mip 开始，所选层或 cube 面逐个处理。原 Texture 的 base/max level、分量映射和存储描述继续决定普通采样。用 FBO 明确每一步的源和目标，还能沿用 resolve 的附件能力、区域和原生错误路径。

Resolve 使用同格式、等尺寸的单层或逐层配对 blit，源位置为唯一基础 mip，目标位置可为匹配尺寸的非零 mip。观察到的颜色 resolve 遵循 OpenGL 对归一化或浮点 multisample 的语义，具体样本归约由设备定义。用公开 typed clear 初始化候选 MS 颜色源。采用 `ContextState` 控制不受残留区域限制的传输状态，并保证后续 Draw 重新落实自己的状态。 [OpenGL 4.6 Core §18.3.1](https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf) · [原生状态归属](../../../../Docs/Architecture.md#command-execution)

Mip 生成从已有的 RHI 即时命令互操作边界提供小型扩展，并通过 Validation 转发。公共 NVRHI `CommandList` 的接口形状保留。Mip 基础层读和目标写分别采用 CopySource、CopyDest，resolve 对应已有 ResolveSource、ResolveDest。先验证全请求的范围、所需能力、持有引用和用途，再发出传输。Native 失败沿既有诊断和无效录制处理已发前缀。

访问跟踪记录原始对象的 byte range 或 mip/layer/aspect，FBO 操作用 FRAMEBUFFER 消费域，普通 Buffer copy/clear 用 BUFFER_UPDATE，纹理复制/上传/下载用对应的 TEXTURE_UPDATE。像素传输的 Buffer 端按实际 pixel buffer 域核对，专用 staging Buffer 以方向和映射租用约束 CPU/GPU 交接。合并必要位后由现有发射者执行，记录实际命令的范围和是否完整覆写。Mip 内部逐级 blit 使用已排序的附件访问，后续 Draw/Dispatch 仍使用底层子资源的同一访问历史。 [同步合同](../../../../Docs/Architecture.md#barriers) · [OpenGL 4.6 Core §7.13](https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf)

<a id="staging-layout"></a>
### 3. 用实际布局和原物理包保护 staging

把 Texture staging 表示为有确定每子资源布局的 Buffer 存储。Buffer 与 Texture staging 复用设备的不可变存储及真实删除协议。初始化按 format 的像素/压缩块大小、各 mip 的实际尺寸和层数检查 offset、stride、总大小及原生参数转换。非压缩先采用四字节行对齐，返回该子资源真实 pitch；BC5 使用实际压缩块行。复制用 mip/layer 加区域确定两端起点，明确 pack/unpack 的 offset、row length、alignment 和压缩块参数。当前 `ContextState` 统一接管像素存储和 PBO 绑定，后续普通 `WriteTexture` 恢复其紧密源布局。 [上游 staging 布局](../../../../Docs/Reference/NVRHI/RecordingAndBarriers.md#n6) · [OpenGL pack/unpack](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_compressed_texture_pixel_storage.txt)

采用按需 map/unmap。每个 staging 同时最多有一份 CPU 借用或一个持有它的物理录制包，同一包可依序访问多个合法范围。这样可用既有完成收集证明 CPU 映射时没有相冲突的 GPU 使用。Buffer Map 返回整 Buffer 起点，Texture Map 选择单个完整 mip/layer 起点，区域 copy 仍可选取非零偏移。调用方消费已知已复制的区域。Write staging 只在相应 CPU 借用成功 Unmap 后允许作为源，调用方保证所复制字节已填写。避免常驻映射的额外 CPU 可见性协议，也不将 map 调用作为推进 GPU 的等待点。

有效 Read 复制在实发前建立工作包保活和新使用身份，覆盖的 staging 数据在该使用完成前不可映射。正常接受时登记非零提交身份，只有该包真实退休才使其已成功写入的 staging 结果可映射。内部分配、只用于孤包收拢的 fence、无效录制或丢失环境没有公开接受身份。后端读回 gate 通过包关联和已完成结果状态判定，普通帧由调用方先 Poll Event，Map 不替代 Poll 或 Wait。无效调用保留已有有效结果，调用方用复制诊断及请求身份识别自己的结果。 [已有退休权威](../../../../Docs/Architecture.md#retirement) · [上游 map 与队列完成](../../../../Docs/Reference/NVRHI/ExecutionAndLifetime.md#n10)

设备公开 staging 使用独立的初始上限 128 MiB、128 个存储对象，按实际对齐分配计费，包含在途 lease 和待 owner 删除的分配。同一 Texture wrapper 和它的内部 backing 只计一次。上限是初始可执行边界，覆盖两个紧密 3840×2160 的 RGBA16F 平面所需字节。显式超限用预算诊断退回，存在有效数据或 pending 工作时调用者可继续观察旧请求。后续 Screenshot/Picking 采用自己的有限请求窗口，D08-T06 建立图对象复用。

### 4. 保持 Event 快照，Timer 以接受身份与双 available 发布

Event 仍在 Set 时捕获最后接受的 Graphics 提交。建立 256 个 CPU 观察者的设备上限。最后 Release 归还 Event 容量，Reset 可复用同一观察者，内部 GPU 完成和保活不由 Event 生命周期驱动。保留 Timer 的 512 个 native 名称上限，每个 Timer 两个 timestamp。预算计入已分配及待删除的原生名字，由 owner 实际清理后归还。

Timer 关联本次 recording 和正常提交的接受身份，在真实接受后使用双 available，非阻塞 Poll 才可能解析当前结果。录制被放弃即使内部收拢也不可成为结果，失效或超时由已有诊断路径区分。结果和整个物理包的退休分别处理，复用时依 recording/使用身份隔离迟到的旧包收拢。正常帧保持现有 Runtime 预分配、跳过满轮转、先 Poll 后 Get 及原 frame ticket 的发布方式。

### 5. 由现有图权限安排复制，调用方确认本次请求

图内 Texture staging readback Pass 使用 `CopySource` 声明实际范围并 `MarkReadback` 确保所需生产者存活。回调对该版本执行正常 staging copy，授权按图的物理 Texture 和范围校验。外部 staging 由读回发起方提供，RHI 在实发前校验其所有权与可用性。图尾 Texture/Buffer `ExportUsage::Readback` 暴露实际 CopySource 对象，调用方可在图退出后于合法录制中 copy。Mip 与 resolve 由 Automatic 或显式状态路径执行，图按现有可声明用途提供传输复制。 [图执行](../../../../Docs/Architecture.md#barriers) · [读回声明](../../../../Engine/Renderer/RenderGraph/RenderGraph.hpp)

调用方在干净的执行结果与实际复制所属录制成功提交后捕获 Event，后续 Poll 完成时再 Map。与请求匹配的身份、范围及取消状态由请求 owner 持有，成功 Map 只表明该 staging 数据可安全借用。取消时释放应用结果需求；在途命令及资源仍按原提交退休，放弃录制的前缀使用既有退出协议。 [完成观察与请求身份](../../../../Docs/Architecture.md#readbacks)

## Risks / Trade-offs

| 风险 | 设计约束和可观测结果 |
|---|---|
| 单个图层或边界行取错 | 用同一已检查的 footprint 计算复制与 Map，原生回放验证非对称数据、压缩尾块、padding 和未选范围 |
| 陈旧结果、时序冒用或 map 隐式等待 | 首次写入、接受、退休和 CPU 借用有各自前提。以未提交的新复制、后续 Pending、取消/放弃后的重用及真实数据区分 |
| 同一对象在采样、传输和后继 Shader 写之间切换 | 按实际原生域与范围建立依赖，回放检查重叠结果、次序及相邻哨兵 |
| 样本查询等同于完整 MSAA 执行 | Resolve 仅公布已实现且观察到的源/目标配对和样本集合，原生证据记录实际 FBO 与 blit，Draw profile 按各自合同验证 |
| 退出或超额释放导致重用尚在途的存储 | staging 和查询预算随实际 owner 或原生退休归还，失效 context 进入项目 CPU 收拢流程 |

## Migration Plan

以现有接口先落地真实操作、staging 保活和完成 gate，再让能力表与既有图/统计消费者的验证接上。当前落地不涉及场景文件迁移。后续消费者按各自方案采用已验证的格式和样本，完整工作包通过独立审查与最终验证后同步主规范。
