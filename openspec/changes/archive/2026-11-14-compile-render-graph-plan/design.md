# Design

## Context

见 [提案](proposal.md#why)。D05-T01 的 `RenderGraph` 在 `Seal` 后固定资源、Pass 和导出表。写入按资源版本索引到 `VersionProducer`，`PassUse` 保留实际访问范围、上一版本及 Load/Store 效果。`RequireContent` 已能拒绝声明期可判定的内容空洞，编译需要获得各部分的生产者并判断整个 Pass 图的覆盖顺序。 [封存入口](../../../../Engine/Renderer/RenderGraph/RenderGraph.hpp) · [声明类型](../../../../Engine/Renderer/RenderGraph/GraphResources.hpp) · [范围与内容检查](../../../../Engine/Renderer/RenderGraph/PassResources.cpp)

## Goals / Non-Goals

### Goals

- 让同一份封存声明经过纯 CPU 编译，得到可供执行器消费的有序计划。调用方可用固定输入核对活性、旧内容是否仍可读取、首末使用以及物理分配预算。
- 在图计划阶段明确验证描述、格式、样本、附件及用途组合。给定能力由调用方在编译前提供，输入不足时以可诊断的失败交回调用方。

### Non-Goals

- D05-T03 负责首次使用物化、绑定/附件创建和 GraphManaged 同步，D05-T04 负责产品帧接线，D05-T05 负责失败回退及取消。跨帧池、预算与退休由相应资源 owner 按架构及后续工作包推进。 [执行器](../../../../Docs/Developments/D03-D05-FramePipeline.md#d05-t03) · [资源池](../../../../Docs/Architecture.md#graph-resources)

## Decisions

### 1. 以封存图和显式 CPU 能力为输入

在 `PhysaraRenderGraph` 内提供编译入口，复用 Core/RHI、既有 GraphId、PassId、范围和 `GraphResult`。编译借用同一图代的封存记录，以临时存储构建计划，全部校验通过才发布。调用方让原图、导入引用和 arena 中的回调数据存活至 CPU 消费结束。计划用身份和自身的计划值关联原图，不接管导入句柄或回调，也不改变 T01 对候选 Pass 的原子发布。

调用方给出只读的 CPU 能力数据和瞬态字节预算。能力覆盖本包消费的描述、格式、视图用途、样本与尺寸限制。按实际组合核对存储和视图，公共 `QueryFormatSupport` 的 OpenGL 二维基线结果只用于它实际回答的用途。编译直接消费给定的数据，实际设备的能力取得和原生创建由调用方和执行器负责。已封存记录的图代和范围沿用声明合同，校验重点是声明后才确定的组合与计划约束。 [RHI 能力边界](../../../../RHI/README.md#opengl-原生消费边界) · [格式查询](../../../../RHI/OpenGL/Device.cpp#L440)

### 2. 逐范围求内容来源，分别表达存储顺序

对 Pass 的显式读、写入 `Load` 的前版读取和外部导出，沿指定版本向初始版本回溯。复用 `GraphRanges.hpp` 的规范区间关系和现有内容效果规则。每条有效写入只覆盖当前仍待解释的交集，记录该部分的生产者，再追溯剩余范围。版本 0 仅在导入明确提供有效内容时填补剩余部分。`StoreDontCare` 或未定义写入与未决范围相交即失败。`Load` 即使伴随完整覆盖仍有显式旧内容读取。沿用范围切分，避免按 texel、字节或每个版本复制完整状态表。 [版本声明合同](../../../specs/render-graph-contract/spec.md#requirement-record-version-producers-and-content-effects) · [范围操作](../../../../Engine/Renderer/RenderGraph/GraphRanges.hpp)

数据边从实际内容生产者指向消费者。存储顺序按同一资源身份上实际重叠的访问建立。较早的重叠写入约束较新写入（WAW），旧版读约束会覆盖该范围的较新写入（WAR）。关系按 Pass 去重，同时保留可用于诊断的资源、版本及冲突类别。约束直接覆盖需要排序的存活写入，不能依赖可能被裁剪的中间写入串接。仅按逻辑版本串行会把范围外写入卷入依赖；仅保留内容边会漏掉仍存活的读后覆写与写后覆写。

声明可按序积累多个用途，执行按 Pass 调度。同 Pass 的独立重叠读写或重叠写入要求内部顺序时，编译返回非法 feedback。附件写的 `Load` 通过上一版本建立内容边，配套的 clear 由计划单独安排。只读深度附件加采样属于联合读用途，按给定能力与实际范围判定。先验证全图的描述、用途组合和 Pass 间环，再裁剪。环诊断从未排完的约束中定位冲突，不以声明顺序补齐。 [架构 §6.3](../../../../Docs/Architecture.md#graph-compile) · [只读深度与 Load 合同](../../../../Docs/Architecture.md#barriers)

普通纹理复制和读回以存储格式传输 texel。对于 packed depth/stencil，访问须声明两个 aspect，单 aspect 的 typed clear 可分别定义内容。编译在求依赖前拒绝实际操作无法兑现的范围，版本回溯仍按所声明的精确范围进行。 [公共 TextureSlice](../../../../RHI/RHI.hpp#L739) · [普通复制](../../../../RHI/OpenGL/CommandListTransfer.cpp#L268) · [typed clear](../../../../RHI/OpenGL/CommandListTransfer.cpp#L148)

### 3. 明确根与图尾交接

现有 `GraphExport` 作为图尾消费。`ExportUsage::Readback` 表达图外读回需求。图内读回 Pass 在 setup 明确声明读回副作用，标记指向一条已有的 `CopySource` 读取，与候选 Pass 一并发布。根只需保留这个类型和所关联的读用途，实际请求、staging 资源、票据和完成结果仍由读回消费链持有。这个约束避免通用强制保留开关，也让没有图内产物的读回 Pass 具有可追踪的依赖。 [导出类型](../../../../Engine/Renderer/RenderGraph/GraphResources.hpp) · [读回边界](../../../../Docs/Architecture.md#readbacks)

从根逆向沿内容边标记活性。保留存活端点之间的存储顺序，按声明索引选择当前可运行的最早 Pass。直接保留所有被覆写上传会消耗无效的内存和录制工作；从根出发保留仍被读取的内容即可。根只指向所需版本和范围。顺序确定后，逐项检查是否存在仍会执行且覆盖图尾所需范围的较新写入。图尾消费必须发生在全部 Pass 后，故冲突属于编译失败。未覆盖的区域和直接导出的有效导入可交接。

### 4. 从实际顺序计算寿命与物理槽

首末使用取有序计划中实际访问的位置。图尾交接占用最后一个 Pass 之后的位置，可用于无图内访问的直接导入导出。导出把整项物理存储保活到交接，其内容依赖与覆盖校验仍按导出范围。导入沿用外部对象和允许范围，规划使用中的瞬态资源时为同一逻辑资源的所有版本保留共同映射。

首发按首用位置分配，平局以固定资源类型顺序及声明索引打破。复用最早建立且已释放的兼容槽，要求上一资源的末用严格早于新资源首用。兼容性采用同类完整存储形状、格式、样本、用途及影响资源或视图行为的描述字段，诊断名不参与。这个保守键允许相同描述复用，免去推导用途并集、格式重解释或追求全局最优分配。计划记录每个槽的完整描述与预计字节数。缓冲按字节容量，纹理用公共格式块元数据逐 mip 汇总，按维度计层、体深和样本。检查所有字节乘加后再累计不同槽，导入计入使用记录而不占瞬态槽预算。估算的单位是逻辑存储字节，执行器处理实际分配与状态延续。 [复用条件](../../../../Docs/Architecture.md#graph-resources) · [格式块元数据](../../../../RHI/RHI.hpp#L311)

### 5. 交付执行边界所需的计划值

计划按 Pass 记录实际版本、范围和用途，显式列出清除值及清除所需的传输用途。附件 Clear 也须满足所选范围的清除能力，清除步骤先于后续附件访问。纯传输清除保持复制目标用途。单 Pass 的颜色附件沿声明顺序编号，深度和模板共用兼容的附件视图。核对附件数量、实际所选 mip 的尺寸、单层视图、角色与样本。视图信息保留原资源格式、所选范围及用途，供 T03 解析附件和绑定。Shader 布局和实际绑定对象由 Pass 及执行器持有。只读深度联合采样的信息保留给统一的 RHI 适配。窗口输出携带实际导入的 entry/final 状态，`Presentation` 是图外用途，`PresentColor` 按宿主的 ShaderResource 进出状态交接。 [附件实际约束](../../../../RHI/OpenGL/Framebuffer.cpp#L83) · [清除用途](../../../../RHI/OpenGL/CommandListTransfer.cpp#L85) · [唯一同步发射者](../../../../Docs/Architecture.md#barriers) · [宿主目标](../../../../Platform/Presentation.hpp#L32)

当前普通用途的状态以纹理 mip/layer 和整个 Buffer 为单位。编译检查共享状态单元的同时用途，允许当前 RHI 支持的只读组合及 `CopySource | CopyDest`，并按能力表核对同一 mip/layer 的只读深度附件联合采样。清除仍单独请求状态。执行器在提交普通用途前按相同粒度合并状态，Pass 权限保留各自的精确范围。 [OpenGL 状态组合](../../../../RHI/OpenGL/CommandListState.cpp#L61) · [公共状态跟踪](../../../../RHI/Common/StateTracking.cpp#L481)

纹理 `ShaderRead` 导出交付该范围的同格式采样视图，沿用 `Sampled` 的视图能力与层数限制。呈现能力分为纹理形状和宿主要求的完整结束状态两项，编译仅接受同时满足两项的呈现导出。诊断先记录原因、资源类型及索引和已知 Pass 索引，再附加调试名称。 [采样视图](../../../../RHI/OpenGL/Bindings.cpp#L135) · [窗口交接检查](../../../../RHI/OpenGL/WindowPresentation.cpp#L45)

## Risks / Trade-offs

- **逻辑版本的尾部写入遮住其它范围的生产者** → 用交叠区间求内容来源，核对双生产者、范围外写入、丢弃空洞及局部 Load。
- **活性掩盖存储风险** → 用会改变可运行集合的旧版读和重叠写入观察真实顺序。另用图尾的旧版导出核对死写和存活覆写的区别。
- **物理复用过早或预算低估** → 用同描述的相邻寿命、同一 Pass 内的重叠寿命、导出终点及精确预算临界值核对。以完整存储描述估算，包括未访问的 mip 或层。
- **计划兼容被误作设备执行保证** → 以 CPU 能力夹具证明计划的支持/拒绝分支。首次物化、联合深度采样、原生 barrier 和呈现由 D05-T03/T04 在真实消费链验证。

## Migration Plan

先在现有图目标中交付编译入口，以独立 CPU 消费者读计划。D05-T03 用顺序、物理槽、首次使用、用途及外部状态接通执行器，D05-T04 用当前诊断上传与 UI 合成路径消费该入口，D05-T05 根据明确的编译/执行失败处理旧输出、回退及取消。 [后续工作包](../../../../Docs/Developments/D03-D05-FramePipeline.md#d05-t03) · [帧流程](../../../../Docs/Architecture.md#frame-flow)
