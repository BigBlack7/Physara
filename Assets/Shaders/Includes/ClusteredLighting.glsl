#ifndef PHYSARA_CLUSTERED_LIGHTING_GLSL
#define PHYSARA_CLUSTERED_LIGHTING_GLSL

#include "FrameUniforms.glsl"

struct ClusterEntry
{
    uint offset;
    uint count;
};

layout(std430, binding = PHYSARA_BINDING_CLUSTER_ENTRIES)readonly buffer ClusterEntryBuffer
{
    ClusterEntry uClusterEntries[];
};

layout(std430, binding = PHYSARA_BINDING_CLUSTER_LIGHT_INDICES)readonly buffer ClusterLightIndexBuffer
{
    uint uClusterLightIndices[];
};

uint GetClusterDepthSlice(float viewDepth)
{
    float depth = clamp(viewDepth, uFrame.clusterGrid.depthParams.x, uFrame.clusterGrid.depthParams.y);
    float slice = floor(log(depth) * uFrame.clusterGrid.depthParams.z + uFrame.clusterGrid.depthParams.w);
    return min(uint(max(slice, 0.0)), uFrame.clusterGrid.dimensions.z - 1u);
}

uvec3 GetFragmentClusterCoordinate(vec3 worldPosition)
{
    uvec3 coordinate;
    coordinate.xy = min(
        uvec2(gl_FragCoord.xy) / max(uFrame.clusterGrid.dimensions.ww, uvec2(1u)),
        uFrame.clusterGrid.dimensions.xy - uvec2(1u));
    float viewDepth = max(-(uFrame.camera.view * vec4(worldPosition, 1.0)).z, uFrame.clusterGrid.depthParams.x);
    coordinate.z = GetClusterDepthSlice(viewDepth);
    return coordinate;
}

uint GetClusterIndex(uvec3 coordinate)
{
    return coordinate.x +
           uFrame.clusterGrid.dimensions.x * (coordinate.y + uFrame.clusterGrid.dimensions.y * coordinate.z);
}

ClusterEntry GetFragmentCluster(vec3 worldPosition)
{
    return uClusterEntries[GetClusterIndex(GetFragmentClusterCoordinate(worldPosition))];
}

vec3 VisualizeFragmentCluster(vec3 worldPosition, ClusterEntry cluster)
{
    uvec3 coordinate = GetFragmentClusterCoordinate(worldPosition);
    float slice = float(coordinate.z) / max(float(uFrame.clusterGrid.dimensions.z - 1u), 1.0);
    vec3 sliceColor = 0.5 + 0.5 * cos(6.2831853 * (slice + vec3(0.0, 0.33, 0.67)));
    float occupancy = clamp(float(cluster.count) / max(float(uFrame.clusterGrid.counts.z), 1.0), 0.0, 1.0);
    vec3 heat = vec3(
        smoothstep(0.35, 1.0, occupancy),
        smoothstep(0.0, 0.65, occupancy) - smoothstep(0.65, 1.0, occupancy),
        1.0 - smoothstep(0.0, 0.5, occupancy));
    vec2 tilePosition = mod(gl_FragCoord.xy, vec2(float(uFrame.clusterGrid.dimensions.w)));
    vec2 tileDistance = min(
        tilePosition,
        vec2(float(uFrame.clusterGrid.dimensions.w)) - tilePosition);
    float boundary = 1.0 - step(1.25, min(tileDistance.x, tileDistance.y));
    return mix(mix(sliceColor, heat, 0.65), vec3(1.0), boundary);
}

#endif