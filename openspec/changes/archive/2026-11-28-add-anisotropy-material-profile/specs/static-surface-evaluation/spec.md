# Spec Delta

## ADDED Requirements

### Requirement: Resolve the anisotropic axes in the effective surface frame

方向槽 SHALL 用自己的有效 UV、采样器、仿射变换及梯度进行线性采样。RG SHALL 从 [0,1] 映射为 [-1,1] 的二维方向并归一，B SHALL 乘材质强度得到有效强度 `s`。缺席方向槽或采样方向退化时，旋转前的方向取 +T。缺席方向槽的强度乘数为 1。采样取得的方向 SHALL 在 Primitive 的切线空间应用正向 `anisotropyRotation`，贴图变换作用于采样坐标。方向槽与基底 normal 采用不同有效 UV 时，二者 SHALL 按各自坐标取样并使用同一基础切线空间解释向量。

有效方向 SHALL 由已校验的原始 T、`cross(N,T) × tangent.w` 映射，经实例仿射的线性部分进入世界空间，再相对带 normal map 和面向的着色法线正交化。垂直粗糙度轴 SHALL 在同一半球框架内重建。基底 normal 的纹理变换、实例 determinant 及背面处理 SHALL 与既有几何合同一致。插值或法线投影使各向异性轴不可定义的像素 SHALL 使用有限的等向基底响应。

#### Scenario: Distinguish image addressing from material direction rotation

- **WHEN** 基底 normal 使用 UV0，方向图以变换 UV1 采样不同的 RG/B 纹素，并改变一次材质旋转
- **THEN** 方向图读到预计的线性通道，+T 或已编码方向按正弧度朝 +B 转动。纹理变换与材质旋转分别反映在采样位置和高光轴

#### Scenario: Follow mirrored corners and non-uniform instance axes

- **WHEN** 带前后可见标记的拉丝标样使用斜向 RG 或材质角，在独立改变 UV 手性、负 determinant 和非均匀缩放的实例上取样，并用非平坦的基底 normal 对照
- **THEN** 正、背面高光轴与各自几何和着色法线的世界方向相符，镜像接缝和实例变换按各自的手性起作用，面向只作用一次

### Requirement: Evaluate the anisotropic GGX base response

各向异性镜面 SHALL 在共享感知粗糙度过滤和下限之后，从 `alpha=p²`、`alphaT=mix(alpha,1,s²)`、`alphaB=alpha` 得到两轴参数。方向、垂直轴和同一个着色法线的投影 SHALL 输入各向异性 GGX NDF 与高度相关 Smith visibility。visibility 含微表面分母，光照外层乘一次入射余弦。Fresnel、漫反射、金属混合、发射、光衰减及阴影继续按对应核心或 IOR/specular 基底合同求值。

`s=0` 或两轴粗糙度相等时，镜面 SHALL 收敛到对应的等向响应。粗糙度下限、掠射与退化半角 SHALL 交付有限结果，并沿用共享半球判断。

#### Scenario: Compare distinguishable anisotropic light samples and isotropic limits

- **WHEN** 固定同一几何、灯光和 F0/F90，比较方向强度中间值、旋转四分之一圈及零强度，并在最小与最大粗糙度取代表性正向和掠射样点
- **THEN** 两主轴光瓣与采用标准双轴粗糙度的独立数值参考一致，旋转交换主轴，零强度和等轴边界恢复等向结果，合法边界输出有限
