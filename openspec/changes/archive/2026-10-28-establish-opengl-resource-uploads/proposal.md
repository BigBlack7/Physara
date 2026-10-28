# Proposal

## Why

D02-T03 已完成真实 OpenGL 设备、基础资源及删除协议, 但公共 WriteBuffer/WriteTexture 所属的原生 CommandList、内容上传和提交仍明确不可用. D02-T04 必须把 CPU 源数据消费、实际写入、完成保护与关闭连成可验收的一条路径, 才能继续验证资源内容.

## What Changes

- 以 [D02-T04](../../../../Docs/Developments.md#d02-t04) 为主工作包, 实现有界的普通 Buffer 区间写入和 Texture 完整 mip/单层或完整 volume 写入, 明确源数据在 Write 返回后可释放. 初始纹理写入限定现有 13 种非深度/模板颜色格式和 7 种非 MS 维度; MS、深度/模板及压缩等内容写入保持明确拒绝, 不改变其既有分配支持.
- 接入上传专用的 Graphics/单即时列表, 保留完整公共接口和 Validation 路径. 将 [D03-T03](../../../../Docs/Developments.md#d03-t03) 的必要 open/close 子集及 [D03-T04](../../../../Docs/Developments.md#d03-t04) 的提交/fence/在途保活基础随上传先落地, Execute 不重放. Shader、绑定、Draw/Dispatch、volatile/push constants、完整 barrier lowering、RenderGraph 和呈现继续由原工作包实现.
- 在同一 Device 中建立有界上传存储、实际完成回收和未提交即时工作的退出协议. 区分拒绝当前操作、已经发出但未获票据的工作、已提交与已完成, 不能靠丢弃列表撤回 GL 工作或提前复用内存.
- 更新 Host 的关闭前置, 区分真实外部 owner 与 Device 自己的在途保活. 外部 owner 仍可重试拒绝; 只有内部工作时按实际完成收拢. 等待超时保留必要状态, fence/查询失败与 context 失效沿明确的受控退出路径处理.
- 从 [D02-T05](../../../../Docs/Developments.md#d02-t05) 吸收本包必要的 native 内容读回检查, 验证非对称数据、行序、mip/layer、来源释放、预算、拒绝、失败和在途退出. 这些证据不关闭 T05 全面验收、D03 绘制验收或 D01/D02 阶段出口.

**规划与实施入口:** D01 原 change 仍为 15/16, 真实跨屏及阶段出口保留未通过. 维护者于本次 apply 明确选择“同意限定推进，修订规划后继续（推荐）”, 单独允许本 change 的 T04 初始上传、必要即时列表/fence/在途保活/Host 关闭和 T05 最小读回先实施. 上传与完成协议不依赖跨屏比例变化, 仍须回归受影响宿主/输入. 该例外不继承或扩大 T01/T02/T03 授权, 不放行完整 T05/D03、呈现或 D01/D02 总出口; D01 原验收补足后恢复普通前置.

## Capabilities

### New Capabilities

- `opengl-resource-uploads`: 公共 RHI 上可验证的初始上传、源数据消费、单即时列表、真实提交/完成、有界存储、在途保活和设备退出行为.

### Modified Capabilities

- `opengl-device-resources`: 将 resource-only 阶段的列表/内容/提交拒绝限定为本包之外的操作; 分配证据与新增上传证据分别报告, 保留后续工作及前置边界.
- `native-window-host`: 扩展关闭协议以识别内部在途引用、保留外部 owner 的可重试拒绝, 并覆盖真实上传完成、未获票据工作和收拢超时.

## Impact

生产改动预计集中在 RHI/OpenGL 的 CommandList、Upload、Submission、ContextState、资源私有状态和既有 Device/删除实现, 以及 OpenGL.hpp 和 Platform 的 RHI 关闭互操作. 必须逐项覆盖现有 61 个 CommandList 纯虚方法及 68 个 Device 方法; 未交付方法明确拒绝. 六个通用公共头的字段/布局/默认值、Common/Validation 独立消费和全部已采纳通用合同保持.

不增加产品入口、测试框架、CI、依赖下载或 reference target. 实施期按实际源文件更新模块 CMake、Build 和 RHI 来源说明; 当前只更新本 change 与 Developments 对应工作包. Architecture 作为既有设计依据保持不变, 既有主规格/归档和 D01 原 change 不在规划阶段修改. 具体算法、预算、所有权、状态表、替代方案及验证步骤进入 design/tasks.
