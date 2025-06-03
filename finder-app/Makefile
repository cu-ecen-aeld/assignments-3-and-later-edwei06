# Makefile for building the writer application

# Use native compiler unless CROSS_COMPILE is provided
CC ?= gcc
ifdef CROSS_COMPILE
    CC := $(CROSS_COMPILE)gcc
endif

CFLAGS = -Wall -Werror -O2

# Source and target definitions
SRC = writer.c
TARGET = writer

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET) *.o

.PHONY: all clean

