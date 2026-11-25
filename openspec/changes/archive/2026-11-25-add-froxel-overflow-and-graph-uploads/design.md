# Design

## Context

`ViewWork` 按需交付 D10-T01 的 `FroxelLightLists`。其头表已经包含 32 位 `offset`、`count` 和 `overflow`，当前分箱在总索引超额时返回 `IndexCapacity`。`LightRowCursor` 与 `PunctualLights` 使用方向灯前缀及完整局部灯后缀，阴影引用与该灯表逐行配对。图已经有拥有型 `UploadBatch`、精确的 Buffer 上传 Pass 和正常接受回执。 [前置设计](../2026-11-25-prepare-cpu-froxel-light-lists/design.md) · [CPU 列表](../../../../Engine/Renderer/Lighting/Froxel.cpp) · [灯表](../../../../Engine/Renderer/Lighting/PunctualLights.cpp) · [上传合同](../../../specs/static-gpu-residency/spec.md)

本工作包将显式 Froxel 请求接到可验证的图交付和 Shader 候选消费。运行验证从 `build/d10-t02/` 装配受光实验 Pass，直接使用本轮生产代码和公共光照函数。Forward+ 策略组装、计划变体和有效路径切换由 D10-T03、T04 接手，D10-T05 在实际路径间测量收益。 [工作包边界](../../../../Docs/Developments/D08-D11-RenderPaths.md#d10-t02)

## Goals

- 预算变化只改变哪些格使用全灯退路，格与灯行可以独立核对。
- 每份可消费的 GPU 数据都能从受光 Pass 追溯到同一冻结 View 的拥有型上传及实际接受结果。
- 诊断可解释当前预算、逻辑用量和退路工作量。 [Froxel 规格](specs/froxel-light-preparation/spec.md) · [统计规格](specs/render-frame-statistics/spec.md)

## Decisions

### 1. 按 cell 接纳完整索引区间

继续使用 D10-T01 的稳定边界、保守相交和 `LightRowCursor`。第一次遍历统计有限候选并收齐全局局部灯。按 X、Y、Z 的既有扁平顺序扫描 header。剩余额度以条目计，取 `FroxelRequest::indexLimit` 与生成接口上限的较小值，缺省沿用生成上限。使用有界的剩余量与候选数比较，之后只为获准的区间分配、填充索引。第二遍按相同灯行顺序填充获准的格，工作存储按格数、可见灯数和获准条目计。 [Froxel ABI](../../../../Engine/Shader/Interface/StaticForward.json) · [灯准备](../../../../Docs/Architecture.md#light-preparation)

正常格写 `overflow = 0`。一格超出剩余索引额度时写 `{offset = 0, count = 0, overflow = 1}`，Shader 先检查标记。继续尝试后继格可让空格及仍能容纳的格使用有效区间。CPU 总索引不足从全结果拒绝改为逐格退路。网格无法表示、完整头表超限、灯表行号超限和存储不足仍通过原有 View 准备结果报告。完整灯通路可独立取得当前可见集。 [CPU 准备接口](../../../../Engine/Renderer/Prepare/ViewPreparation.hpp)

Filament 固定版本在记录池中先建立全灯记录，空间耗尽后供余下格引用。Physara 的生成 ABI 已有独立 overflow 字段，完整灯表也已随 View 准备，因此用现有完整灯行承担退路。 [Filament 参考分析](../../../../Docs/Reference/Filament/VisibilityAndCommands.md#f4) · [Filament 固定源码](https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/Froxelizer.cpp#L753-L812)

<a id="graph-delivery"></a>
### 2. 同一冻结 View 的数据作为一组交付

在现有 Renderer 准备入口将同一 View 的常量和三个结构化表交给所属安全帧槽。上传准备核对表的有效计数、生成 stride、字节范围和绑定能力，在分配及复制前预留整组上传字节和笔数。`UploadBatch` 持有完整提交到批次的源字节及目标强引用。后续驻留或载荷准备失败时归还该组额度，保留早先的批次前缀。共享灯表继续由其已有 owner 生产。 [GPU 驻留](../../../../Docs/Architecture.md#gpu-residency) · [上传包](../../../../Engine/Renderer/Materials/StaticGpu.hpp)

当前 OpenGL 反射要求固定记录数组，活动绑定须覆盖完整块。首版用现有固定容量，以零填充定义未使用尾区，再由图明确上传与绑定完整块。三个 SSBO 每套合计 2,162,684 字节，另有 `FroxelView` 常量、已有灯表及实际后端上传区段。逻辑索引预算限制有效条目，上传需求按实写字节计入，块容量按既有驻留限额准入。空索引或空全局列表以已定义的固定块和零有效计数交付。固定块完整写入使首次构图也能证明内容有效，适配现有要求已初始化导入的 `ReadIndexedRows` 合同。 [结构缓冲反射](../../../../RHI/OpenGL/Shader.cpp) · [绑定范围检查](../../../../RHI/OpenGL/CommandListGraphics.cpp) · [图读取范围](../../../../Engine/Renderer/RenderGraph/PassResources.cpp)

`FroxelView` 的生成块为 608 字节。宿主在已有设备能力查询中保存实际 UBO 块容量，供 Froxel 准备和图 profile 使用。图按既有常量子区间对齐合同验证各次实际绑定。 [能力查询](../../../../Platform/OpenGLDeviceHost.cpp) · [图 profile](../../../../Runtime/Frame.cpp)

场景上传声明为共享批次内的每个目标建立 `CopyDestination` 写版本。Froxel 读者从同一份版本表定位所属 View 的四个版本，声明 `GraphUsage::ShaderRead` 或 `ConstantBuffer`。已有灯表、阴影和环境输入按实际受光配方声明。消费者使用图授权的版本和范围建立绑定，由现有 RHI 跟踪器建立后继访问。封存后从存活 Pass 收集上传需求，失败前缀及重试沿用现有协议。帧包源字节保留到图录制或放弃，GPU 绑定、存储及后端上传区段按真实完成票据退休。 [场景上传](../../../../Engine/Renderer/Paths/Forward.cpp) · [图执行](../../../../Docs/Architecture.md#barriers) · [NVRHI 上传参考](../../../../Docs/Reference/NVRHI/RecordingAndBarriers.md#n6)

### 3. 候选选择与统一灯求值

Froxel 消费者用 D10-T01 的画布左上坐标和米制正向视深查询。正常格以绝对灯行号归并有序的有限灯索引与全局局部灯，使参与贡献的灯保持完整灯表的相对顺序。overflow 或无有效 cell 进入完整局部灯循环，直接消费 `[directionalCount, directionalCount + localCount)`。方向灯前缀单独求值。所有候选继续沿用 `EvaluateDirect`、配对的阴影引用和预曝光累加。 [坐标查询](../../../../Shaders/Lighting/Froxel.glsl) · [当前光照](../../../../Shaders/Lighting/Punctual.glsl) · [光照聚合](../../../../Docs/Architecture.md#lighting)

显式 Froxel 消费者使用生成接口中对应的 PerView b1 及三个结构化只读绑定。普通 Forward 的完整灯消费者按其已有活动绑定配方取得输入。全 View 数据在准备时不可用且全灯通路有效时，仅后者可供选择。受光图一旦开始录制，失败按现有图执行、前缀与恢复合同处理，下一次候选重新准备。 [绑定接口](../../../../Engine/Shader/Interface/StaticForward.json) · [图失败合同](../../../specs/render-graph-execution/spec.md)

### 4. 准备事实与接受身份

由列表准备 owner 计量每 View 的总格数、overflow 格数、有效索引条目及额度。可见灯裁剪完成时即可报告完整局部灯后缀的长度，后续附件拒绝仍保留该次遍历量和实际原因。准备/图准入状态随本次 View 输入通过现有只读统计通道呈现。实际上传沿现有上传统计计字节和笔数，最近正常画面及计时继续使用自己的提交身份。GPU 实测的着色与退路占比由 D10-T05 用固定口径获取。 [帧统计](../../../specs/render-frame-statistics/spec.md) · [测量口径](../../../../Docs/Architecture.md#benchmark)

## Risks / Trade-offs

| 风险 | 对应检查 |
| --- | --- |
| 预算判定把空格与溢出格混为一谈，或填充覆盖后续区间 | CPU 独立候选验证恰好可容纳、超额、空格、零预算与 32 位行范围 |
| 活动绑定比已定义数据宽，或某路消费早于上传或串用另一 View 的数据 | 原生反射和 Bind 验证实际 stride、块及读写版本，以不同输入的 View 和失败前缀后的重试核对读回 |
| 退路漏灯、重复评估无范围灯或更换阴影引用 | 用同一受光 Shader 包的全灯选择作对照，读回有可辨点光、聚光和全局灯贡献的 HDR，并确认覆盖的分支 |
| 固定块的真实字节与小预算下的有效条目混淆 | 分别核对逻辑计数、上传需求和实际存活写入，构图采集原生准入和可定位的失败状态 |
