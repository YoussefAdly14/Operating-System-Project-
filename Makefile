CC      = gcc
CFLAGS  = -Wall -Wextra -std=c99 -g
TARGET  = os_simulator
SRCS    = main.c queue.c memory.c process.c mutex.c interpreter.c scheduler.c
OBJS    = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) *.txt disk_*.txt

run: all
	./$(TARGET)

.PHONY: all clean run
