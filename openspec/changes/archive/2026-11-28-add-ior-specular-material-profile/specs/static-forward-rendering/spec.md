# Spec Delta

## ADDED Requirements

### Requirement: Deliver the active IOR and specular opaque profile across render paths

受支持的 lit OPAQUE 或 MASK 材质具有源 IOR/specular 扩展声明或对应的有效实例覆盖时 SHALL 启用 IOR/specular profile。缺省值和常量、纹理参与同一选定 profile，清除全部相关覆盖后按源声明解析。当轮有效材质、真实纹理代和实际受光 profile SHALL 与该 View 的 Draw、批次及已接受输出对应。既有材质预览按各自的基色、法线、材质通道或发射语义交付。

Forward SHALL 用全灯表求扩展颜色。Forward+ 与 Deferred 的 `ForwardExtension` SHALL 用现有 Froxel 候选及保守退路求同一响应，消费实际获准的阴影和环境。Deferred 的扩展表面 SHALL 先参与 `SurfaceAux`，获胜像素交付 D32、G1 的法线及过滤后粗糙度和 G3 的 `0x0004`，核心光照只接受 G3 位 0。扩展颜色 SHALL 复用相同覆盖与胜出次序，按最终 D32 等深读取并写入预曝光 HDR。早期深度与阴影沿用可见几何、双面和基色 alpha 规则。

独立表面及仅深度消费者的就绪范围 SHALL 依据实际读取的数据与角色确定。扩展色的准备等待、拒绝或资源超限 SHALL 沿 View 交付合同给出可定位状态，保留已有可读输出。待匹配的正常提交成功后 SHALL 反映该 View 实际使用的材质与 Shader 生产身份。

#### Scenario: Shade a real masked extension over a core surface
- **WHEN** 一个从标准 glTF 导入的 IOR/specular 前景使用有洞 MASK，遮挡核心材质，另外带有同类的较远表面或有限远面标样，从同一冻结输入请求三条路径的 Direct Lighting 以及 Physical Lighting
- **THEN** 每路的覆盖与 D32 配对。Deferred 中获胜的前向扩展显示 G3=4，孔内核心像素走核心照明。各路使用当前对应的扩展颜色角色，并在登记的取样与渲染精度内交付匹配的工作色

#### Scenario: Deliver the extension surface while its color role is unavailable
- **WHEN** View 请求独立表面，而当前 IOR/specular 颜色包缺失，随后请求需要该颜色的正常图，最后恢复合法包
- **THEN** 独立表面交付匹配的 G3 和深度，正常颜色请求给出具体缺项并保留已有可读结果，匹配资源就绪后由新一次正常提交交付扩展颜色
