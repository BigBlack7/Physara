# Design

## Context

`ViewWork::Prepare` 已按同一冻结 `SceneFrame` 交付相机、有效渲染矩形、稳定裁剪范围和可见灯索引。当前色彩 Pass 使用完整灯表，其方向灯和局部灯各自成段，阴影引用使用同一个灯行顺序。Froxel 在这份视图工作集上增加候选选择。 [立项](proposal.md) · [View 准备](../../../../Engine/Renderer/Prepare/ViewPreparation.cpp) · [灯表](../../../../Engine/Renderer/Lighting/PunctualLights.cpp) · [阴影引用](../../../../Engine/Renderer/Draw/StaticDraw.cpp#L1073)

## Goals / Non-Goals

### D10-T01 交付

- 交付按需调用的 CPU 分箱入口，使用正式 View 准备的只读输入。结果足以用生成的数据布局查找 cell、解析候选行并报告当前不可用原因。
- 固定 CPU 与 Shader 的坐标、边界和数据语义，并能用已生成接口及独立样点核对。 [Froxel 规格](specs/froxel-light-preparation/spec.md)

### 后续工作包

- D10-T02 以相同 ABI 接续预算退路、图内上传及统计。D10-T03 使用共同着色入口消费候选，D10-T04 管理活动计划与历史，D10-T05 完成路径 HDR 和成本验收。 [开发计划](../../../../Docs/Developments/D08-D11-RenderPaths.md#d10-t02)

## Decisions

<a id="view-inputs"></a>
### 使用同轮 View 工作集

`Renderer/Prepare` 沿 `ViewWork` 的正式入口提供相机、可见灯、`layouts.render.rect` 和已求值的稳定侧面范围。`Renderer/Lighting` 从只读输入构造每 View 结果，借用只覆盖所属 `FrameSlot` 的当次使用。分箱在显式需要候选时调用，普通 Forward 从同一准备值取得完整灯表。同轮多个 View 只提取一次场景，候选随各自相机、矩形和采样范围生成。首包按正确性重新生成需要的工作集，静态资产、场景记录和阴影保留各自的现有 owner。 [帧槽合同](../../../../Docs/Architecture.md#frame-state) · [场景快照合同](../../../specs/scene-frame-preparation/spec.md)

<a id="stable-grid"></a>
### 让 CPU 和 GLSL 查询读取同一份边界

- 网格由有效渲染矩形的左上起算，XY 按像素 tile 划分，余数归最后一列或行。CPU 用稳定投影的 X/Y 尺度生成 tile 侧面。最外侧 cell 延伸到已有采样 profile 的 guard band。查询用 `renderPixel - renderRect.origin - jitterPixels` 得到稳定像素。下界为零时，先用 FP32 位型的顺序比较去原点后的坐标与抖动量，零原点直接取画布输入位型。这样能在设备将次正规数清零之前判定零外缘。有留边时，覆盖内的负稳定坐标可归入边缘 cell。超出扩展范围返回无有效 cell。
- Z 使用米制正向视深 `d = -viewZ`。深度端点取相机的 `nearPlane` 和 `cullingFar`。透视以两端的对数值插值求各层边界，正交直接对深度插值，零近面沿线性分支处理。边界先用高精度计算，再发布为严格递增的 FP32 值。端点的舍入须保守覆盖源区间。CPU 分箱使用已发布边界并计入表示误差。转换后出现非有限、端点无法保守表示或边界重合时，返回本轮网格不可用原因。
- GLSL 对已发布深度边界作有界查找。区间内部为 `[edge[i], edge[i+1])`，最后一层包含有效远面。CPU 对光体相交使用 cell 闭包，使按查询规则选中的任一相邻 cell 都保留必要候选。cell 按 X 最快、其次 Y、最后 Z 顺序展开，线性下标为 `(z * tileCountY + y) * tileCountX + x`。
- 查询的深度输入为线性视深，后续颜色消费者从自身当前投影取得该值。CPU 和 GLSL 先按 FP32 位型拒绝负的非零视深，使设备在浮点比较中将次正规数当作零时也保持相同边界。正交零近面接受正负零。有效性先于取整和数组访问。T01 的验证消费者使用明确的渲染像素与视深输入，T02/T03 验证真实片元位置与投影输入接入。 [坐标约定](../../../../Docs/Architecture.md#conventions) · [稳定相机](../../../../Docs/Architecture.md#view-preparation)

共享边界表让 CPU 与 Shader 在高速变化的视深区间也比较同一批表示值。深度表随 View 参数一并生成，查询成本计入后续同画质对比。Filament 的固定版本提供 CPU 候选构造与记录压紧参考。 [Filament Froxelizer](https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/Froxelizer.cpp#L731)

<a id="candidate-rows"></a>
### 沿同一灯行顺序保守分箱

1. 以 `PreparedView::lights` 解析只读灯记录，用 `LightRowCursor` 的分段顺序取得绝对 GPU 灯行。方向灯在完整灯表前缀求值。无显式 range 的局部灯按行号写入全局段，有限局部灯进入分箱。后续 `EvaluateDirect` 可用同一行访问灯数据和阴影引用。 [完整灯表](../../../specs/punctual-lighting/spec.md) · [着色消费者](../../../../Shaders/Lighting/Punctual.glsl)
2. 每个有限灯先以影响球缩小潜在 Z 和 XY 区间，再对 cell 的保守平面作相交判定。有界聚光以外锥与径向范围的交集求保守支持边界，可借用 `ShadowVolumes` 的几何支持计算。灯在眼点后方但范围跨越近面时，候选范围以裁剪后的正向视深估算。直接光的范围和外锥依据灯记录及着色编码，阴影投影距离、近面和阴影准入状态各自服务阴影生产。
3. 相切或无法稳健证明分离的 cell 保留候选。保守误差考虑视空间变换、GPU 范围及角度的窄化。外半锥角接近或等于直角时仍使用放宽后的锥缘支持距离。查询方向先减去最近的极轴，再经两次叉乘取得横向支点方向。正背向的零横向分量使用任一等价锥缘方向，返回的距离取实际支点在查询方向上的投影。灯行按现有顺序遍历，cell 与全局索引各自递增且互斥。 [半空间计算](../../../../Engine/Renderer/Prepare/ViewFrustum.hpp) · [聚光支持边界](../../../../Engine/Renderer/Lighting/LightVolume.hpp) · [glTF 灯范围及外锥](https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_lights_punctual#range-property)

<a id="bounded-abi"></a>
### 在现有声明与容量内发布完整数组

| 数据 | 含义 |
|---|---|
| 每 View 网格参数 | 投影类型、有效渲染矩形、当前 jitter、稳定外缘、tile 和层数、有限视深边界及有效索引数量 |
| Cell header | `uint32` 的 `offset`、`count`、`overflow`。`offset` 是有限索引段内的条目下标。每个有效 cell 有一条 header，空 cell 的 `count` 为 0 |
| 有限索引段 | 按 header 的 `offset` 与 `count` 读取 `uint32` 灯行号，每个 cell 内行号递增 |
| 全局局部灯段 | 无显式 range 的 `uint32` 灯行号，按当前 View 的有效全局数量读取 |

复用 `StaticForward` 的权威接口声明和既有 `std140`、`std430` 生成规则，生成 C++ 和 GLSL 表示。header 和索引 record 的字段宽度、成员偏移和数组 stride 由生成与活动反射核实。声明的静态容量取生成器现有预算内的有界值，工作区只构造当前实际需要的数据。

先核算 cell 乘积与 header 字节范围，再累计各 cell 的成员数和索引前缀。用宽于 ABI 字段的算术核对配置上限、已有完整灯表行数、各段编码宽度及缓冲字节预算，通过后再生成完整压紧段。计数、前缀、填写次序支持按有界容量分配暂存。不确定时按保守候选计数。以每条 header 12 B 估算，1080p 的 16×16 tile、24 层需要 2,350,080 B，超过单个定长记录数组的 1 MiB 上限。32×32 tile、24 层的同输入为 587,520 B，可作为容量参考，实际可表示尺寸由生成容量与所选网格共同确定。

T01 的成功结果交付正常 header，`overflow` 为 0。不可表示或容量、分配失败返回原因，发布边界以完整结果为单位，完整可见灯仍可读取。T02 用 `overflow` 表达单 cell 回退，并为没有可用网格的 View 提供整段回退。两种回退均以当前完整局部灯段为来源，对方向灯保持一次求值。 [Shader 声明](../../../../Engine/Shader/Interface/README.md) · [记录数组上限](../../../../Engine/Shader/ShaderSchema.cpp#L398) · [统一光照查询](../../../../Docs/Architecture.md#lighting)

## Risks / Trade-offs

| 风险 | 处理 |
|---|---|
| 非对称留边、正交零近面或极端透视让 CPU 和 GLSL 选中不同 cell | 用有效渲染矩形和同一套边界。以非对称独立样点核对内部边界、外缘与远面，无法表示时明确返回当前不可用 |
| 聚光裁剪过紧或行号错配造成缺光或错阴影 | 以可证明分离为剔除条件，独立灯覆盖参考同时验证真实灯身份、range 语义及有锥角区分的代表灯 |
| 密集列表或无效尺寸耗尽索引与暂存 | 先核算容量再生成完整记录，以超过旧式小位宽的合法输入区分截断，再测试紧邻超限的不可用和恢复 |
| 共用声明允许编辑激活当前 Forward 尚无配方的绑定 | 用正式生成目标重新生成受影响的包。热重载准入按现有 PerView 布局核对所有 StaticForward 角色的活动绑定，覆盖本帧尚未请求的角色。开发验证的 Froxel Shader 只在 `build/d10-t01/` 构建 |
