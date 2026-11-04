# D00–D02 · 工程、窗口与 RHI 资源

[开发计划与当前阶段](Developments.md#current-stage) · [D00](#dev-d00) · [D01](#dev-d01) · [D02](#dev-d02)

<a id="dev-d00"></a>
## D00 · 建立可重建的工程与开发基线

**性质：** 基线。**入口：** D00 为工程起点。

**阶段目标：** 形成轻量、可干净配置和构建的 2.0 工程，确认资源归属与依赖来源。

<a id="d00-t01"></a>
### D00-T01 · 确认重建边界与可用资源清单

**职责位置：** 工程根、Assets、ThirdParty。**前置：** 本阶段入口满足。**架构依据：** [1.1](../Architecture.md#build)、[3.1](../Architecture.md#asset-types)、[12.3](../Architecture.md#project-docs)。

- [x] 核对当前保留的模型、纹理、环境、图标及第三方源码，记录用途、来源与许可；未核实项明确注明。
- [x] 确认 glTF/GLB 模型与 JSON 编辑器场景的目录归属，场景通过资源引用组合模型；启动输入在实际加载能力实现时确定。
- [x] 对齐现有目录与架构职责，保留当前有用途的资源和源码，初始化不依赖旧引擎程序或生成缓存。

**完成条件：** 当前资源和目录有明确归属，来源及待核实项可查；工程重建不依赖个人机器路径或旧构建残留。

<a id="d00-t02"></a>
### D00-T02 · 建立 target 级构建与依赖锁定

**职责位置：** CMakeLists、各模块 CMakeLists、ThirdParty。**前置：** [D00-T01](#d00-t01)。**架构依据：** [1.1](../Architecture.md#build)。

- [x] 用根及各模块 `CMakeLists.txt` 组织实际构建内容，明确源码、语言标准、第一方警告与公共/私有依赖；按需加入已有实现。
- [x] 第三方保持精简源码与头文件，在 `ThirdParty/README.md` 记录固定版本或 commit、来源、许可和必要修改；NVRHI 仅供阅读参考，不加入构建。
- [x] 验证离线配置、源码树外 Debug/Release 构建与缺失依赖诊断；需要编译的第三方源码随工程生成到 build 目录。

**完成条件：** 空构建目录可离线重建当前工程，依赖方向清楚，第三方不继承第一方警告策略，版本与来源记录和实际源码一致。

<a id="d00-t03"></a>
### D00-T03 · 确认构建入口与使用说明

**职责位置：** 工程根、各模块 CMakeLists。**前置：** [D00-T02](#d00-t02)。**架构依据：** [1.1](../Architecture.md#build)。

- [x] 在工程说明中给出所需编译工具、configure/build 命令、构建输出位置与当前可用范围。
- [x] 为第一方 CMake 中的目标职责、依赖关系和必要平台设置补充简明中文说明，保持模块组织直观。
- [x] 核对当前配置的正常构建与必要失败诊断；临时输出保留在 build 中，未实现的程序和图形能力如实注明。

**完成条件：** 读者能从说明与 CMake 理解当前构建内容并复现构建，实际检查结果与工程能力一致。

<a id="d00-t04"></a>
### D00-T04 · 初始化原生 OpenSpec 与文档关联

**职责位置：** openspec、Docs/Reference、Docs/Developments。**前置：** [D00-T02](#d00-t02)。**架构依据：** [12.3](../Architecture.md#project-docs)、[12.4](../Architecture.md#feature-closure)。

- [x] 记录使用的 OpenSpec 版本、官方 schema 和项目配置，沿原生工作流管理后续 change。
- [x] 对齐 AGENTS.md、Architecture.md 与本计划；引用已有 Filament/NVRHI 分析，保留源码版本及许可证信息。
- [x] 确认任务 ID 到 change 的关联写法和状态更新规则；按实际验证结果更新完成标记。

**完成条件：** 当前文档相互一致，后续工作包能关联原生 change，实施与验证记录有单一归属。

**D00 阶段出口：** 在新构建目录完成声明配置的 configure/build；CMake 组织、依赖记录、资源归属和文档一致，当前未实现的能力清楚可见。

**当前状态** D00 已完成并归档。 [establish-d00-baseline](../../openspec/changes/archive/2026-10-22-establish-d00-baseline/tasks.md)。

<a id="dev-d01"></a>
## D01 · 建立 Core 与窗口宿主的可靠寿命

**性质：** 基线。**入口：** [D00](#dev-d00) 的阶段出口通过。

**阶段目标：** 获得可采集输入、处理尺寸变化、正确退出的真实 GL 4.6 窗口，只加入本阶段已经消费的基础机制。

<a id="d01-t01"></a>
### D01-T01 · 落地最小身份、错误与计时机制

**职责位置：** Engine/Core。**前置：** 本阶段入口满足。**架构依据：** [1.2](../Architecture.md#core)、[conventions](../Architecture.md#conventions)。

- [x] 实现窗口与后续真实消费者需要的类型化身份、秒计时、带上下文错误结果及有界日志；不同身份不可混用。
- [x] 核对正常与初始化失败诊断、日志容量、时间单位和最小 owner 的关闭顺序，不预建当前没有消费者的帧内存或作业系统。

**完成条件：** 窗口链能表达身份、失败和时间，不引入全局服务定位器；FrameArena、后台作业和文件监视分别随实际消费者接入。

**当前状态** 已完成并归档。 [establish-native-gl-host](../../openspec/changes/archive/2026-10-25-establish-native-gl-host/tasks.md)。

<a id="d01-t02"></a>
### D01-T02 · 实现 DeviceHost 和原生 GL 启动边界

**职责位置：** Platform/DeviceHost、GlfwWindow、OpenGLDeviceHost。**前置：** [D01-T01](#d01-t01)。**架构依据：** [1.5](../Architecture.md#platform-start)、[1.4](../Architecture.md#runtime)。

- [x] 依次完成 GLFW 初始化、窗口/context 创建、current、入口加载、实际版本/limits 查询；Device 工厂在 D02 接入。
- [x] 实现唯一宿主和主线程合同，隔离 GlfwAccess 私有借用；Engine targets 不链接 Platform。
- [x] 为 GLFW/context/加载失败和部分初始化退出建立真实宿主检查；记录实际平台与驱动信息。

**完成条件：** 真实 GL context 可重复创建销毁，失败不留下全局 GLFW 状态或半初始化宿主。

**当前状态** 已完成并归档。 [establish-native-gl-host](../../openspec/changes/archive/2026-10-25-establish-native-gl-host/tasks.md)。

<a id="d01-t03"></a>
### D01-T03 · 实现窗口状态与有序输入快照

**职责位置：** Platform/Input.hpp、InputState、GlfwInput、GlfwWindow。**前置：** [D01-T02](#d01-t02)。**架构依据：** [2.1](../Architecture.md#input)、[conventions](../Architecture.md#conventions)。

- [x] 采集逻辑尺寸、framebuffer 像素尺寸、content scale、焦点、鼠标键盘与文本，形成有序事件和 poll 后只读快照。
- [x] 定义按键释放、失焦清理、鼠标捕获和 closeRequested；平台请求在确定边界应用。
- [x] 检查 DPI/跨屏比例、零尺寸、快速 resize 和文本输入，确认单位转换只在约定边界发生。

**完成条件：** 输入和尺寸不依赖初始窗口值；关闭请求由上层解释，窗口不会在回调里自行销毁。

**当前状态** 已完成并归档。 [establish-window-input-runtime](../../openspec/changes/archive/2026-11-02-establish-window-input-runtime/tasks.md#cross-monitor-verification)。

<a id="d01-t04"></a>
### D01-T04 · 建立单产品入口及分阶段退出

**职责位置：** Runtime/Main.cpp。**前置：** [D01-T02](#d01-t02)、[D01-T03](#d01-t03)。**架构依据：** [1.4](../Architecture.md#runtime)、[frame-flow](../Architecture.md#frame-flow)、[11.5](../Architecture.md#shutdown)。

- [x] Main.cpp 只装配当前存在的 Core/DeviceHost，保留后续 Editor、RenderEngine 的依赖方向；不创建第二个产品入口。
- [x] 落实 poll、关闭/最小化判断与平台请求的基本顺序，暂不宣称具备场景渲染能力。
- [x] 检查正常、初始化失败及用户关闭的逆依赖释放；后续每引入一个实际 owner 就扩充同一退出路径。

**完成条件：** 启动程序与隐藏窗口检查复用同一宿主；最小化不创建零尺寸 GPU 目标。

**当前状态** 已完成并归档。 [establish-window-input-runtime](../../openspec/changes/archive/2026-11-02-establish-window-input-runtime/tasks.md#cross-monitor-verification)。

**D01 阶段出口：** 单入口可反复启动/关闭；初始化失败、失焦、DPI 变化和零像素窗口均有确定行为，全部原生对象在所属线程释放。

**当前状态** D01 阶段出口已通过。 [本轮 tasks](../../openspec/changes/archive/2026-11-02-establish-window-input-runtime/tasks.md#cross-monitor-verification)。

<a id="dev-d02"></a>
## D02 · 移植 RHI 合同并实现原生资源

**性质：** 基线。**入口：** [D01](#dev-d01) 的阶段出口通过。

**阶段目标：** 保留 NVRHI 完整公共谱系，先实现基础 OpenGL 资源与可验证的设备/资源生命周期。

**前置调整** D01 真实跨屏验收曾获限时豁免，允许 D02-T01 至 T05 按各自批准的范围先行。2026-11-01 验收补齐，例外已解除。 [前置与退出依据](../../openspec/changes/archive/2026-11-02-establish-window-input-runtime/design.md#closeout)。

<a id="d02-t01"></a>
### D02-T01 · 机械移植完整公共接口与来源映射

**职责位置：** RHI/RHI.hpp、Resource.hpp、Containers.hpp、ShaderTypes.hpp、Utils.hpp。**前置：** 本阶段入口满足。**架构依据：** [7.1](../Architecture.md#rhi)。

- [x] 按 Architecture 固定 NVRHI commit 移植公共类型、接口、句柄、描述符、资源状态和扩展方法，应用受控命名规则。
- [x] 记录上游路径/目标路径、hash、许可及非机械差异；保留未来 Vulkan 所需字段和虚函数。
- [x] 建立公共消费者编译检查和接口差异核对；不把 GL 暂不支持的 API 从公共合同删掉。

**完成条件：** 公共声明与固定上游有可审查的映射，业务层不出现窗口、ImGui 或 CombinedTextureSampler 公共类型。

**当前状态** 已完成并归档。 [establish-rhi-public-contract](../../openspec/changes/archive/2026-10-25-establish-rhi-public-contract/tasks.md)。

<a id="d02-t02"></a>
### D02-T02 · 移植 Common 与 Validation 并定义初始 profile

**职责位置：** RHI/Common、RHI/Validation、Validation.hpp。**前置：** [D02-T01](#d02-t01), 并满足阶段入口。**架构依据：** [7.2](../Architecture.md#rhi-objects)、[7.5](../Architecture.md#barriers)、[7.7](../Architecture.md#capabilities)。

- [x] 移植资源引用、状态跟踪及公共辅助逻辑，核对 GL 所需的后端分类、状态和合法调用验证。
- [x] 定义初始 GL 限制策略和基于底层查询的格式/usage 检查; 不把策略、格式表或 CPU fixture 当作真实 caps, 未实现操作明确拒绝.
- [x] 增加描述符、range、绑定与能力的正反用例；状态跟踪先保留单一权威，为 D05/D08 接入发射。

**完成条件：** 正式静态目标的 Common/Validation 可独立构建、链接, CPU 正反用例验证通用算法、输入拒绝、策略过滤、调用状态和 CPU 所有权. 包装保留底层失败, 能区分非法调用与不支持; 本包不依赖尚待 T03 创建的真实 GL Device, 不声称已证明原生成功. 实施与证据见 [归档 change](../../openspec/changes/archive/2026-10-26-establish-rhi-common-validation/tasks.md).

**当前状态** 已完成并归档。 [establish-rhi-common-validation](../../openspec/changes/archive/2026-10-26-establish-rhi-common-validation/tasks.md)。

<a id="d02-t03"></a>
### D02-T03 · 接入 GL Device、资源对象与基本格式

**职责位置：** RHI/OpenGL.hpp、RHI/OpenGL、Platform/RhiDeviceHost、DeviceHost。**前置：** [D02-T01](#d02-t01)、[D02-T02](#d02-t02) 完成, D01 出口通过。**架构依据：** [7.3](../Architecture.md#opengl)、[7.2](../Architecture.md#rhi-objects)、[7.7](../Architecture.md#capabilities)。

- [x] 在有效 context 上创建 Device，读取并保存 limits/格式支持，设置并查询 GL_UPPER_LEFT、ZERO_TO_ONE, 保持公共 RasterState 合同；实际 raster 映射与像素验收由 D03-T03 完成。
- [x] 实现 DSA Buffer、Texture、独立 Sampler、基础视图/Framebuffer 和图形目标所需格式；创建失败有明确诊断。
- [x] 验证尺寸、mip/layer、格式、sRGB/data、usage 和样本限制；公开 API 保持 Texture/Sampler 分离。
- [x] 将同一 Validation 包装接入真实 GL Device, 在 Release 核对本机 limits、每格式 sample 支持、资源 Device/context 归属与正反调用; 不增加平行参数检查系统.

**完成条件：** 基础资源和 Validation 包装在真实设备上创建与检查, 设备 caps 与实际启用能力一致, 非法描述/跨设备资源在 Release 也不会发出非法原生命令.

**当前状态** 已完成并归档。 [establish-opengl-device-resources](../../openspec/changes/archive/2026-10-27-establish-opengl-device-resources/tasks.md)。

**范围** 本包交付 GL Device 与基础资源。上传归 T04，资源内容一致性归 T05，RasterState、实际绕序和 Reverse-Z 像素归 [D03-T03](D03-D05-FramePipeline.md#d03-t03)。

<a id="d02-t04"></a>
### D02-T04 · 实现安全的初始上传与资源释放协议

**职责位置：** RHI/OpenGL 资源/上传/删除与上传限定列表/提交, Platform 关闭互操作。**前置：** [D02-T03](#d02-t03) 完成, 并通过 D01 出口。**架构依据：** [7.4](../Architecture.md#uploads)、[7.6](../Architecture.md#command-execution)、[11.1](../Architecture.md#retirement)、[11.5](../Architecture.md#shutdown)。

- [x] 实现有界、拥有源数据的基础 Buffer/Texture 内容写入路径；声明 CPU 数据可以释放的确切时刻。
- [x] 建立上传所需的 Graphics/单即时列表、一次提交、真实 fence/完成及在途保活基础, 供 D03 在同一对象上扩展。
- [x] 实现 T03 无提交资源的 wrapper 引用与 context 线程原生删除队列；设备关闭前收拢该已交付子集。
- [x] 验证资源创建失败、重复分配释放、视图持有关系及原生名称复用时的缓存失效。
- [x] 验证上传区段与资源只在真实完成后退休, 并完成未提交即时前缀、外部/内部引用区分和可重试 drain 的关闭协议。

**完成条件：** CPU 源数据释放时刻明确, 上传内容及寿命有真实设备证据; 资源和上传区段的退休不依赖固定帧差, 没有悬空来源、过早复用或错误线程删除。

**衔接** T04 沿用 T03 的同一设备与删除协议，扩展内容上传、真实提交完成和源数据寿命。

**当前状态** 已完成并归档。 [tasks](../../openspec/changes/archive/2026-10-28-establish-opengl-resource-uploads/tasks.md)。

<a id="d02-t05"></a>
### D02-T05 · 建立真实资源与设备能力一致性用例

**职责位置：** RHI、Platform 的正式消费者与 native 验证边界。**前置：** [D02-T03](#d02-t03)、[D02-T04](#d02-t04) 完成, 并通过 D01 出口。**架构依据：** [12.1](../Architecture.md#tests)、[7.7](../Architecture.md#capabilities)、[conventions](../Architecture.md#conventions)。

- [x] 建立共享正式 DeviceHost/Validation/backend 的 native fixture，验证 buffer 数据、颜色纹理行序、mip/layer 和格式读写; 对已有分配支持的深度/模板/MS, 由 fixture 原生初始化和必要 resolve 后检查存储内容。
- [x] 记录 GL graphics 单队列/单即时列表等 profile；对 replay、异步队列、RT/mesh/sparse 等当前能力明确拒绝。
- [x] 测试异常描述、资源耗尽可控失败与设备前资源释放；测试辅助原生操作只位于 fixture/后端边界。
- [x] 复核同一 Validation 包装下的真实 caps/格式/样本/资源归属和原生行为一致性; CPU double 不替代此项证据.

**完成条件：** 当前 D02 资源 profile 的能力、分配/上传、失败、归属和释放均有真实设备证据, 原生辅助与产品完成链的证据不混用; 测试环境与产品使用同一个 GL 后端实现。

**范围** 验收当前 Validation 下的 GL 资源与上传 profile。fixture 的原生辅助操作仅证明资源内容。

**衔接** 产品 clear、Shader、Draw 和绘制提交归 [D03](D03-D05-FramePipeline.md#dev-d03)，完整 transfer、resolve、staging、readback 与查询归 [D08-T05](D08-D11-RenderPaths.md#d08-t05)。这些能力不作为 D02 出口的前置。

**当前状态** 已完成并归档。 [tasks](../../openspec/changes/archive/2026-10-29-establish-opengl-resource-conformance/tasks.md)。

**D02 阶段出口：** 公共层、Common、Validation 可构建；真实 GL 设备可创建/写入/释放基础资源，不支持项明确报错，设备/context 退出顺序正确。

**当前状态** D02 阶段出口已通过，D03 可按普通前置推进。 [本轮 tasks](../../openspec/changes/archive/2026-11-02-establish-window-input-runtime/tasks.md#cross-monitor-verification)。

<a id="baseline-review"></a>
D02 完成后补充的首次全仓审查及整改已完成并归档，关联 [fix-validation-cpu-boundaries](../../openspec/changes/archive/2026-11-03-fix-validation-cpu-boundaries/tasks.md#review-loop-closeout)。
