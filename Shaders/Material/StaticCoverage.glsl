// 纹理和 sampler 固定在批次绑定中. 两个片元入口用同一 flat 行号取 UV 变换, 基色因子和覆盖阈值.
#define PHYSARA_FORWARD_MATERIAL (materialRows.records[vMaterialRow])

// dx 和 dy 分别沿屏幕 x 和 y 方向求导. 每个向量的 xy 属于 UV0, zw 属于 UV1.
struct MaterialUvGradients
{
    vec4 dx;
    vec4 dy;
};

// 先对两个插值 UV 求导, 再让每个实例选择 UV 和变换. 调用点须在材质分歧和 discard 之前.
MaterialUvGradients CaptureMaterialUvGradients()
{
    vec4 uv = vec4(vUv0, vUv1);
    return MaterialUvGradients(dFdx(uv), dFdy(uv));
}

// 槽变换已在 CPU 合成 scale/rotation. w 记录 UV 集, 负值表示缺席.
bool HasMaterialTexture(int slot)
{
    return PHYSARA_FORWARD_MATERIAL.uvX[slot].w >= 0.0;
}

// 单个材质槽的采样坐标及两个屏幕方向上的坐标变化量, 可一起交给 textureGrad.
struct MaterialUvSample
{
    vec2 uv;
    vec2 dx;
    vec2 dy;
};

// CPU 打包的行给出当前材质的 UV 仿射. 平移只影响坐标, 线性部分变换梯度.
MaterialUvSample MaterialUv(int slot, MaterialUvGradients gradients)
{
    vec4 x = PHYSARA_FORWARD_MATERIAL.uvX[slot];
    vec3 y = PHYSARA_FORWARD_MATERIAL.uvY[slot].xyz;
    bool first = x.w == 0.0;
    vec3 inputUv = vec3(first ? vUv0 : vUv1, 1.0);
    vec2 dx = first ? gradients.dx.xy : gradients.dx.zw;
    vec2 dy = first ? gradients.dy.xy : gradients.dy.zw;
    return MaterialUvSample(vec2(dot(x.xyz, inputUv), dot(y, inputUv)),
                            vec2(dot(x.xy, dx), dot(y.xy, dx)),
                            vec2(dot(x.xy, dy), dot(y.xy, dy)));
}

// SRGB 纹理由采样器解码 RGB, alpha 保持覆盖率; 因子和顶点色已经在线性域.
vec4 SampleBaseColor(MaterialUvGradients gradients)
{
    vec4 texel = vec4(1.0);
    if (HasMaterialTexture(0))
    {
        MaterialUvSample sampleUv = MaterialUv(0, gradients);
        texel = textureGrad(sampler2D(baseColorImage, baseColorSampler), sampleUv.uv, sampleUv.dx, sampleUv.dy);
    }
    return PHYSARA_FORWARD_MATERIAL.baseColor * vColor * texel;
}

// 同一阈值和真采样 alpha 同时控制 Depth 和 Color. 恰等于阈值的片元保留.
void ApplyCoverage(float alpha)
{
    if (PHYSARA_FORWARD_MATERIAL.masked != 0u && alpha < PHYSARA_FORWARD_MATERIAL.alphaCutoff)
    {
        discard;
    }
}
