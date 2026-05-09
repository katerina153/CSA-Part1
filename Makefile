CC      ?= gcc
CFLAGS  ?= -std=c17 -Wall -Wextra -O2
TARGET   = cache_sim

all: $(TARGET)

$(TARGET): cache_sim.c
	$(CC) $(CFLAGS) -o $@ $<

run: $(TARGET)
	./$(TARGET) trace_file.txt 256 16

clean:
	rm -f $(TARGET)

.PHONY: all run clean
