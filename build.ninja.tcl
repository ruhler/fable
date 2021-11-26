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
  set cflags "-std=c99 -pedantic -Wall -Werror -gdwarf-3 -ggdb -O3"
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

# Compile a .s file to .o
#
# Inputs:
#   obj - the .o file to build (include $::obj directory).
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
#   module_path - the module path to compile.
#   search_path - the search path.
#   export - "" or "--export ..." arg to pass to compile function.
#   args - additional dependencies, typically including the .fble file.
proc fbleobj { obj compile module_path search_path export args } {
  set s [string map {.o .s} $obj]
  build $s "$compile $args" "$compile $export $module_path $search_path > $s"

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
#   bin - the binary file to build (include $::bin directory).
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
foreach {x} [glob tools/*.c] {
  set base [file rootname [file tail $x]]
  obj $::obj/$base.o $x "-I fble/include"
  bin $::bin/$base "$::obj/$base.o" "-L $::lib -lfble" $::libfble
  bin_cov $::bin/$base.cov "$::obj/$base.o" "-L $::lib -lfble.cov" $::libfblecov
}

# fble programs native library 
set fbleprgmsnative_objs [list]
foreach {x} { char.fble int.fble string.fble } {
  lappend fbleprgmsnative_objs $::obj/$x.o
  obj $::obj/$x.o prgms/$x.c "-I fble/include -I prgms"
}
lib $::lib/libfble-prgms-native.a $fbleprgmsnative_objs

# fble programs binaries
foreach {x} { fble-md5 fble-stdio fble-app } {
  obj $::obj/$x.o prgms/$x.c "-I fble/include -I /usr/include/SDL2"
  bin $::bin/$x "$::obj/$x.o" \
    "-L $::lib -lfble -lfble-prgms-native -lSDL2" \
    "$::libfble $::lib/libfble-prgms-native.a"
}

# Compiled variations of some of the tools.
obj $::obj/fble-compiled-test.o tools/fble-test.c "-DFbleCompiledMain=FbleCompiledMain -I fble/include"
obj $::obj/fble-compiled-mem-test.o tools/fble-mem-test.c "-DFbleCompiledMain=FbleCompiledMain -I fble/include"
obj $::obj/fble-compiled-stdio.o prgms/fble-stdio.c "-DFbleCompiledMain=FbleCompiledMain -I fble/include"
obj $::obj/fble-compiled-app.o prgms/fble-app.c "-DFbleCompiledMain=FbleCompiledMain -I fble/include -I /usr/include/SDL2"
obj $::obj/fble-compiled-profiles-test.o tools/fble-profiles-test.c "-DFbleCompiledMain=FbleCompiledMain -I fble/include"

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
#   fble-test-*-error tests.
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

        fbleobj $o $::bin/fble-compile.cov $path $::spectestdir "" $::spectestdir/test.fble
      }

      fbleobj $::spectestdir/test.o $::bin/fble-compile.cov /test% $::spectestdir "--export FbleCompiledMain" $::spectestdir/test.fble
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
      test $::spectestdir/test.tr "tools/run-spec-test.tcl $::bin/fble-test.cov $::spectestdir/test.fble" \
        "tclsh tools/run-spec-test.tcl $::spectcl $::bin/fble-test.cov --profile $::spectestdir /test%"

      lappend ::spec_tests $::spectestdir/test-compiled.tr
      bin $::spectestdir/compiled-test \
        "$::obj/fble-compiled-test.o $::spectestdir/libtest.a" \
        "-L $::lib -L $::spectestdir -lfble -ltest" "$::libfble"
      test $::spectestdir/test-compiled.tr \
        "tools/run-spec-test.tcl $::spectestdir/compiled-test" \
        "tclsh tools/run-spec-test.tcl $::spectcl $::spectestdir/compiled-test --profile"
    }

    proc fble-test-compile-error { loc expr args } {
      spec-test-extract

      lappend ::spec_tests $::spectestdir/test.tr
      test $::spectestdir/test.tr "tools/run-spec-test.tcl $::bin/fble-test.cov $::spectestdir/test.fble" \
        "tclsh tools/run-spec-test.tcl $::spectcl $::bin/fble-test.cov --compile-error $::spectestdir /test%"

      # TODO: Test that we get the desired compilation error when running the
      # compiler?
    }

    proc fble-test-runtime-error { loc expr args } {
      spec-test-extract
      spec-test-compile $args

      lappend ::spec_tests $::spectestdir/test.tr
      test $::spectestdir/test.tr "tools/run-spec-test.tcl $::bin/fble-test.cov $::spectestdir/test.fble" \
        "tclsh tools/run-spec-test.tcl $::spectcl $::bin/fble-test.cov --runtime-error $::spectestdir /test%"

      lappend ::spec_tests $::spectestdir/test-compiled.tr
      bin $::spectestdir/compiled-test \
        "$::obj/fble-compiled-test.o $::spectestdir/libtest.a" \
        "-L $::lib -L $::spectestdir -lfble -ltest" "$::libfble"
      test $::spectestdir/test-compiled.tr \
        "tools/run-spec-test.tcl $::spectestdir/compiled-test" \
        "tclsh tools/run-spec-test.tcl $::spectcl $::spectestdir/compiled-test --runtime-error"
    }

    proc fble-test-memory-constant { expr } {
      spec-test-extract
      spec-test-compile {}

      lappend ::spec_tests $::spectestdir/test.tr
      test $::spectestdir/test.tr "tools/run-spec-test.tcl $::bin/fble-mem-test.cov $::spectestdir/test.fble" \
        "tclsh tools/run-spec-test.tcl $::spectcl $::bin/fble-mem-test.cov $::spectestdir /test%"

      lappend ::spec_tests $::spectestdir/test-compiled.tr
      bin $::spectestdir/compiled-test \
        "$::obj/fble-compiled-mem-test.o $::spectestdir/libtest.a" \
        "-L $::lib -L $::spectestdir -lfble -ltest" "$::libfble"
      test $::spectestdir/test-compiled.tr \
        "tools/run-spec-test.tcl $::spectestdir/compiled-test" \
        "tclsh tools/run-spec-test.tcl $::spectcl $::spectestdir/compiled-test"
    }

    proc fble-test-memory-growth { expr } {
      spec-test-extract
      spec-test-compile {}

      lappend ::spec_tests $::spectestdir/test.tr
      test $::spectestdir/test.tr "tools/run-spec-test.tcl $::bin/fble-mem-test.cov $::spectestdir/test.fble" \
        "tclsh tools/run-spec-test.tcl $::spectcl $::bin/fble-mem-test.cov --growth $::spectestdir /test%"

      lappend ::spec_tests $::spectestdir/test-compiled.tr
      bin $::spectestdir/compiled-test \
        "$::obj/fble-compiled-mem-test.o $::spectestdir/libtest.a" \
        "-L $::lib -L $::spectestdir -lfble -ltest" "$::libfble"
      test $::spectestdir/test-compiled.tr \
        "tools/run-spec-test.tcl $::spectestdir/compiled-test" \
        "tclsh tools/run-spec-test.tcl $::spectcl $::spectestdir/compiled-test --growth"
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
  "$::bin/fble-profiles-test prgms /Fble/ProfilesTest% > $::test/fble-profiles-test.prof"

# Compile each .fble module in the prgms directory.
set ::fble_prgms_objs [list]
foreach dir [dirs prgms ""] {
  lappend build_ninja_deps "prgms/$dir"
  foreach {x} [glob -tails -directory prgms -nocomplain -type f $dir/*.fble] {
    set mpath "/[file rootname $x]%"

    # Generate a .d file to capture dependencies.
    build $::prgms/$x.d "$::bin/fble-deps prgms/$x" \
      "$::bin/fble-deps $::prgms/$x.d prgms $mpath > $::prgms/$x.d" \
      "depfile = $::prgms/$x.d"

    fbleobj $::prgms/$x.o $::bin/fble-compile $mpath prgms "" $::prgms/$x.d
    lappend ::fble_prgms_objs $::prgms/$x.o
  }
}

# libfbleprgms.a
set ::libfbleprgms "$::lib/libfble-prgms.a"
lib $::libfbleprgms $::fble_prgms_objs

# fble-disassemble test
test $::test/fble-disassemble.tr \
  "$::bin/fble-disassemble $::prgms/Fble/Tests.fble.d" \
  "$::bin/fble-disassemble prgms /Fble/Tests% > $::prgms/Fble/Tests.fbls"

# Fble/Tests.fble tests
test $::test/fble-tests.tr "$::bin/fble-stdio $::prgms/Fble/Tests.fble.d" \
  "$::bin/fble-stdio prgms /Fble/Tests%" "pool = console"

# fble-md5 test
test $::test/fble-md5.tr "$::bin/fble-md5 $::prgms/Md5/Main.fble.d" \
  "$::bin/fble-md5 /dev/null prgms /Md5/Main% > $::test/fble-md5.out && grep d41d8cd98f00b204e9800998ecf8427e $::test/fble-md5.out > /dev/null"

# fble-cat test
test $::test/fble-cat.tr "$::bin/fble-stdio $::prgms/Stdio/Cat.fble.d" \
  "$::bin/fble-stdio prgms /Stdio/Cat% < README.txt > $::test/fble-cat.out && cmp $::test/fble-cat.out README.txt"

# fble-stdio test
test $::test/fble-stdio.tr "$::bin/fble-stdio $::prgms/Stdio/Test.fble.d" \
  "$::bin/fble-stdio prgms /Stdio/Test% > $::test/fble-stdio.out && grep PASSED $::test/fble-stdio.out > /dev/null"

# Build an fble-stdio compiled binary.
proc stdio { name path } {
  build $::src/$name.s $::bin/fble-compile \
    "$::bin/fble-compile --export FbleCompiledMain $path > $::src/$name.s"
  asm $::obj/$name.o $::src/$name.s
  bin $::bin/$name \
    "$::obj/$name.o $::obj/fble-compiled-stdio.o" \
    "-L $::lib -lfble -lfble-prgms -lfble-prgms-native" \
    "$::libfble $::libfbleprgms $::lib/libfble-prgms-native.a"
};

stdio fble-stdio-test "/Stdio/Test%"
stdio fble-tests "/Fble/Tests%"
stdio fble-bench "/Fble/Bench%"
stdio fble-debug-test "/Fble/DebugTest%"

# /Stdio/Test% compilation test
test $::test/fble-stdio-test.tr $::bin/fble-stdio-test \
  "$::bin/fble-stdio-test > $::test/fble-stdio-test.out && grep PASSED $::test/fble-stdio-test.out > /dev/null"

# /Fble/Tests% compilation test
test $::test/fble-compiled-tests.tr $::bin/fble-tests \
  "$::bin/fble-tests" "pool = console"

# fble-compiled-profiles-test
fbleobj $::obj/fble-compiled-profiles-test-fble-main.o $::bin/fble-compile \
  /Fble/ProfilesTest% prgms "--export FbleCompiledMain" \
  prgms/Fble/ProfilesTest.fble
bin $::bin/fble-compiled-profiles-test \
  "$::obj/fble-compiled-profiles-test.o $::obj/fble-compiled-profiles-test-fble-main.o" \
  "-L $::lib -lfble -lfble-prgms" "$::libfble $::libfbleprgms"
test $::test/fble-compiled-profiles-test.tr \
  "$::bin/fble-compiled-profiles-test" \
  "$::bin/fble-compiled-profiles-test > $::test/fble-compiled-profiles-test.prof"

# Test that there are no dwarf warnings in the generated fble-debug-test
# binary.
build "$::obj/fble-debug-test.dwarf $::test/fble-debug-test.dwarf-warnings.txt" \
  "$::bin/fble-debug-test" \
  "objdump --dwarf $::bin/fble-debug-test > $::obj/fble-debug-test.dwarf 2> $::test/fble-debug-test.dwarf-warnings.txt"

# TODO: Understand why this test fails.
#test $::test/dwarf-test.tr \
#  "$::test/fble-debug-test.dwarf-warnings.txt" \
#  "cmp /dev/null $::test/fble-debug-test.dwarf-warnings.txt"

test $::test/fble-debug-test.tr \
  "$::bin/fble-debug-test tools/fble-debug-test.exp" \
  "expect tools/fble-debug-test.exp > /dev/null"

# Build an fble-app compiled binary.
proc app { name path } {
  build $::src/$name.s $::bin/fble-compile \
    "$::bin/fble-compile --export FbleCompiledMain $path > $::src/$name.s"
  asm $::obj/$name.o $::src/$name.s ""
  bin $::bin/$name \
    "$::obj/$name.o $::obj/fble-compiled-app.o" \
    "-L $::lib -lfble -lfble-prgms -lfble-prgms-native -lSDL2" \
    "$::libfble $::libfbleprgms $::lib/libfble-prgms-native.a"
}

app fble-invaders "/Invaders/App%"
app fble-graphics "/Graphics/App%"
app fble-pinball "/Pinball/App%"

# test summary
build $::test/tests.txt "$::tests" "echo $::tests > $::test/tests.txt"
build $::test/summary.tr "tools/tests.tcl $::test/tests.txt" \
  "tclsh tools/tests.tcl $::test/tests.txt && echo PASSED > $::test/summary.tr"

# build.ninja
build $::out/build.ninja "build.ninja.tcl $::build_ninja_deps" \
  "tclsh build.ninja.tcl > $::out/build.ninja"
