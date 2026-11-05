# Shader 声明与编译合同

此目录用于存放正式业务的版本化 JSON 声明, 作为数据、资源与 stage IO 的权威来源. 当前尚无业务 Shader 输入, 此处只保留格式说明. 开发验证的 schema、程序请求及 GLSL 样例归类存放在 build 下, 不作为正式声明交付. 生成文件只进入 binary tree, 业务代码不另写同一份 C++/GLSL binding 或布局.

接口 version 1 的根字段为 version、name、blocks、resources、io. 名称以 ASCII 标识符表达, interface/block 用 PascalCase, 字段和资源用 camelCase. 拒绝重复 key/身份、未知字段、C++ 保留名、固定 glslang 词法器的全部关键字/保留词汇(包括扩展词汇)及生成名称冲突. 字段前缀 physara 保留给 padding. 共享 JSON 字节入口拒绝原始 NUL, 转义仍按 JSON 解码, 不能以 NUL 前的合法前缀忽略后缀.

| 声明 | 字段与含义 |
|---|---|
| block | name、layout (std140/std430)、非空 fields |
| field | name、type、可选正整数 array; 缺省为非数组, array=1 仍是数组 |
| resource | name、kind、set、slot、非空 stages; buffer 必须引用 block |
| structured-buffer | 另需正整数 capacity, 表达固定 record 数量, 不是 descriptor 数组 |
| stage IO | stage、direction (in/out)、name、location、type、可选 interpolation (默认 smooth) |

type 支持 float32/int32/uint32、float/int/uint 的 2/3/4 分量向量和 float2x2/float3x3/float4x4 列主序矩阵. 不支持嵌套 record、bool、16/64 位数、运行期数组、descriptor 数组或矩阵/数组 IO. 整数 interstage IO 必须 flat. IO location 为 0..31, interpolation 仅适用于 VS 输出和 FS 输入, 可为 smooth/flat/noperspective. VS 输入和 FS 输出只接受缺省或 smooth, 活动反射按最终 IR 装饰记录, 不复制不适用的限定.

std140/std430 分别计算 offset、实际字段 extent、矩阵列 stride、数组 stride 和 block 分配大小. 例如 std140 单 float 的 extent=4、分配大小=16; vec3 后的 float 可位于 offset=12; 标量数组在 std140 的 stride=16、std430 的 stride=4. CPU 头用固定宽度类型、显式 padding/stride 和 static_assert 表达这些量, 不假定宿主向量/矩阵类型与 GPU 一致.

set=0/1/2/3 分别为 PerView/PerRenderable/PerMaterial/PerPass, slot=0..127. kind 为 constant-buffer (std140)、structured-buffer (readonly/std430)、texture2d、sampler. 实际 canonical binding 从公开 RHI::VulkanBindingOffsets 推导, 不另建可修改 offset 配置. b/t/s 同号合法, structured-buffer 和 texture2d 共用 t 类别, 同 set/binding 不能重复. 生成 RHI 描述显式采用 descriptor-set 模式, 集合 visibility 是各项声明 visibility 的并集.

每个接口输出 Data.hpp、Bindings.hpp、Vertex.glsl、Fragment.glsl、Expected.json、Manifest.json. CPU 数据头只需标准库, 绑定头另需公开 RHI. Manifest 最后发布并含全部 artifact 摘要; 编译前逐份校验, 缺失/修改/混合代会失败. 不变内容不重写. 单个输出目录必须串行只有一个写入者; 多文件生成失败可留下新旧文件, 但不能被原 manifest 认作完整新代, 修复后重新 generate.

## 程序与 GraphicsBaselineV1

程序根字段为 version=1、name、interface、profile、stages、defines. interface 必须等于所选生成目录的接口名. stages 必须恰好一个 vertex 和一个 fragment, source 相对 Source 根, entry 固定 main. defines 是有限离散声明: name、allowed (1..8 个不同的 0..16 整数)、value (必须属于 allowed), 最多 64 项. 不接受任意编译 flags 或自动枚举连续参数.

GraphicsBaselineV1 固定 Vulkan GLSL 450 core -> Vulkan 1.1 / SPIR-V 1.3. 不自动分配 binding/location, 不做源文本 patch 或 optimizer 重写, Debug/Release 的 Shader 语义选项相同. 仅接入 glslang、SPIRV-Tools validator 与 SPIRV-Cross core; 输出必须先通过 validator, 再从最终 IR 核对字段/资源/IO、活动子集及 FS 对 VS 的兼容性.

不支持 compute、写 storage、push constant、descriptor indexing、subpass input、combined sampler 声明、specialization constant 或未知扩展. 源扩展白名单只有 GL_GOOGLE_include_directive 的 enable/require; IR 不接受扩展, capability 限 Shader/Matrix. 活动 VS builtin 限 Position/PointSize/VertexIndex/InstanceIndex, FS 限 FragCoord/PointCoord/FrontFacing/FragDepth, execution mode 限 FS OriginUpperLeft/DepthReplacing. 这些是 CPU 编译 profile, 不代表当前 GL 后端已支持 Draw/Pipeline/lowering.

include 只有两个唯一且互不重叠的虚拟根:

- 尖括号使用 <Source/相对路径> 或 <Interface/相对路径>.
- 引号仅相对当前文件, 不 fallback 到其他根; 拒绝绝对路径、越根、别名、缺失文件和宏拼接操作数.
- 声明图必须无环, 即使循环被 guard/条件编译遮蔽也失败. guard 和实际条件展开仍交给 glslang.
- 声明边保守进入依赖, sourceMap.expanded 另标识实际展开的边. 错误保留逻辑原文件/行号和引用链.
- 用户源不接受 #line 指令, 避免实际展开伪装成另一条声明边的原行号; 只由 glslang 自己维护 include 的位置切换.

源字节原样参与身份. 声明扫描与 glslang 共用仅将 CRLF/CR 规范为 LF 的编译视图, 保持原文件的行计数与续行一致; 固定编译器直接读取裸 CR 时不会推进行号. 实际展开按编译器报告的原引用位置标记, 不能把同目标的其它声明边一并标记; include 位置须能对应原文件行, 不接受用户重映射后的歧义位置. 构建输入(schema、程序、Source/Interface 根及其文件)的路径组件不允许目录联接、符号链接或其它 Windows reparse point, 首次读取即拒绝. 这样不会把可切换的逻辑路径固定成 depfile 中的旧物理目标; 单次构建期间输入应保持稳定.

预算为 schema/程序各 1 MiB、单源 8 MiB、总输入 64 MiB、依赖最多 256、include 深度 64、block/record 数组最多 1 MiB、完整包 64 MiB, JSON 深度最多 64. 输入预算不代表编译器堆内存的硬上限.

## CPU API、身份和文件

[Compiler.hpp](../Compiler.hpp) 的环境拥有进程级初始化权, 同时最多一个, 由调用者串行使用. 输入仅在同步调用内借用, 返回包拥有 words、反射、依赖与源映射; 环境销毁后包仍有效. view/span 只在相应拥有者存活且未修改时有效. 核心不安装产品进程的全局错误 handler.

可传播的异常转成有界 Failure. MSVC Debug STL 默认/移动构造可能在 noexcept 内分配 proxy, 这类致命 OOM 无法转回结果或保证同进程恢复. 构建驱动对此输出无需动态分配的命令类别诊断并非零退出. Debug 迭代器检查保持开启. 不能以受控故障检查声称覆盖所有自然 OOM 或上游内部泄漏.

身份使用 CNG SHA-256, 对版本化 JSON 字段编码取摘要, 不使用对象 padding、地址、绝对工作目录或时间戳.

| 身份 | 输入 |
|---|---|
| abiHash | 接口语义、字段顺序/类型/派生布局、绑定与 stage IO、规则版本和实际 RHI offsets; 对象顺序/纯格式不影响 ABI |
| contentHash | 原始请求、原始 schema 摘要、生成 manifest/artifacts、全部声明/实际源的逻辑路径及内容摘要、固定选项与工具指纹 |
| compilerFingerprint | 实际保留编译/验证/反射源码与本地构建适配、生成/编译核心和固定 JSON 头、编译器/目标系统/VS 实际所选 SDK/配置/有效 flags 和规则版本 |

工具指纹由 CMake 根据实际输入派生, 不是第二份手工来源登记簿. 来源记录仍只有 [ThirdParty/README.md](../../../ThirdParty/README.md). 构建依赖使用正常 mtime, 不声称在保留全部 mtime 的篡改后自动运行规则; 直接调用 build 命令会强制重新编译并核对内容.

.pshader version 1 是一份完整 VS/FS 包. 定宽前缀逐字段按小端编码:

| 字节位置 | 内容 |
|---|---|
| 0 | 8 bytes magic: PSHADER 后跟零 |
| 8 / 12 | uint32 版本 1 / 前缀长度 64 |
| 16 / 24 | uint64 UTF-8 JSON 元数据长度 / payload 字节长度 |
| 32 | 32 bytes SHA-256, 计算时此槽为零, 覆盖整个文件 |
| 64 | 元数据, 后补零至 4 字节对齐, 再跟各阶段小端 words |

元数据包含完整接口声明快照及其身份、profile、阶段/入口/范围、最终活动反射及依赖/源映射. 读取时重新解析快照并计算 ABI, 再核对各阶段最终 IR, 不能仅相信文件中写入的 ABI 字符串. 未活动的声明仍保留在快照中, 不计作实际资源占用. 范围必须连续、对齐且不重叠, words 必须满足 profile 及所记反射. [ReadPackage](../Package.hpp) 必须传入消费者预期 ABI, 通常取生成头的 AbiHash; 不能只信任文件名.

发布用同目录单个 .tmp, 完成并关闭后替换, 失败返回错误; 相同内容不替换. 驱动先完成 depfile/stamp 的必要原子写入, 在替换前把临时文件时间设为上一成功包的时间, 未变字节的辅助输出也使用该时间. 包是 CMake 首输出与 depfile 目标, 最后才发布; 辅助时间不越过旧包, 不能提前表示成功. 包发布失败后清理 stamp 的错误仍会报告, 但恢复构建不依赖清理成功; 首次没有包时仍是缺失必要输出. 包字节相同且核对成功时只刷新包的求值时间, 避免输入只改 mtime 后反复编译. 辅助文件只在命令返回成功后可信, 成功时 stamp 等于包的 contentHash. 下一次运行清理本输出遗留临时文件, 不按 hash 留存无界历史. 每个程序只有一个包生成 target, 消费者依赖该 target 后读取包路径. CMake 规则失败即阻止依赖消费者, 不能把旧包当作新输入成功. 运行命令见 [Build](../../../Docs/Build.md#shader-构建).
