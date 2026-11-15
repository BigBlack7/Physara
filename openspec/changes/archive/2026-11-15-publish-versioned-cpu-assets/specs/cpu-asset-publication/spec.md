# Spec Delta

## Purpose

为静态场景与编辑器提供可固定版本的 CPU 资产数据，使后台导入在受控资源根和明确容量内完成，并且只有当前有效的结果能在主线程成为下游可见的记录。

## ADDED Requirements

### Requirement: Read asset bytes from a controlled resource root

资产导入 SHALL 接收相对于给定项目资源根的路径，并且只读取本导入类型允许的 `Assets` 子目录中的普通文件。路径验证 SHALL 覆盖规范化后的引用和实际打开的文件。读取 SHALL 在配置的字节上限内完成，并以送入解码器的同一份完整字节计算源内容指纹。路径拒绝、不可读、短读或超预算 SHALL 返回可区分的有界错误。

#### Scenario: Resolve an image without depending on the current directory
- **WHEN** 调用方给定项目资源根，并从另一工作目录请求一张允许目录内的合法图片
- **THEN** 系统以给定根定位图片，并为该记录提供与已知文件字节匹配的内容指纹

#### Scenario: Reject an out-of-root reference
- **WHEN** 输入为绝对资源路径，或相对路径规范化后或经文件系统重定向后落到允许目录外
- **THEN** 系统拒绝该来源，并且源正文未交给读取或解码阶段

#### Scenario: Reject a file beyond the byte limit
- **WHEN** 已定位的普通文件超过本次读取的字节上限
- **THEN** 系统报告预算错误，并且该次结果没有有效像素或可发布版本

### Requirement: Admit bounded asynchronous asset requests

系统 SHALL 将逻辑 `AssetId`、每次导入请求及当前已知源代分别标识。没有请求的资产以 `Unloaded` 表示，已准入的请求以 `Loading` 表示。调用方对同一规范化来源、同一导入配置及同一已知源代的重复请求 SHALL 共享正在执行的作业。系统 SHALL 对排队、执行和待取结果应用共同的有界准入，并对输入和像素执行字节预算。作业准入额度耗尽时 SHALL 立即返回可重试的拒绝，拒绝的请求不进入 `Loading`。

#### Scenario: Coalesce requests for the same source generation
- **WHEN** 同一图像加载中，调用方以规范化后相同的资源路径和同一导入配置再次请求
- **THEN** 两次调用观察同一个资产与当前请求，完成后只产生一次可见发布

#### Scenario: Retry after the completion capacity is released
- **WHEN** 当前准入额度已被运行作业或尚未取走的完成结果占用，调用方请求另一资产
- **THEN** 新请求返回可重试的拒绝且未开始导入。已占用额度释放后可重新提交

### Requirement: Publish only current immutable asset revisions

完成结果 SHALL 自持有效载荷或失败诊断。主线程只在 CPU 帧边界接收仍匹配当前请求、已知源代和目标资产及基准版本的结果。显式源失效 SHALL 立即撤销旧代的发布资格，独立于后继请求是否准入。完整成功的内容 SHALL 一次性发布为不可变 `CpuReady` 记录，已发布 `ContentRevision` 只随所接受的新内容推进，相同来源与导入配置的相同源指纹复用原版本。对当前有效请求，失败 SHALL 返回 `Failed` 及有界原因。失败、取消和不匹配结果 SHALL 保留先前有效内容，已经持有旧版本的调用方可读到原值直至释放引用。`CpuReady` 只证明 CPU 数据就绪。

#### Scenario: Publish a complete image on the owner boundary
- **WHEN** worker 完成当前有效请求，并且 owner 先观察状态再处理这次完成结果
- **THEN** 发布前状态仍为 `Loading`，处理后调用者整体取得有效像素、源指纹和同一 `ContentRevision`，记录中只有 CPU 数据与引用

#### Scenario: Drop a result for an older source generation
- **WHEN** 调用方显式声明同一来源已有新一代，并在新代成功发布后才收到旧代的完成结果
- **THEN** 旧结果被丢弃。当前状态、诊断和版本仍对应新代，事先固定的旧有效引用依然可读

#### Scenario: Invalidate an old request while admission is full
- **WHEN** 调用方显式使正在加载的来源失效，但紧接的新请求因额度已满而尚未准入
- **THEN** 旧请求的结果仍被丢弃，最后一次已发布的有效内容可读

#### Scenario: Retain a version when content is unchanged or a replacement fails
- **WHEN** 已发布资产经显式刷新先读到相同源字节，随后又遇到一次无效输入
- **THEN** 第一次刷新复用已有版本。第二次返回 `Failed` 和原因。原记录及其版本仍可取得

### Requirement: Cancel and join outstanding asset work

取消 SHALL 使对应请求失去发布资格，并使仍为当前的请求可观察为 `Cancelled`。已撤销的请求即使之后完成，也 SHALL 由系统释放独立载荷。开始关闭后系统 SHALL 拒绝新请求、停止完成结果发布、取消未完成请求、收拢 worker，并在返回时释放系统尚未发布的载荷。

#### Scenario: Cancel a request with an undelivered completion
- **WHEN** 调用方取消一个正在加载且尚未发布的请求，其完成结果随后到达 owner
- **THEN** 调用方仍观察 `Cancelled`，本次请求不产生新版本。退还额度后可以发起新请求

#### Scenario: Stop with queued and running work
- **WHEN** 服务在至少一项任务尚未收拢时开始关闭，并收到新的提交请求
- **THEN** 新提交被拒绝，未发布任务失去发布资格。关闭返回时 worker 已退出，未发布结果已释放

### Requirement: Preserve image interpretation separately from decoded pixels

首版图片导入 SHALL 将每样本至多 8 位的 PNG 和 8 位 JPEG 解码为拥有型 RGBA8，并保留宽高、实际像素布局、从顶部开始的行序和一个有效图像层级，输入及结果均受预算约束。颜色或数据用途通过独立的 CPU 描述引用同一图片版本。颜色 RGB SHALL 标记为 sRGB，数据通道 SHALL 保持线性。基础颜色和 Editor 图像的覆盖率 alpha SHALL 保持线性且不预乘，源没有 alpha 时按不透明解释。法线和 ORM 的数据解释 SHALL 保留对应通道。HDR/EXR 元数据 SHALL 区分调用方声明的源编码、线性辐亮度与亮度标定，缺少标定时表示相对值。超出首版像素格式的解码请求 SHALL 返回明确的未支持结果。

#### Scenario: Interpret the same pixels as color and linear data
- **WHEN** 调用方将同一张已发布、具有已知透明像素的 PNG 分别描述为 Editor 颜色与线性数据
- **THEN** 两种描述可共享同一只读像素版本，并具有不同的用途身份。色彩图的覆盖率与解码的 alpha 一致，像素字节和行序保持一致

#### Scenario: Describe an uncalibrated environment source before decoding is supported
- **WHEN** 调用方描述为环境使用的 HDR 或 EXR 源且未提供亮度标定，并请求解码其像素
- **THEN** 环境描述表达各自的源编码、线性辐亮度和相对标定。解码请求报告格式未支持，不产生 `CpuReady` 记录
