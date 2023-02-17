namespace eval "include" {
  foreach {x} [build_glob $::s/include/fble -tails *.h] {
    dist_s include/fble/$x
    install $x $::config::includedir/fble/$x
  }
}
