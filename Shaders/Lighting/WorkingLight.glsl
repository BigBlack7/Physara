// RGBA16F 工作域共用的位宽控制. 指数缩放保留可表示的超白值和逐通道亮度差.
const float MaxWorkingLight = 65504.0;

// 先按通道合并二进制指数, 再将结果限制到工作附件的有限上界.
vec3 ScaleWorkingLight(vec3 value, ivec3 exponent)
{
    ivec3 normalization;
    vec3 mantissa = frexp(value, normalization);
    ivec3 bounded = clamp(normalization + exponent, ivec3(-126), ivec3(17));
    return min(ldexp(mantissa, bounded), vec3(MaxWorkingLight));
}

vec3 ScaleWorkingLight(vec3 value, int exponent)
{
    return ScaleWorkingLight(value, ivec3(exponent));
}

// 将源 RGB, 波瓣权重和场景倍率的尾数分别合入, 避免亮源和小倍率在缩放前越出 FP32.
vec3 ScaleEnvironmentLight(vec3 radiance, vec3 response, float mantissa, ivec3 exponent)
{
    ivec3 lightExponent;
    ivec3 responseExponent;
    vec3 light = frexp(radiance, lightExponent);
    vec3 factor = frexp(response, responseExponent);
    return ScaleWorkingLight(light * factor * mantissa, lightExponent + responseExponent + exponent);
}
