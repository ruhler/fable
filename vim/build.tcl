foreach dir [dirs $::s/vim ""] {
  foreach {x} [build_glob $::s/vim/$dir -type f *] {
    dist_s $x
  }
}
