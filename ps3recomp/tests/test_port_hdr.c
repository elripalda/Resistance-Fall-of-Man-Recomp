/* port_hdr.h: the SDR -> HDR expansion curve the present pass uses, and its inverse. */
#include <stdio.h>
#include <math.h>
#include "port_hdr.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(void)
{
    const float ratios[] = { 1.0f, 203.0f / 203.0f, 400.0f / 203.0f, 1000.0f / 203.0f, 4000.0f / 100.0f };
    for (unsigned r = 0; r < sizeof ratios / sizeof *ratios; r++) {
        const float R = ratios[r];
        CHECK(fabsf(port_hdr_expand(1.0f, R) - R) < 1e-4f * R, "white -> peak, R %.3f got %f", R, port_hdr_expand(1.0f, R));
        float prev = -1.0f;
        for (int i = 0; i <= 4096; i++) {
            const float y = i / 4096.0f, y2 = port_hdr_expand(y, R);
            CHECK(y2 >= prev, "monotonic, R %.3f y %f", R, y);
            prev = y2;
            if (y <= PORT_HDR_KNEE) CHECK(y2 == y, "identity below the knee, R %.3f y %f", R, y);
            const float back = port_hdr_compress(y2, R);
            CHECK(fabsf(back - y) < 2e-4f, "round trip, R %.3f y %f back %f", R, y, back);
        }
        /* slope 1 at the knee: no seam where expansion starts */
        const float h = 1e-3f;
        const float s = (port_hdr_expand(PORT_HDR_KNEE + h, R) - port_hdr_expand(PORT_HDR_KNEE, R)) / h;
        CHECK(fabsf(s - 1.0f) < 1e-2f, "C1 at the knee, R %.3f slope %f", R, s);
        CHECK(port_hdr_compress(R * 2.0f, R) == 1.0f, "compress clamps above peak, R %.3f", R);
    }
    for (float n = 0.0f; n <= 10000.0f; n += 37.0f) {
        const float d = port_hdr_pq_decode(port_hdr_pq_encode(n));
        CHECK(fabsf(d - n) < 0.01f * n + 0.01f, "PQ round trip %f -> %f", n, d);
    }
    CHECK(fabsf(port_hdr_pq_encode(10000.0f) - 1.0f) < 1e-5f, "PQ 10000 nits = 1");
    CHECK(fabsf(port_hdr_pq_encode(100.0f) - 0.5081f) < 1e-3f, "PQ 100 nits = 0.508, got %f", port_hdr_pq_encode(100.0f));
    if (!fails) printf("port_hdr: all checks passed\n");
    return fails != 0;
}
