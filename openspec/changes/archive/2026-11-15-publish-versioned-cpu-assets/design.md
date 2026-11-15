# Design

## Context

D05 的运行循环在 `PollEvents` 后读取关闭请求和轮次上限，在继续的轮次才尝试取得帧槽。暂停窗口和槽忙仍处理输入与有限等待。资源定位已通过 Windows 主模块的路径查找配置对应的 UI Shader 包。Core 已有值身份和有界文本，Engine 尚无资产服务。动机见 [proposal](proposal.md)，行为以两份增量规范为准。 [Runtime 主循环](../../../../Runtime/Main.cpp#L145) · [可执行文件定位](../../../../Runtime/Resources.cpp#L17) · [Core 身份](../../../../Engine/Core/Identity.hpp)

## Goals / Non-Goals

### Goals

- 由正式的 CPU 图片导入入口消费 Core Jobs 与 IO。将可变请求状态集中在 AssetManager owner。发布结果可以脱离 worker、临时输入和帧槽独立读取。
- 建立有界的完成交接以及单一停止、取消和 join 顺序。Runtime 在 CPU 输入轮次推进资产完成，基础 UI 使用自己的就绪和失败协议。
- 以有类型的解码结果和用途描述衔接 T02 的外部图像及 T05 的纹理准备。

### Non-Goals

- T02 负责模型与依赖的完整内容身份、容器解码和静态数据校验。T03 负责实例及持久化。T05 负责 GPU 资源和图上传。D07-T04 负责环境像素解码及 IBL。
- 资产浏览、用户重导入、实例重绑定和卸载流程归 D16-T01。D06-T01 提供可被显式调用的失效机制。正式编辑器的导入入口按 D06 后续工作包接入。 [工作包顺序](../../../../Docs/Developments/D06-D07-SceneAndForward.md#dev-d06) · [D16-T01](../../../../Docs/Developments/D16-D17-EditorAndDelivery.md#d16-t01)

## Decisions

### 1. Core 管机制，AssetManager 管发布

| 所在模块 | 持有和推进的状态 |
|---|---|
| Core IO | 只读资源根与已校验的文件字节快照。对快照计算内容摘要。 |
| Core Jobs | 有界作业、取消信号、运行线程和拥有型完成结果的交接。 |
| AssetManager | 规范化来源查找、请求状态及诊断、最近已发布版本。只有 owner 可发起请求、取消、失效和发布。 |
| Runtime | 资源根和服务装配，选择 CPU 结果的处理时点，关闭时先关闭发布目标并收拢服务。 |
| 下游 CPU 消费者 | 持有指定版本的共享只读引用。替换 AssetManager 的当前版本时，已有引用继续读取自己的像素和元数据。 |

主线程处理轻量请求和有界完成批。后台作业只接收拥有的请求值、只读根及停止信号，读、hash、解码都在 worker 内完成。交接带完整载荷或有界失败值，引用寿命独立于临时输入。使用当前完成批的返回值报告实际发布即可，持久的版本事实由资产记录维护。 [Core 合同](../../../../Docs/Architecture.md#core) · [资产合同](../../../../Docs/Architecture.md#asset-types)

### 2. 资源位置和实际读取对象一起校验

- 项目资源路径按 `Assets/...` 相对于 Runtime 给定的资源根解释。Runtime 缺省按可执行文件目录定位根，复用现有宽字符模块目录查询。CPU 消费入口接受调用者明确提供的根。在首次实际请求时处理文件系统状态。开发 fixture 通过同一个入口显式提供根。
- 业务入口先规范化相对来源，校验所属目录和当前导入类型。T01 的独立 LDR 输入位于 `Assets/Textures` 或 `Assets/Editor`。绝对路径、越界路径、设备、目录及流等超出普通文件合同的输入返回路径错误。T02 在这里接入从模型目录解析的外部引用，并把允许的文件范围传给 Core。
- worker 以只读方式打开本地普通文件。在读取正文前，查询**同一个打开句柄**的规范最终位置，根据选定资源根和允许的目录完整组件校验。解析后落到根外、无法证实包含关系或对象类型不符时拒绝。以兼容只读共享的方式取得快照。取得长度并校验预算及整数范围后再分配，读取不完整时结束本次请求。哈希和解码使用这份缓冲区。
- 把 `Engine/Shader/BuildSupport.cpp` 中的 CNG SHA-256 字节原语提取到 Core。Shader wrapper 保持既有小写十六进制输出与错误转换，并将共用摘要实现纳入 Shader 构建身份所需的输入。Shader 的文件规则属于其各自的输入和包协议。 [资源路径约定](../../../../Docs/Architecture.md#asset-persistence) · [CreateFileW 与共享方式](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew) · [打开句柄的最终路径](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfinalpathnamebyhandlew)

### 3. 分开身份、请求代和发布版本

| 事实 | 使用方式 |
|---|---|
| 规范来源 | 项目资源根内的逻辑相对路径与实际导入配置。同一规范来源的重复请求可按值合并。用途、sampler 和 UV 参数由各自描述保留。 |
| `AssetId` | 复用 Core 的类型化 index/generation。失效目标或槽复用后的旧请求可被识别。 |
| 请求号与源代 | 一次准入产生一个请求号。显式声明源变化推进源代，并立即使旧请求丧失发布资格。源代描述 owner 已知的先后关系。 |
| 源内容指纹和 `ContentRevision` | SHA-256 标识实际输入字节。`ContentRevision` 标识本资产已接受的不可变内容，从首次发布开始随新内容递增。T02 将模型及相关输入纳入资产的持久化 `sourceVersion`。 |

准入时记录 `AssetId`（含 generation）、规范来源、请求号、源代和当前已发布版本。显式刷新将基准记录的源指纹作为值交给 worker。字节未变化时可以在解码前完成比对和确认，避免为了重复读取占用另一份像素额度。owner 在消费结果时对比身份和版本并确认取消状态。只有仍匹配的请求可以更新状态、诊断和记录。匹配成功且源指纹、导入配置相同时保留原记录并将当前代确认就绪。新内容整体替换当前引用。失败记录在本次请求，其对应的最后有效版本仍可单独取得。

同代的普通请求先检查已有进行中的作业或已确认的记录，再决定准入。失效与准入分别生效。即使队列暂满而新请求需要重试，旧源也已经失去发布资格。取消影响匹配的请求，已经确定为过期的失败随结果释放。该路径以显式通知为源变化边界。读取完成后发生的外部编辑，在调用者发出失效前不改变已取得快照的身份。 [不可变资产](../../../../Docs/Architecture.md#asset-types) · [源版本与实例固定](../../../../Docs/Architecture.md#asset-persistence)

### 4. 让待发布结果占用原作业额度

每个成功准入的作业占有一个额度，直至 owner 取走结果或停止时收拢作业。额度覆盖排队、运行和完成待取，worker 完成时已有交接空间。记录表项数、每项输入/解码尺寸和保留的像素字节都有有限上限。核对像素布局后先为候选预留像素额度，发布时额度随拥有型像素交接。已发布的像素在拥有引用存续期间继续计入预算，替换旧版时计入仍由读者持有的字节。最后一个引用释放时退还额度，预算状态随这些引用保活。

以可立即返回的结果报告暂满或停止。取消排队的任务可以在实际移除后释放额度。已运行的任务检查停止信号，计算结束后交出或销毁其自持结果。按文件读取、摘要、解码各阶段处理取消，关闭等待已经进入的底层 I/O 或解码调用返回，随后 join。启动失败、预算错误、解码错误或分配失败按发生阶段交出有界原因并退还未移交的额度。运行期间主线程只消费已经到达的完成结果。 [Core 作业退出](../../../../Docs/Architecture.md#core) · [取消与关闭](../../../../Docs/Architecture.md#shutdown)

### 5. 用途描述引用同一份 CPU 像素

`PhysaraAsset` 通过固定 stb 的内存入口解码 PNG/JPEG，实现宏只由实际解码的编译单元定义。适配层关闭 stb 失败字符串，移除未知 PNG 关键块分支的共享缓冲区写入，由适配层返回自持诊断。读取有界块头时识别并拒绝 CgBI，保障直通 RGBA 合同。JPEG 头探测返回失败时按 SOF 精度字段区分非 8 位输入与普通损坏。检查格式、长度、图像头、位深和可表示的像素字节数，再解码并核对返回布局。首版产出从顶部起的连续 RGBA8 和单层有效像素。完整输出的所有权随候选交给 owner。 [PNG 块格式](https://www.w3.org/TR/png-3/) · [JPEG T.81 附录 B](https://www.w3.org/Graphics/JPEG/itu-t81)

CPU 用途描述取值如下。

| 用途 | 图像解释 |
|---|---|
| 基础颜色、Editor 彩色图像 | sRGB RGB。若使用覆盖率 alpha，则按线性直通解释，保持非预乘。无 alpha 的源为不透明。 |
| Emissive、normal、ORM | Emissive 解释为 sRGB RGB。normal/ORM 直接读线性数据通道，按用途决定通道。 |
| HDR/EXR 环境 | 记录 Radiance HDR 或 OpenEXR 编码、线性辐亮度及标定信息。没有明确标定时为相对亮度。本包验证元数据和像素解码的未支持结果。 |

解码后的值可以生成不同的用途身份，两个描述共同持有不可变存储。sampler、UV 集和变换属于后续纹理槽的引用。T05 据 `AssetId + ContentRevision + usage`、格式及采样合同确定 GPU view/绑定，按实际用途生成 mip，并以对应图工作获接受作为 GPU 版本确认。 [图像语义](../../../../Docs/Architecture.md#asset-images) · [驻留和上传](../../../../Docs/Architecture.md#gpu-residency) · [glTF 颜色和数据通道](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)

### 6. 接入现有输入轮次与关闭顺序

`Runtime::RunEditor` 先判断 Fatal、poll 结果和关闭条件。对继续的一轮，在 `AcquireFrame` 与 `EditorSession::Advance` 之前处理 CPU 完成。窗口暂停时仍经过此处。资产诊断交给当前请求的 CPU 状态和有界错误入口。基础 UI 的启动条件和已取得的 GPU 工作由其现有 owner 处理。

在正常结束、早退和异常路径共用的作用域中，主线程先关接单与结果发布，再取消、唤醒并 join，最后释放待处理数据和相关 owner。发布目标在该步骤完成前仍存活，收拢期间的完成结果只用于释放载荷。后台线程在首次有效作业用到时启动，因此空场景启动不拉起解码工作。 [主循环合同](../../../../Docs/Architecture.md#frame-flow) · [关闭顺序](../../../../Docs/Architecture.md#shutdown)

T03 的 Scene 实例消费已确定的资产版本，T04 从同一份 Scene 状态提取保留该版本的只读 `SceneFrame`，交给 T05 准备驻留及构图。T06 将编辑器的用户意图接入这条输入链，T07 通过保存重开、真实呈现和退出验证各 owner 的寿命。 [D06 阶段出口](../../../../Docs/Developments/D06-D07-SceneAndForward.md#dev-d06)

## Risks / Trade-offs

| 失效方式 | 设计处理 |
|---|---|
| 校验路径与实际读取对象分离 | 规范化结果只用于请求与早期拒绝。读取前复核实际打开对象，由该对象提供用于解码及 hash 的字节。 |
| 新请求暂满，或取消与完成交错 | 先按已知事实更新资格。旧请求继续占用真实资源直至释放，由 owner 的发布检查处理交错。 |
| CPU 记录已替换，旧版本仍在使用 | 拥有型只读引用和字节预算覆盖实际读者的寿命。资产表只持有各来源当前版本。 |
| 资产哈希提取影响 Shader 产物 | 沿真实 EditorUi 包生成和消费路径核对构建输入、包验证与现有 ABI。 |
| 实际应用在 D06 后续工作包才产生 glTF/Scene 请求 | 使用正式图片解码和真实发布入口验证时序，以现有 Runtime 验证启动及退出编排。模型、环境像素和首个真实视口各有后续验收。 |
