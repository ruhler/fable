
exec rm -rf out
exec mkdir -p out/test/malformed

set FLAGS [list -I . -std=c99 -pedantic -Wall -Werror -O0 -fprofile-arcs -ftest-coverage -gdwarf-3 -ggdb]

foreach {x} [lsort [glob fblc/*.c]] {
  puts "cc $x"
  exec gcc {*}$FLAGS -c -o out/[string map {.c .o} [file tail $x]] $x
}
puts "ld -o out/fblc"
exec gcc {*}$FLAGS -o out/fblc -lgc {*}[glob out/*.o]

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

# Well formed tests:
foreach {x} [lsort [glob test/????v-*.fblc]] {
  puts "test $x"
  set fgot out/[string map {.fblc .got} [file tail $x]]
  set fwnt out/[string map {.fblc .wnt} [file tail $x]]
  exec ./out/fblc $x main > $fgot
  exec grep "/// Expect: " $x | sed -e "s/\\/\\/\\/ Expect: //" > $fwnt
  exec diff $fgot $fwnt
}

# Malformed tests:
foreach {x} [lsort [glob test/????e-*.fblc]] {
  puts "test $x"
  set fgot out/[string map {.fblc .got} [file tail $x]]
  expect_status 65 ./out/fblc $x main 2> $fgot
}

# Report how much code coverage we have from the spec tests.
exec gcov {*}[glob out/*.o] > out/fblc.spectest.gcov
exec mv {*}[glob *.gcov] out
puts [exec tail -n 1 out/fblc.spectest.gcov]

# Test fblc.
puts "test ./out/fblc"
expect_status 64 ./out/fblc

puts "test ./out/fblc --help"
expect_status 0 ./out/fblc --help

puts "test ./out/fblc no_such_file"
expect_status 66 ./out/fblc no_such_file main

puts "test ./out/fblc clock/clock.fblc incr 'Digit:1(Unit())'"
exec ./out/fblc clock/clock.fblc incr "Digit:1(Unit())" > out/clockincr.got
exec echo "Digit:2(Unit())" > out/clockincr.wnt
exec diff out/clockincr.wnt out/clockincr.got

# Test the calculator.
puts "test calc/calc.fblc"
exec ./out/fblc calc/calc.fblc main > out/calc.got
exec grep "/// Expect: " calc/calc.fblc | sed -e "s/\\/\\/\\/ Expect: //" > out/calc.wnt
exec diff out/calc.wnt out/calc.got

# Report the final test coverage stats.
exec gcov {*}[glob out/*.o] > out/fblc.gcov
exec mv {*}[glob *.gcov] out
puts [exec tail -n 1 out/fblc.gcov]

