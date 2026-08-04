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
    double utc_sec = twi;
    while (utc_sec < 0) utc_sec += 86400.0;
    while (utc_sec >= 86400.0) utc_sec -= 86400.0;
    char utc_str[16];
    format_time(utc_sec, utc_str, sizeof(utc_str));
    printf("Earliest night: %04d-%02d-%02d starts at %s UTC\n", ey, em, ed, utc_str);

    // Current UTC time with nanosecond resolution
    struct timespec now_ts;
    if (clock_gettime(CLOCK_REALTIME, &now_ts) == -1) {
        perror("clock_gettime");
        exit(1);
    }

    bigint delta_ns = its_elapsed_ns(&now_ts, offset);

    // Print total nanoseconds as a signed integer
    printf("Nanoseconds since epoch start: ");
    print_bigint(delta_ns);
    putchar('\n');

    // Floor division to get days and remaining nanoseconds
    bigint sec_ns;
    bigint day_b = fdivmod(delta_ns, ITS_DAY_NS, &sec_ns);

    long day = (long)day_b;
    double sec = (double)sec_ns / 1e9;   // seconds within the day

    long years, months, days_rem;
    decompose_its(day, &years, &months, &days_rem);

    char sec_str[16];
    format_time(sec, sec_str, sizeof(sec_str));
    printf("ITS time: %ldy %ldm %ldd %s\n", years, months, days_rem, sec_str);

    free_eop();

    return 0;
}
