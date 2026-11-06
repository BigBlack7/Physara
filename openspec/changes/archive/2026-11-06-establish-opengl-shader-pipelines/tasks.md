# Tasks

当前状态: **已完成并归档, B 代码/注释验收及最终 verify 均通过**. 仅覆盖 [D03-T02](../../../../Docs/Developments/D03-D05-FramePipeline.md#d03-t02), 前置 D02 出口和 D03-T01 已通过. 全部 23/23 项完成, 三份主规范已同步, 归档与后置核对成功. 范围及取舍见 [design](design.md), 行为见 [新增能力](specs/opengl-shader-pipelines/spec.md)、[可用范围修订](specs/opengl-device-resources/spec.md) 和 [源扩展白名单修订](specs/shader-build/spec.md).

以下 1–7 由 A 实施/自检; 8.1、8.3 只由维护者调度的独立 B 完成, 8.2 由 A 完成; 9 在 B 验收最终版本后由 A 收尾. 实施中按下方验证预算复用结果, 不为每个任务重新跑同一组检查.

## 1. 固定 GLSL 依赖与构建边界

- [x] 1.1 A 从现有 SPIRV-Cross 固定 commit 精简补入 `spirv_glsl.cpp/.hpp`, 保留许可并更新 ThirdParty 来源、字节摘要与裁剪记录; 完成以两文件来源、实际 include 闭包和无新增上游工程/工具/测试为证据.
- [x] 1.2 A 显式建立 `spirv-cross-glsl` 与 backend PRIVATE 依赖, 保持通用 RHI 独立及第一方警告设置; 用受影响 target 构建与依赖图核对完成 V1, 同步 CMake 说明, 不添加 fixture 或默认样例 target.

## 2. canonical 包到自持 RHI Shader

- [x] 2.1 A 实现 backend Shader 的字节拥有、所选入口/profile 解析与资源/IO 反射, 替换对应 Unsupported 桩; 以 V2 的字节借用结束后仍可读取、代表性非法入口/字节拒绝和完整职责/借用注释确认完成.
- [x] 2.2 A 在 Engine/Shader 增加复用 ReadPackage 的薄 RHI 消费入口与必要 PUBLIC 依赖, 两阶段全成功后才交付; 用匹配包和一次 ABI 不匹配的 V2 检查确认, 同步该入口声明说明与 Docs/Build 的真实消费方式.
- [x] 2.3 A 按维护者 2026-11-06 决定补入 samplerless 源扩展白名单并同步 shader-build delta/声明说明; 通过正式包链编译 texture-only fetch、核对实际 dummy pair 和未制造逻辑 Sampler, 再用一个未声明扩展拒绝检查确认白名单没有泛化, 不改 IR/包版本或 ABI.

## 3. 逻辑布局与不可变 BindingSet

- [x] 3.1 A 实现当前 profile 的常规布局、两种 set 策略、受检 bindingOffsets 与资源类别, 保留公开独立 Texture/Sampler 合同; 通过 V3 的同号跨 set 正常输入及一个策略/布局冲突拒绝确认, 在 RHI/README 明确支持表和扩展限制.
- [x] 3.2 A 实现 BindingSet 的设备归属、资源类型、range/alignment、二维 view/分量映射与依赖强引用, 不在创建时绑定全局 GL 单元; 复用 V3 正常集合及 V5 的外来资源/非法范围代表检查, 以描述、解析记录和释放原始句柄后依赖仍在确认, 注释解释 trackLiveness 的边界.

## 4. 跨阶段活动绑定计划与真实限额

- [x] 4.1 A 查询实际 Shader/绑定/顶点相关 DeviceCaps, 实现跨阶段活动资源合并、布局/块与 IO 一致性及稳定紧凑分配; 用 V3 的正常计划和 V4 的共享活动/不活动声明检查确认, 不按整个项目声明占单元, 注释解释各计数的含义.
- [x] 4.2 A 通过实际 IR 分析采样 pair、跨 set 解析、同纹理双 sampler 及 texture-only dummy pair, 明确唯一 dummy sampler owner 和退休; 同一 V3 程序观察三种 pair 与独立 sampler 参数, 不另建公共 combined 类型或测试专用入口.
- [x] 4.3 A 实现每 stage、combined、物理 binding-index 与 block 大小限额拒绝, 保存可诊断的逻辑到原生映射; 用 V4 的代表性 stage/combined 区别及一组合法边界/紧邻超限确认, 不穷举所有设备限制, 原生查询事实与纯 CPU 限额样例分别记录.

## 5. GLSL 460 lowering 与原生 Program

- [x] 5.1 A 实现基于固定扩展点的第一方 GLSL 适配, 按计划重映射资源, 保留布局/IO, 明确 clip 配置和原生 instance/vertex builtin; 以 V3 的实际生成与 native compile/link、无模拟 base-instance uniform、关键原生反射对照确认, 在 override 旁说明固定命名与选项例外.
- [x] 5.2 A 实现临时 Shader/Program 的 RAII、失败日志定位及全成功发布, 已发布 Program 复用既有预分配删除协议; 用 V5 的可达失败、主要错误保留和旧有效对象不变确认, 同步 RHI/README 的准备成功/执行未实现边界, 未实际触发的 driver 失败明确记录而不伪称运行.

## 6. InputLayout、图形 Pipeline 与原生退休

- [x] 6.1 A 实现 InputLayout 的公开顺序/location 展开、格式/stride/频率与实际 limits 检查, 与活动 VS 输入核对; 用 V3/V5 的属性输入、builtin-only 空输入及一个类型/位置不匹配确认, 同步声明和实现注释.
- [x] 6.2 A 实现基础单样本 GraphicsPipeline 固定状态、阶段和 FramebufferInfo/片元输出兼容检查, 包含旧 framebuffer 重载的归属检查; 用 V5 的不同身份/尺寸但同兼容信息 FBO、一个不兼容目标和 depth-only 准备确认, 不接入动态图形状态或 Draw.
- [x] 6.3 A 完成 Shader/Layout/Set/Pipeline 强引用链、Program/view/dummy sampler owner 和 worker Release/owner drain; 用 V6 真实退休与正常 Host 关闭确认, 不新增 final Release 分配、缓存引用环或测试 friend, 在完整涉及文件中解释寿命与退出顺序.

## 7. 集成自检与交接

- [x] 7.1 A 按 V1/V7 完成一次所需正式 Debug/Release 集成构建、最小真实 GL 准备检查及文档/依赖核对; 汇总实际运行与可复用证据, 确认普通构建不依赖本地测试, 所有检查输入/脚本仅在 build/d03-t02, 不扩展为旧全量矩阵.
- [x] 7.2 A 在本 tasks 记录版本、相关 dirty/untracked 输入、检查命令与结果/限制, 并向 Docs/CodeReview 填写本轮实际交接; 开发计划仅改为待独立审查并引用 change, 8.1/8.3 保持未勾选, 不自行创建/联系 B 会话或宣称最终验收.

## 8. 独立审查、注释与整改闭环

- [x] 8.1 B 独立审查实际交接版本及直接消费者, 通读本轮涉及的完整第一方文件, 按紧凑 Doxygen/实现分层补齐职责、接口合同及流程说明; 在当前问题单记录稳定 CR、覆盖、验证范围和代码/注释各自结论, 由 B 本人确认本项.
- [x] 8.2 A 按稳定 CR 整改并运行受影响的最小检查, 保留 B 注释并同步受修改影响的说明; 在问题单只勾 A 项并记录实际证据. 无正式发现时仅在 B 明确结论后记不适用, 不代签 B 验收.
- [x] 8.3 B 复核最终输入、全部正式问题关闭或有依据撤回、匹配证据及完整注释覆盖, 对正确性与可读性分别验收; 不通过继续同轮往返, 由 B 本人填写最终版本/结论并确认本项.

## 9. 最终 verify、同步与归档

- [x] 9.1 A 在 8.3 通过后运行原生 openspec-verify-change, 以最终输入核对 requirements/scenarios/design/tasks 和受影响构建/原生证据, 完成 strict validation; 有代码或构建输入变化先交 B 复核, 不能用任务计数代替验收.
- [x] 9.2 A 将 CR 修正/复核摘要、B 代码与注释结论、最终版本、命令/结果及未覆盖范围留存到本 tasks, 核对主规范同步内容与文档链接; 开发计划只记录 D03-T02 已验收待归档及引用.
- [x] 9.3 A 按原生 sync/archive 流程同步新能力与可用范围修订并归档, 成功后才标记 D03-T02 完成并引用归档, 按模板重置问题单; 核对 archive、主规范、下一工作包入口与最终 strict validation, 归档失败保留现场, 本项实际完成后才勾选.

<a id="verification-budget"></a>
## 验证预算与停止条件

下表是实施的最小充分检查, 不是本轮已经运行的结果. 每个 spec 场景可由组合消费者、直接观察或合同/代码检查证明, 不机械扩展成逐场景测试. 同一输入与环境已匹配的有效结果直接引用, 输入改变才重跑受影响部分.

| ID / 影响 | 具体风险 | 最小检查与独立预期 | 停止条件 |
|---|---|---|---|
| V1 / L2 构建依赖 | 新 GLSL 依赖拉入业务/上游工程或导致普通工程依赖 fixture | 在支持的 VS2022/MSVC x64 环境核对固定来源, 构建受影响 targets; 最终用一个新 binary tree 构建正式 Debug/Release. 通用 RHI 独立消费者仅在其输入变化时重跑, 否则复用匹配证据并核对依赖图 | 当前必要配置成功, 无源码生成物、配置下载或测试目标. 不新增其它 generator/SDK 矩阵 |
| V2 / L2 模块边界 | 包 ABI 未核对或 Shader 借用失效, 裸模块接受错误执行模型 | 复用一个当前生成的匹配包, 释放包存储后核对 Shader 字节和阶段; 一次 required ABI 不匹配应在 RHI 创建前失败, 一次直接入口/长度不合法应明确拒绝 | 覆盖文件/ABI 与裸模块两个不同边界即可; 不重测 D03-T01 全部 JSON/include/OOM 用例 |
| V3 / L2+L3 正常准备 | 逻辑 set 混淆、pair 错配、GL 布局改变或只有文本成功 | 一个真实 VS/FS 程序合并使用常量、只读 record、两个 set 的同号 slot、同纹理两个 sampler、跨 stage pair 与独立 texture-only fetch. 手工预期例如两个 UBO 物理点但三个 stage-block 贡献, 三个 texture units 但四个 stage-sampler 贡献. 以实际 Program link 状态、native 活动 block/member/sampler 查询与正常准备记录对照; 使用 builtin 和属性输入 | 正常链与每个独立 pair 路径有证据. 只观察实际活动字段; 不穷举所有布局组合或新增另一套参考编译器 |
| V4 / L2 计划限制 | 活动/不活动及 per-stage/combined 计数混淆, 共享单元掩盖超限 | 复用 V3 的逻辑资源, 用生产计划算法的值输入检查不活动声明不占单元、顺序/descriptor-set 策略与代表性冲突, 并选一组可接受限额及紧邻超限; 原生阶段限额为零时按真实结果拒绝该活动用法 | 每项不同计数风险有可区分预期. 合成 DeviceCaps 只证明 CPU 判断, 不冒充实际设备支持; 不逐 family/字段/容量扫描 |
| V5 / L2+L3 拒绝与兼容 | 外来/错型/越界资源被接受, 目标与 IO 不匹配, 失败发布半成品 | 复用 V3 资源检查一个外来/错误资源和一个非法显式范围, 两个不同尺寸同格式 FBO 兼容、一个不兼容目标; 用 builtin-only/depth-only 准备补足空属性/缺 PS 的实际路径. 能自然触发的 compile/link/准备失败验证无部分发布, 主要错误与旧有效对象保留; 无自然触发的 driver 失败分支核对 RAII/发布代码并记未实测 | 已改变的边界各有来源明确的证据. 不新增逐分配故障、GL 调用扫描或产品故障开关, 不为尚未实现的 SetGraphicsState 建测试 |
| V6 / L3 新对象寿命 | 原始 handle 释放后悬空、worker 发 GL 或 device/dummy 引用环 | 复用准备对象释放调用方原始依赖, 核对余下 owner; worker 释放最后的未执行 Pipeline/Set, owner 回收观察 native Program/view/sampler 退休及正常 Host 退出. CPU 无分配退休沿已有队列合同和改动审查核对 | 新对象的完整依赖与退休链通过. 不重建旧资源/上传压力矩阵, 不把未执行对象退休当成 Draw 在途证明 |
| V7 / L0+范围 | 文档与真正能力不一致, 开发材料进入正式工程 | 检查 touched 文档/链接、来源、命名例外和完整文件注释; 核对 Draw/Dispatch 等未实现入口仍明确拒绝, 对新支持表有一个实际调用对照 | 支持/排除、真实环境和证据一致, 未运行项保留, B 审查记录完整. 不为文档新增行为测试或性能测量 |

同一 fixture 可分组调用这些检查, 无需七个临时工程. 开发材料统一在 `build/d03-t02/`, 不进版本控制/正式目标, 删除后不自动恢复. 正常编译/link/清理在 Debug/Release 直接覆盖, 纯 L0/CPU 输入同源结果不为了配置名重复证明. 场景出现具体缺口或失败时才追加对应检查, 每个新增用例注明它独自捕获的缺陷.

<a id="evidence"></a>
## 执行与证据记录

以下先保留 2026-11-05 的规划记录, 再记录 2026-11-06 的 A 实施/自检. A 证据不能代替独立 B 验收或最终 verify.

2026-11-05 规划自检: 原生 `openspec status --change establish-opengl-shader-pipelines --json` 确认 proposal/specs/design/tasks 四类产物均已存在且依赖完整; `openspec validate establish-opengl-shader-pipelines --strict --json` 通过, 无 issue. 两份 delta 共新增 9 条要求/26 个场景、修改 1 条要求/4 个场景, 修改项原有 3 个场景完整保留. 本地 L0 文档检查覆盖 7 份文档的 140 处本地链接/锚点, 无错误; D03-T03 起的后续工作包内容未改, 22 项实施/验收任务完成数为零. 这些结果只证明规划一致性, 不证明 GL 实现或 B 审查通过.

### 2026-11-06 A 实施与输入

维护者批准为 texture-only fetch 增加唯一的 samplerless 源扩展. proposal/design/tasks 与 shader-build delta 已同步, SourceGraph 只接受 GOOGLE include 和 EXT samplerless 的 enable/require; 没有开放其它扩展, 没有修改 canonical Vulkan 1.1/SPIR-V 1.3、资源类型、包版本或数据/绑定 ABI. 当前三份 delta 共新增 9 条要求/26 个场景、修改 2 条要求/7 个场景, 两个修改项的既有场景全部保留. 新增 2.3 后任务总数为 23.

交接基准为 HEAD `2ae2f10a9694f3d045717049b1938160ad8c466b`. 没有暂存、提交或推送. 检查的是该基准加当前未暂存/未跟踪输入, 不能只按 commit 复现:

- 已修改实现/构建: `Engine/Shader/{CMakeLists.txt,SourceGraph.cpp}`, `RHI/OpenGL/{Backend.hpp,CMakeLists.txt,Device.cpp,Submission.cpp,Unsupported.cpp}`, `ThirdParty/ShaderToolchain/CMakeLists.txt`.
- 新增第一方: `Engine/Shader/RhiProgram.hpp/.cpp`, `RHI/OpenGL/{ShaderCaps,Shader,Bindings,InputLayout}.hpp/.cpp`, `RHI/OpenGL/Pipeline.hpp`, `RHI/OpenGL/{BindingPlan,Lowering,Pipeline}.cpp`. 合计涉及 19 份完整第一方 C++ 文件和 3 份 CMake, 由 B 按整个文件核对注释.
- 新增上游: `ThirdParty/spirv-cross/spirv_glsl.cpp/.hpp`, 原样取自已有固定 commit `f11ba9f0b21ba8fc15153d50a2a1ae31ab1cf8f7`. 两文件 SHA-256 分别为 `7f211747e8e879700119baa7bdd05dc2103e70b5aec297bf751ecc5a3ba3e1c5` / `c3580710895edfabda19e4e70efa818b42a71460451ca024b747ebfd335ca43b`. 只补 include 闭包, 没有上游工程、CLI、生成器或测试.
- 文档: `Docs/Build.md`, `Docs/CodeReview.md`, 两份当前开发计划, `Engine/Shader/Interface/README.md`, `RHI/README.md`, `ThirdParty/README.md`, 及本 change 的 6 份文档. 保留问题单原有的 build 审查排除说明. Architecture、主 specs、历史 archive、generic RHI 和 D03-T03 起的计划内容未变.

交付链为包读取/ABI 校验 → 自持 canonical Shader → 逻辑 Layout/Set → 活动资源/pair 计划 → GLSL 460 → 原生 compile/link → 不可变单样本 Pipeline. BindingSet 已实际消费 constant-buffer ranges, 因而开放对应 Feature; 部分范围同时服从公共 256-byte 约束与原生 offset alignment. `VertexBuffer` 格式能力只对当前属性路径支持的格式返回. 原生 view/Program/dummy sampler 沿既有预分配删除队列退休, 不新增公共 combined 类型、测试钩子或第二套执行链.

### 当前输入和产物身份

生产字节在以下最终构建和运行期间未变. `build/d03-t02/current-inputs.json` 保留逐文件字节数/摘要和工作树状态; `CaptureEvidence.py` 仅为本地记录脚本. 聚合规则是按路径排序, 对每项的 UTF-8 `path + NUL + sha256 + LF` 串联后取 SHA-256:

- 仓库非文档输入共 1363 份, 排除 .agents/.codex/Docs/openspec 和 Markdown, 聚合 `43275620ea993be07789b87d24567f90c4e175a60e0b2d0e26b665a7bd105a6c`.
- 实际 fixture 的 Schema/Main/Builtin/RejectedExtension JSON 和源、Checks.cpp、CMakeLists 共 10 份, 聚合 `14bd14df379f015474d7de5b7522144f9174e64b8e8e460f2e6e7004d6c34e3a`. fixture 只在 `build/d03-t02/fixture`, 不进入审查源码范围或正式默认目标.
- 两配置生成接口 ABI 均为 `92104038f65fba4fc662b5900c51d0807bbeab8446ca17949186c779aabddcf1`, Manifest SHA-256 均为 `335d278300c8ef4d7205d28502d3acee5c385a5ca4167f255815a98071043d10`.

| 产物 / SHA-256 | Debug | Release |
|---|---|---|
| 正式 Physara.exe, 只构建未运行 | `44ae526790d3c9ba4f51af755218ca6ef4c228bf106baeb53ddada9689e91c9a` | `79ea0612cb4eef8ebbbddedcc1391b36dd1f58baae5889769f53679a6a7b865d` |
| 实际运行的 D03T02Checks.exe | `e0d0c8b93244758e4f340230f9ea0a85386354e1ad592c26ad56726bbbb8bf6a` | `4cfa9ddcffc679d43c0c981cecef6480f03df3621b9bf283008d301edf56195f` |
| Main.pshader | `4dbb97df200edbb1c70736d89382545b9e1c64501d574dd91a17b815c116dd72` | `6ea79f0a68d6f8fccae70f2ce093f6e0f98236e2a82d7776c772d28b00bcf4ba` |
| Builtin.pshader | `45b2393dda3aefbd61db4d659d1e440eac0567ced34487aa8466f5ba113fea3d` | `cb28df94854d638392ff7dc0099f40efc60fa3a1e233828173383d7b433a07ae` |

本地材料可随 build 删除, 上述输入范围、预期、命令、结果和限制仍在本 tasks 保留. 跨工作树审查应核对实际 dirty/untracked 字节; 如需复跑, 另行提供当前本地 fixture, 不在正式工程中自动恢复.

### 实际命令与观察

环境为 Windows x64 / VS2022 / CMake 3.30.0-rc4 / MSVC 19.44.35228.0 / SDK 10.0.26100.0. 两个 binary tree 为本包新建, native 的 source 是仓库根, checks 的 source 是本地 fixture 并通过 add_subdirectory 消费正式 targets. 使用原有 /W4 /WX、Release /O2 与 Debug 检查; 没有降低警告、优化或迭代器检查. GL 为 Intel / Intel(R) RaptorLake-S Mobile Graphics Controller / OpenGL 4.6.0 - Build 32.0.101.6129, GLSL 4.60.

| 检查 | 当前输入对应的命令与结果 |
|---|---|
| 正式默认工程 V1/V7 | `cmake --build build/d03-t02/native --config Debug --parallel` 与 Release 均退出 0. 当前日志为 formal-debug-current.log、formal-debug-current-confirm.log、formal-release-current.log. 普通工程没有本地 fixture 目标/输入, ShaderBuild 随正式工程构建, 默认不生成样例包 |
| 本地消费者构建 | `cmake --build build/d03-t02/checks --config Debug --target D03T02Checks --parallel` 退出 0; Release 同命令在保留原选项的重试后退出 0. 最终成功日志为 checks-debug-current-build.log / checks-release-current-build-retry4.log |
| 真实原生运行 V2/V3/V5/V6 | `build/d03-t02/checks/Debug/D03T02Checks.exe` 与 Release 均退出 0; 日志 checks-debug-current-run.log / checks-release-current-run.log. 运行前对应构建已成功; 没有以旧 exe 代替失败构建 |
| 白名单独立拒绝 2.3 | `build/d03-t02/checks/bin/Debug/PhysaraShaderBuild.exe build build/d03-t02/fixture/RejectedExtension.json build/d03-t02/fixture build/d03-t02/checks/Generated/Shader/Debug/Fixture build/d03-t02/rejected.pshader build/d03-t02/rejected.d build/d03-t02/rejected.stamp` 实际退出 1, 定位 fragment/main、Source/RejectedExtension.frag:2、extension is outside GraphicsBaselineV1; 未发布 rejected.pshader |
| L0/V7 | `python build/d03-t02/planning/CheckDocuments.py` 核对 13 份文档、181 处本地链接/锚点、delta 保留场景与后续范围, 无错误; `git -c core.safecrlf=false diff --check` 通过. `openspec validate establish-opengl-shader-pipelines --strict --json` 退出 0, valid=true、issues 为空, root 为当前仓库; 这是结构自检, 不是最终 native verify |

V2 实测匹配包完整消费、ABI 不匹配拒绝、包存储结束后 Shader 字节仍拥有、错误裸入口/字节长度拒绝. V3 正常链同时包含 UBO、只读 record SSBO、跨 set 同号 slot/独立采样器、跨 stage pair、同纹理双 sampler, 以及源中真正的 samplerless fetch. 计划明确观察到 dummy pair, 没有为 fetch 增加逻辑 Sampler; 设备唯一 dummy 在正常关闭时随 owner 回收. 手工预期和实际一致: 2 个物理 UBO 点/3 个 stage-block 贡献, 1 个 SSBO, 3 个 texture units/4 个 stage-sampler 贡献. Native Program 实际 link 成功, 独立查询 UBO offset 0/64/80、matrix/array stride 16 和列主序, SSBO offset 0/16、record stride 32; sampler 参数分别保持 nearest/linear. GLSL 使用原生 gl_VertexID、gl_InstanceID + gl_BaseInstance, 未出现模拟 base-instance uniform.

V4 使用同一生产 CPU 计划算法和正常 IR, 证明不活动布局 CB127 不占单元、顺序/descriptor-set 策略等效而混合策略拒绝; combined UBO 限额 3 接受、2 拒绝, 片元限额 1 独立拒绝. 这些是合成 caps 的 CPU 判断, 不是设备限额数值或其它 GPU 证据; 正常 Program 则用 BuildShaderCaps 的真实查询值检查并通过, 没有逐字段/容量扫描.

V5 实测外来 device、错型资源、非法部分 buffer range、VS 属性数值类别/目标类别不匹配的拒绝; 合法部分常量范围通过 Validation 到 raw backend. 失败返回空对象并保留原有 Program; 不同身份/尺寸的同兼容信息 FBO 和旧重载可用, builtin-only 输入与无 PS 的 depth-only Pipeline 准备成功. 上传列表的 SetGraphicsState 仍明确 Unsupported. 最初本地检查曾错误要求空上传 Execute 返回 0, 以及误以原生 16-byte alignment 代替公共 256-byte 范围约束; 已修正检查输入/预期, 没有改变既有空提交合同.

V6 先释放调用方依赖再由 worker 释放最后的未执行 Pipeline/Set, owner GC 前对象仍在, 后续真实查询确认 Program、view 和普通 sampler 已退休; dummy 继续由 Device 保活并在 Host 正常关闭时回收. 无 final Release 新增删除节点分配、无 worker GL 调用的结论来自预分配队列实现检查与原生退休观察, 不代表在途 Draw 测试.

复用而未重跑: D03-T01 的 schema/包格式/JSON/include/OOM 全套用例, D02 的通用 Device/资源/上传退休矩阵和 generic RHI 独立工程. 本包只核对其未变输入/既有合同与新增直接消费者, 不宣称这些旧全量用例在本轮重新通过. 新增 GLSL 两文件的依赖/许可/原样字节和命名 override 例外由源与 CMake 检查确认.

工具链限制: Debug 构建曾在 glslang limits / STL algorithm 出现 C1001, 原日志 checks-debug-build.log / checks-debug-explicit-fetch-build-3.log 与原选项重试日志均保留. 当前 Release 消费者构建又先后在 STL xlocmon、xcharconv_ryu、algorithm、xutility 出现 C1001, 保留 checks-release-current-build.log 和 retry/retry2/retry3; 第四次原样重试 retry4 成功. 根因未确定, 不把偶发失败改写成全程无错误, 不据此弱化编译选项. 早期未通过/过渡 fixture 的日志不作最终行为证据.

未实测的 native driver compile/link 错误、自然 OOM、context loss 和抛分配异常路径只检查 RAII、诊断与发布顺序, 没有增加故障开关或逐分配注入. 不承诺 STL/第三方 noexcept 致命 OOM 能在同进程恢复. 当前合法输入已覆盖 Debug/Release 正常 compile/link/清理.

原生创建/link 之外的像素、非零实例/索引实际数值、upper-left 区域 depth clear、图形状态发射与 barrier、Draw 在途引用、提交消费、呈现和跨后端语义均未在本 change 规划为已通过, 继续留在相应工作包.

<a id="review-retention"></a>
## 审查与收尾留存

第 1 轮整改的最小回归: 复用现有 VS/FS 和真实 Host 的 Validation Device, 对照顺序/显式布局的同阶段跨 set b0 映射, 保留重复有效 set/混合策略拒绝(CR-001); 用一份两层两 mip 的 2DArray 显式绑定 mip1/layer1 的二维视图并查询 native view, 拒绝多层二维请求(CR-002); 两个真实 Device 的外来 framebuffer 经 Validation 拒绝, 同设备不同尺寸兼容 FBO 接受(CR-003). 一个受控 CPU 诊断输入覆盖 5KB 日志、先于日志的双阶段身份、生成文本跨片段重组与实际行号(CR-004); native driver 失败仍记未实测. 正式 Debug/Release 与同一消费者检查使用含 B 注释的当前输入, 不新增 Draw、逐维度或旧全量矩阵.

2026-11-06 首次 A 实施/自检已交接到 [当前问题单](../../../../Docs/CodeReview.md). 当时原生 `openspec instructions apply --change establish-opengl-shader-pipelines --json` 确认 spec-driven、17/23 完成、剩余 6 项; ready 只表示 CLI 可继续, 不代表 B 验收. 首次交接时 B 尚未开始, 没有代签 B 项或把 A 检查当最终验收. 后续 B 初审及本轮整改记录见下文.

首次 A 交接时正式 CR、整改/复核仍待开始, 当时 8.1/8.2/8.3 均未勾选. 后续进展见下文; 只有 B 验收最终版本后才执行最终 verify、主规范同步或归档, 9.1/9.2/9.3 保持未勾选; 问题单仅在留存且归档成功后重置.

<a id="review-r1-evidence"></a>
### 2026-11-06 B 初审与 A 第 1 轮整改

本节保留截至 A 整改交接时的输入、命令和观察, 其中“待 B”是当时状态; 当前 B 结论及注释后的验收输入见 [本轮 B 复核](#review-r1-b-check).

B 已在问题单记录 CR-001(P1)、CR-002/003/004(P2), 以及 N-001 文档建议, 初审结论未通过; 已补齐 20 份 C++ 与 3 份 CMake 的注释, 未构建/运行或读取 build. A 接收的 23 文件原始字节聚合 `5c194f4f865aa70ebcbb71b142e21117b8a3ea037c97aea322af9de77f513e12` 与 B 交接一致, 先留存接收版本后整改. 21 份 B 注释文件保持字节原样, Pipeline 两文件的说明随 CR-004 同步; 本轮新涉及两份 Validation 文件, 待 B 完整核对. 不代勾 B 的 8.1/8.3 或问题单复核项.

| 发现 | A 修正与实际证据 |
|---|---|
| CR-001 | Pipeline 摘要在 GL 顺序模式用 layoutIndex 解释 set, 显式模式保留 registerSpace; 单独 BindingSet 的摘要继续从自身布局描述比较, D3D11 忽略空间及其它 API 语义未改. 复用同一 canonical VS/FS, 实际 Host Validation 接受顺序/显式 Pipeline, FS 同时活动 set0/b0 与 set3/b0, 两阶段 lowering 映射完全一致. 重复有效 set/混合策略仍拒绝 |
| CR-002 | 仅扩充 OpenGL Texture_SRV 对单样本 array/cube storage 的显式二维单层维度兼容, 原范围/format/用途/native 检查保留. 两层两 mip 的 2DArray 经 Validation 创建 Texture2D view, 原生查询 min-layer=1、min-level=1、num-layers=1、width=4; 两层二维请求拒绝. cube/cube-array 的相同规则仅核对合同/源码, 未做维度矩阵或像素读取 |
| CR-003 | 两个图形入口共用 ValidateGraphicsPipelineDesc, 保留 framebuffer 对象一次委托到原生重载, owner 早退先于创建 Program. 实际 Host Validation 在 A 上传第二份真实 Device 的 FBO 返回空并报告 framebuffer ownership, 同设备 8/16 宽度兼容 FBO 均接受, 旧 Program 有效. 两份 Device 使用同一当前 GL context 的不同 owner; 未宣称跨 context 隔离. 未创建新 Program 同时由早退调用链核对, 没有测试计数器/registry |
| CR-004 | 生成字符串保留到本次 link 返回, 通过生产 ReportProgramFailure 先输出所有参与阶段的 entry/source, 后输出有界日志和带生成 byte/line/column 的完整 GLSL 分片. 5KB 受控日志确认身份在前、log 截断标记及每条消息 <=511 bytes; 源跨 256-byte 片段精确重组, driver 所指生成第 3 行对应实际文本. 该项是生产 CPU 格式化路径检查, compile/link 失败分支的数据流/RAII 另由代码核对; 没有伪称真实 driver 失败已运行 |
| N-001 | ThirdParty 说明收敛为实际五项静态 target, 明确 SPIRV-Cross core/GLSL backend. 与五项 add_library 及实际消费者核对, 上游源码、许可和固定版本未改 |

功能差异仅为 `RHI/Validation/ValidationDevice.cpp`、`ValidationBackend.hpp`、`RHI/OpenGL/Pipeline.cpp`、`Pipeline.hpp` 四份文件; 本轮没有新增正式源文件. 同步 RHI/ThirdParty README、change design/tasks、问题单与开发计划状态. 公共 RHI、主规范、历史归档和 Architecture 未改; D03-T03 起的内容未改.

本轮精确输入仍为 HEAD `2ae2f10a9694f3d045717049b1938160ad8c466b` 加全部相关 dirty/untracked 字节, 未暂存/提交. 聚合规则沿上述 path/NUL/sha256/LF:

- 当前 25 文件审查范围(原 B 23 份加 Validation 两份): `3a4a6ae6700d3c19623dffaf1ab7408d190db4013683000b7c84e07f9281a1fe`.
- 当前 1363 份生产非文档输入: `924d9508291635c61ce0adb2c8750c97bdbb58d098268aaea8d2bb5c5a3995d8`. 逐文件清单在本地 `build/d03-t02/review-r1/current-inputs.json` / `corrected-scope.json`, B 接收快照与差异分别在 received / corrections.diff; 不作为永久工程入口.
- 当前同一 fixture 的 10 份实际输入: `272ba920d5e3e83e6b859c6a57ff637508d5c1281edfa83a66034af3cea0728a`. 新回归仅追加到 build 中的现有 Checks.cpp, 没有正式测试 target/宏/入口.
- 生成接口 ABI 与 Manifest 摘要仍与首次 A 记录相同. B 注释进入 compiler fingerprint 后, Main/Builtin 均由正式 ShaderBuild 重新生成, 未复用旧包作为当前输入证据.

| 当前产物 / SHA-256 | Debug | Release |
|---|---|---|
| 实际运行的 D03T02Checks.exe | `2ac20a3fefeb91b3cb89afa897405c61edf486d2938a44571b7bdaeef739aef7` | `b08ecef13ff64ebaeb41cc8f23c9ef0494fcaaf770aab58452fa473d70dd7921` |
| Main.pshader | `12d13831e553e7c9fa187265a9400d5363a796f0da930787adce2bc151598a26` | `23c5d8154e0bb19ab964bfe1f3308d32b8e82e81c7fef281d8ab82ba410973ee` |
| Builtin.pshader | `7ce770f9ec3f89014a4815e01040c30490bd142140ecf51e5b05303c44d9a74e` | `fedc5a99057d48798516e890ba35faabf9306c6e49ba74522ae7ac27ca988f32` |
| 正式 Physara.exe, 只构建未运行 | `fed4933e77a7d7c7f9cf43cf3234127079f2b3d7395b1dbddcbba2d97d1f5921` | `b71f990efff85eef72ee4349b1c029c8bd62c9ecaa725a2f45f53c27b5fd13ed` |

当前命令均在仓库根执行, 日志按工作包放在 `build/d03-t02/review-r1/`:

- `cmake --build build/d03-t02/native --config Debug --parallel` 与 Release 均退出 0, 最终日志 formal-debug-current.log / formal-release-current.log.
- `cmake --build build/d03-t02/checks --config Debug --target D03T02Checks --parallel` 与 Release 均退出 0, 最终日志 checks-debug-build-3.log / checks-release-build-2.log.
- `build/d03-t02/checks/Debug/D03T02Checks.exe` 与 Release 均退出 0, 最终日志 checks-debug-run-2.log / checks-release-run.log. 同时通过原 V2/V3/V4/V5/V6 与四项新增回归, 包括实际 link/反射、strong refs、worker 退休和 Host 正常关闭; 没有把 CR-004 CPU 输入称作 GL 失败复现.
- 环境仍为 VS2022/MSVC 19.44.35228.0、SDK 10.0.26100.0、Intel RaptorLake-S Mobile / GL 4.6.0 Build 32.0.101.6129. 保留 /W4 /WX、Debug 检查、Release /O2. 仅保留重载的一次兼容委托局部抑制 C4996, 原因/范围见 design 决策 7 与调用旁注释.

失败也留存: 首次 checks-debug-build.log / formal-release-build.log 因这一次 deprecated 委托触发 C4996/C2220, 后按局部兼容协议处理; checks-debug-run.log 虽通过四项新增回归, 仍在原 BindingSet 检查退出 1. 原因是 A 改摘要参数后遗漏另一个调用点, 修正后重建全部受影响配置并得到上述完整成功结果, 不以此前局部通过交接. 本轮未遇到 C1001, 首次实施的 C1001 历史及根因未定限制仍保留. 没有运行未成功构建或输入不匹配的旧 exe.

旧 D02/D03-T01 的未变完整矩阵未重跑, 不由本轮结果重新宣布通过. 当前公共合同与 CMake 依赖图未变, generic RHI 的两份实现输入已在正式/消费者两配置的 PhysaraRHI target 中重建; 未另建独立配置矩阵. 真实 driver compile/link 失败、自然 OOM/context loss、Draw/像素/在途与呈现仍未实测, 不扩展本包验收. 待 B 对上述最终输入复核代码与完整注释, 再进行最终 verify、同步及归档; A 本轮不执行这些收尾.

本轮 L0 收尾: `python build/d03-t02/planning/CheckDocuments.py` 核对 13 份文档/201 处本地链接与锚点, 无错误, 主规范/归档/Architecture/公共 RHI 合同及后续工作包内容未改; `git -c core.safecrlf=false diff --check` 通过. `openspec validate establish-opengl-shader-pipelines --strict --json` 退出 0, valid=true、issues 为空, root 为当前仓库. 原生 apply instructions 确认 18/23、剩余 5 项、ready; 这是当前任务计数/结构自检, 不是 B 最终验收或最终 verify. 结束前核对生产输入、fixture 与 14 份产物 SHA-256 均未改变, 问题单仅 4 个 A 项勾选、0 个 B 项勾选.

<a id="review-r1-b-check"></a>
### 2026-11-06 B 第 1 轮整改复核与注释验收

维护者再次交由原独立 B 会话复核. B 已确认代码正确性与注释可读性均通过, 关闭 CR-001 至 CR-004, 本次无新增正式问题; N-001 的五项静态 target/core+GLSL 文档说明已与 CMake 核对. 由 B 本人在本次确认 8.1/8.3, 保留 A 的 8.2; 9.1/9.2/9.3 尚未执行, 不以本次结构检查代替最终 verify.

| CR | B 代码复核及证据范围 |
|---|---|
| CR-001 | 确认摘要显式接收有效空间, GL 顺序 Pipeline 用布局位置, 显式模式用 registerSpace; 已核对单独 BindingSet 的另一个调用点及真正重复/混合策略拒绝, 其它 API 规则保持. A 的 Host Validation 同阶段跨 set b0 两配置记录与该调用链一致 |
| CR-002 | 确认仅为 OpenGL Texture_SRV 的显式二维单层 array/cube storage 放行继续委托, 原范围/用途/format 与 backend 精确视图检查保留. A 的 mip1/layer1 native 参数及多层拒绝记录匹配源码; cube 族只有合同/代码核对 |
| CR-003 | 确认两重载共用描述校验, 实际 framebuffer 一次委托到原生对象重载, Owner 拒绝先于 Program 创建; 单次 C4996 抑制配对, 公共属性/target 门槛未改. A 记录的两个 Device 共用同一 context, 只证明对象归属拒绝和同设备兼容, 不证明跨 context 隔离 |
| CR-004 | 确认双阶段文本在工厂栈上活到 link 返回, compile/link 失败均同步报告阶段身份、有界日志和完整生成 GLSL 分片; 不经通用 Report 再截断, 不新增缓存. A 的长日志/重组结果是生产格式化函数的 CPU 证据, native 失败数据流/RAII 由 B 源码核对, driver 失败仍未实测 |

验收版本为分支 `v2.0`, HEAD `2ae2f10a9694f3d045717049b1938160ad8c466b` 加当前相关 dirty/untracked 文件. B 按实际原始字节独立重算 25 文件范围, 接收时与 A 的 `3a4a6ae6700d3c19623dffaf1ab7408d190db4013683000b7c84e07f9281a1fe` 一致. 本次补注释后的同范围摘要为 **`61c02208dbe0708564efad12adc154764f03c8776b8e4a45c4f3016cb6f12ca9`**, 规则仍为排序后 UTF-8 `path + NUL + sha256 + LF` 聚合. 该摘要覆盖 22 份 C++ 与 3 份 CMake, 不含文档/build, 也不是上节 A 自检时的 1363 文件全生产摘要.

完整文件覆盖沿初审的 20 份 C++ / 3 份 CMake, 本次增加通读 `RHI/Validation/ValidationBackend.hpp`、`ValidationDevice.cpp`, 重读 Pipeline.hpp/.cpp 及修复相关的公共 RHI、Host 和原生 view/归属消费者. B 仅在这 4 份 Pipeline/Validation 文件中补注释, 其余 21 份与 A 接收输入逐字节相同; 对 25 份文件比较确认 C++ 非注释 token/预处理指令未变, CMake 字节未变. 注释覆盖有效空间、一次委托、CPU 异常边界、同步诊断借用及生成行/字节列语义, 未改行为、公共 API、上游文件或构建规则.

B 实际执行源码/合同/实际 diff、输入摘要和注释等价核对, 均通过. `git -c core.safecrlf=false diff --check` 通过, 14 份文档/204 处本地链接与锚点无错误; `openspec validate establish-opengl-shader-pipelines --strict --json` 退出 0, valid=true、issues 为空, root 为当前仓库. 任务核对为 20/23, 剩余三项均属 A 最终收尾. 两配置构建、GL 原生准备/反射/退休及 CR-001 至 CR-004 回归采用上节 A 已记录的命令、产物身份和观察; B 未复跑, 未读取 build 中的脚本/fixture/原始日志, 不将 A 运行记录写成本次 B 实测. 本次只有经等价核对的注释差异, 该复用不增加任何未覆盖的 GPU 结论.

最终 verify 由 A 对 B 验收输入及其可复用/需更新证据按既有预算完成, 再留存、同步和归档; 再次改变行为或构建输入先交 B 复核实际影响. driver compile/link 错误、自然 OOM/context loss、旧 D02/D03-T01 完整矩阵、Draw/像素/在途与呈现仍按上节限定, C1001 历史保留. 主规范、Architecture、历史 archive 和 D03-T03 起的内容未改, 当前问题单继续保留至收尾成功.

<a id="final-verification"></a>
## 最终 verify 与收尾

2026-11-06 在 B 完成 8.1/8.3 且四项 CR 均复核通过后, A 执行仓库原生 openspec-verify-change 工作流: 以 CLI 的 status/apply contextFiles 为输入, 核对全部 proposal/design/tasks 和三份 delta, 再对实际代码、直接消费者及既有证据检查完整性、正确性和一致性. 此工作流不是虚构的 CLI verify 子命令. 开始时 20/23 完成, 仅 9.1/9.2/9.3 三个收尾项未完; 按任务规则将其列为待完成的流程项, 没有将其误报为缺失实现或提前宣称所有任务已完成. 本次核对完成后确认 9.1, 其余收尾依次完成后在本节追加最终状态.

| 维度 | 核对结果 |
|---|---|
| 完整性 | 1–8 的实现、检查与独立审查全部完成; 9 条新增与 2 条修改要求均有实现, 三份 delta 共 33 个场景均已核对处理和证据方式, 无遗漏行为. 开始时三项流程任务待收尾 |
| 正确性 | 11/11 要求与 33/33 场景已有实现/合同、组合运行或受控 CPU 证据对应, 见下表. 未把源码审查当原生运行, native 失败分支按预算由 CPU 诊断与 RAII/数据流核对, 未声称自然 driver 失败实测 |
| 一致性 | 11 项设计决定与代码依赖、profile、ABI、活动预算、GLSL 适配、所有权/退休及验证边界一致. 当前检查无新增代码问题、未覆盖场景警告或模式建议. 仅将 Migration Plan 的旧“两份 delta”文字修正为已批准的三份, 不改变设计或行为 |

| 要求 / 场景数 | 当前实现位置与核对依据 |
|---|---|
| Consume canonical modules / 3 | `Engine/Shader/RhiProgram.cpp:11` 先 ReadPackage/ABI 再创建完整阶段; `RHI/OpenGL/Shader.cpp:190` 复制字节并反射, `:259` 的裸边界拒绝. V2 的自持字节、ABI、入口/长度证据, 包完整性/profile 沿已有包合同 |
| Preserve logical layout identities / 3 | `RHI/OpenGL/Bindings.cpp:36` 和 `BindingPlan.cpp:36` 的策略/类别; Validation 的有效 set 摘要. V3/V4 与 CR-001 的真实 Host 顺序/显式布局、重复有效 set/混合策略拒绝 |
| Create immutable binding sets / 3 | `RHI/OpenGL/Bindings.cpp:128` / `:176` 的归属、准确 range/view 与强引用. V5/V6、CR-002 的实际 mip1/layer1 和多层拒绝; format/记录粒度/limits 的分支由源核对, 不扩大成逐字段矩阵 |
| Allocate a deterministic compact plan / 4 | `RHI/OpenGL/BindingPlan.cpp:36` 合并/排序/分配, `:158` 起区分物理、stage、combined; ShaderCaps 实际查询. V3 的 2 UBO/3 stage-block、3 units/4 stage-sampler, V4 的不活动声明和 CPU 限额边界, 布局/IO 冲突由组合检查与源核对 |
| Combine sampling pairs / 3 | Shader 的 dummy/combined IR 分析、计划的双方 key 与 `Lowering.cpp` 映射. V3 同纹理双 sampler、跨 set/stage 和真实 samplerless dummy pair; 固定 sampler 的参数/唯一 owner 与关闭链已核对 |
| Lower and link the native program / 3 | `RHI/OpenGL/Lowering.cpp:19` 的固定 override 与 `:47` 起 GLSL 460/clip/base-instance 配置; `Pipeline.cpp:254` 的最后发布. V3 native link/布局反射/无模拟 uniform, V5 旧对象保持与 CR-004 诊断/RAII 核对; native compile/link 错误未自然触发 |
| Prepare input and target compatibility / 3 | `InputLayout.cpp` 的顺序/location/格式/stride/频率与 caps; Pipeline 固定状态/IO、纯值 info 与 `Pipeline.cpp:381` 的对象归属. V5 builtin-only/depth-only、错型输入/目标、兼容 FBO, CR-003 的 Validation 外来 FBO 拒绝 |
| Extend context-owned retirement / 2 | BindingSet/Pipeline 强引用与预分配 NativeDeletion, `Device.cpp:273` 转交, `Submission.cpp:459` 的 dummy 退出. V6 worker 释放后 owner 原生回收及 Host 正常关闭, 不代表 Draw 在途 |
| Deliver rebuildable preparation / 2 | 三份 CMake 依赖与精简上游字节保持, V1 的独立正式 binary tree Debug/Release、V7 的源码/文档范围, B 的 22 份 C++ / 3 份 CMake 注释验收. 生成/测试材料仅在 build, 没有默认 fixture 或恢复步骤 |
| Reject unavailable operations / 4 | `RHI/OpenGL/Unsupported.cpp` 与 `CommandListUnsupported.cpp:123` 起保留拒绝. 原有列表/扩展/引用 helper 合同未改, V5 实际 SetGraphicsState 拒绝; 创建/link 未变为 Draw/Dispatch/票据或完整渲染 |
| Compile against canonical profile / 3 | `Engine/Shader/SourceGraph.cpp` 仅放行 include/samplerless 的 enable/require, 原编译/最终 IR 验证不变. 2.3 的正式 fetch 包/dummy 组合与未知扩展退出 1, Vulkan 1.1/SPIR-V 1.3、资源与包 ABI 保持 |

验收输入仍为 v2.0 / HEAD `2ae2f10a9694f3d045717049b1938160ad8c466b` 加当前相关 dirty/untracked. A 独立重算 25 份审查文件原始字节聚合, 与 B 验收的 `61c02208dbe0708564efad12adc154764f03c8776b8e4a45c4f3016cb6f12ca9` 完全匹配; 当前 1363 份生产非文档输入聚合为 `72ea4f572b7a3ee89195a2318632a8077747628574e4e560ea2dac2a5fb17120`. 与 A 原生运行输入相比, 仅 B 已确认非注释 token/指令等价的 Pipeline.hpp/.cpp、ValidationBackend.hpp/ValidationDevice.cpp 四份注释不同, 其余输入逐字节一致.

复用上节 A 第 1 轮整改的 Debug/Release 正式构建、完整消费者及 CR 回归结果, 没有重新编译或运行旧 exe 冒充本次新运行. 本次核对实际 fixture 的 10 份输入与全部 14 份构建/包产物 SHA-256, 均与 A 运行记录一致. 这四份注释文件不在 Engine/Shader/CMakeLists.txt 的 compiler fingerprint 输入集合中, canonical 工具/包身份没有因本次 B 注释变化而失效. 因此最终仅需 L0/源码与证据核对, 没有新增或重复行为测试; 复用范围与 B 不复跑的事实分别保留.

实际执行 `openspec list/status/instructions apply` 核实当前仓库 root、spec-driven 和 20/23; `openspec validate establish-opengl-shader-pipelines --strict --json` 退出 0, valid=true、issues 为空. `python build/d03-t02/final-verify/CheckCloseout.py` 在同步前检查 16 份文档/204 处链接与锚点, 无错误, `git -c core.safecrlf=false diff --check` 通过. 当前 A/B 的四个问题项均已勾选, N-001 已关闭, B 的代码/注释结论均通过. 最终输入、产物核对和重置前问题单快照仅暂存 build/d03-t02/final-verify, 结论在本 tasks 永久留存.

未实测的 driver compile/link 错误、自然 OOM/context loss、非零 builtin 的实际数值/像素、upper-left 区域 clear、Draw/在途/呈现及跨 GPU/后端结论保持未实测. 这些是既定准备级验收的限制, 没有用本次归档扩大 D03 的阶段出口. 原 C1001 失败日志与原选项重试史保留, 本次 L0 收尾不新增该类构建结论.

2026-11-06 同步留存: 按 status 的三个 specs.existingOutputPaths 和一次成功的 specs instruction 快照执行原生 sync-specs 智能合并, 未新增/删除其它 capability. 新建 opengl-shader-pipelines 主规范的 9 条要求/26 个场景, Purpose 原样取自 delta; 修改 opengl-device-resources 的 unavailable 要求(保留旧 3 场景并补准备/执行区分), 修改 shader-build canonical profile(保留旧 2 场景并补 samplerless fetch). 既有 Purpose、其它要求与场景全部保留, 主规范只有普通 Requirements 段, 无 delta 操作头或 TBD.

逐条重比三份 delta 与主规范, 11 个要求块全部一致, 再次同步将无剩余变更. `openspec validate --specs --strict --json` 退出 0, 10/10 主规范 valid=true, 无 ERROR/WARNING; 五条 INFO 为长段落提示, 未据此扩展文档重构. 同步后 L0 检查覆盖 17 份文档/204 处本地链接与锚点, 无错误; B 的 25 文件验收摘要不变. 四项 CR 的修正/复核、N-001、B 代码/注释通过、最终输入和复用/未覆盖范围均已在本 tasks 留存, 确认 9.2. 此时仅剩归档任务 9.3, 按项目合同不得提前勾选, 将在实际归档及其后置核对成功后确认.

2026-11-06 归档收尾完成: `openspec archive establish-opengl-shader-pipelines --skip-specs --yes --json` 退出 0, 实际目标为 `openspec/changes/archive/2026-11-06-establish-opengl-shader-pipelines`. 三份主规范已在前一步完成智能同步和逐块验证, --skip-specs 仅避免重复应用, 不是省略同步; --yes 沿维护者已授权的收尾执行, 唯一未勾选的 9.3 按合同在归档成功后才确认. 原 .openspec.yaml 原始字节保留, 活动目录已移走, 本归档 13 处外部相对链接已按新深度修正, 既有历史 archive 未改写.

归档后再次比对三份主规范与本归档 delta, 11 个要求块一致; B 的 25 文件验收摘要与 1363 份生产输入均保持最终验收字节. `openspec validate --all --strict --json` 退出 0, 10/10 主规范通过, 活动 change 数为零; `openspec validate --archived --strict --json --report findings` 退出 0, 12/12 归档任务完整性通过. D03-T02 三项计划已勾选并引用本归档, 当前入口切换到未创建 change/未实施的 D03-T03, 后续工作包正文和验收门槛未改.

最终问题单已按 Docs/CodeReview.md.tpl 逐字节重置, 维护者的 build 审查排除说明保留. 归档后的 L0 检查为 17 份文档/180 处本地链接与锚点, 无错误, diff --check 通过. 本包 **23/23 全部完成**, 起始 verify 报告中三项流程待办均已实际关闭; 最终完整性、正确性和一致性核对完成, 无 CRITICAL/WARNING/SUGGESTION 遗留, 四项正式 CR 与 N-001 的结论及验证限制均留存在本 tasks. 本次不新增构建、原生或性能实测结论, 未暂存/提交/推送.
