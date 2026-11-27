# Tasks

## 1. A · 标准资产与实例覆盖

- [x] 1.1 扩展源材质、槽语义和导入支持报告。以 `build/d12-t01/` 下的小型静态 glTF/GLB 资源树通过正式导入口核对缺省、必需声明、源 IOR=0、大于 1 的颜色因子和同图像的 A/RGB 独立用途。复用原纹理解析检查 UV 变换及有效 UV 集。以一个非法 IOR、一个缺 UV 槽和一个同材质 Unlit 冲突核对字段原因。只变更参与导入的外部扩展纹理验证新 sourceVersion 与旧固定版本。 [导入增量](specs/static-gltf-import/spec.md)
- [x] 1.2 完成 Scene 有效值、Inspector 编辑和 v1 稀疏覆盖。对共享源的两个实例走实际编辑、清除、Undo/Redo 及保存重开，核对来源、非目标实例和新旧版本。以零 IOR 的合法源继承、非法实例写入和省略新字段的旧 v1 证明兼容边界，以纹理覆盖对照核对用途和 UV 设置。 [编辑增量](specs/editor-domain-edits/spec.md) · [文档增量](specs/scene-document-persistence/spec.md)

## 2. A · 生成接口与物理响应

- [x] 2.1 扩展 `StaticForward` 生成接口、材质行编码及产品目录。先构建受影响的接口和包，以生成布局、stride、两阶段反射和活动资源核对核心、扩展颜色、核心 GBuffer、SurfaceAux、深度及阴影角色。核对 `t5` 材质表与 `t6/s6`、`t7/s7` 的映射，枚举实际可达角色并记录数量。编译端点一致、原有角色的活动资源配方完整后停止本组接口检查。 [ABI 与角色设计](design.md#material-abi-and-color-roles)
- [x] 2.2 实现共享材质采样、F0/F90、介质直接光预算及金属混合。用同一数值夹具的非对称贴图以及正入射、掠射、零强度、非默认 IOR 和零 IOR 标样，核对可代表介质、金属和混合响应的独立解析值。以纯金属因子变化对照检查介质串扰，以颜色因子和强度组合区分 F0 先限幅再乘权重的顺序。 [表面增量](specs/static-surface-evaluation/spec.md)
- [x] 2.3 接通共享 DFG 与 IBL，用独立高精度方向反照率参考及原生样点核对非单位 F90、匹配粗糙度和视角、一次镜面补偿。复用同一材质夹具获得单位环境的零强度与全反射见证，再用仅直接光、加入非对称环境、AO 与 +1 EV 对照核对工作色归因。记录实时近似的偏差来源和样点误差界。 [环境增量](specs/environment-lighting/spec.md)

## 3. A · 实际路由、资源与交付

- [x] 3.1 从有效材质选择路由和产品角色，贯通 Draw/批次、已冻结的材质版本、Pipeline 及图资源。采用一组真实导入的 MASK 扩展与核心前后遮挡、Unlit 及有限远面标样，检查 G1、G3 和 D32。用材质覆盖关键值和等深结果核对共同胜出次序，保留阴影 caster 的覆盖证据。固定场景、曝光、栅格、采样和候选策略，比较三路径匹配票据的 Direct Lighting 与 Physical Lighting 工作色，记录实际扩展 Draw 数和颜色生产身份。
- [x] 3.2 按角色落实新增图读与帧前准入，接通 Inspector 支持说明和受光就绪反馈。用一次原生扩展角色故障及合法恢复覆盖冷准备、独立表面交付、正常颜色拒绝和未选角色的热候选拒绝。核对旧完整代、保留的输出及恢复票据，并记录本次原生活动绑定和设备上限所对应的配置。 [三路径增量](specs/static-forward-rendering/spec.md) · [Shader 准入增量](specs/shader-generation-reload/spec.md)
- [x] 3.3 用完成帧核对实例覆盖从继承到扩展、清除及 Undo 后的材质身份、路由和历史前驱。使用同一可见模型和实际图资源交付，验证成功编辑按静态内容域使相应 View 历史失效，失败编辑保留此前可用结果。运行已有编辑器受影响检查，将可重现的可见结果记录在本任务证据。 [静态内容合同](../../../../Docs/Architecture.md#static-content)

## 4. A · 集成检查与独立审查交接

- [x] 4.1 重建 Debug 和 Release 正式产品，共享 ABI 的直接消费者使用本轮生成物和包。用本机构建及隐藏启动确认实际装配，运行 D11 三路径受影响的 `mixed` 及带环境、阴影的代表性 `reference` 对照。复用已有冻结场景和判据，当前实现需要读回的项目以当前包重放。按原生提交票据配对设备、模式、渲染矩形、场景版本和 Shader 代。核心区沿 D11 记录的阈值，扩展覆盖与深度先对齐，再按冻结的工作附件量化和参考误差判断 HDR。 [构建入口](../../../../Docs/Build.md) · [D11 复验入口](../../../../Docs/Performance/ThreePathReference.md#replay)
- [x] 4.2 核对规格、实际命令与证据范围，运行 `openspec validate add-ior-specular-material-profile --strict`。将实现输入、生成 ABI、正式包、回放夹具、设备和产物身份、实际执行、复用与未覆盖项登记在 change 的任务证据中。按 [AgentModels](../../../../Docs/Developments/AgentModels.md#review-tier) 在 [当前问题单](../../../../build/d12-t01/final/review-signed.md) 给出 B 档位建议及具体风险，交由维护者安排独立审查。

开发用样例、脚本、回放及临时工程放在 `build/d12-t01/` 下。以上本地检查在已变更合同及其直接消费者有对应证据后停止，追加检查以失败、输入变化或具体缺口为依据。 [验证停止条件](../../../../.agents/skills/physara-verification/SKILL.md)

## 5. B 审查、A 整改与 B 复核

- [x] 5.1 独立 B 核对当轮源码、包与证据身份，审查 Asset/Scene 字段、介质/金属能量、实际 Shader 活动资源和三路径辅助遮挡，完成受影响第一方文件的注释核对。在当前问题单登记已验证的问题、依据及复核要求。首审登记 CR-001 至 CR-007，等待 A 整改。[当前问题单](../../../../build/d12-t01/final/review-signed.md)
- [x] 5.2 A 修正 CR-001 至 CR-007 以及复核新增的 CR-008，完成相关回放并更新证据、输入身份和交接说明。 [首轮整改](#a-correction-1) · [CR-008 整改](#a-correction-2)
- [x] 5.3 独立 B 在对应修正版本上复核，补齐受影响文件的职责、接口和流程注释。问题单所有本轮事项关闭且验收覆盖到最终输入后，由 B 记录签收版本并勾选 B 复核项。 [独立审查闭环](../../../../AGENTS.md#review-loop) [B 签收记录](#b-recheck-2)

## 6. A · 最终验证、同步与归档

- [x] 6.1 据 B 签收的输入执行最终 OpenSpec verify 和所需原生验证，复用身份完全匹配的证据，对后续变化和具体缺口重建或重放。总结 T01 实际支持范围、已接受组合和三路径证据，将档位建议、实际 B 配置与验收记录归入当前 tasks。 [最终验证](#final-verification)
- [x] 6.2 在最终 verify 通过后同步增量、归档 change、验证主规格及工作包状态与链接，再重置当前问题单。开发计划引用归档和实际验收，其余工作包按自己的前置推进。 [工作包](../../../../Docs/Developments/D12-D13-Materials.md#d12-t01) · [归档核对](#archive-closeout)

<a id="a-local-verification"></a>
## A 本地验证

待审输入为 `v2.0@5a02cd87b2e208cfdcb5a668b2cf1b99de226843` 加本次工作树。原生回放使用当前配置的正式库、包和生成接口。D12 标样由本地脚本生成，D11 回放沿用冻结场景和比较程序。设备为 Intel RaptorLake-S Mobile，OpenGL 4.6.0，驱动 32.0.101.6129。 [标样](../../../../build/d12-t01/Fixture.py) · [回放入口](../../../../build/d12-t01/RunReplay.ps1)

| 检查 | 结果 |
|---|---|
| 资产与实例 | glTF、GLB 的必需扩展及缺省、源 IOR=0、非单位颜色因子和 A/RGB 分槽通过。非法 IOR、缺 UV、同材质 Unlit 冲突定位到字段或槽。仅替换外部扩展纹理即改变模型指纹，已固定旧版本继续可读。共享模型的实例编辑、非法写入、清除、Undo/Redo、旧 v1 与存有新稀疏字段的 v1 重开通过。 [Debug](../../../../build/d12-t01/ior-assets-Debug.log) · [Release](../../../../build/d12-t01/ior-assets-Release.log) |
| ABI 与产品角色 | `ForwardMaterial` 为 320 字节，IOR 位于 300、specular 位于 304。共同的材质表保持 t5。两种生成配置中的 12 个扩展颜色包均使用新增 t6/s6、t7/s7，原有角色按实际阶段配方检查。扩展色有 7–10 个活动片元纹理，设备片元限额为 32。 [Debug](../../../../build/d12-t01/ior-admission-Debug.log) · [Release](../../../../build/d12-t01/ior-admission-Release.log) |
| 冷包、图与热候选 | 同 ABI 的冷包缺少 `specularImage` 时，80×48 独立表面仍以原生票据交付 G3 4/1 和 D32 0.625/0.5。场景中的 specular 强度、颜色和 AO 图像在恢复后的颜色图中可达。正常颜色报告包角色和缺项。未选的热角色触发同样的资源检查，旧完整代继续交付，合法包恢复后读回扩展 HDR。 [Debug](../../../../build/d12-t01/ior-fault-Debug.log) · [Release](../../../../build/d12-t01/ior-fault-Release.log) |
| 三路径与编辑历史 | Direct 和 Physical 的真实导入场景各有 4 个扩展 Draw。两组 D32 和各自的 10 个阴影层逐字节相同，17,328 个 G3=4 像素的扩展 HDR 逐字节相同。MASK 孔、被遮挡扩展、Unlit、有限远面 D32=0、等深胜出和 G1 均通过。实例覆盖、清除及 Undo 的扩展 Draw 计数依次为 5、4、5。成功编辑使历史前驱失效，非法写入保有原版本和有效前驱。Undo 的 HDR 与 G3 恢复，三次 D32 一致。 [已接受票据](../../../../build/d12-t01/ior-routes-Release.log) · [像素与历史摘要](../../../../build/d12-t01/native/ior-proof-routes.json) |
| 直接光、DFG 与 IBL | 对感知粗糙度 0.55、视角 0 和 1.1 弧度的 16 个样点比较非对称纹理、F0 限幅、非单位 F90、纯介质、纯金属及中间金属度。以独立入射方向半球积分核对量化 DFG 的预期响应，最大通道绝对误差在曝光还原后分别为 Direct 0.001762、带直接光的单位环境 0.005543、无灯单位环境 0.000391。误差涉及 DFG 积分与插值、纹理取样及 RGBA16F 量化。无灯全反射样点为 0.999609，零强度的结果对应基色漫反射。非对称环境与 DFG-only 对照验证 AO 归因，+1 EV 的 85,728 个已着色像素全部精确翻倍，D32 一致。 [实机票据](../../../../build/d12-t01/ior-light-Release.log) · [数值参考与读回摘要](../../../../build/d12-t01/native/ior-proof-all.json) · [独立积分](../../../../build/d12-t01/IorProof.py) |
| 正式构建与 D11 回归 | Windows x64、MSVC 19.44.35228、SDK 10.0.26100.0 的 Debug 和 Release 目标均构建通过，两版的 `--hidden --poll-limit=3` 均返回 0。Debug 原生回放覆盖资产与文档、ABI、故障准入及 `mixed`。Release 完成本轮全部原生模式与独立数值比较，重放 D11 的 `mixed` 及带环境、阴影的 `reference`。D11 核心像素沿原容差，`reference` 的 11 个阴影切片逐字节一致。 [Debug 构建](../../../../build/d12-t01/product-Debug.log) · [Release 构建](../../../../build/d12-t01/product-Release.log) · [D11 mixed](../../../../build/d12-t01/native/mixed-proof.json) · [D11 reference](../../../../build/d12-t01/native/reference-three-path-proof.json) |

SDK 按现有版本显式传给 MSBuild，以避开沙箱下的用户 SDK 目录探测。设备证据来自上述单台 OpenGL 机器。热检查从正式包构造候选后进入 Renderer 准入，实例操作通过编辑会话的类型化入口及文件作业。OpenSpec 严格校验通过。 [严格校验](../../../../build/d12-t01/openspec-validate.log) · [构建命令和 SHA-256 清单](../../../../build/d12-t01/handoff.json)

A 建议独立 B 使用 Y 档，GPT-6.2 Astra，`high`。复核重点为核心与扩展的材质行及活动绑定、介质和金属的直接与环境预算、实际获胜的 G3/D32、未就绪角色及 View 历史。维护者确定实际审查配置。 [当前问题单](../../../../build/d12-t01/final/review-signed.md) · [分级依据](../../../../Docs/Developments/AgentModels.md#review-tier)

<a id="a-correction-1"></a>
## A 第 1 轮整改

本轮依据 B 首审登记的 CR-001 至 CR-007 逐项修正。复核输入为 `v2.0@5a02cd87b2e208cfdcb5a668b2cf1b99de226843` 加当前工作树，含独立首审补充的注释、12 个未跟踪产品声明及当前 change。重建后的库、包、回放脚本及读回按 SHA-256 登记。 [原问题单](../../../../build/d12-t01/final/review-signed.md) · [整改交接清单](../../../../build/d12-t01/a-corrections/handoff.json)

| 问题 | 整改证据 |
|---|---|
| CR-001 源重载容量 | 正式源观察、候选交付和帧边界发布贯通。初始 44 个包，接口重编再增加 37 个，共 8,485,388 B，默认 16 MiB 有余量。兼容换行保持 HDR/D32。实际着色修改使同一覆盖点的蓝通道从 0.00162697 降至 0.000813484，新代正常提交。显式 8 MiB 在旧代仍持有时保留原容量拒绝。 [Release](../../../../build/d12-t01/a-corrections/correction-reload-Release.log) |
| CR-002、004 共享能量 | 单位环境、固定非单位 AO 下，0.25 金属度结果与两个实测端点的线性插值误差低于 0.00012。有效半角低于着色法线和严格退化半角分别以独立解析工作色核对，曝光还原后的绝对误差均低于 0.00006。 [样点及票据](../../../../build/d12-t01/a-corrections/correction-light-Release.log) |
| CR-003 辅助覆盖 | 单面扩展与双面 Unlit 等深，并列另一个由两张扩展纹理组成的共面组。独立 SurfaceAux 冷、暖及正常 Deferred 的 G1/G3/D32 一致。两个样点的分类为 2/4，深度均为 0.5，正常三路径的胜出工作色逐通道相同。单独验证了 OpenGL 批次间屏障，当前正式回放未启用定位时的逐批查询。 [独立表面](../../../../build/d12-t01/a-corrections/correction-surface-Release.log) · [三路径回放](../../../../build/d12-t01/a-corrections/correction-surface-runtime-Release.log) |
| CR-005、006 上传额度 | 只有颜色读取的 UV1 在独立辅助表面中省下 32 B 顶点写入，实测顶点写入 160/160/192 B。五个核心槽齐全时存活图读使用对应的 4 个实际图像，缺席活动槽加入 4 B 白图。用真实 Draw 准备阶段的预留账目核对 64 MiB 临界准入，并单独运行正常图验证配对颜色和深度。 [冷准备与原生图](../../../../build/d12-t01/a-corrections/correction-budgets-Release.log) |
| CR-007 组件存在性 | 固定源 mesh 相同，关闭投影时 ImGui 显示 Primitive 及 IOR/specular 输入，类型化领域编辑接受对应值。移除 MeshRenderer 时隐藏表面并撤销实例材质查询。 [面板回放](../../../../build/d12-t01/a-corrections/correction-inspector-Release.log) |

Debug 和 Release 正式目标均构建通过，隐藏窗口启动正常。冷缺包与热候选拒绝各用对应的准备入口。冷包先在正常 View 请求中定位 `specularImage` 缺项并继续交付原 Forward，随后单独提交不合法的热候选并读回原代。回放明确检查 `ScenePipelineUnavailable`，等待中的提示只用于观察准备进度。 [Debug 构建](../../../../build/d12-t01/a-corrections/product-Debug.log) · [Release 构建](../../../../build/d12-t01/a-corrections/product-Release.log) · [Debug 故障回放](../../../../build/d12-t01/a-corrections/ior-fault-Debug.log) · [Release 故障回放](../../../../build/d12-t01/a-corrections/ior-fault-Release.log)

Release 重放 D12 的 Direct 和 Physical 三路径、独立 BRDF/DFG 积分、MASK 及历史恢复。Debug 和 Release 重放 D11 混合表面，Release 还重放包含环境与 11 层阴影的 D11 参考场景。 [D12 像素与数值](../../../../build/d12-t01/a-corrections/native/ior-proof-all.json) · [Debug 混合表面](../../../../build/d12-t01/a-corrections/debug/mixed-proof.json) · [Release 混合表面](../../../../build/d12-t01/a-corrections/native/mixed-proof.json) · [D11 参考比较](../../../../build/d12-t01/a-corrections/native/reference-three-path-proof.json)

原生证据覆盖 Intel RaptorLake-S Mobile 的 OpenGL 4.6.0，驱动 32.0.101.6129。B 本轮另需复核等深批次的 OpenGL 状态、局部屏障和引用寿命，以及受整改影响的注释。 [OpenGL 发射](../../../../RHI/OpenGL/CommandListDraw.cpp) · [本轮问题单](../../../../build/d12-t01/final/review-signed.md)

<a id="b-recheck-1"></a>
## B 第 1 轮整改复核

B 在整改清单的 574 条记录与当前文件匹配后，隔离重放 Release 的六个定向模式，核对受影响的注释及 OpenGL 绘制包的 FBO 引用寿命。CR-001 至 CR-007 均已复核通过。实测共面顺序反转后，左侧可见表面在三路径中一起从 Unlit 变为受光扩展。非单位 AO 下，中间金属度与实测端点插值的最大误差为 0.000110。[B 读回核对](../../../../build/d12-t01/b-recheck-1/B-readbacks.log)

新增 CR-008 待 A 整改。独立 SurfaceAux 为 OPAQUE 扩展上传了只供颜色使用的基色图。4096×4096 输入正常完成 CPU 导入，但独立表面因 64 MiB 上传预留被拒绝。同一材质的小图与无图对照具有逐字节相同的 G1/G3/D32。任务 5.3 保持待验收。[CR-008 与复核要求](../../../../build/d12-t01/final/review-signed.md#cr-008) · [B 单表面实测](../../../../build/d12-t01/b-recheck-1/opaque-surface-proof.log)

<a id="a-correction-2"></a>
## A 第 2 轮整改

独立 OPAQUE 辅助表面使用独立的材质缓存键。原生基色槽借用该行已经准备的表面图像，Graph 按实际绑定合并读。颜色和 MASK 依然按各自的角色准备真实基色。复核使用 `v2.0@5a02cd87b2e208cfdcb5a668b2cf1b99de226843` 加当前工作树及重新构建的正式产物。 [CR-008](../../../../build/d12-t01/final/review-signed.md#cr-008) · [交接清单](../../../../build/d12-t01/a-correction-2/handoff.json)

- **冷、暖表面**　B 的无图、2×2 和 4096×4096 OPAQUE 输入在两个配置下均交付相同的 G1/G3/D32。中心 G3=4、D32=0.5。冷图各写入一笔共用 4 B 白图，已有颜色行时独立表面的暖图继续只读表面所需图像，纹理上传为 0。带实际法线与粗糙度图的对照读回对应的法线和粗糙度，存活纹理写入是一笔 16 B。 [Debug](../../../../build/d12-t01/a-correction-2/cr008-Debug.log) · [Release](../../../../build/d12-t01/a-correction-2/cr008-Release.log)
- **实际基色与拒绝**　用半精度 HDR 及独立颜色计算确认恢复正常颜色后仍取用真实 sRGB 基色。切换前后的 HDR 全图一致。MASK 在透明图中心保留 G3=0、D32=0。4096×4096 基色在正常颜色请求中按 64 MiB 上传额度拒绝，随后的独立表面仍正常交付。 [颜色与预算对照](../../../../build/d12-t01/a-correction-2/cr008-Release.log)
- **受影响回归**　Debug 和 Release 正式构建及隐藏启动通过，两版均重放上传预算与冷热缺包路径。Release 另重放共面辅助表面、同场景三路径及 D11 混合表面对照。实机覆盖 Intel OpenGL 4.6。 [Debug 构建](../../../../build/d12-t01/a-correction-2/product-Debug.log) · [Release 构建](../../../../build/d12-t01/a-correction-2/product-Release.log) · [实际三路径](../../../../build/d12-t01/a-correction-2/correction-surface-runtime-Release.log) · [D11 像素比较](../../../../build/d12-t01/a-correction-2/native/mixed-proof.json)

<a id="b-recheck-2"></a>
## B 第 2 轮复核与签收

B 签收 D12-T01 的 `v2.0@5a02cd87b2e208cfdcb5a668b2cf1b99de226843` 加 A 第 2 轮整改工作树。交接清单 SHA-256 为 `992b45a999ed9b08b6fcdbeef4ee891dda8c595e67610ed31d4869375d670aa4`。落签前核对全部 471 条记录，含 64 个工作树文件、两个构建配置的库、包和登记的回放。CR-001 至 CR-007 沿用第 1 轮 B 复核，CR-008 本轮通过。实际审查档位、模型与推理级别待维护者确认。[签收清单](../../../../build/d12-t01/a-correction-2/handoff.json) · [当前问题单](../../../../build/d12-t01/final/review-signed.md#cr-008)

B 隔离重放 Intel OpenGL 4.6 的 Debug 和 Release。无图、2×2 和 4096×4096 OPAQUE 基色输入交付相同的独立表面，冷准备均为 291,744 B，纹理上传均为 4 B。两个配置下各七组表面读回的 G1/G3/D32 与上轮 B 的冻结对照逐字节相同。MASK 保留透明孔，实际表面贴图得到相应法线和粗糙度，切回正常颜色的 HDR 全图与首次颜色一致。大图颜色请求按限额拒绝后，独立表面仍可提交。B 核实了用途键、绑定复用及图读的注释，本轮只更新验收记录。任务 6.1、6.2 待 A 完成。[Debug](../../../../build/d12-t01/b-recheck-2/cr008-Debug.log) · [Release](../../../../build/d12-t01/b-recheck-2/cr008-Release.log) · [像素对账](../../../../build/d12-t01/b-recheck-2/readbacks.log)

<a id="final-verification"></a>
## A 最终验证

2026-11-28 核对独立 B 落签后的分支、基准和全部 64 个修改及未跟踪文件。交接清单的 471 条记录中，469 条的大小及 SHA-256 一致。另两条是 B 落签后更新的问题单和本任务文档。CR-001 至 CR-008 均经 B 关闭。B 确认 Asset/Scene 输入和版本寿命、Inspector 与 v1 读写、Renderer 角色绑定与图读、Shader 生成和着色、OpenGL 等深批次及 FBO 保活的注释。A 建议 B 使用 Y 档、GPT-6.2 Astra `high`，实际档位、模型及推理级别待维护者登记。 [签收问题单](../../../../build/d12-t01/final/review-signed.md) · [输入核对](../../../../build/d12-t01/final/signed-inputs.json)

最终支持范围为 lit metallic-roughness 的 OPAQUE 和 MASK。IOR、specular 可各自声明或组合，并可通过实例稀疏覆盖生效。源 IOR=0 走兼容模式。三条路径共享介质和金属响应，扩展色按全灯表或 Froxel、Direct 或两类 Physical、阴影配置使用 12 个已准入的颜色角色。Deferred 输出可见辅助表面的 G1、G3=4 与 D32，并用匹配的扩展角色交付颜色。 [设计](design.md) · [角色准入](../../../../build/d12-t01/final/ior-admission-Release.log)

Windows x64、Intel RaptorLake-S Mobile OpenGL 4.6.0（驱动 32.0.101.6129）上，按签收摘要复用 Debug、Release 正式构建，重新隐藏启动，两版均正常退出。Release 隔离重放资产与 v1、活动 ABI、Inspector、三路径与历史、BRDF/DFG/IBL 及 CR-008。复用同一签收输入的 Debug/Release 上传预算与冷、热准入，以及 Release 的共面顺序、混合表面和 B 的专项复核。两种配置各 45 份 Shader 包均与首轮整改构建具有相同摘要。 [最终验证清单](../../../../build/d12-t01/final/verification-manifest.json) · [整改回归](#a-correction-2)

| 合同 | 最终证据 |
|---|---|
| 导入、实例编辑和场景 v1 | 合法 glTF/GLB、独立槽、外部图像版本、非法项拒绝、稀疏覆盖和旧 v1 重开均通过。Inspector 按 MeshRenderer 的存在性显示可编辑 Primitive。 [资产和文档](../../../../build/d12-t01/final/ior-assets-Release.log) · [Inspector](../../../../build/d12-t01/final/correction-inspector-Release.log) |
| 共享表面与环境照明 | 正视及斜视的独立半球积分对照通过。Direct、带直接光的单位环境和无灯单位环境的曝光还原后最大通道绝对误差分别为 0.001762、0.005543、0.000391。非对称环境的 AO 与 DFG-only 对照通过，+1 EV 的 85,728 个已着色像素精确翻倍。 [原生票据](../../../../build/d12-t01/final/ior-light-Release.log) · [数值对照](../../../../build/d12-t01/final/replay/build/d12-t01/a-correction-2/native/ior-proof-all.json) |
| 三路径、历史和独立辅助表面 | Direct 和 Physical 各有 4 个扩展 Draw。各组完整 D32 和 10 层影图逐字节一致，17,328 个 G3=4 像素的 HDR 逐字节一致。MASK 孔、有限远面及等深胜出通过，Undo 恢复路由和颜色。CR-008 的 35 份 G1/G3/D32 和 HDR 读回逐字节匹配 A、B 的冻结结果。 [三路径票据](../../../../build/d12-t01/final/ior-routes-Release.log) · [CR-008 对账](../../../../build/d12-t01/final/cr008-readbacks.json) |
| Shader 冷、热准入 | 320 B 材质行、12 个扩展色角色和实际片元纹理数均通过。签收回放验证冷缺包时的独立表面、正常颜色拒绝、未选热角色拒绝保留旧代和合法恢复。 [ABI 回放](../../../../build/d12-t01/final/ior-admission-Release.log) · [Debug 准入](../../../../build/d12-t01/a-correction-2/ior-fault-Debug.log) · [Release 准入](../../../../build/d12-t01/a-correction-2/ior-fault-Release.log) |

规格完整性核对覆盖七项能力中的八条需求和十六个场景。当前实现、受影响消费者和既有架构合同一致。原生结论限于上述 OpenGL 设备和现有单样本渲染路径。OpenSpec 1.13.2 严格验证通过。 [change 验证](../../../../build/d12-t01/final/openspec-change-validation.json)

<a id="archive-closeout"></a>
## 规格同步与归档

- 七份增量已同步到对应主规格，新增六条需求并更新两条需求。同步后 45 份主规格和 D12-T01 change 全部通过严格校验，归档后 45 份主规格复验通过。 [同步与摘要](../../../../build/d12-t01/final/spec-sync.json) · [同步后校验](../../../../build/d12-t01/final/openspec-sync-validation.json) · [归档后校验](../../../../build/d12-t01/final/openspec-main-validation.json)
- 2026-11-28 归档 `add-ior-specular-material-profile`，OpenSpec 活动列表为空。开发计划已登记 D12-T01 完成，D12-T02 的前置已满足。 [活动列表回执](../../../../build/d12-t01/final/openspec-post-archive-list.json) · [开发计划](../../../../Docs/Developments/Developments.md#current-stage) · [D12-T02](../../../../Docs/Developments/D12-D13-Materials.md#d12-t02)
- 核对归档文件、签收产品输入、主规格、工作包状态及文档链接后，按空模板重置当前问题单。B 落签的问题单按原文留存。 [签收问题单](../../../../build/d12-t01/final/review-signed.md) · [归档核对记录](../../../../build/d12-t01/final/archive-audit.json)
