# Filament Windows OpenGL RHI 与 CPU/GPU 渲染管线协同机制总结

> 观察范围：本文聚焦 Filament 仓库中 OpenGL 后端，尤其是 Windows 平台 WGL 接入层，以及 Filament 应用程序侧（CPU 端）如何组织场景、材质、描述符、命令和帧图，并与 GPU 侧着色器/缓冲/纹理布局达成约定。
>
> 主要源码入口：`filament/backend/src/opengl/*`、`filament/backend/src/opengl/platforms/PlatformWGL.cpp`、`filament/backend/include/private/backend/*`、`filament/src/details/*`、`filament/src/RenderPass.cpp`、`filament/src/fg/*`、`libs/filabridge/include/private/filament/*`。

---

## 1. 总体分层：Filament 的 RHI / Backend 抽象

Filament 的渲染后端不是让上层直接调用 GL/Vulkan/Metal，而是通过一套 backend Driver API 将应用侧的资源创建、状态绑定、绘制命令录制成命令流，再由后端线程或提交点执行。整体可以理解为：

```text
应用 API 层（Engine / Renderer / View / Scene / Material / Renderable）
        |
        v
Filament 内部数据层（FEngine、FView、FScene、FRenderableManager、FMaterialInstance）
        |
        v
FrameGraph + RenderPass（组织 pass、资源生命周期、排序后的 draw command）
        |
        v
backend::DriverApi / CommandStream（CPU 侧写入命令流）
        |
        v
OpenGLDriver（解释命令为 OpenGL 状态与 draw call）
        |
        v
OpenGLPlatform / PlatformWGL（窗口系统、GL context、swapchain、present）
        |
        v
GPU（GL program、buffer、texture、FBO、sampler、descriptor 约定）
```

核心特点：

1. **API 与图形后端解耦**：上层只关心 `Engine`、`Renderer`、`View`、`MaterialInstance` 等对象；后端通过 `DriverApi` 接收统一命令。
2. **资源以 Handle 间接引用**：CPU 上层持有抽象 handle，OpenGL 后端在 handle arena 中构造 `GLTexture`、`GLBufferObject`、`GLDescriptorSet` 等具体对象。
3. **命令流解耦应用线程与后端执行**：多数 Driver API 调用并不立即执行 GL，而是 placement-new 成命令，放入 `CircularBuffer`；同步查询才直接调用后端。
4. **FrameGraph 管资源生命周期**：渲染 pass 声明读写资源，FrameGraph 编译后裁剪无用 pass、计算资源 first/last user，并在执行时创建/销毁实际 GPU 资源。
5. **CPU/GPU 数据布局由 filabridge 共享头约定**：`UibStructs.h`、`SibStructs.h`、`EngineEnums.h` 同时服务 CPU 填充和 shader 生成，保证 binding point、UBO std140 布局和 shader 侧一致。

---

## 2. Windows 平台 OpenGL RHI：PlatformWGL 的组织方式

### 2.1 PlatformWGL 的职责

Windows 上 OpenGL 后端的平台层位于：

- `filament/backend/include/backend/platforms/PlatformWGL.h`
- `filament/backend/src/opengl/platforms/PlatformWGL.cpp`

它继承 `OpenGLPlatform`，负责所有与 WGL / Win32 窗口系统相关的事项：

1. 创建主 OpenGL context。
2. 创建用于获取 WGL 扩展函数的临时 context。
3. 选择并设置像素格式 `PIXELFORMATDESCRIPTOR`。
4. 加载 GL 函数指针（BlueGL）。
5. 创建额外共享 context，供其他线程使用。
6. 将 Filament 的 `SwapChain` 映射到 Windows 的 `HWND + HDC`。
7. 实现 `makeCurrent` 和 `commit`，分别对应 `wglMakeCurrent` 和 `SwapBuffers`。

### 2.2 Context 创建流程

`PlatformWGL::createDriver` 的关键流程如下：

1. 初始化 `PIXELFORMATDESCRIPTOR`：
   - `PFD_DRAW_TO_WINDOW`
   - `PFD_SUPPORT_OPENGL`
   - `PFD_DOUBLEBUFFER`
   - RGBA 32-bit color
   - 24-bit depth
2. 创建一个 1x1 的 dummy `STATIC` 窗口。
3. 对 dummy 窗口的 `HDC` 调用 `ChoosePixelFormat` / `SetPixelFormat`。
4. 用 legacy `wglCreateContext` 创建临时 context，并 `wglMakeCurrent`。
5. 通过 `wglGetProcAddress("wglCreateContextAttribsARB")` 获取现代 context 创建函数。
6. 尝试创建 OpenGL 4.5 到 4.1 的 context：
   - major 固定为 4
   - minor 从 5 递减到 1
7. 删除临时 context，切换到正式 context。
8. 调用 `bluegl::bind()` 加载 OpenGL entry points。
9. 调用 `OpenGLPlatform::createDefaultDriver` 创建 `OpenGLDriver`。

这套设计的关键点是：Windows 下必须先有一个当前 context，才能拿到 `wglCreateContextAttribsARB`，所以必须先创建临时 context。

### 2.3 Windows 共享 context 的特殊处理

`PlatformWGL` 中有一个固定数量的共享 context 池：

```cpp
static constexpr int SHARED_CONTEXT_NUM = 2;
std::vector<HGLRC> mAdditionalContexts;
std::atomic<int> mNextFreeSharedContextIndex{0};
```

它在主 context 创建成功后立即创建额外共享 context。源码注释明确说明：这是 Windows 特定 workaround，因为共享 context 必须在与 primary context 同一线程上初始化。之后 `createContext(bool shared)` 不是临时创建，而是从池中取一个已经创建好的 context 并 `wglMakeCurrent`。

技术意义：

- 避免跨线程懒创建共享 context 时踩到 Windows/WGL 约束。
- 允许后端的额外线程在需要时拥有共享 GL namespace。
- 通过 `mAdditionalContextsLock` 保护共享 context vector。

### 2.4 SwapChain 映射

`PlatformWGL` 定义内部结构：

```cpp
struct WGLSwapChain {
    HDC hDc = NULL;
    HWND hWnd = NULL;
    bool isHeadless = false;
};
```

普通窗口 swapchain：

1. `nativeWindow` 被解释为 `HWND`。
2. 调用 `GetDC(HWND)` 得到 `HDC`。
3. 对这个 `HDC` 使用与主 context 相同的 pixel format。
4. 保存到 `WGLSwapChain`。

Headless swapchain：

1. 创建一个 `WS_POPUP` 隐藏窗口。
2. 调整窗口大小以匹配目标 client rect。
3. 获取 `HDC` 并设置 pixel format。

`makeCurrent` 要求 draw/read swapchain 相同，因为 WGL 版本不支持 distinct draw/read swap chain。`commit` 则直接对当前 `HDC` 调用 `SwapBuffers`。

### 2.5 PlatformWGL 的限制和约束

1. **不支持独立 draw/read swapchain**：`makeCurrent` 中要求 `drawSwapChain == readSwapChain`。
2. **像素格式必须匹配**：Windows 的 HGLRC 与 HDC pixel format 必须兼容，创建 swapchain 时复用 `mPfd`。
3. **present 简单直接**：`commit` 只有 `SwapBuffers`，没有像 Vulkan/Metal 那样的显式 present queue。
4. **OpenGL 版本目标是桌面 GL 4.1+**：从 4.5 降到 4.1 尝试创建 context。
5. **GL 函数动态加载**：使用 BlueGL，在 context current 之后 bind entry points。

---

## 3. OpenGL 后端的源码组织

OpenGL 后端主要文件及职责：

| 文件 / 模块 | 职责 |
|---|---|
| `OpenGLDriver.h/.cpp` | 后端核心，接收 Driver 命令，创建 GL 资源，绑定 pipeline/descriptor，发出 draw/dispatch/readback。 |
| `OpenGLDriverBase.h` | OpenGLDriver 的基础抽象与公共状态。 |
| `OpenGLContext.h/.cpp` | 查询 GL/GLES 版本、扩展、限制、bug workaround、feature level，管理 per-thread `OpenGLState`。 |
| `OpenGLState.h/.cpp` | GL 状态缓存层，封装 enable/disable、buffer/texture/FBO/VAO/sampler/program 等状态，减少重复 GL 调用。 |
| `OpenGLProgram.h/.cpp` | shader program 编译、link、use、uniform/descriptor 映射、program binary/blob cache。 |
| `GLTexture.h` | OpenGL texture 对象结构，包括 target、internalFormat、levels、external texture 等。 |
| `GLBufferObject.h/.cpp` | OpenGL buffer 对象结构与缓冲更新辅助。 |
| `GLDescriptorSet.h/.cpp` | 将 Filament descriptor set 抽象映射到 GL UBO/SSBO/texture/sampler binding。 |
| `GLDescriptorSetLayout.h` | descriptor set layout 的 GL 侧元信息。 |
| `GLMemoryMappedBuffer.h/.cpp` | readback / mapped buffer 相关路径。 |
| `OpenGLTimerQuery.h/.cpp` | GPU timer query。 |
| `ShaderCompilerService.h/.cpp` | shader 编译服务、并行编译/延迟 link 支持。 |
| `OpenGLBlobCache.h/.cpp` | program binary / shader cache。 |
| `GLUtils.h/.cpp` | 格式、枚举、工具函数转换。 |
| `BindingMap.h` | 绑定点映射辅助。 |
| `gl_headers.h/.cpp` | GL/GLES/WebGL 平台头与 BlueGL 接入。 |
| `platforms/PlatformWGL.cpp` | Windows WGL 平台实现。 |
| `platforms/PlatformEGL.cpp`、`PlatformGLX.cpp`、`PlatformCocoaGL.mm` 等 | 其他平台 OpenGL context / swapchain 实现。 |

---

## 4. OpenGLDriver 的关键实现机制

### 4.1 Driver 创建与版本检查

OpenGLDriver 创建时先查询 GL 版本，确保满足后端最低要求。GLES 路径有 ES2 / ES3 feature level 分支；桌面 Windows WGL 路径通常是 GL 4.x。创建后会根据 context 能力决定 shader model、feature level、扩展支持、bug workaround、是否支持 GPU timer、并行 shader compile 等。

### 4.2 Dispatcher：命令流到具体后端函数的跳转表

`OpenGLDriver::getDispatcher()` 生成 `ConcreteDispatcher<OpenGLDriver>`。特殊点：如果 context 是 ES2，会把 `draw2` 的 dispatcher 替换为 `draw2GLES2`。这说明 Driver API 仍然统一叫 `draw2`，但后端可以根据 feature level 替换实现。

### 4.3 Handle 与资源对象

OpenGLDriver 内部定义一组 `GL*` 结构继承 `Hw*` 抽象：

- `GLSwapChain`
- `GLVertexBufferInfo`
- `GLVertexBuffer`
- `GLIndexBuffer`
- `GLRenderPrimitive`
- `GLBufferObject`
- `GLTexture`
- `GLRenderTarget`
- `GLDescriptorSetLayout`
- `GLDescriptorSet`
- `GLMemoryMappedBuffer`
- `OpenGLProgram`

创建资源通常分为两阶段：

1. `createXXXS()`：同步返回 handle，分配一个 `Hw*` handle id。
2. `createXXXR(handle, ...)`：命令流执行时在 handle 对应内存中构造具体 `GL*` 对象，并调用 GL 创建函数。

例子：创建 `BufferObject` 时，`createBufferObjectCommon` 会：

- 对 vertex buffer binding 先解绑 VAO，避免 VAO 捕获错误状态。
- ES2 的 uniform buffer 走 emulated UBO：分配 CPU 内存并给一个虚拟 id。
- 正常路径调用 `glGenBuffers`、`glBindBuffer`、`glBufferData`。

这种 S/R 分离可以让应用线程立即得到 handle，同时把真实 GL 创建放到后端命令执行阶段。

### 4.4 Buffer 更新策略

Filament 有多种 buffer 更新 API：

- `updateBufferObject`
- `updateBufferObjectAsync`
- `updateBufferObjectUnsynchronized`
- `resetBufferObject`
- `copyToMemoryMappedBuffer`

技术要点：

1. **数据所有权通过 `BufferDescriptor` 移动到命令流**：避免上层数据生命周期短于后端执行。
2. **异步接口返回 `AsyncCallId`**：允许后端排队上传并在完成后回调。
3. **ES2 UBO emulation**：老 GLES2 没有 UBO，所以 uniform 数据可在 CPU 侧模拟并在 program use / draw 前设置。
4. **Unsynchronized update**：用于调用者知道不会读写冲突的路径，减少同步开销。

### 4.5 Texture 创建与 storage

`textureStorage` 统一处理不同 GL texture target：

- `GL_TEXTURE_2D`
- `GL_TEXTURE_CUBE_MAP`
- `GL_TEXTURE_3D`
- `GL_TEXTURE_2D_ARRAY`
- `GL_TEXTURE_CUBE_MAP_ARRAY`
- multisample texture（受 feature gate 控制）

现代 GL/GLES3 使用 `glTexStorage2D/3D` 创建 immutable storage；ES2 回退到逐 mip level 的 `glTexImage2D`。

技术意义：

- immutable storage 有利于驱动优化和错误检查。
- ES2 回退保证 feature level 0 仍能运行。
- external image / stream texture 通过平台层接入 native image 或视频流。

### 4.6 Program 与 shader 编译

`OpenGLProgram` 负责封装 GL program。`OpenGLDriver::createProgramR` 在 handle 上构造 `OpenGLProgram`。实际 `useProgram` 时可能触发 compile/link，并可能阻塞到 link 完成。

技术点：

1. `ShaderCompilerService` 可 tick 和后台/分阶段处理编译任务。
2. `OpenGLBlobCache` 支持 program binary 或 shader blob cache，降低后续启动/编译成本。
3. `bindPipeline` 中调用 `useProgram`，program 成功切换后会标记所有 descriptor set binding 失效，因为不同 program 的 uniform block / sampler location 可能不同。

### 4.7 OpenGLState：状态缓存和去重

OpenGL 是隐式全局状态机，Filament 用 `OpenGLState` 做状态缓存：

- 当前 program
- 当前 FBO
- 当前 VAO / render primitive
- raster state：cull、blend、depth、color mask
- stencil state
- texture unit 与 texture binding
- sampler cache
- viewport / scissor / depth range
- buffer binding

`OpenGLDriver` 尽量不直接调用裸 `glEnable/glBind*`，而是走 `OpenGLState` 的封装，以减少重复状态变更。

典型例子：

- `setRasterState` 根据 `RasterState` 设置 culling、front face、blend equation/function、depth test/write、color mask、alpha-to-coverage、depth clamp。
- `bindRenderPrimitive` 通过 `gl.bindVertexArray(&rp->gl)` 和 `updateVertexArrayObject` 维护 VAO。
- context 切换时调用 `unbindEverything()`，强制清空缓存，避免缓存与真实 GL 状态不一致。

### 4.8 PipelineState 的 GL 映射

Filament 的 `PipelineState` 包含：

- raster state
- stencil state
- polygon offset
- program handle
- primitive type
- vertex buffer info
- pipeline layout（descriptor set layout handles）

`OpenGLDriver::bindPipeline` 会：

1. 设置 raster state。
2. 设置 stencil state。
3. 设置 polygon offset。
4. `useProgram`。
5. 取 program 的 push constants 信息。
6. 保存当前 pipeline layout 的 descriptor set layout。

OpenGL 没有 Vulkan 那样的 pipeline object，Filament 在 GL 后端把 pipeline 拆成即时状态设置 + program bind + layout 校验。

### 4.9 DescriptorSet 在 OpenGL 中的模拟

Filament 内部已经采用类似 Vulkan/Metal 的 descriptor set 抽象，但 OpenGL 没有原生 descriptor set。因此 `GLDescriptorSet` 做了映射：

- buffer descriptor：映射到 UBO/SSBO binding point。
- dynamic buffer：保存 base offset，draw 前结合 dynamic offset。
- texture sampler：映射到 texture unit + sampler object。
- ES2：用特殊 `BufferGLES2`、`SamplerGLES2` 路径模拟。

`bindDescriptorSet` 不一定立即 GL bind，它主要更新当前 bound descriptor set 状态，并标记失效。真正 draw 前，`draw2/drawArrays` 检查 `mInvalidDescriptorSetBindings | mInvalidDescriptorSetBindingOffsets`，调用 `updateDescriptors`，由 descriptor set 根据当前 program 绑定实际 UBO/texture/sampler。

这样做的好处：

1. **惰性绑定**：descriptor 多次变更但没有 draw，不会产生 GL 调用。
2. **program-aware binding**：不同 shader 的 binding 需求可在 draw 前与 program 对齐。
3. **offset-only 快路径**：仅 dynamic offset 改变时可避免完整 layout 校验。
4. **跨后端统一**：上层和材质系统以 descriptor set 思维组织资源，GL 只是后端适配。

### 4.10 RenderTarget / RenderPass / FBO

`beginRenderPass` 的主要工作：

1. tick shader compiler。
2. 保存当前 render target 和 render pass params。
3. 绑定目标 FBO。
4. 对 default render target 从当前 swapchain 得到附件集合。
5. 根据 pass flags 处理 clear / discardStart。
6. MSAA render target 可能涉及 resolve load/store。
7. 设置 viewport 和 depth range。
8. Debug 构建下用特殊颜色清理 discarded buffer，帮助发现错误依赖。

`endRenderPass` 的主要工作：

1. 如果有 read FBO / MSAA resolve，执行 resolve store。
2. 根据实际是否写过 color/depth/stencil 修正 discardEnd。
3. 对 default framebuffer 考虑 platform preserved flags。
4. 如果支持 `EXT_discard_framebuffer`，调用 invalidate framebuffer。
5. 清空当前 render pass target。

OpenGL 后端借助 render pass 的 clear/discard 语义，尽量给 tile-based GPU 或驱动提供 load/store 优化机会，即使 OpenGL 本身没有 Vulkan/Metal 那样明确的 render pass load/store 模型。

### 4.11 Draw 调用路径

典型 draw path：

1. `RenderPass::Executor` 根据排序后的 command 决定是否需要切 material、pipeline、primitive。
2. 绑定 per-view descriptor set。
3. `MaterialInstance::use` 绑定 per-material descriptor set。
4. 绑定 per-renderable descriptor set，并传入 dynamic offsets。
5. 如有 morphing，设置 push constant。
6. 根据 primitive 类型调用：
   - indexed：`driver.draw2(indexOffset, indexCount, instanceCount)`
   - non-indexed：`driver.drawArrays(vertexOffset, vertexCount, instanceCount)`
7. OpenGLDriver draw 前更新失效 descriptors。
8. GL 后端调用：
   - `glDrawElementsInstanced`
   - 或 `glDrawArraysInstanced`

### 4.12 Swapchain 与 frame 提交

Renderer begin/end frame 与 OpenGL swapchain 的关系：

1. `FRenderer::beginFrame`：
   - 保存 frame id 和时间。
   - `swapChain->makeCurrent(driver)`，最终进入 OpenGLDriver / PlatformWGL 的 `makeCurrent`。
   - 更新 streams。
   - `driver.tick()`。
   - 如果决定渲染，则调用 `driver.beginFrame` 和 `engine.prepare(driver)`。
2. `FRenderer::render`：执行 view 的 render job，构建并执行 FrameGraph。
3. `FRenderer::endFrame`：
   - `mSwapChain->commit(driver)`，最终到 PlatformWGL 的 `SwapBuffers`。
   - `engine.submitFrame()`。
   - `driver.endFrame(frameId)`。
   - 再次 `driver.tick()`。

### 4.13 Windows OpenGL 后端的实际 present

在 WGL 上：

```text
FRenderer::endFrame
  -> FSwapChain::commit(driver)
  -> DriverApi::commit
  -> OpenGLDriver::commit
  -> PlatformWGL::commit
  -> SwapBuffers(HDC)
```

这说明 Filament 的 OpenGL/WGL present 是直接双缓冲交换，frame pacing / callback / timestamp 能力取决于平台层是否额外实现相关接口。

---

## 5. CPU 端应用程序侧的数据组织

### 5.1 FEngine：资源和管理器的总入口

`FEngine` 是 `Engine` 的具体实现，保存一个 context 下几乎所有全局资源与管理器：

- DriverApi / CommandStream
- CommandBufferQueue
- JobSystem
- EntityManager
- TransformManager
- RenderableManager
- LightManager
- CameraManager
- MaterialCache
- PostProcessManager
- UboManager
- 默认材质、默认 IBL、dummy texture、fullscreen triangle 等
- ResourceList：管理 BufferObject、Texture、Material、View、Renderer、Scene 等对象生命周期

它也提供对象创建/销毁入口：

- `createVertexBuffer`
- `createIndexBuffer`
- `createTexture`
- `createMaterial`
- `createMaterialInstance`
- `createRenderable`
- `createLight`
- `createRenderer`
- `createView`
- `createSwapChain`

### 5.2 Entity + Component Manager

Filament 采用 ECS 风格：

- `utils::Entity` 是对象身份。
- `TransformManager` 管层级变换。
- `RenderableManager` 管 mesh / primitive / material / bounding box / visibility / skinning / morphing。
- `LightManager` 管光源。
- `CameraManager` 管相机组件。
- `Scene` 只引用 entity 集合，并在 prepare 阶段生成可见性和排序所需的缓存。

这种设计把数据从 API 对象拆到 SoA / component storage 中，有利于 CPU 侧批处理、可见性计算和缓存友好遍历。

### 5.3 Scene / View / Renderer 的关系

- `Scene`：包含哪些实体参与渲染。
- `View`：定义如何看 scene，包括 camera、viewport、render target、后处理、抗锯齿、阴影、AO、SSR、fog、动态分辨率等。
- `Renderer`：驱动帧循环，beginFrame/render/endFrame。

一个 Renderer 可以渲染多个 View；每个 View 在 render 时生成自己的 FrameGraph 和 pass 集合。

### 5.4 FView prepare：把场景转成 GPU 可消费数据

`FView` 的 prepare 阶段做大量 CPU 工作：

1. 相机矩阵、投影、视口、时间、曝光等写入 per-view uniforms。
2. Scene 可见性计算，筛选可见 renderables。
3. Light 可见性和 froxelization 准备。
4. 阴影 caster 分区：方向光阴影、点/聚光阴影等。
5. `scene->prepareVisibleRenderables` 生成排序/绘制需要的 renderable 数据。
6. `updateUBOs` 更新 per-renderable UBO。
7. 设置 common per-renderable descriptor set。
8. skinning、morphing、hybrid instancing 准备 descriptor set / buffer。
9. color pass descriptor set 准备 camera、time、fog、temporal noise、material globals。
10. `commitUniforms` 将 dirty uniform buffer 提交到 backend buffer object。
11. `commitDescriptorSet` 提交 per-view descriptor set。

这一步是 CPU/GPU 协同的核心：CPU 把高层场景对象转成紧凑、连续、按 binding 组织的 GPU 数据。

### 5.5 Material / MaterialInstance 的数据组织

`Material` 表示 shader 代码和参数接口；`MaterialInstance` 表示一份具体参数。

CPU 侧：

- Material 编译后拥有不同 variant 的 shader program。
- Material 定义 per-material descriptor set layout。
- MaterialInstance 保存参数值，拥有自己的 descriptor set。
- 渲染前 `prepareProgram` 确保对应 variant 的 program 编译。
- draw loop 中 `mi->use(driver, variant)` 绑定 per-material descriptor set。

GPU 侧：

- material 参数进入 `PER_MATERIAL` descriptor set。
- material uniforms 通常绑定到 `PerMaterialBindingPoints::MATERIAL_PARAMS`。
- material samplers 按材质编译期生成的 SIB / descriptor layout 绑定。
- variant 决定 shader 宏、功能分支和 program。

### 5.6 RenderPass command buffer：CPU 热路径优化

Filament 不按实体原始顺序直接 draw，而是在 `RenderPass::appendCommands` 中生成 command 数组。每个 command 有一个 64-bit key，编码 pass/channel/material/primitive/depth 等排序信息。

关键优化：

1. 可并行生成 command。
2. command 数组排序后可减少 material / pipeline / primitive 切换。
3. 最后追加 sentinel command。
4. 主线程遍历 commands 调用 `prepareProgram`，避免 shader 编译相关逻辑散落到 worker。
5. `Executor` 热循环中缓存当前 material instance、pipeline、primitive，只在变化时发 DriverApi 命令。
6. per-renderable UBO 使用 dynamic offset，每个 draw 只改变 offset，不需要重建 UBO。

### 5.7 FrameGraph：pass 与资源生命周期组织

FrameGraph 的职责：

1. View 渲染过程中声明 pass 和 texture/render target 资源。
2. 编译阶段裁剪不可达 pass。
3. 计算每个资源的 first user / last user。
4. 执行阶段在资源第一次使用前 devirtualize，最后一次使用后 destroy。
5. 每个 active pass 调用自己的 execute lambda，向 DriverApi 发命令。

技术意义：

- 避免创建未使用资源。
- 自动释放临时纹理。
- 让后处理链、阴影、SSR、TAA、bloom 等复杂 pass 更容易组合。
- 为图形调试器/fgviewer 提供 pass/resource 图。

---

## 6. CPU 与 GPU 的数据架构约定

### 6.1 Descriptor set 级别约定

Filament 把 GPU 资源分成三个核心 descriptor set：

| Set | 名称 | 用途 |
|---:|---|---|
| 0 | `PER_VIEW` | 每个 View / pass 共享的数据：相机、时间、光照、阴影、IBL、SSAO、SSR、fog 等。 |
| 1 | `PER_RENDERABLE` | 每个 renderable / draw 的数据：model matrix、normal matrix、object id、skinning、morphing、instance 等。 |
| 2 | `PER_MATERIAL` | 每个 MaterialInstance 的参数：material uniforms 和 material samplers。 |

这种分组非常重要：

- View 级数据变化频率低于 draw，可在 pass 前绑定。
- Renderable 级数据按 draw 变化，但统一放入大 UBO，draw 时用 dynamic offset。
- Material 级数据按 material instance 变化，排序后可减少切换。

### 6.2 Binding point 约定

`EngineEnums.h` 定义了 binding points：

- `PerViewBindingPoints::FRAME_UNIFORMS`
- `SHADOWS`
- `LIGHTS`
- `RECORD_BUFFER`
- `FROXEL_BUFFER`
- `STRUCTURE`
- `SHADOW_MAP`
- `IBL_DFG_LUT`
- `IBL_SPECULAR`
- `SSAO`
- `SSR / SSR_HISTORY`
- `FOG`

`PER_RENDERABLE` 包括：

- `OBJECT_UNIFORMS`
- `BONES_UNIFORMS`
- `MORPHING_UNIFORMS`
- `MORPH_TARGET_POSITIONS`
- `MORPH_TARGET_TANGENTS`
- `BONES_INDICES_AND_WEIGHTS`

`PER_MATERIAL` 包括：

- `MATERIAL_PARAMS`

### 6.3 UBO 布局约定：std140 C++ struct

`UibStructs.h` 是 CPU/GPU 协定的核心文件。它明确说明：这里定义的所有 UBO C struct 被 Filament 用来填充 uniform 值，也被 filabridge 用来获取 interface block 名称。

关键约束：

1. 必须遵守 std140 layout。
2. 修改 struct 必须同步更新 UIB generator。
3. `PerViewUib` 固定为 2 KiB。
4. `PerRenderableData` 固定为 256 bytes。
5. minspec UBO size 以 ES3.0 的 16 KiB 为约束。
6. 光源、骨骼、morph target、shadow map 数量都受到 UBO size / texture layer / 数据类型限制。

### 6.4 PerViewUib 内容

`PerViewUib` 的 block 名称是 `FrameUniforms`，包含：

- view/world/clip 矩阵及逆矩阵。
- stereo eye matrices。
- clip transform / clip control。
- time / userTime / temporal noise。
- physical resolution / logical viewport scale-offset。
- LOD bias / derivatives scale。
- camera near/far/exposure/EV100。
- AO 参数。
- froxel 参数。
- IBL SH、luminance、roughness level。
- 方向光、太阳、阴影 cascade、VSM 参数。
- fog 参数。
- SSR reprojection / UV transform / thickness / bias / distance / stride。
- material globals `custom[4]`。
- ES2 rec709 emulation flag。

### 6.5 PerRenderableData 内容

`PerRenderableData` 每个 renderable 256 bytes，包含：

- `worldFromModelMatrix`
- `worldFromModelNormalMatrix`
- `morphTargetCount`
- `flagsChannels`
- `objectId`
- `userData`
- reserved padding

`flagsChannels` 打包了：

- 是否 skinning
- morphing 数量/标记
- contact shadows
- 是否有 instance buffer
- render channel bits

渲染时所有可见 renderable 的 `PerRenderableData` 连续写入一个大 UBO。draw loop 中通过：

```cpp
uint32_t offset = info.index * sizeof(PerRenderableData);
driver.bindDescriptorSet(info.dsh, PER_RENDERABLE, {{ offset, info.skinningOffset }, driver});
```

把当前 draw 对应的数据窗口传给 shader。

### 6.6 Sampler Interface Block 约定

`SibStructs.h` 定义 sampler index：

- `PerViewSib::SHADOW_MAP`
- `IBL_DFG_LUT`
- `IBL_SPECULAR`
- `SSAO`
- `SSR`
- `STRUCTURE`
- `FOG`

morphing 和 skinning 也有各自 sampler index。这样 CPU descriptor set 和 shader sampler 声明可以稳定对应。

### 6.7 Material variant 与 specialization constants

Filament 材质有 variant 系统，用于组合动态光照、阴影、fog、SSR、VSM、depth pass 等 shader 变体。`ReservedSpecializationConstants` 保留了一组引擎内部 specialization constant，例如：

- backend feature level
- max instances
- static texture target workaround
- sRGB swapchain emulation
- froxel buffer height
- PowerVR workaround
- stereo eye count
- SH bands count
- shadow sampling method

这让 CPU 可以根据后端能力和当前 View 配置选择更合适的 shader specialization，减少 shader 中不必要分支。

---

## 7. CPU/GPU 高效协同的技术要点

### 7.1 按更新频率拆分数据

Filament 把数据拆成：

- Per-frame / per-view：相机、光照、fog、IBL、SSR、AO。
- Per-renderable：变换、object flags、skinning/morphing offset。
- Per-material：材质参数和纹理。

这与 GPU binding 频率一致，避免每 draw 重传 View 数据。

### 7.2 大 UBO + dynamic offset

对 renderable 数据，Filament 不为每个对象创建一个 UBO，而是把可见对象数据打包进连续大 UBO。每个 draw 只改变 dynamic offset。

好处：

- 减少 buffer object 数量。
- 减少 driver validation 成本。
- CPU 写入连续内存，cache 友好。
- GPU 读取规则固定，shader 简单。

### 7.3 CommandStream 降低线程耦合

`CommandStream` 把 Driver API 封装成命令对象：

- 异步 API：写入 `CircularBuffer`。
- 同步 API：直接调用 Driver。
- 返回 handle 的 API：先同步分配 handle，再写入真实创建命令。

命令对象保存参数 tuple，执行时调用具体 Driver 方法。这样应用侧无需立即等待 GL 执行，也可以集中后端调用。

### 7.4 FrameGraph 避免无效 GPU 工作

FrameGraph compile 会裁剪不可达 pass，并按 first/last user 管理资源生命周期。临时纹理只在必要时间存在，后处理链中不用的路径不会执行。

### 7.5 RenderPass 排序减少状态切换

RenderPass command key 排序后，热循环会尽量复用：

- material instance
- pipeline state
- render primitive
- descriptor set
- scissor

只有发生变化时才发 `bindPipeline`、`bindRenderPrimitive`、`bindDescriptorSet`。这对 OpenGL 尤其重要，因为 GL 状态切换和 driver validation 成本较高。

### 7.6 OpenGLState 避免重复 GL 调用

OpenGLState 缓存真实 GL 状态的 Filament 视图，避免重复 `glBind*`、`glEnable`、`glDisable`、`glUseProgram`。context 切换或外部 stream texture 改变时会显式 invalidate。

### 7.7 惰性 descriptor 更新

GLDescriptorSet 更新不等于立刻 GL bind。只有 draw 前发现 descriptor set 或 dynamic offsets 失效，才 `updateDescriptors`。这种策略避免了无 draw 的中间绑定，也能在 program 确定后做正确 location / binding 映射。

### 7.8 Shader 编译服务与 program cache

OpenGL shader 编译/link 可能阻塞。Filament 用：

- `ShaderCompilerService::tick()`
- program prepare / compile priority
- parallel shader compile extension（如可用）
- blob cache

尽量把 shader 编译成本提前、异步化或缓存化。

### 7.9 Clear / discard / invalidate 语义

虽然 OpenGL render pass 不像 Vulkan 显式，但 Filament 仍在 `beginRenderPass/endRenderPass` 贯彻 clear / discard：

- pass 开始清理或 invalidate 不需要 load 的附件。
- pass 结束 discard 不需要 store 的附件。
- MSAA 目标按 resolve action 处理。

这对移动 GPU 和 tile renderer 特别重要，也能帮助桌面驱动优化 framebuffer 压缩/加载。

### 7.10 多线程准备与单线程命令写入边界

Filament 会用 JobSystem 并行做可见性、command 生成等 CPU 工作，但 DriverApi 命令写入有线程约束，debug 下用 `driver.debugThreading()` 检查。这样既利用多核 CPU，又避免命令流多线程写入复杂度。

### 7.11 Feature level 兼容路径

同一套上层架构支持：

- ES2 / feature level 0
- GLES3+
- desktop GL
- WebGL

OpenGL 后端中大量条件处理：

- ES2 UBO emulation
- ES2 texture storage 回退
- ES2 draw2 dispatcher 替换
- sRGB swapchain rec709 emulation
- 扩展与 bug workaround

这让 API 层尽量统一，同时后端按能力降级。

---

## 8. 一帧渲染的端到端流程

下面以 Windows OpenGL 为例串起一帧：

```text
用户调用 renderer.beginFrame(swapChain)
  -> FRenderer::beginFrame
    -> swapChain->makeCurrent(driver)
      -> DriverApi::makeCurrent 命令
      -> OpenGLDriver::makeCurrent
      -> PlatformWGL::makeCurrent
      -> wglMakeCurrent(HDC, HGLRC)
    -> driver.updateStreams
    -> driver.tick
    -> driver.beginFrame
    -> engine.prepare(driver)

用户调用 renderer.render(view)
  -> FRenderer::renderInternal
    -> 创建 RootArenaScope（per-renderpass arena）
    -> 创建 root job
    -> renderJob
      -> View prepare：可见性、灯光、阴影、UBO、descriptor set
      -> 创建 FrameGraph
      -> 添加 shadow / depth / color / post-process / present 等 pass
      -> fg.compile：裁剪 pass，计算资源生命周期
      -> fg.execute(driver)
        -> 每个 pass devirtualize resource
        -> pass lambda 中 beginRenderPass
        -> RenderPass::Executor::execute
          -> bind per-view descriptor set
          -> material 切换时 bind per-material
          -> per draw bind per-renderable dynamic offset
          -> bindPipeline / bindRenderPrimitive
          -> draw2 / drawArrays
        -> endRenderPass
        -> last user 后 destroy resource
    -> engine.flush：提交命令流

用户调用 renderer.endFrame()
  -> FRenderer::endFrame
    -> swapChain->commit(driver)
      -> OpenGLDriver::commit
      -> PlatformWGL::commit
      -> SwapBuffers(HDC)
    -> engine.submitFrame
    -> driver.endFrame
    -> driver.tick
```

---

## 9. OpenGL 后端与现代显式 API 的差异适配

Filament 上层越来越接近现代显式 API 的模型：descriptor set、pipeline layout、render pass、FrameGraph、resource lifetime。但 OpenGL 是老式隐式状态机。因此 OpenGL 后端做了几层适配：

1. **Descriptor set -> GL binding points / texture units**
2. **PipelineState -> 多个 GL 状态调用 + glUseProgram**
3. **RenderPass load/store -> clear / invalidate / resolve**
4. **RenderTarget -> FBO / renderbuffer / texture attachment**
5. **Push constants -> program uniform 或后端模拟路径**
6. **Command buffer -> CPU side CircularBuffer + backend execution**
7. **Pipeline layout validation -> debug 下 descriptor set layout 校验**

这种设计牺牲了一些 GL 的直接性，但换来了跨后端统一和上层渲染架构稳定。

---

## 10. 总结：Filament 如何让 CPU 和 GPU 高效协同

Filament 的高效协同来自一组互相配合的设计：

1. **统一后端抽象**：应用层不直接碰 GL，资源和命令走 DriverApi。
2. **分频率的数据布局**：per-view / per-renderable / per-material 对应 GPU descriptor set。
3. **共享 struct 约定**：CPU C++ struct 与 shader interface block 共享布局来源，减少错配。
4. **连续内存和 dynamic offset**：大量对象数据以大 UBO 批量上传，每 draw 只改 offset。
5. **FrameGraph 管资源与 pass**：只执行必要 pass，只创建必要资源。
6. **RenderPass command 排序**：减少 pipeline、material、primitive 切换。
7. **OpenGLState 缓存**：减少重复 GL state call。
8. **惰性 descriptor bind**：到 draw 前才基于 program 真正更新 GL binding。
9. **异步命令流**：应用线程录制命令，后端集中执行。
10. **shader 编译和缓存机制**：降低运行期 program link/compile 阻塞。
11. **平台层隔离**：Windows WGL 只负责 context/swapchain/present，OpenGLDriver 不关心 Win32 细节。
12. **feature level 降级**：统一 API 下按 GL/GLES/WebGL 能力选择不同实现路径。

对 Windows OpenGL 来说，最核心的 RHI 实现点是：`PlatformWGL` 建立 HGLRC/HDC/HWND 世界，`OpenGLDriver` 将 Filament 的现代化渲染抽象翻译成 GL 状态机操作，`OpenGLState` 和 `GLDescriptorSet` 则负责把高层 pipeline/descriptor 模型高效映射到 OpenGL 的 program、UBO、texture unit、sampler、VAO、FBO 和 draw call。

---

## 11. 推荐继续阅读的源码路径

如果要继续深入，建议按以下顺序阅读：

1. `filament/backend/src/opengl/platforms/PlatformWGL.cpp`：Windows context / swapchain / present。
2. `filament/backend/include/backend/platforms/OpenGLPlatform.h`：平台层抽象。
3. `filament/backend/src/opengl/OpenGLDriver.h`：OpenGL 后端资源结构。
4. `filament/backend/src/opengl/OpenGLDriver.cpp`：资源创建、render pass、pipeline、draw。
5. `filament/backend/src/opengl/OpenGLState.h/.cpp`：状态缓存。
6. `filament/backend/src/opengl/GLDescriptorSet.h/.cpp`：descriptor set 到 GL 的映射。
7. `filament/backend/include/private/backend/CommandStream.h`：命令流。
8. `filament/backend/include/private/backend/DriverAPI.inc`：统一 Driver API 列表。
9. `filament/src/details/Renderer.cpp`：beginFrame/render/endFrame 主流程。
10. `filament/src/details/View.cpp`：View prepare、UBO 和 descriptor set 提交。
11. `filament/src/RenderPass.cpp`：command 生成、排序后的执行热循环。
12. `filament/src/fg/FrameGraph.cpp`：FrameGraph 编译与执行。
13. `libs/filabridge/include/private/filament/UibStructs.h`：CPU/GPU UBO 布局契约。
14. `libs/filabridge/include/private/filament/EngineEnums.h`：descriptor set / binding point / specialization constant 契约。
15. `libs/filabridge/include/private/filament/SibStructs.h`：sampler index 契约。
