namespace eval "fble/bin" {
  foreach {x} [glob fble/bin/*.c] {
    set base [file rootname [file tail $x]]
    obj $::out/fble/bin/$base.o $x "-I fble/include"
    bin $::out/fble/bin/$base "$::out/fble/bin/$base.o $::libfble" ""
    bin_cov $::out/fble/bin/$base.cov "$::out/fble/bin/$base.o $::libfblecov" ""
  }
}
