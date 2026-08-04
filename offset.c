// offset.c - generate offset.dat (UT1 seconds of day of the earliest
// astronomical nightfall in Novosibirsk over the 1976-2026 window).
#include "common.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    load_finals(FINALS_FILE);
    build_spline();
    double offset = compute_offset();
    if (offset < 0.0) { fprintf(stderr, "Offset computation fuckup\n"); return 1; }
    FILE *f = fopen(OFFSET_FILE, "w");
    if (!f) { perror("offset.dat"); return 1; }
    fprintf(f, "%.6f\n", offset);
    fclose(f);
    return 0;
}
