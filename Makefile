CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -pthread
LDFLAGS = -pthread

SRCS = main.c pool.c worker.c jobqueue.c
OBJS = $(SRCS:.c=.o)
TARGET = poolsched

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c poolsched.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
