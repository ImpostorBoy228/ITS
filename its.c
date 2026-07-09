#define _GNU_SOURCE
#include "common.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gmp.h>

static void double_to_ns(double val, mpz_t out) {
    double ns = val * 1e9;
    mpz_set_d(out, ns);
}

/* Convert struct timespec to mpz_t nanoseconds since epoch */
static void timespec_to_ns(const struct timespec *ts, mpz_t out) {
    mpz_set_si(out, ts->tv_sec);
    mpz_mul_ui(out, out, 1000000000ULL);
    mpz_add_ui(out, out, ts->tv_nsec);
}

// ----
int main(void) {
    load_finals(FINALS_FILE);
    build_spline();

    double offset = get_offset();
    char off_str[16];
    format_time(offset, off_str, sizeof(off_str));
    printf("Day start offset: %.6f UT1 sec (%s)\n", offset, off_str);

    // Earliest night: date and UTC time of its start
    {
        int ey, em, ed;
        double twi = compute_earliest_night(&ey, &em, &ed);
        double mjd = jdn(ey, em, ed) - 2400000.5;
        double dut1 = interpolate_dut1_spline(mjd);
        double utc_sec = twi - dut1;
        while (utc_sec < 0) utc_sec += 86400.0;
        while (utc_sec >= 86400.0) utc_sec -= 86400.0;
        char utc_str[16];
        format_time(utc_sec, utc_str, sizeof(utc_str));
        printf("Earliest night: %04d-%02d-%02d starts at %s UTC\n", ey, em, ed, utc_str);
    }

    // Epoch in UTC as struct timespec
    struct timespec epoch_ts;
    epoch_ts.tv_sec = (time_t)EPOCH_UNIX;
    epoch_ts.tv_nsec = 0;

    // Current UTC time with nanosecond resolution
    struct timespec now_ts;
    if (clock_gettime(CLOCK_REALTIME, &now_ts) == -1) {
        perror("clock_gettime");
        exit(1);
    }

    // dUT1 for epoch and now (spline gives double)
    double epoch_dut1 = interpolate_dut1_spline(mjd_from_unix(epoch_ts.tv_sec));
    double now_dut1 = interpolate_dut1_spline(mjd_from_unix(now_ts.tv_sec));

    // oh fuck we are going to have big shit
    mpz_t epoch_ns, now_ns;
    mpz_t epoch_dut1_ns, now_dut1_ns;
    mpz_t offset_ns;
    mpz_t epoch_start_ns;   /* start of day 0 in UT1 */
    mpz_t delta_ns;         /* nanoseconds from epoch start */
    mpz_t divisor;
    mpz_t day_mpz, sec_ns_mpz;

    mpz_init(epoch_ns);
    mpz_init(now_ns);
    mpz_init(epoch_dut1_ns);
    mpz_init(now_dut1_ns);
    mpz_init(offset_ns);
    mpz_init(epoch_start_ns);
    mpz_init(delta_ns);
    mpz_init_set_ui(divisor, 86400000000000ULL);  // 86400 * 1e9
    mpz_init(day_mpz);
    mpz_init(sec_ns_mpz);

    timespec_to_ns(&epoch_ts, epoch_ns);
    timespec_to_ns(&now_ts, now_ns);

    double_to_ns(epoch_dut1, epoch_dut1_ns);
    double_to_ns(now_dut1, now_dut1_ns);
    double_to_ns(offset, offset_ns);

    /* epoch_start_ns = epoch_ns + epoch_dut1_ns + offset_ns */
    mpz_add(epoch_start_ns, epoch_ns, epoch_dut1_ns);
    mpz_add(epoch_start_ns, epoch_start_ns, offset_ns);

    /* delta_ns = now_ns + now_dut1_ns - epoch_start_ns */
    mpz_add(delta_ns, now_ns, now_dut1_ns);
    mpz_sub(delta_ns, delta_ns, epoch_start_ns);

    // Print total nanoseconds as a signed integer
    gmp_printf("Nanoseconds since epoch start: %Zd\n", delta_ns);

    /* Use GMP division to get days and remaining nanoseconds */
    mpz_fdiv_q(day_mpz, delta_ns, divisor);
    mpz_fdiv_r(sec_ns_mpz, delta_ns, divisor);

    long day = mpz_get_si(day_mpz);
    double sec = mpz_get_d(sec_ns_mpz) / 1e9;   // seconds within the day

    long years = day / ITS_YEAR_DAYS;
    long rem = day % ITS_YEAR_DAYS;
    long months = rem / ITS_MONTH_DAYS;
    rem %= ITS_MONTH_DAYS;
    long days_rem = rem;

    char sec_str[16];
    format_time(sec, sec_str, sizeof(sec_str));
    printf("ITS time: %ldy %ldm %ldd %s\n", years, months, days_rem, sec_str);

    // clear all GMP variables
    mpz_clear(epoch_ns);
    mpz_clear(now_ns);
    mpz_clear(epoch_dut1_ns);
    mpz_clear(now_dut1_ns);
    mpz_clear(offset_ns);
    mpz_clear(epoch_start_ns);
    mpz_clear(delta_ns);
    mpz_clear(divisor);
    mpz_clear(day_mpz);
    mpz_clear(sec_ns_mpz);

    free_eop();

    return 0;
}
