# D06–D07 · 静态场景、资产与物理 Forward

[开发计划与当前阶段](Developments.md#current-stage) · [D06](#dev-d06) · [D07](#dev-d07)

<a id="dev-d06"></a>
## D06 · 接入标准资产、场景与首个真实视口

**性质：** 基线。**入口：** [D05](D03-D05-FramePipeline.md#dev-d05) 的阶段出口通过。

**阶段目标：** 把 glTF CPU 数据、Scene、只读准备、GPU 驻留与 Forward 几何连到现有编辑器帧图。

<a id="d06-t01"></a>
### D06-T01 · 建立不可变资产、请求与版本发布

**职责位置：** Engine/Asset、Core Jobs/IO、Assets。**前置：** 本阶段入口满足。**架构依据：** [1.2](../Architecture.md#core)、[3.1](../Architecture.md#asset-types)、[3.3](../Architecture.md#asset-images)。

- [ ] 实现 AssetId/ContentRevision、Unloaded/Loading/CpuReady/Failed/Cancelled、源身份和不可变 CPU 资产记录。
- [ ] 随异步资产导入引入有界 Core 作业队列、独立拥有结果、取消和帧边界完成队列；实现所需受控路径、文件读取与内容 hash，验证请求/源/目标版本后发布，CPU Ready 与 GPU Ready 分离。
- [ ] 检查重复请求、迟到结果、停止接单、取消和 join；worker 不访问 UI、Scene 可变状态或原生设备。
- [ ] 建立图片、HDR/EXR 和 Editor 图像的 color/data/alpha 元数据，按语义区分同像素的不同解释。

**完成条件：** 迟到/取消结果安全丢弃，CPU 记录只含标准数据和引用，不带 RHI/UI 对象。

<a id="d06-t02"></a>
### D06-T02 · 实现标准 glTF 模型解码与校验

**职责位置：** Engine/Asset。**前置：** [D06-T01](#d06-t01)。**架构依据：** [3.2](../Architecture.md#asset-import)、[3.3](../Architecture.md#asset-images)、[3.4](../Architecture.md#asset-persistence)。

- [ ] 支持 glTF/GLB、URI/buffer/accessor 的 offset/stride/normalized/sparse、索引与合法拓扑处理，校验尺寸溢出和预算。
- [ ] 解码静态层级、核心材质/纹理、相机和灯；明确三角形拓扑转换、缺 NORMAL 的逐面法线及忽略源 TANGENT、MikkTSpace 面角点输出的拆顶点/重建索引、UV0/UV1、负缩放和扩展报告，缺必需 UV 不伪造输入。
- [ ] 在 Asset 导入入口完成 CPU 校验；未知 required 拒绝，实际使用 Skin/Morph 的实例明确拒绝，静态节点按初始变换导入，动画曲线不播放并报告；其余可选未支持项报告可见降级。

**完成条件：** 正常与异常输入可无 GPU/UI 地校验，glTF/GLB 模型只读且只提供静态资产，不承担场景 JSON 或模型写出。

<a id="d06-t03"></a>
### D06-T03 · 实现 Scene 实例、变换与 JSON 场景读写

**职责位置：** Engine/Scene、Editor/Session。**前置：** [D06-T02](#d06-t02)。**架构依据：** [4.1](../Architecture.md#scene)、[3.4](../Architecture.md#asset-persistence)。

- [ ] 落地 entt、EntityHandle generation、稳定实体身份、父子关系和 SourceRef；从 glTF 资产节点实例化独立 Scene 实例。
- [ ] 验证无环层级、TRS 可表达性和非均匀/负缩放，按脏根传播 world transform/bounds，无脏根不推进版本；区分保存用 DocumentRevision、非相机静态内容的 RenderContentRevision 和 CameraRevision，撤销/重做沿同一入口更新。
- [ ] 实现新增/删除/重设父级及实例参数覆盖的领域入口；材质字段和纹理资源/UV/sampler 覆盖归 Scene，未覆盖值引用只读模型，删除后旧请求/选中身份失效，Scene 不创建 GPU 资源。
- [ ] 由 EditorSession 串行组合单个 `Assets/Scenes/*.scene.json` 的读写；Scene 提供实体/实例覆盖快照和候选校验，主 View 提供相机及 requested 配置；资源引用统一相对资源根，不复制模型或纹理正文。
- [ ] 打开期间冻结文档编辑并保留 UI/取消；校验通过后整体替换 Scene 和主 View，失败保留原状态。保存使用只读快照并允许后续编辑，成功仅标记对应版本，另存路径只在对应操作成功后更新。

**完成条件：** 父子更新和实体复用可测，同一资产可多实例使用且不共享可变场景状态；JSON 保存重开能恢复当前承诺的实例与设置，模型正文仍由 glTF/GLB 提供。

<a id="d06-t04"></a>
### D06-T04 · 提取 SceneFrame 并准备相机与可见集

**职责位置：** Renderer/Prepare、View、Engine/Scene Camera。**前置：** [D06-T03](#d06-t03)。**架构依据：** [5.2](../Architecture.md#prepare)、[5.3](../Architecture.md#view-preparation)、[4.3](../Architecture.md#camera)、[5.1](../Architecture.md#frame-state)。

- [ ] 从 Scene 提取一次连续只读对象/Primitive/灯描述和索引，多 View 共享 SceneFrame，静态资产由 handle/revision 引用。
- [ ] 实现 glTF perspective/orthographic 与编辑器相机，校验已声明投影 profile 的 FOV/magnification/near/far；区分有限稳定裁剪投影与渲染投影，固定 aspect 使用 letterbox 与有效图像矩形，正交视线方向不套透视公式；相机移动与静态内容版本分别更新，带灯/可渲染组件或影响渲染子树的相机实体变换同时更新内容版本。
- [ ] 实现可解释的 CPU bounds/culling 与无效/零尺寸 View 处理，稳定视锥为最大 jitter/滤波范围留 guard band；相机坐标与裁剪结果使用固定非对称场景验证。

**完成条件：** 构图和 execute 不遍历/修改 ECS，多视口不会复制整份材质与场景数据。

<a id="d06-t05"></a>
### D06-T05 · 建立几何/纹理驻留和首个 Forward 子图

**职责位置：** Renderer/Materials、Draw、Paths、Passes、Shaders/Geometry/Material。**前置：** [D06-T04](#d06-t04)。**架构依据：** [5.5](../Architecture.md#gpu-residency)、[5.6](../Architecture.md#draws)、[6.1](../Architecture.md#paths)、[8.2](../Architecture.md#geometry)、[8.5](../Architecture.md#materials)。

- [ ] 实现按 asset+revision 的初始 Geometry/TextureCache、材质 GPU 行、绑定和拥有型 UploadBatch，上传在图内执行。
- [ ] 实现共享静态 VertexFetch/SurfaceFrame、world 变换与 normal/tangent/handedness，统一当前 depth/color 的 alpha 覆盖；接入标准 UV/normal/ORM、doubleSided、texture_transform、emissive_strength 和 unlit 语义，后续 shadow/ID 消费时复用。
- [ ] 建立 DrawRecord 与 Forward 的 PrepareSurface/ShadeOpaque 合同；先以标准 unlit 和显式材质/法线诊断入口验证真实几何，lit PBR 在 D07 闭环。

**完成条件：** glTF 可以正确出现在真实视口；无灯光模型的诊断输出不标成已完成 PBR，不在 execute 创建或查找资产。

<a id="d06-t06"></a>
### D06-T06 · 接入最小可用场景编辑与相机导航

**职责位置：** Editor/Panels、Commands、Viewport、Session。**前置：** [D06-T03](#d06-t03)、[D06-T04](#d06-t04)、[D06-T05](#d06-t05)。**架构依据：** [2.3](../Architecture.md#edits)、[2.4](../Architecture.md#viewport-input)、[10.1](../Architecture.md#editor-output)。

- [ ] 接入层级树、选择、Transform Inspector、导入状态和 View 面板，以及 JSON 场景的新建/打开/保存；属性修改通过既有事务与 undo/redo。
- [ ] 实现 fly/orbit/pan、窗口到视口的单次坐标映射和帧边界 extent 请求；主 View 的导航 cameraPose、可选 cameraEntityId 和渲染请求可保存，额外 View 保持会话状态。
- [ ] 验证删除选中实体、隐藏/恢复视口、父级缩放下变换及撤销；不为界面建立另一套 Scene。

**完成条件：** 真实场景可用最小编辑器操作，属性变化沿 revision→准备→上传→图输出生效。

<a id="d06-t07"></a>
### D06-T07 · 建立静态资产到窗口的集成门槛

**职责位置：** Engine/Asset、Engine/Scene、Renderer、Editor。**前置：** [D06-T02](#d06-t02)、[D06-T05](#d06-t05)、[D06-T06](#d06-t06)。**架构依据：** [12.1](../Architecture.md#tests)、[12.4](../Architecture.md#feature-closure)、[11.5](../Architecture.md#shutdown)。

- [ ] 覆盖多 Primitive/材质、UV0/UV1、缺切线、稀疏 accessor、负缩放、遮挡、正交/透视和双实例场景。
- [ ] 在导入中取消/退出、重复加载、资产失败和快速删除/resize，核对 CPU 发布、GPU 引用与旧 View 输出寿命。
- [ ] 把标准输入解释、几何诊断图及编辑操作加入可重现检查，保留明确的当前支持 profile。

**完成条件：** 第一次完整输入→场景→准备→图→RHI→UI→呈现的纵向链通过，后续增加物理模型无需重构入口。

**D06 阶段出口：** 可导入、查看、变换和删除 glTF 模型实例，并用 JSON 保存、重开编辑器场景；几何/纹理/相机语义正确，Scene 不持 GPU 对象，导入失败/取消不会发布半份资产。

<a id="dev-d07"></a>
## D07 · 形成可用的物理 Forward 渲染器

**性质：** 基线。**入口：** [D06](#dev-d06) 的阶段出口通过。

**阶段目标：** 完成核心 metallic-roughness、物理灯光/相机、IBL 和正确显示，使后续路径有统一的参考输出。

<a id="d07-t01"></a>
### D07-T01 · 完成共享核心表面与 BRDF

**职责位置：** Shaders/Material、BRDF、Common、Renderer/Materials。**前置：** 本阶段入口满足。**架构依据：** [8.5](../Architecture.md#materials)、[8.1](../Architecture.md#shader-runtime)、[4.4](../Architecture.md#physical-inputs)。

- [ ] 形成 MaterialInputs→SurfaceGeometry→PixelParams，按 Filament 路线实现 GGX、height-correlated Smith、Schlick、漫反射及配套能量项。
- [ ] 实现法线/切线与双面语义、粗糙度过滤和边界处理；核心模型只存在一份，depth/color 复用 alpha 与几何合同。
- [ ] 建立白炉、粗糙度/金属度极限、方向反照率和 CPU 数值参考；标明模型近似，不凭视觉调整任意补偿常数。

**完成条件：** 核心材质在共享库中可验证，连续参数不生成指数级 Shader 变体。

<a id="d07-t02"></a>
### D07-T02 · 实现物理相机与预曝光输入

**职责位置：** Engine/Scene Camera、View、Renderer/Prepare、Editor/Panels。**前置：** [D07-T01](#d07-t01)。**架构依据：** [4.3](../Architecture.md#camera)、[4.4](../Architecture.md#physical-inputs)、[9.1](../Architecture.md#temporal-contract)。

- [ ] 实现 ISO、快门、光圈、EV compensation、传感器、焦距/视场及对焦距离的单位和计算，保留 glTF yfov 权威语义。
- [ ] 按既定标定生成 preExposure，写入统一 View ABI，并把当前值和最后成功渲染值分开保存。
- [ ] 接入物理参数命令与界面，测试 EV 档位、长度换算和零/非法输入；快门不被宣称为已有运动模糊。

**完成条件：** 曝光倍率可计算与复现，同一参数在灯光、自发光、背景和后处理使用相同曝光域。

<a id="d07-t03"></a>
### D07-T03 · 实现统一物理灯列表与 Forward 直接光

**职责位置：** Renderer/Lighting、Prepare、Shaders/Lighting。**前置：** [D07-T01](#d07-t01)、[D07-T02](#d07-t02)。**架构依据：** [5.4](../Architecture.md#light-preparation)、[4.4](../Architecture.md#physical-inputs)、[8.6](../Architecture.md#lighting)、[8.7](../Architecture.md#opaque-shading)。

- [ ] 接入 KHR_lights_punctual 的方向/点/聚光灯、颜色、lux/candela、衰减、spot 角和 range；未指定 range 保持无穷语义。
- [ ] 准备完整可见灯集合与统一 EvaluateLighting，Forward 先遍历完整候选列表，阴影输入在 D09 接入同一合同。
- [ ] 验证 inverse-square、聚光边界、光源单位、曝光相应关系、多灯叠加与零灯场景。

**完成条件：** 直接光结果有数值依据，后续 Forward+/Deferred 只更换候选选择和表面获取。

<a id="d07-t04"></a>
### D07-T04 · 完成环境准备、DFG 与 IBL

**职责位置：** Engine/Asset Environment、Renderer/Lighting、Shaders/BRDF/Lighting。**前置：** [D07-T01](#d07-t01)、[D07-T02](#d07-t02)、[D07-T03](#d07-t03)。**架构依据：** [3.3](../Architecture.md#asset-images)、[5.4](../Architecture.md#light-preparation)、[8.6](../Architecture.md#lighting)。

- [ ] 实现 HDR 环境到 cubemap、GGX prefilter、SH9 与固定 Filament DFG 编码；按实施 spec 选择可验证的冷路径/图内绘制预计算。
- [ ] 建立含源 hash、处理设置、算法/工具版本的 EnvironmentCache，旋转在查询空间处理，标明相对/绝对亮度校准。
- [ ] 整合漫反射/镜面 IBL、AO 输入默认值和天空背景；环境 Pending 保留旧有效代，不能黑图假装就绪。

**完成条件：** 核心材质直接光与 IBL 使用同一模型，环境旋转/曝光不重复烘焙或重复乘强度。

<a id="d07-t05"></a>
### D07-T05 · 接入固定显示 profile 与基础 FXAA

**职责位置：** Renderer/PostProcess、Shaders/PostProcess、Editor/View。**前置：** [D07-T02](#d07-t02)、[D07-T04](#d07-t04)。**架构依据：** [9.4](../Architecture.md#display-output)、[conventions](../Architecture.md#conventions)。

- [ ] 接入 Architecture 固定 Filament ACESToneMapper/ColorGrading profile 与成对原色转换，形成 display-linear ViewOutput。
- [ ] 打通 None/FXAA、输出尺寸和基础空间缩放的图组装，FXAA 按感知亮度合同取样，不重复曝光/tonemap。
- [ ] 验证 SRGB 颜色纹理、线性数据纹理、自发光、高亮、UI 色块和窗口编码，保留 HDR 比较出口。

**完成条件：** 首个 PBR 纵向切片已有正式显示链，后续镜头/时域阶段只扩充相应 Pass。

<a id="d07-t06"></a>
### D07-T06 · 开放核心物理编辑并建立参考场景

**职责位置：** Editor/Panels、Commands、Assets/Scenes。**前置：** [D07-T03](#d07-t03)、[D07-T04](#d07-t04)、[D07-T05](#d07-t05)。**架构依据：** [2.3](../Architecture.md#edits)、[4.4](../Architecture.md#physical-inputs)、[12.1](../Architecture.md#tests)、[12.2](../Architecture.md#benchmark)。

- [ ] 接入实例材质因子/纹理覆盖、物理灯、环境和曝光 Inspector，显示导入支持范围与 Pending/失败原因；修改 Scene 覆盖与主 View 请求，不改写共享 glTF/GLB。
- [ ] 建立材质球、标准尺度、已知光照、HDR 环境和非对称方向测试，固定输入 hash、曝光和线性/HDR 误差口径。
- [ ] 采集 CPU/RHI 基本计数与资源数量，接好 GPU timer 指标接口，原生 query 就绪后再填时延；真实比较结论才保存长期性能报告。

**完成条件：** 可加载、编辑并正确显示标准 PBR 场景，后续阶段有统一视觉/数值和成本基线。

**D07 阶段出口：** 标准静态场景在固定曝光下完成直接光+IBL 的核心 PBR；有 HDR/数值参考，视口与窗口颜色一致，None/FXAA 已可用。
