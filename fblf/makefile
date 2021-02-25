
.PHONY: all
all: tests
	./tests

tests: tests.c heap.c
	gcc -std=c99 -pedantic -Wall -Werror -gdwarf-3 -ggdb -o $@ tests.c heap.c

