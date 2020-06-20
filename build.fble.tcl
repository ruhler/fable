
exec rm -rf out

# Note: to profile, add -pg flag here, then after running fble-test, run:
#  gprof out/bin/fble-test
set ::FLAGS [list -std=c99 -pedantic -Wall -Werror -O0 -gdwarf-3 -ggdb -fprofile-arcs -ftest-coverage -I/usr/include/SDL2]

# Compile source for libfble.a
proc gcc_fble {args} {
  puts "gcc $args"
  exec gcc {*}$::FLAGS -I fble {*}$args
}

# Compile source for executables linking to libfble.a
proc gcc_prgm {args} {
  puts "gcc $args"
  exec gcc {*}$::FLAGS -I out/include -L out/lib {*}$args
}

# Compile all object files.
# We compile these separately and ensure they are placed in a subdirectory of
# the out directory so that the profile information generated when running
# code from the objects is placed in a subdirectory of the out directory.
set ::obj out/obj
exec mkdir -p $::obj
set fble_objs [list]
foreach {x} [glob fble/*.c] {
  set object $::obj/[string map {.c .o} [file tail $x]]
  lappend fble_objs $object
  gcc_fble -c -o $object $x
}

# Generate and compile the fble parser.
exec mkdir -p out/src
run bison --report=all --report-file=out/src/parse.tab.report.txt -o out/src/parse.tab.c fble/parse.y 
gcc_fble -c -o $::obj/parse.tab.o out/src/parse.tab.c
lappend fble_objs $::obj/parse.tab.o

# Generate libfble.a
exec mkdir -p out/lib out/include
run ar rcs out/lib/libfble.a {*}$fble_objs
exec cp {*}[glob fble/fble*.h] out/include

# Compile the remaining executables
foreach {x} [glob test/*.c prgms/*.c] {
  set object $::obj/[string map {.c .o} [file tail $x]]
  gcc_prgm -c -o $object $x
}

set ::bin out/bin
exec mkdir -p $::bin
gcc_prgm -o $::bin/fble-disassemble $::obj/fble-disassemble.o -lfble
gcc_prgm -o $::bin/fble-test $::obj/fble-test.o -lfble
gcc_prgm -o $::bin/fble-mem-test $::obj/fble-mem-test.o -lfble
gcc_prgm -o $::bin/fble-profile-test $::obj/fble-profile-test.o -lfble
gcc_prgm -o $::bin/fble-app $::obj/fble-app.o -lfble -lSDL2
gcc_prgm -o $::bin/fble-md5 $::obj/fble-md5.o -lfble
gcc_prgm -o $::bin/fble-stdio $::obj/fble-stdio.o -lfble

proc write_modules { dir modules } {
  foreach m $modules {
    set name [lindex $m 0]
    set value [lindex $m 1]
    set submodules [lrange $m 2 end]
    exec echo $value > $dir/$name.fble
    exec mkdir -p $dir/$name
    write_modules $dir/$name $submodules
  }
}

proc fble-test-error-run { dir loc expr modules } {
  exec mkdir -p $dir
  set fprgm $dir/_.fble
  exec echo $expr > $fprgm
  write_modules $dir $modules

  set errtext [exec $::bin/fble-test --error $fprgm $dir]
  exec echo $errtext > $dir/_.err
  if {-1 == [string first ":$loc: error" $errtext]} {
    error "Expected error at $loc, but got:\n$errtext"
  }
}

proc fble-test-run { dir cmd expr modules } {
  # Write the program to file.
  exec mkdir -p $dir
  set fprgm $dir/_.fble
  exec echo $expr > $fprgm
  exec ln langs/fble/Nat.fble $dir/Nat.fble
  write_modules $dir $modules

  # Execute the program.
  exec {*}$cmd $fprgm $dir
}

# See langs/fble/README.txt for the description of this function
proc fble-test { expr args } {
  set tloc [info frame -1]
  set line [dict get $tloc line]
  set file [dict get $tloc file]

  # Run test without profiling enabled
  set dir "out/[relative_file $file]:$line"
  testln $tloc [relative_file $file] fble-test-run $dir "$::bin/fble-test" $expr $args

  # Run test with profiling enabled
  set dir "out/[relative_file $file]-profile:$line"
  testln $tloc "[relative_file $file]-profile" fble-test-run $dir "$::bin/fble-test --profile" $expr $args
}

# See langs/fble/README.txt for the description of this function
proc fble-test-error { loc expr args } {
  set tloc [info frame -1]
  set line [dict get $tloc line]
  set file [dict get $tloc file]
  set dir "out/[relative_file $file]:$line"
  testln $tloc [relative_file $file] fble-test-error-run $dir $loc $expr $args
}

# See langs/fble/README.txt for the description of this function
proc fble-test-memory-constant { expr } {
  set tloc [info frame -1]
  set line [dict get $tloc line]
  set file [dict get $tloc file]
  set dir "out/[relative_file $file]:$line"
  testln $tloc [relative_file $file] fble-test-run $dir $::bin/fble-mem-test $expr {}
}

# See langs/fble/README.txt for the description of this function
proc fble-test-memory-growth { expr } {
  set tloc [info frame -1]
  set line [dict get $tloc line]
  set file [dict get $tloc file]
  set dir "out/[relative_file $file]:$line"
  testln $tloc [relative_file $file] fble-test-run $dir "$::bin/fble-mem-test --growth" $expr {}
}

# Source all *.tcl files under the given directory, recursively.
proc source_all { dir } {
  foreach {x} [lsort [glob -nocomplain -type d $dir/*]] {
    source_all $x
  }

  foreach {x} [lsort [glob -nocomplain -type f $dir/*.tcl]] {
    source $x
  }
}

source_all langs/fble

exec mkdir -p out/cov/spec
run -ignorestderr gcov {*}$::fble_objs > out/cov/spec/fble.gcov
exec mv {*}[glob *.gcov] out/cov/spec

exec mkdir -p out/test
testn fble-profile-test exec $::bin/fble-profile-test > out/test/fble-profile-test.txt
testn fble-disassemble exec $::bin/fble-disassemble prgms/Fble/Tests.fble prgms > out/test/FbleTests.fble.s
testn fble-tests exec $::bin/fble-stdio --profile out/test/fble-tests.prof prgms/Fble/Tests.fble prgms >@ stdout
testn fble-md5 exec $::bin/fble-md5 prgms/Md5/Main.fble prgms /dev/null
testn fble-cat exec $::bin/fble-stdio prgms/Stdio/Cat.fble prgms < README.txt | cmp README.txt -

exec mkdir -p out/cov/all
run -ignorestderr gcov {*}$::fble_objs > out/cov/all/fble.gcov
exec mv {*}[glob *.gcov] out/cov/all

