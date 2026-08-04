#ifndef ITS_TEST_HARNESS_H
#define ITS_TEST_HARNESS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdint.h>
#include <unistd.h>
#include "common.h"

extern unsigned long g_checks;
extern unsigned long g_failed;
extern int g_groups;
extern int g_has_finals;

#define CHECK(cond) do { \
    g_checks++; \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        g_failed++; \
    } \
} while (0)

#define CHECK_MSG(cond, ...) do { \
    g_checks++; \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        fprintf(stderr, "      "); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
        g_failed++; \
    } \
} while (0)

#define CHECK_NEAR(a, b, tol) do { \
    double _a = (double)(a); \
    double _b = (double)(b); \
    g_checks++; \
    if (!(fabs(_a - _b) <= (double)(tol))) { \
        fprintf(stderr, "FAIL %s:%d: %s vs %s: %.10f vs %.10f (tol %.3g)\n", \
                __FILE__, __LINE__, #a, #b, _a, _b, (double)(tol)); \
        g_failed++; \
    } \
} while (0)

#define CHECK_BIG(a, b) do { \
    g_checks++; \
    if ((a) != (b)) { \
        fprintf(stderr, "FAIL %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b); \
        g_failed++; \
    } \
} while (0)

#define GROUP_START(name) do { g_groups++; printf("== %s\n", (name)); } while (0)
#define SKIP(note) do { printf("   SKIP: %s\n", (note)); } while (0)

void cap_start(const char *path);
const char *cap_read(const char *path);

void test_srand(unsigned long seed);
double test_rand(void);

void test_arith(void);
void test_calendar(void);
void test_format(void);
void test_spline(void);
void test_sun(void);
void test_offset(void);
void test_its_elapsed(void);

#endif
