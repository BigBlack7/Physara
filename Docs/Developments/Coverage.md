# 功能与工程覆盖索引

[开发计划与当前阶段](Developments.md#current-stage)

## 5. 全功能与工程覆盖核对

### 5.1 架构单元到工作包

下表覆盖 Architecture.md 的全部细分设计单元。“主要工作包”是首次闭环与关键扩充位置，关联不代表每个引用都要另开 change；各阶段回归还会消费此前能力。

| 架构单元 | 主要工作包 | 闭环重点 |
|---|---|---|
| [1.1 CMake、ThirdParty 与构建依赖](../Architecture.md#build) | [D00-T02](D00-D02-Foundation.md#d00-t02)、[D00-T03](D00-D02-Foundation.md#d00-t03)、[D17-T05](D16-D17-EditorAndDelivery.md#d17-t05) | 单向构建、锁定依赖、干净构建与部署 |
| [1.2 Core：后续阶段共同使用的最小机制](../Architecture.md#core) | [D01-T01](D00-D02-Foundation.md#d01-t01)、[D04-T04](D03-D05-FramePipeline.md#d04-t04)、[D06-T01](D06-D07-SceneAndForward.md#d06-t01)、[D08-T07](D08-D11-RenderPaths.md#d08-t07) | 身份/计时、有实际消费者的 arena/作业/IO 与变更通知 |
| [1.3 Shader 构建：先生成合同，再形成编译包](../Architecture.md#shader-build) | [D03-T01](D03-D05-FramePipeline.md#d03-t01)、[D03-T02](D03-D05-FramePipeline.md#d03-t02)、[D08-T07](D08-D11-RenderPaths.md#d08-t07) | 生成 ABI、canonical IR、反射/lowering、完整代发布 |
| [1.4 Runtime 单入口与长期对象装配](../Architecture.md#runtime) | [D01-T04](D00-D02-Foundation.md#d01-t04)、[D04-T01](D03-D05-FramePipeline.md#d04-t01)、[D17-T05](D16-D17-EditorAndDelivery.md#d17-t05) | 单 Main.cpp、对象装配、编辑器产品入口 |
| [1.5 Platform 创建窗口、context 与 RHI Device](../Architecture.md#platform-start) | [D01-T02](D00-D02-Foundation.md#d01-t02)、[D02-T03](D00-D02-Foundation.md#d02-t03)、[D19-T02](D18-D19-Extensions.md#d19-t02) | 窗口/context/设备边界与部分初始化退出 |
| [2.1 窗口事件、输入快照与坐标单位](../Architecture.md#input) | [D01-T03](D00-D02-Foundation.md#d01-t03)、[D04-T01](D03-D05-FramePipeline.md#d04-t01)、[D06-T06](D06-D07-SceneAndForward.md#d06-t06) | 事件/快照、逻辑像素与 DPI、捕获/失焦 |
| [2.2 EditorSession、面板与 UI 输入解释](../Architecture.md#editor-input) | [D04-T01](D03-D05-FramePipeline.md#d04-t01)、[D04-T05](D03-D05-FramePipeline.md#d04-t05)、[D06-T06](D06-D07-SceneAndForward.md#d06-t06) | Session/面板、私有 GLFW 适配、编辑意图 |
| [2.3 领域命令、撤销与应用边界](../Architecture.md#edits) | [D04-T05](D03-D05-FramePipeline.md#d04-t05)、[D06-T06](D06-D07-SceneAndForward.md#d06-t06)、[D16-T02](D16-D17-EditorAndDelivery.md#d16-t02) | 领域事务、拖拽合并、撤销/重做、保存 |
| [2.4 视口输入、相机/Gizmo 与拾取请求](../Architecture.md#viewport-input) | [D06-T06](D06-D07-SceneAndForward.md#d06-t06)、[D16-T03](D16-D17-EditorAndDelivery.md#d16-t03)、[D16-T04](D16-D17-EditorAndDelivery.md#d16-t04) | 导航/Gizmo、坐标、异步选择与过期身份 |
| [3.1 AssetManager 与标准数据合同](../Architecture.md#asset-types) | [D06-T01](D06-D07-SceneAndForward.md#d06-t01)、[D16-T01](D16-D17-EditorAndDelivery.md#d16-t01) | 标准资产、不可变发布、源身份/内容版本 |
| [3.2 glTF 模型解码与校验](../Architecture.md#asset-import) | [D06-T02](D06-D07-SceneAndForward.md#d06-t02)、[D12-T06](D12-D13-Materials.md#d12-t06)、[D13-T06](D12-D13-Materials.md#d13-t06) | glTF 语义、Asset 内部 CPU 校验、完整扩展报告 |
| [3.3 纹理、环境与编辑器资源的语义](../Architecture.md#asset-images) | [D06-T01](D06-D07-SceneAndForward.md#d06-t01)、[D06-T02](D06-D07-SceneAndForward.md#d06-t02)、[D07-T04](D06-D07-SceneAndForward.md#d07-t04) | 颜色/数据/环境/UI 图像语义与纹理 key |
| [3.4 场景实例化、重导入与保存](../Architecture.md#asset-persistence) | [D06-T03](D06-D07-SceneAndForward.md#d06-t03)、[D16-T01](D16-D17-EditorAndDelivery.md#d16-t01)、[D16-T02](D16-D17-EditorAndDelivery.md#d16-t02) | 实例化、重导入、JSON 场景保存/重开与模型引用恢复 |
| [4.1 Scene、层级与增量变换](../Architecture.md#scene) | [D06-T03](D06-D07-SceneAndForward.md#d06-t03)、[D06-T06](D06-D07-SceneAndForward.md#d06-t06)、[D16-T03](D16-D17-EditorAndDelivery.md#d16-t03) | entt、层级无环、增量 TRS、版本与命令 |
| [4.2 静态内容版本与编辑失效](../Architecture.md#static-content) | [D06-T03](D06-D07-SceneAndForward.md#d06-t03)、[D06-T06](D06-D07-SceneAndForward.md#d06-t06)、[D14-T04](D14-D15-ImagePipeline.md#d14-t04) | DocumentRevision、RenderContentRevision、CameraRevision 与编辑失效 |
| [4.3 相机投影、物理镜头与曝光输入](../Architecture.md#camera) | [D06-T04](D06-D07-SceneAndForward.md#d06-t04)、[D07-T02](D06-D07-SceneAndForward.md#d07-t02)、[D15-T04](D14-D15-ImagePipeline.md#d15-t04) | 标准投影、稳定/抖动、物理曝光/镜头 |
| [4.4 材质、灯光与环境的物理创作参数](../Architecture.md#physical-inputs) | [D07-T01](D06-D07-SceneAndForward.md#d07-t01)、[D07-T02](D06-D07-SceneAndForward.md#d07-t02)、[D07-T03](D06-D07-SceneAndForward.md#d07-t03) | glTF 参数、物理单位、显式环境/镜头设置 |
| [5.1 RenderEngine、Renderer、View 与帧槽](../Architecture.md#frame-state) | [D04-T04](D03-D05-FramePipeline.md#d04-t04)、[D06-T04](D06-D07-SceneAndForward.md#d06-t04)、[D08-T01](D08-D11-RenderPaths.md#d08-t01) | RenderEngine/Renderer/View、完成驱动 FrameSlot |
| [5.2 SceneFrame：从 ECS 到连续只读快照](../Architecture.md#prepare) | [D06-T04](D06-D07-SceneAndForward.md#d06-t04)、[D08-T01](D08-D11-RenderPaths.md#d08-t01) | 一次提取、只读连续快照、多 View 共享 |
| [5.3 View 相机、裁剪与可见工作集](../Architecture.md#view-preparation) | [D06-T04](D06-D07-SceneAndForward.md#d06-t04)、[D08-T01](D08-D11-RenderPaths.md#d08-t01)、[D10-T01](D08-D11-RenderPaths.md#d10-t01) | 相机/culling、可见集、View uniform |
| [5.4 灯列表、阴影计划与环境准备](../Architecture.md#light-preparation) | [D07-T03](D06-D07-SceneAndForward.md#d07-t03)、[D09-T01](D08-D11-RenderPaths.md#d09-t01)、[D10-T01](D08-D11-RenderPaths.md#d10-t01) | 完整灯集合、保守阴影计划、Froxel |
| [5.5 GPU 驻留、材质绑定与上传包](../Architecture.md#gpu-residency) | [D06-T05](D06-D07-SceneAndForward.md#d06-t05)、[D08-T02](D08-D11-RenderPaths.md#d08-t02)、[D16-T01](D16-D17-EditorAndDelivery.md#d16-t01) | 资源/实例材质/静态内容版本、上传包、重导入退休 |
| [5.6 材质路由、DrawRecord、排序与 Batch](../Architecture.md#draws) | [D08-T03](D08-D11-RenderPaths.md#d08-t03)、[D11-T02](D08-D11-RenderPaths.md#d11-t02)、[D13-T01](D12-D13-Materials.md#d13-t01) | 完整 key/实例/indirect、材质路由与透明顺序 |
| [5.7 Pipeline 准备、热重载与路径切换事务](../Architecture.md#plan-readiness) | [D08-T07](D08-D11-RenderPaths.md#d08-t07)、[D10-T04](D08-D11-RenderPaths.md#d10-t04)、[D11-T04](D08-D11-RenderPaths.md#d11-t04)、[D15-T06](D14-D15-ImagePipeline.md#d15-t06) | 有限变体、热重载代、原子路径/AA 切换 |
| [6.1 三路径子图与完整 Pass 生产消费](../Architecture.md#paths) | [D06-T05](D06-D07-SceneAndForward.md#d06-t05)、[D10-T03](D08-D11-RenderPaths.md#d10-t03)、[D11-T03](D08-D11-RenderPaths.md#d11-t03) | 两个策略组装入口与统一上下游输出 |
| [6.2 图资源版本、Pass 数据与权限](../Architecture.md#graph) | [D05-T01](D03-D05-FramePipeline.md#d05-t01)、[D05-T04](D03-D05-FramePipeline.md#d05-t04) | typed ID/版本、声明权限、拥有型 PassData |
| [6.3 依赖、活性、拓扑与物理计划](../Architecture.md#graph-compile) | [D05-T02](D03-D05-FramePipeline.md#d05-t02)、[D08-T06](D08-D11-RenderPaths.md#d08-t06) | hazards/活性/拓扑/物理计划、无非法图兜底 |
| [6.4 资源池、跨帧输入与外部输出](../Architecture.md#graph-resources) | [D05-T03](D03-D05-FramePipeline.md#d05-t03)、[D08-T06](D08-D11-RenderPaths.md#d08-t06)、[D14-T04](D14-D15-ImagePipeline.md#d14-t04) | 首次使用分配、兼容池、跨帧角色与预算 |
| [7.1 NVRHI 公共层的移植与命名边界](../Architecture.md#rhi) | [D02-T01](D00-D02-Foundation.md#d02-t01)、[D02-T02](D00-D02-Foundation.md#d02-t02)、[D19-T01](D18-D19-Extensions.md#d19-t01) | 固定 NVRHI 源基线、完整公共谱系与差异映射 |
| [7.2 Device、资源、绑定、Pipeline 与 Validation](../Architecture.md#rhi-objects) | [D02-T03](D00-D02-Foundation.md#d02-t03)、[D03-T02](D03-D05-FramePipeline.md#d03-t02)、[D19-T03](D18-D19-Extensions.md#d19-t03)、[D19-T04](D18-D19-Extensions.md#d19-t04) | 资源/绑定/Pipeline/Validation 与 native 行为 |
| [7.3 OpenGL 资源与 Shader/Pipeline lowering](../Architecture.md#opengl) | [D02-T03](D00-D02-Foundation.md#d02-t03)、[D03-T02](D03-D05-FramePipeline.md#d03-t02)、[D03-T03](D03-D05-FramePipeline.md#d03-t03) | DSA、分离纹理采样器 lowering、GL builtin/坐标语义 |
| [7.4 UploadBatch、volatile 常量与 push constants](../Architecture.md#uploads) | [D02-T04](D00-D02-Foundation.md#d02-t04)、[D08-T02](D08-D11-RenderPaths.md#d08-t02)、[D10-T02](D08-D11-RenderPaths.md#d10-t02) | 拥有源数据、持久映射/volatile/push、完成驱动复用 |
| [7.5 图执行器、访问用途与单一同步发射者](../Architecture.md#barriers) | [D05-T03](D03-D05-FramePipeline.md#d05-t03)、[D08-T04](D08-D11-RenderPaths.md#d08-t04)、[D19-T05](D18-D19-Extensions.md#d19-t05) | GraphManaged/自动入口、单 tracker/emitter、多消费域 |
| [7.6 即时 CommandList、状态缓存与真实 Draw](../Architecture.md#command-execution) | [D03-T03](D03-D05-FramePipeline.md#d03-t03)、[D08-T03](D08-D11-RenderPaths.md#d08-t03)、[D08-T04](D08-D11-RenderPaths.md#d08-t04) | 即时列表不重放、状态缓存、draw/compute/copy |
| [7.7 能力与不支持项的可见合同](../Architecture.md#capabilities) | [D02-T05](D00-D02-Foundation.md#d02-t05)、[D08-T04](D08-D11-RenderPaths.md#d08-t04)、[D17-T01](D16-D17-EditorAndDelivery.md#d17-t01) | 真实 limits/格式/队列与可见不支持原因 |
| [7.8 同一合同下的 Vulkan 接入位置](../Architecture.md#vulkan) | [D19-T01](D18-D19-Extensions.md#d19-t01)、[D19-T02](D18-D19-Extensions.md#d19-t02)、[D19-T05](D18-D19-Extensions.md#d19-t05)、[D19-T07](D18-D19-Extensions.md#d19-t07) | 同合同后端、WSI 寿命、完整跨后端验证 |
| [8.1 GLSL 模块架构与薄 Pass 入口](../Architecture.md#shader-runtime) | [D03-T01](D03-D05-FramePipeline.md#d03-t01)、[D07-T01](D06-D07-SceneAndForward.md#d07-t01)、[D08-T07](D08-D11-RenderPaths.md#d08-t07) | 职责分层、薄入口、受控变体/依赖 |
| [8.2 静态几何与 Depth/Normal/GBuffer](../Architecture.md#geometry) | [D06-T05](D06-D07-SceneAndForward.md#d06-t05)、[D11-T01](D08-D11-RenderPaths.md#d11-t01)、[D11-T02](D08-D11-RenderPaths.md#d11-t02)、[D16-T04](D16-D17-EditorAndDelivery.md#d16-t04) | 统一静态几何/覆盖、ID与成对历史深度、核心 GBuffer/SurfaceAux |
| [8.3 生成阴影并形成可查询可见性](../Architecture.md#shadows) | [D09-T01](D08-D11-RenderPaths.md#d09-t01)、[D09-T02](D08-D11-RenderPaths.md#d09-t02)、[D09-T03](D08-D11-RenderPaths.md#d09-t03)、[D09-T04](D08-D11-RenderPaths.md#d09-t04) | CSM/spot/point、Hard/PCF、预算与画外 caster |
| [8.4 AO、接触阴影与前帧辐亮度 SSR](../Architecture.md#surface-effects) | [D14-T01](D14-D15-ImagePipeline.md#d14-t01)、[D14-T02](D14-D15-ImagePipeline.md#d14-t02)、[D14-T03](D14-D15-ImagePipeline.md#d14-t03)、[D14-T04](D14-D15-ImagePipeline.md#d14-t04) | 完整表面、SAO/contact/SSR、正确历史/曝光 |
| [8.5 MaterialInputs 到 PixelParams 的统一物理模型](../Architecture.md#materials) | [D07-T01](D06-D07-SceneAndForward.md#d07-t01)、[D12-T06](D12-D13-Materials.md#d12-t06)、[D13-T06](D12-D13-Materials.md#d13-t06) | 核心+全部目标标准扩展、直接光/IBL/路由闭环与实例覆盖 |
| [8.6 方向/局部灯、阴影与 IBL 的统一聚合](../Architecture.md#lighting) | [D07-T03](D06-D07-SceneAndForward.md#d07-t03)、[D07-T04](D06-D07-SceneAndForward.md#d07-t04)、[D10-T03](D08-D11-RenderPaths.md#d10-t03)、[D11-T03](D08-D11-RenderPaths.md#d11-t03) | 同一聚合器、候选选择、阴影/环境/DFG |
| [8.7 Forward、Forward+ 与 Deferred 如何消费同一模型](../Architecture.md#opaque-shading) | [D07-T03](D06-D07-SceneAndForward.md#d07-t03)、[D10-T03](D08-D11-RenderPaths.md#d10-t03)、[D11-T03](D08-D11-RenderPaths.md#d11-t03) | 三路径同模型/单位，HDR 等价 |
| [8.8 天空背景、透明覆盖与物理透射](../Architecture.md#transparency) | [D13-T01](D12-D13-Materials.md#d13-t01)、[D13-T02](D12-D13-Materials.md#d13-t02)、[D13-T03](D12-D13-Materials.md#d13-t03)、[D13-T06](D12-D13-Materials.md#d13-t06) | 透明排序、背景/历史、透射/吸收/色散/reactive |
| [9.1 相机重投影、曝光与历史有效性](../Architecture.md#temporal-contract) | [D14-T04](D14-D15-ImagePipeline.md#d14-t04)、[D15-T01](D14-D15-ImagePipeline.md#d15-t01)、[D15-T03](D14-D15-ImagePipeline.md#d15-t03) | 相机重投影、静态内容版本、历史角色、曝光/有效性 |
| [9.2 TAA 重投影、裁剪与累积](../Architecture.md#taa) | [D15-T03](D14-D15-ImagePipeline.md#d15-t03)、[D15-T07](D14-D15-ImagePipeline.md#d15-t07) | 重投影/拒绝/裁剪、reactive、三路径序列回归 |
| [9.3 DoF、Bloom 与镜头效果](../Architecture.md#lens-effects) | [D15-T04](D14-D15-ImagePipeline.md#d15-t04)、[D15-T05](D14-D15-ImagePipeline.md#d15-t05)、[D18-T05](D18-D19-Extensions.md#d18-t05) | 物理 DoF、HDR Bloom、可选 flare |
| [9.4 ToneMap、ColorGrading、FXAA 与 ViewOutput](../Architecture.md#display-output) | [D07-T05](D06-D07-SceneAndForward.md#d07-t05)、[D15-T06](D14-D15-ImagePipeline.md#d15-t06) | 固定 ToneMapper/原色适配、LUT/FXAA/输出 |
| [10.1 编辑叠加、视口输出与本帧纹理解析](../Architecture.md#editor-output) | [D05-T04](D03-D05-FramePipeline.md#d05-t04)、[D11-T05](D08-D11-RenderPaths.md#d11-t05)、[D16-T03](D16-D17-EditorAndDelivery.md#d16-t03) | 稳定 UI 纹理、本帧解析、typed preview/叠加 |
| [10.2 ImGui RHI Renderer 与纹理协议](../Architecture.md#imgui-renderer) | [D04-T02](D03-D05-FramePipeline.md#d04-t02)、[D05-T04](D03-D05-FramePipeline.md#d05-t04)、[D19-T06](D18-D19-Extensions.md#d19-t06) | RHI VB/IB/clip/offset、纹理协议、线性合成 |
| [10.3 Platform 末端复制、提交与窗口呈现](../Architecture.md#present) | [D04-T03](D03-D05-FramePipeline.md#d04-t03)、[D19-T02](D18-D19-Extensions.md#d19-t02)、[D19-T06](D18-D19-Extensions.md#d19-t06) | GL 末端复制在 fence 前、Vulkan acquire/present 独立寿命 |
| [11.1 RHI 在途引用与原生删除](../Architecture.md#retirement) | [D03-T04](D03-D05-FramePipeline.md#d03-t04)、[D08-T02](D08-D11-RenderPaths.md#d08-t02)、[D17-T04](D16-D17-EditorAndDelivery.md#d17-t04) | 在途引用、完成票据、context 前删除 |
| [11.2 FrameSlot 复用与 View 历史提交](../Architecture.md#history-commit) | [D05-T05](D03-D05-FramePipeline.md#d05-t05)、[D14-T04](D14-D15-ImagePipeline.md#d14-t04)、[D15-T01](D14-D15-ImagePipeline.md#d15-t01) | 只有成功场景提交推进，失败/UI-only/跳帧不混用 |
| [11.3 Event/Timer、截图与异步拾取](../Architecture.md#readbacks) | [D08-T05](D08-D11-RenderPaths.md#d08-t05)、[D16-T04](D16-D17-EditorAndDelivery.md#d16-t04)、[D16-T05](D16-D17-EditorAndDelivery.md#d16-t05)、[D19-T05](D18-D19-Extensions.md#d19-t05) | query/row pitch、截图出口、异步身份安全拾取 |
| [11.4 统计快照、缓存预算与诊断面板](../Architecture.md#diagnostics) | [D08-T08](D08-D11-RenderPaths.md#d08-t08)、[D16-T06](D16-D17-EditorAndDelivery.md#d16-t06)、[D17-T02](D16-D17-EditorAndDelivery.md#d17-t02) | 事实 owner、快照/节流/容量、图/资源/成本 |
| [11.5 取消、失败与完整关闭](../Architecture.md#shutdown) | [D01-T04](D00-D02-Foundation.md#d01-t04)、[D05-T05](D03-D05-FramePipeline.md#d05-t05)、[D16-T07](D16-D17-EditorAndDelivery.md#d16-t07)、[D19-T02](D18-D19-Extensions.md#d19-t02) | 逐阶段失败释放、停止发布/join、提交/WSI 退出 |
| [12.1 按功能安排验证](../Architecture.md#tests) | [D02-T05](D00-D02-Foundation.md#d02-t05)、[D06-T07](D06-D07-SceneAndForward.md#d06-t07)、[D17-T01](D16-D17-EditorAndDelivery.md#d17-t01)、[D19-T07](D18-D19-Extensions.md#d19-t07) | 按实际功能验证 CPU 合同、原生语义和行为/序列 |
| [12.2 Benchmark 与性能报告时机](../Architecture.md#benchmark) | [D07-T06](D06-D07-SceneAndForward.md#d07-t06)、[D10-T05](D08-D11-RenderPaths.md#d10-t05)、[D17-T02](D16-D17-EditorAndDelivery.md#d17-t02)、[D17-T03](D16-D17-EditorAndDelivery.md#d17-t03) | 随功能埋点、同口径比较、按需正式报告 |
| [12.3 Docs、Reference 与 OpenSpec 的唯一归属](../Architecture.md#project-docs) | [D00-T04](D00-D02-Foundation.md#d00-t04)、[D16-T02](D16-D17-EditorAndDelivery.md#d16-t02)、[D17-T05](D16-D17-EditorAndDelivery.md#d17-t05) | 当前规范/架构、参考、计划与原生 specs/change |
| [12.4 新增能力仍保持一条完整链](../Architecture.md#feature-closure) | [D06-T07](D06-D07-SceneAndForward.md#d06-t07)、[D12-T06](D12-D13-Materials.md#d12-t06)、[D16-T07](D16-D17-EditorAndDelivery.md#d16-t07)、[D17-T01](D16-D17-EditorAndDelivery.md#d17-t01) | 每项贯穿输入、GPU、UI/统计、退休与验证 |

### 5.2 目标 glTF 与重要 profile 的最终交付位置

| 能力 | 基线工作包 | 关键验收 |
|---|---|---|
| 核心节点/mesh/材质/图像/sampler、perspective/orthographic | D06-T01–T05、D07-T01 | 标准解码、默认值、单位/坐标、纹理/法线、可见性 |
| 静态几何与相机重投影 | D06-T03/T05、D14-T04、D15-T03 | 静态覆盖、内容编辑失效、相机/颜色/深度历史同次提交 |
| KHR_lights_punctual | D06-T02、D07-T03、D09 | 物理强度、range、完整灯集合、三类阴影 |
| KHR_materials_unlit | D06-T05、D11-T02 | 免灯光语义、SurfaceAux 遮挡；相机重投影与拾取随 D14/D16 消费时回归 |
| KHR_texture_transform | D06-T02/T05、D12-T06 | 每槽 UV 集/变换与切线相关语义 |
| KHR_materials_emissive_strength | D06-T05、D07-T05、D15-T05 | HDR 自发光、预曝光与显示，不自动产生 Light |
| KHR_materials_ior / specular | D12-T01 | Fresnel、直接光/IBL、ForwardExtension |
| KHR_materials_clearcoat | D12-T02 | 独立上层/法线、基底衰减、环境响应 |
| KHR_materials_sheen | D12-T03 | Charlie/Neubelt、DFG/IBL 与叠层 |
| KHR_materials_anisotropy | D12-T04 | 方向、切线、直接光与 IBL |
| KHR_materials_iridescence | D12-T05 | 薄膜近似、单位、组合参考 |
| 核心 alpha BLEND | D13-T01/T06 | 稳定排序、覆盖、静态深度重投影/reactive、选择策略 |
| KHR_materials_transmission | D13-T03 | 反射/透射、粗糙背景采样、越界退路 |
| KHR_materials_volume | D13-T04 | 标准厚度/吸收、Beer–Lambert 与尺度 |
| KHR_materials_dispersion | D13-T05 | 多波段折射、质量/成本边界 |
| None/FXAA/TAA、SAO/contact/SSR、DoF/Bloom、高度雾、固定显示 profile | D07-T05、D14、D15 | 三路径同配置、历史角色/曝光、序列/颜色回归 |

每种材质扩展还要进入 D12-T06 / D13-T06 的组合、三路径和几何出口回归，以及 D16 的保存/重导入/拾取/编辑链。目标之外的 required 扩展仍拒绝，不能把“不崩溃加载”记为支持。

### 5.3 工程目录的责任与交付阶段

目录沿用 Architecture.md。以下给出开发归属，不增加空模块；文件只在相应能力实际落地时出现。

| 工程位置 | 主要工作包/阶段 | 交付责任 |
|---|---|---|
| 根及各模块 CMakeLists.txt | D00-T02/T03、D17-T05、D19-T01 | target 关系、必要生成步骤、声明配置与部署 |
| RHI 公共头：RHI.hpp / Resource.hpp / Containers.hpp / ShaderTypes.hpp / Utils.hpp | D02-T01 | 完整 NVRHI 公共谱系的命名移植 |
| RHI/Common、Validation、Validation.hpp | D02-T02、D05-T03、D08-T04 | 引用、合同验证、统一状态跟踪 |
| RHI/OpenGL.hpp、RHI/OpenGL | D02–D04、D08 | GL 工厂、资源/Shader/绑定/命令/提交/退休 |
| Platform/DeviceHost.hpp / DeviceHost.cpp / OpenGLDeviceHost.cpp | D01-T02/T04、D04-T03、D05-T05 | 窄宿主、初始化、PresentColor/呈现、取消/退出 |
| Platform/Input.hpp / InputState / GlfwInput.cpp / GlfwAccess.hpp | D01-T03、D04-T01 | 输入状态/单位、窗口事件、Editor 私有适配借用 |
| Engine/Core | D01-T01、D04-T04、D06-T01、D08-T07、D16-T06 | 身份/计时日志、按实际消费接入 arena/作业/IO/取消/变更通知 |
| Engine/Asset | D06-T01/T02、D12/D13、D16-T01/T02 | glTF/GLB 标准导入/发布、扩展、校验与重导入 |
| Engine/Scene | D06-T03/T04、D07-T02/T03、D16-T02 | 实例/层级/变换、可保存快照与候选场景校验、主相机引用、静态创作参数、内容版本 |
| Engine/Shader、Interface | D03-T01、D08-T07 | 权威 ABI、所需编译/生成步骤、包/反射、变体/热重载代 |
| Engine/Renderer 根状态 | D04-T04、D06-T04、D08 | RenderEngine/Renderer/View 与帧内合同 |
| Renderer/Prepare | D06-T04、D08-T01、D10 | 静态场景提取、相机/culling、灯工作集 |
| Renderer/Draw | D06-T05、D08-T03、D11-T02、D13-T01 | 路由、排序、完整 key、实例/indirect |
| Renderer/Materials | D06-T05、D07-T01、D08-T02、D12/D13 | 材质/纹理 GPU 记录、绑定和版本驻留 |
| Renderer/Lighting | D07-T03/T04、D09、D10 | 灯、环境、阴影计划/Froxel 与缓存 |
| Renderer/RenderGraph | D05、D08-T06、D14-T04 | 图合同/编译/执行、对象池、历史与外部输出 |
| Renderer/Paths、Passes | D06-T05、D09–D14 | 三路径固定组装入口和共享真实 Pass |
| Renderer/PostProcess | D07-T05、D15 | 时域、镜头、显示和 ViewOutput |
| Editor/Session、Commands | D04-T01/T05、D06-T06、D16 | 选择/事务/undo、工作区、未保存状态 |
| Editor/Panels | D04-T01、D06-T06、D07-T06、D16-T06 | 随功能接入控制、资源/属性、支持/失败状态 |
| Editor/Viewport | D06-T06、D16-T03/T04 | 导航、Gizmo、叠加和异步拾取 |
| Editor/ImGui | D04-T01/T02、D05-T04 | 私有输入适配、RHI renderer、纹理/帧绑定 |
| Editor/Diagnostics | D08-T08、D11-T05、D16-T06 | 只读统计、图/资源与 typed preview |
| Runtime/Main.cpp | D01-T04、D04、D05、D16-T07 | 唯一产品入口、帧编排、对象装配/逆序关闭 |
| Assets/Models | D00-T01、D06 | 标准 glTF/GLB 模型及来源 |
| Assets/Scenes | D06-T03、D16-T02 | JSON 编辑器场景、模型引用和场景设置 |
| Assets/Textures、Environments、Editor | D00-T01、D06-T01、D07-T04、D04-T02 | 场景纹理/HDR 环境、图标字体与语义 |
| Shaders/Common、Geometry | D03-T01、D06-T05 | 公共数值/ABI 消费、共享静态几何 |
| Shaders/Material、BRDF、Lighting | D07、D10–D13 | 统一物理模型、候选聚合与标准扩展 |
| Shaders/Shadow、ScreenSpace | D09、D14、可选 D18 | 阴影与屏幕方法，有限质量 profile |
| Shaders/PostProcess、UI、Passes | D04-T02、D07-T05、D15、贯穿各 GPU 阶段 | 显示/镜头、线性 UI、薄 Pass 入口 |
| ThirdParty/CMakeLists.txt、README.md | D00-T02、D19-T01 | 精简依赖构建、固定版本/来源/许可和必要修改记录；NVRHI 只供参考 |
| AGENTS.md、Docs/Architecture.md、Reference、Developments/Developments.md | D00-T04，相关 change 同步 | 通用准则、当前架构、参考分析与本计划 |
| openspec/config.yaml、specs、changes | D00-T04，贯穿所有实施 | 锁定原生流程、已采纳行为和可追踪历史 |
| 后续 RHI/Vulkan.hpp、RHI/Vulkan、Platform/VulkanDeviceHost.cpp | D19 | 只在后端接入时落地，沿既有接口和寿命模型 |
