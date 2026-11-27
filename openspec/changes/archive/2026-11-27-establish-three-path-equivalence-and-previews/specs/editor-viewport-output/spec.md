# Editor Viewport Output Spec Delta

## ADDED Requirements

### Requirement: Inspect typed opaque surface previews for the current view

编辑者 SHALL 能在主 View 的普通受光结果旁选择一种表面预览。三种策略在拥有可采样的配对主 D32 时 SHALL 提供标明近端为 1 的 Reverse-Z 深度。Deferred 的最终表面可交付时 SHALL 提供 G0 线性基色与 AO、G1 世界着色法线与过滤后的感知粗糙度及核心金属度、G2 预曝光自发光和 G3 路由。前向策略仅凭其普通深度提供深度预览，依赖 GBuffer 的选项显示当前不可用原因。

预览 SHALL 在同图以类型化读取将当前资源转换为普通可采样颜色，再通过显式图读取交给 UI。深度、法线、核心参数和路由 SHALL 使用同一生产 View 的实际渲染栅格、有效矩形及各自的最终版本。预览 SHALL 明确标示当前通道与颜色含义。深度灰阶用 `[0,1]` 设备深度值，值为 0 时保留其数值语义。G3 路由以有区别的颜色区分核心、Unlit、不透明前向扩展与无覆盖。G0、G1 金属度、G2 只在最终核心分类有效时显示核心材质值。辅助覆盖处 SHALL 仍能检查最终法线、粗糙度及其路由。留边 SHALL 保持已定义的预览背景。

#### Scenario: Inspect the final visible material through a masked auxiliary surface

- **WHEN** Deferred 的前景 Unlit MASK 跨过有不同基色、AO、金属度与自发光的核心接收面，另一块辅助几何被核心遮挡，并选看同一渲染矩形的多个通道
- **THEN** 孔洞和遮挡处显示实际胜出的核心数据，前景覆盖处保留 Unlit 标记、最终 G1 法线和粗糙度，核心专属通道以无有效核心值表示。可识别整数路由类别的输出通过普通颜色采样进入 UI。

#### Scenario: Inspect reverse depth on each strategy and classify a covered far plane

- **WHEN** 有有效主 D32 的相同单样本 View 轮换三策略，固定比例相机的有效图像中包含 D32 为 0 的有限远面与空白
- **THEN** 三种策略的深度预览各自按当前渲染像素显示近深度与零深度。在 Deferred 中，路由和法线预览从分类区分有效远面和空白。图像外留边保持背景。前向的 GBuffer 选项报告缺少当前表面。

### Requirement: Bind surface previews only to a live matching frame

主 View 的表面预览 SHALL 使用与普通显示色独立的会话输出角色和逻辑纹理身份。只有当前普通视口和预览各有实际 UI 读取、该帧所选策略交付匹配输入、预览程序与资源准入均就绪时，才 SHALL 交付本帧的预览颜色。绑定及原生引用 SHALL 分别沿本图和实际提交完成保护。尺寸、场景、相机、实际路径、当前模式或所选通道变更后，预览 SHALL 取匹配本轮的输出或带原因的占位。

关闭、隐藏或全部裁剪预览时，新增的图生产和原生资源工作 SHALL 随消费消失。当前无匹配普通颜色、正在显示安全 UI、预览输入或可用程序缺失，以及预览专属准入失败时，预览 SHALL 使用可诊断占位。若选中的预览超出 UI 几何预算，或原因标签超出正常图剩余的上传额度，SHALL 保留空画布并按正常图的提交票据记录当前原因。诊断记录 SHALL 保留完整票据和可辨认的原因，较长原因允许截断。普通主 View、独立离屏 View 及普通显示色的发布 SHALL 继续遵循其实际准备和提交合同。预览采样自身 SHALL 只有诊断含义，受光路径、曝光、AA、历史、文档编辑和主视口导航命中仍仅由各自已有入口与接受条件决定。

#### Scenario: Keep the current preview version through a change of layout and path

- **WHEN** 正在预览的 Deferred View 重设尺寸、路径请求尚待准备，随后普通帧接受 Forward 并再切回 Deferred，且仍有早先使用预览的提交在途
- **THEN** 每张正常图只绑定与其实际生产相符的预览版本。接受 Forward 后所选的 GBuffer 通道显示占位，恢复 Deferred 后使用该帧的新布局和数据。旧引用由原提交安全退休。

#### Scenario: Keep normal color when a diagnostic consumer disappears or is unavailable

- **WHEN** 一帧存在预览需求但其专属程序或资源无法准入，随后条件恢复、预览可读，下一轮预览关闭或完全被裁剪
- **THEN** 普通颜色在自身合法时继续被实际采样并正常接受。预览先显示当前原因，额度不足以附带图像或原因标签时按同一完整提交票据记录相应原因。恢复后才采样当前预览，撤去消费后只有普通帧所需的生产和资源工作，也没有新的预算拒绝记录。
