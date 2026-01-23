namespace eval "pkgs/core" {
  fbld_header_usage $::b/pkgs/core/fble-stdio.usage.h $::s/pkgs/core/fble-stdio.fbld \
    fbldUsageHelpText
  fbld_header_usage $::b/pkgs/core/fble-cli.usage.h $::s/pkgs/core/fble-cli.fbld \
    fbldUsageHelpText

  # .c library files.
  set objs [list]
  foreach {x} { char.fble cli.fble debug.fble int.fble stdio.fble stdio.native.fble string.fble } {
    lappend objs $::b/pkgs/core/$x.o
    obj $::b/pkgs/core/$x.o $::s/pkgs/core/$x.c \
      "-I $::s/include -I $::s/pkgs/core -I $::b/pkgs/core" \
      "$::b/pkgs/core/fble-stdio.usage.h $::b/pkgs/core/fble-cli.usage.h"
  }
  pkg core [list std] "" $objs

  # Check doc comments
  foreach {x} [build_glob $::s/pkgs/core -tails "*.h" "*.c"] {
    fbld_check_dc $::b/pkgs/core/$x.dc $::s/pkgs/core/$x
  }

  # fble-stdio program.
  fbld_man_usage $::b/pkgs/core/fble-stdio.1 $::s/pkgs/core/fble-stdio.fbld
  install $::b/pkgs/core/fble-stdio.1 $::config::mandir/man1/fble-stdio.1
  obj $::b/pkgs/core/fble-stdio.o $::s/pkgs/core/fble-stdio.c \
    "-I $::s/include -I $::s/pkgs/std -I $::s/pkgs/core"
  bin $::b/pkgs/core/fble-stdio \
    "$::b/pkgs/core/fble-stdio.o" \
    "$::b/pkgs/core/libfble-core$::lext $::b/pkgs/std/libfble-std$::lext $::b/lib/libfble$::lext" ""
  install $::b/pkgs/core/fble-stdio $::config::bindir/fble-stdio

  # fble-cli program.
  fbld_man_usage $::b/pkgs/core/fble-cli.1 $::s/pkgs/core/fble-cli.fbld
  install $::b/pkgs/core/fble-cli.1 $::config::mandir/man1/fble-cli.1
  obj $::b/pkgs/core/fble-cli.o $::s/pkgs/core/fble-cli.c \
    "-I $::s/include -I $::s/pkgs/std -I $::s/pkgs/core"
  bin $::b/pkgs/core/fble-cli \
    "$::b/pkgs/core/fble-cli.o" \
    "$::b/pkgs/core/libfble-core$::lext $::b/pkgs/std/libfble-std$::lext $::b/lib/libfble$::lext" ""
  install $::b/pkgs/core/fble-cli $::config::bindir/fble-cli

  # Build an fble-stdio compiled binary.
  #
  # Inputs:
  #   target - the file to build.
  #   path - the module path to use as Stdio@ main.
  #   libs - additional fble packages this depends on, not including core,
  #          without the fble- prefix.
  #   lflags - additional linker flags to use when creating the binary.
  proc ::stdio { target path libs lflags} {
    set objs $target.o
    set nlibs ""
    foreach lib [lreverse $libs] {
      append nlibs " $::b/pkgs/$lib/libfble-$lib$::lext"
    }
    append nlibs " $::b/pkgs/core/libfble-core$::lext $::b/pkgs/std/libfble-std$::lext $::b/lib/libfble$::lext"

    fblemain $target.o $::b/bin/fble-compile "--main FbleStdioMain -m $path"
    bin $target $objs $nlibs $lflags
  }

  # Runs an fble-stdio command with proper dependency tracking.
  #   target - where to put the output of the fble-stdio command.
  #   cmdargs - arguments to fble-stdio
  #   args - additional dependencies.
  proc ::run_stdio { target cmdargs args } {
    build $target "$::b/pkgs/core/fble-stdio $args" \
      "$::b/pkgs/core/fble-stdio --deps-file $target.d --deps-target $target $cmdargs > $target" \
      "depfile = $target.d"
  }

  # Runs an fble-stdio tests suite interpreted.
  proc ::run_stdio_tests { target cmdargs deps } {
    run_stdio $target.out "$cmdargs -- --prefix Interpreted." $deps
    testsuite $target $target.out "cat $target.out"
  }

  # Build an fble-cli compiled binary.
  #
  # Inputs:
  #   target - the file to build.
  #   path - the module path to use as Main@.
  #   libs - additional fble packages this depends on, not including core,
  #          without the fble- prefix.
  #   lflags - additional linker flags to use when creating the binary.
  proc ::cli { target path libs lflags} {
    set objs $target.o
    set nlibs ""
    foreach lib [lreverse $libs] {
      append nlibs " $::b/pkgs/$lib/libfble-$lib$::lext"
    }
    append nlibs " $::b/pkgs/core/libfble-core$::lext $::b/pkgs/std/libfble-std$::lext $::b/lib/libfble$::lext"

    fblemain $target.o $::b/bin/fble-compile "--main FbleCliMain -m $path"
    bin $target $objs $nlibs $lflags
  }

  # Runs an fble-cli command with proper dependency tracking.
  #   target - where to put the output of the fble-cli command.
  #   cmdargs - arguments to fble-cli
  #   args - additional dependencies.
  proc ::run_cli { target cmdargs args } {
    build $target "$::b/pkgs/core/fble-cli $args" \
      "$::b/pkgs/core/fble-cli --deps-file $target.d --deps-target $target $cmdargs > $target" \
      "depfile = $target.d"
  }

  # Runs an fble-cli tests suite interpreted.
  proc ::run_cli_tests { target cmdargs deps } {
    run_cli $target.out "$cmdargs -- --prefix Interpreted." $deps
    testsuite $target $target.out "cat $target.out"
  }

  # /Core/Stdio/Cat% interpreted test.
  run_cli $::b/pkgs/core/Core/Stdio/fble-cat.out \
    "-I $::s/pkgs/std -I $::s/pkgs/core -m /Core/Stdio/Cat% < $::s/README.fbld"
  test $::b/pkgs/core/Core/Stdio/fble-cat.tr \
    "$::b/pkgs/core/Core/Stdio/fble-cat.out $::s/README.fbld" \
    "diff -Z $::b/pkgs/core/Core/Stdio/fble-cat.out $::s/README.fbld"

  # /Core/Stdio/Cat% interpreted test 2.
  run_cli $::b/pkgs/core/Core/Stdio/fble-cat.2.out \
    "-I $::s/pkgs/std -I $::s/pkgs/core -m /Core/Stdio/Cat% -- $::s/README.fbld"
  test $::b/pkgs/core/Core/Stdio/fble-cat.2.tr \
    "$::b/pkgs/core/Core/Stdio/fble-cat.2.out" \
    "diff -Z $::b/pkgs/core/Core/Stdio/fble-cat.2.out $::s/README.fbld"

  # /Core/Stdio/Cat% compiled.
  cli $::b/pkgs/core/fble-cat "/Core/Stdio/Cat%" "" ""
  install $::b/pkgs/core/fble-cat $::config::bindir/fble-cat
  fbld_man_usage $::b/pkgs/core/fble-cat.1 $::s/pkgs/core/fble-cat.fbld
  install $::b/pkgs/core/fble-cat.1 $::config::mandir/man1/fble-cat.1

  # /Core/Stdio/FastCat% compiled.
  stdio $::b/pkgs/core/fble-fast-cat "/Core/Stdio/FastCat%" "" ""
  install $::b/pkgs/core/fble-fast-cat $::config::bindir/fble-fast-cat

  # fble-fast-cat test.
  build $::b/pkgs/core/fble-fast-cat.out \
    "$::b/pkgs/core/fble-fast-cat $::s/README.fbld" \
    "$::b/pkgs/core/fble-fast-cat < $::s/README.fbld > $::b/pkgs/core/fble-fast-cat.out"
  test $::b/pkgs/core/fble-fast-cat.tr \
    "$::b/pkgs/core/fble-fast-cat.out $::s/README.fbld" \
    "diff -Z $::b/pkgs/core/fble-fast-cat.out $::s/README.fbld"

  # /Core/Stdio/HelloWorld% interpreted test.
  run_cli $::b/pkgs/core/Core/Stdio/fble-cli.out \
    "-I $::s/pkgs/std -I $::s/pkgs/core -m /Core/Stdio/HelloWorld%"
  test $::b/pkgs/core/Core/Stdio/fble-cli.tr $::b/pkgs/core/Core/Stdio/fble-cli.out \
    "grep hello $::b/pkgs/core/Core/Stdio/fble-cli.out"

  # /Core/Stdio/HelloWorld% compiled test.
  cli $::b/pkgs/core/Core/Stdio/fble-cli-test "/Core/Stdio/HelloWorld%" "" ""
  test $::b/pkgs/core/Core/Stdio/fble-cli-test.out \
    $::b/pkgs/core/Core/Stdio/fble-cli-test \
    "$::b/pkgs/core/Core/Stdio/fble-cli-test > $::b/pkgs/core/Core/Stdio/fble-cli-test.out"
  test $::b/pkgs/core/Core/Stdio/fble-cli-test.tr $::b/pkgs/core/Core/Stdio/fble-cli-test.out \
    "grep hello $::b/pkgs/core/Core/Stdio/fble-cli-test.out"

  # Core/Tests interpreted
  run_cli_tests $::b/pkgs/core/Core/tests.tr "-I $::s/pkgs/std -I $::s/pkgs/core -m /Core/Tests%" ""

  # Core/Tests compiled
  cli $::b/pkgs/core/Core/core-tests "/Core/Tests%" "" ""
  testsuite $::b/pkgs/core/Core/core-tests.tr $::b/pkgs/core/Core/core-tests \
    "$::b/pkgs/core/Core/core-tests --prefix Compiled."
}
