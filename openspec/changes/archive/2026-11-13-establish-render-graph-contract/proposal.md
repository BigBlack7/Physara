# Proposal

## Why

D04 已通过阶段出口，编辑器能通过公共 RHI 绘制诊断视口和 UI，但上传、清屏、绘制与窗口交接的依赖仍隐含在 Runtime 的调用顺序里。先交付 [D05-T01](../../../../Docs/Developments/D03-D05-FramePipeline.md#d05-t01) 的图声明合同，让 [D05](../../../../Docs/Developments/D03-D05-FramePipeline.md#dev-d05) 后续编译和执行共用同一份可校验的资源描述、版本和用途。

## What Changes

- 在 `Engine/Renderer/RenderGraph` 建立仅依赖 Core 和公共 RHI 的声明层。纹理与缓冲使用类型化的图代、索引和逻辑版本，支持有范围的读写、显式导入和导出；等价导入共用身份，冲突导入和越界范围可诊断地拒绝。
- 用读写版本及所选范围表达旧内容依赖、清除、完整覆盖、局部保留和丢弃。写入创建新的逻辑版本，不隐式复制物理存储；导出指向明确的版本和外部用途。
- setup 在声明时运行，延迟回调的数据由图持有。Pass 资源访问按当前 Pass 的版本、用途和范围授权；非法访问在 Debug 和 Release 都失败。
- 用 CPU 消费者证明同一声明合同能表达现有的视口和字体上传、UI 几何读取、清屏与合成以及最终 `PresentColor`。构图不创建原生资源、发命令或取得窗口。

## Capabilities

### New Capabilities

- `render-graph-contract`：图内资源身份、范围、版本、外部资源边界，以及拥有型 Pass 声明与资源权限。

### Modified Capabilities

## Impact

- 增加 RenderGraph 的声明模块、构建目标及诊断入口；沿用现有 Core 身份与帧内存、公共 RHI 描述和句柄。Runtime、Editor、Platform、Shader 与后端行为不在本次改动范围内。 [架构 §6.2](../../../../Docs/Architecture.md#graph) · [当前帧编排](../../../../Runtime/Main.cpp)
- D05-T02 消费声明记录建立依赖、活性和物理计划；T03 接通首次使用物化、实际命令与同步；T04 接入产品并替换现有顺序编排；T05 收拢失败、UI-only 回退与取消。各包保留开发计划规定的独立验收。 [后续工作包](../../../../Docs/Developments/D03-D05-FramePipeline.md#d05-t02)
