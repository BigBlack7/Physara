# Design

## Context

[提案](proposal.md)关联 D07-T04。当前 Forward 在冻结的 View 工作集上准备 Draw 和灯表，`ShadeOpaque` 写 `RGBA16_FLOAT` 和 D32，`ForwardDisplay` 消费工作色。共享 BRDF 已定义 DFG 能量项，正式 Direct Lighting 使用单次散射。AssetManager 已有环境编码和标定值类型，环境像素在请求入口被拒绝。RHI 能分配 cube 存储及上传单面，生成接口、活动反射、采样视图及 Runtime 图 profile 目前以二维消费者为主。 [颜色入口](../../../../Shaders/Passes/Forward/Color.frag) · [共享 BRDF](../../../../Shaders/BRDF/CoreBrdf.glsl) · [资产请求](../../../../Engine/Asset/AssetManager.cpp) · [GL 采样视图](../../../../RHI/OpenGL/Bindings.cpp) · [Runtime 图](../../../../Runtime/Frame.cpp)

## Goals / Non-Goals

**Goals**  
使正常产品入口从 v1 文档、不可变源和图内资源到 HDR 像素可追溯。冻结实际被消费的环境角色、朝向、标定、强度及相机曝光，以同一帧回执确认它们。

**Non-Goals**  
D07-T05 持有显示变换与 FXAA，D07-T06 持有创作界面与参考场景。首版图形能力限于本包消费者需要的单样本、非数组颜色 cube，预计算 profile 限于静态远场环境和核心各向同性 BRDF。 [阶段边界](proposal.md#what-changes)

## Decisions

<a id="source-and-scene"></a>
### 1. 场景保存请求，资产发布线性源

- `Editor/Session` 将 v1 可省略的顶层 `environment` 读成类型化请求。对象使用 `path`、`rotation`、`intensity`、`lighting`、`sky` 五个字段。旋转按现有 Scene xyzw 单位四元数合同校验，表示 `worldFromEnvironment`。强度为无量纲非负值，GPU 编码须保有零和正值语义。至少显式启用一个角色。Scene 持有请求和固定 CPU 版本，文档快照写回请求。来源标定、进程内 revision 和 GPU 资源由实际所属对象交付。 [场景持久化增量](specs/scene-document-persistence/spec.md) · [现有旋转合同](../../../../Engine/Scene/SceneTransform.cpp)
- 环境属于现有 Open 的必要 CPU 依赖。复用 `ResourcePath`、有界读取、源哈希及 `SceneOpenInputs` 的请求/源核对。缓存中的旧内容与磁盘不符时按外置图像的现有方式重取，场景只在完整候选成功时替换。保存记录环境路径及参数，重开时从当前实际源取得版本。CPU 发布后的烘焙和 GPU 准备在视口中继续。 [场景依赖准备](../../../../Editor/Session/SceneDocumentAssets.cpp) · [资源路径与读取](../../../../Engine/Core/ResourceIO.hpp)
- 为 `Assets/Environments` 提供类型化环境请求并共用现有 CPU 资产准入。私有链接仓内 TinyEXR，先从同一受控字节快照解析 header，再验证单 part、scanline、RGB 或 RGBA、完整通道采样和 2:1 数据窗口。处理 ZIP、PIZ 及当前测试使用的未压缩 scanline。从窗口高度和压缩方式推导块数，显式 `chunkCount` 须与之相符。核对实际偏移表范围，并把偏移表、平面和 RGB 输出计入解码预算。按固定 TinyEXR 返回的数据窗口坐标交付顶部起行的线性 float RGB。丢弃覆盖率语义，有限负分量在导入处归零，拒绝非有限像素。仓内三份源的头均为 2048×1024、FLOAT RGBA，覆盖 ZIP 和 PIZ，源文件均少于 32 MiB，单份解码的四通道平面数据为 32 MiB。 [环境导入增量](specs/cpu-asset-publication/spec.md) · [固定解码库](../../../../ThirdParty/README.md) · [现有导入额度](../../../../Engine/Asset/AssetManager.hpp)
- 缺席色度采用线性 Rec.709/D65 的导入缺省。显式的其他色域给出不支持原因。合法 `whiteLuminance` 记录文件声明的单位白亮度 `cd/m²` 和来源，以该值将单位 RGB 换算到标定亮度。缺席则标记 Relative，倍率为 1，场景强度继续由创作输入决定。现有三份资产均缺色度与 `whiteLuminance`，应显示 Relative。 [色彩域](../../../../Docs/Architecture.md#conventions) · [物理输入](../../../../Docs/Architecture.md#physical-inputs) · [OpenEXR 可选属性](https://openexr.com/en/rb-3.1/StandardOptionalAttributes.html)

<a id="bake-profile"></a>
### 2. 在有界 CPU 冷路径生成匹配的环境产品

- 采用 `Renderer/Lighting` 的进程内 `EnvironmentCache`，复用 Core 的有界 JobQueue。worker 从固定 CPU 版本生成纯 CPU 数据，主线程按当前请求接收，RHI 资源只在 owning context 上准备。缓存以实际源 SHA-256、投影和色彩解释、分辨率及 mip、过滤与采样设置、算法及工具版本为身份。朝向、强度、曝光随冻结的准备值交付。图内逐面预计算需要专用 Pass、管线、临时面和同步，当前消费者使用可独立核对的 CPU 数据与现有图上传。 [环境准备合同](../../../../Docs/Architecture.md#light-preparation) · [Core 作业](../../../../Engine/Core/Jobs.hpp)
- 固定首版基础 cube 每面 256×256。等距柱状输入在环境空间用 `u = 0.5 + atan2(x,z)/(2π)`、`v = 0.5 − asin(y)/π` 查源，沿经度环绕、纬度夹边。采用 cube 的 `+X, −X, +Y, −Y, +Z, −Z` 顺序及标准方向到 texel 映射，在一个位置定义逻辑行 0，源接缝和跨面过滤按相邻方向处理。天空角色使用基础层。照明角色使用该基础层派生的 GGX 产品，层级固定为 256、128、64、32、16。 [Filament 等距柱状转换](https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/libs/ibl/src/CubemapUtils.cpp#L181) · [OpenGL 4.6 §8.13](https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf)
- 令目标 mip 为 `j`，`maxMip=4`，烘焙用 `p_j = 1−sqrt(1−j/maxMip)`、`alpha=p_j²`。第 0 层为基础数据。余下各层按固定 Filament 路线预准备 Hammersley 半角序列，从 `N=V` 反射得 L，保留 `NoL>0` 的样点并以 NoL 加权归一化。每层采样数依次为 1024、2048、4096、8192。从基础 cube 的抗走样输入层级按样本与源 texel 立体角比计算采样 footprint，LOD 偏置沿固定参考的 `K=4`。源层级为积分取样服务，目标层级的内容来自卷积。运行时三线性查询 `maxMip×p×(2−p)`。 [固定粗糙度映射](https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/tools/cmgen/src/cmgen.cpp#L908) · [固定 GGX 积分](https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/libs/ibl/src/CubemapIBL.cpp#L355)
- 用 cube texel 立体角在 double 中投影实 SH，保留 `l=0..2`。以 `1、2/3、1/4` 对应余弦卷积及 Lambert `1/π`，将基函数归一化并入九个 RGB 系数，Shader 按同一排列重建并将截断振铃的负结果归零。天空和照明消费各自产品，可共用来源及 CPU 基础层。仅实际需要照明时请求其 GGX 和 SH 产品。 [固定 SH 计算](https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/libs/ibl/src/CubemapSH.cpp#L511)
- 天空与镜面 cube 采用线性 `RGBA32_FLOAT`，作为有限的 HDR RGB 及常量 alpha 上传。两种角色都用时约 14 MiB 的紧密 cube 数据。以实际字节和子资源数计入 CPU 工作及 GPU 驻留/上传预算，沿现有准入区分暂时占用与单次超限。限制进行中的烘焙，合并同一键的需求并保留当前、候选和仍被帧持有的产品。空闲产品可按预算淘汰。CPU 工作在阶段、面或行批次检查停止，关闭时先停止并收拢作业，再释放 owner。该预计算保有量是尺寸推算，首包仍需按真实峰值核对暂存和对齐。 [Forward 驻留与上传](../../../../Engine/Renderer/Materials/StaticGpu.hpp) · [设备上传限制](../../../../RHI/OpenGL/Upload.hpp)

<a id="dfg-profile"></a>
### 3. DFG 与环境源生命周期独立

- 固定 128×128 的线性半精度 DFG，RGBA 的 R/G 分别保存 B、A+B，蓝通道保存参考 cloth 积分，A 为固定填充。以每个 texel 中心的 `NoV` 和 `p` 积分，逻辑上传行按 p 递增。用匹配 T01 的 GGX、完整 Smith、Schlick 及 `alpha=p²`，沿固定参考以 1024 个样点积分 GGX，4096 个样点积分 cloth。用同一 profile 身份生成一次 CPU 候选，由同设备的物理帧共享 GPU 代。复验打包后的 `0 ≤ R ≤ G ≤ 1` 且 G 为正有限数。 [固定 DFG 生成与通道](https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/libs/ibl/src/CubemapIBL.cpp#L789) · [固定 DFG 格式](https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/DFG.cpp#L41) · [T01 能量合同](../2026-11-17-add-core-surface-brdf/design.md)
- GLSL 用 `CoreNormalViewCosine` 与 `PixelParams` 的过滤后 p，进行夹边双线性查表。CPU 积分参考和 GPU 读回用同一个实际采样网格与行序，端点处以实际边缘 texel 中心做过滤预期。新表与环境角色的上传各有依赖。场景有受光 Draw 时，Physical Lighting 无环境也消费匹配 DFG。 [核心 DFG 规范](specs/environment-lighting/spec.md)

<a id="sampled-cube"></a>
### 4. 贯通真实 cube 采样维度

- 权威 schema 增加 `texturecube`，用生成的 GLSL `textureCube` 与独立 Sampler。阶段反射、ABI 和 Shader 包保留图像维度，按 SPIR-V 实际的 Dim2D 或 DimCube 校验。公共绑定仍使用 `Texture_SRV` 及已有视图维度字段。沿实际活动资源建立紧凑 GL 采样对。GL 在创建时验证单 cube 的六面和选定 mip，状态验证时检查活动图像目标与原生视图一致。基础 cube 和预滤波 cube 使用实际格式/目标的采样能力，跨面线性过滤由现有 GL context 状态 owner 管理。 [生成接口](../../../../Engine/Shader/Interface/README.md) · [SPIR-V 校验](../../../../Engine/Shader/Spirv.cpp) · [GL 视图](../../../../RHI/OpenGL/Bindings.cpp)
- 由 OpenGL 后端已有的目标能力表经 Platform 窄入口向 Runtime 提供真实 cube 尺寸和采样格式标记。物理图 profile 补齐 6 面的格式/维度能力，图按确切 mip 和面授权并物化 cube 视图。扩展现有 `AddTextureUpload` 交付 mip 与 face，声明 `CopyDestination` 的精确范围，执行写相同子资源。环境纹理写进入现有纹理上传段。显示或着色 Pass 读取所有实际可采样层，依照写后的图版本和现有状态交接；设备和图共用已观察的能力。 [图编译合同](../../../../Docs/Architecture.md#graph-compile) · [图上传入口](../../../../Engine/Renderer/FrameGraph.cpp) · [目标格式能力](../../../../RHI/OpenGL/Formats.cpp)
- 同一颜色源及共享 include 供既有预览、DFG 物理着色和依赖 cube 的 IBL 变体。变体只表达活动资源差异。继续在四组逻辑集合的限制内使用现有 PerPass 集合，set 3 按实际活动布局交付灯、DFG、环境常量和采样对。沿同一生成 schema 派生布局。共享 ABI 的颜色包在创建 RHI Shader 前核对已验证的片元活动 set 3 是否完整匹配请求的角色。Depth 使用实际深度资源集合。天空使用自己的薄 Pass 接口。朝向、强度、曝光和粗糙度仍作为数据。按真实需求载入包、创建 Pipeline 和准备绑定。 [现有四组规则](../../../../RHI/OpenGL/BindingPlan.cpp) · [Forward 程序及布局](../../../../Engine/Renderer/Materials/StaticGpu.cpp) · [正式构建](../../../../Engine/CMakeLists.txt)

<a id="physical-forward"></a>
### 5. 在共享着色和天空 Pass 消费当帧输入

- 现有 View Settings 增加运行期 `Physical Lighting`。在现有覆盖和几何粗糙度导数约定上构造一次 `PixelParams`，用共享 BRDF 求值和已转换的 DFG 缩放直接镜面项，逐灯继续用冻结的完整候选与一次余弦。直接光诊断仍使用其现有已验收入口。IBL 使用法线和基底镜面方向，镜面用固定 Filament 主导方向近似 `mix(reflect(-V,N),N,alpha²)`，两者统一用 `environmentFromWorld` 变换后查询。漫反射用 SH9、漫反射色和 `1−E`。SH9 按 RGB 分别归一化，Shader 将各通道还原指数与环境倍率及预曝光合并。镜面使用预滤波入射量和 `E×C`。 [核心表面规范](../../../specs/static-surface-evaluation/spec.md) · [固定主导方向](https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/shaders/src/surface_light_indirect.fs#L115)
- 缺席的屏幕 AO 使用单位可见性，材质 AO 复用已采样的 occlusion 和 strength。间接漫反射采用固定 Filament 多反弹 AO 近似，权重限制在 [0,1]，间接镜面采用 Lagarde 遮蔽。缺席材质槽的单位可见性使两者返回单位权重。自发光和各灯保持 T03 预曝光。烘焙值留在线性源域，环境与天空各乘一次文件标定、场景强度和同帧预曝光。沿现有工作色范围处理极值，显式保护到实际格式的窄化、乘积和累加。 [AO 固定参考](https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/shaders/src/surface_ambient_occlusion.fs#L85) · [光照合同](../../../../Docs/Architecture.md#lighting)
- 在 `ShadeOpaque` 后将 `Sky` 声明为加载上一 HDR 颜色的部分写，`ForwardDisplay` 读取该图版本。工作色 alpha 用作不透明覆盖，清除为 0，有效几何写 1。天空仅在有效图像矩形内以 `srcRGB×(1−dstAlpha)+dstRGB×dstAlpha` 混合并写 RGB。沿设备查询检查 `RGBA16_FLOAT` 混合能力。恰在有限远平面的几何可写有效的 D32 零，覆盖位保留其工作色并让 MASK 孔洞使用天空。相机透视射线由当前渲染投影逆变换得到，正交使用固定前向，方向经世界朝向和环境逆旋转查询基础 cube。有效天空是 Runtime 进入正常 Forward 图的一项独立需求，覆盖没有几何的天空场景。 [天空组装顺序](../../../../Docs/Architecture.md#paths) · [当前深度比较](../../../../Engine/Renderer/Materials/StaticGpu.cpp) · [Forward 显示](../../../../Shaders/Passes/Forward/Display.frag)

<a id="acceptance"></a>
### 6. 按实际用到的角色确认完整代

- SceneFrame/PreparedView 冻结场景、请求及不可变 CPU 版。实际查询参数、当前预曝光及所需角色跟随本轮帧准备，进入图的各产品来自确切请求身份。仅有天空或场景内暂时没有可见受光 Draw 时，实际活跃资源范围由 Sky/Color 消费决定。朝向、亮度及有效源代按渲染内容合同传播，场景请求的持久化身份与渲染确认分别由 Scene 和 View 管理。 [场景内容版本](../../../../Docs/Architecture.md#static-content) · [帧槽合同](../../../../Docs/Architecture.md#frame-state)
- 首次体化使用独立候选分配。使用者读到完整写入后版本，未接受的候选不得以已初始化的资源导入。图执行和接受前沿帧槽强持有实际资源。在发布点核对匹配的当前请求、存活的必要上传、正常图实际采样及可靠回执，再一并确认 View 输出和环境代。RHI 票据保活所有已发工作的资源到真实完成。候选失败或撤销时沿现有前缀退出和重试协议处理，旧已接受代的内容和保活独立。 [Forward 上传确认](../../../../Engine/Renderer/Paths/Forward.cpp) · [Runtime 回执](../../../../Runtime/Frame.cpp) · [退休合同](../../../../Docs/Architecture.md#retirement)
- View 状态分别显示请求和已接受的路径、源代、指纹、旋转、强度、标定及用途。安全 UI 保留上一正常图及其相机记录，视口标签同时显示当前等待或拒绝原因和旧图来源。正常烘焙等待只更新状态。worker 启动失败走环境拒绝通道。匹配的新候选完整接受时，环境和相机、模式、曝光一起更新。场景省略环境时按明确无环境交付受光画面。 [视口增量](specs/editor-viewport-output/spec.md) · [现有接受合同](../../../specs/editor-viewport-output/spec.md)

## Risks / Trade-offs

| 风险 | 控制及证据 |
| --- | --- |
| EXR 解码解释、磁盘源变化和解码峰值 | 复用受控字节快照和当前操作校验，先确认 header 与实际分配上限。用仓内两种压缩和一份已知超白的标样验证。 |
| GL 接受错误形状或采样到未定义的面、层级 | 真实目标能力和视图维度贯穿授权及图形状态。原生验证合并六面、mip、接缝及一次代表性拒绝。 |
| DFG 轴向、SH 归一化、卷积或曝光重复 | 用解析方向反照率、非对称环境和固定曝光比较工作色，选择能区分同一模型错接的点。 |
| 首次烘焙和同帧上传有实际容量开销 | worker 有界准备，复用已接受资源。显式检查含环境首帧的实际准入，用既有 Pending 和永久超限诊断。 |
| 天空覆盖远平面或新旧环境混合 | 检查有限远平面、MASK、有效画幅、两种深度组合及一次 A 到 B 的未确认与恢复路径。 |

## Migration Plan

已有 v1 文件按省略环境恢复。带环境的 v1 使用本次指定的新增字段。直接光、其他诊断和 Base Color 初始选择沿现有入口，新增模式由用户选择。正式构建在各配置生成与消费相符的包和接口，运行说明随实现记录支持的 EXR profile、场景字段、视口状态及预曝光输出。回退时配套恢复同一代程序、生成接口和 Shader 包，旧版本可继续读取省略 `environment` 的 v1 文档。 [已声明的构建入口](../../../../Docs/Build.md) · [场景文档兼容](specs/scene-document-persistence/spec.md)
