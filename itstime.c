#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
    int human = 0;
    int jap = 0;
    int astro = 0;
    if (argc == 2 && strcmp(argv[1], "-h") == 0) human = 1;
    else if (argc == 2 && strcmp(argv[1], "-j") == 0) jap = 1;
    else if (argc == 2 && strcmp(argv[1], "-A") == 0) astro = 1;
    else if (argc > 1) { fprintf(stderr, "Usage: %s [-h|j|A]\n", argv[0]); exit(1); }

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

    bigint epoch_ns = timespec_to_ns(&epoch_ts);
    bigint now_ns = timespec_to_ns(&now_ts);
    bigint night_dut1_ns = double_to_ns(night_dut1);
    bigint now_dut1_ns = double_to_ns(now_dut1);
    bigint offset_ns = double_to_ns(offset);

    bigint epoch_start_ns = epoch_ns + night_dut1_ns + offset_ns;
    bigint delta_ns = now_ns + now_dut1_ns - epoch_start_ns;

    if (!human && !jap && !astro) {
        print_bigint(delta_ns);
        putchar('\n');
    } else {
        bigint sec_ns;
        bigint day_b = fdivmod(delta_ns, (bigint)ITS_DAY_NS, &sec_ns);

        long day = (long)day_b;
        double sec = (double)sec_ns / 1e9;
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
        } else if (astro) {
            format_time(sec, sec_str, sizeof(sec_str));
            printf("%ldy %ldm %ldd %s\n", years, months, days_rem, sec_str);
            Vec3 nsk = getNsk(mjd_from_unix(now_ts.tv_sec));
            Vec3 sun = getSun(mjd_from_unix(now_ts.tv_sec));
            printf("NSK: (%.3f, %.3f, %.3f)  ", nsk.x, nsk.y, nsk.z);
            printf("Sun: (%.3f, %.3f, %.3f)\n", sun.x, sun.y, sun.z);
        } else {
            format_time_j(sec, sec_str, sizeof(sec_str));
            printf("%ld年%ld月%ld日%s\n", years, months, days_rem, sec_str);
        }
    }

    free_eop();
    return 0;
}
