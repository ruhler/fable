foreach dir [dirs $::s/book ""] {
  foreach {x} [build_glob $::s/book/$dir -type f *] {
    dist_s $x
  }
}
