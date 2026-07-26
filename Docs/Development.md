# Physara 开发计划

> 本文档是 Physara 渲染器"重构主干 + 纠错优化 + 功能拓展"的完整任务分解。
> 编号规则:`模块号.子任务号`(如 `4.2`),与 `Docs/TaskDetail.md` 中的方案存档一一对应,修改任一文档时请保持编号同步。
> 状态标记:`[ ]` 未完成,`[√]` 已完成。子任务描述只写"要达成的核心目标",不展开具体实现步骤(实现方案见 `TaskDetail.md`)。
>
> 制定依据:对 `Engine/Renderer`、`Engine/RHI`、`Backend/OpenGL`、`Assets/Shaders`、`Editor`、`Platform`、`Engine/Scene`、`Engine/Resource` 的代码级审查,以及 `Docs/FilamentRenderSide.md`、`Docs/FilamentAppSide.md` 中 Filament 材质/光照/相机模型与 CPU-GPU 协同经验的对比分析。本计划已经过两轮针对源码的批判性复核(第一轮:纵向渲染算法纠错;第二轮:横向架构隐患扫描,含 RHI 可扩展性、Editor 架构、跨层参数打包、CPU↔GPU 契约漂移、Platform/Scene/Resource 卫生),见文末"复核修订记录"。
>
> ⭐核心工作流-每任务落地原则：每项子任务开始时都遵循流程：任务吸收-问题再核验-方案选型-任务落地-结果审查-再验证-任务签收。
> 1. **任务吸收**——该阶段先接取任务，确认任务范围和上下文，明确任务目标。
> 2. **问题再核验**——在任务吸收后,对任务范围内的现有代码进行批判性复核,确认是否存在该问题。
> 3. **方案选型**——在问题再核验后,根据任务目标和现有代码进行方案选择，优先现代最佳引擎实践，架构型的完善避免补丁式更改。
> 4. **任务落地**——在方案选型后,根据任务目标和现有代码进行具体实现。
> 5. **再验证**——在任务落地后,对任务目标和现有代码进行具体实现的结果进行审查，明确方案是否解决问题。
> 6. **任务签收**——在再验证后,如果实现方案通过，那么开始更新文档内容进行对应记录；否则将当前失败方案和结果按顺序记录到TaskDetail对应位置中，然后从方案选型再次开始该工作流
>
> 顺序原则:
> 1. **测量先行**——任何性能相关改动之前,先用现有 per-pass GPU/CPU 计时与 benchmark 定位真实瓶颈,禁止基于假设排定高风险优化的优先级(见阶段 0)。
> 2. **地基优先**——先打通 RHI 提交层与公共基础设施,再自下而上重构渲染管线各模块。
> 3. **重构与纠错合并**——同一批文件的架构调整与已知算法/性能问题在同一阶段一次做透,避免重复返工。
> 4. **验证护栏**——每个模块重构前后以 golden image 比对画面,GPU 契约变更后运行契约校验脚本(见阶段 0)。
> 5. **拓展建立在稳固地基之上**——材质模型与物理光照/相机拓展安排在阶段一收尾之后。
> 6. **横向地基优先于纵向拓展**——跨层的架构隐患(契约防漂移、序列化单点维护、枚举映射统一、上帝类拆分)会拖累后续每一个纵向任务,须尽早处理;其中 `模块 P.4`(CPU↔GPU 契约防漂移)与 `模块 H2.1`(序列化单点维护)是阶段二拓展的硬前置。

---

## 阶段 0 — 前置基建:测量与验证(须先于一切重构)

当前"简单场景仅 45fps"的结论尚未定位到具体瓶颈。默认场景仅 3 个内建图元 + 3 个光源,CPU 侧 draw 提交量极小,因此瓶颈**更可能在 GPU 侧**(Bloom 多 pass、CSM、Deferred 全屏、可能的 MSAA),而非 CPU 提交。项目已内置完整的 per-pass GPU/CPU 计时(`FrameData.hpp` 的 `RendererGPUTimingScope` + `Renderer.cpp:961-973`)与 benchmark(median/P95),必须先读数再决策。

### 模块 P — 性能基线与验证基建

- [√] P.1 读取现有 per-pass GPU/CPU 计时与 benchmark(median/P95),判定当前帧时为 CPU-bound 还是 GPU-bound、定位主导 pass;并对比 wall-clock 帧时与 `gpuFrameMs + cpuMs` 之和,排除 VSync/present 限制(45fps 需确认是否为垂直同步半刷新伪像)
  - **实测(2026-07-25,默认场景 1154×673,VSync 关,2.49M tris):FPS 43,GPU med/p95 = 22.68/24.18ms,CPU med/p95 = 12.47/43.15ms → 由 GPU 封顶,判定 GPU-bound(非 VSync)。GPU 分解:Shadow 15.48ms(≈64%)| Forward 7.97 | Post 0.57 | Sky/GBuf/Light/Trans/Grid ≈0。Shadow Pass 152 draw(4 级联 × ~38 caster,零裁剪)。CPU p95 尖刺 43ms(median 仅 12.47)= 周期性 CPU 卡顿,疑与动态 buffer VRAM↔HOST 迁移或 PSO 编译相关,待查。**
- [ ] P.2 建立视觉回归基建:用 `RendererCapture` 对默认场景产出基准 golden image,约定每个模块重构前后比对;凡涉及 GPU 数据布局/契约变更的任务,强制运行 `Tools/verify_gpu_contracts.py`
  - **[部分实现 · change pre-refactor-validation-guards]** 比对器 `Tools/compare_golden.py` 已完成(容差 diff + diff 图 + `--update` 显式刷新)。用法:`python Tools/compare_golden.py <candidate.png> <golden.png> [--tol 8 --fail-ratio 0.002 --diff out.png]`。引擎确定性捕获编排(固定场景/相机/分辨率 + IBL-ready 门控 + warmup)待构建落地。
- [√] P.3 依据 P.1 结论裁定/微调阶段一模块顺序。**裁定(据 P.1 实测 GPU-bound):阶段一 fps 序 = 模块 5 Shadow(#1,独占 GPU 64%,收益最大)→ 模块 6.2/6.4 Deferred → 模块 7 IBL/Cluster;模块 8 Bloom 实测仅 0.57ms、fps 优先级降级(保留为代码整洁,非性能);模块 0 CommandStream 维持"架构地基",fps 上不抢跑,其解耦子项(0.6–0.15)仍按地基推进。原假设"Bloom 是第二嫌疑""CommandStream 救 fps"均被实测推翻。**
- [√] P.4 **[关键·契约防漂移]** 扩展 CPU↔GPU 契约校验以覆盖**结构体字段布局**(现 `verify_gpu_contracts.py` 只校验枚举/常量数值,不校验 `CameraData`/`LightData`/`MaterialGPUData`/`ObjectData`/`ShadowData`/`FrameUniforms` 的字段顺序/类型/std140-std430 对齐)。方案任选:编译期 `offsetof`/`static_assert` 对照、或脚本解析 C++ struct 与 GLSL struct 比对、或单源代码生成两端定义。**此项必须先于模块 4.2 的布局重设计**,否则布局改动将静默漂移导致画面全错却不报错(`ObjectData` 现有 4×uint32→uvec4 隐式映射即为无人校验的高危点)。同时(第四轮)将校验扩到 **binding 号一致性**(C++ `GPUBufferBinding`/`GPUTextureBinding` ↔ GLSL `Common.glsl` 的 `PHYSARA_BINDING_*` 手工同步、无防护),并清理死枚举(`GPUBufferBinding::Shadow=6`/`IBL=7` 实际数据嵌在 FrameUniforms binding 0、从未被引用;`binding=4` 被 4 个枚举语义重载)
  - **[已实现 · change pre-refactor-validation-guards]** 编译期 `sizeof`+逐字段 `offsetof` 布局锁定(`GPUContracts.hpp`)+ `verify_gpu_contracts.py` 扩展结构体字节布局/binding/死枚举校验。用法:`python Tools/verify_gpu_contracts.py`(已通过;额外发现 `RenderSettings` 亦为死枚举)。布局锁定的编译期验证待 VS 构建确认。

---

## 阶段一:重构主干与纠错

### 模块 0 — RHI / Backend 提交层(架构地基)

当前 `OpenGLCommandList` 对每个 RHI 调用都同步下发 GL 命令,CPU 无法领先 GPU 工作(对比 Filament `CommandStream` 机制,详见 `FilamentAppSide.md` 7.3)。此模块是 RHI 抽象层的正经地基,也是未来复杂场景/编辑器多物体扩展的前提。

> **排位说明(经复核修订)**:此模块原被定为"45fps 头号嫌疑"。但默认场景仅 3 个 draw,CPU 提交开销极小,CommandStream 对当前场景 fps 收益很可能接近于零。其价值在于**架构地基**而非即时性能修复。是否维持"第一模块"由 P.1/P.3 的测量结论最终裁定。
>
> **多后端范围(经复核决策)**:第二轮扫描评估"增加 Vulkan/D3D12 痛苦指数 4.5/5"(当前为 GL 式即时命令模型,缺 swapchain/fence/command-buffer/descriptor-set/renderpass 抽象)。**决策:YAGNI——暂缓多后端重型抽象**(rules.mdc 明确仅 OpenGL 4.6)。本模块只做"无论单/多后端都划算"的解耦(0.6–0.9),不引入 swapchain/fence/descriptor 等新抽象;日后若真要加后端再单列大模块。

- [ ] 0.1 引入命令缓冲机制,解耦 `RHICommandList` 调用录制与 GL 实际执行时机(参考 Filament CommandStream + CircularBuffer 思路)
- [ ] 0.2 完善 `OpenGLState`(现 `OpenGLCommandList.hpp` 内 `GLState`)状态缓存覆盖面,统一走缓存路径,支持按类别 `Invalidate`(而非现在的 `InvalidateExternalState()` 全量清空)
- [ ] 0.3 提交热循环增加 pipeline / render primitive / descriptor 变化去重缓存,避免相邻 draw 重复下发相同状态(注:此为 RHI/后端层的"忽略冗余 GL 调用",与模块 3.2"上层不发出冗余调用"是同一关切的两层,需在实现时明确职责边界)
- [ ] 0.4 Descriptor(纹理/UBO 绑定)绑定惰性化,建立 program → texture unit / uniform location 映射缓存,draw 前统一 flush
- [ ] 0.5 PSO 编译异步化 + program binary 缓存,消除编辑器切换材质时的同步编译卡顿;并扩展为 **GLSL 变体编译结果磁盘缓存**(第四轮发现:现每次冷启动重编所有变体,无落盘复用)
- [ ] 0.6 拆分 `OpenGLCommandList.cpp` 上帝类(现约 2000 行、25+ 状态子结构):将状态缓存、PSO 应用、资源绑定、barrier 推断、GPU 计时、ImGui 状态存取等职责分离为独立单元
- [ ] 0.7 统一枚举→GL 映射:确立 `OpenGLTypeMapping.hpp` 为唯一映射源,消除 `OpenGLSampler.cpp`(重复实现 `ToGLMagFilter`/`ToGLMinFilter`)与 `OpenGLPipeline.cpp`(游离的 `VertexFormat→GL`)、`OpenGLCommandList.cpp`(匿名空间 `ResourceState→GLbitfield`)的重复/游离映射
- [ ] 0.8 拆分 `RHIDefinitions.hpp`(现 313 行"垃圾桶":统计结构 + 计时结果 + 全部枚举 + Viewport/ClearValue + Barrier 工厂混居)为职责清晰的多文件(如 `RHIStatistics.hpp` / `RHIResourceBarrier.hpp` / 枚举定义)
- [ ] 0.9 ImGui 后端走 RHI:让 `OpenGLImGuiBackend` 通过 `RHICommandList`/`RHIBuffer`/`RHITexture` 提交,替代现在直接裸调 `glCreateBuffers`/`glUseProgram`/`glBindFramebuffer`,并移除 `OpenGLCommandList` 中 ImGui 专用的 GL 类型方法(`SetImGuiRenderState`/`AdoptImGuiDrawState`)
- [ ] 0.10 `PipelineStateCache` 碰撞加固(第三轮发现):现 `m_Cache` 直接以 `size_t` 哈希为键(`PipelineStateCache.cpp:43`),哈希碰撞会返回错误 PSO。改为存储 `RHIPipelineStateDesc` 并做完整相等比对(或哈希键 + 桶内 desc 比较)。实际 PSO 数量少、碰撞概率极低,属廉价可修的正确性隐患
- [ ] 0.11 统一描述符模型(第三轮发现):`RHIResourceSet` 现仅含纹理 span,与 `GPUContracts` 的 `GPUResourceSetIndex`(PerView/PerRenderable/PerMaterial 三层)脱节,buffer 绑定走另一条 `SetUniformBuffer` 通路——两套绑定模型共存不连通。收敛为统一的资源集抽象(与 0.4 惰性绑定协同)
- [ ] 0.12 barrier 精度(第四轮发现):`ToGLMemoryBarrierBits`(`OpenGLCommandList.cpp:164`)在非零状态但无 access 位时 fallback 到 `GL_ALL_BARRIER_BITS`(性能杀手),应去除;两参版 `TextureBarrier`/`BufferBarrier`(`:1524-1549`)忽略参数、固定发 3-bit 组合,应按实际 access 精确发 bits。与 RenderGraph barrier 翻译协同
- [ ] 0.13 bindless 驻留管理(第四轮发现):`OpenGLTexture.cpp:159-185` 的 bindless handle 一经 `MakeResident` 缓存、仅在纹理销毁时 `MakeNonResident`,无驻留上限检查、无显式释放路径——长生命周期(AssetManager 共享)纹理的 handle 永驻,大量纹理时踩 GPU 驻留上限。补驻留计数/上限查询/显式释放
- [ ] 0.14 上传路径同步优化(第四轮发现):现动态 buffer 走 `glNamedBufferSubData`(driver 同步)+ 3 段 ring 错位,无显式 fence。**注:GL 延迟删除保证其正确、非 UAF**,但可能产生同步 stall;CommandStream 改造时评估 persistent-mapped ring + `glFenceSync` 以消除 stall(`OpenGLBuffer.cpp:96-125`、`FrameUploadAllocator.cpp` retired buffer)
  - **[已复现·实锤] resize 时 GL 131186 刷屏**:动态场景 buffer 以 `glNamedBufferStorage(GL_MAP_WRITE_BIT|GL_DYNAMIC_STORAGE_BIT)` 建、每帧 `glMapNamedBufferRange` 写(`OpenGLBuffer.cpp:27,76`),驱动为满足 CPU map 将其 **VRAM→HOST 迁移**(deopt)。resize 拖拽的高频重渲染帧反复 map → 刷屏;无物体时场景 buffer 空不写故不报。根因即本项——用 persistent-coherent 映射让 buffer 常驻 VRAM 可根治
- [ ] 0.15 GL 调试回调节流(resize bug 暴露):`OpenGLDevice.cpp:75` `GLDebugCallback` 对每条 warning 无去重/节流,131186 等性能提示会刷屏。按消息 id 去重(`unordered_set` + 首次记录)或对已知性能提示降频/过滤

### 模块 1 — 公共基础设施收敛

- [ ] 1.1 提取重复的 `MaxValue<T>` 模板(现分散于 `GPUScene.cpp`/`MaterialTextureCache.cpp`/`ForwardOpaquePass.cpp`/`PostProcessPass.cpp`/`TextureLoader.cpp` 共 5 处)到统一工具头
- [ ] 1.2 清理 `RGBuilder::Read()` / `Write()` 等无实际作用的冗余转发 API
- [ ] 1.3 梳理 `HashCombine`、缓冲区扩容(倍增策略)、对齐计算等零散工具函数,建立统一归属位置约定,减少后续新增模块时的重复实现倾向

### 模块 2 — RenderGraph / Renderer 提交框架重构

- [ ] 2.1 拆分 `Renderer::BuildRenderGraph()`(现约 408 行单函数、8 个 lambda)为独立的 Pass 注册单元,消除巨函数
- [ ] 2.2 消除各 `XxxPassContext` 结构体在多个 lambda 中重复填充相同字段(`device`/`commandList`/`shaderLibrary`/`pipelineCache`/`frameData` 等)的问题,设计公共基础上下文
- [ ] 2.3 将 Benchmark(`PipelineBenchmarkSettings`/`State`)、Capture 等辅助职责从 `Renderer` 核心门面中剥离,降低 `Renderer.cpp`(现约 1050 行)体量(注:剥离后需同步检查 `RendererSettingsPanel` 等 Editor 面板对 Renderer 内部的耦合)
- [ ] 2.4 资源 resize 生命周期安全(第三轮发现):`DeferredResources::Ensure()` 在 resize 时先 `Reset()` 旧纹理再建新的,若 RenderGraph pass 已捕获旧纹理裸指针进命令列表,存在悬垂访问窗口。补齐 resize 栅栏/冲刷或延迟释放机制,确保跨 resize 不引用已释放的 GPU 纹理。**并做 resize 去抖**:拖拽时高频 resize 事件触发的连续重渲染帧会反复 map 动态 buffer(放大 0.14 的 VRAM↔HOST 迁移刷屏,实测 GL 131186),应合并/去抖 resize、避免每事件全量重建与重映射

### 模块 3 — RenderProxy / CommandExecutor 整理

- [ ] 3.1 用模板统一 `RenderCullDrawBuckets` / `RenderDrawBuckets` / `RenderCullCommandBuckets` / `RenderCommandBuckets` 四份几乎相同的桶结构体
- [ ] 3.2 `RenderCommandExecutor` 提交热循环对接模块 0.3 的去重缓存能力,避免相邻相同 mesh/primitive 的重复状态设置(与 0.3 分层协作,勿重复实现)

### 模块 4 — Material 数据层校正与扩展策略

此模块是阶段二材质拓展的关键前置:当前 `MaterialGPUData` 8 个 `vec4` 通道已用满、无预留空间。**但项目已存在 `ShaderFeatureMask` 位掩码变体系统**(`Shader.hpp` + `ShaderLibrary` 按变体编译缓存),因此材质扩展存在"胖结构 + uber shader" 与 "扩展变体、shader 精简" 两条路线,须在本模块显式决策(详见 4.2)。

- [ ] 4.1 校正 `MaterialInstance` / `MaterialSignature` / `MaterialInstanceRegistry` / `MaterialTextureCache` 四者职责边界,拆分 `MaterialTextureCache`(现约 548 行)过重的职责(纹理上传 / texture set 去重 / bindless 索引管理三者分离)
- [ ] 4.2 **[关键设计决策]** 确定材质扩展路线:方案 A 胖 `MaterialGPUData` 预留字段 + uber shader,方案 B 扩展 `ShaderFeatureMask` 变体 + 保持 GPU 数据精简(Filament 7.1.2 明确反对运行时 uber shader,且项目已有变体基建,倾向 B)。据此重新设计 GPU 契约布局。此项变更后须运行 `Tools/verify_gpu_contracts.py`
- [ ] 4.3 建立 `MaterialSignature` 哈希覆盖字段与 `MaterialComponent` 字段同步更新的机制,避免未来新增字段时签名遗漏
- [ ] 4.4 收敛材质数据流跳数(现 5+ 跳:`MaterialComponent` → `FrameData` 存整份组件副本 → `MaterialGPUData` → GLSL `MaterialData` → `MaterialInputs` → `PixelMaterial`):消除 `FrameData::materials` 存整份 `MaterialComponent`(含 `materialPath`/纹理路径等 GPU 不用字段)的冗余副本,缩短打包链。顺带评估相机侧 `RenderView`→`CameraData` 纯搬运层是否可坍缩(小收益)

### 模块 5 — ShadowPass 纠错

> **[P.1 实测 = 阶段一 #1 fps 目标]** Shadow GPU 15.48ms、独占约 64%。Pass Draws 152 = 4 级联 × ~38 caster、**零裁剪**,把 2.49M-tri 场景重栅格化 4 次。5.1 级联裁剪 + 5.3 采样/分辨率/级联数 预计单模块可砍数毫秒(24→~17ms,→~58fps)。此为砍 fps 的最高杠杆,先做。

- [ ] 5.1 修正各级联无条件绘制全部 shadow caster 的问题,补齐有效的遮挡/视锥裁剪(现每级联完整遍历绘制,无裁剪)
- [ ] 5.2 治理硬编码的 `depthBias`/`slopeBias`/`depthPadding` 等常量,提供合理可调范围,缓解 shadow acne 与 peter-panning
- [ ] 5.3 评估 PCF/PCSS 采样次数(现 5x5=25 次/像素,PCSS 更高)与级联数量的性能开销,确定合理默认值(收益由 P.1 阴影 GPU 计时佐证)

### 模块 6 — GBuffer / Deferred / Forward 纠错

> 复核修订:原 6.1"修正 D_GGX"已删除——经源码核对,`BRDF.glsl:20-25` 的 `D_GGX` 正是 Filament 标准优化式,与教科书 GGX 数学恒等,**实现正确,无需改动**。原 6.2"点/聚光阴影"经核实为**新功能而非纠错**(需 cube/atlas 基建),已迁至阶段二模块 14.3。

- [ ] 6.1 **[提优先级·全路径曝光审计]** 统一 pre-exposure/曝光在**所有着色路径**的应用。第三轮核实:当前仅 punctual 光与 emissive 乘了 `GetPreExposure`,而 **deferred 方向光(`Lighting.glsl:35`)、forward 方向光、IBL(Forward 与 Deferred 两路,`IBL.glsl`)均未乘**,skybox 则叠加 `exp2(uSkyboxParams.x)` 与 `GetPreExposure` 疑似双重;`ResolveExposureAdjustment` 在 `Composite.frag` 与 `BloomPrefilter.frag` 各自重复定义。需审计并统一所有曝光应用点(方向光/punctual/emissive/IBL/skybox)。⚠️ 默认 pre-exposure≈1 时该不一致隐性不显,会在**模块 15 启用非单位预曝光/自动曝光时爆发**,故为模块 15 的硬前置
- [ ] 6.2 优化 Deferred Lighting 全屏绘制对天空像素等无效区域的开销,减少不必要的 depth 读取与世界坐标重建(收益由 P.1 的 `deferredLightingGpuMs` 佐证)
- [ ] 6.3 BRDF 一致性审计:`BuildBRDFInputs` 计算的 `f90` 在 specular 路径(`EvaluateSpecularBRDF` 用单参 `F_Schlick`)未生效;漫反射固定 Lambert、已定义的 `Fd_Burley` 未接入——评估是否统一(低优先,非 bug)

### 模块 7 — IBL / ClusteredLighting 纠错

- [ ] 7.1 校验 SH 系数(irradianceSH)在 CPU 端编码与 GPU 端重建之间的一致性(归一化常数、球谐基函数系数)
- [ ] 7.2 统一 Cluster 深度分片 CPU(`double` 精度)与 GPU(`float log`)计算精度,消除边界 slice 误判风险
- [ ] 7.3 评估 Cluster 每帧全量重建策略的可扩展性,视光源规模决定是否引入增量更新

### 模块 8 — PostProcess / Bloom 治理

> ~~P.1 若确认 GPU-bound,本模块很可能是当前场景 fps 的最大收益点(默认开启 Bloom,14 个全屏 pass)。~~ **[P.1 实测修正]** Bloom(prefilter/down/up/composite)实测仅 0.09+0.06+0.06+0.36 ≈ **0.57ms**,远非瓶颈。fps 优先级**降级**:本模块保留为**代码整洁/pass 数收敛**(减 CPU 提交、降复杂度),不再当作 fps 手段。

- [ ] 8.1 评估并降低 Bloom mip 层级数量或合并渲染通道(现 7 层 downsample + 7 层 upsample = 14 个独立 RenderPass,含 TextureBarrier 与状态切换),优先级由 P.1 的 bloom GPU 计时确认
- [ ] 8.2 合并 Bloom Prefilter 等独立小 Pass 到主管线中,减少 CPU 端提交次数

### 模块 9 — 收尾清理

- [ ] 9.1 全量复查潜在死代码与冗余接口(需交叉验证真实调用点,避免误删)
- [ ] 9.2 同步更新 `Docs/Physara.md` 目录说明,使其反映阶段一重构后的实际架构

---

## 阶段一·横向架构地基(与纵向管线并行/穿插)

> 第二轮扫描发现的跨层架构隐患。用 `H` 前缀模块以避免打乱 0–15 的既有编号。这些工作不依赖具体渲染算法,可与模块 0–9 并行推进;但 `H2.1` 序列化单点维护是阶段二的硬前置(否则每加一个材质/光照/相机字段都要三处手改且易漏)。

### 模块 H1 — Editor / ImGui 架构重构

第二轮评估 Editor 前端架构为 **C 级(有风险)**:无面板基类、`EditorContext` 沦为上帝 DTO、3D 渲染嵌在 UI 帧内、ImGui 绕过 RHI、输入路由分散。以下为解耦方向(H1.2 依赖模块 2 的 Renderer 门面稳定后再做)。

- [ ] H1.1 引入 `IPanel` 接口 + 面板注册表,消除 `EditorApp` 中 7 个硬编码面板成员与 `DrawDockedPanels` 的 if-else 链(现加一个面板需改 5+ 处)
- [ ] H1.2 解耦 `EditorContext` 上帝 DTO:提取 `EditorBridge`/统一的渲染设置应用入口,收敛 `EditorApp` 中 50+ 行 `settings → Engine::XXXSettings` 的手工逐字段映射;面板不再直接持有并改写引擎内部状态
- [ ] H1.3 绘制阶段分离:将 `OnUIRender`(现把 `RenderSceneView`→`Renderer::RenderScene` 夹在 `ImGui::NewFrame`/`Render` 之间)拆为 `PreUpdate → RenderScene → BuildUI → Present`,厘清 3D 渲染与 UI 的边界,为多视口/并行留出空间
- [ ] H1.4 `ComponentDrawer.hpp`(673 行全 inline)迁至 `.cpp` 消除编译依赖扩散;拆分 `SceneViewPanel.cpp`(867 行,含 338 行单函数 `DrawOverlay`)的过长函数
- [ ] H1.5 输入路由集中化:统一 ImGui want-capture、视口导航捕获、全局快捷键、光标模式为按优先级的处理链,替代现在分散在 EditorApp/SceneViewPanel 的条件守卫;并避免被禁用面板每帧仍全量绘制
- [ ] H1.6 交互层写回正确性(第三轮发现):`Gizmo` 写回经 `Scene::SetWorldMatrix`→`glm::decompose`,在**负缩放父级**下会丢失/镜像旋转(glm 固有问题),且 `SetWorldMatrix` 的失败返回值被忽略;`EditorCamera::SyncFromSceneCamera` 从含非均匀/负缩放的世界矩阵提取 forward 不鲁棒,`Dolly/Rotate` 输入未乘 deltaTime(帧率相关)。审视交互层的 TRS 分解与世界↔局部换算

### 模块 H2 — Scene / Resource / Serialization / Platform 卫生

- [ ] H2.1 **[阶段二硬前置]** 消除 `SceneSerializer` 的"改一个字段改三处"(组件 struct + Serialize + Deserialize 手写 JSON、字段名硬编码字符串):引入字段绑定表/宏辅助/代码生成使 struct↔JSON 单点维护;补齐已写入却从不读取的 `version` 兼容校验
- [ ] H2.2 `GLTFLoader` 职责分层:分离"加载资源到 `AssetManager`"与"构建场景实体"(现 `LoadToScene` 返回 `Entity`,跨越 Resource→Scene 两层)
- [ ] H2.3 拆分 `Scene.cpp` 过载职责(实体 CRUD / 父子关系链表操作 / 场景相机管理 / 世界变换计算,现至少 5 类不相关职责混居);并修正 `TransformSystem` 脏标记传播——现靠递归参数(`parentDirty` 布尔)下传而**不持久化到子节点 `worldDirty` 字段**,极端帧序下子孙可能漏更新用到过期父矩阵,应改为显式标脏子孙(第三轮发现)
- [ ] H2.4 明确 `AssetManager` 线程安全策略(现全无锁,而 IBL 预计算已用 `std::thread`,存在潜在竞争);评估 16-bit generation 绕回风险
- [ ] H2.5 Platform 健壮性:修正 `glfwInit`(在 `Create`)/`glfwTerminate`(在 `Destroy`)不配对的双重终结风险、`ScrollCallback` 水平偏移硬编码 0 丢失、图标加载失败静默无日志;将 `RuntimeBackendFactory`(现处 `RHI` 命名空间却创建 `Platform::` 对象)归位;评估提供 headless / 无 Editor 运行路径
- [ ] H2.6 `Texture` 资产补齐色彩空间(第三轮发现):现 CPU `Texture`(`Types/Texture.hpp`)无 `colorSpace` 字段,sRGB/linear 在 `MaterialTextureCache` 按纹理槽硬编码。当前 glTF 按槽约定可工作,但阶段二新增 clearcoat/sheen 等纹理类型时该硬编码映射会漏、也无法序列化/手动覆盖。将色彩空间显式落到资产模型(与模块 10.2 联动)

---

## 阶段二:功能拓展

> 建立在阶段一稳固地基之上。参考 `Docs/FilamentRenderSide.md` 材质系统章节(2.3-2.8)与"对自研渲染器的架构启示"(7.1-7.3)。渲染器仅读取 glTF,拓展内容对应 `KHR_materials_*` 系列扩展。

### 模块 10 — 材质扩展机制搭建

先建立"如何新增一个材质模型"的通用通道,而非直接实现具体模型。落地模块 4.2 的路线决策。

> **第四轮量化(重要)**:实现级审查确认当前 shader 材质架构是阶段二的**系统级硬前置,工作量远超"加字段"**。现状:`EvaluateSurfaceBRDF`(`BRDF.glsl:96-99`)**硬编码 Lambert+GGX 单一流水线**;`Lighting.glsl` 按 `shadingModel` 仅有 **LIT/UNLIT 两分支**、不可插拔;`PixelMaterial`/`MaterialInputs`(`Material.glsl:19-66`)**零预留字段**;`MaterialInputs` 用 6× `bool`(std430 各占 4B)编码纹理存在位、浪费且会随模型增多膨胀。因此模块 11-13 动手前,必须先完成本模块的可插拔 BRDF/着色模型重构。

- [ ] 10.1 依据模块 4.2 选定的路线(倾向变体扩展),设计可插拔材质模型的 GPU 数据布局与 `ShaderFeatureMask`/着色模型枚举扩展策略;同时规划 shader 侧着色模型的文件组织(参考 Filament `surface_shading_model_*.fs` 拆分,避免 `BRDF.glsl`/`Material.glsl` 随模型增多而臃肿)
- [ ] 10.2 搭建 `GLTFLoader` 对 `KHR_materials_*` 扩展的通用解析注册框架(现仅硬编码解析 `pbrMetallicRoughness`/部分 `transmission`/`emissive_strength`)
- [ ] 10.3 设计 `Material.glsl` 中 `PrepareMaterial()` 的着色模型分支/变体扩展点,避免后续每加一个模型都改动主流程
- [ ] 10.4 **[第四轮·可插拔 BRDF 重构]** 将 `EvaluateSurfaceBRDF` 从硬编码 Lambert+GGX 改为按着色模型分发的可插拔结构(`#define` 驱动的独立 shading-model include,或 shadingModel 分支但收敛在单一扩展点);同步把 `MaterialInputs` 的多个 `bool` 收敛为位标志,并为 `PixelMaterial` 规划分模型的扩展字段容纳方式(避免结构无限膨胀)。这是模块 11-13 的公共前置

### 模块 11 — Sheen / Cloth 材质模型(改动量最小,优先验证扩展机制)

- [ ] 11.1 CPU 组件/资源层(`MaterialComponent`/`Material.hpp`)新增 `sheenColor`、`sheenRoughness` 字段
- [ ] 11.2 `GLTFLoader` 新增 `KHR_materials_sheen` 扩展解析
- [ ] 11.3 `BRDF.glsl` 新增 Charlie NDF 与 Neubelt Visibility,实现 sheen 层组合公式
- [ ] 11.4 `ShadingModel` 新增 `Cloth` 枚举值及对应 BRDF 路径(Lambert 漫反射 + 染色 sheen 高光,无 Fresnel 项)

### 模块 12 — Clearcoat 材质模型(改动量中等)

- [ ] 12.1 CPU 组件/资源层新增 `clearCoat`、`clearCoatRoughness`、`clearCoatNormal` 字段与对应纹理槽
- [ ] 12.2 `GLTFLoader` 新增 `KHR_materials_clearcoat` 扩展解析
- [ ] 12.3 `BRDF.glsl` 新增 Kelemen Visibility 与 clearcoat 双层叠加组合公式(含 IOR 反推基础层 F0)
- [ ] 12.4 `SurfaceMaterial.glsl` 新增 clearcoat 法线纹理独立采样通道

### 模块 13 — Subsurface 与 Transmission/Refraction 材质模型(改动量最大)

- [ ] 13.1 CPU 组件/资源层新增 `thickness`、`subsurfacePower`、`subsurfaceColor`、`transmission`、`ior`、`absorption`、`dispersion` 等字段
- [ ] 13.2 `GLTFLoader` 新增 `KHR_materials_volume`、`KHR_materials_ior`、`KHR_materials_dispersion` 解析,完善现有 `KHR_materials_transmission` 支持(现仅读 `transmissionFactor`)
- [ ] 13.3 `BRDF.glsl` 新增次表面散射漫反射模型(前向散射 + 背向透射近似,非离线扩散方程求解)
- [ ] 13.4 评估并实现折射所需的 screen-space refraction pass 或 cubemap 近似方案,可能涉及 RenderGraph 结构调整(注:此屏幕空间折射与阶段三模块 16 的 SSR 属同一技术家族、共用 post-opaque 场景颜色基建,应建立在 16.1 之上,勿各建一次)

### 模块 14 — 物理光照补全

> 含承接自原模块 6.2 的点/聚光阴影——这是需要新建阴影基建的功能项,非小修。

- [ ] 14.1 接入 IES Profile 光源分布数据参与实际光照计算(当前 `LightComponent.iesProfilePath` 字段已存在但未接入渲染路径)
- [ ] 14.2 实现 Area Light 的实时求值(当前动态光照循环明确不支持面积光)
- [ ] 14.3 **实现点光/聚光阴影**(承接原 6.2):点光需 cube shadow map、聚光需额外 shadow map,配套 shadow atlas 分配与 GPU 侧 `EvaluatePunctualLight`/Deferred cluster 光照循环的阴影采样接入(现二者完全无阴影)

### 模块 15 — 物理相机补全

- [ ] 15.1 引入 Reverse-Z 与无限远渲染投影,分离"渲染投影"与"裁剪投影"语义(裁剪/阴影级联/光源影响范围判断仍使用有限 far)
- [ ] 15.2 校验 `CameraInfo` 式单帧快照机制,确保渲染过程中不读取可变的 Camera 状态
- [ ] 15.3 与模块 6.1 联动,确认曝光/预曝光在物理相机链路上的一致应用(EV100 → pre-exposure → 各光照路径)
- [ ] 15.4 reverse-Z 连带审计(第三轮发现):引入 reverse-Z/无限远投影后,所有假设标准 [-1,1] NDC 深度 `* 0.5 + 0.5` 的 shader 需连带修正——已知点:`WorldGrid.frag`(射线-平面深度写回)、`Composite.frag`(`LinearizeDepth`)、`Skybox`(far 深度)。逐一核对深度重建/线性化公式

---

## 阶段三:高级渲染特性(对齐 Filament,优先级低于阶段一/二)

> 第五轮对齐 Filament 两份文档发现:计划对 Filament 的"架构与正确性"吸收充分,但遗漏了一批 **Filament 有而 Physara 从未构建**的渲染特性。它们多为大功能、非重构/纠错必需,故单列阶段三,作为"全链路 PBR 参考 Filament"的完成度补齐,按需推进。

### 模块 16 — 屏幕空间反射与折射(共享场景颜色基建)

> SSR 与屏幕空间折射是同一技术家族(post-opaque 复用场景颜色缓冲)。原模块 13.4 的折射应建立在此共享基建之上,避免只为折射建一次、日后加 SSR 再建一次。

- [ ] 16.1 建立 post-opaque 场景颜色拷贝 + mip 链基建(供 SSR / 屏幕空间折射 / 粗糙反射共用),接入 RenderGraph
- [ ] 16.2 SSR(FR 2.8.2):低粗糙度屏幕空间反射,按粗糙度/命中与 IBL 镜面混合,屏外缺失区域回退 IBL
- [ ] 16.3 将模块 13.4 的屏幕空间折射改建在 16.1 基建之上(去重)

### 模块 17 — 后处理与物理相机进阶

- [ ] 17.1 DoF 景深:消费 `CameraComponent` 已有的光圈 f-stop/焦距/对焦距离(现仅用于 FOV/曝光、未驱动景深),实现后处理景深(FR 4.7:景深属后处理,非 Surface BRDF)
- [ ] 17.2 TAA:时间抗锯齿 + jitter(jitter 只修饰单帧渲染投影、不污染裁剪矩阵,与 15.1 reverse-Z 协同),并与现有 MSAA 做取舍
- [ ] 17.3 Color Grading:在 tone mapping 前引入可配置色彩分级(LUT),明确分层"曝光(物理线性)→ 分级 → 显示变换"(FR 4.6 step 6)

### 模块 18 — 光照与 AO 进阶

- [ ] 18.1 太阳角径 / 软方向光(FR 3.4):方向光引入角半径,影响镜面高光大小与软阴影,而非当前的纯方向点
- [ ] 18.2 进阶 AO(FR 2.8.1):bent normal + GTAO 多弹跳 + micro-shadowing(现仅有 Lagarde 式 specular AO)
- [ ] 18.3 混合模式补全(FR 2.8.3):在 Opaque/Mask/Blend 之上补 ADD/SCREEN/MULTIPLY/FADE 等(艺术向,最低优先)

---

## 明确暂缓 / 与 Filament 的已知分歧(自觉决策,非疏漏)

第五轮对齐时确认以下 Filament 能力**有意不纳入当前计划**,记录为自觉决策而非遗漏:

- **多线程 render prepare / job 系统**(FA 7.10):现为单线程。Filament 将多线程 prepare 与单线程命令写入分离。暂缓——待 P.1 证明 CPU-bound 且单线程确为瓶颈再评估(与模块 0 CommandStream 是不同维度的事)。
- **自动曝光闭环**(亮度统计 → EV 反馈,FR 4.7):现为手动 EV100;Filament 亦将其定位为 app 级可选。暂缓;需要时从前帧亮度平滑驱动 EV。
- **Specialization constants**(FA 6.7):GL 4.6 纯 GLSL 路径无此能力(需 SPIR-V)。属单后端限制,不追。
- **多局部 IBL Probe 混合**(FR 3.8):Filament 亦为单个远距离探针,保持单探针。
- **雾 / 体积光**(FR 4.6 帧流程含 Fog):当前疑缺;若确有需求归入阶段三候选,先确认需求再排。

---

## 复核修订记录

针对源码的批判性复核结论(已并入上述计划):

| 项 | 原计划 | 复核结论 | 处理 |
|---|---|---|---|
| D_GGX | 6.1 修正疑似非标准 GGX | `BRDF.glsl:20-25` 为 Filament 标准优化式,数学恒等,正确 | 删除该项 |
| 点/聚光阴影 | 6.2 归为"纠错" | 实为新功能,需 cube/atlas 基建 | 迁至 14.3,重定性为 feature |
| 曝光不一致 | 6.3 普通条目 | 方向光(主光)未预曝光,影响物理相机链路 | 提优先级为 6.1,链接模块 15 |
| CommandStream | 模块 0,45fps 头号嫌疑 | 场景仅 3 draw,CPU 提交量极小,fps 收益存疑 | 重写立项理由为"架构地基",排位由 P.1/P.3 裁定 |
| 测量 | 无 | 已内置 per-pass GPU/CPU 计时 + benchmark 未被利用 | 新增阶段 0 / 模块 P |
| 验证 | 无 | 大重构无回归护栏,存在 RendererCapture/契约脚本可用 | 新增 P.2 golden image + 契约校验 |
| 材质扩展路线 | 4.2 直接"预留字段"(胖结构) | 已有 ShaderFeatureMask 变体系统,存在更优路线 | 4.2 升级为显式设计决策 |
| **第二轮·CPU↔GPU 契约漂移** | 无防护 | 校验脚本只查枚举值,不查结构体布局;`ObjectData` 有隐式 uvec4 映射 | 新增 P.4,列为模块 4.2 硬前置 |
| **第二轮·RHI 多后端** | 未评估 | 增第二后端痛苦 4.5/5(即时命令模型,缺 swapchain/fence/descriptor 等) | YAGNI 暂缓重型抽象,模块 0 只做解耦(0.6–0.9) |
| **第二轮·枚举映射散乱** | 未覆盖 | Sampler/Pipeline/CommandList 存在重复与游离映射 | 新增 0.7 统一映射源 |
| **第二轮·OpenGLCommandList/RHIDefinitions 臃肿** | 仅计划 CommandStream | ~2000 行上帝类 + 313 行垃圾桶头 | 新增 0.6/0.8 拆分 |
| **第二轮·Editor 架构 C 级** | 仅提 RendererSettingsPanel 耦合 | 上帝 DTO / 无面板基类 / 渲染嵌 UI 帧 / ImGui 绕过 RHI / 输入分散 | 新增模块 H1(+0.9 ImGui 走 RHI) |
| **第二轮·材质 5 跳打包** | 模块 4 泛提 | `FrameData` 存整份组件副本,GPU 不用字段随行 | 新增 4.4 收敛跳数 |
| **第二轮·SceneSerializer 三处维护** | 未覆盖 | 手写 JSON 与组件定义脱钩,加字段改三处 | 新增 H2.1,列为阶段二硬前置 |
| **第二轮·Platform/Resource 卫生** | 未覆盖 | glfwInit/terminate 不配对、滚轮 bug、GLTFLoader 越层、AssetManager 无锁、工厂命名空间错位 | 新增 H2.2–H2.5 |
| **第三轮·曝光系统性不一致** | 6.1 仅 deferred 方向光 | forward 方向光、IBL 两路均未预曝光,skybox 疑似双重;默认 pre-exposure≈1 时隐性 | 6.1 升级为全路径曝光审计,列为模块 15 硬前置 |
| **第三轮·Shadow.vert 用 CAMERA 槽** | (子 agent 疑为高危) | 已核实 `ShadowPass.cpp:453/279` 每级联把 light VP 绑到 CAMERA 槽,**阴影正确** | 假阳性,不改;仅记"槽位复用"可读性小注 |
| **第三轮·PSO 缓存哈希碰撞** | 未覆盖 | `PipelineStateCache.cpp:43` 仅以哈希为键无 desc 比对(实际碰撞概率极低) | 新增 0.10 加固 |
| **第三轮·描述符模型脱节** | 未覆盖 | `RHIResourceSet` 仅纹理,与 `GPUResourceSetIndex` 三层脱节 | 新增 0.11 统一 |
| **第三轮·resize 悬垂窗口** | 未覆盖 | DeferredResources resize 与 RenderGraph 旧纹理指针 | 新增 2.4 |
| **第三轮·Texture 无色彩空间** | 未覆盖 | 资产不带 sRGB/linear,缓存层按槽硬编码,阶段二会漏 | 新增 H2.6,联动 10.2 |
| **第三轮·Transform 脏标记不持久化** | 未覆盖 | 靠递归参数下传,不写子节点 worldDirty | 并入 H2.3 |
| **第三轮·交互写回正确性** | 未覆盖 | Gizmo 负缩放 decompose、忽略返回值;EditorCamera 帧率相关 | 新增 H1.6 |
| **第三轮·reverse-Z 隐患** | 未覆盖 | WorldGrid/Composite/Skybox 假设 [-1,1] NDC | 新增 15.4 连带审计 |
| **第四轮·shader 架构阻塞** | 模块 10 泛提"加字段" | `EvaluateSurfaceBRDF` 硬编码 Lambert+GGX、仅 LIT/UNLIT、零预留字段 | 锐化模块 10,新增 10.4 可插拔 BRDF 重构、列为 11-13 前置 |
| **第四轮·stencil 未实现** | (子 agent 疑为高危 bug) | 已核实 `RHIPipelineState.hpp:41` 仅 `stencilTest=false // 后续再扩展`,RHI 无 stencil 参数、无 pass/shader 使用 | 非 bug,是**注释写明的功能空缺**;待需求驱动再扩,入待观察 |
| **第四轮·retired buffer "UAF"** | (子 agent 疑为高危 UAF) | 已核实上传走 `glNamedBufferSubData`(同步)、删除走 `glDeleteBuffers`(GL **延迟删除**保证安全) | 非 UAF,是 perf(stall);降级并入 0.14 |
| **第四轮·barrier 过保守** | 未覆盖 | `GL_ALL_BARRIER_BITS` fallback + 2 参 barrier 忽略参数 | 新增 0.12 |
| **第四轮·bindless 无驻留管理** | 未覆盖 | handle 只在纹理销毁时释放、无上限检查 | 新增 0.13 |
| **第四轮·无 GLSL 磁盘缓存** | 0.5 仅 program binary | 每次冷启动重编所有变体 | 扩 0.5 |
| **第四轮·binding 号无校验** | P.4 仅结构布局 | 枚举↔`PHYSARA_BINDING_*` 手工同步;死枚举 Shadow=6/IBL=7 | 扩 P.4 |
| **第四轮·底层 GL 实现扎实** | — | std140/std430 布局全对、DSA 全面、immutable storage、timestamp query ring 均现代正确 | 新增"已验证良好"基线节,重构勿 churn |
| **第五轮·Filament 特性对齐疏漏** | 阶段二仅材质 + 光照/相机基础 | SSR/DoF/TAA/太阳角径/进阶 AO/混合模式 等 Filament 有而计划漏 | 新增阶段三(模块 16-18)收纳 |
| **第五轮·draw 排序/镜面 AO** | (疑为疏漏) | 已核实 `RenderProxy::SortBuckets/BuildSortKey`、`Math.glsl:ComputeSpecularAO` 均已具备 | 澄清:非疏漏,记入"已验证良好" |
| **第五轮·SSR/折射/DoF 共享基建** | 折射孤立在 13.4 | 三者共用 post-opaque 场景颜色基建 | 16.1 统一,13.4 改建其上 |
| **第五轮·明确暂缓项** | 静默未提 | 多线程 prepare / 自动曝光 / spec constants / 雾 | 新增"明确暂缓"节,自觉决策 |
| **P.1 实测·GPU-bound & Shadow 主导** | 假设 Bloom/CommandStream 是 fps 关键 | 实测 GPU 22.68ms 封顶,Shadow 15.48ms(64%),Bloom 仅 0.57ms | P.3 定序:模块 5 升 #1;模块 8 fps 降级;模块 0 不抢跑 |
| **实测·resize GL 131186 刷屏 bug** | 未知 | 动态 buffer(GL_MAP_WRITE)映射致 VRAM→HOST 迁移,resize 高频放大 + 日志无节流 | 归 0.14(根因)+ 0.15(日志节流)+ 2.4(resize 去抖) |
| **实测·CPU p95 尖刺 43ms** | 未知 | median 12.47 但 p95 43.15,周期性 CPU 卡顿 | 记入 P.1,疑与 buffer 迁移/PSO 编译相关,待查 |

---

## 第三轮待观察项(低优先,重构对应模块时顺带处理,勿单列投入)

以下为第三轮逐模块扫描中确认的低severity项,记录备查:

- **RendererCapture 同步回读 stall**:`glGetTextureSubImage` 在主渲染路径同步回读,capture 帧有尖峰。可改 PBO + fence 异步(仅影响截图,非常规帧)。归属:模块 2/8 附近或 P.2 golden image 工具。
- **IBL panorama mip 初值**:`IBLPrecompute.cpp:224` `BuildPanoramaMipChain` 初值 `1.f` 使边界像素偏亮,轻微影响预滤波源。归属:模块 7。
- **Bloom soft-knee 作用点**:`BloomPrefilter.frag:75` soft-threshold 作用于 13-tap 混合后的颜色而非逐 4-tap 小组,可能丢局部高频亮度。归属:模块 8。
- **MeshComponent 材质槽不匹配静默**:`materialSlots` 数量小于 primitive 数时静默 fallback 到首个空槽、无告警。归属:模块 H2(健壮性)。
- **EditorCamera Dolly/Rotate 帧率相关**:未乘 deltaTime(已在 H1.6 提及交互层,此处为具体点)。
- **LightProxyPass spot cone 可见性**:`&&` 链式 `Project` 致部分出屏时整锥消失;billboard handle 无引用计数。归属:模块 H1。
- **RHI 死字段/注释**:`RHIFramebufferDesc` 的 Vulkan-only `renderPassDesc` 在单后端下恒 unused;`RHISamplerDesc.anisotropy` 注释"1.0 为关闭"表述不准(1.0 即各向同性)。归属:模块 0.8 拆分时顺带清理。
- **RHIPipelineStateDesc 缺 depthBiasClamp**:阴影偏移精细控制可能不足,按需补。归属:模块 5.2。
- **GetLocalMatrix() 非 const 却返回 const&**:`TransformComponent.hpp` 有隐藏副作用(改 dirty),语义矛盾。归属:模块 H2.3。

**第四轮(实现级)补充:**

- **Stencil 尚未实现**:`RHIPipelineState.hpp:41` 仅占位 `stencilTest`,RHI 无 stencil op/func/ref/mask,GL 后端未应用 stencil 状态。当前无任何 pass/shader 使用 stencil,故非 bug;待有需求(如特定遮罩/描边)时**同步扩展 RHI 抽象 + GL 后端**。
- **MDI stride 无范围校验**:`OpenGLCommandList.cpp:1514` `glMultiDrawElementsIndirect` 未校验 stride ≥ `sizeof(RHIDrawIndexedIndirectCommand)`,上层传错会 GPU 崩且难查。归属:模块 0.6 拆分时加断言。
- **非动态 buffer 误带 `GL_DYNAMIC_STORAGE_BIT`**:`OpenGLBuffer.cpp:27` 静态 buffer 也带该 flag,驱动可能分配更动态内存池。归属:模块 0。
- **FrameUploadAllocator kAlignment 硬编码 256**:`FrameUploadAllocator.hpp` 硬编码,而 `OpenGLBuffer` 从设备查询对齐;建议改为运行时传入。归属:模块 0.14 或 1。
- **`GL_ARB_shading_language_include` require 却未生效**:15 个 pass shader 声明该扩展,但 `ShaderLoader` 手工文本展开 include,扩展从未被驱动使用;且嵌套 include 的 `#line` 偏移不精确(编译报错行号可能错)。归属:模块 10 shader 组织重构时清理。
- **binding=4 语义重载**:`GPUBufferBinding` 中 InstanceIndices/PostProcessSettings/SkyboxSettings/WorldGridSettings 同为 4(不同 shader 中安全但易混淆)。归属:P.4 清理。

---

## 已验证良好(重构时勿 churn)

第四轮实现级审查确认以下部分**现代且正确**,作为回归基线保护,重构时不要为"统一风格"而误改坏:

- **std140/std430 布局全部匹配**:所有 GPU 结构(`CameraData`/`ObjectData`/`MaterialGPUData`/`LightData`/`ShadowData`/`IBLData`/`ClusterGridData`/`FrameUniforms`)C++ 与 GLSL 布局一致;设计者刻意只用 `mat4`/`vec4`/`uvec4` 规避了 vec3/标量数组的 std140 陷阱。→ P.4 是**防未来漂移的护栏**,非修现有 bug。
- **DSA 使用充分正确**:凡有 DSA 等价的操作(纹理/buffer/VAO/FBO/sampler 创建与更新)全部用 `glCreate*`/`glNamed*`/`glTexture*`;非 DSA 处(VAO 绑定、`glUseProgram`、`glBindBufferBase/Range`、`GL_DRAW_INDIRECT_BUFFER`)均为 spec 无替代,属最佳实践。
- **Immutable storage**:纹理 `glTextureStorage*D`、buffer `glNamedBufferStorage`,零废弃的 `glTexImage2D`/`glBufferData`。
- **Timestamp GPU 计时**:8 帧 query 环 + `GL_QUERY_RESULT_AVAILABLE` 非阻塞检查 + `GL_TIMESTAMP`,最佳实践,可直接用于模块 P.1 的瓶颈定位。
- **Framebuffer**:`glCheckNamedFramebufferStatus` 完整性检查、MRT/纯深度 draw buffers、Depth24Stencil8 附件点、array layer 绑定均正确。
- **上传策略骨架**:大 SSBO + `glBindBufferRange` 动态偏移、offset 对齐从设备查询、材质按 hash 变化才上传——方向正确(仅同步/fence 层面可再优化,见 0.14)。
- **阴影级联绑定**:每级联把 light VP 写入 cascade camera 并绑到 CAMERA 槽,`Shadow.vert` 使用正确(第三轮已核实)。
- **draw 排序(减少状态切换)**:`RenderProxy::SortBuckets`/`BuildSortKey`(`RenderProxy.cpp:167,252,483`)已按 64 位键(深度/mesh/primitive/material)排序,对齐 Filament FA 7.5。→ 已具备,勿当疏漏。
- **镜面遮蔽 + 能量补偿**:`Math.glsl:79 ComputeSpecularAO`(Lagarde 式,`IBL.glsl:72` 应用)与 `BRDF.glsl EnergyCompensation`(多散射)已具备。

---

## 关联文档

- 每个子任务的具体实现方案见 `Docs/TaskDetail.md`(按本文档编号对应存档)
- Filament 材质/光照/相机模型参考:`Docs/FilamentRenderSide.md`
- Filament CPU/GPU 协同与 RHI 组织参考:`Docs/FilamentAppSide.md`
- 当前工程目录与模块职责说明:`Docs/Physara.md`
