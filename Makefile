CC      = gcc
CFLAGS  = -std=c11 -Iinclude -Wall -Wextra -pthread
LDFLAGS = -pthread

SRCS = src/main.c src/pool.c src/worker.c src/jobqueue.c
OBJS = $(SRCS:.c=.o)
TARGET = poolsched

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c include/poolsched.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
