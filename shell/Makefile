CC = gcc
CFLAGS = -std=c99 \
	-D_POSIX_C_SOURCE=200809L \
	-D_XOPEN_SOURCE=700 \
	-Wall -Wextra -Werror \
	-Wno-unused-parameter \
	-fno-asm

SRC_DIR = src
BUILD_DIR = .
TARGET = $(BUILD_DIR)/shell.out

SRCS = $(wildcard $(SRC_DIR)/*.c)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGET)