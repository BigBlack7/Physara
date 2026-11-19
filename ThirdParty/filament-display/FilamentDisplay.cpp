/*
 * Copyright (C) 2021 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
//
// Physara adaptation of the scalar ACES, color transforms, and gamut mapping from
// Filament b4dfdb073d5543d8569d37e0ee26f79cf794db14. The original vector/matrix
// operations use GLM here, and the wrapper fixes neutral grading and linear Rec.709 output.

#include "FilamentDisplay.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vector_relational.hpp>

// ColorSpaceUtils.h: Copyright (C) 2020 The Android Open Source Project.
// ColorSpaceUtils.cpp: Copyright (C) 2021 The Android Open Source Project.

namespace Physara::FilamentDisplay
{
using float2 = glm::vec2;
using float3 = glm::vec3;
using mat3f = glm::mat3;
using std::min;
using std::max;
using glm::clamp;
using glm::dot;
using glm::mix;
using glm::sign;
using glm::smoothstep;

static constexpr mat3f XYZ_to_sRGB{
     3.2404542f, -0.9692660f,  0.0556434f,
    -1.5371385f,  1.8760108f, -0.2040259f,
    -0.4985314f,  0.0415560f,  1.0572252f
};

static constexpr mat3f sRGB_to_XYZ{
     0.4124560f,  0.2126730f,  0.0193339f,
     0.3575760f,  0.7151520f,  0.1191920f,
     0.1804380f,  0.0721750f,  0.9503040f
};

static constexpr mat3f Rec2020_to_XYZ{
     0.6369530f,  0.2626983f,  0.0000000f,
     0.1446169f,  0.6780088f,  0.0280731f,
     0.1688558f,  0.0592929f,  1.0608272f
};

static constexpr mat3f XYZ_to_Rec2020{
    1.7166634f,  -0.6666738f,  0.0176425f,
    -0.3556733f,  1.6164557f, -0.0427770f,
    -0.2533681f,  0.0157683f,  0.9422433f
};

static constexpr mat3f AP1_to_XYZ{
     0.6624541811f,  0.2722287168f, -0.0055746495f,
     0.1340042065f,  0.6740817658f,  0.0040607335f,
     0.1561876870f,  0.0536895174f,  1.0103391003f
};

static constexpr mat3f XYZ_to_AP1{
     1.6410233797f, -0.6636628587f,  0.0117218943f,
    -0.3248032942f,  1.6153315917f, -0.0082844420f,
    -0.2364246952f,  0.0167563477f,  0.9883948585f
};

static constexpr mat3f AP1_to_AP0{
     0.6954522414f,  0.0447945634f, -0.0055258826f,
     0.1406786965f,  0.8596711185f,  0.0040252103f,
     0.1638690622f,  0.0955343182f,  1.0015006723f
};

static constexpr mat3f AP0_to_AP1{
     1.4514393161f, -0.0765537734f,  0.0083161484f,
    -0.2365107469f,  1.1762296998f, -0.0060324498f,
    -0.2149285693f, -0.0996759264f,  0.9977163014f
};

static constexpr mat3f sRGB_to_OkLab_LMS{
     0.4122214708f, 0.2119034982f, 0.0883024619f,
     0.5363325363f, 0.6806995451f, 0.2817188376f,
     0.0514459929f, 0.1073969566f, 0.6299787005f
};

static constexpr mat3f OkLab_LMS_to_OkLab{
     0.2104542553f,  1.9779984951f,  0.0259040371f,
     0.7936177850f, -2.4285922050f,  0.7827717662f,
    -0.0040720468f,  0.4505937099f, -0.8086757660f
};

static constexpr mat3f OkLab_to_OkLab_LMS{
     1.0000000000f,  1.0000000000f,  1.0000000000f,
     0.3963377774f, -0.1055613458f, -0.0894841775f,
     0.2158037573f, -0.0638541728f, -1.2914855480f
};

static constexpr mat3f OkLab_LMS_to_sRGB{
     4.0767416621f, -1.2684380046f, -0.0041960863f,
    -3.3077115913f,  2.6097574011f, -0.7034186147f,
     0.2309699292f, -0.3413193965f,  1.7076147010f
};

static const mat3f sRGB_to_Rec2020 = XYZ_to_Rec2020 * sRGB_to_XYZ;
static const mat3f Rec2020_to_sRGB = XYZ_to_sRGB * Rec2020_to_XYZ;
static const mat3f Rec2020_to_AP0 = AP1_to_AP0 * XYZ_to_AP1 * Rec2020_to_XYZ;
static const mat3f AP1_to_Rec2020 = XYZ_to_Rec2020 * AP1_to_XYZ;
static constexpr float3 LUMINANCE_AP1{0.272229f, 0.674082f, 0.0536895f};

static float MinComponent(float3 rgb) noexcept {
    return min(rgb.r, min(rgb.g, rgb.b));
}

static float MaxComponent(float3 rgb) noexcept {
    return max(rgb.r, max(rgb.g, rgb.b));
}

static float3 xyY_to_XYZ(float3 v) noexcept {
    const float a = v.z / max(v.y, 1e-5f);
    return {v.x * a, v.z, (1.0f - v.x - v.y) * a};
}

static float3 XYZ_to_xyY(float3 v) noexcept {
    const float sum = max(v.x + v.y + v.z, 1e-5f);
    return {v.x / sum, v.y / sum, v.y};
}

static float3 sRGB_to_OkLab(float3 x) noexcept {
    const auto lms = sRGB_to_OkLab_LMS * x;
    return OkLab_LMS_to_OkLab * float3(std::cbrt(lms.x), std::cbrt(lms.y), std::cbrt(lms.z));
}

static float3 OkLab_to_sRGB(float3 x) noexcept {
    const auto lms = OkLab_to_OkLab_LMS * x;
    return OkLab_LMS_to_sRGB * (lms * lms * lms);
}

static float rgb_2_saturation(float3 const rgb) noexcept {
    // Input:  ACES
    // Output: OCES
    constexpr float TINY = 1e-5f;
    float const mi = MinComponent(rgb);
    float const ma = MaxComponent(rgb);
    return (max(ma, TINY) - max(mi, TINY)) / max(ma, 1e-2f);
}

static float rgb_2_yc(float3 const rgb) noexcept {
    constexpr float ycRadiusWeight = 1.75f;

    // Converts RGB to a luminance proxy, here called YC
    // YC is ~ Y + K * Chroma
    // Constant YC is a cone-shaped surface in RGB space, with the tip on the
    // neutral axis, towards white.
    // YC is normalized: RGB 1 1 1 maps to YC = 1
    //
    // ycRadiusWeight defaults to 1.75, although it can be overridden in function
    // call to rgb_2_yc
    // ycRadiusWeight = 1 -> YC for pure cyan, magenta, yellow == YC for neutral
    // of same value
    // ycRadiusWeight = 2 -> YC for pure red, green, blue  == YC for  neutral of
    // same value.

    float const r = rgb.r;
    float const g = rgb.g;
    float const b = rgb.b;

    float const chroma = std::sqrt(b * (b - g) + g * (g - r) + r * (r - b));

    return (b + g + r + ycRadiusWeight * chroma) / 3.0f;
}

static float sigmoid_shaper(float const x) noexcept {
    // Sigmoid function in the range 0 to 1 spanning -2 to +2.
    float const t = max(1.0f - std::abs(x / 2.0f), 0.0f);
    float const y = 1.0f + sign(x) * (1.0f - t * t);
    return y / 2.0f;
}

static float glow_fwd(float const ycIn, float const glowGainIn, float const glowMid) noexcept {
    float glowGainOut;

    if (ycIn <= 2.0f / 3.0f * glowMid) {
        glowGainOut = glowGainIn;
    } else if ( ycIn >= 2.0f * glowMid) {
        glowGainOut = 0.0f;
    } else {
        glowGainOut = glowGainIn * (glowMid / ycIn - 1.0f / 2.0f);
    }

    return glowGainOut;
}

static float rgb_2_hue(float3 const rgb) noexcept {
    // Returns a geometric hue angle in degrees (0-360) based on RGB values.
    // For neutral colors, hue is undefined and the function will return a quiet NaN value.
    float hue = 0.0f;
    // RGB triplets where RGB are equal have an undefined hue
    if (!(rgb.x == rgb.y && rgb.y == rgb.z)) {
        hue = (180.0f / std::numbers::pi_v<float>) * std::atan2(
                std::sqrt(3.0f) * (rgb.y - rgb.z),
                2.0f * rgb.x - rgb.y - rgb.z);
    }
    return (hue < 0.0f) ? hue + 360.0f : hue;
}

static float center_hue(float const hue, float const centerH) noexcept {
    float hueCentered = hue - centerH;
    if (hueCentered < -180.0f) {
        hueCentered = hueCentered + 360.0f;
    } else if (hueCentered > 180.0f) {
        hueCentered = hueCentered - 360.0f;
    }
    return hueCentered;
}

static float3 darkSurround_to_dimSurround(float3 linearCV) noexcept {
    constexpr float DIM_SURROUND_GAMMA = 0.9811f;

    float3 XYZ = AP1_to_XYZ * linearCV;
    float3 xyY = XYZ_to_xyY(XYZ);

    xyY.z = clamp(xyY.z, 0.0f, 65504.0f);
    xyY.z = std::pow(xyY.z, DIM_SURROUND_GAMMA);

    XYZ = xyY_to_XYZ(xyY);
    return XYZ_to_AP1 * XYZ;
}

static float3 ACES(float3 color, float brightness) noexcept {
    // Some bits were removed to adapt to our desired output

    // "Glow" module constants
    constexpr float RRT_GLOW_GAIN = 0.05f;
    constexpr float RRT_GLOW_MID = 0.08f;

    // Red modifier constants
    constexpr float RRT_RED_SCALE = 0.82f;
    constexpr float RRT_RED_PIVOT = 0.03f;
    constexpr float RRT_RED_HUE   = 0.0f;
    constexpr float RRT_RED_WIDTH = 135.0f;

    // Desaturation constants
    constexpr float RRT_SAT_FACTOR = 0.96f;
    constexpr float ODT_SAT_FACTOR = 0.93f;

    float3 ap0 = Rec2020_to_AP0 * color;

    // Glow module
    float const saturation = rgb_2_saturation(ap0);
    float const ycIn = rgb_2_yc(ap0);
    float const s = sigmoid_shaper((saturation - 0.4f) / 0.2f);
    float const addedGlow = 1.0f + glow_fwd(ycIn, RRT_GLOW_GAIN * s, RRT_GLOW_MID);
    ap0 *= addedGlow;

    // Red modifier
    float const hue = rgb_2_hue(ap0);
    float const centeredHue = center_hue(hue, RRT_RED_HUE);
    float hueWeight = smoothstep(0.0f, 1.0f, 1.0f - std::abs(2.0f * centeredHue / RRT_RED_WIDTH));
    hueWeight *= hueWeight;

    ap0.r += hueWeight * saturation * (RRT_RED_PIVOT - ap0.r) * (1.0f - RRT_RED_SCALE);

    // ACES to RGB rendering space
    float3 ap1 = clamp(AP0_to_AP1 * ap0, float3(0.0f), float3(65504.0f));

    // Global desaturation
    ap1 = mix(float3(dot(ap1, LUMINANCE_AP1)), ap1, RRT_SAT_FACTOR);

    // NOTE: This is specific to Filament and added only to match ACES to our legacy tone mapper
    //       which was a fit of ACES in Rec.709 but with a brightness boost.
    ap1 *= brightness;

    // Fitting of RRT + ODT (RGB monitor 100 nits dim) from:
    // https://github.com/colour-science/colour-unity/blob/master/Assets/Colour/Notebooks/CIECAM02_Unity.ipynb
    constexpr float a = 2.785085f;
    constexpr float b = 0.107772f;
    constexpr float c = 2.936045f;
    constexpr float d = 0.887122f;
    constexpr float e = 0.806889f;
    float3 const rgbPost = (ap1 * (a * ap1 + b)) / (ap1 * (c * ap1 + d) + e);

    // Apply gamma adjustment to compensate for dim surround
    float3 linearCV = darkSurround_to_dimSurround(rgbPost);

    // Apply desaturation to compensate for luminance difference
    linearCV = mix(float3(dot(linearCV, LUMINANCE_AP1)), linearCV, ODT_SAT_FACTOR);

    return AP1_to_Rec2020 * linearCV;
}

/*
 * The section below is subject to the following license.
 * Source: https://bottosson.github.io/posts/gamutclipping/
 *
 * Copyright (c) 2021 Björn Ottosson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

// TODO: Replace compute_max_saturation() with solution from
//       https://simonstechblog.blogspot.com/2021/06/implementing-gamut-mapping.html to
//       target arbitrary output gamuts. The solution below works only for sRGB/Rec.709

// Finds the maximum saturation possible for a given hue that fits in sRGB
// Saturation here is defined as S = C/L
// a and b must be normalized so a^2 + b^2 == 1
static float compute_max_saturation(float const a, float const b) noexcept {
    // Max saturation will be when one of r, g or b goes below zero.

    // Select different coefficients depending on which component goes below zero first
    float k0, k1, k2, k3, k4, wl, wm, ws;

    if (-1.88170328f * a - 0.80936493f * b > 1) {
        // Red component
        k0 = +1.19086277f;
        k1 = +1.76576728f;
        k2 = +0.59662641f;
        k3 = +0.75515197f;
        k4 = +0.56771245f;
        wl = +4.0767416621f;
        wm = -3.3077115913f;
        ws = +0.2309699292f;
    } else if (1.81444104f * a - 1.19445276f * b > 1) {
        // Green component
        k0 = +0.73956515f;
        k1 = -0.45954404f;
        k2 = +0.08285427f;
        k3 = +0.12541070f;
        k4 = +0.14503204f;
        wl = -1.2681437731f;
        wm = +2.6097574011f;
        ws = -0.3413193965f;
    } else {
        // Blue component
        k0 = +1.35733652f;
        k1 = -0.00915799f;
        k2 = -1.15130210f;
        k3 = -0.50559606f;
        k4 = +0.00692167f;
        wl = -0.0041960863f;
        wm = -0.7034186147f;
        ws = +1.7076147010f;
    }

    // Approximate max saturation using a polynomial:
    float S = k0 + k1 * a + k2 * b + k3 * a * a + k4 * a * b;

    // Do one step Halley's method to get closer
    // this gives an error less than 10e6, except for some blue hues where the dS/dh
    // is close to infinite
    // this should be sufficient for most applications, otherwise do two/three steps

    float const k_l = +0.3963377774f * a + 0.2158037573f * b;
    float const k_m = -0.1055613458f * a - 0.0638541728f * b;
    float const k_s = -0.0894841775f * a - 1.2914855480f * b;

    {
        float const l_ = 1.f + S * k_l;
        float const m_ = 1.f + S * k_m;
        float const s_ = 1.f + S * k_s;

        float const l = l_ * l_ * l_;
        float const m = m_ * m_ * m_;
        float const s = s_ * s_ * s_;

        float const l_dS = 3.f * k_l * l_ * l_;
        float const m_dS = 3.f * k_m * m_ * m_;
        float const s_dS = 3.f * k_s * s_ * s_;

        float const l_dS2 = 6.f * k_l * k_l * l_;
        float const m_dS2 = 6.f * k_m * k_m * m_;
        float const s_dS2 = 6.f * k_s * k_s * s_;

        float const f = wl * l + wm * m + ws * s;
        float const f1 = wl * l_dS + wm * m_dS + ws * s_dS;
        float const f2 = wl * l_dS2 + wm * m_dS2 + ws * s_dS2;

        S = S - f * f1 / (f1 * f1 - 0.5f * f * f2);
    }

    return S;
}

// finds L_cusp and C_cusp for a given hue
// a and b must be normalized so a^2 + b^2 == 1
static float2 find_cusp(float const a, float const b) noexcept {
    // First, find the maximum saturation (saturation S = C/L)
    float const S_cusp = compute_max_saturation(a, b);

    // Convert to linear sRGB to find the first point where at least one of r,g or b >= 1:
    float3 const rgb_at_max = OkLab_to_sRGB({1.0f, S_cusp * a, S_cusp * b});
    float const L_cusp = std::cbrt(1.0f / MaxComponent(rgb_at_max));
    float const C_cusp = L_cusp * S_cusp;

    return { L_cusp, C_cusp };
}

// Finds intersection of the line defined by
// L = L0 * (1 - t) + t * L1;
// C = t * C1;
// a and b must be normalized so a^2 + b^2 == 1
static float find_gamut_intersection(float const a, float const b, float const L1, float const C1, float const L0) noexcept {
    // Find the cusp of the gamut triangle
    float2 const cusp = find_cusp(a, b);

    // Find the intersection for upper and lower half separately
    float t;
    if (((L1 - L0) * cusp.y - (cusp.x - L0) * C1) <= 0.f) {
        // Lower half
        t = cusp.y * L0 / (C1 * cusp.x + cusp.y * (L0 - L1));
    } else {
        // Upper half

        // First intersect with triangle
        t = cusp.y * (L0 - 1.f) / (C1 * (cusp.x - 1.f) + cusp.y * (L0 - L1));

        // Then one step Halley's method
        {
            float const dL = L1 - L0;
            float const dC = C1;

            float const k_l = +0.3963377774f * a + 0.2158037573f * b;
            float const k_m = -0.1055613458f * a - 0.0638541728f * b;
            float const k_s = -0.0894841775f * a - 1.2914855480f * b;

            float const l_dt = dL + dC * k_l;
            float const m_dt = dL + dC * k_m;
            float const s_dt = dL + dC * k_s;


            // If higher accuracy is required, 2 or 3 iterations of the
            // following block can be used:
            {
                float const L = L0 * (1.f - t) + t * L1;
                float const C = t * C1;

                float const l_ = L + C * k_l;
                float const m_ = L + C * k_m;
                float const s_ = L + C * k_s;

                float const l = l_ * l_ * l_;
                float const m = m_ * m_ * m_;
                float const s = s_ * s_ * s_;

                float const ldt = 3 * l_dt * l_ * l_;
                float const mdt = 3 * m_dt * m_ * m_;
                float const sdt = 3 * s_dt * s_ * s_;

                float const ldt2 = 6 * l_dt * l_dt * l_;
                float const mdt2 = 6 * m_dt * m_dt * m_;
                float const sdt2 = 6 * s_dt * s_dt * s_;

                float const r = 4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s - 1;
                float const r1 = 4.0767416621f * ldt - 3.3077115913f * mdt + 0.2309699292f * sdt;
                float const r2 = 4.0767416621f * ldt2 - 3.3077115913f * mdt2 + 0.2309699292f * sdt2;

                float const u_r = r1 / (r1 * r1 - 0.5f * r * r2);
                float t_r = -r * u_r;

                float const g = -1.2681437731f * l + 2.6097574011f * m - 0.3413193965f * s - 1;
                float const g1 = -1.2681437731f * ldt + 2.6097574011f * mdt - 0.3413193965f * sdt;
                float const g2 = -1.2681437731f * ldt2 + 2.6097574011f * mdt2 - 0.3413193965f * sdt2;

                float const u_g = g1 / (g1 * g1 - 0.5f * g * g2);
                float t_g = -g * u_g;

                float const b0 = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s - 1;
                float const b1 = -0.0041960863f * ldt - 0.7034186147f * mdt + 1.7076147010f * sdt;
                float const b2 = -0.0041960863f * ldt2 - 0.7034186147f * mdt2 + 1.7076147010f * sdt2;

                float const u_b = b1 / (b1 * b1 - 0.5f * b0 * b2);
                float t_b = -b0 * u_b;

                t_r = u_r >= 0.f ? t_r : std::numeric_limits<float>::max();
                t_g = u_g >= 0.f ? t_g : std::numeric_limits<float>::max();
                t_b = u_b >= 0.f ? t_b : std::numeric_limits<float>::max();

                t += min(t_r, min(t_g, t_b));
            }
        }
    }

    return t;
}

constexpr float sgn(float const x) noexcept {
    return (float) (0.f < x) - (float) (x < 0.f);
}

// Adaptive L0 = 0.5, with alpha set to 0.05 by default
// The threshold parameters defines a flexible range above 1.0 and below 0.0 where out-of-gamut
// values are still considered in-gamut. This helps control for inaccuracies in the previous
// color grading steps that may slightly deviate from in-gamut values when they shouldn't.
inline float3 gamut_clip_adaptive_L0_0_5(float3 rgb,
        float alpha = 0.05f, float threshold = 0.03f) noexcept {

    if (all(lessThanEqual(rgb, float3{1.0f + threshold})) &&
            all(greaterThanEqual(rgb, float3{-threshold}))) {
        return rgb;
    }

    float3 const lab = sRGB_to_OkLab(rgb);

    float const L = lab.x;
    float const eps = 0.00001f;
    float const C = max(eps, std::sqrt(lab.y * lab.y + lab.z * lab.z));
    float const a_ = lab.y / C;
    float const b_ = lab.z / C;

    float const Ld = L - 0.5f;
    float const e1 = 0.5f + std::abs(Ld) + alpha * C;
    float const L0 = 0.5f * (1.0f + sgn(Ld) * (e1 - std::sqrt(e1 * e1 - 2.0f * std::abs(Ld))));

    float const t = find_gamut_intersection(a_, b_, L, C, L0);
    float const L_clipped = L0 * (1.f - t) + t * L;
    float const C_clipped = t * C;

    return OkLab_to_sRGB({L_clipped, C_clipped * a_, C_clipped * b_});
}

static float3 gamutMapping_sRGB(float3 const rgb) noexcept {
    return gamut_clip_adaptive_L0_0_5(rgb);
}

/*
 * End source: https://bottosson.github.io/posts/gamutclipping/
 */


glm::vec3 GradeSdr(glm::vec3 rec709) noexcept {
    // 项目入口接收已预曝光的线性 Rec.709. ACES 在 Rec.2020 中取单位倍率, 回到目标原色后才做色域映射.
    const auto rec2020 = sRGB_to_Rec2020 * rec709;
    const auto mapped = Rec2020_to_sRGB * ACES(rec2020, 1.0f);
    return clamp(gamutMapping_sRGB(mapped), float3(0.0f), float3(1.0f));
}
}
