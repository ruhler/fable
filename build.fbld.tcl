
exec rm -rf out/fbld
set FLAGS [list -I fblc -I fbld -std=c99 -pedantic -Wall -Werror -O0 -fprofile-arcs -ftest-coverage -gdwarf-3 -ggdb] 

# Compile all object files.
# We compile these separately and ensure they are placed in a subdirectory of
# the out directory so that the profile information generated when running
# code from the objects is placed in a subdirectory of the out directory.
exec mkdir -p out/fbld/obj
foreach {x} [glob fbld/{fbld-check,fbld-md5,fbld-snake,fbld-test,check,compile,load,loc,name,program}.c] {
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
set ::fbldmd5 ./out/fbld/fbld-md5
run gcc {*}$FLAGS -o $::fbldcheck out/fbld/obj/fbld-check.o -L out/fbld -lfbld -L out/fblc -lfblc
run gcc {*}$FLAGS -o $::fbldtest out/fbld/obj/fbld-test.o -L out/fbld -lfbld -L out/fblc -lfblc
run gcc {*}$FLAGS -o out/fbld/fbld-md5 out/fbld/obj/fbld-md5.o -L out/fbld -lfbld -L out/fblc -lfblc
run gcc {*}$FLAGS -o out/fbld/fbld-snake out/fbld/obj/fbld-snake.o -L out/fbld -lfbld -L out/fblc -lfblc -lncurses

proc fbld-check-error-internal { program loc testloc} {
  set line [dict get $testloc line]
  set file [dict get $testloc file]
  set name "[file tail $file]_$line"

  exec mkdir -p out/test/fbld
  set fprogram ./out/test/fbld/$name.fbld
  exec echo $program > $fprogram

  set errtext "no error reported"
  try {
    set errtext [exec $::fbldcheck --error $fprogram]
  } on error {results options} {
    error "$file:$line: error: fbld-check failed: $results"
  }

  exec echo $errtext > ./out/test/fbld/$name.err
  if {-1 == [string first ":$loc: error" $errtext]} {
    error "$file:$line: error: Expected error at $loc, but got:\n$errtext"
  }
}

# See langs/fblc/README.txt for the description of this function
proc fblc-check-error { program loc } {
  fbld-check-error-internal $program $loc [info frame -1]
}

# See langs/fbld/README.txt for the description of this function
proc fbld-check-error { program loc } {
  fbld-check-error-internal $program $loc [info frame -1]
}

proc fbld-test-internal { program entry args script loc} {
  set line [dict get $loc line]
  set file [dict get $loc file]
  set name "[file tail $file]_$line"

  # Write the script to file.
  exec mkdir -p out/test/fbld
  set fscript ./out/test/fbld/$name.script
  exec rm -f $fscript
  exec touch $fscript
  foreach cmd [split [string trim $script] "\n"] {
    exec echo [string trim $cmd] >> $fscript
  }

  # Write the program to file.
  set fprogram ./out/test/fbld/$name.fbld
  exec echo $program > $fprogram

  try {
    exec $::fbldtest $fscript $fprogram $entry {*}$args
  } on error {results options} {
    error "$file:$line: error: \n$results"
  }
}

# See langs/fblc/README.txt for the description of this function.
proc fblc-test { program entry args script } {
  fbld-test-internal $program $entry $args $script [info frame -1]
}

# See langs/fbld/README.txt for the description of this function.
proc fbld-test { program entry args script } {
  fbld-test-internal $program $entry $args $script [info frame -1]
}

foreach {x} [lsort [glob langs/fblc/*.tcl]]  {
  puts "test $x"
  source $x
}

foreach {x} [lsort [glob langs/fbld/*.tcl]]  {
  puts "test $x"
  source $x
}

exec mkdir -p out/fbld/cov/spec
run gcov {*}$::fbld_objs > out/fbld/cov/spec/fbld.gcov
exec mv {*}[glob *.gcov] out/fbld/cov/spec

run $::fbldtest prgms/AllTests.wnt prgms/ Test@AllTests
run $::fbldcheck prgms/ Snake
run $::fbldmd5 prgms/ Md5@Md5@Md5 /dev/null | grep d41d8cd98f00b204e9800998ecf8427e

exec mkdir -p out/fbld/cov/all
run gcov {*}$::fbld_objs > out/fbld/cov/all/fbld.gcov
exec mv {*}[glob *.gcov] out/fbld/cov/all
