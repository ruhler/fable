
exec rm -rf out
set FLAGS [list -I fble -std=c99 -pedantic -Wall -Werror -O0 -fprofile-arcs -ftest-coverage -gdwarf-3 -ggdb] 

# Compile all object files.
# We compile these separately and ensure they are placed in a subdirectory of
# the out directory so that the profile information generated when running
# code from the objects is placed in a subdirectory of the out directory.
exec mkdir -p out/fble/obj
foreach {x} [glob fble/*.c] {
  set obj out/fble/obj/[string map {.c .o} [file tail $x]]
  run gcc {*}$FLAGS -c -o $obj $x
}

# Generate and compile the fble parser.
exec mkdir -p out/fble/src
run bison --report=all --report-file=out/fble/src/parse.tab.report.txt -o out/fble/src/parse.tab.c fble/parse.y 
run gcc {*}$FLAGS -c -o out/fble/obj/parse.tab.o out/fble/src/parse.tab.c

# Generate libfble.a
set fble_objs [list]
foreach {x} [list alloc loc parse.tab vector] {
  lappend fble_objs out/fble/obj/$x.o
}
run ar rcs out/fble/libfble.a {*}$fble_objs

# Compile the executables
set ::fbletest ./out/fble/fble-test
run gcc {*}$FLAGS -o $::fbletest out/fble/obj/fble-test.o -L out/fble -lfble

test exec $::fbletest prgms/snake.fble
