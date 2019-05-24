
exec rm -rf out

# Note: to profile, add -pg flag here, then after running fble-test, run:
#  gprof out/bin/fble-test
set ::FLAGS [list -std=c99 -pedantic -Wall -Werror -O0 -gdwarf-3 -ggdb -fprofile-arcs -ftest-coverage ]

# Compile source for libfble.a
proc gcc_fble {args} {
  puts "gcc $args"
  exec gcc {*}$::FLAGS -I fble {*}$args
}

# Compile source for executables linking to libfble.a
proc gcc_prgm {args} {
  puts "gcc $args"
  exec gcc {*}$::FLAGS -I out/include -L out/lib {*}$args
}

# Compile all object files.
# We compile these separately and ensure they are placed in a subdirectory of
# the out directory so that the profile information generated when running
# code from the objects is placed in a subdirectory of the out directory.
set ::obj out/obj
exec mkdir -p $::obj
set fble_objs [list]
foreach {x} [glob fble/*.c] {
  set object $::obj/[string map {.c .o} [file tail $x]]
  lappend fble_objs $object
  gcc_fble -c -o $object $x
}

# Generate and compile the fble parser.
exec mkdir -p out/src
run bison --report=all --report-file=out/src/parse.tab.report.txt -o out/src/parse.tab.c fble/parse.y 
gcc_fble -c -o $::obj/parse.tab.o out/src/parse.tab.c
lappend fble_objs $::obj/parse.tab.o

# Generate libfble.a
exec mkdir -p out/lib out/include
run ar rcs out/lib/libfble.a {*}$fble_objs
exec cp {*}[glob fble/fble*.h] out/include

# Compile the executables
foreach {x} [glob test/*.c prgms/*.c] {
  set object $::obj/[string map {.c .o} [file tail $x]]
  gcc_prgm -c -o $object $x
}

set ::bin out/bin
exec mkdir -p $::bin
gcc_prgm -o $::bin/fble-test $::obj/fble-test.o -lfble
gcc_prgm -o $::bin/fble-ref-test $::obj/fble-ref-test.o -lfble
gcc_prgm -o $::bin/fble-mem-test $::obj/fble-mem-test.o -lfble
gcc_prgm -o $::bin/fble-snake $::obj/fble-snake.o -lncurses -lfble
gcc_prgm -o $::bin/fble-Snake $::obj/fble-Snake.o -lncurses -lfble
gcc_prgm -o $::bin/fble-tictactoe $::obj/fble-tictactoe.o -lfble
gcc_prgm -o $::bin/fble-md5 $::obj/fble-md5.o -lfble

proc fble-test-error-run { tloc loc expr } {
  set line [dict get $tloc line]
  set file [dict get $tloc file]
  set name "out/[relative_file $file]:$line"

  exec mkdir -p [file dirname $name]
  set fprgm $name.fble
  exec echo $expr > $fprgm
  set errtext [exec $::bin/fble-test --error $fprgm]
  exec echo $errtext > $name.err
  if {-1 == [string first ":$loc: error" $errtext]} {
    error "Expected error at $loc, but got:\n$errtext"
  }
}

proc fble-test-run { cmd loc expr } {
  set line [dict get $loc line]
  set file [dict get $loc file]
  set name "out/[relative_file $file]:$line"

  # Write the program to file.
  exec mkdir -p [file dirname $name]
  set fprgm $name.fble
  exec echo $expr > $fprgm

  # Execute the program.
  exec {*}$cmd $fprgm
}

# See langs/fble/README.txt for the description of this function
proc fble-test { expr } {
  set loc [info frame -1]
  testl $loc fble-test-run $::bin/fble-test $loc $expr
}

# See langs/fble/README.txt for the description of this function
proc fble-test-error { loc expr } {
  set tloc [info frame -1]
  testl $tloc fble-test-error-run $tloc $loc $expr
}

# See langs/fble/README.txt for the description of this function
proc fble-test-memory-constant { expr } {
  set loc [info frame -1]
  testl $loc fble-test-run $::bin/fble-mem-test $loc $expr
}

# See langs/fble/README.txt for the description of this function
proc fble-test-memory-growth { expr } {
  set loc [info frame -1]
  testl $loc fble-test-run "$::bin/fble-mem-test --growth" $loc $expr
}

# Source all *.tcl files under the given directory, recursively.
proc source_all { dir } {
  foreach {x} [lsort [glob -nocomplain -type d $dir/*]] {
    source_all $x
  }

  foreach {x} [lsort [glob -nocomplain -type f $dir/*.tcl]] {
    source $x
  }
}

source_all langs/fble

exec mkdir -p out/cov/spec
run gcov {*}$::fble_objs > out/cov/spec/fble.gcov
exec mv {*}[glob *.gcov] out/cov/spec

test exec $::bin/fble-ref-test
test exec $::bin/fble-test prgms/fble-snake.fble
test exec $::bin/fble-test prgms/fble-tictactoe.fble
test exec $::bin/fble-test prgms/fble-Snake.fble prgms
test exec $::bin/fble-test prgms/AllTests.fble prgms
test exec $::bin/fble-md5 prgms/fble-md5.fble prgms /dev/null

exec mkdir -p out/cov/all
run gcov {*}$::fble_objs > out/cov/all/fble.gcov
exec mv {*}[glob *.gcov] out/cov/all

