// Depth/Color 共用位置表达式与 invariant 约束, 保持前置深度的 Equal 比较所需一致性.
// 矩阵来自当前相机和完整实例仿射, UV 保持 glTF 行序; 片元阶段完成法线归一化和切线正交化.
invariant gl_Position;

void FetchStaticVertex()
{
    gl_Position = perView.clipFromWorld * (perObject.worldFromLocal * vec4(aPosition, 1.0));
    vColor = aColor;
    vUv0 = aUv0;
    vUv1 = aUv1;
    vNormal = mat3(perObject.normalFromLocal) * aNormal;
    vTangent = vec4(0.0);
    if (material.uvX[2].w >= 0.0)
    {
        // Q 的逆给出采样坐标第一轴在原单位切线框架中的方向.
        vec3 localBitangent = cross(aNormal, aTangent.xyz) * aTangent.w;
        vec3 mappedTangent = aTangent.xyz * material.normalBasis.x + localBitangent * material.normalBasis.y;
        vTangent = vec4(mat3(perObject.worldFromLocal) * mappedTangent,
                        aTangent.w * material.normalBasis.z * perObject.orientation);
    }
}
