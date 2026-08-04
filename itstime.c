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

    struct timespec now_ts;
    if (clock_gettime(CLOCK_REALTIME, &now_ts) == -1) { perror("clock_gettime"); exit(1); }

    bigint delta_ns = its_elapsed_ns(&now_ts, offset);

    if (!human && !jap && !astro) {
        print_bigint(delta_ns);
        putchar('\n');
    } else {
        bigint sec_ns;
        bigint day_b = fdivmod(delta_ns, ITS_DAY_NS, &sec_ns);

        long day = (long)day_b;
        double sec = (double)sec_ns / 1e9;
        long years, months, days_rem;
        decompose_its(day, &years, &months, &days_rem);

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
