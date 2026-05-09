CC      ?= gcc
CFLAGS  ?= -std=c17 -Wall -Wextra -O2
TARGET   = cachesim

all: $(TARGET)

$(TARGET): cachesim.c
	$(CC) $(CFLAGS) -o $@ $<

validate: $(TARGET)
	./$(TARGET) WAWB_validation.txt WAWB 256 16
	./$(TARGET) WAWT_validation.txt WAWT 256 16

clean:
	rm -f $(TARGET)

.PHONY: all validate clean
