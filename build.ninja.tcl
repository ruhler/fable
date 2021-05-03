# ninja-based description of how to build fble and friends.
#
# First time setup:
#   mkdir -p out
#   tclsh build.ninja.tcl > out/build.ninja
#
# Afterwards:
#   ninja -f out/build.ninja

# Output directories used for build.
set ::out "out"
set ::bin "$::out/bin"
set ::obj "$::out/obj"
set ::lib "$::out/lib"
set ::src "$::out/src"
set ::prgms "$::out/prgms"
set ::test "$::out/test"
set ::cov "$::out/cov"

# build.ninja header.
puts "builddir = $::out"
puts {
# We build everything using this one generic rule, specifying the command
# explicitly for every target. Because defining separate rules for different
# command lines doesn't seem to buy us much.
rule rule
  description = $out
  command = $cmd
}

# build --
#   Builds generic targets.
#
# Inputs:
#   targets - the list of targets produced by the command.
#   dependencies - the list of targets this depends on.
#   command - the command to run to produce the targets.
#   args - optional additional "key = argument" pairs to use for ninja rule.
proc build { targets dependencies command args } {
  puts "build [join $targets]: rule [join $dependencies]"
  puts "  cmd = $command"

  foreach kv $args {
    puts "  $kv"
  }
}

# obj --
#   Builds a .o file.
#
# Inputs:
#   obj - the .o file to build (include $::obj directory).
#   src - the .c file to build the .o file from.
#   iflags - include flags, e.g. "-I foo".
#   args - optional additional dependencies.
proc obj { obj src iflags args } {
  set cflags "-std=c99 -pedantic -Wall -Werror -gdwarf-3 -ggdb"
  set cmd "gcc -MMD -MF $obj.d $cflags $iflags -c -o $obj $src"
  build $obj "$src $args" $cmd "depfile = $obj.d"
}

# obj_cov --
#   Builds a .o file with test coverage enabled.
#
# Inputs:
#   obj - the .o file to build (include $::obj directory).
#   src - the .c file to build the .o file from.
#   iflags - include flags, e.g. "-I foo".
#   args - optional additional dependencies.
proc obj_cov { obj src iflags args } {
  set gcda $::obj/[string map {.o .gcda} $obj]
  set cflags "-std=c99 -pedantic -Wall -Werror -gdwarf-3 -ggdb -fprofile-arcs -ftest-coverage"
  set cmd "rm -f $gcda ; gcc -MMD -MF $obj.d $cflags $iflags -c -o $obj $src"
  build $obj "$src $args" $cmd "depfile = $obj.d"
}

# tobj --
#   Builds a .o file using tcc.
#
# Used for compiling large generated .c files, where tcc has much nicer memory
# requirements than gcc does. 
#
# Inputs:
#   obj - the .o file to build (include $::obj directory).
#   src - the .c file to build the .o file from.
#   iflags - include flags, e.g. "-I foo".
#   args - optional additional dependencies.
proc tobj { obj src iflags args } {
  #set cflags "-std=c99 -pedantic -gdwarf-3 -ggdb -fprofile-arcs -ftest-coverage -pg"
  #set cflags "-std=c99 -pedantic -gdwarf-3 -ggdb"
  #set cmd "gcc -MD -MF $obj.d $cflags $iflags -c -o $obj $src"
  #set cflags "-pedantic -g -I /usr/include"
  #set cmd "clang -MD -MF $obj.d $cflags $iflags -c -o $obj $src"
  set cflags "-pedantic -Wall -Werror -g -I /usr/include"
  set cmd "tcc -MD -MF $obj.d $cflags $iflags -c -o $obj $src"
  build $obj "$src $args" $cmd "depfile = $obj.d"
}

# lib --
#   Build a library.
#
# Inputs:
#   lib - the library file to build
#   objs - the list of .o files to include in the library.
proc lib { lib objs } {
  build $lib $objs "rm -f $lib ; ar rcs $lib $objs"
}

# bin --
#   Build a binary.
#
# Inputs:
#   bin - the binary file to build (include $::bin directory).
#   objs - the list of .o files to build from.
#   lflags - library flags, e.g. "-L foo/ -lfoo".
#   args - optional additional dependencies.
proc bin { bin objs lflags args } {
  #set cflags "-std=c99 -pedantic -Wall -Werror -gdwarf-3 -ggdb -fprofile-arcs -ftest-coverage -pg"
  set cflags "-std=c99 -pedantic -Wall -Werror -gdwarf-3 -ggdb"
  build $bin "$objs $args" "gcc $cflags -o $bin $objs $lflags"
}

# bin_cov --
#   Build a binary with test coverage enabled.
#
# Inputs:
#   bin - the binary file to build (include $::bin directory).
#   objs - the list of .o files to build from.
#   lflags - library flags, e.g. "-L foo/ -lfoo".
#   args - optional additional dependencies.
proc bin_cov { bin objs lflags args } {
  set cflags "-std=c99 -pedantic -Wall -Werror -gdwarf-3 -ggdb -fprofile-arcs -ftest-coverage"
  build $bin "$objs $args" "gcc $cflags -o $bin $objs $lflags"
}

# test --
#   Build a test result.
#
# Inputs:
#   tr - the .tr file to output the results to.
#   deps - depencies for the test.
#   cmd - the test command to run, which should exit 0 to indicate the test
#         passed and non-zero to indicate the test failed.
#   args - optional additional "key = argument" pairs to use for ninja rule.
#
# Adds the .tr file to global list of tests.
set ::tests [list]
proc test { tr deps cmd args} {
  lappend ::tests $tr
  build $tr $deps "$cmd && echo PASSED > $tr" {*}$args
}

# Any time we run glob over a directory, add that directory to this list.
# We need to make sure to include these directories as a dependency on the
# generation of build.ninja.
set ::build_ninja_deps [list]

# .o files used to implement libfble.a
set ::fble_objs [list]
set ::fble_objs_cov [list]
lappend ::build_ninja_deps "fble/src"
foreach {x} [glob -tails -directory fble/src *.c] {
  set object $::obj/[string map {.c .o} $x]
  set object_cov $::obj/[string map {.c .cov.o} $x]
  obj $object fble/src/$x "-I fble/include -I fble/src"
  obj_cov $object_cov fble/src/$x "-I fble/include -I fble/src"
  lappend ::fble_objs $object
  lappend ::fble_objs_cov $object_cov
}

# parser
eval {
  # Update local includes for fble/src/parse.y here.
  # See comment in fble/src/parse.y.
  set includes {
    fble/include/fble-alloc.h
    fble/include/fble-load.h
    fble/include/fble-loc.h
    fble/include/fble-module-path.h
    fble/include/fble-name.h
    fble/include/fble-string.h
    fble/include/fble-vector.h
    fble/src/expr.h
  }
  set report $::src/parse.tab.report.txt
  set tabc $src/parse.tab.c
  set cmd "bison --report=all --report-file=$report -o $tabc fble/src/parse.y"
  build "$tabc $report" "fble/src/parse.y $includes" $cmd

  obj $::obj/parse.tab.o $src/parse.tab.c "-I fble/include -I fble/src"
  obj_cov $::obj/parse.tab.cov.o $src/parse.tab.c "-I fble/include -I fble/src"
  lappend ::fble_objs $::obj/parse.tab.o
  lappend ::fble_objs_cov $::obj/parse.tab.cov.o
}

# libfble.a
set ::libfble "$::lib/libfble.a"
lib $::libfble $::fble_objs

set ::libfblecov "$::lib/libfble.cov.a"
lib $::libfblecov $::fble_objs_cov

# fble tool binaries
lappend build_ninja_deps "tools"
foreach {x} [glob tools/*.c prgms/fble-md5.c prgms/fble-stdio.c] {
  set base [file rootname [file tail $x]]
  obj $::obj/$base.o $x "-I fble/include"
  bin $::bin/$base $::obj/$base.o "-L $::lib -lfble" $::libfble
  bin_cov $::bin/$base.cov $::obj/$base.o "-L $::lib -lfble.cov" $::libfblecov
}
obj $::obj/fble-app.o prgms/fble-app.c "-I fble/include -I /usr/include/SDL2"
bin $::bin/fble-app $::obj/fble-app.o "-L $::lib -lfble -lSDL2" $::libfble

# tests
test $::test/true.tr "" true
#test $::test/false.tr "" false

# Returns the list of all subdirectories, recursively, of the given directory.
# The 'root' directory will not be included as a prefix in the returned list
# of directories.
# 'dir' should be empty or end with '/'.
proc dirs { root dir } {
  set l [list $dir]
  foreach {x} [glob -tails -directory $root -nocomplain -type d $dir*] {
    set l [concat $l [dirs $root "$x/"]]
  }
  return $l
}

# fble language spec tests
# 
# The code for running the spec tests is split up a little bit awkwardly
# because we want to reuse ninja to build intermediate artifacts used in the
# test. The intermediate artifacts we need to build depend on the contents of
# the test specification. We split it up as follows:
# * build.ninja.tcl (this file) - reads the spec tests and uses it to generate
#   build rules, because apparently ninja needs all the build rules to show up
#   in build.ninja from the beginning. Does as little else as possible because
#   build.ninja.tcl is rerun for lots of reasons. In particular, does not
#   extract the .fble files from the test.
# * tools/extract-spec-test.tcl - extracts the .fble files from a particular
#   test. This is run at build time so we avoid re-extracting the .fble files
#   repeatedly if the test specification hasn't changed.
# * tools/run-spec-test.tcl - a helper function to execute a spec test that
#   takes the command to run as input. The primary purpose of this is to
#   encapsulate an otherwise complex command line to run the test and give us
#   nice test failure messages. It reads the test spec for the test to run,
#   but only to know what error location, if any, is expected for
#   fble-test-error tests.
set ::spec_tests [list]
foreach dir [dirs langs/fble ""] {
  lappend build_ninja_deps "langs/fble/$dir"
  foreach {t} [lsort [glob -tails -directory langs/fble -nocomplain -type f $dir/*.tcl]] {
    set ::specroot [file rootname $t]
    set ::spectestdir $::test/$specroot
    set ::spectcl langs/fble/$t
    lappend build_ninja_deps $::spectcl

    # Returns the list of .fble files for modules used in the test, not
    # including the top level .fble file.
    proc collect_modules { dir modules } {
      set fbles [list]
      foreach m $modules {
        set name [lindex $m 0]
        set submodules [lrange $m 2 end]
        lappend fbles $dir/$name.fble
        lappend fbles {*}[collect_modules $dir/$name $submodules]
      }
      return $fbles
    }

    # Emit build rules to compile all the .fble files for the test to .c
    # files.
    # TODO: Make sure we only compile modules that can be reached from the
    # test.fble file.
    # TODO: Compile the generated c files and link those together too.
    proc spec-test-compile { modules } {
      set fbles [collect_modules "" $modules]
      lappend fbles "/test.fble"

      foreach x $fbles {
        set path "[string map {* {}} [file rootname $x]]%"
        set fble $::spectestdir$x
        set c [string map {.fble .c} $fble]
        build $c "$::bin/fble-compile $fble" \
        "$::bin/fble-compile $path $fble $::spectestdir > $c"
      }
    }

    proc spec-test-extract {} {
      build $::spectestdir/test.fble \
        "tools/extract-spec-test.tcl $::spectcl langs/fble/Nat.fble" \
        "tclsh tools/extract-spec-test.tcl $::spectcl $::spectestdir langs/fble/Nat.fble"
      lappend ::spec_tests $::spectestdir/test.tr
    }

    proc fble-test { expr args } {
      spec-test-extract
      #spec-test-compile $args

      # We run with --profile to get better test coverage for profiling.
      test $::spectestdir/test.tr "tools/run-spec-test.tcl $::bin/fble-test.cov $::spectestdir/test.fble" \
        "tclsh tools/run-spec-test.tcl $::spectcl $::bin/fble-test.cov --profile $::spectestdir/test.fble $::spectestdir"
    }

    proc fble-test-error { loc expr args } {
      spec-test-extract
      test $::spectestdir/test.tr "tools/run-spec-test.tcl $::bin/fble-test.cov $::spectestdir/test.fble" \
        "tclsh tools/run-spec-test.tcl $::spectcl $::bin/fble-test.cov --error $::spectestdir/test.fble $::spectestdir"
    }

    proc fble-test-memory-constant { expr } {
      spec-test-extract
      #spec-test-compile {}
      test $::spectestdir/test.tr "tools/run-spec-test.tcl $::bin/fble-mem-test.cov $::spectestdir/test.fble" \
        "tclsh tools/run-spec-test.tcl $::spectcl $::bin/fble-mem-test.cov $::spectestdir/test.fble $::spectestdir"
    }

    proc fble-test-memory-growth { expr } {
      spec-test-extract
      #spec-test-compile {}
      test $::spectestdir/test.tr "tools/run-spec-test.tcl $::bin/fble-mem-test.cov $::spectestdir/test.fble" \
        "tclsh tools/run-spec-test.tcl $::spectcl $::bin/fble-mem-test.cov --growth $::spectestdir/test.fble $::spectestdir"
    }
    source $::spectcl
  }
}

# Code coverage from spec tests.
build $::cov/gcov.txt "$::fble_objs_cov $::spec_tests" \
  "gcov $::fble_objs_cov > $::cov/gcov.txt && mv *.gcov $::cov"

# fble-profile-test
test $::test/fble-profile-test.tr $::bin/fble-profile-test \
  "$::bin/fble-profile-test > /dev/null"

# fble-profiles-test
test $::test/fble-profiles-test.tr \
  "$::bin/fble-profiles-test prgms/Fble/ProfilesTest.fble" \
  "$::bin/fble-profiles-test prgms/Fble/ProfilesTest.fble > $::test/fble-profiles-test.prof"

# Compile each .fble module in the prgms directory.
set ::fble_prgms_objs [list]
foreach dir [dirs prgms ""] {
  lappend build_ninja_deps "prgms/$dir"
  foreach {x} [glob -tails -directory prgms -nocomplain -type f $dir/*.fble] {
    # Generate a .d file to capture dependencies.
    build $::prgms/$x.d "$::bin/fble-deps prgms/$x" \
      "$::bin/fble-deps $::prgms/$x.d prgms/$x prgms > $::prgms/$x.d" \
      "depfile = $::prgms/$x.d"

    # Generate a .c file.
    set path "/[file rootname $x]%"
    build $::prgms/$x.c \
      "$::bin/fble-compile $::prgms/$x.d" \
      "$::bin/fble-compile $path prgms/$x prgms > $::prgms/$x.c"

    # Generate a .o file.
    tobj $::prgms/$x.o $::prgms/$x.c "-I fble/include -I fble/src"
    lappend ::fble_prgms_objs $::prgms/$x.o
  }
}

# libfbleprgms.a
set ::libfbleprgms "$::lib/libfbleprgms.a"
lib $::libfbleprgms $::fble_prgms_objs

# fble-disassemble test
test $::test/fble-disassemble.tr \
  "$::bin/fble-disassemble $::prgms/Fble/Tests.fble.d" \
  "$::bin/fble-disassemble prgms/Fble/Tests.fble prgms > $::prgms/Fble/Tests.fble.s"

# Fble/Tests.fble tests
test $::test/fble-tests.tr "$::bin/fble-stdio $::prgms/Fble/Tests.fble.d" \
  "$::bin/fble-stdio prgms/Fble/Tests.fble prgms" "pool = console"

# fble-md5 test
test $::test/fble-md5.tr "$::bin/fble-md5 $::prgms/Md5/Main.fble.d" \
  "$::bin/fble-md5 prgms/Md5/Main.fble prgms /dev/null > $::test/fble-md5.out && grep d41d8cd98f00b204e9800998ecf8427e $::test/fble-md5.out > /dev/null"

# fble-cat test
test $::test/fble-cat.tr "$::bin/fble-stdio $::prgms/Stdio/Cat.fble.d" \
  "$::bin/fble-stdio prgms/Stdio/Cat.fble prgms < README.txt > $::test/fble-cat.out && cmp $::test/fble-cat.out README.txt"

# fble-stdio test
test $::test/fble-stdio.tr "$::bin/fble-stdio $::prgms/Stdio/Test.fble.d" \
  "$::bin/fble-stdio prgms/Stdio/Test.fble prgms > $::test/fble-stdio.out && grep PASSED $::test/fble-stdio.out > /dev/null"

# /Stdio/Test% compilation test
obj $::obj/fble-compiled-stdio.o prgms/fble-stdio.c "-DFbleCompiledMain=FbleStdioMain -I fble/include"
build $::src/fble-stdio-test.c $::bin/fble-compile \
  "$::bin/fble-compile /Stdio/Test% --export FbleStdioMain > $::src/fble-stdio-test.c"
obj $::obj/fble-stdio-test.o $::src/fble-stdio-test.c "-I fble/include -I fble/src"
bin $::bin/fble-stdio-test \
  "$::obj/fble-stdio-test.o $::obj/fble-compiled-stdio.o" \
  "-L $::lib -lfble -lfbleprgms" "$::libfble $::libfbleprgms"
test $::test/fble-stdio-test.tr $::bin/fble-stdio-test \
  "$::bin/fble-stdio-test > $::test/fble-stdio-test.out && grep PASSED $::test/fble-stdio-test.out > /dev/null"

# /Fble/Tests% compilation test
build $::src/fble-tests.c $::bin/fble-compile \
  "$::bin/fble-compile /Fble/Tests% --export FbleStdioMain > $::src/fble-tests.c"
obj $::obj/fble-tests.o $::src/fble-tests.c "-I fble/include -I fble/src"
bin $::bin/fble-tests \
  "$::obj/fble-tests.o $::obj/fble-compiled-stdio.o" \
  "-L $::lib -lfble -lfbleprgms" "$::libfble $::libfbleprgms"
test $::test/fble-compiled-tests.tr $::bin/fble-tests \
  "$::bin/fble-tests" "pool = console"

# /Fble/Bench% compiled binary
build $::src/fble-bench.c $::bin/fble-compile \
  "$::bin/fble-compile /Fble/Bench% --export FbleStdioMain > $::src/fble-bench.c"
obj $::obj/fble-bench.o $::src/fble-bench.c "-I fble/include -I fble/src"
bin $::bin/fble-bench \
  "$::obj/fble-bench.o $::obj/fble-compiled-stdio.o" \
  "-L $::lib -lfble -lfbleprgms" "$::libfble $::libfbleprgms"

# fblf compilation test
obj $::obj/fblf-heap.o prgms/Fblf/fblf-heap.c "-I prgms/Fblf"
build $::src/fblf-tests.c \
  "$::bin/fble-stdio $::prgms/Fblf/Lib/Tests/Compile.fble.d" \
  "$::bin/fble-stdio prgms/Fblf/Lib/Tests/Compile.fble prgms > $::src/fblf-tests.c"
obj $::obj/fblf-tests.o $::src/fblf-tests.c "-I prgms/Fblf"
bin $::bin/fblf-tests "$::obj/fblf-tests.o $::obj/fblf-heap.o" ""
test $::test/fblf-tests.tr $::bin/fblf-tests $::bin/fblf-tests

# fblf md5 compilation test
build $::src/fblf-md5.c \
  "$::bin/fble-stdio $::prgms/Fblf/Lib/Md5/Stdio.fble.d" \
  "$::bin/fble-stdio prgms/Fblf/Lib/Md5/Stdio.fble prgms > $::src/fblf-md5.c"
obj $::obj/fblf-md5.o $::src/fblf-md5.c "-I prgms/Fblf"
bin $::bin/fblf-md5 "$::obj/fblf-md5.o $::obj/fblf-heap.o" ""
test $::test/fblf-md5.tr $::bin/fblf-md5 \
  "$::bin/fblf-md5 /dev/null > $::test/fblf-md5.out && grep d41d8cd98f00b204e9800998ecf8427e $::test/fblf-md5.out > /dev/null"

# test summary
build $::test/tests.txt "$::tests" "echo $::tests > $::test/tests.txt"
build $::test/summary.tr "tools/tests.tcl $::test/tests.txt" \
  "tclsh tools/tests.tcl $::test/tests.txt && echo PASSED > $::test/summary.tr"

# build.ninja
build $::out/build.ninja "build.ninja.tcl $::build_ninja_deps" \
  "tclsh build.ninja.tcl > $::out/build.ninja"

# TODO: Add a profiling variant of things?
#
# For profiling:
# * add to gcc flags: -fprofile-arcs -ftest-coverage -pg to obj and bin rules.
# * remove all the generated .gcda files to start fresh.
# * run benchmark as desired.
# * run gprof passing the benchmark executable run on the command line.
# * run gcov out/obj/*.o and move *.gcov files where preferred.
# * remove gmon.out
