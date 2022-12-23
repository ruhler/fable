namespace eval "www" {
  build "$::b/www/index.html" "$::s/README.adoc" \
    "asciidoc -o $::b/www/index.html $::s/README.adoc"
  build "$::b/www/spec/fble.html" "$::s/spec/fble.adoc" \
    "asciidoc -o $::b/www/spec/fble.html $::s/spec/fble.adoc"

  phony "www" [list $::b/www/index.html $::b/www/spec/fble.html doxygen]
}
