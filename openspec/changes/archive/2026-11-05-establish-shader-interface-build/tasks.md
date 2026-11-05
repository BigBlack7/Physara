# Tasks

本 change 的业务目标仅覆盖 [D03-T01](../../../../Docs/Developments/D03-D05-FramePipeline.md#d03-t01), 按 [spec](specs/shader-build/spec.md) 与 [design](design.md) 实施; 本轮兼做维护者授权的全工程开发测试清理, 见 7.3. 复选框记录实际实施与验收状态, 规划产物齐备不代表实现或验收通过. 开发测试代码/脚本/样例/临时工程只放在 build 下归类目录, 不纳入正式源码或默认构建; 命令、结果与限制保存在本文件, 不新增统一测试框架或逐变更验证文档.

验证遵循 [与风险相称的验证](../../../../.agents/skills/physara-verification/SKILL.md). 先使用已有检查与独立预期, 新用例只补足能捕获不同缺陷的场景. 以下检查类别可在同一入口合并运行, spec 中已承诺的场景仍须有证据覆盖.

| 对应任务 | 具体风险 | 最小有效检查 |
|---|---|---|
| 1.1-1.3 | 来源不明、依赖污染、编译环境泄漏 | 固定来源/许可/摘要核对, 受影响库与独立公共头消费者的 Debug/Release 构建, 初始化/销毁检查 |
| 2.1-2.4、4.1 | 布局或 binding/IO 错配被生成器自身预期掩盖 | 独立手算边界与生成头 static_assert, 实际程序的最终 IR 反射, 对应不同 ABI 缺陷的拒绝输入 |
| 3.1-3.3、4.2-4.3 | include/编译身份漏项、错误定位丢失、请求数据串用 | 主 VS/FS 程序, 逐类输入变化与 profile 拒绝检查, 原始源行诊断, 同步请求结束后包数据消费 |
| 5.1-5.2 | 不完整/错 ABI 包被接受、失败覆盖旧包 | 包往返及各独立格式/ABI 拒绝风险, 一次对应发布失败和修复后重试 |
| 6.1-6.3、7.1 | 生成顺序竞态、失效漏项、无关重建或消费者集成回归 | 完整生成链的最终输入在 VS Debug/Release 上从空 binary tree 构建, 无改动/相关输入变动/缺失输出检查, 受影响消费者集成 |

各项满足列出的验收结果即停止追加检查. 相同源码/生成输入、工具与配置、依赖及检查输入身份已有匹配结果时, 后续任务引用并标明复用; 仅因输入变化、失败或具体缺口重跑受影响部分. 每类边界按独立缺陷归并用例, 记录实际运行、复用和未覆盖范围, 不用任务组切换作为重复全量构建的理由.

## 1. Agent-A: 固定来源与模块构建

- [x] 1.1 按 design 决策 5 精简保留固定 glslang、SPIRV-Tools、SPIRV-Headers 与 SPIRV-Cross 的实际实现、传递头、许可和七份固定生成源码, 移除未使用的上游工程/实现/工具/测试/示例. 同步 ThirdParty/README.md 的完整 commit、来源、归档 SHA-256、裁剪及快照复现说明. 验证: 本轮保留的原始文件逐字节对照清理前输入, 七份快照从固定输入重新生成并按 LF 核对; 审查实际保留范围, 不虚称本轮重新下载归档.
- [x] 1.2 用 ShaderToolchain/CMakeLists.txt 显式组织四个实际静态目标, 保持异常/RTTI/配置语义与私有依赖, 不加载上游工程; 同步 Docs/Build.md 移除普通构建的 Python 前提. 验证: 新 binary tree 的正式 Shader 消费者在 Debug/Release 编译链接, 无下载/安装/工具查找替换/Python 生成/上游 CLI 或测试 target; 用一个实际生成/编译/反射/包读取程序对比整理前 ABI 和最终 SPIR-V payload, 不重复旧全量边界检查.
- [x] 1.3 建立 PhysaraShader 与模块内部 PhysaraShaderBuild 的同步核心/薄驱动边界, 交付公共合同、初始化/结束 RAII 和受控错误结果及基本注释. 验证: 公共头各自可包含, 独立 CPU 消费者不需图形 context/原生头, 成功/初始化失败/重复调用可释放退出; 检查核心不 include 其生成物, RHI 及 OpenGL 不增加 Engine 反向依赖.

## 2. Agent-A: 单一接口声明与生成

- [x] 2.1 实现版本 1 接口/程序请求解析及 design 决策 2、11 的类型、集合、IO 与预算检查, 在权威声明旁说明格式和失败语义. 验证: 合法最小输入成功; 重复 JSON key/身份、未知字段/版本/类型、非法维度、零长数组、超预算和算术溢出均返回明确失败及字段路径.
- [x] 2.2 实现独立的 std140/std430 布局计算, 生成固定宽度 C++ 数据、显式 stride/padding 及布局 static_assert, 随实现保留手算边界输入. 验证: MSVC Debug/Release 消费生成头, 对单 scalar block、vec3+scalar、标量/向量数组、mat2/mat3/mat4 及 record 数组检查 offset/extent/对齐/stride, 不用生成器自身输出作为唯一数值预期.
- [x] 2.3 从同一输入生成 GLSL 数据与 stage IO、RHI 绑定头及反射预期; 根据公开 VulkanBindingOffsets 推导映射并明确 descriptor-set 模式. 验证: 同集合 b0/t0/s0、不同集合、阶段可见性和边界 slot 映射正确; 重复映射、越界、未知资源与 descriptor 数组被拒绝, 生成逻辑不分配 GL 单元.
- [x] 2.4 交付生成完成描述、产物摘要及不变内容不重写机制, 在驱动说明多输出失败/重试协议. 验证: 中途写入失败、混合代、缺失/修改任一产物都不能被编译入口认作完整新接口, 修复后可重试; 内容不变时数据头与 GLSL 的字节和修改时间保持不变.

## 3. Agent-A: 统一源依赖与 canonical 编译

- [x] 3.1 实现 design 决策 7 的唯一虚拟根/相对 include、声明图检查、编译器 includer 与源映射, 明确文件内容的拥有寿命. 验证: 传递/重复 include、guard、路径带空格可用; 缺失、重复根标签、越根、声明循环及宏拼接 include 明确失败, 嵌套语法错误定位到原文件/行号并保留引用链; 新增同名影子文件不改变已解析路径.
- [x] 3.2 实现 GraphicsBaselineV1 的显式选项、glslang VS/FS 编译/链接和 Vulkan 1.1 / SPIR-V 1.3 验证, 并给出 profile 权威说明及阶段注释. 验证: 合法程序各阶段通过 validator, 实际入口/profile 可查询; 非 main、其他阶段、写 storage、push constant、descriptor indexing、subpass input 与未知扩展均受控失败, 不自动提升目标或更改源文本.
- [x] 3.3 完成编译公共边界与诊断, 对请求、源、依赖图及工具对象实行 RAII/预算/可传播异常处理, 区分 noexcept 内的致命 OOM. 验证: Debug/Release 的无效源、超预算、可传播的分配失败、第三方异常及输出不可写有失败状态和上下文, 这些可返回失败后下一合法请求成功; 持续 OOM 诊断不依赖分配. noexcept 内的致命 OOM 在独立构建进程中有界报告命令上下文并非零退出且保留旧包, 不承诺同进程恢复, 不降低 Debug 迭代器检查或在产品进程安装 handler.

## 4. Agent-A: 最终反射与完整身份

- [x] 4.1 从验证后的最终 SPIR-V 提取数据/资源/阶段 IO 和活动子集, 实现 schema ABI 与 VS/FS 兼容核对. 验证: 与 2.2 的独立布局预期一致; 有效但 offset/stride/binding/类型/可见性错误的 IR 被核对入口拒绝; 缺失/冲突 FS 输入失败, 合法未消费 VS 输出与不活动资源不会制造原生占用或虚假覆盖.
- [x] 4.2 实现规范化 abiHash、完整 contentHash 和可追溯 compilerFingerprint, 使用稳定 SHA-256 并说明字段编码/版本/路径规则. 验证: 固定公开 hash 向量通过; 逐个改变源/include、接口语义、合法 define、入口/阶段、选项/profile、生成规则或工具指纹时身份正确失效; JSON 纯格式变化保持 ABI, 更换绝对工作目录保持同输入身份, padding/地址/时间戳不参与.
- [x] 4.3 将反射、身份、依赖和诊断关联到同一次同步编译请求, 接入强制重新核对入口及完整成功/失败结果. 验证: 连续交替编译两个不同程序不会混用数据; 不活动声明与实际使用区分可见; 失败结果不能取得成功包或因旧输出存在而返回成功; 请求/编译环境销毁后成功包的自有数据仍有效, 借用只在包拥有者存活时使用.

## 5. Agent-A: 完整编译包与 CPU 消费

- [x] 5.1 实现 design 决策 10 的定宽版本化单文件包、确定性元数据、完整摘要和有界读取, 明确拥有者/view 寿命及预期 ABI 参数. 验证: 正常双阶段包往返一致; 截断、未知版本、长度加法溢出、越界/重叠、错误对齐/摘要/阶段和 ABI 不匹配全部失败, 不直接序列化 C++ 对象布局或向 RHI 暴露 Engine 包类型.
- [x] 5.2 实现同目录事务写入、完整包替换、不变内容不替换及有限临时文件清理, 随实现说明失败退出. 验证: 有旧包时注入编译/写入/替换失败, 旧包字节保持完整且新构建明确失败; 修复后可重试; 遗留临时文件可清理, 重复构建不积累按 hash 保存的无界历史.

## 6. Agent-A: 真实程序与 CMake 集成

- [x] 6.1 在 build 中提供薄 VS/FS 常量/纹理测试程序和 CPU 检查输入, 数据、IO 与 b/t/s 绑定来自生成接口, Common 只有实际复用的无资源函数. 验证: 常量、独立纹理/sampler 与用户 stage IO 影响输出并出现在活动反射中; std430 record 有实际消费证据. 测试输入不作为正式 Shader 交付, 可随 build 删除.
- [x] 6.2 正式模块 CMake 保留核心/驱动和接受显式输入路径的生成规则, 为必要输出声明 OUTPUT/DEPENDS/DEPFILE 与配置隔离; 开发样例及检查目标只在 build 中的本地工程注册, 同步 Docs/Build.md. 验证: 新目录的普通 Debug/Release 默认构建不需要测试材料; 本地工程使用同一规则生成实际包, 删除必要输出能恢复且配置不覆盖. 已有生成头初建/布局证据按未变范围引用, 不重建额外生成器矩阵.
- [x] 6.3 将增量检查输入与脚本移到 build, 对本轮路径参数/接入变化检查正常生成、无改动不重写和传递源变化会更新包; 原 schema/define/失败重试/工具指纹等未改算法可明确引用历史证据. 检查只针对受影响规则, 不默认重跑旧全量 runner 或通过测试脚本修改正式源码.

## 7. Agent-A: 集成自检与交接

- [x] 7.1 汇总整理后的正式 Debug/Release 干净构建、受影响生成规则检查和未变 CPU 算法/独立消费者的复用证据, 区分清理前后输入. Runtime 输入未变则不重复 native smoke. 验证: 本轮风险各有对应证据, 第一方无新增警告, 当前构建不引用测试材料; 命名例外说明、文档/链接及未覆盖范围核对后停止扩大检查.
- [x] 7.2 核对 spec/design/实际实现、依赖来源、构建说明及 A 基本注释, 更新供维护者调度 B 的实际 diff/新增文件/输入摘要和证据. 旧交接包因本轮整理失效, 不代表当前版本; build 为可清理空间, 若跨工作树复现则另行提供实际本地输入, 不依赖自动恢复测试目录. 第 8 节保持未勾选, A 不代替 B 验收.
- [x] 7.3 按维护者要求一并清出早期 Core/Platform 测试专用头、宏分支及 RHI 测试 friend/前置声明, 原文件放入 build, 保留原产品分支. 核对正式源码无残留测试入口, 受影响 Debug/Release 默认构建及产品隐藏窗口启动/退出通过; 同步移除 Build 中已失效的历史 fixture 当前入口, 不扩大为原生全量测试.

## 8. 独立审查、整改与复核

- [x] 8.1 Agent-B 独立执行 Review/Annotation, 覆盖实际第一方 C++、Shader、CMake、受影响消费者、依赖接入及文档, 直接补充已理解代码的中文注释, 在 Docs/CodeReview.md 记录版本/覆盖/问题/证据. 验证: B 确认实际输入与 7.2 一致, 正式问题有稳定 CR 编号及分立 A/B 复选框, 无问题也给出实际覆盖与结论; 本项只能由 B 确认, A 自检不得代替.
- [x] 8.2 Agent-A 按正式问题逐条整改、同步注释并重跑受影响检查, 将修正和自检证据写入问题单, 仅勾选 A 整改项后交回 B. 验证: 每个 CR 要求均有对应实际 diff 和匹配输入的结果; 不删除/降级问题或填写 B 复核. 无需整改时记录不适用及 B 的依据, 不虚构修正.
- [x] 8.3 Agent-B 执行 Check/Annotation, 检查实际修正、回归及最终注释并逐条关闭/有依据撤回问题, 给出本轮验收结论. 验证: 正式问题全部经 B 确认, 注释满足 [注释标准](../../../../AGENTS.md#annotation), 最终输入与证据匹配; 新问题或版本变动继续 A/B 闭环, 未覆盖能力保持明确限制, 只有 B 可勾选本项及问题单 B 字段.

## 9. 最终 verify、同步与归档

- [x] 9.1 在 B 验收后的匹配最终输入上执行原生 openspec-verify-change 流程, 核对完整性/正确性/设计一致性并运行 `openspec validate establish-shader-interface-build --strict --json`, 必要时重跑受影响检查. 验证: 实际证据覆盖本 spec 全部场景, 无未关闭正式问题, 变更后失效的 B 验收已重新复核; 结构校验不替代行为证据.
- [x] 9.2 将审查范围、最终版本、B 结论、CR 编号及修正/复核摘要、注释覆盖和证据位置写入本 tasks, 仅在 [D03-T01](../../../../Docs/Developments/D03-D05-FramePipeline.md#d03-t01) 记录完成状态与 change 引用. 验证: CodeReview 清空前审查信息已完整留存, 其余 D03 工作包仍按真实状态保留; 无长期设计变化时 Architecture 不变, 不新增平行验证报告.
- [x] 9.3 按原生流程同步 shader-build delta 并归档, 归档成功后恢复 Docs/CodeReview.md 未启动模板. 验证: 新主 spec 保留 Purpose 和全部要求/场景, archive 完整保留产物与证据且活动 change 已移除; 归档或同步失败时保留本轮问题单与实际状态, 不提前清空或写成已交付.

实施证据当前状态: D03-T01 已完成并归档, 27/27 项完成. B 已验收 v2.3, CR-001 至 CR-009 全部关闭; 最终 verify、审查留存、主 spec 同步与问题单重置均完成.

规划检查 (2026-11-03): OpenSpec 1.13.2 原生 status 确认四类规划产物齐备; `openspec validate establish-shader-interface-build --strict --json` 通过, 无 issues. 本地核对 10 条要求、28 个场景、26 项任务, 全部任务未勾选, 相对链接/锚点与尾随空白检查通过. 这些仅是规划产物检查, 不作为上述实施、B 验收或最终 verify 证据.

## 实施证据

2026-11-04, Agent-A 基准为 `cb5660942c5f0f02a8afb38cc6dca7e0691c6e8b`. 1.1 从 Khronos 官方 codeload 的固定 commit ZIP 获取四份输入, `ZipFile.testzip()` 全部通过, 归档 SHA-256 与 [ThirdParty 来源清单](../../../../ThirdParty/README.md#源码归档校验值) 一致. glslang/Tools/Headers/Cross 分别保留 148/972/89/22 个文件, 保留文件逐一与归档内容比较; 仅 SPIRV-Tools `source/CMakeLists.txt` 与 `utils/update_build_version.py` 的已记录构建适配有差异: -B/Table 依赖与固定版本描述的惰性 fallback. 保留输入的比较记录已在最终交接前重新核对. 原始 ZIP 和机器核对记录在忽略的 `build/shader-interface/downloads` 与 `build/shader-interface/dependency-inputs.json`, 交接时须提供实际输入.

1.2 首次配置日志为 `build/shader-interface/configure.log`: MSBuild 探测 SDK 时被沙箱拒绝访问用户 SDK 目录, 尚未进入第三方/C++ 编译. 本机 CMake 为 3.30.0-rc4, Python 为 3.12.9; 随后通过受审的沙箱外构建访问已安装 SDK, configure-toolchain.log 配置成功. VS 2022 Enterprise / MSVC 19.44.35228.0 / SDK 10.0.26100.0; 四库 Debug 与 Release 构建成功, 日志为 dependencies-debug-complete.log、dependencies-release.log. 网络只用于此前 ZIP 获取, 配置/编译不下载或安装依赖, 未启用 optimizer/CLI/测试; 最初异常支持为 Sync, 随后根据真实初始化分配失败改为 /EHs (VS SyncCThrow), 上游选项局限于 ThirdParty/ShaderToolchain 子目录. 首次编译暴露的两个缺失传递头已从相同 ZIP 补齐, 旧失败日志保留, 最终来源记录为本段的文件数.

维护者确认的边界调整 (2026-11-04): Debug STL 默认 vector/string 及 vector move 探针都在 GuardBuild 内进入 terminate, 三份 probe 日志退出码均为 86. 维护者明确选择保留 Debug 检查, 可传播异常返回 Failure, noexcept 内的致命 OOM 由构建驱动有界报告并非零退出. proposal/spec/design/tasks 已同步, 不将修正单个默认构造误写为覆盖全部内部 OOM. CNG 私有 storage 已改为可抛出的显式 count 构造; cpu-debug-fixed.log 的布局/最终 IR/包/可返回失败与重建环境检查通过, 还不是最终输入或 B 验收证据.

### 清理前本地构建与行为证据 (Agent-A, 2026-11-04, 历史)

以下保存当时实际命令和输入位置, 不是当前工程入口. 本轮已将测试材料移到 build, 移除正式测试目标/故障钩子并调整 CMake 路径参数; 旧 exe、指纹和交接包不代表整理后的版本. 未变核心算法证据的复用与本轮新运行结果见文末, 不因这些历史用例数量而重复全量检查.

本轮只实施 D03-T01. 实际落地位置为 Engine/Shader、Engine/Shader/Interface、根 Shaders/Common 与 Shaders/Passes. 新增库/薄驱动/显式 CPU 检查, 以及 Conformance、NoRecords、LayoutCases 三份程序包. 完整接口声明以项目值类型随包拥有, 读取重新计算 ABI 并核对最终 IR; 活动反射与未活动声明分开. RHI、OpenGL、Platform、Runtime 源码和长期 Architecture/主 specs/archive 未修改.

| 范围 | 实际命令/证据与结果 |
|---|---|
| 来源/许可 | dependency-inputs.json 与 downloads 四份原始 ZIP. 最终再次逐文件比较 148/972/89/22 份保留输入, 只有 ThirdParty/README 已记载的两处 Tools 文件差异. 记录完整 pin、许可证和归档 SHA-256, CMake 不获取/安装依赖. |
| 干净与受影响构建 | 新 binary tree `build/shader-interface/final clean`, configure-final.log. `cmake --build "build/shader-interface/final clean" --config Debug --clean-first --target ALL_BUILD PhysaraShaderChecks PhysaraShaderFatalProbe --parallel 3` 的 final-debug-clean-build.log 成功. 后续快照与排版修正只重建 Shader/直接检查消费者, 复用输入未变的四库及 Core/RHI/Platform/Runtime; 匹配最终输入的补建为 final-debug-restored-build.log. Release 首次完整构建 final-release-build.log 成功. 最终成功日志无新增警告. |
| 最终 CPU/文件/进程 | `python -B Engine/Shader/Checks/RunChecks.py "build/shader-interface/final clean" Debug` 与 Release. handoff-debug-checks.log、handoff-release-checks2.log 各通过 96 项, 对应目录由日志首行和 handoff-check-index.json 指向; 每项 Runs.json 记录实际命令、exe SHA 与退出码. 均先要求当前构建成功, 失败构建后的旧 exe 不计入通过证据. |
| 独立布局与 IR | CpuChecks.cpp 的手算 static_assert/数值预期与 LayoutCases 实际 VS 输出共同覆盖单 scalar、vec3+scalar、std140/std430 数组和 mat2/mat3/mat4、record capacity/stride. 最终 IR 独立 validator 先通过, 然后对有效但 offset、数组/matrix stride、binding、类型或 visibility 错配做拒绝检查. 常量/纹理/sampler/IO/只读 record 实际活动; NoRecords 保留完整声明但活动 record 缺失. |
| 解析/预算/诊断 | 两配置覆盖重复 key/身份、未知版本/字段/类型/维度、非法数组/映射、声明/单源/总字节与文件数/深度预算, 含 64 层与 256 个唯一文件的合法边界及越界. include guard/重复/传递/相对路径/空格/影子文件、缺失/越根/循环/宏路径、原文件行号和引用链均检查. 声明图改为显式 DFS 栈, 避免深层检查本身耗尽线程栈. |
| 身份与完整包 | 固定 SHA-256 空串/abc 向量通过; 格式变化保持 ABI, 语义/源/include/合法 define/工具改变使相应身份失效. 不支持的入口、stage/profile/flags 直接失败. 包覆盖截断/版本/长度溢出/对齐/重叠/范围/摘要/阶段/入口/反射/预期 ABI, 并拒绝另一合法 ABI 包伪装身份字符串. 写入/替换失败保留旧完整包, 处理混合代/遗留 temp/修复重试. |
| 生命周期与 OOM | 成功包在环境销毁后仍有效, 交替请求与重新创建环境不串用数据. 可返回分配异常、持续 OOM 有界诊断、第三方异常、同一环境语法失败后重试通过. 真实 ShInitialize C 桥接在 /EHsc 下不能正确展开, 四库与 Shader 核心局部改 /EHs (SyncCThrow) 后初始化失败重试通过; 未降低 Debug iterator 检查. 独立 FatalProbe 共用正式驱动 handler, 两配置致命失败有上下文并非零退出, 旧包保留; 核心不安装产品进程全局 handler. |
| CMake 增量/迁移 | IncrementalChecks.py 对真实输入临时改动并恢复. Debug 的 incremental-debug2.log 已通过全部行为, 最终恢复补建因 C1001 失败后由最终匹配构建恢复; Release 的 final-release-incremental.log 全部成功, 28 项记录在 checks-Release-1793783143886342000/Runs.json, Originals/Restored 摘要相等. 覆盖无改动不编译/不改写、传递 include、新依赖边、define、schema、失败重试、工具指纹与每份必要输出删除. VS 丢失 BYPRODUCT depfile 不会重跑, 现将 package/depfile/stamp 全部列为 OUTPUT. 中央生成 target 的文件/target 依赖确保唯一顺序. 路径带空格和绝对目录迁移通过, 新影子文件不改变解析. |
| 配置与标准库边界 | final-artifacts.json 记录最终两配置 exe、包、ABI、content/tool 指纹及 payload SHA. 三程序 D/R ABI 与规范 SPIR-V 完全一致, 工具指纹区分实际配置, 输出互不覆盖. 四个公共头独立 TU 由不引入编译器/JSON/原生头的 ShaderHeaders 消费; RHI/Backend 未引入 Engine 反向依赖. |

实际环境为 Windows x64、VS 2022 Enterprise/MSVC 19.44.35228.0、SDK 10.0.26100.0、CMake 3.30.0-rc4、Python 3.12.9. 默认沙箱的 SDK 探测拒绝访问已安装的用户 SDK 目录, 显式 SDK/原生属性仍失败; 经工具审批允许该系统读取后构建. 初始四份源码获取之外, 配置/构建只用保留输入, 无下载步骤.

保留所有失败记录: 多处间歇 MSVC C1001/CL 内部退出经未改选项重试, 没有压低警告、关闭异常或 Debug 检查. 旧输入一次合法编译访问冲突的五次同 exe 重试均成功, 原因未定; 当前最终两配置完整检查通过. 一次最终 Release 替换遇到 Win32 5 并正确返回 I/O 失败, 未加生产自动重试, 原日志与随后未变输入的完整通过记录均保留. 这些环境现象不写成已修复的编译器/系统缺陷.

Runtime 的构建/链接与生命周期输入未变, 复用上述源码未变证明, 未重复 native smoke. 未声称覆盖其它编译器/GPU、ASan、全部自然 OOM、第三方内部所有泄漏、GL lowering、原生 Pipeline/Draw 或画面. 本轮满足 A 的 CPU/构建验收后停止扩大检查, 8/9 节仍须维护者调度的独立 B 闭环及之后的 verify/sync/archive.

### 清理前 A 交接 (历史, 已被本轮整理取代)

基准仍为 cb5660942c5f0f02a8afb38cc6dca7e0691c6e8b, 未暂存/提交/推送. 当前代码/CMake/Shader/Schema/检查输入及不可变技术说明共 1280 份原始字节记录在 `build/shader-interface/source-manifest.json`, 聚合 SHA-256 为 `bb5d8b1aa91bcbdbff78f81f381af06a9c52ea2d2ea5e21da2d5fb92a0793d72`. 为避免进度文件自引用, 清单明确排除 tasks、CodeReview 与两份 Developments 状态文件; 全部 staged/unstaged/untracked/删除状态及这些文档另由 full-change-manifest.json、handoff.json 和 actual-change.diff 记录. Shaders 已有真实源, 空 .gitkeep 删除列入实际 diff.

`handoff-check-index.json` 指向最终 Debug 96 项与 Release 96 项的实际目录, 并逐项确认 Runs.json 中 exe SHA 与最终文件相同. 最终规范包/工具身份见 final-artifacts.json. `handoff-inputs.zip` 打包真实新增/修改源、固定下载输入、最终本地 fixture/日志与检查 exe; build 被忽略, 跨工作树不能只看 commit 或声称 checkout 会重建这些输入. 自检脚本是已实现 Shader 模块的显式 CPU 消费者, 生成输出和本地日志供复现, 不作为额外代码审查范围.

A 的本地 strict validate 通过 (1 change / 0 issues), 文档本地链接/锚点、源树无本轮生成物、保护范围未修改已核对. 这不是 9.1 的 B 验收后最终 verify. 本轮问题单已只填写 A 的交接信息, B 审查/注释/复核与正式 CR 尚未开始. 维护者继续从 8.1 调度, A 不创建或消息另一个审查会话; 不执行 spec 同步、归档或清空问题单.

### 开发材料整理 (Agent-A, 2026-11-04)

维护者要求本轮把开发测试移出正式工程, 并约束过度防御与过量测试. 实际调整如下:

- Engine/Shader/Checks、五份接口/程序 JSON 和五份 GLSL 已移至 `build/development-checks/establish-shader-interface-build`, 只由本地 CMake 工程注册. 正式 Shader CMake 保留库/驱动和接受显式输入路径的生成规则, 移除默认样例包与全部检查目标; 正式 BuildDriver 不再含分配注入钩子.
- includeSystem/includeLocal/releaseInclude 的声明注明 glslang 固定 override 名称, to_json/from_json 注明 nlohmann_json 的 ADL 固定钩子. 第一方普通函数继续 PascalCase.
- 早期 Core/Platform 两份测试专用头移入 `build/development-checks/legacy-hooks`, 从 13 个正式文件移除 56 个测试宏分支及 15 处测试前置声明/friend. 有 #else 时保留原产品分支, 没有引入替代测试入口; 原始文件与变动摘要保存在该目录的 removed-hooks.json. 只收整受影响签名排版与不再使用的 include.
- AGENTS 保持简短入口; C++ 技能明确不为不可达状态增加防御、不重复内部合同校验及命名例外需就近注释; 验证技能规定 build 归类存放、最小代表检查、复用与耗时/Token 停止条件; CMake 技能禁止正式测试目标/故障钩子/自动恢复测试. OpenSpec context、Developments 与 Build 同步. Architecture 只把“测试组织未定”的旧句改为路由到开发约定, 技术合同不变. 历史 archive 不修改.

| 本轮实际检查 | 命令、结果与证据 |
|---|---|
| 新目录正式默认构建 | `cmake -S . -B build/development-checks/shader-clean -G "Visual Studio 17 2022" -A x64`, 然后 `cmake --build build/development-checks/shader-clean --config Debug --parallel 3` 与 Release 均成功. configure-shader-clean.log、build-shader-clean-debug.log、build-shader-clean-release.log 在 build/development-checks. 正式工程只生成 PhysaraShader/PhysaraShaderBuild 两个 Shader vcxproj, 没有检查目标、默认样例包或对本地测试输入的引用. |
| 旧钩子清理后的受影响构建 | 沿用上述新目录, 对最终 Core/Platform/RHI 输入各补建 Debug/Release 默认目标成功; build-cleanup-debug.log、build-cleanup-release.log. 没有重复清理或编译输入未变的四库, 第一方无新增编译警告. |
| 显式路径生成与增量 | 本地 CMake 以 PHYSARA_SHADER_BUILD_TOOL 指向刚构建的正式工具, 从当前模块读取同一生成规则, 在 `build/development-checks/shader rule check` 生成一个 Conformance 包, 不重复构建四库或 CPU 批量 runner. `python -B build/development-checks/CheckCleanup.py` 通过: 无改动不运行生成/编译且文件摘要/mtime 不变; 仅修改本地 Common include 会更新包, 恢复后包摘要回到原值; 删除必要 depfile 会恢复; Release 使用对应正式工具并独立输出. 六次构建命令、退出码及工具 SHA 在 cleanup-rule-runs.json, 对应 rules-*.log. |
| 正式宿主短流程 | 两配置 `Physara.exe --hidden --poll-limit=3` 各执行一次并返回 0, 日志为 smoke-cleanup-Debug.log / smoke-cleanup-Release.log, exe SHA 与退出码见 cleanup-smoke.json. 这只验证清理后的正常启动/退出, 不代表原生资源全矩阵或 Shader Draw 通过. |

复用范围: 布局/解析/include 图/编译/最终 IR/包读写算法未改, 公共 Shader 数据合同未改; 两个命名头只添加说明, 驱动只删除测试钩子. 清理前的独立布局/IR/包/错误边界证据按这些未变语义保留, 不冒充本轮工具指纹/可执行文件已运行旧 96 项. 本轮不运行旧完整边界、分配点扫描、旧原生 fixture 或其它生成器矩阵, 不补写同类用例; 受影响构建/生成/启动风险已有充分证据后停止.

最后只将 DeviceHostFactory 声明按 120 列约定换行, 未改 token、宏或续行, 按 L0 核对后不重复构建/运行. 与旧交接源码逐字节比较, Shader 的 18 份 cpp/hpp 中 15 份完全相同, 其余正是上述两个命名注释头与删除测试钩子的驱动. 本轮第一方文档的 312 个本地链接/锚点核对通过, diff 空白检查通过; 原样保留的上游 README 不因裁剪目录中缺少上游测试/文档而改写或补回这些目录.

旧 build/shader-interface/handoff.json、source-manifest.json 与 handoff-inputs.zip 已失效, 只代表清理前历史. 当前交接继续以基准 commit 加全部实际未提交内容为准; 本轮摘要、diff、文件与本地输入清单记录在 `build/development-checks/cleanup-handoff.json`. build 及其检查材料可整目录删除, 正式工程不会恢复测试目录, 普通重新构建不需要这些材料. 若跨工作树复现本地检查, 须另行提供当时实际输入. B 尚未审查/注释/验收, 本轮不执行最终 verify、同步或归档.

前轮开发测试整理的源清单为 1327 项, SHA-256: `bcd337d279a690a8a2552da0e5cce1171d9f8ab58acf54e38c3811fb7235aa49`. 以上 cleanup 清单/交接仅代表该时点, 现已被下述第三方精简整改取代. 本轮不自动恢复已清理的旧测试目录. A 本地 strict validate 只作结构核对, 9.1 仍等待 B 验收后执行.

### 第三方精简整改 (Agent-A, 2026-11-04)

维护者指出第三方接入未遵守既有精简源码直接入库要求. 原因是 A 在 design 决策 5 选择复用上游工程, 仅禁用未用 target, 却保留了大量未用实现和构建目录; 不是原文档缺少“实际需要的源码/头/许可, 由项目 CMake 直接组织”的要求. 本轮已纠正实现并强化 physara-cmake、Architecture 1.1、ThirdParty/README 和 OpenSpec context 中“关闭 target 不能代替源码裁剪”的说明. 固定 commit 与原始归档 SHA-256 不变, 不重新下载或声称本轮再次核对已清理的原始下载 ZIP.

| 依赖 | 整理前文件数 | 当前文件数 | 当前保留范围 |
|---|---:|---:|---|
| glslang | 148 | 104 | GLSL/SPIR-V 编译、Windows、默认资源限制、实际传递头与许可, 加固定 build_info.h; 移除 C 包装、StandAlone 与上游工程/版本输入 |
| SPIRV-Tools | 972 | 134 | 基础 parser/validator、必要 util/val 与私有 PCH 头、许可, 加六份固定表/版本文件; 移除 optimizer/fuzz/reduce/link/lint/diff、未用 option 实现、上游工程/脚本/工具/测试/示例 |
| SPIRV-Headers | 89 | 10 | 实际包含的 unified1 头、LICENSE 与 REUSE 归属声明; 生成用语法 JSON 不进入普通构建 |
| SPIRV-Cross | 22 | 19 | 四个 core 实现、实际头与许可; 移除上游工程、README 和版本模板 |

四库合计 1231 -> 267 个文件, 原始文件字节合计 21,541,564 -> 10,118,696. 260 份保留的原始文件与本轮清理前 backup ZIP 逐字节一致, 没有修改算法或版权说明; 另七份生成源码在裁剪前从固定版本模板、完整语法、registry 和生成脚本重新生成, 与当时实际构建产物按 LF 规范化比较完全一致. build_info.h 取固定 CHANGES.md 版本, build-version.inc 使用固定完整 commit, 不含本机路径/墙钟. 生成命令与摘要在 `build/dependency-curation/applied.json`, 逐文件比较摘要在 source-audit.json, 清理前真实源码在 before.zip. 这些是忽略的本地整改材料, 不作为新依赖登记簿或自动恢复入口.

ShaderToolchain/CMakeLists.txt 现只显式创建 glslang、glslang-default-resource-limits、SPIRV-Tools-static、spirv-cross-core, 分别编译 39/1/77/4 个实际源文件. 不加载四库上游 CMake, 不创建其额外 target 或查找 Git/Python/系统替代库. /EHs、RTTI、Debug iterator 及 Tools 的 Debug context 配置保留, 第三方 flags/include 局部限定. 必要生成源码随库编译, 普通构建不运行生成器, 删除 build 后仍可重建. 源码升级才从固定完整归档在 build 中复现, 文档中的普通 Python 前提和原两处上游构建适配说明已更新.

本轮实际运行:

- `cmake -S . -B build/dependency-curation/clean -G "Visual Studio 17 2022" -A x64` 成功, configure-clean.log 无 Python/Git 查找或依赖下载/安装步骤.
- `cmake --build build/dependency-curation/clean --config Debug --target PhysaraShaderBuild --parallel 3` 与 Release 各成功一次, build-debug.log / build-release.log 无新增编译警告或错误. 从空 binary tree 构建实际消费者, 未利用旧工具或库替代本轮链接.
- `python -B build/dependency-curation/Smoke.py baseline build/bin/Release/PhysaraShaderBuild.exe` 在裁剪前记录当时正式工具结果; 裁剪后分别以 Debug / Release 标签指向 clean/bin 对应新工具执行. 只有一组最小常量/独立纹理/sampler/跨阶段 IO 输入, 三轮均完成 generate、build、check. 结果的 ABI 相同, 最终 VS/FS payload SHA-256 均为 `260322823e0515ae167c8a29b536c49704237004a9e3d8e512c6a582870bd78c`. 工具 SHA、完整命令/退出码和日志在 smoke/各标签/result.json 及同目录日志; 工具指纹/包内容身份随真实输入变化, 不要求与旧工具相同.
- 原始文件/生成快照审计通过, 四库目录无上游 CMake、cmake 脚本或 Python. 依赖清单、构建说明、设计/任务与第一方文档链接/空白核对后停止; 不新增测试框架或边界用例矩阵.

本轮没有修改第一方 Shader 算法、公开数据合同或 Runtime/Core/Platform/RHI 的源内容. 复用此前已记录的未变布局/解析/编译/反射/包语义结论; 不恢复或运行旧 96 项 runner、分配点探针、原生全矩阵、其它生成器或 native smoke. 本轮开始时先前忽略的交接/测试目录不在当前树, 历史日志路径只作当时记录, 不声称可以直接从当前 checkout 重现这些历史检查. 当前源码与本轮实际输入/日志由 `build/dependency-curation/handoff.json` 和配套清单记录; build 仍可整目录清理. 独立 B 尚未审查/注释/验收, 8/9 节继续待办, 不执行最终 verify、同步或归档.

当前 363 项源清单 SHA-256: `2cfc78ed5aad015bf062137e286f04065da8820f785d25b04d975d90c1cdd1c8`, 记录在 source-manifest.json. 为避免自引用排除 tasks、CodeReview 与两份 Developments 状态文件; 它们及所有实际未提交/新增/删除内容另由 full-change-manifest.json 与 actual-change.diff 记录. 本轮 15 份第一方 Markdown 的 313 个本地链接/锚点通过, diff 空白检查通过; A 的 `openspec validate establish-shader-interface-build --strict --json` 为 1 change / 0 issues, 仅证明结构正确, 不代替 B 验收后的 9.1.


<a id="a-首轮审核整改-2026-11-05"></a>
### A 首轮审核整改 (2026-11-05)

维护者交回 B 初审后, A 按 CR-001 至 CR-009 修正并保留 B 的全部注释/结论/复核字段. 九项 A 整改项与任务 8.2 已完成, 仍由 B 逐项复核. 当前 v2.1 基准 commit 仍为 cb5660942c5f0f02a8afb38cc6dca7e0691c6e8b, 无暂存改动. 逐项原因与结果见 [CodeReview](../../../../Docs/CodeReview.md); 设计取舍及三处 glslang 私有适配见 design 决策 14. 本轮没有修改长期 Architecture 技术合同或扩大 D03-T01 范围.

本轮所有脚本、JSON/GLSL 样例、故障输出目录、CMake 消费者和隔离副本只在忽略的 build/shader-review-fixes 下. 这是可删除的本地证据空间, 正式工程不依赖或恢复这些材料. 交接不同工作树时提供真实本地输入, 不只提供路径.

| 对应风险 | 实际命令、结果与范围 |
|---|---|
| 当前受影响构建 | 复用第三方精简整改的 build/dependency-curation/clean 配置和未变 Core/RHI/其余依赖, 配置仍选择 VS2022/MSVC 19.44.35228/SDK 10.0.26100.0, 目标系统 10.0.26200. 当前 glslang 适配与 Shader 核心/驱动 Debug、Release 都已重建成功, 无新增警告. 最后命令为 cmake --build build/dependency-curation/clean --config Debug 或 Release --target PhysaraShaderBuild --parallel 1, 日志 build-debug-final.log、build-release-final.log. |
| CR-001/002/004/005/006/007/009 | python -B build/shader-review-fixes/Regressions.py Debug 与 Release 均通过, 每配置 48 次工具调用(含准备正常包、预期失败及恢复). regressions-final-Debug/Release 保存当前 exe 摘要、逐次命令/退出码/诊断和实际输入; 预期来自旧包 SHA、原行号/声明边、独立 SPIR-V 装饰及 ABI/身份. 各次调用合并覆盖七组独立缺陷, 没有恢复旧全量 runner. 首次裸 CR 行号失败及随后修正前的结果单独保留, 不冒充最终通过证据. |
| CR-003 | build/shader-review-fixes/parallel 的本地 CMake 工程直接读取正式生成函数, 使用当前 Release 驱动, 两个消费者共享 TwinPackageTarget. cmake --build build/shader-review-fixes/parallel-out --config Release --target CheckBoth --parallel 2, 首轮包编译一次、无改动重建零次, 两次退出 0. 结果 parallel-final-results.json, 日志 parallel-final-first.log、parallel-final-unchanged.log. 不再使用多个独立消费者只依赖数据文件的旧用法. |
| CR-008 | Fingerprint.py 的隔离输入复用本轮未变的第三方/Core/RHI 静态库, 只重建隔离 Shader 核心/驱动. 初次生成/构建后只向隔离 json.hpp 追加一行注释并构建; 实际命令、最后成功日志和包身份在 fingerprint-results.json、fingerprint-runs.json、fingerprint-build-after-controller-retry.log、fingerprint-after-final-generate.log、fingerprint-after-final-compile.log. 正式 JSON 头未修改. |
| CR-008 的复用范围 | 隔离运行完成后仅又为最终源图加入用户 #line 防歧义拒绝. CMake 指纹公式、JSON 输入依赖和 SDK 字段均未再变, 故复用这一隔离差分证据; 该隔离 exe 不代表最终源图版本. 最终 #line 和原行号行为由当前 Debug/Release 回归证明. 实际 SDK 仅测 10.0.26100.0, 未测 SDK 切换, 不声称第二 SDK 构建通过. |
| 文档与边界 | 同步 Build、Interface、ThirdParty 来源适配说明、design 和 delta spec; 修复 RHI README 指向已删除 Build 锚点的链接. 开发计划只更新 A 完成整改/待 B 复核状态, B 初审结论和全部复核框保持原样. |

隔离 JSON 头差分的 compilerFingerprint 从 28e3f78a7e258fe6a8ce9749bea4101189ce4ca11575b9d156e490891c68b3a1 变为 350cecbbd091f9e986206a43a5a3ca26b44717dda66dcafe84003c8230bfdbd9; contentHash 从 80fcfab1a29fc0595f568c031b7ff1d2f991dbd39b3b27e83accfacab0accc5f 变为 3701921d3dbdc7b8b05d79b98e6bc08b81c91873f8fc2bf54282c013f5c63924. 两次 ABI 均为 ba4bcaae35a70ac572abcc41bb485c2afdccae6040a2af26fda3b395acf7e48b. 这证明 JSON 输入进入实际工具身份, 不把纯注释改动当作 ABI 变化.

MSVC 在若干主构建/隔离构建中出现 C1001, 位置涉及不同 STL 模板. 失败日志全部保留; 未更换编译选项、关闭 Debug 检查或加入产品重试逻辑. 只有实际成功重建后的 exe 才用于上面的通过记录, 不用旧可执行文件掩盖失败, 也不宣称单线程解决了编译器问题. 本次没有新增性能结论, 未重跑输入/合同未变的 GPU 或 Runtime 原生矩阵.

交接源码清单沿 B 初审 363 项的同一顺序、原字节与删除标记计算, 摘要从 b8b80bf77f7a9a1ec7923ac55c0ba1cfd99935aa932e8fd97f984f08c1b97d2f 变为 69bee9376adba3b311db7dee1892f45b9351aa9fb679c151d0c3b9db58467ddf. source-manifest.json 排除四份状态文档以免自引用; full-change-manifest.json、actual-change.diff、delta-from-b.diff、source-delta-from-b.json 与 handoff.json 另记录完整工作区输入、相对 B 的实际修正、编译身份/当前 exe 和证据位置. B 的全部新增注释纳入当前输入. 当前 Debug exe SHA-256 为 7352d351f391606c4cd749df5edb6b936ca93d843fdd2f8944b3fb1dbdae331f, Release 为 c179eee3ad8032b6f0a923146bf43e4b8aa79ace87883e0ef525b3973cfccddb.

下一步由维护者调度 B 对 v2.1 修正执行 8.3. 本轮只完成 A 整改自检, 不填写 B 通过、执行 9.1 最终 verify、同步或归档.

收尾文档核对: 11 份相关 Markdown 的 206 处本地链接/锚点通过, Shader 内 B 输入已有的 77 行独立注释均保留, 问题单的 B 字段逐行不变; docs-audit.json 记录 23 项完成、8.3/9.1/9.2/9.3 待办. `git -c core.safecrlf=false diff --check` 通过. `openspec validate establish-shader-interface-build --strict --json` 返回 valid=true、issues=[]; 这只是当前规划产物的结构检查, 不代替 B 验收或任务 9.1 的最终 verify.

<a id="b-首轮整改复核-2026-11-05"></a>
### B 首轮整改复核 (2026-11-05)

B 按维护者调度复核 v2.1。实际基准 commit 为 `cb5660942c5f0f02a8afb38cc6dca7e0691c6e8b`，363 项输入摘要 `69bee9376adba3b311db7dee1892f45b9351aa9fb679c151d0c3b9db58467ddf` 与当前工作区逐项匹配。相对 B 初审注释版的 19 份改动已核对，其余输入保持清单一致。Debug/Release 驱动摘要分别为 `7352d351f391606c4cd749df5edb6b936ca93d843fdd2f8944b3fb1dbdae331f`、`c179eee3ad8032b6f0a923146bf43e4b8aa79ace87883e0ef525b3973cfccddb`。

- CR-002 至 CR-009 已由 B 复核通过，逐项依据保存在 [CodeReview](../../../../Docs/CodeReview.md)。B 审阅 A 的定点回归逻辑后，仅调整输出根并执行 Debug/Release 各 48 次工具调用。原有七组回归均通过。B 另从全新 VS2022 输出目录验证双消费者并行生成一次、无改动零次。CR-008 复用未变指纹公式的 A 隔离运行，已核对真实前后包和对应工具身份，SDK 切换仍未实测。
- CR-001 保持开放。B 令旧包只读，再修改 VS 输入并执行实际消费者构建，最终替换以 Win32 5 失败且旧包保留。解除只读后原命令返回 0，却零次重新编译。旧包内容身份为 `6be569f3…`，完成 stamp 为 `086e7eac…`，直接编译当前输入也得到后者。需要补充失败标记失效与 CMake 恢复检查，8.3 的验收条件尚未满足。
- B 将 BuildDriver.cpp 第 96 行的成功恢复断言改为关联 CR-001 的 TODO，关闭条件为最终包发布失败后下一次 CMake 构建实际重跑。只改一行注释，生产 token 不变。按同口径重算的注释后输入摘要为 `41280ad8d1fa53e1afdd56a87e48e20a308462f81c3ffb8ea85a045c50d5383e`。上述工具运行对应 A 的 v2.1 字节，A 补充修正后需重建并更新工具指纹。
- 本轮本地证据根为 `build/agent-b-review/establish-shader-interface-build/20261105-938127de`，含输入身份、两配置 commands/results、`cmake-consumers/results.json`、失败/恢复日志及 `publish-failure.json`。这些是可清理的 build 材料。A 原始证据保留在原目录。

本次结论为待 A 补充整改及 B 复核。A 的首轮自检记录保留，B 对 CR-001 的复核框及任务 8.3 待办。最终 verify、同步和归档在最终版本通过 B 验收后继续。

<a id="a-补充整改-2026-11-05"></a>
### A 补充整改 (2026-11-05)

针对 B 保留的 CR-001, v2.2 只补最终发布失败后的完成标记清理: WritePackage 返回 Failure 时先删除已刷新的 stamp, 再传播失败. 删除自身失败同样由已有错误边界报告, 不加重试、默认成功或备份/回滚框架. 原 depfile/stamp 先写、包最后发布顺序保留, 辅助输出写失败不会覆盖旧包. 已解决的 B TODO 改为说明真实清理行为的中文注释, 其余 B 注释与复核字段保留.

| 范围 | 实际运行与结果 |
|---|---|
| 当前两配置重建 | 在既有 build/dependency-curation/clean 中执行 cmake --build build/dependency-curation/clean --config Debug 或 Release --target PhysaraShaderBuild --parallel 1, 两次退出 0, 无新增警告. 依赖/Core/RHI 未变, 只重建受指纹与驱动源码影响的 Shader 目标. 日志在 build/shader-review-followup/build-debug.log、build-release.log. 本轮未遇到 C1001, 既有失败日志继续保留. |
| 正式消费者的真实失败恢复 | python -B build/shader-review-followup/CheckPublishRecovery.py 直接加载正式 CMake 生成函数, 在独立 Debug/Release 输出树使用当前驱动. 每配置初建编译 1 次; 修改 VS 后把旧包设为只读, 构建因 Win32 5 退出 1, 旧包 SHA 不变且仍可 check, 已刷新 stamp 不存在; 解除只读后用相同 CMake 命令重建, 编译 1 次, 新包/stamp 内容身份相同且依赖摘要匹配当前 VS 原字节; 再构建编译 0 次. 共 12 次命令, 输入/命令/结果在 consumers-Debug/Release、commands.json、results.json 和配置名开头的逐次日志. |
| 复用及停止范围 | 复用 B 已确认的 CR-001 辅助输出失败保包证据与 CR-002 至 CR-009 结论; 这些实现分支/词法协议/反射/JSON/SDK 公式均未变. 不重跑 48 次回归、隔离 JSON 差分、GPU 或 Runtime 矩阵. 本轮新增材料只在 build/shader-review-followup, 删除 build 不影响正式工程. |

基准 commit 仍为 cb5660942c5f0f02a8afb38cc6dca7e0691c6e8b, 无暂存变化. 已核对 B 注释后 363 项摘要为 41280ad8d1fa53e1afdd56a87e48e20a308462f81c3ffb8ea85a045c50d5383e; 当前源码/合同摘要为 569e22745c52be8e009ab74fcbc75b195e2e677a0571be39219f2023a3be621f, 只改变 BuildDriver 与 Build/Interface/design/spec 四份对应说明. 问题单、开发计划和本 tasks 单独列入完整工作区清单, 避免摘要自引用. source-manifest.json、source-delta-from-b.json、full-change-manifest.json、delta-from-b.diff、actual-change.diff 与 handoff.json 记录实际输入、完整修改/新增/删除和当前构建身份.

本轮 Debug exe SHA-256 为 5a5ac2fb618e3632c4df3a643126ade737891a1ea77d5adc4c69566a367b1301, Release 为 9b467b0fcc81183d657cc07d3ea1346f9bc7eeaf5a50a7ccb228145ec287fe4f. 这些与本轮结果逐一匹配; 旧 v2.1 工具/输入只作为明确复用范围内的历史证据. 8.2 补充自检已完成, 8.3 及全部 B 确认由维护者调度的 B 继续; 未执行最终 verify、同步或归档.

文档自检通过: 11 份 Markdown、207 处本地链接/锚点均有效; B 的逐项证据、8 项已通过和 1 项待复核字段与本轮接收输入逐行一致, 任务仍为 23/27. `git -c core.safecrlf=false diff --check` 通过. 官方 CLI 的 `openspec validate establish-shader-interface-build --strict --json` 返回 valid=true、issues=[], 记录在本轮 openspec-validate.json; 沙箱对全局 npm 包的读取限制通过受审的沙箱外调用解决, 未重装或变更 CLI. 这只是规划结构检查, 不是任务 9.1 的最终 verify.

<a id="b-补充整改复核-v22-2026-11-05"></a>
### B 补充整改复核 v2.2 (2026-11-05)

B 核对 v2.2 的全部 363 项输入及 Debug/Release 驱动，与 A 交接逐字节匹配。源码/合同摘要为 `569e22745c52be8e009ab74fcbc75b195e2e677a0571be39219f2023a3be621f`，相对前次 B 注释版只有 BuildDriver 与四份对应合同说明变化。当前 Debug/Release exe SHA-256 分别为 `5a5ac2fb618e3632c4df3a643126ade737891a1ea77d5adc4c69566a367b1301`、`9b467b0fcc81183d657cc07d3ea1346f9bc7eeaf5a50a7ccb228145ec287fe4f`。

- B 将 A 的定点恢复检查放到独立输出目录，复用正式 CMake 函数和当前驱动。Debug/Release 原有只读包失败恢复均通过，确认旧包可读、stamp 缺失、解除只读后编译 1 次，随后无改动 0 次。这部分共 12 次命令。
- 本次新代码引入的 stamp 删除失败分支仍不满足 CR-001。B 在 Release 以只读句柄允许 stamp 的共享读写、阻止删除，再修改 VS 并使旧包只读。构建在删除 stamp 时报文件占用并退出 1，旧包保留，stamp 已刷新。关闭占用句柄、解除包只读后，同一构建命令返回 0 且零次编译。包记录的 VS 摘要仍是 `cf75449f…`，当前源为 `11a36551…`。这两次调用证明解除清理失败后仍能把旧包当作当前输入交付。
- CR-001 保持开放，要求失败状态在 stamp 删除受阻时同样阻止后续构建跳过求值。复核以相同文件占用场景为代表，解除失败条件后应实际编译当前源，使包、stamp 与当前源摘要一致，再无改动构建应为零次。CR-002 至 CR-009 的实现未变，保留既有 B 通过结论。
- B 仅在 BuildDriver 的 remove 前增加一行关联 CR-001 的 TODO，标明清理受阻后仍须恢复构建。生产 token 不变，注释后输入摘要为 `6720d0fc024e0d6bfbf212bec2dc4667764170384a9107e1619709f50580660f`。本轮运行对应 A v2.2 输入，后续实际修正需重新构建并更新工具身份。
- 本地证据根为 `build/agent-b-review/establish-shader-interface-build/20261105-followup-e5dc2d81`，包含 CheckFollowup.py、commands.json 的 14 次调用、两配置 results.json，以及 stamp-lock-results.json 和对应日志。这些是可清理的 build 材料。

B 本次结论为待 A 继续整改。任务 8.3 及第 9 节的条件仍待满足，正式问题单继续保留完整往返记录。

<a id="a-完成时间整改-v23-2026-11-05"></a>
### A 完成时间整改 v2.3 (2026-11-05)

CR-001 的根因是提前写入的辅助输出时间能掩盖失败的旧包. 本轮先试验统一包为 CMake 首输出与 depfile 目标, 但 VS 仍跟踪三份输出, 占用恢复检查返回零次编译, 该尝试未通过. 对应源码/检查脚本在 failed-package-only.zip, 早期 commands.json 与日志保留. 最终修正不依赖事后清理: depfile/stamp 的可见时间不越过上一成功包, 原子写入口先关闭临时流、设置时间再替换; 相同字节的辅助输出也校正时间. 首次缺少包时仍缺失必要输出. 包最后发布, 内容未变且核对成功时只刷新包的求值时间, 保持字节/内容身份不变并收敛增量规则. stamp 清理失败仍会报错. 私有时间参数默认不指定, 其余现有消费者沿用原行为.

| 范围 | 当前运行证据 |
|---|---|
| 两配置重建 | cmake --build build/dependency-curation/clean --config Debug 或 Release --target PhysaraShaderBuild --parallel 1, 当前最终两次都退出 0, 无新增警告或 C1001. build/shader-review-commit/build-debug-final.log、build-release-final.log. 私有头变化涉及的 Shader 源全部重建; 其余依赖/Core/RHI 未变. |
| 普通失败与占用恢复 | python -B build/shader-review-commit/CheckCompletion.py, Debug/Release 使用当前驱动和正式 CMake 函数. 原只读包失败保留可读旧包、删除 stamp, 恢复编译 1 次、无改动 0 次. B 的 GENERIC_READ、共享读写但禁止删除句柄场景同时改变 include 列表, 当前 stamp 原子替换先失败, 两份辅助输出时间均不越过旧包, 旧包 SHA 不变; 释放句柄/解除包只读后实际编译 1 次, 包记录的源摘要和 stamp 匹配当前输入, 随后 0 次. |
| 完成时间规则的直接消费者 | 两配置只改源 mtime 后各求值 1 次, 包字节/内容身份不变, 后续构建 0 次. Debug 删除 stamp 与 depfile 后恢复 1 次, 包字节不变, 后续 0 次. 两消费者仍共享唯一生成 target. 上述合计 24 次工具调用, 命令/退出码/工具 SHA 与输入在 final-commands.json、final-results.json、final-* 日志、final-consumers-Debug/Release. |
| 复用及停止范围 | 辅助输出发布失败保包由本轮 stamp 占用导致的原子替换失败覆盖, 不扩展相同入口的输出参数矩阵. CR-002 至 CR-009 的词法/布局/IR/JSON/SDK 公式未变, 保留 B 结论; 通用原子写未指定时间时的流程未变. 未重跑 48/96 项 runner、隔离 JSON 差分、GPU 或 Runtime 矩阵. 所有测试材料仅在 build/shader-review-commit, 不进入正式工程或默认构建. |

基准 commit 仍为 cb5660942c5f0f02a8afb38cc6dca7e0691c6e8b, 无暂存变化. 已核对 B 注释后 363 项摘要为 6720d0fc024e0d6bfbf212bec2dc4667764170384a9107e1619709f50580660f; 当前为 12e3991c28c7bc71bf431ab30313eaab6c60f3cbd047cf06e11f9186e3ff2b03, 修改驱动、私有原子写头/实现、Shader CMake 与四份对应合同说明. source-manifest.json、source-delta-from-b.json、full-change-manifest.json、actual-change.diff、delta-from-b.diff 与 handoff.json 记录当前输入和匹配工具. 四份状态文档单列于完整清单, 避免源码摘要自引用.

当前 Debug exe SHA-256 为 fc0b64a217f2f85ca3998b3eae26060f9c84f1d656953d75240fd61c465fa98e, Release 为 790b10621a7de100dc74bea93745689d17a6e6315d7e4484e55b481393891ca5. A 的 8.2 补充自检完成, CR-001 与 8.3 继续由 B 复核; B 的原有八项通过、补充发现和结论均保留. 未执行最终 verify、同步或归档, Architecture 技术合同未修改.

文档自检: 11 份 Markdown 的 208 处本地链接/锚点通过, CodeReview 的全部 B 字段与本轮接收版本逐行一致, 8 项通过/1 项待复核及 23/27 任务状态未由 A 改为验收. `git -c core.safecrlf=false diff --check` 通过; `openspec validate establish-shader-interface-build --strict --json` 返回 valid=true、issues=[], 仅作为结构校验. 证据在 docs-audit.json、diff-check.log、openspec-validate.json. 本轮按 design 决策 12 已声明的 VS2022/MSVC x64 multi-config 入口验证, 未扩大生成器支持范围.

<a id="b-最终复核-v23-2026-11-05"></a>
### B 最终复核 v2.3 (2026-11-05)

B 验收通过 v2.3 的 D03-T01 CPU Shader 构建链。CR-001 至 CR-009 全部经 B 确认关闭，任务 8.3 完成。验收基准 commit 为 `cb5660942c5f0f02a8afb38cc6dca7e0691c6e8b`，全部工作区修改、新增与删除已核对。363 项源码/合同摘要为 `12e3991c28c7bc71bf431ab30313eaab6c60f3cbd047cf06e11f9186e3ff2b03`，与 A 的 v2.3 交接相同。Debug/Release 驱动 SHA-256 分别为 `fc0b64a217f2f85ca3998b3eae26060f9c84f1d656953d75240fd61c465fa98e`、`790b10621a7de100dc74bea93745689d17a6e6315d7e4484e55b481393891ca5`。

- **实际复核范围** 相对前次 B 注释版的 8 份源码/合同差分，包括驱动、私有原子写入口与消费者、Shader CMake 以及四份同步说明。辅助时间先在已关闭临时文件上设置再替换，原字节相同分支也校正时间。未指定时间的原子写消费者沿原流程执行。包是首输出及 depfile 目标，成功发布或成功核对相同字节后才推进求值时间。
- **本轮 B 运行** 审阅 CheckCompletion.py 的断言后，只调整仓库/输出定位，在独立目录使用当前正式生成函数和驱动运行 Debug/Release。共 24 次调用通过，覆盖普通发布失败、stamp 共享读写但禁止删除并改变 include 列表的失败、恢复编译 1 次及无改动 0 次、输入仅 mtime 变化的收敛，以及 Debug 缺失辅助输出恢复。旧包 SHA/可读性、辅助时间、当前源摘要、包身份与 stamp 均有实际核对。
- **已有问题的最终状态** CR-001 的两轮补充发现均由本次时间合同和上述检查关闭。CR-003 受影响的 CMake 双消费者链在本轮同样保持唯一生成。CR-002、CR-004 至 CR-009 的布局、源图、反射、JSON 和指纹公式未变，沿明确范围复用既有 B 结论。
- **注释与版本** B 已核对 A 为完成时间、同字节求值及原子替换补充的中文注释，其含义与实际实现一致。当前实现的 CR-001 TODO 已由对应行为说明替代，验收输入保持上述 v2.3 摘要。
- **证据位置** 本轮 B 的可清理本地材料位于 `build/agent-b-review/establish-shader-interface-build/20261105-v23-fd71884b`，包含 inputs.json、CheckCompletionB.py、final-commands.json、final-results.json 与逐次日志。A 的原证据保留在 build/shader-review-commit。支持环境为本机 VS2022/MSVC x64 multi-config 与 SDK 10.0.26100.0，既有 SDK 切换、其它生成器及后续 GPU 工作包的未覆盖范围继续按原记录说明。

当前共 24/27 项完成。下一步由 A 对此验收输入执行 9.1 最终 verify，完成 9.2 留存及阶段状态，再按 9.3 同步和归档。验收后代码或构建输入变化时，受影响范围重新交 B 复核。问题单在归档成功后按模板重置。

<a id="final-verification"></a>
### 最终 verify 与审查留存 (2026-11-05)

Agent-A 在 B 最终验收后执行原生 openspec-verify-change. repo-local / spec-driven, 四类规划产物可读且完整. 当前验收输入仍为 v2.3, 363 项原始字节/删除标记逐项匹配摘要 12e3991c28c7bc71bf431ab30313eaab6c60f3cbd047cf06e11f9186e3ff2b03. A/B 两个独立目录的两配置共四份包、对应 exe、当前源摘要及 stamp 再核对一致, 未使用构建失败后的旧程序. 本轮没有修改验收后的产品源码或构建输入, 因此复用已匹配的行为检查, 不重复运行测试矩阵.

| verify 维度 | 结论 |
|---|---|
| 完整性 | 27/27 项完成, 10 条 ADDED 要求、29 个场景全部映射. 初始 24/27 的三项收尾动作已在实际完成后逐项消解, 无缺失需求或未完成任务. |
| 正确性 | 10/10 要求、29/29 场景由下表的当前实现、匹配 A/B 运行及明确范围的历史证据覆盖. 原有临时矩阵可能已清理, 不把历史工具当作当前运行; 未变算法的复用按前文记录. |
| 一致性 | design 的 14 项决定与当前依赖方向、所有权、profile、单包发布、时间合同和测试隔离一致. 文件/命名空间/私有 helper/有界错误/中文注释与项目模式及 B 注释相符. 三处 glslang 私有适配及命名例外均有说明. |

要求与场景的对应关系如下. 所有行均已核对, 没有跳过的适用 verify 检查, 没有新增实现层 CRITICAL/WARNING/SUGGESTION. SDK 切换、额外生成器和后续 GPU 工作不属于此次验收声明.

| 要求 / 场景数 | 实现定位与证据 |
|---|---|
| 权威接口 / 3 | ShaderSchema.cpp:130/325 与 Interface.cpp:55/251. 手算 std140/std430、生成头 static_assert、受控拒绝输入沿前文布局/独立消费者证据复用; CR-006/007 的发布前名称/插值拒绝由当前 B 结论补足. |
| 逻辑绑定 / 2 | ShaderSchema.cpp:105 与 RHI::VulkanBindingOffsets, Interface.cpp 的绑定生成和 Spirv.cpp 的 set/binding 核对. 不混淆同号 b/t/s, 未引入 GL 原生单元. 前文真实纹理/sampler 与冲突检查复用. |
| canonical profile / 2 | Compiler.cpp:185 的固定环境和 Spirv.cpp:13/132 的最终验证. 已有基线与不支持能力拒绝证据, CR-002 的扩展白名单回归由 B 两配置确认. |
| 完整源依赖 / 3 | SourceGraph.cpp:133/177/379、Compiler.cpp 的依赖和引用链. CR-002/004/005 的三种换行、根边界、联接拒绝与实际引用位置经 B 确认; v2.3 再覆盖 include 列表改变后的真实恢复. |
| 最终 IR / 4 | Spirv.cpp:63/341/386 与 Package.cpp 的重反射. 独立布局/合法错配 IR、阶段兼容与未活动声明证据沿未变实现复用; CR-007 用实际 IR 装饰补足插值核对. |
| 完整身份 / 3 | ShaderSchema.cpp:499、Compiler.cpp:294/355 与 Shader CMake 的实际源码/JSON/SDK 指纹. 历史跨绝对目录、格式保持 ABI 和变体失效证据复用; CR-008 的真实 JSON 隔离差分及本轮源/包/exe 核对支持当前身份. |
| 完整包发布/消费 / 3 | Package.cpp:191/294/330、BuildSupport.cpp:246 和 BuildDriver.cpp. 读取/摘要/ABI/损坏包历史证据沿未变格式复用; CR-009 完整 JSON 拒绝与 CR-001 多轮最终恢复均经 B 关闭. |
| 离线/增量构建 / 4 | ShaderToolchain/CMakeLists.txt 和 Shader CMake 显式目标/输出/DEPFILE. 精简依赖后从空 binary tree 构建证据、当前匹配两配置重建和 B 独立 24 次调用覆盖顺序、唯一写入、1/0 收敛及缺失输出恢复. |
| 可定位失败 / 3 | BuildSupport.hpp:53、CompilerEnvironment.cpp:13 与 BuildDriver.cpp:17. 字段/源图/不可写输出由当前回归证明; 未变的可传播分配边界和 noexcept 致命 OOM 处理沿维护者已批准边界及独立进程证据复用, 不关闭 Debug 检查. |
| 真实开发消费者 / 2 | build 中的 Conformance/布局/独立 CPU 消费者历史记录及当前 TrimSmoke 实际输入. 正式默认构建只有产品库/驱动和接受显式路径的规则, 无测试目标/默认包. D03-T01 完成声明不延伸至 GL Pipeline、Draw 或 GPU 读回. |

`openspec validate establish-shader-interface-build --strict --json` 在匹配验收输入上通过, valid=true、issues=[]. 这与上述实现/场景核对分开记录. 新运行仅为来源/身份及结构/文档核对, 未新增行为矩阵. 本地记录位于 build/shader-finalize 的 workflow-start.json、accepted-inputs.json、verify-report.json 和 verify-validate.json; accepted-before-close.zip 保留清理问题单前的原文与规划输入. build 仍是可清理空间, 永久结论以本 tasks 为准.

<a id="review-retention"></a>
### 已关闭问题与注释留存

全部问题均由 B 于 2026-11-05 确认关闭, A 不代签 B. 以下摘要与前文 A/B 往返记录共同保留根因、修正和实际复核依据, 不依赖归档后已重置的当前问题单.

| CR / 优先级 | 修正及 B 关闭依据 |
|---|---|
| CR-001 / P1 | 必要辅助输出先写、包最后提交; v2.1/v2.2 的新 stamp 与清理受阻发现进一步改为原子发布前钳制辅助时间, 成功包推进完成时间. B v2.3 两配置独立 24 次检查证实失败保旧包、占用/依赖列表改变后编译 1 次、随后 0 次、时间变化及缺失输出均收敛. |
| CR-002 / P1 | 原字节保留身份, 扫描与 glslang 共用 LF 编译视图. B 两配置确认 LF/CRLF/CR 都拒绝白名单外扩展, include/续行与原第 2 行诊断正确. |
| CR-003 / P1 | 每程序唯一 PackageTarget, 多消费者共享 target. B 首建包生成 1 次、无改动 0 次, v2.3 双消费者仍保持唯一写入. |
| CR-004 / P1 | 首读拒绝任一路径组件的 Windows reparse point, 再按普通路径检查允许根. B 确认目录联接首次及改指目标均明确失败并保留旧包, 入口说明一致. |
| CR-005 / P2 | 固定 glslang 的只读观测回调报告原 include 行, 精确标记声明边; 用户 #line 拒绝. B 确认同目标第 5 行未展开、第 7 行展开, 未执行位置不进入实际引用链. |
| CR-006 / P2 | 声明名称复用固定编译器词表, 拒绝保留字. B 确认 Globals.sample 在发布前失败、诊断定位块/字段、原 Manifest 不变, 合法基线通过. |
| CR-007 / P2 | 非跨阶段端点拒绝非默认插值, 所有活动 IO 从最终 IR 装饰反射. B 核对 FS flat 拒绝、smooth ABI 稳定及合法跨阶段 flat 与 IR 一致. |
| CR-008 / P2 | 指纹纳入实际 VS SDK 及两个固定 JSON 头、仓库相对身份与 configure 依赖. B 核对隔离 json.hpp 单独变化使 compilerFingerprint/contentHash 改变而 ABI 不变; SDK 切换未实测范围保留. |
| CR-009 / P2 | 共享 JSON 字节边界拒绝原始 NUL. B 两配置核对声明/程序/重算完整摘要的包元数据后缀均失败, 合法 Unicode 转义仍按领域规则处理. |

B 初审直接完善了 ShaderSchema/Interface 的布局与绑定注释、Compiler/Spirv 的 manifest/链接/IR 活动范围、SourceGraph/Package 的借用与游标寿命、BuildDriver/CMake 的输出与共享前置说明. 后续核对三处 glslang 私有适配的版权/用途及驱动、BuildSupport、CMake 的完成时间注释, 最终 TODO 已由实现说明替代. 非阻塞的 RHI 历史检查锚点也经 B 确认修复. B 最终版本、输入、工具摘要、覆盖/排除项及证据根见上方 B 最终复核 v2.3, 九项均为实际复核通过而非撤回.

<a id="archive-completion"></a>
### 同步与归档完成 (2026-11-05)

已创建[shader-build 主规范](../../../specs/shader-build/spec.md), Purpose 与 delta 原文一致, 10 条要求和 29 个场景逐块相同, 无残留 delta 操作标题. 同步后的全部 9 份主规范严格校验通过. change 已移动至 openspec/changes/archive/2026-11-05-establish-shader-interface-build, .openspec.yaml 随目录保留且摘要未变; 外部相对链接按归档层级更新, 需求语义不变.

在 B 结论、九项 CR 修正/复核摘要、注释覆盖、当前输入/工具与检查限制留存后, 已按模板重置 Docs/CodeReview.md. 9.3 在同步、实际移动和重置均成功后才勾选, 最终 27/27; 原生 verify 最初列出的三项收尾待办全部完成, 无未关闭的实现问题或跳过的适用检查. 开发计划仅完成 D03-T01 并引用本记录, 下一工作包为 D03-T02, 后续阶段未被提前勾选. 本轮没有修改 B 验收后的产品源码、构建输入或长期 Architecture 技术合同.

归档前的当前问题单和规划原文备份、来源/包身份核对、结构校验及完成状态位于 build/shader-finalize. 它们属于可清理材料, 本 tasks 保留永久决定和验收结论, 正式工程不依赖这些本地文件. 未执行提交、推送或发布.

归档后核对: 活动 change 列表为空, 全部 9 份规范再次严格校验通过; 12 份相关 Markdown 的 170 处本地链接/锚点有效, 当前问题单与模板字节一致, 27/27 完成, D03-T02 及后续工作包正文未变. 359 项非 OpenSpec 既有输入仍匹配 B 验收清单. 结果见 build/shader-finalize/post-archive-audit.json、final-validate.json 与 final-diff-check.log.
