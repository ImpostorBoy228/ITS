#include "test_harness.h"

void test_arith(void) {
    GROUP_START("fdivmod brute force (a in [-500,500], b in [-50,50])");
    long pairs = 0;
    for (long a = -500; a <= 500; a++) {
        for (long b = -50; b <= 50; b++) {
            if (b == 0) continue;
            bigint q, r;
            q = fdivmod((bigint)a, (bigint)b, &r);
            CHECK_MSG(q * b + r == (bigint)a, "a=%ld b=%ld", a, b);
            if (b > 0)
                CHECK_MSG(r == 0 || (r > 0 && r < (bigint)b),
                          "floor rem (b>0) a=%ld b=%ld", a, b);
            else
                CHECK_MSG(r == 0 || (r > (bigint)b && r < 0),
                          "floor rem (b<0) a=%ld b=%ld", a, b);
            pairs++;
        }
    }
    printf("   %ld pairs exercised\n", pairs);

    GROUP_START("fdivmod hand-picked");
    {
        bigint q, r;
        q = fdivmod(7, 3, &r);
        CHECK(q == 2 && r == 1);
        q = fdivmod(-7, 3, &r);
        CHECK(q == -3 && r == 2);
        q = fdivmod(7, -3, &r);
        CHECK(q == -3 && r == -2);
        q = fdivmod(-7, -3, &r);
        CHECK(q == 2 && r == -1);
        q = fdivmod(9, 3, &r);
        CHECK(q == 3 && r == 0);
    }

    GROUP_START("double_to_ns");
    CHECK_BIG(double_to_ns(1.0), (bigint)1000000000);
    CHECK_BIG(double_to_ns(0.5), (bigint)500000000);
    CHECK_BIG(double_to_ns(-1.5), (bigint)-1500000000);
    CHECK_BIG(double_to_ns(0.0), (bigint)0);
    CHECK_BIG(double_to_ns(1.5), (bigint)1500000000);
    CHECK_BIG(double_to_ns(2.0), (bigint)2000000000);
    CHECK_BIG(double_to_ns(0.1), (bigint)100000000);
    CHECK_BIG(double_to_ns(-0.1), (bigint)-100000000);

    GROUP_START("timespec_to_ns");
    CHECK_BIG(timespec_to_ns(&(struct timespec){0, 0}), (bigint)0);
    CHECK_BIG(timespec_to_ns(&(struct timespec){1782086400, 0}),
              (bigint)1782086400000000000);
    CHECK_BIG(timespec_to_ns(&(struct timespec){1, 5}), (bigint)1000000005);
    CHECK_BIG(timespec_to_ns(&(struct timespec){-1, 0}), (bigint)-1000000000);
    CHECK_BIG(timespec_to_ns(&(struct timespec){-1, 500}), (bigint)-999999500);
    CHECK_BIG(timespec_to_ns(&(struct timespec){0, 999999999}),
              (bigint)999999999);

    GROUP_START("print_bigint (stdout capture)");
    {
        const char *path = "/tmp/its_pb_out.txt";
        const struct { long long val; const char *want; } pb[] = {
            {0LL, "0"},
            {123456789LL, "123456789"},
            {1782086400000000000LL, "1782086400000000000"},
            {-9876543210LL, "-9876543210"},
        };
        for (size_t i = 0; i < sizeof(pb) / sizeof(pb[0]); i++) {
            cap_start(path);
            print_bigint((bigint)pb[i].val);
            const char *got = cap_read(path);
            CHECK_MSG(strcmp(got, pb[i].want) == 0,
                      "print_bigint(%lld) got [%s]", pb[i].val, got);
        }
    }
}
