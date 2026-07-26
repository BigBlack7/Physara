# Engine Architecture

```text
Physara/
├── CMakeLists.txt                                      // 配置 C++20、统一输出目录并装配各工程模块
├── README.md                                           // 项目简介、效果截图与顶层架构入口
├── Assets/                                             // 编辑器与渲染器运行时资源
│   ├── Gallery/                                        // README 使用的渲染效果截图
│   ├── Icons/                                          // 编辑器窗口、资源类型与视口控件图标
│   ├── Models/                                         // glTF 模型、几何缓冲及其配套纹理资源
│   ├── Scenes/                                         // 示例场景序列化数据
│   ├── Shaders/                                        // 运行时加载并生成 Variant 的 GLSL 源码
│   │   ├── Includes/                                   // 跨渲染 Pass 复用的公共 GLSL 模块
│   │   │   ├── BRDF.glsl                               // GGX、Smith、Fresnel 与能量补偿 BRDF 实现
│   │   │   ├── ClusteredLighting.glsl                  // Cluster 索引换算、灯光列表读取与调试着色
│   │   │   ├── Common.glsl                             // GPU 公共结构、绑定常量、曝光和基础约定
│   │   │   ├── FrameUniforms.glsl                      // 声明每帧相机、阴影、IBL 与 Cluster UBO
│   │   │   ├── IBL.glsl                                // SH 漫反射、预滤波环境反射与 BRDF LUT 求值
│   │   │   ├── Lighting.glsl                           // 平行光、点光和聚光灯的统一直接光照计算
│   │   │   ├── Material.glsl                           // GPU 材质数据、材质输入与像素材质结构
│   │   │   ├── Math.glsl                               // 饱和、亮度、法线映射和遮蔽等数学函数
│   │   │   ├── Packing.glsl                            // 八面体法线、RGBE 与材质通道打包解包
│   │   │   ├── Shadowing.glsl                          // CSM 选择及 Hard、PCF、Poisson、PCSS 采样
│   │   │   └── SurfaceMaterial.glsl                    // Bindless/传统纹理采样与表面材质解析
│   │   └── Passes/                                     // 各 RenderGraph Pass 使用的 Shader 入口
│   │       ├── Deferred/
│   │       │   ├── DeferredLighting.frag               // 从 GBuffer 重建表面并执行 Clustered 延迟光照
│   │       │   ├── GBuffer.frag                        // 写入 BaseColor/AO、Normal、Material 和 Emissive
│   │       │   └── GBuffer.vert                        // 为 GBuffer 阶段输出法线、切线、UV 与材质索引
│   │       ├── Forward/
│   │       │   ├── Forward.frag                        // 统一实现 Forward、Forward+、透明与 Unlit 着色
│   │       │   └── Forward.vert                        // 读取对象及实例索引并输出世界空间顶点属性
│   │       ├── PostProcess/
│   │       │   ├── BloomCopy.frag                      // 复制 Bloom 层并作为金字塔合成输入
│   │       │   ├── BloomDownsample.frag                // 对高亮图像执行抗闪烁降采样
│   │       │   ├── BloomPrefilter.frag                 // 按曝光、阈值和 soft-knee 提取高亮
│   │       │   ├── BloomUpsample.frag                  // 逐层上采样并累加低分辨率 Bloom
│   │       │   ├── Composite.frag                      // 合成 HDR、Bloom、调色、AA 与 DebugView
│   │       │   └── Composite.vert                      // 生成后处理全屏三角形及纹理坐标
│   │       ├── Shadow/
│   │       │   ├── Shadow.frag                         // 阴影深度阶段的最小片元入口
│   │       │   └── Shadow.vert                         // 按级联光源矩阵绘制实例化阴影投射物
│   │       ├── Skybox/
│   │       │   ├── Skybox.frag                         // 采样等距柱状 HDR 环境并输出预曝光天空
│   │       │   └── Skybox.vert                         // 生成全屏天空方向而不依赖天空盒网格
│   │       └── WorldGrid/
│   │           ├── WorldGrid.frag                      // 绘制带主轴、主网格和距离淡出的 XZ 网格
│   │           └── WorldGrid.vert                      // 用逆 VP 反投影生成网格射线端点
│   └── Textures/                                       // 示例材质贴图与 EXR 环境光资源
├── Backend/                                            // RHI 的具体图形 API 后端
│   ├── CMakeLists.txt                                  // 构建 Backend 静态库并链接 OpenGL 依赖
│   ├── RuntimeBackendFactory.cpp                       // 创建窗口、输入、RHI Device 与 ImGui 后端
│   ├── RuntimeBackendFactory.hpp                       // 声明运行时图形后端选择和创建结果
│   └── OpenGL/                                         // OpenGL 4.6 Core Profile 后端
│       ├── OpenGLBuffer.cpp                            // 创建、更新、映射并管理 OpenGL Buffer
│       ├── OpenGLBuffer.hpp                            // 定义 RHIBuffer 的 OpenGL 实现
│       ├── OpenGLCommandList.cpp                       // 执行 RenderPass、资源绑定、Draw/MDI、Barrier 和 Query
│       ├── OpenGLCommandList.hpp                       // 定义命令接口实现、状态缓存和时间戳查询环
│       ├── OpenGLDevice.cpp                            // 初始化 GL 能力并创建所有 OpenGL RHI 资源
│       ├── OpenGLDevice.hpp                            // 定义 RHIDevice 的 OpenGL 实现
│       ├── OpenGLFramebuffer.cpp                       // 创建和校验 OpenGL FBO 附件组合
│       ├── OpenGLFramebuffer.hpp                       // 定义 RHIFramebuffer 的 OpenGL 实现
│       ├── OpenGLImGuiBackend.cpp                      // 将 ImGui DrawData 转换为 RHI/OpenGL 绘制命令
│       ├── OpenGLImGuiBackend.hpp                      // 定义与编辑器隔离的 ImGui OpenGL 后端
│       ├── OpenGLPipeline.cpp                          // 链接 Shader Program 并应用固定功能 PSO 状态
│       ├── OpenGLPipeline.hpp                          // 定义 RHIPipelineState 的 OpenGL 实现
│       ├── OpenGLSampler.cpp                           // 创建并配置 OpenGL Sampler Object
│       ├── OpenGLSampler.hpp                           // 定义 RHISampler 的 OpenGL 实现
│       ├── OpenGLShader.cpp                            // 编译单个 GLSL Shader Stage 并输出诊断
│       ├── OpenGLShader.hpp                            // 定义 RHIShader 的 OpenGL 实现
│       ├── OpenGLTexture.cpp                           // 创建纹理、上传像素、生成 Mip 与管理 Bindless Handle
│       ├── OpenGLTexture.hpp                           // 定义 RHITexture 的 OpenGL 实现
│       └── OpenGLTypeMapping.hpp                       // 集中转换 RHI 枚举与 OpenGL 类型
├── Docs/                                               // 架构、实现细节与参考资料
│   ├── FilamentAppSide.md                              // Filament 应用程序侧架构整理
│   ├── FilamentRenderSide.md                           // Filament 渲染器侧技术整理
│   └── Physara.md                                      // 当前工程目录与模块职责说明
├── Editor/                                             // ImGui 场景编辑器与交互工具
│   ├── CMakeLists.txt                                  // 构建 Editor 静态库并链接 Engine 与 Platform
│   ├── Camera/
│   │   ├── EditorCamera.cpp                            // 实现 Orbit、Fly、输入捕获和 RenderView 生成
│   │   └── EditorCamera.hpp                            // 定义编辑器相机模式、输入帧与相机设置
│   ├── Core/
│   │   ├── EditorApp.cpp                               // 装配场景、Renderer、面板并驱动编辑器帧
│   │   ├── EditorApp.hpp                               // 定义编辑器应用主体及其模块所有权
│   │   ├── EditorAppHost.cpp                           // 连接窗口、输入、RHI、ImGui 与 EditorApp 生命周期
│   │   ├── EditorAppHost.hpp                           // 定义隔离平台细节的编辑器宿主接口
│   │   ├── EditorContext.hpp                           // 汇总选择、视口、渲染设置和共享 UI 状态
│   │   ├── EditorTheme.cpp                             // 配置 Physara 的 ImGui 颜色和控件风格
│   │   ├── EditorTheme.hpp                             // 声明编辑器主题应用入口
│   │   ├── IconManager.cpp                             // 加载图标纹理并向 ImGui 提供稳定句柄
│   │   ├── IconManager.hpp                             // 定义编辑器图标种类与缓存接口
│   │   ├── ShortcutRegistry.cpp                        // 注册并分发编辑器快捷键动作
│   │   └── ShortcutRegistry.hpp                        // 定义快捷键上下文、动作描述和查询接口
│   ├── Interaction/
│   │   ├── Gizmo.cpp                                   // 使用 ImGuizmo 编辑实体世界变换并写回组件
│   │   ├── Gizmo.hpp                                   // 定义 Gizmo 绘制及交互占用状态
│   │   ├── LightProxyPass.cpp                          // 将场景灯光投影为视口 Billboard 代理
│   │   ├── LightProxyPass.hpp                          // 定义灯光代理覆盖层绘制接口
│   │   ├── Picking.cpp                                 // 通过屏幕射线、AABB 和三角形测试选择实体
│   │   └── Picking.hpp                                 // 定义拾取请求、选择语义和拾取接口
│   └── Panels/
│       ├── ComponentDrawer.hpp                         // 提供各 ECS Component 的 Inspector 绘制实现
│       ├── ContentBrowserPanel.cpp                      // 浏览 Assets 并触发场景、模型和环境资源操作
│       ├── ContentBrowserPanel.hpp                      // 定义内容浏览器面板状态与接口
│       ├── HelpShortcutsPanel.cpp                       // 按注册表显示快捷键帮助弹窗
│       ├── HelpShortcutsPanel.hpp                       // 定义快捷键帮助面板
│       ├── HierarchyPanel.cpp                           // 显示实体树并处理创建、选中和父子关系
│       ├── HierarchyPanel.hpp                           // 定义场景层级面板及实体创建类型
│       ├── InspectorPanel.cpp                           // 调度选中实体各组件的属性编辑器
│       ├── InspectorPanel.hpp                           // 定义 Inspector 面板接口
│       ├── LogPanel.cpp                                // 显示、过滤、搜索并复制环形日志
│       ├── LogPanel.hpp                                // 定义日志级别、缓存行和面板状态
│       ├── RendererSettingsPanel.cpp                    // 编辑管线、阴影、IBL、后处理、网格和基准设置
│       ├── RendererSettingsPanel.hpp                    // 定义渲染设置面板接口
│       ├── SceneViewPanel.cpp                           // 展示最终 RT 并协调相机、拾取、Gizmo 和覆盖层
│       └── SceneViewPanel.hpp                           // 定义场景视口面板、图标和交互状态
├── Engine/                                             // 与图形 API 和编辑器解耦的引擎核心
│   ├── CMakeLists.txt                                  // 构建 Engine 静态库并登记全部引擎源码
│   ├── Core/
│   │   ├── Log.cpp                                     // 初始化 spdlog 控制台与内存环形 Sink
│   │   ├── Log.hpp                                     // 定义 Engine/Editor 日志器及日志宏
│   │   ├── Time.cpp                                    // 更新帧间隔与累计运行时间
│   │   └── Time.hpp                                    // 定义全局帧时间访问接口
│   ├── Renderer/                                       // 管线编排、GPU 数据和渲染 Pass
│   │   ├── ClusteredLightGrid.cpp                       // 以 Count-Prefix-Fill 构建 Cluster 连续灯光索引
│   │   ├── ClusteredLightGrid.hpp                       // 定义可复用的 CPU Cluster 网格和缓存数据
│   │   ├── DeferredResources.cpp                        // 创建、缩放和释放 Deferred GBuffer 资源
│   │   ├── DeferredResources.hpp                        // 封装 GBuffer 纹理及 Framebuffer 生命周期
│   │   ├── FrameData.cpp                                // 重置和维护每帧渲染数据与统计
│   │   ├── FrameData.hpp                                // 定义帧数据、CPU/GPU 统计和计时作用域
│   │   ├── FrameUploadAllocator.cpp                     // 聚合帧内上传并一次刷新连续脏区间
│   │   ├── FrameUploadAllocator.hpp                     // 定义对齐分配、Staging 和临时 Buffer 生命周期
│   │   ├── FrustumPartition.cpp                         // 构建可供 CSM 与 Cluster 复用的对数深度分区
│   │   ├── FrustumPartition.hpp                         // 定义视锥深度切片结果与分区接口
│   │   ├── GPUContracts.hpp                             // 对齐 C++/GLSL Binding、枚举和 GPU 结构布局
│   │   ├── GPUScene.cpp                                 // 上传 Frame、Object、Light、Material 和 Cluster 表
│   │   ├── GPUScene.hpp                                 // 集中持有当前帧 GPU Buffer Allocation
│   │   ├── IBLResources.cpp                             // 异步预计算并上传 SH、预滤波 Cubemap 和 BRDF LUT
│   │   ├── IBLResources.hpp                             // 管理环境路径、预览结果与 IBL GPU 生命周期
│   │   ├── MaterialInstance.hpp                         // 定义稳定材质实例 ID 与实例数据
│   │   ├── MaterialInstanceRegistry.cpp                 // 去重并生成当前帧材质实例表
│   │   ├── MaterialInstanceRegistry.hpp                 // 定义材质实例注册和索引查询
│   │   ├── MaterialSignature.cpp                        // 计算材质状态与资源引用的稳定签名
│   │   ├── MaterialSignature.hpp                        // 声明材质签名构建接口
│   │   ├── MaterialTextureCache.cpp                     // 缓存默认/资产纹理并构建传统与 Bindless 表
│   │   ├── MaterialTextureCache.hpp                     // 定义材质纹理集、Handle 表和上传缓存
│   │   ├── MeshGPUCache.cpp                             // 将 Mesh 几何分配到共享 Geometry Page
│   │   ├── MeshGPUCache.hpp                             // 定义几何页、GPU Primitive 与 Mesh 缓存
│   │   ├── PipelineStateCache.cpp                       // 按 PSO 描述哈希复用已创建 Pipeline
│   │   ├── PipelineStateCache.hpp                       // 定义 Pipeline State 缓存接口
│   │   ├── RenderCommandExecutor.cpp                    // 将排序命令合并为 Direct、Instanced 或 MDI 提交
│   │   ├── RenderCommandExecutor.hpp                    // 定义提交模式、回调和 Indirect Run
│   │   ├── Renderer.cpp                                 // 构建三条管线 RenderGraph 并驱动完整渲染帧
│   │   ├── Renderer.hpp                                 // 定义 Renderer 门面、资源所有权和 Benchmark 状态
│   │   ├── RendererCapture.cpp                          // 从最终纹理回读并编码 PNG/JPG 图像
│   │   ├── RendererCapture.hpp                          // 定义截图格式、请求和结果
│   │   ├── RenderPath.hpp                               // 定义 Forward、Forward+、Deferred 及显示名称
│   │   ├── RenderProxy.cpp                              // 收集、裁剪、分桶、排序并生成实例化 RenderCommand
│   │   ├── RenderProxy.hpp                              // 定义 DrawItem、RenderBucket 与 RenderCommand 数据
│   │   ├── RenderView.cpp                               // 从相机参数构建 View/Projection 及逆矩阵
│   │   ├── RenderView.hpp                               // 定义视口和渲染相机快照
│   │   ├── UploadHasher.hpp                             // 提供 GPU 上传内容的稳定哈希工具
│   │   ├── Passes/
│   │   │   ├── DeferredLightingPass.cpp                // 绑定 GBuffer、灯光、IBL 与阴影执行全屏光照
│   │   │   ├── DeferredLightingPass.hpp                // 定义 Deferred Lighting Pass 上下文与资源
│   │   │   ├── ForwardOpaquePass.cpp                   // 提交 Forward/Forward+、透明及 Unlit 几何
│   │   │   ├── ForwardOpaquePass.hpp                   // 定义前向光照模式和 Pass 上下文
│   │   │   ├── GBufferPass.cpp                         // 清理并填充 Deferred MRT 与共享深度
│   │   │   ├── GBufferPass.hpp                         // 定义 GBuffer Pass 上下文和命令提交状态
│   │   │   ├── PostProcessPass.cpp                     // 构建 Bloom 金字塔并执行最终合成与 DebugView
│   │   │   ├── PostProcessPass.hpp                     // 定义后处理、Tone Mapping、AA 和调试设置
│   │   │   ├── ShadowPass.cpp                          // 拟合并绘制 CSM 各级联深度层
│   │   │   ├── ShadowPass.hpp                          // 定义软阴影算法、级联设置和缓存状态
│   │   │   ├── SkyboxPass.cpp                          // 管理环境纹理并绘制 HDR 天空背景
│   │   │   ├── SkyboxPass.hpp                          // 定义 Skybox Pass 上下文与资源状态
│   │   │   ├── WorldGridPass.cpp                       // 将 XZ 世界网格合成到场景 HDR 目标
│   │   │   └── WorldGridPass.hpp                       // 定义网格间距、主线和淡出设置
│   │   ├── Precompute/
│   │   │   ├── IBLPrecompute.cpp                       // 计算环境 Cubemap、SH、镜面 Mip 和 BRDF LUT
│   │   │   └── IBLPrecompute.hpp                       // 定义 IBL 预计算参数、结果和渐进阶段
│   │   └── RenderGraph/
│   │       ├── PassNode.hpp                            // 定义 Pass 资源访问声明与执行回调
│   │       ├── RenderGraph.cpp                         // 编译依赖、分配瞬态纹理、插入 Barrier 并执行 Pass
│   │       ├── RenderGraph.hpp                         // 定义每帧图描述和跨帧 Texture Pool
│   │       ├── ResourceNode.hpp                        // 定义导入/瞬态纹理节点和资源句柄
│   │       ├── RGBuilder.cpp                           // 记录 Pass 的 Read、Write 和 Execute 声明
│   │       └── RGBuilder.hpp                           // 定义链式 RenderGraph Pass 构建接口
│   ├── Resource/                                       // CPU 资产、类型安全句柄与加载器
│   │   ├── AssetManager.cpp                            // 注册、查询并管理路径到资源实例的映射
│   │   ├── AssetManager.hpp                            // 定义类型安全资产仓库和记录信息
│   │   ├── AssetPath.cpp                               // 规范化 Assets 相对路径并判断资产类型
│   │   ├── AssetPath.hpp                               // 定义资产路径分类和解析接口
│   │   ├── BuiltinPrimitives.cpp                       // 生成并注册立方体、球体和平面等内建网格
│   │   ├── BuiltinPrimitives.hpp                       // 声明内建几何注册入口
│   │   ├── Handle.hpp                                  // 定义带代数版本的类型安全资源句柄
│   │   ├── ShaderLibrary.cpp                           // 加载、编译、缓存并热重载 Shader Variant
│   │   ├── ShaderLibrary.hpp                           // 定义 Shader Program 描述和 Variant 库
│   │   ├── Loaders/
│   │   │   ├── GLTFLoader.cpp                          // 解析 glTF 并导入实体、网格、材质和纹理
│   │   │   ├── GLTFLoader.hpp                          // 定义 glTF 场景导入接口
│   │   │   ├── ShaderLoader.cpp                        // 展开 GLSL include 并注入 Feature/Stage Define
│   │   │   ├── ShaderLoader.hpp                        // 定义 Shader 源码加载描述
│   │   │   ├── TextureLoader.cpp                       // 解码 LDR、HDR 与 EXR 纹理数据
│   │   │   └── TextureLoader.hpp                       // 定义纹理加载接口
│   │   └── Types/
│   │       ├── Material.hpp                            // 定义 PBR 材质资源及其组件数据
│   │       ├── Mesh.hpp                                // 定义 CPU 顶点、Primitive、AABB 和网格资源
│   │       ├── Shader.cpp                              // 实现 Shader Feature Mask 与 Variant 有效性
│   │       ├── Shader.hpp                              // 定义 Shader Feature、源码和编译后 Variant
│   │       └── Texture.hpp                             // 定义纹理像素、格式、色彩空间和尺寸元数据
│   ├── RHI/                                            // 后端无关的渲染硬件接口
│   │   ├── RHIDefinitions.hpp                          // 定义格式、状态、Barrier、统计和渲染基础枚举
│   │   ├── Command/
│   │   │   └── RHICommandList.hpp                      // 抽象 RenderPass、资源绑定、Draw、Barrier 和 Query
│   │   ├── Core/
│   │   │   ├── IImGuiBackend.hpp                       // 隔离编辑器与具体 ImGui 图形后端
│   │   │   └── RHIDevice.hpp                           // 抽象 RHI 资源创建、能力查询和命令列表
│   │   ├── Descriptors/
│   │   │   ├── RHIBufferDesc.hpp                       // 描述 Buffer 大小、用途和更新方式
│   │   │   ├── RHIIndirectDrawCommand.hpp              // 定义与 GPU 间接绘制布局一致的五字段命令
│   │   │   ├── RHIRenderPrimitive.hpp                  // 描述顶点/索引绑定与可绘制 Primitive
│   │   │   ├── RHIResourceSet.hpp                      // 描述一组 Shader 纹理与 Sampler 绑定
│   │   │   ├── RHISamplerDesc.hpp                      // 描述过滤、寻址、各向异性和深度比较
│   │   │   └── RHITextureDesc.hpp                      // 描述纹理维度、格式、Mip、采样数和用途
│   │   ├── Pipeline/
│   │   │   ├── RHIFramebuffer.hpp                      // 抽象 RenderTarget 附件集合
│   │   │   ├── RHIPipelineState.hpp                    // 描述 Shader、顶点布局和固定功能 PSO
│   │   │   └── RHIRenderPassDesc.hpp                   // 描述附件 Load/Store、清屏和 RenderPass 兼容性
│   │   └── Resource/
│   │       ├── RHIBuffer.hpp                           // 抽象 GPU Buffer 更新与查询
│   │       ├── RHISampler.hpp                          // 抽象纹理采样器对象
│   │       ├── RHIShader.hpp                           // 抽象单个已编译 Shader Stage
│   │       └── RHITexture.hpp                          // 抽象纹理上传、Mip、Bindless 与原生句柄
│   └── Scene/                                          // ECS 场景、组件和系统
│       ├── Entity.cpp                                  // 实现实体有效性和组件操作辅助逻辑
│       ├── Entity.hpp                                  // 封装 entt Entity 与类型安全组件接口
│       ├── EntityId.hpp                                // 定义引擎实体 ID 和空实体常量
│       ├── Scene.cpp                                   // 管理实体生命周期、层级和场景更新
│       ├── Scene.hpp                                   // 定义持有 entt Registry 的场景容器
│       ├── SceneSerializer.cpp                         // 序列化和反序列化场景 JSON
│       ├── SceneSerializer.hpp                         // 定义场景文件保存与加载接口
│       ├── Components/
│       │   ├── CameraComponent.hpp                     // 定义透视/正交物理相机及手动曝光参数
│       │   ├── LightComponent.hpp                      // 定义平行光、点光和聚光灯物理参数
│       │   ├── MaterialComponent.hpp                   // 定义 PBR、Unlit、透明模式和纹理槽
│       │   ├── MeshComponent.hpp                       // 定义 Mesh Primitive 引用、材质槽和局部包围盒
│       │   ├── RelationshipComponent.hpp               // 定义父子实体关系
│       │   ├── TagComponent.hpp                        // 保存实体可读名称
│       │   ├── TransformComponent.cpp                  // 计算并缓存 Local/World 变换及父子换算
│       │   └── TransformComponent.hpp                  // 定义世界位置语义和变换脏标记
│       └── Systems/
│           ├── LightSystem.cpp                         // 收集可见灯光并转换为 GPU LightData
│           ├── LightSystem.hpp                         // 定义灯光收集系统接口
│           ├── RenderSystem.cpp                        // 收集 Mesh/Material/Transform 为渲染提交项
│           ├── RenderSystem.hpp                        // 定义场景渲染提交结构和可复用 Scratch
│           ├── TransformSystem.cpp                     // 按层级传播并更新实体世界变换
│           └── TransformSystem.hpp                     // 定义场景变换更新系统
├── Platform/                                           // 文件系统、输入与窗口平台抽象
│   ├── CMakeLists.txt                                  // 构建 Platform 静态库并链接 GLFW
│   ├── FileSystem/
│   │   ├── FileSystem.cpp                              // 实现资产路径解析和二进制/文本文件访问
│   │   └── FileSystem.hpp                              // 定义平台无关文件系统接口
│   ├── Input/
│   │   ├── GLFWInput.cpp                               // 将 GLFW 键鼠和光标模式映射为 IInput
│   │   ├── GLFWInput.hpp                               // 定义 GLFW 输入后端
│   │   ├── IInput.hpp                                  // 抽象按键、鼠标、光标和帧输入查询
│   │   └── KeyCodes.hpp                                // 定义后端无关键盘与鼠标枚举
│   └── Window/
│       ├── GLFWWindowContext.hpp                        // 连接 GLFW Window、Input 与回调用户数据
│       ├── GLFWWindowOpenGL.cpp                         // 创建 OpenGL 4.6 窗口并处理尺寸、图标和 VSync
│       ├── GLFWWindowOpenGL.hpp                         // 定义 OpenGL GLFW 窗口实现
│       └── IWindow.hpp                                 // 抽象窗口生命周期、尺寸、事件和垂直同步
├── Runtime/                                            // 可执行程序入口与模块装配
│   ├── CMakeLists.txt                                  // 构建 Physara 可执行文件并链接核心模块
│   └── Main.cpp                                        // 创建后端和 EditorAppHost 并运行主循环
├── ThirdParty/                                         // 由 CMake 管理的外部依赖，内部源码不在此展开
│   ├── entt/                                           // ECS Registry 与 View
│   ├── glad/                                           // OpenGL 函数加载器
│   ├── glfw/                                           // 窗口、上下文与输入
│   ├── glm/                                            // 图形数学库
│   ├── imgui/                                          // ImGui、ImGuizmo 与编辑器 UI
│   ├── nlohmann/                                       // JSON 解析与场景序列化
│   ├── spdlog/                                         // 日志系统
│   ├── stb/                                            // 图像加载与写出
│   ├── tinyexr/                                        // EXR 环境图读取
│   └── tinygltf/                                       // glTF 2.0 解析
└── Tools/
    └── verify_gpu_contracts.py                          // 校验 C++ 与 GLSL 的 Binding 和结构布局契约
```
