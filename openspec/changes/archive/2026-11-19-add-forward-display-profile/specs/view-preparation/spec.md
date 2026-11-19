# Spec Delta

## MODIFIED Requirements

### Requirement: Fit fixed projections to the effective image rectangle

View SHALL 从当前输出像素范围计算居中的有效图像矩形。固定透视 aspect 与正交 `xmag / ymag` SHALL 保持源比例，通过 letterbox 或 pillarbox 适配输出。缺省透视 aspect SHALL 取有效图像的像素宽高比。场景准备 SHALL 分别交付显示画布及其有效矩形、渲染栅格及其有效矩形。固定 aspect 使用同一源比例在两张栅格分别居中适配；整数适配沿用非限制轴向下取整，奇数余量分配在右或下。缺省 aspect 的透视在两张栅格都使用完整画布，投影比例取当前显示有效图像。相机投影及图像方向由同一快照交付，覆盖和深度使用实际渲染矩形；采样偏移及保守范围使用有效渲染像素。连续图像坐标 SHALL 从显示有效矩形映射，矩形外的留边返回无图像坐标。无布局、任一所需尺寸为零或任一有效矩形无法表示非空图像时 SHALL 返回明确的不可准备状态和空工作集。

#### Scenario: Fit a fixed camera to a differently shaped canvas

- **WHEN** 宽高比为 2 的固定相机准备 640×480 输出，面板内容与其图像区域分别用于指针坐标映射
- **THEN** 有效矩形为 `x ∈ [0,640)`、`y ∈ [80,400)`，图像坐标从矩形内映射，上下留边没有有效命中，投影仍按比例 2 求值

#### Scenario: Resume a view after an unusable layout

- **WHEN** View 缺少布局、换算后存在零轴或留边后无法得到有效像素，随后取得有效尺寸
- **THEN** 前者没有可用相机矩阵、图像映射或可见索引，后者从当前像素尺寸和请求重新准备

#### Scenario: Use one fixed projection at two sampling extents

- **WHEN** 宽高比为 2 的固定相机准备 321×241 显示画布和 241×180 渲染栅格
- **THEN** 显示矩形为 `[0,321) × [40,200)`，渲染矩形为 `[0,241) × [30,150)`，相同图像 UV 对应同一投影视线，命中和采样偏移各自使用已声明的矩形
