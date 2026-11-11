# D03–D05 · Shader、编辑器呈现与整帧 RenderGraph

[开发计划与当前阶段](Developments.md#current-stage) · [D03](#dev-d03) · [D04](#dev-d04) · [D05](#dev-d05)

<a id="dev-d03"></a>
## D03 · 打通 Shader ABI 与第一次原生绘制

**性质：** 基线。**入口：** [D02](D00-D02-Foundation.md#dev-d02) 的阶段出口通过。

**阶段目标：** 从统一 Shader 输入到 RHI Pipeline、Draw、提交票据和像素读回形成首条 GPU 纵向链。

<a id="d03-t01"></a>
### D03-T01 · 实现权威 Shader 接口与编译步骤

**职责位置：** Engine/Shader、Engine/Shader/Interface、Shaders/Common、相关 CMakeLists。**前置：** 本阶段入口满足。**架构依据：** [1.3](../Architecture.md#shader-build)、[8.1](../Architecture.md#shader-runtime)。

- [x] 以单一 schema 生成 C++/GLSL 布局，校验尺寸、offset、矩阵、数组和固定宽度字段；生成物进入 build。
- [x] 在 Shader 模块及其 CMake 中接入所需编译步骤，处理 include、入口、选项、工具版本与依赖 hash，输出 canonical Vulkan SPIR-V、反射和可定位错误。
- [x] 在 build 下用真实常量/纹理程序验证增量构建、干净构建和 ABI 不匹配失败；开发测试材料不进入正式源码或默认构建。

**完成条件：** 生成/编译步骤不依赖消费其结果的 Renderer，业务不手写另一份 CPU/GPU 布局。

**当前状态** 已完成并归档；实现、独立审查与最终验证记录见 [establish-shader-interface-build](../../openspec/changes/archive/2026-11-05-establish-shader-interface-build/tasks.md)。

<a id="d03-t02"></a>
### D03-T02 · 实现 GL Shader、绑定与 Pipeline lowering

**职责位置：** RHI/OpenGL Shader/Bindings/Pipeline、Engine/Shader。**前置：** [D03-T01](#d03-t01)。**架构依据：** [7.3](../Architecture.md#opengl)、[7.2](../Architecture.md#rhi-objects)、[1.3](../Architecture.md#shader-build)。

- [x] 通过 SPIRV-Cross 把 D03-T01 当前 canonical graphics profile lowering 为 GLSL 460，映射 builtin、clip 与资源布局，完成 native program link。
- [x] 按活动资源反射建立 PipelineBindingPlan，合并 Texture/Sampler 采样对并紧凑分配当前 profile 的 UBO、只读 SSBO 和 texture 单元，校验真实 limits；image/UAV 随 D08-T04 扩充，当前明确拒绝且不预占单元。
- [x] 实现 BindingLayout/BindingSet、GraphicsPipeline、基础 framebuffer 兼容检查；以共享纹理不同 sampler 等用例校验 lowering。

**完成条件：** 逻辑 set/binding 与原生单元映射可诊断，合法接口不因全项目预留大量固定 binding 而误判设备不足。

**当前状态** 已完成并归档；范围、证据与后续任务见 [establish-opengl-shader-pipelines](../../openspec/changes/archive/2026-11-06-establish-opengl-shader-pipelines/tasks.md)。本包沿 D03-T01 的当前 graphics profile 交付准备能力，volatile/push 消费仍归 D08-T02，Draw、在途绘制与像素门槛分别归 D03-T03/T04/T05。T05 审查发现的零长度绑定范围存量缺陷已由 [fix-opengl-binding-buffer-ranges](../../openspec/changes/archive/2026-11-08-fix-opengl-binding-buffer-ranges/tasks.md#final-verification) 修正并独立验收归档。

<a id="d03-t03"></a>
### D03-T03 · 实现单即时列表与基础 Draw 状态

**职责位置：** RHI/OpenGL CommandList/Graphics/ContextState。**前置：** [D03-T02](#d03-t02)。**架构依据：** [7.6](../Architecture.md#command-execution)、[7.5](../Architecture.md#barriers)。

- [x] 实现 open/close、clear、vertex/index 绑定、动态 viewport/scissor、raster/depth/blend、draw/indexed/instanced 和必要 copy。
- [x] 区分 ContextState、访问 tracker 与数据 revision；为实际消费发出最小必要同步，clear 的用途先于后续 draw 用途建立。
- [x] 用非对称图验证 CCW/负缩放、Reverse-Z、scissor、sRGB、GL_InstanceID+baseInstance 和已含 baseVertex 的顶点索引。

**完成条件：** 原生 Draw 在即时列表调用时发出，Execute 不重放；基本状态及索引语义通过像素/已知数据检查。

**当前状态** 已完成并归档；独立 B 代码/注释验收、最终 verify、规范同步与归档记录见 [establish-opengl-immediate-draw](../../openspec/changes/archive/2026-11-07-establish-opengl-immediate-draw/tasks.md#final-verification)。完整在途验收见 [D03-T04](#d03-t04)，稳定图像门槛见 [D03-T05](#d03-t05)。

**与 D02-T04 的交付衔接：** 上传限定列表的 open/close、owner/唯一 claim 和一次提交基础已在 [上传 change](../../openspec/changes/archive/2026-10-28-establish-opengl-resource-uploads/design.md) 建立. 本包已在同一列表上完成基础图形执行验收, 未复制第二套列表; 完整在途与稳定图像门槛继续归 D03-T04/T05.

**坐标验收前置：** D02-T05 的 UPPER_LEFT 区域 depth clear 历史观测见 [资源一致性 design](../../openspec/changes/archive/2026-10-29-establish-opengl-resource-conformance/design.md) 决策 12. 本包已满足定位与产品 UPPER_LEFT clear/Draw 验收前置, 输入、路径决定及结果留存在归档 change; 独立 B 复核与最终 verify 均通过.

<a id="d03-t04"></a>
### D03-T04 · 实现提交票据、在途引用与完成回收

**职责位置：** RHI/OpenGL 提交/queries/删除。**前置：** [D03-T03](#d03-t03)。**架构依据：** [11.1](../Architecture.md#retirement)、[7.4](../Architecture.md#uploads)、[11.3](../Architecture.md#readbacks)。

- [x] Execute 为已发出的列表建立 submission/fence/flush 与 in-flight 引用批次，区分 recording、submitted、completed；fence 创建失败不返回有效 ticket，未获票据的引用保留到受控退出。
- [x] 用实际完成值控制上传数据、查询和资源删除；初始同步读回仅可用于明确的测试/冷路径，正常帧不等待全设备。
- [x] 检查关闭后提交、重复执行拒绝、提交后立即释放 CPU 句柄、在途退出和未提交即时工作退出；区分 poll 的 Complete/Pending/Failed，只有 Complete 推进完成值。Fatal 停止新渲染而不等于 device lost，可用 context 的无 fence 工作在退出收拢，失效 context 不等待失败 ID。

**完成条件：** 像素读回和资源退休由真实完成驱动，CPU 释放不导致 GPU 或上传区段提前复用。

**与 D02-T04 的交付衔接：** 上传 change 已实现其必要的真实 fence/flush、完成收集、内部引用/预算和无 fence 退出基础. 本包在同一 Device/packet/删除协议上覆盖真实 Draw、后续上传版本、查询及其它在途消费者, 完整验收既有票据/失败合同; 不因有限子集交付而提前勾选本包或 D03 出口.

**当前状态：** 已完成并归档；A/B 审查、最终 verify、主规范同步与交付边界见 [establish-opengl-submission-retirement](../../openspec/changes/archive/2026-11-08-establish-opengl-submission-retirement/tasks.md#final-verification)。稳定图像门槛与 D03 出口见下方 D03-T05，产品呈现仍归 D04。

<a id="d03-t05"></a>
### D03-T05 · 形成首个稳定原生绘制门槛

**职责位置：** RHI、Renderer、Shaders/Passes。**前置：** [D03-T03](#d03-t03)、[D03-T04](#d03-t04)。**架构依据：** [12.1](../Architecture.md#tests)、[conventions](../Architecture.md#conventions)、[8.1](../Architecture.md#shader-runtime)。

- [x] 把四角色块/标记、深度梯度、非零实例及索引偏移组织为固定测试输入，验证真实后端输出。
- [x] 覆盖 clear→draw→copy/readback、不同 Framebuffer、纹理采样和实例范围；增加 Validation 失败用例。
- [x] 清理只为验证产生的重复生产代码，测试使用正式 Shader/RHI 链；不新增产品 Viewer 或直接 GL 的 Renderer。

**完成条件：** 第一次图像成功可重现且可自动判定，后续阶段能够复用同一编译、绑定、提交和完成链。

**当前状态：** 已完成并归档；[establish-opengl-native-draw-gate](../../openspec/changes/archive/2026-11-08-establish-opengl-native-draw-gate/tasks.md#final-verification) 与关联存量修正 [fix-opengl-binding-buffer-ranges](../../openspec/changes/archive/2026-11-08-fix-opengl-binding-buffer-ranges/tasks.md#final-verification) 均已通过 B 代码/注释复核、A 最终 verify 和主规范同步，验收结论及边界分别留存在归档 tasks。

**D03 阶段出口：** 已知图案经 canonical Shader→GL lowering→公共 RHI Draw→真实完成→读回通过；非零 base instance/base vertex、方向、深度、颜色验证有效。

**出口状态** 已通过。最终输入、实际运行/复用结果和边界见上方两份 change 的最终验收留存。后续工作包状态由[当前阶段](Developments.md#current-stage)记录。

<a id="dev-d04"></a>
## D04 · 建立带编辑器的可呈现程序

**性质：** 基线。**入口：** [D03](#dev-d03) 的阶段出口通过。

**阶段目标：** 让唯一 Runtime 真正启动 Editor，打通 UI 输入、RHI 绘制、窗口末端复制和有界帧槽。

<a id="d04-t01"></a>
### D04-T01 · 建立 EditorSession、面板和输入路由

**职责位置：** Editor/Session、Panels、ImGui、Runtime。**前置：** 本阶段入口满足。**架构依据：** [2.2](../Architecture.md#editor-input)、[1.4](../Architecture.md#runtime)。

- [x] 接入固定版本 Dear ImGui docking 与官方 GLFW 输入适配，私有适配文件负责回调链和 GlfwAccess。
- [x] 建立 Session、面板注册/布局、选择身份及编辑意图入口；此时无领域数据的控件保持未启用。
- [x] 统一文本/模态、Gizmo、相机的输入优先级和 focus/capture 清理，禁止各面板直接接管 GLFW 回调。

**完成条件：** 面板可以组合/隐藏，Engine 不反向依赖 Editor，Editor 通过领域窄入口访问数据，不承担宿主 context 所有权。

**当前状态** 已完成验收并归档。 [establish-editor-session-input](../../openspec/changes/archive/2026-11-10-establish-editor-session-input/tasks.md#final-verification)

<a id="d04-t02"></a>
### D04-T02 · 实现 ImGui 公共 RHI 绘制与纹理协议

**职责位置：** Editor/ImGui、Shaders/UI。**前置：** [D04-T01](#d04-t01)。**架构依据：** [10.2](../Architecture.md#imgui-renderer)、[10.1](../Architecture.md#editor-output)。

- [x] 实现 UI VB/IB 上传、clip/scissor、DPI、Idx/VtxOffset、reset-state callback 和 draw commands，全部走公共 RHI；callback 命令完成后跳过普通纹理解析与绘制。
- [x] 实现当前 ImGui 纹理 create/update/destroy 协议、字体与稳定 UiTextureId/generation，绑定引用受提交完成保护；像素/更新矩形有独立所有权，实际执行并有效提交才确认更新，失败保留请求重试。
- [x] 按线性域 straight-alpha 合成，处理 sRGB 创作配色、图标纹理和字体覆盖率；验证图像与文本边缘。

**完成条件：** UI 不使用 ImGui 原生 OpenGL renderer；纹理销毁或重建不会留下悬空 draw-data 引用。

**当前状态** 已完成验收并归档。 [establish-imgui-rhi-renderer](../../openspec/changes/archive/2026-11-10-establish-imgui-rhi-renderer/tasks.md#final-verification)

<a id="d04-t03"></a>
### D04-T03 · 接入 PresentColor 和正确窗口提交

**职责位置：** Platform/OpenGLDeviceHost、RHI/OpenGL 互操作、Runtime。**前置：** [D04-T02](#d04-t02)。**架构依据：** [10.3](../Architecture.md#present)、[11.1](../Architecture.md#retirement)、[conventions](../Architecture.md#conventions)。

- [x] 按帧槽创建非零尺寸 PresentColor，Runtime 只把 RHI 目标送入绘制；默认 framebuffer 不伪装为普通纹理。
- [x] 列表关闭后进行 host 末端原生复制，维护同一 ContextState 并显式保活源，然后 Execute 插 fence、flush，最后 SwapBuffers。
- [x] 核对窗口 sRGB 能力与行序，验证颜色只编码一次；失败目标不复制/交换，末端复制本身失败也进入取消；单独统计 PresentWait 与 GPU 工作完成。

**完成条件：** 最终窗口源访问在本帧 tracking fence 之前，呈现互操作不会污染下一帧 RHI 状态缓存。

**当前状态** 已完成验收并归档。 [establish-opengl-window-presentation](../../openspec/changes/archive/2026-11-11-establish-opengl-window-presentation/tasks.md#final-verification)

<a id="d04-t04"></a>
### D04-T04 · 接入有界 FrameSlot 与最小视口资源身份

**职责位置：** Runtime、Engine/Renderer 帧状态、Editor/Viewport。**前置：** [D04-T03](#d04-t03)。**架构依据：** [5.1](../Architecture.md#frame-state)、[10.1](../Architecture.md#editor-output)、[11.2](../Architecture.md#history-commit)。

- [ ] 随真实帧数据引入 FrameArena，检查容量、对齐、算术溢出与失效借用；建立有界 FrameSlot、提交引用和票据回收，复用依据完成值，等待/查询失败不能 reset 该槽。
- [ ] 建立稳定 ViewId 与 UI 纹理注册；基础 UI 程序/字体失败明确终止启动，无 Scene 或场景准备中仍可清屏呈现 UI 和可诊断占位，NoSceneOutputs 不推进 View 历史。
- [ ] 覆盖窗口/视口连续 resize、最小化恢复、最后一帧在途退出和字体/目标重建；请求不在 GPU execute 中修改。

**完成条件：** 入口始终为带编辑器的程序，测试图与真实场景能力状态可区分，帧槽不会凭 CPU 帧号复用。

**当前状态** 前置已满足，待立项。

<a id="d04-t05"></a>
### D04-T05 · 建立领域编辑命令与渲染配置请求边界

**职责位置：** Editor/Commands、Session、Viewport。**前置：** [D04-T01](#d04-t01)、[D04-T04](#d04-t04)。**架构依据：** [2.3](../Architecture.md#edits)、[2.4](../Architecture.md#viewport-input)、[5.7](../Architecture.md#plan-readiness)。

- [ ] 建立命令/事务、拖拽合并和 undo/redo 的窄协议，先覆盖真实存在的 View/编辑器配置；后续 Scene 命令扩充同一入口。
- [ ] 分离 View 设置、编辑器布局和平台 VSync/标题请求，保留 requested/effective、Pending/失败原因。
- [ ] 测试事务边界、合并、撤销重做及 Session 销毁；面板不保存另一份可分叉的权威渲染设置。

**完成条件：** 当前 View/编辑器设置的编辑可应用及撤销，后续领域操作沿同一窄入口加入；不提前建立通用命令框架。

**D04 阶段出口：** 真实窗口稳定显示 Docking 编辑器及测试视口；字体更新、resize/DPI、颜色、提交前末端复制、在途关闭均可验证。

<a id="dev-d05"></a>
## D05 · 让整帧经 RenderGraph 执行

**性质：** 基线。**入口：** [D04](#dev-d04) 的阶段出口通过。

**阶段目标：** 将上传、测试视口、UI 与窗口输出纳入同一声明、编译和执行链，建立图正确性及失败退路。

<a id="d05-t01"></a>
### D05-T01 · 实现图资源、版本与 Pass 权限

**职责位置：** Engine/Renderer/RenderGraph。**前置：** 本阶段入口满足。**架构依据：** [6.2](../Architecture.md#graph)。

- [ ] 实现 typed GraphId、代校验、纹理/Buffer 描述、subresource/range、import/export 与读写版本。
- [ ] 实现 setup 即时声明及 execute 的拥有型 PassData/arena 捕获；执行期只能访问 Pass 已声明资源。
- [ ] 覆盖同一外部资源重复导入、失效 ID、越界访问、Load/Store 内容有效性与资源写版本用例。

**完成条件：** 图合同足以表示真实 UI/上传/输出，不依赖跨帧保存 GraphId 或回调借用栈变量。

<a id="d05-t02"></a>
### D05-T02 · 实现依赖编译、活性裁剪与确定顺序

**职责位置：** RenderGraph Compiler。**前置：** [D05-T01](#d05-t01)。**架构依据：** [6.3](../Architecture.md#graph-compile)。

- [ ] 由资源访问建立 RAW/WAR/WAW、版本和 range 依赖，检查未初始化读、非法 feedback、覆盖和环。
- [ ] 从外部输出/side effects 建立活性，稳定拓扑排序并形成首末使用和物理分配计划。
- [ ] 用跨分支、同资源不同 range、被裁剪上传和读回副作用验证编译；非法图不给按声明顺序执行的兜底。

**完成条件：** 纯 CPU 测试能验证正确依赖和失败诊断，compile 不调用原生创建或发出 Draw。

<a id="d05-t03"></a>
### D05-T03 · 实现图执行、首次使用分配与单一同步

**职责位置：** RenderGraph Executor、RHI Common/OpenGL。**前置：** [D05-T02](#d05-t02)。**架构依据：** [6.4](../Architecture.md#graph-resources)、[7.5](../Architecture.md#barriers)、[7.4](../Architecture.md#uploads)。

- [ ] 在 execute 的首次实际使用建立兼容物理资源及 PassResources，接通上传、clear、采样、attachment 等实际用途；创建/recipe 失败明确停止并报告，不能向 Pass 交付空资源。
- [ ] GraphManaged 模式关闭 RHI 自动推断，由图提交用途给同一 tracker/emitter；正常与失败退出均恢复模式，失效设备不再发原生命令；独立 RHI 调用仍用其自动入口。
- [ ] 验证 clear 所需状态先于 Draw、depth 只读附件并采样的联合用途，以及上传→采样、附件→UI 和 Shader 读→后续写的原生结果；RAW/WAR/WAW 都遵守唯一同步发射合同。

**完成条件：** 一个依赖只有一个同步发射权威；图编译与 GL 原生执行边界清楚。

<a id="d05-t04"></a>
### D05-T04 · 将 UI、窗口和本帧纹理绑定接入图

**职责位置：** Renderer、Editor/ImGui、Viewport、Runtime。**前置：** [D05-T03](#d05-t03)。**架构依据：** [10.1](../Architecture.md#editor-output)、[10.2](../Architecture.md#imgui-renderer)、[10.3](../Architecture.md#present)、[frame-flow](../Architecture.md#frame-flow)。

- [ ] 用图构建上传、验证视口和 UI Pass，导出窗口 PresentColor；把 D04 的临时顺序编排收敛到唯一 Graph 路径。
- [ ] UiTextureId 保持稳定，FrameSlotUiFrameBindings 保存本帧 GraphId，执行时从 PassResources 解析实际纹理。
- [ ] 验证隐藏面板/多次 Image 引用、图资源裁剪、目标换代与绑定在途保活；图 ID 不进入 Session 长期状态。

**完成条件：** 同一帧图可解释 UI 读了哪个视口版本和最终窗口目标，构图过程中不绘制。

<a id="d05-t05"></a>
### D05-T05 · 实现图失败、UI fallback 与帧取消

**职责位置：** Runtime、Renderer、DeviceHost。**前置：** [D05-T04](#d05-t04)。**架构依据：** [frame-flow](../Architecture.md#frame-flow)、[11.2](../Architecture.md#history-commit)、[11.5](../Architecture.md#shutdown)。

- [ ] 实现尚未执行的主图编译失败后的安全 UI-only 图，显式导入未被覆写的上一有效输出；无有效输出时展示明确状态，记录 NoSceneOutputs。
- [ ] fallback 也失败或执行中途失败时按宿主协议取消；不在部分场景命令后拼 UI 图，不读取无效窗口颜色，已经发出的 GL 工作仍完成提交/退休或受控退出。
- [ ] 注入 UI/读回准备、fallback 准备、首次资源/recipe 创建、构图、部分命令执行和提交失败；准备阶段 Fatal 立即转受控退出，FrameSlot、PresentationFrame、上传请求/版本、历史与在途引用均不失去 owner，UI 纹理更新可重试。

**完成条件：** 错误帧不会推进场景历史、破坏下一帧或泄漏呈现资源，失败诊断可定位实际 Pass/资源。

**D05 阶段出口：** 产品帧只经统一 Graph 路径；未初始化读、非法 feedback/环会失败，正常帧可到窗口；执行前失败可显示安全旧输出/UI，中途失败走取消，均不推进失败场景的历史。
