# Proposal

## Why

D06-T01 已交付受控读取、异步作业和 CPU 资产发布。[D06-T02](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t02) 将标准 glTF 模型接入该入口，让后续 Scene 能取得完整、可引用且经过校验的静态数据。

## What Changes

- 在 Engine/Asset 导入 `Assets/Models` 下只读的 glTF 2.0 / GLB，通过现有资产作业发布选定 scene 的拥有型 CPU 记录。模型源及其外部依赖共同形成可持久化的 `sourceVersion`。
- 校验容器、相对 URI、buffer、accessor 及其尺寸和预算，解码 offset、stride、normalized、sparse 和合法索引。以保留绕序的三角形索引、局部 bounds 和源 Primitive 槽交付静态几何。
- 保留局部变换和负缩放。按静态表面合同生成缺失法线和所需切线，保留有效 UV0/UV1。交付核心材质、图像和纹理用途、sampler、相机、punctual 灯，以及 D06 消费的 unlit、texture transform 和 emissive strength 数据。
- 明确拒绝未支持的 required 扩展和所选 scene 实际使用的 Skin/Morph。标明动画不播放和可选扩展的核心回退，给失败提供定位；引用、语义或预算不满足时保留原有有效模型版本。
- [D06-T03](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t03) 消费模型版本构建 Scene 并处理可编辑场景文件。[D06-T04](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t04) 实现视口投影和裁剪准备。[D06-T05](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t05) 负责 GPU 驻留和 Forward 绘制。

## Capabilities

### New Capabilities

- `static-gltf-import`: 定义静态 glTF/GLB 的受控依赖、几何及材质语义、支持诊断和模型版本交付。

### Modified Capabilities

现有 [CPU 资产发布规范](../../../specs/cpu-asset-publication/spec.md) 提供通用发布合同，模型专属行为由新规范定义。

## Impact

- `Engine/Asset` 新增静态模型记录和导入入口，复用 Core 读取、作业和现有 AssetManager owner。现有图片请求继续作为同一发布服务的消费者。
- 私有接入仓库固定的 TinyGLTF C11 与 MikkTSpace。TinyGLTF 使用针对本项目的有界元数据读取适配，受控外部解析与 CPU 校验归 Asset。输出只向下游公开项目拥有的只读值。
- 第一轮验证使用仓库模型和少量可独立核对预期的本地 GLB/glTF fixture，覆盖版本变化、失败及退出。参见 [资产合同](../../../../Docs/Architecture.md#asset-import) 和 [源版本](../../../../Docs/Architecture.md#asset-persistence)。
