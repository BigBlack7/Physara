# Design

## Context

动机与范围见 [proposal](proposal.md). 已检查 [D03-T05](../../../../Docs/Developments/D03-D05-FramePipeline.md#d03-t05)、它引用的 [验证合同](../../../../Docs/Architecture.md#tests)、[坐标/颜色](../../../../Docs/Architecture.md#conventions) 与 [Shader 分层](../../../../Docs/Architecture.md#shader-runtime), 并核对正式源码、直接消费者和既有本地 fixture. 当前观察如下, 不等同于本包已经运行成功:

| 边界 | 现有入口/事实 | 本包如何消费 |
|---|---|---|
| Shader | [RhiProgram](../../../../Engine/Shader/RhiProgram.hpp) 校验预期 ABI 后交付 VS/PS; 模块 CMake 已提供接口/包规则 | 显式 schema/程序请求, 生成 C++/GLSL 与绑定, 使用包的真实 content/ABI 身份 |
| Draw | [DrawDirect](../../../../RHI/OpenGL/CommandListDraw.cpp) 即时发出, 已检查 index byte interval; lowering 包含 base instance 适配 | 公共 DrawIndexed 与 Draw, 不复制编译或参数变换逻辑 |
| 目标/传输 | [Framebuffer](../../../../RHI/OpenGL/Framebuffer.cpp)、[图形候选](../../../../RHI/OpenGL/CommandListGraphics.cpp)、[copy](../../../../RHI/OpenGL/CommandListTransfer.cpp) 已支持所需单样本二维子集 | 切换真实兼容目标、同格式区域 copy、不同附件的后继采样 |
| 完成 | [Event 合同](../../../specs/opengl-completion-queries/spec.md) 已交付; T04 正向链在实际 Draw 后等待捕获的提交 | Close/Execute 非零后 Set Event, 有界等待且 Poll 确认, 然后读回 |
| 读回 | [Unsupported](../../../../RHI/OpenGL/Unsupported.cpp) 仍拒绝公共 staging/map; T03/T04 本地 fixture 使用完成后的 DSA 观察 | 限定在 build 的同类冷路径, 不向公共 RHI 增加临时接口 |
| 产品 | [Runtime](../../../../Runtime/Main.cpp) 仍是窗口事件循环; Engine 目前只有 Core/Shader targets | 本包没有生产 Renderer/Pass 消费者, 不创建空模块、第二入口或正式测试 Shader |

[现有九条图形 requirements](../../../specs/opengl-immediate-draw/spec.md) 和 Shader/查询规范已覆盖逐项行为. [本次 delta](specs/opengl-immediate-draw/spec.md) 仅增加端到端交付保证, 不重写这些合同. T03/T04 本地材料在规划时存在, 开始实施时已被清理; 本包按固定输入创建自己的消费者, 不恢复旧测试目录或将其作为永久依赖.

## Goals / Non-Goals

**Goals:** 用一个有限原生消费者串联正式 Shader/RHI 链, 以运行前确定的数值预期证明图案、偏移、目标切换、copy/采样及完成后的读回; 使后续工作包能够复用同一模块/API 和图案定义.

**Non-Goals:** 不建立生产测试服务、Renderer、Viewer、通用测试框架、图像资产库或自动生成 fixture 工具. 不扩展公共读回、呈现、拾取/截图、FrameSlot、RenderGraph、Shader profile、MSAA 或执行能力. 不为阶段验收重新穷举 T01–T04 的 OOM、查询预算、device loss 等已经有独立证据的风险.

## Decisions

### 1. 一个 build 消费者, 正式状态沿既有 owner

本地材料统一进入 `build/d03-t05/fixture/`, 其 binary tree、结果及检查脚本也只在 `build/d03-t05/` 分类存放. 临时 CMake 以 EXCLUDE_FROM_ALL 消费正式工程, 显式链接实际需要的 Platform/Shader/RHI native targets; 每份包只有一个生成 target, 消费者依赖该 target 后读取输出. 正式根 CMake 不注册测试目标或样例包.

Platform 继续拥有窗口/current context; Shader 拥有 CPU ABI/包合同; Pipeline/ContextState/访问 tracker/packet 拥有各自现有状态; 本地消费者仅拥有固定输入、外部 RHI 句柄和判定结果. 同步、引用与删除不另建序列或 registry. 如发现可复现生产缺陷, 在对应现有 owner 修复, 先明确风险并加载实际修改所需技能; 没有缺陷就不预设生产重构任务.

选择原因: 本包的新增消费者是验收图案, 而非业务渲染. 把样例写进 `Engine/Renderer` 或 `Shaders/Passes` 会与开发测试存放合同冲突; 为了读回提前实现完整 staging/map 则会改变工作包和审查边界. 现有生产文件没有为 T05 保留一套重复测试 Renderer; 实施时仅清理本包真正引入或发现的测试专用入口, 不机械整理无关文件.

### 2. 固定小图与独立 oracle

主参考为 64×48、单样本 `RGBA32_FLOAT` 与 `D32` 附件, 逻辑 row 0 在顶部. 颜色 clear 为 `(0.0625, 0.0625, 0.0625, 1)`, 深度 clear=0, 主比较 Greater. 无随机、动画、时间驱动或外部资产. schema 定义常量、固定实例记录、Texture/Sampler 和 stage IO; CPU 只用生成的数据/绑定, 不手抄 GPU block 布局. 顶点属性描述仍按公共 InputLayout 表达.

| 可区分标记 | 固定输入与检查点 | 独立预期 |
|---|---|---|
| 四角 | 各角矩形半宽4/半高3, 中心依次为 `(8,6)`、`(56,6)`、`(8,42)`、`(56,42)` | TL=`(.75,.125,.25,1)`, TR=`(.125,.5,.25,1)`, BL=`(.125,.25,.75,1)`, BR=`(.75,.5,.125,1)`; 未画点 `(32,40)` 仍为 clear |
| 实例/索引 | firstInstance=2, instanceCount=4; 前两条及尾后一条实例记录为不同干扰值. R16 索引 binding offset=4 bytes、firstIndex=3、baseVertex=8, 合法 quad 索引为 `0,1,2,2,1,3` | 实际 vertex fetch 与 builtin 分别可判定; InstanceIndex 为2..5、VertexIndex 属8..11, 漏加/重复加偏移产生不同标记; 干扰记录不进入画面 |
| 深度梯度 | 中央 `[16,48)×[16,32)` 先画深度 .5、颜色等于 TL 的底, 再画颜色等于 TR、深度沿 x 从 .25 到 .75 的面, w=1 | 像素 `(20,24)` 保持底色/.5; `(44,24)` 为 TR/`.6953125`; 未画点 `(32,40)` 深度仍0. 数值取像素中心, 不是从读回拟合 |
| L 形 | 白色条 `[16,18)×[4,12)` 与 `[18,24)×[10,12)` | `(17,5)`、`(21,11)` 为白, `(21,5)` 为 clear |
| 正反面 | 左侧单元 `[2,14)×[16,32)` 的模型/NDC CCW 三角形, 右侧单元是相同几何以各自中心做负 x 缩放后的平移副本; 使用 Back cull、按投影后目标绕序选择公开 frontCounterClockwise=false | 左侧内部点 `(5,25)` 有标记, 右侧镜像内部点 `(58,25)` 仍为 clear; 不改模型索引补偿 backend 绕序 |
| scissor | 几何覆盖 `[32,48)×[4,12)`, scissor 限定为 `[36,44)×[4,12)`, 颜色 `(.5,.125,.75,1)` | `(38,8)` 为指定色, `(34,8)`、`(46,8)` 为 clear |

顶点/实例记录包括合法填充与干扰前后缀, 保证所有合法索引在容量内. Shader 中若用 builtin 选择有限数组, 先约束索引, 错误身份只输出错误标记, 不以未定义越界来区分结果. 四角的片元结果同时检查实例身份与 vertex builtin, 不仅靠正确的几何取数证明 lowering. 四角主图 Pipeline 可关闭 cull, 绕序标记使用同一 Shader 的独立 raster 配置; 左侧三角形的逻辑顶点为 `(3,17),(3,29),(13,29)`, z=.6, 右侧按中心 x=56 镜像相应的局部顶点. 非索引 Draw 用于深度/采样部分, 继续消费同一正式执行链.

实施澄清: 上述逻辑顶点的二维 signed area 为负, 经固定 `NDC.y=1-y/24` 投影后为模型/NDC CCW; 公共 RasterState 表达的是目标空间, 因此选 `frontCounterClockwise=false`. 首轮本地消费者误选 true, 左侧被剔除而四角/其它点正确. 此处纠正规划中把模型与目标绕序混称的描述, 保持顶点顺序、clip、正反面像素预期及后端映射不变, 不依实际失败更改 oracle 或建立 API 特判.

oracle 是固定坐标/颜色/身份和独立的线性插值、颜色转换预期, 不从被测 helper、实际输出或 golden 截图生成. 浮点颜色绝对误差≤`2e-4`, D32≤`2e-6`; 只在面内部与明确清除区采样, 排除三角边及 scissor 边的覆盖差异. 不要求整幅光栅图的逐字节跨驱动一致.

### 3. 把目标切换、copy/采样和颜色域合入同一有限链

同一主图 Pipeline 先绘制 A(64×48), 再绘制 B(96×72), 两者格式/样本/质量相同、对象不同. B 使用自身完整 viewport, 参考区域按1.5倍缩放; 内部点用对应像素中心的预期, 不直接比较两幅不同尺寸图的原始字节. 两者先分别完成并读回必要点, 不能以第二次绘制掩盖第一次目标错误. 正式资源访问仍由既有 Automatic 路径建立, 显式模式的逐项验收复用 T03 证据.

把 A 的 `[0,32)×[0,24)` 区域经公共 copy 写入已清除的 C(64×48) 的 `(8,8)`, C/A 格式均为 RGBA32_FLOAT, C 的哨兵 clear 为 `(.03125,.09375,.15625,1)`. 比较 C 的 `(16,14)` 对应 A 左上标记、`(28,28)` 对应 A 中央带底色、`(4,4)` 保留哨兵. 后继公共 Draw 使用独立采样 Pipeline、C 的 SRV 与 nearest sampler, 输出到 D(64×48) 的另一附件, 以 texel 中心对应的 UV 检查相同三点及 L 形. copy 之外不缩放/转换, 采样时不额外翻转; 源不与正在写的目标重叠. 这一序列独自捕获“单项函数正确但跨目标/版本/用途串接错误”.

同一消费者增加一个16×16的 `SRGBA8_UNORM` 目标: 公共 Draw 写入线性 `(.25,.5,.75,.5)`, 存储字节预期 `(137,188,225,128)`, 各允许1个8位量化单位. 随后经正式采样路径写入浮点目标, RGB 与输入线性值的绝对差≤`.006`, alpha 与 `.5` 的绝对差≤`1/255`. 该固定界限包容量化, 能区分漏/重复编码; 不从该次实际字节反算“必然正确”的目标. 不额外遍历 blend/stencil/format 枚举, 这些单项合同已有 T03 证据.

### 4. 明确冷路径读回例外, 不改变完成权威

顺序固定为公共命令 → Close → Execute 得非零 ID → Set Event → 在现有有限期限内 Wait 并 Poll 确认完成 → 本地读回. Wait 为 void, 因此还须核对完成和诊断; 不把新建 Event 的零快照 true 当作本次完成. 失败、超时、输入不匹配或原生读回错误使该次检查失败, 按正式 Host/Device 关闭协议退出, 不永久 Pending 或继续积累结果.

**例外的原因与范围:** 当前公共 staging/map 尚不支持, 而 T04 已允许显式测试/冷路径读回. 因此只有 build 中的观察 helper 可以借用当前设备实现的原生 texture 名称, 使用既有 loader 的 DSA 读取实际 texel. 这是本地验证实现, 不是公共 native 导出 ABI, 不进入 Renderer/Runtime. 原生代码仅用于设备身份查询、完成后的读回与必要 pixel-pack 状态设置/恢复; 不原生 clear/Draw/copy、手编 GL 程序、补 barrier、glFinish 或修正 clip/行序.

helper 预先分配有界 CPU 输出, 显式令 pack buffer 为0并设置紧凑 row/image/skip/alignment 等实际使用的 pack 状态, 完成后恢复其触及状态; 不修改后端已缓存的绘制/unpack/query 状态. CPU 输出始终按项目统一行序解释. 若观察发现状态干扰或行序矛盾, 先隔离观察 helper 与正式路径, 保留失败, 不更新预期使之通过.

### 5. 最小失败检查与停止条件

只新增两个共享集成输入的非法检查: (a) 已知 IB 容量的紧邻越界 DrawIndexed, (b) SRV 与可写 attachment 的重叠 feedback 候选. 每项使用新 recording 和独立目标, 记录 Error 作用域并核对未发生非法绘制/目标变化. 不依赖之前有效状态的回退, 不要求每个 void 拒绝都污染整包; 合法已发 clear 前缀仍可按既有规则提交/完成. 需要观察“未发非法 Draw”时只用本地计数或目标结果, 不增加生产故障开关.

Release 运行完整共享链及上述两项拒绝, 再从独立进程重复一次正向链; Debug 运行同一参考正向链及两项拒绝, 不扩展成参数组合矩阵. 这三次进程运行区分跨进程复现与 Debug/Release 断言差异, 不是性能采样. 已有 T03 的 R32/显式用途等单项与 T04 查询预算/失败退出不重跑, 除非本包实际修改相应输入或发现缺口.

正式默认构建的干净配置/构建结果在其实际输入等价时可以引用前包证据, 写清身份及等价范围; 正式代码/构建输入变化则运行直接受影响目标和消费者. 不以删除维护者整个 build 来证明可清理性, 也不自动恢复旧测试. 共享链覆盖所有 delta 场景、两项拒绝有证据、正式隔离性已核对且所需审查完成即停止; 只在失败、输入变化或具体独立风险缺口时扩大检查.

### 6. 证据与后续复用的边界

本包记录基准 commit、实际 dirty/untracked 输入, fixture/schema/Shader 内容身份, package content/ABI、构建配置/flags、exe 身份和设备/驱动. 先确认当前输入对应实际构建的 exe 再运行; 原始失败/重试及未覆盖范围保存在 tasks, 不运行旧 exe 后称为当前实现. 复用已归档检查要指出其独立合同、原输入与本包适用性, 不能用 T03/T04 局部成功替代新集成图案的实际运行.

本地文件可删除, 复现从本设计的固定输入、tasks 中的最终补充及维护者另行提供的实际 fixture 开始; 路径本身不是永久可执行入口. 后续 D04/D05 复用的是正式编译、绑定、提交/完成 API 与参考图案语义, 不是把 build 文件变成生产依赖.

独立 B 按 [review 技能](../../../../.agents/skills/physara-review/SKILL.md) 审查本轮正式 diff、合同及交接证据, build 生成物/测试材料不列入代码审查范围. 如本包没有修改正式 C++/Shader/CMake, B 明确代码/注释适用范围而非虚构新增文件覆盖; 若有实际修复, 对其完整第一方文件完成审查和注释. A 不自签 B 的结论. B 最终验收后才最终 verify、同步本 delta、归档及重置问题单.

### 7. B 首轮审查后的存量修正

B 在直接绑定 owner 发现 CR-001, 由独立 [范围修正 change](../2026-11-08-fix-opengl-binding-buffer-ranges/design.md) 承接已归档 D03-T02 的零长度语义缺陷. T05 的图案、Shader/JSON、ABI和容差不变, 受影响消费者改用合法的零长度绑定重新验证; 本地 CB 分配容量为256、生成payload仍64, 继续满足既有公共输入约束. 正式修改及其 delta 属于修正 change, 本包留存新的共享输入和回归, 不把原 A 接收身份当作 B 注释/整改后的当前身份.

同一问题单关联两份 change, B 最终复核确认对应范围并自行签署其任务. 之后分别 verify/sync/archive, 两份关联范围均归档及留存完成后才重置问题单.

## Risks / Trade-offs

- [新门槛与既有检查重复] → 原有九条 requirement 不复制; 新 delta 只保证集成输出, 有限图案和两个拒绝共享输入, 不新建测试基础设施.
- [oracle 受同一错误污染或容差掩盖缺陷] → 先固定坐标、记录身份和数值预期, 检查面内部/清除区, 分域容差; 意外失败先定位而非调整阈值.
- [原生观察被误当作公共能力] → helper 仅在 build、真实完成之后使用, 不发绘制/同步补丁; 公共 staging/map 与产品呈现仍不支持.
- [本机通过被误报为普适正确或性能结论] → 记录实际设备/驱动和构建, 不声称跨 GPU/系统、自然 OOM/loss、长期压力或性能; 必要环境缺失保留待验收.

## Migration Plan

1. 实施阶段只准备 build 下的消费者与本包记录; 如有生产缺陷, 在原 owner 定点修复并提供匹配证据, 不改变模块方向.
2. A 自检后交维护者调度 B, 保留实际输入/结果与未关闭问题; 整改及 B 复核完成前不验收 D03 出口.
3. B 最终验收及最终 verify 通过后同步/归档, 更新 T05/D03 出口事实并指向归档; 后续进入 D04-T01, 不在本包提前实施. 若中止本包, 保留问题与未完成状态, build 本地材料不成为正式构建前提.
