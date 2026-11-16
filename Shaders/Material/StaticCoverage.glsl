// 槽变换已在 CPU 合成 scale/rotation. w 记录 UV 集, 负值表示缺席.
bool HasMaterialTexture(int slot)
{
    return material.uvX[slot].w >= 0.0;
}

// 使用有效 UV 集, CPU 打包的两行分别应用 scale, rotation 和 offset 的合成仿射.
vec2 MaterialUv(int slot)
{
    vec2 uv = material.uvX[slot].w == 0.0 ? vUv0 : vUv1;
    vec3 inputUv = vec3(uv, 1.0);
    return vec2(dot(material.uvX[slot].xyz, inputUv), dot(material.uvY[slot].xyz, inputUv));
}

// SRGB 纹理由采样器解码 RGB, alpha 保持覆盖率; 因子和顶点色已经在线性域.
vec4 SampleBaseColor()
{
    vec4 texel = HasMaterialTexture(0) ? texture(sampler2D(baseColorImage, baseColorSampler), MaterialUv(0))
                                      : vec4(1.0);
    return material.baseColor * vColor * texel;
}

// 同一阈值和真采样 alpha 同时控制 Depth 和 Color. 恰等于阈值的片元保留.
void ApplyCoverage(float alpha)
{
    if (material.masked != 0u && alpha < material.alphaCutoff)
    {
        discard;
    }
}
