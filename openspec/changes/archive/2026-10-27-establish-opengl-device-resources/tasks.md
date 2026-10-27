# Tasks

## 1. 实施入口与既有合同

- [x] 1.1 核对 D01 原 change 的真实不同缩放跨屏证据、4.1 和 Developments D01 出口, 保存 15/16/单屏的实际入口结论; 验证本轮维护者新增的 T03 限定授权已同步 proposal/design/spec/tasks/Developments, 明确适用范围、原因、回归和退出条件. 不以单屏/注入值替代跨屏, 不从 T01/T02 自动推导许可; 无有效授权时停止实施.
- [x] 1.2 在入口满足后保存当前代码/主规格/第三方来源标识, 重读本 change 全部产物及届时 D01 已采纳合同, 核对 68 项 Device 和资源接口、root/模块 target、来源与风格约束. 验证没有影响本设计的未决依赖, 现有 generic 消费者入口和 D01 旧场景完整, 具体来源及差异随实现记录到 RHI/README; 不改历史归档或按实施进度修改 Architecture.

## 2. 原生组件、设备工厂与真实能力

- [x] 2.1 按 design 2/3/11 新增真实 PhysaraRHIOpenGL target、OpenGL.hpp 及私有跨 cpp 合同, 保持 PhysaraRHI 的六公共头/Common/Validation 闭包; 为 Device 全部 override 建立已实现/明确拒绝矩阵, 含扩展及 deprecated 重载. 在 Debug/Release 编译后端头的独立首 include 和正式 target, 验证 generic 独立 add_subdirectory(RHI) 无 glad/Platform/native include/link, 新 header 不泄漏原生类型、无匿名域或旧域 alias, target 警告/标准只按本模块施加.
- [x] 2.2 实现 design 5 的 17 格式映射、实际 target/limits/样本集合与保守 flags, 所有 Feature false/output 不变; 不把 CAVEAT/任意非零、MAX_SAMPLES 或 format metadata 当正向支持. 借用正式 Host 的 current context 在 Debug/Release 检查所有映射/排除/未知格式、九种 target 及完整 native 查询结果, 最小所需用途缺失时失败; 保存实际 driver/limits/MS 集合, 不为无法自然触发的极值制造硬件支持结论.
- [x] 2.3 完成 design 3/4/8 的 CreateDevice/ShutdownDevice、context/线程/入口/阶段与 GL 首错边界, 初始化并查询 clip convention, bad_alloc 无半对象, 无资源时关闭幂等. Debug/Release 在正式 Host/context 和同一工厂上验证创建、API/callback、缺失 user/check/entry、wrong thread/current、native query/CPU 失败及完整 Unsupported 矩阵, 禁止无关 payload 解引用、假列表/票据/WaitForIdle 成功; 更新后端头和 RHI 说明. 早期 fixture 可显式借用原 Host context 调用正式工厂, 7.1 接入后统一改用 Host 借用, 不在同一 context 重复装配生产 Device.

## 3. Buffer 与必要退休协议

- [x] 3.1 同时落地 design 6/9 的普通 immutable Buffer、Device 强引用、创建前预留的删除记录和窄锁 FIFO, 使最终 Release 无新分配/无外线程 GL, RunGarbageCollection 在 owning context 排空. 随实现更新 buffer/退休头合同和 RHI 来源/范围说明; Debug/Release 以真实 native 查询检查容量/storage flags/desc, worker final Release、禁分配释放、owner collection 精确一次删除、原生工厂根引用关闭前提和空队列重复收拢, 不使用假的 native name 或固定帧延迟.
- [x] 3.2 补齐 Buffer 的 signed-size、raw/structured 布局及 cpu/volatile/typed/virtual/shared/RT 边界, 保留 usage 字段但明确未实现执行路径. 在 Debug/Release 检查 0/最大整数/不整除/不可用 profile 的零 native 分配, CPU 准备及 native name/storage 后受控失败均无公开句柄/泄漏并保留主错, 失败后能重新分配释放; 检查 source 公共字段/值/布局/引用计数未变, 不真申请极端容量或耗尽 GPU.

## 4. Texture 存储与维度

- [x] 4.1 实现 design 6 的九种 Texture storage 映射、实际格式/usage/MS 限制和 component mapping, 将同一 owner/无分配退休协议用于纹理; 同步其头合同与 RHI 支持矩阵. 在 Debug/Release 实测每种受支持维度的非对称尺寸、各 mip、array/cube face、MS 和 effective format, 验证 linear/sRGB 区别、真实查询支持与创建一致, 初始 state/clear metadata 不产生像素初始化或 native barrier.
- [x] 4.2 补齐 Texture shape/GLsizei/mip/layer/sampleQuality/用途和所有未实现格式的拒绝, 对未支持 GetNativeObject/GetNativeView 返回空. Debug/Release 检查错误 cube face、部分维度不匹配、过大 mip/layer/尺寸、实际样本集合中的缺失值、isUAV/tiled/virtual/shared 和非法 swizzle 的零创建, 以及 name/storage/参数阶段的 CPU/native 故障回滚; 不 clamp 显式非法输入, 不读取未初始化像素, 记录自然覆盖与受控分支的区别.

## 5. 独立 Sampler

- [x] 5.1 按 design 8 实现完整基础 filter/address、finite border/bias/anisotropy 和 Standard/Comparison 参数映射, 保持固定来源的 GL_LESS shadow 语义; 使用同一 Device/退休协议并说明 CPU/原生寿命. Debug/Release 实测全部布尔过滤组合、五种 address 和两种 reduction 的 native 参数, 同一 Texture 的两套 Sampler 不修改纹理参数, Comparison 不受主视图 Reverse-Z 改写.
- [x] 5.2 拒绝非有限/越 native 范围参数及 Minimum/Maximum, 覆盖 sampler CPU 预备、native 创建和设置失败. Debug/Release 验证失败不偷偷钳制/降级、无部分 sampler/额外诊断循环, worker final Release 与关闭前删除精确一次; 随本组更新 RHI 的 sampler 支持/不支持说明和必要合同.

## 6. Framebuffer、内部 view 与归属

- [x] 6.1 按 design 7 实现单 mip/单层附件的完整 CPU 预检、Framebuffer 强持有依赖、必要 typeless view、named draw/read buffer 配置、COMPLETE 后发布和 FramebufferInfoEx; 源码明确 glGenTextures -> glTextureView 的前提, 不为统一 DSA 外观误用已给 target 的名字. Debug/Release 实测 color-only/depth-only/组合、array/cube face/MS、mip extent 与兼容 SRGB view, GetDesc/Info 与实际附件一致、read/draw binding 不变, texture caller handle 释放后附件仍可观察; 同步附件范围与只读标记合同.
- [x] 6.2 覆盖 typed/不兼容 view、非法显式 range、错角色、extent/sample 不一致、layered/3D/无附件/shading-rate/只读颜色和实际附件限制. Debug/Release 检查后项无效整批零 FBO/view 创建, 以及 view/FBO 设置和 incomplete 后完整撤销/主错保留; 不以 Resolve 交集修复越界, 不构造超出 StaticVector 存储前提的 UB, 环境无法自然触发的限制仅以标明的受控分支验证.
- [x] 6.3 在正式主 Host/context 外用临时受控兼容 context 和真实 Device/资源检查 foreign owner, 逐项比对 required 入口地址与已有 GLAD, 严格还原 current 与辅助对象, 不重载全局入口. Debug/Release 验证 wrong implementation/foreign texture 在 native 使用前拒绝, FBO retirement 先于依赖纹理删除、禁分配/worker 释放无 GL、实际 name reuse 不复用旧身份或 view. 记录辅助 context 的 driver/profile/入口兼容证据; 无条件时保留该验收未完成, 不以 CPU 假资源代替 native 证明.

## 7. DeviceHost 装配与安全关闭

- [x] 7.1 按 design 4 在同一 StartNativeHost 中接入 raw/Validation/adapter, 追加而不重排阶段/错误, Ready 前完成实际设备; 增加 owner/Ready 限制的 borrowed GetDevice, fixture 改用正式 Host 装配. 同步公开寿命/错误注释、Build 实际依赖与范围. Debug/Release 检查可见/隐藏、borrow 不增引用、inactive/wrong-thread 借用为空、factory/Validation/后续 swap/input 失败的逆序释放和后续重建, 首错/native code 与 cleanup/日志失败区分, callback 不抛出/无悬空目标.
- [x] 7.2 将 raw==2/validated==1 的已知 owner 预检加入任何 ready/input/context teardown 之前, 保持调用方先停止资源用户的关闭合同; 额外资源/Device owner 返回可重试 ResourceInUse, 不锁存输入错误. Debug/Release 分别检查四类活资源、外部 raw/validated owning 引用、FBO 内部依赖和 worker 最后释放, 观察拒绝前后 ready/input/callback/context/进程 claim 不变, 收拢并释放后同一 Host 重试成功; 不新增全局 registry 或重复计数真相.
- [x] 7.3 完成预检后的 owning-context deletion drain -> validated -> raw -> adapter -> 原有 native 关闭链, raw ShutdownDevice 要求唯一根引用且幂等; Normal 和 ContextUnavailable 两种退出区分预检拒绝/已停止带错, faulted/lost context 不继续 GL 或等待伪票据. Debug/Release 验证排空顺序、正常/中途失败/复合 cleanup/Stopped old owner、Callback/Diagnostics 退出, 同一进程重复创建释放; 外部 owner 未收拢就析构的合同违例在隔离子进程 fail-fast. 受控 context-loss/错误分支不宣称真实 GPU 失效, 随本组更新 Host/Build/RHI 的实际关闭合同.

## 8. 跨模块回归、原生 verify 与交付

- [x] 8.1 从新 binary 目录离线配置全部产品和正式 native 消费者 Debug/Release, 运行本包已逐组交付的全部 native 正反/故障场景、独立 generic CPU/首 include/旧接口拒绝, 以及 Core/输入/原宿主和 Runtime help/unknown/hidden 精确 0/2/0/正常退出回归. 用实际命令、File API/符号证明 /WX 无新警告、generic 无 loader 泄漏、Platform -> backend -> generic 单向和私有 native 类型/检查符号, 不新增工具/测试框架/第二产品入口或引用 NVRHI target.
- [x] 8.2 对两份 delta 的全部要求/场景执行原生 openspec-verify-change 和 Code Review, 核对 design 1-12、完整 Device/资源 override、CPU/原生两条寿命、callback、caps/flags、预分配退休、可重试关闭及错误退出. 验证实际 native 证据而非只看结构/CPU, 执行 strict、本地 Markdown 链接/锚点、来源/许可/风格和 git diff --check; 修正后只复验受影响检查, 缺证据不通过对应任务, 不放宽断言或伪造设备成功.
- [x] 8.3 按实际结果更新 Developments T03、T04 已共用的删除子集与 T05/D03 剩余范围, 将环境/命令/原始输出和可复核定位补入本 tasks, 清理过期 Build/RHI 说明. 验证 Architecture/既有归档未被任务叙事修改、公共合同与实现一致, 不因 T03 通过关闭 T04/T05/D02 出口, 不在 apply 执行主规格同步、归档或 Git 提交.

## 要求与任务对应

| 能力要求 | 任务 | 主要证据 |
|---|---|---|
| Create a device for the owning current context | 2.1-2.3, 7.1 | 正式工厂/Host、current/线程、clip 查询、首 include |
| Expose actual resource capabilities conservatively | 2.2, 4.1-4.2 | 17 格式/九 target、实际 MS 集合及零 output 改写 |
| Create immutable buffer allocations | 3.1-3.2 | 实际容量/storage、边界/故障、无假上传 |
| Create texture storage with explicit shape and usage | 4.1-4.2 | native mip/layer/format/sample 与未初始化语义 |
| Keep view interpretation compatible with storage | 4.2, 6.1-6.2 | typeless/compatibility class、精确 range、无假 native export |
| Keep texture and sampler state independent | 5.1-5.2 | native 参数、纹理状态、GL_LESS/越界拒绝 |
| Compose valid framebuffer attachments | 6.1-6.2 | completeness、Info、绑定不变及整批失败 |
| Reject foreign resources before native work | 6.3 | 真实 foreign Device/context、RTTI、name reuse |
| Preserve resource dependency ownership | 3.1, 6.1, 7.2 | 强引用与 final release、外部 owning handle |
| Delete native objects on the owning context thread | 3.1, 6.3, 7.3 | worker 零 GL/零分配、FIFO/精确一次/context 退出 |
| Preserve resource creation failures transactionally | 2.3, 3.2, 4.2, 5.2, 6.2, 7.1 | CPU/native 分阶段注入、回滚、首错/次错 |
| Reject unavailable operations without fictitious GPU work | 2.1, 2.3 | 完整 68 方法矩阵、unused payload、空/0/false/logic_error |
| Consume native and generic capabilities independently | 2.1, 8.1 | 新独立 binary、实际 targets/flags/links |
| Report allocation separately from completed rendering | 1.1, 8.2-8.3 | D01 入口、native/注入边界及实际工作包状态 |
| native-window-host: Create a verified native OpenGL context | 2.3, 7.1, 8.1 | 全部旧场景、新 RHI/Validation 失败与借用 |
| native-window-host: Release native ownership completely and repeatably | 7.2-7.3, 8.1 | 全部旧场景、ResourceInUse、drain/重试/重复创建 |

## 证据记录约定

每项实现时记录真实命令、构建配置、观察值、对照条件和原始输出路径. CPU 分支、受控 native 返回、真实 GL 对象查询和 OS/driver 观察分开注明; 不以数量相等代替内容审查, 不以临时 fixture 成功证明不存在的产品执行路径. 当前进度以清单及下方实际实施证据为准.

原生环境记录 Windows/SDK/MSVC/CMake、固定 GLFW/GLAD、GL vendor/renderer/version/profile、可见性和当前显示器/DPI 条件; 新 binary 与产物保留在 build/artifacts/establish-opengl-device-resources/. 后续首次实际配置时把所执行命令追加于此, 不把示例命令或历史 build 的结果当作本包执行证据.

## 规划检查记录 (2026-10-26)

已核对当前源代码、架构/开发计划、四个主规格全文及相关 D01 产物, 按官方产物指令完成 proposal、两份 delta spec、design 和 tasks. 当前共 14 项新增要求、2 项修改要求、60 个 WHEN/THEN 场景, 既有宿主两项要求的 7 个旧场景完整保留. design 的 12 项决策明确 17 格式/九维度、generic/native 目标边界、无分配退休、引用预检、Normal/ContextUnavailable 退出和后续工作包范围, 没有影响实现路径的未决问题.

本轮原生 status 为 4/4 规划产物完成, apply 解析为 ready、0/20 实施任务完成; strict 6/6 通过, 本 change 无提示, native-window-host 的原有两条长要求 INFO 保留. 10 份相关 Markdown 的 964 个本地链接/锚点、68 项 Device 虚方法及 33 个所引用 GL 常量的实际声明核对通过, git diff --check 通过. 规划审查修正了正常关闭拒绝与已停止带错的区别, 补齐不可用 context 的可观察退出场景, 保持完整原生前置.

原始规划检查和 CLI 输出保存在 ignored build/artifacts/establish-opengl-device-resources/, 检查脚本只读取源码/文档并写该 binary 路径. 当前只修改本 change 和 Developments 的待实施关联, Architecture、AGENTS、Build、RHI 来源记录、生产代码、第三方、既有主规格/归档及 D01 change 保持不变. D01 仍为 15/16 且阶段入口未满足; 没有运行本包 C++ 构建或 native 测试、勾选实施任务、同步主规格、归档或提交 Git. 上述结构/文档结果不作为未来资源行为的执行证据.

## 实施入口核验 (2026-10-26)

本次 apply 已重新读取 CLI 返回的全部 contextFiles 和当前规范, schema 为 spec-driven、apply 解析 ready、进度 0/20; ready 只表明产物齐备, 不代替任务 1.1 的前置验收. 原生 list 显示 D01 原 change 仍为 15/16, 其 4.1 未勾选且 Developments D01 出口未通过; 当前 System.Windows.Forms.Screen.AllScreens 报告 1 个活动显示器, 无法完成不同缩放的真实跨屏验收, 不从该查询推断本机 DPI 或像素尺寸.

按任务 1.1 和 design 第 1 节, 当前入口不通过, 本项及所有后续实施项保持未完成. 已向维护者提出本 change 范围内的明确例外选择, 等待决定后才能修改前置或实施 native; 既有 T01/T02 授权没有自动扩大. 此次只记录核验, 未修改生产代码、Architecture、原 change 或主规格, 未执行 C++/native 测试、归档或 Git 提交.

## 限定前置授权 (2026-10-26)

维护者随后明确答复“同意限定推进，修订规划后继续（推荐）”. 该授权只允许本 change 的 T03 设备、基础资源、必要删除及既定 native 测试, 已同步当前五类文档. D01 4.1/阶段出口、T04 上传、T05 全面读回、D03 绘制提交和 D01/D02 总出口保留未完成. 前一节保留当时入口核验事实, 不将其写成已通过或重写历史. 实施入口现在按本次明确例外核对, 不再重复请求同一授权.

## 实施证据

1.1: 本次明确授权已同步 proposal/design/资源 delta/tasks/Developments, strict 6/6 通过, 当前 change 无提示. D01 原任务、归档和主规格未改, 15/16/单屏事实保持; 本任务通过的是已授权的限定入口, 不是 D01 出口.

1.2: PrepareImplementation.py 已保存 implementation-inputs.json: 原始 tracked 源码/主规格 hash、Git 基线、完整 68 项 Device 声明和固定 15 个输入的 LF SHA256, 全部来源匹配. 已重读当前产物、公开资源/宿主/私有互操作及 root/module CMake, 保留无 loader 的 generic 独立消费入口. Native 查询 fixture 使用正式 Host、同一 GLAD/第三方 target, 暂无生产源改动.

3.1/4.1/5.1/6.1 (2026-10-27): native/checks 正式 target 的 host-storage-debug-run.log 与 storage-release-run.log 均通过, 各命令/配置/源内容 hash 见 runs.jsonl. 在同一正式 Host/Validation 上查询 Buffer 容量/immutable flags、九维度所有 mip/层数/实际 MS、所有映射或排除格式和非 MS swizzle, 80 种 sampler filter/address/reduction 组合及独立纹理状态, color/depth/组合/array/cube/MS FBO、单层 mip extent 和 typeless SRGB view、深度只读 metadata、外部 read/draw binding 与 caller texture 释放后的强依赖. Buffer worker final Release 在禁分配下零 GL, owner 精确一次删除及空队列重复收集通过. 这是存储/参数/寿命查询, 未读取未初始化像素或调用上传/绘制/等待. 更广的故障及 Host 回归随其它项继续; 编译期和 fixture 查询错误已修正, 初始失败日志保留.


## 原生实施与逐组证据 (2026-10-27)

环境: Windows x64, MSVC 19.44.35228/SDK 10.0.26100.0, CMake 3.30.0-rc4; 使用仓库固定 GLFW/GLAD. 实际 vendor 为 Intel, renderer 为 Intel(R) RaptorLake-S Mobile Graphics Controller, version 为 4.6.0 Build 32.0.101.6129, Core profile. limits 为 texture/cube 16384、3D 2048、array layers 2048、color/draw attachments 8、MAX_SAMPLES 16、anisotropy 16、LOD bias 15. 当前仍只有 1 个显示器, 原输入检查输出 CROSS_DPI_VERIFIED=0, 不关闭 D01 缺口.

可复核入口均在 ignored build/artifacts/establish-opengl-device-resources/. RunStep.py 的 runs.jsonl 及 Suite.py 的 suites.jsonl 保存实际参数、配置、返回码、耗时、环境开关和运行时源 LF hash; 命名区分的初始编译/fixture 失败日志保留; 迭代复跑的同名汇总 log 保存最新输出, suites.jsonl 保留各次参数/返回码/源标识, 不将早期结果作为最终源码的证据. 构建 source 为 checks、generic-checks、regression-checks 及仓库根, binary 分别为 native、generic、regression-final、product, 都由本次新目录离线配置. 使用 Visual Studio 17 2022/x64 和 SDK 参数, PHYSARA_SOURCE 指向本检出目录. 生产构建不含任何检查宏或测试入口.

2.1-2.3: 后端静态目标和完整 68 项 override 已编译, deprecated 重载/扩展保留. 两配置首 include 的 HeaderCheck/BackendHeaderCheck、generic 六公共头/三个 Common 私有头和正式消费者通过. 12 项 generic CPU/契约程序各配置通过, 10 类旧接口编译各按预期拒绝. implementation-inputs.json/device-method-matrix.json 保存输入与方法归属; ImplementationAudit.py 复核全部定义、源码/许可/命名、闭包、File API 和生产库无检查符号. Native factory/Host、mandatory context/callback/entry、CPU 准备、wrong thread/current、clip 查询、所有 Feature false 且 output 不变及完整 Unsupported 集合通过. 真实 17 格式 x 9 target 的 flags/view class 与完整样本集合逐项匹配; MS 实际为 {16,8,4,2}, 当前 profile 采用 {2,4,8}. CAVEAT、非 boolean、低版本、非法 limit 和 query error 为单独标明的受控分支, 不宣称本机自然出现这些能力缺失.

3.1-6.3: native-Debug-all.log/native-Release-all.log 包含 Buffer/九维 Texture/80 种 Sampler 参数组合/FBO 和 view 的真实查询. 已检查全部映射/排除格式、非对称尺寸、每 mip/array/cube face/MS、linear/sRGB、component mapping、LESS、metadata 保留、深度只读标记、FramebufferInfo 与 binding 不变. 所有可用单层附件维度还实际建立 SRGB reinterpret view, 按实际 view target 预检. 原始异常输入均检查零 native 创建, name/storage/parameter/view/FBO setup/incomplete 的 17 个取得真实对象后受控失败检查回滚及首错. CPU wrapper/长名称/退休节点分配点失败后可重试. Hole sample、较低附件限制和单层 view target 缺少支持以受控 caps 分支验证, 与本机实际支持分开; 没有真申请最大容量、耗尽 GPU 或读取未初始化像素.

四种 wrapper 的 worker final Release 在禁 CPU 分配下没有 GL, owner 排空精确一次; 实际观察 FBO -> view -> texture 的 FIFO 次序和依赖强引用. 辅助真实 context 的 vendor/renderer/Core 和 33 个 required entry 地址在切换前后匹配, 从未重载 GLAD; foreign Device 的真实纹理及另一 live 实现均在 FBO/view 创建前拒绝, 两设备随后可用. 本机实际 deleted texture 1 -> 1、FBO 1 -> 1 在第一次再分配即复用, 新 extent/format/附件查询没有旧身份或 view 误认.

7.1-7.3: 同一正式 Host 在 Ready 前装配 raw/Validation, borrowed GetDevice 不加引用, wrong-thread/inactive 返回空. RHI/Validation/swap/input 失败、随后重建、CPU/native 主错与 GLFW cleanup/日志失败均检查. 四类活资源、额外 raw/validated 引用和 FBO 内部依赖关闭前拒绝, Ready/input/current/callback/process claim 保留, 释放/join 后同一 Host 重试成功. raw owner 唯一性、Stopped 幂等、旧 Host 不影响新 Host、context 恢复失败及 known-loss 分支通过. OUT_OF_MEMORY 不自动推断 context lost, CONTEXT_LOST 受控返回后不再删除; 保留首 native code. 停止后纯 CPU Device 的最终引用可在 worker 释放, 没有 context check/GL/callback; Host 的 owner-thread 合同保持. 外部 owner 未收拢就析构的隔离子进程精确退出 86, 原 Host 的两种致命合同退出仍为 91.

实现随审查修正了三个边界: native 错误携带对象/标签与实际调用位置; array/cube 单层 view 使用实际 view target 的 caps; 普通初始状态元数据与通用 GL profile 保持一致, 不把 StreamOut/Present metadata 当作执行能力. C++20 的既有 enum flag operator 非 constexpr, 因而普通状态集合使用只读局部值, 不修改公共运算符. 编译期符号比较、fixture 的 GL 查询和负例构造错误都已纠正并保留原始失败; 未通过放宽断言、移除测试或全局关闭诊断消除失败.

8.1 的已运行回归: 独立 generic 两配置 12 正例/10 编译负例, Core/原 Host/输入的两配置全部通过; 原 Host 每配置含 32 次生命周期, 输入 callback/窗口/焦点/文本/ImGui 链及现有故障用例保留. Runtime 每配置 20 个 CLI、14 个故障/owner 场景、可见 resize/iconify/close 子进程检查及标准 help/unknown/hidden 精确 0/2/0 通过. 最终 native/产品源变动后只重建/复验受影响闭包, Core/通用算法来源及代码未变. API/主规格/历史归档/Architecture/第三方均由保护检查确认未修改. 尚待本章最终交付核验结束才勾选第 8 组.


## 最终 verify、Code Review 与交付 (2026-10-27)

按 openspec-verify-change 的完整性、正确性、一致性三维核对 CLI 返回的五份当前产物. 正式 OpenSpec 1.13.2 的 list/status/apply、strict 输出保存在 verify-openspec-*.json/log; status 的规划完成字段未被当作实施完成. 全部 20 项验收已完成, 最终 apply 状态另行回读确认. 未执行 sync/archive 或 Git 提交.

| 维度 | 最终结果 |
|---|---|
| 完整性 | 20/20 任务, 14 ADDED + 2 MODIFIED 要求都有实现和验收; 原宿主 7 个旧场景正文完整保留 |
| 正确性 | 两份 delta 的 60 个场景逐项核对代码与 native/CPU/受控分支证据, 真实分配/查询/归属不由 CPU double 替代 |
| 一致性 | design 1-12、68 项 Device 闭包、公开字段/值/布局、来源/许可、模块依赖及命名符合; 本地链接与 git diff --check 通过 |

Code Review 已回答: caps 由实际 driver 查询构造且不可变; wrapper 自持描述和 Device, FBO 的 descriptor/strong handles/兼容信息分别承担借用合同、所有权和只读派生数据, 无无意义大数据搬运. 没有视图缓存或地址 registry, FIFO 节点在创建前取得, 不形成 Device 循环. final Release 无 GL/新分配, FBO/view 在 storage 前退休; 最终 CPU Device 引用在已停止后可跨线程释放. callback 先保存有界主错再写日志, 寿命覆盖全部借用, 所有关闭前置可解释. 当前没有业务 GPU 提交, 不宣称 CPU 引用归零证明 GPU 完成; 上传和真实在途引用保护继续归 T04/D03.

本轮 verify 另外发现并修正了 collection/Shutdown 的既存 GL 首错检查遗漏. 新的 PriorErrorCollectionChecks 在真实 buffer 已退休时注入 prior INVALID_ENUM/CONTEXT_LOST, 证明错误发生前零删除、有效 context 可重试、loss 分支不再 GL; 正常关闭开始后的错误仍停止并由 context 最终销毁未删对象. 完整 native Debug/Release、两种正式产品构建、首 include/隔离析构及 0/2/0 入口在修正后通过; ImplementationAudit.py 和 strict 重新通过. 初始编译/检查输入错误与本次修正均未靠放宽检查掩盖.

复核实际 CPU/native 两条寿命、范围和失败路径后, 没有未修复 CRITICAL/WARNING/SUGGESTION, 没有以缺证据通过本 change 的适用验收项. 来源和结构检查是补充, 不是 GPU 语义证明. 本机未自然触发真实 GPU reset/OOM、极限资源容量或不同缩放跨屏; 受控错误/限制分支明确标记, 不把它们描述为硬件观测. 资源内容/行序、raster 图像、Shader/Draw/提交、性能和其它平台不在本包完成声明内.

Developments 对应工作包已更新: T03 完成其限定资源范围, T04 共用已验证删除子集而上传仍待实施, T05 和 D02 总出口保持未完成. Architecture、四个既有主规格、历史归档、D01 原 change 和第三方未改. 当前仍为已实现但未归档/未提交的 change; 可复核源标识与检查汇总见 ignored final-verification.json 和既有原始日志.

## 归档复核 (2026-10-27)

按 openspec-archive-change 与 openspec-verify-change 重新读取当前规划产物和实际实现. CLI 的规划产物全部完成, 任务为 20/20, 原生 strict 6/6 通过. 对照上轮 final-verification.json 的源标识, 唯一代码差异为 Backend.hpp 的访问标签缩进; 格式化到 stdout 后与前次内容指纹一致, 没有语义修改, 保留维护者当前格式. 本轮没有修改生产 C++.

归档复测输出保存在 ignored build/artifacts/establish-opengl-device-resources/archive-review/. ArchiveReview.py 的 build/run 命令分别以 native、regression-final、product 和 Debug/Release 为参数, 共 6 次受影响目标构建及 30 条执行命令通过, 未发现新增编译警告. native all 在两配置重跑真实存储/参数/格式/样本/附件/跨设备/name reuse、受控故障、退休和 Host 关闭场景; 同时通过首 include、caps probe、Core/宿主/输入消费者及正式产品 0/2/0 入口. 隔离 owner 违例精确退出 86 为预期检查结果. 每次命令、返回码和当时源码指纹见 runs.jsonl, 标准输出/错误按 label 分开保存.

| 核验维度 | 归档复核结果 |
|---|---|
| 完整性 | 20/20 任务, 14 ADDED + 2 MODIFIED 要求及 60 个场景都有对应实现和证据; 两项宿主要求的 7 个旧场景保留 |
| 正确性 | 原生分配/参数/归属/退出复测通过; CPU 与受控错误证据的适用边界保持, 不读取未初始化内容或虚构 GPU 完成 |
| 一致性 | design 1-12、68 项 Device 方法、两目标依赖闭包、命名/来源/许可和公开合同相符; ImplementationAudit.py 通过 |

Code Review 重点复核整批附件预检早于 native 创建、资源强引用与 FIFO 删除顺序、首错/回滚、无分配 worker Release、已停止 Device 的 CPU 寿命和 Host 的可重试 ResourceInUse. 没有未修复 CRITICAL/WARNING/SUGGESTION. 来源说明同步消除已完成 T03 的未来时叙述, 明确 stencil SRV/内容访问仍未启用. Architecture、D01 原 change、第三方和历史归档保持不变.

当前 delta 同步评估为: 新建 opengl-device-resources 主规格的 14 项要求/47 个场景; 修改 native-window-host 的两项要求, 涉及 13 个场景并完整保留其中 7 个旧场景, 其它六项要求及现有 Purpose 保留. 其它三个主规格不变. 同步选择及实际归档结果在维护者答复后记录, 此处不预先宣称已同步或已移动. D01 跨屏缺口、T04 上传、T05 全面读回、D03 绘制提交及阶段出口仍未完成; 本轮未提交 Git.

## 同步与归档结果 (2026-10-27)

维护者明确选择“同步主规格后归档（推荐）”. 按同一 repo root 获取一次官方 specs 指令并复用该快照, 未配置额外 rules; 只把已验证的 delta 合并到主规格, 不复制指令/context 或新增实现计划. 新建 opengl-device-resources 的 14 项要求/47 个场景; native-window-host 的两项要求同步后总计 8 项要求/28 个场景, 既有 Purpose、其余六项要求和 7 个旧场景正文保留. 其它三个主规格的字节指纹未变. 全部 delta 重新比对已同步, 主规格 strict 5/5、移动前全量 strict 7/7 通过.

已将 change 移入 openspec/changes/archive/2026-10-27-establish-opengl-device-resources/, 保留 .openspec.yaml、全部任务和两份 delta 原文. 只为移动修复 proposal/design 的三处外部相对链接; Developments 对应工作包及 RHI 来源说明已指向归档, Architecture 和既有其它归档未改. 原始同步/移动/验证输出保留在 ignored archive-review/, 当前状态以该节和后续最终核验为准, 上方实施与归档前记录保留其发生时事实. 本轮没有创建 Git 提交.

归档后的最终核验通过: active list 已移除本 change, D01 原 change 仍为 15/16; 全量 strict 6/6 通过. 15 份相关 Markdown 的 965 个本地链接/锚点及 git diff --check 通过. 全部生产 C++/CMake 源标识与归档复测时一致, 两份 delta 与元数据字节保留; Architecture、D01 原 change、其它既有归档、三个未涉及主规格和 Git HEAD 均保持不变. 最终汇总见 ignored archive-review/archive-final-verification.json, 没有未完成的本 change 任务或未修复评审项; D01 跨屏与后续工作包边界保持上文所述.
