
# Run a command, printing the command as it is run.
proc run {args} {
  puts $args
  exec {*}$args
}

exec rm -rf out
set FLAGS [list -I . -std=c99 -pedantic -Wall -Werror -O0 -fprofile-arcs -ftest-coverage -gdwarf-3 -ggdb] 

# Compile libfblc.a
exec mkdir -p out/libfblc
set fblc_srcs [list check.c exec.c fblcs.c load.c parse.c resolve.c value.c]
set fblc_objs [list]
foreach {x} $fblc_srcs {
  set obj out/libfblc/[string map {.c .o} $x]
  lappend fblc_objs $obj
  run gcc {*}$FLAGS -c -o $obj fblc/$x
}
run ar rcs out/libfblc.a {*}$fblc_objs

# Compile the executables
set ::fblc ./out/fblc
set ::fblccheck ./out/fblc-check
set ::fblctest ./out/fblc-test
run gcc {*}$FLAGS -o $::fblc fblc/fblc.c -L out -lfblc
run gcc {*}$FLAGS -o $::fblccheck fblc/fblc-check.c -L out -lfblc
run gcc {*}$FLAGS -o $::fblctest fblc/fblc-test.c -L out -lfblc
run gcc {*}$FLAGS -o out/fblc-snake fblc/fblc-snake.c -L out -lfblc -lncurses
run gcc {*}$FLAGS -o out/fblc-tictactoe fblc/fblc-tictactoe.c -L out -lfblc

proc check_coverage {name} {
  exec mkdir -p out/coverage/$name
  run gcov {*}[glob out/libfblc/*.o] > out/coverage/$name/fblc.gcov
  exec mv {*}[glob *.gcov] out/coverage/$name
  exec rm {*}[glob *.gcno *.gcda]
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

set ::skipped 0
proc skip { args } {
  set ::skipped [expr $::skipped + 1]
}

foreach {x} [lsort [glob langs/fblc/*.tcl]]  {
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
puts "  Spec    : [exec tail -n 1 out/coverage/spectest/fblc.gcov]"
puts "  Overall : [exec tail -n 1 out/coverage/overall/fblc.gcov]"

