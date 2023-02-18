namespace eval "include" {
  foreach {x} [build_glob $::s/include/fble -tails *.h] {
    dist_s include/fble/$x
    install $::s/include/fble/$x $::config::includedir/fble/$x
  }
}
