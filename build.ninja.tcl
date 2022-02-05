# ninja-based description of how to build fble and friends.
#
# First time setup:
#   tclsh build.ninja
#
# Afterwards:
#   ninja -f out/build.ninja

# output directory used for build.
set ::out "out"

exec mkdir -p $::out
set ::build_ninja [open "$::out/build.ninja" "w"]

# build.ninja header.
puts $::build_ninja "builddir = $::out"
puts $::build_ninja {
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
  puts $::build_ninja "build [join $targets]: rule [join $dependencies]"
  puts $::build_ninja "  cmd = $command"

  foreach kv $args {
    puts $::build_ninja "  $kv"
  }
}

# obj --
#   Builds a .o file.
#
# Inputs:
#   obj - the .o file to build.
#   src - the .c file to build the .o file from.
#   iflags - include flags, e.g. "-I foo".
#   args - optional additional dependencies.
proc obj { obj src iflags args } {
  set cflags "-std=c99 -pedantic -Wall -Werror -gdwarf-3 -ggdb -O3"
  set cmd "gcc -MMD -MF $obj.d $cflags $iflags -c -o $obj $src"
  build $obj "$src $args" $cmd "depfile = $obj.d"
}

# obj_cov --
#   Builds a .o file with test coverage enabled.
#
# Inputs:
#   obj - the .o file to build.
#   src - the .c file to build the .o file from.
#   iflags - include flags, e.g. "-I foo".
#   args - optional additional dependencies.
proc obj_cov { obj src iflags args } {
  set gcda [string map {.o .gcda} $obj]
  set cflags "-std=c99 -pedantic -Wall -Werror -gdwarf-3 -ggdb -fprofile-arcs -ftest-coverage"
  set cmd "rm -f $gcda ; gcc -MMD -MF $obj.d $cflags $iflags -c -o $obj $src"
  build $obj "$src $args" $cmd "depfile = $obj.d"
}

# Compile a .s file to .o
#
# Inputs:
#   obj - the .o file to build.
#   src - the .s file to build the .o file from.
#   args - optional additional dependencies.
proc asm { obj src args } {
  set cmd "as -o $obj $src"
  build $obj "$src $args" $cmd
}

# Compile a .fble file to .o.
#
# Inputs:
#   obj - the name of the .o file to generate.
#   compile - the name of the fble-compile executable.
#   compileargs - args to pass to the fble compiler.
#   args - additional dependencies, typically including the .fble file.
proc fbleobj { obj compile compileargs args } {
  set s [string map {.o .s} $obj]
  build $s "$compile $args" "$compile $compileargs > $s"

  set cmd "as -o $obj $s"
  build $obj "$s $args" $cmd
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
#   bin - the binary file to build.
#   objs - the list of .o files to build from.
#   lflags - library flags, e.g. "-L foo/ -lfoo".
#   args - optional additional dependencies.
proc bin { bin objs lflags args } {
  #set cflags "-std=c99 -pedantic -Wall -Werror -gdwarf-3 -ggdb -no-pie -fprofile-arcs -ftest-coverage -pg"
  set cflags "-std=c99 -pedantic -Wall -Werror -gdwarf-3 -ggdb -no-pie -O3"
  build $bin "$objs $args" "gcc $cflags -o $bin $objs $lflags"
}

# bin_cov --
#   Build a binary with test coverage enabled.
#
# Inputs:
#   bin - the binary file to build.
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

# libfble.a
eval {
  set fble_objs [list]
  set fble_objs_cov [list]

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
    set report $::out/fble/src/parse.tab.report.txt
    set tabc $::out/fble/src/parse.tab.c
    set cmd "bison --report=all --report-file=$report -o $tabc fble/src/parse.y"
    build "$tabc $report" "fble/src/parse.y $includes" $cmd

    obj $::out/fble/src/parse.tab.o $::out/fble/src/parse.tab.c "-I fble/include -I fble/src"
    obj_cov $::out/fble/src/parse.tab.cov.o $::out/fble/src/parse.tab.c "-I fble/include -I fble/src"
    lappend fble_objs $::out/fble/src/parse.tab.o
    lappend fble_objs_cov $::out/fble/src/parse.tab.cov.o
  }

  # .o files
  lappend ::build_ninja_deps "fble/src"
  foreach {x} [glob -tails -directory fble/src *.c] {
    set object $::out/fble/src/[string map {.c .o} $x]
    set object_cov $::out/fble/src/[string map {.c .cov.o} $x]
    obj $object fble/src/$x "-I fble/include -I fble/src"
    obj_cov $object_cov fble/src/$x "-I fble/include -I fble/src"
    lappend fble_objs $object
    lappend fble_objs_cov $object_cov
  }

  # libfble.a
  set ::libfble "$::out/fble/src/libfble.a"
  lib $::libfble $fble_objs

  set ::libfblecov "$::out/fble/src/libfble.cov.a"
  lib $::libfblecov $fble_objs_cov
}

# fble tool binaries
lappend build_ninja_deps "tools"
foreach {x} [glob tools/*.c] {
  set base [file rootname [file tail $x]]
  obj $::out/tools/$base.o $x "-I fble/include"
  bin $::out/tools/$base "$::out/tools/$base.o" "-L $::out/fble/src -lfble" $::libfble
  bin_cov $::out/tools/$base.cov "$::out/tools/$base.o" "-L $::out/fble/src -lfble.cov" $::libfblecov
}

# fble programs native library 
set fbleprgmsnative_objs [list]
foreach {x} { Core/char.fble Core/int.fble Core/string.fble } {
  lappend fbleprgmsnative_objs $::out/prgms/$x.o
  obj $::out/prgms/$x.o prgms/$x.c "-I fble/include -I prgms"
}
lib $::out/prgms/libfble-prgms-native.a $fbleprgmsnative_objs

# fble programs binaries
foreach {x} { fble-md5 fble-stdio fble-app } {
  obj $::out/prgms/$x.o prgms/$x.c \
    "-I fble/include -I /usr/include/SDL2"
  bin $::out/prgms/$x "$::out/prgms/$x.o" \
    "-L $::out/fble/src -L $::out/prgms -lfble -lfble-prgms-native -lSDL2 -lGL" \
    "$::libfble $::out/prgms/libfble-prgms-native.a"
}

# Compiled variations of some of the tools.
obj $::out/tools/fble-compiled-test.o tools/fble-test.c "-DFbleCompiledMain=FbleCompiledMain -I fble/include"
obj $::out/tools/fble-compiled-mem-test.o tools/fble-mem-test.c "-DFbleCompiledMain=FbleCompiledMain -I fble/include"
obj $::out/prgms/fble-compiled-stdio.o prgms/fble-stdio.c "-DFbleCompiledMain=FbleCompiledMain -I fble/include"
obj $::out/prgms/fble-compiled-app.o prgms/fble-app.c "-DFbleCompiledMain=FbleCompiledMain -I fble/include -I /usr/include/SDL2"
obj $::out/tools/fble-compiled-profiles-test.o tools/fble-profiles-test.c "-DFbleCompiledMain=FbleCompiledMain -I fble/include"

# tests
test $::out/true.tr "" true
#test $::out/false.tr "" false

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
#   fble-test-*-error tests.
set ::spec_tests [list]
foreach dir [dirs langs/fble ""] {
  lappend build_ninja_deps "langs/fble/$dir"
  foreach {t} [lsort [glob -tails -directory langs/fble -nocomplain -type f $dir/*.tcl]] {
    set ::specroot [file rootname $t]
    set ::spectestdir $::out/langs/fble/$specroot
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

    # Emit build rules to compile all the .fble files for the test together
    # into a libtest.a library.
    proc spec-test-compile { modules } {
      set fbles [collect_modules "" $modules]
      lappend fbles "/Nat.fble"

      set objs [list]
      foreach x $fbles {
        set path [file rootname $x]%
        set fble $::spectestdir$x

        set o [string map {.fble .o} $fble]
        lappend objs $o

        fbleobj $o $::out/tools/fble-compile.cov "-c -I $::spectestdir -m $path" $::spectestdir/test.fble
      }

      fbleobj $::spectestdir/test.o $::out/tools/fble-compile.cov "-c -e FbleCompiledMain -I $::spectestdir -m /test%" $::spectestdir/test.fble
      lappend objs $::spectestdir/test.o

      lib $::spectestdir/libtest.a $objs
    }

    proc spec-test-extract {} {
      build "$::spectestdir/test.fble"  \
        "tools/extract-spec-test.tcl $::spectcl langs/fble/Nat.fble" \
        "tclsh tools/extract-spec-test.tcl $::spectcl $::spectestdir langs/fble/Nat.fble"
    }

    proc fble-test { expr args } {
      spec-test-extract
      spec-test-compile $args

      # We run with --profile to get better test coverage for profiling.
      lappend ::spec_tests $::spectestdir/test.tr
      test $::spectestdir/test.tr "tools/run-spec-test.tcl $::out/tools/fble-test.cov $::spectestdir/test.fble" \
        "tclsh tools/run-spec-test.tcl $::spectcl $::out/tools/fble-test.cov --profile -I $::spectestdir /test%"

      lappend ::spec_tests $::spectestdir/test-compiled.tr
      bin $::spectestdir/compiled-test \
        "$::out/tools/fble-compiled-test.o $::spectestdir/libtest.a" \
        "-L $::out/fble/src -L $::spectestdir -lfble -ltest" "$::libfble"
      test $::spectestdir/test-compiled.tr \
        "tools/run-spec-test.tcl $::spectestdir/compiled-test" \
        "tclsh tools/run-spec-test.tcl $::spectcl $::spectestdir/compiled-test --profile"
    }

    proc fble-test-compile-error { loc expr args } {
      spec-test-extract

      lappend ::spec_tests $::spectestdir/test.tr
      test $::spectestdir/test.tr "tools/run-spec-test.tcl $::out/tools/fble-test.cov $::spectestdir/test.fble" \
        "tclsh tools/run-spec-test.tcl $::spectcl $::out/tools/fble-test.cov --compile-error -I $::spectestdir /test%"

      # TODO: Test that we get the desired compilation error when running the
      # compiler?
    }

    proc fble-test-runtime-error { loc expr args } {
      spec-test-extract
      spec-test-compile $args

      lappend ::spec_tests $::spectestdir/test.tr
      test $::spectestdir/test.tr "tools/run-spec-test.tcl $::out/tools/fble-test.cov $::spectestdir/test.fble" \
        "tclsh tools/run-spec-test.tcl $::spectcl $::out/tools/fble-test.cov --runtime-error -I $::spectestdir /test%"

      lappend ::spec_tests $::spectestdir/test-compiled.tr
      bin $::spectestdir/compiled-test \
        "$::out/tools/fble-compiled-test.o $::spectestdir/libtest.a" \
        "-L $::out/fble/src -L $::spectestdir -lfble -ltest" "$::libfble"
      test $::spectestdir/test-compiled.tr \
        "tools/run-spec-test.tcl $::spectestdir/compiled-test" \
        "tclsh tools/run-spec-test.tcl $::spectcl $::spectestdir/compiled-test --runtime-error"
    }

    proc fble-test-memory-constant { expr } {
      spec-test-extract
      spec-test-compile {}

      lappend ::spec_tests $::spectestdir/test.tr
      test $::spectestdir/test.tr "tools/run-spec-test.tcl $::out/tools/fble-mem-test.cov $::spectestdir/test.fble" \
        "tclsh tools/run-spec-test.tcl $::spectcl $::out/tools/fble-mem-test.cov -I $::spectestdir /test%"

      lappend ::spec_tests $::spectestdir/test-compiled.tr
      bin $::spectestdir/compiled-test \
        "$::out/tools/fble-compiled-mem-test.o $::spectestdir/libtest.a" \
        "-L $::out/fble/src -L $::spectestdir -lfble -ltest" "$::libfble"
      test $::spectestdir/test-compiled.tr \
        "tools/run-spec-test.tcl $::spectestdir/compiled-test" \
        "tclsh tools/run-spec-test.tcl $::spectcl $::spectestdir/compiled-test"
    }

    proc fble-test-memory-growth { expr } {
      spec-test-extract
      spec-test-compile {}

      lappend ::spec_tests $::spectestdir/test.tr
      test $::spectestdir/test.tr "tools/run-spec-test.tcl $::out/tools/fble-mem-test.cov $::spectestdir/test.fble" \
        "tclsh tools/run-spec-test.tcl $::spectcl $::out/tools/fble-mem-test.cov --growth -I $::spectestdir /test%"

      lappend ::spec_tests $::spectestdir/test-compiled.tr
      bin $::spectestdir/compiled-test \
        "$::out/tools/fble-compiled-mem-test.o $::spectestdir/libtest.a" \
        "-L $::out/fble/src -L $::spectestdir -lfble -ltest" "$::libfble"
      test $::spectestdir/test-compiled.tr \
        "tools/run-spec-test.tcl $::spectestdir/compiled-test" \
        "tclsh tools/run-spec-test.tcl $::spectcl $::spectestdir/compiled-test --growth"
    }
    source $::spectcl
  }
}

# Code coverage from spec tests.
build $::out/cov/gcov.txt "$::fble_objs_cov $::spec_tests" \
  "gcov $::fble_objs_cov > $::out/cov/gcov.txt && mv *.gcov $::out/cov"

# fble-profile-test
test $::out/tools/fble-profile-test.tr $::out/tools/fble-profile-test \
  "$::out/tools/fble-profile-test > /dev/null"

# fble-profiles-test
test $::out/tools/fble-profiles-test.tr \
  "$::out/tools/fble-profiles-test prgms/Fble/ProfilesTest.fble" \
  "$::out/tools/fble-profiles-test -I prgms /Fble/ProfilesTest% > $::out/tools/fble-profiles-test.prof"

# Compile each .fble module in the prgms directory.
set ::fble_prgms_objs [list]
foreach dir [dirs prgms ""] {
  lappend build_ninja_deps "prgms/$dir"
  foreach {x} [glob -tails -directory prgms -nocomplain -type f $dir/*.fble] {
    set mpath "/[file rootname $x]%"

    # Generate a .d file to capture dependencies.
    build $::out/prgms/$x.d "$::out/tools/fble-deps prgms/$x" \
      "$::out/tools/fble-deps -I prgms -t $::out/prgms/$x.d -m $mpath > $::out/prgms/$x.d" \
      "depfile = $::out/prgms/$x.d"

    fbleobj $::out/prgms/$x.o $::out/tools/fble-compile "-c -I prgms -m $mpath" $::out/prgms/$x.d
    lappend ::fble_prgms_objs $::out/prgms/$x.o
  }
}

# libfbleprgms.a
set ::libfbleprgms "$::out/prgms/libfble-prgms.a"
lib $::libfbleprgms $::fble_prgms_objs

# fble-disassemble test
test $::out/tools/fble-disassemble.tr \
  "$::out/tools/fble-disassemble $::out/prgms/Fble/Tests.fble.d" \
  "$::out/tools/fble-disassemble -I prgms -m /Fble/Tests% > $::out/prgms/Fble/Tests.fbls"

# Fble/Tests.fble tests
test $::out/prgms/Fble/fble-tests.tr "$::out/prgms/fble-stdio $::out/prgms/Fble/Tests.fble.d" \
  "$::out/prgms/fble-stdio -I prgms /Fble/Tests%" "pool = console"

# fble-md5 test
test $::out/prgms/fble-md5.tr "$::out/prgms/fble-md5 $::out/prgms/Md5/Main.fble.d" \
  "$::out/prgms/fble-md5 /dev/null -I prgms /Md5/Main% > $::out/prgms/fble-md5.out && grep d41d8cd98f00b204e9800998ecf8427e $::out/prgms/fble-md5.out > /dev/null"

# fble-cat test
test $::out/prgms/Stdio/fble-cat.tr "$::out/prgms/fble-stdio $::out/prgms/Stdio/Cat.fble.d" \
  "$::out/prgms/fble-stdio -I prgms /Stdio/Cat% < README.txt > $::out/prgms/Stdio/fble-cat.out && cmp $::out/prgms/Stdio/fble-cat.out README.txt"

# fble-stdio test
test $::out/prgms/Stdio/fble-stdio.tr "$::out/prgms/fble-stdio $::out/prgms/Stdio/Test.fble.d" \
  "$::out/prgms/fble-stdio -I prgms /Stdio/Test% > $::out/prgms/Stdio/fble-stdio.out && grep PASSED $::out/prgms/Stdio/fble-stdio.out > /dev/null"

# Build an fble-stdio compiled binary.
proc stdio { name path } {
  build $::out/prgms/$name.s $::out/tools/fble-compile \
    "$::out/tools/fble-compile -e FbleCompiledMain -m $path > $::out/prgms/$name.s"
  asm $::out/prgms/$name.o $::out/prgms/$name.s
  bin $::out/prgms/$name \
    "$::out/prgms/$name.o $::out/prgms/fble-compiled-stdio.o" \
    "-L $::out/fble/src -L $::out/prgms -lfble -lfble-prgms -lfble-prgms-native" \
    "$::libfble $::libfbleprgms $::out/prgms/libfble-prgms-native.a"
};

stdio fble-stdio-test "/Stdio/Test%"
stdio fble-tests "/Fble/Tests%"
stdio fble-bench "/Fble/Bench%"
stdio fble-debug-test "/Fble/DebugTest%"

# /Stdio/Test% compilation test
test $::out/prgms/fble-stdio-test.tr $::out/prgms/fble-stdio-test \
  "$::out/prgms/fble-stdio-test > $::out/prgms/fble-stdio-test.out && grep PASSED $::out/prgms/fble-stdio-test.out > /dev/null"

# /Fble/Tests% compilation test
test $::out/prgms/Fble/fble-compiled-tests.tr $::out/prgms/fble-tests \
  "$::out/prgms/fble-tests" "pool = console"

# fble-compiled-profiles-test
fbleobj $::out/prgms/fble-compiled-profiles-test-fble-main.o $::out/tools/fble-compile \
  "-c -e FbleCompiledMain -I prgms -m /Fble/ProfilesTest%" \
  prgms/Fble/ProfilesTest.fble
bin $::out/prgms/fble-compiled-profiles-test \
  "$::out/tools/fble-compiled-profiles-test.o $::out/prgms/fble-compiled-profiles-test-fble-main.o" \
  "-L $::out/fble/src -L $::out/prgms -lfble -lfble-prgms" "$::libfble $::libfbleprgms"
test $::out/prgms/Fble/fble-compiled-profiles-test.tr \
  "$::out/prgms/fble-compiled-profiles-test" \
  "$::out/prgms/fble-compiled-profiles-test > $::out/prgms/Fble/fble-compiled-profiles-test.prof"

# Test that there are no dwarf warnings in the generated fble-debug-test
# binary.
build "$::out/prgms/fble-debug-test.dwarf $::out/prgms/fble-debug-test.dwarf-warnings.txt" \
  "$::out/prgms/fble-debug-test" \
  "objdump --dwarf $::out/prgms/fble-debug-test > $::out/prgms/fble-debug-test.dwarf 2> $::out/prgms/fble-debug-test.dwarf-warnings.txt"

# TODO: Understand why this test fails.
#test $::out/prgms/dwarf-test.tr \
#  "$::out/prgms/fble-debug-test.dwarf-warnings.txt" \
#  "cmp /dev/null $::out/prgms/fble-debug-test.dwarf-warnings.txt"

test $::out/prgms/fble-debug-test.tr \
  "$::out/prgms/fble-debug-test tools/fble-debug-test.exp" \
  "expect tools/fble-debug-test.exp > /dev/null"

# Build an fble-app compiled binary.
proc app { name path } {
  build $::out/prgms/$name.s $::out/tools/fble-compile \
    "$::out/tools/fble-compile -e FbleCompiledMain -m $path > $::out/prgms/$name.s"
  asm $::out/prgms/$name.o $::out/prgms/$name.s ""
  bin $::out/prgms/$name \
    "$::out/prgms/$name.o $::out/prgms/fble-compiled-app.o" \
    "-L $::out/fble/src -L $::out/prgms -lfble -lfble-prgms -lfble-prgms-native -lSDL2 -lGL" \
    "$::libfble $::libfbleprgms $::out/prgms/libfble-prgms-native.a"
}

app fble-invaders "/Invaders/App%"
app fble-graphics "/Graphics/App%"
app fble-pinball "/Pinball/App%"

# test summary
build $::out/tests.txt "$::tests" "echo $::tests > $::out/tests.txt"
build $::out/summary.tr "tools/tests.tcl $::out/tests.txt" \
  "tclsh tools/tests.tcl $::out/tests.txt && echo PASSED > $::out/summary.tr"

# build.ninja
build $::out/build.ninja "build.ninja.tcl" "tclsh build.ninja.tcl" \
  "depfile = $::out/build.ninja.d"

# build.ninja.d implicit dependency file.
set ::build_ninja_d [open "$::out/build.ninja.d" "w"]
puts $::build_ninja_d "$::out/build.ninja: $::build_ninja_deps"

