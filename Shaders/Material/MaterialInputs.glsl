#ifndef PHYSARA_MATERIAL_INPUTS_GLSL
#define PHYSARA_MATERIAL_INPUTS_GLSL

// 取样层交给派生参数层的核心 glTF 通道, 不携带资源绑定, 灯或视向.
// 数据来自已验证的材质因子和纹理/顶点色乘积; 各字段有限且在 [0, 1] 内.
// baseColor.rgb 为线性颜色, alpha 保留覆盖率; 不预乘 alpha 或曝光.
// perceptualRoughness 保留原始感知粗糙度, 过滤归 PixelParams; occlusion 保留材质 AO, 不烘入其它通道.
struct MaterialInputs
{
    vec4 baseColor;
    float perceptualRoughness;
    float metallic;
    float occlusion;
};

#endif
