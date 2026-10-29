# Design

## Context

动机与范围见 [proposal](proposal.md). 规划基线为 `1e5b60a7092062614383c4dc0e981bfc1cce1978` (`D02-T04实现`), 开始规划时工作区干净. 已核对 AGENTS、Architecture 7.2/7.3/7.7/11.1/11.5/12.1、Developments D01-D03 与 D08-T05、四个相关主规格, 并检查正式格式表/资源创建/Framebuffer/Validation/列表拒绝/提交/Host 关闭和 T03/T04 fixture. 本文件记录实施方法与局部修正, 实际命令、结果及覆盖限制以 [tasks](tasks.md) 的逐项证据为准.

| 已观察实现 | 对本包的约束 |
|---|---|
| `Formats.cpp` 缓存 17 种格式 × 9 个 target; 公共 `QueryFormatSupport` 返回 2D 基线, 创建读取具体 target 的 caps | 不能把公共 flags 当成每个 target/sample/操作的支持承诺, 不增加新公共 caps API |
| 13 种颜色格式 × 7 个非 MS 维度具有 `WriteTexture`; 四种深度/模板和 MS 仅有分配支持 | 原生存储内容证明不能扩展 WriteTexture, clear/resolve 仍属于 fixture |
| 正式 Host 装配 raw Device 和 Validation, 借用 `GetDevice`; 一个进程最多一个 Host | 常规检查经正式包装; 外设备检查沿既有私有入口创建独立 context/Device, 不创建第二个 Host |
| 私有 `ResourceChecks`/`UploadChecks` friend 与 `DeviceHostChecks::BorrowRawDevice` 已存在 | 复用边界, 不加公共 GLuint 导出或测试注册表 |
| ContextState 当前只权威管理 unpack; 列表与 Device 已有 packet/fence/在途引用/可重试 drain | fixture 自己拥有 PACK/clear/scissor 状态和辅助 fence; 不让生产 tracker 猜测原生辅助写入 |
| T03/T04 的原始 fixture/日志位于忽略的 build 树, 本地仍可读取 | 可以复用源与入口, 必须记录本包实际源标识; 不能把旧二进制、旧日志当本包结果 |

## Goals / Non-Goals

**Goals:** 用一个有限、可复现的资源验证闭环完成 D02-T05: 实际查询 → raw/Validation 请求 → 原生对象/已初始化内容 → 合法完成 → 释放/失败退出. 明确哪些证据检验存储, 哪些检验上传和寿命, 记录实际设备支持和未覆盖范围.

**Non-Goals:** 不改变既有公开 API、资源描述、格式映射、profile、预算或错误合同; 不实现 proposal 排除的执行/读回能力. T05 的“完整”只指当前 D02 资源 profile 的四项目标, 不指 Architecture 中未来全部 GL 基础能力. Shader 采样、绕序、Reverse-Z 比较、sRGB 输出编码及公共传输链分别随 D03/D08 验收.

## Decisions

### 1. 以 D02-T05 为唯一工作包, 保留独立实施入口

D02-T03/T04 已提交. 2026-10-29 只读核对确认 D01 tasks 4.1 的真实跨屏验收仍未完成 (15/16). 维护者随后明确答复“同意限定推进, 修订入口后继续”, 批准本 change 独立限定例外: 先执行既定 T05 资源一致性验证及恢复既有合同的最小修正. 在本包 native 检查或生产修正前同步 proposal/design/tasks/Developments, 不继承旧 T03/T04 例外.

该例外的原因限定为本包资源 profile 不消费跨屏比例; 影响到的宿主/输入仍回归. 它只准许本包验收与符合既有合同的最小修正, 不关闭 D01 验收, 不放行 Shader/Draw、公共传输/呈现或 D01/D02 总出口. T05 全部通过可以完成本工作包, D02 出口仍须另外满足 D01 及阶段全部条件.

**取舍:** 不把 T05 推迟到 Shader/Draw 之后, 因为 D03 以 D02 出口为前置会形成循环. 本包验证资源存储, D03 验证产品执行, D08-T05 验证公共传输/查询. 这是开发计划的实施分工澄清, 不改变架构技术方案.

### 2. 使用官方 skip_specs, 追踪到既有要求而非新增重复要求

本包只补充现有合同的测试和说明, `.openspec.yaml` 设置 `skip_specs: true`; specs 原生产物应显示 `skipped` 且不创建对应文件. proposal/design/tasks 仍按依赖完成, 主规格及历史归档保持原样. 不为使验证工具通过而发明新的资源能力.

| 现有权威要求 | 本包拟补充或重跑的证据 | 任务 |
|---|---|---|
| device-resources: `Create a device for the owning current context`, `Consume native and generic capabilities independently` | 同一正式 Host/backend、环境与入口要求、generic/生产依赖 | 2.1, 2.3, 7.1 |
| device-resources: `Expose actual resource capabilities conservatively`, `Create texture storage with explicit shape and usage` | 全部映射格式/target 与实际样本集合, 原生存储及深度/MS 内容 | 3.1, 3.2, 5.1-5.4 |
| device-resources: `Keep view interpretation compatible with storage`, `Compose valid framebuffer attachments`, `Keep texture and sampler state independent` | 附件实际 mip/layer/view 与角色、依赖/采样参数独立 | 3.2, 5.2, 6.3 |
| device-resources: `Create immutable buffer allocations`; uploads: `Write checked ordinary buffer intervals`, `Write the requested texture subresource`, `Interpret pitches without reading padding as texels`, `Preserve uploaded encoding and row order` | 非零区间与边界, 13 色/7 维度、mip/layer/face/volume、padding/编码与源提前释放 | 4.1, 4.2 |
| device-resources: `Reject foreign resources before native work`, `Preserve resource creation failures transactionally`; common-validation: `Reject unsafe inputs before device work`, `Compose validation with explicit CPU ownership` | 两层合法语义、拒绝前零副作用、外设备/错误实现、可控分配/原生失败 | 6.1, 6.2 |
| device-resources: `Reject unavailable operations without fictitious GPU work`; common-validation: `Distinguish API policy from underlying capabilities`, `Reject operations outside the initial OpenGL policy` | 27 个 feature 输出不变, 单 Graphics/immediate 与排除族, 完整接口闭包 | 3.3 |
| device-resources: `Preserve resource dependency ownership`, `Delete native objects on the owning context thread`; uploads: `Consume borrowed CPU data before write returns`, `Bound pending upload storage and work`, `Reclaim only proven completed work`, `Retire abandoned immediate work safely`, `Distinguish internal retention from external ownership`, `Drain issued work before native-environment release`; host: `Release native ownership completely and repeatably` | 真实完成/worker/保活/名字复用/退出, 受控 Pending 与失效分开 | 4.1, 4.2, 6.3 |
| uploads: `Keep transfer-state publication separate from recording`, `Submit closed upload batches without replay`, `Preserve upload failures transactionally`; common-validation: 列表/状态要求 | 复用现有 T04/T02 回归, 校验本包 fixture 没有绕过完成或污染 tracker | 6.2, 6.3, 7.2 |
| device-resources: `Report allocation separately from completed rendering`; host: 环境/失败要求 | 当前环境、实际执行命令、证据类别、未覆盖/未获准入口, 不提前关闭阶段 | 1.1, 7.3, 8.1, 8.2 |

表中简称分别链接 [device-resources](../../../specs/opengl-device-resources/spec.md)、[uploads](../../../specs/opengl-resource-uploads/spec.md)、[common-validation](../../../specs/rhi-common-validation/spec.md)、[host](../../../specs/native-window-host/spec.md), 并非新 spec 条款. 任务 1.2 再将适用场景关联到实际用例名/日志, 没有重新运行的部分保留原证据归属.

若测试发现违反已采纳行为的缺陷, 先保留失败复现、在本 design 追加原因/局部方案/来源差异与验证, 再作最小修正; `skip_specs` 允许这种恢复既有行为的修复. 若需要增加能力、改变 observable 结果或偏离硬合同, 先解决冲突、修订 proposal/design/tasks, 移除 `skip_specs` 并按官方 instructions 创建对应 delta; 本包不预授权这种范围变化.

**备选:** 新建 conformance 主规格会复制既有要求, 把测试组织冒充产品能力; 只做结构校验又不能补齐 GPU 证据. 选择既有行为加明确验证映射.

### 3. fixture 共享正式装配, 按测试所需能力分组

本包验证入口及源码存于忽略的 `build/artifacts/establish-opengl-resource-conformance/checks/`, 独立构建目录分别承载 native、generic、无检查宏的 product 与集成回归. 沿用 T03/T04 的本地 CMake wrapper, 显式链接正式 targets, 所需 native 检查宏仅设置在此验证配置. 资源/深度/MS 增量进入本包的 ResourceChecks, 上传/完成回归沿用 UploadChecks, 各为独立检查程序; 不将它们各自的 main 或不同定义的私有 friend accessor 链接到同一程序. 不修改生产根 CMake, 不建立公共测试框架/安装步骤/第二产品程序.

Session 顺序为 Diagnostics → 正式 DeviceHost → 借用 validated/raw Device 与 context. raw 借用不新增 root; 临时 owning handle 必须显式作用域释放. callback 和私有 GL 观察 guard 覆盖被观察对象的寿命, 先恢复函数指针再释放借用来源. 正常产品工作经 `host->GetDevice`, raw 同等请求用于定位拒绝边界与 backend 防线; 不仅在 raw 跑正例、在 Validation 跑负例.

分组为 caps/storage、profile、buffer/color-upload、depth-stencil、multisample、failure/ownership、completion/shutdown. 每组记录所需入口和结果, 缺少 fixture 专用入口不改变 Device 工厂必需入口或产品支持. 本机必测分组缺入口必须报告未通过/未覆盖, 不能返回成功或从计数中悄悄删除. fixture 在 GL 调用前检查 typed clear、readback 的类型/容量; 类型错误通过预检拒绝验证, 不发送规范规定结果未定义的 clear. 比较器和 guard 的反例只改可合法访问的预期/哨兵字节, 不故意越界.

辅助状态 guard 只覆盖实际改动: PACK/UNPACK 的 buffer、alignment、row/image length、skip、swap/LSB, 第 0 个 scissor 开关/box 和 color 写掩码, depth/stencil 写掩码, framebuffer-sRGB 和 dither; clip 状态的显式保存/恢复见决策 12. clear 只消费 scissor 0, 故使用 indexed 入口, 不修改其它 viewport/draw-buffer 的状态, 并用第 1 个索引的独立值回归. DSA 操作不应改变 read/draw FBO 绑定, 逐组检查这一点. restore 后, 新建并 Open 专用产品列表, 再调用 `ClearState` 使当前 unpack cache 失效, 随后做一次已知上传验证, 不以“后面会自然重绑”掩盖污染. 不增加与产品 ContextState 竞争的长期缓存.

**取舍:** 不提取单独可安装 fixture 库, 当前只有验证消费者. 仅集中真实复用的 session/状态/比较逻辑, 保留 case 职责; 第一方采用具名领域、static 私有函数及项目命名/中文说明规范, 不引入匿名命名空间.

### 4. caps 的期望来自规范、实际查询和操作结果

验证矩阵由 fixture 的最小输入表列出 17 个公共格式与对应 sized internal format, 以及 9 个 target. 该表用于构造输入和核对存储编码, 不复制 backend 的 capabilities 算法或持有另一份可变 DeviceCaps. 对每个组合记录 native internal-format/usage 查询、样本集合、raw/Validation 结果与实际创建/存储观测. 正向判断不能只比较 production cache 和按同一公式重算的 cache.

公共 flags 按当前 2D 基线解释, raw 与 Validation 的查询结果一致; 创建同时满足公开策略和具体 target/format/usage. `ShaderLoad/ShaderSample` 的格式位表明格式用途, 不使 Shader 工厂或 Draw 可用. 正向请求分别验证 storage-only、shader-resource storage 和可用的 render-target/depth-stencil storage; 缺少某个用途时, 对该描述作拒绝断言, 不能将整个格式标为忽略.

普通维度 sample=1; 两种 MS target 逐个检查 profile 允许的 2/4/8 与实际集合的交集. 实际支持而 profile 排除的样本不启用, 候选不支持的样本即使低于 max 也必须拒绝. MS 的非法 0/1/3、超 profile、quality、mip/shape 条件检查零原生分配. 若实际集合刚好覆盖所有候选, 对“集合有洞”分支单独用明确标注的受控值检查防线, 不声称本机自然出现该硬件条件.

尺寸取小而非对称的 5×3, 1D 的 height/depth=1, cube/cube-array 为合法 5×5 方形并至少 6/12 faces, array 至少 3 层, 3D 至少 3 slices, 非 MS 至少两个 mip. 根据每组合实际允许的用途检查全部有效子资源; limits+1、UINT32_MAX、无效枚举与组合在分配前拒绝. 不尝试在真实最大尺寸分配显存, 不以资源耗尽估计 hardware limits.

已排除/未知格式无支持; 27 个 Feature 在当前 native profile 全部 false 且输出 sentinel 不变, Validation 不扩大结果. Graphics 单队列、单 open immediate, 每次录制只能提交一次, 同一列表在接受提交后可重新 Open 新录制; 完整 61 项 CommandList/68 项 Device 方法逐项保留正向或明确拒绝的追踪. 不把资源 flags 推导为 general clear/copy/resolve/query/Draw 成功.

**备选:** 只查询 flags 或只重跑 T03 分配无法证明内容; 盲目全笛卡尔积要求每组合成功又会误判真实不支持 target. 采用有限输入集合, 每一行都有成功、预期拒绝或未覆盖原因.

### 5. Buffer/颜色上传复用正式路径, 加强独立预期与边界

从 T04 当前 fixture 复用已知 Buffer interval 与纹理数据生成, 经正式 Write → Close → Execute → 实际完成 → 原生只读查询. 独立期望由输入 marker/坐标及紧凑 CPU 数据生成; 不调用被测 `GetUploadFormat` 生成唯一比较标准. fixture 的小型 format/type oracle 按测试数据表示列明, 与实际 native storage 查询交叉检查.

Buffer 初始化完整背景, 再覆盖非零 offset 和 256 KiB 阈值两侧/更大写入, 比较目标区间和前后哨兵. Texture 复跑 13 色/7 维度的有效 mip/layer/face/volume, 写不同 marker 并验证未选中的已初始化子资源不变. 保留非 texel 整数倍 padding、无关 depthPitch、单行 rowPitch=0 与失败 footprint; Write 返回后立即覆盖并释放源. 对 integer/encoded bytes 精确比较, half/float 用有限且可表示的值, 不用任意 NaN 或 C++ padding 作预期.

原生读取检查 `bufSize`/GLsizei 与总字节算术, PACK buffer=0, 明确 stride/skip 和前后 guard bytes. 不读取创建后未初始化区域, 不把描述 clearValue、tracker stateInitialized 或 fence 完成当作像素已初始化. 这组使用产品完成链, 不用 fixture 自己的 fence 代替上传完成语义.

**备选:** 只用截图或根据 native output 反算 expected 无法独立证明字节、方向与越界. 选择输入驱动的数值比较.

### 6. 深度/模板只检验既有 storage/attachment, fixture 不发布 RHI 状态

四种存储为 D16/GL_DEPTH_COMPONENT16、D24S8/GL_DEPTH24_STENCIL8、D32/GL_DEPTH_COMPONENT32F、D32S8/GL_DEPTH32F_STENCIL8. 对实际具备 DepthStencil 的非 MS 附件维度, 经 RHI 创建 texture 和单 mip/单 layer Framebuffer; 比较 native attachment identity、level/layer、尺寸与 sized format 后, fixture 执行具名 FBO clear. 3D 附件继续拒绝; 对实际不支持的 format/target 记录并断言创建失败, 不临时造另一种存储替代.

初始化每个拟读取的 mip/layer/face, 再按原生图像坐标布置不同区域/子资源 marker. Depth 使用 0、0.25、0.5、1, stencil 使用 0x00/0x13/0xA7/0xFF. 深度单独读为 GL_DEPTH_COMPONENT/GL_FLOAT, 模板单独读为 GL_STENCIL_INDEX/GL_UNSIGNED_BYTE, 避免对 packed DS 的 C++ padding/未定义位作比较. D16/D24 的绝对误差上限取一个该格式 UNORM 量化步长加两个 float epsilon; D32 的这些可精确表示值按数值比较. 模板精确比较.

联合初始化后分别只清 depth 或 stencil 的选中区域, 验证另一 aspect 及其他已初始化 mip/layer 保持. clear 类型匹配附件, 显式写掩码/scissor, 不依赖调用前状态. 这组证明 storage 和附件寻址; 不证明项目 raster/scissor 坐标、depth test、Reverse-Z 或 stencil Shader 采样.

辅助 GL 操作发生在没有 active RHI recording 且前一产品批次已真实完成时. 不调用产品 `BeginTracking` 或 `Submitted` 为这些原生清除编造状态, 验证结束丢弃专用资源, 不让它们进入后续业务列表. 再用独立资源检查产品 cache 恢复.

### 7. MS 内容先同值清样本, 再合法 resolve 到单样本读取

对实际支持 renderable 的全部映射颜色和深度/模板格式, 在 Texture2DMS 与 Texture2DMSArray 中逐一覆盖支持的 2/4/8. array 选择每个 layer 通过单层 FBO, 不能用 layered FBO 的 layer-zero 行为假装验证所有层. 验证实际样本数/fixed locations 和附件选择, 先初始化所有读取区域, 每区域的所有 sample 写入同一已知值.

resolve 目标由 RHI 创建同格式、同尺寸、sample=1 的纹理/FBO, fixture 执行不缩放的 GL_NEAREST native blit 后读取目标. 颜色输入按类型使用正确 clear variant, UNORM/sRGB 采用 0/1 端点以避免将本包扩展为编码测试; float 使用有限可表示值. integer/stencil 精确比较, depth 沿决策 6 的误差口径. 不读取 MS texture 的 GetTextureSubImage, 不假定非一致样本的平均/深度选择策略, 不证明逐 sample Shader 写入或产品 ResolveTexture.

上述 native 方法依据 [OpenGL 4.6 Core, 2022-05-05](https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf) 的 8.11、17.4.3.1/17.4.3.2、18.3.1: texture subimage 查询排除 MS; clear 形式对应附件类型且作用于各 sample; MS blit 有尺寸、类型和 depth/stencil 格式约束. 对同值样本比较能避开实现相关 resolve 取值. 固定 glad 头/加载器已含相关入口, apply 仍检查实际指针, 不因声明存在就宣称本机可执行.

**备选:** 引入测试 Shader 才能写逐 sample 数据, 会依赖尚未交付的编译/绑定/Draw 或形成另一套 GL Pipeline. 本包采用清除同值样本, 将逐 sample 语义留给真实执行工作包.

### 8. 辅助工作与产品工作各自保活, 所有失败都有退出

fixture 原生 clear/blit 不在 RHI packet 中, Session 必须以外部强句柄保留全部源/目标/FBO 和 guard 至自身真实 fence 完成. 辅助 fence 在最后 native 操作后创建并 flush, 有一个覆盖整次等待的 steady-clock deadline; poll 区分 Complete/Pending/Failed. 只有实际 Complete 才正常删除辅助 fence/资源. 不拿空 batch/假 submission ID 为 fixture 工作背书, 不把该 fence 当成产品 lifetime 证据.

Session 同时拥有辅助环境的不可用事实与最终关闭. fixture 的检查/读回和辅助 Poll 观察到 GL_CONTEXT_LOST 时立即记录该事实; 冷 finish 再失败也不能继续假定辅助工作可安全退休. 辅助对象和状态 guard 读取同一事实停止 GL, 仅释放 CPU 引用; 若外部别名还活着, 等它们结束后由 Session 纯 CPU 根预检、ContextUnavailable drain、正式 Host Shutdown 收拢. 未预期的辅助/状态失败经检查程序异常边界返回失败, 不升级为用例成功; 明确受控不可用用例单独标识. 不新增生产环境 API、全局状态或第二个 caps/资源 registry. 受控 Poll loss、冷收拢失败和晚于辅助对象的外部别名释放均须验证失效后零 GL.

辅助等待超时即该 case 未通过, 不先 Reset 句柄或销毁 Host. 可用且 owning/current 的冷路径允许一次明确 `glFinish` 收拢后释放, 不升级为产品成功. 如果 context 已不可用, 停止 GL, 释放全部外部 CPU 强引用, 对私有借用 raw 验证既有根预检后调用 `DrainDeviceWork(..., ContextUnavailable)` 撤销 Device CPU 状态; 随后正式 Host Shutdown 释放已停止设备和确切 context. 这些是 fixture 的现有互操作边界, 不增加 Host 公共方法或伪造硬件 loss; 不用诊断 Fatal 代替不可用事实. 该失败清理必须可复现验证, 不能让 Session 析构在错误路径偷偷触发未准备的 Host fail-fast.

产品寿命检查仍使用实际 Write/Execute/fence/collector/drain. 自然真实完成与函数指针受控 Pending/错误各自独立分组; 注入前收拢所有 fixture 辅助工作, guard 在所属对象释放前恢复, 不在一个 case 同时混入未追踪的 native 写入和故障注入. 验证 worker 最后释放无 GL/无新分配、FBO/view/texture 删除顺序、共享资源跨多个 packet 保留、真实名字复用和无 idle cache 增长. 强引用和内部 holds 不算一件事, 不能只查 GLuint 是否还存在来替代 wrapper/packet 保活.

Shutdown 覆盖外部 resource/list/root 的 ResourceInUse 保持 Active、内部引用不误拒绝、真实完成后 Ready、受控 Pending 保留环境并可重试、无 fence/WAIT_FAILED 的可用 context 冷退出、已知不可用后零 GL. 本包重跑 T04 归档审查中的总 deadline、排空中 loss 后零 GL、unmap 单诊断回归. 实际 loss/资源耗尽不可稳定触发时明确标注控制点和覆盖性质.

**备选:** 直接清纹理再释放 CPU handle 会绕过产品在途保活而产生错误测试; 用一次同步读回证明所有生产完成路径又会掩盖 Pending. 选择两条拥有者清楚、证据互不替代的路径.

### 9. 无效输入与失败按边界判断结果, 不要求两层日志逐字相同

同一个 initialized 请求分别经 raw 和 Validation, 合法输入应获得相同有效描述/存储/内容; 非法请求可以在不同层被拒绝, 但均不得触碰被拒绝的原生 allocation/write/fence 或修改受保护 output/既有内容. 操作与稳定原因分类可观察, 不比较地址、整条日志文本或来源行号. 已委托失败保留空/false/0, 已消耗的 validated wrapper 状态不恢复重放.

外设备检查沿用私有独立 GLFW context 和 Device, 不取得第二个 Host claim. 在切换前确认所用 GL 入口地址兼容; 不能重载全局 glad 函数表破坏主 Host. 供应方资源保持活着, 接收方 owning/current 时拒绝 foreign buffer/texture/list/FBO 依赖, 两方之后仍能完成自己的合法工作并逆序关闭. 不使用悬空指针模拟错误资源.

可控 CPU bad_alloc、GL allocation/mapping/fence/flush/wait/unmap 失败复用既有 injection 边界. 捕获位置和已获得的真实对象须明确, 检查首错、未发布回滚、可能已发出前缀保活、单次诊断与零后续失效 GL. 不实际循环耗尽显存, 不放宽断言/禁用诊断消除失败. 若该工具链重复发生既有 MSVC 内部编译错误, 保留日志/重试事实, 不静默降级警告或优化.

### 10. 用可追踪任务交付, 不把规划或旧证据计作完成

Debug/Release 分别构建当前正式 native/generic 与无检查宏 product, 检查依赖方向、头自包含、完整接口和生产无检查符号. 重跑相应 T03/T04 及 Core/Host/输入/Runtime 回归; untouched 纯 generic 用例可记录同源码有效结果, 任何生产修正必须扩大到实际受影响消费者. 不为纯文档创建镜像测试或性能报告.

每次实际执行记录 commit/工作树差异及源/fixture 标识、配置、工具链、OS、GL vendor/renderer/version/driver、命令/返回码、分组/比较结果/未覆盖. 入口应能从明确输入重新构建, 不仅引用某台机器的旧 executable. 输出可在忽略 build 树, tasks 记录复现方法和证据位置; 若需长期保留 fixture 组织, 由维护者既有测试组织决定, 本包不预建根测试框架.

普通文档边界沿 AGENTS: Developments 对应工作包记录入口/分工/状态, Build 写实际复现入口, RHI README 写本轮实际 profile. Architecture 没有长期技术变化, 不修改; 过去归档不重写. 所有用例和原生 verify/Code Review 通过后才勾选本包, 前置缺口、失败或未覆盖不能靠降低 D02 出口要求掩盖.

### 11. 实施发现的局部修正: 过滤规范禁止的 3D 深度/模板存储

2026-10-29 Debug `ResourceChecks conformance-matrix` 在原生产源上复现: Intel 32.0.101.6129 对 `GL_TEXTURE_3D` 的 D16/D24S8 查询返回 supported/renderable/ShaderLoad 的正向值, 后端照单缓存并尝试分配; D24S8 5×3×3、2 mip 在 `CreateTexture.Storage` 返回 `0x505`, 本组退出 22. 失败命令、源码/fixture SHA 与完整日志已由 `runs.jsonl` 记录, 另保留 `depth-volume-before-fix.log` 和 `depth-volume-repro.json`. 这是小型非法组合的失败, 不推断为真实显存耗尽.

[OpenGL 4.6 Core §8.5](https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf) 明确列出 DEPTH_COMPONENT/DEPTH_STENCIL/STENCIL_INDEX 的合法 texture targets, 不含 TEXTURE_3D; §8.19 的 immutable storage 不使非法 format/target 合法. 既有要求 `Expose actual resource capabilities conservatively` 要求实际能力与已实现 profile 相交, `Create texture storage with explicit shape and usage` 要求分配前拒绝不兼容组合. 只比较驱动 query 与同公式 cache 会漏掉这一规范约束.

最小方案: 在 `Formats.cpp::ReadFormat` 查询/发布正向 caps 前过滤 `Texture3D && format.hasDepth`, 保留默认空 caps; 原 `CreateTexture` 的 target caps 防线由此在原生分配前拒绝四种深度/模板格式, 不新增另一份创建规则或公共 flags API. 公共 QueryFormatSupport 的 2D 基线、合法 1D/array/cube/MS、颜色 3D 及上游通用输入规则保持其既有合同. GL backend 为项目自有实现, 不修改固定 NVRHI 参考快照或其 hash.

回归: 独立矩阵按规范将这四个 target/format 组合判为不支持, 仍记录原始 driver query 以暴露其差异; storage-only/SRV/DS 三种用途 × raw/Validation 均零 `glCreateTextures`, 随后合法资源/上传仍成功. 原 caps 对照用例补入该规范约束, 不删除该行或跳过断言. 补跑 Debug/Release 全部 native、generic、无检查宏 product 和 Host/输入/Runtime. 本修正恢复主规格已要求的保守交集, `skip_specs` 保持成立, Architecture/主规格/历史归档不修改.

### 12. 辅助原生清除固定并恢复 clip 状态, 保留本机 UPPER_LEFT 观测

2026-10-29 `ResourceChecks depth-stencil` 在 D16 cube mip=1/layer=0 的 2×2 图像复现: scissor=(1,0,1,1), 期望清除 (1,0), 实际读取为 (1,1); 未修改数值误差, 初始化和各 face/mip 寻址已先通过. 失败为 exit 23, 保存 `upper-left-clear-before-guard.log` 及带当前源/fixture 标识的 `upper-left-clear-repro.json`. [ARB_clip_control 问题 12](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_clip_control.txt) 明确 scissor 的窗口坐标不随 clip origin 改变, 该设备/驱动组合的此观测不能作为坐标转换的规范依据.

本包原目标是 storage/attachment, 决策 6 已明确不验收产品 raster/scissor/Reverse-Z. fixture 的局部方案为状态 guard 再保存 CLIP_ORIGIN/CLIP_DEPTH_MODE, 辅助 clear/blit/readback 期间显式使用 LOWER_LEFT/ZERO_TO_ONE 的原生图像坐标, 退出恢复确切原值; 不是从实际输出反算 expected 或对上传内容作翻转. 受控不可用模式仍停止所有状态恢复调用. 矩阵、深度/MS 的同值样本与两 aspect/区域/层保留全部继续执行, 产品创建时的 UPPER_LEFT/ZERO_TO_ONE 合同和上传路径不变.

回归须验证恢复后的原生 clip 查询等于进入值、独立产品 Open/ClearState/上传通过, 并运行全部原定维度和两个配置. 本机 UPPER_LEFT 原生区域行为单独记录为 D03 坐标/执行验收的待定位事项; 不声称它已通过, 不通过修改产品绕序或放宽比较掩盖. 此为有明确消费范围的 fixture 状态选择, 不新增产品能力或缩减 T05 内容/寿命检查, Architecture 与主规格保持原样.

## Risks / Trade-offs

- [已有日志很全面, 容易错当本次结果] → 固定当前源/fixture/config 并逐组记录本包实际运行; 历史记录只作为输入证据.
- [测试自身调用了错误的 typed clear、packed read 或 MS read] → 小型独立 oracle、分 aspect 比较、同格式/同尺寸 resolve 和明确的 GL 错误/guard 检查; 不从 production helper 复制预期.
- [用 caps 不支持跳过本该覆盖的格式] → 逐组合记录负例, 既定 baseline 必须满足; 缺 fixture 入口、driver 错误和不支持分别记录, 不返回统一成功.
- [fixture 影响 ContextState 或逃逸了未追踪 GL 工作] → 独立资源、状态 guard、公开 ClearState 和后续已知上传; 强引用保留到辅助真实完成, 与产品 failure 组隔离.
- [已有 native 证据只覆盖单一 Windows/驱动与单屏环境] → apply 记录当次实际范围, 不宣称跨厂商/平台/跨屏验收. D01 未满足或没有本包明确例外时不启动 native 实施.
- [纯验证中发现需要扩大能力的缺陷] → 先区分恢复既有合同与改变合同; 前者补充局部设计/失败回归, 后者重新规划 delta 和范围, 不在测试中绕过.

## Migration Plan

1. 完成本轮 proposal/design/tasks、官方 skip 状态及 strict/链接/改动范围核对; 全部实施任务保持未完成.
2. apply 先满足决策 1 的入口, 固定输入与测试入口后按 tasks 逐组构建验证; 发现失败时先留复现并修订局部设计.
3. 逐组通过后汇总现有规格的证据、完成 Code Review/原生 verify, 更新 T05 与实际边界. `skip_specs` 保持成立时归档无需主规格同步; 如实际行为变化, 先走规格修订与验证.
4. 回退仅撤销本包的 fixture/文档及独立可审查修正, 保留失败记录; 不改已归档的 T03/T04 或回退已提交上传行为, 不触发部署/提交/推送等外部动作.
