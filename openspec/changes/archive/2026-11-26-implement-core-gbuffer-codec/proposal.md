# Proposal

## Why

D10 已交付可比较的 Forward 与 Forward+。D11-T01 需要把两者共用的核心表面模型存入固定附件，使后续表面路由和屏幕光照可以从同一像素恢复材质、着色法线、预曝光自发光和照明资格。 [工作包与前置](../../../../Docs/Developments/D08-D11-RenderPaths.md#d11-t01) · [几何合同](../../../../Docs/Architecture.md#geometry) · [材质合同](../../../../Docs/Architecture.md#materials)

## What Changes

- 新增可由 Renderer 独立声明的核心表面 Pass、正式几何着色包和共享解码入口。四个单样本颜色附件合计 18 B/像素，配独立 Reverse-Z D32。
- 沿用有效静态实例、材质覆盖、MASK、双面和粗糙度过滤。明确各通道的颜色域、量化、有限值范围和路由位的初始化。
- 补齐混合颜色附件所需的 OpenGL 格式用途，以及图形片元从无符号二维纹理读取整数的 Shader 包与原生绑定合同。通过图内真实消费者解码，按字段与独立预期对照。
- T02 消费深度、G1 和 G3 实现 SurfaceAux；T03 以解码结果接入光照和天空；T04 开放 View 路径切换；T05 交付预览与整帧等价、成本对比。T01 的验收点是可供这些消费者使用的核心表面。

## Capabilities

### New Capabilities

- `core-gbuffer` 定义图内生产、四附件布局、有效表面及编解码精度。 [增量规格](specs/core-gbuffer/spec.md)

### Modified Capabilities

- `opengl-device-resources` 增加两种颜色纹理格式按设备查询和组合使用的合同。 [增量规格](specs/opengl-device-resources/spec.md)
- `shader-build` 增加图形片元无符号二维 fetch 的声明、ABI 和反射合同。 [增量规格](specs/shader-build/spec.md)
- `opengl-shader-pipelines` 增加对应原生 Shader、活动 SRV 数值类型匹配与绘制前拒绝行为。 [增量规格](specs/opengl-shader-pipelines/spec.md)

## Impact

涉及 Renderer 的 Pass、Draw 与 RenderGraph 消费入口，Material/Geometry Shader 及正式包构建，Engine/Shader 的接口与包校验，以及 RHI/OpenGL 的格式映射和活动资源绑定。现有 `static-surface-evaluation` 提供材质求值依据，Forward 和 Forward+ 是受共享函数调整影响的直接消费者。
