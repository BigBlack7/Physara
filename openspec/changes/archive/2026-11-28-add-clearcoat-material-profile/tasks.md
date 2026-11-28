# Tasks

## 1. A · 标准资产与有效实例

- [x] 1.1 接入 clearcoat 声明、三个槽和切线前提。用 `build/d12-t02/` 下共享小夹具通过正式 glTF/GLB 入口验证缺省、required、非对称打包 R/G、独立 normal scale、有效 UV 与纹理变换。将同一源图像替换一次，核对版本变化及旧固定结果。对同一族夹具验证可用源切线与可用基底 normal 的准入、不可用切线/UV、一个越界因子和同材质排斥，记录实际字段原因。 [导入规格](specs/static-gltf-import/spec.md)
- [x] 1.2 将 Scene 的清漆字段及纹理槽接入 Inspector 和 v1 稀疏读写。两个实例共享源时以正式编辑入口验证写入、清除、Undo/Redo、用途及各自有效 UV。保存重开覆盖项和已有 v1，核对显式零与继承的身份。以一个非法槽候选同时验证领域拒绝和 v1 打开保留活动文档的边界。 [编辑规格](specs/editor-domain-edits/spec.md) · [文档规格](specs/scene-document-persistence/spec.md)

## 2. A · 生成 ABI、双法线与层合成

- [x] 2.1 更新 StaticForward 的权威 schema、有效材质行及静态几何准备。构建生成接口及其 C++ 直接消费者，核对 stride、t5 材质表、t8–t10 与 sampler 映射、每槽 UV 变换及清漆框架的顶点布局。用一组同时含有共用 UV 和两套有效 UV 的输入核对角点接缝、几何身份与实际准备数据。 [ABI 与角色](design.md#abi-and-roles)
- [x] 2.2 接通全灯 Direct 的独立法线、逐灯清漆与基底衰减。沿共享几何夹具使用不同有效 UV、非对称法线/UV 变换及一个带镜像手性的双面样点，核对各槽框架和接缝对应的工作色。选择基底 NoL 为零但清漆可见、以及光线越过几何半球的见证，与解析出射权重和单位 Fresnel GGX 对照。用同一批样点核对零强度、粗糙度下限、核心与 IOR/specular 基底及有限输出。 [几何和法线](design.md#source-and-geometry) · [表面规格](specs/static-surface-evaluation/spec.md)
- [x] 2.3 接通 Physical 的清漆 DFG、环境反射与分层 emission。复用同一夹具，以无发射白炉中的独立高精度积分核对清漆粗糙度、视角和能量预算。用获准的非对称 EXR 对照基底和清漆反射方向及匹配滤波，再核对无环境但有灯的 DFG-only。最后用无灯无环境且有自发光的样点核对正视、掠射及一次曝光变化的出射衰减。先登记积分和工作色量化误差界，对照成立后结束本组数值验证。 [层合成](design.md#coat-lighting) · [环境规格](specs/environment-lighting/spec.md) · [参考资产](../../../../Assets/README.md#forward-参考场景)
- [x] 2.4 补齐全灯与 Froxel 的十二个清漆颜色角色，编译正式包并核对两阶段反射和实际设备绑定上限。逐角色检查清漆顶点框架与活动纹理，确认核心、IOR/specular、遮挡和独立表面消费者的生成接口与活动绑定配对。目录与可达角色一致后结束接口枚举。 [准入规格](specs/shader-generation-reload/spec.md)

## 3. A · 路由、资源准入与正常交付

- [x] 3.1 接通三路径的清漆颜色角色、有效材质身份及 SurfaceAux。用真实导入的清漆 MASK、底部核心遮挡、清漆叠加 IOR/specular 和一个等深胜出对照，固定相机、光照、环境、曝光、阴影与矩形。为 Direct 和 Physical 配对各路正常接受票据、D32、阴影和显示映射前 HDR，另读 Deferred 的 G1/G3。核对四附件、基底 G1、扩展 Draw 数、MASK 孔和精度界内的工作色。 [三路径规格](specs/static-forward-rendering/spec.md) · [现有比较判据](../../../../Docs/Performance/ThreePathReference.md#quality)
- [x] 3.2 使清漆纹理、附加几何和 Shader 需求按实际角色准备。用一个清漆绑定缺项覆盖冷包准入、独立表面的资源及正常颜色拒绝、未选角色的热候选拒绝和合法恢复。用同一 OPAQUE 标样核对独立表面在无清漆颜色时仍取得完整 G1/G3/D32，并在恢复后确认实际槽读。以合法共享接口候选的真实编码占用验证包缓存容纳活动代和候选，按当前配置记录额度与整代发布。 [准入规格](specs/shader-generation-reload/spec.md)
- [x] 3.3 用完成帧核对合法实例清漆覆盖、清除及 Undo 后的材质身份、所选颜色及历史前驱。复用 1.2 的共享源与 3.1 的可见标样，核对 Inspector 中准备或拒绝状态和最后接受颜色。有效修改后相关历史失效，非法覆盖保留此前可读结果与合法历史。 [静态内容](../../../../Docs/Architecture.md#static-content) · [编辑规格](specs/editor-domain-edits/spec.md)

## 4. A · 集成验证与审查交接

- [x] 4.1 用配套生成接口重建 Debug 和 Release 正式产品并隐藏启动。复用 D11 的混合表面方法和 D12-T01 的 IOR/specular 对照，在当前包重放受 ABI 或基底求值影响的代表场景。比较同输入的覆盖、深度、工作色与提交身份，必要时复用匹配的既有阴影证据。记录实际的设备/驱动、配置、分辨率、曝光和渲染路径，确认代表性回归通过。 [构建说明](../../../../Docs/Build.md) · [三路径复验](../../../../Docs/Performance/ThreePathReference.md#replay) · [T01 原生验证](../2026-11-28-add-ior-specular-material-profile/tasks.md#final-verification)
- [x] 4.2 执行 `openspec validate add-clearcoat-material-profile --strict`，按七项规格核对已执行、复用和待复核范围。交接登记版本、修改和未跟踪输入、包、夹具与读回身份。按档位规则在当前问题单给出 B 建议，说明双切线、层权重、活动资源和辅助顺序风险，交由维护者调度。 [审查档位](../../../../Docs/Developments/AgentModels.md#review-tier) · [当前问题单](../../../../build/d12-t02/final/review-signed.md)

## 5. B 审查、A 整改与 B 复核

- [x] 5.1 独立 B 核对交接输入，围绕 Khronos 字段与切线条件、基底保真、配对 DFG、实际 Shader 配方及图内覆盖开展复核。补齐受影响第一方文件的职责、接口和流程注释，把可复现问题、实际覆盖和证据写入当前问题单。2026-11-28 首审登记 CR-001 至 CR-004，待 A 整改。 [本轮问题单](../../../../build/d12-t02/final/review-signed.md) · [审查闭环](../../../../AGENTS.md#review-loop)
- [x] 5.2 A 依据问题单逐项整改和自检，按受影响风险重建或重放，登记对应问题编号、变更、验证输入及结果。由独立 B 核对更新后的输入和验证要求。 [当前问题单](../../../../build/d12-t02/final/review-signed.md) · [第 1 轮整改](#a-correction-1) · [第 2 轮整改](#a-correction-2)
- [x] 5.3 独立 B 核对最终输入和整改证据，按问题单复核受影响的实际行为及注释。各项关闭或有依据撤回后，登记签收版本、复核范围、实际审查档位与限制。

## 6. A · 最终验证、同步与归档

- [x] 6.1 对 B 签收版本完成最终 OpenSpec verify 和针对性原生验证。复用完全匹配的证据，针对具体变化或缺口重建、重放，登记工作包实际支持范围、合法组合、三路径结果、档位记录和独立验收证据。 [最终验证](#final-verification)
- [x] 6.2 在最终验证通过后同步对应主规格并归档。核对主规格严格校验、归档状态、计划引用和文档链接。把签收问题单留存后按模板重置当前问题单，开发计划登记 D12-T02 完成和后续依赖。 [工作包](../../../../Docs/Developments/D12-D13-Materials.md#d12-t02) · [归档记录](#archive-closeout)

<a id="a-local-verification"></a>
## A 本地验证与交接

2026-11-28，Windows x64、MSVC 19.44、Windows SDK 10.0.26100.0。Debug 和 Release 重建了 `Physara` 与配套 Shader 包，均通过 `--hidden --poll-limit=3`。Release 原生回放使用 Intel RaptorLake-S Mobile Graphics Controller，OpenGL 4.6.0，驱动 32.0.101.6129。三路径主回放为 640×480、单采样，曝光 `f/4 · 1/16 s · ISO 100`。独立表面回放为 80×48。 [Release 构建](../../../../build/d12-t02/product-Release.log) · [Debug 构建](../../../../build/d12-t02/product-Debug.log)

- 导入与文档。`coat-assets` 经正式 Asset/Scene/Editor 入口比对 glTF 与 GLB、默认值、线性 R/G、UV 变换和法线缩放。替换清漆图像触发源版本更新，旧固定像素保留。非法 UV、切线前提、因子和扩展组合有字段诊断。旧 v1 与稀疏清漆 v1 均可重开，显式零和继承可区分。相同的非法清漆槽用途由领域和 v1 打开拒绝，活动场景、已有文档路径和版本保持。 [导入与文档回放](../../../../build/d12-t02/coat-assets-Release.log)
- ABI 和实际角色。生成材质行 448 B，材质表在 t5，清漆图像和 sampler 在 t8–t10 / s8–s10。共用切线和双切线布局的 stride 分别为 64 B 和 88 B，异 UV 标样按联合接缝从 4 顶点拆为 6 顶点并保留两种手性。十二个新增颜色配方逐一核对顶点输入、阶段反射和绑定上限。各清漆片元配方使用 10–13 个纹理单元，设备为 32 个片元纹理单元、16 个顶点属性。 [生成与准入回放](../../../../build/d12-t02/coat-admission-Release.log)
- 光照与几何。双 UV 接缝分侧与镜像双面按解析 GGX 对照。基底 `NoL=0` 的两个样点有约 `3.40×10⁻⁵` 和 `6.31×10⁻⁵` 的清漆工作色。光线进入几何背面后，镜像侧两个清漆法线仍朝向光源，实际清漆贡献为零。感知粗糙度输入 0 与 0.045 在标样上产出相同且有限的 HDR。白炉用 256×256 单位 Fresnel GGX 积分对照两种清漆粗糙度及两个视角，四点最大误差 `0.000263`，低于预设 `0.007`。通过非对称 EXR、更换清漆法线、DFG-only 和无灯无环境发射回放，发射与出射 Fresnel 相符，增加一档曝光后输出随曝光翻倍。 [光照回放](../../../../build/d12-t02/coat-light-Release.log) · [数值与三路径读回](../../../../build/d12-t02/proof-light-routes.json)
- 三路径和编辑。Direct 与 Physical 均对照 Forward、Forward+、Deferred 的实际接受票据，每种模式的 17,328 个清漆胜出像素在预曝光 HDR 上逐位相同。D32 和 10 层阴影逐字节相同。Deferred 的 MASK 孔、等深胜出和有限远面与预期配对，扩展 G1 保留基底法线和粗糙度。编辑、清除和 Undo 对应扩展数 `5 → 4 → 5`，Undo 恢复 HDR/G3，D32 一致。非法编辑保留历史前驱。实际 Inspector 记录准备、拒绝和最后接受的清漆身份。 [三路径回放](../../../../build/d12-t02/coat-routes-Release.log) · [Inspector 记录](../../../../build/d12-t02/native/coat-inspector-accepted.txt)
- 隔离与整代恢复。原生冷包和当前未选热角色均使用真实编译的 `clearcoatNormalImage` 缺项包。冷包的独立 OPAQUE 表面图只准备 2 个材质图像。恢复正常受光后读取 8 个，包含清漆槽。两次提交的 G1、G3 和 D32 字节一致。热候选拒绝后代 1 保留相容历史，随后合法全包发布代 2，Deferred 正常接受。共享 ABI 的源重载通过真实编译使活动代从 2 到 3，包缓存同时保有 105 项，占 12,607,148 B，额度 16,777,216 B。 [冷包与热候选](../../../../build/d12-t02/coat-fault-Release.log) · [源重载](../../../../build/d12-t02/coat-reload-Release.log)
- 既有行为。用当前 Release 可执行输入重放 D11 混合表面和 D12-T01 IOR/specular 三路径。沿用两组对照的原始精度界，重新读取当前的覆盖、D32、HDR 与正常提交身份。IOR/specular 对照重新读取配对阴影。 [D11 对照](../../../../build/d12-t02/native/mixed-proof.json) · [D12-T01 对照](../../../../build/d12-t02/native/ior-proof-routes.json)
- 交接输入。七份 delta spec 随 change 通过 OpenSpec 严格校验。原生回放的八个模式均在同一版 Release 回放程序通过，待审工作树、Debug/Release 包、夹具、程序和读回均登记 SHA-256。 [严格校验](../../../../build/d12-t02/openspec-validate.json) · [整组回放](../../../../build/d12-t02/replay-Release.log) · [待审输入](../../../../build/d12-t02/handoff.json)

A 建议 B 审查档位 Y（GPT-6.2 Astra `high`）。双切线与镜像手性、GGX/DFG 层权重、活动 Shader 资源以及 SurfaceAux 胜出顺序涉及多个模块，均有可分别复核的原生见证。B 的实际配置与独立签收待维护者调度。 [本轮问题单](../../../../build/d12-t02/final/review-signed.md#本轮交接)

<a id="a-correction-1"></a>
## 第 1 轮 A 整改与自检

2026-11-28，基准 `a8eee97ef236fbd5851f5bd1fb362408b0d076e8` 加 B 首审注释和本轮整改。重新构建 Debug 与 Release 的 `Physara` 和配套 Shader 包，两者均通过 `--hidden --poll-limit=3`。定向读回在 Intel RaptorLake-S Mobile Graphics Controller、OpenGL 4.6.0、驱动 32.0.101.6129 的 Release 运行。 [Debug 构建](../../../../build/d12-t02/a-correction-1/product-Debug.log) · [Release 构建](../../../../build/d12-t02/a-correction-1/product-Release.log)

- **CR-001**　固定源 UV 使用已验证框架，新增或改用的 UV 由正式角点生成校验。B 的三角形输入在 typed edit 和 v1 打开均触发 `clearcoatNormalTexture.texCoord` 的 MikkTSpace 单位框架拒绝。原覆盖、有效版本及活动文档保持，合法源 UV0 和可生成的 UV1 对照通过。 [原生编辑和 v1 回放](../../../../build/d12-t02/a-correction-1/cr001-frame-Release.log)
- **CR-002**　冷包和热候选在同一角色检查中核对双切线属性及跨阶段传递。真实编译的包分别缺 `aTangent` 或 `vTangent`，报告对应 vertex 或 fragment 阶段。拒绝未选热角色时旧代仍可提交，合法完整候选再由实际 Deferred 受光接受。十二个正式角色的属性、跨阶段传递与绑定逐项通过。 [冷、热与恢复](../../../../build/d12-t02/a-correction-1/coat-fault-Release.log) · [片元见证](../../../../build/d12-t02/a-correction-1/cr002-varying-Release.log) · [正式包反射](../../../../build/d12-t02/a-correction-1/coat-admission-Release.log)
- **CR-003**　环境基底漫反射和镜面权重在工作色限幅前施加清漆透过率。无灯单位环境中，以校准源和两档曝光对照，未衰减值约 100,000，解析透过率约 0.509338。两类样点实际为 50,912 和 25,456，量化误差低于预设 64。零强度与无层基底逐位一致，D32 保持。 [独立计算及原始读回摘要](../../../../build/d12-t02/a-correction-1/bright-proof.json)
- **CR-004**　用已接受帧和实际 Inspector 文本核对受光、Base Color、再次请求受光和重开文档。诊断显示当前预览及上一笔清漆受光，返回受光准备期间保留旧票据，正常接受后显示新票据。新场景的诊断和受光各按自己的身份交付。 [面板回放](../../../../build/d12-t02/a-correction-1/cr004-inspector-Release.log) · [诊断文本](../../../../build/d12-t02/a-correction-1/native/cr004-base-color.txt) · [返回受光文本](../../../../build/d12-t02/a-correction-1/native/cr004-return-lit.txt)
- **直接回归**　当前包重放清漆资产和文档、三路径覆盖、阴影、双法线和环境积分，以及 IOR/specular 基底。Direct 与 Physical 各 17,328 个清漆胜出像素的三路径 HDR 字节一致，三路 D32 和十层阴影字节一致。独立 OPAQUE 表面在颜色被拒绝及合法恢复后仍对齐 G1、G3 和 D32。 [回放汇总](../../../../build/d12-t02/a-correction-1/replay-Release.log) · [三路径与光照数值](../../../../build/d12-t02/a-correction-1/proof-light-routes.json) · [既有基底](../../../../build/d12-t02/a-correction-1/native/ior-proof-routes.json)

A 完成四项自检。独立 B 复核、配置登记和签收待维护者调度。待审工作树、正式产品、故障包、回放程序和读回均登记 SHA-256。 [整改输入清单](../../../../build/d12-t02/a-correction-1/handoff.json) · [当前问题单](../../../../build/d12-t02/final/review-signed.md#本轮交接)

<a id="b-recheck-1"></a>
## 第 1 轮 B 整改复核

2026-11-28，已核对 A 整改输入。CR-002 的双切线准入、CR-003 的亮区 IBL 通过。CR-001 的基底换 UV 和 CR-004 的阴影诊断身份仍待整改。新增 CR-005（清漆角色漏验 UV1）及 CR-006（标量编辑重复生成切线）。B 已更新涉及的接口与阶段说明，5.3 保留待签收。 [当前问题单与逐项证据](../../../../build/d12-t02/final/review-signed.md) · [本轮输入](../../../../build/d12-t02/a-correction-1/handoff.json)

<a id="a-correction-2"></a>
## 第 2 轮 A 整改与自检

2026-11-28，基准 `a8eee97ef236fbd5851f5bd1fb362408b0d076e8` 加 B 复核注释及本轮整改。Debug 和 Release 正式构建、隐藏启动及 OpenSpec 严格校验通过。十项原生回放使用本机 Intel RaptorLake-S Mobile Graphics Controller、OpenGL 4.6.0、驱动 32.0.101.6129 的 Release。 [整组回放](../../../../build/d12-t02/a-correction-2/replay-Release.log) · [严格校验](../../../../build/d12-t02/a-correction-2/openspec-validate.json)

- **CR-001**　分别确认基底和清漆的有效法线 UV。两槽继承 UV0 时，单独将任一槽换到 B 的平行投影 UV1，编辑和 v1 均按对应槽报告 MikkTSpace 框架错误。原覆盖、Scene 版本及活动文件保持。可生成的合法 UV1 在双框架构造和文档重开通过。核心覆盖在首次启用清漆时校验有效法线框架。 [基底槽回放](../../../../build/d12-t02/a-correction-2/cr001-base-Release.log) · [清漆槽回放](../../../../build/d12-t02/a-correction-2/cr001-frame-Release.log)
- **CR-004**　级联诊断以已接受的诊断类型和阴影请求代识别，真实面板经过受光票据 6、四级联诊断票据 7、关闭后等待及受光票据 10。诊断和等待阶段保留票据 6，下一次受光接受后更新。Base Color 和跨场景票据的既有回放通过。 [级联转换](../../../../build/d12-t02/a-correction-2/cr004-shadow-Release.log) · [材质诊断和重开](../../../../build/d12-t02/a-correction-2/cr004-inspector-Release.log)
- **CR-005**　清漆颜色角色核对完整的活动属性和跨阶段输入，包括两套 UV。真实缺 `aUv1` 的冷包及未选热包被定位到 vertex，真实缺 `vUv1` 的包被定位到 fragment。热拒绝沿用代 1，合法代 2 实际提交 Deferred。十二个正式配方在原生设备通过角色检查。同一非对称 R 纹理在 UV0、UV1 实例上产生相反的增强侧，四点辐亮度最大绝对误差低于 0.001。 [冷、热与恢复](../../../../build/d12-t02/a-correction-2/coat-fault-Release.log) · [活动片元输入](../../../../build/d12-t02/a-correction-2/cr005-varying-Release.log) · [UV 样点读回](../../../../build/d12-t02/a-correction-2/uv-proof.json)
- **CR-006**　事务以相同固定模型、源 Primitive、槽及有效 UV 复用已经提交的切线验证。18,432 三角形样例中，五次纯粗糙度编辑实际调用角点生成验证 0 次。首次派生、实际切回派生 UV、重开 v1 分别调用 1 次。计数仅插入工作包回放使用的 SceneMaterial 副本，校验和编辑仍走正式代码。 [同步编辑回放](../../../../build/d12-t02/a-correction-2/cr006-scalar-Release.log) · [探针来源](../../../../build/d12-t02/a-correction-2/StageCorrections.py)
- **直接回归与证据复用**　同版输入的 Direct 和 Physical 各有 17,328 个清漆胜出像素在三路径 HDR 逐字节相同。三路径的 D32 与十层阴影也逐字节一致。颜色阻断和恢复前后，独立 OPAQUE 表面的 G1、G3、D32 读回一致。两种配置的各 57 份正式包与 6 份生成接口文件均与首轮整改证据的 SHA-256 匹配。 [三路径与独立表面](../../../../build/d12-t02/a-correction-2/proof-routes.json) · [包与接口核对](../../../../build/d12-t02/a-correction-2/retained-package-identity.json)

A 自检已完成。下一步是维护者调度独立 B 复核四项整改、实际输入及受影响注释。 [第 2 轮待审输入](../../../../build/d12-t02/a-correction-2/handoff.json) · [问题单](../../../../build/d12-t02/final/review-signed.md#本轮交接)

<a id="b-final-review"></a>
## 第 2 轮 B 复核与签收

2026-11-28，B 签收 `v2.0` 基准 `a8eee97ef236fbd5851f5bd1fb362408b0d076e8` 加第 2 轮整改工作树及 B 的一行同 UV 框架注释。CR-001 至 CR-006 全部通过，代码和注释均已验收。A 建议审查档位 Y，实际调度的模型与推理级别未获维护者确认。 [B 问题单](../../../../build/d12-t02/final/review-signed.md) · [签收清单](../../../../build/d12-t02/b-recheck-2/signed-inputs.json)

- CR-001、006：B 实测基底与清漆的非法 UV 按各自槽拒绝。在 18,432 三角形网格上，首次派生调用角点验证一次，五次标量提交调用零次，切回派生 UV 再调用一次。核对 v1 新候选独立验证。 [B 领域实测](../../../../build/d12-t02/b-recheck-2/witness.log) · [B 事务实测](../../../../build/d12-t02/b-recheck-2/ui-scalar.log)
- CR-004：B 在实际面板验证清漆受光票据 5、级联诊断票据 6 及恢复受光票据 9。诊断和关闭后等待均展示历史票据 5。 [诊断标签](../../../../build/d12-t02/b-recheck-2/inspector-shadow.txt) · [恢复前标签](../../../../build/d12-t02/b-recheck-2/inspector-return.txt)
- CR-002、005：B 重跑缺基底切线、缺 UV1 的包和合法包。真实包分别遭到对应属性拒绝或获准。核对片元缺项、热拒绝、恢复提交及非对称 UV 四点原始 HDR，最大绝对辐亮度误差 0.000727。 [B 准入实测](../../../../build/d12-t02/b-recheck-2/witness.log) · [本轮 UV 帧](../../../../build/d12-t02/a-correction-2/native/cr005-uv-sampling.json)
- CR-003：保持第一轮亮区 IBL 的两档曝光结论。期间相关 Shader 源、正式包及生成接口匹配。B 本轮独立复算 Direct 与 Physical 各 17,328 个清漆像素，三路 HDR、D32 与阴影按既定口径一致。 [亮区读回](../../../../build/d12-t02/a-correction-1/bright-proof.json) · [包与接口核对](../../../../build/d12-t02/a-correction-2/retained-package-identity.json)

本轮原生覆盖限于交接记录的 Intel OpenGL 4.6 Release。Debug/Release 产品构建、文档打开及完整热候选恢复沿用 A 的同输入证据。6.1、6.2 待 A 执行。

<a id="final-verification"></a>
## A 最终验证

2026-11-28，A 核对 B 签收的 `v2.0@a8eee97ef236fbd5851f5bd1fb362408b0d076e8` 加第 2 轮整改工作树。452 条记录的文件大小和 SHA-256 全部匹配，其中工作树 71 条、正式产品 180 条、原生回放 181 条、前序证据 8 条、B 独立复核 12 条。CR-001 至 CR-006 的代码与注释均经 B 验收。A 建议 B 使用 Y 档（GPT-6.2 Astra `high`），实际档位、模型与推理级别待维护者登记。 [签收清单](../../../../build/d12-t02/b-recheck-2/signed-inputs.json) · [签收问题单](../../../../build/d12-t02/final/review-signed.md) · [输入核对](../../../../build/d12-t02/final/signature-verification.json)

本包支持 lit metallic-roughness 的 OPAQUE 和 MASK，可在核心或 IOR/specular 基底上叠加清漆。基底与清漆法线各用其有效 UV，共用 UV 时共用几何切线框架，各槽仍使用自己的采样变换。生成材质行 448 B，十二个清漆颜色角色覆盖三路径、Direct 与 Physical 的对应配方，Deferred 保持四个 GBuffer 附件。 [设计与范围](design.md) · [实际角色与反射](../../../../build/d12-t02/final/coat-admission-Release.log)

正式构建至 B 签收之间的源码差异为 `SceneMaterial.cpp` 中的一行注释。A 按签收摘要复用 Debug、Release 产品与配套包，两版均重新通过 `--hidden --poll-limit=3`。十项 Release 原生回放使用独立探针目录和匹配的正式库，从签收的 Scene 源码重建角点计数探针。实机为 Windows x64、Intel RaptorLake-S Mobile Graphics Controller、OpenGL 4.6.0，驱动 32.0.101.6129。三路径使用 640×480 单采样，曝光 `f/4 · 1/16 s · ISO 100`。 [验证清单](../../../../build/d12-t02/final/verification-manifest.json) · [原生回放](../../../../build/d12-t02/final/replay-Release.log)

| 规格与场景 | 最终证据 |
|---|---|
| [静态 glTF 导入](specs/static-gltf-import/spec.md)（2 条需求、7 个场景） | 当前 glTF/GLB 回放覆盖 required、默认值、线性 R/G、normal scale、有效 UV、变换、非法槽与扩展组合、替换图像及旧版本保活。既有未知扩展的 required 拒绝、可用及不可用核心回退沿用 D06 的验证，核对扩展报告实现。 [资产回放](../../../../build/d12-t02/final/coat-assets-Release.log) · [既有扩展证据](../2026-11-16-import-static-gltf-models/tasks.md#final-verification) |
| [实例编辑](specs/editor-domain-edits/spec.md)（1 条需求、6 个场景）及 [场景文档](specs/scene-document-persistence/spec.md)（1 条需求、2 个场景） | 当前回放覆盖共享源实例的独立编辑、清除、Undo/Redo、旧 v1 重开、继承与显式零、非法候选保留活动状态。18,432 三角形标样的五次标量编辑触发零次角点生成验证，切回派生 UV 及重开 v1 各一次。Inspector 的受光、级联诊断、等待与恢复票据在当前回放确认。基底及清漆非法 UV 的对照分别参见当前和签收回放。 [基底 UV](../../../../build/d12-t02/final/cr001-base-Release.log) · [清漆 UV](../../../../build/d12-t02/a-correction-2/cr001-frame-Release.log) · [计数](../../../../build/d12-t02/final/cr006-scalar-Release.log) · [面板票据](../../../../build/d12-t02/final/cr004-shadow-Release.log) |
| [共享表面](specs/static-surface-evaluation/spec.md)（1 条需求、3 个场景）及 [环境响应](specs/environment-lighting/spec.md)（1 条需求、2 个场景） | 当前非对称 UV0/UV1 四点最大绝对辐亮度误差 0.000727。按两版配套包与生成接口的相同摘要复用 64 项整改证据，重新计算双法线、几何半球、白炉、非对称 EXR、DFG-only 与发射对照。白炉最大误差 0.000263，低于 0.007 界限。两档曝光高亮 IBL 的读值为 50,912 与 25,456，零清漆对照与纯基底一致。 [UV 证据](../../../../build/d12-t02/final/uv-proof.json) · [复用及重算](../../../../build/d12-t02/final/retained-evidence.json) · [接口身份](../../../../build/d12-t02/a-correction-2/retained-package-identity.json) |
| [三路径交付](specs/static-forward-rendering/spec.md)（2 条需求、4 个场景） | Direct 和 Physical 各有 17,328 个清漆胜出像素，Forward、Forward+ 与 Deferred 的显示映射前 HDR 对这些像素逐字节一致。每组完整 D32 及十层阴影逐字节一致。MASK 孔、等深胜出、有限远面与基底 G1 对照通过，清除和 Undo 的扩展数为 `5 → 4 → 5`。当前 D11 混合表面回放通过，IOR/specular 独立基底沿用相同包的三路径证据。 [当前三路径读回](../../../../build/d12-t02/final/proof-routes.json) · [IOR/specular 对照](../../../../build/d12-t02/a-correction-1/native/ior-proof-routes.json) · [D11 回放](../../../../build/d12-t02/final/native/mixed-proof.json) |
| [Shader 准入和重载](specs/shader-generation-reload/spec.md)（1 条需求、2 个场景） | 十二个正式角色通过设备准入。真实缺 UV1 顶点输入的冷包被拒绝，独立 OPAQUE 表面仍接受。未选热角色拒绝后保留代 1，完整候选发布代 2 并实际提交 Deferred。独立表面前后 G1、G3、D32 字节一致。签收的切线与片元缺项证据仍匹配。用当前源码编译的合法重载从代 2 发布至代 3，活动代与候选合计 105 项、12,608,204 B，低于 16,777,216 B 配额。 [冷包与热包](../../../../build/d12-t02/final/coat-fault-Release.log) · [B 准入实测](../../../../build/d12-t02/b-recheck-2/witness.log) · [源重载](../../../../build/d12-t02/final/coat-reload-Release.log) |

OpenSpec 1.13.2 严格校验通过，核对七项能力、九条需求、二十六个场景对应的当前消费者与验证记录。Debug 本轮执行正式启动检查，原生读回范围为签收记录中的 Intel OpenGL Release 设备和单采样路径。 [严格校验](../../../../build/d12-t02/final/openspec-change-validation.json)

<a id="archive-closeout"></a>
## 规格同步与归档

- 七份增量已同步到对应主规格，新增六条需求并更新三条需求。同步后 45 份主规格和 D12-T02 change 均通过严格校验，归档后 45 份主规格复验通过。 [同步摘要](../../../../build/d12-t02/final/spec-sync.json) · [同步后主规格校验](../../../../build/d12-t02/final/openspec-sync-validation.json) · [同步后 change 校验](../../../../build/d12-t02/final/openspec-post-sync-change-validation.json) · [归档后主规格校验](../../../../build/d12-t02/final/openspec-main-validation.json)
- 2026-11-28 归档 `add-clearcoat-material-profile`，OpenSpec 活动列表为空。开发计划已登记 D12-T02 完成，D12-T03 前置已满足。 [归档回执](../../../../build/d12-t02/final/openspec-archive.json) · [活动列表](../../../../build/d12-t02/final/openspec-post-archive-list.json) · [开发计划](../../../../Docs/Developments/Developments.md#current-stage) · [D12-T03](../../../../Docs/Developments/D12-D13-Materials.md#d12-t03)
- 核对归档内容、已签收的源码和产品、主规格及本次文档链接后，按模板重置当前问题单。B 签收的问题单以原始字节留存，SHA-256 为 `21bf0745aa56c5665efc97924534a7a24bca3e4ff44bbd4cb27ec462f4a1e570`。 [签收问题单](../../../../build/d12-t02/final/review-signed.md) · [重置前核对](../../../../build/d12-t02/final/archive-before-reset.json)
