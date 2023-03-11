namespace eval "include" {
  dist_s $::s/include/build.tcl
  foreach {x} [build_glob $::s/include/fble *.h] {
    dist_s $x
    install $x $::config::includedir/fble/[file tail $x]
  }

  # Man pages for fble-value.h
  foreach x [list FbleNewValueHeap FbleEval] {
    man_dc $::b/include/fble/$x.3 $::s/include/fble/fble-value.h $x
    install $::b/include/fble/$x.3 $::config::mandir/man3/$x.3
  }
}
