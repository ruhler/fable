namespace eval "spec" {
  dist_s $::s/spec/build.tcl
  dist_s $::s/spec/fble.fbld
  dist_s $::s/spec/fble.fbld.tcl
  dist_s $::s/spec/fble.lang
  dist_s $::s/spec/style.fbld
  dist_s $::s/spec/README.fbld

  foreach dir [dirs $::s/spec/SpecTests ""] {
    foreach {x} [build_glob $::s/spec/SpecTests/$dir -nocomplain -type f *] {
      dist_s $x
    }
  }

  # Fble spec www.
  ::html_doc $::b/www/spec/fble.html $::s/spec/fble.fbld $::s/spec/fble.fbld.tcl
  www $::b/www/spec/fble.html

  # Fble Style Guide.
  ::html_doc $::b/www/spec/style.html $::s/spec/style.fbld
  www $::b/www/spec/style.html
}
