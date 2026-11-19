# 资源

模型统一使用 Models 下只读的 glTF/GLB，提供静态几何和默认材质。每个编辑器场景使用 Scenes 下的一个 `*.scene.json`，保存 Assets 资源引用、静态实体/层级/TRS、灯与物体参数、实例材质/纹理覆盖和主 View 相机及 requested 渲染设置；场景格式及加载、保存职责见 [架构文档 3.4](../Docs/Architecture.md#asset-persistence)。新建空场景直接创建内存数据，无需预置模型文件。

Textures、Environments 和 Editor 分别保存辅助纹理、环境图及编辑器图标。glTF 自身引用的纹理和 buffer 随模型组织，不拆散其相对引用。替换实例材质纹理时引用 Assets 下的资源，覆盖只写入场景 JSON，不修改共享模型；普通保存不生成第二套网格或材质资产文件。

## 现有资源来源

下表记录目前能够从资源中确认的信息；未知来源和许可如实保留待核实状态。

| 资源 | 已知来源 | 待核实内容 |
|---|---|---|
| Models/castle | glTF asset.source 指向 CG 美术之家 model/104214.html | 作者和原始许可 |
| Models/courtyard | glTF asset.source 指向 CG 美术之家 model/106689.html | 作者和原始许可 |
| Models/su7_ultra | glTF asset.source 指向 CG 美术之家 model/108183.html | 作者和原始许可 |
| Models/camera | Blender exporter 元数据 | 原始来源和许可 |
| Models/gallinera_table、small_lpg_tank | Blender exporter 元数据 | 原始来源和许可 |
| Textures/wall、tile、brain | 当前保留的辅助纹理 | 原始来源和许可 |
| Environments | 当前保留的 EXR 环境图 | 来源、许可和亮度标定 |
| Editor | 当前保留的图标 | 作者、来源和许可 |

当前目标只包含静态场景。实际使用 Skin/Morph 的实例不纳入支持，静态节点可使用初始变换，动画曲线不播放并报告。能解析文件不等于支持其全部材质或扩展；实际资产支持情况随导入与渲染实现核对。

## Forward 参考场景

把下表四项按原路径放入实际 exe 同级的资源树。用 `File > Open Scene` 打开 `Assets/Scenes/ForwardReference.scene.json`，在 `View Settings > Preview` 选择 `Physical Lighting`。编辑演示用 `Save Scene As` 保存派生文档。

下表四项为 Physara 第一方原创资产。维护者允许在 Physara 及其衍生版本中使用、复制和修改这四项参考资产，并随项目分发原件及修改件。单独分发或在其他项目中使用须另行取得维护者授权。

| 资产 | SHA-256 |
|---|---|
| [场景文档](Scenes/ForwardReference.scene.json) | `da5a26a3e365d3e9c0f6ca4532099ac25f62cc94bbdc77cbbd2a9132521c22d0` |
| [核心材质及节点](Models/ForwardReference/Reference.gltf) | `f976c234a38aeb607be9f5ebc0647f826dedf4c6d0ac3bec0a898a3999d72f90` |
| [网格顶点和索引](Models/ForwardReference/Geometry.bin) | `ef2c9b2d386c9645b61c2bbfb3176af7b165ca55ced9fee5548c14bb31c5facb` |
| [相对辐亮度 EXR](Environments/ForwardReference.exr) | `8d5e5c29f59a35a5d14239829e7c434ebba01bdad065c88d8b98b76c260108cf` |

glTF scene 0 的固定 `sourceVersion` 为 `19bc38d466634badb6be69ccd70f5be73d79dfe9c0bda7f5c5a51d6ef1495700`。文本输入按 LF 固定；工程所用 SHA-256 基于实际开出的文件字节。

- 场景相机位于 (0, 0, 8) m，朝 −Z，正交范围 8×6 m。固定 f/4、1/16 s、ISO 100、补偿 0 EV，预曝光为 1/307.2。主 View 初始请求 1.0 倍、None。数值基线使用 640×480 的有效栅格。
- 上排四个球显示介质与金属各两档粗糙度。左侧灰板中心 (-2.5, -1.2, 0) m，板宽和绿色刻度间距各为 1 m。右侧灰板是同一源 Primitive 的独立实例。右下红色斜边用于 FXAA，对应发光样片的中心为 (0, -1.65, 0) m。
- 基线使用 96 lux 方向灯、位于左板正前方 2 m 的 16 cd 点光和照向右板的 240 cd 聚光。隔离直接光时，用 4 lux 方向灯及 16 cd 点光分别对照 2 m 和 4 m。
- EXR 为 128×64 的线性相对辐亮度，按环境单位方向记录 `R=1+0.5d.x`、`G=1+0.25d.z`、`B=1`。基线环境倍率为 16，解析天空对照采用倍率 1。后续 HDR 与窗口比较见 [D07-T06 开发计划](../Docs/Developments/D06-D07-SceneAndForward.md#d07-t06)。
