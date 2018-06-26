
exec rm -rf out
set FLAGS [list -I fblc -std=c99 -pedantic -Wall -Werror -O0 -fprofile-arcs -ftest-coverage -gdwarf-3 -ggdb] 

# Compile all object files.
# We compile these separately and ensure they are placed in a subdirectory of
# the out directory so that the profile information generated when running
# code from the objects is placed in a subdirectory of the out directory.
exec mkdir -p out/fble/obj
foreach {x} [glob fble/*.c] {
  set obj out/fble/obj/[string map {.c .o} [file tail $x]]
  run gcc {*}$FLAGS -c -o $obj $x
}

# Generate libfble.a
set fble_objs [list]
foreach {x} [list alloc vector] {
  lappend fble_objs out/fble/obj/$x.o
}
run ar rcs out/fble/libfble.a {*}$fble_objs

