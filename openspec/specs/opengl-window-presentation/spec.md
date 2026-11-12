# opengl-window-presentation Specification

## Purpose

定义 OpenGL 宿主如何取得窗口像素目标、呈现一帧有效的 SDR 编辑器颜色，以及如何独立报告缓冲交换与 GPU 完成。使调用者能判断目标何时可使用、失败后哪些工作仍需保活、何时可以安全复用资源。

## Requirements

### Requirement: Acquire a bounded window presentation target

宿主 SHALL 在所属线程按 Runtime 提供的有界帧槽索引为可渲染窗口取得与实际 framebuffer 像素尺寸一致的 `PresentColor`。每个索引 SHALL 只取得对应槽的目标，槽仍在途时返回可重试状态。只有首次使用或该槽上次提交已经真实完成时，宿主才可复用或更换目标。基线目标 SHALL 为单 mip、单样本二维 `SRGBA8_UNORM` 颜色附件，导出为 ShaderResource。绘制消费者只接收公共 RHI 目标和像素范围，默认 framebuffer SHALL 留在原生呈现边界。取得身份 SHALL 关联当前宿主、帧槽索引、目标代和本次取得，仅可结束一次。槽索引和取得身份 SHALL 在 Debug 和 Release 的宿主入口校验。零像素、暂停或暂不可用 SHALL 返回明确状态而无可录制目标。

#### Scenario: Acquire a renderable window target
- **WHEN** 窗口有非零像素范围且调用者指定可用呈现槽
- **THEN** 返回该槽中与当前像素尺寸匹配的 RHI 目标及有效取得身份，首个写入清除或完整覆盖颜色

#### Scenario: Suspend acquisition without a substitute target
- **WHEN** 窗口最小化或像素范围为零，随后恢复
- **THEN** 暂停期间无目标取得、分配、复制或交换，恢复后按新一轮观测的像素范围取得目标

#### Scenario: Reject an expired presentation identity
- **WHEN** 调用者指定越界槽索引，或提交已经取消、结束或属于其他宿主、槽或目标代的取得身份
- **THEN** 返回可诊断拒绝，不发出原生复制或交换，也不消耗另一份有效取得

#### Scenario: Rebuild one slot using the newest framebuffer extent
- **WHEN** 同一窗口连续改变像素尺寸，指定的呈现槽仍在途，随后真实完成
- **THEN** 待完成期间该槽的源和目标代保持有效，取得恢复时按最新有效像素尺寸成组重建该槽的目标，其它在途槽保持各自的目标和票据

### Requirement: Copy valid color before the tracking fence and swap

有效窗口帧 SHALL 先初始化颜色，再完成合成及列表关闭。OpenGL 末端复制 SHALL 在同一 context 上于关闭后发出，并先为实际源访问登记保活。Graphics 提交 SHALL 在末端复制之后插入有效跟踪 fence 并 flush。仅当该帧复制和提交成功，且同一取得仍具备交换资格时，宿主 SHALL 尝试一次缓冲交换。末端互操作 SHALL 保持下一次 RHI 命令使用的原生状态与其声明相符。

#### Scenario: Present one accepted editor frame
- **WHEN** 当前取得对应的有效颜色列表已经关闭，末端复制和提交接受成功
- **THEN** 可观察到源访问、跟踪 fence、flush 和同一窗口交换依次发生，交换结果关联本次取得和真实非零票据

#### Scenario: Reestablish RHI state after native presentation
- **WHEN** 末端复制使用了与前一列表不同的原生 framebuffer 或颜色及裁剪状态，后续列表使用前一帧的 RHI 绘制状态
- **THEN** 后续绘制仍写入其声明目标，scissor 与 sRGB 行为符合该列表设置

### Requirement: Deliver one SDR display encoding and correct row order

窗口复制 SHALL 依据当前 context 实际观测的默认后缓冲编码传递 `PresentColor` 的 SDR 颜色。sRGB 附件输出 SHALL 对线性 RGB 编码一次，线性附件路径 SHALL 输出一次编码后的 SDR RGB。窗口上、下、左、右 SHALL 对应同一逻辑像素方向，行序适配仅在呈现边界发生。

#### Scenario: Copy asymmetric UI colors to an sRGB back buffer
- **WHEN** 实际后缓冲为 sRGB，源为含半透明 UI 的非对称 sRGB 图案
- **THEN** 后缓冲保留图案的正确四角及裁剪边界，已知底色和叠加区的 RGB 对应一次显示编码

#### Scenario: Copy encoded SDR colors to a linear back buffer
- **WHEN** 实际后缓冲为线性，源已按 sRGB 存储，且复制范围逐像素对应
- **THEN** 后缓冲 RGB 仍为一次编码的 SDR 值，方向与非对称源图一致

### Requirement: Cancel failed frames without discarding issued work

窗口目标失效、录制或关闭失败 SHALL 取消本帧的复制与交换。末端复制报告 Error 或 Fatal SHALL 取消交换。取得取消 SHALL 保留已发工作的真实提交或受控退出责任。完整 UI 录制及其确认 SHALL 沿同一帧的有效回执，失败的录制或复制 SHALL 保留未确认的纹理请求。已取得的前缀票据只能用于相应资源的退休。

#### Scenario: Invalidate a frame before the terminal copy
- **WHEN** 颜色准备、录制或关闭被拒绝，或取得的尺寸在末端复制前已过期
- **THEN** 本帧没有末端复制或交换，可能已发出的前缀沿原设备的接受或收拢路径处理

#### Scenario: Fail a native terminal copy
- **WHEN** 末端原生复制实际报告错误
- **THEN** 保留有界诊断，禁止交换和本帧 UI 上传确认，仍保活所有可能已发工作

#### Scenario: Receive no accepted submission after the copy
- **WHEN** 复制发出后本次 Execute 未发布有效票据
- **THEN** 不交换、不报告有效完成，停止新帧并保留未证明完成的引用至受控退出

### Requirement: Retire the presentation source using real completion

末端复制使用的源纹理、附件 view 或 FBO 及相关目标代 SHALL 至少保留至覆盖该访问的真实完成。正常帧 SHALL 依据本槽提交的完成观察复用或替换目标，待完成时维持有界背压。完成查询失败 SHALL 停止复用并沿设备退出合同处理。正常帧 SHALL 避免全设备等待。

#### Scenario: Observe a busy slot before reusing or replacing it
- **WHEN** 已交换或取消的有效提交仍待实际完成，下一次取得请求同一槽
- **THEN** 源资源保持在途，槽在完成前不被覆盖或替换，真实完成后才可再次取得

#### Scenario: Close with an outstanding terminal source access
- **WHEN** 最后一帧仍在途，或 fence 或完成查询无法证明本次源访问完成
- **THEN** 停止发新工作，在适用的设备收拢或 context 失效协议中结束保活，不发布虚构的完成

### Requirement: Observe swap waiting and GPU completion independently

宿主 SHALL 以单调 CPU 秒值记录实际窗口交换调用的 `PresentWait` 与操作结果，GPU 工作完成 SHALL 来自相应设备提交的真实完成观察。交换状态、VSync 请求和 CPU 等待时间 SHALL NOT 用作 GPU 完成证明或并入 GPU 耗时。

#### Scenario: Observe a swap before the submission is observed complete
- **WHEN** 已接受的有效帧完成一次交换调用，而提交完成尚未得到设备确认
- **THEN** 交换结果和等待时长独立可观察，提交仍按票据推进完成

#### Scenario: Report a swap failure after submission acceptance
- **WHEN** 颜色复制和提交已被接受，随后窗口交换报告失败
- **THEN** 返回该次交换失败和实际等待区间，已接受工作继续按原票据退休
