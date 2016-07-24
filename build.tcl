
exec rm -rf out
exec mkdir -p out/test/malformed

set FLAGS [list -I . -std=c99 -pedantic -Wall -Werror -O0 -fprofile-arcs -ftest-coverage -gdwarf-3 -ggdb]

foreach {x} [lsort [glob fblc/*.c]] {
  puts "cc $x"
  exec gcc {*}$FLAGS -c -o out/[string map {.c .o} [file tail $x]] $x
}
puts "ld -o out/fblc"
exec gcc {*}$FLAGS -o out/fblc -lgc {*}[glob out/*.o]

# Well formed tests:
foreach {x} [lsort [glob test/????v-*.fblc]] {
  puts "test $x"
  set fgot out/[string map {.fblc .got} [file tail $x]]
  set fwnt out/[string map {.fblc .wnt} [file tail $x]]
  exec ./out/fblc $x > $fgot
  exec grep "/// Expect: " $x | sed -e "s/\\/\\/\\/ Expect: //" > $fwnt
  exec diff $fgot $fwnt
}

# Malformed tests:
foreach {x} [lsort [glob test/????e-*.fblc]] {
  puts "test $x"
  set fgot out/[string map {.fblc .got} [file tail $x]]
  try {
    exec ./out/fblc $x 2> $fgot
    error "Expected exit code 65, but got 0"
  } trap CHILDSTATUS {results options} {
    set status [lindex [dict get $options -errorcode] 2]
    if {$status != 65} {
      error "Expected exit code 65, but got $status"
    }
  }
}

# Report how much code coverage we have from the spec tests.
exec gcov {*}[glob out/*.o] > out/fblc.spectest.gcov
exec mv {*}[glob *.gcov] out
puts [exec tail -n 1 out/fblc.spectest.gcov]

# Test the calculator.
puts ""
puts "test calc/calc.fblc"
exec ./out/fblc calc/calc.fblc > out/calc.got
exec grep "/// Expect: " calc/calc.fblc | sed -e "s/\\/\\/\\/ Expect: //" > out/calc.wnt
exec diff out/calc.wnt out/calc.got

# Report the final test coverage stats.
exec gcov {*}[glob out/*.o] > out/fblc.gcov
exec mv {*}[glob *.gcov] out
puts [exec tail -n 1 out/fblc.gcov]

