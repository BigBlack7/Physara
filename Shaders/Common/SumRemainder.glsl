#ifndef PHYSARA_SUM_REMAINDER_GLSL
#define PHYSARA_SUM_REMAINDER_GLSL

// 顶点仿射求值与片元相对距离共用的 TwoSum, 各分量独立处理.
// 调用方传入同一对 a, b 相加所得的有限 FP32 舍入值 sum, 将返回的余量合入低位.
// precise 保持重建两个加数各自舍入误差所需的运算顺序.
vec3 SumRemainder(vec3 a, vec3 b, vec3 sum)
{
    precise vec3 roundedB = sum - a;
    precise vec3 error = (a - (sum - roundedB)) + (b - roundedB);
    return error;
}

#endif
