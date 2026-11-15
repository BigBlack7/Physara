# Proposal

## Why

D05 已交付可呈现的编辑器帧图。进入 D06 后，[D06-T01](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t01) 需要先交付一个 CPU 资产入口，让后续模型导入和 Scene 能取得完整、可固定版本的标准数据，并在取消、失败或源变化时保留最后有效的内容。

## What Changes

- 提供版本化 CPU 资产记录及 `Unloaded`、`Loading`、`CpuReady`、`Failed`、`Cancelled` 状态。运行期资产身份、请求、源变化与已发布内容版本分别可校验。
- 从受控资源根异步读取文件，对实际解码字节计算内容指纹。用仓库现有 stb 导入 PNG/JPEG，给调用者返回拥有型、不可变像素及有界诊断。
- 对排队、执行和待取结果统一做有界准入。相同来源、同代的重复请求共享工作。继续的 CPU 输入轮次接收完成结果，只有仍匹配并通过校验的请求才能发布。关闭时停止接单和发布、取消并收拢作业。
- 用途决定像素解释。CPU 记录区分 sRGB 颜色、线性数据、覆盖率 alpha 和线性 HDR 辐亮度，允许同一图片版本供不同用途共享存储。HDR/EXR 在本包交付元数据合同。
- [D06-T02](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t02) 接入 glTF/GLB 容器、外部依赖读取及静态语义校验。[D06-T03](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t03) 消费确定的 CPU 版本建立 Scene 和场景文件。[D06-T05](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t05) 通过 Renderer 引入 GPU 驻留与首个 Forward 子图。环境像素解码和 IBL 由 [D07-T04](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d07-t04) 接入。

## Capabilities

### New Capabilities

- `cpu-asset-publication`: 规定受控源读取、异步请求、取消与退出、不可变版本发布和图片语义。

### Modified Capabilities

- `runtime-lifecycle`: 规定唯一 Runtime 的 CPU 资产完成处理边界及退出收拢顺序。

## Impact

- `Engine/Core` 提供受控读取、字节摘要和 CPU 作业机制。`Engine/Asset` 使用它们实现生产图片导入入口并管理 CPU 状态、版本及用途描述。
- `Runtime` 装配资产服务并在主循环推进完成结果。直接消费本包结果的入口是 CPU 图片导入 API。编辑器的模型打开和资产交互随 D06 后续工作包接入。
- `PhysaraAsset` 私有消费仓库固定的 stb。Core 复用原 Shader 模块的 CNG SHA-256 实现，Shader 继续沿现有摘要接口生成和核对包。
