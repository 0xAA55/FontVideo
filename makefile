CC=gcc
LD=gcc
CFLAGS=-Wall -O3 -flto -I. -Iinclude
LDLIBS=-lavcodec -lavutil -lswresample -lm -lsoundio -pthread
LDFLAGS=-static -O3

OBJS=entry.o fontvideo.o avdec.o siowrap.o

all: fontvideo

fontvideo: $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) $(LDLIBS) -o $@

clean:
	rm -f fontvideo $(OBJS)
