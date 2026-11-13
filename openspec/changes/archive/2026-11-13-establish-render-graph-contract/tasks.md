# Tasks

## 1. 图资源、范围与导入

- [x] 1.1 在 `Engine/Renderer/RenderGraph` 建立声明模块和 `PhysaraRenderGraph` 目标。用普通 CMake 配置编译该目标，确认 CPU 消费者只需 Core 与公共 RHI，不链接 OpenGL、Platform 或 Editor。
- [x] 1.2 实现类型化资源身份及图代、索引、版本检查。在 CPU 消费者中建立两张同索引、同版本的图，验证默认身份和跨图身份被拒绝，并检查纹理/缓冲身份不能互传。 [身份合同](specs/render-graph-contract/spec.md#requirement-keep-typed-resource-versions-local-to-their-graph)
- [x] 1.3 实现描述、严格范围、外部导入与显式导出。用同一外部对象核对等价导入返回初始版本，写入后仍如此，冲突导入不新增身份。用一个不合格式或越界的纹理范围及紧邻缓冲末端的合法/越界访问验证不裁剪。 [导入和范围](specs/render-graph-contract/spec.md#requirement-declare-exact-resource-ranges-and-canonical-imports)

## 2. 写版本和内容有效性

- [x] 2.1 实现从当前写版本产生新版本、唯一生产者及确定范围的声明记录。以清屏后 Load 合成核对两个版本、旧版本仍可明确引用、Load 的前一内容依赖及清屏无旧内容读取。 [写版本合同](specs/render-graph-contract/spec.md#requirement-record-version-producers-and-content-effects)
- [x] 2.2 将初始内容、局部保留和丢弃与 RHI 状态分开。用 atlas 局部更新确认旧 mip 内容依赖与其它 mip 未初始化；用未写目标的 Load 及 StoreDontCare 后的读取/导出核对拒绝和版本诊断。保留编译所需的范围与效果，不实现 T02 的覆盖分析。

## 3. Pass 数据和访问

- [x] 3.1 实现即时 setup 与拥有型延迟数据，使用有界 arena 并在归还存储前销毁对象。用一个有析构观察的值验证 setup 结束后仍可取得原值、构图不调用 execute；用写声明后的拒绝和小预算验证失败不发布回调、版本或导出记录，且候选对象被清理。 [Pass 数据合同](specs/render-graph-contract/spec.md#requirement-own-deferred-pass-data-after-immediate-setup)
- [x] 3.2 实现以当前 Pass 声明为依据的资源解析入口。用同一个查询消费者核对正确范围的采样获准，错误用途、超出声明范围和未声明版本被拒绝，并确认被拒绝的请求不会到达解析器。 [权限合同](specs/render-graph-contract/spec.md#requirement-restrict-resource-access-to-the-current-pass-declaration)

## 4. CPU 集成和 A 本地验证

- [x] 4.1 复用前面各组的同一 CPU 消费者，声明诊断和字体上传、VB/IB/常量范围、窗口色清除、UI 合成及导出。对照固定输入核对读取的生产版本、目标 Load 及合成后的导出版本；不创建 Device、不打开 CommandList、不使用窗口。 [编辑器帧声明](specs/render-graph-contract/spec.md#requirement-describe-editor-frame-dependencies-without-device-work)
- [x] 4.2 对匹配源码运行受影响目标的 Debug/Release 构建、Release 的声明消费者和一次普通产品构建，核对工程没有依赖本地检查材料。留存命令、配置、输入与产物身份、结果和未覆盖范围；确认所有新合同已有证据后停止扩展。T01 没有原生命令或产品行为改动，GPU 像素与窗口验证留给 T03–T05。

检查代码、脚本和临时工程只放在 `build/d05-t01/`，与各实现组同时完成其直接检查。采用正式声明/查询入口和最小 CPU 解析替身，不向正式源码或默认构建加入测试开关。只有输入变化或发现具体缺口才追加或复跑。 [验证约定](../../../../.agents/skills/physara-verification/SKILL.md)

## 5. 独立 A/B 审查与注释

- [x] 5.1 A 本地检查通过后，在 [本轮问题单](../../../../build/d05-t01/final-verification/b-signed-ledger.md) 登记基准、实际差异与未跟踪文件、构建和检查入口及限制，交维护者调度 B；本地结果不勾选 B 或最终验收项。
- [x] 5.2 B 在独立会话核对实际版本、图记录及权限、拥有型数据和直接消费者证据；补齐涉及的完整第一方文件注释，登记有稳定编号的发现及复核要求。 [第一轮审查](../../../../build/d05-t01/final-verification/b-signed-ledger.md#问题与整改)
- [x] 5.3 A 按编号整改，复跑能区分实际缺陷的检查，并记录修正输入、结果和限制；如无正式发现，记录适用的自检结论。 [A 整改与自检](#a-correction-verification)
- [x] 5.4 B 独立复核修正版与证据、关闭或有依据地撤回正式发现，分别记录正确性和注释结论。 [B 整改复核](#b-correction-review) · [审查闭环](../../../../AGENTS.md#review-loop)

## 6. 最终验证与归档

- [x] 6.1 B 签收后按 OpenSpec verify 将六条需求及其场景对应到签收版本的真实实现和 CPU 证据，复验具体缺口；留存最终验证、适用环境和未覆盖范围。结构校验不替代行为验收。 [最终验证](#final-verification)
- [x] 6.2 最终验证通过后同步 `render-graph-contract` 的 Purpose 与需求，运行锁定版本的 OpenSpec 严格校验，确认其它主规范不受影响。 [主规范同步](#spec-sync)
- [x] 6.3 留存 A/B 和最终证据后归档，核对文件、链接和任务状态。更新 D05-T01 状态及下一工作包引用；归档成功后按规则重置当前问题单。 [归档结果](#archive-result)

<a id="a-local-verification"></a>
## A 实施与本地验证

基准为 `v2.0` 的 `c209586d7af4c908c972d51a52fd51887d454754`。正式实现新增 CPU 目标 `PhysaraRenderGraph`。图使用专用帧 arena 持有回调和数据，写入与导出随 setup 成功一并发布。图层只拒绝能由声明直接判定的未定义内容。需要合并多条局部效果的覆盖分析和物理覆写顺序由 D05-T02 处理。 [实现入口](../../../../Engine/Renderer/RenderGraph/RenderGraph.hpp) · [范围与权限](../../../../Engine/Renderer/RenderGraph/PassResources.cpp)

Visual Studio 2022 17.14、MSVC 19.44.35228、Windows SDK 10.0.26100.0，x64。受限环境中的初次 MSBuild 读取 SDK 用户缓存时报告 MSB4184。允许读取后，使用现有 SDK 完成构建。以下命令从仓库根目录运行。

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -T host=x64
cmake --build build --config Debug --target PhysaraRenderGraph --parallel 1
cmake --build build --config Release --target Physara PhysaraRenderGraph --parallel 2
cmake -S build/d05-t01/consumer -B build/d05-t01/consumer-build -G "Visual Studio 17 2022" -A x64 -T host=x64
cmake --build build/d05-t01/consumer-build --config Debug --target PhysaraRenderGraphChecks --parallel 1
cmake --build build/d05-t01/consumer-build --config Release --target PhysaraRenderGraphChecks --parallel 1
./build/d05-t01/consumer-build/Debug/PhysaraRenderGraphChecks.exe
./build/d05-t01/consumer-build/Release/PhysaraRenderGraphChecks.exe
```

正式图目标两种配置、Release 产品及 UI Shader 包、独立 CPU 消费者两种配置均通过构建。生成的 CPU 链接项目只依赖图、Core 与通用 RHI 的 Physara 库，PE 导入只有 C++/C 运行库和 Kernel32。运行前后核对 1,118 个源码、构建及夹具输入文件，八个产物留有 SHA-256。 [正式 Debug 图构建](../../../../build/d05-t01/evidence/build-graph-debug-final.log) · [Release 产品及图构建](../../../../build/d05-t01/evidence/build-product-release-final.log) · [CPU 链接依赖](../../../../build/d05-t01/evidence/cpu-dependencies.log) · [输入清单](../../../../build/d05-t01/evidence/source-sha256.txt) · [产物清单](../../../../build/d05-t01/evidence/product-sha256.txt)

| 任务 | 实际结果 |
|---|---|
| 1.1–1.3 | 两图同索引同版本的 ID、默认 ID 和未发布版本均被拒绝。纹理与缓冲类型不可互传。同一外部对象的等价导入返回版本 0。冲突范围、内容声明、越界 mip、aspect 与越界一字节被拒绝。 |
| 2.1–2.2 | clear 与 Load 合成各自有生产者。旧版本可用于明确读取，旧版本写入被拒绝。局部 atlas 更新保留该 mip 的旧内容依赖，连续写同一 mip 不定义其余 mip。未写 Load、丢弃后的读和导出均报告资源及版本。 |
| 3.1–3.2 | setup 后的回调读取原始值。正常消费、放弃和失败均验证对象寿命。失败 setup 不发布版本、导出或回调。超预算不运行 setup。精确采样和合法的缓冲子区间被转交解析器，错误用途、超出 Pass 范围及另一版本在转交前被拒绝。 |
| 4.1–4.2 | CPU 消费者声明诊断图、字体、VB/IB/常量上传及清屏后的 UI 合成，核对了生产版本和最终窗口导出。Debug 和 Release 各五组返回 0。 |

CPU 消费者使用本地 RHI 对象替身和正式图入口，范围为本包的声明、数据寿命与权限。图编译、物理资源、GL 同步、像素和窗口呈现由后续工作包验收。本地 OpenSpec 1.13.2 严格校验通过 19/19。 [夹具入口](../../../../build/d05-t01/consumer/GraphChecks.cpp) · [Debug 结果](../../../../build/d05-t01/evidence/run-cpu-debug-final.log) · [Release 结果](../../../../build/d05-t01/evidence/run-cpu-release-final.log) · [OpenSpec 校验](../../../../build/d05-t01/evidence/openspec-strict.json) · [待审文件清单](../../../../build/d05-t01/evidence/review-files.txt)

<a id="a-correction-verification"></a>
## A 整改与自检

基准仍为 `v2.0` 的 `c209586d7af4c908c972d51a52fd51887d454754`，暂存区为空。整改前核对了 B 交接清单的 24 项哈希，包含实际源码、构建文件、OpenSpec、夹具和 B 的两个可执行文件。修正版承接 B 的注释，补充三条整改涉及的范围、清除和导入说明。 [本轮问题单](../../../../build/d05-t01/final-verification/b-signed-ledger.md#问题与整改) · [B 到 A 的源码差异](../../../../build/d05-t01/a-correction/source-corrections.diff) · [待复核工作树清单](../../../../build/d05-t01/a-correction/review-files-sha256.txt)

| 条目 | 实际修正与复验 |
|---|---|
| CR-001 | 从待查询范围中扣除后续有效写入。B 夹具复跑确认仅补写 mip 0–1 时全范围 Read 与 Export 拒绝。A 补充检查同一输入的 Load 拒绝和失败 Pass 的发布状态，确认补写 mip 2 后全范围 Load、Export 成功。另检查深度与模板独立性及字节范围中的空洞。 |
| CR-002 | 传输清除按首发配置检查 `isRenderTarget`。B 原版反例不再发布清除 Pass 或写版本。A 补充检查诊断与空导出表，并确认普通采样目标上的完整上传和保留式更新。既有 CPU 消费者覆盖 `PresentColor` 的合法传输清除。 |
| CR-003 | 共同导入入口在持有引用前核对入口和结束的公共 RHI 位域。B 原版夹具的两个非法状态均被拒绝。A 补充核对 `Unknown`、诊断字段及资源、无留存引用和合法组合状态的等价导入。 |

在 MSVC 19.44.35228 x64、Windows SDK 10.0.26100.0 下重编 B 的原版 `ReviewChecks.cpp` 和 A 的 `CorrectionChecks.cpp`，两种配置各两次运行均返回 0。正式图目标和既有 `PhysaraRenderGraphChecks` 也重新构建，两种配置的既有五组检查均返回 0。构建前后核对源码与夹具哈希，并记录产物身份。 [直接编译与运行入口](../../../../build/d05-t01/a-correction/RunDirectChecks.ps1) · [B 夹具 Debug](../../../../build/d05-t01/a-correction/run-ReviewChecks-Debug.log) · [B 夹具 Release](../../../../build/d05-t01/a-correction/run-ReviewChecks-Release.log) · [A 补充 Debug](../../../../build/d05-t01/a-correction/run-CorrectionChecks-Debug.log) · [A 补充 Release](../../../../build/d05-t01/a-correction/run-CorrectionChecks-Release.log) · [源码及直接编译产物](../../../../build/d05-t01/a-correction/direct-sha256.txt)

正式 CMake 复跑使用仓库根目录下的以下命令，其中 `$config` 分别为 `Debug` 和 `Release`。 [既有消费者 Debug](../../../../build/d05-t01/a-correction/run-GraphChecks-Debug.log) · [既有消费者 Release](../../../../build/d05-t01/a-correction/run-GraphChecks-Release.log) · [CMake 输入及产物](../../../../build/d05-t01/a-correction/cmake-sha256.txt)

```powershell
cmake --build build --config $config --target PhysaraRenderGraph --parallel 1
cmake --build build/d05-t01/consumer-build --config $config --target PhysaraRenderGraphChecks --parallel 1
& "build/d05-t01/consumer-build/$config/PhysaraRenderGraphChecks.exe"
```

本轮验证覆盖受影响的 CPU 声明合同，产品构建证据沿用 A 首轮记录。OpenSpec 1.13.2 严格校验通过 19/19。 [OpenSpec 结构校验](../../../../build/d05-t01/a-correction/openspec-strict.json)

<a id="b-correction-review"></a>
## B 整改复核与签收

B 对基准 `c209586d7af4c908c972d51a52fd51887d454754` 上的实际工作树复核。A 交接工作树的 16 项、直接编译的 21 项和 CMake 的 81 项 SHA-256 均匹配。随后在 MSVC 19.44.35228 x64、Windows SDK 10.0.26100.0 下，从当前源码独立编译首轮 `ReviewChecks.cpp` 和 A 的 `CorrectionChecks.cpp`，Debug 与 Release 四次运行均返回 0。正式图目标和原 CPU 消费者沿用已核对输入、产物和日志的 A 整改结果。 [B 编译输入及产物](../../../../build/d05-t01/b-recheck/tested-sha256.txt) · [A 正式构建证据](../../../../build/d05-t01/a-correction/cmake-sha256.txt)

| 条目 | B 独立复核 |
|---|---|
| CR-001 | 仍有丢弃或未写内容的全范围 Read、Load、Export 被拒绝。相邻已写 mip 可导出，补写最后一 mip 后全范围 Load、Export 恢复。深度/模板各自有效，缓冲字节空洞仍被拒绝。 |
| CR-002 | 普通采样纹理的传输 Clear 被拒绝，未发布 Pass、写版本或导出。完整上传和保留式更新可声明。原 CPU 消费者的合法 `PresentColor` 清除及 UI 合成检查通过。 |
| CR-003 | 导入入口与结束状态在记录和引用留存前校验。未定义位与 `Unknown` 被拒绝，诊断含资源与失败字段。合法等价导入沿用版本 0。 |

三项正式问题均关闭，代码正确性通过本轮 B 验收。七个涉及的第一方文件注释复核通过。B 更新了剩余范围回溯、附件及传输清除、导入参数的注释，核对了纯注释差异、预处理指令和续行。签收输入记录了补齐注释后的源码、构建文件、合同和夹具身份，CPU 产物对应注释更新前的同一实现。 [本轮问题单](../../../../build/d05-t01/final-verification/b-signed-ledger.md#问题与整改) · [B 注释差异](../../../../build/d05-t01/b-recheck/comments.diff) · [B 签收输入 SHA-256](../../../../build/d05-t01/b-recheck/accepted-input-sha256.txt)

| 夹具 | Debug | Release |
|---|---|---|
| B 首轮反例 | [通过](../../../../build/d05-t01/b-recheck/run-ReviewChecks-Debug.log) | [通过](../../../../build/d05-t01/b-recheck/run-ReviewChecks-Release.log) |
| A 整改边界与恢复 | [通过](../../../../build/d05-t01/b-recheck/run-CorrectionChecks-Debug.log) | [通过](../../../../build/d05-t01/b-recheck/run-CorrectionChecks-Release.log) |

本轮独立复跑覆盖整改影响的 CPU 声明行为。图编译、物化、GPU 像素、后端同步和窗口呈现由 D05 后续工作包验收。D05-T01 的最终 OpenSpec 验证按任务 6.1 执行。

<a id="final-verification"></a>
## 最终验证

2026-11-13 按 OpenSpec 1.13.2 的 spec-driven 工作流核对 proposal、design、delta 和 tasks。13 项实施及独立审查任务均完成。基准为 `v2.0` 的 `c209586d7af4c908c972d51a52fd51887d454754`，暂存区为空。逐项重算 B 签收的 23 项 SHA-256，源码、构建文件、合同与夹具均匹配。签收输入清单的 SHA-256 为 `c52a0a8b1b558db2d3f1597176be6274177aafc9bbf67a9aa1782c6f4f68eab2`。原问题单按字节留存，SHA-256 为 `6fbbf7c68e5c8a25ab01bb76449405bb5d29f2065fc3092e0cdb3167a925c6ae`，其中相对链接以原 `Docs/` 位置为基准。 [B 复核与注释验收](#b-correction-review) · [签收问题单原件](../../../../build/d05-t01/final-verification/b-signed-ledger.md)

在 Windows x64、MSVC 19.44.35228 和 Windows SDK 10.0.26100.0 下，用正式 CMake 工程重建 Debug、Release 图目标及既有 `PhysaraRenderGraphChecks`，两种配置各五组检查均返回 0。Release 产品和 UI Shader 包构建通过。生成的 CPU 链接项目只引用 RenderGraph、Core 和公共 RHI 的 Physara 库，Release 可执行文件仅导入 C++/C 运行库与 Kernel32。构建和运行前后 1,124 项输入匹配，记录了八个产物的 SHA-256。 [命令与退出码](../../../../build/d05-t01/final-verification/commands.json) · [输入和产物](../../../../build/d05-t01/final-verification/source-and-products.json) · [链接依赖](../../../../build/d05-t01/final-verification/cpu-dependencies.log) · [Debug 五组检查](../../../../build/d05-t01/final-verification/run-GraphChecks-Debug.log) · [Release 五组检查](../../../../build/d05-t01/final-verification/run-GraphChecks-Release.log)

复用 B 的 `ReviewChecks` 和 `CorrectionChecks` 共四次通过的运行。重算 B 运行使用的 21 项输入与产物哈希，两处签收前后的文件差异均只涉及注释，对应 B 留存的原始快照。 [B 证据核对](../../../../build/d05-t01/final-verification/b-evidence-match.json)

| 需求 | 场景 | 签收实现与证据 |
|---|---:|---|
| [类型化身份与图代](specs/render-graph-contract/spec.md#requirement-keep-typed-resource-versions-local-to-their-graph) | 1 | `AcquireGeneration` 分配唯一图代，声明与查询校验图代、索引和版本。CPU 检查编译期纹理/缓冲类型隔离，以及同索引同版本的跨图身份、默认和未发布身份被拒绝。 [图代](../../../../Engine/Renderer/RenderGraph/RenderGraph.cpp#L17) · [声明校验](../../../../Engine/Renderer/RenderGraph/PassResources.cpp#L133) · [CPU 身份与范围](../../../../build/d05-t01/consumer/GraphChecks.cpp#L127) |
| [严格范围与规范导入](specs/render-graph-contract/spec.md#requirement-declare-exact-resource-ranges-and-canonical-imports) | 2 | `DeclareResource` 保存描述及引用，检查规范范围和进出状态，再按 RHI 对象归并等价导入。CPU 检查写入后的等价导入、冲突、越界 mip 和 aspect、缓冲末端和越界一字节。CR-003 复查无效位与 `Unknown` 均被拒绝，且失败的导入不留存引用。 [导入](../../../../Engine/Renderer/RenderGraph/RenderGraph.cpp#L93) · [CPU 导入状态](../../../../build/d05-t01/a-correction/CorrectionChecks.cpp#L170) |
| [写版本生产者与内容效果](specs/render-graph-contract/spec.md#requirement-record-version-producers-and-content-effects) | 3 | `Write` 和 `PublishPass` 将前版、范围和唯一生产者写入逻辑记录。`RequireContent` 回溯剩余范围。CPU 检查 Clear 后 Load、显式旧版本、atlas 更新及未写或丢弃内容拒绝。CR-001 和 CR-002 覆盖部分补写后的恢复、aspect/字节空洞及非法传输清除。 [写入](../../../../Engine/Renderer/RenderGraph/PassResources.cpp#L363) · [内容回溯](../../../../Engine/Renderer/RenderGraph/PassResources.cpp#L263) · [CPU 内容效果](../../../../build/d05-t01/consumer/GraphChecks.cpp#L193) · [CPU 整改](../../../../build/d05-t01/a-correction/CorrectionChecks.cpp#L55) |
| [即时 setup 与拥有型数据](specs/render-graph-contract/spec.md#requirement-own-deferred-pass-data-after-immediate-setup) | 2 | `AddPass` 同步完成 setup，将已构造对象的所有权随成功发布转给图。CPU 检查源值改写后消费原值、未消费图销毁、非法用途后候选数据销毁且无部分发布，以及 arena 超预算。 [Pass 所有权](../../../../Engine/Renderer/RenderGraph/RenderGraph.hpp#L157) · [CPU 寿命与发布](../../../../build/d05-t01/consumer/GraphChecks.cpp#L309) |
| [当前 Pass 访问权限](specs/render-graph-contract/spec.md#requirement-restrict-resource-access-to-the-current-pass-declaration) | 1 | `ResolveDeclared` 要求当前 Pass 的一条同版本、同用途声明完整包含请求。CPU 检查允许采样和缓冲子区间，错误用途、扩大范围与其它版本在调用解析器前失败。 [解析授权](../../../../Engine/Renderer/RenderGraph/PassResources.cpp#L478) · [CPU 权限](../../../../build/d05-t01/consumer/GraphChecks.cpp#L401) |
| [编辑器帧的 CPU 声明](specs/render-graph-contract/spec.md#requirement-describe-editor-frame-dependencies-without-device-work) | 1 | 现有 Graph 声明入口记录诊断纹理、字体及 UI 缓冲上传的生产版本，`PresentColor` 清除后合成显式 Load，并导出合成后的版本、范围和呈现用途。CPU 消费者使用本地替身，整个声明过程未接入设备、命令列表或窗口。 [CPU 帧声明](../../../../build/d05-t01/consumer/GraphChecks.cpp#L461) |

六条需求、十个场景均与实现和证据对应。类型化版本、单链内容记录、专用 arena 和 Pass 授权符合设计及架构 §6.2，目标依赖与模块组织符合构建规范。OpenSpec 同步前严格校验 19/19 通过，最终 verify 通过。图依赖编译、实际物化与同步、GPU 像素及窗口呈现留待 D05-T02 至 T05 验收。 [设计决定](design.md#decisions) · [架构 §6.2](../../../../Docs/Architecture.md#graph) · [同步前严格校验](../../../../build/d05-t01/final-verification/openspec-strict-before-sync.json)

<a id="spec-sync"></a>
### 主规范同步

按 OpenSpec 1.13.2 同步新增能力 `render-graph-contract`。新主规范逐字保留 delta 的 Purpose、六条需求和十个场景。18 份既有主规范的文件 SHA-256 均匹配。主规范严格校验 19/19，含活动 change 的严格校验 20/20。 [主规范](../../../specs/render-graph-contract/spec.md) · [同步清单](../../../../build/d05-t01/final-verification/spec-sync.json) · [同步后严格校验](../../../../build/d05-t01/final-verification/openspec-strict-after-sync.json)

<a id="archive-result"></a>
### 归档结果

`openspec archive establish-render-graph-contract --yes --skip-specs --json` 退出 0，归档目录为 `2026-11-13-establish-render-graph-contract`。主规范已在移动前同步并复核。移动时为 15/16，剩余的 6.3 对应归档和文档收尾。五个文件移动前后 SHA-256 均匹配，随后更新归档相对链接，历史审查指向本轮留存。活动 change 清单为空，归档后 OpenSpec 严格校验 19/19 通过。 [CLI 记录](../../../../build/d05-t01/final-verification/archive-cli.json) · [移动前清单](../../../../build/d05-t01/final-verification/archive-inputs.json) · [归档后严格校验](../../../../build/d05-t01/final-verification/openspec-strict-after-archive.json)

归档后复核五个归档产物、19 份主规范及 B 签收的 23 项输入。开发计划记录 D05-T01 已验收归档，D05-T02 待立项。核对相对路径和锚点后将问题单按空模板重置，SHA-256 与模板相同。任务完成 16/16。 [链接与身份核对](../../../../build/d05-t01/final-verification/archive-link-and-identity-check.json) · [开发计划](../../../../Docs/Developments/Developments.md#current-stage) · [问题单模板](../../../../Docs/CodeReview.md.tpl) · [重置记录](../../../../build/d05-t01/final-verification/ledger-reset.json)
