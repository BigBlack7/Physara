#version 450 core
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_samplerless_texture_functions : require
#include <Interface/Fragment.glsl>
#include <Source/PostProcess/DisplayImage.glsl>

/**
  G3D version of FXAA. See copyright and warranty statement below.

  G3D Innovation Engine http://casual-effects.com/g3d
  Copyright 2000-2018, Morgan McGuire
  All rights reserved
  Available under the BSD License
*/

/*============================================================================


                    NVIDIA FXAA 3.11 by TIMOTHY LOTTES
               (modified for G3D with bug fixes and PC GLSL preamble)

------------------------------------------------------------------------------
COPYRIGHT (C) 2010, 2011 NVIDIA CORPORATION. ALL RIGHTS RESERVED.
------------------------------------------------------------------------------
TO THE MAXIMUM EXTENT PERMITTED BY APPLICABLE LAW, THIS SOFTWARE IS PROVIDED
*AS IS* AND NVIDIA AND ITS SUPPLIERS DISCLAIM ALL WARRANTIES, EITHER EXPRESS
OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL NVIDIA
OR ITS SUPPLIERS BE LIABLE FOR ANY SPECIAL, INCIDENTAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES WHATSOEVER (INCLUDING, WITHOUT LIMITATION, DAMAGES FOR
LOSS OF BUSINESS PROFITS, BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION,
OR ANY OTHER PECUNIARY LOSS) ARISING OUT OF THE USE OF OR INABILITY TO USE
THIS SOFTWARE, EVEN IF NVIDIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
DAMAGES.

*/

// 上游 Filament 固定快照的 FXAA 3.11 opaque PC console / G3D 路径.
// 坐标以来源像素表示, A 是感知亮度, RGB 混合仍保持显示线性.
vec3 FilterDisplayEdge(vec2 center)
{
    float nw = SampleDisplayImage(center + vec2(-0.5, -0.5)).a;
    float sw = SampleDisplayImage(center + vec2(-0.5,  0.5)).a;
    float ne = SampleDisplayImage(center + vec2( 0.5, -0.5)).a;
    float se = SampleDisplayImage(center + vec2( 0.5,  0.5)).a;
    vec4 middle = SampleDisplayImage(center);
    float low = min(min(nw, sw), min(ne, se));
    float high = max(max(nw, sw), max(ne, se));
    float contrast = max(high, middle.a) - min(low, middle.a);
    if (contrast < max(0.04, 0.08 * high))
    {
        return middle.rgb;
    }
    // 两对对角亮度差给出边缘切线. 中心有反差但方向退化时不沿任意轴模糊.
    float diagonal1 = sw - ne;
    float diagonal2 = se - nw;
    vec2 tangent = vec2(diagonal1 + diagonal2, diagonal1 - diagonal2);
    float magnitude = length(tangent);
    if (magnitude < 0.00006103515625)
    {
        return middle.rgb;
    }
    vec2 direction = tangent / magnitude;
    vec4 inner = SampleDisplayImage(center - direction) + SampleDisplayImage(center + direction);
    float sharpness = max(abs(direction.x), abs(direction.y)) * 8.0 * 0.015;
    vec2 outerStep = direction * (2.0 * min(contrast / sharpness, 3.0));
    vec4 filtered = (inner + SampleDisplayImage(center - outerStep) + SampleDisplayImage(center + outerStep)) * 0.25;
    // 四点平均的亮度越出邻域就退回内侧两点; 保留四分之一中心色以免细线被抹去.
    vec3 selected = filtered.a < low || filtered.a > high ? inner.rgb * 0.5 : filtered.rgb;
    return mix(selected, middle.rgb, 0.25);
}

void main()
{
    if (!InsideDisplayRect(gl_FragCoord.xy, displayRects.renderRect))
    {
        outColor = DisplayBackground;
        return;
    }
    outColor = vec4(FilterDisplayEdge(gl_FragCoord.xy), 1.0);
}
