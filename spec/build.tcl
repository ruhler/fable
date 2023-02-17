namespace eval "spec" {
  # Fbld spec www.
  ::html_doc $::b/www/spec/fble.html $::s/spec/fble.fbld $::s/spec/fble.fbld.tcl
  www $::b/www/spec/fble.html
}
