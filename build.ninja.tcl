# ninja-based description of how to build fble and friends.
#
# First time setup:
#   mkdir -p out
#   tclsh build.ninja.tcl > out/build.ninja
#
# Afterwards:
#   ninja -f out/build.ninja

# Output directories used for build.
set ::out "out"
set ::bin "$::out/bin"
set ::obj "$::out/obj"
set ::lib "$::out/lib"
set ::src "$::out/src"
set ::include "$::out/include"
set ::prgms "$::out/prgms"
set ::test "$::out/test"

# build.ninja header.
puts "builddir = $::out"
puts {
# We build everything using this one generic rule, specifying the command
# explicitly for every target. Because defining separate rules for different
# command lines doesn't seem to buy us much.
rule rule
  description = $out
  command = $cmd
}

# build --
#   Builds generic targets.
#
# Inputs:
#   targets - the list of targets produced by the command.
#   dependencies - the list of targets this depends on.
#   command - the command to run to produce the targets.
#   args - optional value to use for ninja 'depfile'
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

# test --
#   Build a test result.
#
# Inputs:
#   tr - the .tr file to output the results to.
#   deps - depencies for the test.
#   cmd - the test command to run, which should exit 0 for PASSED.
#
# Adds the .tr file to global list of tests to run.
set ::tests [list]
proc test { tr deps cmd } {
  lappend ::tests $tr
  build $tr $deps \
    "$cmd > $tr 2>&1 && echo PASSED >> $tr || echo FAILED >> $tr"
}

# Any time we run glob over a directory, add that directory to this list.
# We need to make sure to include these directories as a dependency on the
# generation of build.ninja.
set ::globs [list]

# .o files used to implement libfble.a
set ::fble_objs [list]
lappend ::globs "fble"
foreach {x} [glob fble/*.c] {
  set object $::obj/[string map {.c .o} [file tail $x]]
  obj $object $x "-I fble"
  lappend ::fble_objs $object
}

# parser
eval {
  # Update local includes for fble/parse.y here.
  # See comment in fble/parse.y.
  set includes "fble/fble.h fble/syntax.h"
  set report $::src/parse.tab.report.txt
  set tabc $src/parse.tab.c
  set cmd "bison --report=all --report-file=$report -o $tabc fble/parse.y"
  build "$tabc $report" "fble/parse.y $includes" $cmd

  obj $::obj/parse.tab.o $src/parse.tab.c "-I fble"
  lappend ::fble_objs $::obj/parse.tab.o
}

# libfble.a
set ::libfble "$::lib/libfble.a"
build $::libfble $::fble_objs "ar rcs $::libfble $::fble_objs"

# public header files for libfble
set hdrs [list]
foreach {x} [glob fble/fble*.h] {
  set hdr "$::include/[file tail $x]"
  lappend hdrs $hdr
  build $hdr $x "cp $x $hdr"
}

# fble tool binaries
lappend globs "tools"
foreach {x} [glob tools/*.c prgms/fble-md5.c prgms/fble-stdio.c] {
  set base [file rootname [file tail $x]]
  obj $::obj/$base.o $x "-I $::include" $hdrs
  bin $::bin/$base $::obj/$base.o "-L $::lib -lfble" $::libfble
}
obj $::obj/fble-app.o prgms/fble-app.c "-I $::include -I /usr/include/SDL2" $hdrs
bin $::bin/fble-app $::obj/fble-app.o "-L $::lib -lfble -lSDL2" $::libfble

# tests
test $::test/true.tr "" true
#test $::test/false.tr "" false

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

# fble language spec tests
foreach dir [dirs langs/fble ""] {
  lappend globs "langs/fble/$dir"
  foreach {t} [lsort [glob -tails -directory langs/fble -nocomplain -type f $dir/*.tcl]] {
    set root [file rootname $t]
    set tcl langs/fble/$t
    set tr $::test/$root.tr
    set x "tools/spec-test.tcl \
      $::bin/fble-test \
      $::bin/fble-mem-test \
      langs/fble/Nat.fble \
      $::test/$root \
      $tcl"
    build $::test/$root "" "mkdir -p $::test/$root"
    test $tr $x "tclsh $x"
  }
}

# fble-profile-test
test $::test/fble-profile-test.tr $::bin/fble-profile-test \
  "./$::bin/fble-profile-test"

# dependency files for the top level .fble files used in tests
foreach x {
  Fble/Tests.fble
  Md5/Main.fble
  Stdio/Cat.fble
  Stdio/Test.fble
  Fblf/Lib/Tests/Compile.fble
  Fblf/Lib/Md5/Stdio.fble
} {
  build $prgms/$x.d "$::bin/fble-deps prgms/$x" \
    "$::bin/fble-deps $prgms/$x.d prgms/$x prgms > $prgms/$x.d" \
    $prgms/$x.d
}

# fble-disassemble test
test $::test/fble-disassemble.tr \
  "$::bin/fble-disassemble $::prgms/Fble/Tests.fble.d" \
  "./$::bin/fble-disassemble prgms/Fble/Tests.fble prgms"

# Fble/Tests.fble tests
test $::test/fble-tests.tr "$::bin/fble-stdio $::prgms/Fble/Tests.fble.d" \
  "./$::bin/fble-stdio prgms/Fble/Tests.fble prgms"

# fble-md5 test
test $::test/fble-md5.tr "$::bin/fble-md5 $::prgms/Md5/Main.fble.d" \
  "./$::bin/fble-md5 prgms/Md5/Main.fble prgms /dev/null"

# fble-cat test
test $::test/fble-cat.tr "$::bin/fble-stdio $::prgms/Stdio/Cat.fble.d" \
  "./$::bin/fble-stdio prgms/Stdio/Cat.fble prgms < README.txt | cmp README.txt -"

# fble-stdio test
test $::test/fble-stdio.tr "$::bin/fble-stdio $::prgms/Stdio/Test.fble.d" \
  "./$::bin/fble-stdio prgms/Stdio/Test.fble prgms | grep PASSED"

# fblf compilation test
obj $::obj/fblf-heap.o prgms/Fblf/fblf-heap.c "-I prgms/Fblf"
build $::src/fblf-tests.c \
  "$::bin/fble-stdio $::prgms/Fblf/Lib/Tests/Compile.fble.d" \
  "./$::bin/fble-stdio prgms/Fblf/Lib/Tests/Compile.fble prgms > $::src/fblf-tests.c"
obj $::obj/fblf-tests.o $::src/fblf-tests.c "-I prgms/Fblf"
bin $::bin/fblf-tests "$::obj/fblf-tests.o $::obj/fblf-heap.o" ""
test $::test/fblf-tests.tr $::bin/fblf-tests ./$::bin/fblf-tests

# fblf md5 compilation test
build $::src/fblf-md5.c \
  "$::bin/fble-stdio $::prgms/Fblf/Lib/Md5/Stdio.fble.d" \
  "./$::bin/fble-stdio prgms/Fblf/Lib/Md5/Stdio.fble prgms > $::src/fblf-md5.c"
obj $::obj/fblf-md5.o $::src/fblf-md5.c "-I prgms/Fblf"
bin $::bin/fblf-md5 "$::obj/fblf-md5.o $::obj/fblf-heap.o" ""
test $::test/fblf-md5.tr $::bin/fblf-md5 "./$::bin/fblf-md5 /dev/null"

# test summary
build $::test/summary.txt "tools/tests.tcl $::tests" \
  "tclsh tools/tests.tcl $::tests > $::test/summary.txt"

# build.ninja
build $::out/build.ninja "build.ninja.tcl $::globs" \
  "tclsh build.ninja.tcl > $::out/build.ninja"

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

