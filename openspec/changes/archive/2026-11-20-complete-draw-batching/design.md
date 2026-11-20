# Design

## Context

Forward 从冻结 SceneFrame 和每路 PreparedView 得到可见 Primitive。当前准备代码按 Pipeline、材质行、几何和深度排好记录，仍以独立对象常量和绑定到单行的材质集合逐项调用 `DrawIndexed`。几何已经按可兼容布局分配页，两个 View 共用驻留版本和场景上传包。GL 已支持直接实例和 canonical InstanceIndex，间接 Buffer 参数、图用途及执行入口仍缺少这一路消费。 [提案](proposal.md) · [Draw 准备](../../../../Engine/Renderer/Draw/StaticDraw.cpp) · [Forward 执行](../../../../Engine/Renderer/Paths/Forward.cpp) · [OpenGL 现有范围](../../../../RHI/README.md#opengl-原生消费边界)

## Goals / Non-Goals

- 让同一 View 的对象和材质索引随批次一次准备，明确每批可共享的状态、Shader 取数、上传范围和原生执行顺序。以同次冻结输入的有序逐项绘制和已知样本作对照，保留已有几何、材质取样、覆盖及深度合同。 [Draw 与批次](../../../../Docs/Architecture.md#draws)
- 本轮只涉及现有 Forward 静态表面和 CPU 生产的 indexed 参数。透明与可传输表面继续走现有不支持诊断，GPU 写入参数及其多消费域同步归 D08-T04。全场景性能评价归 D08 阶段出口。 [阶段计划](../../../../Docs/Developments/D08-D11-RenderPaths.md#dev-d08)

## Decisions

### 排序与批次兼容

- Draw 准备沿用现有可见索引、路由和资源句柄。先解析本 View 实际使用的深度/颜色 Pipeline、采样视图与 sampler，再按 Pipeline、完整采样绑定、顶点及索引页、从近到远的深度和稳定源次序排序。身份取自本轮冻结数据与完整的逻辑资源记录，地址、原生名称或 hash 只在能够保持相等语义的场合使用。深度/颜色采用同一有序表面序列，包含等深时的固定次序。
- 合批还要核对各 Pass 的 Pipeline 与布局、固定及动态光栅状态、所有活动绑定及其范围、顶点流页/起点/stride、索引页及格式，以及实例块和对象/材质表范围。有效视图的格式、维度、mip/layer 和实际 sampler 都属于采样身份。完整比较在有序记录的相邻项之间进行，紧凑排序键和 hash 只辅助查找。数值材质差异通过同一已绑定表的行索引表达，能否合批仍由实际状态决定。
- 相邻且同一索引序列、基顶点和兼容状态的项可扩展同一实例命令。相邻同状态的不同几何命令组成固定数量的 MDI 批次。实例块空间、表页或原生参数限制使序列分段时，保持原有顺序和每项身份。逐项直接绘制用于同口径验收，可从同一完整记录展开。 [Forward 增量](specs/static-forward-rendering/spec.md)

### 实例表、驻留行与索引单位

- 由现有 `StaticForward` 接口生成只读 `std430` 的对象与材质表，以及每 View 的 `DrawInstanceData`。实例行保存固定宽度的对象、有效材质行索引。canonical `InstanceIndex` 选取绑定实例块中的行，顶点读取正确的对象变换和材质数据，并通过 flat 材质行索引把同一有效行交给 Depth 和 Color 入口。保留既有 normal/tangent、双面/镜像、alpha cutoff、Unlit 及光照坐标计算。实例索引、表索引都在生产端检查容量，GL lowering 沿用既有 firstInstance 语义。 [Shader 接口生成](../../../../Engine/Shader/Interface/README.md) · [接口输入](../../../../Engine/Shader/Interface/StaticForward.json)
- Depth 和 Color 在材质分歧前捕获两组插值 UV 的梯度。各材质槽以仿射变换的线性部分映射对应梯度，覆盖和诊断预览都用显式梯度采样。几何粗糙度导数在 View 的统一光照模式内求值，随后进入逐实例 Lit/Unlit 分支和覆盖测试。 [GLSL 4.60 导数和纹理函数](https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.4.60.html)
- 对象与材质编码沿用现有版本与失效来源。将逐对象常量和逐材质行 UBO 替换为有界的可索引驻留页，保持稳定行区间，由共享页绑定承载当前状态。实例块依 View 与 FrameSlot 独立，包含轻量行索引。选择现有生成器可反射的有限块容量，按实际 SSBO 对齐、extent、阶段/combined 绑定限额及工作集预算分段。表的绑定完整区间须真实定义，首次使用时为需要的范围登记初始化，包括约定的 padding。后续只登记失效或变动的行，行的实际读取由已检查的实例索引确定。在途 Draw 仍引用的行保有对应代。冷初始化纳入实际上传与预算，驻留表未写的区域继续遵守精确图范围合同。 [静态 GPU 合同](../../../../Docs/Architecture.md#gpu-residency) · [驻留增量](specs/static-gpu-residency/spec.md)
- 顶点页按兼容流布局及 stride 分配可表示的区间，批内绑定统一页起点。CPU 从页内字节差换算出 `baseVertexLocation`，保留源局部索引及绕序。索引页从 byte 0 绑定，`startIndexLocation` 从索引区间除以元素宽度得到。间接参数范围的 offset 单独以字节计，`startInstanceLocation` 是当前实例块的行号。所有对齐、范围、乘加及窄化在填充参数前证明。Shader 内的 InstanceIndex 和 VertexIndex 使用已经包含各自基值的语义。 [公共记录](../../../../RHI/RHI.hpp) · [原生参数格式](https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf#page=396)

### 上传、图权限与退休

- 以 D08-T02 的场景上传包保存 CPU 载荷和目标版本。每 View 在封存前登记其实际使用的实例表和参数区间，沿用物理页的单一 canonical import、脏写合并、存活消费者选择与设备准入。经过安全槽复用且内容相同的已接受范围可直接导入，变化的排序、可见记录或材质映射只产生对应写入。只有带存活用途的上传纳入正常录制，失败、裁剪及仅发出前缀都沿既有回执合同处理。 [上传与准入](../../../../Docs/Architecture.md#uploads) · [D08-T02 基线](../2026-11-20-complete-versioned-gpu-residency/tasks.md#final-verification)
- RenderGraph 增加 Buffer 的 `IndirectArgument` 读用途并集中映射到同名公共 RHI state。Pass 声明匹配版本的参数字节区间、实际顶点和索引区间与采样子资源。冷结构化表的整页读取依赖同图的完整初始化。暖页由导入事实证明整页已定义，额外授权精确匹配的 SSBO 绑定，只有实际被实例索引的行参与版本依赖和查询权限。图编译据此裁剪无消费者的增量写入，后端按实际绑定区间校验设备对齐和使用权限。普通 RHI 参数 Buffer 的内容由其生产者保证，后端核对命令可知的实际参数区间和已授权用途。 [图增量](specs/render-graph-contract/spec.md) · [图执行与同步](../../../../Docs/Architecture.md#barriers)
- 每路候选及安全 FrameSlot 保有所引用数据的版本。包实际使用的状态、间接 Buffer 和上传区段加入现有在途保活，由真实完成及受控退出释放。准备数据遇到临时容量占用返回既有 Pending，确定超限沿准备诊断报告请求量和限制。空场景或无可绘分支沿用已有背景和诊断行为。 [帧槽](../../../../Docs/Architecture.md#frame-state) · [提交保活](../../../../Docs/Architecture.md#retirement)

### 固定数量的 OpenGL indexed indirect

- 由 GL 4.6 Core 的设备创建校验 `glDrawElementsIndirect` 和 `glMultiDrawElementsIndirect` 所需入口。复用普通不可变 Buffer、现有公开 `isDrawIndirectArgs` 和 `GraphicsState.indirectParams`，按受支持用途和实际 limits 创建。单命令使用 indexed indirect，多命令使用 MDI，消耗紧排的公共 20 字节记录。公开接口形状与编号延续现状。 [即时执行合同](../../../../Docs/Architecture.md#command-execution) · [OpenGL Core §10.4](https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf#page=396)
- 固定 NVRHI 的 Vulkan 路径会把 IndexBufferBinding 字节偏移交给原生索引绑定，OpenGL 的 indexed indirect 直接从 element Buffer 的字节 0 根据 firstIndex 寻址。故本 GL profile 的间接 Draw 在命令边界拒绝非零 index binding offset，Renderer 使用页基准生成参数。既有直接 indexed 继续使用自己的 byte offset。参数 Buffer 的 offset 仍允许合法非零值。 [固定 NVRHI 实现](../../../../ThirdParty/nvrhi/src/vulkan/vulkan-graphics.cpp) · [OpenGL Core §10.4](https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf#page=396)
- Raw 与 Validation 都依据当前有效的 owning Graphics recording、真实参数 Buffer、索引角色及已绑定的图形状态处理调用。校验 offset 的 4 字节对齐、`drawCount` 原生表示及完整区间，合法零数量在发命令前返回。Validation 复用 GL 有效录制快照及现有诊断路径。开放这一调用后，非索引间接、计数缓冲及相关命令仍沿已有限定范围拒绝。 [OpenGL 执行增量](specs/opengl-immediate-draw/spec.md) · [资源增量](specs/opengl-device-resources/spec.md)
- Validation 在委托 `SetGraphicsState` 前校验公开候选，OpenGL 录制继续负责实际归属、设备限制和生效状态。预检临时存储分配失败时，Validation 用无分配诊断拒绝候选。图内预检拒绝后，包装层通过现有 `ClearState` 撤销旧绘制状态，保留当前图授权、用途追踪及已发前缀的引用。普通录制中的预检拒绝保持之前的有效状态。 [RHI 公共输入边界](../../../../Docs/Architecture.md#rhi-objects)
- 把 indirect Buffer 绑定纳入 ContextState 的唯一缓存及失效/删除路径。每次消费核对实际状态、当前 volatile/push range 及 attachment 访问。Automatic 推导 indirect 用途，Explicit 与 GraphManaged 使用已提交的对应状态，后者还校验实际参数区间。当前生产者使用有序 Buffer 更新/复制和既有 coherent staging。OpenGL 命令顺序加现有上传协议满足这条依赖。Shader 写入所需的 `COMMAND` barrier 归 D08-T04。 [上游记录参考](../../../../Docs/Reference/NVRHI/RecordingAndBarriers.md#n7) · [OpenGL Core §2.1 与 §7.13](https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf)

### 统计与验收口径

- 保留 Runtime 现有已接受图快照，Renderer 的可绘项、批次和参数记录按对应 View 和实际执行的几何 Pass 解释。RHI `draws` 仍以实际原生调用计，一次 MDI 记一次；绑定和上传继续按真实操作计。整图原生计数包括其 UI 等消费者，面板标明作用域。实例和参数的上传量来自实际执行的对应写入，与静态表行、View 数据和设备总量分开。 [统计增量](specs/render-frame-statistics/spec.md) · [诊断合同](../../../../Docs/Architecture.md#diagnostics)
- 用固定相机、画质和非对称输入对照直接、单 indirect 及 MDI 的读回和逻辑/原生计数。具备等价曝光、输出和有效采样范围后才做计数比较。CPU/GPU 性能结论需要另有同口径测量。 [验证与性能合同](../../../../Docs/Architecture.md#tests) · [测量口径](../../../../Docs/Architecture.md#benchmark)

## Risks / Trade-offs

- 位宽溢出或页身份误配会让合法命令取到别的几何或材质。用完整兼容性、页内单位转换和大材质集合的不同像素标记验证。
- 全范围绑定会放大冷启动工作量。只初始化实际使用的表范围，记录完整容量及冷写入，暖帧继续沿版本和脏范围复用。
- 两个几何 Pass 对 OPAQUE/MASK 或等深顺序的解释偏离会改变覆盖与深度。共用生成接口、既有覆盖函数和有序记录，用固定像素对照两条深度配置。
- 复用参数 Buffer 时缓存命中、相邻范围和未完成票据容易掩盖失效。验证同一 Buffer 更新后的原生结果、范围权限及已发批次释放原句柄后的真实完成。

## Migration Plan

先接通普通参数 Buffer、图用途与原生 indexed 命令，再切换现有 Forward 的对象/材质数据与生成接口。随 Depth/Color 一同交付批次和上传，最后连通 Runtime/Editor 统计及回归证据。发布前准备整份兼容 Shader/绑定代，准备失败沿当前视图诊断及上一有效输出机制处理。资产和文档格式沿现有入口读取。 [实施任务](tasks.md)
