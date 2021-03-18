# Script to generate ninja/build.ninja.

# build.ninja
puts {
builddir = ninja

rule build_ninja 
  command = tclsh $in > $out

build ninja/build.ninja: build_ninja build.ninja.tcl
}

# .o files for libfble.a
set ::obj ninja/obj
puts {
cFlags = -std=c99 -pedantic -Wall -Werror -gdwarf-3 -ggdb

rule cc_libfble
  command = gcc $cFlags -I fble -c -o $out $in
}

set ::internal_headers [glob fble/*.h]
set ::obj ninja/obj
set fble_objs [list]
foreach {x} [glob fble/*.c] {
  set object $::obj/[string map {.c .o} [file tail $x]]
  lappend fble_objs $object
  # TODO: Use gcc to generate better header dependencies.
  puts "build $object: cc_libfble $x | $::internal_headers"
}

# parse.tab.c
puts {
rule bison
  command = bison --report=all --report-file=$report -o $tab_c $in
}

# TODO: Use gcc to generate better header dependencies somehow?
puts -nonewline "build ninja/src/parse.tab.c ninja/src/parse.tab.report.txt: bison fble/parse.y | $::internal_headers"
puts {
  tab_c = ninja/src/parse.tab.c
  report = ninja/src/parse.tab.report.txt
}
# TODO: Use gcc to generate better header dependencies somehow?
puts "build $::obj/parse.tab.o: cc_libfble ninja/src/parse.tab.c | $::internal_headers"
lappend fble_objs $::obj/parse.tab.o

# libfble.a
puts {
rule ar
  command = ar rcs $out $in
}
puts "build ninja/lib/libfble.a: ar $fble_objs"

# Public header files.
puts {
rule copy
  command = cp $in $out
}

set ::public_headers [list]
foreach {x} [glob fble/fble*.h] {
  lappend ::public_headers "ninja/include/[file tail $x]"
}

foreach {x} [glob fble/fble*.h] {
  puts "build ninja/include/[file tail $x]: copy $x"
}

# fble-test
puts {
rule cc_tool_obj
  command = gcc $cFlags -I ninja/include -c -o $out $in

rule cc_tool
  command = gcc $cFlags -L ninja/lib -o $out $in -lfble
}

puts "build ninja/obj/fble-test.o: cc_tool_obj tools/fble-test.c | $::public_headers"
puts "build ninja/bin/fble-test: cc_tool ninja/obj/fble-test.o | ninja/lib/libfble.a"

