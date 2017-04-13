
# Run a command, printing the command as it is run.
proc run {args} {
  puts $args
  exec {*}$args
}

exec rm -rf out
set FLAGS [list -I . -std=c99 -pedantic -Wall -Werror -O0 -fprofile-arcs -ftest-coverage -gdwarf-3 -ggdb] 

# Compile all object files.
# We compile these separately and ensure they are placed in a subdirectory of
# the out directory so that the profile information generated when running
# code from the objects is placed in a subdirectory of the out directory.
exec mkdir -p out/obj/fblc out/obj/fbld
foreach {x} [glob fblc/*.c fbld/*.c] {
  set obj out/obj/[string map {.c .o} $x]
  run gcc {*}$FLAGS -c -o $obj $x
}

# Generate libfblc.a
set fblc_objs [list]
foreach {x} [list check exec fblcs load parse resolve value vector] {
  lappend fblc_objs out/obj/fblc/$x.o
}
run ar rcs out/libfblc.a {*}$fblc_objs

# Compile the executables
set ::fblc ./out/fblc
set ::fblccheck ./out/fblc-check
set ::fblctest ./out/fblc-test
set ::fbldtest ./out/fbld-test
run gcc {*}$FLAGS -o $::fblc out/obj/fblc/fblc.o -L out -lfblc
run gcc {*}$FLAGS -o $::fblccheck out/obj/fblc/fblc-check.o -L out -lfblc
run gcc {*}$FLAGS -o $::fblctest out/obj/fblc/fblc-test.o -L out -lfblc
run gcc {*}$FLAGS -o $::fbldtest out/obj/fbld/fbld-test.o -L out -lfblc
run gcc {*}$FLAGS -o out/fblc-snake out/obj/fblc/fblc-snake.o -L out -lfblc -lncurses
run gcc {*}$FLAGS -o out/fblc-tictactoe out/obj/fblc/fblc-tictactoe.o -L out -lfblc

proc check_coverage {name} {
  exec mkdir -p out/cov/$name
  run gcov {*}$::fblc_objs > out/cov/$name/fblc.gcov
  exec mv {*}[glob *.gcov] out/cov/$name
}

# Spec tests
# Test that the given fblc text program is malformed, and that the error is
# located at loc. loc should be of the form line:col, where line is the line
# number of the expected error and col is the column number within that line
# of the expected error.
proc fblc-check-error { program loc } {
  set testloc [info frame -1]
  set line [dict get $testloc line]
  set file [dict get $testloc file]
  set name "[file tail $file]_$line"

  exec mkdir -p out/test/fblc
  set fprogram ./out/test/fblc/$name.fblc
  exec echo $program > $fprogram
  try {
    set errtext [exec $::fblccheck --error $fprogram]
  } on error {results options} {
    error "$file:$line: error: fblc-check passed unexpectedly: $results"
  }
  exec echo $errtext > ./out/test/fblc/$name.err
  if {-1 == [string first ":$loc: error" $errtext]} {
    error "$file:$line: error: Expected error at $loc, but got:\n$errtext"
  }
}

# Test running the process 'entry' in 'program' with given arguments.
# The 'script' should be a sequence of commands of the form
# 'put <port> <value>', 'get <portid> <value>', and 'return <value>'.
# The put command causes the value to be written to the given port. The get
# command gets a value from the given port and checks that it is equivalent to
# the given value. The return command checks that the process has returned a
# result equivalent to the given value.
proc fblc-test { program entry args script } {
  set loc [info frame -1]
  set line [dict get $loc line]
  set file [dict get $loc file]
  set name "[file tail $file]_$line"

  # Write the script to file.
  exec mkdir -p out/test/fblc
  set fscript ./out/test/fblc/$name.script
  exec rm -f $fscript
  exec touch $fscript
  foreach cmd [split [string trim $script] "\n"] {
    exec echo [string trim $cmd] >> $fscript
  }

  # Write the program to file.
  set fprogram ./out/test/fblc/$name.fblc
  exec echo $program > $fprogram

  try {
    exec $::fblctest $fscript $fprogram $entry {*}$args
  } on error {results options} {
    error "$file:$line: error: \n$results"
  }
}

# See langs/fbld/README.txt for the description of this function.
proc fbld-test { program entry args script } {
  set loc [info frame -1]
  set line [dict get $loc line]
  set file [dict get $loc file]
  set name "[file tail $file]_$line"
  set dir ./out/test/fbld/$name
  exec mkdir -p $dir

  # Generate the script.
  set fscript $dir/script
  exec rm -f $fscript
  exec touch $fscript
  foreach cmd [split [string trim $script] "\n"] {
    exec echo [string trim $cmd] >> $fscript
  }

  # Write the modules to file.
  foreach m $program {
    exec echo [lindex $m 1] > $dir/[lindex $m 0]
  }

  try {
    exec $::fbldtest $fscript $dir $entry {*}$args
  } on error {results options} {
    error "$file:$line: error: \n$results"
  }
}

set ::skipped 0
proc skip { args } {
  set ::skipped [expr $::skipped + 1]
}

foreach {x} [lsort [glob langs/fblc/*.tcl]]  {
  puts "test $x"
  source $x
}

foreach {x} [lsort [glob langs/fbld/*.tcl]]  {
  puts "test $x"
  source $x
}

check_coverage spectest

# Additional Tests.

# Execute the given command, raising an error if the exit status doesn't match
# the given expected status.
proc expect_status {status args} {
  set got -1
  try {
    run {*}$args
    set got 0
  } trap CHILDSTATUS {results options} {
    set got [lindex [dict get $options -errorcode] 2]
  }
  if {$got != $status} {
    error "Expected exit status $status, but got $got"
  }
}

expect_status 1 $::fblc
expect_status 0 $::fblc --help
expect_status 1 $::fblc no_such_file main
run $::fblc prgms/clock.fblc incr "Digit:1(Unit())" > out/clockincr.got
exec echo "Digit:2(Unit())" > out/clockincr.wnt
exec diff out/clockincr.wnt out/clockincr.got

run $::fblc prgms/calc.fblc main > out/calc.got
exec echo "TL:pass(Unit())" > out/calc.wnt
exec diff out/calc.wnt out/calc.got

run $::fblc prgms/tictactoe.fblc TestBoardStatus > out/tictactoe.TestBoardStatus.got
exec echo "TestResult:Passed(Unit())" > out/tictactoe.TestBoardStatus.wnt
exec diff out/tictactoe.TestBoardStatus.wnt out/tictactoe.TestBoardStatus.got

run $::fblc prgms/tictactoe.fblc TestChooseBestMoveNoLose > out/tictactoe.TestChooseBestMoveNoLose.got
exec echo "PositionTestResult:Passed(Unit())" > out/tictactoe.TestChooseBestMoveNoLose.wnt
exec diff out/tictactoe.TestChooseBestMoveNoLose.wnt out/tictactoe.TestChooseBestMoveNoLose.got

run $::fblc prgms/tictactoe.fblc TestChooseBestMoveWin > out/tictactoe.TestChooseBestMoveWin.got
exec echo "PositionTestResult:Passed(Unit())" > out/tictactoe.TestChooseBestMoveWin.wnt
exec diff out/tictactoe.TestChooseBestMoveWin.wnt out/tictactoe.TestChooseBestMoveWin.got

run $::fblc prgms/boolcalc.fblc Test > out/boolcalc.Test.got
exec echo "TestFailures:nil(Unit())" > out/boolcalc.Test.wnt
exec diff out/boolcalc.Test.wnt out/boolcalc.Test.got

run $::fblc prgms/ints.fblc Test > out/ints.Test.got
exec echo "TestFailureS:nil(Unit())" > out/ints.Test.wnt
exec diff out/ints.Test.wnt out/ints.Test.got

run $::fblccheck prgms/snake.fblc

check_coverage overall

# Report summary results
puts "Skipped Tests: $::skipped"
puts "fblc Coverage: "
puts "  Spec    : [exec tail -n 1 out/cov/spectest/fblc.gcov]"
puts "  Overall : [exec tail -n 1 out/cov/overall/fblc.gcov]"

