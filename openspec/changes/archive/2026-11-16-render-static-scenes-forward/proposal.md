# Proposal

## Why

[D06-T04](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t04) 已提供固定 CPU 资产版本、只读 SceneFrame 和每 View 的相机与可见集。当前视口仍显示测试图，[D06-T05](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t05) 要让已加载的静态 glTF 几何通过正式帧图出现在编辑器中，并能检查覆盖、材质取样和法线。

## What Changes

- 从当前帧固定的模型、图像与实例覆盖准备初始 GeometryCache、TextureCache、材质 GPU 行及预解析绑定。拥有型 UploadBatch 交给图执行，实际获接受的版本才可供后续帧复用。
- 共用静态 `VertexFetch`、`SurfaceFrame` 与深度/颜色覆盖。处理负缩放、双面、UV0/UV1、`KHR_texture_transform`、标准颜色与数据纹理通道、`KHR_materials_emissive_strength` 以及 `KHR_materials_unlit`。
- 用 `DrawRecord` 把已准备的资源交给 Forward 的 `PrepareSurface` 和 `ShadeOpaque`。主视口显示真实深度遮挡和标准 Unlit；现有视口提供显式的 Base Color、法线、材质通道及自发光预览，标清诊断模式与不支持项。UI 沿本图采样、提交回执和安全回退合同消费颜色。
- 文件面板、编辑和导航由 D06-T06 接续。D07 接续光照、BRDF、IBL、曝光和正式显示。D08-T02 和 D08-T03 扩展池化驻留和间接批次，D13-T01 接续 `BLEND`。 [D06-T06](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t06) · [D07](../../../../Docs/Developments/D06-D07-SceneAndForward.md#dev-d07) · [D08-T02](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t02) · [D08-T03](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t03) · [D13-T01](../../../../Docs/Developments/D12-D13-Materials.md#d13-t01)

## Capabilities

### New Capabilities

- `static-gpu-residency` — 准备固定资产代、语义匹配的纹理及有效材质行，按图上传并确认，按完成票据退休。
- `static-surface-evaluation` — 共享静态顶点、表面框架、glTF 基础取样及深度/颜色的覆盖规则。
- `static-forward-rendering` — 把可见工作集组装为单样本 Forward，输出真实几何的覆盖、深度、Unlit 色和显式表面诊断。

### Modified Capabilities

- `editor-viewport-output` — 现有稳定视口 token 解析当前 Forward 预览，匹配场景、相机、模式和布局的候选获接受后才生效。
- `editor-frame-graph` — 将场景上传、深度、颜色和 UI 读取纳入同一图的活性、预算与输出确认合同。
- `runtime-lifecycle` — 在已有发布、编辑和准备边界接入场景驻留与 Forward，沿现有生命周期处理等待、失败及退出。

## Impact

- 涉及 Renderer 的 Materials、Draw、Paths、Passes、View，静态 Geometry/Material GLSL 及生成 ABI，Runtime 装配与 Editor 的视口消费。复用现有 glTF、Scene JSON、RenderGraph、公共 RHI 和 OpenGL 产品路径。 [驻留](../../../../Docs/Architecture.md#gpu-residency) · [Draw](../../../../Docs/Architecture.md#draws) · [Forward 子图](../../../../Docs/Architecture.md#paths) · [静态几何](../../../../Docs/Architecture.md#geometry) · [材质](../../../../Docs/Architecture.md#materials)
- 验收用真实 GL 上的非对称 glTF 场景，核对深度和颜色读回、材质数值、版本复用及失败/完成边界。产品以空文档启动，场景通过既有会话入口注入验证，开发 fixture 留在 `build/d06-t05/`。
