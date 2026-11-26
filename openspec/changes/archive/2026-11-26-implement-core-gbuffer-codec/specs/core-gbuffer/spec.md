# Spec Delta

## Purpose

本能力规定静态场景中核心不透明与遮罩材质的可见表面如何编码到固定的单样本图附件，以及后续渲染消费者如何按像素恢复这些材质参数、判断照明资格并解释量化误差。

## ADDED Requirements

### Requirement: Produce a typed single-sample core surface

生产者 SHALL 为同一 View、同一栅格产生按 G0 至 G3 排列的四个同尺寸单样本附件和独立的 Reverse-Z D32，各附件作为匹配的图资源版本交付。G0 为 RGBA8_SRGB，RGB 存储 sRGB 编码基色，A 存储线性 AO。G1 为 RGBA16_FLOAT，依次存储世界空间单位着色法线的有符号八面体 XY、过滤后的感知粗糙度、金属度。G2 为 R11G11B10_FLOAT，存储预曝光自发光。G3 为 R16_UINT，位 0 表示 DeferredEligible，本版保留位写零。消费者 SHALL 在同一像素读取各附件，使用 G3 位 0 判定核心照明资格，并从配套的 D32 读取几何深度。

#### Scenario: Consume mixed attachments from one graph producer

- **WHEN** 同一图内的后继 Pass 按像素读取一个有已知通道值的核心三角形和旁边的背景
- **THEN** 三角形处可取得上述四种类型的匹配结果和几何深度，颜色附件的格式占用合计 18 B/像素，D32 占 4 B/像素；背景的深度和资格位均为零。

#### Scenario: Distinguish a finite far-plane surface from cleared eligibility

- **WHEN** 有限正交相机的核心表面在 D32=0 通过深度测试，同帧包含未覆盖的背景和正深度的核心表面
- **THEN** 远平面与较近表面的 G3 位 0 均置位，二者均可解码用于核心照明。背景的资格为零。后继清除某像素的资格时，该像素跳过核心照明，即使其深度为正。

### Requirement: Preserve visible coverage and effective instance material

核心写入 SHALL 使用现有静态实例变换、顶点和槽覆盖、有效材质行、面剔除及遮罩合同。只有通过覆盖和深度测试的核心 OPAQUE/MASK 片元才可写入资格位及配套深度和字段。启动一次核心表面生产时 SHALL 明确初始化背景深度与资格，沿用池内存时亦满足此合同。Unlit 或超出核心模型的几何不参与本次核心写入。

#### Scenario: Resolve a masked instance over a distinct rear surface

- **WHEN** 前方 MASK 实例的 alpha 分别小于和等于 cutoff，后方是带不同材质覆盖的同网格实例，视野还包含未覆盖区域
- **THEN** 小于阈值处保留后方的深度和材质，等于阈值处写前方深度和材质；两处 G0.a 各为该表面的 AO。曾含有效像素的存储被复用后，当前背景仍不可解码为有效表面。

#### Scenario: Encode the back of a mirrored double-sided surface

- **WHEN** 核心绘制使用带法线贴图、负 determinant 及非均匀缩放的双面实例，视角看到其有效背面
- **THEN** 解码法线的方向符合共享切线框架和一次背面翻转，同一输入在单面剔除时保持对应覆盖结果。

### Requirement: Reconstruct the shared core response within format precision

系统 SHALL 在几何片元取得覆盖分歧前所需的梯度，并在编码前完成法线贴图和几何粗糙度过滤。解码 SHALL 返回线性基色、线性 AO、单位着色法线、过滤后感知粗糙度和金属度，按共享核心模型派生 diffuseColor、F0、F90 与微表面粗糙度。自发光 SHALL 仅施加一次当前预曝光，按通道饱和到 G2 的有限上界；解码返回工作域自发光。对固定输入的字段误差 SHALL 可由对应的 sRGB/UNORM8、binary16、八面体编解码或 R11G11B10 精度解释。

#### Scenario: Decode a filtered material with independent color and data channels

- **WHEN** 已知线性基色、非单位 AO 与中间金属度的表面具有非零几何粗糙度核，编码后从原像素解码
- **THEN** RGB 仅经历一对 sRGB 存取转换，AO 按线性 8 位量化，粗糙度匹配一次过滤的结果，单位法线和核心波瓣参数落在各自登记的格式误差内。

#### Scenario: Decode very bright emission at non-unit exposure

- **WHEN** 分别写入零、可分辨的非单位预曝光值及超过某一通道上界的自发光
- **THEN** 零保持为零，可表示值匹配一次预曝光后的通道量化，越界值在 R/G 的 65024 或 B 的 64512 处饱和，其余可表示通道独立保留。
