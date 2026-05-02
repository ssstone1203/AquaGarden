# Native build on Phytium Pi (aarch64)
CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2
TARGET := spi_scope_demo

all: $(TARGET)

$(TARGET): spi_scope_demo.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)

.PHONY: all clean
