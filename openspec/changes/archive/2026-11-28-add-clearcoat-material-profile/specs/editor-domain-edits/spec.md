# Spec Delta

## MODIFIED Requirements

### Requirement: Edit surface fields on the selected model instance

Inspector SHALL 按选中实体的固定源版本列出可编辑的源 Primitive 槽，并区分继承值与实例覆盖。编辑 SHALL 支持线性 base color RGBA、metallic、roughness、normal scale、occlusion strength、线性 emissive 和 emissive strength，以及当前表面 profile 的 alpha 和 double-sided 字段。对于使用 metallic-roughness 的 Primitive，Inspector SHALL 提供 IOR、specular 强度和线性 RGB 颜色，以及 specular 强度与颜色槽。每个被支持的纹理槽 SHALL 可选用 `Assets/Textures` 下的项目图像并编辑适用的 sampler、已有 UV 集和 UV 变换，色彩和通道解释 SHALL 由槽用途决定。Inspector SHALL 允许清除单个材质字段或纹理槽的覆盖并显示继承后的有效值。已导入材质的预览限制 SHALL 与可创作字段分开显示。

离散提交、清除、Undo 和 Redo SHALL 通过同一 Scene 编辑边界更新目标实例的稀疏覆盖。请求 SHALL 绑定文档、实体代、固定模型版本及源 Primitive 和字段或纹理槽。其它字段、其它实例与共享 glTF/GLB SHALL 保持原值。字段或采样组合无效时 SHALL 报告被拒绝的目标及原因。

从源 glTF 继承 IOR=0 的材质 SHALL 显示兼容模式，其源 IOR 字段为只读，specular 强度、颜色和纹理可按标准编辑。普通实例 IOR 覆盖 SHALL 使用有限可表示的 `ior >= 1`。声明或编辑扩展字段之后，界面 SHALL 可辨认继承或覆盖、当前准备状态及最后正常接受的着色结果。改变有效材质的编辑 SHALL 沿静态内容版本合同使相关 View 的不兼容历史失效。

Inspector SHALL 为当前 lit metallic-roughness 实例提供 clearcoat 强度、感知粗糙度和独立清漆 normal scale，以及强度、粗糙度、清漆法线三个槽的继承值和稀疏编辑。提交、清除和恢复 SHALL 使用同一个有效材质校验，按目标 Primitive 检查槽用途、UV、切线空间、因子与材质组合。清漆与基底法线的字段和纹理 SHALL 可独立编辑。已声明或覆盖的清漆 SHALL 在 Inspector 中显示对应实际准备状态及最后正常接受的颜色身份。

#### Scenario: Edit and reset one inherited surface field
- **WHEN** 同一模型被实例化两次，用户在其中一个 Primitive 上写入 roughness 覆盖、清除该字段，然后撤销清除
- **THEN** 该实例后续被接受的帧先使用覆盖，再使用源 roughness，最后恢复覆盖；该 Primitive 的其它覆盖、另一实例及共享源保持原值

#### Scenario: Bind a valid texture to its slot and reject an incompatible edit
- **WHEN** 用户将项目纹理用于支持该采样组合的 base color 槽，然后为该槽提交目标 Primitive 缺失的 UV 集
- **THEN** 成功项以 sRGB 颜色语义采样，拒绝项指出有效 UV 的限制，已接受纹理、Scene 版本和历史保持有效。线性数据槽沿自身的用途解释图像

#### Scenario: Edit, clear and undo one instance of an extended material
- **WHEN** 两个实例共用同一固定源，用户修改一个实例的 specular 颜色和图像槽，清除覆盖，再撤销清除
- **THEN** 该实例在随后各次正常接受的帧使用当前有效参数与纹理解释，各次有效内容变化按 Scene 合同失效相关历史，另一实例和源模型仍按各自数据渲染

#### Scenario: Inspect compatibility IOR and reject an invalid override
- **WHEN** 一个源材质使用 IOR=0，随后通过实例编辑尝试改变其 IOR，或者在普通实例上提交显式零 IOR
- **THEN** 兼容材质显示源模式，越过规则的请求交付定位到字段的原因，现有实例覆盖和有效渲染版本保有原来的资格

#### Scenario: Apply and clear clearcoat on one instance with an existing base profile
- **WHEN** 两个实例共享固定的无清漆源，用户在一个实例上设置清漆因子和贴图，清除覆盖，随后 Undo 和 Redo
- **THEN** 该实例的颜色和路由跟随清漆或继承的基底，各次有效内容变更使相关 View 的不兼容历史失效，另一个实例和共享源按自己的材质渲染

#### Scenario: Retain the current clearcoat edit when the target cannot use its normal slot
- **WHEN** 用户为现有 lit 实例提交使用不可用 UV 或无法取得切线框架的清漆法线覆盖
- **THEN** 面板定位被拒绝的槽和原因，原来接受的稀疏覆盖、场景修订、历史及可读帧保有资格
