# Spec Delta

## ADDED Requirements

### Requirement: Author and inspect anisotropy on a compatible static instance

Inspector SHALL 为兼容 lit OPAQUE/MASK Primitive 提供 [0,1] 强度、弧度旋转和线性 RGB 方向纹理槽的继承值与稀疏编辑。纹理槽 SHALL 复用项目图像、sampler、有效 UV 和 UV 变换编辑。显式零强度 SHALL 可区别于继承，清除单项 SHALL 恢复对应源值。正常提交、清除、撤销及重做 SHALL 通过同一有效材质边界校验，包括基底 normal 有效 UV 或框架来源发生改变的情况。超出支持的材质组合或无法交付所需切线空间的请求 SHALL 明确拒绝，原实例状态、历史和可用帧保有资格。

Inspector SHALL 区分固定源的支持或降级说明、当前请求的有效因子、颜色等待或拒绝，以及该 View 对应实例和 Primitive 最后一次实际接受的各向异性受光。仅有表面或诊断输出时 SHALL 保留最后有效颜色身份。有效内容改变 SHALL 沿 Scene 版本合同失效不兼容的 View 历史。

#### Scenario: Set explicit zero, restore inheritance and replay the instance edit

- **WHEN** 两个实例固定同一模型，用户为一个目标改变旋转和方向槽，提交显式零强度，再清除强度覆盖、撤销及重做
- **THEN** 目标实例的继承/覆盖和后来接受的颜色与当前有效材质相符，相关有效内容变化失效不兼容历史。另一实例和源记录继续按各自的固定值使用

#### Scenario: Reject incompatible frame edits and report actual color delivery

- **WHEN** 已正常交付各向异性颜色的实例申请不可用的基底 normal UV 或混层编辑，随后 View 交付表面诊断或等待颜色资源
- **THEN** 非法编辑显示目标及原因并保有有效 Scene 状态。诊断或等待状态与最后正常接受的实际受光身份均可辨认
