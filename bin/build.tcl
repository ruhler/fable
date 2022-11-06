namespace eval "bin" {
  foreach {x} [glob $::s/bin/*.c] {
    set base [file rootname [file tail $x]]
    obj $::b/bin/$base.o $x "-I $::s/include"
    bin $::b/bin/$base \
      "$::b/bin/$base.o $::b/lib/libfble.a" ""
    bin_cov $::b/bin/$base.cov \
      "$::b/bin/$base.o $::b/lib/libfble.cov.a" ""
    install_bin $::b/bin/$base
  }

  build $::b/bin/fble-compile.1 $::b/bin/fble-compile \
    "help2man -N -o $::b/bin/fble-compile.1 -n 'compile an fble module' $::b/bin/fble-compile"
}
