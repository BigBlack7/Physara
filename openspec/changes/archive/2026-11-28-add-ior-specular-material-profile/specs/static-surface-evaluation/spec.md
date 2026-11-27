# Spec Delta

## ADDED Requirements

### Requirement: Derive the shared opaque IOR and specular response

IOR/specular 表面 SHALL 从同一有效材质和几何求得基础波瓣。介质强度为 `specularFactor × specularTexture.A`，线性介质颜色为 `specularColorFactor × decodeSrgb(specularColorTexture.RGB)`，缺席纹理的乘数为 1。普通 IOR 的基准反射率为 `((ior−1)/(ior+1))²`。兼容 IOR=0 的基准 Fresnel 恒为 1，再应用 specular 强度和颜色。介质 F0 为基准反射率乘颜色、逐通道限制上界为 1 后乘强度，F90 为强度。金属波瓣的 F0 来自有效 baseColor，F90 为 1。

直接光 SHALL 用介质 Fresnel 最大颜色分量的补数缩放介质 Lambert 项，并按有效 metallic 混合完整的介质及金属响应。金属镜面贡献独立于介质 IOR、specular 强度与颜色。法线、粗糙度过滤、微表面量纲、几何覆盖和预曝光沿各自的共享合同。数值调节 SHALL 作用于同一已启用的材质 profile。

#### Scenario: Resolve dielectric endpoints and the metal boundary
- **WHEN** 相同几何受固定直接光照射，分别使用缺省扩展值、不同 IOR、有色 specular、零强度及 IOR=0，且有纯介质、纯金属和一个中间金属度的对照
- **THEN** 可辨别的正入射与掠射样点符合标准端点和介质能量分配，零强度给出介质漫反射，兼容值产生指定的角度响应。纯金属输出与介质参数无关，中间金属度按两完整波瓣的比例响应
