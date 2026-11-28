# Spec Delta

## ADDED Requirements

### Requirement: Deliver active sheen across the supported opaque render paths

受支持 lit OPAQUE/MASK 材质的有效 sheen 颜色因子非零时，颜色 SHALL 使用匹配的 sheen 受光角色，支持核心或 IOR/specular 基底及同时存在的清漆。有效因子为零时 SHALL 选择对应的基底或清漆响应，既有深度、阴影、表面诊断和实例数据按实际用途交付。

Forward SHALL 通过全灯表求色，Forward+ 与 Deferred ForwardExtension SHALL 通过匹配的 Froxel 候选和保守退路提供相同响应。Deferred SHALL 使用现有四附件和 D32，以相同覆盖及次序在 SurfaceAux 交付获胜者的基底 G1 与 G3=4，之后在匹配 D32 等深合成前向扩展色。几何、双面、MASK、深度和阴影 SHALL 共同沿现有基底覆盖合同。

仅需独立表面或深度的请求 SHALL 消费其实际资源。sheen 正常颜色若因资源不可用而等待或被拒绝，SHALL 提供具体原因并保留旧可读输出资格。接受的颜色与历史 SHALL 对应实际提交的材质、环境及 Shader 代。

#### Scenario: Deliver a masked sheen winner and its base surface across paths

- **WHEN** 真实 glTF 的 sheen MASK 遮挡核心表面，同一视图包含清漆组合、基底 IOR/specular 与等深对照，并以冻结输入分别请求三路径的 Direct 和 Physical Lighting
- **THEN** 配对的 D32 和阴影与实际覆盖相符。Deferred 孔内仍可用核心光照，扩展获胜处交付基底 G1 与 G3=4。三路径在已登记工作格式精度内交付匹配 HDR

#### Scenario: Accept only the outputs whose sheen resources are ready

- **WHEN** 一个 View 只需独立表面，另一次请求需 sheen 受光，但当时缺乏匹配色包或所需环境产物，随后该资源就绪
- **THEN** 独立表面交付对应的 G1、G3 和 D32。受光请求暂停或拒绝并给出原因，恢复后的新正常票据确认实际 sheen 材质、环境和 Shader 代

## MODIFIED Requirements

### Requirement: Deliver the active IOR and specular opaque profile across render paths

受支持的 lit OPAQUE 或 MASK 材质具有源 IOR/specular 扩展声明或对应的有效实例覆盖时 SHALL 启用 IOR/specular 基底。仅使用该扩展基底时 SHALL 选择 IOR/specular 受光角色。具有清漆声明或覆盖、生效的 sheen 或两者组合时 SHALL 在匹配的叠层角色内使用相同的 IOR/specular 响应。缺省值和常量、纹理参与同一选定 profile，清除全部相关覆盖后按源声明解析。当轮有效材质、真实纹理代和实际受光 profile SHALL 与该 View 的 Draw、批次及已接受输出对应。既有材质预览按各自的基色、法线、材质通道或发射语义交付。

Forward SHALL 用全灯表求扩展颜色。Forward+ 与 Deferred 的 `ForwardExtension` SHALL 用现有 Froxel 候选及保守退路求同一响应，消费实际获准的阴影和环境。Deferred 的扩展表面 SHALL 先参与 `SurfaceAux`，获胜像素交付 D32、G1 的法线及过滤后粗糙度和 G3 的 `0x0004`，核心光照只接受 G3 位 0。扩展颜色 SHALL 复用相同覆盖与胜出次序，按最终 D32 等深读取并写入预曝光 HDR。早期深度与阴影沿用可见几何、双面和基色 alpha 规则。

独立表面及仅深度消费者的就绪范围 SHALL 依据实际读取的数据与角色确定。扩展色的准备等待、拒绝或资源超限 SHALL 沿 View 交付合同给出可定位状态，保留已有可读输出。待匹配的正常提交成功后 SHALL 反映该 View 实际使用的材质与 Shader 生产身份。

#### Scenario: Shade a real masked extension over a core surface
- **WHEN** 一个从标准 glTF 导入的 IOR/specular 前景使用有洞 MASK，遮挡核心材质，另外带有同类的较远表面或有限远面标样，从同一冻结输入请求三条路径的 Direct Lighting 以及 Physical Lighting
- **THEN** 每路的覆盖与 D32 配对。Deferred 中获胜的前向扩展显示 G3=4，孔内核心像素走核心照明。各路使用当前对应的扩展颜色角色，并在登记的取样与渲染精度内交付匹配的工作色

#### Scenario: Deliver the extension surface while its color role is unavailable
- **WHEN** View 请求独立表面，而当前 IOR/specular 颜色包缺失，随后请求需要该颜色的正常图，最后恢复合法包
- **THEN** 独立表面交付匹配的 G3 和深度，正常颜色请求给出具体缺项并保留已有可读结果，匹配资源就绪后由新一次正常提交交付扩展颜色

### Requirement: Deliver clearcoat across the supported opaque render paths

受支持的 lit OPAQUE/MASK 材质具有源清漆声明或对应有效实例覆盖时 SHALL 选择匹配的清漆受光角色，并使用其冻结的核心或 IOR/specular 基底。sheen 同时生效时 SHALL 选择同时交付两个波瓣的角色。强度、粗糙度、贴图和零强度响应 SHALL 由当前材质值确定。清除全部清漆覆盖后，清漆 SHALL 按源声明求值，路由由当时的有效材质和其它生效层共同确定。基色、法线、材质通道及自发光诊断 SHALL 沿各自的基底采样语义交付。

Forward SHALL 通过全灯表求清漆受光，Forward+ 及 Deferred 的 `ForwardExtension` SHALL 使用匹配的 Froxel 候选和保守退路求同一响应。Deferred SHALL 以现有四附件及 D32 交付表面，清漆的可见获胜像素在 SurfaceAux 交付 D32、基底 G1 法线与过滤粗糙度以及 G3 的 `0x0004`。其颜色 SHALL 使用同一冻结的覆盖和胜出次序并按最终深度等深合成。前置深度、阴影 caster、双面和 MASK SHALL 使用当前几何和基色 alpha 合同。

独立表面、仅深度和诊断 SHALL 按实际角色读取基底表面或覆盖资源。正常受光提交的清漆纹理、切线和 Shader 生产身份 SHALL 来自该 View 已准备的同一有效材质和 Shader 代。清漆颜色角色等待或被拒绝时 SHALL 报告对应原因并保留已有可读输出。

#### Scenario: Match the coated MASK surface and lighting across paths
- **WHEN** 真实导入的清漆 MASK 盖在核心表面上，固定可见几何中含 IOR/specular 基底的清漆与等深对照，并从同一输入请求三路径的 Direct Lighting 和 Physical Lighting
- **THEN** 三路的覆盖和 D32 匹配。Deferred 孔内保留核心资格，清漆胜出处交付基底 G1 与 G3=4，并在对应 Shader 身份和工作附件精度内交付匹配 HDR

#### Scenario: Deliver the independent surface while clearcoat color is unavailable
- **WHEN** 当前清漆色包或仅供清漆颜色的资源无法准备，View 先请求独立表面，再请求受光输出，之后提供兼容资源
- **THEN** 独立表面按可见覆盖交付配对的 G1、G3 和 D32。受光图报告清漆颜色的缺项并保留旧可读结果，恢复后以匹配的新正常票据交付实际清漆颜色
