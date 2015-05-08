
exec rm -rf out
exec mkdir -p out/test/malformed

set FLAGS [list -I . -std=c99 -pedantic -Wall -Werror -O0 -fprofile-arcs -ftest-coverage -gdwarf-3 -ggdb]

foreach {x} [glob fblc/*.c] {
  exec gcc {*}$FLAGS -c -o out/[string map {.c .o} [file tail $x]] $x
}
exec gcc {*}$FLAGS -o out/fblc -lgc {*}[glob out/*.o]

exec ./out/fblc test/basic.fblc > out/test/basic.got
exec diff out/test/basic.got test/basic.wnt

foreach {x} [glob test/malformed/*.fblc] {
  exec ./out/fblc --expect-error $x 2> out/$x.out
}

exec gcov {*}[glob out/*.o] > out/fblc.gcov
exec mv {*}[glob *.gcov] out

puts DONE

