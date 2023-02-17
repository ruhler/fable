foreach dir [dirs $::s/vim ""] {
  foreach {x} [build_glob $::s/vim/$dir -tails -type f *] {
    dist_s vim/$dir/$x
  }
}
