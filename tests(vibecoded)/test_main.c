#include "test_harness.h"

unsigned long g_checks = 0;
unsigned long g_failed = 0;
int g_groups = 0;
int g_has_finals = 1;

static int g_saved_fd = -1;

void cap_start(const char *path) {
    fflush(stdout);
    g_saved_fd = dup(STDOUT_FILENO);
    if (g_saved_fd < 0) { perror("dup"); exit(1); }
    FILE *f = freopen(path, "w", stdout);
    if (!f) { perror("freopen"); exit(1); }
}

const char *cap_read(const char *path) {
    fflush(stdout);
    fclose(stdout);
    if (dup2(g_saved_fd, STDOUT_FILENO) < 0) { perror("dup2"); exit(1); }
    close(g_saved_fd);
    g_saved_fd = -1;
    stdout = fdopen(STDOUT_FILENO, "w");
    if (!stdout) { perror("fdopen"); exit(1); }
    FILE *f = fopen(path, "r");
    if (!f) { perror("fopen"); exit(1); }
    static char buf[256];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);
    return buf;
}

static unsigned long g_rng_state = 0x9E3779B97F4A7C15UL;

void test_srand(unsigned long seed) { g_rng_state = seed; }

double test_rand(void) {
    g_rng_state = g_rng_state * 6364136223846793005UL + 1442695040888963407UL;
    return (double)((g_rng_state >> 11) & 0xFFFFFFFFUL) / (double)0x100000000UL;
}

int main(void) {
    printf("ITS test suite (C23, bigint)\n");
    test_arith();
    test_calendar();
    test_format();
    test_spline();
    test_sun();
    test_offset();
    test_its_elapsed();
    printf("\n");
    if (g_failed == 0) {
        printf("ALL TESTS PASSED (%lu checks, %d groups)\n", g_checks, g_groups);
        return 0;
    }
    printf("FAILURES: %lu of %lu checks failed in %d groups\n",
           g_failed, g_checks, g_groups);
    return 1;
}
