foreach dir [dirs $::s/thoughts ""] {
  foreach {x} [build_glob $::s/thoughts/$dir -type f *] {
    dist_s $x
  }
}
