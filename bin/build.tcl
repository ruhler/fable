namespace eval "bin" {

  set bin_sources {
    fble-compile.c
    fble-deps.c
    fble-disassemble.c
    fble-perf-profile.c
  }

  foreach {x} $bin_sources {
    set base [file rootname [file tail $x]]

    # Generated header file for help usage text.
    fbld_header_usage $::b/bin/$base.usage.h $::s/bin/$base.fbld \
      fbldUsageHelpText

    # The binary.
    obj $::b/bin/$base.o $::s/bin/$x "-I $::s/include -I $::b/bin" \
      $::b/bin/$base.usage.h
    bin $::b/bin/$base "$::b/bin/$base.o" "$::b/lib/libfble.so" ""
    bin_cov $::b/bin/$base.cov "$::b/bin/$base.o" "$::b/lib/libfble.cov.so" ""
    install $::b/bin/$base $::config::bindir/$base

    # Man page.
    fbld_man_usage $::b/bin/$base.1 $::s/bin/$base.fbld
    install $::b/bin/$base.1 $::config::mandir/man1/$base.1
  }

  # Check doc comments
  foreach {x} [build_glob $::s/bin -tails "*.h" "*.c"] {
    fbld_check_dc $::b/bin/$x.dc $::s/bin/$x
  }

  # Generate a .d file for a .fble file.
  #   target - the name of the .d file to generate.
  #   module - the module path to generate it for.
  #   flags - search path flags for the module.
  #   args - additional dependencies for build
  proc ::fbledep { target module flags args} {
    build $target "$::b/bin/fble-deps $args" \
      "$::b/bin/fble-deps $flags -t $target -m $module > $target" \
      "depfile = $target"
  }

  # Compile a .fble file to .o via aarch64.
  #
  # Inputs:
  #   obj - the name of the .o file to generate.
  #   compile - the name of the fble-compile executable.
  #   compileargs - args to pass to the fble compiler.
  #   args - additional dependencies, typically including the .fble file.
  proc ::fbleobj_aarch64 { obj compile compileargs args } {
    set s [string map {.o .s} $obj]
    build $s "$compile $args" "$compile $compileargs > $s"
    set cmd "as -o $obj $s"
    build $obj "$s $args" $cmd
  }

  # Compile a .fble file to .o via c.
  #
  # Inputs:
  #   obj - the name of the .o file to generate.
  #   compile - the name of the fble-compile executable.
  #   compileargs - args to pass to the fble compiler.
  #   args - additional dependencies, typically including the .fble file.
  proc ::fbleobj_c { obj compile compileargs args } {
    set c [string map {.o .c} $obj]
    build $c "$compile $args" "$compile -t c $compileargs > $c"
    set cmd "gcc -c -o $obj -I $::s/include $c"
    build $obj "$c $args" $cmd
  }

  # Compile a .fble file to .o.
  #
  # Via whatever target it thinks is best.
  #
  # Inputs:
  #   obj - the name of the .o file to generate.
  #   compile - the name of the fble-compile executable.
  #   compileargs - args to pass to the fble compiler.
  #   args - additional dependencies, typically including the .fble file.
  proc ::fbleobj { obj compile compileargs args } {
    if {$::arch == "aarch64"} {
      fbleobj_aarch64 $obj $compile $compileargs {*}$args
    } else {
      fbleobj_c $obj $compile $compileargs {*}$args
    }
  }

  # Test for fble-perf-profile.c
  # The input file is designed to match the first "Test a simple call profile"
  # in test/fble-profile-test.c. The target output will need to be updated any
  # time the output format of FbleGenerateProfileReport changes.
  build $::b/bin/fble-perf-profile.test.got \
    "$::b/bin/fble-perf-profile $::s/bin/fble-perf-profile.test.in" \
    "$::b/bin/fble-perf-profile < $::s/bin/fble-perf-profile.test.in > $::b/bin/fble-perf-profile.test.got"
  test $::b/bin/fble-perf-profile.tr \
    "$::s/bin/fble-perf-profile.test.want $::b/bin/fble-perf-profile.test.got" \
    "diff --strip-trailing-cr $::s/bin/fble-perf-profile.test.want $::b/bin/fble-perf-profile.test.got"

}
