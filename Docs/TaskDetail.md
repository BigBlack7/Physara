# Physara 任务实现方案存档

> 本文档与 `Docs/Development.md` 的任务编号一一对应,记录每个子任务的**具体实现方案思路**。
> 不重复 `Development.md` 中已写明的任务目标,只存档"怎么做"。
> 使用方式:开始某个子任务前,在对应编号下补充方案要点、关键设计决策、涉及文件清单;完成后可补充实际结果与偏差说明。
> 模板占位统一使用 `_(待填充)_`,填写时直接替换即可,无需改变文档结构。

---

## 阶段 0 — 前置基建:测量与验证

### 模块 P — 性能基线与验证基建

#### P.1 性能基线测量(CPU/GPU bound 判定)
**[已完成 2026-07-25]** 默认场景 1154×673、VSync 关、2.49M tris:FPS 43;GPU med/p95 22.68/24.18ms、CPU med/p95 12.47/43.15ms → **GPU-bound**。GPU 分解:Shadow 15.48(64%)/Forward 7.97/Post 0.57/其余≈0;Shadow 152 draw(4级联×~38,零裁剪)。CPU p95 尖刺 43ms 待查(疑 buffer 迁移/PSO 编译)。→ 结论见 P.3。

#### P.2 golden image 回归 + 契约校验基建
_(待填充)_

#### P.3 依据测量结论裁定阶段一顺序
**[已完成]** GPU-bound 定序:模块 5 Shadow = #1 fps 目标;模块 6.2/6.4 → 7 次之;模块 8 Bloom(0.57ms)fps 降级为代码整洁;模块 0 维持架构地基不抢跑。推翻"Bloom 第二嫌疑""CommandStream 救 fps"。

#### P.4 CPU↔GPU 结构体布局契约防漂移
_(待填充)_

---

## 阶段一:重构主干与纠错

### 模块 0 — RHI / Backend 提交层

#### 0.1 命令缓冲机制
_(待填充)_

#### 0.2 OpenGLState 状态缓存完整化
_(待填充)_

#### 0.3 提交热循环状态去重缓存
_(待填充)_

#### 0.4 Descriptor 惰性绑定
_(待填充)_

#### 0.5 PSO 异步编译 + program binary 缓存
_(待填充)_

#### 0.6 拆分 OpenGLCommandList 上帝类
_(待填充)_

#### 0.7 统一枚举→GL 映射源
_(待填充)_

#### 0.8 拆分 RHIDefinitions.hpp
_(待填充)_

#### 0.9 ImGui 后端走 RHI
_(待填充)_

#### 0.10 PipelineStateCache 碰撞加固
_(待填充)_

#### 0.11 描述符模型统一(RHIResourceSet ↔ GPUResourceSetIndex)
_(待填充)_

#### 0.12 barrier 精度(去除 GL_ALL_BARRIER_BITS fallback)
_(待填充)_

#### 0.13 bindless 纹理驻留管理
_(待填充)_

#### 0.14 上传路径同步优化(persistent-mapped + fence,非 UAF 是 perf)
_(待填充)_

#### 0.15 GL 调试回调节流(按 id 去重,治 131186 刷屏)
_(待填充)_

---

### 模块 1 — 公共基础设施收敛

#### 1.1 MaxValue<T> 去重
_(待填充)_

#### 1.2 RGBuilder 冗余 API 清理
_(待填充)_

#### 1.3 工具函数归位约定
_(待填充)_

---

### 模块 2 — RenderGraph / Renderer 提交框架重构

#### 2.1 拆分 BuildRenderGraph 巨函数
_(待填充)_

#### 2.2 消除 PassContext 重复填充
_(待填充)_

#### 2.3 Benchmark/Capture 职责剥离
_(待填充)_

#### 2.4 资源 resize 生命周期安全(DeferredResources / RenderGraph)
_(待填充)_

---

### 模块 3 — RenderProxy / CommandExecutor 整理

#### 3.1 Bucket 结构体模板化统一
_(待填充)_

#### 3.2 提交热循环对接去重缓存
_(待填充)_

---

### 模块 4 — Material 数据层校正与扩展策略

#### 4.1 Material 四元组职责校正
_(待填充)_

#### 4.2 [关键设计决策] 材质扩展路线(胖结构 vs 变体)+ GPU 契约布局
_(待填充)_

#### 4.3 MaterialSignature 同步机制
_(待填充)_

#### 4.4 材质数据流跳数收敛(消除 FrameData 整份组件副本)
_(待填充)_

---

### 模块 5 — ShadowPass 纠错

#### 5.1 级联裁剪补齐
_(待填充)_

#### 5.2 Bias 常量治理
_(待填充)_

#### 5.3 PCF/PCSS 采样开销评估
_(待填充)_

---

### 模块 6 — GBuffer / Deferred / Forward 纠错

#### 6.1 曝光补偿一致性(方向光缺失 pre-exposure)
_(待填充)_

#### 6.2 Deferred Lighting 无效区域开销优化
_(待填充)_

#### 6.3 BRDF 一致性审计(f90/Lambert-Burley)
_(待填充)_

---

### 模块 7 — IBL / ClusteredLighting 纠错

#### 7.1 SH 系数一致性校验
_(待填充)_

#### 7.2 Cluster 深度分片精度统一
_(待填充)_

#### 7.3 Cluster 重建策略评估
_(待填充)_

---

### 模块 8 — PostProcess / Bloom 治理

#### 8.1 Bloom mip 层级与通道合并
_(待填充)_

#### 8.2 Bloom Prefilter 等小 Pass 合并
_(待填充)_

---

### 模块 9 — 收尾清理

#### 9.1 死代码复查
_(待填充)_

#### 9.2 Physara.md 同步更新
_(待填充)_

---

## 阶段一·横向架构地基

### 模块 H1 — Editor / ImGui 架构重构

#### H1.1 IPanel 接口 + 面板注册表
_(待填充)_

#### H1.2 解耦 EditorContext 上帝 DTO / EditorBridge
_(待填充)_

#### H1.3 绘制阶段分离(PreUpdate→RenderScene→BuildUI→Present)
_(待填充)_

#### H1.4 ComponentDrawer 迁 .cpp + 拆 SceneViewPanel 过长函数
_(待填充)_

#### H1.5 输入路由集中化
_(待填充)_

#### H1.6 交互层写回正确性(Gizmo 负缩放/返回值、EditorCamera 帧率相关)
_(待填充)_

---

### 模块 H2 — Scene / Resource / Serialization / Platform 卫生

#### H2.1 SceneSerializer 单点维护(消除改字段改三处)
_(待填充)_

#### H2.2 GLTFLoader 职责分层(Resource / Scene 分离)
_(待填充)_

#### H2.3 拆分 Scene.cpp 过载职责
_(待填充)_

#### H2.4 AssetManager 线程安全策略
_(待填充)_

#### H2.5 Platform 健壮性 + RuntimeBackendFactory 归位
_(待填充)_

#### H2.6 Texture 资产补齐色彩空间
_(待填充)_

---

## 阶段二:功能拓展

### 模块 10 — 材质扩展机制搭建

#### 10.1 可插拔布局/变体扩展策略 + shader 侧文件组织
_(待填充)_

#### 10.2 KHR_materials_* 通用解析框架
_(待填充)_

#### 10.3 PrepareMaterial() 着色模型分支/变体扩展点
_(待填充)_

#### 10.4 可插拔 BRDF 重构(消除硬编码 Lambert+GGX,11-13 前置)
_(待填充)_

---

### 模块 11 — Sheen / Cloth 材质模型

#### 11.1 CPU 字段新增
_(待填充)_

#### 11.2 KHR_materials_sheen 解析
_(待填充)_

#### 11.3 Charlie NDF + Neubelt Visibility
_(待填充)_

#### 11.4 Cloth 着色模型分支
_(待填充)_

---

### 模块 12 — Clearcoat 材质模型

#### 12.1 CPU 字段与纹理槽新增
_(待填充)_

#### 12.2 KHR_materials_clearcoat 解析
_(待填充)_

#### 12.3 Kelemen Visibility + 双层叠加公式
_(待填充)_

#### 12.4 Clearcoat 法线纹理通道
_(待填充)_

---

### 模块 13 — Subsurface 与 Transmission/Refraction 材质模型

#### 13.1 CPU 字段新增
_(待填充)_

#### 13.2 KHR_materials_volume/ior/dispersion 解析
_(待填充)_

#### 13.3 次表面散射漫反射模型
_(待填充)_

#### 13.4 Screen-space refraction 方案
_(待填充)_

---

### 模块 14 — 物理光照补全

#### 14.1 IES Profile 接入
_(待填充)_

#### 14.2 Area Light 实时求值
_(待填充)_

#### 14.3 点光/聚光阴影实现(cube/atlas,承接原 6.2)
_(待填充)_

---

### 模块 15 — 物理相机补全

#### 15.1 Reverse-Z + 无限远渲染投影
_(待填充)_

#### 15.2 CameraInfo 单帧快照校验
_(待填充)_

#### 15.3 曝光/预曝光链路一致性(联动 6.1)
_(待填充)_

#### 15.4 reverse-Z 连带审计(WorldGrid/Composite/Skybox 深度)
_(待填充)_

---

## 阶段三:高级渲染特性(对齐 Filament)

### 模块 16 — 屏幕空间反射与折射(共享场景颜色基建)

#### 16.1 post-opaque 场景颜色拷贝 + mip 链基建
_(待填充)_

#### 16.2 SSR 屏幕空间反射(与 IBL 混合/回退)
_(待填充)_

#### 16.3 折射改建在 16.1 之上(承接 13.4,去重)
_(待填充)_

---

### 模块 17 — 后处理与物理相机进阶

#### 17.1 DoF 景深(消费已有光圈/焦距/对焦距离)
_(待填充)_

#### 17.2 TAA + jitter(与 15.1 reverse-Z 协同)
_(待填充)_

#### 17.3 Color Grading(LUT,曝光→分级→显示变换分层)
_(待填充)_

---

### 模块 18 — 光照与 AO 进阶

#### 18.1 太阳角径 / 软方向光
_(待填充)_

#### 18.2 进阶 AO(bent normal / GTAO 多弹跳 / micro-shadowing)
_(待填充)_

#### 18.3 混合模式补全(ADD/SCREEN/MULTIPLY/FADE)
_(待填充)_
