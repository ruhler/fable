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
  description = $out
  command = tclsh $in > $out

cFlags = -std=c99 -pedantic -Wall -Werror -gdwarf-3 -ggdb

rule obj
  description = $out
  depfile = $out.d
  command = gcc -MMD -MF $out.d $cFlags $iflags -c -o $out $in
  
rule parser
  description = $tab_c
  command = bison --report=all --report-file=$report -o $tab_c $in

rule lib
  description = $out
  command = ar rcs $out $in

rule exe
  description = $out
  depfile = $out.d
  command = gcc -MMD -MF $cFlags $lflags -o $out $in $libs

rule copy
  description = $out
  command = cp $in $out
}

puts "build ninja/build.ninja: build_ninja build.ninja.tcl"

# .c files used to implement libfble.
set libfblesrcs {
  alloc.c compile.c eval.c generate_c.c instr.c load.c heap.c
  profile.c syntax.c type.c typecheck.c value.c vector.c
}

set obj ninja/obj
set fble_objs [list]
foreach {x} $libfblesrcs {
  set object $obj/[string map {.c .o} $x]
  lappend fble_objs $object
  puts "build $object: obj fble/$x"
  puts "  iflags = -I fble"
}

# For any changes to how parser_includes works, please update the comment
# above the local includes in fble/parse.y.
set parser_includes "fble/fble.h fble/syntax.h"
puts "build ninja/src/parse.tab.c ninja/src/parse.tab.report.txt: parser fble/parse.y | $parser_includes"
puts "  tab_c = ninja/src/parse.tab.c"
puts "  report = ninja/src/parse.tab.report.txt"

puts "build $obj/parse.tab.o: obj ninja/src/parse.tab.c"
puts "  iflags = -I fble"
lappend fble_objs $obj/parse.tab.o

puts "build ninja/lib/libfble.a: lib $fble_objs"

# public header files for libfble.
set libfblehdrs {
  fble.h fble-alloc.h fble-name.h fble-profile.h fble-value.h fble-vector.h
}
foreach {x} $libfblehdrs {
  puts "build ninja/include/$x: copy fble/$x"
}

set prgms {
  tools/fble-disassemble.c tools/fble-mem-test.c tools/fble-compile.c
  tools/fble-profile-test.c tools/fble-test.c
  prgms/fble-md5.c prgms/fble-stdio.c
}

foreach {x} $prgms {
  set base [file rootname [file tail $x]]
  puts "build ninja/obj/$base.o: obj $x"
  puts "  iflags = -I ninja/include"
  puts "build ninja/bin/$base: exe ninja/obj/$base.o | ninja/lib/libfble.a"
  puts "  lflags = -L ninja/lib"
  puts "  libs = -lfble"
}

puts "build ninja/obj/fble-app.o: obj prgms/fble-app.c"
puts "  iflags = -I ninja/include -I /usr/include/SDL2"
puts "build ninja/bin/fble-app: exe ninja/obj/fble-app.o | ninja/lib/libfble.a"
puts "  lflags = -L ninja/lib"
puts "  libs = -lfble -lSDL2"

