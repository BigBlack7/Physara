# Tasks

## 1. 接入正式 Shader ABI 与准备入口

- [x] 1.1 在现有 `PhysaraShaderBuild` 链路连接 Forward 工作配置的 Shader 接口和程序包，按配置生成 ABI、反射、依赖与 Runtime 资源位置。用 V1 核对实际加载包、反射、原生 link、附件与绑定能力，保持基础 UI 启动合同。
- [x] 1.2 在 Renderer 建立设备级资源 owner 和可复用帧槽准备值，声明能力、驻留和附件预算以及失败结果。用 V2 核对资源与帧槽生命期，确认候选和已接受代由各自 owner 交付。

## 2. 准备静态几何、纹理、材质及上传

- [x] 2.1 用 SceneFrame 固定版本准备几何和有效材质行，解析原始或实例覆盖的图像、sampler、UV 及绑定。用 V2 验证同源复用、局部覆盖、相机变化和真实版本替换。
- [x] 2.2 按实际用途生成有效纹理层级和可访问范围。用 V2 核对同像素颜色/数据绑定、显式 mip 和基础层的计划，再用 V3 的缩小样本验证原生选择和过滤值。
- [x] 2.3 实现拥有目标及源范围的 UploadBatch，封存后用现有图上传链声明写入与后继读取。合并实际存活的场景和 UI 写入进行准入。用 V2 核对声明范围和准入顺序，V5 核对未消费/失败候选的确认与真实完成。

## 3. 实现共用静态几何、取样与覆盖

- [x] 3.1 实现共同位置和表面框架，保留 CCW、源切线及实例 handedness，覆盖改变 normal 所用 UV 时准备匹配框架。用 V3 核对不对称前后侧、非均匀负缩放及改变 normal UV 的输出，无法形成框架时核对字段原因。
- [x] 3.2 以共同材质取样求值 UV0/UV1、纹理变换、sRGB/线性通道、ORM、normalScale、emissiveStrength 和 Unlit。用 V3 的分通道读回对照独立预期，核对缺席槽与 Unlit 色。
- [x] 3.3 让 Depth 与 Color 共享顶点位置、OPAQUE/MASK 覆盖和双面规则。用 V3 叠放近远几何，分别核对实际深度与色，含阈值相等及低于阈值的像素。

## 4. 组装 Forward 和真实视口

- [x] 4.1 构建 `DrawRecord` 与不透明/MASK/Unlit 路由，定位超出 profile 的 Primitive。用 V2 核对程序、绑定和索引范围都在准备阶段解析，用 V3 验证混有 `BLEND` 时已支持部分仍有正确覆盖及可见原因。
- [x] 4.2 通过 Forward `PrepareSurface` 与 `ShadeOpaque` 组装真实 Reverse-Z 与线性工作颜色，按有效图像矩形绘制。用 V3 比较直接输出和显式前置深度两条组合，同时核对留边和剔除后的空画面。
- [x] 4.3 形成 display-linear 语义的 `ViewOutput`，现有视口加会话内模式入口与当前显示状态。用 V3 核对工作色和显示色的数值边界，用 V4 核对 UI 实际消费、逻辑 token、模式与输入矩形。
- [x] 4.4 把场景分支接入 Runtime 的正常图、等待与独立安全 UI，关联实际图、帧槽和宿主回执。用 V4 核对正式产品消费，用 V5 验证全裁剪、容量等待、前置失败、已发前缀及恢复发布边界。

## 5. A 本地验证与交接

- [x] 5.1 A 核对 D06-T05 的三个工作目标和正式消费者，通过受影响构建、V1–V5、OpenSpec 严格校验及文档链接检查。将实际命令、输入、源码、产物身份、结果及未覆盖范围记录在本 tasks，按 [当前问题单](../../../../Docs/CodeReview.md) 移交实际 diff、未跟踪文件和所用 fixture，由维护者调度 B。

## 6. 独立审查、整改与复核

- [x] 6.1 B 按 [独立审查闭环](../../../../AGENTS.md#review-loop) 核对实际版本、GPU 资源及票据、Shader ABI/材质、图依赖和错误恢复及其直接消费者。通读涉改完整第一方文件并补齐职责、接口合同及流程注释，在当前问题单登记实际覆盖、稳定 CR 编号与结论。
- [x] 6.2 A 按稳定 CR 编号纠正并自检，用对应回归证据核对实际修订，记录修正及输入身份。没有待整改项时据实登记不适用后由 A 勾选。
- [x] 6.3 B 对最终实际输入复核功能行为、回归证据及涉改注释，分别确认代码正确性和注释可读性，记录验收版本并勾选 B 项。

## 7. 最终验证、同步与归档

- [x] 7.1 A 对 B 签收版本运行或复用匹配的 V1–V5 构建、CPU、原生 GL 与会话证据，完成最终 verify。核对源码、生成包、可执行产物、fixture 及实际设备，记录有效证据和限制。
- [x] 7.2 A 留存审查、整改、复核及最终验收证据，同步三个新增及三个修改能力的 delta，核对既有场景完整保留并运行严格校验。
- [x] 7.3 A 通过 OpenSpec 归档并核对归档链接及开发计划完成状态，保留最终证据后按模板恢复 [当前问题单](../../../../Docs/CodeReview.md)。

## 验证范围与停止条件

优先复用仍匹配的正式构建、GL 探针和场景会话验证方式。用一份小型非对称 glTF/PNG/v1 场景及必要的输入变体承载下面的判定。开发探针、脚本、样例、故障注入与临时工程放在 `build/d06-t05/`。探针可借用正式帧入口和同一 Forward 组装函数，将实际颜色/深度作为受声明的验证输出，在有效 GL context 上按真实完成票据读回。原生读回只用于开发探针。 [构建说明](../../../../Docs/Build.md) · [验证规则](../../../../.agents/skills/physara-verification/SKILL.md) · [D06-T04 验收证据](../2026-11-16-prepare-static-scene-views/tasks.md#final-verification)

| 验证组 | 最小有效检查与独立预期 | 停止条件 |
|---|---|---|
| V1 · 生成 ABI 与正式构建 | 通过正式 CMake 构建 Debug/Release 的 `Physara`，核对配置对应的 `.pshader`、生成布局、活动 binding/stage IO、GL lowering 和原生链接。换工作目录启动。使用受影响产品与正式库执行 V2–V5 所需检查，其它既有检查按匹配证据复用。 | 实际消费者用配置对应的包，反射匹配，正式目录可独立于开发 fixture 构建，原生 Pipeline 与真实附件配置成功。 |
| V2 · 准备复用与图账单 | 两个同源实例和一张有颜色、数据两个用途的图像。依次仅改相机、改一个材质因子/外置图像、发布同一逻辑模型新版本。比较固定 key、图声明的字节范围/层级、各次实际写入及准备的 Draw。取一项资源需求超出已知硬边界，检查 Release 给出原因并保持旧有效状态。 | 无关静态记录复用、受影响行单独更新、Draw 引用对应版本。纹理/缓冲准入与编译存活写入一致，硬超限可定位。 |
| V3 · 原生深度、覆盖与材质 | 在较宽画布用固定 aspect 的正交相机投影非对称近远面。近远深度选用固定可计算的值，例如 near=0、far=8 时视空间 z 为 -2/-6，应读到 0.75/0.25，留边深度为 0。比较同一像素的直接路径与前置深度路径。MASK 像素含乘积等于和小于 cutoff 的样本。带镜像/双面法线样本和独立的 UV1 与旋转/偏移判定，复用同一表面查看颜色/数据通道、不同 sampler 及 Unlit。sRGB 码值 128 解码约 0.216，线性值约 0.502；黑白等量缩小在线性域期望 0.5。Emissive 取一个可表示且大于 1 的线性值，同时读工作附件和 View 显示色。 | 原生读回区分反向深度、绕序、UV/mip、通道和 gamma 错误，两个深度组合得到一致覆盖。以固定像素、误差容限和实际布局记录数值，并确认不支持路由的原因。 |
| V4 · 正式会话和视口到窗口 | 链接正式 Runtime 与模块产物，通过 `EditorSession::RequestSceneFile` 打开同一场景。空文档先显示测试图；在场景接受、切换预览模式及改变固定 aspect 后核对同一 View token、当前颜色、有效矩形和输入映射。检查实际 UI 采样/窗口接受及呈现，用 `Physara --hidden --poll-limit=6` 核对空文档正式启动和有序退出。 | 同一路径的接受帧出现符合预期的非对称几何与 UI，正常提交、窗口交换和退出结果各自可观察，留边及无有效图像时映射正确。 |
| V5 · 拒绝、裁剪、在途与恢复 | 使用同一场景做一次全裁剪、一次场景上传容量等待但基础 UI 可用、一次场景程序无法准备和一次确有前缀命令的失败。再以有效模型/材质换代，保留旧提交所有权直到真实完成。检查正常、占位/旧输出和恢复帧的回执、具体错误、输入映射、上传计数和保活。注入仅用于 build 内的受影响图或资源边界。 | 未执行或前缀工作未被确认，安全 UI 不接受场景候选，实际已发引用在覆盖其访问的票据后可退休，恢复帧与自己的颜色、相机及上传匹配。 |

同一探针可覆盖多个场景。使用匹配签收输入及产物的证据后停止。失败、输入变化或明确缺口才追加验证。优先核对受影响的消费者；D06-T07 的全阶段集成矩阵、D07 的 PBR 数值基准和 D08 的批次/容量测量由各自工作包承担。

<a id="a-local-verification"></a>
## A 本地核对

2026-11-16，在 Windows x64、MSVC 和 SDK `10.0.26100.0` 下通过正式 `Physara` 目标构建 Debug、Release。原生探针使用 Intel RaptorLake-S Mobile Graphics Controller、OpenGL 4.6.0、驱动 `32.0.101.6129`。两种配置都链接实际静态库和 `Runtime/Frame.obj`，从其他工作目录运行 `Physara --hidden --poll-limit=6`，并在同一正式消费链读回场景和窗口颜色。

| 验证组 | 实际结果 |
|---|---|
| V1 | 两个配置分别生成四个 `.pshader`。首个场景以生成的 ABI、真实附件和预制绑定通过 GL lowering 与原生链接。缺少场景 Shader 包时视口报告原因，已有基础 UI 仍获成功回执。 |
| V2 | 首次接受对应 9 次纹理写入、23 次缓冲写入，原版本和布局复用时无需静态重传。实例覆盖生成一份外置图像、局部材质行和一套替换 UV 几何，原实例值不受影响。改预览或纯相机只写 80 字节的 View 行。实际源替换发布同一资产的新 revision，旧/新输出保有各自像素。`4000×4000` 在分配附件前报告预算拒绝。 |
| V3 | 原生像素区分 UV0/UV1、纹理变换、同图像的夹取/重复采样、sRGB/线性 mip 与 ORM 通道。镜像、单面和双面读回符合绕序与法线预期。可用的 UV 覆盖得到新切线，退化 UV 明确报告 `normalTexture.texCoord`。MASK 在阈值下露出远面，阈值处保留近面。两种深度组合均读到近/远 0.75/0.25 与留边深度 0，自发光在工作附件大于 1，显示色按本阶段映射限幅。 |
| V4 | 正式会话通过 Save As/Open 组合原 glTF、场景文档和正交相机。产品帧内同一 token 从测试图换到 Forward 颜色，窗口目标读回与视口样本相符。切换模式及对同一画布更换相机矩形均只由实际接受回执发布，图像坐标受有效矩形约束。 |
| V5 | 全裁剪不发场景工作。实际封闭列表占用 64 MiB 上传池，主视口显示等待并保留已确认代，基础 UI 继续，真实提交和完成后重试通过。通过正式图执行器得到实际前缀票据，失败候选未获上传确认，下次完整图重写。以原生安全 UI 合成只读旧 Forward 颜色，票据 `20`，交换成功，旧色可读而本轮输入映射关闭。 |

修正了两处由原生测试暴露的边界。新建材质纹理按首次真实 `Common` 状态导入并沿同一边界交还，已上传事实仍单独确认。GL typed clear 显式绑定同一辅助 FBO 以覆盖深度绘制后的整个颜色附件，合并路径和前置深度路径的留边均回归通过。 [图声明](../../../../Engine/Renderer/Paths/Forward.cpp) · [原生清除](../../../../RHI/OpenGL/CommandListTransfer.cpp)

本地重放入口为 `python build/d06-t05/a-local/VerifyLocal.py Debug` 或 `Release`。最后一轮在核对完整正式源码清单匹配后复用相同构建产物，重新编译并运行两个配置的探针与产品短运行。构建、生成包、实际 probe/exe、fixture、命令和日志摘要保存在同目录 `debug/evidence.json`、`release/evidence.json`。`handoff.json` 记录 1578 个正式构建输入的清单入口，以及 30 个修改、22 个新增的待审正式文件和 `tracked-a.diff`。其 SHA-256 为 `7399f4dfcf16740d5decbcfec91692a47b1440af9ad895b5429324fbfa3d9311`。 [正式包入口](../../../../Docs/Build.md#shader-构建) · [当前问题单](../../../../Docs/CodeReview.md)

证据范围限于上述 OpenGL 环境。执行失败前缀由 build 内探针追加单个失败 Pass 验证，安全 UI 通过独立的正式 UI 合成与宿主呈现入口验证。版本换代覆盖旧提交尚未消费完成证明的区间，未强制构造三槽持续饱和。模式切换通过 View 的会话请求入口验证。上述结果为 A 本地证据，B 验收后另记录最终核对。

<a id="independent-review"></a>
## 审查、整改与复核

<a id="a-correction-verification"></a>
### 第 1 轮 A 整改与回归

B 初审提出 CR-001、CR-002 并补齐注释。A 在实际工作树核对 52 个正式文件的 B 注释后摘要 `dacaefe9d694fef8f3c803d5598f9164b5ebbac8483a3d70ce6a58920f6aed15`，以该版本为整改输入。A 只在相关 View、帧编排及编辑器消费者中改动 11 个正式文件。按问题单原始字节算法计算的整改后摘要为 `57e7ce737b9ee32765442f4f098d8a4506e008461d5adc123c1afb9dab3ccdf8`。 [问题单](../../../../Docs/CodeReview.md)

| 编号 | 改动与独立预期 | Debug 与 Release 实际结果 |
|---|---|---|
| CR-001 | Runtime 把永久超限和暂时背压分别交给 View，普通 UI 占位读取同一原因。`4096×4095` 图像的解码为 67,092,480 字节，实际场景独立账单 67,092,800 字节可准入。加入真 UI 几何后由设备判定硬超限。 | UI-only 帧获接受，原有效比例、颜色对象及输出代保留，当前输入映射不可用。视口原生读回显示 `Forward upload limit exceeded`。下一轮 Inspector 布局显示 `Failed - Forward upload limit`。V5 真实暂时背压仍显示等待，按提交和完成重试成功。 |
| CR-002 | 视口呈现短标识，`Frame status` 从最近接受的显示色中同步借用完整摘要及实际模式并换行展示。混合模型包括当前可绘的表面、退化 UV1 法线覆盖及 `BLEND` 两项跳过原因。 | 144 px 视口保持世界法线输出及有效矩形内的 UV，留边无输入。原生窗口合成和菜单布局日志均确认含 `normalTexture.texCoord` 和末尾 `alphaMode BLEND is unsupported` 的 183 字节完整摘要可读。 |

用 `python build/d06-t05/a-correction/VerifyCorrection.py Debug` 和 `Release` 重放。两个配置在 MSVC、SDK `10.0.26100.0` 下构建正式目标，链接对应的正式 `Runtime/Frame.obj`、库与 Shader 包，在 Intel OpenGL 4.6 (`32.0.101.6129`) 上运行上述定向回归和产品短运行。本次 Debug 探针在检查源码与此前同轮成功构建输入全量匹配后用 `--skip-build` 重放。`build/d06-t05/a-correction/handoff.json` 记录两配置命令、日志、fixture、实际 exe 与 Shader 包摘要及 1578 个构建输入。该交接清单 SHA-256 为 `967601d8180a13e9f794b982ceabcb6f0e01bcbdf67df1c663811d495b3ffc21`。同目录的 `debug/` 和 `release/` 有 `evidence.json`，包含实际渲染窗口读回与布局日志，Release 图像另生成 PNG 供查看。

本轮证据覆盖两条整改路径、已有的暂时背压用例和产品短运行。菜单通过 ImGui 的菜单项身份打开，布局、绘制、接受和像素读回走正式消费路径。A 自检已登记，B 复核结论见下节。

### B 独立复核与签收

2026-11-16，B 关闭 CR-001 与 CR-002，代码正确性和注释可读性均通过，无新增正式问题。初审覆盖 52 个正式文件及直接消费者，32 个文件补充注释。本轮复核 A 的 11 个整改文件，补正 `Editor/ImGui/UiRenderer.cpp` 的等待原因说明、`Engine/Renderer/View.hpp` 的显示色与历史记录说明及 `Runtime/Frame.cpp` 的普通图重建终止条件。B 说明本轮以源码和注释核对为依据，引用 A 的本地回归记录。原生验证证据见本文件的最终验收。

| 问题 | B 复核结论 |
|---|---|
| CR-001 | 实际准入决定等待、场景硬上限或测试图预算原因。普通图重建沿 View 的同一原因显示占位，新候选不发布，已接受比例、颜色代和安全 UI 合同有效。 |
| CR-002 | 视口显示降级标识，完整原因经 `Frame status` 从最近接受的场景预览读取。同步借用及失败时的摘要发布规则核对通过。 |

签收由 B 在问题单登记。52 个签收文件按问题单原始字节算法得到 `ebb3c16a16ecff2274c678379828aa4ea7679318af750860feab303b1ae6de28`。原签收问题单副本留在 `build/d06-t05/final/review-signed.md`，SHA-256 为 `b6d2b386b1b482095acf7645fccbbf2de2035e657aea35f2250d0409a24c5c10`。A 按签收记录完成后续验证。 [问题单](../../../../Docs/CodeReview.md)

2026-11-16，B 在维护者提醒后补齐本人负责的 6.1、6.3 勾选。此前已完成初审与最终复核，但只在问题单签收，遗漏了本任务表的完成登记。本次再次逐文件核对上述 52 个正式文件，与 B 签收版本的原始字节完全一致；CR-001、CR-002 维持关闭，代码审查和注释可读性结论不变。本次仅补齐流程记录，没有修改实现或重新运行原生测试。

| B 注释覆盖 | 留存结论 |
|---|---|
| Asset/Scene 与 Renderer | 覆盖 GltfGeometry、GltfTangents、StaticTangents、SceneMaterial/SceneState、StaticGpu/StaticTextures、StaticDraw、Forward、View、FrameGraph、CameraSnapshot/ViewPreparation，核对固定版本、借用与保有、预算、候选上传、图权限和接受/完成的区别。 |
| Editor、Runtime 与 GL 消费者 | 覆盖 UiRenderer/UiTextures/UiFrame、EditorSession、Workspace、Viewport、Runtime 的 Main/Frame/Resources 及 CommandListTransfer；核对状态所有者、旧输出、输入映射、诊断可读性、重建终止和资源释放时机。 |
| Shader、schema 与 CMake | 覆盖共享静态几何/表面/覆盖/预览模块、Forward 的五个阶段入口与三个程序声明、两个接口 schema，以及 Engine、Asset、Runtime 的 CMake；核对 ABI、颜色域、MASK、绕序、包路径及唯一生成目标。 |

B 初审在 32 个文件中完善注释；整改复核覆盖 A 的 11 个改动文件，其余 41 个与初审后版本一致，最终又完善 3 个文件的注释。B 实际执行源码/合同、消费者与非注释内容比较、diff 和链接核对；未扫描 build、未独立核验底层构建产物或运行 GPU/UI。针对性原生回归引用 A 的匹配源码记录，后续签收版本的实际构建、V1–V5 和整改回归由 A 执行，见下节。勾选表示 B 自己的审查职责完成，不替代 A 的验证与归档责任。

<a id="final-verification"></a>
## 最终验收与证据

2026-11-16，A 运行 `python build/d06-t05/final/VerifySigned.py`。基准为 `f1d5b8003bb54c0e8feff358db8c0ceb4b2fb3b3`。先比较基准、30 个修改和 22 个新增文件的原始字节摘要，再核对 1578 个正式构建输入。与整改验证相比，3 个 B 复核注释文件的字节更新，其余正式输入逐一一致。签收版构建清单 SHA-256 为 `9784463fc89a60435f03ebcce3e1ea114a5067d26fc7c48dba54b65dd163aa2c`。

| 配置 | 签收版本实际运行 |
|---|---|
| Debug | 正式 `Physara` 构建、V1–V5、`Physara --hidden --poll-limit=6`、CR-001 和 CR-002 均通过。 |
| Release | 同样通过正式构建、V1–V5、产品短运行和两项针对性回归。 |

两套探针复用对应配置的签收版正式对象、库及 Shader 包，按原来的源码和 fixture 重新链接、运行。检查在 Intel RaptorLake-S Mobile Graphics Controller、OpenGL 4.6.0、驱动 `32.0.101.6129`，Windows SDK `10.0.26100.0` 下完成。两个配置的完整探针各运行一次产品短运行，此结果供同配置的定向探针共用。`build/d06-t05/final/signed-run-summary.json` 关联正式输入、签收记录、四份 `evidence.json` 和日志，SHA-256 为 `b79cff6e701569ee689f7f0ce0922b9207bc0d09d5b01268ec414d58a145638f`。

| 规范 | 实现与验证 |
|---|---|
| 静态 GPU 驻留，4 项要求、7 个场景 | 资产键、材质覆盖、sampler 和 mip 进入已准备的 GPU 代。V2、V3、V5 核对复用、上传、退休和实际容量边界。 [静态准备](../../../../Engine/Renderer/Draw/StaticDraw.cpp) · [纹理](../../../../Engine/Renderer/Materials/StaticTextures.cpp) |
| 静态表面，4 项要求、6 个场景 | V3 核对 UV、镜像、法线、双面、深度覆盖和标准通道。 [顶点求值](../../../../Shaders/Geometry/StaticVertexFetch.glsl) · [覆盖](../../../../Shaders/Material/StaticCoverage.glsl) |
| 静态 Forward，3 项要求、6 个场景 | V1、V3、V4 从预解析 Draw 验证真实深度、工作色和带模式的显示色。CR-002 核对混合支持项的降级诊断及有效像素。 [子图](../../../../Engine/Renderer/Paths/Forward.cpp) · [菜单](../../../../Editor/Panels/Workspace.cpp) |
| 编辑器帧图，5 项修改、14 个场景 | V4、V5 与 CR-001 验证实际采样、裁剪、图内写入、普通 UI 重建及安全图接受边界。 [帧编排](../../../../Runtime/Frame.cpp) · [UI 合成](../../../../Editor/ImGui/UiRenderer.cpp) |
| 视口输出，2 项修改、10 个场景 | V4、V5 核对稳定 token、相机和图像矩形、匹配的颜色代及回退。两项 CR 核对诊断、确认资格和输入映射。 [View](../../../../Engine/Renderer/View.cpp) · [会话](../../../../Editor/Session/EditorSession.cpp) |
| Runtime 生命周期，1 项修改、4 个场景 | V4、V5 验证 CPU 发布及编辑后准备，场景程序缺失时保有基础 UI 并恢复。 [产品调度](../../../../Runtime/Main.cpp) · [帧边界](../../../../Runtime/Frame.cpp) |

最终证据覆盖 19 项要求的实现路径和 47 个场景的合同核对。V1–V5 与定向探针实际运行代表性风险用例，含 0.75/0.25 Reverse-Z 深度、阈值覆盖、真实 texel 与窗口像素、已发失败前缀、同轮上传准入与旧颜色安全采样。用例在上述单一原生 OpenGL 环境运行，换代验证保留有待消费完成结果的旧引用，三槽持续饱和、跨驱动性能和正式物理光照属于后续范围。

### 主规范同步

2026-11-16，OpenSpec 1.13.2 定位并校验此 change 的六份规范增量。同步合入 11 项新增和 8 项修改要求，涉及 47 个场景，其中 23 个为现有条目，24 个为新增。核对既有规范的 Purpose、未受影响的要求和场景均保留，同步结果具备幂等性。`build/d06-t05/final/spec-sync.json` 留存同步清单。主规范严格校验 31/31，通过含活动 change 的全量严格校验 32/32。

| 主规范 | 合入范围 |
|---|---|
| [静态 GPU 驻留](../../../specs/static-gpu-residency/spec.md) | 新建，4 项要求、7 个场景。 |
| [静态表面求值](../../../specs/static-surface-evaluation/spec.md) | 新建，4 项要求、6 个场景。 |
| [静态 Forward](../../../specs/static-forward-rendering/spec.md) | 新建，3 项要求、6 个场景。 |
| [编辑器帧图](../../../specs/editor-frame-graph/spec.md) | 修改 5 项，新增 2 个场景。 |
| [编辑器视口输出](../../../specs/editor-viewport-output/spec.md) | 修改 2 项，新增 2 个场景。 |
| [Runtime 生命周期](../../../specs/runtime-lifecycle/spec.md) | 修改 1 项，新增 1 个场景。 |

B 已在问题单及本任务表完成签收与 6.1、6.3 登记。A 的最终 verify 和主规范同步记录已留存，剩余 7.3 由 A 执行归档、核对开发计划并在归档成功后重置问题单。

### 归档

2026-11-16，OpenSpec 1.13.2 将 `render-static-scenes-forward` 按 `spec-driven` 移入 `2026-11-16-render-static-scenes-forward`。六份规范增量和 `.openspec.yaml` 保持原始字节，提案、设计和任务中的相对链接随目录调整。开发计划登记 D06-T05 已验收归档，当前工作包推进到 D06-T06。 [开发计划](../../../../Docs/Developments/Developments.md#current-stage)

归档后 OpenSpec 活动 change 为 0，31 份主规范均通过严格校验。已核对开发计划和归档的相对链接、签收源码及既有归档。问题单按模板逐字节恢复，归档任务完成 19/19。
