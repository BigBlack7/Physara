# D16–D17 · 编辑工作流与 OpenGL 交付

[开发计划与当前阶段](Developments.md#current-stage) · [D16](#dev-d16) · [D17](#dev-d17)

<a id="dev-d16"></a>
## D16 · 完成编辑、持久化、读回与诊断闭环

**性质：** 基线。**入口：** [D15](D14-D15-ImagePipeline.md#dev-d15) 的阶段出口通过。

**阶段目标：** 在已接入的编辑器基础上补齐日常创作/调试流程，闭合资产变更、场景保存、GPU 结果消费和资源卸载。

<a id="d16-t01"></a>
### D16-T01 · 完成资产浏览、重导入与卸载

**职责位置：** Engine/Asset、Renderer caches、Editor/Panels/Session。**前置：** 本阶段入口满足。**架构依据：** [3.1](../Architecture.md#asset-types)、[3.4](../Architecture.md#asset-persistence)、[5.5](../Architecture.md#gpu-residency)。

- [ ] 完成资源浏览/过滤、加载状态、缩略图有界缓存及关联源诊断，面板不重复解析资产。
- [ ] 实例固定当前有效 CPU 版本，候选重导入先校验 SourceRef、节点/Primitive/材质槽映射，再准备 GPU 新代并显式激活；无法证明兼容时保留旧实例并报告，不能按旧索引自动跟随最新 AssetId。
- [ ] 检查同源多实例、快速修改/取消/卸载、已选资产删除和在途旧代；磁盘源已变化而实例版本冲突未解决时阻止普通保存，提供重新绑定/重新实例化，不建立资产历史仓库。

**完成条件：** 重新导入不是原地覆盖活资源，CPU/GPU readiness 与实际可用版本可追踪。

<a id="d16-t02"></a>
### D16-T02 · 完善 JSON 场景持久化与工作区恢复

**职责位置：** Engine/Scene、Engine/Asset、Editor/Session/Commands。**前置：** [D16-T01](#d16-t01)。**架构依据：** [3.4](../Architecture.md#asset-persistence)、[2.3](../Architecture.md#edits)、[12.3](../Architecture.md#project-docs)。

- [ ] 完善单个 `Assets/Scenes/*.scene.json` 的受支持字段，保存 Assets 引用、稳定实体/层级/TRS、物体和灯光参数、实例材质/纹理覆盖、环境及 mainView；mainView 保存导航 cameraPose、可选 cameraEntityId、physicalLens 与 requested 渲染设置，其余 View 为会话状态。
- [ ] 由 EditorSession 串行协调打开/保存/另存/关闭；打开冻结文档编辑但保留取消，候选校验后一次发布，保存使用只读快照并允许继续编辑，成功只标记对应版本，另存路径仅在对应操作成功后更新。Scene 不依赖 Renderer 或 JSON 类型，布局/选中项独立管理。
- [ ] 检查另存、未知版本、坏引用/丢失资产、路径、重导入冲突和保存重开；恢复每种已支持材质/纹理覆盖及主 View 设置后继续编辑保存，失败不替换有效场景、不破坏原文件，也不因迟到结果清除新修改。

**完成条件：** JSON 场景保存重开保持已承诺的实例、覆盖与设置，资产引用可定位；glTF/GLB 保持标准模型格式，编辑器状态不混入场景内容。

<a id="d16-t03"></a>
### D16-T03 · 完成 Gizmo、编辑叠加和选择显示

**职责位置：** Editor/Viewport、Commands、Shaders/UI/Passes。**前置：** [D16-T01](#d16-t01)。**架构依据：** [2.4](../Architecture.md#viewport-input)、[10.1](../Architecture.md#editor-output)、[2.3](../Architecture.md#edits)。

- [ ] 实现 world/local Gizmo、父逆变换与单次拖拽事务；对不可表达 shear 的操作明确拒绝/约束。
- [ ] 实现 grid、灯/相机图标、选择轮廓等 display-linear 叠加，深度读取和 stable/jitter 映射遵守 View 合同。
- [ ] 验证 undo/redo、负/非均匀缩放、DPI/render scale、相机捕获和隐藏面板，叠加不进入物理/时域历史。

**完成条件：** 叠加与操作定位一致，编辑可视化不修改 PBR 参数或占有原生图形状态。

<a id="d16-t04"></a>
### D16-T04 · 完成异步 GPU 拾取与失效防护

**职责位置：** Editor/Viewport、Renderer/Passes、RHI Queries/Readback。**前置：** [D16-T03](#d16-t03)。**架构依据：** [11.3](../Architecture.md#readbacks)、[2.4](../Architecture.md#viewport-input)、[8.2](../Architecture.md#geometry)。

- [ ] 随首次 GPU 拾取建立 R32_UINT ID 出口与 pick-index 映射，0 为背景、1..N 查捕获表，检查容量与索引范围；复用已有静态几何/alpha 覆盖，明确 NoHit、透明选择及图标优先策略。
- [ ] 实现 copy→close/submit→query→后续 poll→ready map，返回前验证 Scene/内容、View/generation/extent、相机版本及最新选择 requestId，不能在鼠标回调同步 ReadPixels。
- [ ] 测试背景/越界结果、实体槽复用、删除、场景切换、resize、DPI、相机变化及连续点击/层级树选择后的迟到请求、静态实例/扩展/透射和 CPU 图标命中优先级。

**完成条件：** 点击结果只能应用于原请求仍有效的目标，没有 GPU wait 或过期身份误选。

<a id="d16-t05"></a>
### D16-T05 · 完成截图、query 与非阻塞结果消费

**职责位置：** RHI Queries/Staging、Renderer、Core jobs、Editor/Panels。**前置：** [D16-T01](#d16-t01)。**架构依据：** [11.3](../Architecture.md#readbacks)、[conventions](../Architecture.md#conventions)、[11.1](../Architecture.md#retirement)。

- [ ] 完成 Event/Timer 可轮转对象与 row-pitch 正确的 staging 读取，未 ready 保持 Pending，控制在途容量。
- [ ] 接入 SceneHDR、ViewOutput 和含 UI 窗口目标截图，copy 位于对应生产者之后；CPU 编码作业独立拥有结果。
- [ ] EXR 去预曝光并标记 scene-linear，PNG 输出正确显示编码；测试非紧密 row pitch、取消、连续请求和退出。

**完成条件：** 结果消费不阻塞正常绘制，截图颜色/方向与各出口合同一致。

<a id="d16-t06"></a>
### D16-T06 · 完成模块化诊断与调试面板

**职责位置：** Editor/Diagnostics/Panels、Core/Renderer/RHI 统计 owner。**前置：** [D16-T04](#d16-t04)、[D16-T05](#d16-t05)。**架构依据：** [11.4](../Architecture.md#diagnostics)、[10.1](../Architecture.md#editor-output)、[12.2](../Architecture.md#benchmark)。

- [ ] 提供 CPU/GPU、FrameSlot 背压、PresentWait、Draw/bind/upload/barrier、缓存/图/资源及 Froxel/路由等只读快照。
- [ ] 完善材质/灯/相机/效果面板和 typed preview，显示 Pending/失败/有效配置；统计事实由真实 owner 发布。
- [ ] 为日志、曲线、缩略图、图摘要和查询 ring 设置容量/节流，测量面板隐藏/显示成本并避免遍历场景重算统计。

**完成条件：** 诊断能解释当前成本与退路，关闭面板不会继续无谓生成昂贵展示数据。

<a id="d16-t07"></a>
### D16-T07 · 验证编辑全流程与关闭完整性

**职责位置：** Editor、Engine/Asset、Renderer、Platform。**前置：** [D16-T01](#d16-t01)、[D16-T02](#d16-t02)、[D16-T03](#d16-t03)、[D16-T04](#d16-t04)、[D16-T05](#d16-t05)、[D16-T06](#d16-t06)。**架构依据：** [12.1](../Architecture.md#tests)、[11.5](../Architecture.md#shutdown)、[12.4](../Architecture.md#feature-closure)。

- [ ] 固定打开、导航、编辑材质/灯/静态物体、切路径/效果、拾取、截图、保存和重开的用户操作序列。
- [ ] 在加载/重载/查询在途时关闭，先停止接单再取消/join，再完成 GPU/Editor/Renderer/Device/context 的逆依赖释放。
- [ ] 测试未保存关闭取消、部分初始化、主图/fallback 与 fence 创建/查询失败；分别覆盖可用 context 上 Fatal 的 GPU 收拢和真实设备失效退出，检查引用/原生对象/线程均退出。

**完成条件：** 完整编辑器是渲染器的可用前端，失败与关闭路径与正常功能同样闭环。

**D16 阶段出口：** 可以完成打开→编辑→撤销→保存→重开；重导入与拾取/截图均异步且抗失效，面板隐藏成本和统计预算可测。

<a id="dev-d17"></a>
## D17 · 完成 OpenGL 基线验收与可复用性能基准

**性质：** 基线。**入口：** [D16](#dev-d16) 的阶段出口通过。

**阶段目标：** 对已逐步验证的系统执行整体交付验收，确认支持矩阵、可重建运行包、质量与资源预算。

<a id="d17-t01"></a>
### D17-T01 · 审计全模块合同与 native conformance

**职责位置：** Engine、Editor、RHI、Platform。**前置：** 本阶段入口满足。**架构依据：** [12.1](../Architecture.md#tests)、[7.7](../Architecture.md#capabilities)、[12.4](../Architecture.md#feature-closure)。

- [ ] 按本文覆盖表逐项核对所有基线行为、API 合同、模块依赖和关闭协议；补齐真正缺失的场景而非重复已足够的测试。
- [ ] 在声明支持的设备/驱动/操作系统配置运行 native suite，明确未运行项及最低 caps；不扩大到未经覆盖的环境。
- [ ] 核对所有 profile、格式、绑定 limits、三路径和 None/FXAA/TAA，未支持项保持明确能力结果。

**完成条件：** 交付能力与真实证据一致，未解决关键正确性/寿命问题不被性能工作掩盖。

<a id="d17-t02"></a>
### D17-T02 · 完成同一 Runtime 的性能采样流程

**职责位置：** Runtime、Core/Renderer stats、Assets。**前置：** [D17-T01](#d17-t01)。**架构依据：** [12.2](../Architecture.md#benchmark)、[11.4](../Architecture.md#diagnostics)。

- [ ] 在同一 Runtime 入口中接入确定性预热/采样流程，复用正式 Scene/Renderer/Graph/DeviceHost 和已有统计，按需输出测量结果。
- [ ] 固定场景 hash、相机、版本、设备/驱动、分辨率、路径、AA、效果、扩展比例、灯、VSync 和采样数。
- [ ] 输出 CPU/GPU 分开 median/p95、帧槽/PresentWait、分配/上传/绑定/显存等；必要时隔离 UI 成本并同时测正常编辑模式。

**完成条件：** 同口径数据可重复生成，结果不会把跳过扩展、丢灯或隐藏质量差异当性能提升。

<a id="d17-t03"></a>
### D17-T03 · 建立画质/成本基准和有依据优化

**职责位置：** Renderer/RHI/Editor 对应 owner、按需 Docs/Performance。**前置：** [D17-T02](#d17-t02)。**架构依据：** [12.2](../Architecture.md#benchmark)、[11.4](../Architecture.md#diagnostics)、[12.3](../Architecture.md#project-docs)。

- [ ] 选择小/多实例/多灯/复杂静态几何/扩展/透明/高分辨率等代表负载，对照已有画质门槛明确瓶颈。
- [ ] 仅为已测瓶颈提出后续聚焦 change，保持输入/质量一致并报告改善和代价，性能目标按目标硬件确定。
- [ ] 需要长期复用比较时再建立 Docs/Performance 报告；原始临时产物保留在 build/artifacts，不给每任务强制新增报告。

**完成条件：** 形成可持续测量基线，优化没有破坏物理语义、三路径等价或可理解的所有权。

<a id="d17-t04"></a>
### D17-T04 · 完成连续运行与资源压力验收

**职责位置：** Renderer、Editor、Platform、所有资源 owner。**前置：** [D17-T01](#d17-t01)、[D17-T02](#d17-t02)。**架构依据：** [11.1](../Architecture.md#retirement)、[11.2](../Architecture.md#history-commit)、[11.5](../Architecture.md#shutdown)、[11.4](../Architecture.md#diagnostics)。

- [ ] 循环导入/卸载、场景切换、三路径/AA/效果切换、热重载、DPI/resize、最小化恢复和多 View。
- [ ] 覆盖图/Shader/资产失败、query/上传环回和预算耗尽，检查内存/对象/缓存高水位稳定及 CPU/GPU 引用退休。
- [ ] 分别测试在途关闭和初始化每个失败点，确认回调恢复、作业 join 和 context 前原生删除。

**完成条件：** 长时间操作和失败注入没有无界增长、死锁、过期历史、非法调用或资源泄漏。

<a id="d17-t05"></a>
### D17-T05 · 完成可重建运行包与用户入口

**职责位置：** CMake targets、Runtime、Assets/Shaders、Docs。**前置：** [D17-T01](#d17-t01)、[D17-T04](#d17-t04)。**架构依据：** [1.1](../Architecture.md#build)、[1.4](../Architecture.md#runtime)、[12.3](../Architecture.md#project-docs)。

- [ ] 实现当前支持环境的部署/安装布局，随包提供正确 Shader 包、启动资源、依赖运行库与许可信息。
- [ ] 从干净目录验证 Debug/Release 构建和无源码工作区的启动，正常入口始终启动带编辑器的渲染器。
- [ ] 核对源码/缓存/资源路径、错误提示与已支持配置，补必要运行说明，归档已完成 OpenSpec changes。

**完成条件：** 用户无需作者机器路径或旧 build 缓存即可启动已声明产品；基线可提交最终验收。

**D17 阶段出口：** 全部基线工作包与声明能力验证通过；可重建、可启动、可编辑、可保存，三路径/AA/目标 glTF profile 与真实支持范围一致。
