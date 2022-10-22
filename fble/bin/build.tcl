namespace eval "fble/bin" {
  foreach {x} [glob $::s/fble/bin/*.c] {
    set base [file rootname [file tail $x]]
    obj $::b/fble/bin/$base.o $x "-I $::s/fble/include"
    bin $::b/fble/bin/$base \
      "$::b/fble/bin/$base.o $::b/fble/lib/libfble.a" ""
    bin_cov $::b/fble/bin/$base.cov \
      "$::b/fble/bin/$base.o $::b/fble/lib/libfble.cov.a" ""
  }
}
