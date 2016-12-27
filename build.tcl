
exec rm -rf out
exec mkdir -p out/test out/fblc out/fblcbi out/prgms 
set FLAGS [list -I . -std=c99 -pedantic -Wall -Werror -O0 -fprofile-arcs -ftest-coverage -gdwarf-3 -ggdb] 

# Compile fblc-check, fblcbe, fblcbi
set objs [list]
foreach {x} [glob fblcbi/*.c] {
  set obj out/fblcbi/[string map {.c .o} [file tail $x]]
  if {-1 == [lsearch [list fblcbi/fblcbe.c fblcbi/fblcbi.c fblcbi/fblc-check.c] $x]} {
    lappend objs $obj
  }
  puts "cc $x"
  exec gcc {*}$FLAGS -c -o $obj $x
}
foreach {x} [list fblcbe fblcbi fblc-check] {
  puts "ld -o out/$x"
  exec gcc {*}$FLAGS -o out/prgms/$x -lgc out/fblcbi/$x.o {*}$objs
}

# Compile fblc
foreach {x} [lsort [glob fblc/*.c]] {
  puts "cc $x"
  exec gcc {*}$FLAGS -c -o out/fblc/[string map {.c .o} [file tail $x]] $x
}
puts "ld -o out/fblc"
exec gcc {*}$FLAGS -o out/prgms/fblc -lgc {*}[glob out/fblc/*.o]

# Compile pgrms
set FLAGS [list -std=c99 -pedantic -Wall -Werror -O0 -ggdb]
foreach {x} [glob prgms/*.c] {
  puts "cc $x"
  set exe out/prgms/[string map {.c ""} [file tail $x]]
  exec gcc {*}$FLAGS -o $exe $x
}

set ::fblc ./out/prgms/fblc
set ::fblccheck ./out/prgms/fblc-check
set ::testfblc ./out/prgms/testfblc
set ::fblcbe ./out/prgms/fblcbe
set ::fblcbi ./out/prgms/fblcbi

proc check_coverage {name} {
  exec mkdir -p out/$name/fblc out/$name/fblcbi
  exec gcov {*}[glob out/fblc/*.o] > out/$name/fblc.gcov
  exec mv {*}[glob *.gcov] out/$name/fblc
  exec gcov {*}[glob out/fblcbi/*.o] > out/$name/fblcbi.gcov
  exec mv {*}[glob *.gcov] out/$name/fblcbi
}

# Spec tests
# Test that running function or process 'entry' in 'program' with the given
# 'args' and no ports leads to the given 'result'.
proc expect_result { result program entry args } {
  set loc [info frame -1]
  set line [dict get $loc line]
  set file [dict get $loc file]

  try {
    set got [exec echo $program | $::fblc /dev/stdin $entry {*}$args]
    if {$got != $result} {
      error "$file:$line: error: Expected '$result', but got '$got'"
    }
  } trap CHILDSTATUS {results options} {
    error "$file:$line: error: Expected '$result', but got:\n$results"
  }
}

# Test that running the process 'entry' in 'program' with given ports and
# arguments leads to the given 'result'.
# The ports should be specified as {i <portid>} for input ports and as
# {o <portid>} for output ports. The 'script' should be a sequence of commands
# of the form 'put <portid> <value>' and 'get <portid> <value>'. The put
# command causes the value to be written to the given port. The get command
# gets a value from the given port and checks that it is equivalent to the
# given value.
proc expect_proc_result { result program entry ports args script } {
  set loc [info frame -1]
  set line [dict get $loc line]
  set file [dict get $loc file]
  set name "[file tail $file]_$line"

  # Set up the port spec.
  set port_specs [list]
  foreach {type id} [join $ports] {
    lappend port_specs $type:$id
  }
  set portspec [join $port_specs ","]

  # Write the script to file.
  set fscript ./out/test/$name.script
  exec rm -f $fscript
  foreach cmd [split [string trim $script] "\n"] {
    exec echo [string trim $cmd] >> $fscript
  }

  # Write the program to file.
  set fprogram ./out/test/$name.fblc
  exec echo $program > $fprogram

  try {
    set got [exec $::testfblc $portspec $fscript $::fblc $fprogram $entry {*}$args]
    if {$got != $result} {
      error "$file:$line: error: Expected '$result', but got '$got'"
    }
  } trap CHILDSTATUS {results options} {
    error "$file:$line: error: Expected '$result', but got:\n$results"
  }
}

# Test that the given fblc text program is malformed.
proc fblc-check-error { program } {
  set loc [info frame -1]
  set line [dict get $loc line]
  set file [dict get $loc file]
  set name "[file tail $file]_$line"

  try {
    set fprogram ./out/test/$name.fblc
    exec echo $program > $fprogram
    exec $::fblccheck --error $fprogram
  } trap CHILDSTATUS {results options} {
    error "$file:$line: error: fblc-check passed unexpectedly"
  }
}

# Test that running function or process 'entry' in 'program' with the given
# 'args' and no ports leads to the given 'result'.
# result, and args should be specified as a sequence of ascii digits '0' and
# '1'.
# entry should be specified as an integer.
# program should be specified as a text program, which will be converted to a
# binary encoding using fblcbe.
proc expect_result_b { result program entry args } {
  set loc [info frame -1]
  set line [dict get $loc line]
  set file [dict get $loc file]
  set name "[file tail $file]_$line"

  try {
    set fprogram ./out/test/$name.fblc
    set fbits $fprogram.bin
    exec echo $program > $fprogram
    exec $::fblcbe $fprogram > $fbits
    set got [exec $::fblcbi $fbits $entry {*}$args]
    if {$got != $result} {
      error "$file:$line: error: Expected '$result', but got '$got'"
    }
  } trap CHILDSTATUS {results options} {
    error "$file:$line: error: Expected '$result', but got:\n$results"
  } on error msg {
    error "$file:$line: error: $msg"
  }
}

set ::skipped 0
proc skip { args } {
  set ::skipped [expr $::skipped + 1]
}

foreach {x} [lsort [glob test/*.tcl]]  {
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
    exec {*}$args
    set got 0
  } trap CHILDSTATUS {results options} {
    set got [lindex [dict get $options -errorcode] 2]
  }
  if {$got != $status} {
    error "Expected exit status $status, but got $got"
  }
}

# Test fblc.
puts "test $::fblc"
expect_status 64 $::fblc

puts "test $::fblc --help"
expect_status 0 $::fblc --help

puts "test $::fblc no_such_file"
expect_status 66 $::fblc no_such_file main

puts "test prgms/clock.fblc"
exec $::fblc prgms/clock.fblc incr "Digit:1(Unit())" > out/clockincr.got
exec echo "Digit:2(Unit())" > out/clockincr.wnt
exec diff out/clockincr.wnt out/clockincr.got

puts "test prgms/calc.fblc"
exec $::fblc prgms/calc.fblc main > out/calc.got
exec grep "/// Expect: " prgms/calc.fblc | sed -e "s/\\/\\/\\/ Expect: //" > out/calc.wnt
exec diff out/calc.wnt out/calc.got

puts "test prgms/tictactoe.fblc TestBoardStatus"
exec $::fblc prgms/tictactoe.fblc TestBoardStatus > out/tictactoe.TestBoardStatus.got
exec echo "TestResult:Passed(Unit())" > out/tictactoe.TestBoardStatus.wnt
exec diff out/tictactoe.TestBoardStatus.wnt out/tictactoe.TestBoardStatus.got

puts "test prgms/tictactoe.fblc TestChooseBestMoveNoLose"
exec $::fblc prgms/tictactoe.fblc TestChooseBestMoveNoLose > out/tictactoe.TestChooseBestMoveNoLose.got
exec echo "PositionTestResult:Passed(Unit())" > out/tictactoe.TestChooseBestMoveNoLose.wnt
exec diff out/tictactoe.TestChooseBestMoveNoLose.wnt out/tictactoe.TestChooseBestMoveNoLose.got

puts "test prgms/tictactoe.fblc TestChooseBestMoveWin"
exec $::fblc prgms/tictactoe.fblc TestChooseBestMoveWin > out/tictactoe.TestChooseBestMoveWin.got
exec echo "PositionTestResult:Passed(Unit())" > out/tictactoe.TestChooseBestMoveWin.wnt
exec diff out/tictactoe.TestChooseBestMoveWin.wnt out/tictactoe.TestChooseBestMoveWin.got

puts "fblcbe tictactoe.fblc (NewGame is 33)"
exec $::fblcbe prgms/tictactoe.fblc > out/prgms/tictactoe.fblc.bin

check_coverage overall

# Report summary results
puts "Skipped Tests: $::skipped"
puts "fblc Coverage: "
puts "  Spec    : [exec tail -n 1 out/spectest/fblc.gcov]"
puts "  Overall : [exec tail -n 1 out/overall/fblc.gcov]"
puts "fblcbi Coverage: "
puts "  Spec    : [exec tail -n 1 out/spectest/fblcbi.gcov]"
puts "  Overall : [exec tail -n 1 out/overall/fblcbi.gcov]"

