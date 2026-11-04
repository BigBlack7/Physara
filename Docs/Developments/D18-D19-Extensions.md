# D18–D19 · 可选高级能力与 Vulkan 后端

[开发计划与当前阶段](Developments.md#current-stage) · [D18](#dev-d18) · [D19](#dev-d19)

<a id="dev-d18"></a>
## D18 · 按需增加独立高级 profile

**性质：** 可选。**入口：** [D17](D16-D17-EditorAndDelivery.md#dev-d17) 的阶段出口通过。

**阶段目标：** 对架构中明确可选的能力分别实施、验证和启用，保持基础单样本三路径配置稳定。

<a id="d18-t01"></a>
### D18-T01 · Forward/Forward+ MSAA profile

**职责位置：** Renderer/Paths、RenderGraph、RHI、PostProcess。**前置：** 本阶段入口满足。**架构依据：** [5.7](../Architecture.md#plan-readiness)、[8.2](../Architecture.md#geometry)、[7.7](../Architecture.md#capabilities)。

- [ ] 定义 Forward/Forward+ 多样本附件、边缘/深度/normal resolve 和透明/后处理兼容矩阵。
- [ ] 实现所选样本与格式、资源代/切换、结果 resolve 和有效配置显示；Deferred MSAA 继续保持不支持。
- [ ] 对 alphaMask、几何边缘、静态实例、透明、TAA/SSR 等兼容组合做 native conformance，并比较画质/带宽/显存。

**完成条件：** 只有明确验证的组合启用；切入 Deferred 时拒绝或按用户选择的可见 policy 解析配置。

<a id="d18-t02"></a>
### D18-T02 · PCSS 阴影 profile

**职责位置：** Renderer/Lighting、Shaders/Shadow、Editor/Panels。**前置：** 本阶段入口满足。**架构依据：** [8.3](../Architecture.md#shadows)、[7.7](../Architecture.md#capabilities)。

- [ ] 定义 blocker search、半影估计、光源尺寸和 filter 质量，复用已有 shadow-depth 语义。
- [ ] 实现级联/spot/point 计划支持范围与参数，控制采样预算及失败/不支持显示。
- [ ] 验证接触硬远处软、bias/边界、不同光源与三路径，和 PCF 比较质量及成本。

**完成条件：** PCSS 独立于基础 PCF 验收，不能把采样图案菜单当作算法完成。

<a id="d18-t03"></a>
### D18-T03 · moments 阴影存储 profile

**职责位置：** Renderer/Lighting/Passes、Shaders/Shadow、RenderGraph。**前置：** 本阶段入口满足。**架构依据：** [8.3](../Architecture.md#shadows)、[6.4](../Architecture.md#graph-resources)。

- [ ] 在独立 spec 中选择并解释 EVSM/EVSSM 路线、精度、指数/可见性近似与适用灯类型。
- [ ] 同时实现 moments 生成、资源格式、过滤、mip/blur 与采样，切换时使不兼容缓存和资源失效。
- [ ] 验证 light bleeding、数值溢出、边界、质量与资源/带宽成本，保留基础 shadow-depth profile。

**完成条件：** 新存储和查询链一起完成，有数值和原生证据，名称与实际算法一致。

<a id="d18-t04"></a>
### D18-T04 · SSR Hi-Z traversal profile

**职责位置：** Shaders/ScreenSpace、Renderer/Passes。**前置：** 本阶段入口满足。**架构依据：** [8.4](../Architecture.md#surface-effects)、[12.2](../Architecture.md#benchmark)。

- [ ] 在基线 ray march 已有画质参考上定义层级步进、交点精化和厚度规则。
- [ ] 复用深度层级/ReflectionHdr/曝光与置信度合同，保持屏幕外和无效历史退路。
- [ ] 比较高/低 roughness、薄几何、反向深度和分辨率下的错误/漏交及 GPU 成本。

**完成条件：** 优化用同质量证据证明收益，不改变历史来源或丢弃困难像素。

<a id="d18-t05"></a>
### D18-T05 · 可选镜头 flare profile

**职责位置：** Renderer/PostProcess、Shaders/PostProcess、Editor/Panels。**前置：** 本阶段入口满足。**架构依据：** [9.3](../Architecture.md#lens-effects)、[9.4](../Architecture.md#display-output)。

- [ ] 定义作为图像效果的输入域、亮度/质量参数、遮挡近似和与 Bloom/DoF 的固定组合顺序。
- [ ] 实现图内 Pass、资源预算、开关裁剪与显示设置，不把 flare 纳入表面能量积分。
- [ ] 验证曝光变化、高亮、镜头边缘和 UI 不受污染，测量各质量成本。

**完成条件：** 可选图像效果拥有完整 profile，关闭时无不必要资源与执行成本。

**D18 阶段出口：** 本阶段每项独立决定是否实施；选中的 profile 完成输入/图/RHI/输出/UI/退出/原生验证后才启用，不作为 OpenGL 基线完成前提。

<a id="dev-d19"></a>
## D19 · 在同一合同下接入 Vulkan

**性质：** 后续。**入口：** [D17](D16-D17-EditorAndDelivery.md#dev-d17) 的阶段出口通过。

**阶段目标：** 在完整 OpenGL 渲染器之后迁移 NVRHI Vulkan 后端，验证同一 Scene/Shader/Graph/Editor 的跨后端行为。

<a id="d19-t01"></a>
### D19-T01 · 确定 Vulkan 源映射与设备基础 profile

**职责位置：** RHI/Vulkan.hpp、RHI/Vulkan、Platform、ThirdParty。**前置：** 本阶段入口满足。**架构依据：** [7.8](../Architecture.md#vulkan)、[7.1](../Architecture.md#rhi)、[7.7](../Architecture.md#capabilities)。

- [ ] 按固定 NVRHI 源基线核对实际 Vulkan feature/extension/loader 与提交要求，记录命名和必要差异。
- [ ] 选择并验证可覆盖现有基础 renderer 的设备/格式/样本/绑定 profile，保留不支持项的真实报告。
- [ ] 补充构建与许可/部署依赖；不因接入 Vulkan 自动开启 RT、mesh、descriptor indexing 或 async queues。

**完成条件：** 需要的原生能力与上游实现一致，基础上层合同不通过新增后端特判来补洞。

<a id="d19-t02"></a>
### D19-T02 · 实现 VulkanDeviceHost 与呈现生命周期

**职责位置：** Platform/VulkanDeviceHost.cpp、DeviceHost。**前置：** [D19-T01](#d19-t01)。**架构依据：** [7.8](../Architecture.md#vulkan)、[10.3](../Architecture.md#present)、[11.5](../Architecture.md#shutdown)。

- [ ] 实现 instance/device/queue 组合、surface/swapchain、acquire token、窗口状态和 native 宿主所有权。
- [ ] 定义 acquire wait、render-finished/present 信号、out-of-date/suboptimal、零尺寸与重建协议。
- [ ] 实现取消时消费 acquire 信号/处理图像的完整协议，区分渲染完成和呈现引擎已释放资源。

**完成条件：** 宿主可安全 acquire/取消/重建/退出，Engine 不获得 swapchain token 或平台原生类型。

<a id="d19-t03"></a>
### D19-T03 · 迁移 Vulkan 资源与视图

**职责位置：** RHI/Vulkan、Common/Validation。**前置：** [D19-T01](#d19-t01)、[D19-T02](#d19-t02)。**架构依据：** [7.8](../Architecture.md#vulkan)、[7.2](../Architecture.md#rhi-objects)、[1.3](../Architecture.md#shader-build)。

- [ ] 迁移 Buffer/Texture/views/Sampler/Framebuffer、原生存储与映射，按公共资源描述和引用模型实现。
- [ ] 对照公共接口验证格式/usage/range/对齐、视图兼容、内存/引用关系和初始资源状态，保持来源可追踪。
- [ ] 补齐非 coherent 主机映射：按 nonCoherentAtomSize 对齐所需范围，CPU 写后条件 flush、GPU 完成后 CPU 读前条件 invalidate，并建立主机访问依赖；本工作包检查映射范围、对齐和主机访问操作，GPU 可见性随 D19-T05 的正式 copy/readback 链验证，像素 Draw 随命令链接入。

**完成条件：** 同一公共资源合同有真实 Vulkan 存储/读写证据，原生对象和内存生命周期可解释。

<a id="d19-t04"></a>
### D19-T04 · 迁移 Vulkan 绑定、Shader 与 Pipeline

**职责位置：** RHI/Vulkan Binding/Shader/Pipeline。**前置：** [D19-T03](#d19-t03)。**架构依据：** [7.8](../Architecture.md#vulkan)、[7.2](../Architecture.md#rhi-objects)、[1.3](../Architecture.md#shader-build)。

- [ ] 迁移 binding layout/set、descriptor 管理、canonical SPIR-V Shader 与 graphics/compute Pipeline，保持公开 Texture/Sampler 语义。
- [ ] 验证 Shader ABI、layout/Framebuffer 兼容、缓存 key、动态状态和 descriptor 在途版本/保活。
- [ ] 对照固定上游行为和现有公共消费者完成有效/非法绑定、不同 sampler、PSO 和资源限额用例；GL lowering 不进入上层。

**完成条件：** 既有 Shader 包和绑定合同可在 Vulkan 建立有效 Pipeline，来源差异和真实能力可追踪。

<a id="d19-t05"></a>
### D19-T05 · 迁移命令、上传、barrier 与完成退休

**职责位置：** RHI/Vulkan CommandList/Queue/Upload/Queries。**前置：** [D19-T03](#d19-t03)、[D19-T04](#d19-t04)。**架构依据：** [7.8](../Architecture.md#vulkan)、[7.5](../Architecture.md#barriers)、[7.4](../Architecture.md#uploads)、[11.1](../Architecture.md#retirement)、[11.3](../Architecture.md#readbacks)。

- [ ] 迁移图形/compute、copy/clear/indirect、staging、query 和公共状态转换；接入同一 GraphManaged 用途入口，copy 入口独立退出活动 rendering scope，不能依赖空 barrier 列表偶然触发退出。
- [ ] 建立真实提交/完成值、版本化上传、在途绑定/资源引用与 native 删除；设备失效必须锁存 Fatal，Execute 返回后先检查再推进历史，停止新提交且不等待失败 ID。
- [ ] 检查 producer/consumer、跨帧上传、释放后提交、coherent/非 coherent copy/readback 可见性、非紧密 row pitch、查询及设备失效退出；首发保持单线程录制，未来启用并行时先以每列表/线程 VCB 状态或原子游标解决共享 cursor，并审核其余共享字段，slot CAS 不能代替该协议。

**完成条件：** 已有 GL 合同用例可按 Vulkan native 行为验证，状态与寿命正确性不靠 deviceWaitIdle。

<a id="d19-t06"></a>
### D19-T06 · 接通整帧图、UI 与窗口呈现

**职责位置：** RenderGraph、Runtime、Editor/ImGui、Platform。**前置：** [D19-T02](#d19-t02)、[D19-T03](#d19-t03)、[D19-T05](#d19-t05)。**架构依据：** [7.8](../Architecture.md#vulkan)、[frame-flow](../Architecture.md#frame-flow)、[10.2](../Architecture.md#imgui-renderer)、[10.3](../Architecture.md#present)。

- [ ] 把 acquire 的 RHI 目标/状态导入相同 Graph，在提交边界接入 wait/signal，按宿主协议 present。
- [ ] 复用同一 UI RHI renderer、稳定纹理 ID、资源池和完成驱动 FrameSlot，核对 clip/viewport/winding 与 sRGB。
- [ ] 验证主图失败/UI fallback、未提交取消、窗口最小化、out-of-date 和在途重建，不新增第二套渲染主循环。

**完成条件：** 唯一产品入口可选择 Vulkan 并启动完整编辑器，平台语义差异局限于指定边界。

<a id="d19-t07"></a>
### D19-T07 · 完成跨后端渲染与能力一致性

**职责位置：** RHI、Renderer、Editor、Platform。**前置：** [D19-T06](#d19-t06)。**架构依据：** [12.1](../Architecture.md#tests)、[7.8](../Architecture.md#vulkan)、[conventions](../Architecture.md#conventions)、[8.7](../Architecture.md#opaque-shading)。

- [ ] 比较 GL/Vulkan 三路径、目标 glTF 扩展、静态几何、阴影、透明/SSR、None/FXAA/TAA、镜头与最终显示。
- [ ] 验证非零实例/索引、方向/深度、拾取/截图、热重载、资源池/在途退出和最低能力设备。
- [ ] 按数值格式/算法误差定义跨后端容差，明确不同 caps 导致的有效 profile，不伪造完全相同的硬件能力。

**完成条件：** 跨后端相同配置结果等价，后端差异不导致静默缺失功能或换一套物理模型。

<a id="d19-t08"></a>
### D19-T08 · 完成 Vulkan 成本、部署与交付

**职责位置：** Runtime、各模块 CMakeLists、ThirdParty、Docs。**前置：** [D19-T07](#d19-t07)。**架构依据：** [12.2](../Architecture.md#benchmark)、[7.8](../Architecture.md#vulkan)、[12.3](../Architecture.md#project-docs)。

- [ ] 在同场景/配置和可比设备上采集 CPU/GPU/提交/呈现/显存指标，解释 API/驱动差异。
- [ ] 完成 Vulkan 依赖、Shader 包、启动选择、设备错误诊断与干净安装验证；同步当前支持矩阵。
- [ ] 完成长期架构变化与已采纳行为同步、原生 verify/archive；可选 D18 profile 仅移植已选且有证据的部分。

**完成条件：** Vulkan 后端成为同一 renderer 的受支持实现，后续优化建立独立 change 而不改写已有完成记录。

**D19 阶段出口：** 声明的 Vulkan 基础 profile 通过 RHI 原生与整帧一致性验收；窗口 acquire/present/退休/取消独立正确，切换后端无需另写上层 Renderer。
