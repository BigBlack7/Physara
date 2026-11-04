# D12–D13 · 材质扩展、透明与透射

[开发计划与当前阶段](Developments.md#current-stage) · [D12](#dev-d12) · [D13](#dev-d13)

<a id="dev-d12"></a>
## D12 · 完成标准不透明材质扩展

**性质：** 基线。**入口：** [D11](D08-D11-RenderPaths.md#dev-d11) 的阶段出口通过。

**阶段目标：** 沿标准 glTF 扩展、共享物理库和 ForwardExtension 路由逐项闭环，控制组合与能量近似。

<a id="d12-t01"></a>
### D12-T01 · 实现 IOR 与 specular profile

**职责位置：** Engine/Asset、Renderer/Materials、Shaders/Material/BRDF/Lighting。**前置：** 本阶段入口满足。**架构依据：** [8.5](../Architecture.md#materials)、[4.4](../Architecture.md#physical-inputs)、[8.6](../Architecture.md#lighting)。

- [ ] 接入 KHR_materials_ior/specular 的规范默认值、因子/颜色/纹理与扩展关系，扩充权威 ABI 和 feature key。
- [ ] 在共享 PixelParams 中计算 dielectric Fresnel 与能量分配，同时完成直接光和环境响应；核对标准 specular 的 F0/F90 适配，以及金属分支不受 dielectric weight 错误调制。
- [ ] 接入 ForwardExtension、Inspector 和相同静态几何/alpha，验证不同 IOR/specular 与三路径 HDR 一致性。

**完成条件：** 扩展通过完整输入到输出链，不只改变某条路径的直接光高光。

<a id="d12-t02"></a>
### D12-T02 · 实现 clearcoat 叠层 profile

**职责位置：** Asset、Materials、Shaders/Material/BRDF/Lighting。**前置：** [D12-T01](#d12-t01)。**架构依据：** [8.5](../Architecture.md#materials)、[8.6](../Architecture.md#lighting)、[8.2](../Architecture.md#geometry)。

- [ ] 导入 clearcoat 因子、roughness、独立 normal 与纹理变换，生成有限 profile 和 GPU 参数。
- [ ] 实现上层微表面与基底衰减、直接光及配套 IBL，核对双法线、几何可见性和 clearcoat 对 emission 的叠层衰减。
- [ ] 验证参考材质、法线极值、base+coat 能量近似及 Deferred SurfaceAux 覆盖，补 Inspector/历史失效回归。

**完成条件：** 清漆层在三路径和两类光照中有一致物理语义，扩展不增加全局 GBuffer 附件。

<a id="d12-t03"></a>
### D12-T03 · 实现 sheen 与配套环境响应

**职责位置：** Asset、Materials、Shaders/BRDF/Lighting、EnvironmentCache。**前置：** [D12-T01](#d12-t01)。**架构依据：** [8.5](../Architecture.md#materials)、[8.6](../Architecture.md#lighting)。

- [ ] 导入 sheen color/roughness 与标准交互关系，接入共享参数、纹理与 UI 编辑。
- [ ] 实现 Charlie/Neubelt 及固定参考的叠层/能量处理，准备并验证对应 DFG/IBL 数据。
- [ ] 验证布料参考、极端 roughness、其他层组合、路由和几何各出口，记录所采用实时近似的边界。

**完成条件：** sheen 不借用无配套环境项的普通 GGX 高光，不宣称具备未设计的完整皮下散射。

<a id="d12-t04"></a>
### D12-T04 · 实现 anisotropy 与方向语义

**职责位置：** Asset、Materials、Shaders/Geometry/BRDF/Lighting。**前置：** [D12-T01](#d12-t01)。**架构依据：** [8.5](../Architecture.md#materials)、[8.6](../Architecture.md#lighting)、[8.2](../Architecture.md#geometry)。

- [ ] 导入 anisotropy 强度/方向/纹理并校验切线依赖；按架构适配标准 glTF 与 Filament 的参数化，统一 UV 变换和切线/副切线语义，不直接复制同名参数。
- [ ] 实现各向异性微表面、粗糙度与方向处理，完成匹配的直接光和 IBL 近似。
- [ ] 验证旋转、镜像 UV、负缩放、非均匀缩放和缺失切线的可见行为，补路由、Inspector 及参考图。

**完成条件：** 方向随正确几何空间变化，三路径表现一致且默认 isotropic 材质成本不被无谓增加。

<a id="d12-t05"></a>
### D12-T05 · 实现 iridescence 薄膜 profile

**职责位置：** Asset、Materials、Shaders/Material/BRDF/Lighting。**前置：** [D12-T01](#d12-t01)。**架构依据：** [8.5](../Architecture.md#materials)、[8.6](../Architecture.md#lighting)。

- [ ] 导入薄膜因子、IOR、厚度参数/纹理与规范交互，建立单位及合法值检查；允许 iridescenceThicknessMinimum 大于 Maximum，不将合法反向插值误判为非法范围。
- [ ] 实现固定参考的薄膜 Fresnel 实时近似并配合基底/其他层的顺序，同时覆盖直接光和 IBL。
- [ ] 验证厚度、角度、金属/介质变化与组合参考，接入 ForwardExtension、编辑及静态几何出口测试。

**完成条件：** 薄膜近似与扩展支持边界明确，shader 组织保持共享模型层次。

<a id="d12-t06"></a>
### D12-T06 · 完成扩展组合与能力回归

**职责位置：** Engine/Asset、Renderer、Editor、Engine/Shader。**前置：** [D12-T02](#d12-t02)、[D12-T03](#d12-t03)、[D12-T04](#d12-t04)、[D12-T05](#d12-t05)。**架构依据：** [8.5](../Architecture.md#materials)、[12.1](../Architecture.md#tests)、[5.7](../Architecture.md#plan-readiness)。

- [ ] 按标准排斥/叠层规则建立有限合法组合与非法组合场景，覆盖已支持 texture_transform/emissive_strength/unlit。
- [ ] 在三路径中比较直接光、IBL、阴影、SurfaceAux 遮挡、depth；核对变体规模及绑定 limits。
- [ ] 更新导入/渲染 capability 与面板，启用扩展需要完整就绪；编译失败保持旧有效代而不半发布。

**完成条件：** 单项扩展与声明可用组合有真实证据；未声明组合有明确处理，后续透射复用同一模型合同。

**D12 阶段出口：** 目标不透明扩展均完成导入、ABI、直接光、IBL、三路径路由、覆盖/历史失效 和参考图；能力表只声明已通过的组合。

<a id="dev-d13"></a>
## D13 · 完成透明覆盖和物理透射

**性质：** 基线。**入口：** [D12](#dev-d12) 的阶段出口通过。

**阶段目标：** 把核心 alpha blend 与 transmission/volume/dispersion 放入统一 HDR 合成，维持表面、历史和选择语义。

<a id="d13-t01"></a>
### D13-T01 · 实现稳定排序 alpha blend

**职责位置：** Renderer/Draw、Paths、Shaders/Material/Passes。**前置：** 本阶段入口满足。**架构依据：** [5.6](../Architecture.md#draws)、[8.8](../Architecture.md#transparency)、[8.2](../Architecture.md#geometry)。

- [ ] 完成核心 alphaMode=BLEND 的稳定远到近排序、透明 depth/write/blend 和双面语义，共用物理材质与灯光。
- [ ] 限定只合并相邻兼容透明项，保留排序 tie-breaker；记录排序透明的固有限制及可见质量策略。
- [ ] 验证透明叠层、mask/blend 区别、静态实例、自发光、阴影接收与三路径结果；明确可见表面与 alpha 阈值的合同。

**完成条件：** 透明不因减少 bind 而破坏顺序，覆盖率 alpha 与光学透射率是不同参数。

<a id="d13-t02"></a>
### D13-T02 · 建立天空与折射背景 mip

**职责位置：** Renderer/Paths、Passes、RenderGraph。**前置：** [D13-T01](#d13-t01)。**架构依据：** [8.8](../Architecture.md#transparency)、[9.1](../Architecture.md#temporal-contract)、[8.6](../Architecture.md#lighting)。

- [ ] 固定不透明+ForwardExtension+天空的组合位置，生成折射用预曝光 HDR 背景及其 mip。
- [ ] 按折射背景的真实采样声明图访问和 mip 生成依赖；SSR 的 ReflectionHdr 与历史深度随 D14-T04 首次接入。
- [ ] 检查背景资源活性、曝光改变、resize/隐藏恢复、在途资源退休及采样层级，保证折射不会读写同一颜色版本。

**完成条件：** 折射消费已完成的不透明/扩展/天空背景，背景 mip 与曝光域正确；不提前分配尚无消费者的时域历史。

<a id="d13-t03"></a>
### D13-T03 · 实现 transmission 与折射路由

**职责位置：** Asset、Materials、Shaders/Material/Lighting、Renderer/Paths。**前置：** [D13-T02](#d13-t02)。**架构依据：** [8.5](../Architecture.md#materials)、[8.8](../Architecture.md#transparency)、[8.6](../Architecture.md#lighting)。

- [ ] 导入 transmission 因子/纹理及 IOR 依赖，设置 Refractive 路由、背景采样输入和能力状态。
- [ ] 实现反射/透射能量分配、直接光 BTDF、粗糙折射背景 mip 和屏幕外环境退路；直接光、背景与 IBL 一起验收，背景采样沿一致预曝光域。
- [ ] 验证 coverage/optical transmission 分离、背景遮挡/越界、直接光/IBL 与三路径一致，补 Inspector/覆盖/历史失效。

**完成条件：** 透射不是简单降低 alpha，屏幕近似和有限背景信息的限制在 profile 中清楚定义。

<a id="d13-t04"></a>
### D13-T04 · 实现 volume 吸收与厚度

**职责位置：** Asset、Materials、Shaders/Material/Lighting。**前置：** [D13-T03](#d13-t03)。**架构依据：** [8.5](../Architecture.md#materials)、[8.8](../Architecture.md#transparency)。

- [ ] 导入 thickness、attenuationColor/Distance 与依赖关系，定义长度单位和无穷/零边界。
- [ ] 按 Beer–Lambert 实现路径长度/厚度的吸收近似；明确 mesh 空间厚度与 world 空间吸收距离的转换，验证实例缩放及 shear 限制，并与 transmission、折射方向和现有层组合。
- [ ] 验证标准体积材质、尺度变化、薄壁/厚物体、能量/颜色及三路径，补参数编辑和几何覆盖回归。

**完成条件：** 体积扩展的吸收与覆盖 alpha 分离，不声称屏幕折射已实现任意多次体散射。

<a id="d13-t05"></a>
### D13-T05 · 实现 dispersion 的有限质量 profile

**职责位置：** Asset、Materials、Shaders/Material/Lighting、View。**前置：** [D13-T04](#d13-t04)。**架构依据：** [8.5](../Architecture.md#materials)、[8.8](../Architecture.md#transparency)、[7.7](../Architecture.md#capabilities)。

- [ ] 导入 dispersion 参数和规范依赖，建立多波段 IOR/折射采样的有限质量等级。
- [ ] 结合 transmission/volume 完成色散、越界/缺背景退路和预曝光处理，明确多采样成本。
- [ ] 验证标准参考、强色散边缘、不同质量误差及三路径；UI 仅显示已完成 profile 和真实 effective 设置。

**完成条件：** 目标 glTF 透射扩展支持闭环，质量与成本可说明，未完成高档配置不宣称支持。

<a id="d13-t06"></a>
### D13-T06 · 整合静态透明链与跨路径回归

**职责位置：** Renderer/Passes、View、Editor。**前置：** [D13-T01](#d13-t01)、[D13-T02](#d13-t02)、[D13-T03](#d13-t03)、[D13-T04](#d13-t04)、[D13-T05](#d13-t05)。**架构依据：** [8.8](../Architecture.md#transparency)、[9.1](../Architecture.md#temporal-contract)、[8.2](../Architecture.md#geometry)、[12.1](../Architecture.md#tests)。

- [ ] 形成统一 CompositeOutput，固定折射与 Alpha Blend 的组装顺序及代表性深度合同；具体 temporal reactive/invalidity 资源在 TAA 首次消费时按需接入。
- [ ] 复核折射背景、覆盖、排序与图访问，检查没有非法颜色 feedback；后续拾取与 TAA 在各自消费者接入时选择保守策略。
- [ ] 覆盖静态透明/透射实例、相机移动与曝光切换、背景变化、排序交叉和资源重载，对三路径比较同一场景。

**完成条件：** 三路径共享同一透明组装，背景与覆盖合同清楚，当前渲染结果具有可验证的有限近似边界。

**D13 阶段出口：** 核心透明和标准透射扩展可编辑可渲染；三路径使用同一透明链，折射背景、预曝光、覆盖与组合顺序明确。
