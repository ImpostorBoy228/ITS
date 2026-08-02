#include "common.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// ----
int main(void) {
    load_finals(FINALS_FILE);
    build_spline();

    double offset = get_offset();
    char off_str[16];
    format_time(offset, off_str, sizeof(off_str));
    printf("Day start offset: %.6f UT1 sec (%s)\n", offset, off_str);

    // Earliest night: date and UTC time of its start
    int ey, em, ed;
    double twi = compute_earliest_night(&ey, &em, &ed);
    double night_mjd = jdn(ey, em, ed) - 2400000.5;
    double night_dut1 = interpolate_dut1_spline(night_mjd);
    double utc_sec = twi - night_dut1;
    while (utc_sec < 0) utc_sec += 86400.0;
    while (utc_sec >= 86400.0) utc_sec -= 86400.0;
    char utc_str[16];
    format_time(utc_sec, utc_str, sizeof(utc_str));
    printf("Earliest night: %04d-%02d-%02d starts at %s UTC\n", ey, em, ed, utc_str);

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

    // dUT1 for nightfall and now
    double now_dut1 = interpolate_dut1_spline(mjd_from_unix(now_ts.tv_sec));

    bigint epoch_ns = timespec_to_ns(&epoch_ts);
    bigint now_ns = timespec_to_ns(&now_ts);
    bigint night_dut1_ns = double_to_ns(night_dut1);
    bigint now_dut1_ns = double_to_ns(now_dut1);
    bigint offset_ns = double_to_ns(offset);

    bigint epoch_start_ns = epoch_ns + night_dut1_ns + offset_ns;
    bigint delta_ns = now_ns + now_dut1_ns - epoch_start_ns;

    // Print total nanoseconds as a signed integer
    printf("Nanoseconds since epoch start: ");
    print_bigint(delta_ns);
    putchar('\n');

    // Floor division to get days and remaining nanoseconds
    bigint sec_ns;
    bigint day_b = fdivmod(delta_ns, (bigint)ITS_DAY_NS, &sec_ns);

    long day = (long)day_b;
    double sec = (double)sec_ns / 1e9;   // seconds within the day

    long years = day / ITS_YEAR_DAYS;
    long rem = day % ITS_YEAR_DAYS;
    if (day < 0 && rem != 0) { years--; rem += ITS_YEAR_DAYS; }
    long months = rem / ITS_MONTH_DAYS;
    rem %= ITS_MONTH_DAYS;
    long days_rem = rem;

    char sec_str[16];
    format_time(sec, sec_str, sizeof(sec_str));
    printf("ITS time: %ldy %ldm %ldd %s\n", years, months, days_rem, sec_str);

    free_eop();

    return 0;
}
