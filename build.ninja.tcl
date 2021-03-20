# ninja-based description of how to build fble and friends.
#
# First time setup:
#   mkdir -p ninja
#   tclsh build.ninja.tcl > build.ninja
#
# Afterwards:
#   ninja -f ninja/build.ninja

# TODO: Add coverage a profiling variants of things?
#
# For coverage:
# * add to gcc flags: -fprofile-arcs -ftest-coverage
# * run tests as desired.
# * run gcov passing all the libfble .o files on the command line.
# * move *.gcov to wherever I prefer they end up.
#
# For profiling:
# * add to gcc flags: -pg
# * run benchmark as desired.
# * run gprof passing the benchmark executable run on the command line.
# * run gcov and move *.gcov files where preferred as described for coverage.
# * remove gmon.out

# Store ninja output files in ninja/ directory.
set builddir "ninja"
puts "builddir = $builddir"

puts {
rule tclsh 
  description = $out
  command = tclsh $in > $out

cFlags = -std=c99 -pedantic -Wall -Werror -gdwarf-3 -ggdb

rule rule
  description = $out
  command = $cmd

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
  command = gcc $cFlags $lflags -o $out $in $libs

rule copy
  description = $out
  command = cp $in $out

rule dir
  description = $out
  command = mkdir -p $out

rule test
  description = $out
  command = $cmd > $out 2>&1 && echo PASSED >> $out || echo FAILED >> $out

# Sample usage:
#  build Foo.fble.d: fbledeps Foo.fble | ninja/bin/fble-deps
#    dir = prgms
rule fbledeps
  description = $out
  depfile = $out
  command = ./ninja/bin/fble-deps $out $in $dir > $out
}

# Any time we run glob over a directory, add that directory to this list.
# We need to make sure to include these directories as a dependency on the
# generation of build.ninja.
set globs [list]

set obj ninja/obj
set fble_objs [list]
lappend globs "fble"
foreach {x} [glob fble/*.c] {
  set object $obj/[string map {.c .o} [file tail $x]]
  lappend fble_objs $object
  puts "build $object: obj $x"
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
set hdrs [list]
foreach {x} [glob fble/fble*.h] {
  set hdr "ninja/include/[file tail $x]"
  lappend hdrs $hdr
  puts "build $hdr: copy $x"
}

lappend globs "tools"
foreach {x} [glob tools/*.c prgms/fble-md5.c prgms/fble-stdio.c] {
  set base [file rootname [file tail $x]]
  puts "build ninja/obj/$base.o: obj $x | $hdrs"
  puts "  iflags = -I ninja/include"
  puts "build ninja/bin/$base: exe ninja/obj/$base.o | ninja/lib/libfble.a"
  puts "  lflags = -L ninja/lib"
  puts "  libs = -lfble"
}

puts "build ninja/obj/fble-app.o: obj prgms/fble-app.c | $hdrs"
puts "  iflags = -I ninja/include -I /usr/include/SDL2"
puts "build ninja/bin/fble-app: exe ninja/obj/fble-app.o | ninja/lib/libfble.a"
puts "  lflags = -L ninja/lib"
puts "  libs = -lfble -lSDL2"

set tests [list]

lappend tests ninja/tests/true.tr
puts "build ninja/tests/true.tr: test"
puts "  cmd = true"

# lappend tests ninja/tests/false.tr
puts "build ninja/tests/false.tr: test"
puts "  cmd = false"

# Returns the list of all subdirectories, recursively, of the given directory.
# The 'root' directory will not be included as a prefix in the returned list
# of directories.
# 'dir' should be empty or end with '/'.
proc dirs { root dir } {
  set l [list]
  foreach {x} [lsort [glob -tails -directory $root -nocomplain -type d $dir*]] {
    lappend l $x
    set l [concat $l [dirs $root "$dir$x/"]]
  }
  return $l
}

set spectestdirs [dirs langs/fble ""]

foreach dir $spectestdirs {
  lappend globs "langs/fble/$dir"
  foreach {t} [lsort [glob -tails -directory langs/fble -nocomplain -type f $dir/*.tcl]] {
    set root [file rootname $t]
    set tcl langs/fble/$t
    set tr ninja/tests/$root.tr
    lappend tests $tr
    set x "tools/spec-test.tcl \
      ninja/bin/fble-test \
      ninja/bin/fble-mem-test \
      langs/fble/Nat.fble \
      ninja/tests/$root \
      $tcl"
    puts "build ninja/tests/$root: dir"
    puts "build $tr: test | $x"
    puts "  cmd = tclsh $x"
  }
}

lappend tests ninja/tests/fble-profile-test.tr
puts "build ninja/tests/fble-profile-test.tr: test | ninja/bin/fble-profile-test"
puts "  cmd = ./ninja/bin/fble-profile-test"

set mains {
  Fble/Tests.fble
  Md5/Main.fble
  Stdio/Cat.fble
  Stdio/Test.fble
  Fblf/Lib/Tests/Compile.fble
  Fblf/Lib/Md5/Stdio.fble
}
foreach x $mains {
  puts "build ninja/$x.d: fbledeps prgms/$x | ninja/bin/fble-deps"
  puts "  dir = prgms"
}

lappend tests ninja/tests/fble-disassemble.tr
puts "build ninja/tests/fble-disassemble.tr: test | ninja/bin/fble-disassemble ninja/Fble/Tests.fble.d"
puts "  cmd = ./ninja/bin/fble-disassemble prgms/Fble/Tests.fble prgms"

lappend tests ninja/tests/fble-tests.tr
puts "build ninja/tests/fble-tests.tr: test | ninja/bin/fble-stdio ninja/Fble/Tests.fble.d"
puts "  cmd = ./ninja/bin/fble-stdio prgms/Fble/Tests.fble prgms"

lappend tests ninja/tests/fble-md5.tr
puts "build ninja/tests/fble-md5.tr: test | ninja/bin/fble-md5 ninja/Md5/Main.fble.d"
puts "  cmd = ./ninja/bin/fble-md5 prgms/Md5/Main.fble prgms /dev/null"

lappend tests ninja/tests/fble-cat.tr
puts "build ninja/tests/fble-cat.tr: test | ninja/bin/fble-stdio ninja/Stdio/Cat.fble.d"
puts "  cmd = ./ninja/bin/fble-stdio prgms/Stdio/Cat.fble prgms < README.txt | cmp README.txt -"

lappend tests ninja/tests/fble-stdio.tr
puts "build ninja/tests/fble-stdio.tr: test | ninja/bin/fble-stdio ninja/Stdio/Test.fble.d"
puts "  cmd = ./ninja/bin/fble-stdio prgms/Stdio/Test.fble prgms | grep PASSED"

lappend tests ninja/tests/fblf-tests.tr
puts "build ninja/obj/fblf-heap.o: obj prgms/Fblf/fblf-heap.c"
puts "  iflags = -I prgms/Fblf"
puts "build ninja/src/fblf-tests.c: rule | ninja/bin/fble-stdio ninja/Fblf/Lib/Tests/Compile.fble.d"
puts "  cmd = ./ninja/bin/fble-stdio prgms/Fblf/Lib/Tests/Compile.fble prgms > ninja/src/fblf-tests.c"
puts "build ninja/obj/fblf-tests.o: obj ninja/src/fblf-tests.c"
puts "  iflags = -I prgms/Fblf"
puts "build ninja/bin/fblf-tests: exe ninja/obj/fblf-tests.o ninja/obj/fblf-heap.o"
puts "build ninja/tests/fblf-tests.tr: test | ninja/bin/fblf-tests"
puts "  cmd = ./ninja/bin/fblf-tests"

lappend tests ninja/tests/fblf-md5.tr
puts "build ninja/src/fblf-md5.c: rule | ninja/bin/fble-stdio ninja/Fblf/Lib/Md5/Stdio.fble.d"
puts "  cmd = ./ninja/bin/fble-stdio prgms/Fblf/Lib/Md5/Stdio.fble prgms > ninja/src/fblf-md5.c"
puts "build ninja/obj/fblf-md5.o: obj ninja/src/fblf-md5.c"
puts "  iflags = -I prgms/Fblf"
puts "build ninja/bin/fblf-md5: exe ninja/obj/fblf-md5.o ninja/obj/fblf-heap.o"
puts "build ninja/tests/fblf-md5.tr: test | ninja/bin/fblf-md5"
puts "  cmd = ./ninja/bin/fblf-md5 /dev/null"

puts "build ninja/tests/summary.txt: tclsh tools/tests.tcl $tests"

puts "build ninja/build.ninja: tclsh build.ninja.tcl | $globs"

