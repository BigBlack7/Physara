# Design

## Context

当前 Forward 在同一离屏画布声明 RGBA16F 工作色、D32 深度和 SRGBA8 显示色。天空在不透明颜色后写 HDR，`ForwardDisplay` 用 `texelFetch` 逐像素限幅。Editor 从面板和实际 framebuffer/client 比率得到乘过 render scale 的单一范围，Runtime 只在正常合成消费匹配颜色且提交获接受后更新 View，失败时走同一 token 的安全 UI。 [提案](proposal.md) · [现有显示](../../../../Shaders/Passes/Forward/Display.frag) · [布局换算](../../../../Editor/Viewport/Viewport.cpp) · [帧确认](../../../../Runtime/Frame.cpp)

## Goals / Non-Goals

交付 Renderer/PostProcess 可消费的单样本 HDR → SDR 合同。场景与相机继续只提供本帧物理输入，PostProcess 只拥有显示变换所需资源，View 保存用户请求和实际接受记录，Editor 及 Platform 分别承担线性合成和窗口呈现。前向阶段直接交付的 HDR、深度和投影足以用于数值比较。

可编辑 ColorGrading、HDR 显示设备、历史及镜头效果、通用截图交付由各自工作包定义。本包固定显示参数，只开放 None/FXAA 和现有渲染比例入口。 [阶段边界](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d07-t05) · [显示输出](../../../../Docs/Architecture.md#display-output)

## Decisions

<a id="fixed-profile"></a>
### 1. 固定线性输出的 ACES 查表

沿项目固定 Filament 快照使用 `ACESToneMapper` 及 `gamutMapping_sRGB` 对应路径。CPU 查表节点按以下合同求值。linear Rec.709/D65 转至 linear Rec.2020，中性白平衡和创作参数取恒等值，应用 ACES 的单位亮度倍率，转回目标 Rec.709/D65 并做目标色域映射，最后限在 SDR [0,1]。选用线性输出，额外曝光乘数为 1，luminance scaling 和抖动关闭。CPU LUT 包含成对的原色矩阵、色调映射和色域处理，场景 Shader 继续在同一工作原色和预曝光域。 [架构约定](../../../../Docs/Architecture.md#conventions) · [固定 ToneMapper](https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/ToneMapper.cpp#L377) · [固定 ColorGrading](https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/details/ColorGrading.cpp#L1069) · [色域映射](https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/ColorSpaceUtils.cpp#L263)

- HDR shaper 取固定 Filament LogC 的正向曲线 `s(x) = 0.244161 × log10(5.555556 × x + 0.047996) + 0.386036`，在 Physical Lighting 输入时将各通道限制到 `[0, s⁻¹(1)]`，上端约为 58.8566。查表坐标将 `[s(0), 1]` 重映射到 `[0,1]`，令黑对应首节点。CPU 以反函数解码均匀采样节点，GPU 以同一曲线编码。该零点归一化是 Physara 的 LUT 约定，色调算子及固定色域参考使用同一上游版本。上端截断只作用于显示查表，HDR 源保留附件可表示的超白。 [LogC 曲线](https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/materials/colorGrading/colorGrading.fs#L1)
- 采用 64³ 个线性节点，RGBA16F 单 mip 512×512 atlas，蓝轴 64 个切片按 8×8 排列，每片两个轴分别是 R/G。该布局复用正式二维 Shader 反射、纹理上传和双线性采样。每次在相邻两个 B 切片内按 R/G 的有效 texel center 双线性取样，再按 B 插值。明确形状和范围便于独立核对切片拼接、暗部与饱和通道，保留现有二维包合同。 [Shader 生成约定](../../../../Engine/Shader/Interface/README.md) · [固定显示要求](specs/display-post-processing/spec.md)
- Renderer 的设备级 PostProcess 资源拥有一个固定 profile 的 LUT，首次实际消费时构建 CPU 数据和 GPU 候选。复用图上传及提交确认，只有完整上传和消费获正常提交才进入可复用状态。取消时旧有效图继续只读，未确认资源的存活及恢复沿已有图票据合同。曝光、AA、场景和尺寸变化重选当帧输入，共用同一固定 LUT。生成失败及设备能力不足沿 View 拒绝原因报告。 [已有上传与确认](../../../../Engine/Renderer/Paths/Forward.cpp)

<a id="image-layout"></a>
### 2. 同一投影的渲染与输出布局

Editor 从封存的逻辑内容宽高和实际 framebuffer/client 比率得到未取整的 `P`，逐轴换算后封存 `output = floor(P)` 和 `render = floor(P × renderScale)`，使用现行 [0.5,1] 合法比例。View 以相同相机输入在两个栅格各自适配。固定 aspect 沿现有整数 FitImage，奇数余量在右或下。导航和无显式 aspect 的透视使用整个栅格，投影 aspect 取显示画布。HDR、天空、覆盖、D32 和采样偏移使用实际渲染矩形。显示矩形使用相同归一化图像坐标，逻辑输入由封存布局直接映射到已接受显示矩形；半开边界和留边拒绝命中。无场景测试图沿用缩放后的栅格及其接受规则。窗口尺寸仍独立由宿主选择。 [布局增量](specs/view-preparation/spec.md) · [已实现相机准备](../../../../Engine/Renderer/Prepare/ViewPreparation.cpp)

在所属边界按有限正尺寸、渲染目标与纹理设备能力、各阶段真实格式及存在的附件尺寸做有界像素和字节检查。包括显示目标、HDR/D32 及需要的中间 LDR，按已启用的图路径计费。零尺寸、隐藏或不支持的请求沿既有暂停、占位或安全 UI 处理。独立显示布局避免由 ImGui 隐式放大低分辨率 ViewOutput，也为后续显示线性叠加保留准确坐标。 [视口增量](specs/editor-viewport-output/spec.md) · [图预算入口](../../../../Runtime/Frame.cpp)

<a id="fxaa-and-output"></a>
### 3. 保持显示计算域与取样顺序

Forward 声明场景、深度与天空版本，PostProcess 消费最后的 HDR 颜色，图显式声明工作色及 LUT 采样、各显示阶段的读写和最终 ViewOutput。正常路径顺序为固定显示变换或诊断限幅 → 可选 FXAA → 可选双线性缩放 → 显示色写入。None 跳过 FXAA，渲染和显示的有效矩形同位置同尺寸时为逐像素映射。尾部阶段直接写最终 SRGBA8，只有存在后继计算时使用 RGBA16F 中间色。图像外给既有场景底色 `linear RGB = 0.018`、alpha 1，矩形内的输出 alpha 为 1。 [现有底色及工作附件](../../../../Engine/Renderer/Paths/Forward.cpp) · [显示后处理增量](specs/display-post-processing/spec.md)

- FXAA 采用固定快照的 opaque FXAA 3.11 分支与对应锐度和亮度阈值，按现有 preset 取 8、0.08、0.04。Physara 以线性 Rec.709 的亮度开平方近似感知亮度，必要时通过中间纹理 alpha 交付该标量。边缘方向和阈值消费该标量，所有 RGB 过滤在线性显示域。使用实际渲染栅格的 texel 步长，采样坐标夹在有效矩形的首末 texel center。固定色块保持原色，矩形外的底色由终段写入。 [固定 FXAA 参数](https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/materials/antiAliasing/fxaa/fxaa.mat#L39) · [FXAA 输入约定](https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/materials/antiAliasing/fxaa/fxaa.fs#L131)
- 空间缩放按目标有效矩形内的像素中心映射归一化 UV，再映射来源有效矩形内双线性取样。后处理终点是 ViewOutput。存储写入由 SRGBA8 编码，UI 采样返回 display-linear，再按自身顶点和图标色彩解码、直通 alpha 合成。窗口依据实际后缓冲编码呈现并维持逻辑行序。 [颜色合同](../../../../Docs/Architecture.md#conventions) · [UI 合成](../../../specs/editor-ui-rendering/spec.md) · [窗口呈现](../../../specs/opengl-window-presentation/spec.md)
- 供比较的 HDR 保持图上显示前的 `workingColor`、配对 D32 和实际渲染快照。需要读回时从其授权图输出取 RGBA16F，并随样本记录 preExposure、工作原色和渲染矩形。比较出口按需消费，不为普通帧保留额外读回缓冲或跨帧 HDR 副本。 [现有 HDR 验证](../2026-11-18-add-forward-environment-lighting/tasks.md#final-verification)

<a id="accepted-settings"></a>
### 4. 请求、回执和构建入口

View 拥有类型化 AA 请求与文档 revision，EditorSession 在 v1 边界映射 `mainView.requested.antiAliasing` 的 `none` 或 `fxaa`。省略采用 None；显式错误按原路径消息拒绝 Open，保存只捕获请求。AA 或布局变更通过现有待确认流程进入下一帧。物理场景用实际 None/FXAA，无场景测试图只发布测试图，材质等诊断预览固定使用 None 并显示 FXAA 请求当前未被消费。 [v1 增量](specs/scene-document-persistence/spec.md) · [Forward 增量](specs/static-forward-rendering/spec.md)

与 View、Scene、相机、请求代和两个矩形均匹配且实际被正常合成采样后，接受的提交才同时确认实际显示 profile、AA、布局和相机。缺包、实际能力、上传或图执行失败沿现有原因路径报告，用安全 UI 保留同一 View 最后接受的图及有效信息，当前图像命中暂停。取得可靠提交和回收完成是不同事件。继续按完成票据持有旧纹理，出现布局变化、候选被拒或替换时校验相应资源代。 [接受规范](specs/editor-viewport-output/spec.md) · [稳定 token 合同](../../../specs/editor-viewport-output/spec.md)

新增显示、FXAA 和缩放着色入口沿当前 ShaderBuild 声明生成、活动反射和逐配置包。PostProcess 在实际需求出现时取得匹配包和绑定，Runtime 通过原生 exe 相对路径提供经配置的包地址。固定 ACES、色域及 FXAA 的移植只保留所用代码与必要的传递输入、版本和许可，按项目已有位置记录调整及来源。 [构建接入](../../../../Runtime/CMakeLists.txt) · [移植约定](../../../../ThirdParty/README.md)

## Risks / Trade-offs

- 源域、矩阵方向、LUT 轴或插值偏差会改变肤色、原色和高亮 → 用固定上游直接函数建立中性、非对称饱和及查表非网格参考，拆分 LUT 误差与存储量化。
- FXAA 错读 RGB 编码值、采到留边或放大后才过滤会改变边缘 → 用同场景 None/FXAA、低亮度斜边与固定画幅，在实际 GL 读回源和输出，排除重复转换。
- 输出范围与渲染栅格取整不同、包不可用或候选未确认时容易产生错代图像 → 用一个奇数尺寸与一次同尺寸请求失败复核范围和回执，覆盖已接受旧图、正常恢复和实际 UI 显示。

## Migration Plan

默认 Base Color 和 None 的 v1 文件可按原入口打开。保存缺省 None 时可省略新字段，使用 FXAA 的文件由本版读取请求。部署按配置配套构建 Physara、生成接口和 Shader 包。回退在同一版本组合恢复程序和包，交换给旧读者的 v1 文档使用省略 AA 字段的副本。 [构建与运行入口](../../../../Docs/Build.md)
