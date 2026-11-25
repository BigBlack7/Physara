# Proposal

## Why

[D09-T02](../../../../Docs/Developments/D08-D11-RenderPaths.md#d09-t02) 和 [D09-T03](../../../../Docs/Developments/D08-D11-RenderPaths.md#d09-t03) 已能生成并诊断三类灯的阴影深度。普通 Forward 已使用完整灯表，但尚未消费这些深度。[D09-T04](../../../../Docs/Developments/D08-D11-RenderPaths.md#d09-t04) 将阴影可见性接入共享直接光求值，使同一场景中的方向光、聚光和点光可以同时投射可见阴影。

## What Changes

- 提供 Hard 和固定核 PCF。比较和接收偏移遵循常规阴影深度，级联过渡混合已求得的可见性，聚光外锥和点光面边界的 PCF 使用已定义的采样余量。
- 普通 Direct Lighting 和 Physical Lighting 按当前灯记录使用其获准的完整阴影组。阴影预算不足、关闭或在覆盖范围外时，该灯仍按原参数参与完整灯表求值。材质响应、环境光和自发光保持各自的物理含义。
- 继续使用 View 会话设置和现有 Renderer 面板，提供过滤、分辨率和预算控制。统计可核对请求、规划降级和已接受的深度生产、复用及光照消费。
- 以固定输入验证预曝光线性 HDR 的逐灯遮挡结果，分别记录阴影关闭、Hard 和 PCF 的成本。资源压力门槛及扩大画质/成本矩阵归 [D09-T05](../../../../Docs/Developments/D08-D11-RenderPaths.md#d09-t05)，PCSS 和 moments 归可选 profile。

## Capabilities

### New Capabilities

### Modified Capabilities

- `shadow-planning` — 拟合最大比较核与接收偏移所需的覆盖，沿用有界分组准入和区域版本。
- `shadow-rendering` — 普通光照及诊断共用深度生产、Hard/PCF 可见性、层权限和接受后复用合同。
- `punctual-lighting` — 将同轮完整灯表与当前可查询的阴影组配对，按灯应用可见性。
- `static-forward-rendering` — 在正式 HDR 与诊断输出中使用同一阴影查询。
- `view-preparation` — 校验 per-View 过滤及预算请求，为实际显示分支准备对应的阴影输入。
- `render-frame-statistics` — 区分 CPU 逻辑预算、当前原因及该 View 的已接受阴影工作。
- `shader-build` — 在生成接口及 SPIR-V 包中保留浮点二维数组采样维度。
- `opengl-shader-pipelines` — 准备并校验真实二维数组采样视图及对应的活动绑定。
- `render-graph-execution` — 交付有确切层权限及定义版本的 D32 数组采样和只读保留。

## Impact

- Renderer 的 View、Prepare、Lighting、Draw 和 Forward 组装增加当前消费的阴影数据及图依赖。Depth、Shadow、Lighting 和 Forward Shader 继续沿用静态几何及 MASK 覆盖。会话请求和统计以现有 Editor/Runtime 通道传递，场景文档沿用 v1。
- Shader 生成、图编译/执行与 OpenGL 绑定补齐 D32 数组的共享消费所需能力，保留同格式单层写入与已接受内容的保活。依赖沿用仓库内固定工具及源快照。
- 工作包合同来自 [Architecture 5.4](../../../../Docs/Architecture.md#light-preparation)、[8.3](../../../../Docs/Architecture.md#shadows)、[8.6](../../../../Docs/Architecture.md#lighting) 和 [8.7](../../../../Docs/Architecture.md#opaque-shading)。当前实现入口为 OpenGL Forward，共享查询供后续路径消费。
