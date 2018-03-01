
exec rm -rf out
set FLAGS [list -I fblc -std=c99 -pedantic -Wall -Werror -O0 -fprofile-arcs -ftest-coverage -gdwarf-3 -ggdb] 

# Compile all object files.
# We compile these separately and ensure they are placed in a subdirectory of
# the out directory so that the profile information generated when running
# code from the objects is placed in a subdirectory of the out directory.
exec mkdir -p out/fblc/obj
foreach {x} [glob fblc/*.c] {
  set obj out/fblc/obj/[string map {.c .o} [file tail $x]]
  run gcc {*}$FLAGS -c -o $obj $x
}

# Generate libfblc.a
set fblc_objs [list]
foreach {x} [list check compile exec fblcs load malloc parse value vector] {
  lappend fblc_objs out/fblc/obj/$x.o
}
run ar rcs out/fblc/libfblc.a {*}$fblc_objs

# Compile the executables
set ::fblc ./out/fblc/fblc
set ::fblccheck ./out/fblc/fblc-check
set ::fblctest ./out/fblc/fblc-test
run gcc {*}$FLAGS -o $::fblc out/fblc/obj/fblc.o -L out/fblc -lfblc
run gcc {*}$FLAGS -o $::fblccheck out/fblc/obj/fblc-check.o -L out/fblc -lfblc
run gcc {*}$FLAGS -o $::fblctest out/fblc/obj/fblc-test.o -L out/fblc -lfblc
run gcc {*}$FLAGS -o out/fblc/fblc-snake out/fblc/obj/fblc-snake.o -L out/fblc -lfblc -lncurses
run gcc {*}$FLAGS -o out/fblc/fblc-tictactoe out/fblc/obj/fblc-tictactoe.o -L out/fblc -lfblc

# See langs/fblc/README.txt for the description of this function
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
    fail $name "$file:$line: error: fblc-check passed unexpectedly: $results"
    return
  }
  exec echo $errtext > ./out/test/fblc/$name.err
  if {-1 == [string first ":$loc: error" $errtext]} {
    fail $name "$file:$line: error: Expected error at $loc, but got:\n$errtext"
  }
}

# See langs/fblc/README.txt for the description of this function
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
    fail $name "$file:$line: error: \n$results"
  }
}

foreach {x} [lsort [glob langs/fblc/*.tcl]]  {
  puts "test $x"
  source $x
}

exec mkdir -p out/fblc/cov/spec
run gcov {*}$::fblc_objs > out/fblc/cov/spec/fblc.gcov
exec mv {*}[glob *.gcov] out/fblc/cov/spec

run $::fblctest prgms/clockincr.wnt prgms/clock.fblc incr "Digit:1(Unit())"
run $::fblctest prgms/calc.wnt prgms/calc.fblc main
run $::fblctest prgms/tictactoe.TestBoardStatus.wnt prgms/tictactoe.fblc TestBoardStatus
run $::fblctest prgms/tictactoe.TestChooseBestMoveNoLose.wnt prgms/tictactoe.fblc TestChooseBestMoveNoLose
run $::fblctest prgms/tictactoe.TestChooseBestMoveWin.wnt prgms/tictactoe.fblc TestChooseBestMoveWin
run $::fblctest prgms/boolcalc.wnt prgms/boolcalc.fblc Test
run $::fblctest prgms/ints.wnt prgms/ints.fblc Test
run $::fblccheck prgms/snake.fblc

exec mkdir -p out/fblc/cov/all
run gcov {*}$::fblc_objs > out/fblc/cov/all/fblc.gcov
exec mv {*}[glob *.gcov] out/fblc/cov/all
