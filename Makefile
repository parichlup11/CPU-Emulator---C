CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -pedantic -O2 -D_POSIX_C_SOURCE=200809L

all: cpu compiler

cpu: main.c cpu.c cpu.h
	$(CC) $(CFLAGS) -o cpu main.c cpu.c

compiler: compiler.c
	$(CC) $(CFLAGS) -o compiler compiler.c

clean:
	rm -f cpu compiler *.o *.bin

.PHONY: all clean