# Runtime Lifecycle Spec Delta

## ADDED Requirements

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
