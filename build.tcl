
exec rm -rf out
exec mkdir -p out/test/malformed out/fblc out/prgms

set FLAGS [list -I . -I fblc -std=c99 -pedantic -Wall -Werror -O0 -fprofile-arcs -ftest-coverage -gdwarf-3 -ggdb]

foreach {x} [lsort [glob fblc/*.c]] {
  puts "cc $x"
  exec gcc {*}$FLAGS -c -o out/fblc/[string map {.c .o} [file tail $x]] $x
}

foreach {x} [lsort [glob prgms/*.c]] {
  puts "cc $x"
  set obj out/prgms/[string map {.c .o} [file tail $x]]
  exec gcc {*}$FLAGS -c -o $obj $x

  set exe out/prgms/[string map {.c ""} [file tail $x]]
  puts "ld -o $exe"
  exec gcc {*}$FLAGS -o $exe -lgc $obj {*}[glob out/fblc/*.o]
}

set ::fblc ./out/prgms/fblc
exec gcc -std=c99 -Wall -Werror -o out/proc_test_driver test/proc_test_driver.c

proc check_coverage {name} {
  exec mkdir -p out/$name
  exec gcov {*}[glob out/fblc/*.o out/prgms/fblc.o] > out/$name/fblc.gcov
  exec mv {*}[glob *.gcov] out/$name
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

  # Set up the ports.
  set port_files [list]
  set port_specs [list]
  foreach {type id} [join $ports] {
    set portfile ./out/$name.$id
    exec rm -f $portfile
    exec mkfifo $portfile
    lappend port_files $portfile
    lappend port_specs $type:$id:$portfile
  }

  exec rm -f ./out/$name.script
  foreach cmd [split [string trim $script] "\n"] {
    exec echo [string trim $cmd] >> ./out/$name.script
  }
  exec echo $program > ./out/$name.fblc
  exec sh -c "./out/proc_test_driver $port_specs < ./out/$name.script > ./out/$name.script.out 2> ./out/$name.script.err" &

  try {
    set got [exec $::fblc ./out/$name.fblc $entry {*}$port_files {*}$args]
    if {$got != $result} {
      error "$file:$line: error: Expected '$result', but got '$got'"
    }
  } trap CHILDSTATUS {results options} {
    error "$file:$line: error: Expected '$result', but got:\n$results"
  }

  set got [read [open ./out/$name.script.err]]
  if {$got != ""} {
    error "$file:$line: error: $got"
  }
}

# Test that running function or process 'entry' in 'program' with the given
# 'args' and no ports leads to an error indicating the program is malformed.
proc expect_malformed { program entry args } {
  set loc [info frame -1]
  set line [dict get $loc line]
  set file [dict get $loc file]
  set name "[file tail $file]_$line"

  try {
    exec echo $program > ./out/$name.fblc
    set got [exec $::fblc ./out/$name.fblc $entry {*}$args]
    error "$file:$line: error: Expected error, but got '$got'"
  } trap CHILDSTATUS {results options} {
    exec echo $results > ./out/$name.err
    set status [lindex [dict get $options -errorcode] 2]
    if {65 != $status} {
      error "$file:$line: error: Expected error code 65, but got code '$status'"
    }
  }
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

# Old well formed tests:
foreach {x} [lsort [glob test/????v-*.fblc]] {
  puts "test $x"
  set fgot out/[string map {.fblc .got} [file tail $x]]
  set fwnt out/[string map {.fblc .wnt} [file tail $x]]
  exec $::fblc $x main > $fgot
  exec grep "/// Expect: " $x | sed -e "s/\\/\\/\\/ Expect: //" > $fwnt
  exec diff $fgot $fwnt
}

# Old malformed tests:
foreach {x} [lsort [glob test/????e-*.fblc]] {
  puts "test $x"
  set fgot out/[string map {.fblc .got} [file tail $x]]
  expect_status 65 $::fblc $x main 2> $fgot
}

check_coverage oldspec

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

check_coverage overall

# Report how much code coverage we have.
puts ""
puts "Coverage: "
puts "  Spec    : [exec tail -n 1 out/spectest/fblc.gcov]"
puts "  Old Spec: [exec tail -n 1 out/oldspec/fblc.gcov]"
puts "  Overall : [exec tail -n 1 out/overall/fblc.gcov]"

