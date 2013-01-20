CC := gcc
SRCS := spinlock_test.c
OBJS := $(SRCS:%.c=%.o)
DEBUG_FLAGS := -g
LD_FLAGS = -lpthread
CFLAGS := -Wall -O2 $(DEBUG_FLAGS) -DTEST_TRY_LOCK -DNUM_THREADS=65535 #-DNUM_THREADS=255
TARGETS := spinlock_test

all: $(TARGETS)

spinlock_test: $(OBJS)
	$(CC) $(DEBUG_FLAGS) -o $@ $^ $(LD_FLAGS)

%.o:%.c
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -f *~ $(OBJS) $(TARGETS)