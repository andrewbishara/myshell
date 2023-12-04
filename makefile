CC = gcc
CFLAGS= -g -std=c99 -Wall -fsanitize=address,undefined

all: mysh testwrite testread

mysh: mysh.o
	$(CC) $(CFLAGS) $^ -o $@

testwrite: teswrite.o
	$(CC) $(CFLAGS) $^ -o $@

testread: testread.o
	$(CC) $(CFLAGS) $^ -o $@
