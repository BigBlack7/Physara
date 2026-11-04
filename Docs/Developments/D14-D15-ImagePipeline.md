# D14–D15 · 屏幕空间效果、时域与显示

[开发计划与当前阶段](Developments.md#current-stage) · [D14](#dev-d14) · [D15](#dev-d15)

<a id="dev-d14"></a>
## D14 · 接入屏幕空间遮蔽、接触阴影与 SSR

**性质：** 基线。**入口：** [D13](D12-D13-Materials.md#dev-d13) 的阶段出口通过。

**阶段目标：** 在完整表面上接入三路径共享屏幕效果，并随 SSR 首次建立独立 ReflectionHdr 与成对历史深度。

<a id="d14-t01"></a>
### D14-T01 · 建立深度层级和共同屏幕输入

**职责位置：** Renderer/Passes、Shaders/ScreenSpace、RenderGraph。**前置：** 本阶段入口满足。**架构依据：** [8.4](../Architecture.md#surface-effects)、[8.2](../Architecture.md#geometry)、[6.1](../Architecture.md#paths)。

- [ ] 根据消费者需要请求完整 SurfaceOutput，核对 Forward 前置表面与 Deferred+SurfaceAux 的真实最近覆盖；SSR 启用时提供基底 eligibility，Unlit/各向异性及独立透明表面不冒充合法反射输入。
- [ ] 构建深度金字塔/位置重建及 normal/roughness 输入，reduction 读取主深度 convention。
- [ ] 提供深度/法线/层级 typed preview，验证 mip/尺寸/奇数分辨率、Reverse-Z 和遮挡边界。

**完成条件：** 屏幕效果不会在尚缺扩展表面的核心 GBuffer 上先执行。

<a id="d14-t02"></a>
### D14-T02 · 实现 Filament 式 SAO 与过滤

**职责位置：** Shaders/ScreenSpace、Renderer/Passes、Editor/Panels。**前置：** [D14-T01](#d14-t01)。**架构依据：** [8.4](../Architecture.md#surface-effects)、[8.6](../Architecture.md#lighting)。

- [ ] 实现按世界半径投影的 SAO 采样、bias、分辨率/质量 profile 和 depth/normal-aware 过滤，透视按深度缩放，正交使用固定像素/世界比例。
- [ ] 将结果接入间接光 AO/可选已有合同输入，区分材质 occlusion、屏幕 AO 与直接光阴影。
- [ ] 验证接触/薄物体/背景、camera scale、扩展遮挡、关闭等于中性输入和三路径结果。

**完成条件：** AO 的作用域与近似明确，未遮挡像素不因重复乘 AO 莫名变暗。

<a id="d14-t03"></a>
### D14-T03 · 实现屏幕接触阴影

**职责位置：** Shaders/ScreenSpace/Shadow、Renderer/Lighting/Passes。**前置：** [D14-T01](#d14-t01)。**架构依据：** [8.4](../Architecture.md#surface-effects)、[8.3](../Architecture.md#shadows)、[8.6](../Architecture.md#lighting)。

- [ ] 实现有限步数深度追踪、厚度/偏移和质量边界，定义与 shadow-map 可见性组合的规则。
- [ ] 接入相应灯/材质可见性输入和中性退路；离屏或深度不足不伪造完整场景阴影。
- [ ] 验证薄片、屏幕边界、自遮挡、静态实例和不同分辨率，暴露步数/质量及成本。

**完成条件：** 接触阴影补充已有 shadow map，算法边界和屏幕信息缺失可解释。

<a id="d14-t04"></a>
### D14-T04 · 实现前帧辐亮度 SSR 基线

**职责位置：** Shaders/ScreenSpace、Renderer/Passes、View histories。**前置：** [D14-T01](#d14-t01)、[D14-T02](#d14-t02)。**架构依据：** [8.4](../Architecture.md#surface-effects)、[8.6](../Architecture.md#lighting)、[9.1](../Architecture.md#temporal-contract)。

- [ ] 首次接入静态深度与相机矩阵重投影，建立 ReflectionHdr 及与颜色同次成功提交的历史深度/相机/曝光；从不透明+扩展+天空之后、透明之前导出，内容版本改变、首帧或 camera cut 时失效。
- [ ] 实现当前表面投影深度 ray march、交点/厚度、置信度与历史深度拒绝；按 Filament 思路生成反射辐亮度 Gaussian RGBA mip，roughness/距离选择 LOD，并回退到匹配的 IBL。
- [ ] 按当前/历史 preExposure 比重标定，处理 disocclusion、history invalid、越界和 roughness，回退到匹配的 IBL；基线粗糙 SSR 只支持透视，正交请求显示不支持而非套用透视 LOD。
- [ ] SSR 作为预曝光入射辐亮度只替换合法各向同性基底的环境项，保留上层衰减及独立清漆/Sheen IBL，各向异性基底回退自身环境近似；不重复乘环境 intensity，检查镜面方向、曝光跃迁、纯相机移动及静态内容编辑后的失效。

**完成条件：** SSR 只使用正确历史角色，首帧和缺失样本没有未初始化读或镜头/UI 反馈。

<a id="d14-t05"></a>
### D14-T05 · 完成屏幕效果跨路径与压力验证

**职责位置：** Renderer、Editor/Diagnostics、Runtime。**前置：** [D14-T02](#d14-t02)、[D14-T03](#d14-t03)、[D14-T04](#d14-t04)。**架构依据：** [12.1](../Architecture.md#tests)、[8.4](../Architecture.md#surface-effects)、[12.2](../Architecture.md#benchmark)。

- [ ] 固定核心/扩展/Unlit 前后遮挡、透明背景、相机运动和曝光切换的回归，比较三路径 HDR。
- [ ] 覆盖奇数尺寸、render scale、效果切换、多 View 暂停恢复与在途资源重建，确认关闭效果可裁剪相关 Pass。
- [ ] 测量 depth hierarchy、AO、contact、SSR 各自成本与质量，Hi-Z traversal 优化另走可选任务。

**完成条件：** 基线屏幕效果具备输入完整性、历史安全、质量等级和独立成本证据。

**D14 阶段出口：** SAO、接触阴影、SSR 在三路径同输入下结果一致；扩展/Unlit 正确遮挡，历史失效/越界有正确退路，效果不污染能量与曝光语义。

<a id="dev-d15"></a>
## D15 · 完成时域、镜头和显示链

**性质：** 基线。**入口：** [D14](#dev-d14) 的阶段出口通过。

**阶段目标：** 把 HDR 合成、基础高度雾、TAA、物理 DoF、Bloom 和完整显示设置组织为唯一后处理链。

<a id="d15-t01"></a>
### D15-T01 · 完善 HDR 历史准入与失效协议

**职责位置：** View、Renderer/PostProcess、Shaders/PostProcess。**前置：** 本阶段入口满足。**架构依据：** [9.1](../Architecture.md#temporal-contract)、[11.2](../Architecture.md#history-commit)。

- [ ] 统一 camera cut、尺寸/路径/投影/曝光、RenderContentRevision 及质量变更的历史兼容判定；纯相机 pose 变化不递增静态内容版本，继续使用相机重投影。
- [ ] 核对 CompositeOutput 的静态代表性深度、透明 reactive/invalidity 和曝光比，保证所需元数据随历史版本一起发布。
- [ ] 测试 NoSceneOutputs、最小化、暂停 View、多视口不同节奏和曝光跃迁，只在成功场景提交时轮转。

**完成条件：** 所有 temporal 消费者可判断历史是否可用，不拼接失败帧、不同内容版本或另一 View 的相机/深度历史。

<a id="d15-t02"></a>
### D15-T02 · 实现 TAA 前高度雾合成

**职责位置：** Renderer/PostProcess、Shaders/PostProcess、Editor/Panels。**前置：** [D15-T01](#d15-t01)。**架构依据：** [9.1](../Architecture.md#temporal-contract)、[9.4](../Architecture.md#display-output)。

- [ ] 定义架构基线高度雾的物理/近似参数、空间与曝光域，使用明确的代表性场景深度。
- [ ] 实现 HDR 合成并放入 TAA 之前，结合透明/reactive 处理和中性关闭输入；明确多层透明与离屏信息的近似。
- [ ] 验证相机高度/距离、曝光、天空/透明、三路径及历史稳定性，接入参数命令和可见质量设置。

**完成条件：** 高度雾在唯一后处理链的固定位置工作，单位、历史影响与近似边界可验证。

<a id="d15-t03"></a>
### D15-T03 · 实现 HDR TAA 重投影与累积

**职责位置：** Renderer/PostProcess、Shaders/PostProcess、View。**前置：** [D15-T01](#d15-t01)、[D15-T02](#d15-t02)。**架构依据：** [9.2](../Architecture.md#taa)、[9.1](../Architecture.md#temporal-contract)、[6.1](../Architecture.md#paths)。

- [ ] 实现 jitter 序列与投影、重建/重投影、深度 disocclusion、neighborhood clamp 和历史权重。
- [ ] 建立独立的 TaaHdr 及成对历史深度，相机/曝光/jitter 与颜色来自同次成功提交；静态内容改变时失效历史，纯相机移动保留重投影资格，历史在 DoF/Bloom 前导出。
- [ ] 接入当前透明的代表性深度与 reactive/invalidity、曝光重标定及无效退路，不增加逐对象 motion/前态；多层透明无法精确对应单层深度时保守降低历史权重。
- [ ] 以静态细节、camera cut、相机移动/透明、曝光和路径切换序列检查 ghosting、抖动与收敛，给出允许误差。

**完成条件：** TAA 是三路径共同后处理，ReflectionHdr 与 TaaHdr 保持各自固定生产点。

<a id="d15-t04"></a>
### D15-T04 · 实现物理景深 DoF

**职责位置：** Renderer/PostProcess、Shaders/PostProcess、Editor/Panels。**前置：** [D15-T03](#d15-t03)。**架构依据：** [9.3](../Architecture.md#lens-effects)、[4.3](../Architecture.md#camera)。

- [ ] 由传感器、焦距、光圈和对焦距离生成 CoC，统一米/镜头单位、像素尺度与符号。
- [ ] 实现 CoC/tile、近远景分离、gather/blur/compose 的有限质量方案，资源和用途由图声明。
- [ ] 验证焦平面、近/远遮挡、亮点和 render scale；基线只对透视相机提供薄透镜 DoF，正交请求显示不支持并保持有效配置可见，参数编辑不改变场景灯光。

**完成条件：** DoF 位于 TAA 后且不写入 TaaHdr/ReflectionHdr，物理参数对可观察景深有一致影响。

<a id="d15-t05"></a>
### D15-T05 · 实现 HDR Bloom 与亮度语义

**职责位置：** Renderer/PostProcess、Shaders/PostProcess、Editor/Panels。**前置：** [D15-T03](#d15-t03)。**架构依据：** [9.3](../Architecture.md#lens-effects)、[9.4](../Architecture.md#display-output)。

- [ ] 实现多尺度 downsample/upsample/compose 与质量等级，明确 threshold 的绝对/预曝光域及强度含义。
- [ ] 把 Bloom 放在正确镜头链位置，不创建 Light、不重复影响表面光照，不把发光材质自动当面积光源。
- [ ] 验证曝光改变、HDR 高亮、emissive_strength、奇数尺寸和关闭效果，测量资源/带宽成本。

**完成条件：** Bloom 的曝光与历史顺序正确，效果开关不会改变核心 PBR 物理输入。

<a id="d15-t06"></a>
### D15-T06 · 完善 ColorGrading、输出与 AA 兼容

**职责位置：** Renderer/PostProcess、View、Editor/Panels、Shaders/PostProcess。**前置：** [D15-T03](#d15-t03)、[D15-T04](#d15-t04)、[D15-T05](#d15-t05)。**架构依据：** [9.4](../Architecture.md#display-output)、[5.7](../Architecture.md#plan-readiness)、[conventions](../Architecture.md#conventions)。

- [ ] 完善设置变化驱动的 LUT、白平衡、创作调整和 gamut mapping，继续使用 D07 固定 ToneMapper profile。
- [ ] 完成 HDR→时域/镜头→ToneMap/ColorGrading→可选 FXAA/空间缩放→display-linear ViewOutput 的组装和中间输出。
- [ ] 正式启用三路径 None/FXAA/TAA 兼容矩阵，验证不同渲染/显示分辨率，明确 SDR sRGB 之外未承诺的输出 profile。

**完成条件：** 完整后处理只有一条权威顺序，AA/路径切换的 effective 配置真实可见。

<a id="d15-t07"></a>
### D15-T07 · 建立时域序列、镜头和颜色门槛

**职责位置：** Renderer、Editor。**前置：** [D15-T01](#d15-t01)、[D15-T02](#d15-t02)、[D15-T03](#d15-t03)、[D15-T04](#d15-t04)、[D15-T05](#d15-t05)、[D15-T06](#d15-t06)。**架构依据：** [12.1](../Architecture.md#tests)、[9.2](../Architecture.md#taa)、[9.3](../Architecture.md#lens-effects)、[conventions](../Architecture.md#conventions)。

- [ ] 以多帧序列验证相机移动、静态内容编辑、透明、SSR、曝光、camera cut、路径/AA 切换和隐藏恢复，既检查 HDR 也检查最终显示。
- [ ] 验证灰阶/高亮/gamut、DoF/Bloom 开关与 UI 线性合成，保证颜色转换和窗口编码只在定义位置发生。
- [ ] 压力测试历史/镜头资源在途 resize、取消、热重载和关闭，记录质量档位及独立 CPU/GPU 成本。

**完成条件：** 完整物理渲染到显示链可复现，三路径共享效果的等价性与时间连续性有证据。

**D15 阶段出口：** 三路径均支持 None/FXAA/TAA 的基础单样本 profile；曝光/相机重投影/历史正确，镜头与显示效果不进入错误历史角色或 UI 能量链。
