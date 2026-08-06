# Physara 任务实现方案

> 本文档与 `Docs/Development.md` 的任务编号一一对应。
> 每个子任务当前只保留简要描述。开始实施前，在对应位置补充现状核验、方案选型、涉及文件与验证方式；完成后记录结果与偏差。
> 性能任务由人工启动渲染器或抓帧完成测量，并以阶段 0 基线为对照。GPU 数据契约变更使用 `Tools/verify_gpu_contracts.py` 检查。

---

## 阶段 0 — 前置测量与契约检查

### 模块 P — 性能基线与 GPU 契约

#### P.1 手工性能基线
固定运行条件，由人工记录 CPU、GPU、pass 时间和环境信息，建立后续对照。

#### P.2 GPU CPU↔GPU 契约检查
完善并运行 GPU 契约检查脚本，覆盖结构字段、SSBO 前缀、绑定和着色器声明；同步清理失效任务编号注释。

#### P.3 阶段一性能优先级复核
根据 P.1 的实际测量结果，调整性能相关任务的推进顺序。

---

## 阶段一 — 核心架构地基

### 模块 0 — RHI / OpenGL Backend 提交层

#### 0.1 命令录制/回放层
明确可选命令录制和回放的生命周期、数据所有权与提交语义。

#### 0.2 OpenGL 状态缓存
完善状态缓存失效与外部状态互操作。

#### 0.3 提交热循环去重
整理既有去重逻辑并保留必要统计。

#### 0.4 Descriptor 惰性绑定
评估仅提交发生变化资源槽位的绑定策略。

#### 0.5 PSO 编译与 program binary 缓存
评估异步编译和磁盘缓存的架构、失效与回退策略。

#### 0.6 拆分 OpenGLCommandList
按职责拆分过大的命令列表实现。

#### 0.7 枚举到 GL 映射收敛
统一纯类型映射的来源。

#### 0.8 拆分 RHIDefinitions
按职责拆分 RHI 公共定义。

#### 0.9 ImGui 后端走 RHI
将 ImGui 资源和绘制提交迁入 RHI 边界。

#### 0.10 缓存碰撞与失效
加固管线和网格缓存的身份比较与生命周期失效。

#### 0.11 描述符模型统一
统一资源集与 GPU set index 的真实语义。

#### 0.12 精确 barrier
建立精确资源访问同步和 RenderGraph 依赖规则。

#### 0.13 bindless 纹理驻留
管理 bindless handle 驻留、预算和延迟回收。

#### 0.14 上传同步优化
基于实际测量评估上传路径与 fence 优化。

#### 0.15 GL 调试回调治理
聚合、限频并保留高严重度调试消息。

#### 0.16 Reverse-Z RHI 前置
建立 Reverse-Z 所需 depth compare、clear 与 clip-depth 能力，并统一 Depth24Stencil8、Depth32F 等深度格式的附件识别与映射。

### 模块 1 — 公共基础设施收敛

#### 1.1 MaxValue 去重
清理重复最大值工具并改用标准库。

#### 1.2 RGBuilder 冗余 API 清理
移除无价值短转发，保留明确资源访问 API。

#### 1.3 工具函数归位
收敛通用工具，保留领域工具的独立语义。

### 模块 2 — RenderGraph / Renderer 框架

#### 2.1 拆分 BuildRenderGraph
按渲染职责拆分 RenderGraph pass 注册。

#### 2.2 PassContext 收敛
消除各 pass 的重复帧级上下文填充。

#### 2.3 辅助职责剥离
从 Renderer 剥离 benchmark、capture 等辅助职责。

#### 2.4 Resize 重建收敛
处理高频 resize、最小化和恢复的资源重建，并保证导入纹理的描述完整保留颜色空间等资源语义。

#### 2.5 Renderer 生命周期
补齐 shutdown、重初始化和 GPU 资源释放。

#### 2.6 Debug view 收敛
将 debug view 枚举、参数来源和 shader 分发收敛为单一契约，移除各渲染路径间的魔法下标。

### 模块 H2 — Scene / Resource / Serialization / Platform 卫生

#### H2.1 SceneSerializer 单点维护
统一序列化字段维护、版本迁移和原子加载；未知版本须显式拒绝或迁移，任何失败不得清空原 Scene。

#### H2.2 GLTFLoader 职责分层
分离文档解析、资源注册与场景构建；收敛重复 Material→MaterialComponent 转换，保证字段完整传递并避免重复解析同一文档。

#### H2.3 Scene 职责拆分
拆分实体、关系、相机和变换职责，保证查询无副作用；区分纯主相机查询与显式单相机规范化，并防御畸形层级销毁。

#### H2.4 AssetManager 线程与缓存契约
明确资源管理器线程访问、缓存淘汰和 generation 回绕语义。

#### H2.5 Platform 生命周期
完善窗口库生命周期、输入、运行时工厂归属、VSync 配置和文件系统关闭顺序。

#### H2.6 纹理颜色空间模型
分离源图数据与采样用途/视图的颜色空间解释。

#### H2.7 文件系统与构建边界
收敛路径安全、字体资产路径、平台支持和构建可移植性。

#### H2.8 IBL 异步作业生命周期
管理异步 IBL 作业的取消、关闭和代际结果。

### 模块 H1 — Editor / ImGui 架构

#### H1.1 IPanel 与面板注册表
建立面板接口和注册驱动的 Editor 组织方式。

#### H1.2 EditorContext / EditorBridge
拆分编辑器状态与渲染器交互边界。

#### H1.3 绘制阶段分离
分离场景更新、渲染、UI 构建和呈现阶段，并统一 Editor、Renderer 与引擎 `Time` 的 delta time 来源。

#### H1.4 编辑器源码收敛
迁移 inline 实现并拆分过长 UI 函数。

#### H1.5 输入路由集中化
建立统一的输入捕获和事件消费优先级，并让 EditorCameraInputFrame 完整快照所消费的输入状态。

#### H1.6 TRS 写回与相机同步
处理变换写回失败、特殊矩阵和相机旋转基；明确编辑器相机与场景多相机/主相机策略，禁止隐式破坏场景相机数据。

---

## 阶段二 — 渲染主干纠错与收敛

### 模块 3 — RenderProxy / CommandExecutor

#### 3.1 SidedBuckets 收敛
抽取 bucket 公共容器行为，保留领域职责。

#### 3.2 实例与提交去重整理
整理既有实例合并和 direct/indirect 提交能力。

#### 3.3 Draw 排序与合并率
依据实测评估排序正确性、overdraw 与实例合并。

### 模块 4 — Material 数据层与扩展策略

#### 4.1 Material 职责校正
校正材质实例、签名、注册表和纹理缓存的职责。

#### 4.2 材质扩展路线决策
选择胖结构/uber shader 或 feature variant/分桶路线。

#### 4.3 MaterialSignature 同步机制
防止材质字段变化遗漏签名更新。

#### 4.4 材质数据流收敛
移除 GPU 打包链中的不必要组件副本和跳数。

### 模块 5 — ShadowPass

#### 5.1 CSM 成本拆解
根据性能基线分析级联、裁剪、分辨率和提交成本。

#### 5.2 Bias 常量治理
统一阴影 bias、padding、clamp 与参数单位。

#### 5.3 Shadow filter 评估
比较不同阴影滤波方案的画质与性能。

#### 5.4 Alpha-mask shadow caster
实现 alpha-mask 材质的阴影采样与 discard。

### 模块 6 — GBuffer / Deferred / Forward

#### 6.1 曝光职责统一
统一各 radiance 来源与后处理的曝光链路。

#### 6.2 Deferred 无效区域优化
依据测量决定是否优化 Deferred 背景区域。

#### 6.3 BRDF 一致性审计
审计 f90、漫反射和镜面 BRDF 的模型选择。

### 模块 7 — IBL / ClusteredLighting

#### 7.1 SH 系数一致性
检查 SH 基函数、顺序、卷积和归一化一致性。

#### 7.2 Cluster slice 边界
检查 CPU/GPU slice 公式及边界行为。

#### 7.3 Cluster 重建策略
依据光源规模评估全量和增量重建。

### 模块 8 — PostProcess / Bloom

#### 8.1 Bloom mip 收敛
评估 Bloom mip 数、pass 数和资源布局。

#### 8.2 Bloom pass 合并
评估 prefilter、downsample 和资源通道合并。

### 模块 9 — 收尾清理

#### 9.1 死代码复查
清理确认无调用的代码、接口与注释。

#### 9.2 Physara.md 同步
按实际代码结构更新工程说明文档。

---

## 阶段三 — 材质、光照与相机功能拓展

### 模块 10 — 材质扩展机制

#### 10.1 可插拔材质架构
建立材质布局、feature mask、pipeline 与 shader 组织策略。

#### 10.2 KHR_materials 解析框架
建立 glTF 材质扩展的通用解析和注册机制。

#### 10.3 PrepareMaterial 扩展点
建立着色模型输入准备的扩展边界。

#### 10.4 可插拔 BRDF
将硬编码 BRDF 重构为按模型分发的架构。

### 模块 11 — Sheen / Cloth

#### 11.1 Sheen CPU 数据
增加 sheen 数据、纹理槽、序列化和编辑器支持。

#### 11.2 KHR_materials_sheen 解析
解析 sheen glTF 扩展。

#### 11.3 Charlie 与 Neubelt
实现 sheen 所需 NDF 与可见性项。

#### 11.4 Cloth 着色模型
接入 Cloth/Sheen 的材质和光照路径。

### 模块 12 — Clearcoat

#### 12.1 Clearcoat CPU 数据
增加 clearcoat 数据、纹理槽与 GPU 契约。

#### 12.2 KHR_materials_clearcoat 解析
解析 clearcoat glTF 扩展。

#### 12.3 Clearcoat BRDF
实现双层 clearcoat 的可见性和组合公式。

#### 12.4 Clearcoat 法线
实现 clearcoat 独立法线纹理通道。

### 模块 13 — Subsurface / Transmission / Refraction

#### 13.1 CPU 数据扩展
按近似方案分批扩展次表面、传输、体积和折射数据。

#### 13.2 glTF 扩展解析
解析 transmission、volume、ior 与 dispersion 扩展。

#### 13.3 次表面散射模型
实现适用于实时渲染的次表面散射近似。

#### 13.4 折射方案
建立折射材质与屏幕空间折射的接入策略。

### 模块 14 — 物理光照

#### 14.1 IES Profile
接入 IES 资产、GPU 数据和光照采样。

#### 14.2 Area Light
实现面积光的实时数据、求值和编辑器支持。

#### 14.3 点光/聚光阴影
实现点光与聚光阴影的分配、更新和光照接入。

### 模块 15 — 物理相机

#### 15.1 Reverse-Z 与无限远投影
实现 Reverse-Z 渲染投影并保留有限裁剪投影。

#### 15.2 单帧相机快照
完善 RenderView / FrameData 的值语义快照。

#### 15.3 曝光链路对接
对接统一曝光职责与相机参数。

#### 15.4 Reverse-Z 消费者审计
迁移深度重建、裁剪、调试和渲染状态消费者。

---

## 阶段四 — 高级渲染特性

### 模块 16 — 屏幕空间反射与折射

#### 16.1 共享 scene-color 基建
建立透明前 scene-color copy 与 mip 链。

#### 16.2 SSR
实现屏幕空间反射及 IBL 回退。

#### 16.3 屏幕空间折射复用
使折射复用模块 16.1 的共享资源。

### 模块 17 — 后处理与物理相机进阶

#### 17.1 DoF
增加对焦距离并实现物理景深。

#### 17.2 TAA
实现 jitter、velocity、history 与重投影。

#### 17.3 Color Grading
实现 LUT 色彩分级与固定后处理顺序。

### 模块 18 — 光照与 AO 进阶

#### 18.1 太阳角径
用物理太阳角径统一高光和阴影半影。

#### 18.2 GTAO 与进阶 AO
分阶段实现基础、时域和可选高级 AO。

#### 18.3 混合模式补全
补全 ADD、SCREEN、MULTIPLY、FADE 等混合模式。
