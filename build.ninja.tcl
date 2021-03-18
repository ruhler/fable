# ninja-based description of how to build fble and friends.
#
# First time setup:
#   mkdir -p ninja
#   tclsh build.ninja.tcl > build.ninja
#
# Afterwards:
#   ninja -f ninja/build.ninja

# Store ninja output files in ninja/ directory.
set builddir "ninja"
puts "builddir = $builddir"

puts {
rule build_ninja 
  command = tclsh $in > $out

cFlags = -std=c99 -pedantic -Wall -Werror -gdwarf-3 -ggdb

rule obj
  command = gcc $cFlags $iflags -c -o $out $in
  
rule parser
  command = bison --report=all --report-file=$report -o $tab_c $in

rule lib
  command = ar rcs $out $in

rule exe
  command = gcc $cFlags $lflags -o $out $in $libs

rule copy
  command = cp $in $out
}

puts "build ninja/build.ninja: build_ninja build.ninja.tcl"

set internal_headers [glob fble/*.h]
set obj ninja/obj
set fble_objs [list]
foreach {x} [glob fble/*.c] {
  set object $obj/[string map {.c .o} [file tail $x]]
  lappend fble_objs $object
  # TODO: Use gcc to generate better header dependencies.
  puts "build $object: obj $x | $internal_headers"
  puts "  iflags = -I fble"
}

# TODO: Use gcc to generate better header dependencies somehow?
puts "build ninja/src/parse.tab.c ninja/src/parse.tab.report.txt: parser fble/parse.y | $internal_headers"
puts "  tab_c = ninja/src/parse.tab.c"
puts "  report = ninja/src/parse.tab.report.txt"

# TODO: Use gcc to generate better header dependencies somehow?
puts "build $obj/parse.tab.o: obj ninja/src/parse.tab.c | $internal_headers"
puts "  iflags = -I fble"
lappend fble_objs $obj/parse.tab.o

puts "build ninja/lib/libfble: lib $fble_objs"

set public_headers [list]
foreach {x} [glob fble/fble*.h] {
  lappend public_headers "ninja/include/[file tail $x]"
  puts "build ninja/include/[file tail $x]: copy $x"
}

foreach {x} [glob tools/*.c prgms/fble-md5.c prgms/fble-stdio.c] {
  set base [file rootname [file tail $x]]
  puts "build ninja/obj/$base.o: obj $x | $public_headers"
  puts "  iflags = -I ninja/include"
  puts "build ninja/bin/$base: exe ninja/obj/$base.o | ninja/lib/libfble"
  puts "  lflags = -L ninja/lib"
  puts "  libs = -lfble"
}

puts "build ninja/obj/fble-app.o: obj prgms/fble-app.c | $public_headers"
puts "  iflags = -I ninja/include -I /usr/include/SDL2"
puts "build ninja/bin/fble-app: exe ninja/obj/fble-app.o | ninja/lib/libfble"
puts "  lflags = -L ninja/lib"
puts "  libs = -lfble -lSDL2"

