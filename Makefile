CC      ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra -std=c99
TARGET   = cache_sim
SRCS     = cache_sim.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

run: $(TARGET)
	./$(TARGET) sample_trace.txt

clean:
	rm -f $(TARGET)
