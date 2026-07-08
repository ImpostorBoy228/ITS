#define _GNU_SOURCE
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <gmp.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define LAT 55.03
#define LON 82.93
#define ZENITH 108.0
#define SECS_PER_DAY 86400.0
#define OFFSET_FILE "offset.dat"
#define FINALS_FILE "finals.all"
#define EPOCH_UNIX 1782086400.0   // 2026-06-22 00:00:00 UTC
				  
typedef struct {
    uint64_t lo;
    uint64_t hi; 
} uint128_t;


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
    if (offset < 0.0) { fprintf(stderr, "Offset computation failed\n"); exit(1); }
    f = fopen(OFFSET_FILE, "w");
    if (f) { fprintf(f, "%.6f\n", offset); fclose(f); }
    return offset;
}

/* ---------- UT1 from finals.all ---------- */
static double *mjd_arr = NULL;
static double *dut1_arr = NULL;
static int n_eop = 0;

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
        if (strncmp(line, "MJD", 3) == 0) continue;   // skip header
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
        // ignore lines with invalid data (e.g., all blanks)
        if (mjd > 0 && dut1 > -10 && dut1 < 10) {
            mjd_arr[i] = mjd;
            dut1_arr[i] = dut1;
            i++;
        }
    }
    n_eop = i;
    fclose(f);
}

double interpolate_dut1(double mjd) {
    if (n_eop == 0) return 0.0;
    if (mjd <= mjd_arr[0]) return dut1_arr[0];
    if (mjd >= mjd_arr[n_eop-1]) return dut1_arr[n_eop-1];
    int lo = 0, hi = n_eop - 1;
    while (hi - lo > 1) {
        int mid = (lo + hi) / 2;
        if (mjd_arr[mid] < mjd) lo = mid;
        else hi = mid;
    }
    double t = (mjd - mjd_arr[lo]) / (mjd_arr[hi] - mjd_arr[lo]);
    return dut1_arr[lo] + t * (dut1_arr[hi] - dut1_arr[lo]);
}

// formatting 
void format_time(double sec, char *buf, size_t size) {
    int h = (int)(sec / 3600);
    int m = (int)(fmod(sec, 3600) / 60);
    int s = (int)(fmod(sec, 60));
    snprintf(buf, size, "%02d:%02d:%02d", h, m, s);
}

// MAIN
int main(void) {
    load_finals(FINALS_FILE);

    double offset = get_offset();
    char off_str[16];
    format_time(offset, off_str, sizeof(off_str));
    printf("Day start offset: %.3f UT1 sec (%s)\n", offset, off_str);

    // epoch in UT1: 2026-06-22 00:00:00 UTC + dut1(epoch) + offset
    double epoch_utc = EPOCH_UNIX;
    double epoch_dut1 = interpolate_dut1(mjd_from_unix((time_t)epoch_utc));
    double epoch_ut1 = epoch_utc + epoch_dut1;
    double epoch_start_ut1 = epoch_ut1 + offset;   // start of day 0

    // current UT1
    time_t now_utc = time(NULL);
    double now_dut1 = interpolate_dut1(mjd_from_unix(now_utc));
    double now_ut1 = (double)now_utc + now_dut1;

    // ITS time
    double diff = now_ut1 - epoch_start_ut1;
    long day = (long)floor(diff / SECS_PER_DAY);
    double sec = diff - day * SECS_PER_DAY;

    long years = day / 147;
    long rem = day % 147;
    long months = rem / 21;
    rem %= 21;
    long weeks = rem / 7;
    long days_rem = rem % 7;

    if (sec < 0) { sec += SECS_PER_DAY; day--; }

    char sec_str[16];
    format_time(sec, sec_str, sizeof(sec_str));
    printf("ITS time: %ldy %ldm %ldw %ldd %s\n", years, months, weeks, days_rem, sec_str);

    // optional: show UTC and UT1 for reference
    char utc_buf[32], ut1_buf[32];
    strftime(utc_buf, sizeof(utc_buf), "%Y-%m-%d %H:%M:%S", gmtime(&now_utc));
    time_t ut1_ts = (time_t)now_ut1;
    strftime(ut1_buf, sizeof(ut1_buf), "%Y-%m-%d %H:%M:%S", gmtime(&ut1_ts));
    printf("UTC: %s\nUT1: %s\n", utc_buf, ut1_buf);
    
    mpz_clear(nanoseconds);
    return 0;
}
