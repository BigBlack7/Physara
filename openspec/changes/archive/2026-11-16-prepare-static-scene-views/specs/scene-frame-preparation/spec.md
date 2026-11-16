# Scene Frame Preparation Delta

## Purpose

规定 Renderer 如何把同一 Scene 的一致静态状态提取成帧槽保活的连续只读记录，并让多个 View 的可见工作集共享这些记录及固定版本资产。

## ADDED Requirements

### Requirement: Share one immutable scene frame across views

同一帧引用同一 Scene 的 View SHALL 共享一份 `SceneFrame`。该帧 SHALL 带 Scene 身份及静态内容版本，提供连续的对象、Primitive 和灯记录与帧内索引。对象世界变换、绕序和身份每实体记录一次，Primitive 按对象索引关联该变换，并携带其世界 bounds、显示和投影标志。无有效表面的源槽 SHALL 保留其资产定位语义，且不进入可绘工作集。灯记录 SHALL 保留有效的世界位置或发射方向、线性颜色、标准强度单位、可选范围和锥角。各 View SHALL 持有各自的索引集合，并从同一份不可变记录取得数据。

#### Scenario: Prepare two views from one extracted scene

- **WHEN** 同一已提交 Scene 含有多个 Primitive 的实例以及点光、聚光或方向灯，并为两个相机准备 View
- **THEN** 两个 View 的索引均指向同一 SceneFrame，各自可见集合可不同，每个对象的变换与材质引用只由共享记录提供

#### Scenario: Retain source slots for empty primitives

- **WHEN** 模型网格在一个空表面之后还有带几何的源槽，且其中一个有效对象在 Scene 中关闭可见性
- **THEN** 非空几何仍能定位原模型和源槽，空表面及关闭可见性的对象均不进入该 View 的可绘集合

### Requirement: Resolve static assets for the lifetime of the frame

SceneFrame SHALL 按当前实例实际生效的资产身份与 revision 记录几何、共享材质和纹理。源 Primitive 与材质引用 SHALL 在对应固定模型版本内解释。实例修改过的值 SHALL 由该帧只读记录提供，外置图像 SHALL 保活其确切版本。帧消费者 SHALL 通过索引、固定资产引用及帧内值解析数据，共享同版网格和图像正文。

#### Scenario: Keep frame material resolution tied to the active instance version

- **WHEN** 帧提取两个同源实例，其中一个替换材质因子及外置纹理，然后 AssetManager 发布同路径的更新版本，活动文档之后被替换
- **THEN** 帧存续期间两实例的原引用仍按各自有效值解析，原模型和外置图像可读，新 Scene 的帧按自己的身份和实际资产版本准备

### Requirement: Reuse scene records only within a safely acquired slot

SceneFrame 及其共享资产 SHALL 由当前安全取得的 FrameSlot 支持，借用以该次槽使用为界。该槽已提取的 Scene 身份及静态内容版本吻合时 SHALL 复用对象、Primitive 和灯记录。身份或静态内容版本改变时 SHALL 发布对应的新记录与索引。一个槽中的准备、取消和重置 SHALL 保持其他未回收槽的记录有效。提取失败 SHALL 返回有界原因，准备值只交付完整帧。

#### Scenario: Rotate slots across camera motion and scene replacement

- **WHEN** 两个槽已提取同一 Scene，纯相机移动后其中一个槽先安全复用，随后内容改变或换成 revision 数值相同的新 Scene
- **THEN** 纯相机移动复用安全槽的静态记录但可见索引仍按当前相机准备，静态内容变化或不同 Scene 身份触发新提取，另一槽的已有记录和资产引用仍有效

