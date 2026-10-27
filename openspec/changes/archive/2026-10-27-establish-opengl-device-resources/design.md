# Design

## Context

动机及工作包范围见 [proposal](proposal.md). 当前代码基线为 `fbc7f9e2f975c0d8433962e9d2cc720e0d371f9e`. 已核对四个主规格、D01 的 window-input/runtime-lifecycle change、Developments D01/D02/D03、Architecture 1.5/7.1-7.7/11.1/11.5、现有 DeviceHost 创建/关闭、完整 Device/资源接口、Validation 输入/profile 和 CMake.

当前事实:

- PhysaraRHI 只有 Common/Validation 的 STATIC 实现. Device 有完整的 68 项虚方法, Texture/Buffer 等没有统一 GetDevice. Resource/RefCounter 管 CPU 引用, 不能自行解决 context 或 GPU 寿命.
- DeviceHost 持有唯一 GLFW/window/context 和 Diagnostics 借用, 记录 owner 线程; StartNativeHost 在 InspectEnvironment 后直接配置 swap interval 和输入, ReleaseNativeState 在解绑输入后销毁 context/window. 当前没有 RHI owner 或原生资源.
- GLAD 已固定为 OpenGL 4.6 Core、无额外扩展的生成快照. 当前只检查启动查询入口, 新资源路径必须补查实际会调用的入口, 不能仅依赖 GLAD_GL_VERSION_4_6.
- 通用 Validation 的初始 GL 策略只允许 ConstantBufferRanges 的底层查询; 完整 Device 方法必须仍有实现归属, 未落地部分不得制造成功. 通用层不能验证真实资源 owner、limits 或每格式样本集合.
- rhi-public-contract 的独立消费者场景明确排除 GL loader 依赖. 因而不能把 GLAD 接到 PhysaraRHI 的 PRIVATE links 后声称消费者仍与 native 无关; 静态依赖仍会传播到链接闭包.
- D01 当前只有单显示器证据, 任务 4.1 和阶段出口未完成. T01/T02 的例外没有自动扩大; 本次 apply 的新增明确授权及边界见下节.

本 change 新增 [opengl-device-resources](specs/opengl-device-resources/spec.md), 并完整修改 [native-window-host 的两项要求](specs/native-window-host/spec.md). 宿主旧场景全部保留; 不修改已有主规格或历史归档.

## Goals / Non-Goals

**Goals:** 在现有宿主内建立真实设备和基础分配对象, 明确 native 合法性、CPU 所有权、零引用后的原生删除及显式关闭重试; 新后端可通过正式目标消费, 既有通用目标继续完全独立.

**Non-Goals:** 创建 CommandList、上传/映射、内容 clear/copy/readback、Shader/Binding/Pipeline、状态 barrier 发射、提交/fence、呈现、persistent mapping、资源池或视图缓存. 本包的 clip 查询不代替 D03 的绕序/深度/行序像素验收. 不添加新的全局 Context 服务、错误框架、RHI 参数检查器、原生对象地址 registry 或通用测试框架.

## Decisions

### 1. 实施入口和交付范围

实施的第一项检查已经确认 D01 为 15/16、当前 1 个活动显示器, 因而原始前置未通过. 维护者随后明确答复“同意限定推进，修订规划后继续（推荐）”, 授权在保留 D01 缺口的条件下先实施本 change 的 T03 设备、基础资源、必要删除和全部既定 native 检查. 此处记录独立的本次授权, 不追改 D01 历史或假称原前置已满足.

例外原因是本包的 context 内分配、设备能力和对象查询不依赖不同屏幕的 logical/framebuffer/scale 变化. 适用范围严格限本包资源闭环; 必须重跑受影响 Host/输入/退出回归, 任何跨屏结果仍如实为未覆盖. 退出条件是 D01 原 4.1 获得真实不同缩放双屏证据并通过其阶段出口, 此后恢复普通入口. T04 上传、T05 全面读回、D03 Shader/Draw/提交、窗口呈现和跨屏 UI 不获得提前实施许可.

本包覆盖 T03, 同时完成其创建对象不可缺少的 T04 原生删除子集. T04 的上传、内容寿命和后续增强仍未完成; T05 全面数据/行序读回以及 D02 总出口仍未完成. 不把资源留给下一 change 再补析构, 也不提前实现 D03 提交系统作为释放的替代.

**备选:** 未获本次授权前直接沿用 T02 例外进入 native 会违反明确前置; 将 T03/T04/T05/D03 合并会扩大为另一项未讨论的绘制闭环. 采用已批准的本包限定顺序, 只纳入必要的资源退出协议.

### 2. 真实构建边界和文件职责

```text
Physara(Runtime) -> PhysaraPlatform -> PhysaraRHIOpenGL -> PhysaraRHI
                         |                   |
                         +-> Core/glfw/glad   +-> glad (PRIVATE)
独立通用消费者 -------------------------------> PhysaraRHI
```

PhysaraRHI 的 sources/include/links 和六公共头保持既有闭包. 新增 RHI/OpenGL/CMakeLists.txt 声明实际 STATIC 目标 PhysaraRHIOpenGL, PUBLIC 链接 PhysaraRHI、PRIVATE 链接现有 glad; 根 CMake 在 RHI 之后、Platform 之前显式加入该目录, Platform PRIVATE 链接新目标. 不通过 TARGET glad 的隐式开关、空 target 或 EXCLUDE_FROM_ALL 隐藏依赖错误. 独立通用消费者仍可只 add_subdirectory(RHI), 不需要创建 glad 或新后端目标.

| 位置 | 本包职责 |
|---|---|
| RHI/OpenGL.hpp | 后端工厂、借用 context 校验与显式设备关闭合同; 只含 RHI/标准类型 |
| RHI/OpenGL/Backend.hpp | Device、实际资源类、caps、删除记录的跨 cpp 私有合同 |
| RHI/OpenGL/Device.cpp | 工厂/状态/入口/线程、查询、原生删除及完整 Device 方法归属 |
| RHI/OpenGL/Unsupported.cpp | 同一 Device 的不可用接口闭包, 含扩展/deprecated 重载的明确失败; 不创建替代对象 |
| RHI/OpenGL/Formats.cpp | 本包格式映射、target 查询和设备能力构造 |
| RHI/OpenGL/Buffer.cpp | Buffer 合同与 immutable allocation |
| RHI/OpenGL/Texture.cpp | 九种 Texture 存储、component mapping 和必要视图构造 |
| RHI/OpenGL/Sampler.cpp | 独立 Sampler 参数映射 |
| RHI/OpenGL/Framebuffer.cpp | 完整附件预检、私有视图/附件引用、完整性与兼容信息 |
| Platform/DeviceHostInternal.hpp、RhiDeviceHost.cpp | adapter、raw/validated Device owner、工厂与关闭互操作; 与 loader/窗口查询分担不同寿命职责 |
| Platform/DeviceHost.hpp/.cpp、GlfwWindow.cpp | 借用入口、阶段/错误和最终 Ready/退出顺序 |

Backend.hpp 是这些 cpp 的真实私有合同, 不能进入公开依赖. 不为文件数机械增设空模块; 若完整 Device override 声明触发头长度评审, 说明其接口闭包和私有消费关系, 不切成若干无职责的 helper 头. 第一方使用 Physara::RHI::OpenGL 和既有 Platform 域、PascalCase、中文原因注释、所属域内 static, 不引入匿名域或空泛 Detail 域.

新 GL 实现按公共合同原创, 不是从不存在的 NVRHI GL backend 机械搬运. 若复制固定参考中的代码, 保留 MIT/来源并逐项记录; 不复制 Vulkan SDK 对象或枚举作为 GL 占位. 实施时更新 RHI/README 的新入口、支持矩阵和差异, Build 的实际目标与宿主能力; Architecture 保持既有长期设计.

### 3. 工厂互操作、借用与状态

新增后端入口采用 RHI 既有 handle/MessageCallback 失败形式, 不将 Core/Platform 引入 RHI. 具体公开合同为:

```cpp
namespace Physara::RHI::OpenGL
{
    struct ContextAccess
    {
        void *user = nullptr;
        bool (*isCurrent)(void *) noexcept = nullptr;
    };

    struct DeviceDesc
    {
        MessageCallback *messageCallback = nullptr;
        ContextAccess context;
    };

    enum class ShutdownMode : uint8_t
    {
        Normal,
        ContextUnavailable
    };

    DeviceHandle CreateDevice(const DeviceDesc &desc, uint32_t *nativeError = nullptr);
    bool ShutdownDevice(RHI::Device *device, ShutdownMode mode, uint32_t *nativeError = nullptr);
}
```

nativeError 仅借用到返回, 入口先写 0, GL 失败时保存首个实际 GLenum 数值, 不从日志文本解析或制造错误码. 详细原因沿现有 callback, 所有失败返回空/false; 引用型 crash helper 保持报告后 logic_error. 工厂/资源创建的 bad_alloc 转为空, 未发布状态由 RAII 撤销. 其它 CPU 异常沿明确的头合同; Host noexcept 边界转换为现有 HostError, 不让异常穿过 GLFW/native callback.

ShutdownMode 明确区分普通 current-context 删除和外部 owner 已确定不能再执行 GL、即将释放所属 context 的退出. ContextUnavailable 只清理 CPU 退休记录并停止设备, 不调用 context 校验/GL、不宣称完成了 GL 删除或 GPU 工作; 调用方必须继续释放那个 context. 它不是日常收集或绕过活资源/线程预检的选项. 普通缺少 current 的调用应拒绝而不自动使用这个模式.

ContextAccess 是外部 context 的窄互操作, 不是状态/服务容器. user 和 isCurrent 均不得为空, user/callback 必须覆盖设备全部 CPU 寿命, 校验不可抛出/重入; Platform 的实现先核对 owner 线程, 再比较 glfwGetCurrentContext() 与该 State 的 window. 工厂仅在校验成立后记录线程和建立 Device, 不加载 GLFW、切换 context 或拥有 window. 首发每个宿主/context 只装配一个生产 Device, 直接互操作调用方负责相同的独占与寿命前提.

Device 阶段为 Active、Faulted、Stopped. 原生操作先检查线程、阶段和 context 校验, 再检查本地输入/owner 和实际 caps, 最后进入 GL 调用范围. CPU AddRef/Release 与不可变 GetDesc 可跨线程, 原生操作/collection/关闭只能在 owner 线程. Stopped 对象不再调用 context 校验或 GL; 停止后不允许新工作.

DeviceHost 增加 `RHI::Device* GetDevice() const noexcept`, 通过前置声明提供类型, 不在宿主公开头包含 GL 或后端实现. 只在 owner 线程、Ready 时返回 validated Device 借用, 其它情况返回空. 调用方若构造 RefCountPtr 就取得真实额外引用, 必须在 Host 成功关闭前释放. 不增加第二套 device getter 链或把 raw Device 暴露给 Runtime.

**备选:** 由 RHI 再加载一次入口或寻找当前 GLFW 会造成反向依赖; 借用指针没有生命周期条件会让调用者误以为可跨 Host 保存. 两者均不采用.

### 4. 宿主创建、诊断与回滚

创建顺序明确为 GLFW/window -> current -> GLAD -> 原有环境检查 -> raw Device -> Validation -> swap interval -> input -> Ready. HostStage 追加 RhiCreation/RhiShutdown, HostErrorReason 追加 RhiCreation/RhiShutdown/ResourceInUse, 保持旧值和阶段名映射的对应关系. 产品参数、事件发布、主循环和单入口保持原语义.

State 持有 raw DeviceHandle、validated DeviceHandle 和借用 Diagnostics 的 RHI MessageCallback adapter. 成员声明先 adapter、再 raw、最后 validated, 保证隐式析构也不先销毁 callback; 原生关闭仍须显式执行. adapter 使用现有 Core::BoundedText/Diagnostics, Message 实现不抛出; 在 RHI 创建/关闭范围先保存首个有界错误再尝试写日志. 返回值和 nativeError 决定 HostError, 日志失败不覆盖主错误. 普通非法资源请求只记录该请求, 不因 MessageSeverity 自动改变 Host 输入状态或推断 device lost. 不注册第二个 GL debug logger.

验证层在两种构建配置均装配同一个现有 CreateValidationLayer. 不增加运行参数或隐藏的 Release 绕过路径; raw backend 仍执行自己的原生合法性检查. 任一后续阶段失败都先释放未公开 validated -> ShutdownDevice(raw)/raw -> adapter 的使用关系, 再按旧链回滚 input/context/window/GLFW. RHI/nativeError 及原 Host cleanup 次错分别保留.

Host ready 输入/窗口协议的范围仍由 D01 负责, 新 RHI 不接管主循环. 旧 stopAfter、missing-entry 等临时 fixture 随实际阶段扩充, 不在生产 HostDesc 或 CLI 增加注入开关.

### 5. 实际 caps 与初始资源矩阵

Device 私有 caps 仅保存本包正在消费的观察: 纹理/3D/cube 尺寸、array layers、颜色附件/DrawBuffers、sample 的实际集合、sampler anisotropy/bias 范围及下表各 target 的格式性质. 所有查询检查返回/GL 错误和数值可表示性, 失败不继续用 0 或猜测默认值. HostInfo 继续是宿主启动观察的自持快照, 不成为第二套可编辑 RHI 能力策略.

公开 targetless QueryFormatSupport 使用 Texture2D/单样本的已实现用途基线, 创建时还检查实际 target 的单独记录. 不将多种 target 的 flags 做并集. 只把完整支持且本包创建可用的 Texture、RenderTarget、DepthStencil、Blendable、ShaderLoad/ShaderSample 返回为正; 格式层的 shader 标志只说明合法资源解释, 不证明 CreateShader/Draw 已实现. ShaderSample 不被解释为整数纹理可线性过滤. UAV/image/typed-buffer/vertex/index 格式标志本包保持 None, 相关用途后续与真实消费者一同启用.

按 native target/internalformat 查询 INTERNALFORMAT_SUPPORTED、颜色/深度/模板性质、FRAMEBUFFER_RENDERABLE/BLEND、相关纹理阶段可用性和 VIEW_COMPATIBILITY_CLASS, 不能把任意非零枚举或 CAVEAT 当完整支持. multisample 只在适用 target 查询 NUM_SAMPLE_COUNTS/SAMPLES, 与现有 Validation 允许的 {2,4,8} 求交, 不从 MAX_SAMPLES 推导中间值. 参考 [Khronos internal-format 查询](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glGetInternalformat.xhtml) 和 [OpenGL 4.6 Core 第 22.3 节](https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf).

| RHI 格式 | GL sized storage |
|---|---|
| R8_UNORM / R8_UINT / RG8_UNORM | GL_R8 / GL_R8UI / GL_RG8 |
| RGBA8_UNORM / RGBA8_UINT / SRGBA8_UNORM | GL_RGBA8 / GL_RGBA8UI / GL_SRGB8_ALPHA8 |
| R16_FLOAT / RG16_FLOAT / RGBA16_FLOAT | GL_R16F / GL_RG16F / GL_RGBA16F |
| R32_UINT / R32_FLOAT / RG32_FLOAT / RGBA32_FLOAT | GL_R32UI / GL_R32F / GL_RG32F / GL_RGBA32F |
| D16 / D24S8 / D32 / D32S8 | GL_DEPTH_COMPONENT16 / GL_DEPTH24_STENCIL8 / GL_DEPTH_COMPONENT32F / GL_DEPTH32F_STENCIL8 |

此表是 17 项本包映射, 正 flags 仍以实际查询为准; 其它 Format, 包括 BGRA/packed/BC 和 stencil-only 格式, 明确 None/Unsupported, 不以近似格式替换. 色/深度 baseline 的最小创建检查以 RGBA8_UNORM、SRGBA8_UNORM、RGBA16_FLOAT、R32_UINT、D32 为必验; 缺少本包所需基础用途时工厂报告 UnsupportedEnvironment, 不发布缺失基础资源的 Ready Device. 其它映射记录实际 flags, 不要求每个格式在全部维度/用途均支持.

本包所有 Feature 都为 false 且不改 output. ConstantBufferRanges 虽可被既有 Validation 委托, 真实绑定 range 尚未落地, 因而 raw Device 仍为 false; 后续 D03 接入 range 并验证后再启用. 不修改现有 Profile 的 27 项策略或将 GL 4.6 的硬件设施当已实现 RHI 操作.

### 6. Buffer 与 Texture 分配

Buffer 支持普通 vertex/index/constant、structured/raw 存储描述, cpuAccess=None, 非 volatile/virtual/shared/RT, canHaveTypedViews=false 且 format=UNKNOWN. canHaveRawViews 的容量及非零 structured stride 使用相应 4 字节/stride 整除约束; 公共 usage 字段保持, 不冒充对应 draw/dispatch 已可执行. uint64 byteSize 先检查非零和 GLsizeiptr 正范围, 不先窄化. UBO/SSBO 单次绑定范围不等于整个 allocation 的大小限制, 不用未来 binding limits 错拒可分范围的大 buffer.

先构造 wrapper、desc 和删除记录, 再 glCreateBuffers/glNamedBufferStorage, data=null, flags=GL_DYNAMIC_STORAGE_BIT. immutable 指存储布局不可重建, 不意味着内容只读; dynamic-storage 使已规划的 T04 普通内容更新可沿同一对象扩展, 本包仍不调用上传 API. 不映射、不零填、不调用 glFinish. 见 [Khronos buffer storage](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glBufferStorage.xhtml).

Texture 在 descriptor/mip/layer/sample/字节计算可表示且符合 caps 后使用 glCreateTextures 和对应 storage 调用:

| 公开维度 | GL target | 存储参数 |
|---|---|---|
| Texture1D | GL_TEXTURE_1D | TextureStorage1D(width) |
| Texture1DArray | GL_TEXTURE_1D_ARRAY | TextureStorage2D(width, arraySize) |
| Texture2D | GL_TEXTURE_2D | TextureStorage2D(width, height) |
| Texture2DArray | GL_TEXTURE_2D_ARRAY | TextureStorage3D(width, height, arraySize) |
| TextureCube | GL_TEXTURE_CUBE_MAP | TextureStorage2D(width, height), arraySize=6 |
| TextureCubeArray | GL_TEXTURE_CUBE_MAP_ARRAY | TextureStorage3D(width, height, arraySize), faces 为 6 的倍数 |
| Texture3D | GL_TEXTURE_3D | TextureStorage3D(width, height, depth), arraySize=1 |
| Texture2DMS | GL_TEXTURE_2D_MULTISAMPLE | TextureStorage2DMultisample, 仅 1 mip |
| Texture2DMSArray | GL_TEXTURE_2D_MULTISAMPLE_ARRAY | TextureStorage3DMultisample, 层数为 arraySize |

各非数组/非 3D 字段继续遵守公共维度语义, mip 缩小空间尺寸、不缩小 array/cube face 数. sampleQuality=0, 非 MS 的 sampleCount=1; MS 采用 fixedsamplelocations=true 和实际支持集合. sample/mip 不适用的普通参数不能盲目设置到 MS target.

isShaderResource/isRenderTarget 按实际 target 用途检查; isUAV、tiled、virtual、shared、shading-rate 本包拒绝. isTypeless 表示后续合法 view 解释资格, 实际 storage 仍用表中明确 sized 格式, 不分配一个“无格式”GL 对象. defaultComponentMapping 的四通道先合法化到 GL swizzle, 非 MS 资源按公共语义保存/应用; 不对整数/深度格式添加颜色 gamma 或 swizzle 兜底. descriptor 的 initialState/keepInitialState/clearValue 只保存合同, 未经过实际内容操作不得发布 stateInitialized 或填充像素.

### 7. 基础视图与 Framebuffer

本包附件模式为一个 mip、一个 array layer/cube face, 同时支持 1D/2D 和 MS 对应的单层附件; 3D 附件、layered、无附件 FBO、shading-rate attachment 及只读颜色附件明确 Unsupported. 深度附件 isReadOnly 保存在 desc, 供后续 pipeline/write 检查, 不声称 GL FBO 本身提供写保护. 不增加业务可见的 Framebuffer cache.

完整预检顺序: count/非空 -> 实际 OpenGL resource 动态类型 -> m_Device 与当前 Device 相同 -> 全部 range/format/role/caps -> mip extent/sample 一致 -> CPU desc/依赖引用/删除记录准备 -> native 创建. 外来 Texture 在读取其 nativeName 前拒绝. RHI 原有 generic input 仍负责公共语义, backend 只补充实际 GL 合法性和 owner, 不新建平行参数校验服务; raw 路径也必须满足它自身的安全前提.

同格式、单层/单 mip 可直接以 DSA attachment 绑定原 storage; 需要不同合法 format 的 attachment, 才建立该 FBO 自有 view. 重新解释必须 isTypeless=true、同一允许范围且实际 VIEW_COMPATIBILITY_CLASS 兼容, 不只比较字节数. 使用 glGenTextures 预留尚未获得 target 的 view 名, 随后 glTextureView; 不使用已由 glCreateTextures 初始化 target 的名字. view 只覆盖请求的 mip/layer, 以其 level 0 附件引用. 这是 [Khronos texture-view 前提](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glTextureView.xhtml), 不可由“全部 DSA”外观覆盖.

不建通用视图缓存: FBO 最多拥有 MaxRenderTargets+1 个必要 view, 删除和引用都随该 owner. typed 同格式避免无意义 view. 基础 FB 使用 glCreateFramebuffers、glNamedFramebufferTexture/Layer 和 named draw/read-buffer 设置; depth-only 明确 draw/read 为 NONE. 最后 glCheckNamedFramebufferStatus 为 COMPLETE 才发布. 整个过程不改变外部 read/draw FBO binding, 失败即撤销未发布的 FBO/views. FramebufferInfoEx 根据实际有效 desc 形成, 不复制另一份可编辑附件事实.

array/cube 的单层 reinterpret view 使用实际 1D/2D/MS view target 的支持和 view class, 与 parent storage target 分别核对, 不从 array target 的支持推断该 view. 初始状态沿用通用 GL profile 的普通状态位, 包括 StreamOut/Present 的 metadata; 与所有其它 initial-state/clear 字段一样, 保存它不启用相应执行操作或发布初始化结果.

现有 ObjectTypes 只有 D3D/Vulkan 协议标识, 本包不为测试追加 GL GLuint 公共导出或改写 Resource.hpp. GetNativeObject/GetNativeView 对未支持标识返回空. 附件所需 views 在后端内部真实实现; fixture 通过私有检查入口只读取得观测对象, 该入口不成为 Runtime/Renderer 原生逃生接口. 未来 binding 所需 SRV/UAV views 由真实消费者扩充.

### 8. Sampler 与坐标边界

独立 glCreateSamplers/glSamplerParameter* 保留 minFilter/magFilter/mipFilter 的布尔组合, 对应 min 的四种 mip 过滤、mag 的 nearest/linear. 五种 address mode 映射到 CLAMP_TO_EDGE/REPEAT/CLAMP_TO_BORDER/MIRRORED_REPEAT/MIRROR_CLAMP_TO_EDGE; borderColor 四通道必须有限. mipBias 和 maxAnisotropy 检查实际 GL_MAX_TEXTURE_LOD_BIAS/GL_MAX_TEXTURE_MAX_ANISOTROPY, 不依赖 GL 自动钳制.

Standard 设置 compare mode NONE. Comparison 设置 COMPARE_REF_TO_TEXTURE 和 GL_LESS, 与 [固定 Vulkan 来源](../../../../ThirdParty/nvrhi/src/vulkan/vulkan-texture.cpp) 的 compareOp=eLess、Architecture 的默认 ShadowDepthConvention 一致. 主视图 Reverse-Z 不改变 sampler 的 shadow 比较合同. Minimum/Maximum 依赖本包未启用的 reduction 路径, 明确拒绝而非使用 Standard. 参数含义依据 [Khronos sampler 参数](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glSamplerParameter.xhtml).

工厂设置并查询 glClipControl(GL_UPPER_LEFT, GL_ZERO_TO_ONE), 不在 Texture 或每 Pass 做额外翻转. [clip-control 的 front-facing 规则](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glClipControl.xhtml) 保持后端适配责任; 本包没有 RasterState/Pipeline 的实际消费者, 不用该查询宣称绕序或 Reverse-Z 图像已验收. D03-T03 随真正 raster 状态实现按公共目标空间语义映射并以非对称图检查, T05/D03 负责内容行序与输出.

### 9. CPU 引用、无分配退休和 native 错误

各资源复用现有 RefCounter<接口>, 保存 desc、raw DeviceHandle 和创建前预分配的删除记录. 不修改公共引用计数语义. FBO 另持有纹理强引用与其必要 view; Device 不强持有所有活资源, 不形成正常活资源循环. 公开 desc 借用仍止于资源最后引用, GetDesc 的数据不被逐帧重建.

删除记录自持 native 名称、类型和固定上限的附属 view, 采用 Device 私有 mutex + intrusive FIFO 连接, 记录节点已在创建成功前取得. 最终 Release 的析构只转移该记录, 不分配、不调用 GL、不等待 GPU; 然后释放自身的 Device CPU 引用. Device 根 owner 必须保持到全部资源释放且删除队列排空, Host 通过下节协议保证; 原生工厂互操作调用方也遵守此项前提. 队列只拥有记录, 不反向持有 DeviceHandle, 不形成 queue/Device 循环.

FBO 析构先将 FBO/views 记录入队, 再清除 texture handles, 保证由它触发的纹理记录在 FIFO 后面. Owner 的 RunGarbageCollection 先在锁内摘取已退休链, 再在锁外按序删除; 附属 view/FBO 在 parent storage 之前删除, 新产生记录继续排空. 不持锁调用用户 callback 或 GL. 同步设施自身失败属于既有不可恢复合同路径, 不从 noexcept 析构抛出或靠丢记录继续运行.

队列容量来自已经成功创建、已预留节点的实际资源数, 没有后台自动增长/预算缓存. 排空和成功关闭后节点全部释放. 没有 ContextState/native binding cache 消费者时不预建 cache; 原生名称重用不得产生视图复用误认. D03 以后以同一个 Device 收集在途引用, 真实完成前不会到达最终 Release, 不平行增加另一删除系统.

普通 GL 操作在 CPU/owner/caps 预检后才检查首个既存 GL error, 有错误则拒绝本次操作而不无声清空后继续. 每个 native 创建/配置阶段检查实际错误和对象状态, 保存首错, 未发布对象由当前 context 的 RAII 撤销. CPU bad_alloc、native name/存储失败与 FBO incomplete 分别可观察; error 输出与 callback 不替换原主错误.

遇到 native 状态不能可靠继续时 Device 进入 Faulted, 停止新创建. 是否可执行 GL cleanup 依据 context 校验和实际 reset/原生结果, 不由日志级别或 GL_OUT_OF_MEMORY 字符串推断 device lost. 当前没有内容/提交工作, 不发布完成 ID 或调用 WaitForIdle 伪收拢; context 已失效时停止 GL 调用、撤销 CPU owner, 原生对象随其所属 context 的最终销毁退出. 未证实的 GPU 丢失/驱动 OOM 不列为本机已覆盖.

**备选:** final Release 分配队列节点会使 OOM 退出泄漏; worker 直接 glDelete 违反 context 合同; intrusive lock-free 队列增加无必要的内存回收证明. 采用预分配记录与普通窄锁, 不引入无消费者的资源池或按固定帧延迟删除.

### 10. 宿主关闭预检和可重试拒绝

调用方先停止资源使用者、join 已有 worker、释放所有外部资源及 owning Device handles, 再在 owner 线程调用 Shutdown. 借用的 Device 及 callback 不能越过成功 Host 关闭; 这种跨关闭借用没有伪有效保证.

State 的根引用固定为 raw DeviceHandle 1 个、validated DeviceHandle 1 个; Validation 另持有 raw 引用. 因此在无外部 owner、无活资源时 validated.GetRefCount()==1, raw.GetRefCount()==2. 原子 GetRefCount 仅在调用方已收拢使用者的关闭边界用于验证此已知拥有关系, 不是常规业务的引用计数猜测或并发退出协议. 队列不持 Device 引用, 已退休对象不影响此基线.

预检必须早于 state.ready=false、输入解绑和任何 RHI/context teardown. 多余引用或活资源时返回 HostErrorReason::ResourceInUse, adapter/context/window/输入/Ready 和原有状态保持可用, 让调用方释放后重试. 不把这一可恢复拒绝锁存为输入故障. raw ShutdownDevice 在 validated 已释放后要求 GetRefCount()==1 的唯一根引用, 额外 device/resource 引用使其拒绝且不先转 Stopped; 不另维护一份活资源计数或通过全局 registry 搜集 owner.

预检通过后, 确认或恢复所属 window 的 current context, 排空退休节点, 停止 raw 新工作, 先 Reset validated, 再 ShutdownDevice(raw, Normal)/Reset raw, 最后撤销 RHI adapter 借用. 随后沿既有链解除输入/回调、detach/destroy window/context、terminate GLFW 和释放进程 claim. raw 的关闭幂等, 后续旧 Host Shutdown 不影响新 Host. RHI/native cleanup 次错与旧清理错误合并而不覆盖主错.

实际 reset/CONTEXT_LOST 表明失效, 或 Host 恢复所属 current 失败且必须最终释放该 context 时, 记录原始失败后采用 ShutdownDevice(raw, ContextUnavailable), 无后续 GL 地撤销 CPU 退休状态, 再释放所属 native 环境. 该模式不推断硬件已经丢失, 只表达退出时原生调用不可用. 不尝试等待不存在的票据, 也不对已知失效 context 继续 glDelete.

关闭返回值表达本次是否无错: 线程/current/额外引用预检失败时不改变阶段, 可以修正后重试; 真正开始清理后即使 native 删除出错, 也保留首错、丢弃剩余 CPU 记录并进入 Stopped, 遗留原生对象由即将销毁的所属 context 退出. Host 不因 false 就把已停止设备当活跃对象使用, 或在释放 raw 后再调用它. 显式 ResourceInUse 保留 owner 供重试; 若调用方直接销毁仍有外部 RHI owner 的 DeviceHost, 属于破坏必要寿命的合同违例, 析构沿已有 TerminateForContract 处理, 不能忽略 Shutdown 错误后释放 State. fixture 在隔离子进程验证此路径, 不使测试进程或产品假成功.

### 11. 完整接口与后续能力

实现前逐项核对公共 Device 的全部 68 方法, 形成实现/拒绝矩阵, 包括 deprecated overload 和扩展. 本包正向路径只有基础创建、只读 desc/格式/Feature/API/callback、删除 collection 和 backend admin shutdown; GetNativeObject/Queue 对未支持类型返回空. Map/Unmap、staging、memory binding、Shader/Library/InputLayout/Binding/Pipeline、CommandList、queries、WaitForIdle、提交/队列/RT/Mesh/feedback/CoopVec 均按现有结果形式显式 Unsupported, 在读取无关 payload 前拒绝.

可合法空 batch 的 ExecuteCommandLists 保持 0, 非空 batch 明确拒绝, 不创建列表占位或票据. QueryFeatureSupport 全 false/output 不变. 具有数值/结构输出的不可用扩展保留文档规定的失败值且报告原因, 不写 out 参数; Aftermath 引用 getter 报告后 logic_error. RunGarbageCollection 真正排空删除记录, 不把其它未实现 void 方法当成功空操作. T04/D03 未来直接扩展这些已有对象和同一 Device 生命周期.

### 12. 验证与文档证据

本次规划的检查仅证明产物完整、语义一致、链接正确和未越界编辑; 不运行或标记未来 native 任务. 实施期逐项配套下列证据, 命令/工具链/driver/构建和原始输出进入 tasks 及 ignored build/artifacts/establish-opengl-device-resources/.

- 独立 generic 消费者: 新 binary、六个公共头/私有 Common 首 include、既有 CPU/负例与完整 token/许可对照, 不加 glad/Platform. 用 CMake File API/实际命令确认 generic link 闭包未变.
- Native 消费者: 正式 Host/PhysaraRHIOpenGL、Debug/Release, 可见和隐藏创建, 17 格式的全部查询分支与九种 Texture 维度. 以 glGetNamedBufferParameteri64v、glGetTextureLevelParameteriv、glGetSamplerParameter*、named framebuffer 查询检查真实对象; 不读未初始化像素.
- caps/拒绝: 映射/未映射/未知格式, target/usage、每格式 MS set 的存在与缺失、signed-size/extent/layer/mip/stride 临界, invalid sampler, 单层 FB、typeless SRGB view、incomplete 与后项无效的整批附件零创建. 实际支持的非最高样本值也要选择, 不只测 MAX.
- 归属: 同一个正式 Host/context 为主 fixture. 另用 fixture 自己严格 RAII 管理的同 driver/profile 辅助 context 与真实 Device 生成 foreign resource, 恢复主 current 后检查拒绝; 它只用于负例, 不替代产品 Host. 辅助 current 下先通过 glfwGetProcAddress 逐项比对本包 required 表与已有 GLAD 地址, 相同才使用同一入口; 不符则该环境未满足本负例, 不重新装载/更换全局 GLAD 蒙混. 恢复主 current 后再确认入口和设备仍可用, 不用 CPU 假纹理冒充跨设备 native 证据.
- 寿命: 外部 Texture 释放后 FB 仍有效; 各资源 worker final Release 零 GL/零新分配, owner collection 只删一次; FBO/views/texture 顺序、重复分配释放和实际 name reuse 不用缓存误判. 多余 validated/raw 引用和 live resource 都触发 ResourceInUse, owner 收拢释放后重试关闭成功.
- 失败: CPU 准备各点、取得 native 名/存储/视图/FBO 后失败、Validation composition、RHI/旧 input 后阶段失败/cleanup 复合失败, 均以检查构建的临时 hook/观测覆盖. 真实 API 查询和受控返回注入分开记录, 不在生产 CLI 添加测试开关; 不真耗尽 GPU 内存证明错误分支.
- 回归: 全产品干净 Debug/Release, help/unknown/hidden 精确 0/2/0, Core/输入/宿主的现有 lifecycle/失焦/文本/关闭/故障检查, 当前来源/风格/内部链接, native-window-host 全旧场景和 D01 已采纳合同继续成立. 新 RHI 输入/关闭前置随新 owner 加入同一路径.
- 工具检查: 原生 status/apply readiness、strict、Markdown 相对链接/锚点和 git diff --check. Code Review 回答 owner、跨边界数据、队列退出、失败主错和 GPU 何时可能访问; CPU 注入与 native 观察分别界定.

## Risks / Trade-offs

- [D01 环境仍缺] -> 本轮明确例外仅覆盖 T03 资源闭环, 其余入口和 D01 跨屏验收保留; 不用资源 native 结果替代跨屏证据.
- [最小格式/用途与全部公开枚举不同] -> 17 映射和 resource-only flags 明列, 其它返回不支持, 不缩减公共声明; 后续扩展须有对应 native 证据.
- [generic/native 静态目标混合泄漏 loader] -> 两个真实消费目标, generic 独立配置/链接与 File API 复核.
- [显式关闭开始拒绝活 RHI owner] -> 用现有 HostError 增补可恢复 ResourceInUse, 所有用户先释放再关闭; 析构违例 fail-fast, 不留下悬空 callback/context.
- [退休记录和引用在 OOM/worker 路径丢失] -> 创建前预留记录、窄锁 FIFO、根 owner 保留到排空, 故障下验证零新分配与精确一次删除.
- [GL 查询与 driver 支持有 caveat] -> 仅启用当前完整支持交集, 失败不默认成功; 记录实际配置, 不从另一 API 或另一 driver 推导本机能力.
- [辅助 context 或输入回归受实际环境限制] -> 与主资源用例分开记录, 缺证据的任务不完成; 不伪造跨设备或跨屏结果.

## Migration Plan

1. 核对本轮限定授权已同步所有当前规划及 Developments, D01 未完成事实和例外退出条件仍明确, 再保存实现输入并推进本包.
2. 建立独立 backend target、factory/admin 合同和资源创建/退出必需的 CPU owner; 随每个资源交付其实际 native 正反与失败回归.
3. 在已有 Host 创建/关闭链装配 raw/Validation 和 adapter, 运行新关闭拒绝及原有输入/进程场景, 更新实际 Build/RHI 说明.
4. 全部本包行为及 Code Review 通过后才将 Developments T03 标完成; T04/T05 和 D02 出口维持实际未完成范围, Architecture 不累加实施状态. 同步/归档按原生流程另行执行, 不自动提交 Git.
