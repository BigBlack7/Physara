# Tasks

执行依据为 [design](design.md) 及其列出的四个既有主规格; `skip_specs: true` 表示没有产品合同增量, 不表示跳过行为验证. 每项实际通过后才勾选, 同时在本文件对应任务追加命令、输入/源码标识、配置、结果、日志位置及限制. 各组随自身工作补齐验证和说明; 最后两组只汇总集成结论.

2026-10-28 规划核对: 官方 OpenSpec 1.13.2 的 `status --change establish-opengl-resource-conformance --json` 已确认 proposal/design/tasks 完成、specs skipped; `instructions apply --change establish-opengl-resource-conformance --json` 为 0/22. `validate --all --strict --json` 为 8/8, 本轮四份文档的 812 项本地链接/锚点及 `git diff --check` 通过. 实际改动仅本 change 和 Developments; 未编译或运行本包 C++/native 检查, 不据此勾选下列任务. 原始规划输出位于忽略的 `build/artifacts/establish-opengl-resource-conformance/planning-*.json`.

## 1. 确认实施入口和证据基线

- [x] 1.1 只读核对 D01 真实跨屏验收与阶段出口; 若仍未满足, 在实施任何本包 native 检查或生产修正前取得本 change 独立限定授权, 并同步 proposal/design/tasks/Developments 的原因、范围、回归和退出条件. 验证: D01 出口证据或本包明确授权可追溯, 旧 T03/T04 例外未被继承, D01/D02/D03 状态未提前关闭; 未获准时本项保持未完成且不启动后续 native 工作.

1.1 证据 (2026-10-29): D01 tasks 4.1 仍未完成, CLI 为 15/16. 维护者在本会话明确批准本包 T05 验证及恢复既有合同的最小修正, 答复“同意限定推进, 修订入口后继续”. 已先同步四类文档; 原因、范围、Host/输入回归及退出条件见 design 决策 1. 尚未运行本包 native 检查, D01/D02/D03 出口保持原状态.
- [x] 1.2 固定当前 commit/工作树、正式源与 T03/T04 fixture 输入标识, 把 design 的既有要求映射到具体检查组与可复现入口, 列明历史结果和本包待重跑项. 验证: 每个 D02-T05 目标都有确定用例/预期/归属, 没有“后续再调查整体代码”的空任务, 主规格和历史归档保持原样且 `skip_specs` 与实际范围一致.

1.2 证据 (2026-10-29): `python build/artifacts/establish-opengl-resource-conformance/Run.py prepare` 与 `EvidenceMap.py` 返回 0. 基线仍为 `1e5b60a7092062614383c4dc0e981bfc1cce1978`, 初始工作树只有 Developments 和本 change 规划. `implementation-inputs.json` 固定正式源、复用 T04 fixture 和受保护文档的 SHA-256; `requirement-cases.json` 将 design 引用的 32 项要求及四个主规格的 47 项要求/169 个场景列入具体 native/generic/集成分组, 状态仍为待本包运行. CommandList/Device 的完整签名矩阵为 61/68. 历史 T03/T04 结果仅作为输入, 本包日志另记 `runs.jsonl`/`suites.jsonl`; 未修改主规格或历史归档, `skip_specs` 仍成立.

## 2. 建立共享正式装配的资源验证入口

- [x] 2.1 复用正式 DeviceHost、Validation 和既有私有检查边界建立 Session, 明确 Diagnostics/callback/context/raw 借用和临时强句柄的退出顺序. 验证: Debug/Release 的隐藏宿主可创建、查询并释放实际资源, 普通请求同时覆盖 validated/raw, 重复关闭/重建成功, 生产无新增测试 CLI、公开 native export 或第二 Host.

2.1 证据 (2026-10-29): Debug/Release 的 `Run.py final-native <配置>` 均返回 0 (含预期合同失败码 86/87). ResourceChecks 的 conformance-fixture 使用正式 Diagnostics/Host/raw/Validation, 两层真实创建资源并连续三次关闭/重建; callback 先于 Host 创建、后于 Host 销毁, raw 为借用. 命令与本轮 source/fixture SHA 见 `runs.jsonl`, 对应 `ResourceChecks-<配置>-all.log`; 没有新增产品入口或公开原生导出. 原始输出位于忽略的 `build/artifacts/establish-opengl-resource-conformance/`.
- [x] 2.2 完成 fixture 所需状态 guard、独立 format/type/expected 数据及辅助 fence 的拥有/失败清理. 验证: 类型/容量不匹配在 GL 调用前拒绝, 改动合法范围内的预期/哨兵可触发失配而不故意越界; 状态污染后恢复再由 Open/ClearState/已知产品上传检查通过; 辅助 Complete/Pending/Failed、超时冷退出和已知不可用零 GL 均不提前释放句柄, 在本任务记录控制点与证据性质.

2.2 证据 (2026-10-29): 两配置 conformance-fixture 均通过. 错误 typed clear/短 readback 容量在置空相关 GL 入口时拒绝, overflow/expected/guard 反例使用合法内存. PACK/UNPACK、索引 0 scissor/color、depth/stencil、sRGB/dither 和 clip 状态保存/恢复, 索引 1 不变; 新产品 Open/ClearState/上传读回及 clip 原值回查通过. 辅助真实 fence 的 Complete、受控 Pending 后完成、20ms 超时、WAIT_FAILED、无 fence 冷退出按预期保留 owner, 仅失败冷路径各一次 finish. 显式受控不可用模式在 55 个入口置空时 CPU 撤销并正式关闭, 不是硬件 loss 证明. UPPER_LEFT 原始失配及 fixture 的局部选择见 design 决策 12, 不算产品坐标验收. 原始输出位于忽略的 `build/artifacts/establish-opengl-resource-conformance/`.
- [x] 2.3 在忽略的本包 build/artifacts 目录组织 native/generic/product/集成配置, 保留独立的 ResourceChecks/UploadChecks 及分组入口, 同步实际可运行的 Build 说明. 验证: 当前源能分别 configure/build Debug/Release, fixture 专用入口缺失报告未通过或未覆盖, 根 CMake/产品依赖不变, 无测试框架/CI/格式门禁或 NVRHI target, 私有 accessor 无跨程序误合并, 新入口按说明可复现.

2.3 证据 (2026-10-29): `Run.py configure` 和 `BuildAll.py` 完成 native/generic/product/regression-complete 四棵独立目录的 Debug/Release 构建; 成功日志无第一方警告. `Inspect.py build` 确认 8 个配置模型、133 个实际编译输入、依赖和生产符号. Build 已列实际分组命令及忽略输入的可用条件; ResourceChecks/UploadChecks 分别链接, 缺 indexed 入口在 GL 前明确失败. 根 CMake/正式依赖没有修改, 没有测试框架/CI/NVRHI target. 构建中 C1001/CL 进程异常和相同选项的有界重试保留原日志, 不解释为已解决根因. 原始输出位于忽略的 `build/artifacts/establish-opengl-resource-conformance/`.

## 3. 核对能力、存储和拒绝 profile

- [x] 3.1 建立 17 格式 × 9 target 的查询/用途/样本矩阵, 区分公共 2D flags、具体 target caps 与实际创建, 用直接查询及操作结果交叉证明. 验证: raw/Validation 的公开查询一致; 允许的 2/4/8 逐个正验, 集合外和 MS 非法 0/1/3/quality/mip 均零原生分配拒绝; baseline 必须满足, 所有组合有明确结果而非静默跳过, 当组补记本机环境与支持边界.

3.1 证据 (2026-10-29): 两配置 conformance-matrix 均覆盖 153 行查询, raw/Validation 合计实际创建 1302 个资源、636 个零分配拒绝. 2D flags/实际 target/用途分开; 本机两种 MS 的样本查询为 16/8/4/2, 只接受 profile 内 2/4/8, 0/1/3/9/16/32/UINT32_MAX、quality/mip 拒绝; baseline 必须满足. controlled missing-4 分支两层拒绝, 不声称本机自然缺样本. 原始 3D depth 查询与规范排除同时记录; CPU 未替代原生查询/存储证据. 原始输出位于忽略的 `build/artifacts/establish-opengl-resource-conformance/`.
- [x] 3.2 对矩阵允许的 storage-only、SRV storage 和 RT/DS storage 创建小型非对称资源, 核对实际 immutable format、全部 mip/layer/face/volume、MS samples/fixed locations 和 Framebuffer/view 寻址. 验证: raw/Validation 合法结果一致, 错误角色/范围/typed reinterpret/不兼容 view/3D 或 layered FBO 拒绝, FBO read/draw 绑定不变, sampler/texture 状态与依赖仍独立; 结果只说明存储/附件.

3.2 证据 (2026-10-29): 两配置 matrix 对 storage-only/SRV/RT-DS 的合法请求逐 mip 查询 immutable/sized format/shape/sample/fixed-locations, 检查 2176 个实际单 mip/layer/face 附件; 无效角色/view/range/3D/layered FBO 与 sampler 独立性沿 ResourceChecks texture/sampler/framebuffer/boundaries 重跑. DSA read/draw FBO 绑定保持. 3D 深度/模板在分配前拒绝, 不读未初始化内容, 结果只说明存储与附件. 原始输出位于忽略的 `build/artifacts/establish-opengl-resource-conformance/`.
- [x] 3.3 复核 27 个 Feature、Graphics 单队列/单 open immediate 及完整 61 项列表/68 项 Device 的已交付/拒绝闭包, 同步当组 profile 说明. 验证: 不支持 feature 的输出 sentinel 不变, replay/异步队列/RT/mesh/sparse 等在无关 payload 前拒绝, clear/copy/resolve/Shader/Draw/公共 staging/query 仍明确失败, 无假对象/票据/空成功或接口裁剪.

3.3 证据 (2026-10-29): 两配置 factory/matrix/unsupported 和 UploadChecks list/edge 均通过; raw/Validation 的 27 个 Feature 全 false 且 16-byte sentinel 不变. Graphics/immediate、单 open、一次录制/提交和 reopen 行为通过, replay/异步队列/扩展及尚未交付的 clear/copy/resolve/Shader/Draw/staging/query 明确拒绝. 公开 61/68 项签名矩阵及原生闭包保留, 未裁剪接口或制造成功对象/票据. 原始输出位于忽略的 `build/artifacts/establish-opengl-resource-conformance/`.

## 4. 补强正式上传的独立数据证明

- [x] 4.1 复跑 Buffer 背景加非零 offset 覆盖、256 KiB 阈值两侧与大写入、零长度/边界/溢出及源返回后销毁, 用独立输入和有 guard 的实际读回比较. 验证: Debug/Release 的两层合法路径目标区间精确匹配、其余已初始化字节不变, 被拒绝写入零副作用, 正式提交/实际完成先于读取, 当组记录配置和结果.

4.1 证据 (2026-10-29): 两配置 UploadChecks buffer 都在 raw 与正式 Validation 跑完整背景、非零 offset、256KiB 前后及大写入, 包含源立即覆盖/释放、前后 guard、零长度/越界/overflow/状态负例, 正式提交和实际完成先于独立 native 读取; 目标与背景精确匹配. 当次日志为 `UploadChecks-<配置>-all.log`, 未用 fixture fence 替代产品完成. 原始输出位于忽略的 `build/artifacts/establish-opengl-resource-conformance/`.
- [x] 4.2 复跑 13 颜色格式 × 7 非 MS 维度及所有有效 mip/layer/face/volume, 加入已初始化未选中子资源、PACK guard、非 texel 倍数 padding 和独立 format/type oracle. 验证: integer/encoded bytes 精确, half/float 有限可表示值一致, row/slice 与 swizzle/sampler 独立; 源立即覆盖释放仍通过, 短/溢出 footprint 拒绝, 不读取未初始化数据, 当组说明实际支持与覆盖数量.

4.2 证据 (2026-10-29): 两配置各通过原 702 次纹理子资源读回, 加上独立 oracle 的 raw/Validation 13x7 全 mip/layer/face/volume 初始化 1404 次与单子资源覆盖后保留 1404 次有 guard 读回. 各尺寸/marker 由 CPU 输入决定, 不用生产 GetUploadFormat 决定预期; 非 texel 倍数 padding、单行/单 depth pitch、源立即覆盖释放、有限 half/float、integer/encoded bytes 和 swizzle/sampler 独立性通过. 原 footprint 拒绝用例也已重跑. 原始输出位于忽略的 `build/artifacts/establish-opengl-resource-conformance/`.

## 5. 验证深度/模板与多重采样存储内容

- [x] 5.1 为 D16/D24S8/D32/D32S8 的实际允许非 MS 附件创建 mip/layer/face, 经 fixture 具名 FBO clear 并在辅助真实完成后分 aspect 读回. 验证: Debug/Release 的 depth marker 按 design 量化误差、stencil 精确比较, native 附件身份/格式/范围一致; 不支持组合明确拒绝, 未启用公开 WriteTexture/clear 或 stencil SRV.

5.1 证据 (2026-10-29): 两配置 D16/D24S8/D32/D32S8 六种合法非 MS 附件维度的所有 mip/layer/face 初始化、附件身份/格式/范围及分 aspect 读回通过. 四种 3D 组合每层 API 都明确拒绝 (共 8 个预期拒绝); depth 按 D16/D24 一个 UNORM 步长加两个 float epsilon 或 D32 精确比较, stencil 按 byte 精确比较. WriteTexture/产品 clear/stencil SRV 未启用. 原始输出位于忽略的 `build/artifacts/establish-opengl-resource-conformance/`.
- [x] 5.2 补齐 depth-only、stencil-only、区域及子资源保留检查, 并对 fixture 状态/辅助资源做退出审查. 验证: 联合初始化后一次只改选中 aspect/区域/mip/layer, 其余已初始化内容不变, guard/GL 错误检查有效; 资源在辅助完成前保留、之后原生删除, 再做独立产品上传通过, 不宣称已验证 raster/Reverse-Z.

5.2 证据 (2026-10-29): 两配置各 1040 组 depth-image 内容比较包含联合初始化、depth-only/stencil-only 的局部覆盖及其余已初始化区域/mip/layer 保留, 所有读回 guard/错误检查通过. 辅助资源保留到自身实际完成, 可用退出按 FBO 依赖顺序回收, 恢复状态后独立产品上传及 clip 原值检查通过. 原生辅助使用显式 LOWER_LEFT, 不宣称 raster/Reverse-Z 或本机 UPPER_LEFT 组合已验证. 原始输出位于忽略的 `build/artifacts/establish-opengl-resource-conformance/`.
- [x] 5.3 对实际 renderable 的全部映射颜色格式, 在两种 MS target 与每个支持的 2/4/8 样本上逐层布置同值样本 marker, 用同格式同尺寸的单样本目标作 fixture resolve. 验证: 逐层/区域内容按类型匹配且实际样本数正确, 没有 MS 直接 texture readback、缩放或 Shader 旁路; 产品 ResolveTexture 仍拒绝, 当组记录实际支持集合与拒绝结果.

5.3 证据 (2026-10-29): 两配置各对 13 种 renderable 颜色格式、MS/MSArray 和实际 2/4/8 完成 312 层同值样本清除及同格式 5x3/GL_NEAREST resolve 后的 guarded 内容比较. 原生 samples/fixed locations 与每层附件匹配, UNORM/sRGB 用 0/1, float 用有限精确值, integer exact. MS 没有直接 texture readback、缩放、Shader 旁路, 产品 ResolveTexture 仍拒绝. 原始输出位于忽略的 `build/artifacts/establish-opengl-resource-conformance/`.
- [x] 5.4 将同值样本及逐层 resolve 覆盖到四种 depth/stencil 格式的实际允许 MS 组合, 保持两 aspect 和不同 layer 独立. 验证: depth 满足既定误差、stencil 精确匹配, 源/目标格式尺寸及 mask/filter 合法, 不假定不同 sample 的实现相关选择或平均; 辅助等待/清理和未覆盖项可追溯.

5.4 证据 (2026-10-29): 两配置各对四种深度/模板格式、两种 MS target、2/4/8 完成 96 层 resolve, 与颜色合计 408 层. 联合清除及独立 depth/stencil 区域 marker 保持另一 aspect 和其它 layer, depth 误差/stencil 精确比较通过. 本机该 profile 的 MS 组合无额外缺失; 不证明不同 sample 的选择/平均或实际 draw. 辅助完成/退出证据与产品 packet 分开. 原始输出位于忽略的 `build/artifacts/establish-opengl-resource-conformance/`.

## 6. 验证失败、归属和完整释放

- [x] 6.1 在 raw 与同一正式 Validation 下覆盖 initialized 异常描述、limits+1/算术边界、错误活类型和独立 context/Device 的 foreign 资源/列表. 验证: 被拒绝 allocation/write/fence 的计数为零且 output/既有内容不变, 两设备后续合法工作均成功, 全局 GLAD 表不重载、入口地址兼容且归属不依赖名字/地址 registry; 当组记录实际及受控条件.

6.1 证据 (2026-10-29): 两配置 raw/Validation 的 initialized 非法 Buffer/Texture 描述、limits/算术边界和受控样本缺口在 native allocation 前拒绝. ForeignCases 使用活的独立 GLFW context/Device, 验证函数地址兼容且不重载 GLAD; 合法 footprint 的 foreign Buffer/Texture、错误活 Texture 实现、foreign 列表/FBO 依赖在相关写入/fence 入口置空时拒绝, 每操作保留诊断. 两方后续正式上传、实际完成和独立内容比对成功. 没有地址 registry/悬空对象或两层日志逐字比较. 原始输出位于忽略的 `build/artifacts/establish-opengl-resource-conformance/`.
- [x] 6.2 复用受控 CPU bad_alloc 和原生 allocation/map/fence/flush/wait/unmap 失败, 先收拢辅助工作, 保留每个失败位置与回滚/前缀证据. 验证: 不发布半成品、不丢失首错、无重复诊断、无可能在途区段过早复用/失效后 GL, 注入 guard 正常恢复; 若需生产修正, 先在 design 说明原因/最小方案/来源差异并保留失败复现, 修复后扩展当组回归.

6.2 证据 (2026-10-29): 两配置 ResourceChecks failures/boundaries 和 UploadChecks failure/native-edge/edge 通过当前受控 CPU/native allocation/map/fence/flush/wait/unmap 失败矩阵, 检查首错、回滚/已发出前缀保活、单次诊断和 guard 恢复. 自然资源耗尽/硬件 loss 未触发. 本轮发现的非法 3D 深度/模板 caps 已先存 exit-22 复现与输入 SHA、再写 design 决策 11 后以 Formats.cpp 5 行恢复既有合同, 后续完整矩阵及两配置回归通过; 参考快照/主规格不变. 原始输出位于忽略的 `build/artifacts/establish-opengl-resource-conformance/`.
- [x] 6.3 复跑 T03/T04 的 worker 最后释放、FBO/view/texture 删除顺序、共享资源多 packet/名字复用、预算恢复和 Host drain 矩阵. 验证: 实际完成驱动一次退休, worker 无 GL/新分配, genuine external owner 与内部 hold 区分, ResourceInUse/Pending 保持正确环境且重试成功, usable/unavailable 退出各自合法, 总 deadline/loss 后零 GL/unmap 单诊断回归通过; 自然 GPU 和受控 Pending/Failed 分开说明.

6.3 证据 (2026-10-29): 两配置 T03/T04 worker 最后释放零 GL/无新分配、FBO/view/storage FIFO、共享资源多 packet、实际名称复用、64MiB/64 packet/4096 写入/资源预算恢复及 external/internal holds 通过. Host ResourceInUse 保持 Active、Pending 保留并重试、无 fence/WAIT_FAILED 冷退出和显式不可用零 GL 各通过. 归档审查回归显示 loss 后 GL=0, 两实际提交共享总等待 Debug 约 5.00004s/Release 约 5.00001s, unmap 诊断=1; 这些是协议/受控条件证据, 不作性能结论. 原始输出位于忽略的 `build/artifacts/establish-opengl-resource-conformance/`.

## 7. 构建与集成回归

- [x] 7.1 从当前输入分别验证 generic 独立消费者、正式 native 消费者和无检查宏的生产构建, 核对头/布局/依赖及链接符号. 验证: Windows x64/MSVC Debug/Release 通过必要 configure/build/消费者检查, 无新增第一方警告、native 反向依赖、检查符号泄漏或额外产品入口, 检查命令和当前源标识可复现.

7.1 证据 (2026-10-29): `BuildAll.py` 完成 native/generic/product/regression-complete 各 Debug/Release, 当前源的 native 公开/私有头消费者通过. `Inspect.py build` 返回 0, 核对 133 个实际编译输入、8 个配置模型、生产 8 份库符号, generic 不配置/链接 GL/Core/Platform, product 无检查宏/检查符号, 根构建没有第二产品入口/NVRHI target 或反向依赖. `build-model.json`/`compile-inputs.json`/`symbol-verification.json` 保留命令与输入 SHA. 本机 Windows 11 26200 x64、MSVC 19.44.35228、SDK 10.0.26100.0、CMake 3.30.0-rc4、OpenSpec 1.13.2; 原始环境见 environment.json. 中途 MSVC C1001/CL 异常保留且仅按相同选项有界重试, 成功构建无新增警告, 未宣称已解决工具链根因. 原始输出位于忽略的 `build/artifacts/establish-opengl-resource-conformance/`.
- [x] 7.2 运行本包全部分组及受影响的 T02/T03/T04、Core/Host/输入/Runtime 集成回归; 生产修正按影响面扩展, 每项未重跑或未覆盖明确归属. 验证: 当前配置必要检查全部通过, 旧日志未冒充本次结果, 错误不能通过放宽断言/降低警告/关闭诊断消除, 不把单屏/单驱动结果扩展到其他环境.

7.2 证据 (2026-10-29): `RegressionAll.py` 两配置均返回 0, `suites.jsonl` 最终 142 个唯一检查均 valid: 每配置 generic 12 正例/10 编译负例、产品 5 个入口/隐藏运行、Core/Host/输入 7 个消费者、Runtime 37 个正常/错误/退出场景. 编译负例中的 C1001 被判无效并重试, 最终拒绝日志均有实际 C++ 接口错误且不含 C1001. 本轮 native 两配置完整资源/上传/旧归档回归和头/预期失败检查均通过; 先针对附件预检重跑 ResourceChecks, 后因 Session/状态/辅助失效传播同时影响两个检查程序, 已重建并重跑两配置完整 native; 通用/产品/集成输入未变, 保留其本轮同源码证据. Audit 按实际 include 依赖及正式 C++ SHA 验证有效输入, 没有以历史二进制或日志冒充本轮结果. 验证实际 Intel GL 4.6.0/32.0.101.6129, 其它枚举显卡/平台未原生验证, D01 真实跨屏仍未完成. 原始输出位于忽略的 `build/artifacts/establish-opengl-resource-conformance/`.
- [x] 7.3 汇总各组已随任务记录的证据, 对齐 Build 的实际入口、RHI README 的存储/上传/fixture/执行边界和 Developments 对应工作包. 验证: 环境/源码/fixture/命令/结果/限制可追溯, D03/D08 能力仍归原包, Architecture 和历史归档未加入实施叙事, T05 与 D01/D02 出口分开判断, 无新增流程外必交文档.

7.3 证据 (2026-10-29): Build 已列当前本地输入、独立 binary 和各分组的可复现命令; RHI README 说明实际资源/上传、辅助 fence/读回与产品能力的边界及自有 GL caps 修正来源. Developments 的 T05 记录本包独立授权、实现/验证进展, D03-T03 追踪本机 UPPER_LEFT 原生区域观测, D01/D02 出口仍未通过; 最终 8.2 之后才勾选 T05 目标. Architecture/主规格/历史归档 45 个保护文件逐字节不变. 各组真实命令、source/fixture SHA、配置、日志、受控条件和限制已随任务记录, 未新增流程外验证文档/测试组织或性能结论. 原始输出位于忽略的 `build/artifacts/establish-opengl-resource-conformance/`.

## 8. 一致性复核与交付

- [x] 8.1 对 fixture、任何局部修正和证据完成 Code Review 及原生 verify, 逐项核对 design 的要求映射、状态权威、辅助/产品两条寿命、失败/退出和独立 oracle. 验证: 复核问题均关闭且有对应证据, 所有适用场景有实际结论, `skip_specs` 仍成立或已按原生流程修订 delta, 不以工具结构校验代替原生结果.

8.1 证据 (2026-10-29): 已按 openspec-verify-change 的 Completeness/Correctness/Coherence 核对当前产物、实际源码与逐组证据. official specs=skipped, delta 的覆盖/实现/场景检查为 Not applicable; 另按既有 47 项要求/169 个场景 (其中 design 直接引用 32 项) 复核 native/generic/集成对应组及适用条件, 没有用结构校验代替 GPU 证明. 12 项设计决策均有实现或明确范围依据, 生产修正只在 Formats.cpp 5 行过滤非法 3D 深度/模板 caps. Code Review 检查资源/状态权威、独立预期、两个完成链、先保活后清理、typed/容量/范围、context/worker/关闭及命名边界. 审查修正了 fixture 的 indexed 状态影响范围、合法 foreign footprint/诊断归属及颜色 clear 的实际附件类型防线; 最后一项以实际 depth FBO 加置空 color clear 验证无错误调用, Debug/Release 受影响资源全组重跑通过. 未关闭问题在本 change 范围内为 0; D01、其它平台/设备、真实硬件 loss 与 D03 UPPER_LEFT 行为按其范围继续未验收. 不自动同步主规格、提交或归档. 原始输出位于忽略的 `build/artifacts/establish-opengl-resource-conformance/`.
- [x] 8.2 运行 OpenSpec strict、任务状态、文档链接和最终改动范围检查, 在满足入口及本包全部验收后更新 D02-T05 完成事实并给出交付结论. 验证: 工具状态与实际结果一致, 未完成项没有勾选, D01 跨屏与 D01/D02 总出口仍按真实条件判断; 主规格无本包不需要的同步, 本任务不自行提交/推送或归档.

后续审查收口 (2026-10-29): 2.2/8.1 曾重开, 先前证据保留其时点, 现已由下述复核再次关闭. 辅助 Poll/冷 finish 及状态/读回发现 context 不可用时须传播到 Session, 阻止后续 GL, 等所有外部 CPU owner 结束后按既有 ContextUnavailable 收拢并正式关闭. 补充受控 loss、冷收拢失败及延后 owner 释放的零 GL 回归, Debug/Release 实际通过后关闭本次审查项. 生产源码范围不变.


2.2/8.1 最终补充证据 (2026-10-29): 两配置重新 `Run.py build <配置> native` 和 `Run.py final-native <配置>` 均通过. 辅助组现在包含 7 种模式及一项 deferred-owner 退出: 真实完成、受控 Pending/超时/WAIT_FAILED/无 fence、显式不可用、Poll 观察到受控 GL_CONTEXT_LOST、冷 finish 受控失败, 以及辅助对象先于外部别名结束. 对后三类在 55 个原生入口置空时停止辅助/状态调用, 由 Session 在全部 CPU owner 结束后执行纯 CPU 根预检/ContextUnavailable drain/正式 Host 关闭. 一般 OOM 不被推断为硬件 loss, 冷收拢无法证明完成时才选择明确的不可用退出, 不发布产品完成 ID. 未预期的辅助/状态异常经检查 main 的本地异常边界返回失败, 不掩盖为成功; 该边界不改变产品 CLI. 新错误检查、readback、状态 guard 与辅助对象共享 Session 的同一不可用事实, 无全局状态/registry. 受影响所有分组重跑后仍为 matrix 153/1302/636/2176、depth 1040、MS 408, 上传独立初始化/保留各 1404; `Inspect.py build` 重新固定 133 个编译输入, 无检查宏生产的符号/依赖不变. Code Review/verify 范围内 CRITICAL=0/WARNING=0/SUGGESTION=0, 没有以未验证项换取通过.

8.2 证据 (2026-10-29): 当前官方 OpenSpec 1.13.2 strict 为 8/8, 六份当前文档的 834 项本地链接/锚点和 `git diff --check` 通过, 45 个 Architecture/主规格/历史归档文件 SHA-256 与实施前相同. 8 个构建配置、142 个通用/产品/集成检查及两配置完整 native/头/预期失败检查均有当前输入证据. 本包 22/22 实际完成后, Developments 的 T05 四项目标已勾选, 仍保留本 change 限定授权及 D01/D02 出口未通过; D03 的 UPPER_LEFT 原生区域问题继续独立追踪. 最终状态、strict、源/fixture/文档 SHA 与实际范围保存于忽略的 `final-*.json`/`final-verification.json`, 无本包 delta 主规格同步, 未暂存、提交、推送或归档.

| 原生 verify 维度 | 最终结论 |
|---|---|
| 完整性 | 22/22; official specs skipped, delta 覆盖检查 Not applicable; 既有合同映射另行复核 |
| 正确性 | 当前 native/generic/集成证据覆盖本包适用行为, 生产局部修正恢复既有要求; 47 项既有要求/169 个场景的组映射保留在原始证据 |
| 一致性 | 12 项设计决策、具名域/静态 helper、状态权威与两条寿命/失败/退出协议复核通过; 无本包未关闭问题 |

覆盖限制: 仅当前 Intel GL 4.6.0/32.0.101.6129 上的 Windows x64 原生证据, 系统枚举的其它显卡不计作已验证设备. 真实跨屏 D01、产品 UPPER_LEFT 坐标/Shader/Draw/呈现、公共传输/query 仍按原工作包验收. OOM/Pending/错误/不可用注入是协议证据, 不是实际显存耗尽或硬件 loss. MSVC 间歇 C1001/CL 进程异常的原日志与同选项重试已保留, 根因未解决; 未降低编译或检查门槛.


## 归档复核 (2026-10-29)

维护者在完成实施后明确请求验证归档. 本轮 `ArchiveReview.py native` 重新执行 Debug/Release 完整资源/上传、头/能力探针及预期合同失败检查, 22 次程序调用全部取得预期返回码; 新的命令、当前源/fixture 标识与二进制 SHA 记录在 `archive-native-runs.json`. 本机 matrix 153 行、1302 实际创建/636 零分配拒绝/2176 附件检查、depth 1040、MS 408 和上传独立初始化/保留各 1404 再次通过. 实际 GPU 仍为 Intel GL 4.6.0/32.0.101.6129; 未扩大覆盖结论.

归档 Code Review 重新核对生产 5 行 caps 过滤、默认空 caps、2D 公共 flags/实际 target 的交集, 以及 fixture 的独立 oracle、typed/容量预检、状态恢复、两条 fence 寿命、Session 不可用事实及晚释放别名/关闭. CRITICAL=0/WARNING=0/SUGGESTION=0. `Audit.py final` 重新验证 8 个构建配置和 142 项通用/产品/集成检查的实际输入标识, 它们是本轮实施时已通过的同源码证据, 未伪称在归档时重编译或重新执行. 45 个原保护文件与源码身份保持一致; 原始输出见 `archive-code-review.json` 及 `archive-*.json`/日志.

官方产物图为 proposal/design/tasks done、specs skipped, CLI 任务为 22/22. `artifactPaths.specs.existingOutputPaths` 为空, 因此没有 delta 同步步骤或同步确认, 六个主规格保持原样. 按用户本次明确请求归档, 目录日期为 2026-10-29; 链接在移动前按新深度重定位, 归档后再次 strict/链接/文件完整性核验. 本 change 的独立授权、D01 跨屏和 D01/D02 总出口未通过、D03 UPPER_LEFT 待定位及现有环境/工具链限制全部保留. 没有提交或推送 Git.
