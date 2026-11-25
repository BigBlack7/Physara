# shader-generation-reload Spec Delta

## ADDED Requirements

### Requirement: Admit active resources according to the opaque Shader role

产品目录 SHALL 为当前可达的全灯与 Froxel 颜色角色定义有限且可辨认的结构变体。打包冷准备与热重载准入 SHALL 对同一角色使用一致的 CPU 绑定合同，核对生成 ABI、各阶段活动资源、类型、槽位和可交付范围。Froxel 颜色角色 SHALL 具备原有 View 常量和完整灯输入，并在片元阶段具有网格参数、header、有限索引和全局局部灯四个活动资源。全灯颜色、深度、阴影诊断及其余共享角色 SHALL 使用其原有活动绑定配方。当前没有绘制需求的角色也 SHALL 在候选代准入时核对其角色合同。

原生 Pipeline 及布局 SHALL 由实际 View 需求在帧前额度内推进。产品角色及其绑定发生冲突时 SHALL 交付可定位的失败，旧兼容代依照已有的帧与提交所有权继续可用。只有实际需要且具备完整准备内容的角色 SHALL 成为本轮就绪计划的消费者。

同帧离屏重试新增的原生键 SHALL 并入当轮需求去重，保留主路的待办键及其准备原因。同帧补备 SHALL 使用原有剩余额度和当前代，后续预热可接续未完成工作。

当前活动代某角色的程序包因文件缺失、读取超限或包级准入失败而被拒绝时，该角色的所有固定状态键 SHALL 保有同一原因，仍属于本帧实际需求。其它角色 SHALL 在剩余额度内继续准备。资源不足、系统失败及原生设备错误 SHALL 沿用既有中止规则，候选代 SHALL 遵守完整发布条件。

#### Scenario: Prepare the packaged Froxel role on demand

- **WHEN** 无开发源码的会话先运行全灯 Forward，之后请求需要 Froxel 颜色 Pipeline 的正常视图
- **THEN** 全灯图可使用原布局完成。Froxel 角色依照准备额度由其产品包取得正确的活动资源与原生 Pipeline，缺项期间报告 Pending。就绪后该图可绑定四个当前 View 的活动块

#### Scenario: Reject a mismatch in either opaque role before publishing a hot reload

- **WHEN** 候选代中某个 Froxel 角色的活动资源缺失或类型、阶段、槽位错误，或者某个全灯角色意外激活 Froxel 绑定
- **THEN** 准入指出对应角色及资源冲突，即使该角色当前没有被 View 选中。旧兼容代仍可用，修复后的代可按真实需求完成准备并在帧边界发布

#### Scenario: Reject a depth role that requests a set with no layout

- **WHEN** 同 ABI 的深度片元程序在覆盖配方之外激活只有颜色角色能绑定的灯表
- **THEN** 冷包与热重载准入均拒绝该角色并定位资源。当前未请求前置深度时也拒绝候选发布，已有兼容代仍能准备前置深度

#### Scenario: Reject a depth vertex program missing the View transform

- **WHEN** 同 ABI 的深度程序保持覆盖配方，但其顶点阶段未使用必需的 `perView` 变换输入
- **THEN** 冷准备和热重载均定位到该角色的顶点必需项并拒绝新包，已有有效代仍能交付前置深度。深度片元可按原覆盖配方省略该常量

#### Scenario: Reject an opaque color program missing the MASK sampling inputs

- **WHEN** 同 ABI 的普通受光颜色包缺少基色图像或采样器，包含该角色的热重载候选尚未被 View 选中
- **THEN** 冷准备和热重载均定位到颜色角色缺失的材质输入并拒绝新包。保留的合法代及随后启用的修复代均可正确交付 MASK 孔洞后的颜色与深度

#### Scenario: Reject a color vertex program missing its normal-map material input

- **WHEN** 同 ABI 的受光颜色包保留片元材质配方，但失去顶点阶段用来为法线 UV 变换建立正确切线基底的 `materialRows`
- **THEN** 冷准备与未选角色的热重载均报告顶点缺项并拒绝新包。正常颜色仍使用兼容旧代，恢复的合法代继续按材质的法线 UV 变换着色

#### Scenario: Preserve pending primary work during an offscreen retry

- **WHEN** 本轮原生额度已经用完，主路仍等待 Shader 键而归还上传额度，离屏候选重试后可以正常提交
- **THEN** 原生准备快照继续报告主路所欠的角色与预算等待原因。离屏提交后，无新计划的预热可推进主路缺项，同帧新增的创建次数仍受原额度约束

#### Scenario: Prepare the independent offscreen role after a primary package rejection

- **WHEN** 主路的多个固定状态键引用同一个缺失、超限或不兼容的包，并在拒绝后归还批次配额。离屏重试需要尚未准备的兼容角色，且本帧仍有足够原生额度
- **THEN** 主路保留该角色的失败原因及整帧需求。补备跳过已拒角色的其它键，准备离屏缺少的原生对象，使离屏在本轮提交并交付与单独运行一致的输出

#### Scenario: Retry a restored package after the rejected role leaves demand

- **WHEN** 已拒角色退出 View 需求，随后合法包就位，View 重新请求该角色
- **THEN** 准备器读取恢复的包并按原生额度推进。View 只在匹配的正常图提交后更新已接受的输出与路径身份
