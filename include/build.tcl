namespace eval "include" {
  foreach {x} [build_glob $::s/include/fble *.h] {
    dist_s $x
    install $x $::config::includedir/fble/[file tail $x]
  }
}
