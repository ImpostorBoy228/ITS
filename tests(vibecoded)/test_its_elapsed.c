#include "test_harness.h"

static void ts_from_ns(bigint ns, struct timespec *ts) {
    ts->tv_sec = (time_t)(ns / 1000000000);
    ts->tv_nsec = (long)(ns % 1000000000);
}

void test_its_elapsed(void) {
    GROUP_START("its_elapsed_ns (deterministic, fixed instants)");
    if (!g_has_finals) {
        SKIP("finals.all missing");
        return;
    }

    double off = get_offset();
    double dut1_epoch = interpolate_dut1_spline(61213.0);
    bigint E = (bigint)(time_t)EPOCH_UNIX * 1000000000
             + double_to_ns(off)
             - double_to_ns(dut1_epoch);

    struct timespec ts;
    ts_from_ns(E, &ts);
    CHECK_BIG(its_elapsed_ns(&ts, off), (bigint)0);

    struct timespec ts1s = { ts.tv_sec + 1, ts.tv_nsec };
    CHECK_BIG(its_elapsed_ns(&ts1s, off), (bigint)1000000000);

    struct timespec ts1d = { ts.tv_sec + 86400, ts.tv_nsec };
    CHECK_BIG(its_elapsed_ns(&ts1d, off), (bigint)ITS_DAY_NS);

    for (int k = -50; k < 150; k++) {
        bigint base = E + (bigint)k * ITS_DAY_NS / 10;
        struct timespec t1, t2;
        ts_from_ns(base, &t1);
        t2 = t1;
        t2.tv_sec += 86400;
        bigint e1 = its_elapsed_ns(&t1, off);
        bigint e2 = its_elapsed_ns(&t2, off);
        CHECK_MSG(e2 - e1 == ITS_DAY_NS,
                  "instants 86400s apart must differ by exactly one ITS day (k=%d)", k);
    }
}
