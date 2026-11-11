/* SDR -> HDR inverse tone mapping for the final present pass (PC port extra).
 *
 * The D3D12 present blit (rsx_d3d12_backend.c, s_blit_hdr_hlsl) runs the same maths on the GPU;
 * this C copy is the reference for the tests and maps HDR screenshots back to SDR. Keep them in step.
 *
 * Method (luminance-based expansion: paper white stays put, only the highlights grow --
 * the approach RenoDX / Special K / Windows AutoHDR take for SDR content without a separate UI):
 *   1. decode the 8-bit gamma image with a pure 2.2 power (what SDR games were graded on; the
 *      piecewise sRGB curve Windows uses for SDR-in-HDR lifts the blacks),
 *   2. Y = BT.709 luminance of the linear value, 0..1 where 1 is SDR white,
 *   3. Y <= knee (PORT_HDR_KNEE, linear 0.6, about 8-bit code 202) is untouched: everything up to
 *      there is shown exactly as SDR at the paper-white level. Above it
 *         Y' = k + (1 - k) u (1 + a u^3),  u = (Y - k) / (1 - k),  a = (R - 1) / (1 - k),
 *      R = peak / paper. Y' has slope 1 at the knee (no visible seam), is monotonic, and SDR white
 *      (Y = 1) lands exactly on the peak. R = 1 (peak = paper) is a plain SDR-in-HDR mapping,
 *   4. rgb *= Y' / Y (hue and SDR saturation kept), times paper-white nits,
 *   5. BT.709 -> BT.2020, optional saturation around BT.2020 luminance, the largest channel
 *      capped at the peak (uniform scale, hue kept),
 *   6. PQ (SMPTE ST 2084) for HDR10; scRGB goes back to BT.709 and is nits / 80.
 */
#ifndef PORT_HDR_H
#define PORT_HDR_H

#include <math.h>

#define PORT_HDR_KNEE 0.6f

/* Linear SDR luminance (0..1) -> luminance relative to paper white (0..ratio). */
static inline float port_hdr_expand(float y, float ratio)
{
    const float k = PORT_HDR_KNEE;
    if (ratio <= 1.0f || y <= k) return y;
    float u = (y - k) / (1.0f - k);
    if (u > 1.0f) u = 1.0f;
    const float a = (ratio - 1.0f) / (1.0f - k);
    return k + (1.0f - k) * u * (1.0f + a * u * u * u);
}

/* Inverse of port_hdr_expand (HDR screenshots back to SDR). */
static inline float port_hdr_compress(float y2, float ratio)
{
    const float k = PORT_HDR_KNEE;
    if (y2 <= k) return y2;
    if (ratio <= 1.0f) return y2 > 1.0f ? 1.0f : y2;
    const float t = (y2 - k) / (1.0f - k);          /* = u + a u^4 */
    const float a = (ratio - 1.0f) / (1.0f - k);
    if (t >= 1.0f + a) return 1.0f;
    float u = 1.0f;                                  /* Newton from above: f convex and increasing */
    for (int i = 0; i < 24; i++) {
        const float f = u + a * u * u * u * u - t, d = 1.0f + 4.0f * a * u * u * u;
        const float n = u - f / d;
        if (fabsf(n - u) < 1e-7f) { u = n; break; }
        u = n;
    }
    return k + (1.0f - k) * (u < 0.0f ? 0.0f : u);
}

/* SMPTE ST 2084: nits (0..10000) <-> 0..1 signal. */
static inline float port_hdr_pq_encode(float nits)
{
    float l = nits / 10000.0f;
    l = l < 0.0f ? 0.0f : l > 1.0f ? 1.0f : l;
    const float p = powf(l, 0.1593017578125f);
    return powf((0.8359375f + 18.8515625f * p) / (1.0f + 18.6875f * p), 78.84375f);
}

static inline float port_hdr_pq_decode(float e)
{
    e = e < 0.0f ? 0.0f : e > 1.0f ? 1.0f : e;
    const float p = powf(e, 1.0f / 78.84375f);
    const float num = p - 0.8359375f > 0.0f ? p - 0.8359375f : 0.0f;
    return powf(num / (18.8515625f - 18.6875f * p), 1.0f / 0.1593017578125f) * 10000.0f;
}

#endif
