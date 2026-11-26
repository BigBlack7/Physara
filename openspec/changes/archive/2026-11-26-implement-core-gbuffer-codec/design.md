# Design

## Context

当前 Forward 片元入口在逐实例分支和 discard 前取得梯度，随后采样材质、准备切线框架、过滤粗糙度并立即着色。`BuildPixelParams` 的结果仅保存派生波瓣值，原基色和金属度仍须从有效 MaterialInputs 取得。现有 RenderGraph 支持整数清除和类型化资源版本，OpenGL Pipeline 已校验片元输出与颜色附件的数值类别。新缺口在两种纹理格式的原生映射、整数读取的 ABI，以及连接这些能力的核心表面生产者。 [Forward 入口](../../../../Shaders/Passes/Forward/Color.glsl) · [材质派生](../../../../Shaders/Material/CoreSurface.glsl) · [图清除](../../../../Engine/Renderer/RenderGraph/GraphExecutor.cpp) · [原生格式表](../../../../RHI/OpenGL/Formats.cpp)

## Goals / Non-Goals

Renderer 可用同一 PreparedView、Draw 工作集和有效材质声明一次核心表面绘制，并将其结果交给本图的显式消费者。着色包、图入口与解码库是产品交付物。独立参考数据、读回用的消费者入口和样例归入工作包的 build 目录。SurfaceAux、屏幕空间位置重建、光照、编辑器路径请求及性能比较各由后继包交付。 [包边界](proposal.md)

## Decisions

### 图版本和最近核心表面由一次几何绘制产生

Renderer 持有 G0–G3 的附件顺序、格式和独立 D32 描述；接口生成器提供对应片元输出 location、数值类型和供生产/消费共用的字段、资格位约定，单份 codec 实现八面体映射与参数恢复。核心 Pass 从已准备的有效材质行选取 lit OPAQUE/MASK，按现有批次的几何、实例和材质绑定绘制；一个批次若跨越路由边界，应在形成可提交范围时区分。沿用已有间接参数、顶点/索引、材质纹理和缓冲的图读声明。一个图结果携带五个版本及有效矩形，后继显式声明所读版本。常规 Forward 帧无需请求这些附件。把数据暗挂在当前 View 的 Forward 输出会给双路径常规帧增加分配，并丢失生产版本身份。

首写全附件建立定义内容，G3 用 uint 0、深度用 0，G0–G2 使用明确的有限清值。光栅沿用现有 Reverse-Z、剔除及 MASK 语义。通过深度测试的核心片元写完整 G3 值 1。有限远平面可在 D32=0 通过 GreaterOrEqual，此时 G3 仍标记核心表面。背景清零或后继逐像素清除资格时，消费者跳过该像素的核心照明。以先绘后移的一次附件复用观察旧资格是否残留。原始 G0–G2 背景清色不构成材质身份。

### 存入与观察方向无关的核心字段

| 附件 | 写入 | 读取和误差依据 |
|---|---|---|
| G0 | 输出线性 baseColor.rgb 与线性 material AO。现有 framebuffer sRGB 状态只转换 sRGB 附件的 RGB。 | 取同像素解码线性 RGB。用独立 sRGB 分段曲线和 8 位码阶比较，AO 单独按 UNORM8 比较。见证值应能区分重复转换、覆盖 alpha 和 AO。 |
| G1 | 将完成 normal map 和双面处理的世界空间着色法线映射到有符号 oct XY；Z 为过滤后的感知粗糙度，W 为 metallic。 | oct 下半球折叠采用零分量取正的符号规则，包含负 Z 轴。反折并归一化后比较方向，Z/W 按 binary16 精度恢复。把按已过滤 p 派生波瓣的步骤供两条入口共享，保留正下限和 alpha=p²。 |
| G2 | 用现有工作光域的指数缩放安全地应用当帧预曝光，按 R/G 65024、B 64512 分通道限幅后写入。 | 同像素取值已经在预曝光域。由独立的 5 位指数、6/6/5 位尾数量化参考检查正常值、零与有限端点；留给 T03 的接口标明此域。 |
| G3 | 本版本明确写 bit 0 和零保留位，背景执行 typed clear。 | 按 texel 读取 16 位无符号值，以 bit 0 判定是否解释同像素的核心字段，D32 保留为几何深度。支持 T02 逐像素清除资格。 |

G0 保留原基色，可从量化基色与金属度恢复核心 F0、diffuseColor 及固定 F90。若存储 Forward 已经派生的 diffuseColor，纯金属会丢失恢复基色所需的信息。DFG 需要观察方向，在后继光照中求值。Forward 光照入口当前会预曝光传入的自发光，T03 消费 G2 时须直接合成已预曝光值。 [材质与曝光域](../../../../Docs/Architecture.md#materials) · [当前曝光调用](../../../../Shaders/Lighting/Punctual.glsl)

### 按需要扩展纹理格式与整数 fetch

在现有 OpenGL 格式表接入 R11G11B10_FLOAT、R16_UINT，复用目标用途查询、帧缓冲检查与片元输出数值检查。只按已验证用途报告能力。直接对这两个新格式做 staging 读回会额外扩大传输合同；验证可在片元按 texel 解码后写现有支持的 RGBA16F 或 R32_UINT 观察目标。

在既有图形接口中追加片元无符号二维 SRV 表达，沿用 t 类别和 samplerless fetch。生成 GLSL、包序列化及读回、ABI 身份与反射都保留形状及数值类别，保留既有枚举编码和浮点包解释。OpenGL 的独立 SPIR-V 反射需同步接受该声明，将数值类别带到活动绑定与实际 SRV 视图的核对，并沿用现有的 nearest dummy sampler。仅放行源语法会在包验收或原生 Shader 创建时被拒绝；改用浮点 flags 会改变固定布局。 [Shader 接口](../../../../Engine/Shader/Interface/README.md) · [包侧反射](../../../../Engine/Shader/Spirv.cpp) · [原生反射](../../../../RHI/OpenGL/Shader.cpp)

## Risks / Trade-offs

- **编解码同时写错仍能往返。** 原生消费记录原通道与解码结果，对照独立输入。常规 RGB 在 sRGB 码域最多偏 1 码，AO 在 8 位域最多偏 1 码；G1 标量及 G2 的可表示范围内样点允许目标格式相邻 1 个码阶，正负半球的选定法线样点角差上限 0.25°。G2 有限上界、G3 和解析覆盖的预期精确匹配。将观察目标自身的量化单独计入或使用整型输出传递浮点位型。这些是 T01 字段级门槛，T05 的整帧 HDR 差分另按场景登记。
- **驱动报告单项能力，但混合 MRT、整数 fetch 或绑定失配处理未生效。** 同一真实 GL 会话完成写入、取样和读回；用非平凡整数模式与故意互换的视图区分归一化、类型误接和正确拒绝。兼容性复用现有浮点正式包，不增加 GPU 性能门槛。
- **共享材质重构改变已交付路径。** 只抽取同义字段派生和已需共享的求值逻辑；用受影响的 Forward/Forward+ 正式包重建并以已有匹配输入核对代表样点。 [D10 参考](../../../../Docs/Performance/ForwardPlusReference.md)

## Migration Plan

先接入格式和 Shader ABI，再构建正式核心包、Pass 与解码库；普通产品构建从受版本控制的输入生成所需产物。采用增量的包类型标识并用既有浮点包验证兼容。D11-T01 由显式图调用启用，后续包才接到 View 的路径选择；当前场景文档无需迁移。
