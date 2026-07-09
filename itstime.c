// itstime.c
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

int main(int argc, char **argv) {
    int human = 0;
    int jap = 0;
    if (argc == 2 && strcmp(argv[1], "-h") == 0) human = 1;
    else if (argc == 2 && strcmp(argv[1], "-j") == 0) jap = 1;
    else if (argc > 1) { fprintf(stderr, "Usage: %s [-h]\n", argv[0]); exit(1); }

    load_finals(FINALS_FILE);
    build_spline();

    double offset = get_offset();

    struct timespec now_ts;
    if (clock_gettime(CLOCK_REALTIME, &now_ts) == -1) { perror("clock_gettime"); exit(1); }

    double epoch_dut1 = interpolate_dut1_spline(mjd_from_unix((time_t)EPOCH_UNIX));
    double now_dut1 = interpolate_dut1_spline(mjd_from_unix(now_ts.tv_sec));

    double epoch_start_sec = EPOCH_UNIX + epoch_dut1 + offset;
    double now_ut1_sec = now_ts.tv_sec + now_dut1 + now_ts.tv_nsec / 1e9;
    double delta_sec = now_ut1_sec - epoch_start_sec;
    int64_t delta_ns = (int64_t)(delta_sec * 1e9 + 0.5);

    int64_t day_ns = (int64_t)ITS_DAY_NS;
    int64_t days = delta_ns / day_ns;
    int64_t rem_ns = delta_ns % day_ns;
    // floor division so negative deltas stay consistent with `its`
    if (days < 0 && rem_ns != 0) { days--; rem_ns += day_ns; }
    double sec = rem_ns / 1e9;

    int64_t years = days / ITS_YEAR_DAYS;
    int64_t rem = days % ITS_YEAR_DAYS;
    if (years < 0 && rem != 0) { years--; rem += ITS_YEAR_DAYS; }
    int64_t months = rem / ITS_MONTH_DAYS;
    int64_t days_rem = rem % ITS_MONTH_DAYS;

    if (human) {
        char sec_str[16];
        format_time(sec, sec_str, sizeof(sec_str));
        printf("%ldy %ldm %ldd %s\n", years, months, days_rem, sec_str);
    } else if (jap) {
        char sec_str[16];
        format_time_j(sec, sec_str, sizeof(sec_str));
        printf("%ld年%ld月%ld日%s\n", years, months, days_rem, sec_str);
    } else {
        printf("%lld\n", (long long)delta_ns);
    }

    free_eop();
    return 0;
}
