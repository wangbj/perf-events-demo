CC	 = clang
CFLAGS	 = -g -O2 -Wall -D_GNU_SOURCE -D_POSIX_C_SOURCE=20190206
LDFLAGS	 = 

TESTS	:= perf-event-can-receive-signal
TARGET	:= swct-sp swct-mp swct-mp2

all: $(TARGET) $(TESTS)

.c.o:
	$(CC) $< -c -o $@ $(CFLAGS)

swct-sp: swct-sp.o
	$(CC) $^ -o $@

swct-mp: swct-mp.o
	$(CC) $^ -o $@

swct-mp2: swct-mp2.o
	$(CC) $^ -o $@

perf-event-can-receive-signal: perf-event-can-receive-signal.o
	$(CC) $^ -o $@

clean:
	$(RM) *.o $(TARGET) $(TESTS)

.PHONY: all clean tests

tests: $(TESTS)
	@./perf-event-can-receive-signal > /dev/null 2>&1 || if [ $$? -ne 159 ]; then echo "perf-event-can-receive-signal failed" && exit 1; fi
