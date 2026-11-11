/* rsx_vp_cpu.h: CPU evaluation of an NV40 vertex program's position, and the
 * exact camera matrix the DLSS / FSR motion pass takes from it. */
#include "rsx_vp_cpu.h"
#include <stdio.h>
#include <stdlib.h>

static int s_fail;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); s_fail++; } } while (0)

enum { T_TMP = 1, T_IN = 2, T_CONST = 3 };
#define SWZ_ID ((3u << 8) | (2u << 10) | (1u << 12) | (0u << 14))
static uint32_t src(uint32_t type, uint32_t tmp) { return type | (tmp << 2) | SWZ_ID; }
static uint32_t src_xxxx(uint32_t type, uint32_t tmp) { return type | (tmp << 2); }

/* One instruction. vmask/smask bits: 8 = x, 4 = y, 2 = z, 1 = w. */
static void enc(uint8_t* p, uint32_t vec_op, uint32_t sca_op, uint32_t dst_tmp, uint32_t dst_out, int vec_result,
                uint32_t vmask, uint32_t smask, uint32_t s0, uint32_t s1, uint32_t s2,
                uint32_t input_src, uint32_t const_src, int end)
{
    const uint32_t d0 = (dst_tmp << 15) | ((uint32_t)vec_result << 30);
    const uint32_t d1 = ((s0 >> 9) & 0xFFu) | (input_src << 8) | (const_src << 12) | (vec_op << 22) | (sca_op << 27);
    const uint32_t d2 = ((s2 >> 11) & 0x3Fu) | ((s1 & 0x1FFFFu) << 6) | ((s0 & 0x1FFu) << 23);
    const uint32_t d3 = (uint32_t)end | (dst_out << 2) | (0x3Fu << 7) |
                        (((vmask >> 0) & 1u) << 13) | (((vmask >> 1) & 1u) << 14) | (((vmask >> 2) & 1u) << 15) | (((vmask >> 3) & 1u) << 16) |
                        (((smask >> 0) & 1u) << 17) | (((smask >> 1) & 1u) << 18) | (((smask >> 2) & 1u) << 19) | (((smask >> 3) & 1u) << 20) |
                        ((s2 & 0x7FFu) << 21);
    const uint32_t w[4] = { d0, d1, d2, d3 };
    for (int i = 0; i < 4; i++) { p[i * 4] = (uint8_t)w[i]; p[i * 4 + 1] = (uint8_t)(w[i] >> 8); p[i * 4 + 2] = (uint8_t)(w[i] >> 16); p[i * 4 + 3] = (uint8_t)(w[i] >> 24); }
}

/* r0 = W * v0 (W in c4..c7), o0 = VP * r0 (VP in c0..c3): a static-mesh program. */
static uint32_t build_world_vp(uint8_t* uc)
{
    uint32_t n = 0;
    for (uint32_t r = 0; r < 4; r++, n++)
        enc(uc + n * 16, 0x07, 0, 0, 0x1F, 0, 8u >> r, 0, src(T_IN, 0), src(T_CONST, 0), 0, 0, 4 + r, 0);
    for (uint32_t r = 0; r < 4; r++, n++)
        enc(uc + n * 16, 0x07, 0, 0x3F, 0, 1, 8u >> r, 0, src(T_TMP, 0), src(T_CONST, 0), 0, 0, r, r == 3);
    return n;
}

static void mat_mul(const double a[16], const double b[16], double o[16]) { vpcpu_mul4(a, b, o); }
static void persp(double P[16], double fy, double aspect, double zn, double zf)
{
    memset(P, 0, 16 * sizeof(double));
    P[0] = fy / aspect; P[5] = fy; P[10] = zf / (zf - zn); P[11] = -zn * zf / (zf - zn); P[14] = 1.0;
}
static void view_yaw(double V[16], double yaw, double tx, double ty, double tz)
{
    /* world -> camera: rotate by -yaw about y, after moving the camera to the origin */
    const double c = cos(yaw), s = sin(yaw);
    double R[16] = { c, 0, -s, 0,  0, 1, 0, 0,  s, 0, c, 0,  0, 0, 0, 1 };
    double T[16] = { 1, 0, 0, -tx, 0, 1, 0, -ty, 0, 0, 1, -tz, 0, 0, 0, 1 };
    mat_mul(R, T, V);
}
static void set_rows(float* c, int row, const double m[16])
{
    for (int r = 0; r < 4; r++) for (int k = 0; k < 4; k++) c[(row + r) * 4 + k] = (float)m[r * 4 + k];
}
static void translate(double W[16], double x, double y, double z)
{
    double t[16] = { 1, 0, 0, x, 0, 1, 0, y, 0, 0, 1, z, 0, 0, 0, 1 };
    memcpy(W, t, sizeof t);
}

int main(void)
{
    static uint8_t uc[16 * 16];
    const uint32_t n = build_world_vp(uc);
    static float cc[512 * 4], cp[512 * 4];
    const float epi[12] = { 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    double P[16], Vc[16], Vp[16], PVc[16], PVp[16];
    persp(P, 1.5, 16.0 / 9.0, 0.5, 4000.0);
    view_yaw(Vc, 0.10, 3.0, 1.7, -2.0);    /* this frame */
    view_yaw(Vp, 0.07, 2.6, 1.7, -2.3);    /* last frame: turned and stepped */
    mat_mul(P, Vc, PVc); mat_mul(P, Vp, PVp);

    /* position attribute found, program affine, A = VP * W */
    double W1[16]; translate(W1, 10.0, 0.0, 40.0);
    memset(cc, 0, sizeof cc); set_rows(cc, 0, PVc); set_rows(cc, 4, W1);
    memset(cp, 0, sizeof cp); set_rows(cp, 0, PVp); set_rows(cp, 4, W1);
    CHECK(vpcpu_find_pos(uc, n, cc, 0xFFFFu) == 0);
    double Ac[16], Ap[16], M1[16];
    CHECK(vpcpu_affine(uc, n, cc, 0, Ac));
    CHECK(vpcpu_affine(uc, n, cp, 0, Ap));
    CHECK(vpcpu_camera(Ac, epi, Ap, epi, M1));

    /* M reprojects ANY static world point (not just this object) exactly */
    double maxerr = 0.0;
    for (int i = 0; i < 50; i++) {
        const double X[4] = { -30.0 + 1.3 * i, -2.0 + 0.1 * i, 20.0 + 7.0 * i, 1.0 };
        double cpos[4] = {0}, ppos[4] = {0};
        for (int r = 0; r < 4; r++) for (int k = 0; k < 4; k++) { cpos[r] += PVc[r * 4 + k] * X[k]; ppos[r] += PVp[r * 4 + k] * X[k]; }
        if (cpos[3] <= 0.0 || ppos[3] <= 0.0) continue;
        double out[2];
        CHECK(vpcpu_reproj(M1, cpos[0] / cpos[3], cpos[1] / cpos[3], cpos[2] / cpos[3], out));
        const double ex = fabs(out[0] - ppos[0] / ppos[3]), ey = fabs(out[1] - ppos[1] / ppos[3]);
        if (ex > maxerr) maxerr = ex;
        if (ey > maxerr) maxerr = ey;
    }
    CHECK(maxerr < 2e-4);   /* float constants: well under a pixel */

    /* a second static object agrees, a moving one does not; the vote keeps the camera */
    double W2[16], W3c[16], W3p[16], M2[16], M3[16];
    translate(W2, -25.0, 3.0, 90.0);
    translate(W3c, 5.0, 0.0, 30.0); translate(W3p, 2.0, 0.0, 30.0);
    set_rows(cc, 4, W2); set_rows(cp, 4, W2);
    CHECK(vpcpu_affine(uc, n, cc, 0, Ac) && vpcpu_affine(uc, n, cp, 0, Ap) && vpcpu_camera(Ac, epi, Ap, epi, M2));
    set_rows(cc, 4, W3c); set_rows(cp, 4, W3p);
    CHECK(vpcpu_affine(uc, n, cc, 0, Ac) && vpcpu_affine(uc, n, cp, 0, Ap) && vpcpu_camera(Ac, epi, Ap, epi, M3));
    CHECK(vpcpu_mdist(M1, M2) < 1e-3);
    CHECK(vpcpu_mdist(M1, M3) > 1e-2);
    double Ms[3][16]; memcpy(Ms[0], M3, sizeof M3); memcpy(Ms[1], M1, sizeof M1); memcpy(Ms[2], M2, sizeof M2);
    int agree = 0;
    const int win = vpcpu_vote((const double (*)[16])Ms, 3, 1e-3, &agree);
    CHECK(win == 1 || win == 2);
    CHECK(agree == 2);
    CHECK(vpcpu_vote((const double (*)[16])Ms, 1, 1e-3, &agree) == -1);   /* one candidate is never enough */
    CHECK(!vpcpu_is_cut(M1, 1.0));

    /* 180 degree turn: a cut */
    double Vcut[16], PVcut[16], Mc[16];
    view_yaw(Vcut, 0.10 + 3.14159, 3.0, 1.7, -2.0);
    mat_mul(P, Vcut, PVcut);
    set_rows(cp, 0, PVcut); set_rows(cc, 4, W1); set_rows(cp, 4, W1);
    CHECK(vpcpu_affine(uc, n, cc, 0, Ac) && vpcpu_affine(uc, n, cp, 0, Ap) && vpcpu_camera(Ac, epi, Ap, epi, Mc));
    CHECK(vpcpu_is_cut(Mc, 1.0));

    /* skinning-like: o0 depends on two attributes -> no position attribute */
    {
        static uint8_t sk[16 * 3];
        enc(sk + 0, 0x01, 0, 1, 0x1F, 0, 0xF, 0, src(T_IN, 0), 0, 0, 3, 0, 0);          /* r1 = v3 (weights) */
        enc(sk + 16, 0x03, 0, 0x3F, 0, 1, 0xF, 0, src(T_IN, 0), 0, src(T_TMP, 1), 0, 0, 1);  /* o0 = v0 + r1 */
        CHECK(vpcpu_find_pos(sk, 2, cc, 0xFFFFu) == -1);
    }
    /* flow control and predicated writes are refused, not approximated */
    {
        static uint8_t fl[32];
        float o[4]; float v[16][4]; vpcpu_attr_default(v);
        enc(fl, 0x00, 0x08, 0x3F, 0x1F, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        enc(fl + 16, 0x01, 0, 0x3F, 0, 1, 0xF, 0, src(T_IN, 0), 0, 0, 0, 0, 1);
        CHECK(!vpcpu_run(fl, 2, cc, (const float (*)[4])v, o));
        enc(fl, 0x01, 0, 0x3F, 0, 1, 0xF, 0, src(T_IN, 0), 0, 0, 0, 0, 1);
        { uint32_t d0 = (uint32_t)fl[0] | ((uint32_t)fl[1] << 8) | ((uint32_t)fl[2] << 16) | ((uint32_t)fl[3] << 24);
          d0 |= (1u << 13) | (2u << 10);   /* cond test "==" */
          fl[0] = (uint8_t)d0; fl[1] = (uint8_t)(d0 >> 8); fl[2] = (uint8_t)(d0 >> 16); fl[3] = (uint8_t)(d0 >> 24); }
        CHECK(!vpcpu_run(fl, 1, cc, (const float (*)[4])v, o));
    }
    /* scalar unit writes after the vector unit and reads SRC2: o0.w = 1 / c8.x */
    {
        static uint8_t sc[32];
        float o[4]; float v[16][4]; vpcpu_attr_default(v);
        cc[8 * 4] = 4.0f;
        enc(sc, 0x01, 0x02, 0x3F, 0, 0, 0xE, 0x1, src(T_IN, 0), 0, src_xxxx(T_CONST, 0), 0, 8, 1);
        v[0][0] = 2.0f;
        CHECK(vpcpu_run(sc, 1, cc, (const float (*)[4])v, o));
        /* vec_result = 0: the vector result goes to temps only, the scalar owns o0 */
        CHECK(o[3] == 0.25f);
        CHECK(o[0] == 0.0f);
    }
    if (s_fail) { printf("%d check(s) failed\n", s_fail); return 1; }
    printf("vp_cpu_camera: all checks passed\n");
    return 0;
}
