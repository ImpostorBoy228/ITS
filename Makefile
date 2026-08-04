CC = cc
CFLAGS = -O2 -Wall -std=c2x -D_DEFAULT_SOURCE
LDLIBS = -lm
CURLLIBS = -lcurl

all: itstime its-offset itsd

common.o: common.c common.h
	$(CC) $(CFLAGS) -c common.c -o common.o

itstime: itstime.c common.o common.h
	$(CC) $(CFLAGS) itstime.c common.o -o itstime -lm

its-offset: offset.c common.o common.h
	$(CC) $(CFLAGS) offset.c common.o -o its-offset $(LDLIBS)

itsd: itsd.c common.o common.h
	$(CC) $(CFLAGS) itsd.c common.o -o itsd $(LDLIBS) $(CURLLIBS)

clean:
	rm -f its itstime its-offset itsd common.o

TESTS_DIR := tests(vibecoded)

.PHONY: tests test

tests: common.o
	$(CC) $(CFLAGS) -I. '$(TESTS_DIR)'/*.c common.o -o tests $(LDLIBS)
	./tests

test: tests
