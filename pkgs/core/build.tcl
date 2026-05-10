namespace eval "pkgs/core" {
  fbld_header_usage $::b/pkgs/core/fble-cli.usage.h $::s/pkgs/core/fble-cli.fbld \
    fbldUsageHelpText

  # .c library files.
  set objs [list]
  foreach {x} { char.fble cli.fble debug.fble env.fble int.fble io.fble stdio.fble string.fble } {
    lappend objs $::b/pkgs/core/$x.o
    obj $::b/pkgs/core/$x.o $::s/pkgs/core/$x.c \
      "-I $::s/include -I $::s/pkgs/core -I $::b/pkgs/core" \
      "$::b/pkgs/core/fble-cli.usage.h"
  }
  pkg core [list std] "" $objs

  # Check doc comments
  foreach {x} [build_glob $::s/pkgs/core -tails "*.h" "*.c"] {
    fbld_check_dc $::b/pkgs/core/$x.dc $::s/pkgs/core/$x
  }

  # fble-cli program.
  fbld_man_usage $::b/pkgs/core/fble-cli.1 $::s/pkgs/core/fble-cli.fbld
  install $::b/pkgs/core/fble-cli.1 $::config::mandir/man1/fble-cli.1
  obj $::b/pkgs/core/fble-cli.o $::s/pkgs/core/fble-cli.c \
    "-I $::s/include -I $::s/pkgs/std -I $::s/pkgs/core"
  bin $::b/pkgs/core/fble-cli \
    "$::b/pkgs/core/fble-cli.o" \
    "$::b/pkgs/core/libfble-core$::lext $::b/pkgs/std/libfble-std$::lext $::b/lib/libfble$::lext" ""
  install $::b/pkgs/core/fble-cli $::config::bindir/fble-cli

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

  # /Std/Io/Cli/Demo% interpreted test.
  run_cli $::b/pkgs/core/fble-cli.out \
    "-I $::s/pkgs/std -m /Std/Io/Cli/Demo%"
  test $::b/pkgs/core/fble-cli.tr $::b/pkgs/core/fble-cli.out \
    "grep hello $::b/pkgs/core/fble-cli.out"

  # /Std/Io/Cli/Demo% compiled test.
  cli $::b/pkgs/core/fble-cli-demo "/Std/Io/Cli/Demo%" "" ""
  test $::b/pkgs/core/fble-cli-demo.out \
    $::b/pkgs/core/fble-cli-demo \
    "$::b/pkgs/core/fble-cli-demo > $::b/pkgs/core/fble-cli-demo.out"
  test $::b/pkgs/core/fble-cli-demo.tr $::b/pkgs/core/fble-cli-demo.out \
    "grep hello $::b/pkgs/core/fble-cli-demo.out"
}
