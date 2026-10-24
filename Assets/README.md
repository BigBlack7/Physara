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
