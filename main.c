#define _GNU_SOURCE
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <gmp.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define LAT 55.03
#define LON 82.93 // Novosibirsk
#define ZENITH 108.0
#define SECS_PER_DAY 86400.0
#define OFFSET_FILE "offset.dat"
#define FINALS_FILE "finals.all"
#define EPOCH_UNIX 1782086400.0   // 2026-06-22 00:00:00 UTC

// astronomical helpers
double jdn(int y, int m, int d) {
    if (m <= 2) { y--; m += 12; }
    int A = y / 100;
    int B = 2 - A + A / 4;
    return floor(365.25 * (y + 4716)) + floor(30.6001 * (m + 1)) + d + B - 1524.5;
}

void sun_position(double jd, double *decl, double *eq_time) {
    double T = (jd - 2451545.0) / 36525.0;
    double L0 = fmod(280.46646 + 36000.76983 * T + 0.0003032 * T * T, 360.0);
    double M = fmod(357.52911 + 35999.05029 * T - 0.0001537 * T * T, 360.0);
    double C = (1.914602 - 0.004817 * T - 0.000014 * T * T) * sin(M * M_PI / 180.0)
             + (0.019993 - 0.000101 * T) * sin(2 * M * M_PI / 180.0)
             + 0.000289 * sin(3 * M * M_PI / 180.0);
    double sun_lon = L0 + C;
    double obliq = 23.439291 - 0.0130042 * T;
    double alpha = atan2(cos(obliq * M_PI / 180.0) * sin(sun_lon * M_PI / 180.0),
                         cos(sun_lon * M_PI / 180.0)) * 180.0 / M_PI;
    alpha = fmod(alpha + 360.0, 360.0);
    double delta = asin(sin(obliq * M_PI / 180.0) * sin(sun_lon * M_PI / 180.0)) * 180.0 / M_PI;
    *decl = delta;
    double E = L0 - alpha;
    if (E < -180.0) E += 360.0;
    if (E > 180.0) E -= 360.0;
    *eq_time = E * 4.0;
}

double hour_angle(double lat, double decl, double zenith, int sign) {
    double cos_ha = (cos(zenith * M_PI / 180.0) - sin(lat * M_PI / 180.0) * sin(decl * M_PI / 180.0)) /
                    (cos(lat * M_PI / 180.0) * cos(decl * M_PI / 180.0));
    if (cos_ha < -1.0 || cos_ha > 1.0) return 0.0;
    double ha = acos(cos_ha) * 180.0 / M_PI / 15.0;
    return sign * ha;
}

void compute_times(int y, int m, int d, double *sunset, double *twilight_end, double *daylen, int *has_night) {
    double jd = jdn(y, m, d) - 0.5;
    double decl, eq_time;
    sun_position(jd, &decl, &eq_time);
    double noon = 12.0 - LON / 15.0 - eq_time / 60.0;
    double ha_sunset = hour_angle(LAT, decl, 90.833, 1);
    double ha_twilight = hour_angle(LAT, decl, ZENITH, 1);
    *has_night = (ha_twilight != 0.0);
    if (!*has_night) {
        *sunset = -1.0; *twilight_end = -1.0; *daylen = 0.0;
        return;
    }
    double set = noon + ha_sunset;
    double twi = noon + ha_twilight;
    if (set < 0.0) set += 24.0;
    if (twi < 0.0) twi += 24.0;
    if (set >= 24.0) set -= 24.0;
    if (twi >= 24.0) twi -= 24.0;
    *sunset = set * 3600.0;
    *twilight_end = twi * 3600.0;
    *daylen = 2.0 * ha_sunset;
}

// offset computation (UT1‑based)
double compute_offset(void) {
    double min_twilight = 1e9;
    for (int y = 1976; y <= 2026; y++) {
        for (int m = 1; m <= 12; m++) {
            int dim;
            if (m == 2) dim = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 29 : 28;
            else if (m == 4 || m == 6 || m == 9 || m == 11) dim = 30;
            else dim = 31;
            for (int d = 1; d <= dim; d++) {
                double sunset, twilight, daylen;
                int has_night;
                compute_times(y, m, d, &sunset, &twilight, &daylen, &has_night);
                if (!has_night) continue;
                if (twilight < min_twilight) min_twilight = twilight;
            }
        }
    }
    return (min_twilight < 1e9) ? min_twilight : -1.0;
}

double get_offset(void) {
    FILE *f = fopen(OFFSET_FILE, "r");
    double offset;
    if (f && fscanf(f, "%lf", &offset) == 1) { fclose(f); return offset; }
    if (f) fclose(f);
    offset = compute_offset();
    if (offset < 0.0) { fprintf(stderr, "Offset computation fuckup\n"); exit(1); }
    f = fopen(OFFSET_FILE, "w");
    if (f) { fprintf(f, "%.6f\n", offset); fclose(f); }
    return offset;
}

// UT1 from finals.all with cubic spline interpolation
static double *mjd_arr = NULL;
static double *dut1_arr = NULL;
static int n_eop = 0;
static double *second_deriv = NULL;   // for cubic spline

double mjd_from_unix(time_t t) {
    return (double)t / SECS_PER_DAY + 40587.0;   // 1970-01-01 = MJD 40587
}

void load_finals(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) { perror("finals.all"); exit(1); }
    char line[256];
    // first pass: count valid lines
    while (fgets(line, sizeof(line), f)) {
        if (strlen(line) < 68) continue;
        if (strncmp(line, "MJD", 3) == 0) continue;
        n_eop++;
    }
    rewind(f);
    mjd_arr = malloc(n_eop * sizeof(double));
    dut1_arr = malloc(n_eop * sizeof(double));
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

/* Build natural cubic spline coefficients (second derivatives) */
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
    // Tridiagonal system: d[i]*s''[i] + l[i]*s''[i-1] + mu[i]*s''[i+1] = z[i]
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
    // Forward sweep
    for (int i=1; i<n; i++) {
        double factor = l[i] / d[i-1];
        d[i] -= factor * mu[i-1];
        z[i] -= factor * z[i-1];
    }
    // Back substitution
    second_deriv[n-1] = z[n-1] / d[n-1];
    for (int i=n-2; i>=0; i--) {
        second_deriv[i] = (z[i] - mu[i] * second_deriv[i+1]) / d[i];
    }
    free(h); free(b); free(d); free(l); free(mu); free(z);
}

/* Interpolate dUT1 at given MJD using cubic spline */
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
    // Cubic spline formula
    return (1-t)*y0 + t*y1 + (t*t*t - t)*((1-t)*s0 + t*s1)*h*h/6.0;
}

// formatting helper
void format_time(double sec, char *buf, size_t size) {
    int h = (int)(sec / 3600);
    int m = (int)(fmod(sec, 3600) / 60);
    int s = (int)(fmod(sec, 60));
    snprintf(buf, size, "%02d:%02d:%02d", h, m, s);
}

/* Convert a double representing seconds to integer nanoseconds (mpz_t) */
static void double_to_ns(double val, mpz_t out) {
    /* Multiply by 1e9 and round to nearest integer */
    mpq_t q;
    mpq_init(q);
    mpq_set_d(q, val);
    mpq_mul_ui(q, q, 1000000000ULL);
    mpq_round(q, q);        /* round to nearest integer */
    mpz_set_q(out, q);
    mpq_clear(q);
}

/* Convert struct timespec to mpz_t nanoseconds since epoch */
static void timespec_to_ns(const struct timespec *ts, mpz_t out) {
    mpz_set_si(out, ts->tv_sec);
    mpz_mul_ui(out, out, 1000000000ULL);
    mpz_add_ui(out, out, ts->tv_nsec);
}

// ----
int main(void) {
    load_finals(FINALS_FILE);
    build_spline();

    double offset = get_offset();
    char off_str[16];
    format_time(offset, off_str, sizeof(off_str));
    printf("Day start offset: %.6f UT1 sec (%s)\n", offset, off_str);

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

    // dUT1 for epoch and now (spline gives double)
    double epoch_dut1 = interpolate_dut1_spline(mjd_from_unix(epoch_ts.tv_sec));
    double now_dut1 = interpolate_dut1_spline(mjd_from_unix(now_ts.tv_sec));

    // oh fuck we are going to have big shit
    mpz_t epoch_ns, now_ns;
    mpz_t epoch_dut1_ns, now_dut1_ns;
    mpz_t offset_ns;
    mpz_t epoch_start_ns;   /* start of day 0 in UT1 */
    mpz_t delta_ns;         /* nanoseconds from epoch start */
    mpz_t divisor;
    mpz_t day_mpz, sec_ns_mpz;

    mpz_init(epoch_ns);
    mpz_init(now_ns);
    mpz_init(epoch_dut1_ns);
    mpz_init(now_dut1_ns);
    mpz_init(offset_ns);
    mpz_init(epoch_start_ns);
    mpz_init(delta_ns);
    mpz_init_set_ui(divisor, 86400000000000ULL);  // 86400 * 1e9
    mpz_init(day_mpz);
    mpz_init(sec_ns_mpz);

    timespec_to_ns(&epoch_ts, epoch_ns);
    timespec_to_ns(&now_ts, now_ns);

    double_to_ns(epoch_dut1, epoch_dut1_ns);
    double_to_ns(now_dut1, now_dut1_ns);
    double_to_ns(offset, offset_ns);

    /* epoch_start_ns = epoch_ns + epoch_dut1_ns + offset_ns */
    mpz_add(epoch_start_ns, epoch_ns, epoch_dut1_ns);
    mpz_add(epoch_start_ns, epoch_start_ns, offset_ns);

    /* delta_ns = now_ns + now_dut1_ns - epoch_start_ns */
    mpz_add(delta_ns, now_ns, now_dut1_ns);
    mpz_sub(delta_ns, delta_ns, epoch_start_ns);

    // Print total nanoseconds as a signed integer
    gmp_printf("Nanoseconds since epoch start: %Zd\n", delta_ns);

    /* Use GMP division to get days and remaining nanoseconds */
    mpz_fdiv_q(day_mpz, delta_ns, divisor);
    mpz_fdiv_r(sec_ns_mpz, delta_ns, divisor);

    long day = mpz_get_si(day_mpz);
    double sec = mpz_get_d(sec_ns_mpz) / 1e9;   // seconds within the day

    long years = day / 147;
    long rem = day % 147;
    long months = rem / 21;
    rem %= 21;
    long days_rem = rem;

    char sec_str[16];
    format_time(sec, sec_str, sizeof(sec_str));
    printf("ITS time: %ldy %ldm %ldd %s\n", years, months, days_rem, sec_str);

    // Show UTC and UT1
    char utc_buf[32], ut1_buf[32];
    strftime(utc_buf, sizeof(utc_buf), "%Y-%m-%d %H:%M:%S", gmtime(&now_ts.tv_sec));
    /* UT1 second = (now_ns + now_dut1_ns) / 1e9 */
    mpz_t ut1_ns;
    mpz_init(ut1_ns);
    mpz_add(ut1_ns, now_ns, now_dut1_ns);
    time_t ut1_sec = mpz_get_si(ut1_ns) / 1000000000LL;
    strftime(ut1_buf, sizeof(ut1_buf), "%Y-%m-%d %H:%M:%S", gmtime(&ut1_sec));
    printf("UTC: %s\nUT1: %s\n", utc_buf, ut1_buf); // actually same thing
    mpz_clear(ut1_ns);

    // clear all GMP variables
    mpz_clear(epoch_ns);
    mpz_clear(now_ns);
    mpz_clear(epoch_dut1_ns);
    mpz_clear(now_dut1_ns);
    mpz_clear(offset_ns);
    mpz_clear(epoch_start_ns);
    mpz_clear(delta_ns);
    mpz_clear(divisor);
    mpz_clear(day_mpz);
    mpz_clear(sec_ns_mpz);

    free(mjd_arr);
    free(dut1_arr);
    free(second_deriv);

    return 0;
}
