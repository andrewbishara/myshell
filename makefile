CC = gcc
CFLAGS= -g -std=c99 -Wall -fsanitize=address,undefined

all: mysh

mysh: mysh.o
	$(CC) $(CFLAGS) $^ -o $@
