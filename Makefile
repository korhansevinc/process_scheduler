# Makefile for Priority-SRTF Process Scheduler
# Operating Systems Homework 2

CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99 -pthread
TARGET = process_scheduler
SOURCES = process_scheduler.c

# Default target: build the executable
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

# Clean target: remove compiled executable
clean:
	rm -f $(TARGET)

# Phony targets (not actual files)
.PHONY: clean

