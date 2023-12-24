namespace eval "spec" {
  # Fble Spec www.
  fbld_html_doc $::b/www/spec/fble.html "$::s/spec/fble.lib.fbld $::s/spec/fble.fbld"
  www spec/fble.html

  # Fble Style Guide.
  ::fbld_html_doc $::b/www/spec/style.html $::s/spec/style.fbld
  www spec/style.html

  # Spec test README.
  ::fbld_html_doc $::b/www/spec/README.html $::s/spec/README.fbld
  www spec/README.html
}
