# Proposal

## Why

D02-T03/T04 已交付基础资源和初始上传, D02-T05 仍缺少对同一正式 DeviceHost/Validation/OpenGL 实现的完整资源 profile 验收. 下一步应补齐能力、分配、深度/模板/MS 存储内容、失败和退出的一致性证据, 为 D02 收口提供依据, 同时明确 D03 绘制和 D08 公共读回各自的边界.

## What Changes

- 覆盖 [D02-T05](../../../../Docs/Developments.md#d02-t05) 四项既定目标: 复用正式宿主的 native fixture、记录并核对资源 profile、验证失败/释放、通过同一 Validation 检查实际 caps/格式/样本和资源归属.
- 在既有 Buffer 与 13 种颜色格式/7 种非 MS 维度上传读回基础上, 补齐 17 种映射存储格式/9 种 target 的实际支持矩阵. 对支持的深度/模板附件及 MS/MS-array, 由 fixture 原生初始化和必要 resolve 后检查内容; 明确区分资源存储证据与产品命令能力.
- 串起 raw/Validation 的合法结果、原生分配、非法请求零写入、跨设备拒绝和实际回收证据. 受控资源不足、Pending、错误和 context 不可用检查与自然 GPU 完成分别记录, 不主动耗尽机器显存.
- 复用当前列表、提交、删除和 Host 关闭实现. 本包默认只增加/整理验证代码与对应文档; 若发现现有契约的实现缺陷, 先记录可复现失败、在 design 写明最小修正及回归后再改生产代码. 产品行为需要改变时, 先修订本 change 并移除 `skip_specs`, 不能凭测试包扩大支持范围.
- 本轮矩阵发现并修复 3D 深度/模板 caps 盲信驱动查询的问题: 按规范过滤非法组合, 在原生分配前拒绝, 保持设备可用. 失败复现和最小方案见 design 决策 11; 辅助原生状态及本机 UPPER_LEFT 观测见决策 12.
- 澄清开发计划中 T05 与 D03/D08 的验收分工. 实施按 tasks 逐组记录实际证据, 规划完成不计作 GPU 验收.

**实施入口:** D02-T03/T04 已提交, D01 的真实跨屏验收和阶段出口仍未完成. 2026-10-29 维护者明确批准本 change 独立限定例外: 先执行既定 T05 资源一致性验证及恢复既有合同的最小修正. 原因是本包资源 profile 不消费跨屏比例, 受影响的 Host/输入仍须回归. 旧 T03/T04 例外不被继承; Shader/Draw、公共传输与呈现及 D01/D02 总出口继续未完成. 仅本包全部验收通过后完成 T05, 阶段出口仍另外检查 D01 及全部前置.

**非目标:** 扩大资源/上传 profile、产品 clear/copy/resolve、公共 staging/map/readback 或 Event/TimerQuery、Shader ABI/编译/绑定/Pipeline/Draw/Dispatch、全 shader-write barrier、Renderer/RenderGraph、窗口呈现、跨屏 UI、性能结论、Vulkan 或新的产品入口. 对应能力继续归 D03、D08 和各自工作包.

## Capabilities

### New Capabilities

无. 本包为既有资源行为补充验证, 不建立与现有主规格重叠的能力目录.

### Modified Capabilities

无. `.openspec.yaml` 使用官方 `skip_specs: true`, 因为本包不改变可观察产品合同. 验证依据为已有 [opengl-device-resources](../../../specs/opengl-device-resources/spec.md)、[opengl-resource-uploads](../../../specs/opengl-resource-uploads/spec.md)、[rhi-common-validation](../../../specs/rhi-common-validation/spec.md) 和 [native-window-host](../../../specs/native-window-host/spec.md); design 映射具体要求、方法和缺口. 当前不创建 delta specs, 不同步或改写主规格.

## Impact

- 验证复用 `PhysaraPlatform -> PhysaraRHIOpenGL -> PhysaraRHI`, 原生读写仅限 fixture/既有后端私有边界. 不新增公开 native export、全局资源 registry、caps 权威副本或第二套后端/列表.
- 验证源码、构建输出和原始日志按既有方式位于忽略的 `build/artifacts/establish-opengl-resource-conformance/`; 可追踪结论和复现入口写入本 change tasks 与开发计划, 不新增测试框架、CI、格式检查目标或流程外必交文档.
- 实施按实际影响更新 `Docs/Build.md` 的本地复现入口、`RHI/README.md` 的 profile/来源说明及 `Docs/Developments.md` 对应记录. 生产修正仅位于 `RHI/OpenGL/Formats.cpp` 的非法 target 过滤, 未改变公开接口或正式构建依赖; 架构、主规格和历史归档保持原样.
- 检查 Windows x64/MSVC Debug/Release 的正式消费者及受影响回归, 记录当前 commit、源码/fixture 标识、设备/驱动和未覆盖平台. 既有通过记录是输入依据, 不冒充本包重新运行的结果.
