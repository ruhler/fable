
exec rm -rf out
exec mkdir -p out/test/malformed

set FLAGS [list -I . -std=c99 -pedantic -Wall -Werror -O0 -fprofile-arcs -ftest-coverage -gdwarf-3 -ggdb]

foreach {x} [glob fblc/*.c] {
  exec gcc {*}$FLAGS -c -o out/[string map {.c .o} [file tail $x]] $x
}
exec gcc {*}$FLAGS -o out/fblc -lgc {*}[glob out/*.o]

# Well formed tests:
foreach {x} [glob test/???v-*.fblc] {
  set fgot out/[string map {.fblc .got} [file tail $x]]
  set fwnt out/[string map {.fblc .wnt} [file tail $x]]
  exec ./out/fblc $x > $fgot
  exec grep "/// Expect: " $x | sed -e "s/\\/\\/\\/ Expect: //" > $fwnt
  exec diff $fgot $fwnt
}

# Malformed tests:
foreach {x} [glob test/???e-*.fblc] {
  set fgot out/[string map {.fblc .got} [file tail $x]]
  exec ./out/fblc --expect-error $x 2> $fgot
}

exec gcov {*}[glob out/*.o] > out/fblc.gcov
exec mv {*}[glob *.gcov] out

puts [exec tail -n 1 out/fblc.gcov]

