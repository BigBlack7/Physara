# display-post-processing Specification

## Purpose

规定单样本静态场景如何把预曝光的线性 HDR 交付为固定 SDR 显示颜色，明确抗锯齿、空间缩放、颜色编码和 HDR 数值比较各自消费的像素版本与色彩域。

## Requirements

### Requirement: Apply the fixed SDR display profile to the current HDR

Physical Lighting SHALL 消费当前场景的预曝光 linear Rec.709/D65 工作色。固定 ColorGrading profile SHALL 依次执行 Rec.709 到 Rec.2020 的线性原色变换、中性白平衡和创作设置、固定 Filament ACESToneMapper、向目标 Rec.709/D65 变换和 SDR 色域映射。输出 SHALL 为有限的 display-linear RGB，显示值限制在 SDR [0,1]。当前预曝光只在工作色生产时应用一次。显示 profile SHALL 具有可复验的 HDR 查表范围及精度，输出编码在选定的存储或呈现边界发生。

#### Scenario: Map a neutral ramp and asymmetric saturated HDR by the fixed profile

- **WHEN** 物理显示接收固定曝光下的黑、灰、不同档位的中性高亮及非对称饱和样点
- **THEN** 黑保持零，中性高亮按参考响应压缩，位于 profile 有效范围内的非饱和高亮仍可区分，彩色样点在目标色域并符合固定参考的通道值

### Requirement: Filter only the current display-linear image with optional FXAA

Physical Lighting SHALL 支持 None 与 FXAA。None SHALL 将该帧的显示变换结果交给后续输出。FXAA SHALL 依照渲染栅格内的有效图像矩形，在当帧色调映射完成后用感知亮度检测边缘，并对对应的 display-linear RGB 做线性过滤。感知亮度 SHALL 由显示线性 Rec.709 亮度的平方根取得，其中间存储与最终不透明 alpha 各有语义。取样范围 SHALL 限于当前有效图像，保留低于 profile 边缘阈值的平坦区域及其原颜色。曝光与 ToneMap 的作用均由前序颜色生产者完成。

#### Scenario: Smooth an asymmetric image edge while preserving flat colors and padding

- **WHEN** 同一物理场景以 None 和 FXAA 输出，含亮度可区分的倾斜边缘、平坦色块和固定比例留边
- **THEN** FXAA 只在有效图像内的有对比边缘产生预期线性混合；平坦样点和留边在存储精度内匹配，同一帧的 HDR 与深度语义保持各自已有值

### Requirement: Resample the display image within its output rectangle

空间缩放 SHALL 在显示变换及已启用的 FXAA 后进行。正常场景的输出画布、有效显示矩形和来源渲染矩形 SHALL 属于同一 View 请求。缩放 SHALL 按目标像素中心映射相同的归一化图像 UV，对来源有效矩形内的 display-linear RGB 做双线性取样。图像等尺寸同位置时结果 SHALL 为对应像素；边缘 SHALL 使用有效矩形内的样点，留边 SHALL 显示定义好的不透明底色。ViewOutput 的采样语义 SHALL 为 display-linear，颜色编码 SHALL 在 sRGB 存储写入及最终窗口附件各自的边界按合同完成，编辑 UI 以自己的颜色和覆盖率在线性域合成。

#### Scenario: Resize an asymmetric image and compose UI on the accepted output

- **WHEN** 缩小场景渲染尺寸，在有效矩形四角及留边放置可识别样点，并在显示结果上合成已知颜色、覆盖率的 UI
- **THEN** 视口角向、双线性样点和留边匹配当前布局，UI 合成及最终窗口色码符合线性混合和既有 SDR 编码合同

### Requirement: Preserve the matching pre-display HDR comparison source

正常场景结果 SHALL 提供显示前预曝光 HDR 的独立只读输入，包含该源的 linear Rec.709/D65 语义、当前预曝光、渲染矩形及对应场景和相机身份。比较或读回 SHALL 在同一已完成工作上取得该源；在附件可表示的范围内 SHALL 保留超白。AA 和空间输出只消费所需的后续图像版本，显示颜色和窗口编码 SHALL 只从各自生产者取得。

#### Scenario: Compare emissive HDR independently of AA and final encoding

- **WHEN** 固定场景的可表示自发光产生超白工作色，随后提高一档曝光并比较带或不带 FXAA 的显示输出
- **THEN** 显示前测得的对应 HDR 样点随一档曝光翻倍，AA 选择不改变同一曝光下的 HDR，显示线性值由当前 HDR 经一次显示变换得到
