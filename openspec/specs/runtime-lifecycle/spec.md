# runtime-lifecycle Specification

## Purpose

Define how the Physara product composes diagnostics, the verified native host and the editor session on the main thread. The lifecycle specifies startup, input processing, CPU UI frame readiness, idle behavior and orderly shutdown.

## Requirements

### Requirement: Provide one real product entry

The project SHALL build one product executable that composes the diagnostic owner, verified native host and editor session on the process main thread. Default startup SHALL create a verified visible native window. Hidden startup SHALL use the same host, editor and graphics assembly. Each renderer or graphics-device owner SHALL serve an implemented role. The editor SHALL initialize a compatible UI Shader package for the running build configuration, the required base font atlas and the resources needed to compose the editor. On a renderable run the initial font upload SHALL succeed and be accepted before the product reports the editor ready for presentation. With no rendered scene output, the product SHALL still clear the window target and present the dockable editor with a diagnostic viewport. The product SHALL distinguish CPU UI readiness, accepted graphics work and successful window presentation.

#### Scenario: Start the default product
- **WHEN** the product starts without options on a supported system and continues with a renderable window and an available target
- **THEN** the owner thread produces sealed CPU UI frames, composes them through RHI on initialized window-size color and presents the dockable editor with a labeled test viewport or a diagnostic placeholder when its test output is unavailable

#### Scenario: Start a hidden bounded run
- **WHEN** the product starts with hidden mode and a valid poll limit
- **THEN** it uses the same verified host and editor rendering path for eligible iterations and exits successfully after the specified number of successful polls

#### Scenario: Fail when the required UI program cannot be initialized
- **WHEN** the configuration-matched UI Shader package is unavailable or incompatible during normal startup
- **THEN** the product reports a bounded startup failure, releases acquired owners in dependency order and exits with status 1

#### Scenario: Fail when the required base font cannot be initialized
- **WHEN** creation of the required base atlas or preparation or recording of its initial upload fails during initialization or the first eligible rendering iteration
- **THEN** the product reports a bounded startup failure, retains any issued work through controlled shutdown and exits with status 1 without presenting an editor frame with the missing font

### Requirement: Validate explicit startup options
The product SHALL support `--help`, `--hidden`, unsigned 32-bit logical `--width=N` and `--height=N`, and a positive unsigned 64-bit `--poll-limit=N`. Unknown, duplicate, malformed or out-of-range options SHALL exit with status 2 before native initialization. Help SHALL exit with status 0 without requiring native initialization. Zero dimensions SHALL reach host configuration validation and fail startup with status 1.

#### Scenario: Request help without a graphics environment
- **WHEN** the caller supplies valid options including help
- **THEN** usage is printed and status 0 is returned without creating the diagnostic owner or initializing the native library

#### Scenario: Reject invalid argument syntax
- **WHEN** an option is unknown, repeated, malformed, numerically out of range, or uses a zero poll limit
- **THEN** the product prints a bounded argument diagnostic and exits with status 2 before native initialization

#### Scenario: Reject invalid host dimensions
- **WHEN** a correctly parsed startup dimension is zero or exceeds the native dimension range
- **THEN** the normal host factory rejects configuration, the product reports its actual failure and exits with status 1 without a partially ready host

### Requirement: Separate event decisions from idle waiting
The product SHALL consume each successful input publication before choosing whether to continue. It SHALL handle close intent and the configured poll limit before another UI frame or idle wait. Iconified or zero-pixel windows SHALL continue event, input cleanup and close processing while suspending normal UI frame construction. The idle wait for both renderable and suspended windows SHALL be a finite platform event wait. On recovery the editor SHALL consume current observations.

#### Scenario: Consume a close request
- **WHEN** the product receives published native close intent
- **THEN** it chooses orderly exit before another UI frame or idle wait, and explicit shutdown releases the window

#### Scenario: Remain responsive while suspended
- **WHEN** the window is iconified or its framebuffer extent is zero and later recovers or receives close intent
- **THEN** event processing and interaction cleanup continue with finite waits, no replacement target is allocated or submitted, and a recovered UI frame uses current window facts

#### Scenario: Reach the configured poll limit
- **WHEN** the requested number of successful publications has been consumed
- **THEN** the product proceeds to shutdown without another UI frame or wait, and callbacks processed during earlier waits do not count as additional polls

### Requirement: Exit in ownership order with meaningful results

Normal close and poll-limit completion SHALL return status 0 after successful shutdown. Diagnostic creation, host or editor creation, required base UI initialization, unrecoverable runtime processing and explicit shutdown failures SHALL return status 1 with a bounded observable diagnostic. A recoverable frame failure SHALL be reported and resolved through the frame graph fallback or cancellation contract while event processing and retirement continue; that handled failure alone SHALL NOT force a process failure result. A Fatal graphics diagnostic or loss of reliable submission or completion ownership SHALL stop new rendering and enter controlled shutdown.

On the owner thread the editor SHALL release its input adapter and session state before host shutdown, which SHALL precede diagnostic-owner destruction. Cleanup or diagnostic-output failure SHALL NOT replace an earlier operation's failure or falsely imply that acquired state was ready. Fatal severity SHALL NOT be interpreted as proof of native device loss; shutdown SHALL follow the backend's actual health and completion facts.

#### Scenario: Complete normal shutdown

- **WHEN** a normal run exits and the same product is subsequently launched again
- **THEN** each process releases the editor callbacks before the host and diagnostics, and the later process can initialize normally

#### Scenario: Fail after startup has begun

- **WHEN** editor setup or another startup or unrecoverable runtime operation fails, including failure while reporting or cleaning up the original error
- **THEN** the product preserves the original failure, releases every acquired owner in dependency order and returns status 1

#### Scenario: Continue after a handled frame failure

- **WHEN** a running editor handles a graph failure through a successful safe UI frame or safe cancellation, later renders a valid normal frame and then receives normal close intent
- **THEN** input and retirement remain responsive across the failed iteration, no failed output is published, and successful shutdown returns status 0

#### Scenario: Preserve startup failure for a required font

- **WHEN** the required base font cannot be prepared or its initial upload cannot be recorded and accepted
- **THEN** fallback does not hide the startup failure or report the editor ready, outstanding work is retained for controlled shutdown, and the product returns status 1

#### Scenario: Stop after a fatal preparation result

- **WHEN** frame preparation reports Fatal after a presentation acquisition has been obtained
- **THEN** the product does not open a graphics list or start another fallback, releases CPU borrows and resolves the acquisition through legal shutdown ownership, and returns status 1 without waiting on a fabricated completion value

### Requirement: Advance CPU asset completion at the continuing input boundary

Runtime SHALL 在主线程装配 CPU 资产服务，并在每个继续运行的有效输入轮次处理可用结果。完成处理 SHALL 位于终止判定之后、帧槽取得、编辑器准备及图执行之前，且无需等待 worker 或 GPU 完成。窗口不可渲染和帧槽忙时仍使用同一处理边界。单独的资产请求失败 SHALL 作为可恢复的资产状态报告，已就绪的编辑器继续按既有生命周期运行。正常关闭与受控退出 SHALL 在释放发布目标之前停止资产接单及发布，取消并收拢作业。

#### Scenario: Publish CPU results while the window cannot acquire a frame
- **WHEN** 资产完成结果已待取，Runtime 接到一个继续运行的输入轮次，窗口处于暂停状态或全部帧槽在途
- **THEN** 主线程可以当轮发布匹配的 CPU 记录。窗口、帧槽和呈现依然遵循既有取得与等待合同

#### Scenario: Exit instead of publishing a terminal batch
- **WHEN** 关闭请求、轮次上限或受控退出使 Runtime 停止运行，且一个作业仍在执行或有结果待取
- **THEN** 系统停止接单和发布并收拢 CPU 作业，终止路径没有新的资产发布。编辑器、宿主和原有完成协议继续按所有权顺序退出

#### Scenario: Continue the editor after an optional asset failure
- **WHEN** 一个可选图片请求完成且报告读取或解码失败，现有基础 UI 可用
- **THEN** 调用者可观察失败原因，继续的输入轮次和可呈现帧仍按产品原有路径推进

### Requirement: Advance scene document work at the continuing input boundary

正式 Runtime SHALL 以空 Scene、无路径文档和主 View 的默认导航请求启动。每个继续运行的输入轮次 SHALL 在处理可取的资产结果之后且在正常图准备之前推进文档文件操作和匹配的候选发布。完成处理 SHALL 独立于可绘制状态、GPU 槽及呈现进度。文件失败 SHALL 返回会话状态并让正常输入和 UI 继续。终止时 SHALL 停止接收和发布文档工作，取消可取消的作业并收拢实际文件结果，然后释放相应会话、资产和发布目标。

#### Scenario: Start the existing product with an empty document
- **WHEN** 正式产品以常规或隐藏方式启动且没有打开文件请求
- **THEN** 主线程装配同一 Scene、主 View 和 EditorSession，空文档可用，现有诊断视口及可绘制时的 UI 正常推进

#### Scenario: Advance document work without an available frame slot
- **WHEN** 有可取的打开或保存结果，运行继续而窗口暂不可绘制或全部帧槽在途
- **THEN** 编辑器会话仍在 CPU 边界处理匹配结果，图资源的取得和发布继续走原合同

#### Scenario: Retire document work before owner teardown
- **WHEN** 正常关闭、轮次上限或运行失败触发退出，且打开或保存仍在执行或已完成待取
- **THEN** 不再接受新的文档发布，取消与提交按实际文件边界收拢，后台工作结束后才释放相应 owner

### Requirement: Prepare current scene views before graph declaration

正常可准备帧 SHALL 在资产与文档发布、CPU UI 封存及领域事务应用后，用安全取得的槽准备当前 SceneFrame 和 View 工作集。真实场景帧 SHALL 在声明绘制前准备其静态 GPU 驻留、有效 Draw 和拥有型上传；构图与执行 SHALL 仅消费帧内准备结果、声明的图资源及其已拥有依赖。无可准备布局、窗口暂停或无安全槽时 SHALL 跳过本轮场景提取，继续按现有边界推进 CPU 文件、编辑和退休工作。局部 View 或场景资源无法准备时 SHALL 沿用可用的编辑器图和占位，保留相应诊断。测试图与 Forward 预览 SHALL 按各自实际消费的有效回执确认设置，Forward 的场景渲染记录只来自接受的真实场景输出。

#### Scenario: Prepare the scene after this iteration's publication and edits

- **WHEN** 已完成文档或资产结果在继续轮次发布，同一轮 UI 领域事务更新当前 View 或修改场景，且有匹配的布局和安全槽
- **THEN** SceneFrame、相机和 Forward 准备来自该轮最终接受的同一 Scene/View 状态，构图与执行无需访问活动 ECS

#### Scenario: Skip preparation until a layout and slot are available

- **WHEN** 视口隐藏、窗口不可绘制或无安全槽，本轮仍有可取 CPU 工作，随后取得新布局及槽
- **THEN** 前一轮只按已有生命周期推进允许的 CPU 和 UI 工作，后续帧从届时的最新接受状态准备，未占用在途槽

#### Scenario: Keep the diagnostic UI usable after camera preparation fails

- **WHEN** 可见 View 的相机或有效图像矩形准备失败且基础 UI 可用
- **THEN** 该 View 呈现原因，窗口仍可合成其余 UI，无效准备值无法推进任何场景输出或被确认的渲染历史

#### Scenario: Recover from an unavailable scene pipeline with an independent editor path

- **WHEN** 基础 UI 已可用，当前场景的程序包、Pipeline 或驻留资源未能准备，之后一次准备获得匹配的可绘资源
- **THEN** 可恢复期间编辑器继续报告对应原因，未准备好的候选没有有效场景输出；恢复后本帧匹配版本才经图执行和接受回执进入视口
