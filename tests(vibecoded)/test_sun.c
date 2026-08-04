#include "test_harness.h"

static int days_in_month(int y, int m) {
    if (m == 2) return (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 29 : 28;
    if (m == 4 || m == 6 || m == 9 || m == 11) return 30;
    return 31;
}

static void doy_to_md(int year, int doy, int *m, int *d) {
    for (int i = 1; i <= 12; i++) {
        int dim = days_in_month(year, i);
        if (doy <= dim) { *m = i; *d = doy; return; }
        doy -= dim;
    }
    *m = 12;
    *d = 31;
}

void test_sun(void) {
    GROUP_START("sun_position");
    double decl, eq;
    sun_position(jdn(2026, 6, 22), &decl, &eq);
    CHECK_MSG(decl >= 23.0 && decl <= 24.0, "summer solstice decl=%g", decl);
    CHECK_MSG(fabs(eq) < 1020.0, "summer eq_time=%g", eq);
    sun_position(jdn(2026, 12, 22), &decl, &eq);
    CHECK_MSG(decl >= -24.0 && decl <= -23.0, "winter solstice decl=%g", decl);
    CHECK_MSG(fabs(eq) < 1020.0, "winter eq_time=%g", eq);
    sun_position(jdn(2000, 3, 20), &decl, &eq);
    CHECK_MSG(decl >= -1.0 && decl <= 1.0, "vernal equinox decl=%g", decl);

    GROUP_START("hour_angle");
    CHECK_NEAR(hour_angle(66.5, 23.44, 90.833, 1), -1.0, 1e-12);
    CHECK_NEAR(hour_angle(90.0, 23.0, 90.833, 1), -1.0, 1e-12);
    CHECK(hour_angle(0.0, 0.0, 90.833, 1) > 0.0);
    CHECK(hour_angle(0.0, 0.0, 90.833, -1) < 0.0);
    CHECK(hour_angle(90.0, -23.0, 90.833, 1) == -1.0);
    CHECK(hour_angle(0.0, 23.44, 90.833, 1) > 0.0);

    GROUP_START("compute_times golden");
    double sunset, twi, daylen;
    int has_night;
    compute_times(2026, 6, 22, &sunset, &twi, &daylen, &has_night);
    CHECK(has_night == 0);
    compute_times(2026, 12, 12, &sunset, &twi, &daylen, &has_night);
    CHECK(has_night == 1);
    CHECK_MSG(sunset >= 0.0 && sunset < twi, "sunset=%g twi=%g", sunset, twi);
    CHECK_MSG(twi <= 86400.0, "twilight=%g", twi);
    CHECK_MSG(daylen > 0.0 && daylen <= 86400.0, "daylen=%g", daylen);

    GROUP_START("compute_times / sun_position every 7th day of 2026");
    for (int doy = 1; doy <= 365; doy += 7) {
        int m, d;
        doy_to_md(2026, doy, &m, &d);
        double s, t, dl, dc, eq2;
        int hn;
        compute_times(2026, m, d, &s, &t, &dl, &hn);
        sun_position(jdn(2026, m, d), &dc, &eq2);
        CHECK_MSG(dc >= -23.5 && dc <= 23.5, "decl out of range doy=%d decl=%g", doy, dc);
        if (hn) {
            CHECK_MSG(s >= 0.0 && s < t && t <= 86400.0,
                      "times out of order doy=%d sunset=%g twi=%g", doy, s, t);
            CHECK_MSG(dl > 0.0 && dl <= 86400.0, "daylen out of range doy=%d dl=%g", doy, dl);
        } else {
            CHECK_MSG(s == -1.0 && t == -1.0 && dl == 0.0,
                      "no-night day must return -1/-1/0, doy=%d", doy);
        }
    }
}
