// itstime.c
#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

static double *mjd_arr = NULL;
static double *dut1_arr = NULL;
static int n_eop = 0;
static double *second_deriv = NULL;

double mjd_from_unix(time_t t) {
    return (double)t / SECS_PER_DAY + 40587.0;
}

void load_finals(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) { perror("finals.all"); exit(1); }
    char line[256];
    int count = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strlen(line) < 68) continue;
        if (strncmp(line, "MJD", 3) == 0) continue;
        count++;
    }
    rewind(f);
    mjd_arr = malloc(count * sizeof(double));
    dut1_arr = malloc(count * sizeof(double));
    if (!mjd_arr || !dut1_arr) { fprintf(stderr, "malloc failed\n"); exit(1); }
    int i = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strlen(line) < 68) continue;
        if (strncmp(line, "MJD", 3) == 0) continue;
        char mjd_str[10], dut1_str[12];
        strncpy(mjd_str, line + 7, 8); mjd_str[8] = '\0';
        strncpy(dut1_str, line + 57, 10); dut1_str[10] = '\0';
        double mjd = atof(mjd_str);
        double dut1 = atof(dut1_str);
        if (mjd > 0 && dut1 > -10 && dut1 < 10) {
            mjd_arr[i] = mjd;
            dut1_arr[i] = dut1;
            i++;
        }
    }
    n_eop = i;
    fclose(f);
}

void build_spline(void) {
    if (n_eop < 2) {
        second_deriv = NULL;
        return;
    }
    second_deriv = malloc(n_eop * sizeof(double));
    int n = n_eop;
    double *h = malloc((n-1) * sizeof(double));
    double *b = malloc((n-1) * sizeof(double));
    for (int i=0; i<n-1; i++) {
        h[i] = mjd_arr[i+1] - mjd_arr[i];
        b[i] = (dut1_arr[i+1] - dut1_arr[i]) / h[i];
    }
    double *d = malloc(n * sizeof(double));
    double *l = malloc(n * sizeof(double));
    double *mu = malloc(n * sizeof(double));
    double *z = malloc(n * sizeof(double));
    d[0] = 1.0; l[0] = 0.0; mu[0] = 0.0; z[0] = 0.0;
    for (int i=1; i<n-1; i++) {
        double h_im1 = h[i-1];
        double h_i = h[i];
        l[i] = h_im1 / (h_im1 + h_i);
        mu[i] = h_i / (h_im1 + h_i);
        d[i] = 2.0;
        z[i] = 3.0 * ((b[i] - b[i-1]) / (h_im1 + h_i));
    }
    d[n-1] = 1.0; l[n-1] = 0.0; mu[n-1] = 0.0; z[n-1] = 0.0;
    for (int i=1; i<n; i++) {
        double factor = l[i] / d[i-1];
        d[i] -= factor * mu[i-1];
        z[i] -= factor * z[i-1];
    }
    second_deriv[n-1] = z[n-1] / d[n-1];
    for (int i=n-2; i>=0; i--) {
        second_deriv[i] = (z[i] - mu[i] * second_deriv[i+1]) / d[i];
    }
    free(h); free(b); free(d); free(l); free(mu); free(z);
}

double interpolate_dut1_spline(double mjd) {
    if (n_eop == 0) return 0.0;
    if (mjd <= mjd_arr[0]) return dut1_arr[0];
    if (mjd >= mjd_arr[n_eop-1]) return dut1_arr[n_eop-1];
    int i = 0;
    while (i < n_eop-1 && mjd_arr[i+1] < mjd) i++;
    double h = mjd_arr[i+1] - mjd_arr[i];
    double t = (mjd - mjd_arr[i]) / h;
    double y0 = dut1_arr[i], y1 = dut1_arr[i+1];
    double s0 = second_deriv[i], s1 = second_deriv[i+1];
    return (1-t)*y0 + t*y1 + (t*t*t - t)*((1-t)*s0 + t*s1)*h*h/6.0;
}

double get_offset(void) {
    FILE *f = fopen(OFFSET_FILE, "r");
    double offset;
    if (!f) { fprintf(stderr, "offset.dat not found\n"); exit(1); }
    if (fscanf(f, "%lf", &offset) != 1) { fprintf(stderr, "invalid offset.dat\n"); exit(1); }
    fclose(f);
    return offset;
}

void format_time(double sec, char *buf, size_t size) {
    int h = (int)(sec / 3600);
    int m = (int)(fmod(sec, 3600) / 60);
    int s = (int)(fmod(sec, 60));
    snprintf(buf, size, "%02d:%02d:%02d", h, m, s);
}

void format_time_j(double sec, char *buf, size_t size) {
    int h = (int)(sec / 3600);
    int m = (int)(fmod(sec, 3600) / 60);
    int s = (int)(fmod(sec, 60));
    snprintf(buf, size, "%02d時%02d分%02d秒", h, m, s);
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

    struct timespec now_ts;
    if (clock_gettime(CLOCK_REALTIME, &now_ts) == -1) { perror("clock_gettime"); exit(1); }

    double epoch_dut1 = interpolate_dut1_spline(mjd_from_unix((time_t)EPOCH_UNIX));
    double now_dut1 = interpolate_dut1_spline(mjd_from_unix(now_ts.tv_sec));

    double epoch_start_sec = EPOCH_UNIX + epoch_dut1 + offset;
    double now_ut1_sec = now_ts.tv_sec + now_dut1 + now_ts.tv_nsec / 1e9;
    double delta_sec = now_ut1_sec - epoch_start_sec;
    int64_t delta_ns = (int64_t)(delta_sec * 1e9 + 0.5);

    if (human) {
        int64_t day_ns = 86400LL * 1000000000LL;
        int64_t days = delta_ns / day_ns;
        int64_t rem_ns = delta_ns % day_ns;
        double sec = rem_ns / 1e9;
        int64_t years = days / 147;
        int64_t rem = days % 147;
        int64_t months = rem / 21;
        int64_t days_rem = rem % 21;
        char sec_str[16];
        format_time(sec, sec_str, sizeof(sec_str));
        printf("%ldy %ldm %ldd %s\n", years, months, days_rem, sec_str);
    } else if (jap) {
        int64_t day_ns = 86400LL * 1000000000LL;
        int64_t days = delta_ns / day_ns;
        int64_t rem_ns = delta_ns % day_ns;
        double sec = rem_ns / 1e9;
        int64_t years = days / 147;
        int64_t rem = days % 147;
        int64_t months = rem / 21;
        int64_t days_rem = rem % 21;
        char sec_str[16];
        format_time_j(sec, sec_str, sizeof(sec_str));
        printf("%ld年%ld月%ld日%s\n", years, months, days_rem, sec_str);
    }

    else {
        printf("%lld\n", (long long)delta_ns);
    }

    free(mjd_arr);
    free(dut1_arr);
    free(second_deriv);
    return 0;
}
