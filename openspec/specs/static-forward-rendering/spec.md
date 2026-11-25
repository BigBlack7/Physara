# static-forward-rendering Specification

## Purpose

规定首个真实静态场景 Forward 子图如何消费已准备的每 View 工作集，交付反向深度、标准 Unlit 和带明确语义的材质预览。

## Requirements

### Requirement: Assemble the supported visible draws before graph execution

Forward SHALL 使用已准备的 View、对象与可见 Primitive 集，交付预解析的有序 Draw、批次、资源版本、索引区间、绑定和 Pipeline。支持的 OPAQUE、MASK 和 Unlit 绘制 SHALL 使用实际生效材质。超出本 profile 的可见 Primitive SHALL 有可定位的原因，已有可绘项仍可明确标记降级后合成。仅有不可绘项时返回带原因的不可用结果。构图和执行 SHALL 消费帧内准备值，执行只按本图权限访问资源和录制命令，Pipeline、布局和 Shader SHALL 在执行前就绪。

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

Forward 预览 SHALL 在现有 View Settings 提供 Base Color、World Normal、Material Channels、Emissive、Direct Lighting 和 Physical Lighting 模式，初始模式为 Base Color。Unlit 在各模式使用标准基色。其余已支持表面在四种诊断模式显示基色、世界法线映射、`R=AO/G=roughness/B=metallic` 或线性自发光，在 Direct Lighting 显示直接光结果。Physical Lighting SHALL 使用同一表面模型、完整直接光、DFG 与自发光，并按实际环境用途加入 IBL 和天空。场景省略环境时，Physical Lighting SHALL 交付带 DFG 补偿的直接光及自发光。工作颜色 SHALL 保持有限的线性值并保留附件可表示的超白值。Physical Lighting SHALL 由固定 ACESToneMapper/ColorGrading profile 得到 display-linear 颜色，包括该视图内 Unlit 表面所交付的基色。Base Color、World Normal、Material Channels、Emissive 与 Direct Lighting 仍 SHALL 用线性限幅 [0,1] 定义诊断显示颜色，诊断值的含义沿用其工作色合同。ViewOutput SHALL 提供 display-linear 采样语义，UI 沿现有合成与窗口编码合同消费。界面 SHALL 如实报告当前请求、正常图已接受模式、物理照明范围及显示 profile 的可用性。模式切换沿现有 View 运行请求处理，已有 v1 场景 SHALL 可直接恢复。

#### Scenario: Change material preview without rebuilding static storage

- **WHEN** 用户在同一已加载场景的现有视口切换到法线、材质通道或自发光预览
- **THEN** 当前匹配帧显示对应诊断值与模式标识，Unlit 保持基色，已驻留且解释相同的模型、纹理与实例行可复用

#### Scenario: Preserve an emissive value above display white in the working output

- **WHEN** 非 Unlit 表面在 Emissive 预览中产生附件可表示的线性自发光值大于 1
- **THEN** 工作颜色保留该值，视口采样得到固定映射后的显示线性颜色，颜色转换和窗口编码各在自己的边界生效

#### Scenario: Select Direct Lighting from the existing viewport

- **WHEN** 用户打开含灯和已支持表面的场景，从 Base Color 请求 Direct Lighting，随后切回诊断模式
- **THEN** 合法请求先在面板可见，正常图实际采样并接受后确认匹配的模式和色值，暂时不可用时保留旧输出资格，诊断颜色与 Unlit 的标准基色仍可得到

#### Scenario: Shade a scene without an environment in Physical Lighting
- **WHEN** 现有 v1 场景无环境请求，用户选择 Physical Lighting，并使用已支持的受光几何
- **THEN** 正常帧交付带核心 DFG 补偿的直接光和预曝光自发光，视口报告场景未配置环境，遮挡及有效图像矩形符合当轮 View

### Requirement: Shade lit surfaces in the current pre-exposed working domain

Direct Lighting 的受光 OPAQUE 和 MASK SHALL 通过共享表面求值及核心 BRDF 消费完整的当帧直接光。覆盖、变换和背面法线沿用静态表面合同。透视观察向量 SHALL 取片元朝眼点的方向，正交 SHALL 使用固定反视线方向。几何粗糙度 SHALL 在覆盖分歧前取得有效导数，其结果供本片元所有灯使用。直接光各项 SHALL 乘一次入射余弦并线性相加，AO 不衰减直接光或自发光。直接光与线性自发光 SHALL 各乘一次同轮 `preExposure`。无灯时，非发光受光表面直接输出零光照，自发光仍按当帧曝光显示。

#### Scenario: Accumulate direct light and emission at the current exposure

- **WHEN** 固定可见表面和相机几何，先分别测量方向及局部灯，再同时启用并增加一档曝光补偿，最后关闭所有灯
- **THEN** 直接光符合各灯线性叠加，直接光和自发光在工作色可表示的范围内同时随 +1 EV 翻倍，关闭所有灯后罩内像素只保留预曝光自发光或零

#### Scenario: Preserve masked coverage in the two opaque pass arrangements

- **WHEN** 带有遮罩孔和临界 alpha 的受光表面分别使用合并颜色深度和前置深度组合，并从明确投影类型的相机观察
- **THEN** 两种组合得到相同覆盖和反向深度，留下的受光片元使用符合该投影的观察向量、同一表面过滤和直接光响应

### Requirement: Composite the declared sky over uncovered pixels

Physical Lighting SHALL 在有效图像矩形内，以该 View 的渲染朝向及投影查询有效天空资源。只启用天空时，存在有效相机且场景没有几何也 SHALL 交付天空视口。天空 SHALL 仅覆盖未被现有不透明及 MASK 绘制占据的位置，保留几何工作色、场景深度和矩形外底色。合成结果 SHALL 作为预曝光 HDR 版本交给现有显示消费者。

#### Scenario: Fill a mask hole and preserve a far-plane surface
- **WHEN** 显示具有非对称天空、MASK 孔洞和恰好写入 Reverse-Z 深度 0 的有限远平面几何的场景
- **THEN** 天空只出现在背景和未被几何占据的孔内，深度为 0 的有效几何仍保有自己的工作色，两种不透明 Pass 组合的覆盖、深度和画幅结果相符

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

### Requirement: Display a per-view directional cascade diagnostic

Forward SHALL 在正式 ViewOutput 链提供方向光级联诊断。主 View SHALL 由现有 Renderer 面板提交会话开关，离屏 View SHALL 使用其所属 View 的独立请求。有效诊断 SHALL 使用匹配于当前帧场景版本的已定义阴影层和查询数据，通过颜色显示级联及混合权重，通过明暗显示实际可见性，并沿线性诊断显示配置交付。关闭诊断 SHALL 恢复该 View 请求的显示模式。场景文档 SHALL 继续通过 v1 合同读写。

面板 SHALL 可区分当前请求、已接受诊断及不可用原因。阴影禁用或无有效分配时 SHALL 允许可用的普通 Forward 继续出图。未获接受的诊断候选 SHALL 沿现有视口失败退路处理。只有正常图实际消费并接受的诊断结果 SHALL 被标为本轮有效。

#### Scenario: Observe an accepted cascade diagnostic in the current viewport

- **WHEN** 有效 View 开启方向光级联诊断，场景存在遮挡物、可见接收区和跨越级联边界的覆盖，随后关闭诊断
- **THEN** 正常接受后的诊断颜色可追溯到所查级联和实际遮挡，过渡权重在带内变化。关闭后下一次接受的输出按当前请求模式显示。面板可分别核对两次已接受的输出

#### Scenario: Retain a usable viewport when the diagnostic is unavailable

- **WHEN** 当前 View 请求级联诊断，但关闭了阴影规划、未准入级联，或新的诊断候选未获接受
- **THEN** 可用的普通模式或上次有效 ViewOutput 通过原有路径显示，面板提供对应原因。当前完全接受的诊断仍可在设置和资源有效时发布

### Requirement: Display local shadow visibility for the selected light

Forward SHALL 通过正式 ViewOutput 交付当前 View 所选聚光或点光的线性阴影诊断。诊断 SHALL 使用当前接受场景对应的有效深度组和查询数据，以明暗显示实际可见性，点光另以颜色标识所查面，覆盖外用中性显示。主 View SHALL 可从现有 Renderer 面板提交选择及结束诊断，离屏 View SHALL 接受所属 View 的独立请求。结束诊断后 SHALL 按该 View 原请求的普通模式显示，方向光级联诊断 SHALL 保持可选。

面板 SHALL 区分当前所选灯、计划质量与预算原因，及最近一次正常接受的诊断和所属票据。当前无有效分配或诊断不可用时 SHALL 可按可用的普通模式出图；录制或提交失败时 SHALL 沿既有视口退路显示。仅当本轮正常图实际采样并完整接受诊断时，其结果 SHALL 被标为当前有效。普通受光模式 SHALL 按原有完整灯输入求值。

#### Scenario: Show accepted spot and point visibility in the viewport

- **WHEN** 用户从 Renderer 面板先选择一个有实际遮挡的聚光，再选择一个有跨面接收区的点光，最后结束诊断
- **THEN** 各次正常接受的视口颜色反映对应灯的遮挡及点光面，面板显示匹配的灯和接受状态，退出后恢复之前请求的普通画面

#### Scenario: Explain a budget rejection without losing the light or the viewport

- **WHEN** 一个 View 请求点光诊断，但在主方向光和聚光取得区域后剩余预算不足六层，随后调整阴影设置使其全组准入
- **THEN** 拒绝时面板显示当前灯的预算原因，普通受光输出仍包含该点光。若新诊断候选失败，上次接受画面及其票据仍可辨认。当前完整组实际获接受后才显示有效的六面查询

### Requirement: Consume shared shadow visibility in the current forward output

普通 Direct Lighting 与 Physical Lighting SHALL 在写预曝光线性 HDR 时通过同一直接光查询消费当前 View 的阴影可见性。级联、聚光和点光的线性诊断 SHALL 使用相同的覆盖、偏移和过滤查询，并沿用各自的颜色和显示合同。禁用阴影或无可用阴影分配时，普通受光输出 SHALL 使用完整直接光及中性可见性。非受光预览、Unlit、背景合成、自发光和环境贡献 SHALL 保留其各自输出合同。实际准备、录制或提交失败 SHALL 沿用现有失败退路与已接受输出合同。

#### Scenario: Observe a shadow in ordinary HDR and its matching diagnostic

- **WHEN** 以相同场景、相机和曝光读取阴影关闭及开启的受光 HDR，并从同一 View 请求遮挡灯的诊断
- **THEN** 两种受光模式的变化可由该灯的可见性和原直接贡献解释。稳定无遮挡样点、Unlit 和同输入的环境及自发光贡献保持。诊断明暗与受光使用的同一过滤选择及可见性相符。
