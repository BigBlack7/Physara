# Design

## Context

Scene 已支持 `EditOverride`、实体灯组件编辑和固定版本的材质求值。当前环境只随候选 Scene 创建，v1 已支持环境、纹理覆盖和主 View 物理请求的读写。Inspector 目前主要编辑 TRS，物理曝光在 View Settings。D07-T05 已按实际接受的场景和 View 身份确认 HDR、显示颜色及最后渲染参数。 [变更提案](proposal.md) · [实体与材质](../../../../Engine/Scene/Scene.hpp) · [现有 View 编辑](../../../../Editor/Panels/ScenePanels.cpp) · [显示链验收](../2026-11-19-add-forward-display-profile/tasks.md#final-verification)

AssetManager 已发布不可变 Model/Image/Environment 版本和有界诊断，glTF 支持报告由模型版本持有。OpenGL 已交付时间戳对、非阻塞 Poll 和提交保活，Runtime 尚需把它用于用户可见的帧统计。 [AssetManager](../../../../Engine/Asset/AssetManager.hpp) · [原生查询](../../../specs/opengl-completion-queries/spec.md)

## Goals / Non-Goals

**Goals**

- 从 Inspector 字段提交到 Save/Open 和正式输出，再到同一批次的统计，保持 Scene、View、Asset 与提交各自的权威身份。
- 一套紧凑的固定资源同时服务正常编辑和 D07 阶段出口。按实际生产模式比较显示前 HDR、显示域和窗口域。

**Non-Goals**

- 扩展材质与透明合成继续由 D12/D13 交付。当前 Inspector 以核心 OPAQUE/MASK 与现有 Unlit 的适用字段为范围，对已导入的不可绘制组合沿用 Forward 原因。 [D12/D13](../../../../Docs/Developments/D12-D13-Materials.md#dev-d12) · [核心透明](../../../../Docs/Developments/D12-D13-Materials.md#d13-t01)
- D08-T05 完善通用传输、staging 和查询验收。长期多场景性能对照归后续实际测量。 [D08-T05](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t05) · [12.2](../../../../Docs/Architecture.md#benchmark)

## Decisions

<a id="authoring-identity"></a>
### D1. 以字段意图更新当前权威值

Inspector 仅读取选中实体、源 Primitive、稀疏覆盖和有效材质。控件身份随文档、实体代和 Primitive／槽／字段变化，输入确认后交给 EditorSession 的帧边界。用已接受状态合成本次字段所需的 `EditOverride` 或灯组件事务，避免临时面板快照覆盖同轮其它字段。数值以明确提交为基线，资源选择、清除和枚举切换使用离散命令，共用现有有界历史。无净变化的提交经既有入口返回。 [领域编辑](../../../../Docs/Architecture.md#edits) · [生命周期和版本](../../../../Docs/Architecture.md#static-content)

- 材质控件使用线性因子与现有标准范围。通过 `Scene::Material` 显示解析值，清除单字段时保留同一 Primitive 的其它稀疏项。外部纹理用途随槽固定，颜色输入保留 sRGB 语义，normal 和数据槽各用现有线性解释。提供当前 Primitive 已具备的 UV0/UV1、标准 sampler 和 UV 变换。适用性和完整采样组合由 Scene 在应用入口校验，沿既有切线求值及驻留合同消费。 [材质数据合同](../../../../Engine/Scene/SceneMaterial.hpp) · [纹理语义](../../../../Docs/Architecture.md#asset-images)
- 灯控件编辑当前 Directional、Point、Spot 组件。显示 lux 或 cd、米和锥角度数，边界转换为已有角度单位。无 range 复用 `std::optional` 表达；Scene 校验有限强度、颜色、range 与锥角关系。灯位姿沿既有 TRS 和世界刚体方向进入可见灯表。 [物理输入](../../../../Docs/Architecture.md#physical-inputs) · [准备契约](../../../../Docs/Architecture.md#light-preparation)
- Inspector 的主 View 曝光区域与 View Settings 复用字段控件和提交身份。无选中实体时仍可访问环境和主 View。相机源读取主 View 请求，曝光只写入相应 View 文档字段。请求 EV100／preExposure 和最后正常出图值以对应场景标识显示。 [相机](../../../../Docs/Architecture.md#camera) · [主 View 事务规范](specs/editor-domain-edits/spec.md)

<a id="resource-requests"></a>
### D2. 在 CPU 源与正常提交处分别确认编辑

场景环境是 Scene 的可选值。给现有 Scene 编辑／历史入口增加窄环境事务，使用与候选 Scene 创建相同的源、单位四元数、非负倍率和用途校验。初次设置以单位旋转、倍率 1 以及照明和天空为缺省，另有明确移除操作。提交及重放持有该事务前后两项固定版本和设置；在所属 Scene 验证前提后发布。环境变化各推进一次文档与渲染内容 revision，只有相机／主 View 自己的变化才推进它们的版本。保存与打开走 v1 的现有环境、灯、覆盖和 mainView 字段。 [场景规范增量](specs/static-scene/spec.md) · [v1](../../../../Docs/Architecture.md#asset-persistence)

对路径显式确认时，经现有项目路径限制和 AssetManager 提交异步图像或 HDR 申请。EditorSession 每个活动文档只保留一项待确认的来源编辑，绑定 Scene、目标及资产请求代。新来源意图取代上一个；针对同一字段的清除／新编辑或重放使旧意图失效。CPU 结果回到编辑边界复查身份并仅合并目标字段，若当前有连续手势则在历史可提交时再交付。Open 冻结、文档退役或取消先撤销它的提交资格，共享 Asset 发布由 AssetManager 管理。保存捕获已经提交的创作值，后续真正接受的编辑继续置脏。 [资产发布](../../../specs/cpu-asset-publication/spec.md) · [文档冻结与保存](../../../specs/scene-document-persistence/spec.md)

源选取使用现有项目相对路径输入模式，用户可直接输入所需项目纹理或 EXR。Inspector 显示所选模型固定版本的 `supportReport`，已失败的导入继续从 Scene status 显示。回填资源的 CPU Pending/失败、已接受的 Scene 请求和 View 实际结果使用各自的只读状态。相同环境源的旋转、强度与用途沿既有预计算身份选择当轮所需产品。在资源不完整时保旧，匹配的正常帧通过原提交边界确认新代。 [支持报告](../../../specs/static-gltf-import/spec.md) · [照明代与保旧帧](../../../specs/environment-lighting/spec.md)

<a id="reference-inputs"></a>
### D3. 固定一个可编辑场景和少量独立标样

通过默认 exe 同级资源树装配 `Assets/Scenes/ForwardReference.scene.json` 及其自制小型 glTF 和线性 EXR。场景包含介质与金属粗糙度材质球，以及带米制标记、平坦读数区和斜边的非对称布局。保留可分别观察的同源实例、已知朝向与位置的方向灯／点光／聚光灯和显式发光片。源环境以确定性的 equirectangular 线性相对辐亮度构造，使用常量通道与不同轴的方向梯度。采用 `L_R(d)=1+0.5d_x`、`L_G(d)=1+0.25d_z` 时，+X 和 +Z 的区分可独立于资产生成或采样实现推导。资源来源、项目许可、依赖 SHA-256、模型 `sourceVersion`、标记位置和打开步骤集中记录在 Assets 使用说明。 [参考场景合同](specs/forward-reference-scene/spec.md) · [资源位置](../../../../Docs/Build.md#运行)

基线相机固定画幅与姿态，数值检查采用 4:3、640×480 的有效渲染栅格、render scale 1.0、None。光圈 f/4、快门 1/16 s、ISO 100、补偿 0 EV，使 EV100 为 8、预曝光为 `1/307.2`。正式打开后从 View Settings 选择 Physical Lighting。单光、关环境、旋转、+1 EV 和 FXAA 对照由相同起点的类型化编辑产生，随各次 readback 记录请求及实际生产快照。试验副本、生成与读回脚本、日志和图片放在 `build/d07-t06/`，随仓库交付的输入是普通示例资产。 [相机标定](../../../../Docs/Architecture.md#camera) · [正式运行入口](../../../../Docs/Build.md#运行)

比较以配对的 `ForwardOutput` 工作色、D32、生产模式和有效栅格取样。直接光用 Direct Lighting 单独测量，沿 D07-T03 正入射标样配置 4 lx、16 cd／2 m 和 16 cd／4 m，分别验证相等与四分之一的关系。Physical Lighting 另检查直接光与 IBL 同时有贡献、环境旋转后的天空／受光方向和曝光阶跃。HDR 对已知点逐通道报告绝对与相对差，解析直接光和 +1 EV 对照的绝对差上限为 `max(5e-5, 0.02 × |期望值|)`。天空方向通道除以同帧预曝光，再以 0.01 的绝对误差限和解析辐亮度比较。受光 IBL 对照 D07-T04 的独立积分和 RGBA16F 格式预算，方向标样还须判别朝向。 [直接光参考](../2026-11-18-add-forward-direct-lighting/tasks.md#final-verification) · [环境光参考](../2026-11-18-add-forward-environment-lighting/tasks.md#final-verification)

显示比较使用实际 ViewOutput、UI 合成色与原生窗口同一标样对齐后的样点，以独立 SDR 映射、线性采样和实际 sRGB 编码预测 8 位颜色。复用 D07-T05 的每通道 ≤2 个码值判据。相同生产模式、尺寸及曝光下对照 None/FXAA 的显示前 HDR、深度和已标明的斜边。记录实际驱动、窗口 framebuffer、有效矩形及采样点，以便识别翻转、留边和重复编码。 [输出合同](../../../../Docs/Architecture.md#display-output) · [显示验证](../2026-11-19-add-forward-display-profile/tasks.md#final-verification)

<a id="statistics-and-timer"></a>
### D4. 由事实 owner 提供帧统计和有界计时

Runtime 在已有阶段边界用单调时钟取得 CPU 区间，关联当前 UI 批次、实际 SceneFrame／View／Draw 准备、图编译与执行。PresentWait 使用实际交换返回的耗时。Renderer 取已准备 Draw 和灯表项、编图存活 Pass 数及 Forward 驻留 owner 的几何、图像／环境和材质缓存条目数。给每项标明口径和单位，已登记待淘汰的代归其所属缓存。RHI 从 OpenGL 实际发出 Draw、原生绑定、上传字节及 barrier 的位置计数，按录制区间取差；状态缓存消去的操作不计。可读快照仅含值和身份，通过 Runtime 汇集给 Editor，面板不遍历 Scene 或调用 RHI。 [统计合同](../../../../Docs/Architecture.md#diagnostics) · [现有录制与确认边界](../../../../Runtime/Frame.cpp)

快照与实际发布的正常场景绑定，包含对应的 Scene/View、范围、实际 profile/AA、提交及呈现状态。失败尝试、没有新场景结果的帧与安全 UI 各按现有诊断表达，统计面板继续标明最后一次匹配的正常结果。第一版只保留最近的场景统计和有身份的最近就绪计时，界面隐藏时跳过面板工作。 [统计规范](specs/render-frame-statistics/spec.md)

用现有 FrameSlot 数量作查询容量。支持的设备在冷路径取得计时句柄，常规录制只使用当前已可用的项。把一对原生点放在包含场景与 UI 的图执行两端，区间排除窗口交换，使用同一 Graphics 录制。成功提交且确认对应正常场景后保留票据和配置；可继续录制的失败支路需要结束已开始的区间并撤销样本资格，已失效的列表沿现有设备收拢。压力时跳过计时，耗时读取在后续 render 线程调用 `PollTimerQuery`，仅在返回 true 时调用 `GetTimerQueryTime` 并转换成显示单位。Ready、Pending、Unavailable／跳过及失败有明确状态，Ready 时长始终带原票据。样本就绪与提交退休继续各由既有 query 和 Event 合同处理；释放句柄、回收包和关闭设备保持 owner 顺序。 [计时与退休](../../../specs/opengl-completion-queries/spec.md) · [非阻塞获取](../../../../Docs/Architecture.md#readbacks)

固定参考输入验证计数、单位和范围即可形成成本基线。后续确有性能比较时，固定构建、设备／驱动、场景 SHA、相机、分辨率、灯、效果、VSync、预热和采样数量，CPU/GPU／等待分开给出 median/p95 和质量误差。测量继续经过 Runtime 的正常链。 [12.2](../../../../Docs/Architecture.md#benchmark)

## Risks / Trade-offs

- 同一模型的两个实例共享源版本，字段误写会跨实例生效 → 使用原实体和源槽的字段身份，在生产路径用两个实例区分覆盖与继承。
- CPU 源晚于用户清除或更换文档就绪 → 核对当前提交资格，再将完成值合入场景事务；选择一个可控的晚回结果检查。
- 工作色读成已映射颜色或使用了旧统计 → 在同一已接受帧记录相机、预曝光、布局和提交身份；用方向色块及一个晚到时长样本核对。
- 即时录制失败时已有计时点和其他图工作 → 仅成功并匹配的样本可见，包引用及真实完成合同负责回收，按影响复用既有退出证据。

## Migration Plan

已有 v1 文件继续从普通 Open Scene 入口按现有字段读入。实现先接 Scene/View 编辑及状态，再装配正式参考资产和 Runtime 统计。参考原始文件用独立路径管理，编辑演示走另存为。新能力随本 change 通过 B 验收、最终 verify 和 D07 阶段出口后同步正式 Spec。 [开发计划](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d07-t06)
