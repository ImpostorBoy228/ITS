#include "test_harness.h"

static void doy_to_md(int year, int doy, int *m, int *d) {
    static const int mdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    for (int i = 0; i < 12; i++) {
        int dim = mdays[i] + (i == 1 ? leap : 0);
        if (doy <= dim) { *m = i + 1; *d = doy; return; }
        doy -= dim;
    }
    *m = 12;
    *d = 31;
}

void test_calendar(void) {
    GROUP_START("mjd_from_unix");
    CHECK_NEAR(mjd_from_unix(0), 40587.0, 1e-9);
    CHECK_NEAR(mjd_from_unix(86400), 40588.0, 1e-9);
    CHECK_NEAR(mjd_from_unix((time_t)EPOCH_UNIX), 61213.0, 1e-9);
    CHECK_NEAR(mjd_from_unix(-86400), 40586.0, 1e-9);
    CHECK_NEAR(mjd_from_unix(43200), 40587.5, 1e-9);
    CHECK_NEAR(mjd_from_unix(2 * 86400), 40589.0, 1e-9);

    GROUP_START("jdn anchors");
    CHECK_NEAR(jdn(1970, 1, 1), 2440587.5, 1e-9);
    CHECK_NEAR(jdn(2000, 1, 1), 2451544.5, 1e-9);
    CHECK_NEAR(jdn(2026, 6, 22), 2461213.5, 1e-9);
    CHECK_NEAR(jdn(1970, 1, 1) - 2400000.5, mjd_from_unix(0), 1e-9);

    GROUP_START("jdn adjacency (every consecutive pair of 2026)");
    for (int doy = 1; doy < 365; doy++) {
        int m1, d1, m2, d2;
        doy_to_md(2026, doy, &m1, &d1);
        doy_to_md(2026, doy + 1, &m2, &d2);
        CHECK_NEAR(jdn(2026, m2, d2) - jdn(2026, m1, d1), 1.0, 1e-9);
    }

    GROUP_START("jdn leap-year rules");
    CHECK_NEAR(jdn(2000, 3, 1) - jdn(2000, 2, 29), 1.0, 1e-9);
    CHECK_NEAR(jdn(2000, 3, 1) - jdn(2000, 2, 28), 2.0, 1e-9);
    CHECK_NEAR(jdn(1900, 3, 1) - jdn(1900, 2, 28), 1.0, 1e-9);
    CHECK_NEAR(jdn(2024, 3, 1) - jdn(2024, 2, 29), 1.0, 1e-9);
    CHECK_NEAR(jdn(2026, 3, 1) - jdn(2026, 2, 28), 1.0, 1e-9);

    GROUP_START("decompose_its golden");
    const struct { long day; long y, m, dr; } dc[] = {
        {0, 0, 0, 0},
        {1, 0, 0, 1},
        {20, 0, 0, 20},
        {21, 0, 1, 0},
        {146, 0, 6, 20},
        {147, 1, 0, 0},
        {148, 1, 0, 1},
        {294, 2, 0, 0},
        {-1, -1, 6, 20},
        {-21, -1, 6, 0},
        {-22, -1, 5, 20},
        {-147, -1, 0, 0},
        {-148, -2, 6, 20},
    };
    for (size_t i = 0; i < sizeof(dc) / sizeof(dc[0]); i++) {
        long y, m, dr;
        decompose_its(dc[i].day, &y, &m, &dr);
        CHECK_MSG(y == dc[i].y && m == dc[i].m && dr == dc[i].dr,
                  "decompose(%ld) got (%ld,%ld,%ld) want (%ld,%ld,%ld)",
                  dc[i].day, y, m, dr, dc[i].y, dc[i].m, dc[i].dr);
    }

    GROUP_START("decompose_its property (day in [-500,500])");
    for (long day = -500; day <= 500; day++) {
        long y, m, dr;
        decompose_its(day, &y, &m, &dr);
        CHECK_MSG(day == y * ITS_YEAR_DAYS + m * ITS_MONTH_DAYS + dr,
                  "reconstruct day=%ld y=%ld m=%ld dr=%ld", day, y, m, dr);
        CHECK_MSG(m >= 0 && m <= 6, "months range day=%ld m=%ld", day, m);
        CHECK_MSG(dr >= 0 && dr <= 20, "days_rem range day=%ld dr=%ld", day, dr);
    }

    GROUP_START("ITS day arithmetic via fdivmod");
    {
        bigint r;
        CHECK_BIG(ITS_DAY_NS, (bigint)86400000000000);
        CHECK_BIG(fdivmod(ITS_DAY_NS * 1234, ITS_DAY_NS, &r), (bigint)1234);
        CHECK(r == 0);
        CHECK_BIG(fdivmod(ITS_DAY_NS * 1234 + 5, ITS_DAY_NS, &r), (bigint)1234);
        CHECK(r == 5);
        CHECK_BIG(fdivmod(-ITS_DAY_NS * 1234 - 5, ITS_DAY_NS, &r), (bigint)-1235);
        CHECK(r == ITS_DAY_NS - 5);
    }
}
