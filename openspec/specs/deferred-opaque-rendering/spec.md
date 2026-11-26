# Deferred Opaque Rendering

## Purpose

规定静态场景的单样本 Deferred 不透明子图怎样从最终可见表面恢复共享受光输入，消费同一冻结视图的灯光与环境，并把预曝光工作色和配对深度交付给共享天空及显示阶段。

## Requirements

### Requirement: Reconstruct the eligible final surface in the current view

显式准备单样本 Deferred 受光子图时，核心照明 SHALL 在当前有效渲染矩形内按像素读取同一 View 经核心写入及辅助覆盖后的 GBuffer 与 D32。最终 G3 的 DeferredEligible 位 SHALL 决定核心照明资格。有效有限远面上的 D32=0 表面仍具备资格，背景及两类辅助表面 SHALL 跳过核心材质解码和光照。

已通过资格判定的像素 SHALL 使用当前渲染投影的逆变换、像素中心、矩形原点和配对 Reverse-Z 深度恢复相机相对位置、米制正向视深及世界方向。透视观察方向 SHALL 指向眼点，正交 SHALL 使用固定反视线方向。局部灯、阴影和稳定网格查询 SHALL 使用同一恢复位置，并保留当前相机相对坐标精度。核心着色参数 SHALL 从固定格式的已过滤数据求得。BRDF 使用解码后的单位着色法线，阴影接收偏移遵循共享阴影合同。

#### Scenario: Use the rendered pixel rather than the display canvas

- **WHEN** 一个非对称受光表面由带留边且显示、渲染尺寸不同的有偏移透视 View 观察，固定像素的几何交点在已知平面上
- **THEN** 读回像素以实际渲染矩形和投影命中该位置，正向视深、局部灯和视角相关响应符合独立交点参考允许的格式及重建误差。留边无几何覆盖。

#### Scenario: Shade covered orthographic far-plane pixels

- **WHEN** 零近平面的有限正交 View 内同处有 Depth=0 的核心表面、Depth>0 的核心表面及未覆盖背景
- **THEN** 两块有效表面均得到相同观察方向下各自的共享材质与受光响应，并输出几何覆盖；G3 为零的像素仍保有无覆盖底色。

#### Scenario: Preserve depth and finite light under a shared translation along view depth

- **WHEN** 正交相机、核心平面和范围有限的点光沿视线轴共同平移到绝对 FP32 坐标会舍去平面局部小数的位置，对照包含眼点低位为零、非零且可精确编码，以及低位舍入后还有可表示余量的样例
- **THEN** 共同平移前后的核心覆盖一致，最终 D32 与相对几何的解析深度相符，范围内的点光维持非零贡献并符合登记的 HDR 容差。

### Requirement: Evaluate shared lighting from the frozen view inputs

Deferred 的 Direct Lighting 和 Physical Lighting SHALL 消费同一冻结 View 的完整灯表、Froxel 与实际获准的阴影范围，并共享核心参数派生、直接光求值与工作域合成。方向灯、有限候选和无范围局部灯 SHALL 遵循共享行号次序，溢出、无有效格及定位不确定时 SHALL 从同一完整局部灯集求值。Physical Lighting SHALL 使用共享 DFG，并在具有请求且完整的环境照明输入时使用同一 SH、预滤波环境及能量项。

屏幕效果关闭时，光照 SHALL 获得 `screenAO=1`、`SSR.confidence=0` 和屏幕预曝光入射反射 RGB 为零的输入。有效的材质 AO 仍作用于间接照明，直接光及自发光 SHALL 保持各自贡献。直接光和环境光 SHALL 各应用一次当前相机预曝光。G2 的已预曝光自发光 SHALL 直接进入工作色聚合。工作色保持当前有限预曝光线性 HDR 合同及几何覆盖 alpha。

#### Scenario: Consume both normal and fallback light candidates

- **WHEN** 同一核心受光面跨越正常和预算溢出的 Froxel，场景含不同贡献的方向灯、有限灯和无范围局部灯，并能在一个边界样点触发不确定查询
- **THEN** 正常格使用本 View 的紧凑列表，两个退路均保留完整可见灯的贡献和顺序。每个代表性样点的 HDR 与同输入全灯参考匹配到登记的编码和位置重建误差。

#### Scenario: Preserve material AO and single exposure with screen effects disabled

- **WHEN** 非单位预曝光的物理 View 含非零自发光、有区别的材质 AO 和真实环境照明，另外读取固定样点的无环境或无直接光对照
- **THEN** 可辨认出共享 DFG 与当前环境对间接光的响应，材质 AO 只改变该分量，屏幕输入保持中性。G2 自发光与它的工作域数值一致，直接光、间接光和自发光各自符合一次预曝光的参考。

### Requirement: Deliver composable opaque color and unchanged final surface depth

子图 SHALL 交付同一渲染矩形的初始化 RGBA16F 工作色和最终单样本 D32，并提供支撑屏幕输入的匹配表面版本。核心照明 SHALL 声明实际 GBuffer、深度和照明数据的采样或读取权限，按各自最近写版本取得数据。辅助前向阶段 SHALL 使用同一 D32 的只读等深用途，加载当前 HDR，沿最近表面的有效材质和覆盖合成。核心与辅助着色后，实际有效的几何像素 alpha SHALL 为 1，未覆盖处保有约定底色与 alpha=0。共享天空 SHALL 只补有效图像矩形内的无覆盖像素。共享显示阶段 SHALL 消费对应的最终 HDR，深度仍属于同次表面准备。图版本或设备用途无法成立时 SHALL 按现有图失败路径报告原因。

#### Scenario: Compose sky across covered far depth and masked auxiliary boundaries

- **WHEN** 一个带留边的有效 View 中，Unlit 遮罩跨越核心接收面和有天空的背景，且存在有限远平面上的核心或辅助表面
- **THEN** 最后的核心资格、辅助颜色及天空覆盖与最近表面的 G3、D32 配对。核心与辅助几何保有自身 alpha 和颜色，遮罩孔中显示其后的核心或天空，留边保持底色。天空前后沿用同一个 D32 版本，显示映射消费合成后的颜色。

#### Scenario: Deliver an initialized view without a live core color consumer

- **WHEN** 分别准备只有 Unlit 和没有可见几何但含有效天空的 View，复用曾有核心受光几何的附件存储
- **THEN** 仍得到完整工作色和配对 D32，旧几何在背景无残留。只准备并上传当前确有消费者的资源。没有受光核心或扩展的 View 不产生灯表、Froxel 或阴影的受光消费。

### Requirement: Prepare and identify the real deferred color producer

Deferred 子图 SHALL 使用产品 Shader 包和已准备的原生 Pipeline，在同轮冻结输入的图上交付颜色。打包与热重载准入 SHALL 核对核心光照角色实际需要的数值类型、活动资源、阶段及配方。实际受光的主要和附加 View SHALL 分别使用各自的相机、灯与上传版本。独立表面请求 SHALL 交付配对的 GBuffer 与 D32，准备范围由表面实际使用的输入与角色决定。当前请求缺少必要准备或包被拒绝时 SHALL 报告明确状态，保留已有可读输出和兼容 Shader 代。可独立完成的其它 View SHALL 继续按既有准备额度处理。成功交付的生产身份 SHALL 包含实际核心光照程序和与该输出有关的阶段身份。

#### Scenario: Require the living deferred consumers and their matched inputs

- **WHEN** 两路从同一冻结场景分别提交生产 Deferred 与普通 Forward，使用不同相机或有效矩形，随后撤去 Deferred 的颜色只保留独立表面请求
- **THEN** 实际受光角色只读取所属 View 的当前数据，Deferred 的四块 Froxel 共用已有生产者；独立表面无需核心照明角色和专属灯光输入。取得的同次 HDR 与深度带对应的生产身份。

#### Scenario: Deliver the standalone surface with the unused color package missing

- **WHEN** 保留可用的表面所需包，同一受光场景的普通颜色包缺失，并分别请求独立表面和普通颜色加附加表面
- **THEN** 独立表面交付匹配的 GBuffer 和 D32，只准备其使用的几何、相机、材质和表面角色。普通颜色请求报告具体缺失包，恢复合法包后可同时交付普通颜色与附加表面。

#### Scenario: Diagnose an incompatible deferred lighting role

- **WHEN** 打包或热重载的核心光照角色缺少活动读项，或将 G3 声明为非匹配数值类型，随后提供合法配方
- **THEN** 拒绝状态指向对应程序和资源，旧完整代或上次接受输出保持可读。其它仍可满足的计划继续按预算推进。合法配方就绪后的正常提交才交付匹配的新生产身份。
