
# Run a command, printing the command as it is run.
proc run {args} {
  puts $args
  exec {*}$args
}

exec rm -rf out
set FLAGS [list -I fblc -I fbld -std=c99 -pedantic -Wall -Werror -O0 -fprofile-arcs -ftest-coverage -gdwarf-3 -ggdb] 

# Compile all object files.
# We compile these separately and ensure they are placed in a subdirectory of
# the out directory so that the profile information generated when running
# code from the objects is placed in a subdirectory of the out directory.
exec mkdir -p out/obj/fblc out/obj/fbld
foreach {x} [glob fblc/*.c fbld/*.c] {
  set obj out/obj/[string map {.c .o} $x]
  run gcc {*}$FLAGS -c -o $obj $x
}

# Generate and compile the fbld parser.
exec mkdir -p out/src/fbld
run bison -o out/src/fbld/parse.tab.c fbld/parse.y 
run gcc {*}$FLAGS -c -o out/obj/fbld/parse.tab.o out/src/fbld/parse.tab.c

# Generate libfblc.a
set fblc_objs [list]
foreach {x} [list check exec fblcs load malloc parse resolve value vector] {
  lappend fblc_objs out/obj/fblc/$x.o
}
run ar rcs out/libfblc.a {*}$fblc_objs

# Generate libfbld.a
set fbld_objs [list]
foreach {x} [list check compile fbld load parse.tab] {
  lappend fbld_objs out/obj/fbld/$x.o
}
run ar rcs out/libfbld.a {*}$fbld_objs

# Compile the executables
set ::fblc ./out/fblc
set ::fblccheck ./out/fblc-check
set ::fblctest ./out/fblc-test
set ::fbldcheck ./out/fbld-check
set ::fbldtest ./out/fbld-test
run gcc {*}$FLAGS -o $::fblc out/obj/fblc/fblc.o -L out -lfblc
run gcc {*}$FLAGS -o $::fblccheck out/obj/fblc/fblc-check.o -L out -lfblc
run gcc {*}$FLAGS -o $::fblctest out/obj/fblc/fblc-test.o -L out -lfblc
run gcc {*}$FLAGS -o $::fbldcheck out/obj/fbld/fbld-check.o -L out -lfbld -lfblc
run gcc {*}$FLAGS -o $::fbldtest out/obj/fbld/fbld-test.o -L out -lfbld -lfblc
run gcc {*}$FLAGS -o out/fblc-snake out/obj/fblc/fblc-snake.o -L out -lfblc -lncurses
run gcc {*}$FLAGS -o out/fblc-tictactoe out/obj/fblc/fblc-tictactoe.o -L out -lfblc

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

# See langs/fbld/README.txt for the description of this function
proc fbld-check-error { program module loc } {
  set testloc [info frame -1]
  set line [dict get $testloc line]
  set file [dict get $testloc file]
  set name "[file tail $file]_$line"
  set dir ./out/test/fbld/$name
  exec mkdir -p $dir

  # Write the modules to file.
  foreach {name content} $program {
    exec echo $content > $dir/$name
  }

  try {
    set errtext [exec $::fbldcheck --error $dir $module]
  } on error {results options} {
    error "$file:$line: error: $results"
  }
  exec echo $errtext > $dir/$name.err
  if {-1 == [string first "$loc: error" $errtext]} {
    error "$file:$line: error: Expected error at $loc, but got:\n$errtext"
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
  foreach {name content} $program {
    exec echo $content > $dir/$name
  }

  try {
    exec $::fbldtest $fscript $dir $entry {*}$args
  } on error {results options} {
    error "$file:$line: error: \n$results"
  }
}

set ::skipped [list]
proc skip { args } {
  set testloc [info frame -1]
  set line [dict get $testloc line]
  set file [dict get $testloc file]
  set name "[file tail $file]_$line"
  lappend ::skipped $name
}

foreach {x} [lsort [glob langs/fblc/*.tcl]]  {
  puts "test $x"
  source $x
}

exec mkdir -p out/cov/fblc/spec
run gcov {*}$::fblc_objs > out/cov/fblc/spec/fblc.gcov
exec mv {*}[glob *.gcov] out/cov/fblc/spec

foreach {x} [lsort [glob langs/fbld/*.tcl]]  {
  puts "test $x"
  source $x
}

exec mkdir -p out/cov/fbld/spec
run gcov {*}$::fbld_objs > out/cov/fbld/spec/fbld.gcov
exec mv {*}[glob *.gcov] out/cov/fbld/spec

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

exec echo "return Digit:2(Unit())" > out/clockincr.wnt
run $::fblctest out/clockincr.wnt prgms/clock.fblc incr "Digit:1(Unit())"

exec echo "return TL:pass(Unit())" > out/calc.wnt
run $::fblctest out/calc.wnt prgms/calc.fblc main

exec echo "return TestResult:Passed(Unit())" > out/tictactoe.TestBoardStatus.wnt
run $::fblctest out/tictactoe.TestBoardStatus.wnt prgms/tictactoe.fblc TestBoardStatus

exec echo "return PositionTestResult:Passed(Unit())" > out/tictactoe.TestChooseBestMoveNoLose.wnt
run $::fblctest out/tictactoe.TestChooseBestMoveNoLose.wnt prgms/tictactoe.fblc TestChooseBestMoveNoLose

exec echo "return PositionTestResult:Passed(Unit())" > out/tictactoe.TestChooseBestMoveWin.wnt
run $::fblctest out/tictactoe.TestChooseBestMoveWin.wnt prgms/tictactoe.fblc TestChooseBestMoveWin

run $::fblctest prgms/boolcalc.wnt prgms/boolcalc.fblc Test
run $::fblctest prgms/ints.wnt prgms/ints.fblc Test
run $::fblccheck prgms/snake.fblc
run $::fbldtest prgms/UBNatTest.wnt prgms Test@UBNatTest
run $::fbldtest prgms/PrimesTest.wnt prgms Test@PrimesTest

exec mkdir -p out/cov/fblc/all out/cov/fbld/all
run gcov {*}$::fblc_objs > out/cov/fblc/all/fblc.gcov
exec mv {*}[glob *.gcov] out/cov/fblc/all
run gcov {*}$::fbld_objs > out/cov/fbld/all/fbld.gcov
exec mv {*}[glob *.gcov] out/cov/fbld/all

# Report summary results
puts "Skipped Tests:"
foreach test $::skipped {
  puts "  $test"
}
puts "fblc Coverage: "
puts "  Spec: [exec tail -n 1 out/cov/fblc/spec/fblc.gcov]"
puts "  All : [exec tail -n 1 out/cov/fblc/all/fblc.gcov]"
puts "fbld Coverage: "
puts "  Spec: [exec tail -n 1 out/cov/fbld/spec/fbld.gcov]"
puts "  All : [exec tail -n 1 out/cov/fbld/all/fbld.gcov]"

