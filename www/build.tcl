namespace eval "www" {
  build "$::b/www/index.html" "$::s/README.adoc" \
    "asciidoc -o $::b/www/index.html $::s/README.adoc"

  phony "www" [list $::b/www/index.html]
}
