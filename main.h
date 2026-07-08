#ifndef MAIN_H
#define MAIN_H

#include <time.h>
#include <stdint.h>

#define EPOCH_UNIX 1782086400.0
#define SECS_PER_DAY 86400.0
#define OFFSET_FILE "offset.dat"
#define FINALS_FILE "finals.all"

void load_finals(const char *filename);
void build_spline(void);
double interpolate_dut1_spline(double mjd);
double mjd_from_unix(time_t t);
double get_offset(void);
void format_time(double sec, char *buf, size_t size);

#endif
