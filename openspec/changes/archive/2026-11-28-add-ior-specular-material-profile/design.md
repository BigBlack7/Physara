# Design

## Context

D12-T01 以 [proposal](proposal.md) 为范围。当前导入器支持核心 metallic-roughness 及四个已有扩展，材质覆盖和 v1 文档各沿一条生产链。Draw 准备已经具备 `OpaqueForwardExtension` 枚举、SurfaceAux 和辅助颜色合成，实际 lit 材质仍归入核心路由。现有 BRDF 的直接光接受显式 F90，但 DFG 只按核心 F90=1 解释。 [导入入口](../../../../Engine/Asset/GltfData.cpp) · [材质准备](../../../../Engine/Renderer/Draw/StaticDraw.cpp) · [核心 BRDF](../../../../Shaders/BRDF/CoreBrdf.glsl)

[架构 4.4](../../../../Docs/Architecture.md#physical-inputs)、[5.7](../../../../Docs/Architecture.md#plan-readiness) 及 [8.5–8.7](../../../../Docs/Architecture.md#materials) 为标准字段、着色与准备的依据。D11 的混合表面样例观察到了核心与 Unlit，lit ForwardExtension 计数为零。本包采用同一票据与读回方法，增加来自实际导入的受光扩展表面。 [三路径基准](../../../../Docs/Performance/ThreePathReference.md#quality)

## Goals / Non-Goals

### Goals

- 以同一份有效材质数据驱动 Inspector、几何路由、GPU 行及两种光照响应。
- 让同帧的核心与扩展几何各自取得匹配的产品程序、绑定与表面版本，保留提交前就绪与旧版本保活合同。
- 记录 T01 的能量近似与独立检验依据，为后续波瓣提供可复用的基底。

### Non-Goals

- D12-T02 至 T06 分别负责 clearcoat、sheen、anisotropy、iridescence 和阶段组合验收。D13 负责 BLEND、透射和体积。后续拾取功能消费现有覆盖合同。 [材质工作包](../../../../Docs/Developments/D12-D13-Materials.md#d12-t02) · [拾取工作包](../../../../Docs/Developments/D16-D17-EditorAndDelivery.md#d16-t04)
- 采用当前单样本三路径产品规格。四附件核心 GBuffer 继续作为可编码核心表面和辅助表面分类的生产接口。 [静态表面合同](../../../../Docs/Architecture.md#geometry)

## Decisions

### 在资产和实例边界确定有效 profile

`GltfData` 为选定 scene 的材质读取扩展对象、原始标准因子及两个纹理槽，`GltfScene` 补充文件级支持声明。先校验同材质扩展排斥，再进入 Unlit 的专用读取。使用现有纹理表与图像依赖收集，图像版本仍按实际消费字节计算，冷加载和刷新复用同一流程。规格字段出现与否保留在 CPU 记录，材质颜色因子的存储上限来自有限数值表示，取样后的 F0 才按公式限制上界。 [资产记录](../../../../Engine/Asset/ModelAsset.hpp) · [字段规则](specs/static-gltf-import/spec.md)

两个材质槽各有明确用途。强度槽用线性 A 数据，颜色槽用 sRGB RGB。导入、`DescribeImage`、Scene 覆盖与文档槽都采用同一用途解释。扩展贴图复用现有每槽 UV 仿射、有效 UV 检查、mip 和采样缓存，以图像版本、颜色域和采样条件确定 GPU 资源。强度的 A 与 MASK 覆盖分别取各自的纹理与因子。 [资产图像语义](../../../../Docs/Architecture.md#asset-images) · [实例材质解析](../../../../Engine/Scene/SceneMaterial.cpp)

在 `ResolveMaterial` 后根据源声明和稀疏覆盖确定 profile。源有 IOR 或 specular 声明，或实例有相应字段、纹理覆盖的受支持 lit 表面，选用 IOR/specular profile。即使显式扩展为规范默认值，或用户把扩展因子改回默认，已启用的 profile 仍以本设计的扩展能量规则求值。核心与扩展的缺省镜面端点相同。D11 的核心直接 Lambert 与间接 RGB 预算继续用于普通核心材质。清除所有扩展覆盖后重新解析源声明，源无扩展时回到核心 profile。以该结构规则稳定调参期间的路由和程序键。

普通实例 IOR 编辑采用 `ior >= 1`。源文件里的 IOR=0 是固定的 glTF 兼容模式，Inspector 展示该模式并锁定 IOR 数值，镜面强度与颜色仍可编辑。命令校验和文档恢复共同经过 Scene 验证。扩展的三个稀疏字段沿 `MaterialOverride` 与 `PhysicalEdits` 保存到 v1 `material`，两个槽分别使用 `textures.specular` 和 `textures.specularColor`，外置纹理的 `use` 对应同名用途。继承、还原与 Undo/Redo 沿原事务和版本身份，源模型保持只读。 [编辑合同](specs/editor-domain-edits/spec.md) · [文档合同](specs/scene-document-persistence/spec.md) · [Khronos IOR 兼容模式](https://github.com/KhronosGroup/glTF/blob/7cee0fcddd5730e90d488bfb5aa2e52171483dde/extensions/2.0/Khronos/KHR_materials_ior/README.md#specular-glossiness-backwards-compatibility-mode)

<a id="material-abi-and-color-roles"></a>
### 复用生成的材质行，以结构 profile 选择颜色程序

扩展 `StaticForward` 的权威 `ForwardMaterial` 记录，追加 T01 数值与两槽 UV 参数，保持一张现有版本化材质表。统一生成 C++/GLSL 布局、反射和角色资源声明。现有核心纹理占 PerMaterial 的 t0–t4，`materialRows` 占 t5。使用 t6/s6 和 t7/s7 分别绑定 specular 与 specularColor。CPU 与图读取共享从槽到资源的同一映射，并将材质表绑定从槽数量推导中移出。更新行比较、上下游 stride、上传大小及绑定身份，确保存储复用对应实际数据代。 [当前接口](../../../../Engine/Shader/Interface/StaticForward.json) · [生成约定](../../../../Engine/Shader/Interface/README.md)

产品目录只有核心与 IOR/specular 两个材质 profile。本包的扩展颜色采用现有全灯或 Froxel、Direct 或两类 Physical、阴影 Off 或 On 的可达组合，最多 12 个扩展颜色角色。Deferred 扩展重用对应 Froxel 几何颜色角色。前置深度、阴影图、核心 GBuffer、SurfaceAux 和非受光诊断使用各自已有的角色。数值、贴图缺席和两个扩展的声明组合通过材质行表示。颜色角色身份进入产品包、需求登记和原生 Pipeline 键，保留顶点布局、绕序、剔面、表面阶段、目标与完整合批键。通过同一目录核对 CMake 打包和开发热编译的字段集合。 [产品角色](../../../../Engine/Shader/ProductVariants.cpp) · [原生键](../../../../Engine/Renderer/Paths/ForwardShaderKeys.hpp) · [构建入口](../../../../Engine/CMakeLists.txt)

全配置扩展颜色预计激活五个核心贴图、两个扩展贴图及 DFG、反射 cube、阴影数组，共 10 个片元纹理单元。以编译后的实际阶段资源和 OpenGL 限额为准。只有实际消费新槽的角色登记上传和 ShaderRead，缺席槽复用已有缺省图像。核心和扩展实例的结构选择在 Draw 准备确定。Depth、SurfaceAux 和扩展色共用当前生效的数据版本，图中的资源使用由实际生存的角色决定。 [原生绑定计划](../../../../RHI/OpenGL/BindingPlan.cpp) · [角色准入](specs/shader-generation-reload/spec.md)

固定材质布局中的非活动尾槽复用首个已准备的纹理与 sampler。活动槽缺图时仍取得缺省白图，准备更长的前缀时按实际槽更新集合。独立 OPAQUE 辅助表面有单独的材质缓存键。它从粗糙度和法线准备绑定，以同一行已准备的表面图像填充基色绑定。图登记原生配方的实际图像并合并重复读。MASK 及颜色使用各自所需的真实基色。辅助顶点流只包含 MASK 基色、粗糙度和法线所需的附加 UV 及切线。

源重载默认 CPU 包预算为 16 MiB，条目上限为两套产品目录。按编码大小保留活动代与候选的容量，共享 `StaticForward` 的接口重编可交付完整候选。显式限额及只淘汰缓存独占包的持有合同继续按实际容量执行。 [源重载限额](../../../../Engine/Shader/Reload.hpp)

### 逐项分配介质与金属能量

在 `MaterialInputs` 里完成标准纹理乘数，`PixelParams` 保留介质和金属各自的 Fresnel 端点以及混合所需的金属度。定义 `f` 为按 IOR 得到的基准介质反射率乘以线性颜色并先限制到 1 的 RGB，定义 `w` 为 specular 因子与 A 通道的乘积。兼容 IOR=0 令基准反射率为 1。介质端点是 `(w×f,w)`，金属端点是 `(baseColor,1)`。同一个几何半角、GGX、Smith 和入射余弦供各波瓣使用。直接介质 Lambert 权重取 `1−maxRGB(F_dielectric)`。将完整介质响应和完整金属响应按 metallic 混合，可让中间金属度保留属于介质的漫反射预算。 [Khronos Specular 层合成](https://github.com/KhronosGroup/glTF/blob/7cee0fcddd5730e90d488bfb5aa2e52171483dde/extensions/2.0/Khronos/KHR_materials_specular/README.md#implementation) · [共享波瓣规范](specs/static-surface-evaluation/spec.md)

物理光复用现有 GGX 烘焙和感知粗糙度坐标。对于 DFG 样点 `R=B, G=A+B`，采用以下同一实时近似。其中 `E` 是单次散射方向反照率，`C` 是镜面多散射补偿。

```text
A = G - R
E_dielectric = A * (w * f) + R * w
E_metal = A * baseColor + R
C_dielectric = 1 + f * (1 / G - 1)
C_metal = 1 + baseColor * (1 / G - 1)
DiffuseBudget_dielectric = 1 - maxRGB(E_dielectric * C_dielectric)
```

`w` 作为介质层外部权重，因而补偿使用未乘 `w` 的 `f`。Direct Lighting 取单次直接散射，Physical Lighting 的直接镜面和 IBL 镜面各施加一次对应的 `C`。间接漫反射采用补偿后方向反照率的余量，使 IOR=0、白色因子与全强度的介质在白炉中给出全反射。金属镜面使用相同 DFG 编码和自己的参数，材质 AO、源标定、预曝光及 HDR 限幅沿现有聚合位置施加。该模型是本项目的实时近似，T01 通过独立参考锁定响应和允许误差。 [环境照明合同](specs/environment-lighting/spec.md) · [固定 Filament 对照](../../../../Docs/Reference/Filament/Lighting.md#f11)

多反弹漫反射 AO 使用完整介质的基色，介质与金属的各自响应随后按金属度混合。直接光的介质预算使用实际 LoH。有效半角位于着色法线下方时只取消镜面项，退化半角使介质预算采用掠射端点。

### 沿现有图和提交边界确认扩展颜色

`VisitOpaqueDrawShaders` 由有效 Draw 及 View 需求选出实际角色，Shader 代与材质代就绪后交给构图。按消费者准备新槽与绑定，独立表面路径只要求核心写入或 SurfaceAux 的配方。阴影或深度只读取其覆盖资源。把新增颜色槽的就绪检查和纹理图读限定到实际扩展颜色，防止图缺少颜色消费者时仍为颜色占用帧前额度。 [Draw 准备](../../../../Engine/Renderer/Draw/StaticDraw.cpp) · [图纹理读取](../../../../Engine/Renderer/Paths/Forward.cpp)

SurfaceAux 与辅助颜色按冻结 `SceneFrame` 的 Primitive 次序绘制辅助 Draw。主视图先画核心表面，辅助表面接续覆盖等深像素，仅合并相邻兼容的辅助 Draw。完整 Pipeline、几何页及绑定仍用于合批。辅助顺序独立于颜色角色、顶点流和纹理驻留。SurfaceAux 写最终 G3 分类和可见深度，扩展颜色使用等深读和加载后的 HDR。颜色角色只从已冻结的当前 View 取得灯表、Froxel、获准的阴影和环境版本。纯扩展受光 View 仍有辅助颜色所需的灯和环境准备。缺少匹配的颜色包、资源或设备额度时沿现有 Pending、拒绝和上一可读输出规则处理。冷包与热候选通过一致的角色、阶段和 ABI 检查，热发布以整代为单位。 [Deferred 合成合同](../../../specs/core-gbuffer/spec.md) · [颜色交付规格](specs/static-forward-rendering/spec.md)

Intel OpenGL 实机上，共面批次的命令次序一致时，紧接只读等深颜色批次的同一 FBO Draw 仍可能保留较早的颜色。原生发行器在这一批次边界排序片元域的 framebuffer 写入。屏障只依赖已成功发出的上一批次及实际 FBO，View 继续通过图声明跨 Pass 的版本和用途。验收对比冷暖辅助表面、正常三路径 HDR 以及配套 D32。 [OpenGL Draw](../../../../RHI/OpenGL/CommandListDraw.cpp)

## Risks / Trade-offs

- **共享材质表 stride 扩展** → 用生成布局和真实生产包核对顶点及片元消费者。用同源模型的不同覆盖验证行索引、绑定复用与上传账目。后续组合若把行大小变成已测瓶颈，再按对应工作包评估存储布局。
- **用核心 DFG 权重解释非单位 F90** → 用非对称的 DFG 数据、零强度和全反射白炉分别区分遗漏 F90、重复补偿和金属串扰。
- **扩展色未就绪影响可独立输出的表面** → 在真实模型上移除对应色角色，核对独立 SurfaceAux、普通颜色拒绝、合法恢复及热拒绝期间旧代提交。
- **三路相同但物理值错误** → 除跨路径比较，还需直接光解析样点、独立方向反照率参考及纹理通道对照。以 D11 的同口径记录配对核心基线，不把核心 GBuffer 编码容差直接套到共同前向着色的扩展区域。

## Migration Plan

生成并部署与当前 C++ 接口相匹配的一整套产品 Shader 包。应用启动按现有包版本和 ABI 契约处理旧包，开发热重载沿同运行 ABI 完整发布候选。已有 v1 场景以字段缺省继续读取，新 v1 场景保存当前实例覆盖，固定 glTF/GLB 仍提供原始模型数据。A 在独立 B 验收后才执行最终 verify、规格同步和归档。 [材质 ABI 合同](../../../../Docs/Architecture.md#plan-readiness) · [独立审查闭环](../../../../AGENTS.md#review-loop)
