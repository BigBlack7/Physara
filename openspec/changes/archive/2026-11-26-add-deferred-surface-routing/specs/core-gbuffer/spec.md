# Spec Delta

## ADDED Requirements

### Requirement: Resolve the nearest opaque surface across material routes

显式请求完整表面准备的 View SHALL 从当轮冻结的有效材质和可见 Draw 解析不透明路由。核心金属粗糙度材质进入核心写入。Unlit 和已受支持、超出核心模型的 lit 不透明 profile 进入各自的辅助路由。OPAQUE 和 MASK 在各自路由沿用相同的几何变换、面剔除、材质覆盖及 alpha 合同。透明项沿用当前的可见不支持原因。

核心表面初始化后，辅助路由 SHALL 在通过覆盖和 Reverse-Z 深度测试的位置接续写该 View 的 D32、G1 法线及过滤后的感知粗糙度，以及清除 DeferredEligible 的 G3 分类。后继 SHALL 获得同一渲染矩形和最后的匹配图版本。D32 较近的表面获胜。同一像素的 D32 相等时，辅助表面在核心表面之后取得优先权，辅助路由内部按同一稳定 Draw 次序判定最后覆盖者。无辅助批次时，核心版本即为最终版本。所需的表面准备失败或等待期间 SHALL 交付相应原因并保留该 View 的既有退路。

#### Scenario: Resolve a masked auxiliary surface over a distinct core receiver

- **WHEN** 一个源材质为 Unlit 的前景实例经有效材质覆盖采用 MASK 并与核心实例重叠，前景 alpha 分别小于和恰好等于 cutoff，且另一块辅助几何位于核心实例之后
- **THEN** 遮罩孔及较远辅助几何处保留核心的深度、法线、粗糙度和核心资格。恰好等于 cutoff 处交付前景的深度、法线、粗糙度和 Unlit 分类，核心资格清除。

#### Scenario: Identify a far-plane auxiliary surface and initialized background

- **WHEN** 有限正交 View 中同时含有 D32=0 的辅助表面、正深度核心表面、图像内空白及有效矩形外留边，随后复用附件存储绘制表面移出后的图
- **THEN** 第一帧的远平面辅助表面拥有自己的 G1、G3 和几何深度。空白、留边及复用后不再覆盖处的 D32 与 G3 均为零。消费方可仅凭当前分类区分背景、远平面表面和核心照明资格。

### Requirement: Compose auxiliary color at the depth of the final surface

核心照明 SHALL 只对最终 G3 同像素上 DeferredEligible 置位的表面产生贡献。ForwardUnlit/ForwardExtension SHALL 在其所属 View 的最后 D32 版本上进行只读等深测试，使用与 SurfaceAux 一致的辅助路由和覆盖，按同一胜出次序向加载后的不透明工作色写入。Unlit 颜色 SHALL 保持共享的线性基色语义，受支持的 lit 扩展 SHALL 使用其共享前向材质及当前工作色合同。存活片元 SHALL 以 alpha=1 标识几何覆盖。其余像素保留原有工作色及覆盖，最终深度保持为该表面的图版本。

同一 View 请求有效阴影诊断和核心颜色合成时，Renderer SHALL 在准备阶段拒绝组合并报告原因。普通阴影诊断可同时提供独立的最终表面。

#### Scenario: Compose distinct colors through the same mask and shared depth

- **WHEN** 上游核心颜色消费者按最终 G3 在有资格的像素写已知工作色，随后以前一场景的相同覆盖声明辅助前向颜色
- **THEN** 前景辅助表面显示自己的颜色和覆盖。孔内和较远辅助几何处保留核心工作色，无覆盖处保留上游底色及覆盖值。深度读取对应最后的表面版本，辅助像素的核心照明资格仍为零。

#### Scenario: Preserve the same winner across coplanar auxiliary draws

- **WHEN** 同一像素的核心表面先写入 D32，两项颜色与法线不同的可见辅助 Draw 以同一 D32 到达，且两项位于不同批次
- **THEN** 最后覆盖的辅助 Draw 同时决定 G1、G3 和最终颜色。该像素无核心照明贡献，结果遵循冻结批次的稳定顺序。

#### Scenario: Reject mixed scene color and shadow diagnostics

- **WHEN** 已选中有效阴影诊断的 View 同时请求核心颜色合成，随后以同一诊断请求独立表面，再关闭诊断请求合成
- **THEN** 首次准备带原因拒绝。独立表面请求保留普通诊断颜色。关闭诊断后恢复完整合成，Unlit 使用标准线性基色。

## MODIFIED Requirements

### Requirement: Produce a typed single-sample core surface

生产者 SHALL 为同一 View、同一栅格产生按 G0 至 G3 排列的四个同尺寸单样本附件和独立的 Reverse-Z D32，各附件作为匹配的图资源版本交付。G0 为 RGBA8_SRGB，RGB 存储 sRGB 编码基色，A 存储线性 AO。G1 为 RGBA16_FLOAT，依次存储世界空间单位着色法线的有符号八面体 XY、过滤后的感知粗糙度、金属度。G2 为 R11G11B10_FLOAT，存储预曝光自发光。G3 为 R16_UINT，位 0 表示 DeferredEligible。核心片元写 `0x0001`，Unlit 和不透明前向扩展片元分别写 `0x0002` 和 `0x0004`，背景为零。其余位保留为零。消费者 SHALL 在同一像素读取各附件，以 G3 分类判断有效表面，以位 0 判定核心照明资格，并从配套 D32 读取几何深度。辅助像素仅交付 D32、G1 的法线及粗糙度和 G3 分类作为表面数据。完整核心参数的解码 SHALL 仅用于核心资格有效的像素。

#### Scenario: Consume mixed attachments from one graph producer

- **WHEN** 同一图内的后继 Pass 按像素读取一个有已知通道值的核心三角形和旁边的背景
- **THEN** 三角形处可取得上述四种类型的匹配结果和几何深度，颜色附件的格式占用合计 18 B/像素，D32 占 4 B/像素。背景的深度与 G3 均为零。

#### Scenario: Distinguish a finite far-plane surface from cleared eligibility

- **WHEN** 有限正交相机的核心表面在 D32=0 通过深度测试，同帧包含未覆盖的背景和正深度的核心表面
- **THEN** 远平面与较近表面的 G3 位 0 均置位，二者均可解码用于核心照明。背景的资格为零。后继清除某像素的资格时，该像素跳过核心照明，即使其深度为正。
