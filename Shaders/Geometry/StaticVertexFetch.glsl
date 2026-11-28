// Depth/Color 共用位置表达式与 invariant 约束, 保持前置深度的 Equal 比较所需一致性.
// 光照分别插值相对于世界基点的高位和余量. 每个实例从对象行取得自己的世界基点, 在图元内平坦传递.
// UV 保持 glTF 行序, 片元阶段完成法线归一化和切线正交化.
#include <Source/Geometry/LightingPosition.glsl>

invariant gl_Position;

// Dekker 乘积余量. product 是同一对因子已单独舍入的 FP32 乘积.
// 将两个有效数各拆为至多 12 位的高低部分, 用子乘积恢复余量. 位掩码保留符号和指数.
vec3 ProductRemainder(vec3 a, float b, vec3 product)
{
    const uint highMask = 0xfffff000u;
    vec3 aHigh = uintBitsToFloat(floatBitsToUint(a) & uvec3(highMask));
    float bHigh = uintBitsToFloat(floatBitsToUint(b) & highMask);
    precise vec3 aLow = a - aHigh;
    precise float bLow = b - bHigh;
    precise vec3 error = aLow * bLow - (((product - aHigh * bHigh) - aLow * bHigh) - aHigh * bLow);
    return error;
}

// 先求 M * position + (T - anchor) 的高位. 合入乘积、累加和 CPU 编码的余量后再拆为两部分.
// 世界基点只从仿射平移扣除. 相对偏移与片元处加回的基点在算术上共用同一个 FP32 值.
void FetchLightingOffset(ForwardObject object, out vec3 offset, out vec3 low)
{
    mat3 linear = mat3(object.worldFromLocal);
    precise vec3 x = linear[0] * aPosition.x;
    precise vec3 y = linear[1] * aPosition.y;
    precise vec3 z = linear[2] * aPosition.z;
    precise vec3 xy = x + y;
    precise vec3 xyz = xy + z;
    vec3 shift = object.lightingShiftHigh;
    precise vec3 shifted = xyz + shift;
    precise vec3 products = ProductRemainder(linear[0], aPosition.x, x) +
                            ProductRemainder(linear[1], aPosition.y, y) +
                            ProductRemainder(linear[2], aPosition.z, z);
    precise vec3 sums = SumRemainder(x, y, xy) + SumRemainder(xy, z, xyz) +
                        SumRemainder(xyz, shift, shifted);
    // products 和 sums 来自本次顶点运算. inputs 补回 CPU 编码矩阵系数和平移差时留下的低位.
    precise vec3 inputs = object.lightingLinearLow * aPosition + object.lightingShiftLow;
    precise vec3 remainder = (products + sums) + inputs;
    precise vec3 high = shifted + remainder;
    offset = high;
    low = SumRemainder(shifted, remainder, high);
}

// 两个法线槽各自按采样仿射变换原切线, 再经世界线性变换. 手性包含 UV 和实例的朝向.
vec4 TransformMaterialTangent(ForwardObject object, vec4 tangent, vec3 basis)
{
    vec3 bitangent = cross(aNormal, tangent.xyz) * tangent.w;
    vec3 mapped = tangent.xyz * basis.x + bitangent * basis.y;
    return vec4(mat3(object.worldFromLocal) * mapped, tangent.w * basis.z * object.orientation);
}

// 以实例记录定位对象和材质, 输出共用的裁剪位置及当前颜色角色所需的世界空间框架.
void FetchStaticVertex()
{
    // canonical 实例索引已包含本命令的首实例. 实例块给出对象页和材质页各自的行号.
    DrawInstanceData instance = drawInstances.records[gl_InstanceIndex];
    ForwardObject object = objectRows.records[instance.objectRow];
    vMaterialRow = instance.materialRow;
    vLightingAnchor = object.lightingAnchor;
    FetchLightingOffset(object, vLightingOffsetHigh, vLightingOffsetLow);
    // 主表面以眼点为原点, 阴影子视图以光空间投影原点为基准. 两者均使用对象行的高低位偏移.
    // LightingDelta 合回原点的编码余量和作差余差. 取反得到用于投影的米制相对位置.
    LightingPosition origin = LightingPosition(perView.originHigh, perView.originLow, perView.originResidual);
    LightingSurface surface = LightingSurface(vLightingAnchor,
        LightingPosition(vLightingOffsetHigh, vLightingOffsetLow, vec3(0.0)));
    gl_Position = perView.clipFromOrigin * vec4(-LightingDelta(origin, surface, 1.0), 1.0);
    vColor = aColor;
    vUv0 = aUv0;
    vUv1 = aUv1;
    vNormal = mat3(object.normalFromLocal) * aNormal;
    vTangent = vec4(0.0);
    if (materialRows.records[vMaterialRow].uvX[2].w >= 0.0)
    {
        // Q 的逆给出采样坐标第一轴在原单位切线框架中的方向.
        vec3 basis = materialRows.records[vMaterialRow].normalBasis.xyz;
        vTangent = TransformMaterialTangent(object, aTangent, basis);
    }
#ifdef PHYSARA_CLEARCOAT
    vCoatTangent = vec4(0.0);
    if (materialRows.records[vMaterialRow].clearcoat.x > 0.0 &&
        materialRows.records[vMaterialRow].uvX[ClearcoatNormalTextureIndex].w >= 0.0)
    {
        vec3 basis = materialRows.records[vMaterialRow].coatNormalBasis.xyz;
        vCoatTangent = TransformMaterialTangent(object, aCoatTangent, basis);
    }
#endif
#ifdef PHYSARA_ANISOTROPY
    // 两条原轴同时经过实例线性变换. 保留相对轴长, 片元旋转后的斜轴才能随非均匀缩放变化.
    mat3 worldLinear = mat3(object.worldFromLocal);
    vAnisotropyTangent = worldLinear * aTangent.xyz;
    vAnisotropyBitangent = worldLinear * (cross(aNormal, aTangent.xyz) * aTangent.w);
#endif
}
