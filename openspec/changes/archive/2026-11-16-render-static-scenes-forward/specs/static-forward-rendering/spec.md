# Static Forward Rendering Delta

## Purpose

规定首个真实静态场景 Forward 子图如何消费已准备的每 View 工作集，交付反向深度、标准 Unlit 和带明确语义的材质预览。

## ADDED Requirements

### Requirement: Assemble the supported visible draws before graph execution

Forward SHALL 使用已准备的 View、对象与可见 Primitive 集，交付预解析的 Draw、资源版本、索引区间、绑定和 Pipeline。支持的 OPAQUE、MASK 和 Unlit 绘制 SHALL 使用实际生效材质。超出本 profile 的可见 Primitive SHALL 有可定位的原因，已有可绘项仍可明确标记降级后合成；仅有不可绘项时返回带原因的不可用结果。构图或执行消费帧内准备值，执行只按本图权限访问资源和记录命令。

#### Scenario: Render supported primitives alongside an unsupported route

- **WHEN** 一个可见 mesh 同时包含 opaque、masked Unlit 和 `alphaMode=BLEND` Primitive
- **THEN** 前两项使用各自真实几何及材质参与遮挡，透明项有可见的不支持状态，它的像素和深度均不作为不透明输出

#### Scenario: Reuse fixed preparation after the source scene has advanced

- **WHEN** 一份已封存的帧引用确定的场景快照与几何，CPU owner 随后有其它场景或资产版本可用
- **THEN** 此图的 Draw 只消费原准备代及许可范围，后续准备按后来版本产生自己的 Draw

### Requirement: Produce forward depth and color for the effective image rectangle

Forward SHALL 提供按真实需求组装的表面准备和不透明着色入口。前置深度被消费时 SHALL 产生可验证的真实深度，颜色消费其声明的同一版本；其它组合由颜色绘制同时产生深度。单样本主视图 SHALL 使用 Reverse-Z，整张颜色和深度在绘制前初始化，相机输出只落在当帧有效图像矩形。远近遮挡及同深度的确定顺序 SHALL 在两个组合下保持一致。

#### Scenario: Show occlusion and padding in a fixed-aspect view

- **WHEN** 非对称的近远 Primitive 投影相交，固定比例相机的有效图像比离屏画布小
- **THEN** 在重叠处近几何占据反向深度和颜色，图像外保持已清除的背景，图代表本轮实际 View 布局

#### Scenario: Render initialized background after valid frustum culling

- **WHEN** 场景有可绘静态几何且准备成功，本轮相机使所有可绘项离开视锥
- **THEN** Forward 交付该帧的已初始化背景和清除深度，不把上一帧的几何留在当前画面

### Requirement: Display an explicit unlit and material preview profile

Forward 预览 SHALL 提供显式的 Base Color、World Normal、Material Channels 和 Emissive 模式，报告其当前预览状态。Unlit 在各模式使用标准基色，其余已支持表面根据模式显示基色、世界法线映射、`R=AO/G=roughness/B=metallic` 或线性自发光。工作颜色 SHALL 保持有限的线性值，保留附件可表示的超白值。View 输出 SHALL 将预览色线性限幅至 [0,1] 并提供 display-linear 采样语义，UI 沿现有合成与窗口编码合同消费。物理照明和显示 profile 的可用性 SHALL 如实报告。

#### Scenario: Change material preview without rebuilding static storage

- **WHEN** 用户在同一已加载场景的现有视口切换到法线、材质通道或自发光预览
- **THEN** 当前匹配帧显示对应诊断值与模式标识，Unlit 保持基色，已驻留且解释相同的模型、纹理与实例行可复用

#### Scenario: Preserve an emissive value above display white in the working output

- **WHEN** 非 Unlit 表面在 Emissive 预览中产生附件可表示的线性自发光值大于 1
- **THEN** 工作颜色保留该值，视口采样得到固定映射后的显示线性颜色，颜色转换和窗口编码各在自己的边界生效
