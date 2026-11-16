# Design

## Context

[D06-T03](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t03) 消费已验收的 `RequestModel`、`StatusModel` 和不可变 `ModelVersion`。模型记录同时保留紧凑表索引与源 node/Primitive 槽。现有 `EditHistory` 已在 CPU UI 封存后通过 View 字段入口应用及重放请求。Runtime 已在终止判定后、尝试取帧槽前发布资产完成。这个 change 沿 [领域编辑](../../../../Docs/Architecture.md#edits)、[场景存储](../../../../Docs/Architecture.md#asset-persistence) 和 [Scene 版本](../../../../Docs/Architecture.md#static-content) 合同扩展这些入口。

## Goals

- 以一个原生会话里的 Scene、主 View 请求和单文档状态提供可回放的实例编辑与可恢复的持久化。
- 让文档结构、资产身份和文件替换分别在首次跨越各自输入边界时得到验证，文件完成独立于图形提交。
- 为 [D06-T04](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t04) 交付类型化的静态组件、世界数据和版本，为 [D06-T06](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t06) 交付同一套可接线的编辑及文件动作入口。

## Decisions

### 1. Scene 持有实体和有效资产引用

在 `Engine/Scene` 建立 `PhysaraScene`。复用现有 `entt` target 并私有持有 registry。Scene 的公开合同使用项目值类型及只读模型引用。Scene 依赖 Core、Asset 和实现所需的固定数学头。View 继续持有自己的请求，Editor/Session 通过 Scene/View 窄入口组合，JSON 库留在场景文件编解码实现中。由 [Runtime](../../../../Docs/Architecture.md#runtime) 组合实际 owner，产品和本地消费者使用同一套入口。

- Scene 使用稳定字符串 ID 的冷路径索引，实体 handle 使用槽和非回绕的 generation。所有跨帧事务及选择身份绑定 Scene。加载建立新的 Scene 身份，删除使旧 generation 失效。撤销删除通过历史记录恢复原 ID 并分配新 handle，历史在当前文档内重新解析稳定 ID。
- 从某一已选择的 `ModelVersion` 构造一个实例根和所选 scene 的节点。实例根保有不可变记录，节点创作值由 Scene 持有。`SourceRef` 用实例根稳定 ID 和原 glTF node 下标定位。每项 Primitive 覆盖再用源 `sourceSlot` 定位。只在创建或加载候选时把持久索引解析为固定版本的内部引用。
- 允许把源节点重挂到实例外的层级，实例身份沿 SourceRef 保留。删除实体按子树进行，遇到所删实例根时同时收集关联源节点及其子树。删除集合去重，一次退役全部身份。EditorSession 清理无效选择，并通过 View 入口将失去的活动实体相机切回已有导航位姿。历史记录包含受该事务影响的主 View 相机请求，因此可按领域规则撤销该回退。
- 通过现有有界 `EditHistory` 扩展类型化条目。值编辑保存实际前后值，结构编辑仅持有受影响的子树记录及固定资产引用。事务和对应历史项同步接受。会话只对外提供校验过的命令、快照和结果，工作区只消费文件进度和取消入口，完整 File 菜单和属性面板随 D06-T06 接入。

### 2. 只传播受影响的变换与版本

- local 为米制平移、xyzw 单位四元数和有符号逐轴缩放。把导入的仿射矩阵分解后重组核对，反射保留为有符号缩放。对零轴，用其余非零轴补齐正交基。重挂接区分保持 local 和保持 world 两种意图。后者用同轮已接受编辑后的父子 world 推导候选 local，拒绝不可逆父级及无法重组的 shear。由层级合成产生的 world 可包含 shear，仍用完整矩阵计算 bounds。
- 事务验证受影响的链接、局部值及需要更新的组合数值，然后从合并后的脏根按父先子后发布。新候选和结构事务需要校验的结果与实际发布复用同一份计算。按 Primitive 局部 AABB 变换八个角点后取范围，依需要汇总到实体，保留空 Primitive 无 bounds 的语义。遵守 [坐标约定](../../../../Docs/Architecture.md#conventions)，为之后的几何准备保留反射信息。
- 编辑入口区分文档、渲染内容、相机和可持久化 View 设置。变换影响的子树决定相关相机与渲染内容是否变化，混合组件可同时触发两者。导航位姿只由 View 请求拥有。文档比较保存时的 Scene 和 View 各自 revision。View 既有用于输出范围的请求代仍按范围请求更新，避免仅相机记录变化使诊断颜色重建。

### 3. 稀疏覆盖沿用已验证资产语义

按 D06-T02 的核心材质描述，v1 支持 baseColor、metallic、roughness、emissive、emissiveStrength、normalScale、occlusionStrength、alphaMode、alphaCutoff 和 doubleSided 的实例字段。材质的源 unlit 模型类型沿用固定记录。纹理覆盖按 BaseColor、MetallicRoughness、Normal、Occlusion、Emissive 槽保存修改值，已选材质实际消费的槽沿用其 sRGB／线性及 alpha 解释。可单独改路径、sampler 项、UV 集、偏移、缩放或弧度旋转，未提供的项沿用源值。首次给空槽增加纹理时需要有效路径，其余值从标准默认实例化。清除编辑项回到固定版本的值。

外置覆盖通过 `AssetManager` 获取 `Assets/Textures` 的 `CpuReady` PNG/JPEG 及对应的图像使用描述，Scene 拥有其引用。相同像素可在不同槽保留不同解释。验证有效 UV 集及值域后保留给消费者使用。法线纹理改变后，有效 UV 选择与原 Primitive 切线的生成条件一同可查询，生成匹配切线及实际采样由 [D06-T05](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t05) 消费验证。

### 4. v1 文件使用当前领域值

`Editor/Session` 的文件边界以 UTF-8 编解码，严格拒绝无法解释的对象成员。读取按统一预算验证 JSON 结构与字段，再把已支持值变为自持的类型化记录。写出用同一格式和预算。读取节流、字节限制及资产预算沿用 Core 与 Asset。字段名和数值表示固定如下。

| 记录 | v1 表示 |
|---|---|
| 顶层 | `version: 1`、`entities` 数组、`mainView` |
| 实体 | `id`、`name`、`parent`（ID 或 null）、`local`（`translation`、`rotation`、`scale`）、`visible`。静态可见性和投影创作标志随实体记录恢复。 |
| 实例根 | `modelInstance` 记录 `path`、`scene`、`sourceVersion`。版本采用已有 SHA-256 的 64 个十六进制字符表示，写出小写。 |
| 来源与网格 | `sourceRef` 记录 `instanceEntityId`、`node`。存在 `meshRenderer` 时从该节点解析网格并恢复 `castsShadow`。`primitiveOverrides` 逐项以 `primitiveSlot` 定位，记录稀疏的 `material` 与 `textures` 修改。 |
| 纹理覆盖 | `textures` 按标准槽名保存修改项。外置 `path` 关联槽的 `use`，`texCoord` 选择 UV0 或 UV1，`sampler` 记录改动的过滤与环绕字段，`offset`、`scale` 和 `rotation` 表达标准 UV 变换。 |
| 可选静态组件 | `camera.type` 为 `perspective` 或 `orthographic`，透视记录 `yfov`、`near`、`far`（可为 null）、可选 `aspectRatio`，正交记录 `xmag`、`ymag`、`near`、`far`。`light.type` 为 `directional`、`point` 或 `spot`，记录线性 `color` 和 `intensity`（方向灯为 lux，局部灯为 candela），局部灯可记录 `range`，spot 记录 `innerConeAngle`、`outerConeAngle`。 |
| 主 View | `cameraPose` 保存导航 `translation` 和 `rotation`，`cameraEntityId` 保存 ID 或 null，`requested.renderScale` 保存实际请求。恢复通过 View 入口，后续准备负责有效设置。 |

可选组件省略表示该实体无此组件。可选覆盖字段省略表示继承。透视 `far` 的 null 仅表达无限远，灯 `range` 省略使用无截断语义。所有数组索引在窄化前判定类型及范围。模型由 path 与所选 scene 请求，SourceRef 和覆盖只在匹配的 `sourceVersion` 下解释。载入显式实体表即可复原用户更改后的层级及删除结果。投影求值由 D06-T04 接手，环境和物理镜头随 [D07](../../../../Docs/Developments/D06-D07-SceneAndForward.md#dev-d07) 的实际 owner 引入，后续持久化字段由 [D16-T02](../../../../Docs/Developments/D16-D17-EditorAndDelivery.md#d16-t02) 完善。

### 5. 文档操作核对真实源

- Asset 从现有模型指纹输入保留主文件和去重后的实际外部依赖规范路径及摘要。沿用原 `sourceVersion` 算法和相等语义，保留紧凑核对信息。按需在 worker 使用受控读取核对固定版本的主文件与依赖，复用既有读取预算。Save 以拥有型快照中固定的不同模型版本为单位核对一次，冲突定位到引用。采用旧版实例与检查磁盘来源是各自独立的事实。
- Open 使用现有模型及图片请求、准入和完成状态，确认资源对应本次操作的实际读取证据。每次操作合并相同的资产引用，可复用本次加载已证明的字节。命中缓存或已有固定版本时，模型按依赖摘要核对，覆盖图片按当前源指纹核对。已固定模型的规范路径、实际选中 scene、源版本和磁盘读证据均匹配时直接复用，可处理默认选项导入后以显式 scene 重开同一模型。只在确知缓存旧且仍需取得当前资产时，通过现有失效和重请求取得候选，继续按文档记录检查模型版本。保存的版本不匹配即报告冲突，重绑定及显式激活流程由 [D16-T01](../../../../Docs/Developments/D16-D17-EditorAndDelivery.md#d16-t01) 接续。
- 资产 owner 保持主线程。请求准入暂满时在后续 CPU 轮次继续尝试，已有共享作业按其请求票据观察。Open 取消只退役文档操作及其持有的数据，保留其他消费者的共享请求。模型和纹理路径都从项目资源根解释，模型内部 URI 仍从模型目录解析。具体读字节时间以操作使用的受控文件快照为准，之后的外部变更由后续文件操作检查。

### 6. 主线程提交场景，Core 提交文件

1. **开始**　在编辑边界接收一个类型化操作。已有活动文件操作时返回占用。New 或 Open 先终结活动的持久化手势并处理未保存状态。可调用普通保存后再请求打开，或对当前文档版本明确放弃。New 生成无路径空场景和默认主 View 请求。
2. **Open**　冻结文档命令和主 View 持久化请求。用按需激活的单槽 Core JobQueue 读取和解析自持记录，先校验 v1 字段、ID、层级和相机引用。主线程协调必要资产请求，把固定的 CPU 记录与文档值交给 worker，按 Scene/View 的类型化合同校验资产引用并准备独立候选。worker 的输入和结果各自拥有存储。主线程在资产完成之后、图准备之前核对会话、文档、操作及取消状态。所有可能失败的校验和准备在提交前完成，提交点一起安装 Scene 和 View 请求，随后确认路径、保存基线、选择和历史。失败或取消释放候选并恢复原文档编辑。
3. **Save / Save As**　捕获 Scene 和 View 所有者的 revision、创作值、资源引用及目标相对路径。文件 worker 只处理快照，先核对固定模型，编码并写入同目录独占临时文件。Core 在实际目标目录和文件边界复用路径约束，安全替换前确认目标目录身份及文件类型，检查写入与关闭结果。操作进入替换前与取消竞争同一个提交资格，先得到提交资格的操作报告实际替换结果。不能提交的临时文件随操作收拢。
4. **确认**　按当前会话、文档和操作身份采纳结果。Save 成功登记所捕获的两个已保存 revision，Save As 成功再切换路径，后续编辑继续产生未保存状态。若取消后该操作仍在退出 I/O，继续显示收尾状态并回收其槽，已放弃的结果失去发布资格。
5. **关闭**　Runtime 在终止轮次停止新发布。EditorSession 撤销未提交操作并与文件 worker 收拢。已经取得文件提交资格的操作完成实际的提交或失败，结果不访问已关闭会话。再停止 AssetManager 及其作业，释放 Scene、View、编辑器与宿主各自持有的状态。构图和 execute 只消费既有图形数据。

Windows 实现固定已解析的卷目录，在同一目录独占创建临时文件。关闭写入句柄前把替换和清理所需权限留给同一文件对象的独立句柄。目录仍固定时，允许系统为实际替换打开目标父目录，并传入物理绝对路径完成同卷重命名。 [Win32 文件重命名参数](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_rename_info)

## Risks / Trade-offs

| 风险 | 约束和检查 |
|---|---|
| 层级组合与局部可表达性不同，实体重用可能命中过期目标 | 用非对称带符号层级和保世界重挂的可计算结果校验。删除、Undo、重建均按 Scene 和 generation 验证。 |
| 长期持有的有效实例与磁盘源可能分叉 | 文档操作按真实依赖核对。用留有热缓存的外部依赖换代检查打开、保存和独立实例。 |
| 文件取消、关闭和替换可能相互交错 | 以同一提交资格裁决取消与替换。安排带原目标的失败与受控的迟到结果，核对文件字节、路径和两个 owner 的保存 revision。 |
| 首版 schema 需要持续可重开 | 固定 v1 支持字段和值域，开关和参数随实际 owner 下一阶段引入。用同一正式入口往返和拒绝代表性无效文档。 |

## Migration Plan

当前 `Assets/Scenes` 预留目录尚无已交付的场景文档。普通启动继续使用单一正式产品入口，创建空内存 Scene。已有 glTF/GLB 文件由原资产入口只读消费。v1 写出的新文档从首个成功保存起沿本规范重开。构建使用现有锁定源码和普通 CMake 入口。
