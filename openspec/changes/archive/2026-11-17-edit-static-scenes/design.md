# Design

## Context

`Scene` 已提供只读实体遍历、类型化事务和本地到世界的变换传播。`EditorSession` 接管单文档及选择，现有 `EditHistory` 连接 Scene 事务和主 View 字段。`SceneDocuments` 已负责文件操作、候选发布和保存基线。主视口在 CPU UI 封存后生成像素请求，Renderer 从同一 Scene 准备相机和几何，正常图实际采样及提交接受后确认显示代。 [Scene](../../../../Engine/Scene/Scene.hpp) · [会话](../../../../Editor/Session/EditorSession.hpp) · [文件协调器](../../../../Editor/Session/SceneDocuments.hpp) · [帧消费](../../../../Runtime/Frame.cpp)

## Goals / Non-Goals

- 用现有领域 owner 接通层级、局部创作值、文件动作、模型实例化及主 View 导航。一次操作能追溯到它改变的字段、版本及实际图输出。 [领域命令](../../../../Docs/Architecture.md#edits) · [视口输入](../../../../Docs/Architecture.md#viewport-input) · [视口输出](../../../../Docs/Architecture.md#editor-output)
- 产品继续在单个原生窗口中提供主 View。额外 View 沿各自的逻辑身份保有会话请求；D16 的编辑叠加、GPU 拾取、资产浏览和再导入沿开发计划接续。 [视图准备](../../../../Docs/Architecture.md#view-preparation) · [D16-T01](../../../../Docs/Developments/D16-D17-EditorAndDelivery.md#d16-t01) · [D16-T03](../../../../Docs/Developments/D16-D17-EditorAndDelivery.md#d16-t03) · [D16-T04](../../../../Docs/Developments/D16-D17-EditorAndDelivery.md#d16-t04)

## Decisions

### 面板查询随领域版本刷新，操作通过窄意图提交

Hierarchy 从活动 Scene 的 `VisitEntities` 提取展示所需的身份、父节点和名称。投影键包含 SceneId 和文档 revision，展开与滚动归面板；按可见层级生成行，裁剪长列表，隐藏时跳过展开和行构建。用身份作控件键，重复名称和多实例可以独立选择。列表仅保存展示字段和身份；Inspector 按当前选择读取 `Scene::Get`，相机列表只引用合法的场景相机及其稳定 ID，切换文档即废弃旧投影。 [面板归属](../../../../Docs/Architecture.md#editor-input) · [实体身份](../../../../Docs/Architecture.md#scene)

工作区产生选择、字段编辑和 Delete 意图。会话在封存后用实际 Scene/Entity/View 身份解释意图；Delete 调用 `EraseSubtree`，选择退役和主 View 相机回退在现有跨 owner 事务边界完成。撤销根据稳定 ID 恢复领域引用，层级的选择仍以新运行期身份为准。View 面板拥有展示状态，并从 View 读取 requested、effective、等待/拒绝和会话预览。 [历史入口](../../../../Editor/Commands/EditHistory.hpp) · [View 请求](../../../../Engine/Renderer/View.hpp)

### 用一条历史记录提交一次连续编辑

Inspector 展示本地平移（m）、XYZ 旋转（°，依次绕 X、Y、Z 轴，旋转矩阵为 `Rz * Ry * Rx`）和有符号缩放。仅针对实际编辑的字段从当前值生成候选。旋转草稿仅在活跃控件内保留，发布时显式形成单位四元数；未操作的旋转沿用原值。Scene 继续计算 `parentWorld * T * R * S`，局部输入可表达现有负、零与非均匀缩放，有限数及世界数据的合法性沿 Scene 校验。 [局部值与世界数据](../../../../Engine/Scene/Scene.hpp)

活动手势记录目标、首值及最近的接受值。用同一领域入口逐帧预览并在完成时合成一条历史；取消走相同入口恢复首值，目标已退役时丢弃该事务。内部的比例拖拽继续使用相同协议，导航只编辑 pose，显式选择相机编辑相机引用。合并既有历史时保留完整 Scene 事务的重放前提，或从新解析的实体构造字段级重放，以免旧 handle 命中复用槽位。结束后有净变化才增加历史和截断 Redo。活跃手势占用历史边界，待到达的模型实例在 CPU Ready 状态等到可提交再入历史。

Inspector 逐轴读取 ImGui 的激活与失活状态。工作区在同一批次保留上一活动控件的末值及结束，以及当前控件的启动或预览。会话先完成旧事务，再以领域已接受的值为新事务取首值。同一向量内切换轴也按这一顺序提交。

### 文件协调器保持单文档，导入采用共享 CPU 资产结果

File 菜单与 Ctrl+N、Ctrl+O、Ctrl+S、Ctrl+Shift+S 共用动作；Delete 和 Undo/Redo 复用同一作用域。用工作区内 UTF-8 路径输入接收 exe 资源根下的相对路径。Open/Save As 复用 `SceneFileRequest`、受控路径解析和实际 I/O 校验。File/Scene status 使用真实文档身份、路径和操作状态。已有路径的 Save 直接请求保存，无路径时收集 Save As 路径。工作区关掉模态后，保存期间保留继续编辑的入口。 [文件入口](../../../../Editor/Session/EditorSession.hpp) · [资源根](../../../../Runtime/Resources.cpp) · [持久化合同](../../../../Docs/Architecture.md#asset-persistence)

New/Open 在确认前先收束相互冲突的编辑，放弃授权绑定确认时的 `SceneDocumentVersion`。选择保存后继续时，会话只保存一份暂缓的替换意图，跟踪本次保存的操作号及所捕获版本；用协调器提供的匹配终态区分实际成功、取消和失败。仅当实际保存成功且目前仍是保存的版本时继续原动作。文件处理仍由既有协调器串行完成，存档开始、路径更新和候选采用都沿原来边界。

Import Model 将 `Assets/Models` 下的 glTF/GLB 路径及可空无符号 scene 下标交给 `RequestModel`。会话只追踪一个实例意图，由导入身份、SceneId 和 `AssetRequest` 关联，面板可显示 CPU 加载、待事务、已实例化、拒绝或取消及资产支持报告。Runtime 在每个继续的输入轮次先发布 CPU 资产，再推进文档完整候选，随后核对待实例化导入。只接受匹配 `StatusModel` 请求的 CpuReady 记录，走 `Instantiate` 提交独立的实例根及源子树。同源可复用不可变模型，场景改动仍独立。用户取消、接受 New/Open 或停止时退役会话的实例意图。Asset 的请求可由多名消费者合并，管理器保留共享解码并按自己的界限回收；会话终止先撤销实例资格，再由 Runtime 收拢共享资产工作。GPU 可用性继续由 View 的接受结果说明。 [资产请求与共享取消合同](../../../../Engine/Asset/AssetManager.hpp) · [当前 CPU 发布顺序](../../../../Runtime/Main.cpp)

### 导航从当前布局命中，经宿主确认后更新 View

| 交互 | 按下条件 | 位姿变化 |
|---|---|---|
| Fly | 右键 | 鼠标 yaw/pitch，W/A/S/D 沿相机方向移动，Q/E 沿世界 Y 轴移动 |
| Orbit | Alt + 左键 | 优先取有效且在相机前方的选中实体世界 bounds 中心，否则取相机前方固定正距离的点；固定锚点，眼点偏移与相机轴一起旋转 |
| Pan | 中键 | 沿相机右、上轴平移，按逻辑图像高度和正的观察距离求位移尺度，朝向保持 |

用右手系 `+Y` 与局部 `-Z` 相机约定，给俯仰留出有限余量，并将持续键位移的时间步限制为有限短间隔。鼠标位移取 HostFrameInput 中合格捕获区间的逻辑运动量，每批只应用一次。按下本身保持原位姿，发布的四元数经过归一化；实体相机通过稳定 ID 使用 Scene 解析的世界刚体和投影，导航只在相机实体引用为空时可用。切换相机来源属于可撤销的 View 修改。 [相机轴与投影](../../../../Docs/Architecture.md#camera) · [平台输入](../../../../Platform/Input.hpp)

工作区从当轮 ImGui 的控件、模态、遮挡和焦点取得归属。ActionRouter 为每次触发选一个消费者。宿主在有序发布中记录按钮边沿当时的逻辑位置，以及位置事件在有效模式和基线内的位移。相机由此按真实按下位置命中，并在释放边沿截断位移。捕获请求通过 `SetCursorMode` 交给宿主，相对模式生效并建立新基线后才应用运动。结束、Esc、同批失焦/恢复、暂停、隐藏或布局失效由会话清理并请求归还。Runtime 仅在真实归还成功后使用已有逻辑指针同步。正在捕获时可继续处理当次手势的逻辑运动，新的捕获须再次检查资格。 [当前输入路由](../../../../Editor/Session/ActionRouter.cpp) · [光标恢复](../../../../Editor/ImGui/GlfwPlatform.cpp)

### 以当前几何和图确认建立命中资格

用同轮窗口指标和封存的内容矩形生成 `ViewportRequest`。每轴只在该步骤应用一次实际 framebuffer/client 比例和 render scale，取整后交给 View 准备。布局用于输入时复用 `ViewportImage::ImageUv` 和 `ViewImageLayout::ImageUv`，按实际裁剪、画布和固定相机的有效矩形转换。像素请求、导航位姿及最终 Scene 变更在录制前确定，鼠标捕获不改变布局 owner。 [现有映射](../../../../Editor/Viewport/Viewport.cpp) · [View 准备](../../../../Engine/Renderer/Prepare/ViewPreparation.cpp)

把最近一次实际正常采样且发布成功的相机／布局依据作为短小的值交回会话。它包含关联的 Scene 内容及相机版本、View 相机和尺寸请求代、模式与像素布局，以支持诊断图或 Forward 颜色的命中资格核对。输入仍用当前封存的逻辑区域与裁剪，借用的 DrawData、相机/场景工作区、纹理和 GraphId 都留在各自原有的寿命内。隐藏与重新显示、其它场景或相机、不同尺寸以及实际显示安全 UI 或占位时使旧输入映射失效；可见且布局仍有效的活动手势可以等自己的位姿回执，正常匹配输出恢复后才重新开放命中。该边界沿 D06-T05 的实际采样与 `View::Publish` 确认，用同一 View、token 和真实提交完成保活资源。 [接受判断](../../../../Runtime/Frame.cpp) · [图回执](../../../specs/editor-frame-graph/spec.md)

## Risks / Trade-offs

| 风险 | 约束 |
|---|---|
| 子树删除、Undo 恢复或 Open 替换后，旧控件或捕获仍引用同名或复用槽位 | 按当前 SceneId 和 generation 应用意图，历史稳定 ID 在有效重放时解析，选择及编辑草稿随所属目标退役 |
| 保存后继续与异步实例进入同一文档竞态 | 只用匹配保存回执及持久化版本放行替换；导入在自己的边界单次提交，失效意图不会消费旧 CPU 记录 |
| UI 争用、DPI 和图失败使旧矩形误命中或相机跳变 | 当前归属、实际捕获、封存裁剪与正常输出回执共同决定起始资格；帧时间和初始运动基线按输入寿命处理 |

## Migration Plan

使用现有 JSON v1。只有主 View 序列化导航位姿、可空相机实体 ID 和请求比例；会话模式、其它 View、选择及有效输出沿各自 owner 恢复或重建。现有 v1 文档由原有加载流程打开，保存继续使用相同格式及资源根。实现同时更新构建说明中的实际界面入口。 [v1 格式](../../../specs/scene-document-persistence/spec.md) · [运行入口](../../../../Docs/Build.md)
