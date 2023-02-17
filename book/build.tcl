foreach dir [dirs $::s/book ""] {
  foreach {x} [build_glob $::s/book/$dir -tails -type f *] {
    dist_s book/$dir/$x
  }
}
