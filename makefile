CC=gcc
LD=gcc
CFLAGS=-Wall -O3 -flto -Idependencies/include
LDLIBS=-lavformat -lavcodec -lavutil -lswscale -lswresample -lsoundio
LDFLAGS=-static

OBJS=entry.o fontvideo.o avdec.o siowrap.o

all: fontvideo

fontvideo: $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) $(LDLIBS) -o $@

clean:
	rm -f fontvideo
