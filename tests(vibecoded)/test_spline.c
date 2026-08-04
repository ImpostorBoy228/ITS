#include "test_harness.h"

typedef struct { double mjd; double dut1; } Knot;

static void write_fixture(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) { perror("fixture open"); exit(1); }
    const double knots[4][2] = {
        {60000.0, 0.10}, {60001.0, 0.20}, {60002.0, 0.10}, {60003.0, 0.00},
    };
    for (int i = 0; i < 4; i++) {
        char line[80];
        char mjd_buf[16], dut1_buf[16];
        memset(line, ' ', sizeof(line));
        memcpy(line, "73 1 1 ", 7);
        snprintf(mjd_buf, sizeof(mjd_buf), "%7.2f", knots[i][0]);
        memcpy(line + 7, mjd_buf, strlen(mjd_buf));
        snprintf(dut1_buf, sizeof(dut1_buf), "%11.7f", knots[i][1]);
        memcpy(line + 58, dut1_buf, strlen(dut1_buf));
        line[69] = '\n';
        line[70] = '\0';
        fwrite(line, 1, 70, f);
        if (i == 2) {
            char blank[80];
            char blank_mjd[16];
            memset(blank, ' ', sizeof(blank));
            memcpy(blank, "73 1 1 ", 7);
            snprintf(blank_mjd, sizeof(blank_mjd), "%7.2f", 60002.5);
            memcpy(blank + 7, blank_mjd, strlen(blank_mjd));
            blank[69] = '\n';
            blank[70] = '\0';
            fwrite(blank, 1, 70, f);
        }
    }
    fclose(f);
}

static Knot *parse_knots(const char *path, int *count_out) {
    FILE *f = fopen(path, "r");
    if (!f) { *count_out = -1; return NULL; }
    int cap = 2048, n = 0;
    Knot *k = malloc(cap * sizeof(Knot));
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strlen(line) < 68) continue;
        if (strncmp(line, "MJD", 3) == 0) continue;
        char buf[12];
        strncpy(buf, line + 58, 11);
        buf[11] = '\0';
        if (strspn(buf, " \t\r\n") == strlen(buf)) continue;
        char mjd_str[10];
        strncpy(mjd_str, line + 7, 8);
        mjd_str[8] = '\0';
        double mjd = atof(mjd_str);
        double dut1 = atof(buf);
        if (mjd > 0 && dut1 > -10 && dut1 < 10) {
            if (n == cap) { cap *= 2; k = realloc(k, (size_t)cap * sizeof(Knot)); }
            k[n].mjd = mjd;
            k[n].dut1 = dut1;
            n++;
        }
    }
    fclose(f);
    *count_out = n;
    return k;
}

void test_spline(void) {
    const char *fixture_path = "/tmp/its_fixture";

    GROUP_START("spline: synthetic fixture");
    write_fixture(fixture_path);
    free_eop();
    load_finals(fixture_path);
    build_spline();
    CHECK_NEAR(interpolate_dut1_spline(60000.0), 0.10, 1e-6);
    CHECK_NEAR(interpolate_dut1_spline(60001.0), 0.20, 1e-6);
    CHECK_NEAR(interpolate_dut1_spline(60002.0), 0.10, 1e-6);
    CHECK_NEAR(interpolate_dut1_spline(60003.0), 0.00, 1e-6);
    {
        double v = interpolate_dut1_spline(60000.5);
        CHECK_MSG(v > 0.10 && v < 0.20, "fixture midpoint 60000.5 got %g", v);
        CHECK_NEAR(v, 0.16, 1e-6);
    }
    {
        double v = interpolate_dut1_spline(60002.5);
        CHECK_MSG(v > 0.0 && v < 0.10, "blank knot skipped: 60002.5 got %g", v);
    }
    CHECK_NEAR(interpolate_dut1_spline(59999.0), 0.10, 1e-6);
    CHECK_NEAR(interpolate_dut1_spline(60004.0), 0.00, 1e-6);
    free_eop();

    GROUP_START("spline: real finals.all");
    {
        FILE *probe = fopen(FINALS_FILE, "r");
        if (!probe) {
            g_has_finals = 0;
            SKIP("finals.all not found");
            return;
        }
        fclose(probe);
    }
    load_finals(FINALS_FILE);
    build_spline();

    int nk = 0;
    Knot *knots = parse_knots(FINALS_FILE, &nk);
    if (!knots) {
        g_has_finals = 0;
        SKIP("finals.all unreadable");
        return;
    }
    printf("   finals.all: %d real knots parsed\n", nk);

    CHECK_NEAR(interpolate_dut1_spline(41684.0), 0.8084178, 1e-6);
    CHECK_NEAR(interpolate_dut1_spline(61213.0), 0.0113315, 1e-6);
    CHECK_NEAR(interpolate_dut1_spline(61386.0), -0.0547940, 1e-6);
    CHECK_NEAR(interpolate_dut1_spline(61624.0), -0.0559647, 1e-6);
    CHECK_NEAR(interpolate_dut1_spline(61674.0), -0.0559647, 1e-6);
    CHECK_NEAR(interpolate_dut1_spline(40587.0), 0.8084178, 1e-6);

    CHECK(nk == 19941);
    CHECK_NEAR(knots[0].mjd, 41684.0, 1e-9);
    CHECK_NEAR(knots[0].dut1, 0.8084178, 1e-9);
    CHECK_NEAR(knots[nk - 1].mjd, 61624.0, 1e-9);
    CHECK_NEAR(knots[nk - 1].dut1, -0.0559647, 1e-9);

    for (int i = 0; i < nk; i++) {
        double got = interpolate_dut1_spline(knots[i].mjd);
        CHECK_NEAR(got, knots[i].dut1, 1e-6);
    }

    double min_d = knots[0].dut1, max_d = knots[0].dut1;
    for (int i = 1; i < nk; i++) {
        if (knots[i].dut1 < min_d) min_d = knots[i].dut1;
        if (knots[i].dut1 > max_d) max_d = knots[i].dut1;
    }
    test_srand(42);
    for (int i = 0; i < 200; i++) {
        double mjd = 41000.0 + test_rand() * 21000.0;
        double got = interpolate_dut1_spline(mjd);
        CHECK_MSG(got >= min_d - 0.1 && got <= max_d + 0.1,
                  "sanity interp(%g)=%g outside [%g,%g]",
                  mjd, got, min_d - 0.1, max_d + 0.1);
    }

    free(knots);
}
