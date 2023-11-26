namespace eval "spec" {
  # Fble Spec www.
  fbld_html_doc $::b/www/spec/fble.html "$::s/spec/fble.lib.fbld $::s/spec/fble.fbld"
  www $::b/www/spec/fble.html

  # Fble Style Guide.
  ::fbld_html_doc $::b/www/spec/style.html $::s/spec/style.fbld
  www $::b/www/spec/style.html
}
