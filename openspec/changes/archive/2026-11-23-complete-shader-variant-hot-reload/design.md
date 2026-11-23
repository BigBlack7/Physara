# Design

## Context

现有 `Compiler::Build` 已从程序 JSON、接口产物与传递源生成带反射和身份的 canonical 包。它只校验每个 define 的允许值，产品用途由各处加载调用和预期活动资源识别。`ForwardPrograms` 及 `DisplayDevice::PrepareStage` 在 View 准备中首次读包和创建 Pipeline。`Runtime` 已有帧槽、场景和 View 的匹配接受，OpenGL 的创建与回收已经归 context owner。 [Shader 构建](../../../../Docs/Architecture.md#shader-build) · [帧流程](../../../../Docs/Architecture.md#frame-flow) · [准备合同](../../../../Docs/Architecture.md#plan-readiness)

## Goals / Non-Goals

- 准备端交付静态 Forward、天空与显示处理的明确就绪状态，在构图前冻结完整代。帧图继续负责本轮动态绑定、附件和图池准入。
- 首发使用现有 Windows x64 单设备、OpenGL 4.6、单样本图形 profile。EditorUi 由独立的打包程序提供安全合成。后续路径与 AA 事务、物理算法变化以及扩展执行链成本验收按各自工作包推进。

## Decisions

<a id="variant-catalog"></a>
### 1. 从产品角色解析有限变体

以 Engine/Shader 的受控目录登记产品角色与程序请求，`VariantKey` 由程序、阶段/入口、profile 与结构性字段的规范顺序组成。目录声明允许的完整元组，并从选中的元组取得 canonical defines。编译请求原有的逐字段校验继续保护文件入口，目录校验负责跨字段合法性和角色匹配。生成、预热和重载共用同一份角色表。

| 现有产品角色 | 对应编译入口与允许选择 |
| --- | --- |
| 深度及常规颜色 | `StaticDepth` 与 `StaticForward` 各一个现有入口组合。现有诊断预览与直接光模式在已选程序内消费数据 |
| 物理颜色 | `StaticForwardPhysical` 对应 `PHYSARA_PHYSICAL_LIGHTING=1`，`StaticForwardIbl` 对应 `=2`。与当前活动光照布局和资源角色一并核对 |
| 天空及显示 | `ForwardSky`、`ForwardDisplay`、`DisplayColorGrading`、`DisplayFxaa`、`DisplayScale` 按现有入口登记。显示 profile、AA 和实际输出尺寸决定本次需用的现有阶段 |

静态顶点属性布局、镜像、双面和 `SurfacePhase` 仍决定 Pipeline 的输入及固定状态。使用同一 canonical 阶段和接口的状态组合共用 CPU 包。OPAQUE/MASK 保持现有两种表面路径的覆盖合同，材质数值、灯表、曝光、普通预览和显示参数随既有数据上传。可用表按当前已实现角色给出上界，准备端仅请求视图实际需要的键。 [变体行为](specs/shader-build/spec.md) · [现有 Forward 能力](../../../specs/static-forward-rendering/spec.md) · [Filament 材质包与参数分工](../../../../Docs/Reference/Filament/FrameSetup.md#f1)

<a id="source-notification"></a>
### 2. 用当前请求和实际依赖组织 CPU 工作

Runtime 增加开发会话启动参数 `--shader-source-root=<checkout>` 以指定工作区。普通会话沿用 `LocateResources` 得到的包路径。启用时只观察项目选定的 `Shaders`、`Engine/Shader/Interface` 和目录声明所在位置，原生通知读取及取消留在 Windows 文件边界。监听端合并已观察到的路径和根变化。连续保存、替换、缺项及通知溢出以有界脏集合或根标记回到实际输入求值。依赖图由 Shader 决定受影响程序，不由监听层猜测文件用途。 [Core 实际消费边界](../../../../Docs/Architecture.md#core)

沿用 `Core::JobQueue` 的有界提交和取消。一个 Shader worker 持有当前进程的 Compiler 环境，串行处理自持的请求及构建结果。每次记录待处理的最新请求，合并其脏范围。更早的作业可在阶段间取消，进行中的第三方编译返回后再按请求身份丢弃。请求只携带 CPU 数据，推进不借用帧 arena、View、Device 或待销毁窗口。对已有依赖缺失或新增候选路径的情况，保留足以触发后续恢复的观察入口。相同失败在输入或请求变化后才重新求值。

通过已存在的 `GenerateInterface`、`SourceGraph`、`Compiler` 和包验证做实际取证。候选使用自身的接口生成目录或等价自持输入，按现有规则重新计算 ABI 并核对其产物完整性，CMake 产品包和生成头由原构建入口管理。CPU 包查找复核顶层请求、真实传递输入、生成产物、目录中的所选元组及编译器指纹。单配置生成器用实际的 `CMAKE_BUILD_TYPE` 选择配置选项，多配置分别生成各配置的身份。缓存命中跳过实际编译。mtime 和通知仅用于调度，复用和发布依赖内容身份。编译同一候选时缓存已核对的输入字节并确认各程序共享依赖的身份一致。交给 owner 前复核输入闭包。Runtime 在取得 CPU 回执时检查修订，候选完成原生准备后再次以零超时消费通知。已观察到的新编辑使旧候选失去发布资格。

ForwardRenderer 在设备准备器首次接管前记录已观察的最新修订，并持有一份匹配的 CPU 回执。首次装配失败时保留回执、请求号和设备错误，由后续有效 View 或新回执触发装配。设备可用后，在帧前准备阶段把仍属当前修订的回执移交给设备准备器。观察到更晚的修订时释放被替代的回执。

<a id="preparation-budget"></a>
### 3. 在原生准备边界接续缓存与预算

仅图形 owner 将完整 CPU 包转换为 RHI Shader。将现有文件加载的共同校验抽在 Engine/Shader 消费边界，运行时复用当前包读取、角色及活动资源核对。包和 RHI Shader 准备沿用 `LoadRhiProgram` 的消费合同，OpenGL lowering、绑定映射及 native link 继续由 `CreateGraphicsPipeline` 路径形成。Program 身份由程序内容、ABI、实际绑定映射、lowering 规则以及来自设备会话的 vendor、renderer、GL/驱动和 GLSL 身份确定。已准备的 Pipeline 缓存比较包含该 Program 身份的完整键，加入输入布局、固定状态及 `FramebufferInfo` 兼容事实。

首发会话缓存复用已经链接的 Pipeline 及其内部 Program。相同兼容格式的不同图池附件和不同尺寸可以复用，原有 Layer、Viewport 等执行参数仍由图负责。跨会话 driver binary 持久化属于后续缓存扩展，当前设备会话的命中以对象句柄和明确键为依据。实际绑定计划由后端既有规则构造并验证。 [原生复用](specs/opengl-shader-pipelines/spec.md) · [Pipeline 与可变目标的职责](../../../../Docs/Reference/NVRHI/ResourcesAndPipelines.md#n5)

| 边界 | 首发预算与压力处理 |
| --- | --- |
| CPU 待处理工作 | 一份正在执行的候选批和一份合并后的最新需求。结果随候选身份处理，取消或过期后解除持有 |
| CPU 包复用 | 初始硬上限为 18 个条目及 8 MiB 逻辑内容。现有 Release 九个相关编译包合计 311,144 B，用两个版本的产品入口集估算条目需求。字节账户按包的有效负载计，超限时优先释放无当前请求或有效代持有的条目 |
| 原生 Pipeline 槽 | 每代准入按有限目录里的实际兼容键计数，单代上限由现有顶点布局、光栅状态、用途、角色和目标组合导出。设备共享上限为单代上限乘以 `FrameSlotCount + 2`，计入 Active、一个候选及尚被帧持有的代，共享句柄只计一次。超过额度时显示压力，合格对象随真实退休与解除持有归还 |
| 每轮 owner 准备 | 初始每个 Runtime 帧前准备轮最多启动一次新的 Pipeline 创建。布局及绑定配方工作随实际待准备项推进，命中沿现有对象接续。记录 CPU 占用及单次原生调用用时，原生一次调用的耗时由实际环境测量 |

Active、候选及仍由冻结帧持有的旧表都可提供等价键。显示和颜色角色在同一设备会话接续固定的绑定布局，复用时只取得匹配的对象句柄，旧表仍沿原帧寿命退休。只有真正缺项才占设备槽和准备额度。持续请求的失败键只尝试一次。候选在原生失败后释放原生表并保留当前一批已验证的 CPU 包，实际观察到失败键退出并重新进入计划后可重试。更晚的源码修订使旧结果失效。场景与显示候选可跨多轮完成，Debug/Release 使用同一预算合同。

<a id="generation-publication"></a>
### 4. 按就绪计划发布同一完整代

Forward 先完成本轮各路的工作集、上传准备和批次编排，以实际批次、天空和各路显示链汇总全部原生键。槽忙时可沿最近需求预热，实际发布使用当前主视口和离屏 View 的计划。候选持有该请求所需的包、Shader、布局、兼容资源、绑定配方及 Pipeline。两路在构图前从同一张已选表取得句柄，各路仅按自己的键判断 Pending 或失败。

各路在选择 Shader 代之前取得显示配方需要的固定采样器、LUT 和本次可用于上传的载荷，并记入帧的写入额度。任何一路在这一步失败时保留旧代，其它就绪路可以继续提交。共享 LUT 的待写字节由本次帧持有，同一图只登记一次写入。收到正常图的接受票据后，后继帧可将 LUT 作为已定义输入导入。

FrameSlot 在声明图时保存已就绪代及适用的 View 计划，Pass 仅按该代准备出的布局和图授权取得动态资源。每帧的 FBO、图池临时附件和普通绑定仍沿图准入及 recipe 生成，不能把动态附件身份写入 Shader 代。已有几何、材质数值和环境驻留按现有 owner 判断是否兼容。涉及候选专有资源时，用该资源的真实就绪或同队列交接作为准备证明。

ABI 比较以运行中生成头的 hash 和实际消费合同为准。相同 ABI 仍须核对角色、跨阶段 IO、活动资源与绑定计划。深度片元覆盖配方使用材质行及基色纹理和采样器，准备时按反射核对这一活动集合。CPU 数据格式变化由权威接口及普通重建端对齐。候选 Ready 后由 Runtime 在帧边界复核身份并整体发布。设备预留退役记录与句柄计数空间，切换时同步交付代及容量事实。原生创建的 Error 在候选自己的诊断作用域收拢，真实 Fatal 或 context 失效按既有运行退出协议处理。

Active 只表示 Shader 计划可用。View 接受状态继续由匹配正常图输出与 Graphics 票据确认。切换前冻结的槽沿实发包持有旧代。Pending 或 Failed 期间选用仍兼容的旧计划，或通过 D08-T06 的保留输出进入既有 UI 合成。退出依次停止通知、取消并收拢 CPU 作业、释放候选与会话持有，最后由现有 context owner 收拢仍在途的原生依赖。 [Shader 代](specs/shader-generation-reload/spec.md) · [帧消费](specs/editor-frame-graph/spec.md) · [退休与关闭](../../../../Docs/Architecture.md#retirement)

<a id="reload-diagnostics"></a>
### 5. 从实际工作点发布诊断和计量

在 Engine/Shader 的输入检查、编译与反射层保留程序、变体、阶段、原文件位置和有界日志。程序声明的目录拒绝携带实际路径、冲突字段和值及获准条件。接口声明错误附带源路径及已知字段，GLSL 错误使用原始行或实际原生位置。Active 的失败诊断按原生键保留原始原因，需求缩减时保留仍请求的键及其诊断。包构建次数含预检，实际 GLSL 阶段数从 glslang 解析入口计量，编译时间还包含 glslang 链接和 SPIR-V 生成。原生缓存命中按实际查询得到的 Pipeline 键计量，包括显示链的每个就绪阶段。统计快照区分候选状态与 View 最近一次接受提交，面板显隐仅改变自己的构建工作。 [统计能力](specs/render-frame-statistics/spec.md) · [事实归属](../../../../Docs/Architecture.md#diagnostics)

## Risks / Trade-offs

| 风险 | 处理 |
| --- | --- |
| 通知丢失、替换保存或修改抵达顺序改变 | 监听合并后核对实际依赖与内容，旧请求的结果不能发布。用一次真实替换保存和可控的旧完成顺序覆盖两个风险 |
| 相同 ABI 的错误角色或整代混用 | 比较活动资源、stage/entry 和 binding plan，只有当前完整候选在帧边界对所有相关计划发布 |
| GL link 单次运行耗时及暂时容量压力 | 限制每轮启动量并记录实际用时。容量具体限制随已验证的工作集提供压力信号，仍可使用上一就绪计划或输出 |
| 旧代尚在途时发生切换或关闭 | 依赖帧与实际提交保活，沿 owner context 退休。用受控 Pending 和实际完成及关闭回放核对 |

## Migration Plan

1. 先统一现有程序目录、键验证和离线包消费，再在 Forward 与显示准备路径切开查询和原生获取。
2. 接入预算推进与帧边界完整代发布，从打包输入验证正常启动、冷准备和安全 UI。
3. 将开发源观察和 worker 接入同一候选入口，增加只读诊断并复核错误恢复和关闭。开发阶段可通过采用原打包资源启动保留已验证的冷启动路径。
