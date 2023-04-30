CC=gcc
LD=gcc
CFLAGS=-std=c11 -Wall -O3 -flto -mavx -fopenmp -I. -Iinclude
LDLIBS=-lavformat -lavcodec -lavutil -lswresample -lswscale -lsoundio -lm -Lrttimer -lrttimer -LC_dict -lcdict -fopenmp
LDFLAGS=-O3 -Llib

OBJS=entry.o fontvideo.o avdec.o siowrap.o unibmp.o utf.o bunchalloc.o aviwriter.o

all: fontvideo

fontvideo: $(OBJS) rttimer/librttimer.a C_dict/libcdict.a
	$(LD) $(LDFLAGS) $(OBJS) $(LDLIBS) -o $@

rttimer/librttimer.a:
	$(MAKE) -C rttimer

C_dict/libcdict.a:
	$(MAKE) -C C_dict

clean:
	$(MAKE) -C rttimer clean
	$(MAKE) -C C_dict clean
	rm -f fontvideo *.o
