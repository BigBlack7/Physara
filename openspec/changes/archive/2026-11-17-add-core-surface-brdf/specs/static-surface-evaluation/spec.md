# Static Surface Evaluation Spec Delta

## ADDED Requirements

### Requirement: Derive one core metallic-roughness surface response

核心静态表面 SHALL 从有效的线性基色、金属度及感知粗糙度派生共享着色参数。漫反射色为 `baseColor × (1−metallic)`，`F0 = mix(0.04, baseColor, metallic)`，核心 `F90 = 1`。连续因子变化 SHALL 通过材质数据生效，同一绘制布局 SHALL 使用同一份求值和着色程序。材质通道预览中的 G 保留贴图与因子的感知粗糙度乘积。

#### Scenario: Use dielectric, metallic and mixed values from one material layout

- **WHEN** 同一布局对同一非白基色分别使用金属度 0、1 和一个中间值
- **THEN** 介质的 F0 为 0.04 且漫反射包含完整基色，金属的 F0 等于基色且漫反射为零，中间值按线性金属度混合，各次求值使用同一程序

### Requirement: Filter geometric roughness once for core shading

核心粗糙度 SHALL 基于单位几何法线的屏幕导数进行过滤，在选定精度下将过滤后的感知粗糙度限制在正下限至 1，再平方得到微表面粗糙度。光栅片元的导数 SHALL 在逐片元覆盖分歧前取得。着色法线 SHALL 使用已声明的静态切线与双面合同。过滤所得参数 SHALL 供后续求值直接消费一次。

#### Scenario: Broaden a highlight from geometric normal variation

- **WHEN** 两个样点有相同的输入粗糙度，其中一个位于平坦几何，另一个的几何法线有已知屏幕变化量，且样点的输入粗糙度足以观察过滤差异
- **THEN** 有法线变化的样点得到可由所选过滤模型计算的更大微表面粗糙度，材质通道预览仍显示原感知粗糙度，MASK 的可见轮廓保持覆盖合同

### Requirement: Evaluate the shared core BRDF in a defined measure

核心镜面响应 SHALL 使用 GGX NDF、完整的 height-correlated Smith visibility 和 Schlick Fresnel，漫反射 SHALL 使用 Lambert。响应以单位立体角的 BRDF 值交付，Smith visibility 含微表面分母，入射余弦由外层光照或半球积分乘一次。入射方向在着色表面半球外时贡献为零。支持的粗糙度下限、掠射方向和未定义半角 SHALL 产生有限结果。

#### Scenario: Resolve roughness and angular limits

- **WHEN** 核心表面输入粗糙度为 0 或 1，且使用有效法线，分别在正入射、掠射、表面下方和半角退化的方向上求值
- **THEN** 结果分别符合有下限的 GGX 响应、共享掠射约定和零半球外贡献，退化半角的镜面贡献为零，各分量有限

### Requirement: Apply the core DFG encoding and energy response

对于与样点的视角和过滤后感知粗糙度匹配的有效 Filament DFG 值，核心模型 SHALL 以 `R=B`、`G=A+B` 解释 split-sum 系数，取 `E=mix(R,G,F0)`。镜面多散射近似 SHALL 使用 `C=1+F0×(1/G−1)`，间接漫反射预算 SHALL 使用 `1−E`。共享求值 SHALL 交付明确的单次散射和对应能量项，补偿的镜面响应使用一次 C。

#### Scenario: Interpret distinct directional reflectance channels

- **WHEN** 一个核心表面提供来自同一模型的有效 DFG 样点，R 和 G 不同且 F0 的至少一个颜色分量在 0 与 1 之间
- **THEN** 方向反照率、镜面补偿和间接漫反射预算分别符合该 R/G 编码与 F0，各颜色分量可独立对照 CPU 参考

#### Scenario: Integrate a perfectly reflective rough metal in a white furnace

- **WHEN** 理想白色金属在单位均匀半球入射辐亮度下，使用匹配视角和粗糙度的有效 DFG 样点求值
- **THEN** 漫反射为零，单次散射方向反照率符合独立参考积分，补偿后积分在约定容差内接近 1
