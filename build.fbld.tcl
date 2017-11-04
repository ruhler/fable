
exec rm -rf out/fbld
set FLAGS [list -I fblc -I fbld -std=c99 -pedantic -Wall -Werror -O0 -fprofile-arcs -ftest-coverage -gdwarf-3 -ggdb] 

# Compile all object files.
# We compile these separately and ensure they are placed in a subdirectory of
# the out directory so that the profile information generated when running
# code from the objects is placed in a subdirectory of the out directory.
exec mkdir -p out/fbld/obj
foreach {x} [glob fbld/{fbld-check,fbld-snake,fbld-test,check,compile,load,loc,name,program}.c] {
  set obj out/fbld/obj/[string map {.c .o} [file tail $x]]
  run gcc {*}$FLAGS -c -o $obj $x
}

# Generate and compile the fbld parser.
exec mkdir -p out/fbld/src
run bison -o out/fbld/src/parse.tab.c fbld/parse.y 
run gcc {*}$FLAGS -c -o out/fbld/obj/parse.tab.o out/fbld/src/parse.tab.c

# Generate libfbld.a
set fbld_objs [list]
foreach {x} [list check compile load loc name parse.tab program] {
  lappend fbld_objs out/fbld/obj/$x.o
}
run ar rcs out/fbld/libfbld.a {*}$fbld_objs

# Compile the executables
set ::fbldcheck ./out/fbld/fbld-check
set ::fbldtest ./out/fbld/fbld-test
run gcc {*}$FLAGS -o $::fbldcheck out/fbld/obj/fbld-check.o -L out/fbld -lfbld -L out/fblc -lfblc
run gcc {*}$FLAGS -o $::fbldtest out/fbld/obj/fbld-test.o -L out/fbld -lfbld -L out/fblc -lfblc
run gcc {*}$FLAGS -o out/fbld/fbld-snake out/fbld/obj/fbld-snake.o -L out/fbld -lfbld -L out/fblc -lfblc -lncurses

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

  set errtext "no error reported"
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

foreach {x} [lsort [glob langs/fbld/*.tcl]]  {
  puts "test $x"
  source $x
}

exec mkdir -p out/fbld/cov/spec
run gcov {*}$::fbld_objs > out/fbld/cov/spec/fbld.gcov
exec mv {*}[glob *.gcov] out/fbld/cov/spec

skip run $::fbldtest prgms/AllTests.wnt prgms "Test@AllTests"
skip run $::fbldcheck prgms/ Snake

exec mkdir -p out/fbld/cov/all
run gcov {*}$::fbld_objs > out/fbld/cov/all/fbld.gcov
exec mv {*}[glob *.gcov] out/fbld/cov/all
