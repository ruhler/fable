foreach dir [dirs $::s/thoughts ""] {
  foreach {x} [build_glob $::s/thoughts/$dir -tails -type f *] {
    dist_s thoughts/$dir/$x
  }
}
