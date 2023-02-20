namespace eval "spec" {
  dist_s $::s/spec/build.tcl
  dist_s $::s/spec/fble.fbld
  dist_s $::s/spec/fble.fbld.tcl
  dist_s $::s/spec/fble.lang
  dist_s $::s/spec/fble.style.txt
  dist_s $::s/spec/README.fbld

  foreach dir [dirs $::s/spec/SpecTests ""] {
    foreach {x} [build_glob $::s/spec/SpecTests/$dir -nocomplain -type f *] {
      dist_s $x
    }
  }

  # Fbld spec www.
  ::html_doc $::b/www/spec/fble.html $::s/spec/fble.fbld $::s/spec/fble.fbld.tcl
  www $::b/www/spec/fble.html
}
