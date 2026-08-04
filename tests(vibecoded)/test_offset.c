#include "test_harness.h"

void test_offset(void) {
    GROUP_START("compute_earliest_night");
    int y = 0, m = 0, d = 0;
    double twi = compute_earliest_night(&y, &m, &d);
    CHECK(y == 2026);
    CHECK(m == 12);
    CHECK(d == 12);
    CHECK_MSG(twi >= 44193.5 && twi <= 44194.0, "earliest nightfall=%g", twi);

    GROUP_START("compute_offset / get_offset");
    if (!g_has_finals) {
        SKIP("finals.all missing; only DUT1-free offset range checked");
        double off = compute_offset();
        CHECK_MSG(off >= 44193.5 && off <= 44194.0, "offset(no DUT1)=%g", off);
    } else {
        double off = compute_offset();
        CHECK_MSG(off >= 44193.5 && off <= 44193.9, "compute_offset=%g", off);
        double got = get_offset();
        CHECK_NEAR(got, 44193.737495, 1e-6);
        CHECK_MSG(fabs(off - got) < 0.001, "offset drift %g vs %g", off, got);
    }
}
