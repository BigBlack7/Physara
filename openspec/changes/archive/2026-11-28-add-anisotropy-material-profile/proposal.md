# Proposal

## Why

[D12-T04 · 实现 anisotropy 与方向语义](../../../../Docs/Developments/D12-D13-Materials.md#d12-t04) 要让标准 glTF 拉丝材质的高光方向随表面几何正确变化。当前 D12-T01 至 T03 已具备共享基底、扩展颜色路由和环境响应，但导入仍将 `KHR_materials_anisotropy` 视为未支持扩展，颜色角色也缺少方向纹理、切线定向波瓣及匹配 IBL。

## What Changes

- 导入 lit OPAQUE/MASK 材质的 `KHR_materials_anisotropy`，保留强度、以弧度表达的旋转和线性 RGB 纹理。校验切线空间及有效 UV，接入 Inspector、实例稀疏覆盖、撤销重做及 v1 场景读写。
- 在共享表面和光照库内，为核心或 IOR/specular 基底交付各向异性 GGX 直接镜面和配套的 bent-reflection 环境近似。使用固定规范的双轴粗糙度和项目已采纳的 Filament 参考，并覆盖源切线、角点生成、UV 镜像、负与非均匀缩放、双面及基底 normal map。 [材质合同](../../../../Docs/Architecture.md#materials) · [光照合同](../../../../Docs/Architecture.md#lighting) · [几何合同](../../../../Docs/Architecture.md#geometry)
- 复用 Forward 全灯及 Forward+/Deferred ForwardExtension 的 Froxel 消费。Depth、Shadow、SurfaceAux 与诊断继续按各自的资源需求准备。按实际角色验收冷包、候选代的绑定与预算，并以三路径 HDR、深度和参考图闭合交付。
- 首轮范围为一组各向异性颜色角色。需要同时交付 anisotropy 与清漆或 sheen 波瓣的材质，在导入时依 required/optional 规则拒绝或报告降级，实例编辑按相同的颜色可用性边界校验。薄膜由 [D12-T05](../../../../Docs/Developments/D12-D13-Materials.md#d12-t05) 交付，扩展组合及阶段能力回归由 [D12-T06](../../../../Docs/Developments/D12-D13-Materials.md#d12-t06) 汇合，透明与透射留在 [D13](../../../../Docs/Developments/D12-D13-Materials.md#dev-d13)。

## Capabilities

### New Capabilities

### Modified Capabilities

- `static-gltf-import` — 扩展的标准字段、切线前提、作用域及 required/optional 诊断。
- `static-surface-evaluation` — 方向纹理、世界空间框架和双轴微表面响应。
- `editor-domain-edits` — 目标实例的 anisotropy 编辑、几何校验及实际受光状态。
- `scene-document-persistence` — v1 中的稀疏 anisotropy 值和纹理设置。
- `environment-lighting` — 与各向异性轴匹配的环境镜面响应及等向边界。
- `static-forward-rendering` — 三路径使用的各向异性基底、附属表面交付及颜色就绪行为。
- `shader-generation-reload` — 有限颜色角色、实际资源和完整兼容代准入。

## Impact

涉及 Asset、Scene、EditorSession、Inspector、Renderer 的材质/几何/颜色准备、生成 Shader ABI 和共享 Shader。沿用已有 DFG/GGX/SH 产物、覆盖与阴影消费者，以及 Deferred 四附件和 D32。受影响的产品包需与新生成 ABI 一并交付，既有 v1 场景直接读取。 [T01 基底验收](../2026-11-28-add-ior-specular-material-profile/tasks.md#archive-closeout) · [T03 环境与 Shader 交付基线](../2026-11-28-add-sheen-material-profile/tasks.md#final-verification)
