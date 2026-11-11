/*
 * Resistance port: CPU evaluation of an NV40 vertex program's position output,
 * for exact camera motion under DLSS / FSR / XeSS.
 *
 * Same encoding and semantics as rsx_vp_decompiler.c (field layout in its header
 * comment; the vector unit's result is stored before the scalar unit reads SRC2,
 * NV multiply rule 0 * x = 0). Only straight-line programs are evaluated: flow
 * control, condition-code tests and vertex texture fetches are refused (return 0)
 * rather than approximated.
 *
 * vpcpu_affine() turns a program + constant bank into the 4x4 map
 *   o0 = A * (p.x, p.y, p.z, 1)   (p = the position attribute)
 * and vpcpu_camera() turns this frame's and last frame's maps of a STATIC object
 * (plus the viewport epilogues) into the clip -> previous clip matrix of the
 * camera: M = Ep*Ap * inverse(Ec*Ac). Every static surface in the frame shares M.
 * Header only, no D3D: the unit test includes it directly.
 */
#ifndef RSX_VP_CPU_H
#define RSX_VP_CPU_H
#include <stdint.h>
#include <string.h>
#include <math.h>

#define VPCPU_MAX_INSTR 512u

static uint32_t vpcpu_rd(const uint8_t* p)
{ return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

static float vpcpu_mul1(float a, float b) { return (a == 0.0f || b == 0.0f) ? 0.0f : a * b; }

typedef struct { float r[32][4]; float o[16][4]; int a[2][4]; } VpcpuRegs;

static void vpcpu_src(const VpcpuRegs* R, uint32_t src, int is_abs, uint32_t input_src, uint32_t const_src,
                      int index_const, uint32_t addr_sel, uint32_t addr_swz,
                      const float* c, const float (*v)[4], float out[4])
{
    const float* base;
    switch (src & 3u) {
    case 2u: base = v[input_src & 15u]; break;
    case 3u: {
        const uint32_t idx = index_const ? ((const_src + (uint32_t)R->a[addr_sel & 1u][addr_swz & 3u]) & 511u)
                                         : (const_src & 511u);
        base = c + (size_t)idx * 4u;
        break;
    }
    default: base = R->r[(src >> 2) & 31u]; break;
    }
    const uint32_t sw = (src >> 8) & 3u, sz = (src >> 10) & 3u, sy = (src >> 12) & 3u, sx = (src >> 14) & 3u;
    out[0] = base[sx]; out[1] = base[sy]; out[2] = base[sz]; out[3] = base[sw];
    for (int k = 0; k < 4; k++) {
        if (is_abs) out[k] = fabsf(out[k]);
        if ((src >> 16) & 1u) out[k] = -out[k];
    }
}

static void vpcpu_store(float* dst, const float v[4], const int m[4])
{
    for (int k = 0; k < 4; k++) if (m[k]) dst[k] = v[k];
}

/* Runs the program (n instructions at most, little-endian words) and returns
 * o0, the position before the viewport epilogue. 0 = not a program this can
 * evaluate exactly. c = 512 x float4 constants, v = 16 x float4 attributes. */
static int vpcpu_run(const uint8_t* uc, uint32_t n, const float* c, const float (*v)[4], float o0[4])
{
    if (!uc || !n || n > VPCPU_MAX_INSTR) return 0;
    VpcpuRegs R;
    memset(&R, 0, sizeof R);
    for (int i = 0; i < 16; i++) { R.o[i][0] = R.o[i][1] = R.o[i][2] = 0.0f; R.o[i][3] = 1.0f; }
    int ended = 0;
    for (uint32_t i = 0; i < n; i++) {
        const uint8_t* p = uc + (size_t)i * 16u;
        const uint32_t d0 = vpcpu_rd(p), d1 = vpcpu_rd(p + 4), d2 = vpcpu_rd(p + 8), d3 = vpcpu_rd(p + 12);
        const uint32_t vec_op = (d1 >> 22) & 0x1Fu, sca_op = (d1 >> 27) & 0x1Fu;
        const uint32_t input_src = (d1 >> 8) & 0xFu, const_src = (d1 >> 12) & 0x3FFu;
        const int index_const = (int)((d3 >> 1) & 1u);
        const uint32_t src0 = ((d2 >> 23) & 0x1FFu) | ((d1 & 0xFFu) << 9);
        const uint32_t src1 = (d2 >> 6) & 0x1FFFFu;
        const uint32_t src2 = ((d3 >> 21) & 0x7FFu) | ((d2 & 0x3Fu) << 11);
        const int a0 = (int)((d0 >> 21) & 1u), a1 = (int)((d0 >> 22) & 1u), a2 = (int)((d0 >> 23) & 1u);
        const uint32_t addr_sel = (d0 >> 24) & 1u, addr_swz = d0 & 3u;
        const int saturate = (int)((d0 >> 26) & 1u);
        const int cond_test = (int)((d0 >> 13) & 1u);
        const uint32_t cond = (d0 >> 10) & 7u;
        const uint32_t dst_tmp = (d0 >> 15) & 0x3Fu, dst_out = (d3 >> 2) & 0x1Fu, sca_dst_tmp = (d3 >> 7) & 0x3Fu;
        const int vec_result = (int)((d0 >> 30) & 1u);
        const int vm[4] = { (int)((d3 >> 16) & 1u), (int)((d3 >> 15) & 1u), (int)((d3 >> 14) & 1u), (int)((d3 >> 13) & 1u) };
        const int sm[4] = { (int)((d3 >> 20) & 1u), (int)((d3 >> 19) & 1u), (int)((d3 >> 18) & 1u), (int)((d3 >> 17) & 1u) };
        if ((sca_op >= 0x08u && sca_op <= 0x0Cu) || sca_op >= 0x11u) return 0;   /* flow control / unknown */
        if (cond_test && cond != 7u) return 0;                                   /* predicated write */
        if ((vm[0] | vm[1] | vm[2] | vm[3]) && vec_op) {
            float A[4], B[4], C[4], r[4] = {0, 0, 0, 0};
            vpcpu_src(&R, src0, a0, input_src, const_src, index_const, addr_sel, addr_swz, c, v, A);
            vpcpu_src(&R, src1, a1, input_src, const_src, index_const, addr_sel, addr_swz, c, v, B);
            vpcpu_src(&R, src2, a2, input_src, const_src, index_const, addr_sel, addr_swz, c, v, C);
            int k;
            switch (vec_op) {
            case 0x01: for (k = 0; k < 4; k++) r[k] = A[k]; break;
            case 0x02: for (k = 0; k < 4; k++) r[k] = vpcpu_mul1(A[k], B[k]); break;
            case 0x03: for (k = 0; k < 4; k++) r[k] = A[k] + C[k]; break;
            case 0x04: for (k = 0; k < 4; k++) r[k] = vpcpu_mul1(A[k], B[k]) + C[k]; break;
            case 0x05: { const float s = vpcpu_mul1(A[0], B[0]) + vpcpu_mul1(A[1], B[1]) + vpcpu_mul1(A[2], B[2]);
                         for (k = 0; k < 4; k++) r[k] = s; break; }
            case 0x06: { const float s = vpcpu_mul1(A[0], B[0]) + vpcpu_mul1(A[1], B[1]) + vpcpu_mul1(A[2], B[2]) + B[3];
                         for (k = 0; k < 4; k++) r[k] = s; break; }
            case 0x07: { const float s = vpcpu_mul1(A[0], B[0]) + vpcpu_mul1(A[1], B[1]) + vpcpu_mul1(A[2], B[2]) + vpcpu_mul1(A[3], B[3]);
                         for (k = 0; k < 4; k++) r[k] = s; break; }
            case 0x08: r[0] = 1.0f; r[1] = A[1] * B[1]; r[2] = A[2]; r[3] = B[3]; break;
            case 0x09: for (k = 0; k < 4; k++) r[k] = A[k] < B[k] ? A[k] : B[k]; break;
            case 0x0A: for (k = 0; k < 4; k++) r[k] = A[k] > B[k] ? A[k] : B[k]; break;
            case 0x0B: for (k = 0; k < 4; k++) r[k] = A[k] <  B[k] ? 1.0f : 0.0f; break;
            case 0x0C: for (k = 0; k < 4; k++) r[k] = A[k] >= B[k] ? 1.0f : 0.0f; break;
            case 0x0D: {   /* ARL: destination address register is D0.dst_tmp & 1 */
                const uint32_t ad = dst_tmp & 1u;
                for (k = 0; k < 4; k++) if (vm[k]) {
                    float f = floorf(A[k]);
                    if (!(f > -1.0e9f)) f = -1.0e9f;
                    if (f > 1.0e9f) f = 1.0e9f;
                    R.a[ad][k] = (int)f;
                }
                break;
            }
            case 0x0E: for (k = 0; k < 4; k++) r[k] = A[k] - floorf(A[k]); break;
            case 0x0F: for (k = 0; k < 4; k++) r[k] = floorf(A[k]); break;
            case 0x10: for (k = 0; k < 4; k++) r[k] = A[k] == B[k] ? 1.0f : 0.0f; break;
            case 0x11: break;
            case 0x12: for (k = 0; k < 4; k++) r[k] = A[k] >  B[k] ? 1.0f : 0.0f; break;
            case 0x13: for (k = 0; k < 4; k++) r[k] = A[k] <= B[k] ? 1.0f : 0.0f; break;
            case 0x14: for (k = 0; k < 4; k++) r[k] = A[k] != B[k] ? 1.0f : 0.0f; break;
            case 0x15: for (k = 0; k < 4; k++) r[k] = 1.0f; break;
            case 0x16: for (k = 0; k < 4; k++) r[k] = A[k] > 0.0f ? 1.0f : A[k] < 0.0f ? -1.0f : 0.0f; break;
            default: return 0;   /* TXL (vertex texture) and unassigned opcodes */
            }
            if (vec_op != 0x0D) {
                if (saturate) for (k = 0; k < 4; k++) r[k] = r[k] < 0.0f ? 0.0f : r[k] > 1.0f ? 1.0f : r[k];
                if (vec_result && dst_out != 0x1Fu) vpcpu_store(R.o[dst_out & 15u], r, vm);
                if (dst_tmp != 0x3Fu) vpcpu_store(R.r[dst_tmp & 31u], r, vm);
            }
        }
        if ((sm[0] | sm[1] | sm[2] | sm[3]) && sca_op) {
            float C[4], r[4];
            vpcpu_src(&R, src2, a2, input_src, const_src, index_const, addr_sel, addr_swz, c, v, C);
            const float x = C[0];
            float s = 0.0f; int k;
            switch (sca_op) {
            case 0x01: for (k = 0; k < 4; k++) r[k] = C[k]; break;
            case 0x02: s = 1.0f / x; break;
            case 0x03:
                s = 1.0f / x;
                if (!(s >= 5.42101e-20f)) s = 5.42101e-20f;
                if (s > 1.884467e19f) s = 1.884467e19f;
                break;
            case 0x04: { float ax = fabsf(x); if (ax < 1e-10f) ax = 1e-10f; s = 1.0f / sqrtf(ax); break; }
            case 0x05: case 0x0E: s = exp2f(x); break;
            case 0x06: case 0x0D: { float ax = fabsf(x); if (ax < 1e-10f) ax = 1e-10f; s = log2f(ax); break; }
            case 0x07: {
                float w = C[3];
                if (w < -127.996f) w = -127.996f;
                if (w > 127.996f) w = 127.996f;
                const float y = C[1] > 1e-10f ? C[1] : 1e-10f;
                r[0] = 1.0f; r[1] = x > 0.0f ? x : 0.0f; r[2] = x > 0.0f ? exp2f(w * log2f(y)) : 0.0f; r[3] = 1.0f;
                break;
            }
            case 0x0F: s = sinf(x); break;
            case 0x10: s = cosf(x); break;
            default: return 0;
            }
            if (sca_op != 0x01 && sca_op != 0x07) for (k = 0; k < 4; k++) r[k] = s;
            if (saturate) for (k = 0; k < 4; k++) r[k] = r[k] < 0.0f ? 0.0f : r[k] > 1.0f ? 1.0f : r[k];
            if (!vec_result && dst_out != 0x1Fu) vpcpu_store(R.o[dst_out & 15u], r, sm);
            if (sca_dst_tmp != 0x3Fu) vpcpu_store(R.r[sca_dst_tmp & 31u], r, sm);
        }
        if (d3 & 1u) { ended = 1; break; }
    }
    if (!ended) return 0;
    for (int k = 0; k < 4; k++) { o0[k] = R.o[0][k]; if (!(o0[k] == o0[k]) || fabsf(o0[k]) > 3.0e38f) return 0; }
    return 1;
}

static void vpcpu_attr_default(float v[16][4])
{
    for (int i = 0; i < 16; i++) { v[i][0] = v[i][1] = v[i][2] = 0.0f; v[i][3] = 1.0f; }
}
static int vpcpu_differs(const float a[4], const float b[4])
{
    for (int k = 0; k < 4; k++) {
        const float m = fabsf(a[k]) > fabsf(b[k]) ? fabsf(a[k]) : fabsf(b[k]);
        if (fabsf(a[k] - b[k]) > 1e-6f * (1.0f + m)) return 1;
    }
    return 0;
}

/* The single attribute o0 depends on (the position), -1 if none or several
 * (skinning weights / bone indices, morph targets) or the program can't be run.
 * Perturbed by more than 1 so a bone index read through ARL moves too. */
static int vpcpu_find_pos(const uint8_t* uc, uint32_t n, const float* c, uint32_t inmask)
{
    float v[16][4], b[4], o[4];
    vpcpu_attr_default(v);
    if (!vpcpu_run(uc, n, c, (const float (*)[4])v, b)) return -1;
    int found = -1;
    for (int k = 0; k < 16; k++) {
        if (!((inmask >> k) & 1u)) continue;
        vpcpu_attr_default(v);
        v[k][0] = 3.37f; v[k][1] = 5.61f; v[k][2] = 7.83f;
        if (!vpcpu_run(uc, n, c, (const float (*)[4])v, o)) return -1;
        if (vpcpu_differs(o, b)) { if (found >= 0) return -1; found = k; }
    }
    return found;
}

/* o0 = A * (p, 1) for position attribute pa (other attributes (0,0,0,1)).
 * A is row-major. Returns 1 when the program is affine in p (checked at two
 * extra points) and finite, 0 otherwise. */
static int vpcpu_affine(const uint8_t* uc, uint32_t n, const float* c, int pa, double A[16])
{
    if (pa < 0 || pa > 15) return 0;
    float v[16][4], b[4], e[3][4];
    vpcpu_attr_default(v);
    if (!vpcpu_run(uc, n, c, (const float (*)[4])v, b)) return 0;
    for (int j = 0; j < 3; j++) {
        vpcpu_attr_default(v);
        v[pa][j] = 1.0f;
        if (!vpcpu_run(uc, n, c, (const float (*)[4])v, e[j])) return 0;
    }
    for (int r = 0; r < 4; r++) {
        for (int j = 0; j < 3; j++) A[r * 4 + j] = (double)e[j][r] - (double)b[r];
        A[r * 4 + 3] = (double)b[r];
    }
    static const float tp[2][3] = { { 0.37f, -0.61f, 0.83f }, { -2.3f, 1.7f, 3.1f } };
    for (int t = 0; t < 2; t++) {
        float o[4];
        vpcpu_attr_default(v);
        v[pa][0] = tp[t][0]; v[pa][1] = tp[t][1]; v[pa][2] = tp[t][2];
        if (!vpcpu_run(uc, n, c, (const float (*)[4])v, o)) return 0;
        double mag = 1.0;
        for (int r = 0; r < 4; r++) for (int j = 0; j < 4; j++) { const double a = fabs(A[r * 4 + j]); if (a > mag) mag = a; }
        for (int r = 0; r < 4; r++) {
            const double pr = A[r * 4 + 0] * tp[t][0] + A[r * 4 + 1] * tp[t][1] + A[r * 4 + 2] * tp[t][2] + A[r * 4 + 3];
            if (fabs(pr - (double)o[r]) > 2e-5 * mag * 4.0) return 0;
        }
    }
    return 1;
}

static void vpcpu_mul4(const double a[16], const double b[16], double o[16])
{
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            o[r * 4 + c] = a[r * 4 + 0] * b[0 * 4 + c] + a[r * 4 + 1] * b[1 * 4 + c] + a[r * 4 + 2] * b[2 * 4 + c] + a[r * 4 + 3] * b[3 * 4 + c];
}
/* Gauss-Jordan with partial pivoting; 0 when (relatively) singular. */
static int vpcpu_inv4(const double m[16], double inv[16])
{
    double a[4][8];
    double scale = 0.0;
    for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) { a[r][c] = m[r * 4 + c]; a[r][c + 4] = r == c ? 1.0 : 0.0;
                                                               if (fabs(m[r * 4 + c]) > scale) scale = fabs(m[r * 4 + c]); }
    if (!(scale > 0.0) || !(scale < 1e300)) return 0;
    for (int c = 0; c < 4; c++) {
        int p = c;
        for (int r = c + 1; r < 4; r++) if (fabs(a[r][c]) > fabs(a[p][c])) p = r;
        if (fabs(a[p][c]) < 1e-9 * scale) return 0;
        if (p != c) for (int k = 0; k < 8; k++) { const double t = a[c][k]; a[c][k] = a[p][k]; a[p][k] = t; }
        const double d = a[c][c];
        for (int k = 0; k < 8; k++) a[c][k] /= d;
        for (int r = 0; r < 4; r++) if (r != c) {
            const double f = a[r][c];
            if (f != 0.0) for (int k = 0; k < 8; k++) a[r][k] -= f * a[c][k];
        }
    }
    for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) inv[r * 4 + c] = a[r][c + 4];
    return 1;
}
/* The viewport epilogue: pos.xyz = clip.xyz * posscale + clip.w * posoffset. */
static void vpcpu_epi(const float* epi, double E[16])
{
    memset(E, 0, 16 * sizeof(double));
    E[0] = epi[0]; E[5] = epi[1]; E[10] = epi[2]; E[15] = 1.0;
    E[3] = epi[4]; E[7] = epi[5]; E[11] = epi[6];
}
/* Camera: this frame's (unjittered) clip -> last frame's clip, from one static
 * object's two maps. epi = posscale[4], posoffset[4] (un-jittered). */
static int vpcpu_camera(const double Ac[16], const float* epic, const double Ap[16], const float* epip, double M[16])
{
    double Ec[16], Ep[16], C[16], P[16], Ci[16];
    vpcpu_epi(epic, Ec); vpcpu_epi(epip, Ep);
    vpcpu_mul4(Ec, Ac, C); vpcpu_mul4(Ep, Ap, P);
    if (!vpcpu_inv4(C, Ci)) return 0;
    vpcpu_mul4(P, Ci, M);
    for (int k = 0; k < 16; k++) if (!(M[k] == M[k]) || fabs(M[k]) > 1e30) return 0;
    return 1;
}
/* Screen point (ndc x, y, depth z) through M: last frame's ndc xy. 0 = behind the camera. */
static int vpcpu_reproj(const double M[16], double x, double y, double z, double out[2])
{
    const double w = M[12] * x + M[13] * y + M[14] * z + M[15];
    if (!(w > 1e-9)) return 0;
    out[0] = (M[0] * x + M[1] * y + M[2] * z + M[3]) / w;
    out[1] = (M[4] * x + M[5] * y + M[6] * z + M[7]) / w;
    return 1;
}
/* Largest ndc disagreement of two camera matrices over a 3x3 screen grid at
 * three depths (1e9 when one maps a point behind the camera and the other not). */
static double vpcpu_mdist(const double A[16], const double B[16])
{
    static const double g[3] = { -0.6, 0.0, 0.6 }, zs[3] = { 0.99, 0.999, 1.0 };
    double worst = 0.0;
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) for (int k = 0; k < 3; k++) {
        double a[2], b[2];
        const int ia = vpcpu_reproj(A, g[i], g[j], zs[k], a), ib = vpcpu_reproj(B, g[i], g[j], zs[k], b);
        if (ia != ib) return 1e9;
        if (!ia) continue;
        const double d = fabs(a[0] - b[0]) > fabs(a[1] - b[1]) ? fabs(a[0] - b[0]) : fabs(a[1] - b[1]);
        if (d > worst) worst = d;
    }
    return worst;
}
/* Agreement vote: the candidate the most others agree with (within tol ndc).
 * Returns its index, or -1 unless at least 2 agree and they are at least half
 * of all candidates. *agree = size of the winning group. */
static int vpcpu_vote(const double (*Ms)[16], int n, double tol, int* agree)
{
    int best = -1, bc = 0;
    for (int i = 0; i < n; i++) {
        int cnt = 0;
        for (int j = 0; j < n; j++) if (i == j || vpcpu_mdist(Ms[i], Ms[j]) <= tol) cnt++;
        if (cnt > bc) { bc = cnt; best = i; }
    }
    if (agree) *agree = bc;
    if (bc < 2 || bc * 2 < n) return -1;
    return best;
}
/* Camera cut: the view changed so much that last frame's image is no help --
 * at least 7 of 9 screen points (depth 0.999) come from off screen / behind the
 * camera, or they moved more than `far_ndc` on average. */
static int vpcpu_is_cut(const double M[16], double far_ndc)
{
    static const double g[3] = { -0.6, 0.0, 0.6 };
    int off = 0; double sum = 0.0;
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) {
        double p[2];
        if (!vpcpu_reproj(M, g[i], g[j], 0.999, p) || fabs(p[0]) > 1.2 || fabs(p[1]) > 1.2) { off++; sum += 2.0; continue; }
        sum += sqrt((p[0] - g[i]) * (p[0] - g[i]) + (p[1] - g[j]) * (p[1] - g[j]));
    }
    return off >= 7 || sum / 9.0 > far_ndc;
}
#endif
