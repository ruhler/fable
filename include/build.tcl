namespace eval "include" {
  dist_s $::s/include/build.tcl
  foreach {x} [build_glob $::s/include/fble *.h] {
    dist_s $x
    install $x $::config::includedir/fble/[file tail $x]
  }

  man_dc $::b/include/fble/FbleEval.3 $::s/include/fble/fble-value.h FbleEval
  install $::b/include/fble/FbleEval.3 $::config::mandir/man3/FbleEval.3
}
