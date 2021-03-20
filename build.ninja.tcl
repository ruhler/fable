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

set ::out "ninja"
set ::bin "$::out/bin"
set ::obj "$::out/obj"
set ::lib "$::out/lib"
set ::src "$::out/src"
set ::include "$::out/include"
set ::prgms "$::out/prgms"
set ::test "$::out/test"

puts "builddir = $::out"

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
}

# A rule to build generic targets.
# Inputs:
#   targets - the list of targets produced by the command.
#   dependencies - the list of targets this depends on.
#   command - the command to run to produce the targets.
#
# The lists have variable substitution performed on them, so you can use
# variable substitutions in multi-line brace delimited lists.
proc build { targets dependencies command } {
  puts "build [subst [join $targets]]: rule | [subst [join $dependencies]]"
  puts "  cmd = [subst [join $command]]"
}

# Any time we run glob over a directory, add that directory to this list.
# We need to make sure to include these directories as a dependency on the
# generation of build.ninja.
set ::globs [list]

set ::fble_objs [list]
lappend ::globs "fble"
foreach {x} [glob fble/*.c] {
  set object $::obj/[string map {.c .o} [file tail $x]]
  lappend ::fble_objs $object
  puts "build $object: obj $x"
  puts "  iflags = -I fble"
}

# For any changes to how parser_includes works, please update the comment
# above the local includes in fble/parse.y.
set ::parser_includes "fble/fble.h fble/syntax.h"
puts "build $src/parse.tab.c $src/parse.tab.report.txt: parser fble/parse.y | $::parser_includes"
puts "  tab_c = $src/parse.tab.c"
puts "  report = $src/parse.tab.report.txt"

puts "build $::obj/parse.tab.o: obj $src/parse.tab.c"
puts "  iflags = -I fble"
lappend ::fble_objs $::obj/parse.tab.o

set ::libfble "$::lib/libfble.a"
puts "build $::libfble: lib $::fble_objs"

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
  puts "build $::obj/$base.o: obj $x | $hdrs"
  puts "  iflags = -I $::include"
  puts "build $::bin/$base: exe $::obj/$base.o | $::libfble"
  puts "  lflags = -L $::lib"
  puts "  libs = -lfble"
}

puts "build $::obj/fble-app.o: obj prgms/fble-app.c | $hdrs"
puts "  iflags = -I $::include -I /usr/include/SDL2"
puts "build $::bin/fble-app: exe $::obj/fble-app.o | $::libfble"
puts "  lflags = -L $::lib"
puts "  libs = -lfble -lSDL2"

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
  puts "build $prgms/$x.d: rule | $::bin/fble-deps prgms/$x"
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
puts "build $::obj/fblf-heap.o: obj prgms/Fblf/fblf-heap.c"
puts "  iflags = -I prgms/Fblf"

build $::src/fblf-tests.c {
  $::bin/fble-stdio
  $::prgms/Fblf/Lib/Tests/Compile.fble.d
} {
  ./$::bin/fble-stdio prgms/Fblf/Lib/Tests/Compile.fble prgms > $::src/fblf-tests.c
}

puts "build $::obj/fblf-tests.o: obj $::src/fblf-tests.c"
puts "  iflags = -I prgms/Fblf"
puts "build $::bin/fblf-tests: exe $::obj/fblf-tests.o $::obj/fblf-heap.o"
puts "build $::test/fblf-tests.tr: test | $::bin/fblf-tests"
puts "  cmd = ./$::bin/fblf-tests"

lappend ::tests $::test/fblf-md5.tr
build $::src/fblf-md5.c {
  $::bin/fble-stdio
  $::prgms/Fblf/Lib/Md5/Stdio.fble.d
} "./$::bin/fble-stdio prgms/Fblf/Lib/Md5/Stdio.fble prgms > $::src/fblf-md5.c"

puts "build $::obj/fblf-md5.o: obj $::src/fblf-md5.c"
puts "  iflags = -I prgms/Fblf"
puts "build $::bin/fblf-md5: exe $::obj/fblf-md5.o $::obj/fblf-heap.o"
puts "build $::test/fblf-md5.tr: test | $::bin/fblf-md5"
puts "  cmd = ./$::bin/fblf-md5 /dev/null"

puts "build $::test/summary.txt: tclsh tools/tests.tcl $::tests"

puts "build $::out/build.ninja: tclsh build.ninja.tcl | $::globs"

