#include "test_harness.h"

static void check_hms(double sec, const char *buf) {
    int h = (int)(sec / 3600);
    int m = (int)(fmod(sec, 3600) / 60);
    int s = (int)(fmod(sec, 60));
    CHECK_MSG(buf[0] == (char)('0' + h / 10) && buf[1] == (char)('0' + h % 10) &&
              buf[3] == (char)('0' + m / 10) && buf[4] == (char)('0' + m % 10) &&
              buf[6] == (char)('0' + s / 10) && buf[7] == (char)('0' + s % 10),
              "format(%g) got [%s]", sec, buf);
}

static void check_hms_j(double sec, const char *buf) {
    int h = (int)(sec / 3600);
    int m = (int)(fmod(sec, 3600) / 60);
    int s = (int)(fmod(sec, 60));
    CHECK_MSG(buf[0] == (char)('0' + h / 10) && buf[1] == (char)('0' + h % 10) &&
              buf[5] == (char)('0' + m / 10) && buf[6] == (char)('0' + m % 10) &&
              buf[10] == (char)('0' + s / 10) && buf[11] == (char)('0' + s % 10),
              "format_j(%g) got [%s]", sec, buf);
}

void test_format(void) {
    GROUP_START("format_time golden");
    char buf[64];
    format_time(44193.737495, buf, sizeof(buf));
    CHECK(strcmp(buf, "12:16:33") == 0);
    format_time(0.0, buf, sizeof(buf));
    CHECK(strcmp(buf, "00:00:00") == 0);
    format_time(3661.0, buf, sizeof(buf));
    CHECK(strcmp(buf, "01:01:01") == 0);
    format_time(86399.0, buf, sizeof(buf));
    CHECK(strcmp(buf, "23:59:59") == 0);
    format_time(86400.0, buf, sizeof(buf));
    CHECK(strcmp(buf, "24:00:00") == 0);

    GROUP_START("format_time property (100 values)");
    for (int i = 0; i < 100; i++) {
        double sec = (double)(i * 1237) + 0.7;
        char b2[64];
        format_time(sec, b2, sizeof(b2));
        CHECK_MSG(strlen(b2) == 8, "format_time(%g) len=%zu", sec, strlen(b2));
        check_hms(sec, b2);
    }

    GROUP_START("format_time_j");
    format_time_j(44193.737495, buf, sizeof(buf));
    CHECK(strcmp(buf, "12時16分33秒") == 0);
    CHECK_MSG(strlen(buf) == 15,
              "UTF-8 byte length of format_time_j output is 15, got %zu", strlen(buf));
    for (int i = 0; i < 50; i++) {
        double sec = (double)(i * 7331) + 0.3;
        char bj[64];
        format_time_j(sec, bj, sizeof(bj));
        CHECK_MSG(strlen(bj) == 15, "format_time_j(%g) len=%zu", sec, strlen(bj));
        check_hms_j(sec, bj);
    }
}
