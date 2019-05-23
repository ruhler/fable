
exec rm -rf out

# Note: to profile, add -pg flag here, then after running fble-test, run:
#  gprof out/fble/fble-test
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
foreach {x} [list alloc compile eval parse.tab ref syntax value vector] {
  lappend fble_objs out/fble/obj/$x.o
}
run ar rcs out/fble/libfble.a {*}$fble_objs

# Compile the executables
set ::fbletest ./out/fble/fble-test
set ::fblereftest ./out/fble/fble-ref-test
set ::fblememtest ./out/fble/fble-mem-test
set ::fblemd5 ./out/fble/fble-md5
run gcc {*}$FLAGS -o $::fbletest out/fble/obj/fble-test.o -L out/fble -lfble
run gcc {*}$FLAGS -o $::fblereftest out/fble/obj/fble-ref-test.o -L out/fble -lfble
run gcc {*}$FLAGS -o $::fblememtest out/fble/obj/fble-mem-test.o -L out/fble -lfble
run gcc {*}$FLAGS -o ./out/fble/fble-snake out/fble/obj/fble-snake.o -L out/fble -lfble -lncurses
run gcc {*}$FLAGS -o ./out/fble/fble-Snake out/fble/obj/fble-Snake.o -L out/fble -lfble -lncurses
run gcc {*}$FLAGS -o ./out/fble/fble-tictactoe out/fble/obj/fble-tictactoe.o -L out/fble -lfble
run gcc {*}$FLAGS -o $::fblemd5 out/fble/obj/fble-md5.o -L out/fble -lfble

proc fble-test-error-run { tloc loc expr } {
  set line [dict get $tloc line]
  set file [dict get $tloc file]
  set name "out/[relative_file $file]:$line"

  exec mkdir -p [file dirname $name]
  set fprgm $name.fble
  exec echo $expr > $fprgm
  set errtext [exec $::fbletest --error $fprgm]
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
  testl $loc fble-test-run $::fbletest $loc $expr
}

# See langs/fble/README.txt for the description of this function
proc fble-test-error { loc expr } {
  set tloc [info frame -1]
  testl $tloc fble-test-error-run $tloc $loc $expr
}

# See langs/fble/README.txt for the description of this function
proc fble-test-memory-constant { expr } {
  set loc [info frame -1]
  testl $loc fble-test-run $::fblememtest $loc $expr
}

# See langs/fble/README.txt for the description of this function
proc fble-test-memory-growth { expr } {
  set loc [info frame -1]
  testl $loc fble-test-run "$::fblememtest --growth" $loc $expr
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

exec mkdir -p out/fble/cov/spec
run gcov {*}$::fble_objs > out/fble/cov/spec/fble.gcov
exec mv {*}[glob *.gcov] out/fble/cov/spec

test exec $::fblereftest
test exec $::fbletest prgms/fble-snake.fble
test exec $::fbletest prgms/fble-tictactoe.fble
test exec $::fbletest prgms/fble-Snake.fble prgms
test exec $::fbletest prgms/AllTests.fble prgms
test exec $::fblemd5 prgms/fble-md5.fble prgms /dev/null

exec mkdir -p out/fble/cov/all
run gcov {*}$::fble_objs > out/fble/cov/all/fble.gcov
exec mv {*}[glob *.gcov] out/fble/cov/all

