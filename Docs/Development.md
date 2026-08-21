# Physara 开发计划

> 本文档规划 Physara 渲染器的重构、纠错与功能拓展任务。
> 编号规则：`模块号.子任务号`，与 `Docs/TaskDetail.md` 一一对应；修改时须同步编号。
> 状态标记：`[ ]` 未开始、`[~]` 进行中、`[√]` 完成。本次计划从零开始，当前所有任务均为未开始。
> 子任务只描述目标；实施方案、涉及文件、设计决策和完成记录写入 `Docs/TaskDetail.md`。
>
> ⭐核心工作流-每任务落地原则：每项子任务开始时都遵循流程：任务吸收-问题再核验-方案选型-任务落地-结果审查-再验证-任务签收。
> 1. **任务吸收**：该阶段先接取任务，确认任务范围和上下文，明确任务目标。
> 2. **问题再核验**：在任务吸收后，对任务范围内的现有代码进行批判性复核,确认是否存在该问题。
> 3. **方案选型**：在问题再核验后，根据任务目标和现有代码进行方案选择，优先现代最佳引擎实践，架构型的完善避免补丁式更改。
> 4. **任务落地**：在方案选型后，根据任务目标和现有代码进行具体实现。
> 5. **再验证**：在任务落地后，对任务目标和现有代码进行具体实现的结果进行审查，明确方案是否解决问题。
> 6. **任务签收**：再验证后，若实现方案通过，即开始更新文档进行记录；否则将该失败方案和结果按序记录到TaskDetail对应位置，然后从方案选型再次开始该工作流。

## 顺序原则

1. 先建立手工性能基线，再依据实际数据调整性能任务优先级。
2. 先处理跨层地基和渲染框架，再处理具体渲染路径与材质功能。
3. 正确性、架构收敛与性能优化分开判断；未测得收益不引入复杂优化。
4. GPU CPU↔GPU 数据契约变更前后必须运行 `Tools/verify_gpu_contracts.py`。
5. 渲染视觉效果与性能由人工启动渲染器、观察画面或抓帧确认；本计划不建设视觉 golden、截图回归或自动化性能脚本。
6. 新功能建立在稳定的材质、场景、资源与渲染管线地基之上。

---

## 阶段 0 — 前置测量与契约检查

### 模块 P — 性能基线与 GPU 契约

- [ ] P.1 建立手工性能基线：固定场景、相机、分辨率、渲染设置和构建配置；由人工运行或抓帧记录 CPU、GPU、各 pass 时间及硬件环境，作为后续对照基线。
- [ ] P.2 完善并运行 GPU CPU↔GPU 契约检查：以 `Tools/verify_gpu_contracts.py` 校验结构布局、字段、SSBO 前缀、绑定及着色器声明；同步清理脚本和契约头中已失效的任务编号注释，防止契约漂移。
- [ ] P.3 基于 P.1 的人工测量结果，复核阶段一中性能相关任务的实际优先级。

---

## 阶段一 — 核心架构地基

### 模块 0 — RHI / OpenGL Backend 提交层

- [ ] 0.1 评估并引入可选命令录制/回放层，明确命令数据所有权、资源保活、提交与回读语义。
- [ ] 0.2 完善 OpenGL 状态缓存失效模型，覆盖外部 GL、ImGui、资源销毁和名称复用。
- [ ] 0.3 整理现有提交热循环去重逻辑，保留必要统计与回归检查。
- [ ] 0.4 评估资源脏槽与资源集惰性绑定，减少无变化的绑定提交。
- [ ] 0.5 评估 shader variant 异步编译与 program binary 缓存的可行架构。
- [ ] 0.6 拆分 `OpenGLCommandList`，分离状态缓存、管线应用、资源绑定、barrier、计时与 ImGui 互操作职责。
- [ ] 0.7 收敛纯枚举到 OpenGL 类型的映射来源，消除重复转换逻辑，并补全 storage image 对 2D array 等维度的 layered 绑定语义。
- [ ] 0.8 按职责拆分 `RHIDefinitions.hpp`，降低公共头耦合。
- [ ] 0.9 使 ImGui 后端通过 RHI 提交，减少直接 OpenGL 调用与状态耦合。
- [ ] 0.10 加固 `PipelineStateCache` 与 `MeshGPUCache`，处理哈希碰撞、资源身份及失效生命周期。
- [ ] 0.11 明确并统一 `RHIResourceSet` 与 `GPUResourceSetIndex` 的资源集语义。
- [ ] 0.12 完善精确 barrier 语义和 RenderGraph 依赖处理，避免不必要的全局 barrier。
- [ ] 0.13 建立 bindless 纹理驻留、预算、引用和延迟回收管理。
- [ ] 0.14 在测量确认后优化上传同步路径，评估 persistent mapping 与 fence 方案。
- [ ] 0.15 聚合并限频 OpenGL 调试回调，保留关键错误与诊断信息。
- [ ] 0.16 为 Reverse-Z 建立 RHI depth compare、clear depth 与 clip-depth 基础能力，并统一 Depth24Stencil8、Depth32F 等深度格式的附件识别与映射。

### 模块 1 — 公共基础设施收敛

- [ ] 1.1 清理重复的 `MaxValue<T>` 实现，统一使用标准库能力。
- [ ] 1.2 清理 `RGBuilder` 冗余短转发 API，保留语义明确的资源访问声明。
- [ ] 1.3 归位通用哈希、对齐和容量增长工具，保留领域语义独立性。

### 模块 2 — RenderGraph / Renderer 框架

- [ ] 2.1 拆分 `Renderer::BuildRenderGraph()`，按渲染职责组织 pass 注册单元。
- [ ] 2.2 收敛重复的 pass context 填充，建立帧级公共上下文边界。
- [ ] 2.3 从 `Renderer` 中剥离 benchmark、capture 等辅助职责。
- [ ] 2.4 收敛 resize 重建流程，处理连续 resize、最小化和恢复场景，并保证导入纹理的描述完整保留颜色空间等资源语义。
- [ ] 2.5 完善 Renderer shutdown/reinitialize 生命周期与资源释放。
- [ ] 2.6 收敛 debug view 的枚举、参数来源与 shader 分发，消除 Forward、Deferred、Composite 间散落的魔法下标。

### 模块 H2 — Scene / Resource / Serialization / Platform 卫生

- [ ] H2.1 建立 SceneSerializer 单点字段维护、版本迁移与原子加载机制；未知版本须显式拒绝或迁移，失败不得清空原 Scene。
- [ ] H2.2 分离 GLTFLoader 的文档解析、资源注册与场景构建职责；收敛重复 Material→MaterialComponent 转换并保证所有材质字段传递完整，避免重复解析同一文档。
- [ ] H2.3 拆分 Scene 的实体、关系、相机和变换职责，保证相机查询无副作用；区分纯主相机查询与显式单相机规范化，并为畸形层级销毁提供防御。
- [ ] H2.4 明确 AssetManager 线程访问、资源缓存淘汰与 generation 回绕契约。
- [ ] H2.5 修正 Platform 生命周期、窗口输入、运行时工厂归属与会话级 VSync/文件系统关闭顺序。
- [ ] H2.6 建立源图与采样用途/纹理视图分离的颜色空间模型。
- [ ] H2.7 收敛文件系统边界、字体资产路径和构建可移植性策略。
- [ ] H2.8 完善 IBL 异步作业的取消、合并、关闭与结果代际管理。

### 模块 H1 — Editor / ImGui 架构

- [ ] H1.1 建立 `IPanel` 接口与面板注册表。
- [ ] H1.2 拆分 `EditorContext`，建立 Editor 与 Renderer 的稳定桥接边界。
- [ ] H1.3 分离 PreUpdate、RenderScene、BuildUI 与 Present 绘制阶段，并统一 Editor、Renderer 与引擎 `Time` 的 delta time 来源。
- [ ] H1.4 将 `ComponentDrawer` 迁至实现文件并拆分过长 SceneViewPanel 函数。
- [ ] H1.5 建立集中化输入路由与事件消费优先级，并让 EditorCameraInputFrame 完整快照所消费的输入状态。
- [ ] H1.6 完善 Gizmo、相机同步和 TRS 写回的失败处理与变换策略；明确编辑器相机与场景多相机/主相机策略，禁止隐式破坏场景相机数据。

---

## 阶段二 — 渲染主干纠错与收敛

### 模块 3 — RenderProxy / CommandExecutor

- [ ] 3.1 收敛各类 bucket 的公共容器行为，保留领域数据与职责边界。
- [ ] 3.2 整理现有实例合并、direct/indirect 提交与去重统计能力。
- [ ] 3.3 基于 P.1 数据评估 opaque 排序、透明排序和实例合并策略；同时审计排序键位宽、截断碰撞和完整材质身份比较，保证正确性优先于局部性优化。

### 模块 4 — Material 数据层与扩展策略

- [ ] 4.1 校正 MaterialInstance、MaterialSignature、MaterialInstanceRegistry 与 MaterialTextureCache 的职责边界。
- [ ] 4.2 决策材质扩展路线：胖 GPU 结构/uber shader 或 feature variant/材质分桶，并记录架构决定。
- [ ] 4.3 建立 MaterialComponent 与 MaterialSignature 字段同步机制。
- [ ] 4.4 收敛材质从组件到 GPU 的数据流，移除不必要的整份组件副本。

### 模块 5 — ShadowPass

- [ ] 5.1 基于新的性能基线拆解 CSM、caster culling、级联数和分辨率的真实成本。
- [ ] 5.2 治理 shadow bias、padding、clamp 与 UI/CPU/GPU 参数单位。
- [ ] 5.3 对比不同 shadow filter 的画质与光照采样成本。
- [ ] 5.4 实现 alpha-mask shadow caster 的 UV、alpha 采样与 discard 正确性。

### 模块 6 — GBuffer / Deferred / Forward

- [ ] 6.1 统一方向光、局部光、发光、IBL、Skybox 与后处理的曝光职责；明确 pre-exposure 只在 CPU 或 GPU 的单一归属层应用，定义 `ev100` 与 exposure compensation 的组合公式，消除路径间隐式常量偏移。
- [ ] 6.2 依据测量评估 Deferred 无效像素区域的优化必要性。
- [ ] 6.3 审计 BRDF 路径的一致性，决定 f90 与 Lambert/Burley 的使用策略。

### 模块 7 — IBL / ClusteredLighting

- [ ] 7.1 为 SH 系数、顺序、归一化与 diffuse 卷积建立可重复的数值检查流程。
- [ ] 7.2 检查 Cluster slice 的 CPU/GPU 边界公式和极值行为。
- [ ] 7.3 依据光源规模评估 Cluster 全量重建与增量更新策略。

### 模块 8 — PostProcess / Bloom

- [ ] 8.1 评估 Bloom mip 层级、pass 数和资源布局的收敛方案。
- [ ] 8.2 评估 Bloom prefilter、downsample 与资源通道合并方案。

### 模块 9 — 收尾清理

- [ ] 9.1 复查并清理死代码、冗余接口和失效注释。
- [ ] 9.2 根据实际实现同步更新 `Docs/Physara.md`。

---

## 阶段三 — 材质、光照与相机功能拓展

### 模块 10 — 材质扩展机制

- [ ] 10.1 按 4.2 的架构决策建立可插拔材质布局、feature mask、pipeline 和 shader 文件组织。
- [ ] 10.2 建立 `KHR_materials_*` 的通用解析与注册框架。
- [ ] 10.3 建立 `PrepareMaterial()` 的着色模型扩展点。
- [ ] 10.4 将硬编码 BRDF 改为可按着色模型分发的扩展架构。

### 模块 11 — Sheen / Cloth

- [ ] 11.1 增加 sheen CPU 数据、纹理槽、序列化、Inspector 与签名支持。
- [ ] 11.2 解析 `KHR_materials_sheen`。
- [ ] 11.3 实现 Charlie NDF 与 Neubelt Visibility。
- [ ] 11.4 接入 Cloth/Sheen 着色模型分支。

### 模块 12 — Clearcoat

- [ ] 12.1 增加 clearcoat CPU 数据、纹理槽与 GPU 契约支持。
- [ ] 12.2 解析 `KHR_materials_clearcoat`。
- [ ] 12.3 实现 Kelemen Visibility 与双层 clearcoat 组合。
- [ ] 12.4 实现 clearcoat 独立法线纹理通道。

### 模块 13 — Subsurface / Transmission / Refraction

- [ ] 13.1 按选定近似方案分批加入次表面、传输、体积和折射 CPU 数据。
- [ ] 13.2 解析 transmission、volume、ior、dispersion 等 glTF 扩展。
- [ ] 13.3 设计并实现实时次表面散射近似。
- [ ] 13.4 建立折射材质与屏幕空间折射接入方案，复用阶段四共享基建。

### 模块 14 — 物理光照

- [ ] 14.1 接入 IES Profile 资产、GPU 数据和光照采样。
- [ ] 14.2 实现 Area Light 的数据、LTC 计算、剔除、预算与编辑器支持。
- [ ] 14.3 实现点光/聚光阴影的分配、更新、过滤与光照接入。

### 模块 15 — 物理相机

- [ ] 15.1 实现 Reverse-Z 与无限远透视渲染投影，保留有限 culling projection。
- [ ] 15.2 完善 RenderView / FrameData 的单帧相机快照。
- [ ] 15.3 对接 6.1 的曝光职责，形成统一相机曝光链路。
- [ ] 15.4 审计并迁移全部 Reverse-Z 深度消费者。

---

## 阶段四 — 高级渲染特性

### 模块 16 — 屏幕空间反射与折射

- [ ] 16.1 建立透明前 scene-color copy 与 mip 链共享基建。
- [ ] 16.2 实现 SSR，并与 IBL 反射进行混合和回退。
- [ ] 16.3 将屏幕空间折射建立在 16.1 的共享资源基础上。

### 模块 17 — 后处理与物理相机进阶

- [ ] 17.1 增加 focus distance 并实现 DoF。
- [ ] 17.2 实现 TAA、jitter、velocity、history 与失效管理。
- [ ] 17.3 实现 Color Grading LUT，并明确曝光、分级和显示变换顺序。

### 模块 18 — 光照与 AO 进阶

- [ ] 18.1 增加物理太阳角径，并统一高光与阴影半影响应。
- [ ] 18.2 分阶段实现 GTAO、temporal AO 与可选高级 AO。
- [ ] 18.3 补全 ADD、SCREEN、MULTIPLY、FADE 等混合模式。

---

## 当前暂缓项

- 多线程 render prepare / job 系统：等待人工性能基线确认 CPU 侧瓶颈后再评估。
- 自动曝光：当前维持手动 EV 工作流，需求明确后单列。
- Specialization constants：不作为当前 OpenGL GLSL 路径目标。
- 多局部 IBL Probe 混合、雾与体积光：需求明确后再纳入计划。

---

## 关联文档

- 任务实现方案与记录：`Docs/TaskDetail.md`
- Filament 渲染参考：`Docs/FilamentRenderSide.md`
- Filament 应用侧参考：`Docs/FilamentAppSide.md`
- 当前工程结构：`Docs/Physara.md`
