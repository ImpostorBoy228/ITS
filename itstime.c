#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <gmp.h>

static void timespec_to_ns(const struct timespec *ts, mpz_t out) {
    mpz_set_si(out, ts->tv_sec);
    mpz_mul_ui(out, out, 1000000000ULL);
    mpz_add_ui(out, out, ts->tv_nsec);
}

static void double_to_ns(double val, mpz_t out) {
    mpz_set_d(out, val * 1e9);
}

int main(int argc, char **argv) {
    int human = 0;
    int jap = 0;
    if (argc == 2 && strcmp(argv[1], "-h") == 0) human = 1;
    else if (argc == 2 && strcmp(argv[1], "-j") == 0) jap = 1;
    else if (argc > 1) { fprintf(stderr, "Usage: %s [-h]\n", argv[0]); exit(1); }

    load_finals(FINALS_FILE);
    build_spline();

    double offset = get_offset();
    int ey, em, ed;
    compute_earliest_night(&ey, &em, &ed);
    double night_mjd = jdn(ey, em, ed) - 2400000.5;
    double night_dut1 = interpolate_dut1_spline(night_mjd);

    struct timespec now_ts;
    if (clock_gettime(CLOCK_REALTIME, &now_ts) == -1) { perror("clock_gettime"); exit(1); }

    double now_dut1 = interpolate_dut1_spline(mjd_from_unix(now_ts.tv_sec));

    struct timespec epoch_ts;
    epoch_ts.tv_sec = (time_t)EPOCH_UNIX;
    epoch_ts.tv_nsec = 0;

    mpz_t epoch_ns, now_ns;
    mpz_t night_dut1_ns, now_dut1_ns;
    mpz_t offset_ns;
    mpz_t epoch_start_ns;
    mpz_t delta_ns;
    mpz_t divisor;
    mpz_t day_mpz, sec_ns_mpz;

    mpz_init(epoch_ns);
    mpz_init(now_ns);
    mpz_init(night_dut1_ns);
    mpz_init(now_dut1_ns);
    mpz_init(offset_ns);
    mpz_init(epoch_start_ns);
    mpz_init(delta_ns);
    mpz_init_set_ui(divisor, ITS_DAY_NS);
    mpz_init(day_mpz);
    mpz_init(sec_ns_mpz);

    timespec_to_ns(&epoch_ts, epoch_ns);
    timespec_to_ns(&now_ts, now_ns);

    double_to_ns(night_dut1, night_dut1_ns);
    double_to_ns(now_dut1, now_dut1_ns);
    double_to_ns(offset, offset_ns);

    mpz_add(epoch_start_ns, epoch_ns, night_dut1_ns);
    mpz_add(epoch_start_ns, epoch_start_ns, offset_ns);

    mpz_add(delta_ns, now_ns, now_dut1_ns);
    mpz_sub(delta_ns, delta_ns, epoch_start_ns);

    if (!human && !jap) {
        gmp_printf("%Zd\n", delta_ns);
    } else {
        mpz_fdiv_q(day_mpz, delta_ns, divisor);
        mpz_fdiv_r(sec_ns_mpz, delta_ns, divisor);

        long day = mpz_get_si(day_mpz);
        double sec = mpz_get_d(sec_ns_mpz) / 1e9;
        long years = day / ITS_YEAR_DAYS;
        long rem = day % ITS_YEAR_DAYS;
        if (day < 0 && rem != 0) { years--; rem += ITS_YEAR_DAYS; }
        long months = rem / ITS_MONTH_DAYS;
        rem %= ITS_MONTH_DAYS;
        long days_rem = rem;

        char sec_str[16];
        if (human) {
            format_time(sec, sec_str, sizeof(sec_str));
            printf("%ldy %ldm %ldd %s\n", years, months, days_rem, sec_str);
        } else {
            format_time_j(sec, sec_str, sizeof(sec_str));
            printf("%ld年%ld月%ld日%s\n", years, months, days_rem, sec_str);
        }
    }

    mpz_clear(epoch_ns);
    mpz_clear(now_ns);
    mpz_clear(night_dut1_ns);
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
