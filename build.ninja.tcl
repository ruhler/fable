# ninja-based description of how to build fble and friends.
#
# First time setup:
#   mkdir -p out
#   tclsh build.ninja.tcl > out/build.ninja
#
# Afterwards:
#   ninja -f out/build.ninja

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

set ::out "out"
set ::bin "$::out/bin"
set ::obj "$::out/obj"
set ::lib "$::out/lib"
set ::src "$::out/src"
set ::include "$::out/include"
set ::prgms "$::out/prgms"
set ::test "$::out/test"

puts "builddir = $::out"

puts {
rule rule
  description = $out
  command = $cmd

rule copy
  description = $out
  command = cp $in $out

rule dir
  description = $out
  command = mkdir -p $out

rule test
  description = $out
  command = $cmd > $out 2>&1 && echo PASSED >> $out || echo FAILED >> $out
}

# build --
#   Builds generic targets.
#
# Inputs:
#   targets - the list of targets produced by the command.
#   dependencies - the list of targets this depends on.
#   command - the command to run to produce the targets.
#   args - optional value to use for 'depfile'
proc build { targets dependencies command args } {
  puts "build [join $targets]: rule [join $dependencies]"
  puts "  depfile = [join $args]"
  puts "  cmd = $command"
}

# obj --
#   Builds a .o file.
#
# Inputs:
#   obj - the .o file to build (include $::obj directory).
#   src - the .c file to build the .o file from.
#   iflags - include flags, e.g. "-I foo".
#   args - optional additional dependencies.
proc obj { obj src iflags args } {
  set cflags "-std=c99 -pedantic -Wall -Werror -gdwarf-3 -ggdb"
  set cmd "gcc -MMD -MF $obj.d $cflags $iflags -c -o $obj $src"
  build $obj "$src $args" $cmd $obj.d
}

# bin --
#   Build a binary.
#
# Inputs:
#   bin - the binary file to build (include $::bin directory).
#   objs - the list of .o files to build from.
#   lflags - library flags, e.g. "-L foo/ -lfoo".
#   args - optional additional dependencies.
proc bin { bin objs lflags args } {
  set cflags "-std=c99 -pedantic -Wall -Werror -gdwarf-3 -ggdb"
  build $bin "$objs $args" "gcc $cflags -o $bin $objs $lflags"
}

# Any time we run glob over a directory, add that directory to this list.
# We need to make sure to include these directories as a dependency on the
# generation of build.ninja.
set ::globs [list]

set ::fble_objs [list]
lappend ::globs "fble"
foreach {x} [glob fble/*.c] {
  set object $::obj/[string map {.c .o} [file tail $x]]
  obj $object $x "-I fble"
  lappend ::fble_objs $object
}

eval {
  # Update local includes for fble/parse.y here.
  # See comment in fble/parse.y.
  set includes "fble/fble.h fble/syntax.h"
  set report $::src/parse.tab.report.txt
  set tabc $src/parse.tab.c
  set cmd "bison --report=all --report-file=$report -o $tabc fble/parse.y"
  build "$tabc $report" "fble/parse.y $includes" $cmd
}

obj $::obj/parse.tab.o $src/parse.tab.c "-I fble"
lappend ::fble_objs $::obj/parse.tab.o

set ::libfble "$::lib/libfble.a"
build $::libfble $::fble_objs "ar rcs $::libfble $::fble_objs"

# public header files for libfble.
set hdrs [list]
foreach {x} [glob fble/fble*.h] {
  set hdr "$::include/[file tail $x]"
  lappend hdrs $hdr
  puts "build $hdr: copy $x"
}

lappend globs "tools"
foreach {x} [glob tools/*.c prgms/fble-md5.c prgms/fble-stdio.c] {
  set base [file rootname [file tail $x]]
  obj $::obj/$base.o $x "-I $::include" $hdrs
  bin $::bin/$base $::obj/$base.o "-L $::lib -lfble" $::libfble
}

obj $::obj/fble-app.o prgms/fble-app.c "-I $::include -I /usr/include/SDL2" $hdrs
bin $::bin/fble-app $::obj/fble-app.o "-L $::lib -lfble -lSDL2" $::libfble

set ::tests [list]

lappend ::tests $::test/true.tr
puts "build $::test/true.tr: test"
puts "  cmd = true"

# lappend ::tests $::test/false.tr
puts "build $::test/false.tr: test"
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

foreach dir [dirs langs/fble ""] {
  lappend globs "langs/fble/$dir"
  foreach {t} [lsort [glob -tails -directory langs/fble -nocomplain -type f $dir/*.tcl]] {
    set root [file rootname $t]
    set tcl langs/fble/$t
    set tr $::test/$root.tr
    lappend ::tests $tr
    set x "tools/spec-test.tcl \
      $::bin/fble-test \
      $::bin/fble-mem-test \
      langs/fble/Nat.fble \
      $::test/$root \
      $tcl"
    puts "build $::test/$root: dir"
    puts "build $tr: test | $x"
    puts "  cmd = tclsh $x"
  }
}

lappend ::tests $::test/fble-profile-test.tr
puts "build $::test/fble-profile-test.tr: test | $::bin/fble-profile-test"
puts "  cmd = ./$::bin/fble-profile-test"

set ::mains {
  Fble/Tests.fble
  Md5/Main.fble
  Stdio/Cat.fble
  Stdio/Test.fble
  Fblf/Lib/Tests/Compile.fble
  Fblf/Lib/Md5/Stdio.fble
}
foreach x $::mains {
  puts "build $prgms/$x.d: rule $::bin/fble-deps prgms/$x"
  puts "  depfile = $prgms/$x.d"
  puts "  command = $::bin/fble-deps $prgms/$x.d prgms/$x prgms > $prgms/$x.d"
}

lappend ::tests $::test/fble-disassemble.tr
puts "build $::test/fble-disassemble.tr: test | $::bin/fble-disassemble $::prgms/Fble/Tests.fble.d"
puts "  cmd = ./$::bin/fble-disassemble prgms/Fble/Tests.fble prgms"

lappend ::tests $::test/fble-tests.tr
puts "build $::test/fble-tests.tr: test | $::bin/fble-stdio $::prgms/Fble/Tests.fble.d"
puts "  cmd = ./$::bin/fble-stdio prgms/Fble/Tests.fble prgms"

lappend ::tests $::test/fble-md5.tr
puts "build $::test/fble-md5.tr: test | $::bin/fble-md5 $::prgms/Md5/Main.fble.d"
puts "  cmd = ./$::bin/fble-md5 prgms/Md5/Main.fble prgms /dev/null"

lappend ::tests $::test/fble-cat.tr
puts "build $::test/fble-cat.tr: test | $::bin/fble-stdio $::prgms/Stdio/Cat.fble.d"
puts "  cmd = ./$::bin/fble-stdio prgms/Stdio/Cat.fble prgms < README.txt | cmp README.txt -"

lappend ::tests $::test/fble-stdio.tr
puts "build $::test/fble-stdio.tr: test | $::bin/fble-stdio $::prgms/Stdio/Test.fble.d"
puts "  cmd = ./$::bin/fble-stdio prgms/Stdio/Test.fble prgms | grep PASSED"

lappend ::tests $::test/fblf-tests.tr
obj $::obj/fblf-heap.o prgms/Fblf/fblf-heap.c "-I prgms/Fblf"

eval {
  set deps "$::bin/fble-stdio $::prgms/Fblf/Lib/Tests/Compile.fble.d"
  set cmd "./$::bin/fble-stdio prgms/Fblf/Lib/Tests/Compile.fble prgms > $::src/fblf-tests.c"
  build $::src/fblf-tests.c $deps $cmd
}

obj $::obj/fblf-tests.o $::src/fblf-tests.c "-I prgms/Fblf"
bin $::bin/fblf-tests "$::obj/fblf-tests.o $::obj/fblf-heap.o" ""
puts "build $::test/fblf-tests.tr: test | $::bin/fblf-tests"
puts "  cmd = ./$::bin/fblf-tests"

lappend ::tests $::test/fblf-md5.tr
eval {
  set deps "$::bin/fble-stdio $::prgms/Fblf/Lib/Md5/Stdio.fble.d"
  set cmd "./$::bin/fble-stdio prgms/Fblf/Lib/Md5/Stdio.fble prgms > $::src/fblf-md5.c"
  build $::src/fblf-md5.c $deps $cmd
}

obj $::obj/fblf-md5.o $::src/fblf-md5.c "-I prgms/Fblf"
bin $::bin/fblf-md5 "$::obj/fblf-md5.o $::obj/fblf-heap.o" ""
puts "build $::test/fblf-md5.tr: test | $::bin/fblf-md5"
puts "  cmd = ./$::bin/fblf-md5 /dev/null"

build $::test/summary.txt "tools/tests.tcl $::tests" \
  "tclsh tools/tests.tcl $::tests > $::test/summary.txt"

build $::out/build.ninja "build.ninja.tcl $::globs" \
  "tclsh build.ninja.tcl > $::out/build.ninja"

