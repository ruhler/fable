namespace eval "pkgs/std" {
  fbld_header_usage $::b/pkgs/std/fble-cli.usage.h $::s/pkgs/std/fble-cli.fbld \
    fbldUsageHelpText

  # .c library files.
  set objs [list]
  foreach {x} { cli.fble data.fble debug.fble env.fble io.fble stdio.fble } {
    lappend objs $::b/pkgs/std/$x.o
    obj $::b/pkgs/std/$x.o $::s/pkgs/std/$x.c \
      "-I $::s/include -I $::s/pkgs/std -I $::s/out/pkgs/std" \
      "$::b/pkgs/std/fble-cli.usage.h"
  }

  pkg std [list] "" $objs

  # Check doc comments
  foreach {x} [build_glob $::s/pkgs/std -tails "*.h" "*.c"] {
    fbld_check_dc $::b/pkgs/std/$x.dc $::s/pkgs/std/$x
  }

  # fble-cli program.
  fbld_man_usage $::b/pkgs/std/fble-cli.1 $::s/pkgs/std/fble-cli.fbld
  install $::b/pkgs/std/fble-cli.1 $::config::mandir/man1/fble-cli.1
  obj $::b/pkgs/std/fble-cli.o $::s/pkgs/std/fble-cli.c \
    "-I $::s/include -I $::s/pkgs/std"
  bin $::b/pkgs/std/fble-cli \
    "$::b/pkgs/std/fble-cli.o" \
    "$::b/pkgs/std/libfble-std$::lext $::b/lib/libfble$::lext" ""
  install $::b/pkgs/std/fble-cli $::config::bindir/fble-cli

  # Build an fble-cli compiled binary.
  #
  # Inputs:
  #   target - the file to build.
  #   path - the module path to use as Main@.
  #   libs - additional fble packages this depends on, not including std,
  #          without the fble- prefix.
  #   lflags - additional linker flags to use when creating the binary.
  proc ::cli { target path libs lflags} {
    set objs $target.o
    set nlibs ""
    foreach lib [lreverse $libs] {
      append nlibs " $::b/pkgs/$lib/libfble-$lib$::lext"
    }
    append nlibs " $::b/pkgs/std/libfble-std$::lext $::b/lib/libfble$::lext"

    fblemain $target.o $::b/bin/fble-compile "--main FbleCliMain -m $path"
    bin $target $objs $nlibs $lflags
  }

  # Runs an fble-cli command with proper dependency tracking.
  #   target - where to put the output of the fble-cli command.
  #   cmdargs - arguments to fble-cli
  #   args - additional dependencies.
  proc ::run_cli { target cmdargs args } {
    build $target "$::b/pkgs/std/fble-cli $args" \
      "$::b/pkgs/std/fble-cli --deps-file $target.d --deps-target $target $cmdargs > $target" \
      "depfile = $target.d"
  }

  # Runs an fble-cli tests suite interpreted.
  proc ::run_cli_tests { target cmdargs deps } {
    run_cli $target.out "$cmdargs -- --prefix Interpreted." $deps
    testsuite $target $target.out "cat $target.out"
  }

  # /Std/Io/Cli/Demo% interpreted test.
  run_cli $::b/pkgs/std/fble-cli.out \
    "-I $::s/pkgs/std -m /Std/Io/Cli/Demo%"
  test $::b/pkgs/std/fble-cli.tr $::b/pkgs/std/fble-cli.out \
    "grep hello $::b/pkgs/std/fble-cli.out"

  # /Std/Io/Cli/Demo% compiled test.
  cli $::b/pkgs/std/fble-cli-demo "/Std/Io/Cli/Demo%" "" ""
  test $::b/pkgs/std/fble-cli-demo.out \
    $::b/pkgs/std/fble-cli-demo \
    "$::b/pkgs/std/fble-cli-demo > $::b/pkgs/std/fble-cli-demo.out"
  test $::b/pkgs/std/fble-cli-demo.tr $::b/pkgs/std/fble-cli-demo.out \
    "grep hello $::b/pkgs/std/fble-cli-demo.out"

  # Test FbleNewStringValue unicode conversion.
  test $::b/pkgs/std/fble-cli-demo.unicode_arg.out \
    $::b/pkgs/std/fble-cli-demo \
    "$::b/pkgs/std/fble-cli-demo echo aπb > $::b/pkgs/std/fble-cli-demo.unicode_arg.out"
  test $::b/pkgs/std/fble-cli-demo.unicode_arg.tr $::b/pkgs/std/fble-cli-demo.unicode_arg.out \
    "grep aπb $::b/pkgs/std/fble-cli-demo.unicode_arg.out"

  # Test FbleStringValueAccess unicode conversion.
  test $::b/pkgs/std/fble-cli-demo.unicode_filename.out \
    "$::b/pkgs/std/fble-cli-demo $::s/pkgs/std/aπb.txt" \
    "$::b/pkgs/std/fble-cli-demo cat $::s/pkgs/std/aπb.txt > $::b/pkgs/std/fble-cli-demo.unicode_filename.out"
  test $::b/pkgs/std/fble-cli-demo.unicode_filename.tr $::b/pkgs/std/fble-cli-demo.unicode_filename.out \
    "grep helloπ $::b/pkgs/std/fble-cli-demo.unicode_filename.out"

  # Test environment variable access.
  test $::b/pkgs/std/fble-cli-demo.env.out \
    "$::b/pkgs/std/fble-cli-demo" \
    "env FOO=hello $::b/pkgs/std/fble-cli-demo env FOO > $::b/pkgs/std/fble-cli-demo.env.out"
  test $::b/pkgs/std/fble-cli-demo.env.tr $::b/pkgs/std/fble-cli-demo.env.out \
    "grep hello $::b/pkgs/std/fble-cli-demo.env.out"

}
