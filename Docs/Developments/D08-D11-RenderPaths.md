# D08–D11 · 增量准备、阴影与三条渲染路径

[开发计划与当前阶段](Developments.md#current-stage) · [D08](#dev-d08) · [D09](#dev-d09) · [D10](#dev-d10) · [D11](#dev-d11)

<a id="dev-d08"></a>
## D08 · 完善准备、资源预算与执行能力

**性质：** 基线。**入口：** [D07](D06-D07-SceneAndForward.md#dev-d07) 的阶段出口通过。

**阶段目标：** 在现有可用画面的基础上完成大场景所需的增量准备、批处理、图资源池、完整基础 GL 命令和热重载。

<a id="d08-t01"></a>
### D08-T01 · 完成增量 SceneFrame 与多 View 工作集

**职责位置：** Renderer/Prepare、FrameSlot。**前置：** 本阶段入口满足。**架构依据：** [5.2](../Architecture.md#prepare)、[5.3](../Architecture.md#view-preparation)、[5.1](../Architecture.md#frame-state)。

- [ ] 按 revision/合并脏集合更新各 FrameSlot 保留的对象、Primitive、灯提取数组，只在槽可复用时修改并只读发布；结构代变化重建索引与对应上传，相机/View uniform 独立更新，不因 arena reset 重新编码未变场景。
- [ ] 明确 SceneFrame 共享与每 View 可见索引边界，为多个离屏 View 建立独立输出/设置及一次场景提取。
- [ ] 记录提取/裁剪成本、arena 高水位和增长，测试多帧槽轮转、大对象数、空场景、多 View、稀疏编辑及结构增删；验证脏集合有界、空闲不重打包，旧槽不借用已复用 arena。

**完成条件：** 增量性可由计数与版本证明，不以无效的“每帧都脏”缓存保持正确。

<a id="d08-t02"></a>
### D08-T02 · 完成版本化驻留与安全高效上传

**职责位置：** Renderer/Materials、Lighting、RHI/OpenGL Upload。**前置：** [D08-T01](#d08-t01)。**架构依据：** [5.5](../Architecture.md#gpu-residency)、[7.4](../Architecture.md#uploads)、[11.1](../Architecture.md#retirement)。

- [ ] 把初始缓存扩充为受预算约束的 Geometry pages、Texture views、Material tables/Binding caches 与环境版本；有效 mip 层级和 sampler LOD 配套，颜色/数据过滤语义分开，查询环境朝向/强度不触发重复卷积。
- [ ] 实现 persistent/coherent 区段、volatile CB 与 push-constant 兼容区；明确保留 binding 范围，避免与 lowering 分配冲突。
- [ ] 把 recording/submission/completion 关联到区段/版本复用，合并脏上传 range；场景上传包在 Draw 阶段追加 View/实例/indirect 数据后封存并检查全部准备结果，UI 上传独立声明，裁剪或失败不确认上传版本；测试小容量环回、在途改材质/重导入及超预算。

**完成条件：** 不覆盖在途 GPU 数据，缓存与上传区段均有容量、超限策略、失效与退休协议。

<a id="d08-t03"></a>
### D08-T03 · 完成 Draw 排序、实例与间接批次

**职责位置：** Renderer/Draw、RHI/OpenGL Graphics。**前置：** [D08-T02](#d08-t02)。**架构依据：** [5.6](../Architecture.md#draws)、[7.6](../Architecture.md#command-execution)。

- [ ] 以完整兼容 key 排序/分组不透明 Draw，稳定处理 hash 碰撞、ID 宽度溢出、不同纹理/sampler 和几何页。
- [ ] 生成实例表及 indexed indirect 参数，按能力实现基础 profile 所需 indirect/MDI；不以 bindless 假设跨材质无限合批。
- [ ] 验证非零 firstInstance/baseVertex、大材质数、实例索引和结果等价；同步记录 Draw、bind、上传与批次数。

**完成条件：** 批处理保持像素与索引语义，execute 不反射、编译或创建临时 Pipeline。

<a id="d08-t04"></a>
### D08-T04 · 实现 compute 与多消费域同步一致性

**职责位置：** RHI/OpenGL。**前置：** [D08-T02](#d08-t02)。**架构依据：** [7.6](../Architecture.md#command-execution)、[7.5](../Architecture.md#barriers)、[7.7](../Architecture.md#capabilities)。

- [ ] 实现 Renderer 后续需要的 ComputePipeline/dispatch、SSBO/image 与图内计算接口，校验资源布局和工作组/访问 limits。
- [ ] 验证同资源多种消费域的写入/可见版本与未完成 Shader 读访问，覆盖 compute→texture、indirect、buffer update、copy、连续写读及读→后续写，不用单个 pending bool 掩盖 WAR。
- [ ] 扩充 storage 格式/usage、subresource/range、dispatch 边界和 unsupported 正反用例，更新 capability 事实表。

**完成条件：** 后续计算 Pass 可由公共 RHI 表达，正确性不依赖全局重 barrier 或全设备等待。

<a id="d08-t05"></a>
### D08-T05 · 完成传输、staging 与非阻塞 queries

**职责位置：** RHI/OpenGL Transfer/Queries。**前置：** [D08-T02](#d08-t02)、[D08-T04](#d08-t04)。**架构依据：** [7.6](../Architecture.md#command-execution)、[11.3](../Architecture.md#readbacks)、[7.7](../Architecture.md#capabilities)。

- [ ] 完善 clear/copy、mip 生成、基础支持格式的 resolve、Buffer/Texture staging 与真实 row pitch，覆盖数组/cube/mip/range。
- [ ] 实现 EventQuery 捕获提交与可轮转 TimerQuery，提供 available/pending 结果及有界查询容量，正常帧不阻塞获取。
- [ ] 验证 transfer 与 graphics/compute 混合顺序、不同消费域、读回格式/行对齐、取消和在途退出，更新真实格式/样本能力。

**完成条件：** 后续拾取/截图/计时复用正式传输查询链，未完成数据不能提前 map 或报告成功。

<a id="d08-t06"></a>
### D08-T06 · 完成图资源池、跨帧导入与预算

**职责位置：** RenderGraph、RenderEngine 资源池。**前置：** [D08-T02](#d08-t02)、[D08-T04](#d08-t04)、[D08-T05](#d08-t05)。**架构依据：** [6.4](../Architecture.md#graph-resources)、[6.3](../Architecture.md#graph-compile)、[11.4](../Architecture.md#diagnostics)。

- [ ] 按格式/usage/尺寸/样本与真实使用区间复用兼容对象，首次使用 materialize，建立池容量与回收高水位。
- [ ] 完善外部/history/UI/readback 导出、状态回传、子资源活性与 resize 换代，初期仍为单队列且不做 heap aliasing。
- [ ] 压力测试尺寸/格式来回切换、无活性分支、资源池预算耗尽及在途对象保留，发布图摘要供诊断。

**完成条件：** 图对象复用与 GPU 安全可同时证明，缓存增长不会因持续 resize 无界累积。

<a id="d08-t07"></a>
### D08-T07 · 实现有限变体、完整代热重载与准备预算

**职责位置：** Engine/Shader、Renderer/Paths、View、Editor/Diagnostics。**前置：** [D08-T02](#d08-t02)、[D08-T04](#d08-t04)。**架构依据：** [5.7](../Architecture.md#plan-readiness)、[1.3](../Architecture.md#shader-build)、[8.1](../Architecture.md#shader-runtime)。

- [ ] 建立 schema 化 VariantKey 与允许组合、含 include/工具/ABI 及 native 设备驱动身份的 cache key 和明确容量；数值参数仍走数据。
- [ ] 后台处理 CPU 编译，在 GL context 线程按预算 link/建 Pipeline；完整 ABI/绑定/资源代就绪后帧边界发布。
- [ ] 随热重载接入文件变更通知及事件合并；检查 Shader 错误、include 修改、ABI 变化、快速重复请求及在途旧代，失败保留旧完整代并展示源行诊断。

**完成条件：** 热重载和计划准备不在 Draw 内阻塞编译，Pending/Failed 与 Active 有可追踪状态。

<a id="d08-t08"></a>
### D08-T08 · 验证扩展执行链和空闲成本

**职责位置：** RHI、Renderer/RenderGraph、Renderer、Editor/Diagnostics。**前置：** [D08-T03](#d08-t03)、[D08-T04](#d08-t04)、[D08-T05](#d08-t05)、[D08-T06](#d08-t06)、[D08-T07](#d08-t07)。**架构依据：** [12.1](../Architecture.md#tests)、[11.4](../Architecture.md#diagnostics)、[12.2](../Architecture.md#benchmark)。

- [ ] 对冻结场景、稀疏编辑、多 View、连续加载和 Shader 重载运行集成测试，核对上传/创建/绑定与池统计。
- [ ] 验证打开/隐藏统计面板不改变事实归属，未就绪 GPU timer 为 Pending，不触发同步查询。
- [ ] 以同画质比较增量/批处理前后的成本；没有测得收益的复杂优化回到对应 change 重新评估。

**完成条件：** 后续特性建立在已测量、可诊断且寿命可靠的准备/执行链上。

**D08 阶段出口：** 固定场景空闲时无无谓重建/上传，变更只更新相关版本；批次与原生调用可核对，缓存有预算，原生同步与在途重载测试通过。

<a id="dev-d09"></a>
## D09 · 实现完整基础阴影链

**性质：** 基线。**入口：** [D08](#dev-d08) 的阶段出口通过。

**阶段目标：** 将保守阴影 CPU 计划、资源预算、静态几何/遮罩绘制及 Hard/PCF 采样接入共享灯光模型。

<a id="d09-t01"></a>
### D09-T01 · 实现阴影计划、分配与缓存失效

**职责位置：** Renderer/Lighting、Prepare、View、Editor/Panels。**前置：** 本阶段入口满足。**架构依据：** [5.4](../Architecture.md#light-preparation)、[8.3](../Architecture.md#shadows)。

- [ ] 从稳定有限相机范围生成阴影视图和保守 caster 集，包含相机画外但影响可见区域的物体。
- [ ] 建立方向/spot/point 的 region/层分配、质量和字节预算、dirty 更新与稳定 light/shadow 索引。
- [ ] 定义预算耗尽、灯删除、尺寸变化和阴影禁用的有效状态及统计；静态结果按相关版本失效。

**完成条件：** 阴影可从 CPU 计划追溯到实际 light/region，不把主视图可见集直接当 caster 集。

<a id="d09-t02"></a>
### D09-T02 · 实现稳定方向光 CSM

**职责位置：** Renderer/Passes、Lighting、Shaders/Shadow。**前置：** [D09-T01](#d09-t01)。**架构依据：** [8.3](../Architecture.md#shadows)、[8.2](../Architecture.md#geometry)、[conventions](../Architecture.md#conventions)。

- [ ] 实现 1–4 cascade split，透视线性/对数混合、正交线性划分；完整 receiver 包围球固定 XY 尺度并按世界 texel 吸附，Z 包含画外 caster，使用常规 shadow depth clear/compare 与 caster bias。
- [ ] 通过统一几何/alpha Shader 生成深度，处理负缩放、静态实例、mask 和双面投射；由图声明深度写→采样。
- [ ] 实现级联选择/过渡与调试可视化，测试近远范围、慢速相机移动、画外 caster 和 cascade 边界。

**完成条件：** CSM 深度语义独立于主视图 Reverse-Z，稳定性和遮挡正确性有固定场景验证。

<a id="d09-t03"></a>
### D09-T03 · 实现 spot 与 point shadow

**职责位置：** Renderer/Lighting、Passes、Shaders/Shadow。**前置：** [D09-T01](#d09-t01)、[D09-T02](#d09-t02)。**架构依据：** [8.3](../Architecture.md#shadows)、[8.6](../Architecture.md#lighting)。

- [ ] 实现 spot 投影和 point 六面视图、层/atlas 地址及统一近远面；光源无限 range 与有限 shadowDistance 分开，比较量与写入使用相同投影，不能混用径向距离和非线性深度。
- [ ] 共享几何、bias 和采样合同，处理 cubemap/atlas 接缝、越界及失效 region。
- [ ] 验证编辑光源/静态物体后的阴影更新、面边界、未指定 range 的光源规划及有界分配的降级显示。

**完成条件：** 三类 punctual/方向灯均能产生可查询可见性，未分配阴影不会读取无效纹理。

<a id="d09-t04"></a>
### D09-T04 · 接入 Hard/PCF 与统一光照查询

**职责位置：** Shaders/Shadow、Lighting、Renderer/Paths。**前置：** [D09-T02](#d09-t02)、[D09-T03](#d09-t03)。**架构依据：** [8.3](../Architecture.md#shadows)、[8.6](../Architecture.md#lighting)、[8.7](../Architecture.md#opaque-shading)。

- [ ] 实现基础 Hard/PCF 的深度比较、接收偏移、过滤质量和 cascade/spot/point 查询；PCF 过滤比较结果，atlas guard band 与 point 跨面采样覆盖最大核，明确 Poisson 仅为采样分布。
- [ ] 让共享 EvaluateLighting 消费同一 visibility 接口，材质响应与阴影采样解耦。
- [ ] 接入质量/预算 UI 和统计，比较开关阴影的 HDR 输出及性能，PCSS/moments 留到可选 profile。

**完成条件：** 基础阴影与灯光、材质和环境参数相互独立，后续路径无需复制阴影实现。

<a id="d09-t05"></a>
### D09-T05 · 建立阴影回归与资源压力门槛

**职责位置：** Renderer、RenderGraph、RHI。**前置：** [D09-T01](#d09-t01)、[D09-T02](#d09-t02)、[D09-T03](#d09-t03)、[D09-T04](#d09-t04)。**架构依据：** [12.1](../Architecture.md#tests)、[8.3](../Architecture.md#shadows)、[12.2](../Architecture.md#benchmark)。

- [ ] 固定 CSM 稳定、spot 边缘、point 六面、薄片/mask、负缩放和多实例 和画外 caster 回归。
- [ ] 压力测试大灯数、region 重分配、关闭阴影、resize 及在途资源退休，核对图访问与实际成本。
- [ ] 建立不同质量下的画质/成本参考，后续 Forward+/Deferred 对同一阴影结果做一致性检查。

**完成条件：** 阴影具有可重现的质量与预算边界，不依赖手动观察单个静态场景。

**D09 阶段出口：** 方向/聚光/点光阴影在 Forward 可用；画外 caster、级联边界、接缝、深度 convention、预算不足与静态覆盖正确。

<a id="dev-d10"></a>
## D10 · 实现 CPU Froxel 与 Forward+

**性质：** 基线。**入口：** [D09](#dev-d09) 的阶段出口通过。

**阶段目标：** 保持物理模型不变，以 Filament 式 CPU Froxel 改变局部灯候选集合，形成第二条可切换路径。

<a id="d10-t01"></a>
### D10-T01 · 实现有限视域 Froxel 结构与 CPU 分箱

**职责位置：** Renderer/Prepare、Lighting。**前置：** 本阶段入口满足。**架构依据：** [5.4](../Architecture.md#light-preparation)、[5.3](../Architecture.md#view-preparation)。

- [ ] 定义与 Shader 共用的 Froxel 参数、header/index ABI 和 32 位索引语义，网格与有限稳定裁剪范围关联；透视用正 near 的 log-Z，正交用可接受 near=0 的线性 Z，CPU cell 边界与 Shader lookup 配套。
- [ ] 对有界点/聚光灯做保守相交与列表压缩，directional 和未指定 range 的局部灯进入明确的全局候选集合。
- [ ] 测试透视/正交及 near=0、边界 cell、极近/极远、spot、空集合、相机变化和大灯数，比较 CPU 候选覆盖参考。

**完成条件：** 网格不会因透视渲染使用无限远而丢失有限分箱定义，索引宽度与容量不隐式截断。

<a id="d10-t02"></a>
### D10-T02 · 实现溢出退路与图内上传

**职责位置：** Renderer/Lighting、UploadBatch、Shaders/Lighting。**前置：** [D10-T01](#d10-t01)。**架构依据：** [5.4](../Architecture.md#light-preparation)、[5.5](../Architecture.md#gpu-residency)、[7.4](../Architecture.md#uploads)。

- [ ] 按网格/索引预算生成正常或 overflow header，溢出 Shader 改走完整可见灯列表，不截断光源。
- [ ] 把 headers/indices/global lights 放入拥有型上传包，图声明写入和后继 ShaderResource 访问。
- [ ] 暴露溢出次数、索引量和 fallback 成本，强制小预算测试溢出输出与 full-list 一致。

**完成条件：** 内存压力影响成本和可见状态，不能改变场景中的光照内容。

<a id="d10-t03"></a>
### D10-T03 · 实现 Forward+ 子图与计划准备

**职责位置：** Renderer/Paths、Shaders/Passes、Engine/Shader。**前置：** [D10-T02](#d10-t02)。**架构依据：** [6.1](../Architecture.md#paths)、[8.7](../Architecture.md#opaque-shading)、[5.7](../Architecture.md#plan-readiness)。

- [ ] 用同一 PrepareSurface 与共享 EvaluateLighting 接入 Froxel candidate provider，材质/阴影/IBL 代码不分叉。
- [ ] 准备新的结构变体与必要绑定，沿既有 Ready/Pending/Failed 协议进入 View 计划。
- [ ] 在当前已支持 None/FXAA 配置下于帧边界切换，保留几何/材质驻留和编辑状态。

**完成条件：** Forward+ 是同一 Renderer 的不透明策略，不创建第二套 Scene、后处理或资源管理器。

<a id="d10-t04"></a>
### D10-T04 · 实现双路径原子切换与正确历史失效

**职责位置：** View、Renderer/Paths、Editor/Panels。**前置：** [D10-T03](#d10-t03)。**架构依据：** [5.7](../Architecture.md#plan-readiness)、[11.2](../Architecture.md#history-commit)、[2.3](../Architecture.md#edits)。

- [ ] 实现 requested/effective 路径、准备中继续有效计划、失败原因及切换完成的原子发布。
- [ ] 只替换布局/格式相关资源并按兼容性失效历史，禁止 WaitForIdle 或重新加载场景。
- [ ] 测试快速重复请求、编译失败、相机移动中切换、resize 与多 View 独立切换。

**完成条件：** UI 显示的有效路径与真实执行一致，未就绪请求不被伪装为成功。

<a id="d10-t05"></a>
### D10-T05 · 验证 HDR 等价与多灯成本

**职责位置：** Renderer、Runtime、Editor/Diagnostics。**前置：** [D10-T01](#d10-t01)、[D10-T02](#d10-t02)、[D10-T03](#d10-t03)、[D10-T04](#d10-t04)。**架构依据：** [12.1](../Architecture.md#tests)、[12.2](../Architecture.md#benchmark)、[8.6](../Architecture.md#lighting)。

- [ ] 固定同一 SceneFrame、曝光、阴影、IBL、分辨率和画质，对两路径线性/HDR 输出做误差比较。
- [ ] 包含 range 缺省、overflow、极端相机、聚光边界和不同光源规模，核对灯集合无漏项。
- [ ] 分别测量 CPU 分箱、上传和 GPU 灯循环，保留 fallback 比例，不只记录总 FPS。

**完成条件：** Forward+ 的收益与代价可以解释且没有通过丢灯获得，第二路径可作为后续 Deferred 参照。

**D10 阶段出口：** Forward+ 与完整灯循环 Forward 在同输入 HDR 下等价；无穷 range、列表溢出、相机范围和空灯情况均正确。

<a id="dev-d11"></a>
## D11 · 接入 Deferred 并完成三路径切换骨架

**性质：** 基线。**入口：** [D10](#dev-d10) 的阶段出口通过。

**阶段目标：** 实现紧凑核心 GBuffer、扩展表面辅助合同和共享 deferred lighting，保持完整上下游一致。

<a id="d11-t01"></a>
### D11-T01 · 实现固定核心 GBuffer 编解码

**职责位置：** Renderer/Passes、Shaders/Material/Passes。**前置：** 本阶段入口满足。**架构依据：** [8.2](../Architecture.md#geometry)、[8.5](../Architecture.md#materials)。

- [ ] 实现 G0 RGBA8_SRGB、G1 RGBA16F、G2 R11G11B10F、G3 R16_UINT 的 18 B/pixel 核心布局，depth 单独管理。
- [ ] 共享材质采样和法线/roughness/metallic/AO/预曝光 emissive 编解码，清理 DeferredEligible 等 flags 的默认值。
- [ ] 验证 sRGB、量化/极值误差、alphaMask、双面、静态实例和材质覆盖；不扩成承载所有扩展的大型 GBuffer。

**完成条件：** GBuffer 每字段和误差可追溯到共享核心模型，正确性不依赖偶然 clear 值。

<a id="d11-t02"></a>
### D11-T02 · 实现 SurfaceAux 与 ForwardExtension 路由

**职责位置：** Renderer/Draw、Paths、Passes、Shaders/Passes。**前置：** [D11-T01](#d11-t01)。**架构依据：** [8.2](../Architecture.md#geometry)、[5.6](../Architecture.md#draws)、[6.1](../Architecture.md#paths)。

- [ ] 在核心 GBuffer 后为 Unlit/非核心不透明 profile 写真实 depth、normal/roughness，并清除 DeferredEligible。
- [ ] DeferredLighting 只照明 eligible 像素，ForwardUnlit/ForwardExtension 在同一表面深度下输出各自颜色。
- [ ] 以 Unlit 和已有 profile 验证遮挡、覆盖、边界；为 D12 扩展提供相同测试入口。

**完成条件：** AO/SSR 未来读取的是实际最近表面，扩展不会既被 deferred 照明又被 forward 重画。

<a id="d11-t03"></a>
### D11-T03 · 实现共享 DeferredLighting 与子图

**职责位置：** Renderer/Paths、Shaders/Lighting/Passes、RenderGraph。**前置：** [D11-T01](#d11-t01)、[D11-T02](#d11-t02)。**架构依据：** [8.7](../Architecture.md#opaque-shading)、[8.6](../Architecture.md#lighting)、[6.1](../Architecture.md#paths)。

- [ ] 由 GBuffer/depth 重建共享 PixelParams/几何状态，使用既有灯、Froxel、阴影和 IBL EvaluateLighting。
- [ ] 按 PrepareSurface→表面效果输入→ShadeOpaque 的合同组装，输出统一 OpaqueOutput；当屏幕效果未启用使用明确默认输入。
- [ ] 验证 attachment→sampling、联合只读深度用途、无效背景和 sky 拼接的图访问与原生结果。

**完成条件：** Deferred 只改变表面存取和不透明子图，光照公式与后处理没有第三份实现。

<a id="d11-t04"></a>
### D11-T04 · 完成三路径配置兼容与事务切换

**职责位置：** View、Renderer/Paths、Engine/Shader、Editor/Panels。**前置：** [D11-T03](#d11-t03)。**架构依据：** [5.7](../Architecture.md#plan-readiness)、[6.1](../Architecture.md#paths)、[7.7](../Architecture.md#capabilities)。

- [ ] 把三策略注册到固定组装入口，核对每路径输出、布局、资源准备和当前 AA/效果支持矩阵。
- [ ] 在已有 None/FXAA 下完成三路径切换；TAA 待 D15 验收再启用，Deferred+MSAA 能力保持 false。
- [ ] 测试 Pending、失败、快速循环切换、多 View、相机移动和分辨率变化；只使不兼容历史失效且不全设备等待。

**完成条件：** 同一场景与驻留资源可跨三路径复用，requested/effective 和不支持原因完整可见。

<a id="d11-t05"></a>
### D11-T05 · 建立三路径等价与可视化门槛

**职责位置：** Renderer、Editor/Diagnostics。**前置：** [D11-T01](#d11-t01)、[D11-T02](#d11-t02)、[D11-T03](#d11-t03)、[D11-T04](#d11-t04)。**架构依据：** [12.1](../Architecture.md#tests)、[12.2](../Architecture.md#benchmark)、[10.1](../Architecture.md#editor-output)。

- [ ] 对核心/Unlit、mask、静态实例、阴影、无穷灯及 Froxel overflow 做同输入 HDR 等价回归。
- [ ] 提供 GBuffer/深度/法线/路由可视化的 typed preview Pass，UI 不直接解释任意纹理位型。
- [ ] 记录附件字节、带宽相关指标、ForwardExtension 占比和 CPU/GPU 成本，形成首次三路径可比结果。

**完成条件：** 三路径在已启用能力上可比较，新增扩展和时域效果后沿同一用例继续补齐。

**D11 阶段出口：** 三路径在当前支持 profile 下可原子切换；核心材质 HDR 等价，Unlit/扩展表面遮挡正确，GBuffer 与 SurfaceOutput 语义可检查。
