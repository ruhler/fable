namespace eval "bin" {
  dist_s $::s/bin/build.tcl

  set bin_sources {
    fble-compile.c
    fble-deps.c
    fble-disassemble.c
  }

  foreach {x} $bin_sources {
    set base [file rootname [file tail $x]]

    dist_s $::s/bin/$base.c
    dist_s $::s/bin/$base.fbld

    test $::b/bin/$base.fbld.tr \
      "$::s/bin/$base.fbld $::s/fbld/fbld.tcl $::s/fbld/runfbld.tcl $::s/fbld/check.tcl $::s/fbld/usage.man.tcl $::s/fbld/usage.lib.tcl" \
      "tclsh8.6 $::s/fbld/runfbld.tcl $::s/fbld/usage.man.tcl $::s/fbld/usage.lib.tcl $::s/fbld/check.tcl < $::s/bin/$base.fbld"

    # Generated header file for help usage text.
    build $::b/bin/$base.roff \
      "$::s/bin/$base.fbld $::s/fbld/usage.help.tcl $::s/fbld/fbld.tcl $::s/fbld/runfbld.tcl $::s/fbld/roff.tcl $::s/fbld/usage.lib.tcl" \
      "tclsh8.6 $::s/fbld/runfbld.tcl $::s/fbld/usage.help.tcl $::s/fbld/roff.tcl $::s/fbld/usage.lib.tcl < $::s/bin/$base.fbld > $::b/bin/$base.roff"
    build $::b/bin/$base.txt $::b/bin/$base.roff \
      "groff -T ascii < $::b/bin/$base.roff > $::b/bin/$base.txt"
    build $::b/bin/$base.usage.h \
      "$::s/fbld/cdata.tcl $::b/bin/$base.txt" \
      "tclsh8.6 $::s/fbld/cdata.tcl fbldUsageHelpText < $::b/bin/$base.txt > $::b/bin/$base.usage.h"

    # The binary.
    obj $::b/bin/$base.o $::s/bin/$x "-I $::s/include -I $::b/bin" \
      $::b/bin/$base.usage.h
    bin $::b/bin/$base \
      "$::b/bin/$base.o $::b/lib/libfble.a" ""
    bin_cov $::b/bin/$base.cov \
      "$::b/bin/$base.o $::b/lib/libfble.cov.a" ""
    install $::b/bin/$base $::config::bindir/$base

    # Man page.
    build $::b/bin/$base.1 \
      "$::s/bin/$base.fbld $::s/fbld/usage.man.tcl $::s/fbld/fbld.tcl $::s/fbld/runfbld.tcl $::s/fbld/man.tcl $::s/fbld/usage.lib.tcl" \
      "tclsh8.6 $::s/fbld/runfbld.tcl $::s/fbld/usage.man.tcl $::s/fbld/man.tcl $::s/fbld/usage.lib.tcl < $::s/bin/$base.fbld > $::b/bin/$base.1"
    install $::b/bin/$base.1 $::config::mandir/man1/$base.1
  }

  # Generate a .d file for a .fble file.
  #   target - the name of the .d file to generate.
  #   module - the module path to generate it for.
  #   flags - search path flags for the module.
  proc ::fbledep { target module flags } {
    build $target "$::b/bin/fble-deps" \
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
}
