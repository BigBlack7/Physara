# Design

## Context

`ForwardDevice` 已按固定资产版本缓存独占的几何缓冲、材质 UBO 及纹理。`ForwardFrame` 已在两路 View 间共享资源版本和上传确认。现有 `BufferUpload` 从 offset 0 写入，图的 `contentDefined` 描述整个导入范围。这两个入口需要能表达同一页中并存的已接受、待写和空闲区间。 [静态 GPU 记录](../../../../Engine/Renderer/Materials/StaticGpu.hpp) · [Forward 构图](../../../../Engine/Renderer/Paths/Forward.cpp) · [图资源合同](../../../../Docs/Architecture.md#graph)

OpenGL 的小 Buffer 写已走 DSA subdata，大写入和纹理写已走 persistent/coherent chunk、copy 和 PBO。当前 chunk 随物理包创建、随完成退休，并已有准入模拟及可靠 fence 跟踪。`Buffer`、Shader lowering、布局及 `SetPushConstants` 仍有初始 profile 的拒绝路径。 [OpenGL Upload](../../../../RHI/OpenGL/Upload.cpp) · [提交包](../../../../RHI/OpenGL/Submission.hpp) · [RHI 能力表](../../../../RHI/README.md#opengl-原生消费边界)

## Goals / Non-Goals

- **目标** 从同一份图计划追溯 Draw 所取的几何区间、材质代、纹理层级和写入；以 native 完成证据验证区段环回、换代和压力恢复。 [GPU 驻留](../../../../Docs/Architecture.md#gpu-residency) · [上传合同](../../../../Docs/Architecture.md#uploads) · [退休](../../../../Docs/Architecture.md#retirement)
- **范围边界** 此次交付单 Graphics 即时列表、coherent 映射和直接 Draw 的基础 profile。`GraphicsBaselineV1` 包接口继续服务现有产品；图形 push 走公共 `CreateShader` 的 canonical SPIR-V 入口。排序合批、实例/indirect 生产与执行、Compute、通用 mip/transfer/staging、图资源池和热重载各由后续 D08 工作包交付。 [D08 工作包](../../../../Docs/Developments/D08-D11-RenderPaths.md#dev-d08)

## Decisions

### 几何页和材质表以固定版本分配区段

- 几何按兼容顶点布局及索引宽度使用有上限的顶点页和索引页，身份来自固定资产、内容版本和 Primitive。选择可用的对齐区间，连续空间不足时在预算内加页。Draw 绑定本 Primitive 的实际顶点/索引 byte offset，保留原有索引、切线、布局及绕序语义。全页可回收时才退回物理页，运行时保持活区段的地址稳定，避免旧 Draw 和绑定一起迁移。
- 有效材质行以源版本和覆盖身份/revision 定位。材质表按设备 UBO 对齐分配行，直接 Draw 通过表索引和预解析的行范围绑定沿用现有着色 ABI。相同有效行与实际绑定可以共享。编辑只建立需要改变的行和绑定；不同实例和在途旧版本持有各自区间。绑定缓存比较布局身份、行分配代、视图和 sampler 的实际身份，避免页内地址或原生名字重用命中旧集合。按设备和帧工作集限制页字节、行/视图/绑定条目，失去使用资格的强引用可淘汰。 [静态 GPU 驻留增量](specs/static-gpu-residency/spec.md)
- 分配区间由驻留 owner 记账，准备后由本次 Draw 和帧槽保有。SceneFrame 的完整固定模型版本表变动时，撤销已失去场景消费者的旧几何目录项。诊断或仅 UI 的帧也会交付成功提取的完整模型表，包括存在固定模型但没有可绘表面的情况。本轮没有成功提取时沿用上一份驻留观察。仍被某个实例固定的版本即使当前不可见也保留命中资格。真实 Draw/上传引用随本次提交跟踪，包括部分执行的前缀。丢失可靠完成票据的区段留到受控退出。只有相应访问已证明完成且不再被已准备的候选引用，区间才可回到空闲表。清理先释放无效绑定的缓存持有，再回收依赖储存，共享存储容量只计一次。 [帧槽](../../../../Docs/Architecture.md#frame-state) · [关闭](../../../../Docs/Architecture.md#shutdown)

### 纹理解释和环境产品沿用现有版本发布线

- 纹理系统区分像素版本、存储解释和绑定身份。匹配的视图依固定图像身份/revision、颜色/数据用途、格式、维度及 mip/layer 区间解析，sampler 依有效过滤和寻址解析，材质槽另拥有 UV 语义。让视图只暴露可采样的有效范围，无 mip 模式只选择基础层，启用 mip 过滤才使用完整所需链；使用现有 CPU mip 生产，颜色 RGB 在线性域过滤，alpha、法线及 ORM 走数据语义。按实际物理存储和视图/采样器条目收费，兼容视图可以共享经过验证的存储。 [纹理语义](../../../../Docs/Architecture.md#asset-images) · [纹理准备](../../../../Engine/Renderer/Materials/StaticTextures.cpp)
- 材质行可复用时，其图像仍按各自的接受状态和本批完整生产者独立判断。仅被裁剪、从未交给写入的纹理可在原存储补传，并保留现有材质行和绑定。已进入录制但未被完整接受的纹理使用独立存储重试，旧引用随原前缀保活。图导入以对应纹理代的接受状态解释初始内容。材质纹理在创建存储和生成 mip 前算出所需层级的总字节数，由本批上传预算统一准入及报告。
- 复用 `EnvironmentCache` 的冷路径 CPU 产品、需求收集与取消。环境源 hash、烘焙配置/算法版本和用途中选用匹配天空与 prefiltered/SH；DFG 依其独立 profile。旋转、强度及曝光由本轮 View 数据表达。GPU 缓存按实际 cube 面、mip、格式及 DFG 大小计费，新代完整可用后通过正常图确认，旧产品沿提交寿命退休。相同烘焙身份且仍驻留的产品避免排新作业或重传；回收 GPU 冷产品后可用仍有效的 CPU 产品重建。 [环境准备](../../../../Docs/Architecture.md#light-preparation) · [环境合同](../../../specs/environment-lighting/spec.md)

### 图用每页一个身份表示实际初始化区间

- 用现有规范导入验证为每个物理 Buffer 页建立唯一图身份，补充外部 owner 提供的有界初始有效字节区间。普通完整/空导入继续使用已有简写。编译与执行权限查询从同一份区间事实出发，读用指定版本向前找生产者或有效导入内容，两条路径都只声明实际访问。物理状态转换仍由图执行器和已有 RHI tracker 落实。将共享页整块标有效会遮盖未初始化空洞，独立导入各区段会绕过跨访问依赖，因此保持一个页身份。 [图导入增量](specs/render-graph-contract/spec.md) · [同步责任](../../../../Docs/Architecture.md#barriers)
- 在现有 `ForwardFrame` 收齐静态、新版材质/纹理、灯、各 View 与当前 Draw 实际生成的数据，再统一检查范围、预算、所需 Pipeline/绑定和准备诊断，发布只读的场景上传批。后续实例/indirect 生产者使用同一封存边界。缓冲写由确定的目的 offset/size、存储代及拥有的字节表达；相邻或兼容重叠的最终脏区间仅在本图生产顺序与裁剪语义等价时合并，可以为每个已写代追溯最终覆盖。空洞、仍被旧代读取的内容和应独立裁剪的区段保持独立。UI 保有自己的载荷和写入，准入读取编译计划的真实执行顺序。
- 对应存活写入已执行且匹配正常完整图回执时，才记录可供后续帧使用的已上传区间。`PassId` 及整图回执负责定位本轮，目录项保留必要的 CPU 输入身份及重试来源。合并写入如果覆盖多个代，只确认确实完整写到且属于该候选的代；共享代仍由任一存活消费者保留。无有效回执的原生写入仅进入保活与退休。基础 UI 准入或安全图的成功沿既有规则独立处理。 [编辑器帧图合同](../../../specs/editor-frame-graph/spec.md#requirement-publish-only-accepted-graph-outputs)

### OpenGL 复用完成支持的映射区段

- 延续小更新 DSA subdata、大更新 persistent/coherent staging copy 及纹理 PBO 路径。先复用 Device 已完成、兼容尺寸与对齐的映射区段，再按预算增长 chunk。可用区段成为物理包登记的 recording lease，关联实际 fence，retire 后才回到有界可用集。包内可借未用尾部，已占区段不能因换列表、换帧、关闭或 CPU 源释放而覆写。准入与实际分配共用对齐、分块、容量和存活写入顺序约定。空闲设备可容纳的工作仍须取得物理录制包槽及映射余量。槽或在途余量不足时返回 Pending，空闲设备亦无法容纳时报告硬限制。 [上传增量](specs/opengl-resource-uploads/spec.md)
- volatile 按逻辑大小、`maxVersions` 和真实 UBO/映射对齐准备 device-owned backing，独立记录每次写入的有效前缀。包持有新版本和被引用的旧版本，位置 0 合法。活动绑定每次消费从本 recording 的最新有效版本解析原生 range；相同图形状态下的连续写入也更新对应 range。push 用有界 UBO 段，在合法图形状态设置后复制规定大小的数据，使用同一物理包协议。合法状态切换使旧 push 内容失效，已发出 Draw 保持原区段；`ClearState` 保留已经登记的寿命和本记录内的有效 volatile 写入。常量容量耗尽通过原有诊断返回，保留真实前缀。 [公开 RHI 语义](../../../../RHI/RHI.hpp) · [固定上游记录与上传参考](../../../../Docs/Reference/NVRHI/RecordingAndBarriers.md#n6) · [图形执行增量](specs/opengl-immediate-draw/spec.md)
- 在原有绑定计划验证普通/volatile UBO 逻辑声明与 canonical uniform block。一个图形 Pipeline 的活动 push 块使用独立物理 UBO binding，随同普通/volatile UBO 纳入实际阶段及 combined limits，未使用时无需占位。使用固定 SPIRV-Cross 的 IR lowering 选项保留反射字段的 offset/stride、阶段可见性和公共块大小。原生 UBO 的尾部对齐计入 backing 容量，公开写入范围仍由反射与布局的逻辑大小决定。无法保留该布局的请求在发布 Pipeline 前拒绝。彻查 raw 及 Validation 的旧拒绝点，只有本次支持的命令和布局通过既有公共入口。产品 Forward 的普通常量在图内按精确范围上传，用本地 canonical SPIR-V 图形消费者证明新动态路径。 [GL 绑定 lowering](../../../../Docs/Architecture.md#opengl) · [Pipeline 增量](specs/opengl-shader-pipelines/spec.md)

### 分别计量驻留和原生上传

- 以现有 128 MiB/2,048 的单 View 工作集和 256 MiB/4,096 的全局驻留基线，以及 RHI 64 MiB 的上传容量与 4,096 写/列表为初始对照。实现时根据目标代表场景的实际页/行对齐、视图数量和同时在途需求确定有穷页大小、条目限制、可保留空闲容量及 volatile backing 总限，记录最终值与口径。预算核对包括对齐、页空闲段和暂未退休的旧代。若单候选超出硬限制给出请求量和限制；受旧代或在途段暂时占用时先收集完成，按既有 Pending/UI 优先行为重试。小/大写入分界以现有 256 KiB 为起点，调节需同输入的测量。 [Renderer 预算基线](../../../../Engine/Renderer/Materials/StaticGpu.cpp) · [RHI 上传基线](../../../../RHI/OpenGL/Upload.hpp)
- Renderer 统计页与缓存使用量、驻留代及脏范围，RHI 统计映射容量及真正发出的上传、复用和原生分配，Runtime 给快照标注当前场景正常提交，Editor 消费只读值。当前等待原因沿诊断入口可见，失败/取消/安全 UI 保留最后有效正常场景统计。利用已分配容量与实际写次数证明范围合并和复用，计时结论以同场景、同输出测量为前提。 [诊断](../../../../Docs/Architecture.md#diagnostics) · [统计增量](specs/render-frame-statistics/spec.md)

## Risks / Trade-offs

- **缓存目录先换代，旧区间仍被写或读** → 保留帧槽/提交范围 lease，覆盖图中失败前缀和无可靠 fence 的退出，再以真实完成释放旧区段。
- **合并把已初始化空洞或裁剪边界带入同一批** → 使用真实区间及同等活性，保留代与写的对应，比较拒绝后重试及 UI 优先时的实际原生命令。
- **UBO binding 对齐、有效长度或 push 布局失配** → 准备时验证 canonical 和 GL 布局，执行前解析本轮范围，用有区分度的 Shader 读回证明相同状态连续写、切换及环回。
- **空闲常驻页挤占可用容量** → 同时报活跃、可回收和已配置容量，先使用兼容空闲区段，依有界策略释放空页和 cache 强引用。硬超限和短期压力保持可辨认。

## Migration Plan

先接通页内内容和范围上传，再让现有 Forward 改用几何页与材质表。随后沿同一 Device 提交链接通映射复用及动态常量，并使预算诊断走现有发布通道。迁移在 Renderer/RHI 内完成，CPU 资产及场景文件继续由现有入口读取，旧独占资源结构随直接消费者切换移除。 [实施任务](tasks.md)
