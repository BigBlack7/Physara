# Design

## Context

见 [提案](proposal.md)。已验收的 D12-T01/T02 提供源材质和稀疏实例覆盖、统一的 448 B 静态材质行、两套清漆法线框架以及 ForwardExtension。三个受光布局为 Direct、Physical DFG-only、Physical IBL，分别有无阴影配方。现有 `BakeDfg` 以 RGBA16F 保存 GGX R/G 和 Charlie 配 Ashikhmin/Neubelt 形式可见度的 cloth B。`PlanDraws` 在整理可绘项后准备环境，目前按 Physical 受光或天空取产品。环境缓存按源与 profile 区分产物，worker 产生只读像素，Renderer 拥有 GPU 驻留和实际图写入。 [材质 ABI](../../../../Engine/Shader/Interface/StaticForward.json) · [DFG 和环境烘焙](../../../../Engine/Renderer/Lighting/EnvironmentBake.cpp) · [绘制计划](../../../../Engine/Renderer/Draw/StaticDraw.cpp)

## Goals / Non-Goals

一次合成有效材质与覆盖，向两个灯候选策略交付同一物理求值，并使每个准备、上传和采样需求可追溯到相应颜色角色。验证以 lit OPAQUE/MASK、当前单采样三路径和有限叠层为边界。后续波瓣及阶段组合验收由 D12-T04 至 T06 承担，透明和透射归 D13。 [不透明着色合同](../../../../Docs/Architecture.md#opaque-shading) · [工作包划分](../../../../Docs/Developments/D12-D13-Materials.md#dev-d12)

## Decisions

<a id="inputs-and-profile"></a>
### 源材质、实例与 GPU 行

- 在现有 glTF 材质 owner 解析 `sheenColorFactor` 三个线性分量、`sheenRoughnessFactor` 和两个独立槽。颜色为 sRGB RGB，粗糙度为线性 A。因子有限且各分量在 [0,1]，缺省两因子为零，缺图乘数为一。采样器、有效 UV0/UV1、每槽 `KHR_texture_transform`、图像版本与编码用途沿现有纹理链。共享图像记录可用同一源像素生成匹配用途的 GPU 视图。 [固定 Khronos 规范](https://github.com/KhronosGroup/glTF/blob/7cee0fcddd5730e90d488bfb5aa2e52171483dde/extensions/2.0/Khronos/KHR_materials_sheen/README.md)
- 识别 required 和 optional 的标准输入，按源材质身份拒绝与 unlit 或旧 specular-glossiness 共用的声明，包括零因子声明。兼容核心 metallic-roughness、独立 IOR/specular 及 clearcoat。Scene 交易复用现有 `hasSpecularGlossinessExtension` 源标志、有效字段和槽校验。两个 sheen 槽需要各自有效 UV，切线需求由实际使用的基底和清漆法线槽决定。Inspector 与 v1 走同一 typed edit，零、清除覆盖和槽继承具有各自身份。完整候选校验后才发布。 [源材质](../../../../Engine/Asset/GltfData.cpp) · [有效实例](../../../../Engine/Scene/SceneMaterial.cpp) · [v1 解析](../../../../Editor/Session/SceneDocumentRead.cpp)
- 编辑器可显示声明或覆盖，即使有效颜色因子为零。受光角色由提交给 Draw 的有效颜色因子是否全部为零决定。零因子沿实际核心、IOR/specular 或清漆角色准备。纹理在像素上取到黑色时该像素的 sheen 响应为零，Draw 路由仍按有效因子确定。该规则给绑定准备一个与 GPU 采样延迟无关的判据。
- 在现有权威 schema 尾部增加颜色加粗糙度的 `float4`，将两组 UV 行扩展到十二槽。按当前 std430 对齐预计材质行 528 B，以生成布局和实际表读取为准。保留材质表 t5 和此前槽位，新增 set 2 的 t11/s11、t12/s12。颜色角色按槽集合消费，独立 sheen 可跳过清漆槽。独立材质缓冲会另增版本、绑定和提交寿命，当前共享行用同一有效材质身份表达所需参数。 [生成合同](../../../../Docs/Architecture.md#shader-build)

<a id="layer-response"></a>
### Charlie/Neubelt 与叠层预算

- Sheen 使用面向处理和法线贴图后的基底着色法线，以几何及着色半球约束直接入射和出射。粗糙度以共享的未扰动几何法线方差独立过滤，沿共享逻辑使用 0.045 下限并只派生一次 `αs = ps²`。清漆继续用自身法线、粗糙度、Fresnel 与几何约束。 [法线和粗糙度合同](../../../../Docs/Architecture.md#materials) · [固定 Filament 层模型](../../../../Docs/Reference/Filament/Lighting.md)
- 设 `S` 为采样后的线性 sheen 颜色。直接波瓣以 `D = (2 + 1/αs)·(1−NoH²)^(0.5/αs)/(2π)` 及 `V = 1/[4·(NoL + NoV − NoL·NoV)]` 求值。共享照明采集一次光源、距离衰减和阴影，求值使用所在波瓣的入射余弦。使用稳定的半角和界内角余弦，在可见范围内交付 `S × D × V × NoL` 的定向响应。
- 在匹配的 `(NoV, ps)` 取现有 DFG 蓝通道 `Eraw`，保持 R/G 的 GGX 语义。由 `q = 1/max(1,Eraw)`、`Es = min(Eraw,1)` 归一既有 Neubelt 方向能量，基底受光权重为 `1−max(S)·Es`。直接 sheen 乘 `q`，环境 sheen 乘 `Es`，两者共享一次视向查询。规划期按独立入射方向积分，`NoV≈0.02, ps≈0.08` 的未归一方向反照率约为 3.10，故要给实际可见的锐利掠射波瓣与间接光配套限幅。这个受限方向模型和 Khronos 说明允许的 NoV-only 层近似保留可验证的能量预算。逐灯双向层预算涉及额外的入射查表和另一组配套标定。 [DFG 固定源](../../../../Engine/Renderer/Lighting/EnvironmentBake.cpp) · [规范叠层说明](https://github.com/KhronosGroup/glTF/blob/7cee0fcddd5730e90d488bfb5aa2e52171483dde/extensions/2.0/Khronos/KHR_materials_sheen/README.md#sheen-layering)
- DFG 蓝通道在同一 RGBA16F 纹理内独立使用含端点的平方网格。列索引归一后平方得到 `NoV`，行索引归一后平方再映射到 `[0.045,1]`。运行时将其逆映射到首末 texel 中心之间，覆盖 `NoV=0.0001` 的有效掠射端。R/G 积分及查询沿原均匀中心坐标。 [CPU 烘焙](../../../../Engine/Renderer/Lighting/EnvironmentBake.cpp) · [Shader 查表](../../../../Shaders/Lighting/OpaqueLighting.glsl)
- 核心或 IOR/specular 基底先乘 sheen 的透过权重并加 sheen 镜面，再由清漆透过权重作用于整个下层受光后加清漆镜面。环境项对基底环境反射及其可能的屏幕反射输入应用同一基底权重，sheen 独立环境项保有自己的权重。材质 AO 在间接项用各自粗糙度与视角，直接光和发射遵循各自的预曝光合同。发射使用清漆的出射透过率。各 lobe 在进入工作色限幅前应用层权重，以保留可表示的高亮衰减。 [现有环境合成](../../../../Shaders/Lighting/Environment.glsl) · [清漆数值验收](../2026-11-28-add-clearcoat-material-profile/tasks.md#final-verification)

<a id="environment-products"></a>
### 配套预滤波与环境代

- 在现有 CPU bake owner 复用线性源、立方体面与采样方向。sheen 使用独立的 Charlie/Neubelt 归一预滤波立方图，采用固定 split-sum 近似。令代表性 `N=V=R`，其中 `R` 为运行时基底法线的反射方向，按 `D_Charlie·V_Neubelt·NoL` 归一积分源辐亮度。该对称卷积等价于对入射余弦 `μ∈[0,1]` 用 `μ(1−μ)^(0.5/αs)` 加权。归一时约去常量，保证低粗糙度仍有有限权重。运行时以同一 `R` 经环境逆旋转查图，方向反照率幅度由实际 NoV 的蓝通道给出。代表性卷积省去逐像素方向积分，可用独立常量环境和方向矩验证。逐像素的掠射形状、阴影化自遮蔽和完整微纤维传输留在此实时近似的边界之外。
- 采用 64 边长、5 级 RGBA32F 的 Charlie 立方图，mip 按 `ps` 从 0.045 到 1 均匀分布，含首级的专用卷积。约 512 KiB 的像素与最多 30 个层面写入纳入现有驻留和上传额度。环境产品 key 保有源摘要、面与尺寸、mip、卷积/采样约定和版本。可共享同源基础层、已完成的 GGX/SH 与固定 DFG。环境朝向、强度和曝光经当前 View 常量应用一次。
- 用已准备的受光 Draw 决定本 View 对 DFG 与 Charlie 的需求。Direct 的 sheen 读取 DFG 蓝通道继续使用 Direct 灯光，Physical 省略照明环境时使用 DFG-only。仅 Physical 且环境照明开启且实际受光需要 sheen 时添加 Charlie 立方图。Sky-only、端点零因子及独立表面只准备各自的角色。由已有队列处理从普通环境到增加 Charlie 的同源需求，已完成的子产品继续可复用。 [环境缓存](../../../../Engine/Renderer/Lighting/EnvironmentCache.hpp) · [GPU 准备与 View 常量](../../../../Engine/Renderer/Lighting/EnvironmentGpu.cpp)
- CPU 只发布和请求相符的完整只读产品。GPU 先取得实际驻留、上传和采样范围，再为本 View 图声明所需写入与读取。替换或中断时由原有 View 接受规则保活旧输出，确认时同时核对 GGX/SH、Charlie、固定 DFG 和当帧查询参数。多个 View 的需求由 owner 累计，单一路预览或取消仅撤销相应需求。等待或拒绝沿现有环境诊断公开原因。 [环境契约](../../../../openspec/specs/environment-lighting/spec.md)
- 单 worker 分步发布同源基础层、GGX/SH 和 Charlie。每份完成快照复用已就绪的同源子产品；天空或普通照明在自己的子产品齐全时即可使用。各 View 对缺项累计需求，帧尾按活动作业的具体角色取消，保有仍被其它 View 请求的作业。 [环境缓存](../../../../Engine/Renderer/Lighting/EnvironmentCache.cpp)

<a id="routes-and-admission"></a>
### 颜色角色、覆盖与准入

- 沿有限 role 表为 sheen 与 sheen+clearcoat 建立两个颜色族。每族由全灯/Froxel、Direct/DFG-only/IBL 和有无阴影确定 12 个可达配方，共 24 个新角色。IOR/specular 是两族共用的材质行数据分支，现有诊断使用原有角色。独立 sheen 沿用基底顶点输入，组合沿用已验收清漆顶点布局。固定配方使两种几何输入和每阶段实际绑定可分别验证。
- 从有效 `MaterialUse` 和色角色共同派生实际纹理、sampler、UV/跨阶段输入与图读。独立 sheen 与组合角色校验 UV0/UV1；组合角色同时校验原有两套切线及传递。将 Charlie 立方图列为 set 3 的新图像槽，由 IBL sheen 颜色实际采样，使用已有环境 sampler。预计最重的组合 IBL+阴影片元有 16 个纹理单元，最终以生成接口、两阶段活动反射、真实配对及设备上限检查准入。用同一角色检查覆盖正式冷包和整代热候选，包括当前未选角色。 [产品角色目录](../../../../Engine/Shader/ProductVariants.cpp) · [角色检查](../../../../Engine/Renderer/Paths/ForwardShaderPreparation.cpp)
- Forward 选择全灯，Forward+ 与 Deferred ForwardExtension 选择匹配的 Froxel。Depth、MASK、双面与 caster 继续使用共享基底覆盖。Deferred 在四附件的 SurfaceAux 写入实际获胜者的基底 G1、G3=4 与 D32，同一 Draw、有效材质、顺序和最终深度进入等深颜色。独立表面或深度请求只表达实际采样，即使色包或 Charlie 暂缺也可按既有条件交付。正常颜色须等待完整候选，历史与有效预览只跟随正常接受票据。 [几何与 SurfaceAux](../../../../Docs/Architecture.md#geometry) · [计划就绪](../../../../Docs/Architecture.md#plan-readiness)
- 官方 ABI 冷构建和同 ABI 的热重载分别验证。运行时整代准入继续按真实源编译与缓存条目、编码字节及保活限制执行。T03 在原 16 MiB 配额实测拒绝时，活动代仍为 2。按当前产品编码和同驻要求采用 20 MiB 配额，条目配额仍按两倍角色数计算。合法重载发布代 3 时缓存同驻为 153 项、20,189,508 B。 [重载额度](../../../../Engine/Shader/Reload.hpp) · [拒绝记录](../../../../build/d12-t03/sheen-reload-16MiB.log) · [发布记录](../../../../build/d12-t03/sheen-reload-Release.log)
- 正式编译入口把目录核对过的产品名保存进包完整性范围，客户端以它区分 ABI、阶段和反射相同的 Direct 与 Physical DFG-only。候选对新增或已变化的 sheen 包使用设备绑定计划验证实际配对及限额，沿用已经按同设备准入的包。各角色的设备预检和 Pipeline 创建共用帧前额度与累计耗时，活动代优先推进。绑定检查进度随当前候选输入跨帧保留，新请求撤销旧候选。所有角色完成后才建立候选原生表，按实际 View 需求准备 Pipeline 并确认发布。 [包身份](../../../../Engine/Shader/Package.cpp) · [整代准入](../../../../Engine/Renderer/Paths/ForwardShaderPreparation.cpp)
- 单路 View 收尾时保存实际 Draw 的实体代、固定模型版本和源 Primitive，并保留独立清漆、独立 sheen 及组合程序摘要。快照分配失败由该路按准备失败退出，仍可使用此前接受的输出，另一 View 继续独立准备。Inspector 按目标查询实际接受的正常受光票据。成功显示诊断时保留同场景的正常受光快照，诊断请求退回正常受光且提交被接受时则推进受光快照。 [收尾与撤销](../../../../Engine/Renderer/Paths/Forward.cpp) · [View 接受](../../../../Engine/Renderer/View.cpp) · [Inspector](../../../../Editor/Panels/PhysicalInspector.cpp)

## Risks / Trade-offs

| 风险 | 核对与处置 |
|---|---|
| 颜色空间、alpha 槽或变换解释错误 | 一个不对称 RGB/A 输入使用不同有效 UV 和纹理变换，在同一模型的两实例对照编辑、保存与实际采样。 |
| 低粗糙度掠射的原始积分超过一 | DFG 保留实际积分，以现有滤波下限和方向归一同时约束逐灯与间接响应。测一个触发归一的样点及一个未饱和样点，验证白炉与差值。 |
| 低频环境卷积近似损失细节 | 用生成的单位环境和方向梯度核对亮度、逆旋转及 Sheen 与 GGX 的可分辨响应。记录固定方向近似的适用范围。 |
| 额外立方图或角色让冷/热准备出现部分就绪 | 先验证真实冷包和未选热配方的缺项，再用一次环境替换中断及恢复核对上传与接受身份。保存匹配的旧帧及诊断。 |
| 叠层和实际表面覆盖偏离已有材质 | 复用已匹配的 T01/T02 基底与清漆证据，用同一小场景配对三路 HDR、D32、G1/G3、阴影、同深胜出和实例 Undo。 |

## Migration Plan

按新共享接口重新生成产品包及正式直接消费者，运行时仅发布兼容的完整代。新字段作为场景 v1 稀疏成员保存，省略字段按来源继承，既有文档由原有入口打开。新环境角色按产品身份按需取得，从已匹配的基础层和既有照明产物升级。 [场景 v1](../../../../openspec/specs/scene-document-persistence/spec.md) · [Shader 交付](../../../../openspec/specs/shader-generation-reload/spec.md)
