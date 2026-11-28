# Spec Delta

## MODIFIED Requirements

### Requirement: Deliver the active IOR and specular opaque profile across render paths

受支持的 lit OPAQUE 或 MASK 材质具有源 IOR/specular 扩展声明或对应的有效实例覆盖时 SHALL 启用 IOR/specular 基底。仅使用该扩展基底时 SHALL 选择 IOR/specular 受光角色。具有清漆声明或覆盖、生效的 sheen 或两者组合时 SHALL 在匹配的叠层角色内使用相同的 IOR/specular 响应。启用各向异性基底时 SHALL 在该各向异性颜色角色内使用相同的 IOR/specular Fresnel、漫反射预算与金属语义。缺省值和常量、纹理参与同一选定 profile，清除全部相关覆盖后按源声明解析。当轮有效材质、真实纹理代和实际受光 profile SHALL 与该 View 的 Draw、批次及已接受输出对应。既有材质预览按各自的基色、法线、材质通道或发射语义交付。

Forward SHALL 用全灯表求扩展颜色。Forward+ 与 Deferred 的 `ForwardExtension` SHALL 用现有 Froxel 候选及保守退路求同一响应，消费实际获准的阴影和环境。Deferred 的扩展表面 SHALL 先参与 `SurfaceAux`，获胜像素交付 D32、G1 的法线及过滤后粗糙度和 G3 的 `0x0004`，核心光照只接受 G3 位 0。扩展颜色 SHALL 复用相同覆盖与胜出次序，按最终 D32 等深读取并写入预曝光 HDR。早期深度与阴影沿用可见几何、双面和基色 alpha 规则。

独立表面及仅深度消费者的就绪范围 SHALL 依据实际读取的数据与角色确定。扩展色的准备等待、拒绝或资源超限 SHALL 沿 View 交付合同给出可定位状态，保留已有可读输出。待匹配的正常提交成功后 SHALL 反映该 View 实际使用的材质与 Shader 生产身份。

#### Scenario: Shade a real masked extension over a core surface
- **WHEN** 一个从标准 glTF 导入的 IOR/specular 前景使用有洞 MASK，遮挡核心材质，另外带有同类的较远表面或有限远面标样，从同一冻结输入请求三条路径的 Direct Lighting 以及 Physical Lighting
- **THEN** 每路的覆盖与 D32 配对。Deferred 中获胜的前向扩展显示 G3=4，孔内核心像素走核心照明。各路使用当前对应的扩展颜色角色，并在登记的取样与渲染精度内交付匹配的工作色

#### Scenario: Deliver the extension surface while its color role is unavailable
- **WHEN** View 请求独立表面，而当前 IOR/specular 颜色包缺失，随后请求需要该颜色的正常图，最后恢复合法包
- **THEN** 独立表面交付匹配的 G3 和深度，正常颜色请求给出具体缺项并保留已有可读结果，匹配资源就绪后由新一次正常提交交付扩展颜色

## ADDED Requirements

### Requirement: Deliver the active anisotropy profile on the three opaque paths

受支持 lit OPAQUE/MASK 的有效材质强度大于零时 SHALL 选择各向异性颜色角色，按实际参数采样切线方向和每纹素强度。显式零强度或继承零值 SHALL 使用对应的等向材质角色。相同模型的实例 SHALL 使用各自有效材质及框架。同一帧的 Forward 使用全灯，Forward+ 与 Deferred ForwardExtension 使用匹配的 Froxel 候选及保守退路，三者 SHALL 消费等价的直接光、环境和阴影输入。

Deferred SHALL 通过现有 SurfaceAux 交付获胜表面的基底 G1、G3=4 与 Reverse-Z D32，在相同覆盖及次序下以等深合成颜色。孔内核心像素保持 G3 位 0 的照明资格。Depth、shadow caster、表面预览及基底材质诊断 SHALL 继续按实际用途使用相同覆盖和几何。仅独立表面或深度的请求 SHALL 在颜色所需的方向图、额外切线输入或颜色角色未就绪时仍可取得自己的结果。

受光色必需的包、纹理或环境未就绪时，View SHALL 给出当前原因并保有可用旧结果。只有实际正常提交 SHALL 确认该目标的颜色身份和 Shader 代。仅含普通等向基底的工作集 SHALL 沿等向角色和资源需求准备。

#### Scenario: Match the visible anisotropic and base surfaces under the same light

- **WHEN** 从同一冻结输入请求三条路径的 Direct 和 Physical，使用可见的各向异性 MASK、孔内后方核心对照及等深标样
- **THEN** D32、阴影及可见次序一致，可见扩展处在 Deferred 取得基底 G1 和 G3=4，孔内核心仍能受光。工作色在已登记的格式精度内配对

#### Scenario: Deliver only independently ready surface and color outputs

- **WHEN** 一个需要各向异性颜色的 View 当时缺所需色包或颜色专用输入，另一次请求只需要基底表面，随后完整资源可用
- **THEN** 基底表面继续交付匹配的 G1/G3/D32，颜色请求报告可定位原因并保有旧可读输出。合法恢复由匹配的新正常票据确认
