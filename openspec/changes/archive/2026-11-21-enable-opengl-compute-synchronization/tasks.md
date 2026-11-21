# Tasks

## 1. Canonical Compute 准备与存储能力

- [x] 1.1 接通 `ComputeBaselineV1` 的单 CS、接口生成、最终 IR 反射、包构建与消费。用一个有 UBO、structured storage 及带格式 image 的临时程序对照生成范围、逻辑槽和实际 IR。核对包重读、旧 Graphics 包可用以及访问方向或格式不匹配时的拒绝。 [Shader 构建规范](specs/shader-build/spec.md)
- [x] 1.2 查询并公开受支持的 Compute 入口与存储能力，核对普通 Buffer 的 UAV/indirect 组合角色、Texture2D/2DArray 的实际格式及 image 读写 flags。用实际查询值对照允许的描述和越出存储 profile 的请求。同步 [RHI 能力说明](../../../../RHI/README.md#opengl-原生消费边界) 的资源与不支持项。 [资源规范](specs/opengl-device-resources/spec.md)
- [x] 1.3 沿用 SPIRV-Cross 与 Program 准备接通 Compute Pipeline，按活动逻辑资源分配 UBO、SSBO、image 和组合采样。用跨 set 的同号槽核对实际反射、单元、阶段/combined limits 及本地工作组分轴和乘积，超限在成功发布前拒绝。 [Pipeline 规范](specs/opengl-shader-pipelines/spec.md)
- [x] 1.4 接通 immutable storage 绑定记录与 ContextState 的资源映射。用非零 SSBO offset、选定 mip 的单层与连续数组层对照相邻哨兵，核对实际格式、大小、方向和存储寿命。选一个错位或越界范围及一个不能绑定的 image 作拒绝对照。 [存储视图设计](design.md#storage-视图与公开调度边界)

## 2. 即时 Dispatch 与统一 GL 访问同步

- [x] 2.1 接通 raw 与 Validation 的 `SetComputeState`/`Dispatch`、Graphic/Compute 状态切换及常量版本。用小工作量的 Compute → Draw → Compute 核对实际 native 调用和结果，核对上限及紧邻超限的 CPU 决策，原生补合法小调度、零工作量和非法候选。确认直接 Dispatch 可用，间接 Dispatch 和专用队列仍拒绝。更新 [RHI 能力说明](../../../../RHI/README.md#opengl-原生消费边界)。 [执行规范](specs/opengl-immediate-compute/spec.md) · [Validation 规范](specs/rhi-common-validation/spec.md)
- [x] 2.2 接通 RHI 的实际访问集合、分域可见性与 native barrier 发射。用同一写入的 image 先采样后传输、Buffer 先 SSBO 读取后 indexed indirect/复制，核对数据和每个尚未满足的域。以部分 Buffer update 核对更新字节和存活的旧写，复查现有清除、附件与常量/几何域的真实调用映射及缓存命中、换列表消费。 [多域同步设计](design.md#按实际访问发射-opengl-依赖) · [现有 Draw 入口](specs/opengl-immediate-draw/spec.md)
- [x] 2.3 接通同态 UAV 访问及重叠读后写、写后写的执行依赖。用已初始化的 image 和 SSBO 保留先读见证再覆写，连续命令最后读到新值。以不相交和只读对照核对无多余依赖，并检查 UAV 选项不会丢掉已识别的重叠依赖。核对 Automatic 与已显式提交用途的等价结果，缺失用途不发出受影响命令。 [同步规范](specs/opengl-immediate-compute/spec.md)
- [x] 2.4 将实际 Compute 的 Program、绑定、view、Buffer/Texture 及动态区段并入现有录制/退休包。用一条合法 Dispatch 后拒绝另一个候选的真实序列，确认移交/释放调用者引用不提前退休，正常完成或已有受控退出路径可释放前缀。 [退休合同](../../../../Docs/Architecture.md#retirement)

## 3. RenderGraph Storage 版本与实际权限

- [x] 3.1 接通 storage 读、完整写和读改写声明、精确解析与能力输入。以一份 CPU 图对照完整覆盖、读改写保留旧内容和部分范围的前后版本，并验证未定义输入、错误格式或不兼容状态不能形成有效计划。 [图声明规范](specs/render-graph-contract/spec.md) · [编译规范](specs/render-graph-compilation/spec.md)
- [x] 3.2 接通现有活性、RAW/WAR/WAW 调度和版本范围检查。用两个存活消费者、靠后的一次重叠覆写和独立裁剪分支证明旧结果可读且不执行死分支。加一条同 Pass 重叠或旧版本环的拒绝对照，复用同一图的资源描述。 [图编译规范](specs/render-graph-compilation/spec.md)
- [x] 3.3 让 GraphManaged 的 Compute 读写权限和 RHI 状态按计划提交至已有接管入口。复用已验证的 GPU 生产和读取片段运行图内 Compute → Compute/Draw/传输。对照错误方向或越出授权的后续绑定，观察只有合法前缀发出、无失败图输出且归还授权后可建立有效 Automatic 状态。 [图执行规范](specs/render-graph-execution/spec.md)

## 4. 正式入口与跨模块本地检查

- [x] 4.1 构建正式 `Physara` Debug/Release 并运行对应的隐藏启动。按同样的正式构建输入重连 D08-T03 的受影响 Forward/indirect 探针并实际观察像素、命令与上传，补核一份既有图形 Shader 包的反射与 ABI。核对默认构建所需输入均来自正式目录。 [构建入口](../../../../Docs/Build.md) · [D08-T03 回放基线](../2026-11-20-complete-draw-batching/tasks.md#final-verification)
- [x] 4.2 合并记录两条 GPU 数据链上的 barrier bits 和位置、像素/字节、拒绝、状态恢复与退休结果。核对 capability 文档中实际交付的格式、维度、读写和入口对应当前证据。保存 OpenSpec change strict 结果。 [验证范围](#verification-scope) · [GL 能力合同](../../../../Docs/Architecture.md#capabilities)

## 5. A 本地交付与独立审查

- [x] 5.1 A 在 [签收问题单](../../../../build/d08-t04/final/review-signed.md) 登记基准、暂存/未暂存差异及新增文件、输入与产物指纹、实际执行和证据限制。交给维护者调度 B 审查，B 的任务仍待完成。
- [x] 5.2 B 独立核对规范、版本、直接消费者和受影响证据，补齐本轮涉及的第一方文件注释。在当前问题单记录覆盖、CR 编号及结论。首轮 CR-001 至 CR-007 待 A 整改。 [签收问题单](../../../../build/d08-t04/final/review-signed.md#问题与整改) · [独立审查合同](../../../../AGENTS.md#review-loop)
- [x] 5.3 A 按同一 CR 编号整改并复验受影响合同，登记证据和待复核项。无整改项时记录依据。 [首轮整改](#a-correction-1) · [第 2 轮整改](#a-correction-2)
- [x] 5.4 B 复核整改、注释和签收版本，只确认本角色事项。按实际证据关闭或退回问题单项。 [第 1 轮复核](#b-recheck-1) · [第 2 轮签收](#b-recheck-2)

## 6. 最终验证、同步与归档

- [x] 6.1 B 签收后核对实际工作树、包、库、可执行文件和日志身份，复用匹配原生证据并补齐受影响的缺口。逐项核对增量规范、场景和设计，记录 verify 与 change strict 结论。 [最终验证](#final-verification)
- [x] 6.2 按原生 OpenSpec 指令同步已接受 delta，核对保留的要求和场景，运行主规范 strict 校验。 [同步记录](#spec-sync)
- [x] 6.3 正式归档并检查开发计划和工件链接。保存 B 签收和验收证据、更新工作包状态，归档成功后按现有模板重置当前问题单。 [归档收尾](#archive-closeout)

<a id="verification-scope"></a>
## 验证范围与停止条件

| 风险 | 最小充分检查 | 停止条件 |
|---|---|---|
| Shader 编译成功但接口、格式、范围或计数在设备边界错配 | 用步骤 1 的固定包、同号逻辑槽及非零范围检查反射和实际数据。给受支持的 storage 数值类别配可区分的值和相邻哨兵。复用同一输入的格式、范围和限制拒绝对照。 | 活动绑定映射正确，合法结果和确切拒绝均已观察，能力表范围与实际设备和回放一致。 |
| 首次 barrier 后其它消费域读到旧 Shader 写，或普通更新覆盖范围错误 | 两条共享 GPU 链分别覆盖 image 写后采样/传输和 Buffer 写后 SSBO/indirect/copy。在同一 Buffer 链加一次局部 update，保留前后证据。核对实际 native consumer 之前的域位和观察值。 | 各后继域取得预期值及所需依赖。覆盖段和未覆盖段各有独立见证，命令间无 CPU 等待或全域 barrier 代替真实依赖。 |
| 同态 UAV 继续写或覆盖先前读取，引发 RAW/WAR/WAW 错序 | 在上述链保留原位结果与独立旧值见证，包含重叠读后写及连续写。以不相交和只读对照检查无误发。直接序列验证 Automatic 与 Explicit，合法图复用相同 GPU 片段。 | 旧见证和最终结果均正确且发射位置、实际范围符合预期。三种用途来源各验证其改变的边界，无需全排列。 |
| 图授权、失败前缀或跨录制访问历史错接 | CPU 图核对范围版本、活性和一个非法依赖。同一原生图的拒绝变体核对没有额外 Dispatch，并观察真实退休、授权退场和下一次合法录制。 | 存活命令及诊断可定位，失败不发布正常输出，前缀保留到真实完成，合法后继能使用确定数据。 |

测试材料放入 `build/d08-t04/`，如需末端 native 取样，等到实际完成后观察，记录为探针操作。待验收的消费者之间禁止插入测试专用同步。已有证据可证明相同输入和未受影响合同，输入或范围改变时只补对应的回归。记录实际命令、Shader/包、库和可执行文件指纹、驱动、结果与未覆盖范围。成本与性能测量留在对应阶段验收。 [验证规则](../../../../.agents/skills/physara-verification/SKILL.md) · [测试合同](../../../../Docs/Architecture.md#tests)

<a id="a-local-checks"></a>
## A 本地检查

- 正式 Shader 工具生成并重读单阶段 `ComputeBaselineV1` 包，验证 UBO、定长 SSBO 和 image 实际反射。改变活动 image 格式的输入被拒绝，未发布包。前序归档用的 EditorUi 图形包经本轮包读取器重新验证。 [计算包](../../../../build/d08-t04/shader-witness.log) · [拒绝对照](../../../../build/d08-t04/shader-rejection.log) · [既有图形包](../../../../build/d08-t04/shader-graphics.log)
- Release 原生用例在 Windows x64、Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129 上观测到 15 次 Dispatch 和 22 条 barrier。核对同版 image 采样和复制、同版 Buffer 的 SSBO 读取、固定数量 MDI 和传输、局部更新、同态 UAV 及重叠读后写。读回同时保留旧值见证和最终写值。分域观测包含实发 barrier 位及其位置。 [Release 原生日志](../../../../build/d08-t04/ComputeIntegration-Release.log) · [编译命令](../../../../build/d08-t04/compile-ComputeIntegration-Release.txt)
- 同一回放核对非零 mip/层及 SSBO 偏移，R32_UINT、R32_FLOAT 与 RGBA8_UNORM 的数组视图保持相邻层哨兵，RGBA16_FLOAT 单层经过采样和传输。实测常量版本、工作组限制、零工作量、缺失 Explicit 用途和真实退休。错误方向的 GraphManaged 候选只保留一条原生前缀，新的 Automatic 录制、字节读回与完成通过。 [原生检查记录](../../../../build/d08-t04/ComputeIntegration-Release.log)
- CPU 图检查覆盖完整覆盖裁剪、局部覆盖、未初始化读改写及非法同 Pass 重叠。实测图把迟声明的旧版本读放在后继覆写之前，裁剪无消费者分支并读回两个阶段的值。 [CPU 图检查](../../../../build/d08-t04/StorageGraphCpu-Release.log) · [实际图执行](../../../../build/d08-t04/ComputeIntegration-Release.log)
- 用本轮 Release 库重连前序登记的探针。R16/R32 原生间接参数、545 个表面的排序/批次、裁剪后按需上传及混合材质梯度均通过。 [间接绘制](../../../../build/d08-t04/Indirect-Release.log) · [Forward 表](../../../../build/d08-t04/ForwardRows-Release.log) · [混合材质](../../../../build/d08-t04/ForwardGradients-Release.log)
- 正式 Debug/Release 使用 Windows x64 MSVC 和 SDK 10.0.26100.0 构建，`Physara.exe --hidden --poll-limit=5` 均退出 0。Debug 的 MSVC `C1001` 发生在不同编译单元，保留原日志。最终关闭 MSBuild 节点复用并按单任务重试，原 SDK 与编译选项通过。两配置的 Compute 回放均得到 15 次 Dispatch、22 条实际 barrier 和相同读回。 [构建入口](../../../../build/d08-t04/BuildProduct.ps1) · [Debug 构建](../../../../build/d08-t04/build-Debug.log) · [Release 构建](../../../../build/d08-t04/build-Release.log) · [Debug 原生回放](../../../../build/d08-t04/ComputeIntegration-Debug.log) · [编译器日志一](../../../../build/d08-t04/build-Debug-C1001-stalled.log) · [编译器日志二](../../../../build/d08-t04/build-Debug-C1001-second.log)
- OpenSpec 1.13.2 对当前 change 严格校验通过。检查范围为本机报告的实际设备与驱动，后续阶段的性能测量单独留存。 [严格校验](../../../../build/d08-t04/validate-change.json)

- A 交接基准 HEAD `a611873df17b67f038606227841f20a2319bc2b8`，55 个实现文件的清单指纹 `7e17cdd40f5a8665e32783518e12ba99ea474fa8f26d0c1359ff86f727899c0f`。清单按实际字节关联新源码、包、库、可执行文件、调用命令与日志，共 6 次探针及两配置的正式启动。 [源码清单](../../../../build/d08-t04/source.json) · [A 本地证据](../../../../build/d08-t04/a-local-evidence.json) · [签收问题单](../../../../build/d08-t04/final/review-signed.md#本轮交接)

<a id="a-correction-1"></a>
## A 第 1 轮整改与复验

- 核对 B 注释版的 56 个实现输入及指纹 `a76da5c1bb8616779d8a588d642393a1d6466e91f8fba8344541564c548f154a`。整改版保留基准 HEAD，63 个实现输入的指纹为 `18e41b72e23585943e850e3ac34806bc66c3eb9304a011f74b9b90ddbdb85f99`。 [B 基准](../../../../build/d08-t04/a-correction-1/b-received-source.json) · [整改清单](../../../../build/d08-t04/a-correction-1/a-correction-evidence.json)

| 审查项 | 当前整改版的观察 |
|---|---|
| CR-001 | 实际绑定名的 MIN/MAG 都为 `0x2600`，写入读回 17、31，相邻层保留 93。 |
| CR-002 | Automatic 和 GraphManaged 均增加一次 Draw。显式 ClearState 后空数组无法发出 Draw，图内非法候选也无法使用旧状态。 |
| CR-003 | 四次更新分别实际发出 1365、1365、1365、1 笔。含 Shader 前缀的对照在实际复制前仍产生 `GL_BUFFER_UPDATE_BARRIER_BIT` 并读得 127。 |
| CR-004 | 用正式能力入口编译通过 structured UAV 与数组 image 图。禁用 structured UAV 或移除数组 image load 能力时分别拒绝。实测数组层上限为 2048，2D 为 1。 |
| CR-005 | 两条行声明中没有重叠时屏障位为 0，重叠时为 `0x2000`。实际颜色分别为 (61,193,0) 与 (61,127,0)。 |
| CR-006 | 每个越界调用只得到包装层的拒绝。相同入参直接调用 raw 仍被拒绝，合法范围与哨兵均成功。 |
| CR-007 | 单 CS 的实际编译失败和 VS/FS 的实际链接失败各带原生原因。离线格式冲突日志标为 ComputeBaselineV1 / compute。 |

7 项定点检查在 Debug 和 Release 均通过，使用相同的输入与原生设备。 [Release 定点回放](../../../../build/d08-t04/a-correction-1/CorrectionNative-Release.log) · [Debug 定点回放](../../../../build/d08-t04/a-correction-1/CorrectionNative-Debug.log) · [离线诊断](../../../../build/d08-t04/a-correction-1/shader-rejection.log)

- Windows x64 MSVC 与 SDK 10.0.26100.0 的两种正式配置均通过构建和 `Physara.exe --hidden --poll-limit=5`。Debug 编译器 `C1001` 和增量链接 LNK1000 的原始日志留存。结束本次构建中的卡住进程，采用原选项串行重试，移开旧的增量链接缓存后重新链接成功。 [Debug](../../../../build/d08-t04/a-correction-1/build-Debug.log) · [Release](../../../../build/d08-t04/a-correction-1/build-Release.log) · [C1001](../../../../build/d08-t04/a-correction-1/build-Debug-C1001-stalled.log) · [LNK1000](../../../../build/d08-t04/a-correction-1/build-Debug-LNK1000.log)
- Release 的 Compute 综合回放仍记录 15 次 Dispatch、22 条实际 barrier，并通过跨域、局部覆盖、显式用途及真实退休读回。本轮 Release 产物重放 R16/R32 indirect 和 545 表面 Forward 对照，均通过。原生范围限于 Windows x64、Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129。OpenSpec 1.13.2 strict 通过。 [Compute](../../../../build/d08-t04/a-correction-1/ComputeIntegration-Release.log) · [indirect](../../../../build/d08-t04/a-correction-1/Indirect-Release.log) · [Forward](../../../../build/d08-t04/a-correction-1/ForwardRows-Release.log) · [严格校验](../../../../build/d08-t04/a-correction-1/validate-change.json) · [待复核问题单](../../../../build/d08-t04/final/review-signed.md)

<a id="b-recheck-1"></a>
## B 第 1 轮整改复核

- B 核对了 A 的 63 个实现输入，复算指纹 `18e41b72e23585943e850e3ac34806bc66c3eb9304a011f74b9b90ddbdb85f99`。CR-001 至 CR-006 通过。B 在本机 Intel OpenGL 4.6 独立回放 Debug/Release 定点程序及 Release 综合程序，结果与 A 的三份日志相符。 [原生回放及输入身份](../../../../build/d08-t04/b-recheck-1/replay.json)
- CR-007 的原生编译、链接及离线格式拒绝对照通过。公开 raw CS 的资源类别、逻辑 set 及重复 push 布局在更早的拒绝路径上仍错误标记为 graphics。保留 CR-007 和 5.4 待复核。 [B 的实际诊断](../../../../build/d08-t04/b-recheck-1/diagnostic-boundary.log) · [整改要求](../../../../build/d08-t04/final/review-signed.md#问题与整改)
- B 完善 9 个文件的注释，核对有效代码、指令及宏续行一致。交回 A 的注释版指纹为 `23d55cca0b09140acdc1383e46a4a928129bb9aac03b73ef20520eb355935490`。 [源码清单](../../../../build/d08-t04/b-recheck-1/b-source.json) · [注释差异](../../../../build/d08-t04/b-recheck-1/b-annotations.diff)

<a id="a-correction-2"></a>
## A 第 2 轮整改与复验

- 本轮仅处理 CR-007 的准备诊断。raw Shader 报告实际阶段和 profile，使用共用的资源类别及逻辑 set 拒绝原因。Compute Pipeline 在核对 CS 的设备归属和阶段后报告模块身份，重复 push 布局在原生编译前拒绝。原生编译、链接和准备失败共用阶段及 profile 标签。 [Pipeline 增量规范](specs/opengl-shader-pipelines/spec.md) · [审查交接](../../../../build/d08-t04/final/review-signed.md#a-correction-2)
- 按 B 清单核对基准、Git 差异、63 个实现输入及登记的回放输入。相对 B 交回版修改 4 个 OpenGL 实现文件，A 整改指纹为 `17cf3b73b1e2d1779b0a6cc4db12d42e61ab3095eb66e9851fcc42956d790e11`。 [源码与产物证据](../../../../build/d08-t04/a-correction-2/a-correction-evidence.json)
- Debug 和 Release 用本轮 RHI 库重连原生检查。已验证的 Vulkan 1.1 组合 sampler 及 set=4 模块被 `CreateShader` 拒绝，合法 CS 配两份带 push 的布局被 `CreateComputePipeline` 拒绝。三个错误均保留具体原因、`ComputeBaselineV1`、阶段、入口和源。既有七项定点对照通过，包括实际 CS 编译失败和 VS/FS 链接失败，保留原驱动日志。 [SPIR-V 验证](../../../../build/d08-t04/a-correction-2/spirv-validation.log) · [Debug 前置诊断](../../../../build/d08-t04/a-correction-2/DiagnosticBoundary-Debug.log) · [Release 前置诊断](../../../../build/d08-t04/a-correction-2/DiagnosticBoundary-Release.log) · [Debug 定点对照](../../../../build/d08-t04/a-correction-2/CorrectionNative-Debug.log) · [Release 定点对照](../../../../build/d08-t04/a-correction-2/CorrectionNative-Release.log)
- 两配置正式构建及 `Physara.exe --hidden --poll-limit=5` 均通过。本轮 Release Shader 工具重新生成回放实际消费的 ArrayUInt 和 Fetch 包及接口，字节与已登记输入一致。离线格式冲突报告 Compute profile、阶段和源，并拒绝发布包。 [Debug 构建](../../../../build/d08-t04/a-correction-2/build-Debug.log) · [Release 构建](../../../../build/d08-t04/a-correction-2/build-Release.log) · [启动结果](../../../../build/d08-t04/a-correction-2/product-builds.log) · [包复验](../../../../build/d08-t04/a-correction-2/verify-packages.log) · [离线格式对照](../../../../build/d08-t04/a-correction-2/shader-rejection.log)
- 集成同步、indirect 与 Forward 继续引用 [前一轮证据](#a-correction-1)。本轮原生检查覆盖 Windows x64、Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129。OpenSpec 1.13.2 严格校验通过。CR-007 与 5.4 待 B 复核。 [严格校验](../../../../build/d08-t04/a-correction-2/validate-change.json)

<a id="b-recheck-2"></a>
## B 第 2 轮整改复核与签收

- CR-007 通过。B 在 Debug 和 Release 独立重放原先误报的三个公开调用。组合 sampler、逻辑 set=4 和重复 push 布局分别被正确的创建入口拒绝，回调均带有 Compute profile、阶段、入口、对应源和具体原因。原生 CS 编译、VS/FS 链接及离线 image 格式拒绝的对照也通过。 [Debug 前置拒绝](../../../../build/d08-t04/b-recheck-2/DiagnosticBoundary-Debug.log) · [Release 前置拒绝](../../../../build/d08-t04/b-recheck-2/DiagnosticBoundary-Release.log) · [原生回放与离线证据](../../../../build/d08-t04/b-recheck-2/replay.json)
- B 已核对 A 第 2 轮源码指纹 `17cf3b73b1e2d1779b0a6cc4db12d42e61ab3095eb66e9851fcc42956d790e11` 及登记的输入、产物和日志。在 `Shader.hpp` 和 `Pipeline.cpp` 完善诊断标签、文本及描述借用的注释后，有效代码和预处理语义一致。最终签收的 63 项实现输入指纹为 `1429079597f7905ca619980a23a812054a5d3bf766c51ee93a1b88e718f8a8cb`。 [B 签收清单](../../../../build/d08-t04/b-recheck-2/b-source.json) · [注释差异](../../../../build/d08-t04/b-recheck-2/b-annotations.diff)
- CR-001 至 CR-007 全部关闭。B 的原生复验覆盖本机 Windows x64、Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129。两配置正式构建与隐藏启动引用 A 第 2 轮结果，综合同步、indirect 与 Forward 引用第 1 轮记录。下一环节为 A 的 6.1 最终 verify。 [A 第 2 轮复验](#a-correction-2) · [A 第 1 轮复验](#a-correction-1) · [签收问题单](../../../../build/d08-t04/final/review-signed.md#本轮验收与留存)

<a id="final-verification"></a>
## 最终验证

| 维度 | 结论 |
|---|---|
| 完整性 | `spec-driven` 的 proposal、design、tasks 与 9 份 delta 已核对。20 项增量要求、75 个场景均已对照实现和对应证据，既有场景同时核对原要求的保留。最终 verify 后完成 18/20 项。 |
| 正确性 | CR-001 至 CR-007 已由 B 签收。复算 63 个签收实现输入以及 A 第 2 轮的 59 项登记输入、87 项产物和 27 份日志。复用两配置的正式构建和隐藏启动、B 的 4 次原生回放及离线拒绝。另用匹配的 Release 库补验包身份、错误执行模型和未活动布局。Change strict 通过。 [签收清单](../../../../build/d08-t04/final/signed-source.json) · [身份核对](../../../../build/d08-t04/final/verification-evidence.json) · [补验](../../../../build/d08-t04/final/package-admission.json) · [严格校验](../../../../build/d08-t04/final/validate-change.json) |
| 一致性 | 单 CS 沿用包与原生 Program 的准备边界。图编译与执行提交精确用途，设备在命令发出前按实际底层访问确定同步，提交与退休仍由既有即时列表承担。核对工程依赖和公开边界，新增文件归入 OpenGL 私有目标，Runtime 沿现有宿主取得设备能力。 [设计](design.md) · [同步合同](../../../../Docs/Architecture.md#barriers) · [别名与图编译合同](../../../../Docs/Architecture.md#graph-compile) |

| 能力 | 对应实现与验收依据 |
|---|---|
| Shader 构建 | `ComputeBaselineV1` 沿用生成接口、Vulkan 1.1/SPIR-V 1.3 校验和包身份。实测单 CS 与既有图形包读取，格式冲突的反射拒绝未发布包。 [反射](../../../../Engine/Shader/Spirv.cpp) · [包消费](../../../../Engine/Shader/RhiProgram.cpp) · [计算包](../../../../build/d08-t04/shader-witness.log) · [离线拒绝](../../../../build/d08-t04/b-recheck-2/shader-rejection.log) |
| OpenGL 资源与 Pipeline | 核对按目标查询的格式能力、SSBO 对齐和合法范围、image view 的 mip/layer、阶段及 combined limits。实测四种 storage 格式、相邻层哨兵、公开创建拒绝、活动绑定的 set 身份及独立密集单元。Release 补验中 CS 的自持字节与包一致，提交 Fragment 执行模型被拒绝，额外的未活动 UBO 没有占用单元。 [绑定计划](../../../../RHI/OpenGL/BindingPlan.cpp) · [资源创建](../../../../RHI/OpenGL/Bindings.cpp) · [综合回放](../../../../build/d08-t04/a-correction-1/ComputeIntegration-Release.log) · [补验回放](../../../../build/d08-t04/final/PackageAdmission-Release.log) |
| OpenGL 即时 Compute 与 Draw | 核对 Graphics 列表内的直接 Dispatch、耗用常量版本、零组及超限边界、跨列表缓存命中和真实退休。综合回放观察到 15 次 Dispatch 和 22 条原生 barrier，核对 image 采样与复制、SSBO、indexed indirect、部分更新、同态重叠和读后写的顺序及结果。继续引用已登记的 R16/R32 indirect 与 545 表面 Forward 对照。 [调度入口](../../../../RHI/OpenGL/CommandListCompute.cpp) · [原生同步](../../../../RHI/OpenGL/ShaderAccess.cpp) · [综合回放](../../../../build/d08-t04/a-correction-1/ComputeIntegration-Release.log) · [间接绘制](../../../../build/d08-t04/a-correction-1/Indirect-Release.log) · [Forward](../../../../build/d08-t04/a-correction-1/ForwardRows-Release.log) |
| RenderGraph 声明、编译与执行 | 精确范围参与版本、裁剪和 RAW/WAR/WAW 的 CPU 计划，正式宿主能力区分 2D/2DArray 及 storage 方向。执行拒绝越出授权或错误方向，保留一个已发前缀，之后 Automatic 可以恢复消费。分离行的回读对照核对真实授权范围与 barrier。 [用途编译](../../../../Engine/Renderer/RenderGraph/GraphPlanUses.cpp) · [执行器](../../../../Engine/Renderer/RenderGraph/GraphExecutor.cpp) · [CPU 图验证](../../../../build/d08-t04/StorageGraphCpu-Release.log) · [B 原生复核](../../../../build/d08-t04/b-recheck-2/CorrectionNative-Release.log) |
| RHI Validation | 核对公开描述和调度可判定错误在委托前拒绝，raw 独立维持设备、活动接口和 native limits，图内非法候选撤销旧状态的执行资格。B 复验确认三项准备诊断及编译/链接日志均保留各自的操作、阶段和源。 [候选预检](../../../../RHI/Validation/ValidationCommandList.cpp) · [存储绑定预检](../../../../RHI/Validation/ValidationDevice.cpp) · [B 两配置回放](../../../../build/d08-t04/b-recheck-2/replay.json) |

- 签收指纹为 `1429079597f7905ca619980a23a812054a5d3bf766c51ee93a1b88e718f8a8cb`。提交基准为 `a611873df17b67f038606227841f20a2319bc2b8`。独立反算 B 最后两处文件的注释差异，确认可还原 A 第 2 轮实际构建输入。有效代码与预处理指令相同，`Pipeline.cpp` 的实现行号相同，`Shader.hpp` 新增注释不影响处理指令或行号相关语义。本轮复用这些构建与原生回放，并记录补验对应的库字节身份。 [构建与回放身份](../../../../build/d08-t04/a-correction-2/a-correction-evidence.json) · [签收差异](../../../../build/d08-t04/final/signed-source.diff)
- B 的注释验收覆盖 Shader 构建、图声明/编译/执行、OpenGL 资源和 Pipeline、即时访问与提交、Validation 及宿主能力。最后两份文件补齐诊断标签的寿命、待报文本的字节范围及异常期间的 Shader 描述借用。 [首审与整改](#a-correction-1) · [B 最终签收](#b-recheck-2) · [B 注释覆盖](../../../../build/d08-t04/final/review-signed.md#b-的注释覆盖)
- 原生证据限于 Windows x64、Intel RaptorLake-S Mobile、OpenGL 4.6.0 Build 32.0.101.6129。未覆盖其它 GPU 或驱动、编辑器视觉和性能测量。组数上界以现有 CPU 准入和空组对照核对，原生执行使用小规模组数。 [组数及范围回放](../../../../build/d08-t04/a-correction-1/ComputeIntegration-Release.log)

<a id="spec-sync"></a>
## 主规范同步

按 OpenSpec 1.13.2 的 specs 工件指令完成 9 份主规范的同步。新增 `opengl-immediate-compute`，沿用增量的 Purpose。其余 8 份主规范保留既有 Purpose、未变更的要求与场景。同步共新增 7 项要求、更新 13 项要求，并逐项核对 75 个 delta 场景。全量 37 份主规范严格校验通过。 [同步清单](../../../../build/d08-t04/final/spec-sync.json) · [主规范严格校验](../../../../build/d08-t04/final/validate-specs.json)

<a id="archive-closeout"></a>
## 归档收尾

OpenSpec 1.13.2 已在 9 份主规范同步后归档本 change，20 项任务全部完成。D08-T04 已验收并归档，开发计划指向 D08-T05。B 签收原件已留存，当前问题单已按模板复原，归档工件和开发计划的本地引用已复核。 [归档回执](../../../../build/d08-t04/final/archive.json) · [开发计划](../../../../Docs/Developments/Developments.md#current-stage) · [B 签收原件](../../../../build/d08-t04/final/review-signed.md)
