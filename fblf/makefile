
.PHONY: all
all: tests md5
	./tests
	./md5 /dev/null

md5: md5.c heap.c
	gcc -O3 -std=c99 -pedantic -Wall -Werror -gdwarf-3 -ggdb -o $@ md5.c heap.c

tests: tests.c heap.c
	gcc -std=c99 -pedantic -Wall -Werror -gdwarf-3 -ggdb -o $@ tests.c heap.c

