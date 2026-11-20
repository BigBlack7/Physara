# Spec Delta

## MODIFIED Requirements

### Requirement: Assemble the supported visible draws before graph execution

Forward SHALL 使用已准备的 View、对象与可见 Primitive 集，交付预解析的有序 Draw、批次、资源版本、索引区间、绑定和 Pipeline。支持的 OPAQUE、MASK 和 Unlit 绘制 SHALL 使用实际生效材质。超出本 profile 的可见 Primitive SHALL 有可定位的原因，已有可绘项仍可明确标记降级后合成。仅有不可绘项时返回带原因的不可用结果。构图和执行 SHALL 消费帧内准备值，执行只按本图权限访问资源和录制命令，Pipeline、布局和 Shader SHALL 在执行前就绪。

#### Scenario: Render supported primitives alongside an unsupported route

- **WHEN** 一个可见 mesh 同时包含 opaque、masked Unlit 和 `alphaMode=BLEND` Primitive
- **THEN** 前两项使用各自真实几何及材质参与遮挡，透明项有可见的不支持状态，它的像素和深度均不作为不透明输出

#### Scenario: Reuse fixed preparation after the source scene has advanced

- **WHEN** 一份已封存的帧引用确定的场景快照与几何，CPU owner 随后有其它场景或资产版本可用
- **THEN** 此图的 Draw 只消费原准备代及许可范围，后续准备按后来版本产生自己的 Draw

## ADDED Requirements

### Requirement: Preserve opaque order and surface identity through indexed batches

Forward SHALL 按实际 Pipeline、完整纹理与采样器绑定、顶点及索引页、从近到远的深度和稳定源次序组织不透明 Draw。合批 SHALL 比较全部实际状态、活动资源的完整身份及范围、几何输入布局与页基准、索引格式及实例块。排序键相等、hash 碰撞和较大的材质或几何标识 SHALL 保持完整状态与数据的区别。兼容相邻项 SHALL 保持顺序，引用同一索引序列和基顶点时可合为一个实例命令。兼容且相邻的不同命令 SHALL 可组成固定数量的间接批次。容量边界 SHALL 按原序分段，或在准备阶段报告无法满足的限制。

每个实例 SHALL 在当前 View 的绑定数据范围内关联自身对象与有效材质行，保留变换、采样、alpha 覆盖、Unlit 和镜像/双面行为。索引区间、基顶点、首实例及实例数量 SHALL 对应所用的页与实例块。Depth 与 Color SHALL 消费同一有序表面序列及一致的 alpha 合同，覆盖和深度结果 SHALL 与同一序列逐项直接绘制一致。只有已就绪的实例及参数版本 SHALL 进入实际 Pass。

#### Scenario: Group equal texture bindings while preserving numeric overrides

- **WHEN** 多个相邻可见实例共用几何、同一采样绑定和兼容的完整图形状态，并持有不同对象变换和数值材质，后续相邻命令引用同页另一段几何
- **THEN** 同段几何用同一实例命令，两段命令可在一批消费。原生颜色与深度对应各实例的有效行和正确几何，结果与已知标记及同序直接绘制一致

#### Scenario: Keep distinct states at equal fast keys and wide material indices

- **WHEN** 有序工作集包含超过 256 个不同有效材质标识、相同紧凑排序值或 hash 的不同状态，及不同纹理视图、sampler、几何页或输入布局
- **THEN** 各实例选中完整身份对应的数据，只有实际状态兼容的相邻项合并，索引始终在所绑定范围内，边界分段后的展开顺序保持一致

#### Scenario: Preserve coverage and ordering with or without a depth pass

- **WHEN** 固定相机下有重叠、等深及 masked 表面，分别运行颜色写深度与深度预通过后只读着色，并包含只剩有效背景的裁剪 View
- **THEN** 两种配置按同一序列和覆盖合同得到匹配的颜色、深度和遮挡结果，裁剪 View 保留初始化背景且没有表面命令

#### Scenario: Preserve mip sampling with mixed material branches

- **WHEN** 同一实例命令混合共用几何与采样绑定的 OPAQUE/Unlit 和 MASK/Lit 表面，MASK 三角形内的 alpha 存在保留与丢弃区域
- **THEN** 材质分歧及丢弃边缘的 mip 选择、覆盖、深度和颜色均保持有定义，并与同序逐表面直接绘制的结果一致
