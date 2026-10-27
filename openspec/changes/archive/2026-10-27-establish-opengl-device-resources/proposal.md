# Proposal

## Why

D02-T01/T02 已交付完整公共 RHI 和可链接的 Common/Validation, 但 DeviceHost 仍只创建 context, 没有实际设备、资源或设备级格式/样本验证. 本 change 按开发计划推进 D02-T03, 建立由正式宿主创建、校验并安全释放基础 GL 资源的路径, 为 T04 上传和 D03 绘制提供真实对象.

## What Changes

- 新增 OpenGL 后端工厂和基础 Device, 由现有 DeviceHost 在 context current、GLAD 加载和环境检查之后创建, 使用同一 Validation 包装. 可见/隐藏宿主共用路径, Device 成功之前不发布 Ready.
- 通过真实 GL 查询建立与创建路径一致的尺寸、格式、样本和 sampler 限制, 支持本 change 明列的基础 Buffer、九种 Texture 维度、独立 Sampler 和单 mip/单层 Framebuffer 附件及必要内部视图. 不从公共枚举或格式 metadata 推导实际支持.
- 明确 Release 的设备/context 归属、范围和能力检查, 原生创建失败回滚、Framebuffer 对附件的保活及 context 线程原生删除. 将 T03 创建资源所必需的 T04 删除协议子集随资源落地; 不据此勾选整个 T04.
- 新增与真实 owner 生命周期一致的 Device 借用入口和宿主关闭预检: 外部设备引用或活资源尚未释放时, 显式关闭报告 ResourceInUse 并保留宿主供释放后重试, 不提前销毁 context.
- 新增实际消费 GLAD 的后端静态目标 PhysaraRHIOpenGL, 通过 Platform 私有接入, 保持 PhysaraRHI 的公共头/Common/Validation 消费者不依赖 GL loader 或 Platform. 保留完整接口、Physara 命名和既有来源说明.
- 用真实宿主上的资源查询、Framebuffer 完整性、错误拒绝、跨线程最终释放和初始化/关闭回归证明本包行为; 未实现的 Device 入口明确失败, 不补假列表、假提交或第二套参数验证服务.

**范围与入口:** 主工作包为 [D02-T03](../../../../Docs/Developments.md#d02-t03), 仅吸收 T04 的必要删除协议, 不交付 T04 上传、T05 完整内容/行序读回、D03 Shader/绑定/Draw/提交/fence 或窗口呈现. 维护者于本次 apply 明确选择“同意限定推进，修订规划后继续”, 允许保留 D01 跨屏缺口, 先实施本 change 的设备、基础资源、必要删除和既定 native 测试. 这是本次独立授权, 不从 T01/T02 的通用层例外推导; D01 4.1/出口、T04 上传、T05 全面读回、D03 和 D01/D02 总出口继续未完成. 原生分配/查询不依赖跨屏缩放, 受影响宿主/输入仍须回归; 呈现和跨屏 UI 不在本例外范围.

## Capabilities

### New Capabilities

- `opengl-device-resources`: 在实际 current context 上创建与宿主共同退出的设备、基于实际限制创建基础资源、区分格式/用途与完整渲染支持、校验对象归属和附件视图、保活依赖、失败回滚及 context 线程删除, 并保留无 native 依赖的通用消费入口.

### Modified Capabilities

- `native-window-host`: 扩充实际宿主的 Ready 条件, 纳入 RHI 创建/Validation 失败; 扩充关闭合同, 在仍有外部 RHI 引用或活资源时报告可重试的拒绝, 其余既有逆序释放、错误保留及重复创建场景继续有效.

已完整核对现有四个主规格及相关场景. rhi-public-contract 和 rhi-common-validation 的完整声明、CPU 所有权、初始策略和无 native 依赖消费要求保持有效, 不为后端接入改写既有要求. 新 capability 描述新增 native 行为, 不把之前的 CPU 证据升级为 GPU 证明.

## Impact

实现落点为 RHI/OpenGL.hpp、RHI/OpenGL 及其 CMakeLists.txt、根 CMake 的实际接入, Platform 的 DeviceHost/私有状态/OpenGLDeviceHost/关闭链和对应 target. PhysaraRHI 通用目标保持原闭包, Runtime 继续消费同一 DeviceHost, 不增加产品入口或直接 GL 路径. 新后端公开边界不包含 GL/GLFW/Windows 类型, RHI 不反向依赖 Platform/Core; Native GL 类型只在后端与 fixture 的互操作边界出现.

本 change 与 [Developments](../../../../Docs/Developments.md#d02-t03) 记录本轮授权及实际实施状态. Architecture 作为既有设计依据保持不变; 实施时更新 Build 和 RHI 来源/能力说明, 不在架构正文记录任务进度. 既有主规格只在后续原生 sync/archive 中合并, 旧 change/归档、ThirdParty 与工具配置保持不变.

验证使用正式宿主、正式后端和已有轻量检查方式, 临时 fixture、输入/输出与检查保存在 build/artifacts/establish-opengl-device-resources/. 不新增 CI、测试框架、工具安装目标或持久第二入口. 任务完成以实际执行证据为准, apply 不执行主规格同步、归档或 Git 提交.
