# Design

## Context

动机与范围见 [proposal](proposal.md), 行为合同见 [shader-build spec](specs/shader-build/spec.md). 本设计细化 [D03-T01](../../../../Docs/Developments/D03-D05-FramePipeline.md#d03-t01) 与已采纳的 [Architecture 1.3](../../../../Docs/Architecture.md#shader-build)、[8.1](../../../../Docs/Architecture.md#shader-runtime), 不改变长期模块边界.

规划基准为 `090e5d33270d362a76d0167bb203deae4b85027b`. 开始规划时工作树干净、无活动 change, D02 及 `fix-validation-cpu-boundaries` 已归档, 审查问题单为未启动模板. 当前 Engine 只有 Core; 没有 Shader 模块或 Shader 源码, ThirdParty 尚未保留以下编译库. RHI 已有完整 Shader/BindingLayout 合同, OpenGL 对应实现仍属于后续工作包. 下述文件、target、profile 和包格式均为本次拟实现内容, 不是现有可用 API 或已验证结果.

## Goals / Non-Goals

**Goals:**

- 用独立于业务消费者的生成/编译核心, 从声明到最终 IR 建立可检查的 ABI 链.
- 固定首个受限 profile、来源版本、生成依赖和失败退出, 让后续 Pipeline 准备能消费完整、可识别的 CPU 包.
- 通过真实常量/纹理程序和独立的数值预期检查布局、反射及构建行为.

**Non-Goals:**

- 不实现 GL lowering/link、原生绑定分配、Pipeline、Draw、GPU 同步或产品呈现. 不因测试源可编译而开放当前 backend 的能力位.
- 不建立 Shader worker 队列、watcher、热重载代管理、native program binary cache、通用变体枚举框架或独立 Tools 模块. 后续阶段复用本包的编译核心, 其并发及预算按真实消费者另行实现.

## Decisions

### 1. 在 Shader 模块内建立单向的构建依赖

采用 `Engine/Shader` 内的 C++20 库 `PhysaraShader` 和薄构建驱动 `PhysaraShaderBuild`. 驱动只负责读取请求、调用同一生成/编译 API、写输出及设置进程退出状态, 不复制第二套规则. 它是模块内部 build dependency, 不增加 Runtime 产品入口.

```text
固定编译库 + JSON + Core/RHI
              |
        PhysaraShader          (不 include 其生成物)
              |
      PhysaraShaderBuild
         /          \
接口声明 -> 生成步骤 -> 生成 C++/GLSL/预期/ABI
                              |
程序请求 + Shader 源 ----------> 编译/验证/反射 -> Shader 包
                              |
                   CPU 检查 / 后续 Pipeline 准备
```

`PhysaraShader` 的公共头只暴露项目值类型、RHI 合同及明确拥有的数据; 编译器、JSON 和 Win32 头留在私有实现. 公开头实际使用 RHI 类型时通过 PUBLIC 依赖提供 RHI, 公共错误使用 Core 的有界值, 因而 Core 为 PUBLIC; RHI 仅供生成实现使用时为 PRIVATE, 生成绑定的消费 target 单独 PUBLIC 提供 RHI. 编译库、JSON 和系统 hash 实现保持 PRIVATE. RHI 继续只依赖标准库; PhysaraRHIOpenGL 不 include ShaderPackage 或链接 Engine. 生成数据头只需标准库, RHI 绑定头单独生成, 生成消费 target 声明实际 include 根及生成前置.

选择模块内驱动, 而非 Python 另写布局/编译逻辑或依赖 Renderer 的生成程序, 是为了保持规则复用且没有生成循环. 不预建新服务接口/Pimpl 转发层; 仅在隔离第三方头的实际边界使用私有实现.

### 2. 采用版本化的受限 JSON 接口与程序请求

正式业务的接口权威输入放在 `Engine/Shader/Interface`, 当前仅保留格式说明; 开发验证的接口/程序 JSON 放在 build 中的本地工程. 使用现有 nlohmann_json 解析. 接口版本 1 声明以下语义, 解析必须拒绝重复 JSON key、未知字段及无效枚举, 不能让拼写错误静默变成默认值:

| 声明 | 首版内容 |
|---|---|
| 数据 record/block | 稳定名称、有序字段、字段类型/定长数组、std140 或 std430、列主序矩阵 |
| 资源集合 | set、资源名称、b/t/s 类别、slot、具体资源类型、buffer record 与可见阶段 |
| stage interface | VS 输入/输出、FS 输入/输出的名称、location、类型及必要插值限定 |
| 程序请求 | 引用的接口、VS/FS 源路径、入口、profile、有限合法 define 值及本次取值 |

程序请求引用 Shader 模块的接口, 不再抄一份字段/binding/IO 布局. 生成 stage IO 声明供入口 include; 顶点属性的业务含义和缓冲数据来源仍由后续消费者定义. 本阶段的 Common 函数、Conformance/LayoutCases 薄入口与请求都是开发验证输入, 只放在 build 下, 正式 Shaders 不保留这些样例; 不预建尚无算法的目录/模块.

首版数据支持 `float32/int32/uint32`、对应 2/3/4 分量向量、float 的 2x2/3x3/4x4 列主序矩阵、上述类型的正数定长数组. UBO 使用 std140; 只读结构化 buffer 使用 std430 的 record 数组并明确 record stride 和正数容量. 不支持任意嵌套 record、运行时数组、bool、64/16 位字段、指针、宿主 size_t、row-major、显式随意 offset、descriptor 数组或 image/UAV. 新类型须扩展合同及证据, 不以原始字符串穿透生成器.

选 JSON 是因为已有固定来源解析器, 无需引入 YAML/DSL 解释器或代码执行能力. 类型集合故意受限, 但覆盖矩阵、vec3 边界和 std140/std430 差异. 程序 define 采用显式有限值清单, 首个程序只有默认配置及一个用于失效检查的合法值; 不枚举所有组合或引入变体缓存系统.

### 3. 分别计算 GPU 布局与验证宿主布局

生成器按所选布局计算 base alignment、成员 offset、矩阵 column stride、数组/record stride 和 block 所需范围. 全部加法、乘法及对齐先检查溢出, 再转换到目标字段宽度. 不通过 `sizeof(glm::...)` 猜测 GPU 布局.

生成 C++ 使用固定宽度标量存储、必要的显式 padding 和 stride 表示, 提供标准布局/可平凡复制检查、`alignof/sizeof/offsetof` 与必要 stride 的 static_assert. 单个 vec3 后接 float 可能复用第 4 分量位置; 不能把所有 vec3 都包装成 `sizeof == 16` 的类型再机械拼接. 矩阵按列及实际 stride 存储, 数组元素显式表达其 stride. Padding 初始化确定, 但不参与 ABI 语义 hash 或字段比较.

区分成员实际字节 extent、分配所需对齐后的 block 大小及数组元素 stride. 反射库报告的 block 尾部 extent 未必包含 C++ 尾 padding; 验证器按同一标准规则比较对应量, 不直接要求任意反射 size 等于宿主 sizeof. std430 的紧凑标量数组、std140 的 16 字节数组 stride、mat3 的列 stride、vec3+scalar 和单 scalar block 均有预先手算的检查值.

每次接口生成输出 C++ 数据头、RHI 绑定头、按 stage 使用的 GLSL 声明、机器可读反射预期、ABI 身份及含各文件摘要的完成描述. 所有文件由同一解析结果产生; 编译前校验完成描述及各产物摘要, 拒绝混合代. 生成多文件过程中只有完整成功后才更新完成输出, 不变内容不重写.

替代方案是手写 C++/GLSL 并靠几个 sizeof 维持一致, 或强制全部按 vec4 排布. 前者仍有双份权威, 后者掩盖数组/矩阵规则并限制既定接口, 均不采用.

### 4. 逻辑 binding 从公开 RHI 合同推导

set 固定解释为 0 PerView、1 PerRenderable、2 PerMaterial、3 PerPass, 只输出实际声明的集合. 每类 slot 首版限定 0..127; b/t/s 同号互不冲突. Canonical binding 计算读取 `RHI::VulkanBindingOffsets` 的默认值: t=0、s=128、b=256、u=384. 最后一类只保留映射语义, 本 profile 拒绝 UAV 声明. 生成流程用公开值计算, 上述数字不是第二份可独立修改的配置.

生成 `BindingLayoutDesc` 时明确设置 registerSpace 及 `registerSpaceIsDescriptorSet = true`, 各集合模式一致; visibility 来自接口, bindings 按稳定顺序输出. 先检查类别、slot、重复映射、加法/窄化及容量再构造描述. descriptor array 首版只允许 size=1; 数据数组与 descriptor array 是两种合同.

活动反射保留逻辑身份和 canonical set/binding, 不包含 GL 单元. GL 紧凑展平、跨阶段 limits、combined sampler pair 及实际 BindingLayout 对象创建留给 D03-T02. 不采用 `set * 大常数 + slot` 直接作为 GL binding 的方案.

### 5. 固定工具源码组合, 只接入当前需要的部分

2026-11-03 核对的上游固定输入如下. 实施时保留真实源码、构建所需生成脚本和许可证, 将下载归档摘要、裁剪范围及本地适配记录到 `ThirdParty/README.md`; 本规划没有下载或验证这些源码的本机构建.

| 依赖 | 固定输入 | 本包用途与来源 |
|---|---|---|
| glslang 16.6.0 | `e1b562a8bed273a02f30b59b66a5d499793cede5` | GLSL -> SPIR-V, [release/commit](https://github.com/KhronosGroup/glslang/commit/e1b562a8bed273a02f30b59b66a5d499793cede5) |
| SPIRV-Tools | `ef96ed763b43b59b33b31b362f09a02b729fa1c9` | validator, 来自该 glslang 的 [known_good.json](https://raw.githubusercontent.com/KhronosGroup/glslang/16.6.0/known_good.json) |
| SPIRV-Headers | `496543121ce6419f23d6fa5d7194ba66c36212d2` | 上述固定验证工具所需语法/头, 来源同 known_good |
| SPIRV-Cross vulkan-sdk-1.4.363.0 | `f11ba9f0b21ba8fc15153d50a2a1ae31ab1cf8f7` | 最终 IR 的 core 反射, [固定 release](https://github.com/KhronosGroup/SPIRV-Cross/releases/tag/vulkan-sdk-1.4.363.0) |

按项目“精简源码直接入库”的既有合同, 由 ShaderToolchain/CMakeLists.txt 显式列出当前编译目标的源码、include、定义与私有依赖, 不再 add_subdirectory 上游工程. 只保留 GLSL/SPIR-V 编译、默认资源限制、SPIRV-Tools 基础 validator 和 SPIRV-Cross core 的源码/传递头/许可. 移除未用 C 包装、optimizer、fuzz、reduce、link、lint、diff、CLI、测试、示例及仓库构建/维护文件; 关闭 target 不能代替源码裁剪. 对可能分配的编译库显式保留 /EHs 与 RTTI, 不把异常改成 assertion 或关闭 Debug 迭代器检查. 不运行获取脚本、FetchContent 或查找本机替代库.

glslang 的 build_info.h 与 SPIRV-Tools 的六份表/调试枚举/版本文件采用固定生成源码快照, 和已有生成 parser 一起直接编译. 获取/升级时使用上述固定源码与语法输入生成并核对, 来源/命令与摘要记录在 ThirdParty/README.md; 普通配置/构建不运行生成器, 不需要 Python、Bison、googletest 或 Vulkan SDK. 保持项目 CMake 3.24 和实际目标的局部 flags. 2026-11-04 维护者指出此前复用上游工程导致保留范围过宽, 本决策据此纠正; 原生成脚本的两处本地适配随不再使用的脚本/CMake 一并移除, 不修改上游算法.

选择此组合是为了复用 glslang 的已固定兼容输入, 并为最终 IR 采用独立反射来源. 不使用 glslang 的 AST 反射替代最终 IR; 其 [README](https://raw.githubusercontent.com/KhronosGroup/glslang/16.6.0/README.md) 明确区分两者. 固定 SDK tag 只标识 SPIRV-Cross 源版本, 不要求安装或链接 Vulkan SDK. 第三方 provenance 继续以现有 README 为记录入口, 不新增一套手工维护的依赖登记簿.

### 6. 声明首个 canonical profile, 不透传任意 flags

首个 profile 命名为 `GraphicsBaselineV1`:

| 项目 | 固定选择 |
|---|---|
| 源与入口 | Vulkan GLSL 450, VS/FS, 每阶段 `main`; 其他入口/阶段明确拒绝 |
| 输出环境 | Vulkan 1.1 / SPIR-V 1.3, validator 使用同一目标环境 |
| 数据/资源 | 决策 2 的类型、std140 UBO、std430 只读结构化 buffer、独立非数组 texture2D 和 sampler |
| stage IO | 生成显式 location, 支持 float/int/uint 标量或向量, 整数接口要求 flat; 矩阵/数组 IO 暂拒绝 |
| 编译策略 | 无自动 binding/location 分配, 不优化重写 IR, 不做 Shader 语义文本 patch; CRLF/CR 仅规范为 LF 编译视图, 原始字节保留身份; Debug/Release 使用相同 Shader 语义选项 |
| builtin/坐标 | 保持既定 canonical Vulkan builtin、列向量及 NDC z=[0,1] 语义; 不增加 Y/Z 修正或 GL base-instance 替换 |

glslang 负责源语义与链接检查, SPIRV-Tools 验证最终字序列, 模块额外检查 profile 的能力/扩展和接口白名单. 禁止 descriptor indexing、subpass input、push constant、写 storage、compute、bindless、其他 native 专属能力及隐式开启未知扩展. 使用 CPU profile 限制而非虚构当前设备 limits; Native 资源上限在 D03-T02 查询并验证.

该版本号属于具体编译合同, 不是声称所有这些输入已经通过 GL lowering. 本包仅保证 canonical 输出满足声明约束; 实际 lowering、clip 与 instance builtin 数值验证仍需后续真实 backend 证据. 选择保守的 Vulkan 1.1/SPIR-V 1.3, 而非按已安装 SDK 自动提高环境, 使内容身份稳定并限制能力扩张.

### 7. 由统一 includer 管理依赖与诊断

不自行实现 GLSL 宏预处理器. 通过 glslang 的 include 回调提供文件内容, 记录实际展开的传递依赖并把规范化逻辑路径交给诊断. 尖括号 include 必须带唯一虚拟根前缀, 如 `<Source/Common/X.glsl>` 或 `<Interface/X.glsl>`, 分别映射源根与本配置生成根; 引号 include 只相对当前文件, 不逐根 fallback. 重复/冲突根标签在配置时拒绝. 规范化后核实实际文件仍在允许根内, 不靠机器环境搜索路径选择输入. 文件身份使用根标签及相对路径, 保留字节内容 hash, 不记录绝对路径到可重建内容 key.

支持规范的 include guard, 但项目声明图必须无环; 驱动从声明的 include 边核对循环, 即使循环被 guard 遮蔽也拒绝. 首版 include 操作数限定为可静态识别的字符串, 不支持宏拼接 include 名称; 对声明边做缺失/歧义/环检查并保守纳入依赖, 对实际展开边记录诊断链. 这样新建一个影子文件不会改变已存在但未重跑的解析结果. GLSL 条件编译仍交给编译器, 不用正则替换语义.

生成接口使用固定逻辑根, 编译前核对 ABI 与生成完成描述. 源内容及 include 回调返回内存由一次编译请求拥有, 存活至 glslang 完成解析/链接; 包内源映射拥有自己的字符串, 不借用临时 path/string_view.

### 8. 反射最终字节码, 并核对完整程序

只有通过 validator 的最终 SPIR-V 才进入 SPIRV-Cross core. 提取 block 成员装饰/stride、独立纹理与 sampler、buffer 类型/访问、set/binding、阶段和 location. 同时记录声明预期和各阶段活动子集, 每个活动资源必须有声明且类型/可见性匹配, 同一资源跨阶段合同必须一致.

按决策 3 区分布局量后核对, 并对 FS 的每个用户输入查找兼容 VS 输出, 检查 location/component 占用、类型及插值限定. VS 未被 FS 消费的合法输出可以存在; builtin 按 profile 独立验证, 不与用户 location 混合. 生成的 stage IO 仍要经最终反射核对, 不能只相信链接器或生成文本.

对 ABI 负例, 使用检查输入构造可独立通过编译/validator、但 offset/stride/binding 与预期不符的 IR, 再交给同一核对入口. 不在生产编译路径提供“跳过验证”开关. 独立手算布局和固定负例用于避免“生成预期与验证器共享同一错误就通过”.

选择反射实际最终 IR, 而非对源或 AST 的估计, 才能给后续 PipelineBindingPlan 提供准确活动输入. 本包不生成 combined image sampler 或 GLSL 460.

### 9. 明确内容身份、ABI 身份与工具指纹

使用稳定 SHA-256, Windows 当前实现通过模块私有 CNG adapter, 不自行实现密码算法、不使用 std::hash 或宿主对象字节. 输入分字段、带长度且有格式版本地编码, 避免串接歧义; 标识顺序稳定, 字段/IO 顺序按 schema 语义保留, 非语义 JSON 对象顺序规范化.

- `abiHash`: 规范化接口语义, 包含字段顺序/类型/布局/stride、集合/类别/slot、资源与 stage IO、生成 ABI 规则版本及实际 RHI offset 合同. 不包含空白或注释式描述.
- `contentHash`: ABI/原始接口与程序请求内容、顶层与全部声明/实际 include 的逻辑路径和内容摘要、生成物摘要、合法 define 取值、阶段/入口、所有有效选项/profile 及 compilerFingerprint.
- `compilerFingerprint`: 固定编译/验证/反射库版本与实际保留源码摘要/本地适配、生成/编译规则版本及影响输出的构建选项. 不依赖第三方目录不存在的 `.git`、构建日期、绝对路径或仅一个显示版本号.

同一 profile 的选项先构成规范化请求, 再交给工具并参与 hash, 不接受无法进入身份的隐藏命令行 flags. 生成内容不嵌入时间戳/机器路径. 构建系统的时间戳用于决定是否运行规则, 内容 hash 用于该规则内识别输入; 不声称能在未执行规则时检测人为保留全部 mtime 的文件篡改. 检查入口另提供强制重新核对模式用于内容身份和迁移目录检查.

当前只保留每个已声明程序/合法配置的一份最新输出及一个事务临时文件, 不按每个 hash 永久积累 cache. 运行开始清理本输出的遗留临时文件, 成功替换时不保留无界历史. Native cache 和运行期淘汰策略留给真实消费者.

### 10. 一份版本化包拥有所有阶段和元数据

采用单个 `.pshader` 文件, 内容为显式小端定宽前缀、UTF-8 元数据及按 4 字节对齐的 SPIR-V words. 前缀记录 magic、格式版本、元数据/载荷长度和内容摘要; 元数据包含完整接口声明快照、每阶段范围、最终活动反射、依赖/源映射、contentHash、abiHash、profile 与工具指纹. 读取从快照重新计算 ABI, 同时核对文件身份与消费者预期, 并用完整声明检查最终 IR; 不只比较一个可独立改写的 ABI 字符串. CPU 包以项目值类型拥有该快照, 不公开 JSON 类型. 序列化逐字段写入, 不直接写 C++ struct/padding. 读取先检查总长度加法、预算、对齐、范围重叠、摘要及版本, 再解析/验证元数据与各阶段头/入口及调用者所需 ABI.

构建写同目录临时文件, 全部成功并关闭后以同卷替换操作发布; 替换失败要返回失败并清理本次临时文件, 原完整包继续保持旧身份. 生成接口是多输出, 采用决策 3 的完成描述和摘要校验; CMake 失败时禁止后续消费者继续使用混合代. 输出相同则不替换字节; 包的辅助时间和成功求值时间按决策 14 处理, 避免失败被误认完成或内容未变时循环重建.

包的 CPU 对象拥有 words、元数据和诊断字符串, 借用 view 的寿命明确依赖拥有者. 读取方必须提供预期 ABI, 不只相信文件名. 后续准备代码从拥有者的稳定 words 取 byte span 调用现有 RHI `CreateShader`; 本包不修改该公开接口或要求 RHI 接收包结构. 到原生对象的复制/保活责任属于 D03-T02, CPU 包读取成功不代表 GPU 可用.

选择单文件是为了避免多阶段字节码/JSON 各自成功替换造成代混用. 不做可扩展资源归档系统, 不增加后台发布服务.

### 11. 限定状态、寿命和失败边界

编译核心首版是同步 CPU API, 调用者串行拥有一个编译环境; `glslang` 进程初始化/结束由 RAII 环境管理, 确保所有编译对象先销毁. 不承诺任意线程并行构造全局编译器, 不捕获窗口、设备或 Editor 裸指针. 公共结果明确区分成功包与失败诊断; 可传播的第三方异常及分配异常在边界转成项目结果, 第一方可能分配的路径不错误声明 noexcept. MSVC Debug STL 的默认/移动容器构造可能在 noexcept 内分配迭代器 proxy, 第三方内部也可能有无法传播的终止路径. 这类致命 OOM 不承诺库调用后同进程恢复; 仅构建驱动安装无动态分配的 terminate handler, 向 stderr 报告当前生成/编译命令后以非零状态立即退出. 不向产品进程安装全局 handler, 不降低 Debug 迭代器检查, 不伪造 Failure 返回. 驱动以非零退出状态报告失败, 保留原始工具信息及程序/阶段上下文, 不逐层重复记录.

请求结束后同步释放编译器临时对象, 再由环境析构完成全局收尾. 成功包拥有自己的 words、反射和诊断数据, 在请求及编译环境销毁后仍然有效; 从包取得的借用只在包拥有者存活时使用. 本阶段的寿命检查围绕这些销毁/释放边界, 不增加异步取消协议.

首版可检查预算: 单 schema/程序请求各 1 MiB、单源文件 8 MiB、一个程序输入总计 64 MiB、最多 256 个依赖文件、include 深度 64、单数据 block 1 MiB、单完整包 64 MiB. 预算与格式版本一起定义, 越界先拒绝再分配/转换; 这些是 CPU 构建预算, 不代表原生 buffer 支持上限. 编译器内部能传播的分配异常由结果边界失败, noexcept 内致命 OOM 使用上述构建进程失败路径; 输入预算不代表进程堆有严格上限. 两条路径均保留无需动态分配的错误诊断, 致命退出不覆盖旧完整包. 该边界由维护者于 2026-11-04 根据 Debug STL 默认/移动构造的实际退出探针确认, 不声称不可传播的 OOM 可重试当前库调用.

需要验证初始化失败、无效输入、可传播的第三方抛出/分配失败、输出不可写、替换失败和重复请求后释放; 致命 OOM 用独立进程探针检查诊断/非零退出与旧包保留, 不将其计入同进程重试. 不引入无消费者缓存、全局锁或共享可变上下文以隐藏寿命问题.

### 12. CMake 依赖覆盖生成物和传递输入

模块用 `add_custom_command` 的 OUTPUT/DEPENDS 和 DEPFILE 声明必要产物与输入; 非必要衍生产物才使用 BYPRODUCTS. VS 下单独丢失 depfile 不会因 BYPRODUCTS 重跑, 因而本步骤的包、depfile、stamp 全部为必要 OUTPUT. 启动链为编译驱动 target -> 接口输出 -> 程序包 -> 实际消费者, 显式依赖驱动可执行文件以确保工具变更重跑; 规范化编译配置文件也是输入. 不依赖 POST_BUILD、无输入的每次编译脚本或 configure 时调用尚未构建的工具.

输出进入 binary tree 的 `Generated/Shader/<config>/...` 和 `Shaders/<config>/...`, 每个配置有唯一写入者. `physara_add_shader_interface(name schema)` 与 `physara_add_shader_program(name interfaceName program sourceRoot)` 接受调用方显式输入, 先在同一作用域声明接口再声明程序; 消费者提供生成前置与 include 根, 将包输出接入自己的 target. 当前没有业务 Shader 消费者, 正式默认构建只生成库与薄驱动. 首个验证程序的 target 仅由 build 中的本地 CMake 工程注册, 不进入正式工程. 同一自定义命令声明全部必要输出, 删除任一个输出都触发恢复; 接口生成用独立 stamp 跟踪评估, 程序包以成功发布或核对后的包时间推进完成状态, 内容不变时不替换字节.

DEPFILE 纳入源、接口、声明及实际 include; 源或请求变动能引入新的依赖边. 解析规则禁止新增影子文件改变选择. 首版按 [Build](../../../../Docs/Build.md#构建) 已支持的 Visual Studio 2022/MSVC x64 multi-config 入口, 核对 Debug/Release 的首次并行构建、配置隔离及对应增量行为. 路径带空格、无改动重建、传递输入变动和缺失输出恢复分别验证其实际风险. 后续若因真实消费者需要正式增加 Ninja 支持, 同步 Build 入口并验证该生成器的 DEPFILE/输出依赖; 本次基础验收不要求额外生成器矩阵.

验证按 [项目验证技能](../../../../.agents/skills/physara-verification/SKILL.md) 选择最小受影响目标. 每项检查关联到具体缺陷风险和 spec 场景, 已有结果在源码/生成输入、工具与配置、依赖及检查输入身份匹配时可复用. 完整生成链就绪后取得一次匹配最终输入的受支持配置干净构建证据; 后续输入变化只重跑受影响检查. 集成阶段核对证据完整性与真实消费者, 不因进入新任务组重复相同构建或扩大 Runtime 原生检查. 全部 spec 场景及本次实际风险已有充分证据、限制已记录后即停止, 仅在输入变化、失败或具体覆盖缺口出现时追加检查.

选择 CMake 原生依赖追踪, 而非目录扫描时间戳 cache 或运行时文件 watcher. 不增加 CI、格式检查或自动安装工具 target.

### 13. 开发材料隔离与本轮整理

维护者于 2026-11-04 明确要求开发测试只存放在 build 下. 本轮将 Checks、Conformance/NoRecords/LayoutCases 的 JSON、Common/Passes GLSL 和测试 CMake 注册移至 `build/development-checks/establish-shader-interface-build`; 正式驱动移除分配注入宏/调用, 进程错误报告仍是正式失败合同. 删除 build 后正式工程必须能从仓库输入重新配置/构建, 不依赖或自动恢复测试材料. 旧批量 runner 和交接包只保留历史证据身份, 不代表整理后的当前输入.

glslang 的 includeSystem/includeLocal/releaseInclude 是必须匹配的虚函数名, nlohmann_json 的 to_json/from_json 是 ADL 固定钩子名. 在声明旁注明外部协议例外及原因, 其余项目函数仍遵守 PascalCase.

此整理只补受影响的正式构建、显式路径生成和增量检查, 不重跑旧全量边界矩阵. 未变的布局/编译/反射/包实现可按明确范围复用原证据. 内部合同已成立时不重复加防御或故障处理, 每个新检查必须对应尚未覆盖的真实风险; 满足本轮验收后停止, 约束细则维护在项目 C++、CMake 与验证技能.

全仓排查还发现早期 Core/Platform 的 DiagnosticsChecks/DeviceHostChecks 头、56 个测试宏分支及 RHI 的 ResourceChecks/UploadChecks friend. 按同一清理授权移出正式源码, 原始文件保留在 `build/development-checks/legacy-hooks`. 只删除测试启用分支, 有 #else 时保留原产品分支, 并移除测试专用前置声明/friend. 本轮不修改设备/资源/输入/诊断的生产合同, 但这些源码与消费者的输入已变化, 因此补做受影响 Debug/Release 构建和正式产品的隐藏窗口启动/退出检查. 旧 fixture 的注入/私有访问不再适用于当前源码, 不自动修复或重跑历史测试矩阵. Architecture 仅将旧的“测试组织未定”说明改为路由到开发约定, 不修改技术架构合同.

### 14. 第一轮独立审查的修正合同

2026-11-05 按 CR-001 至 CR-009 整改, 保留 B 注释, 不改变 D03-T01 范围或将 CPU 证据升级为 GPU 验收.

- 包在驱动中最后发布: 先完成 depfile 和 stamp 的必要写入, 再调用同一 WritePackage 原子替换. 任一写失败保留上一完整包; 辅助文件只在进程成功后可信, 成功时 stamp 与包 contentHash 一致. B 复核证明事后删除 stamp 可能受占用阻止. VS 规则同时跟踪三份输出, 仅调整首输出仍不足以恢复, 因此 depfile/stamp 的可见时间都不越过上一成功包: WriteAtomically 的私有可选时间参数先在已关闭的临时文件上设置, 再原子替换; 相同字节也校正该时间, 默认未指定时间的消费者维持原行为. CMake 首输出与 depfile 左侧均为包, 只有包发布成功或内容不变而核对成功后才推进包时间. 首次缺少包时始终未完成. stamp 清理错误仍报告, 但时间合同使恢复不再依赖清理成功. 不增加备份/回滚框架.
- 固定 glslang 能处理裸 CR 指令, 但不会推进行号. 扫描与编译共用仅将 CRLF/CR 转为 LF 的视图, 原字节继续参与内容身份, LF 源无需复制. 原文件的物理行和续行一致, 不用 LF 专属注释规则绕过 profile; 这不是为兼容错误改写 Shader 语义.
- 每个程序由 PhysaraShaderPackage<name> 唯一 target 拥有生成命令. CMake 函数另返回 <name>PackageTarget, 消费者共享这个 target 前置, <name>Package 只作数据路径, 不分别给多个消费者复制文件生成规则.
- 构建输入(schema/程序/Source/Interface 及实际文件)首次读取时拒绝任一路径组件的 Windows reparse point, 包括目录联接和符号链接. 单靠把逻辑路径再加进 depfile 不能可靠发现切换到较旧目标, 因此选择明确拒绝可重定向路径. 普通路径仍 canonicalize 并核对允许根; 同步调用期间输入须稳定.
- glslang 的现有 includer 不提供原引用行. 在固定 Public/ShaderLang.h、MachineIndependent/preprocessor/Pp.cpp 增加默认空操作 RecordInclude 观测回调, 只报告实际成功 include 的文件与原行. SourceGraph 精确标记该声明边, 不能把同目标的未执行引用一并标记. 用户源的 #line 在边界拒绝, 避免把编译器坐标伪装成另一条声明边; 坐标切换只由 glslang 的 include 流程管理. 不使用不受官方保证的额外 preprocess 轮次, 不改写 Shader 语义或重复实现条件处理.
- 同一私有适配在 Public/ShaderLang.h 声明 IsReservedIdentifier, MachineIndependent/Scan.cpp 只读查询原 KeywordMap/ReservedSet. 接口声明名称排除该固定词法器的全部关键字/保留词汇(含扩展词汇), 并继续排除 C++ 保留名. 无需启动编译环境、复制第二份 GLSL 名单或修改词法算法. 三处本地适配及整体重建要求登记在 ThirdParty/README.
- 插值限定只适用于 VS 输出和 FS 输入. VS 输入/FS 输出的非默认限定在 schema 边界拒绝, 活动反射一律按最终 IR 装饰生成并与独立声明比较.
- 工具指纹补入固定 JSON 两个头的字节与 CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION, 保留目标系统版本为独立字段. JSON 变化进入 configure 依赖, 文件身份仍按仓库相对路径表达, 不含绝对目录.
- 共享 ParseJson 在调用固定 JSON 库前拒绝原始 NUL, 防止 EOF 语义接受合法前缀并忽略后缀. 合法 JSON 转义仍交给解析器; 声明、请求和包元数据沿同一信任边界.

验证直接对应九项独立缺陷: 两配置受影响构建, 必要的失败保包/恢复、三种换行、源映射、保留字、插值、NUL 和双消费者并行检查. JSON 指纹只在 build 中的隔离输入变化, 复用未变第三方静态库, 不修改正式依赖来运行测试. SDK 记录本机实际选择及源码公式, 没有第二个 SDK 时不声称实测切换. 测试材料仅在 build, 覆盖这些实际风险后停止, 不恢复旧全量 runner 或原生矩阵.

## Risks / Trade-offs

- [固定第三方源尚未在本机编译] -> 第一项实施先核对真实源码/许可证、裁剪范围和 Debug/Release 构建; 记录任何适配. 若需改变已选工具组合, 先更新本 design/指纹与对应任务, 不静默换版本.
- [布局生成与预期共享 bug] -> 手算 std140/std430 边界、MSVC static_assert、最终 IR 反射及构造的有效错配 IR 提供不同来源证据. 真正 CPU/GPU 数据语义仍需后续原生读回.
- [活动裁剪隐藏缺少覆盖] -> 主测试的常量、纹理、sampler 与 IO 都影响最终输出, 断言其活动反射; 不活动声明另做允许缺失检查.
- [构建依赖或多输出发布漏项] -> 空目录初建、传递 include/配置/工具变动、删除输出、失败后重试、多配置及目录迁移逐项检查, 不用旧 exe 结果替代.
- [可定位源码与稳定身份冲突] -> 包内只存规范逻辑路径, 命令行展示可映射本机绝对路径; 错误检查同时断言原始行号与 include 链.
- [受限 profile 暂时不覆盖更多 Shader 类型] -> 明确拒绝, 后续扩展先改合同与验证. 不以“编译器支持”推断 native 支持.

## Migration Plan

1. 先固定 ThirdParty 与 Shader 核心构建, 再接接口生成, 然后接 canonical 编译/验证/反射与包, 最后接真实测试程序和 CMake 失效检查. 每步按 tasks 的验证方法取得证据.
2. 本阶段没有旧 Shader 包或接口需迁移. 包/schema/rule version 有明确拒绝行为; 失败时保留旧完整输出供诊断, 新输入构建仍为失败. 回退源码时重新构建, 不沿用 ABI 不匹配缓存.
3. 实施更新 `Docs/Build.md` 的真实命令/环境和 `ThirdParty/README.md` 的来源事实. 没有长期设计变化时不改 Architecture, 此时不提前勾选 D03 或后续工作包.
4. A 实现/本地检查完成后交接 B, 审查/复核任务保持未完成. 交接记录 commit、相关 staged/unstaged/untracked 内容及来源摘要、检查输入/命令/输出和限制; 不只比对分支或 commit. B 注释进入最终输入, A 不自行填写 B 验收.
5. 正式问题经 B 关闭并验收后, 对最终匹配输入执行原生 OpenSpec verify、必要回归, 再 sync/archive. 将审查编号、修正/复核摘要、注释覆盖和证据保存在本 change tasks; Developments 只记录 D03-T01 完成及 change 引用. 仅归档成功后恢复 CodeReview 空模板.

实施前没有必须等待维护者决定的开放设计项. 依赖归档摘要、实际本机工具版本及检查结果是实施证据, 不在本规划中虚构填写.
