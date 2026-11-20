# Proposal

## Why

[D08-T01](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t01) 让冻结场景跨帧复用提取与静态编码。下一步需要在材质编辑和资产换代期间，让新旧 GPU 版本在有限容量内安全共存，并让后续 Draw 数据沿同一上传与退休链交付。[D08-T02 工作包](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t02)

## What Changes

- 将独占几何缓冲和材质行扩充为有界几何页与材质表。按有效版本复用纹理视图、采样器、材质绑定与环境产品，按实际占用和 GPU 完成退休旧代。保留颜色与数据的 mip 语义，以及环境朝向、强度、曝光作为查询参数的行为。
- 以精确的 GPU 目标范围登记并合并兼容的脏写入。各 View 和实际 Draw 数据生产者结束后封存场景批，按存活计划为场景和 UI 分别声明、共同准入。只在实际执行且获得匹配完整图回执后确认上传版本。
- 复用 OpenGL 的即时记录、persistent/coherent 映射和完成协议。完成可复用区段、版本化 volatile 常量缓冲和 push constant UBO 的实际图形消费，验证范围更新、容量压力及退出时的保活。
- 从 Renderer 和 RHI 的事实来源发布有界占用、复用、上传及等待诊断。暂时缺容量时，沿现有 View Pending 与 UI 优先路径处理。提交硬限制或失败原因可定位到资源及其范围。

## Capabilities

### New Capabilities

沿用项目中承载本工作包的既有 capability。

### Modified Capabilities

- `static-gpu-residency` 定义几何及材质表区段、纹理/环境代、实际 Draw 的拥有型上传封存和预算退休。
- `render-graph-contract` 定义共享 Buffer 导入时的有效内容区间，及未初始化区间的读取拒绝。
- `opengl-device-resources` 定义设备拥有的 volatile 常量缓冲及其逻辑描述。
- `opengl-resource-uploads` 定义 persistent/coherent 区段的有界复用、动态常量写入、耗尽和完成后的回收。
- `opengl-shader-pipelines` 定义图形 volatile 布局匹配、可映射的 canonical push constant 块及其物理 UBO 位置。
- `opengl-immediate-draw` 定义活动 volatile 和 push 数据的验证、逐次 Draw 范围刷新及状态失效。
- `render-frame-statistics` 定义几何页、材质/绑定、纹理/环境和 OpenGL 上传的容量与实际使用快照。

## Impact

- 产品路径从 `AssetManager` 发布的固定 CPU 版本，经现有 Forward 准备、RenderGraph、Validation、OpenGL 到 View 输出及 Editor 诊断。沿用公开 RHI 描述与提交接口，图范围声明负责表达页内资源的内容和依赖。
- 后续工作包分别交付排序/实例/间接批次、计算、通用传输与查询、图资源池及热重载。D08-T02 交付这些路径使用的驻留与上传合同。 [D08 阶段计划](../../../../Docs/Developments/D08-D11-RenderPaths.md#dev-d08)
