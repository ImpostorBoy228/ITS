CC = cc
CFLAGS = -O2 -Wall
LDLIBS = -lm -lgmp
CURLLIBS = -lcurl

all: its itstime its-offset itsd

common.o: common.c common.h
	$(CC) $(CFLAGS) -c common.c -o common.o

its: its.c common.o common.h
	$(CC) $(CFLAGS) its.c common.o -o its $(LDLIBS)

itstime: itstime.c common.o common.h
	$(CC) $(CFLAGS) itstime.c common.o -o itstime $(LDLIBS)

its-offset: offset.c common.o common.h
	$(CC) $(CFLAGS) offset.c common.o -o its-offset $(LDLIBS)

itsd: itsd.c common.o common.h
	$(CC) $(CFLAGS) itsd.c common.o -o itsd $(LDLIBS) $(CURLLIBS)

clean:
	rm -f its itstime its-offset itsd common.o
