# Spec Delta

## MODIFIED Requirements

### Requirement: Preserve image interpretation separately from decoded pixels

首版图片导入 SHALL 将每样本至多 8 位的 PNG 和 8 位 JPEG 解码为拥有型 RGBA8，并保留宽高、实际像素布局、从顶部开始的行序和一个有效图像层级，输入及结果均受预算约束。颜色或数据用途通过独立的 CPU 描述引用同一图片版本。颜色 RGB SHALL 标记为 sRGB，数据通道 SHALL 保持线性。基础颜色和 Editor 图像的覆盖率 alpha SHALL 保持线性且不预乘，源没有 alpha 时按不透明解释。法线和 ORM 的数据解释 SHALL 保留对应通道。环境来源 SHALL 保留源编码、线性辐亮度与亮度标定。亮度标定缺省为相对值。普通图片与环境来源 SHALL 各自按声明的格式和用途接收请求，超出对应 profile 的编码 SHALL 返回明确的未支持结果。

#### Scenario: Interpret the same pixels as color and linear data
- **WHEN** 调用方将同一张已发布、具有已知透明像素的 PNG 分别描述为 Editor 颜色与线性数据
- **THEN** 两种描述可共享同一只读像素版本，并具有不同的用途身份。色彩图的覆盖率与解码的 alpha 一致，像素字节和行序保持一致

#### Scenario: Describe an uncalibrated environment source before decoding is supported
- **WHEN** 调用方描述未带标定的 Radiance `.hdr` 环境源并请求解码
- **THEN** 环境描述保留 Radiance 源编码、线性辐亮度和相对标定，当前解码 profile 报告格式未支持，无新 `CpuReady` 记录

#### Scenario: Describe and decode an uncalibrated environment source
- **WHEN** 调用方将符合环境导入 profile 且缺少亮度标定的 EXR 作为环境请求
- **THEN** 当前成功版本保留 EXR 源身份、线性辐亮度和相对亮度状态，并交付对应的只读 HDR 像素

## ADDED Requirements

### Requirement: Publish versioned HDR environment pixels

环境导入 SHALL 从受控的 `Assets/Environments` 读取实际源字节，按现有作业准入、源和像素预算、指纹和过期发布规则交付不可变版本。支持范围 SHALL 包含仓内 ZIP 和 PIZ 的单 part、scanline、全分辨率 RGB 或 RGBA EXR，尺寸为等距柱状投影所需的 2:1。输入在上边界内解码成顶部起始的有限线性 RGB，接受 HALF 和 FLOAT 分量。已有 alpha SHALL 按忽略解释，有限负 RGB 值在进入物理辐亮度域时归零。色度缺省按 Rec.709/D65 解释，显式声明的非兼容色域或格式 SHALL 给出原因。有效的 `whiteLuminance` SHALL 表示 RGB 单位白的正有限亮度及其文件声明来源，缺席时 SHALL 保留相对标定。无效像素、标定或超预算输入 SHALL 通过有界错误报告。

#### Scenario: Publish a compressed EXR without an SDR conversion

- **WHEN** 请求带可区分行序、已知大于 1 的 RGB 和可选 alpha 的合法环境 EXR
- **THEN** CPU 版本的哈希对应实际读取字节，像素保持约定行序及线性超白范围，alpha 不改变 RGB

#### Scenario: Reject an invalid replacement at the import boundary

- **WHEN** 已有有效环境源版本，后继源代超出解码预算或超出已声明的输入 profile
- **THEN** 本次请求报告对应错误且没有新的 `CpuReady` 版本，旧读者仍持有原像素版本
