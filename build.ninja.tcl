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
rule build
  description = $out
  command = $cmd

cflags = -std=c99 -pedantic -Wall -Werror -gdwarf-3 -ggdb -O3
rule obj
  description = $out
  command = gcc -MMD -MF $out.d $cflags $iflags -c -o $out $src
  depfile = $out.d
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
  puts $::build_ninja "build [join $targets]: build [join $dependencies]"
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
  puts $::build_ninja "build $obj: obj $src $args"
  puts $::build_ninja "  src=$src"
  puts $::build_ninja "  iflags=$iflags"
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

# pkg --
#   Build an fble library package.
#
# Inputs:
#   name - the name of the package, such as 'app'.
#   objs - additional object files to include in the generated library.
proc pkg {name objs} {
  set cflags [exec pkg-config --cflags-only-I fble-$name]
  foreach dir [dirs pkgs/$name ""] {
    lappend ::build_ninja_deps "pkgs/$name/$dir"
    foreach {x} [glob -tails -directory pkgs/$name -nocomplain -type f $dir/*.fble] {
      set mpath "/[file rootname $x]%"

      # Generate a .d file to capture dependencies.
      build $::out/pkgs/$name/$x.d "$::out/fble/bin/fble-deps pkgs/$name/$x" \
        "$::out/fble/bin/fble-deps $cflags -t $::out/pkgs/$name/$x.d -m $mpath > $::out/pkgs/$name/$x.d" \
        "depfile = $::out/pkgs/$name/$x.d"

      fbleobj $::out/pkgs/$name/$x.o $::out/fble/bin/fble-compile "-c $cflags -m $mpath" $::out/pkgs/$name/$x.d
      lappend objs $::out/pkgs/$name/$x.o
    }
  }

  lib $::out/pkgs/$name/libfble-$name.a $objs
}

# Any time we run glob over a directory, add that directory to this list.
# We need to make sure to include these directories as a dependency on the
# generation of build.ninja.
set ::build_ninja_deps [list]

# Set up pkg-config for use in build.
set ::env(PKG_CONFIG_TOP_BUILD_DIR) $::out
set ::env(PKG_CONFIG_PATH) fble
lappend ::build_ninja_deps "fble/fble.pc"
lappend ::build_ninja_deps "fble/fble.cov.pc"
foreach pkg [list core sat app hwdg misc invaders pinball games graphics md5] {
  append ::env(PKG_CONFIG_PATH) ":pkgs/$pkg"
  lappend ::build_ninja_deps "pkgs/$pkg/fble-$pkg.pc"
}

# libfble.a
eval {
  set fble_objs [list]
  set fble_objs_cov [list]

  # parser
  eval {
    # Update local includes for fble/lib/parse.y here.
    # See comment in fble/lib/parse.y.
    set includes {
      fble/include/fble-alloc.h
      fble/include/fble-load.h
      fble/include/fble-loc.h
      fble/include/fble-module-path.h
      fble/include/fble-name.h
      fble/include/fble-string.h
      fble/include/fble-vector.h
      fble/lib/expr.h
    }
    set report $::out/fble/lib/parse.tab.report.txt
    set tabc $::out/fble/lib/parse.tab.c
    set cmd "bison --report=all --report-file=$report -o $tabc fble/lib/parse.y"
    build "$tabc $report" "fble/lib/parse.y $includes" $cmd

    obj $::out/fble/lib/parse.tab.o $::out/fble/lib/parse.tab.c "-I fble/include -I fble/lib"
    obj_cov $::out/fble/lib/parse.tab.cov.o $::out/fble/lib/parse.tab.c "-I fble/include -I fble/lib"
    lappend fble_objs $::out/fble/lib/parse.tab.o
    lappend fble_objs_cov $::out/fble/lib/parse.tab.cov.o
  }

  # .o files
  lappend ::build_ninja_deps "fble/lib"
  foreach {x} [glob -tails -directory fble/lib *.c] {
    set object $::out/fble/lib/[string map {.c .o} $x]
    set object_cov $::out/fble/lib/[string map {.c .cov.o} $x]
    obj $object fble/lib/$x "-I fble/include"
    obj_cov $object_cov fble/lib/$x "-I fble/include"
    lappend fble_objs $object
    lappend fble_objs_cov $object_cov
  }

  # libfble.a
  set ::libfble "$::out/fble/lib/libfble.a"
  lib $::libfble $fble_objs

  set ::libfblecov "$::out/fble/lib/libfble.cov.a"
  lib $::libfblecov $fble_objs_cov
}

# fble/bin
eval {
  lappend ::build_ninja_deps "fble/bin"
  set cflags [exec pkg-config --cflags fble]
  set ldflags [exec pkg-config --static --libs fble]
  set ldflags_cov [exec pkg-config --static --libs fble.cov]
  foreach {x} [glob fble/bin/*.c] {
    set base [file rootname [file tail $x]]
    obj $::out/fble/bin/$base.o $x $cflags
    bin $::out/fble/bin/$base "$::out/fble/bin/$base.o" $ldflags $::libfble
    bin_cov $::out/fble/bin/$base.cov "$::out/fble/bin/$base.o" $ldflags_cov $::libfblecov
  }
}

# fble/test library, tools, and tests.
eval {
  lappend ::build_ninja_deps "fble/test"

  set objs [list]
  foreach {x} [list test.c mem-test.c profiles-test.c] {
    set object $::out/fble/test/[string map {.c .o} $x]
    obj $object fble/test/$x "-I fble/include"
    lappend objs $object
  }
  lib $::out/fble/test/libfbletest.a $objs

  set cflags [exec pkg-config --cflags fble]
  set ldflags "-L $::out/fble/test -lfbletest [exec pkg-config --static --libs fble]"
  set ldflags_cov "-L $::out/fble/test -lfbletest [exec pkg-config --static --libs fble.cov]"

  foreach {x} [glob fble/test/fble-*.c] {
    set base [file rootname [file tail $x]]
    obj $::out/fble/test/$base.o $x $cflags
    bin $::out/fble/test/$base "$::out/fble/test/$base.o" $ldflags \
      "$::out/fble/test/libfbletest.a $::libfble"
    bin_cov $::out/fble/test/$base.cov "$::out/fble/test/$base.o" $ldflags_cov \
      "$::out/fble/test/libfbletest.a $::libfblecov"
  }
}

# fble-profiles-test
test $::out/fble/test/fble-profiles-test.tr \
  "$::out/fble/test/fble-profiles-test fble/test/ProfilesTest.fble" \
  "$::out/fble/test/fble-profiles-test -I fble/test -m /ProfilesTest% > $::out/fble/test/fble-profiles-test.prof"

# fble-compiled-profiles-test
fbleobj $::out/fble/test/ProfilesTest.o $::out/fble/bin/fble-compile \
  "-c -e FbleCompiledMain --main FbleProfilesTestMain -I fble/test -m /ProfilesTest%" \
  fble/test/ProfilesTest.fble
bin $::out/fble/test/ProfilesTest "$::out/fble/test/ProfilesTest.o" \
  "-L $::out/fble/test -lfbletest -L $::out/fble/lib -lfble" "$::libfble"
test $::out/fble/test/ProfilesTest.tr \
  "$::out/fble/test/ProfilesTest" \
  "$::out/fble/test/ProfilesTest > $::out/fble/test/ProfilesTest.prof"


# tests
test $::out/true.tr "" true
#test $::out/false.tr "" false

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
# * fble/test/extract-spec-test.tcl - extracts the .fble files from a particular
#   test. This is run at build time so we avoid re-extracting the .fble files
#   repeatedly if the test specification hasn't changed.
# * fble/test/run-spec-test.tcl - a helper function to execute a spec test that
#   takes the command to run as input. The primary purpose of this is to
#   encapsulate an otherwise complex command line to run the test and give us
#   nice test failure messages. It reads the test spec for the test to run,
#   but only to know what error location, if any, is expected for
#   fble-test-*-error tests.
set ::spec_tests [list]
set ::ldflags_fble [exec pkg-config --static --libs fble]
foreach dir [dirs langs/fble ""] {
  lappend ::build_ninja_deps "langs/fble/$dir"
  foreach {t} [lsort [glob -tails -directory langs/fble -nocomplain -type f $dir/*.tcl]] {
    set ::specroot [file rootname $t]
    set ::spectestdir $::out/langs/fble/$specroot
    set ::spectcl langs/fble/$t
    lappend ::build_ninja_deps $::spectcl

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
    # into a compiled-test binary.
    #
    # Inputs:
    #   main - The value to use for --main option to fble-compile for the top
    #          level module.
    #   modules - Non-top level modules to include.
    proc spec-test-compile { main modules } {
      set fbles [collect_modules "" $modules]
      lappend fbles "/Nat.fble"

      set objs [list]
      foreach x $fbles {
        set path [file rootname $x]%
        set fble $::spectestdir$x

        set o [string map {.fble .o} $fble]
        lappend objs $o

        fbleobj $o $::out/fble/bin/fble-compile.cov "-c -I $::spectestdir -m $path" $::spectestdir/test.fble
      }

      fbleobj $::spectestdir/test.o $::out/fble/bin/fble-compile.cov "--main $main -c -e FbleCompiledMain -I $::spectestdir -m /test%" $::spectestdir/test.fble
      lappend objs $::spectestdir/test.o

      bin $::spectestdir/compiled-test $objs \
        "-L $::out/fble/test -lfbletest $::ldflags_fble" \
        "$::out/fble/test/libfbletest.a $::libfble"

      # Run fble-disassemble here to get test coverage fble-disassemble.
      lappend ::spec_tests $::spectestdir/test.asm.tr
      test $::spectestdir/test.asm.tr \
        "$::out/fble/bin/fble-disassemble.cov $::spectestdir/test.fble" \
        "$::out/fble/bin/fble-disassemble.cov -I $::spectestdir -m /test% > $::spectestdir/test.asm"
    }

    proc spec-test-extract {} {
      build "$::spectestdir/test.fble"  \
        "fble/test/extract-spec-test.tcl $::spectcl langs/fble/Nat.fble" \
        "tclsh fble/test/extract-spec-test.tcl $::spectcl $::spectestdir langs/fble/Nat.fble"
    }

    proc fble-test { expr args } {
      spec-test-extract
      spec-test-compile FbleTestMain $args

      # We run with --profile to get better test coverage for profiling.
      lappend ::spec_tests $::spectestdir/test.tr
      test $::spectestdir/test.tr "fble/test/run-spec-test.tcl $::out/fble/test/fble-test.cov $::spectestdir/test.fble" \
        "tclsh fble/test/run-spec-test.tcl $::spectcl $::out/fble/test/fble-test.cov --profile /dev/null -I $::spectestdir -m /test%"

      lappend ::spec_tests $::spectestdir/test-compiled.tr
      test $::spectestdir/test-compiled.tr \
        "fble/test/run-spec-test.tcl $::spectestdir/compiled-test" \
        "tclsh fble/test/run-spec-test.tcl $::spectcl $::spectestdir/compiled-test --profile /dev/null"
    }

    proc fble-test-compile-error { loc expr args } {
      spec-test-extract

      lappend ::spec_tests $::spectestdir/test.tr
      test $::spectestdir/test.tr "fble/test/run-spec-test.tcl $::out/fble/test/fble-test.cov $::spectestdir/test.fble" \
        "tclsh fble/test/run-spec-test.tcl $::spectcl $::out/fble/test/fble-test.cov --compile-error -I $::spectestdir -m /test%"

      # TODO: Test that we get the desired compilation error when running the
      # compiler?
    }

    proc fble-test-runtime-error { loc expr args } {
      spec-test-extract
      spec-test-compile FbleTestMain $args

      lappend ::spec_tests $::spectestdir/test.tr
      test $::spectestdir/test.tr "fble/test/run-spec-test.tcl $::out/fble/test/fble-test.cov $::spectestdir/test.fble" \
        "tclsh fble/test/run-spec-test.tcl $::spectcl $::out/fble/test/fble-test.cov --runtime-error -I $::spectestdir -m /test%"

      lappend ::spec_tests $::spectestdir/test-compiled.tr
      test $::spectestdir/test-compiled.tr \
        "fble/test/run-spec-test.tcl $::spectestdir/compiled-test" \
        "tclsh fble/test/run-spec-test.tcl $::spectcl $::spectestdir/compiled-test --runtime-error"
    }

    proc fble-test-memory-constant { expr } {
      spec-test-extract
      spec-test-compile FbleMemTestMain {}

      lappend ::spec_tests $::spectestdir/test.tr
      test $::spectestdir/test.tr "fble/test/run-spec-test.tcl $::out/fble/test/fble-mem-test.cov $::spectestdir/test.fble" \
        "tclsh fble/test/run-spec-test.tcl $::spectcl $::out/fble/test/fble-mem-test.cov -I $::spectestdir -m /test%"

      lappend ::spec_tests $::spectestdir/test-compiled.tr
      test $::spectestdir/test-compiled.tr \
        "fble/test/run-spec-test.tcl $::spectestdir/compiled-test" \
        "tclsh fble/test/run-spec-test.tcl $::spectcl $::spectestdir/compiled-test"
    }

    proc fble-test-memory-growth { expr } {
      spec-test-extract
      spec-test-compile FbleMemTestMain {}

      lappend ::spec_tests $::spectestdir/test.tr
      test $::spectestdir/test.tr "fble/test/run-spec-test.tcl $::out/fble/test/fble-mem-test.cov $::spectestdir/test.fble" \
        "tclsh fble/test/run-spec-test.tcl $::spectcl $::out/fble/test/fble-mem-test.cov --growth -I $::spectestdir -m /test%"

      lappend ::spec_tests $::spectestdir/test-compiled.tr
      test $::spectestdir/test-compiled.tr \
        "fble/test/run-spec-test.tcl $::spectestdir/compiled-test" \
        "tclsh fble/test/run-spec-test.tcl $::spectcl $::spectestdir/compiled-test --growth"
    }
    source $::spectcl
  }
}

# Code coverage from spec tests.
build $::out/cov/gcov.txt "$::fble_objs_cov $::spec_tests" \
  "gcov $::fble_objs_cov > $::out/cov/gcov.txt && mv *.gcov $::out/cov"

# fble-profile-test
test $::out/fble/test/fble-profile-test.tr $::out/fble/test/fble-profile-test \
  "$::out/fble/test/fble-profile-test > /dev/null"

# fble 'core' library package.
eval {
  set objs [list]
  set cflags [exec pkg-config --cflags fble]

  # .c library files.
  foreach {x} { Core/char.fble Core/int.fble Core/stdio.fble Core/string.fble } {
    lappend objs $::out/pkgs/core/$x.o
    obj $::out/pkgs/core/$x.o pkgs/core/$x.c "$cflags -I pkgs/core"
  }

  pkg core $objs

  # fble-stdio program.
  obj $::out/pkgs/core/Core/fble-stdio.o pkgs/core/Core/fble-stdio.c \
    [exec pkg-config --cflags fble fble-core]
  bin $::out/pkgs/core/Core/fble-stdio "$::out/pkgs/core/Core/fble-stdio.o" \
    [exec pkg-config --static --libs fble fble-core] \
    "$::libfble $::out/pkgs/core/libfble-core.a"

  # Build an fble-stdio compiled binary.
  #
  # Inputs:
  #   target - the file to build.
  #   path - the module path to use as Stdio@ main.
  #   libs - additional pkg-config named libraries that this depends on.
  #   args - optional additional dependencies.
  proc stdio { target path libs args} {
    build $target.s $::out/fble/bin/fble-compile \
      "$::out/fble/bin/fble-compile --main FbleStdioMain -m $path > $target.s"
    asm $target.o $target.s
    bin $target "$target.o" \
      [exec pkg-config --static --libs fble fble-core {*}$libs] \
      "$::libfble $::out/pkgs/core/libfble-core.a" {*}$args
  }

  # /Core/Stdio/Cat% interpreted test.
  test $::out/pkgs/core/Core/Stdio/fble-cat.tr "$::out/pkgs/core/Core/fble-stdio $::out/pkgs/core/Core/Stdio/Cat.fble.d" \
    "$::out/pkgs/core/Core/fble-stdio -I pkgs/core -m /Core/Stdio/Cat% < README.txt > $::out/pkgs/core/Core/Stdio/fble-cat.out && cmp $::out/pkgs/core/Core/Stdio/fble-cat.out README.txt"

  # /Core/Stdio/Test% interpreted test.
  test $::out/pkgs/core/Core/Stdio/fble-stdio.tr "$::out/pkgs/core/Core/fble-stdio $::out/pkgs/core/Core/Stdio/Test.fble.d" \
    "$::out/pkgs/core/Core/fble-stdio -I pkgs/core -m /Core/Stdio/Test% > $::out/pkgs/core/Core/Stdio/fble-stdio.out && grep PASSED $::out/pkgs/core/Core/Stdio/fble-stdio.out > /dev/null"

  # /Core/Stdio/Test% compiled test.
  stdio $::out/pkgs/core/Core/Stdio/fble-stdio-test "/Core/Stdio/Test%" ""
  test $::out/pkgs/core/Core/Stdio/fble-stdio-test.tr $::out/pkgs/core/Core/Stdio/fble-stdio-test \
    "$::out/pkgs/core/Core/Stdio/fble-stdio-test > $::out/pkgs/core/Core/Stdio/fble-stdio-test.out && grep PASSED $::out/pkgs/core/Core/Stdio/fble-stdio-test.out > /dev/null"

  # Core/Tests interpreted
  test $::out/pkgs/core/Core/tests.tr "$::out/pkgs/core/Core/fble-stdio $::out/pkgs/core/Core/Tests.fble.d" \
    "$::out/pkgs/core/Core/fble-stdio -I pkgs/core -m /Core/Tests%" "pool = console"

  # Core/Tests compiled
  stdio $::out/pkgs/core/Core/core-tests "/Core/Tests%" "fble-core" \
    $::out/pkgs/core/libfble-core.a
  test $::out/pkgs/core/Core/core-tests.tr $::out/pkgs/core/Core/core-tests \
    "$::out/pkgs/core/Core/core-tests" "pool = console"
}

# fble 'app' library package.
eval {
  set objs [list]

  # .c library files.
  foreach {x} { App/app.fble } {
    lappend objs $::out/pkgs/app/$x.o
    obj $::out/pkgs/app/$x.o pkgs/app/$x.c "-I /usr/include/SDL2 -I fble/include -I pkgs/core -I pkgs/app"
  }

  pkg app $objs

  # fble-app program.
  obj $::out/pkgs/app/App/fble-app.o pkgs/app/App/fble-app.c \
    [exec pkg-config --cflags fble fble-core fble-app]
  bin $::out/pkgs/app/App/fble-app "$::out/pkgs/app/App/fble-app.o" \
    [exec pkg-config --static --libs fble-app] \
    "$::libfble $::out/pkgs/core/libfble-core.a $::out/pkgs/app/libfble-app.a"

  # Build an fble-app compiled binary.
  #
  # Inputs:
  #   target - the file to build.
  #   path - the module path to use as App@ main.
  #   libs - addition pkg-config named libraries to depend on.
  #   args - optional additional dependencies.
  proc app { target path libs args} {
    build $target.s $::out/fble/bin/fble-compile \
      "$::out/fble/bin/fble-compile --main FbleAppMain -m $path > $target.s"
    asm $target.o $target.s
    bin $target "$target.o" \
      [exec pkg-config --static --libs fble-app {*}$libs] \
      "$::libfble $::out/pkgs/core/libfble-core.a $::out/pkgs/app/libfble-app.a $args"
  }
}

# fble 'md5' library package.
eval {
  set objs [list]

  # .c library files.
  foreach {x} { Md5/md5.fble } {
    lappend objs $::out/pkgs/md5/$x.o
    obj $::out/pkgs/md5/$x.o pkgs/md5/$x.c "-I fble/include -I pkgs/core -I pkgs/md5"
  }

  pkg md5 $objs

  # Md5/Tests interpreted
  test $::out/pkgs/md5/Md5/tests.tr "$::out/pkgs/core/Core/fble-stdio $::out/pkgs/md5/Md5/Tests.fble.d" \
    "$::out/pkgs/core/Core/fble-stdio [exec pkg-config --cflags-only-I fble-md5] -m /Md5/Tests%" "pool = console"

  # Md5/Tests compiled
  stdio $::out/pkgs/md5/Md5/md5-tests "/Md5/Tests%" "fble-md5" \
    "$::out/pkgs/core/libfble-core.a $::out/pkgs/md5/libfble-md5.a"
  test $::out/pkgs/md5/Md5/md5-tests.tr $::out/pkgs/md5/Md5/md5-tests \
    "$::out/pkgs/md5/Md5/md5-tests" "pool = console"

  # fble-md5 program.
  obj $::out/pkgs/md5/Md5/fble-md5.o pkgs/md5/Md5/fble-md5.c \
    [exec pkg-config --cflags fble fble-core fble-md5]
  bin $::out/pkgs/md5/Md5/fble-md5 "$::out/pkgs/md5/Md5/fble-md5.o" \
    [exec pkg-config --static --libs fble-md5] \
    "$::libfble $::out/pkgs/core/libfble-core.a $::out/pkgs/md5/libfble-md5.a"

  # fble-md5 test
  test $::out/pkgs/md5/Md5/fble-md5.tr "$::out/pkgs/md5/Md5/fble-md5 $::out/pkgs/md5/Md5/Main.fble.d" \
    "$::out/pkgs/md5/Md5/fble-md5 -I pkgs/core -I pkgs/md5 -m /Md5/Main% /dev/null > $::out/pkgs/md5/Md5/fble-md5.out && grep d41d8cd98f00b204e9800998ecf8427e $::out/pkgs/md5/Md5/fble-md5.out > /dev/null"

}

# sat package
eval {
  pkg sat ""

  # /Sat/Tests% interpreted
  test $::out/pkgs/sat/Sat/tests.tr "$::out/pkgs/core/Core/fble-stdio $::out/pkgs/sat/Sat/Tests.fble.d" \
    "$::out/pkgs/core/Core/fble-stdio [exec pkg-config --cflags-only-I fble-sat] -m /Sat/Tests%" "pool = console"

  # /Sat/Tests% compiled
  stdio $::out/pkgs/sat/Sat/sat-tests "/Sat/Tests%" "fble-sat" \
    "$::out/pkgs/core/libfble-core.a $::out/pkgs/sat/libfble-sat.a"
  test $::out/pkgs/sat/Sat/sat-tests.tr $::out/pkgs/sat/Sat/sat-tests \
    "$::out/pkgs/sat/Sat/sat-tests" "pool = console"
}

# hwdg package
eval {
  pkg hwdg ""

  # /Hwdg/Tests% interpreted
  test $::out/pkgs/hwdg/Hwdg/tests.tr "$::out/pkgs/core/Core/fble-stdio $::out/pkgs/hwdg/Hwdg/Tests.fble.d" \
    "$::out/pkgs/core/Core/fble-stdio [exec pkg-config --cflags-only-I fble-hwdg] -m /Hwdg/Tests%" "pool = console"

  # /Hwdg/Tests% compiled
  stdio $::out/pkgs/hwdg/Hwdg/hwdg-tests "/Hwdg/Tests%" "fble-hwdg" \
    "$::out/pkgs/core/libfble-core.a $::out/pkgs/hwdg/libfble-hwdg.a"
  test $::out/pkgs/hwdg/Hwdg/hwdg-tests.tr $::out/pkgs/hwdg/Hwdg/hwdg-tests \
    "$::out/pkgs/hwdg/Hwdg/hwdg-tests" "pool = console"
}

# games package
eval {
  pkg games ""

  # /Games/Tests% interpreted
  test $::out/pkgs/games/Games/tests.tr "$::out/pkgs/core/Core/fble-stdio $::out/pkgs/games/Games/Tests.fble.d" \
    "$::out/pkgs/core/Core/fble-stdio [exec pkg-config --cflags-only-I fble-games] -m /Games/Tests%" "pool = console"

  # /Games/Tests% compiled
  stdio $::out/pkgs/games/Games/games-tests "/Games/Tests%" "fble-games" \
    "$::out/pkgs/core/libfble-core.a $::out/pkgs/app/libfble-app.a $::out/pkgs/games/libfble-games.a"
  test $::out/pkgs/games/Games/games-tests.tr $::out/pkgs/games/Games/games-tests \
    "$::out/pkgs/games/Games/games-tests" "pool = console"
}

# invaders package
eval {
  pkg invaders ""
  app $::out/pkgs/invaders/Invaders/fble-invaders "/Invaders/App%" \
    "fble-invaders" $::out/pkgs/invaders/libfble-invaders.a

  # /Invaders/Tests% interpreted
  test $::out/pkgs/invaders/Invaders/tests.tr "$::out/pkgs/core/Core/fble-stdio $::out/pkgs/invaders/Invaders/Tests.fble.d" \
    "$::out/pkgs/core/Core/fble-stdio [exec pkg-config --cflags-only-I fble-invaders] -m /Invaders/Tests%" "pool = console"

  # /Invaders/Tests% compiled
  stdio $::out/pkgs/invaders/Invaders/invaders-tests "/Invaders/Tests%" "fble-invaders" \
    "$::out/pkgs/core/libfble-core.a $::out/pkgs/app/libfble-app.a $::out/pkgs/invaders/libfble-invaders.a"
  test $::out/pkgs/invaders/Invaders/invaders-tests.tr $::out/pkgs/invaders/Invaders/invaders-tests \
    "$::out/pkgs/invaders/Invaders/invaders-tests" "pool = console"
}

# pinball package
eval {
  pkg pinball ""
  app $::out/pkgs/pinball/Pinball/fble-pinball "/Pinball/App%" \
    "fble-pinball" $::out/pkgs/pinball/libfble-pinball.a

  # /Pinball/Tests% interpreted
  test $::out/pkgs/pinball/Pinball/tests.tr "$::out/pkgs/core/Core/fble-stdio $::out/pkgs/pinball/Pinball/Tests.fble.d" \
    "$::out/pkgs/core/Core/fble-stdio [exec pkg-config --cflags-only-I fble-pinball] -m /Pinball/Tests%" "pool = console"

  # /Pinball/Tests% compiled
  stdio $::out/pkgs/pinball/Pinball/pinball-tests "/Pinball/Tests%" "fble-pinball" \
    "$::out/pkgs/core/libfble-core.a $::out/pkgs/app/libfble-app.a $::out/pkgs/pinball/libfble-pinball.a"
  test $::out/pkgs/pinball/Pinball/pinball-tests.tr $::out/pkgs/pinball/Pinball/pinball-tests \
    "$::out/pkgs/pinball/Pinball/pinball-tests" "pool = console"
}

# graphics package
eval {
  pkg graphics ""
  app $::out/pkgs/graphics/Graphics/fble-graphics "/Graphics/App%" \
    "fble-graphics" $::out/pkgs/graphics/libfble-graphics.a
}

pkg misc ""

set misc_cflags ""
set misc_libs ""
foreach pkg [list core sat app misc hwdg invaders pinball games graphics md5] {
  append misc_cflags " -I pkgs/$pkg"
  append misc_libs " $::out/pkgs/$pkg/libfble-$pkg.a"
}


# /Fble/DebugTest%
build $::out/pkgs/misc/fble-debug-test.s $::out/fble/bin/fble-compile \
  "$::out/fble/bin/fble-compile --main FbleTestMain -m /Fble/DebugTest% > $::out/pkgs/misc/fble-debug-test.s"
asm $::out/pkgs/misc/fble-debug-test.o $::out/pkgs/misc/fble-debug-test.s
bin $::out/pkgs/misc/fble-debug-test "$::out/pkgs/misc/fble-debug-test.o" \
  "-L $::out/fble/test -lfbletest [exec pkg-config --static --libs fble fble-core fble-misc]" \
  "$::libfble $::out/fble/test/libfbletest.a $::out/pkgs/core/libfble-core.a $misc_libs"

# Test that there are no dwarf warnings in the generated fble-debug-test
# binary.
build "$::out/pkgs/misc/fble-debug-test.dwarf $::out/pkgs/misc/fble-debug-test.dwarf-warnings.txt" \
  "$::out/pkgs/misc/fble-debug-test" \
  "objdump --dwarf $::out/pkgs/misc/fble-debug-test > $::out/pkgs/misc/fble-debug-test.dwarf 2> $::out/pkgs/misc/fble-debug-test.dwarf-warnings.txt"

# TODO: Understand why this test fails.
#test $::out/pkgs/misc/dwarf-test.tr \
#  "$::out/pkgs/misc/fble-debug-test.dwarf-warnings.txt" \
#  "cmp /dev/null $::out/pkgs/misc/fble-debug-test.dwarf-warnings.txt"

test $::out/pkgs/misc/fble-debug-test.tr \
  "$::out/pkgs/misc/fble-debug-test fble/test/fble-debug-test.exp" \
  "expect fble/test/fble-debug-test.exp > /dev/null"

# test summary
build $::out/tests.txt "$::tests" "echo $::tests > $::out/tests.txt"
build $::out/summary.tr "fble/test/tests.tcl $::out/tests.txt" \
  "tclsh fble/test/tests.tcl $::out/tests.txt && echo PASSED > $::out/summary.tr"

# build.ninja
build $::out/build.ninja "build.ninja.tcl" "tclsh build.ninja.tcl" \
  "depfile = $::out/build.ninja.d"

# build.ninja.d implicit dependency file.
set ::build_ninja_d [open "$::out/build.ninja.d" "w"]
puts $::build_ninja_d "$::out/build.ninja: $::build_ninja_deps"

