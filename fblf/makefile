
.PHONY: all
all: bits_test demo
	./bits_test
	./demo

bits_test: bits_test.c bits.c bits.h 
	gcc -std=c99 -pedantic -Wall -Werror -gdwarf-3 -ggdb -o $@ bits_test.c bits.c

demo: demo.c
	gcc -std=c99 -pedantic -Wall -Werror -gdwarf-3 -ggdb -o $@ demo.c
