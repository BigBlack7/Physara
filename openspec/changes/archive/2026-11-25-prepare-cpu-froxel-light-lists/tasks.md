# Tasks

## 1. 有限网格及生成接口

- [x] 1.1 A 在正式 View 准备输入上接通显式的 CPU 网格构造，交付稳定 X/Y 外缘、对数或线性深度和清楚的帧槽借用边界。用 V1 核对无限远透视、零近面正交、双 View 及单路输入更新。 [输入与所有权](design.md#view-inputs)
- [x] 1.2 A 在权威 Shader 声明中生成参数、header 和索引布局，并实现与已发布边界对应的 GLSL 查询。用 V2 核对实际读取这些字段的 canonical 包、生成头和活动反射，以片元探针核对独立边界样点的选格。 [边界设计](design.md#stable-grid) · [载荷 ABI](design.md#bounded-abi)

## 2. 保守分箱与压紧

- [x] 2.1 A 复用完整灯表的分段行号，交付独立方向灯前缀、全局局部灯和有限候选。用 V3 的交错灯顺序确认两组局部索引无重复且能解析到正确的灯行与阴影引用。 [灯行合同](design.md#candidate-rows)
- [x] 2.2 A 实现有限点光、聚光的保守候选及确定顺序的压紧。用 V3 的独立样点覆盖、跨近面和旋转聚光判据检查应保留的灯与明确分离项，核对空灯 header。 [候选规格](specs/froxel-light-preparation/spec.md)
- [x] 2.3 A 对网格、索引、字段及实际字节核算容量。V4 覆盖 8 位计数和 16 位偏移边界，以及紧邻容量失败和恢复，核对每个有效 header 对应的完整区间。 [容量合同](design.md#bounded-abi)

## 3. A 自检与交接

- [x] 3.1 A 构建 `PhysaraRenderer`、受共享 ABI 影响的正式 Shader 包及其消费者，完成 V1–V5，严格校验 change。将实际命令、输入身份、观察值和适用范围记入本文件。 [构建入口](../../../../Docs/Build.md#shader-构建)
- [x] 3.2 A 在当前问题单登记基准、改动及未跟踪正式输入、实际构建产物、代表夹具和结果的对应关系，交维护者调度独立 B。只有 A 的实施与自检此时可以勾选。 [交接](../../../../build/d10-t01/final/review-signed.md) · [审查闭环](../../../../AGENTS.md#review-loop)

## 4. 独立审查与整改复核

- [x] 4.1 B 独立核查相机域、覆盖判定、32 位边界、灯行及正式消费者，按本轮改动核对代表证据。在当前问题单记录发现并完善涉及的完整第一方文件的职责、接口和流程注释。 [首轮审查](#b-first-review) · [签收问题单](../../../../build/d10-t01/final/review-signed.md)
- [x] 4.2 A 按 CR 编号整改并自检受影响场景，记录修正输入和匹配证据，交 B 复核。 [首轮整改](#a-correction-1) · [第二轮整改](#a-correction-2) · [审查闭环](../../../../AGENTS.md#review-loop)
- [x] 4.3 B 核对整改、代表性结果及注释，CR-001 至 CR-005 全部关闭，已按源码清单签收。 [首次复核](#b-recheck-1) · [最终复核](#b-recheck-2)

## 5. 最终验证、规格同步及归档

- [x] 5.1 A 在 B 签收后核对源码、构建和证据身份，用签收版本完成最终 verify，核对 D10-T01 完成条件及 V5 的正常受光帧。记录实跑、可复用的签收结果和覆盖边界。 [最终验证](#final-verification) · [D10-T01](../../../../Docs/Developments/D08-D11-RenderPaths.md#d10-t01)
- [x] 5.2 A 留存 B 签收和实际验收证据，同步 `froxel-light-preparation` 到主规格，并对同步结果及 change 进行严格校验。 [规格同步](#spec-sync)
- [x] 5.3 A 归档 change，复核归档位置、状态和计划链接，登记 D10-T01 完成与 D10-T02 前置满足。在签收证据留存后按模板复位问题单。 [归档结果](#archive-closeout) · [开发计划](../../../../Docs/Developments/Developments.md#current-stage)

<a id="verification-scope"></a>
## 验证口径与停止条件

| 检查 | 改变的风险 | 最小可判定结果 |
|---|---|---|
| V1 · 有限视域与独立工作区 | 无限渲染远面进入分箱、正交带零取对数、不同 View 或旧槽的数据互相覆盖 | 同一冻结场景给无限远透视和零近面正交各准备一份网格。几何覆盖近处和远端。核对透视层深比、正交等间距及有限端点，切换两次合法 jitter 后移动或调整其中一路的尺寸，核对该路的新结果、另一路和仍在使用的旧槽 |
| V2 · 查询与布局 | 留边或抖动造成错格，CPU/Shader 字段和 stride 偏移错读 | 用固定比例且两个栅格尺寸不同的有效矩形，给出可手算的非对称 tile、Z 共边界、远面及 guard band 内外样点。以生成接口编译探针，核对最终活动反射的成员偏移、类型和 stride。给小型 OpenGL 片元探针传入明确的渲染像素与视深，读回所选 cell 和可区分的灯行标记，与 CPU 和独立预期核对 |
| V3 · 覆盖与灯行 | 光体跨 cell 或近面漏项，聚光锥方向错用，场景索引被误作 GPU 行 | 一组有界点光、有界旋转聚光、无 range 局部灯及方向灯交错排列。用独立世界样点与球半径、角度直接判断可能贡献，核对相关查询，并用已知与灯影响体分离的 cell 检查剔除。用可区分灯行核对阴影配对，移除灯后覆盖零候选和空 header |
| V4 · 条目宽度和失败边界 | 压紧偏移或数量窄化、容量达限仍发布部分列表、FP32 深度边界重合 | 使用至少 257 盏候选共存且至少一个偏移大于 65,535 的配置，抽查首尾及跨界行和范围。把同一配置收紧到低于实际需要，另用边界重合的输入核对不可用原因，再恢复合法输入并检查完整结果 |
| V5 · 正式 ABI 消费 | 扩充接口后已有包与当前生成头不配对 | 构建当前生成 ABI 对应的正式包及产品，在正常 OpenGL 入口使用既有受光场景，确认同配置的颜色 Pass 成功接受并核对匹配的代表 HDR 样点。证据标明实际包和提交身份 |

开发夹具、探针和原始结果归入 `build/d10-t01/`。先复用输入身份一致的已有检查，新增样点须能区分特定错误。达到 V1–V5 且证据匹配当前输入后停止扩展。V5 验证已有 Forward 的正常 ABI 消费，实际 Froxel 图内读回和强制回退从 D10-T02 验证，两路径同输入 HDR 及成本在 D10-T05 验收。 [验证约定](../../../../.agents/skills/physara-verification/SKILL.md)

<a id="a-local-verification"></a>
## A 本地检查与交接

基准为 `v2.0@e7b1b14c9481ece8cf0e82075aac77ec718946fb`。交接清单 SHA-256 为 `40756e41e4849107f05cb8e068b54f7d5eba089a9e11884b622782917c4fca89`，记录 8 个已跟踪源码改动、4 个未跟踪正式源码、49 项 Release 构建输入与 29 项探针输入。正式库与探针均重新构建或链接到本次输入。 [输入清单](../../../../build/d10-t01/a/handoff.json) · [受跟踪源码补丁](../../../../build/d10-t01/a/tracked-source.diff)

| 检查 | 实际结果 |
|---|---|
| V1 · View 与投影 | 运行 `Froxel-Release.exe cpu`。无限远透视的近面为 0.125 m、稳定远面为 2,048 m，场景远端几何仍可见。正交边界为 0、3、6、9、12 m。两路同轮共享场景记录，同范围 jitter 变化保持候选。移动导航相机并调整矩形后稳定远面为 2,053 m，旧槽及另一路保留原值。 [CPU 日志](../../../../build/d10-t01/a/cpu.log) |
| V2 · ABI 与查询 | 用正式 `PhysaraShaderBuild build` 编译由生成接口和产品查询组成的开发片元包，`Froxel-Release.exe native build/d10-t01/a/FroxelProbe.pshader` 在 Intel RaptorLake-S Mobile、OpenGL 4.6.0 驱动 32.0.101.6129 上提交并读回。9 个样点的 cell、header 偏移和灯行标记与独立预期一致，覆盖远面、tile/层共边界与覆盖外。活动反射的 header stride 为 12 B，三字段偏移为 0、4、8 B，索引 stride 为 4 B，深度边界 stride 为 16 B。11 个正式 StaticForward 系列包与当前 ABI 匹配。 [片元日志](../../../../build/d10-t01/a/native-v2.log) · [反射记录](../../../../build/d10-t01/a/reflection-v2.json) |
| V3 · 聚光及灯行 | 用同一 CPU 探针的独立世界样点核对跨近面点光、旋转聚光和有限远灯的覆盖。位于聚光球界内且在锥尖后方的代表 cell 被剔除。交错序列得到 1 个方向灯和 2 个全局局部灯，索引同时核对正式灯表编码和阴影引用顺序。 [覆盖结果](../../../../build/d10-t01/a/cpu.log) |
| V4 · 压紧与容量 | 320 个 cell 各 257 个候选，共 82,240 个索引，末偏移为 81,983。精确预算接受，差 1 条目拒绝。超限网格乘积与重合的 FP32 深度边界各自返回原因，恢复后交付完整列表，空灯结果仍可查询。 [容量结果](../../../../build/d10-t01/a/cpu.log) |
| V5 · 正常 Forward | 用本版 Release 库重新链接经验证的 D09 观测副本，运行 `V5-Release.exe quality`。冻结的 640×480 Physical Lighting、IBL 与 PCF 512 配置正常接受票据 962。HDR、主深度和 11 层阴影 D32 共 13 份读回与同输入的 D09 参考逐字节一致。 [原生回放](../../../../build/d10-t01/a/v5/handoff.log) · [读回对比](../../../../build/d10-t01/a/v5/readback.json) |

MSVC 19.44.35228、SDK 10.0.26100.0 下构建 Release 的 `Physara` 和 Debug 的 `PhysaraRenderer` 均通过，Release 正式程序运行 `--hidden --poll-limit=15` 返回 0。默认 MSBuild SDK 枚举遇到本机目录访问限制，通过为构建传入已安装的 SDK 路径和版本完成复跑。OpenSpec 1.13.2 的 `validate --all --strict --json` 通过本 change 和 42 份主规格。原生证据限于上述 Release 设备和 D10-T01，Debug 证据为渲染器构建。 [Release 构建](../../../../build/d10-t01/a/build-release-handoff.log) · [Debug 构建](../../../../build/d10-t01/a/build-debug-renderer.log) · [OpenSpec 校验](../../../../build/d10-t01/a/validate-all.json)

<a id="b-first-review"></a>
## B 首轮代码审查与注释

B 核对了上述基准及 12 个正式输入的实际字节，重跑 Release CPU 和 OpenGL 查询。注释后的 GLSL 常规样点 9/9 通过，另在 GPU 读回 count 257、offset 81,983 及段首尾灯行 1、257。V5 复用 A 的已提交帧，B 逐字节比对了 13 对留存图像。11 份文件只调整注释，权威接口 JSON 已核对。 [源码身份](../../../../build/d10-t01/b-review/review-inputs.json) · [宽索引读回](../../../../build/d10-t01/b-review/wide.log)

CR-001 复现未选中的级联诊断启用 Froxel 绑定后仍可发布，重新请求该角色时链接失败。CR-002 复现背向视域的 90° 聚光占用有限索引并触发容量拒绝。CR-003 复现正交零近面下负次正规视深在 CPU 返回无效、OpenGL 选中 cell 0。三项待 A 整改，4.3 最终签收待复核。 [问题单与复核条件](../../../../build/d10-t01/final/review-signed.md) · [CPU 与热重载记录](../../../../build/d10-t01/b-review/observed.log) · [越界样点读回](../../../../build/d10-t01/b-review/query.log)

<a id="a-correction-1"></a>
## A 首轮整改与自检

保留 B 首轮注释并以 `v2.0@e7b1b14c9481ece8cf0e82075aac77ec718946fb` 为基准。按首轮 B 源码清单核对整改前 12 个文件全部相符。整改包含候选准入、聚光支持点和两端深度查询，当前输入共 9 个已跟踪、4 个未跟踪正式源码。交接清单 SHA-256 为 `66d221c379196df6f2ab11a465faf7e74f5ca81fe0f76e4280305d0c1c9ff62e`。 [B 输入](../../../../build/d10-t01/b-review/review-inputs.json) · [A 整改输入及补丁身份](../../../../build/d10-t01/a-correction-1/handoff.json)

| 项目 | 当前结果 |
|---|---|
| CR-001 · 未请求角色的活动绑定 | 运行 `Correction-Release.exe reload`，以 B 原级联包和去掉额外绑定后的新编辑作对照。当前只请求普通颜色时，坏编辑在准入中被定位到 `CascadePreview.frag` 的 `froxelHeaders` 并拒绝，活动代保持 2。原级联管线仍可请求。随后合法编辑通过原生准备，活动代 3 提供新的可用级联管线。 [热重载日志](../../../../build/d10-t01/a-correction-1/reload.log) |
| CR-002 · 半球聚光的保守方向界 | 运行 `Correction-Release.exe cpu`。背向原例在接近直角及 90° 时均按零预算交付 4 个空 header。灯转向相机后仅最后一层包含该灯。锥面与层共边界相切时四层计数为 `0,1,1,1`。另核对正背向的宽锥缘支点单位长度及支持距离。 [CPU 结果](../../../../build/d10-t01/a-correction-1/correction-cpu.log) |
| CR-003 · 零近面下的负次正规视深 | CPU 验证负次正规位型 `0x80000002` 无格，正负零有效。以同一 B 片元输入和当前 GLSL 编包，`Froxel-Release.exe native` 读回同位型为无效哨兵，合法零和首个内部层边界分别命中 cell 0、10，9 个实际样点均通过。 [CPU 结果](../../../../build/d10-t01/a-correction-1/correction-cpu.log) · [OpenGL 读回](../../../../build/d10-t01/a-correction-1/query.log) |
| 相关回归 | `Froxel-Release.exe cpu` 复跑 V1、V3、V4。以当前 Release 库重新链接并运行 `V5-Release.exe quality`，原冻结 PCF 512 配置正常接受票据 949，HDR、主深度和 11 层阴影的 13 份图像数据与 A 前次参考逐字节一致。 [CPU 回归](../../../../build/d10-t01/a-correction-1/baseline-cpu.log) · [正常帧](../../../../build/d10-t01/a-correction-1/v5/normal-forward.log) · [读回对比](../../../../build/d10-t01/a-correction-1/v5/readback.json) |

Release `Physara` 和 Debug `PhysaraRenderer` 构建通过。原生覆盖为本机 Intel RaptorLake-S Mobile、OpenGL 4.6.0、驱动 32.0.101.6129 的 Release，Debug 覆盖渲染器构建。本轮证据供 B 复核及签收。 [Release 构建](../../../../build/d10-t01/a-correction-1/build-release.log) · [Debug 构建](../../../../build/d10-t01/a-correction-1/build-debug.log) · [签收问题单](../../../../build/d10-t01/final/review-signed.md)

<a id="b-recheck-1"></a>
## B 首次整改复核

B 核对 A 清单中的 13 份正式源码及其库、程序包与探针。相对首轮 B 交接，整改涉及的 4 份源码与登记一致。以匹配的 Release 库重新链接定向探针，从当前产品 GLSL 重建深度查询包。B 补充了候选角色准入及共享支点消费者的注释，复核后清单 SHA-256 为 `12a352d0816c41b7c44af977ea35121e999bd1a6056038e2d42edca0960b113e`。 [源码与实跑身份](../../../../build/d10-t01/b-recheck-1/review-inputs.json)

CR-001 至 CR-003 已关闭。坏包在发布前被拒绝，活动代保持 2，原级联管线可用，修正包发布代 3。背向 90° 聚光用零预算得到 4 个空格，转向与相切对照均通过。负次正规视深在 CPU 与 OpenGL 均无有效格，9 个片元样点通过。V1、V3、V4 的 CPU 基线复跑通过。V5 复用 A 的 Release 票据 949，B 核对 13 对留存读回逐字节相同。Debug 证据为 A 的构建记录。 [热重载](../../../../build/d10-t01/b-recheck-1/reload.log) · [CPU](../../../../build/d10-t01/b-recheck-1/correction-cpu.log) · [深度读回](../../../../build/d10-t01/b-recheck-1/depth.log)

新增 CR-004、CR-005，4.3 保持待签收。零留边的 Y 上缘外样点在 CPU 返回空，Intel OpenGL 却读到 cell 0。斜轴反平行查询可将沿轴舍入残差归一化为锥缘方向，使返回的支点落在影响体外。复核条件与对照样点已交 A。 [签收问题单](../../../../build/d10-t01/final/review-signed.md) · [XY 读回](../../../../build/d10-t01/b-recheck-1/xy.log) · [支点实测](../../../../build/d10-t01/b-recheck-1/support.log)

<a id="a-correction-2"></a>
## A 第二轮整改与自检

沿用基准 `v2.0@e7b1b14c9481ece8cf0e82075aac77ec718946fb`，整改前核对 B 最新清单中的 13 份正式输入。相对 B 版本仅改 `Froxel.cpp`、`Froxel.glsl` 与 `LightVolume.hpp`。交接清单 SHA-256 为 `2f7775d51433fa8854b87ed6ddb263e6164abcbc627ee40268abdc8543409f6e`。 [B 复核输入](../../../../build/d10-t01/b-recheck-1/review-inputs.json) · [当前输入与结果身份](../../../../build/d10-t01/a-correction-2/handoff.json)

| 项目 | 实际检查与结果 |
|---|---|
| CR-004 · 零外缘与抖动 | 用 `PhysaraShaderBuild build` 按 B 的原片元输入及当前查询函数构建包，以本版 Release 库重新链接 `XY-Release.exe`。CPU 与 GPU 对 Y 为 `0x80000002`、0、−0.5 的原样点均得到 `-1,0,-1`。`XYJitter-Release.exe` 另使用正次正规 Y 抖动，CPU 与 GPU 均拒绝外侧的零画布输入、接受等于抖动量的边界点。无效读回均未交付 header 偏移或灯行。 [原样点](../../../../build/d10-t01/a-correction-2/xy.log) · [次正规抖动](../../../../build/d10-t01/a-correction-2/xy-jitter.log) |
| CR-005 · 斜轴支持点 | 用当前共享函数构建并运行 `Support-Release.exe`，B 的斜轴宽锥反平行对照得到单位锥缘点，实际投影约为 `3.8146973e-6`，与报告距离一致。阴影有效角域的反平行对照返回锥尖和零距离。`SupportNear-Release.exe` 的两侧近反平行对照满足支点约束、投影和解析距离。原半球朝向反转及相切分箱复跑通过。 [B 对照](../../../../build/d10-t01/a-correction-2/Support.log) · [近反平行](../../../../build/d10-t01/a-correction-2/SupportNear.log) · [分箱](../../../../build/d10-t01/a-correction-2/correction-cpu.log) |
| 相关回归 | 当前 Release CPU 复跑 V1、V3、V4。OpenGL 的 9 个查询样点覆盖非零抖动、留边、内部边界、远面和 CR-003 的负次正规视深，全部符合预期。用当前库重新链接正常 Forward 回放，在冻结 PCF 512 配置接受票据 889，HDR、主深度和 11 层阴影读回共 13 份，与上一轮参考逐字节一致。 [CPU 回归](../../../../build/d10-t01/a-correction-2/baseline-cpu.log) · [查询回归](../../../../build/d10-t01/a-correction-2/query.log) · [正常帧与图像](../../../../build/d10-t01/a-correction-2/v5/readback.json) |

Release `Physara` 与 Debug `PhysaraRenderer` 构建通过。原生证据限于本机 Intel RaptorLake-S Mobile、OpenGL 4.6.0、驱动 32.0.101.6129 的 Release，Debug 证据为渲染器构建。CR-001 的准入源码按 B 清单字节核对，沿用其独立复核。当前仍需 B 对 CR-004、CR-005 与整体版本复核签收。 [Release 构建](../../../../build/d10-t01/a-correction-2/build-release.log) · [Debug 构建](../../../../build/d10-t01/a-correction-2/build-debug.log) · [B 热重载复核](../../../../build/d10-t01/b-recheck-1/reload.log)

<a id="b-recheck-2"></a>
## B 第二次复核与版本签收

B 签收 `v2.0@e7b1b14c9481ece8cf0e82075aac77ec718946fb` 工作区中清单所列的 9 份已跟踪、4 份未跟踪正式源码。签收清单 SHA-256 为 `1de8a447d61597dad0915d4583cf22d40891a7a116ad143b04d8efcf5266944b`。B 核对了 A 第二轮清单、库与探针身份，在 `Froxel.cpp` 与 `LightVolume.hpp` 补充零外缘比较、锥尖及支持距离的注释，并确认两份文件相对 A 仅有独立注释行变更。CR-001 至 CR-005 均已关闭，代码与注释复核通过。 [签收输入与证据身份](../../../../build/d10-t01/b-recheck-2/review-inputs.json) · [签收问题单](../../../../build/d10-t01/final/review-signed.md)

| 复核项 | 签收版本的结果 |
|---|---|
| CR-004 · 画布外缘 | 用当前 GLSL 编包，以签收源码重新编译并链接 CPU 查询。原零留边样点及正次正规抖动对照在 CPU 和 OpenGL 均返回 `-1,0,-1`，越界样点读回无效偏移和灯行。另有 9 个非零抖动、留边、内部层边界、有限远面及负视深样点全部通过。 [零留边](../../../../build/d10-t01/b-recheck-2/XY.log) · [抖动对照](../../../../build/d10-t01/b-recheck-2/XYJitter.log) · [常规查询](../../../../build/d10-t01/b-recheck-2/Query.log) |
| CR-005 · 斜轴支持点 | 从签收头文件重编 B 的原反例。宽锥斜轴得到单位锥缘点，实际投影与报告距离均约为 0.0000038146973。合法窄于半球的反平行输入取锥尖，投影为零。另 4 个近反平行对照满足锥缘、投影及解析距离约束。背向、转向与相切分箱回归通过。 [原反例](../../../../build/d10-t01/b-recheck-2/Support.log) · [近反平行对照](../../../../build/d10-t01/b-recheck-2/SupportNear.log) · [分箱](../../../../build/d10-t01/b-recheck-2/correction-cpu.log) |
| 相关回归及复用 | B 用签收版 Froxel、阴影几何对象和匹配的 Release 库重新链接。V1、V3、V4 的 CPU 基线通过。GPU 读到末格 offset 81,983、count 257、首尾灯行 1 和 257。坏级联编辑仍保留旧管线，合法编辑发布下一代。V5 复用 A 的 PCF 512 提交 889，B 逐字节比较留存的 13 对读回，全部一致。 [CPU 基线](../../../../build/d10-t01/b-recheck-2/baseline-cpu.log) · [GPU 宽索引](../../../../build/d10-t01/b-recheck-2/Wide.log) · [热重载](../../../../build/d10-t01/b-recheck-2/reload.log) · [正常帧](../../../../build/d10-t01/a-correction-2/v5/readback.json) |

本次原生实跑限于 Intel RaptorLake-S Mobile、OpenGL 4.6.0、驱动 32.0.101.6129 的 Release。Debug 复用 A 的渲染器构建记录。A 的 5.1–5.3 待执行。

<a id="final-verification"></a>
## A 最终验证

当前源码的 9 个已跟踪、4 个未跟踪文件逐项匹配 B 签收清单，清单 SHA-256 为 `1de8a447d61597dad0915d4583cf22d40891a7a116ad143b04d8efcf5266944b`。A 核对了清单中登记的探针、对象、程序包及结果，把签收问题单原样留存。原件内相对链接按 `Docs/CodeReview.md` 的位置解释。 [B 签收清单](../../../../build/d10-t01/b-recheck-2/review-inputs.json) · [签收问题单](../../../../build/d10-t01/final/review-signed.md)

| 验证项 | 签收源码上的结果 |
|---|---|
| 任务和交付 | A/B 审查闭环的五个 CR 均已关闭。核对 4 项增量要求与 7 个场景。最终验证完成时为 11/13 项，剩余规格同步及归档两个流程项。 |
| 有限网格与 CPU/Shader 查询 | `ViewWork` 交付同轮有限裁剪范围及稳定参数，CPU 与 GLSL 使用同一套边界。签收版 Release CPU 复跑 V1，透视稳定远面为 2,048 m，正交层边界为 0、3、6、9、12 m。用签收 GLSL 包原生读回 9 个样点，格号及索引符合预期。复用 B 已核对身份的零外缘与次正规抖动对照。 [CPU](../../../../build/d10-t01/final/cpu.log) · [OpenGL 查询](../../../../build/d10-t01/final/query.log) · [B 边界证据](../../../../build/d10-t01/b-recheck-2/review-inputs.json) |
| 候选与有界载荷 | V3 核对灯行、阴影行号及有限光体覆盖。V4 的末格偏移为 81,983，单格 257 条，精确预算接受、少一条则拒绝。B 的斜轴聚光及 GPU 宽索引回归按签收清单复用。生成接口与 11 个 StaticForward 系列 Release 包的 ABI 相符。 [CPU](../../../../build/d10-t01/final/cpu.log) · [支点复核](../../../../build/d10-t01/b-recheck-2/SupportNear.log) · [GPU 宽索引](../../../../build/d10-t01/b-recheck-2/Wide.log) |
| 正常 Forward 与设计一致性 | 重建 Release `Physara` 和 Debug `PhysaraRenderer`，Release 正式启动运行 `--hidden --poll-limit=15` 返回 0。签收源码的 V5 正常受光帧接受票据 908。冻结的 640×480 PCF 512 配置下，HDR、主深度和 11 层阴影共 13 份读回与 B 已核对的票据 889 逐字节相同。帧槽所有权、灯表行映射及现有 Forward 绑定与设计一致。 [Release](../../../../build/d10-t01/final/build-release.log) · [Debug](../../../../build/d10-t01/final/build-debug.log) · [正常帧](../../../../build/d10-t01/final/v5/normal-forward.log) · [逐字节对照](../../../../build/d10-t01/final/v5/readback.json) |

OpenSpec 1.13.2 归档前严格校验通过。最终原生实跑限于本机 Intel RaptorLake-S Mobile OpenGL 4.6.0、驱动 32.0.101.6129 的 Release，Debug 覆盖渲染器构建。最终证据清单 SHA-256 为 `7af16ffbc6b6db1b49e460c91176f8f495b0226fb205072e523bad21543513c1`。 [最终验证清单](../../../../build/d10-t01/final/native-verify.json) · [结构校验](../../../../build/d10-t01/final/validate-before-sync.json)

<a id="spec-sync"></a>
## 规格同步

新增 `froxel-light-preparation` 主规格，原样同步增量的 Purpose、4 项要求与 7 个场景。原有 42 份主规格与同步前逐字节相同。OpenSpec 1.13.2 对 43 份主规格及本 change 的严格校验均通过。 [主规格](../../../specs/froxel-light-preparation/spec.md) · [主规格校验](../../../../build/d10-t01/final/validate-synced-specs.json) · [change 校验](../../../../build/d10-t01/final/validate-synced-change.json)

<a id="archive-closeout"></a>
## 归档与阶段推进

OpenSpec 1.13.2 按 `spec-driven` 将本 change 归档。移动前唯一未勾选项为归档本身，移动与验收链接复位完成后勾选 5.3，最终 13/13。归档后 43 份主规格严格校验通过，活动 change 列表为空。D10-T01 已验收归档，D10-T02 前置已满足并成为当前工作包。B 签收问题单按原字节留存，实时问题单已按模板复位。 [归档回执](../../../../build/d10-t01/final/archive-result.json) · [归档后规格](../../../../build/d10-t01/final/validate-after-archive.json) · [阶段入口](../../../../Docs/Developments/D08-D11-RenderPaths.md#d10-t02) · [签收问题单](../../../../build/d10-t01/final/review-signed.md)
