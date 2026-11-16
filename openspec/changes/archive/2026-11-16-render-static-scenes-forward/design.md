# Design

## Context

`SceneFrame` 固定 `ModelVersion`、`PrimitiveRef` 和实例 `EffectiveMaterial`，`PreparedView` 提供本轮相机、有效图像矩形及可见索引。`Runtime/Frame.cpp` 在安全帧槽中准备这些值，再走 `View::PrepareDiagnostic` 和正常/安全 UI 图。`FrameGraph` 已按存活 Pass 收集上传账单，RHI 支持索引绘制、只读深度附件及指定 mip 写入。 [现有帧入口](../../../../Runtime/Frame.cpp) · [固定 CPU 版本](../../../../Engine/Renderer/Prepare/SceneFrame.hpp) · [上传声明](../../../../Engine/Renderer/FrameGraph.hpp)

## Goals / Non-Goals

- 在同一产品路径形成 `PreparedView → DrawRecord → Forward → ViewOutput → UI → accepted submission`。用有效资源版本和像素证明首个静态几何切片。
- 本切片使用单 Graphics 队列、单样本、显式 texture/sampler 绑定和直接 `DrawIndexed`。物理光照与正式显示由 [D07](../../../../Docs/Developments/D06-D07-SceneAndForward.md#dev-d07) 接续，完整池化、增量上传和间接批次分别由 [D08-T02](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t02) 和 [D08-T03](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t03) 接续。

## Decisions

### 1. 固定版本归 Renderer 缓存，当前候选和 CPU 载荷归 FrameSlot

Renderer 的设备级 owner 持有 GeometryCache、TextureCache、材质行、绑定和 Pipeline。按 `PreparedView` 的可见索引解析帧内引用，Asset 与 Scene 继续持有 CPU 数据。几何以模型 `AssetId + ContentRevision + mesh/sourceSlot + vertex layout` 定位，布局包含实际所需属性并区分切线所用 UV。首版每项可拥有独立 vertex/index 存储，`GeometryRef` 仍提供 layout、页身份和精确索引区间，供后续页分配复用。相同键复用只读已接受存储。

纹理存储键使用模型身份/版本与局部 image 索引，外置图像使用固定 image 身份/版本，另含用途解释、实际格式和有效 mip 策略。纹理槽另行解析视图、有效 sampler、UV 集与变换。合成后的有效 sampler 按寻址、min/mag/mip 组合和视图可访问层级准备。用途对应的颜色/数据格式在设备准备时选定。显式要求 mip 时在冷准备以拥有字节生成小型完整链，sRGB RGB 先解码再过滤并编码，alpha 与线性数据按其数值过滤。仅使用基础层的 minFilter 通过单层视图或单层存储限制 LOD。未给出 min/mag 时本阶段固定普通线性过滤，wrap 采用 glTF 缺省。这样同一已解码图像的存储选择和实际 sampler 都可复现。 [图像合同](../../../../Docs/Architecture.md#asset-images) · [GL 采样视图](../../../../RHI/OpenGL/Bindings.cpp)

标准材质优先共享同一模型版本内的源材质行。实例覆盖以 `SceneId + EntityHandle generation + sourceSlot` 区分，并按固定源版本、完整有效参数和固定纹理/binding 身份判断变更。沿已有 `EffectiveMaterial` 解析，比较实际变化的行。首版由 `MaterialGpuIndex` 解析已准备的只读 GPU 行，可用普通 UBO 独立存储以便单行换代。对象变换从现有连续记录打包，CameraSnapshot 和显示模式按 View 准备，安全槽复用后才可写入其 per-view/per-object 动态存储。创建 Draw 后将仍需写入的内容纳入同一场景上传包。

用设备能力与显式条目、驻留字节、候选上传和图附件预算限制增长。已退出当前使用范围的资源可被选择淘汰，按真实票据保留在途代，预算须计入仍保活的代。为新 revision 或变更的材质创建新候选存储，发布后旧 Draw、图和提交继续持有原对象。局部超限有可见原因，暂时等待可在安全空间释放后重试；超出首版硬上限的候选明确拒绝。退出时先结束帧和提交生命期，再按已有 context 归属交还资源。 [驻留](../../../../Docs/Architecture.md#gpu-residency) · [帧槽](../../../../Docs/Architecture.md#frame-state) · [退休](../../../../Docs/Architecture.md#retirement)

### 2. 声明范围、设备准入和上传确认使用同一份工作集合

准备入口分配存储、解析布局、Pipeline、绑定和目标范围。槽中的 UploadBatch 拥有目标句柄和稳定字节，记录写的 byte range 或 mip/layer、行距和消费用途。字节可来自被固定的不可变载荷或槽内拥有的打包结果，附加完成后封存。图导入候选存储时标记初始内容未定义，每次写产生新版本；后继从同一图读取已声明且初始化的范围。复用已接受代时，导入的 entry/final 和 `contentDefined` 来自真实事实。

复用 `AddTextureUpload`、`AddBufferUpload` 和 `CollectGraphUploads` 的声明/执行链，把纹理写的精确 mip 与 buffer 的实际范围传到 RHI。按共用账单的要求先声明场景和 UI 的纹理上传，再声明缓冲上传，所有采样和绘制消费者读取最终版本。必要时在现有 UI 图构建入口内提前声明纹理生产者，UI 的请求所有权和接受协议继续由 UI 持有。按编译后的实际顺序与存活集合准入，保住基础 UI；框架可用时用带原因的占位呈现暂时等待或超限。全裁剪时可以跳过准备，也可由图裁剪尚未物化的分支。 [帧图](../../../../Docs/Architecture.md#graph) · [上传](../../../../Docs/Architecture.md#uploads) · [同步](../../../../Docs/Architecture.md#barriers)

容量不足时 Runtime 根据实际准入结果移除视口生产者，并由 View 记录相应的等待或硬拒绝。普通占位重建与下一轮设置快照都读取该原因。

缓存版本确认来自本候选存活写入、成功执行/Close 及同一完整接受回执。常驻导入采用 `keepInitialState` 与匹配的合法 entry/final，满足关闭后的重新进入条件。已发前缀按实票保活，重试继续使用合法入口事实和安全存储，恢复资格按现有宿主合同判断。缓存只在真实接受时把候选内容登记为可复用，只有实际完成才释放图中仍引用的物理资源和上传区段。安全 UI 只确认自己的工作。 [图执行](../../../specs/render-graph-execution/spec.md) · [现有失败与回执边界](../../../../Runtime/Frame.cpp)

驻留图片以 Common 作为进出状态，使首次未初始化的存储与后续录制使用相同边界。图另以 `contentDefined` 表达已上传事实。显示色按 ShaderResource 导出并保持该初态，以便安全 UI 复用。 [状态跟踪](../../../../RHI/Common/StateTracking.cpp) · [Forward 声明](../../../../Engine/Renderer/Paths/Forward.cpp)

### 3. 共享 GLSL 解释原始几何及有效标准材质

用 Engine/Shader schema 生成 PerView、PerRenderable、PerMaterial 的 C++ 数据、GLSL、binding 和 stage IO。需要独立 stage IO 的 Depth 与 Color 程序共用 `Shaders/Geometry`、`Shaders/Material` 的函数和生成字段定义；共同顶点输入和位置计算须在实际反射与链接后相容。程序包随配置构建，由 Runtime 从实际 exe 位置定位。可选场景程序或 Pipeline 准备失败作为场景不可用原因交给 View 和现有安全路径，基础 UI 包继续作为启动必需条件。 [Shader 构建与 ABI](../../../../Docs/Architecture.md#shader-build) · [共享 Shader 模块](../../../../Docs/Architecture.md#shader-runtime) · [既有包定位](../../../../Runtime/Resources.cpp)

- 在共享顶点路径使用 `worldFromLocal` 和本轮 `renderClipFromView * viewFromWorld`。保留源 CCW，负 determinant 通过公共 raster state 调整正面。Prepare 校验矩阵的有限 GPU 表示及 normal inverse-transpose 可用性；奇异变换返回对应 Primitive 的不可用原因。法线走 inverse-transpose，切线沿线性变换后相对 N 正交化，bitangent 使用有效 handedness。
- normal 采样使用 `effectiveUV`。原切线对应其 UV 时保留源 `w`；覆盖引入 normal 槽或修改了切线所依赖的 UV 而没有匹配切线时，在准备中复用现有 MikkTSpace 角点生成和拆索引逻辑，生成按 UV 键区分的几何版本。供准备使用的最小类型化入口接受已验证数据，源模型不被修改。退化 UV 无法得到可用框架时报告对应槽。 [源切线约定](../../../../Docs/Architecture.md#asset-import) · [固定角点算法](../../../../Engine/Asset/GltfTangents.hpp)
- 取样坐标按 `offset + R(rotation) * (scale * uv)` 求值。对于 normal 槽，在原局部单位切线框架应用这个线性变换的朝向部分 `Q = R * diag(signX, signY)`，用 `Q` 的逆确定纹理切线方向；scale 的绝对值控制取样频率，零尺度沿正向约定作常量坐标取样。切线变换到世界后用源 `w`、`det(Q)` 和实例 orientation 确定 bitangent 符号。normal 纹理线性解码 `2*rgb-1`，scale 乘 XY 后规范化。双面背面把完成的表面法线翻转一次，避免在坐标适配和材质求值分别翻转。
- Depth 与 Color 调用同一位置和 alpha 求值。保留 `baseColorFactor.a × vertexColor.a × baseColorTexture.a`，MASK 在 `< alphaCutoff` 时 discard。OPAQUE 按几何覆盖。基础纹理颜色乘线性因子和顶点颜色，ORM 以 `mix(1, R, occlusionStrength)`、`G*roughnessFactor`、`B*metallicFactor` 求值；自发光按解码颜色、因子和 strength 相乘。缺席槽用标准因子和缺省样本；Unlit 只从基色及其覆盖产生颜色。 [静态几何](../../../../Docs/Architecture.md#geometry) · [材质输入](../../../../Docs/Architecture.md#materials)

### 4. Forward 子图按实际消费者准备表面并返回两个颜色域明确的结果

Draw 构建在录制之前使用当前可见索引，解析每项几何、材质行、对象数据、覆盖、Pipeline 和绑定。DrawRecord 保留索引区间、绑定与版本身份，按 Pipeline、纹理/采样绑定、几何布局及深度组织不透明项，以稳定身份维持可重现的相等深度顺序。CPU 路由区分不透明、masked、Unlit 与待支持的 `BLEND`。不支持的路由或框架有按源 Primitive/字段的有界原因，可绘集合保持自己的有效性。执行只访问 `PassResources` 授权的版本，核对预制绑定的实际读范围后发直接索引 Draw。

`PrepareSurface` 消费实际 `ViewResourceNeeds`，有早期深度需要时声明 D32 附件、清零并绘制相同覆盖。当前主视口由 `ShadeOpaque` 在单个场景绘制 Pass 生成颜色和深度。有早期深度时 `ShadeOpaque` 附着同版深度只读并按相等深度着色；两种组合使用一致的投影、光栅状态和 MASK 取样，并用相同绘制顺序使 coplanar tie 一致。Reverse-Z 写入采用 clear=0、`GreaterEqual`。所有图形输出先初始化整张画布，几何用 `ViewImageLayout.imageRect` 设置 viewport/scissor。GL 原点和公共 CCW 仅在现有 RHI 边界转换。 [路径输入输出](../../../../Docs/Architecture.md#paths) · [坐标与颜色](../../../../Docs/Architecture.md#conventions)

`OpaqueOutput` 交付真实深度及 `RGBA16_FLOAT` 线性预览颜色，并携带该工作配置的颜色域。默认 Base Color 对 lit 物体显示材质基色。另提供 World Normal (`0.5*N+0.5`)、Material Channels (`R=AO, G=roughness, B=metallic`) 和 Emissive（乘 strength 后的线性值）。Unlit 在这些预览保持标准基色。预览以单位曝光倍率输出，自发光保留附件可表示的超白值，超出 `RGBA16_FLOAT` 有限范围则限于其最大有限值。薄的预览转换 Pass 读入工作颜色并线性限幅到 [0,1]，写入 sRGB 存储、采样语义为 display-linear 的 `ViewOutput`。UI 使用既有线性混合和窗口末端编码。此预览 Pass 是 D07 正式显示生产者的替换位置。 [View 输出合同](../../../../Docs/Architecture.md#display-output) · [编辑器采样](../../../../Docs/Architecture.md#editor-output)

### 5. 以现有编辑器 token 消费当前 View 输出并确认

空文档沿用标记明确的测试图。有静态场景工作集时使用 Forward，视锥剔除后为空可以正常发布清除色。当前可见 Primitive 均超出支持 profile 时显示具体原因；支持项与 `BLEND` 等不支持项并存时保留有效部分并标记降级。受支持绘制所需的驻留、Pipeline、相机或布局未准备好时，整个该 View 候选进入等待或拒绝。Renderer 返回带种类/原因、身份、内容与相机 revision、模式及完整布局的候选。现有视口提供会话内预览模式入口及该帧标识，模式在已有准备边界输入并纳入匹配条件，v1 场景文档按既有字段保存。

正常编辑器合成用同一显示角色和稳定 token 读取实际消费的图颜色，准备与取样都沿本轮已封存 UI。只有当本轮 View 精确匹配且完整提交被接受时，确认生效比例、布局、场景预览代及对应 CameraSnapshot。该场景预览记录服务当前像素和输入映射；物理曝光及 SSR/TAA 历史按后续消费者引入。安全 UI 可显式导入同一 View 上次获接受且内容可证的显示色，须带旧输出标识；在此期间图像输入不可用。新候选分配独立物理输出，保护旧图采样所需的内容。 [提交与历史](../../../../Docs/Architecture.md#history-commit) · [现有安全图](../../../specs/editor-frame-graph/spec.md)

混合场景在候选画面上显示简短降级标识。Frame status 菜单从最近接受的显示色读取实际模式及有界摘要，正文按窗口内容宽度换行。

## Risks / Trade-offs

- 首次准备把精确范围和 mip 载荷纳入同一批，资源规模受首版预算限制。使用可观察的失败/等待与单次真实写入账单验证边界。
- 负缩放、UV 朝向及 GL front face 共同决定着色法线与覆盖。用非对称 normal map、相反的绕序和实际深度/颜色读回锁定结果。
- 即时 GL 中已发命令可能早于失败回执，回收和发布分别由真实完成与接受事实控制。用有前缀工作的失败检查以及在途换代证明两条寿命线。

## Migration Plan

现有模型、图像和 v1 场景经同一会话加载。开发验收将小场景放入 `build/d06-t05/` 的资源根，通过既有 `EditorSession::RequestSceneFile` 和 Runtime 帧消费，文件选择交互由 [D06-T06](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t06) 接续。按 [实现清单](tasks.md) 完成独立复核、最终 verify 和同步归档。
