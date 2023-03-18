namespace eval "spec" {
  # Fble spec www.
  ::html_doc $::b/www/spec/fble.html $::s/spec/fble.fbld $::s/spec/fble.fbld.tcl
  www $::b/www/spec/fble.html

  # Fble Style Guide.
  ::html_doc $::b/www/spec/style.html $::s/spec/style.fbld
  www $::b/www/spec/style.html
}
