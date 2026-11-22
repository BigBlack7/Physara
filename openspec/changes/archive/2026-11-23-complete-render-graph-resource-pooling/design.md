# Design

## Context

- `GraphCompiler` 已按版本及 Texture mip/layer/aspect、Buffer 字节追溯内容。它从显式导出和读回根裁剪，再按完整对象的存活首末位置分配兼容槽，按完整描述计逻辑字节。纹理导入目前只有整允许范围有效或无效，Buffer 已有部分字节覆盖。 [内容追溯](../../../../Engine/Renderer/RenderGraph/GraphCompiler.cpp) · [物理槽计划](../../../../Engine/Renderer/RenderGraph/GraphPlanStorage.cpp) · [导入事实](../../../../Engine/Renderer/RenderGraph/GraphResources.hpp)
- `GraphExecutor` 每次执行私有持有槽表，在首用创建对象，图尾回传对象和出口，归还 GraphManaged 后由 Runtime 关闭和提交。FBO 与 Pass BindingSet 在 Prepare 就绪，实发工作由 RHI 包继续持有。跨列表状态须考虑 Close 恢复及图退出后同录制的 Automatic 消费。 [执行器](../../../../Engine/Renderer/RenderGraph/GraphExecutor.cpp) · [OpenGL Close](../../../../RHI/OpenGL/CommandList.cpp)
- Runtime 已接收宿主呈现票据，按同槽真实完成复用。View 保留最后接受的显示色，附加 View 有独立 ShaderRead 出口。UI 托管纹理和占位目前以实际句柄跨帧导入。主图未执行时可用独立安全图重新导入旧输出。 [帧编排](../../../../Runtime/Frame.cpp) · [View](../../../../Engine/Renderer/View.cpp) · [UI 纹理](../../../../Editor/ImGui/UiTextures.cpp) · [宿主呈现](../../../../Platform/Presentation.cpp)

## Goals / Non-Goals

- 在现有 Windows x64/OpenGL 单 Graphics 队列上完成从存活计划、预算准入、首次物化到真实退休的闭环。由兼容存储复用、像素或字节结果及票据证据共同验收。
- 交付显式跨帧入口、导出保留和状态回传。以产品视口与 UI 作为直接消费者，以成对颜色/深度和 D08-T05 staging 检查后续历史与读回需要的资源合同。实际 SSR、TAA 历史生产点及业务拾取与截图由后续工作包接入。 [SSR 历史](../../../../Docs/Developments/D14-D15-ImagePipeline.md#d14-t04) · [HDR 历史](../../../../Docs/Developments/D14-D15-ImagePipeline.md#d15-t01) · [拾取](../../../../Docs/Developments/D16-D17-EditorAndDelivery.md#d16-t04)
- 首发按完整原生对象复用。异步多队列与 heap aliasing 属于独立立项范围，Shader 热重载和 D08 阶段整体性能测量沿各自工作包推进。 [开发计划边界](../../../../Docs/Developments/Developments.md#current-stage) · [D08-T07](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t07) · [D08-T08](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t08)

## Decisions

<a id="plan-and-pool"></a>
### 1. 复用同一物理描述和存活计划

编译沿用原内容边和稳定拓扑排序。纹理导入的局部有效覆盖归一后交给现有范围相交和未覆盖部分追溯，用同一事实校验声明、编译和执行权限。子资源范围只决定实际生产者、读取授权和内容有效性。槽继续按对象合并存活用途，根到图尾的保留延长整槽寿命。把同一兼容判定及逻辑存储字节计算用于图内计划和池匹配，避免两个层各自解释 storage desc。兼容键覆盖类型、格式、维度、尺寸、层数、mip、样本、usage/视图及状态协议。调试名用于定位。

池由 Renderer 在设备会话中持有，由 Runtime 通过现有初始化和结束路径装配。各 FrameSlot 和正常/安全 UI 图共用该实例。Compile 继续只消费能力和单图计划限额。完成编译与已有上传准入后，Runtime 在 Open 前为最终计划保留匹配空闲对象及可新建额度。预留只处理已存活槽，未被执行消费的预留自动归还。执行器以此在计划首用取得对象，空闲未命中才调用 Device 创建。仅依赖每槽本地缓存会把重复尺寸和预算分摊到各槽，设备级所有权可以在同一账本上回收并解释压力。 [单图合同](specs/render-graph-compilation/spec.md) · [共享池合同](specs/render-graph-resource-pool/spec.md)

<a id="pool-budget"></a>
### 2. 硬上限、空闲保留和准入结果

| 约束 | 初始产品配置 | 计量 |
| --- | --- | --- |
| 单图计划 | 保留已有 `EditorGraphProfile` 字节限制，增加 256 槽的上限 | 存活的不同物理槽，各槽计一次 |
| 设备会话图池 | 768 MiB、256 个 Texture/Buffer 对象 | 预留、已物化、在途与被保留的图池存储合计 |
| 完成后的空闲保留 | 128 MiB、64 个对象 | 已完成且解除持有的可复用存储 |

字节数是逻辑存储量。Texture 按完整格式块、mip、层或深度和样本计，Buffer 按完整容量计。初始上限依据现有两路 Forward 各 128 MiB、UI 上传纹理 32 MiB、测试图 32 MiB 及占位的单图规划，并为上次接受的两路输出和已有有界 UI 纹理留出余量。三帧同时出现高需求时允许产生背压。实现时用 1920×1080 主 View 和 1280×720 离屏 View 的已支持单样本组合记录实际计划、占用峰值和拒绝。小预算注入覆盖上限和淘汰，不要求分配到产品硬上限。默认值为首发预算配置，测得的设备分配开销和性能由相应证据另行报告。 [当前图预算](../../../../Runtime/Frame.cpp) · [Forward 容量](../../../../Engine/Renderer/Paths/Forward.hpp) · [现有 UI 纹理容量](../../../../Editor/ImGui/UiTextures.cpp)

优先保留将由当前计划使用的兼容空闲对象。容量不足时，从其余符合条件的对象按最久未用顺序淘汰，容量推进时把空闲使用降到保留上限。图实际持有的对象在解除持有前保持计费。释放最后的 recipe 依赖和池引用后，在现有 owner/context 回收点推进原生删除。宿主目标、资产驻留、外部 staging 和它们的既有预算独立。图 Pass 的 FBO/BindingSet 继续随实际录制保活，沿现有阶段释放强引用。复用完整资源槽已经能解决重建和 resize 积累，跨帧 FBO/BindingSet 缓存需要独立可测的需求。

空池也装不下的计划返回硬超限，提示请求量和限制。录制或预留等待、已接受提交的完成等待及持续的外部持有分别诊断。附件或绑定即使也随包保活，只要其外部 owner 仍在，退休后就会继续占用容量。执行出口解除已消费计划的预留，已登记录制及导出继续依各自合同持有。主图无法准入在打开录制前转独立安全 UI。安全图重新编译和准入，仍失败则归还取得并走既有有限事件等待和退休推进。首用创建、view/FBO 或 binding 的错误仍按执行失败处理。 [帧恢复合同](specs/editor-frame-graph/spec.md) · [有界帧槽](../../../specs/frame-slot-lifecycle/spec.md)

<a id="handoff-state"></a>
### 3. 区分图内版本、物理状态和已接受内容

| 阶段 | 保持的事实 | 允许的操作 |
| --- | --- | --- |
| 图尾且成功归还 GraphManaged | 同一图代、版本、实际对象、允许的导出范围及用途、图尾状态 | 同一合法录制可按用途接续消费，跨列表输出仍待接受 |
| 成功 Close 且取得匹配接受 | 实际跨列表入口，含 `keepInitialState` 恢复和最后一次合法外部访问，已定义覆盖和匹配票据 | 同设备、同队列重新导入受保留版本，确认对应 owner 的生成结果 |
| 最后一次实际使用已完成且全部持有解除 | 可复用的物理末态和退休证明 | 归还池，后续逻辑资源按新的内容声明使用 |

保留 RHI tracker 与后端发射为唯一同步权威。以实际的录制、Close 和提交关联取得交接事实。具有明确 Close 恢复合同的资源直接核对该合同，其它范围从原录制已知状态回传，覆盖图退出后仍合法执行的命令。纹理物理状态按 mip/layer，Buffer 按对象。图的已定义内容单独按 mip/layer/aspect 或字节覆盖记录。保留单一规范导入身份，允许在同一次声明提供可核实的非均匀状态范围。没有已知状态的部分保持不可重新导入或复用，直到原 owner 能证明可用。保留状态与范围可以用现有图边界和 Physara 即时录制接口向上交付。 [资源声明](specs/render-graph-contract/spec.md) · [执行交接](specs/render-graph-execution/spec.md)

受保留的导出携带设备、物理身份、用途、范围和录制关联，View 或其它 owner 再关联自己的角色与输出代。让跨帧持有包含可追踪的池租用，Pass 及同录制消费者只能借用实际对象。正常图、独立安全 UI、附加离屏消费者和图外读回均将实际登记的使用归入对应录制，并按匹配提交更新最后使用。外部原本持有的普通资源仍走原有导入合同。单独的裸对象地址、引用计数或逻辑帧号不足以决定归还时机。同队列读取旧版可以在其生产提交仍 Pending 时开始，重新分配为另一个资源须等最后实际使用完成。

可原地更新的纹理由图池为 owner 维护逐 mip、layer、aspect 的有效覆盖。每条 Clear 到达可能修改目标的阶段时撤销其覆盖，普通写在用途准备成功并进入回调时撤销覆盖。成功 Pass 按 Store 效果登记，可能已写但未能证明结果的范围仍需重新定义。只读出口保留它自己的内容范围。有该类出口持有时，重叠的原地写在接管录制前拒绝。

只读池项的持有保留本代内容。可原地更新的池项以覆盖实际导入范围的固定快照保护内容。对于具备上述内容保证的已编译纹理导入，在最近已接受观察推进后，预检逐个核对声明范围内的 mip/layer 是否仍与编译所用的入口状态一致。通过预检的录制接续对象的最近已接受观察并登记本次实际使用。

图内引用以同一身份及内容版本解析，写权限由创建者、可写导入的 owner 或相应写操作显式取得。采样别名沿规范版本查找，保留只读权限。合并等价导入时，声明仍保有自己的允许范围，图同时接管各固定快照承诺的保护，直至释放这些持有。写声明、写入对象查询及可写出口都要求带写权限的引用。托管 token 注销后，只要有效驻留别名仍持有源池租用，后登记的等价别名就沿同一来源导入。 [跨帧资源声明](specs/render-graph-contract/spec.md) · [导出与执行](specs/render-graph-execution/spec.md)

<a id="frame-owners"></a>
### 4. 将交接落实到现有消费者

- View 继续以自己的身份、角色、当前请求与输出代确认主视口或离屏输出。最后接受色在新候选成功前持有，resize、相同描述的下一帧和格式变化也走同一规则。UI registry 保持稳定 token，与本图中由 Pass 解析的确切版本衔接。旧图经安全 UI 被实际采样时，旧持有延续至这次读取真正完成。图内帧缓存的引用随其安全复用释放。
- 诊断图、字体 atlas 和占位的初次持久交接保留实际池持有。UI 注册表确认托管创建时，按请求版本、范围、用途和更新权选择出口。已存在的 UI atlas 局部更新仍由 UI owner 按规范导入、Load 和提交确认维护，其可更新内容与 View 的保留版本分开。失败的部分更新不确认新的 UI 请求。Window PresentColor 继续按宿主的独立尺寸和票据交接。
- `ShaderRead` 交付 UI 采样和已支持的历史读取。颜色与深度需要配对时，调用方对同一 View、尺寸/格式代和接受票据一起确认。只在有该消费的图声明对应范围出口，不在每个产品帧额外生产历史。图内读回继续用 `MarkReadback` 和 `CopySource`，图外通过 `Readback` 出口在合法录制追加复制并登记源实际读。读回保留源到相应最后使用安全完成，CPU 结果沿 D08-T05 的接受、Event、非阻塞 Poll 和 Map。 [历史发布](../../../../Docs/Architecture.md#history-commit) · [传输与 staging](../../../specs/opengl-immediate-transfer/spec.md)
- 录制从已知物理状态接续，无内容的池槽仍是本图未定义版本。确认从未使用的预留可立即归还。普通失败前缀若获可靠票据，仍由原包和池保护，完成后只把末态明确的对象归为空闲。受控退出路径保留未获可靠票据的已发工作，状态不明的对象按原设备协议释放。失效 context 下仅进行允许的 CPU 收拢。 [退休协议](../../../../Docs/Architecture.md#retirement) · [退出协议](../../../../Docs/Architecture.md#shutdown)

<a id="graph-diagnostics"></a>
### 5. 从事实 owner 采集图摘要与池快照

由计划提供声明数、活跃数、裁剪数、导入与出口数以及槽数和逻辑字节。Runtime 将图代、候选类型、阶段和接受回执与最近图摘要关联，保留主候选首错及必要的安全 UI 结果。池提供对象与逻辑字节的容量、当前量、高水位和本次准入结果，命中、新建与淘汰在对应动作处计数。已物化的同一对象只统计一次，录制、在途和被保留事实有明确采样时点及归属。RHI 继续提供实际操作和 GPU interval，最近已接受场景与本轮失败或安全图具有各自身份。快照只持有有界值，Editor Statistics 面板直接读，按既有可见性决定构建。 [统计合同](specs/render-frame-statistics/spec.md)

## Risks / Trade-offs

| 风险 | 处理 |
| --- | --- |
| 当图内尾状态、Close 恢复态和实际读写范围不一致时错误归还 | 以 RHI 接受事实交接，验证选层读回及再次导入。每个逻辑版本重新证明自己的有效内容 |
| 旧图仍被外部消费者或合法前缀持有，resize 后原生对象被覆写或从账本消失 | 从输出租用到最后一次实际访问持续计费，用独立新输出代及在途保活证明互不影响 |
| 大计划占用基础 UI 的额度，或尺寸反复变化导致空闲资源无界积累 | 开录制前为实际存活计划准入，完成后按高水位回收。安全图按自身真实需求重新准入，失败时归还取得 |
| 用完成判定以外的 CPU 计数或已就绪 Timer 提前复用 | 只沿原 Graphics 完成事实回收，采用可控制 Pending/失败与原生完成的两类证据 |

## Migration Plan

先扩展计划及导入事实，再接入池准入和录制交接。随后同一提交内贯通 Runtime、View、UI 及统计，验证正常图和独立安全图。阶段落地以实际 Windows/OpenGL 构建和回放为准，现有纯 CPU 构图及独立图执行入口继续服务已有消费者。线上无场景文件、Shader ABI 或资产迁移。 [实施与验收顺序](tasks.md)
