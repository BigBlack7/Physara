# Proposal

## Why

[D06-T02](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t02) 已提供可固定版本的静态 glTF/GLB CPU 记录。[D06-T03](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t03) 要把这些记录变成可独立编辑的 Scene 实例，并让编辑器以单个 JSON 文件保存、重开场景及主 View 请求。

## What Changes

- 在 Engine/Scene 建立实体身份、无环层级、脏根变换与世界 bounds、静态组件和实例覆盖。多个实例引用同一不可变模型，每个实例独立保存创作状态。应用、撤销和重做沿同一领域入口推进对应版本。
- 在 EditorSession 落地 v1 `Assets/Scenes/*.scene.json`，保存稳定实体 ID、层级与 TRS、模型版本引用、静态物体／灯／相机组件、实例材质与纹理覆盖、主 View 导航位姿、可选相机实体和请求渲染比例。模型和纹理正文由各自资产源提供。
- 串行处理新建、打开、保存和另存。打开冻结文档编辑，保留 UI 和取消，校验后一起替换 Scene 和主 View 请求。保存使用拥有型快照并允许继续编辑，安全替换成功后只确认本次保存的版本和路径。
- 接入正式 Runtime 的 CPU 发布及退出边界。[D06-T04](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t04) 消费 Scene 进行帧提取、投影和可见性准备。[D06-T05](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t05) 接入 GPU 驻留和绘制。[D06-T06](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t06) 接入创作面板、导航交互和文件菜单。[D16-T01](../../../../Docs/Developments/D16-D17-EditorAndDelivery.md#d16-t01) 完成重绑定，[D16-T02](../../../../Docs/Developments/D16-D17-EditorAndDelivery.md#d16-t02) 完善后续持久化字段。

## Capabilities

### New Capabilities

- `static-scene` — 实体、层级与实例事务、变换及版本、模型固定版本和 instance override 的 CPU 合同。
- `scene-document-persistence` — v1 文档、受控资源定位与源校验、串行打开及快照保存的合同。

### Modified Capabilities

- `editor-domain-edits` — 在既有编辑历史中应用与重放场景事务，按文档操作状态门控，切换后退役旧文档目标。
- `runtime-lifecycle` — 在继续运行的 CPU 轮次发布场景文件操作，启动空文档并在退出时收拢文件工作。
- `static-gltf-import` — 让固定的模型记录可核对磁盘主文件和实际依赖，区分缓存命中与本次源校验。

## Impact

- 新增 `PhysaraScene` CPU 目标，复用固定 EnTT、项目身份值和已发布的 Asset 记录。Scene 提供领域入口和类型化快照，Renderer/View 提供主 View 请求的类型化恢复入口。
- Editor/Session 私有使用仓库固定的 JSON 库和现有 Core 作业、受控 I/O，沿该边界实现同目录安全替换。Runtime 装配 owner 并推进结果，正常编辑器继续使用现有诊断视口。
- 验收通过正式 CPU 入口和产品会话验证实例隔离、层级与变换、文档往返、磁盘源冲突及取消／保存交错。持久化依据 [架构 3.4](../../../../Docs/Architecture.md#asset-persistence)，Scene 行为依据 [架构 4.1–4.2](../../../../Docs/Architecture.md#scene)。
