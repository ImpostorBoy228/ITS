#define _GNU_SOURCE
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define LAT 55.03
#define LON 82.93
#define ZENITH 108.0
#define SECS_PER_DAY 86400.0
#define OFFSET_FILE "offset.dat"

// todo: migrate from utc to ut1 

/* -------------------- Astronomical calculations -------------------- */

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
        *sunset = -1.0; *twilight_end = -1.0; *daylen = 0.0; return;
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

/* Compute the offset (UT seconds of day when our day starts) by finding earliest twilight end */
double compute_offset(void) {
    double min_twilight = 1e9;
    int best_y = 0, best_m = 0, best_d = 0;
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
                if (twilight < min_twilight) {
                    min_twilight = twilight;
                    best_y = y; best_m = m; best_d = d;
                }
            }
        }
    }
    if (best_y == 0) return -1.0;
    double offset = min_twilight;
    if (offset < 0.0) offset += SECS_PER_DAY;
    return offset;
}

/* Load offset from file, or compute and save if file doesn't exist */
double get_offset(void) {
    FILE *f = fopen(OFFSET_FILE, "r");
    double offset;
    if (f) {
        if (fscanf(f, "%lf", &offset) == 1) {
            fclose(f);
            return offset;
        }
        fclose(f);
    }
    offset = compute_offset();
    if (offset < 0.0) {
        fprintf(stderr, "Failed to compute offset.\n");
        exit(1);
    }
    f = fopen(OFFSET_FILE, "w");
    if (f) {
        fprintf(f, "%.6f\n", offset);
        fclose(f);
    } else {
        fprintf(stderr, "Warning: could not save offset to file.\n");
    }
    return offset;
}

/* Format seconds (0..86400) into HH:MM:SS string */
void format_time(double sec, char *buf, size_t size) {
    int h = (int)(sec / 3600);
    int m = (int)(fmod(sec, 3600) / 60);
    int s = (int)(fmod(sec, 60));
    snprintf(buf, size, "%02d:%02d:%02d", h, m, s);
}

/* -------------------- Our time system -------------------- */

/* Convert UTC time_t to our time: day number since epoch and seconds of day */
void utc_to_our_time(time_t utc, double offset, long *our_day, double *our_sec) {
    double utc_sec = (double)(utc % (time_t)SECS_PER_DAY);
    double diff = utc_sec - offset;
    if (diff < 0.0) diff += SECS_PER_DAY;
    *our_sec = diff;
    double days_since_epoch = (double)utc / SECS_PER_DAY;
    double our_days = days_since_epoch - offset / SECS_PER_DAY;
    *our_day = (long)floor(our_days);
    if (*our_sec >= SECS_PER_DAY - 0.5) {
        *our_sec = 0.0;
        (*our_day)++;
    }
}

/* Get current UTC time as time_t */
time_t current_utc(void) {
    return time(NULL);
}

/* Convert our day and seconds to a printable string (UTC equivalent) */
void our_time_to_utc_string(long our_day, double our_sec, double offset, char *buf, size_t size) {
    double utc_sec = our_sec + offset;
    if (utc_sec >= SECS_PER_DAY) utc_sec -= SECS_PER_DAY;
    time_t utc = (time_t)((our_day + offset / SECS_PER_DAY) * SECS_PER_DAY + utc_sec);
    struct tm *tm = gmtime(&utc);
    strftime(buf, size, "%Y-%m-%d %H:%M:%S UTC", tm);
}


int main(void) {
    double offset = get_offset();
    char offset_str[16];
    format_time(offset, offset_str, sizeof(offset_str));
    printf("Our day starts at UT seconds: %.3f (i.e. %s UTC)\n", offset, offset_str);

    time_t now = current_utc();
    struct tm *now_tm = gmtime(&now);
    char now_str[64];
    strftime(now_str, sizeof(now_str), "%Y-%m-%d %H:%M:%S UTC", now_tm);
    printf("Current UTC: %s\n", now_str);

    long our_day;
    double our_sec;
    utc_to_our_time(now, offset, &our_day, &our_sec);
    char time_str[16];
    format_time(our_sec, time_str, sizeof(time_str));
    // todo: format our_day to years and months, where 1 year = 147 days; 1 month = 21 days
    // that is fucking genius
    printf("ITS time: Day %ld, %s since day start\n", our_day, time_str);
    time_t local_ts = now + 7 * 3600;  // UTC+7
    struct tm *local_tm = gmtime(&local_ts);
    char local_str[64];
    strftime(local_str, sizeof(local_str), "%Y-%m-%d %H:%M:%S", local_tm);
    printf("Local time (Novosibirsk): %s\n", local_str);

    /* Show when our current day started (UTC) */
    double day_start_utc_sec = offset + (our_day * SECS_PER_DAY);
    time_t day_start = (time_t)day_start_utc_sec;
    struct tm *start_tm = gmtime(&day_start);
    char start_str[64];
    strftime(start_str, sizeof(start_str), "%Y-%m-%d %H:%M:%S UTC", start_tm);
    printf("Current our day started at: %s\n", start_str);

    return 0;
}
