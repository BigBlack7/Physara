# Tasks

本 change 仅覆盖 [D03-T03](../../../../Docs/Developments/D03-D05-FramePipeline.md#d03-t03). A 实施与本地检查记录见第 10 节; 未经独立 B 验收, 不执行最终 verify、同步或归档. 每组同步交付基本注释、必要文档和检查; 验证分组共用 build 内 fixture, 不是逐场景新增脚本. A 只勾选自身工作, B 的结论由独立会话填写.

## 1. 坐标定位与现有即时基础

- [x] 1.1 A: 在 `build/d03-t03/` 用历史确切区域/texture 输入复现 UPPER_LEFT 具名 FBO depth clear 观测, 分离 native clear、scissor 与逻辑读回行序; 以 V3 的小复现记录当前设备/驱动、固定预期、原始结果和定位结论, 将合规命令选择补入 design, 不扩大为通用驱动调查.
- [x] 1.2 A: 扩展同一 ContextState、必需 native entries/limits 与有界 VAO/clear 辅助对象, 接到既有创建/删除入口和 Open/Close/ClearState; 以受影响目标编译、实际创建/退出和新旧 recording 的状态观察验证 owner/claim 不变、缓存正确失效且未引入第二套列表/缓存.

## 2. clear/copy 与对应实际用途

- [x] 2.1 A: 在同一 Common tracker/backend 上接通本 profile 的 clear/copy 与图形消费状态, 区分 Automatic/显式用途, 处理 Close 初态恢复和错误录制; 先用 V4 的 transfer 部分/状态记录核对用途顺序与提交发布, 不使用全域 barrier、BeginTracking 假转换或无条件清空未满足依赖.
- [x] 2.2 A: 实现二维选层的 float/整数颜色及 depth/stencil clear, 校验全部选定子资源, 保留未选 aspect, 通过唯一 ContextState 排除残留 scissor/mask 干扰; 用 V1 的 typed 内容/未选子资源读回和 V3 的 depth 复现验证清除完整性, 同步接口/流程注释.
- [x] 2.3 A: 实现普通 Buffer 区间和同格式、单样本二维选层区域 copy, 校验 native 表示、边界与同底层非法重叠; 以 V1 的已知 byte/mip/layer/方向和 V5 的代表性拒绝确认精确改动及外围保留, 同步明确 staging/resolve/1D/3D/Buffer UInt clear 仍未交付.

## 3. 可执行状态、资源映射与必要 owner 接入

- [x] 3.1 A: 实现图形候选状态的同设备/实现、完整 FramebufferInfo、布局、活动 extent/recordStride、VB/IB 与动态输入预检, 在 native 应用前准备必要包引用; 把 Pipeline/集合/Framebuffer 的复合依赖纳入原内部 owner 记账, 保持 trackLiveness 的公开含义; 以 V5 的候选拒绝及 owner 账目检查确认无部分发布、真正外部 owner 可区分, 源码检查引用先于 native 使用且无逐 Draw 递归保活.
- [x] 3.2 A: 按当前 Pipeline 映射应用 UBO/只读 SSBO ranges、跨集合采样对、双 sampler/dummy 及 InputLayout 的属性/VB/IB 计划, 在 plan/任一集合端改变时重映射; 以 V2 的实际 native 绑定/格式/offset 观察验证, 不依赖对象指针相同跳过新 plan, 对应像素证明在 5.1 集成.
- [x] 3.3 A: 完成相关 CMake 显式文件接入、RHI 头/README 的本次消费边界与第一方职责/接口注释; 以目标构建、依赖 diff 和文档链接检查确认无 ABI/公共签名/第三方变更、无默认测试消费者, 注释不把状态准备或保活记账说成 Draw/GPU 完成已验收.

## 4. 单样本固定/动态状态与坐标合同

- [x] 4.1 A: 通过唯一 ContextState 落实 raster/cull/front-face/depthClip/bias、viewport/scissor、depth/stencil 及静态/动态 reference; 以 V3 固定输入的 native 状态观察先核对映射、空数组语义与只读 depth 前提, 在 5.2 读回坐标/深度结果, 不在数据/Shader/后端重复翻转.
- [x] 4.2 A: 实现有效颜色附件的 blend/常量/write mask 与 framebuffer-sRGB 状态, 明确 typed clear 和线性/整数处理; 以 V3 的已知中间值与 native 状态核对编码边界, 像素结果由 5.2 验证, 更新对应注释/已交付 profile 说明, 不静默忽略非默认字段.

## 5. 实际直接 Draw 与交错消费

- [x] 5.1 A: 实现即时 Draw/DrawIndexed/实例参数发射、R16/R32 索引、VB/IB offsets 与 native 类型转换检查, 合法零 count 不发 Draw; 用 V1 的非幂等输出/录制与 Execute 观察及 V2 的已知索引/实例结果证明不重放、baseInstance/baseVertex 各生效一次, 并完成 V5 缺失状态/非法 Draw 的 Release 拒绝.
- [x] 5.2 A: 用 V3 的非对称图、CCW/负缩放、部分 viewport/scissor、Reverse-Z/常规深度、stencil/blend/masks 与 sRGB/线性中间值完成真实像素检查; 将 1.1 的定位落实为产品 UPPER_LEFT 路径的正确结果, 记录固定取点/容差, 未解决不得转交后包而勾选本项.
- [x] 5.3 A: 在 Draw 实际消费处完成用途检查和必要 native 状态重建, 覆盖设置后又 Write/Clear/Copy、相同状态命中与 plan/Framebuffer 切换; 合并 V4 Automatic/显式结果及 V5 feedback 拒绝, 用 V6 的实际 release/abandon/完成/关闭序列证明本次新增引用安全退休, 不重做未变的 fence/poll 全矩阵.

## 6. A 本地集成与交接

- [x] 6.1 A: 构建正式 Debug/Release 的受影响目标及实际消费者, 用新 binary tree 证明正式构建不依赖本地 fixture; 按下列预算完成尚未取得的原生/拒绝集成证据及必要 Debug smoke, 仅当公共/Validation/构建闭包改变时扩大到相应消费者, 保留失败/复用/未覆盖记录, 核对 Runtime 文档未误称产品已有 Renderer/画面.
- [x] 6.2 A: 整理实际 commit、相关暂存/未暂存 diff、新增文件、最终输入及构建/可执行文件/fixture 标识、命令/结果与限制, 更新当前问题单和开发计划为待独立审查后交维护者; 确认 B 项与收尾项仍未勾选, build 材料不是 B 的代码审查范围, 必要复现输入另行交接.

## 7. 独立审查、整改与复核

- [x] 7.1 B: 独立核对合同、实际 diff 与消费者, 通读本轮涉及的完整第一方 C++/Shader/CMake 文件并完善职责、接口合同和流程注释; 在当前问题单写稳定 CR、版本与覆盖, 分别给出正确性/注释结论, 不把 A 本地检查当作 B 结论.
- [x] 7.2 A: 按 CR 逐项整改并纳入 B 注释, 只复查受影响的实际风险/输入, 在当前问题单和本 tasks 留存修正与证据后交回 B; 无正式发现时注明整改不适用及版本未变依据, 不代勾 B 项.
- [x] 7.3 B: 复核实际最终代码、匹配证据和完整文件注释, 闭合全部正式问题并分别确认代码/注释验收; 未通过保留编号继续 A/B 本轮, 仅由 B 勾选本项, 验收后改变代码/构建输入须重新复核受影响范围.

## 8. 最终验证、同步与归档

- [x] 8.1 A: B 接受最终版本后按原生 openspec-verify-change 执行最终 verify, 对照全部 requirement/scenario/design/task 与匹配证据, 解决完整性/正确性/一致性缺口; 结构校验或 CLI 文件存在状态不得代替实际行为和 B 验收.
- [x] 8.2 A: 将 CR、A 修正/B 复核摘要、完整文件注释覆盖、最终版本、实际运行/复用及未覆盖范围保存在本 tasks, 再按原生 sync 同步两份 delta 并严格校验主规范; 确认未改历史归档且 T04/T05/呈现与阶段出口状态诚实.
- [x] 8.3 A: 按原生 archive 归档, 核对归档内容/相对链接与开发计划引用后更新本包完成状态; 仅归档成功后按模板重置当前问题单, 失败则保留现场, 不启动下一工作包实现.

<a id="verification-budget"></a>
## 9. 验证预算与停止条件

| 分组 | 合同与独立风险 | 最小充分检查及停止条件 |
|---|---|---|
| V1 即时 clear/copy/Draw 链 | 执行时机/重放错误, typed clear 被 scissor/mask 截断, copy 偏移或行序错误 | 同一小 fixture 逐组建立已知颜色/整数/双 aspect 与外围内容, 检查非零 byte/二维区域 copy; Draw 接通后用非幂等结果配合录制/Execute native 观察. 各已选内容正确、外围保留且无重放即停止, 不遍历全部资源形状/格式 |
| V2 实际绑定和绘制取数 | set/slot、pair 或 plan 错位, stride/offset、索引宽度和实例 builtin 错误 | 同一正式 Shader 包/RHI 消费者用少量固定子图表示不同集合的常量/只读 SSBO range、双 sampler/跨 set/dummy、Pipeline 切换, 属性/无属性与 R16/R32 的非零偏移/实例标记. native 绑定与独立像素/数值均匹配即停止, 不重跑准备阶段的反射/limit 矩阵 |
| V3 坐标、深度与颜色 | UPPER_LEFT 已知 clear 观测, cull/scissor/Reverse-Z 或颜色/掩码映射错误 | 1.1 先做精确 native 小复现. 后续复用非对称小图, 在内部固定点检查正反面/负缩放、部分区域、两种深度 convention、代表性 stencil/reference 与 blend/masks、非饱和 sRGB/线性值. 当前异常有定位且产品合同下结果正确即停止, 不追查无关驱动问题/每个状态枚举 |
| V4 实际用途与缓存 | 缓存命中跳过依赖, clear 后状态污染, 重复同步或错误数据版本 | 一段 SetGraphicsState→普通 Write/Clear/Copy→Draw 序列, 再用显式用途入口运行同一内容; 观察结果、用途顺序和唯一发射, 确认 Close 恢复/提交发布边界. 不增加未来 UAV/epoch、多队列或性能测试 |
| V5 边界拒绝 | 非法公开输入在 Release 发到 native, 或不支持项伪成功 | 对新增缺口各取一个代表输入: 缺失状态、外来对象/不兼容目标、活动 range/stride、索引范围/native 窄化、非法 feedback、执行 profile 外请求. raw 后端与真实 Validation 消费路径覆盖各自责任, 以零受影响 native 工作和操作诊断判定; 不按所有对象/字段扩成组合矩阵, 同路径匹配证据直接引用 |
| V6 新绘制引用接入 | 提前退休、已发前缀失主、复合内部 owner 被误判外部而无法退出 | 一个有界序列检查默认保活下释放 caller handles→真实完成, 另一个 issued Draw 的 abandon/实际录制错误分支及保留/释放真实外部 owner 的关闭预检; 观察 wrapper/native 退休与正常退出, 不只数 AddRef. trackLiveness=false 的合法 caller 保活由同一消费者/源码核对. 不遍历分配点、fence/poll 故障和完整 T04 矩阵 |

六组是风险分组, 可共用输入与检查结果, 不要求六个程序或把 32 个 scenario 写成 32 个测试. 各组先交付自己能运行的局部观察, 首次 Draw 后才补跨模块像素/寿命集成. 不建立新测试框架或正式故障开关. 开发材料只在 `build/d03-t03/` 分类, 其路径不作为永久工程入口.

默认在同一真实 GL 设备上运行 Release 原生组, Debug 运行 V1/V2 的最小正向链及编译检查. V5 的新公开拒绝须实际运行 Release; Debug 同一分支由配置/源码确认, 存在差异才追加针对性运行. 固定源/fixture/构建/可执行文件及设备/驱动身份后记录结果, 不运行旧 exe 冒充当前结果. 已有相同输入/环境/检查的有效证据可明确复用; 本地材料已删不能假定仍可重跑.

新增检查必须指出独自捕获的缺陷和独立预期. 达成本包合同、各实际风险已有证据并说明限制即停止; 仅失败、输入变化或具体缺口允许扩大. 本包不要求性能结论、跨 GPU/OS、自然 OOM/context loss、全历史矩阵、完整 T04 在途验收、T05 固定图像门槛或呈现结果.

## 10. 实施与交接记录

1.1 实测: `cmake -S build/d03-t03/fixture -B build/d03-t03/checks -G "Visual Studio 17 2022" -A x64`, Release `CoordinateProbe` 构建/运行 exit 0. 初次沙箱配置因 SDK 目录访问拒绝失败, 授权读取已安装工具链后正常配置, 未修改构建选项. MSVC 19.44.35228.0/SDK 10.0.26100.0; Intel RaptorLake-S Mobile/GL 4.6.0 Build 32.0.101.6129. 对 D16 cube mip=1/layer=0 的 2x2 图, 初值全 1、scissor=(1,0,1,1)、clear=0: UPPER_LEFT 的 named/bound FBO clear 均为 [1,1,1,0], texel-region clear 为独立预期 [1,0,1,1]; LOWER_LEFT 三路径均为 [1,0,1,1]. TextureSubImage 与 ReadPixels 逐值一致、GL error=0. 关闭 scissor 的完整子资源 clear 在两种 origin 下均全 0. 本地输入/日志位于 build/d03-t03, 本结果不代替尚未实施的产品 Draw/scissor 验收.

上述 1.1 为实施前定位, 产品路径的后续结果见下表. 规划创建和结构检查不等于实现验收; 独立 B 审查、A 整改/B 复核和最终收尾项继续等待维护者调度.

2026-11-06 规划核对: 使用官方 OpenSpec 1.13.2/spec-driven, proposal/design/specs/tasks 四类产物齐备; `openspec validate establish-opengl-immediate-draw --strict --json` 通过且无 issue, `instructions apply --json` 为 ready、0/21 完成. 两份 delta 共 9 条新增/2 条修订 requirement、32 个 scenario, 修订块保留原有全部场景. L0 核对 7 份相关文档的 152 个本地文件/锚点链接和 diff 通过; T04 以后工作包内容、Architecture、主规范、既有归档、当前问题单及正式实现均未改动. 本记录只证明规划与文档结构, 不计作实现或原生验收.

<a id="native-evidence"></a>
### A 本地实现与原生证据, 2026-11-07

实施仍基于 `v2.0` / `6bad1b83353b3b7e00533a3ebfdc040a4ddf5424`, 未提交或暂存. 环境为 Windows x64、CMake 3.30.0-rc4、MSVC 19.44.35228.0、SDK 10.0.26100.0; 原生检查沿用 1.1 同机默认 GL 设备, Intel RaptorLake-S Mobile / GL 4.6.0 Build 32.0.101.6129, 未更换适配器或产品 clip convention. 开发代码、Shader/schema、CMake 和日志全部在可删除的 `build/d03-t03/`, 正式 CMake 未注册检查或样例包.

| 分组 | 实际运行与独立预期 |
|---|---|
| V1 | Release/Debug `TransferChecks` 通过. RGBA8 array 的 mip=1/layer=1 全部 texel 为 [64,128,191,255], 未选 layer 保持零, 不受原生 scissor/全关 mask 干扰; R32_UINT 全部为 37. D24S8 分别清 depth/stencil 后为 depth=.25/stencil=0x52, 非零区域 copy 保留双 aspect 和外围 .9/0x21. Buffer byte offset 复制读回 [0,0,0,22,33,55,66,0], 颜色区域 copy 保留未选 row. `DrawChecks` 的非幂等混合值在录制/Execute 前后均一致, native Draw 计数不增加 |
| V2 | Release 全链及 Debug 最小链通过正式接口生成→canonical 包→LoadRhiProgram→公共 RHI/真实 Validation/backend. 非零 VB byte offset、stride、实例 slot、RGBA8_UNORM 和 uint 属性得到左右固定点 [2,1,5.665,1.5] / [3,1,7.665,1.5]. R16/R32 使用非零 IB byte offset、firstIndex、baseVertex 和 baseInstance 得到同值. UBO/SSBO offset=256 的 native range 与像素一致; 同集合切换 Pipeline 后 UBO 单元改映射到正确 Buffer. 只换 sampler 集合后 alpha=1, builtin-only/PointSize 路径有实际像素 |
| V3 | Release 在 UPPER_LEFT/ZERO_TO_ONE 下通过. viewport=[2,12)×[1,9), scissor=[4,8)×[2,5): (5,3) 有值, (5,12)/(3,3) 为零. Reverse-Z clear=0/Greater 与常规 clear=1/Less 均取第二个绿色图形, depth 分别为 .8/.2. 负 X 缩放在原 front-face 下被剔除, 仅切换目标空间绕序后恢复, 空动态数组保留本 recording 区域. 动态 blend constant/write mask 得 [ .275,.2,.375,.4 ], stencil 低四位替换后为 0xa7, depth=.7 和外围保持. sRGB Shader 写入 .25/.5/.75 得 bytes≈137/188/225, 采样三次 red 之和≈.75, 编码/解码各一次; 同 Pipeline 可绘制到另一个 8×8 兼容 FBO |
| V4 | Release 同输入的 Automatic/显式路径均通过. SetGraphicsState 后 WriteBuffer、完整 clear、copy 后直接 Draw, 不重复设置同一图形状态; 起始值均 [2,1,5.665,1.5], 后继值均 [2,1,5.915,192/255]. 显式路径请求用途/Commit, 实际最终 CB 为 ConstantBuffer; 缺少用途的反例拒绝且无成功提交. shader/附件用途复用 Common tracker, 当前 profile 无 Shader 写, 源码中未添加全域 barrier 或普通路径 glFinish |
| V5 | Release 运行代表性新增边界. Validation 路径核对未 Open/新 Open/ClearState 后缺状态、不兼容 FBO、clear 越界、Buffer 重叠、索引范围与未交付调用的拒绝. raw 路径核对 foreign 实现、活动 extent/recordStride、feedback、多 viewport、native count 窄化和 copy 非法区间/重叠. 各次有操作诊断, 不增加 Draw 计数或改变当前 Program/FBO, 无 GL error; 非法候选后旧有效状态仍可正确 Draw. 合法零 vertex/instance count 无 Draw |
| V6 | Release 两个有界真实序列通过. issued Draw 后用实际不支持状态使 recording 无效, Execute=0, worker 释放列表只交接; 释放 caller Pipeline/集合/FBO 等后 Program 仍存活, owner WaitForIdle/GC 后像素正确且 Program 已删除. 正常默认保活提交释放 caller handles 后保留一个真实外部 Shader owner, Host.Shutdown 返回 ResourceInUse 且 Ready; 释放后受控关闭 exit=0. trackLiveness=false 的集合另由 caller 保活到 WaitForIdle 再释放. 未重做 fence/poll/故障矩阵 |

颜色浮点容差为 2e-4, depth24 量化容差为一个量化单位或 1e-6 归一化值, sRGB bytes 为 ±1、采样和值容差 .006. 固定取点在图形内部. 各组复用同一小 fixture, 未按格式、对象或分配点穷举.

实施阶段命令与日志如下; 最终交接输入的同命令检查以本节末尾 `handoff-*` 记录为准. 构建返回非零时不会继续运行旧 exe:

```powershell
cmake -S build/d03-t03/fixture -B build/d03-t03/checks -G "Visual Studio 17 2022" -A x64
cmake --build build/d03-t03/checks --config Release --target DrawChecks TransferChecks --parallel 6
# 同输入原选项重试成功: release-final-build-retry.log; Release transfer: release-transfer.log
build/d03-t03/checks/Release/TransferChecks.exe
# 补齐同输入 Automatic/显式对照后的当前 fixture 构建/运行: release-v4-build.log / release-draw-final.log
cmake --build build/d03-t03/checks --config Release --target DrawChecks --parallel 6
build/d03-t03/checks/Release/DrawChecks.exe
cmake --build build/d03-t03/checks --config Debug --target DrawChecks TransferChecks --parallel 6
build/d03-t03/checks/Debug/TransferChecks.exe
build/d03-t03/checks/Debug/DrawChecks.exe smoke
```

上述最终有效构建/运行全部 exit=0; Debug 日志为 `debug-build.log`、`debug-transfer.log`、`debug-draw.log`, V5 仅在 Release 运行. 第一方仍使用原有 /W4 /WX、优化和 Debug 迭代器检查, 未放宽门槛. Debug/Release 执行分支未增加差异; smoke 只减少本地运行范围.

正式重建使用独立 `build/d03-t03/clean-product`, 从根 CMake 配置并构建产品 `Physara` 及实际依赖, 不 add_subdirectory 本地 fixture、不需要或恢复开发输入:

```powershell
cmake -S . -B build/d03-t03/clean-product -G "Visual Studio 17 2022" -A x64
cmake --build build/d03-t03/clean-product --config Release --target Physara --parallel 6
cmake --build build/d03-t03/clean-product --config Debug --target Physara --parallel 6
build/d03-t03/clean-product/bin/Release/Physara.exe --hidden --poll-limit=3
build/d03-t03/clean-product/bin/Debug/Physara.exe --hidden --poll-limit=3
```

独立配置、两种构建和两次实际运行均 exit=0, 日志为 `product-configure.log`、`product-{release,debug}-build.log`、`product-{release,debug}-run.log`. 这些运行验证 Runtime 的实际装配/退出, 未把无 Renderer 的事件循环当作画面验收.

失败与复用:

- MSVC 在不同 STL 位置间歇 C1001, 包括原无改动的 Shader 编译模块; 保留 `graphics-build*.log`、`draw-build-1.log` 和 `release-final-build.log` 等首错记录. 原源码/选项重试通过, 根因未确认, 不以改优化或关 Debug STL 检查规避. 初期 C4456/构造要求等自身编译问题及 fixture 的 include 根/初态设置错误均已修正, 仅后续成功输入计入结果.
- 曾在一次编译失败后执行旧 TransferChecks, `transfer-native-debug-run.log` 明确排除. 后续构建/运行用退出码门控; 最终有效日志与输入/二进制身份在下面交接节登记.
- 早期 native 关闭失败定位为本机对 DSA element-buffer 的合法零解绑返回 INVALID_OPERATION. 统一 VAO 的零解绑改用绑定入口 `glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0)`, 非零仍用 DSA; 无 vendor 判断/恢复 LOWER_LEFT/放宽错误. 最终 clear/copy、Draw 和退出均通过.
- T02 的准备/反射/limits、既有通用 Validation 与未改变的 fence/poll 故障矩阵未重跑, 只复用其源码合同和已归档结果; 不把它们记为本轮实际运行. 本轮不测性能、跨 GPU/OS、自然 OOM/context loss、所有格式/拓扑或完整 T04/T05. 正式 ABI、依赖方向、第三方源码、主规范与历史归档保持不变.

<a id="handoff-inputs"></a>
### 待独立 B 的输入与流程

本轮由 A 完成实现/本地检查, B 的代码正确性与完整文件注释均尚未验收. 维护者通过 [当前问题单](../../../../Docs/CodeReview.md) 调度独立会话; B 项、A 整改/B 复核及最终 verify/sync/archive 保持未勾选. build 材料仅供必要复现, 不作为 B 的代码审查范围; 删除后正式工程仍能重建, 本地检查输入须另行交接.

最终交接前清理了 6 处新注释的 ASCII 标点, 未再改变功能. 对固定输入重新完成两种 fixture 构建/运行与独立正式产品的两配置构建/隐藏窗口运行, 均 exit=0. 当前输入对应的有效日志是 `handoff-release-build.log`、`handoff-release-{transfer,draw}.log`、`handoff-debug-build.log`、`handoff-debug-{transfer,draw}.log` 和 `handoff-product-{release,debug}-{build,run}.log`. Release 为 V1–V6, Debug 为约定的 V1/V2 smoke; 日志中的非法输入/外部 owner 诊断是已核对的预期拒绝.

本地 `build/d03-t03/source-inputs.json` 保存 1065 个源码/CMake 输入文件、12 个 fixture 输入、包和 CMakeCache/二进制的逐文件 SHA256; `review-inputs.json` 保存本轮 39 个实际修改/新增文件, 含 Spec/文档. 清单只用于本次交接, 不是正式构建前置. 当前无暂存内容, 基准 commit 未变化. 源码输入聚合 SHA256 为 `5c9074808523e5bd7a1c2afee96c0f56063030b9648ddbcee130c9fd7ed3cd7d`, fixture 聚合为 `f8c9dad5a2725a45a7e5c089356a7d94eb4d99f9889777c378ba57f4d4145444`. 聚合按路径排序, 对 `path + NUL + 文件 SHA256 + LF` 的 UTF-8 串再做 SHA256; 不把分支/commit 相同视为实际输入相同.

| 最终实际可执行文件, 相对 build/d03-t03 | SHA256 |
|---|---|
| checks/Release/DrawChecks.exe | `78ee2bd3b2ea76217c7344c72ff69f168eeb1a73044590692395e0723feba898` |
| checks/Release/TransferChecks.exe | `5b78819c7846c12bb78fdfc9925a8264e408ebcbd7a3f3d30b8db4bf27a0462d` |
| checks/Debug/DrawChecks.exe | `b15c6177cf19bb0c1924988280ade31bd2c98ec7b00fb650f32c294bb7d991d9` |
| checks/Debug/TransferChecks.exe | `4bda5ad598ebdf8d74fafec157ede19e13fc6d873458307271b638068acc14e4` |
| clean-product/bin/Release/Physara.exe | `93ddcf3870a021262bba87f59950460ac0160e5f811dbc526b7b420ff97b9c35` |
| clean-product/bin/Debug/Physara.exe | `9767f97aa87cf49f00f6d4c04631fcb73d7da95656a5fc7cadd431a2267011c1` |

正式新增文件为 `RHI/OpenGL/CommandListDraw.cpp`、`CommandListGraphics.cpp`、`CommandListOwnership.cpp`、`CommandListTransfer.cpp`、`ContextGraphics.cpp`、`Graphics.cpp`. 其余改动为相关既有 RHI 对象/引用/ContextState/显式源码列表及合同说明, 没有新增测试入口、第三方或公共 ABI. 开发计划保留本工作包待独立验收, 不勾选阶段出口或启动后包.

A 收尾: 本轮 10 份文档的 190 个本地文件/锚点链接与 `git diff --check` 通过. `openspec validate establish-opengl-immediate-draw --strict --json` exit=0、issues=[]; `instructions apply` 为 spec-driven/ready, 15/21 完成, 剩余为 7.1–7.3 与 8.1–8.3. 该结构检查不是最终 verify 或 B 结论, 无同步/归档操作. `build/d03-t03/handoff-fixture.zip` 只打包本轮可删除的复现输入/身份与日志, 不含正式源码副本或新依赖.

<a id="review-round-1"></a>
### 第 1 轮 B 审查与 A 整改

2026-11-07 收到 B 的 3 项 P2 正式发现 CR-001–CR-003 及完整文件注释. 实收 28 个 C++/CMake 输入摘要为 `94e4fdff44de6bd5398ec15b223d80e37cbbffdc375f2d885d30248e84b56303`, 与问题单的 B 注释后摘要一致; 六份 OpenSpec 产物与原 A 交接逐文件 SHA256 一致. 以此作为本轮整改起点, 保留 B 的注释及未验收结论.

整改验证预算: CR-001 在同一真实 raw/Validation 消费者执行 S0 有 IB→S1 双 viewport/无 IB 被拒→DrawIndexed 的单一序列; CR-002 各检查一个活动 Src1Color 拒绝和未使用因子的合法对照, 复用 V3 普通 blend 像素; CR-003 检查关闭 depth test/保留 write flag 的只读附件颜色 Pass 与真正启用写入的拒绝对照. Release 运行这三项和受影响的既有 V3, Debug 运行既有 V1/V2 最小链; 通用 Validation 被修改, 补受影响正式产品两配置的增量构建/运行. 不重做完整 V1–V6、fence/在途/OOM 矩阵或新 binary tree 配置. 开发复现仍只在 build/d03-t03, A 仅更新自身整改项后交 B 复核.

本轮 A 整改与结果:

| CR | 实现与匹配的实际观察 |
|---|---|
| CR-001 | `ValidationCommandList.cpp` 的 GL 设置不再发布未经确认的图形快照, 直接 Draw/DrawIndexed 依 owning recording 检查实际生效状态; `ValidationBackend.hpp` 补齐快照仅用于非 GL 的说明, GL 未交付的 push/间接命令沿既有 Profile 拒绝. 同一 Release fixture 的 raw/Validation 均在拒绝 S1 后继续用 S0 的非零 offset R16 IB 绘制, 固定点分别为 [2,1,5.665,1.5] / [3,1,7.665,1.5]; 拒绝期间 native Program/FBO 和 Draw 计数不变 |
| CR-002 | `Pipeline.cpp` 与 `ContextGraphics.cpp` 共用 BlendFactorUse, 准备时拒绝真正消费 Src1* 的通路, inactive 因子原生化为 One/Zero. 格式决定是否有实际颜色/alpha 分量, 因此唯一缓存的颜色条件从 count 改为实际格式序列. raw/Validation 的活动 Src1Color 创建均明确失败、无 GL error; Min/Max 保留全部四个 unused Src1* 后得 [.1,.2,.3,.5], native SRC_RGB=ONE. 同一普通状态连续 R32→RGBA 后 alpha=.45, 独立捕获附件格式改变后因子未恢复的风险; 未增加格式矩阵 |
| CR-003 | `CommandListGraphics.cpp` 只读深度冲突按 depthTestEnable && depthWriteEnable 判断, stencil 条件保持. raw/Validation 两路径都使用只读深度 FBO、关闭 test/保留默认 write flag 实际绘制 [.2,.4,.6,.8], depth=.65/stencil=0x5a 保持. 随后将真正启用测试/写入的候选拒绝在 native 应用之前 |

Release 的三个 CR 和既有 V3 坐标/cull/depth/stencil/blend/sRGB 检查全部通过, 受控关闭 exit=0. Debug 既有 V1/V2 smoke 与 TransferChecks 通过. 正式 `clean-product` 的 Release/Debug 增量构建及隐藏窗口运行均 exit=0. 本轮没有运行完整 V1–V6 或 T04 矩阵, 没有新增 Shader/JSON、正式测试入口、公共 ABI、依赖或能力表; Generic RHI 保持无 native 依赖. 第一方原有 /W4 /WX、优化与 Debug 迭代器检查不变.

```powershell
cmake --build build/d03-t03/checks --config Release --target DrawChecks --parallel 6
build/d03-t03/checks/Release/DrawChecks.exe review
cmake --build build/d03-t03/checks --config Debug --target DrawChecks TransferChecks --parallel 6
build/d03-t03/checks/Debug/TransferChecks.exe
build/d03-t03/checks/Debug/DrawChecks.exe smoke
cmake --build build/d03-t03/clean-product --config Release --target Physara --parallel 6
cmake --build build/d03-t03/clean-product --config Debug --target Physara --parallel 6
build/d03-t03/clean-product/bin/Release/Physara.exe --hidden --poll-limit=3
build/d03-t03/clean-product/bin/Debug/Physara.exe --hidden --poll-limit=3
```

本轮日志在 `build/d03-t03/review-corrections/`: 最终 Release 为 `release-build-2.log`/`release-run.log`, Debug 为 `debug-build.log`/`debug-transfer.log`/`debug-draw.log`, 正式消费者为 `product-{Release,Debug}-{build,run}.log`. 首次 `release-build.log` 因私有 StaticVector 没有公开比较运算符而 C2243, 已改为 ranges::equal; 此失败保留且未运行旧 exe. 随后的成功构建/运行才计入证据, 本轮无放宽选项或 C1001 重试. 日志中的候选、双源、只读写入及真实外部 owner 拒绝均为已核对的预期路径.

相对 B 的正式 C++ 修改仅 6 文件: `OpenGL/CommandListGraphics.cpp`、`ContextGraphics.cpp`、`ContextState.hpp`、`Pipeline.cpp`, 及 `Validation/ValidationCommandList.cpp`、`ValidationBackend.hpp`, 路径均相对 RHI. B 原 28 个输入有 25 个逐字节保持, 其余受影响说明已同步修改; `Pipeline.cpp` 和两份 Validation 文件加入本轮待复核的完整文件范围, C++/CMake 输入共 31 个. 具体相对 B 的差异保存为本地 `a-corrections.diff`, B 注释后的接收清单保留在 `b-received-inputs.json`, 不覆盖首轮 A 历史输入记录.

本轮 `source-inputs.json` 的源码输入聚合 SHA256 为 `999c2e8f07a4956ce97609a815c8cdaefc6e1c7306d251a984975082ff9a0e66`, fixture 聚合为 `eb83dd3416b972fb635c621d34b9d44b2009e95eccb8f805c2ffff83af1ec6fc`, 算法与上一轮相同. 本轮复现材料只打包为此目录下的 `handoff.zip`; 历史 Release TransferChecks 未重建/重跑, 不列为本轮当前二进制. 以下为实际运行的 5 个当前可执行文件:

| 可执行文件, 相对 build/d03-t03 | 本轮 SHA256 |
|---|---|
| checks/Release/DrawChecks.exe | `14c62376e129ad7a951a6f65e64a8e11a92e50e95b1f93b174e07ae539286cc1` |
| checks/Debug/DrawChecks.exe | `7ef0266eb792264e0e3e6438e2a8bd279514d816187c94be7b9c3d3c9b33e8f8` |
| checks/Debug/TransferChecks.exe | `355febd18954161a2415276356cf2cbddab555c4b32cee281ee77c74287545ff` |
| clean-product/bin/Release/Physara.exe | `ced6389565adb3fe603ba8d616e223056cf20533c594cd296619a0183a0d42f2` |
| clean-product/bin/Debug/Physara.exe | `c5bf9b09ed635257b5267a6ff2d3de5c5ebc8be1f1956386a7316c8796d25405` |

A 已勾选 7.2 与当前问题单中三个 A 整改项, B 的复核项及 7.1/7.3 仍由 B 本人确认和勾选. 原始 B 初审结论保留, 本轮交回 B 复核, 不执行最终 verify、同步、归档或重置问题单.

本轮收尾: 31 个当前 C++/CMake 输入聚合为 `fa0ee9dcc907adad57d75abcffdc7dfd3c38bea8cd9c7e1927b86d8cac7b4448`, 完整 42 个修改/新增文件清单在本轮 `review-inputs.json`, 不凭相同 HEAD 推定输入相同. 当前输入/五个 exe 的 SHA256 逐项匹配, diff 空白检查及 10 份文档的 207 个本地链接通过. OpenSpec strict 校验 exit=0、issues=[], apply 为 ready/16 完成/5 待办; 未把结构检查或 A 复查提升为 B 验收.

<a id="b-recheck-round-1"></a>
### B 第 1 轮整改复核, 2026-11-07

本节保留 B 交回时的结论与当时待办状态. 后续 A 已完成最终 verify/同步, 并成功归档; 本轮 CR 与注释覆盖以本 tasks 为持久记录, 当前问题单仅在归档成功后按模板重置.

B 已独立复核本轮 A 的 6 个完整 C++ 修正文件、直接消费者及相应合同/设计. 实查 `v2.0` / HEAD `6bad1b83353b3b7e00533a3ebfdc040a4ddf5424` 的相关未暂存与未跟踪输入; 当前 31 个 C++/CMake 文件的接收摘要与上述 A 交接的 `fa0ee9dcc907adad57d75abcffdc7dfd3c38bea8cd9c7e1927b86d8cac7b4448` 完全一致. 上轮 B 的 28 个输入中有 25 个逐字节保持, 未变范围复用初审.

| CR | B 复核结论与依据 |
|---|---|
| CR-001 | 通过. GL 设置只委托而不发布包装图形快照; 直接 Draw/DrawIndexed 保留 Open/队列前提后由实际 owning recording 检查生效状态与 IB. push/间接路径先拒绝, 所有相关快照读点及非 GL 原路径均已核对. A 的 raw/Validation S0→S1 拒绝→DrawIndexed 记录覆盖原触发条件 |
| CR-002 | 通过. 准备/应用共享 BlendFactorUse, 活动双源因子在 Program 创建前拒绝, inactive 通路改用 One/Zero, Factor 表无双源项. 已核对实际格式序列参与缓存差分, 复用 A 的活动拒绝、Min/Max、R32→RGBA 与 V3 普通 blend 记录 |
| CR-003 | 通过. 只读深度冲突按 depthTestEnable && depthWriteEnable 判断, 模板独立保护保留; 对照实际 native 状态和 A 的只读颜色 Pass/aspect 保留/真正写入拒绝记录, 原合法组合已可通过 |

B 重新核对并补齐 6 个修正文件的职责/接口/实现原因注释: `OpenGL/CommandListGraphics.cpp`、`ContextState.hpp`、`ContextGraphics.cpp`、`Pipeline.cpp` 和 `Validation/ValidationBackend.hpp`、`ValidationCommandList.cpp`, 路径相对 RHI. 覆盖包含统一 GL 状态 owner、inactive 因子、目标格式差分、只读深度及委托前提, 未修改功能或公共 ABI. 31 个 C++/CMake 输入去除注释后的代码比较一致; B 注释后的最终摘要为 `b3517b4afc5565165257504d82511a6fdd2d25c873b37fa209edeb8fff062e46`, 聚合算法与前文相同. diff 空白检查和 10 份相关文档的 209 个本地链接/锚点检查通过.

代码正确性与注释可读性均通过本轮 B 复核, CR-001–CR-003 全部关闭, 未发现新增正式问题. 稳定 CR、A 修正与逐项 B 确认继续保留在 [当前问题单](../../../../Docs/CodeReview.md), 本节同步留存结论和输入身份. B 仅勾选自己的 7.1/7.3; A 的最终 8.1–8.3 保持待执行, 本轮问题单不重置.

证据使用边界: B 的实际工作为源码、合同、输入摘要、A 回归记录与注释的核对. 原生结果复用前述 A 实际运行的 Release 三个 CR/V3、Debug V1/V2/Transfer 以及正式产品两配置记录; B 未扫描 build 或读取其原始日志, 未独立运行编译/GPU/fixture, 未重新核实 fixture/exe 身份. 注释后的本地代码比较属 L0 检查, 不作为新的 GPU 结果. A 需针对最后版本完成原生 verify 后再同步/归档, 验收后改变代码或构建输入时重新交 B 复核影响范围. T04/T05、呈现及 D03 阶段出口继续按原工作包边界维护.

<a id="final-verification"></a>
### A 最终 verify, 2026-11-07

使用原生 openspec-verify-change 工作流, 当前 change 为 `establish-opengl-immediate-draw`, schema 为 `spec-driven`, planning root 为本仓库. 已读取 CLI status/apply 返回的全部 proposal、design、两份 delta 和 tasks, 并核对实际实现及直接消费者. 本节记录验证结论, 同步和归档按 8.2/8.3 另行完成.

| 维度 | 实际结论 |
|---|---|
| Completeness | 实施/独立审查 18/18 项完成; 11 条 ADDED/MODIFIED requirement 均有实现或交付边界依据. verify 开始时 CLI 为 18/21, 尚未完成的 8.1、8.2、8.3 分别为本次 verify、sync、archive 本身; 本节完成后只勾选 8.1, 不提前勾选后两项 |
| Correctness | 11/11 requirement 已映射, 32/32 scenario 已核对; 真实设备行为沿用下表中与实现对应的 A 实测. D01 前置条件场景核对开发计划, 当前 D01 已通过, 不伪造未满足前置的运行 |
| Coherence | design 的 10 项决定与实现、依赖方向、有限 profile 和最小验证相符; B 已完成完整第一方文件的职责/合同/流程注释验收. 无未验证的必需检查, 无新增行为缺口 |

实现与场景映射如下, 每行的场景数对应 delta 中的全部场景. V1–V6 的固定输入、独立预期、实际值、容差和失败/复用边界见[原生证据](#native-evidence), CR 的更正后证据见[整改记录](#review-round-1)及 [B 复核](#b-recheck-round-1).

| Requirement | 场景 | 实现/文档入口与对应证据 |
|---|---:|---|
| Execute baseline graphics through the existing immediate recording | 2 | [CommandList.cpp](../../../../RHI/OpenGL/CommandList.cpp)、[CommandListDraw.cpp](../../../../RHI/OpenGL/CommandListDraw.cpp) 与既有 Submission; V1 上传/clear/Draw/copy 的单次非幂等结果和 Execute 不增加 Draw, V5 的 Open/Close/ClearState 拒绝 |
| Compose executable graphics state from compatible owned objects | 3 | [CommandListGraphics.cpp](../../../../RHI/OpenGL/CommandListGraphics.cpp) 的候选预检、先保活后发布和 [ValidationCommandList.cpp](../../../../RHI/Validation/ValidationCommandList.cpp) 的 GL 委托; V2 输入格式/slot/offset、V3 兼容不同尺寸 FBO、V5 foreign/range/stride 拒绝及 CR-001/CR-003 |
| Consume the current pipeline resource mapping for every affected binding | 2 | [Graphics.cpp](../../../../RHI/OpenGL/Graphics.cpp)、[ContextState.cpp](../../../../RHI/OpenGL/ContextState.cpp); V2 的 UBO/SSBO byte range、同号不同 set、跨集合采样、双 sampler、texture-only fetch 及 Pipeline/单端集合切换 |
| Preserve direct draw counts offsets and builtin semantics | 3 | [CommandListDraw.cpp](../../../../RHI/OpenGL/CommandListDraw.cpp):24 的参数与 byte interval 检查及即时 native Draw; V2 的两种 IB 宽度、非零 offset/baseVertex/baseInstance 与实例输入, V5 的窄化/越界拒绝和合法零 count |
| Preserve the adopted raster depth and color conventions | 3 | [ContextGraphics.cpp](../../../../RHI/OpenGL/ContextGraphics.cpp)、[Pipeline.cpp](../../../../RHI/OpenGL/Pipeline.cpp) 与 design 决定 5/6; 精确 CoordinateProbe、V3 的非对称区域/cull/Reverse-Z/常规深度/stencil/blend/masks/sRGB, CR-002 的活动双源拒绝/未使用因子/R32→RGBA 及 CR-003 只读 aspect 保留 |
| Clear and copy exactly the supported resources independently of prior draw state | 3 | [CommandListTransfer.cpp](../../../../RHI/OpenGL/CommandListTransfer.cpp) 的整组预检和 typed/full-subresource 操作; V1 的未选 mip/layer/aspect、外围 byte/texel 保留与无翻转复制, V5 越界/重叠/profile 拒绝 |
| Satisfy actual access uses independently of native cache hits | 3 | [CommandListState.cpp](../../../../RHI/OpenGL/CommandListState.cpp)、GraphicsUses 与 DrawDirect 的实际消费; V4 Automatic/显式同结果、Write/Clear/Copy 后不重设图形状态及缺少用途拒绝, V5 重叠 feedback 拒绝. 当前无 Shader 写, 未新增全域 barrier/glFinish |
| Retain newly executed graphics dependencies through the existing retirement protocol | 3 | [CommandListOwnership.cpp](../../../../RHI/OpenGL/CommandListOwnership.cpp)、Upload/Submission/Device 的既有包与删除协议; V6 的 caller release、issued-prefix abandon、真实完成删除与外部 owner 关闭拒绝. trackLiveness=false 由 caller 保活, 未把 T04 矩阵计为本包完成 |
| Deliver a rebuildable limited graphics capability with matching evidence | 2 | [CMakeLists.txt](../../../../RHI/OpenGL/CMakeLists.txt)、[Build](../../../../Docs/Build.md)、[RHI 使用说明](../../../../RHI/README.md); 新 binary tree 正式两配置构建/运行不依赖 fixture, 最终代码/fixture/exe 身份核对及 B 独立验收, 所有开发材料只在 build |
| Reject unavailable operations without fictitious GPU work | 5 | [CommandListUnsupported.cpp](../../../../RHI/OpenGL/CommandListUnsupported.cpp)、[Unsupported.cpp](../../../../RHI/OpenGL/Unsupported.cpp) 与 Validation 的 Profile 拒绝; V5 的未交付入口拒绝及 V1–V4 真实直接图形. 既有 crash helper/未改排除族复用原合同, 未把 link 当作 Draw 支持 |
| Report allocation separately from completed rendering | 3 | [开发计划](../../../../Docs/Developments/Developments.md#current-stage)、本 tasks 和两份 delta 明确分层证据; 当前 D01 阶段出口已通过, 不再使用历史前置例外. T03 结果不闭合 T04/T05、呈现或 D03 阶段出口 |

输入匹配与证据复用:

- 当前分支/HEAD 仍为 `v2.0` / `6bad1b83353b3b7e00533a3ebfdc040a4ddf5424`, 未暂存、提交或推送. 31 个代码/CMake 输入摘要实际重算为 B 最终的 `b3517b4afc5565165257504d82511a6fdd2d25c873b37fa209edeb8fff062e46`.
- 从接收的 B 初审文件与 `a-corrections.diff` 重建 A 最终 6 文件, 逐一与当时 A SHA256 对上; 再以保留字符/字符串载荷的 C++ token 及预处理指令比较当前 B 版本, 证明 6 文件只改注释. 其余源码输入未变. 这是 L0 对照, 没有将旧 exe 运行记为最后注释版本的新编译.
- 本次实际核对 fixture 的 12 个输入聚合为 `eb83dd3416b972fb635c621d34b9d44b2009e95eccb8f805c2ffff83af1ec6fc`, 与 A 回归相同. 五个实际 exe SHA256 均与[整改记录](#review-round-1)相同: Release DrawChecks、Debug DrawChecks/TransferChecks、正式 Release/Debug Physara. 本地核对脚本/清单仅存 `build/d03-t03/final-verification/`.
- 本次复用 A 实测的 Release 三个 CR/V3、Debug V1/V2/Transfer 及正式消费者两配置构建/运行; 未改范围复用初始 V1–V6 的对应结果. 不重跑 GPU、完整矩阵或自然 OOM/context loss. 所有原始失败、排除的旧 exe 运行及 MSVC C1001 限制保留原记录; B 没有独立实测, 本次也没有声称新增原生结果.
- 本次实际运行 `openspec validate establish-opengl-immediate-draw --strict --json` exit=0/无 issue, `git diff --check` exit=0, 10 份相关文档 209 个本地链接/锚点核对无错误. 结构检查只证明结构, 不代替上面的行为依据.

问题分级: 无新增实现 CRITICAL、WARNING 或 SUGGESTION. 原生 verify 的初始任务完整性检查列出的 3 项 CRITICAL 待办为 8.1/8.2/8.3, 需要按顺序执行才能消除; 本节已完成 8.1, 不以 CLI 产物齐备或 B 复核冒充后两项完成. 全部收尾完成后的最终进度与结论在下方留存.


<a id="review-annotation-coverage"></a>
### B 完整文件注释覆盖留存

以下逐字留存 B 最终问题单中的覆盖与验收说明, 归档后不依赖可重置的问题单恢复本轮结论.


以下是 B 初审覆盖及本次整改复核后的最终说明. 初审涉及 28 个代码/CMake 输入; 本次新增核对 `Pipeline.cpp` 与两份 Validation 文件后共 31 个, 原 28 个中有 25 个与上轮 B 交接逐字节一致, 未变范围复用初审.

初审直接完善 25 个文件的注释, 本次再完善受修正影响的 6 个 C++ 文件, 两次合计直接编辑 28 个不同文件的注释. 其余文件保留并核对既有说明. 公共头保留上游许可与原注释, 只补本轮图形合同; 非平凡流程说明使用中文和英文标点, 简单访问器/固定转发共用类型或入口合同.

| 文件或模块 | 文件/类型职责 | 接口合同与失败状态 | 实现阶段与关键原因 | 剩余缺口 |
|---|---|---|---|---|
| `RHI/OpenGL/CommandList.hpp/.cpp`, `CommandListState.cpp` | 明确逻辑列表、Device 稳定物理包与唯一 claim 的职责 | 集合借用、预检失败与 sticky 错误、Automatic/显式合同均保留核对 | 旧 claim、ClearState、用途检查说明与实际 owning recording 一致 | CR-001 已闭合, 无剩余注释缺口 |
| `CommandListGraphics.cpp`, `Graphics.cpp`, `CommandListDraw.cpp` | 候选验证、活动计划消费和单次即时 Draw 分开说明 | 候选发布、trackLiveness=false、单位与只读附件判定已复核 | 补充 depth test 关闭也旁路写入的原因, 其余预检/保活/重映射/issuedWork 说明保留核对 | CR-001/CR-003 已闭合 |
| `CommandListTransfer.cpp`, `CommandListUpload.cpp` | clear/copy 与 CPU 源上传的各自边界清楚; 后者保留既有完整说明 | 补充 typed clear 载荷、完整子资源/未选 aspect、空 copy 和 storage 重叠前提 | 补充辅助 FBO 附件及时解除、整组预检、原始 texel/编码复制; 核对上传的同步源消费和持久区段寿命 | 无新增注释缺口; 原生结果未由 B 重跑 |
| `CommandListOwnership.cpp`, `Upload.hpp/.cpp` | 补充包 lease、父对象强引用边和上传存储的责任区别 | 补充依赖账目为借用、边的重数、真实外部 owner 判断 | 补充包内根去重、先子后父准备、跨包首个/最后一个计账, 不依赖退休顺序 | 无新增注释缺口; 不据静态计账核对宣称 T04 验收 |
| `ContextState.hpp/.cpp`, `ContextGraphics.cpp` | 唯一 native 缓存与有界 VAO/FBO, 新 helper 仅判断因子使用 | 补齐 BlendFactorUse 的 RGB/alpha 含义及有效格式/枚举前提, 其余接口说明保留核对 | 补充 inactive 因子判定与完整格式序列参与差分的原因, 同数目不同格式的恢复合同清楚 | CR-002/CR-003 已闭合 |
| `Backend.hpp`, `Device.cpp`, `Submission.hpp/.cpp` | 保留 Device/删除 FIFO/包/fence 的完整职责说明 | 补充 packetHolds/dependencyHolds 含义、一个 wrapper 一个 Device 根、图形快照借用与区域标记 | 补充发布前准备 caps/辅助对象、删除前解除缓存引用, 包退休后再收拢辅助对象 | 未扩展本轮之外的 fence/退出故障验收 |
| `Bindings.hpp/.cpp`, `InputLayout.hpp/.cpp`, `Pipeline.hpp`, `Shader.hpp/.cpp` | 不可变布局、存储/view、Program/计划及 canonical 反射的职责已复核 | 集合保活、storage 范围、存储宽度、divisor/base instance 与 PointSize 说明保留核对 | Pipeline 重映射及创建/反射/失败回滚说明保持准确 | CR-002 的 profile 边界已由实际准备/应用核对 |
| `Pipeline.cpp`, `Validation/ValidationBackend.hpp`, `Validation/ValidationCommandList.cpp` | 本次纳入完整文件复核; Pipeline 准备与公共包装的职责保持单向 | 补充枚举合法与实际可执行的区别、GL 状态唯一 owner、直接/间接/push 接口前提 | 保留上游许可/非 GL 路径, 补齐 typed clear/copy、间接范围和复杂委托流程的用途说明; GL 拒绝先于未交付载荷消费 | CR-001/CR-002 已闭合, 无剩余注释缺口 |
| `CommandListUnsupported.cpp` | 通读后保留完整命令表及统一 Unsupported 阅读入口 | 核对未交付 staging/resolve/UAV/间接等入口不读取专有 payload、不发替代工作 | 简单固定拒绝转发共用文件和 `Unsupported` 的说明, 无需逐函数重复 | 无新增注释缺口 |
| `RHI/OpenGL/CMakeLists.txt` | 补充正式实现按候选、保活、Draw、传输拆分的组织说明 | 保留 PUBLIC RHI 与 PRIVATE loader/GLSL lowering 的可见性合同 | 说明 native 绑定与固定状态同属唯一 ContextState; 核对显式新增源码无开发测试前置 | 无新增注释缺口, B 未重新 configure/build |
| `RHI/RHI.hpp` | 保留固定 NVRHI 许可/原注释, CommandList 能力说明与当前实现一致 | GraphicsState 借用、typed clear、DrawIndexed 单位及拒绝合同已核对, 无公共签名/布局变化 | 不复制 native 缓存实现; GL/Validation 对拒绝候选后保留生效状态的合同一致 | CR-001 已闭合 |

直接消费者另核对了 `Pipeline.cpp`、`Framebuffer.cpp`、`BindingPlan.cpp`、`Lowering.cpp` 的创建/组合规则, Common tracker 的状态与错误入口, Validation 的候选及索引检查, `Engine/Shader/RhiProgram` 的阶段句柄寿命, Platform 的 raw/Validation 根和关闭预检, 以及 Runtime 当前无图形消费者的边界. 保留这些文件准确的既有说明; 功能缺口按上述 CR 交 A, 没有替 A 改动它们.

注释可读性结论: 通过, 受整改影响的完整文件已重新核对并补齐职责、合同与关键原因. 代码正确性结论: 通过本轮 B 复核, CR-001–CR-003 均已关闭, 未发现新增正式问题. 结论限本 change 的代码审查及所引用的 A 回归记录; 最终原生 verify 仍由 A 按原流程执行.

<a id="spec-sync"></a>
### 主规范同步, 2026-11-07

已取得当前 CLI `instructions specs --change establish-opengl-immediate-draw --json` 的有效产物规则快照, 未配置额外 rules. 仅按 status 的两份 existingOutputPaths 逐份合并, 无改名/移除:

- 新建 [opengl-immediate-draw](../../../specs/opengl-immediate-draw/spec.md), Purpose 沿用 delta, 9 条 ADDED requirement、24 个 scenario 全部同步, 无 TBD.
- 修订 [opengl-device-resources](../../../specs/opengl-device-resources/spec.md) 中能力拒绝及分层验收的 2 条 requirement、8 个 scenario. 原 Purpose 和其余 12 条 requirement 逐字节保留; 两个修订块保留原有场景并加入本次明确交付与后续验收边界.
- 同步后再次逐块比较两份 delta 与主规范, 11 个目标块全部一致, 无待应用差异. `openspec validate --specs --strict --json` exit=0, 11/11 主规范通过; 只有 requirement 文本较长的 INFO 提示, 无 ERROR/WARNING. 未为格式提示改写本次合同或其它能力.

CR-001–CR-003、A 修正、B 复核、完整文件注释覆盖、版本、实测/复用及限制均已在本 tasks 留存. 此时 8.1/8.2 完成, 8.3 仍待真实归档成功, 不以自身收尾待办表示实现缺失. T04/T05、呈现和 D03 阶段出口继续待各自工作包验收.

<a id="archive-closeout"></a>
### 归档收尾与最终结论, 2026-11-07

`openspec archive establish-opengl-immediate-draw --skip-specs --yes --json` exit=0, 原生归档路径为 `openspec/changes/archive/2026-11-07-establish-opengl-immediate-draw/`. 本次未跳过归档验证; `--skip-specs` 仅避免重复执行已经合并且逐块复核完成的同步, CLI 的 `specsUpdated=false` 表示归档命令自身没有再写主规范. 归档前 CLI 为 20/21, 唯一待办是 8.3 自身, 成功归档及后续核对完成后才勾选该项.

归档保留全部产物和 `.openspec.yaml`, 活动目录已消失. 新归档的 proposal/design/tasks 相对链接已按目录深度重定位, 12 份受影响文档与主规范的 211 个本地文件/锚点链接检查通过. 既有归档、Architecture 和未涉及的主规范共 75 个文件的 SHA256 与收尾前相同. 开发计划只更新 T03 完成状态及归档引用, T04 为下一工作包且尚未创建 change/实施; T05、呈现和 D03 阶段出口未勾选.

本 tasks 已留存稳定 CR、A 修正、逐项 B 确认、完整文件注释覆盖、最终版本与证据限制, 在观察到原生归档成功后才将当前问题单恢复为 `Docs/CodeReview.md.tpl` 的原样内容. B 验收后的 31 个代码/CMake 输入仍为 `b3517b4afc5565165257504d82511a6fdd2d25c873b37fa209edeb8fff062e46`, 本次收尾只改文档/规范, 未改变代码或构建输入.

| 最终 verify 维度 | 收尾结果 |
|---|---|
| Completeness | 21/21 tasks 完成; 初始列出的 8.1/8.2/8.3 三项完整性待办已按实际执行顺序闭合 |
| Correctness | 11/11 requirement、32/32 scenario 核对完成, 两份 delta 已同步; 原生结果按版本匹配复用, 未将未运行范围算作实测 |
| Coherence | design 的 10 项决定、第一方规范和独立代码/注释验收一致, 历史合同与后续任务边界保留 |

最终结论: 本包要求的核对全部完成, 无未解决的 CRITICAL、WARNING、SUGGESTION 或必需检查缺口. `establish-opengl-immediate-draw` 已归档, 证据使用及未覆盖范围继续遵守前文记录; 不将本结论扩展为完整渲染、跨设备、T04/T05 或阶段出口验收.
