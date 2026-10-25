# Tasks

范围仅为 [D02-T01](../../../../Docs/Developments.md#d02-t01), 具体方法和前置例外见 [design](design.md), 可观察要求见 [spec](specs/rhi-public-contract/spec.md). 所有复选框表示实现与指定检查共同完成, 规划文件创建不算实施. 实施结果与检查证据见文末; 不修改旧 change 的 4.1 状态, 不启动 Common/Validation、GL backend 或规格同步/归档.

## 1. 固定来源和实施前的局部例外

- [x] 1.1 按 design 第 1 节的五个路径与 LF 算法重核固定输入, 在 RHI/README.md 建立 commit、SHA256、来源/目标映射、MIT 通知位置、完整符号改名规则和差异条目, 同时写清 header-only/外部定义边界. 验证五项输入与已记录 hash 一致, 不符时先查清差异; 核对 ThirdParty 不变, 文本不声称已有 Device/GPU 结果或完整链接实现.
- [x] 1.2 在 Docs/Architecture.md 7.1 的当前映射/受控例外补记 ShaderTypes.hpp、标准容器/来源子命名空间保留及 design 第 5 节三项绑定键修正, 引用本 change 并限定其原因、布局不变和验证方法; 为 D02-T01 关联本 change 并修正 Developments 第 2.2 节过期的下一目标表述. 验证与本 proposal/design 一致、相关链接/锚点有效、D01 缺口与 D02 其他前置不变, 尚未勾选 D02-T01. 此项在移植相关代码前完成.

## 2. 公共基础头及随附 CPU 检查

- [x] 2.1 移植 Resource.hpp 的 Object/ObjectTypes、Resource、RefCountPtr、RefCounter 与 Handle, 应用限定改名并补直接标准头; 随文件保留许可、借用/接管和引用计数不代表 GPU 完成的说明, 更新来源差异. 立即在临时 fixture 的 Debug/Release 编译仅消费此头的程序并运行 CPU probe, 验证初始计数、裸指针 retain、copy/move、兼容转换、self-copy/self-move、Attach/Create/Detach、Reset、空值、借用与恰好一次最终析构; 不兼容转换须编译拒绝, 不创建假 Device.
- [x] 2.2 移植 Containers.hpp 为 StaticVector, 保留 design 列出的标准 aliases/操作名、容量和元素保留/重置语义, 写明合法容量/索引前提并更新来源差异. 按维护者批准的局部修正, initializer-list 构造补 Base() 初始化. 立即用独立头的 Debug/Release 检查验证 0/部分/满容量、构造/赋值及来源支持的 push/pop/resize/fill/swap/emplace、const/mutable 迭代和标准算法, 包含部分填充标量/指针容器的 swap; 简单元素下容器操作不发生动态存储增长, 不在 Release 执行越界或空 back.
- [x] 2.3 将 nvrhiHLSL.h 的共享声明移植到自包含 ShaderTypes.hpp, 保留来源条件分支、GPU 地址/间接 RT/cluster 字段、位域与静态检查, 补 C++ 分支需要的整数头并更新映射说明. 立即验证仅此头的 C++20 Debug/Release 编译、GPU 记录大小/对齐及已初始化位域的已知打包值; 不引入 Shader 编译器/SDK, 结果只记录 C++ profile.

## 3. 完整主合同和公共工具

- [x] 3.1 移植完整 RHI.hpp, 包含所有资源/描述符/格式/状态、绑定/管线、Draw/查询、Device/CommandList/lifetime tracker 及 RT/Meshlet/CoopVec/bindless/sparse 等扩展与弃用重载, 保留字段/默认参数/qualifier/单位; 提供原始版本 27、受控 HeaderVersion 映射和末尾 OpenGL=3. 随头补齐初始化/有效 builder 前提、许可和来源说明, 立即编译总头并在 Debug/Release 核对所有原有枚举/常量、明确初始化的默认字段和源布局断言, 不读取无默认值字段, 不先提供外部方法定义.
- [x] 3.2 按 design 第 5 节修正 BindingSetItem 相等/哈希的 arrayElement 与有效载荷访问, 并使 BindingSetDesc 相等/哈希纳入 trackLiveness; 保持公开签名、字段与 40 字节布局, 同步头内合法 key 前提和非机械差异记录. 立即在 Debug/Release 运行全类型类别的绑定键检查, 分别改变 resource/slot/arrayElement/type/dimension/format/component mapping、mip/layer/byte range、bindings 顺序和 liveness; 验证 reserved 无影响、equal-hash 一致、unordered key 查找/并存和强制碰撞, 不把不同 key 的 hash 必须不同作为断言.
- [x] 3.3 移植 Utils.hpp 的全部声明和来源已有头内行为, 保留 ScopedMarker 的借用前提及 BitSetAllocator 既有声明, 不复制 Common 定义; 在 RHI/README 明确逐项外部定义归属. 立即编译仅包含 Utils.hpp 的 Debug/Release 消费者, 以 compile-only/unevaluated 检查所有工具签名和已有头内接口, 对照 misc/format-info/utils 源确认无导出宏的外部方法也未遗漏; 不执行依赖缺失外部定义或真实 CommandList 的调用.

## 4. 正式消费入口和完整合同核对

- [x] 4.1 在 RHI/CMakeLists.txt 创建列出五个真实头的 PhysaraRHI INTERFACE target, 只导出模块 include 与 C++20, 保持头为第一方诊断范围; 同步 Docs/Build.md 与模块说明中的目标/入口和声明实现边界. 验证 configure 不下载/运行工具、不产生 RHI .lib 或新产品程序, target 无 Core/Platform/native/参考源码依赖, 现有 Runtime 链接不添加未使用 RHI.
- [x] 4.2 建立只消费正式 PhysaraRHI target 的独立临时消费者, 不手工给它加 RHI include 路径; 在自身第一方配置启用既有警告和语言扩展政策. 验证 Debug/Release 下五个独立首 include 翻译单元、完整声明族/重载/const/noexcept/默认参数编译、头内 CPU 行为链接运行, 扩展声明在无 backend 时仍存在; 检查实际 include/编译/链接元数据仅获得本模块和标准库, 旧接口名称与错误 handle 转换的负例按预期失败.
- [x] 4.3 完成固定来源到目标的归一化 token 差异核对, 覆盖所有头/命名空间、字段/默认值、虚函数/重载、宏工厂、枚举和 std::hash; 把每项允许差异对照 RHI/README 与 design, 补齐源/目标内容标识和核对命令. 验证无白名单外差异, 并在临时副本上分别改动源内容、删去一个声明确认 hash/合同核对能报错; 不修改真实参考或用目标反推预期, 不以数量相同代替完整性.

## 5. 集成验收与实施交付

- [x] 5.1 从新 binary 目录离线 configure/build 全部现有产品目标和独立消费者的 Debug/Release, 运行两配置 CPU 检查及真实 Physara 的 help、非法参数、hidden poll-limit=3. 记录工具链/SDK、实际命令、原始输出与返回码 0/2/0; 核对无新增警告、参考/native 依赖泄漏或第二产品入口, 正常宿主退出可重跑. 无原生环境时该部分标未运行并保留本项未完成, 不借 CPU 成功替代产品行为.
- [x] 5.2 对照 8 项要求及全部场景执行原生 verify 与 Code Review, 核对完整声明、命名例外、自包含、初始化前提、CPU 所有权、语义 key、外部定义归属和当前文档; 运行 OpenSpec strict、受影响 Markdown 链接/锚点、git diff --check 及格式/许可检查. 将本次真实实施证据补入 tasks, 仅在各项通过后更新 D02-T01 为已验证并关联本 change; D01 的 4.1、D01/D02 总出口和旧 change 保持实际未完成状态, 不执行提交、主规格同步或归档.

## 要求与实施检查对应

| 要求 | 主要任务 | 关键验收 |
|---|---|---|
| Consume the complete renamed public contract | 1.1, 3.1, 3.3, 4.2, 4.3 | 完整声明族/宏/重载映射, 无未解释删改 |
| Include public headers independently of native facilities | 2.1-2.3, 3.1, 3.3, 4.1, 4.2 | 单头首 include, 正式 target 独立消费者, 元数据隔离 |
| Preserve public values and GPU-facing layouts | 2.3, 3.1, 3.2, 4.3 | enum/default/version, 已知布局, 绑定修正不改变存储 |
| Preserve intrusive CPU reference ownership | 2.1, 4.2 | 引用转移/转换/借用/最终一次析构 |
| Preserve fixed-capacity container interoperability | 2.2, 4.2 | 0/部分/满容量、标准迭代和自身存储不增长 |
| Compare and hash binding identities by semantic fields | 1.2, 3.2, 4.2 | arrayElement、有效 union 载荷、liveness、reserved、碰撞 |
| Trace the port to immutable source inputs | 1.1, 2.1-2.3, 3.1-3.3, 4.3 | 源哈希/路径/许可/差异, 临时副本漂移检查 |
| Distinguish declarations from implemented capabilities | 1.1, 3.3, 4.1, 5.1, 5.2 | header-only/外部定义边界、现有产品不扩功能、DPI 缺口保留 |

## 证据记录约定

实施后在本文件追加每项实际命令/配置、输入和目标内容标识、源码/原始输出位置、结果和未覆盖范围. 临时源/输出统一在 `build/artifacts/establish-rhi-public-contract/`, 不新增 verification.md、测试框架或 CI. 保留成功与必要的预期失败证据, 避免只记摘要无法复核. 结构校验、CPU 行为、产品 native smoke 分别标识, 规划检查不能勾选实现任务.

## 规划核对记录 (2026-10-25)

本轮仅创建原生 change 的规划产物. 已读取既有两个主规格和旧 change 的前置例外, 核对固定公共源、外部定义归属并计算 design 所列五个输入的 SHA256. RHI/CMake、生产代码、当前 Architecture/Build/Developments、两个主规格和旧 change 均尚未修改. 实施编译、CPU 运行、产品 native smoke 和实现 verify 尚未运行; 本次规划的严格结构/链接检查结果在实际执行后另补于此, 不视为上述任务完成.

规划产物已完成原生 `status --json` 与 `instructions apply --json` 检查: spec-driven 四类产物齐备, 实施进度为 0/13. `openspec validate --all --strict` 实际通过 4/4; 本 change 无提示, 既有 native-window-host 有两条要求过长的 INFO, 无失败. 四份新 Markdown 的 14 个本地链接/锚点、空白/冲突标记检查通过, `git diff --check` 通过; 未跟踪的新文件另做内容检查, 不将空的 tracked diff 当作新文件检查. Git 状态仅出现本 change 新目录, 以上结果均为规划检查.

## 实施与验收记录 (2026-10-25)

### 实际范围与工具链

已交付五个公共头和 PhysaraRHI INTERFACE target, 当前来源和逐项差异见 [RHI/README](../../../../RHI/README.md). Resource/容器/descriptor 头内行为可独立使用; Common、Validation、Device/backend 和外部工具函数仍无本轮定义. 完整来源保留 RT/Meshlet/CoopVec/bindless/sparse 等扩展, 未按当前 GL 能力裁剪. 维护者明确批准后, 先同步 design/spec/Architecture/来源说明, 再补 StaticVector initializer-list 的 Base() 初始化及对应检查.

实际环境为 Windows x64、Visual Studio 2022 Enterprise、MSVC 19.44.35228.0 (工具目录 14.44.35207)、Windows SDK 10.0.26100.0、CMake 3.30.0-rc4、Python 3.12.9. 格式工具为已有 clang-format 19.1.5, 使用 Port.py 中的显式 4 空格/Allman/120 列参数, 未新增仓库格式配置. 消费者与第一方产品均保持 C++20、/W4 /WX /utf-8 /permissive- 和禁用语言扩展. 未安装工具、下载依赖或降低诊断.

下列路径均相对项目根目录. 临时源码、核对规则、原始输出和 binary 统一在 `build/artifacts/establish-rhi-public-contract/`, 由现有 build 忽略规则排除, 是本机复核证据, 不作为新的持久测试框架或生产构建输入. 生产文件没有依赖临时脚本或生成输出.

### 固定输入、完整声明和许可 (1.1, 3.1, 3.3, 4.3)

五项源输入按 design 的严格 UTF-8/仅 CRLF 转 LF 算法重新计算, 全部匹配已固定 hash. `Port.py` 从固定输入建立实际声明/函数/常量/成员符号映射, 识别注释和字面量边界; `symbols.json` 保存具体映射. `Audit.py` 对照上游生成的预期 token 与明确适配, 不从目标反推预期, 覆盖整个头、预处理条件、字段/默认值、函数/重载/qualifier、宏工厂和所有 hash 特化. 五个 MIT 通知块与各自源头逐字一致.

| 目标 | 目标 SHA256 (LF) | 已核对有效 token 数 |
|---|---|---|
| RHI/Resource.hpp | `7b2ad9175ef6f2969bd42c15685fd04c299313eab20fad0459f4c35c20313e5e` | 1516 |
| RHI/Containers.hpp | `6f373d1cfa4b33fbb110f4963e26cfcfa1213f6b2f048282b1d33a68574e5111` | 712 |
| RHI/ShaderTypes.hpp | `76f522af9b3f3b791648ebeebb72a991ccc27c92e758fb7453b35d165f4ce93c` | 399 |
| RHI/RHI.hpp | `1d30c8051ee8022067cd5a29852ee46d409e1de05f1ccc084b7e517717af59cf` | 21638 |
| RHI/Utils.hpp | `5b337f89069ef78e1b93abd1ce3e1b919003f65478ecbe6accd1f056c2509f7a` | 517 |

源/目标 hash、token 数及许可结果的原始记录为 `contract-audit.json` 和 `contract-audit.log`. 受控负例只修改 `audit-mutants/` 临时副本: 删除 CreateHeap 声明触发 `Unexplained token drift`, 修改源内容触发 `Input drift`, 均按预期被拒绝; 真实 ThirdParty 未修改. 最后一次格式核对仅调整一行来源注释缩进, 上表已记录调整后的内容标识, token 和编译行为不变.

`Surface.py` 从固定输入生成 `SurfaceChecks.inc` 和 `surface-inventory.json`: 45 类原有 enum 的 389 项值逐项 static_assert, 173 个去重纯虚方法做存在性检查并检查接口抽象性. 完整参数/重载/const/noexcept/默认参数由上述整份 token 对照核实, 其中 copyTexture/createGraphicsPipeline/createMeshletPipeline 的弃用或多重重载没有通过实际调用引入弃用告警. 数量只作索引, 不替代完整对应核对.

### 头内 CPU、默认值与布局 (2.1-2.3, 3.1-3.3)

每个头落地时先用 `early-build/` 中的独立头消费者验证 Debug/Release, 相关 `<Check>-configure.log`、`<Check>-<配置>-build.log`、`<Check>-<配置>-run.log` 保留当时结果. 前期 fixture 显式使用模块 include; 正式 target 建立后, 又在另一个 `consumer-build/` 验证 target 传播, 二者不混作同一证据.

| fixture 源 | 实际通过的合同检查 |
|---|---|
| ResourceChecks.cpp | 初始计数 1、retain/copy/move、兼容转换、自赋值/自移动、借用、Create/Attach/Detach、GetAddressOf/ReleaseAndGetAddressOf/operator&、Reset/null 和最终恰好一次析构; 使用真实存活的 CPU probe, 不创建假 Device |
| ContainersChecks.cpp | 0/部分/满容量、构造/赋值、push/pop/resize/fill/swap/emplace、const/mutable ranges 和标准算法; 简单元素的容器自身不动态增长; 保留 pop 的槽引用和 resize 的重置语义; initializer-list 剩余标量/指针槽初始化及部分填充 swap |
| ShaderTypesChecks.cpp | 仅 C++ profile; GPU 地址 8、地址/stride 16、instance 64 及 cluster 64/72/32/16 字节记录、适用对齐/offset; 已初始化位域的已知打包值 0xa0123456、0xbb123456、0xaa654321 |
| ContractChecks.cpp | 全部原有 enum、固定常量、HeaderVersion/OriginalHeaderVersion=27 与 OpenGL=3; 明确初始化的 descriptor 默认字段/单位、合法 builder 边界、64 位 buffer range、绑定偏移 0/128/256/384、布局 8/40/16/64 及 indirect 16/20/12; offsetof 仅用于 standard-layout |
| BindingChecks.cpp | 15 类 ResourceType、所有公共语义字段与有效 mip/layer/byte range、arrayElement、reserved、bindings 顺序及 trackLiveness; equal/hash 一致、unordered 查找/并存、强制 hash 碰撞; descriptor 拷贝不增加资源引用 |
| UtilsChecks.cpp | 全部 23 个外部自由函数及 BitSetAllocator/ScopedMarker 的 compile-only/unevaluated 接口检查, 不调用缺失定义或真实 CommandList 行为 |

六个 CPU 可执行检查在正式 target 消费的 Debug 和 Release 均链接运行成功, 原始输出为 `public-<Check>-<配置>-run.log`. 不读取未初始化 descriptor, 不执行越界/空 back, 不读取非活动 union, 不以这些 CPU 检查证明 GPU 生命周期或原生图形能力.

### 正式入口、隔离及产品回归 (4.1-4.2, 5.1)

`PublicCheck.py` 在新 `consumer-build/` 只通过 add_subdirectory 加入正式 RHI/CMakeLists.txt, 所有 fixture 仅链接 PhysaraRHI, 未手写 RHI include. 两配置构建五个单头首 include 翻译单元及上述六个检查. `public-inspection.json` 和 `rhi-contract-<配置>.txt` 证明目标类型为 INTERFACE_LIBRARY, sources 为五个真实头, include 仅当前 RHI 模块, feature 仅 cxx_std_20, links/system 为空, 无 RHI .lib. CMake File API 的 44 个 compile group (生成器四配置的元数据) 保持第一方警告、无 PCH/native/参考 include; 实际编译和运行覆盖 Debug/Release, 不将元数据存在当作另两配置已构建.

`WrongHandle` 和 `OldInterface` 在两配置按预期编译失败; `MissingCommon` 实际 ODR-use VerifyHeaderVersion, 在两配置按预期 LNK2019, 证明没有以空实现补齐 Common. 原始失败输出在 `public-<负例>-<配置>-negative.log`, 属于检查通过的预期失败, 非未修复构建问题. `public-configure.log`、`public-<配置>-build.log` 保存正式消费者配置与构建输出.

`ProductCheck.py` 从新的 `product-build/` 离线配置并构建全部现有产品目标 Debug/Release. `product-start.json` 记录初始无 CMakeCache, `product-inspection.json` 确认唯一产品可执行程序仍为 Physara、无 NVRHI 目标、第一方诊断保留、Runtime 未增加无用 RHI 依赖, 无参考/native 依赖从公共 target 泄漏. `product-configure.log` 和 `product-<配置>-build.log` 保留完整输出, 成功构建无新增警告.

两配置分别实际执行 `Physara.exe --help`、`--unknown`、`--hidden --poll-limit=3`, 返回码为 0/2/0; 首轮 hidden 每配置重复三次, 共六次真实宿主创建与正常退出, 原始输出在 `product-help/invalid-<配置>.log` 和 `product-hidden-<配置>-0/1/2.log`. 最终又用严格匹配预期返回码的 runner 复核两配置的 0/2/0, 记录在 `verify-product-codes.json` 与 `verify-product-<用例>-<配置>.log`. 这是现有 native host smoke, 不包含 Device/绘制/提交/呈现能力验证.

首次受限环境 configure 遇到本机 SDK 注册目录读取限制; 使用授权的构建命令权限后完成上述真实 MSBuild, 未以调整 SDK 路径、屏蔽诊断或假造结果绕过. 临时脚本的输出编码和 include 映射问题已修正, 最终消费者/产品构建和对应检查均通过.

### 实际命令与输出入口

工作目录为项目根. 早期分步检查实际使用以下命令, 输出由 RunCheck.py 按上面的命名写入证据目录:

```powershell
$rhiPython = 'D:/MyEnvironment/MSYS2/mingw64/bin/python.exe'
$rhiEvidence = 'build/artifacts/establish-rhi-public-contract'
& $rhiPython "$rhiEvidence/RunCheck.py" ResourceChecks Resource --negative WrongHandle
& $rhiPython "$rhiEvidence/RunCheck.py" ContainersChecks Containers
& $rhiPython "$rhiEvidence/RunCheck.py" ShaderTypesChecks ShaderTypes
& $rhiPython "$rhiEvidence/RunCheck.py" ContractChecks RHI
& $rhiPython "$rhiEvidence/RunCheck.py" BindingChecks RHI
& $rhiPython "$rhiEvidence/RunCheck.py" UtilsChecks Utils --negative OldInterface
& $rhiPython "$rhiEvidence/Surface.py"
& $rhiPython "$rhiEvidence/PublicCheck.py"
& $rhiPython "$rhiEvidence/Audit.py"
& $rhiPython "$rhiEvidence/ProductCheck.py"
& $rhiPython "$rhiEvidence/Hygiene.py"
openspec status --change establish-rhi-public-contract --json
openspec instructions apply --change establish-rhi-public-contract --json
openspec validate --all --strict
git diff --check
```

正式脚本使用的核心 CMake 命令如下; 两配置分别以 Debug/Release 代入, 消费者运行 `consumer-build/bin/<配置>/<Check>.exe`, 产品运行 `product-build/bin/<配置>/Physara.exe`:

```powershell
cmake -S build/artifacts/establish-rhi-public-contract -B build/artifacts/establish-rhi-public-contract/consumer-build -G "Visual Studio 17 2022" -A x64 -DCMAKE_SYSTEM_VERSION=10.0.26100.0 -DPHYSARA_SOURCE=D:/MyWorkSpace/Physara -DUSE_PUBLIC_TARGET=ON
cmake --build build/artifacts/establish-rhi-public-contract/consumer-build --config Debug --parallel
cmake --build build/artifacts/establish-rhi-public-contract/consumer-build --config Release --parallel
cmake -S . -B build/artifacts/establish-rhi-public-contract/product-build -G "Visual Studio 17 2022" -A x64 -DCMAKE_SYSTEM_VERSION=10.0.26100.0
cmake --build build/artifacts/establish-rhi-public-contract/product-build --config Debug --parallel
cmake --build build/artifacts/establish-rhi-public-contract/product-build --config Release --parallel
```

### 原生 verify 与代码评审

使用 openspec-verify-change 的完整性/正确性/一致性检查, 输入为 CLI 返回的全部四类 contextFiles 及实际源码、编译元数据和原始输出; 未用 planning complete 替代实现结论. 该 schema 没有独立 verify CLI 子命令, 以原生 skill 流程执行核对并将报告留在本任务记录中.

| 维度 | 结果与证据 |
|---|---|
| 完整性 | 13 项实施/验收任务均已满足; 8 项 ADDED 要求都有上方任务映射和实际实现, 无 REMOVED/RENAMED 要求 |
| 正确性 | 25/25 场景有适用的编译/CPU/固定来源差异或现有宿主证据; 含维护者批准后新增的部分填充 swap 场景 |
| 一致性 | 7 项设计决策、命名/暂存前提与局部绑定键修正、批准的 Base() 修正均与实现/当前文档一致; 无新增反向依赖、成功占位定义或无界缓存 |

评审定位: Resource.hpp 的 RefCountPtr/RefCounter 保持 CPU 所有权; Containers.hpp 的 initializer-list 初始化和 swap 满足容量合同; RHI.hpp 的 BindingSetItem 相等/哈希使用相同语义字段及匹配活动载荷, BindingSetDesc 同步包含 liveness; Utils.hpp 保留完整声明与来源头内行为; RHI/CMakeLists.txt 仅导出实际公共头. 大头文件、标准协议和来源注释按 design 的有限移植例外处理, 未机械拆分或引入两套入口.

全部适用检查已运行, 未发现 CRITICAL/WARNING/SUGGESTION. OpenSpec strict 4/4 通过, 仅既有 native-window-host 两条长要求 INFO. 受影响八份 Markdown 的本地链接/锚点、五个头的格式稳定性、许可、空白/冲突标记及 git diff --check 均通过; 数量和结果见 `hygiene.json`. 新文件另做检查, 不是依赖 tracked diff 的空结果. ThirdParty、Core/Platform/Runtime、两个主规格和旧 change 无修改.

D02-T01 据此标为已验证、待归档. 本 change 可进入另行授权的原生归档流程, 本次未执行提交、主规格同步或归档. 尚未实施的 Common/Validation/GL backend、HLSL/Slang/GLSL 编译、真实 GPU 语义及性能测量不属于本包验收; D01 的 4.1 真实跨屏 DPI 及 D01/D02 总出口仍未完成, 不因本包通过而勾选.

## 归档前复核与修正 (2026-10-25)

维护者在实施验收后授权再次测试、Code Review 和归档. 以上为实施轮证据快照; 本节记录新增复核及实际发现, 不把先前结论当作未检查的保证, 不改写旧 change 或已归档历史.

### 发现与修正

首次从新的 `archive-review/consumer-build` 和 `archive-review/product-build` 完成 Debug/Release 测试后, 逐个审查命名合同发现 CommandList 的 draw、dispatch、resolveTexture、commitBarriers 仍为来源小写方法. 先前临时映射只列常用动词前缀, 整份 token 对照也使用了同一不完整映射, 因而不足以发现该遗漏. 这违反既定普通函数 PascalCase 规则; 重新打开 3.1、4.2、4.3、5.1、5.2, 按原设计修正为 Draw、Dispatch、ResolveTexture、CommitBarriers, 不保留旧别名. 参数、返回类型、默认参数、qualifier、行为和布局不变, 未扩大本包范围.

修正前先新增 ContractChecks 中四项明确成员签名、Dispatch 默认参数调用的 unevaluated 检查和四个旧名字不存在的 concepts. 在原头上实际编译失败, 四个新方法均触发 C2039; 更新映射后的 token 核对也拒绝原头. 原始输出及实际返回码为 `archive-review/before-fix-naming-build.log`、`before-fix-naming-audit.log` 和 `before-fix-naming.json`. 这些预期失败证明检查能发现此次遗漏, 没有通过放宽断言来修复.

同步澄清 design/来源说明的既定命名规则, 补齐临时符号表, 重新生成来源 enum/接口存在性检查. 另增独立于 Port.MAP/动词前缀的 NamingAudit.py, 逐项按首字母大写规则对照来源/目标的非析构虚方法: Resource 8 项、RHI 177 项; 将 Draw 改回 draw 的临时副本被拒绝. 四个旧方法的编译负例 OldMethods 与正向签名/旧名缺失断言共同验证, 结果为 `archive-review-corrected/naming-audit.json`.

### 修正后的完整测试与评审

实际执行以下命令; 使用前述相同 MSVC/SDK/CMake/Python, 不复用首次复核的 binary 目录:

```powershell
& 'D:/MyEnvironment/MSYS2/mingw64/bin/python.exe' 'build/artifacts/establish-rhi-public-contract/ReviewNaming.py'
& 'D:/MyEnvironment/MSYS2/mingw64/bin/python.exe' 'build/artifacts/establish-rhi-public-contract/Surface.py'
& 'D:/MyEnvironment/MSYS2/mingw64/bin/python.exe' 'build/artifacts/establish-rhi-public-contract/Port.py' --audit
& 'D:/MyEnvironment/MSYS2/mingw64/bin/python.exe' 'build/artifacts/establish-rhi-public-contract/Audit.py'
& 'D:/MyEnvironment/MSYS2/mingw64/bin/python.exe' 'build/artifacts/establish-rhi-public-contract/ReviewCheck.py' archive-review-corrected
& 'D:/MyEnvironment/MSYS2/mingw64/bin/python.exe' 'build/artifacts/establish-rhi-public-contract/NamingAudit.py'
& 'D:/MyEnvironment/MSYS2/mingw64/bin/python.exe' 'build/artifacts/establish-rhi-public-contract/Hygiene.py'
openspec validate --all --strict --json
```

`archive-review-corrected/commands.json` 保存每条实际 CMake/可执行命令、预期及实际返回码、原始 log 路径. `summary.json` 记录两配置的新目录构建、12 次 CPU 检查、8 次预期失败 (WrongHandle/OldInterface/OldMethods/MissingCommon)、6 次真实 hidden 宿主及每配置 0/2/0. 五个头的首 include、完整枚举/声明、CPU 所有权、容器初始化/swap、有效 union 载荷、语义 key/碰撞、外部定义边界均再次通过; 第一方诊断完整且无新增警告. 两个新 binary 的实际命令和源/目标元数据与首次实施相同边界, 44 个 consumer compile group 只含模块 include/C++20, 产品仍只有 Physara 一个入口, Runtime 未增加 RHI 或参考/native 依赖泄漏.

修正后 RHI/RHI.hpp 的 SHA256 (LF) 为 `46fb8ebcb1c5f3aef62af68dbd3016809935404da5d1ff72d3921715720544ac`, 替代上方首次验收表中的 RHI 目标标识; 其余四头及所有固定源 hash 不变, token 数和布局不变. `contract-audit.json` 已更新为该目标标识, 来源漂移/删声明负例和五个精确保留的 MIT 块再次通过.

Code Review 核查了 RefCountPtr 的 retain/借用/转移/最终释放、StaticVector 初始化与整个 backing array 的 swap、BindingSetItem 相等/hash 的相同公共字段与按类型选择的活动载荷、BindingSetDesc 的顺序/liveness 以及 INTERFACE 依赖方向. 所有来源内联行为、保留的初始化/窄字段前提与外部定义归属和当前 design 一致; 未引入缓存、假 Device、Common 占位或后端工厂.

| 维度 | 归档前复核结果 |
|---|---|
| 完整性 | 13/13 任务, 8/8 ADDED 要求有实现/证据, 无删除/改名要求 |
| 正确性 | 25/25 场景有适用证据, 新增独立命名检查与修正前失败证明通过 |
| 一致性 | 7 项设计决策、批准的 Base() 例外和完整命名合同一致; 发现的 1 项命名遗漏已修复, 无遗留 CRITICAL/WARNING/SUGGESTION |

该评审仍只覆盖 Windows x64/MSVC C++20 的公共合同和现有宿主 smoke. 本轮归档授权不代替 D01 的真实跨屏验收, 不完成 Common/Validation、GL Device 或 D01/D02 阶段出口. 主规格同步选择、归档位置及归档后链接/结构检查在实际完成后另记.

## 主规格同步与归档结果 (2026-10-25)

维护者明确选择“同步主规格后归档”. 按 openspec-archive-change 内联执行 openspec-sync-specs, 在写入前获取并核对一次有效的原生 specs instructions; 本项目未配置额外 specs rules. 同步仅新增 [rhi-public-contract 主规格](../../../specs/rhi-public-contract/spec.md), 标题和 Requirements 结构转换为主规格格式, Purpose、8 项 ADDED 要求和 25 个场景逐字保留. 没有 MODIFIED/REMOVED/RENAMED 操作, 没有 TBD Purpose. 对 status 返回的全部 delta 重新比较, 结果为全部 already synced; 主规格 SHA256 (LF) 为 `4a2b714905dc7dcd1cb7beee588d900034d122c1457267088e0f329916135c05`.

归档前再次由 openspec status/list 确认 spec-driven 四类产物均 done、任务 13/13. 核对实际绝对源/目标路径均位于所选项目的 openspec/changes 范围, 目标不存在后, 移动至 `openspec/changes/archive/2026-10-25-establish-rhi-public-contract/`. .openspec.yaml 随目录保留且 SHA256 前后一致: `D002983090ACFE222597827AA3C481B59D3A566C704A6E91E0D50BB1D209297D`. 原生列表已不再列出本 change; 剩余 establish-window-input-runtime 仍为 15/16, 未归档.

同步修正本归档内指向项目文档/来源和活动旧 change 的相对链接, 以及 Architecture、Developments、RHI/README 指向本 change 的链接; Developments 的 D02-T01 状态更新为已完成并归档. 保留规划/实施时的原文和当时不归档的范围记录, 本节记载后续授权与完成事实, 不改写当时要求/取舍.

归档后实际检查通过: `openspec validate --all --strict --json` 为 4/4 (剩余活动 change 和三个主规格), 主规格 strict 为 3/3; 仅既有 native-window-host 的两条长要求 INFO, 无失败. 九份受影响 Markdown 的本地链接/锚点、五个头的格式、空白/冲突标记和 git diff --check 通过. 再次对照归档 delta 验证主规格全部内容, 五个已测试头的内容标识不变; 23 个既有主规格、旧 change 和既有归档文件与归档前快照一致.

实际命令包括 `openspec instructions specs --change establish-rhi-public-contract --json`、`openspec validate --specs --strict --json`、`openspec validate --all --strict --json`、`openspec list --json`, 以及临时 SyncCheck.py、ArchiveRelink.py 和带归档目录参数的 Hygiene.py. 输出/实际路径记录为 `sync-verification.json`、`archive-move.json`、`archive-relinks.json`、`hygiene.json`, 均位于既有 build/artifacts/establish-rhi-public-contract/. 无未完成产物/任务警告, 未执行 Git 提交或推送. D01 跨屏缺口和后续 Common/Validation/GL backend 继续保持未完成.
